// The persona module: a small C API over persona-generator, built into the
// npm package in package/ by scripts/build_package.py.
//
// Every call returns a JSON document, a persona or {"error": "..."}; the
// pointer stays valid until the next call. String inputs are UTF-8 and an
// empty string leaves the choice to the generator.

#include "cpp23_polyfill.hpp"
#include <dasmig/personagen.hpp>
#include <emscripten/emscripten.h>
#include "support.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <exception>
#include <string>

namespace
{

std::string result_; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

const char* reply(std::string json)
{
    result_ = std::move(json);
    return result_.c_str();
}

// The libraries' lite data is compiled in; city's admin1 names are embedded
// in the module's file system (--embed-file), so nothing is fetched.
void load_once()
{
    auto& cg = dasmig::cg::instance();
    if (!cg.has_admin1()) cg.load_admin1("/resources/city/admin1CodesASCII.txt");
}

std::string hex(std::uint64_t value)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string out(16, '0');
    for (int i = 15; i >= 0; --i, value >>= 4) out[static_cast<std::size_t>(i)] = digits[value & 0xf];
    return out;
}

// The next part of a comma-separated list; false at its end.
bool next_part(std::string_view& list, std::string_view& part)
{
    if (list.empty()) return false;
    const auto comma = list.find(',');
    part = list.substr(0, comma);
    list = comma == std::string_view::npos ? std::string_view{} : list.substr(comma + 1);
    return true;
}

// The papers and the wallet the C API's arguments describe.
void add_parts(dasmig::persona_request& request, int identity, std::string_view documents, int finance,
               std::string_view accounts, std::string_view cards, std::string_view card_numbers, const char* network,
               int safe)
{
    std::string_view part;
    if (identity != 0)
    {
        dasmig::identity_request papers;
        while (next_part(documents, part))
        {
            if (part == "passport") papers.documents.push_back(dasmig::id_document_kind::passport);
            else if (part == "id_card") papers.documents.push_back(dasmig::id_document_kind::id_card);
            else throw std::invalid_argument("a document must be \"passport\" or \"id_card\"");
        }
        papers.safe = safe != 0;
        request.identity = std::move(papers);
    }
    if (finance != 0)
    {
        dasmig::finance_request money;
        money.accounts.clear();
        while (next_part(accounts, part))
        {
            if (part == "current") money.accounts.push_back(dasmig::account_kind::current);
            else if (part == "savings") money.accounts.push_back(dasmig::account_kind::savings);
            else throw std::invalid_argument("an account must be \"current\" or \"savings\"");
        }
        while (next_part(cards, part))
        {
            if (part == "debit") money.cards.emplace_back(dasmig::card_kind::debit);
            else if (part == "credit") money.cards.emplace_back(dasmig::card_kind::credit);
            else if (part == "prepaid") money.cards.emplace_back(dasmig::card_kind::prepaid);
            else if (part == "any") money.cards.emplace_back(std::nullopt);
            else throw std::invalid_argument("a card must be \"debit\", \"credit\", \"prepaid\" or \"any\"");
        }
        if (card_numbers == "network_range") money.card_numbers = dasmig::card_source::network_range;
        else if (card_numbers == "test_number") money.card_numbers = dasmig::card_source::test_number;
        else if (card_numbers == "wrong_check") money.card_numbers = dasmig::card_source::wrong_check;
        else if (!card_numbers.empty()) throw std::invalid_argument("card numbers must be \"network_range\", \"test_number\" or \"wrong_check\"");
        if (*network != '\0') money.network = network;
        money.safe = safe != 0;
        request.finance = std::move(money);
    }
}

// The value whose name is `text`, from a list of names in the enum's order.
template <typename E, std::size_t N>
E named(std::string_view text, const std::array<std::string_view, N>& names, const char* what)
{
    for (std::size_t i = 0; i < N; ++i)
        if (names[i] == text) return static_cast<E>(i);
    throw std::invalid_argument(what);
}

// The home the C API's arguments describe; "" leaves a field to be drawn.
void add_home(dasmig::persona_request& request, int home, std::string_view type, std::string_view tenure,
              std::string_view area)
{
    if (home == 0) return;
    static constexpr std::array<std::string_view, 6> types{"detached",   "semi_detached", "flat_small",
                                                           "flat_large", "informal",      "other"};
    static constexpr std::array<std::string_view, 4> tenures{"owner_outright", "owner_mortgage", "tenant_market",
                                                             "tenant_reduced"};
    static constexpr std::array<std::string_view, 3> areas{"city", "town", "rural"};
    dasmig::property_request h;
    if (!type.empty())
        h.type = named<dasmig::dwelling_type>(type, types, "a home type must be detached, semi_detached, flat_small, "
                                                           "flat_large, informal or other");
    if (!tenure.empty())
        h.holding = named<dasmig::tenure>(tenure, tenures, "a tenure must be owner_outright, owner_mortgage, "
                                                           "tenant_market or tenant_reduced");
    if (!area.empty()) h.area = named<dasmig::settlement>(area, areas, "an area must be city, town or rural");
    request.home = h;
}

// The request the C API's string and number arguments describe; "" and -1
// leave a choice to the generator. `relations` is the family: "" for none,
// otherwise "up,down,flags" with the generations above and below (0 to 2) and
// the flags 1 siblings, 2 partners, 4 names, 8 stepparents, 16 home.
dasmig::persona_request make_request(const char* country, const char* sex, int age_min, int age_max, int traits,
                                     const char* reference_date, const char* relations, int identity,
                                     const char* documents, int finance, const char* accounts, const char* cards,
                                     const char* card_numbers, const char* network, int safe, int home,
                                     const char* home_type, const char* home_tenure, const char* home_area)
{
    dasmig::persona_request request;
    if (*country != '\0') request.country = country;
    const std::string_view sex_text{sex};
    if (sex_text == "female") request.person_sex = dasmig::sex::female;
    else if (sex_text == "male") request.person_sex = dasmig::sex::male;
    else if (!sex_text.empty()) throw std::invalid_argument("sex must be \"male\", \"female\" or empty");
    if (age_min >= 0 || age_max >= 0)
    {
        if (age_min < 0 || age_max < 0 || age_min > 120 || age_max > 120)
            throw std::invalid_argument("ages must both be between 0 and 120");
        request.ages = dasmig::age_range{.min = static_cast<std::uint8_t>(age_min),
                                         .max = static_cast<std::uint8_t>(age_max)};
    }
    if (traits >= 0) request.traits = static_cast<std::size_t>(traits);
    if (*reference_date != '\0') request.reference_date = parse_reference_date(reference_date);
    if (*relations != '\0')
    {
        int values[3]{};
        const std::string_view text{relations};
        const char* at = text.data();
        const char* const end = at + text.size();
        for (int i = 0; i < 3; ++i)
        {
            const auto result = std::from_chars(at, end, values[i]);
            if (result.ec != std::errc{} || (i < 2 ? result.ptr == end || *result.ptr != ',' : result.ptr != end))
                throw std::invalid_argument("relations must be \"up,down,flags\"");
            at = result.ptr + 1;
        }
        if (values[0] < 0 || values[0] > 2 || values[1] < 0 || values[1] > 2 || values[2] < 0 || values[2] > 31)
            throw std::invalid_argument("relations: up and down are 0 to 2, flags 0 to 31");
        dasmig::family_request family;
        family.up = values[0];
        family.down = values[1];
        family.siblings = (values[2] & 1) != 0;
        family.partners = (values[2] & 2) != 0;
        family.names = (values[2] & 4) != 0;
        family.stepparents = (values[2] & 8) != 0;
        family.home = (values[2] & 16) != 0;
        request.relations = family;
    }
    add_parts(request, identity, documents, finance, accounts, cards, card_numbers, network, safe);
    add_home(request, home, home_type, home_tenure, home_area);
    return request;
}

} // namespace

extern "C"
{

/// @brief Generate a persona.
/// @param country ISO 3166-1 alpha-2 code (any case), or "" to draw one of
///        the supported countries, weighted by population.
/// @param sex "male", "female", or "" to draw it.
/// @param age_min, age_max Inclusive age range in completed years (0-120),
///        or -1 for both to draw from the country's age pyramid.
/// @param traits Number of personality traits, or -1 for the default (3).
/// @param reference_date The date ages are computed at, YYYY-MM-DD, or "" for
///        birth-generator's default (2023-07-01). Never the clock.
/// @param relations "" for no family, or "up,down,flags" (see make_request).
/// @param identity Non-zero to add the national numbers and documents
///        (persona_request::identity), under `identity`.
/// @param documents The documents, "passport,id_card"; "" for numbers only.
/// @param finance Non-zero to add the accounts and cards
///        (persona_request::finance), under `finance`.
/// @param accounts The accounts, "current,savings"; "" for none.
/// @param cards The cards, "debit,credit,prepaid,any"; "" for none.
/// @param card_numbers "network_range", "test_number", "wrong_check", or "" for
///        the library's default.
/// @param network A card network ("visa"), or "" to draw it.
/// @param safe Non-zero for papers and a wallet by the libraries' safe methods.
/// @param home Non-zero to add the home (persona_request::home), under `home`
///        with `household_income`.
/// @param home_type, home_tenure, home_area The dwelling type, tenure and area
///        to force (property-generator's names), or "" to draw them.
/// @param seed 1 to 16 hexadecimal digits, or "" for a random seed (the
///        persona's signature has the seed that replays it).
EMSCRIPTEN_KEEPALIVE const char* dasmig_persona_generate(const char* country, const char* sex, int age_min,
                                                         int age_max, int traits, const char* reference_date,
                                                         const char* relations, int identity, const char* documents,
                                                         int finance, const char* accounts, const char* cards,
                                                         const char* card_numbers, const char* network, int safe,
                                                         int home, const char* home_type, const char* home_tenure,
                                                         const char* home_area, const char* seed)
{
    try
    {
        load_once();
        const auto request = make_request(country, sex, age_min, age_max, traits, reference_date, relations, identity,
                                          documents, finance, accounts, cards, card_numbers, network, safe, home,
                                          home_type, home_tenure, home_area);

        auto& generator = dasmig::psg::instance();
        return reply(*seed == '\0' ? generator.get_persona(request).to_json()
                                   : generator.get_persona(request, parse_seed(seed)).to_json());
    }
    catch (const std::exception& error) { return reply(error_json(error.what())); }
}

/// @brief Generate personas `first` to `first + count - 1` of a batch, as JSON
///        Lines (one persona per line, each line ending in a line feed).
///
/// Persona `i` is exactly `dasmig_persona_generate` with the seed
/// `core::item_seed(batch_seed, i)`, so a caller can split a batch into ranges
/// and join the results (the npm package does this across Web Workers).
/// Arguments as for dasmig_persona_generate, except:
/// @param seed The batch seed, 1 to 16 hexadecimal digits (required).
/// @param first Index of the first persona (a whole number below 2^53).
/// @param count Number of personas (0 or more).
EMSCRIPTEN_KEEPALIVE const char* dasmig_persona_generate_batch(const char* country, const char* sex, int age_min,
                                                               int age_max, int traits,
                                                               const char* reference_date, const char* relations,
                                                               int identity, const char* documents, int finance,
                                                               const char* accounts, const char* cards,
                                                               const char* card_numbers, const char* network, int safe,
                                                               int home, const char* home_type,
                                                               const char* home_tenure, const char* home_area,
                                                               const char* seed,
                                                               double first, int count)
{
    try
    {
        load_once();
        const auto request = make_request(country, sex, age_min, age_max, traits, reference_date, relations, identity,
                                          documents, finance, accounts, cards, card_numbers, network, safe, home,
                                          home_type, home_tenure, home_area);
        if (*seed == '\0') throw std::invalid_argument("a batch needs a seed");
        if (count < 0) throw std::invalid_argument("count must not be negative");
        if (!(first >= 0.0 && first <= 9007199254740991.0) || first != std::floor(first))
            throw std::invalid_argument("first must be a whole number from 0 to 2^53 - 1");

        std::string lines;
        const dasmig::core::batch_options options{.count = static_cast<std::uint64_t>(count),
                                                  .first = static_cast<std::uint64_t>(first),
                                                  .threads = 1};
        dasmig::psg::instance().for_each_persona(request, options, parse_seed(seed),
                                                 [&lines](std::uint64_t, const dasmig::persona& p) {
                                                     lines += p.to_json();
                                                     lines += '\n';
                                                 });
        return reply(std::move(lines));
    }
    catch (const std::exception& error) { return reply(error_json(error.what())); }
}

/// @brief What this module can generate: the supported countries, the trait
///        catalogue's size (the most traits a request can ask for), the
///        hash of the eight libraries' data, and id-generator's,
///        finance-generator's, and property-generator's and
///        relationship-generator's, combined in that order into the signature
///        of a persona with those parts.
EMSCRIPTEN_KEEPALIVE const char* dasmig_persona_info()
{
    try
    {
        load_once();
        dasmig::core::json_writer w;
        w.begin_object().key("countries").begin_array();
        for (const auto code : dasmig::psg::supported_countries()) w.value(code);
        w.end_array()
            .field("max_traits", dasmig::trtg::instance().trait_count())
            .field("data", hex(dasmig::psg::instance().data_hash()))
            .field("identity_data", hex(dasmig::idg::instance().data_hash()))
            .field("finance_data", hex(dasmig::fng::instance().data_hash()))
            .field("property_data", hex(dasmig::pty::instance().data_hash()))
            .field("relationship_data", hex(dasmig::relg::instance().data_hash()))
            .end_object();
        return reply(w.str());
    }
    catch (const std::exception& error) { return reply(error_json(error.what())); }
}

} // extern "C"
