#include "catch_amalgamated.hpp"
#include "../dasmig/personagen.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>

// The libraries' lite data is compiled in (the Makefile defines the
// DASMIG_*_EMBED macros), so the singletons have it.

using dasmig::persona_request;
using dasmig::psg;

namespace
{
constexpr std::uint64_t seed_a = 0x0123456789abcdefULL;

persona_request request_for(std::string country)
{
    persona_request r;
    r.country = std::move(country);
    return r;
}
} // namespace

// ---------------------------------------------------------------------------
// Loading
// ---------------------------------------------------------------------------

TEST_CASE("the singleton has the data of all eight libraries", "[loading]")
{
    REQUIRE(psg::instance().has_data());
    REQUIRE(psg::instance().data_hash() != 0);
}

TEST_CASE("supported_countries is sorted and every country generates", "[loading]")
{
    const auto countries = psg::supported_countries();
    REQUIRE(countries.size() == 79);
    REQUIRE(std::ranges::is_sorted(countries));
    const auto& gen = psg::instance();
    std::uint64_t seed = 1;
    for (const auto country : countries)
    {
        for (const auto person_sex : {dasmig::sex::female, dasmig::sex::male})
        {
            CAPTURE(country, person_sex);
            auto request = request_for(std::string{country});
            request.person_sex = person_sex;
            const auto p = gen.get_persona(request, seed++);
            REQUIRE(p.country_info.cca2 == country);
            REQUIRE_FALSE(p.name.empty());
            REQUIRE(p.city_info.country_code == country);
        }
    }
}

TEST_CASE("an independent set of library instances gives the same persona", "[loading]")
{
    dasmig::cntg country;
    dasmig::cg city;
    dasmig::bthg birth;
    dasmig::bdg biodata;
    dasmig::ng name;
    dasmig::intg interest;
    dasmig::ocg occupation;
    dasmig::trtg trait;
    dasmig::psg::sources own{&country, &city, &birth, &biodata, &name, &interest, &occupation, &trait};
    dasmig::psg gen{own};
    REQUIRE_FALSE(gen.has_data());
    REQUIRE_THROWS_AS(gen.get_persona(persona_request{}, seed_a), std::runtime_error);
    REQUIRE(gen.load(dasmig::dataset::lite));
    REQUIRE(gen.has_data());
    REQUIRE(gen.data_hash() == psg::instance().data_hash());
    REQUIRE(gen.get_persona(persona_request{}, seed_a).to_json()
            == psg::instance().get_persona(persona_request{}, seed_a).to_json());
    const dasmig::psg moved{std::move(gen)};
    REQUIRE(moved.get_persona(persona_request{}, seed_a).to_json()
            == psg::instance().get_persona(persona_request{}, seed_a).to_json());
}

// ---------------------------------------------------------------------------
// Determinism and replay
// ---------------------------------------------------------------------------

TEST_CASE("a seed replays a persona; other seeds differ", "[replay]")
{
    const auto& gen = psg::instance();
    const auto a = gen.get_persona(persona_request{}, seed_a);
    const auto b = gen.get_persona(persona_request{}, seed_a);
    REQUIRE(a.to_json() == b.to_json());
    REQUIRE(a.seed() == seed_a);
    REQUIRE(a.signature().seed == seed_a);
    REQUIRE(a.signature().data != 0);
    REQUIRE(a.signature().to_string() == b.signature().to_string());

    std::set<std::string> distinct;
    for (std::uint64_t s = 0; s < 50; ++s) distinct.insert(gen.get_persona(persona_request{}, s).to_json());
    REQUIRE(distinct.size() == 50);
}

TEST_CASE("an unseeded persona replays through its seed", "[replay]")
{
    dasmig::psg gen;
    const auto a = gen.get_persona(request_for("BR"));
    const auto b = gen.get_persona(request_for("BR"), a.seed());
    REQUIRE(a.to_json() == b.to_json());
    const auto c = gen.get_persona(request_for("BR"));
    REQUIRE(c.seed() != a.seed());
}

TEST_CASE("the request is part of the signature", "[replay]")
{
    const auto& gen = psg::instance();
    const auto plain = gen.get_persona(request_for("PT"), seed_a);
    auto other = request_for("PT");
    other.traits = 5;
    REQUIRE(gen.get_persona(other, seed_a).signature().data != plain.signature().data);
    REQUIRE(gen.get_persona(request_for("PT"), seed_a).signature().data == plain.signature().data);
}

TEST_CASE("forcing one choice leaves the unrelated draws alone", "[replay]")
{
    const auto& gen = psg::instance();
    persona_request female;
    female.person_sex = dasmig::sex::female;
    persona_request male;
    male.person_sex = dasmig::sex::male;
    for (std::uint64_t s = 1; s <= 20; ++s)
    {
        const auto f = gen.get_persona(female, s);
        const auto m = gen.get_persona(male, s);
        REQUIRE(f.person_sex() == dasmig::sex::female);
        REQUIRE(m.person_sex() == dasmig::sex::male);
        // The country, the city and the personality do not depend on the sex.
        REQUIRE(f.country_info.cca2 == m.country_info.cca2);
        REQUIRE(f.city_info.geonameid == m.city_info.geonameid);
        REQUIRE(f.traits.to_json() == m.traits.to_json());
    }
}

// ---------------------------------------------------------------------------
// The request
// ---------------------------------------------------------------------------

TEST_CASE("the country is honoured in any case", "[request]")
{
    const auto& gen = psg::instance();
    for (const char* code : {"JP", "jp", "Jp"})
    {
        const auto p = gen.get_persona(request_for(code), seed_a);
        REQUIRE(p.country_info.cca2 == "JP");
        REQUIRE(p.city_info.country_code == "JP");
    }
}

TEST_CASE("an unknown country is rejected", "[request]")
{
    REQUIRE_THROWS_AS(psg::instance().get_persona(request_for("ZZ"), seed_a), std::invalid_argument);
}

TEST_CASE("a country without a name culture is rejected, not given another culture's name", "[request]")
{
    // New Zealand has country, city and birth data but no name-generator
    // culture; name-generator's culture::any would draw any culture's name.
    REQUIRE_THROWS_WITH(psg::instance().get_persona(request_for("NZ"), seed_a),
                        Catch::Matchers::ContainsSubstring("no names for country NZ"));
}

TEST_CASE("ages set the age", "[request]")
{
    const auto& gen = psg::instance();
    persona_request r;
    r.ages = dasmig::age_range{.min = 30, .max = 35};
    for (std::uint64_t s = 1; s <= 40; ++s)
    {
        const auto age = gen.get_persona(r, s).age();
        REQUIRE(age >= 30);
        REQUIRE(age <= 35);
    }
    persona_request bad;
    bad.ages = dasmig::age_range{.min = 40, .max = 20};
    REQUIRE_THROWS_AS(gen.get_persona(bad, seed_a), std::invalid_argument);
}

TEST_CASE("the reference date moves the birth year, not the age", "[request]")
{
    const auto& gen = psg::instance();
    persona_request early;
    early.country = "DE";
    early.ages = dasmig::age_range{.min = 40, .max = 40};
    early.reference_date = std::chrono::year_month_day{std::chrono::year{2000}, std::chrono::January, std::chrono::day{1}};
    persona_request late = early;
    late.reference_date = std::chrono::year_month_day{std::chrono::year{2030}, std::chrono::January, std::chrono::day{1}};
    const auto a = gen.get_persona(early, seed_a);
    const auto b = gen.get_persona(late, seed_a);
    REQUIRE(a.age() == 40);
    REQUIRE(b.age() == 40);
    REQUIRE(b.birthday.year - a.birthday.year == 30);
}

TEST_CASE("the number of traits is honoured", "[request]")
{
    const auto& gen = psg::instance();
    for (std::size_t n : {0u, 1u, 3u, 6u})
    {
        persona_request r;
        r.traits = n;
        REQUIRE(gen.get_persona(r, seed_a).traits.traits.size() == n);
    }
}

// ---------------------------------------------------------------------------
// Correlations
// ---------------------------------------------------------------------------

TEST_CASE("the pipeline is consistent", "[pipeline]")
{
    const auto& gen = psg::instance();
    for (std::uint64_t s = 1; s <= 200; ++s)
    {
        const auto p = gen.get_persona(persona_request{}, s);
        CAPTURE(s, p.to_string());
        REQUIRE(p.city_info.country_code == p.country_info.cca2);
        REQUIRE(p.body.bio_sex == p.person_sex());
        REQUIRE(std::ranges::binary_search(psg::supported_countries(), std::string_view{p.country_info.cca2}));
    }
}

TEST_CASE("the age conditions the body and the job", "[pipeline]")
{
    const auto& gen = psg::instance();
    persona_request child;
    child.country = "US";
    child.ages = dasmig::age_range{.min = 6, .max = 10};
    persona_request adult;
    adult.country = "US";
    adult.ages = dasmig::age_range{.min = 30, .max = 50};
    double child_height = 0;
    double adult_height = 0;
    int children_in_work = 0;
    constexpr int n = 200;
    for (std::uint64_t s = 1; s <= n; ++s)
    {
        const auto c = gen.get_persona(child, s);
        const auto a = gen.get_persona(adult, s);
        child_height += c.body.height_cm;
        adult_height += a.body.height_cm;
        if (c.job.job) ++children_in_work;
    }
    REQUIRE(child_height / n < adult_height / n - 30.0);
    REQUIRE(children_in_work == 0);
}

// ---------------------------------------------------------------------------
// Known answers (rule 2): the same seed, request and data give these exact
// results on every compiler, standard library and CPU.
// ---------------------------------------------------------------------------

TEST_CASE("known answers for fixed seeds", "[kat]")
{
    const auto& gen = psg::instance();
    REQUIRE(gen.data_hash() == 0x3229071c54eee6bfULL);

    const auto one = gen.get_persona(persona_request{}, 1);
    REQUIRE(one.to_string() == "Nigel Phillips, 13, male, Gosport, United Kingdom, student");
    REQUIRE(one.signature().to_string() == "0000000000000001-1ecd290e3d20a202");
    const auto two = gen.get_persona(persona_request{}, 2);
    REQUIRE(two.to_string() == "Sadaf Esmaeili, 16, female, Mashhad, Iran, student");
    REQUIRE(two.signature().to_string() == "0000000000000002-1ecd290e3d20a202");

    persona_request brazil;
    brazil.country = "BR";
    brazil.person_sex = dasmig::sex::female;
    brazil.ages = dasmig::age_range{.min = 25, .max = 35};
    brazil.traits = 5;
    const auto three = gen.get_persona(brazil, 3);
    REQUIRE(three.to_string() == "Jéssica Tomaz Cardozo, 34, female, São Paulo, Brazil, Farm labourer, BRL 12,900 a year");
    REQUIRE(three.signature().to_string() == "0000000000000003-98a218df38f7e594");

    persona_request japan;
    japan.country = "jp";
    japan.reference_date = std::chrono::year_month_day{std::chrono::year{2030}, std::chrono::March, std::chrono::day{9}};
    const auto four = gen.get_persona(japan, 4);
    REQUIRE(four.to_string() == "Iwasaki Yasushi, 70, male, Edogawe, Japan, Shop assistant");
    REQUIRE(four.signature().to_string() == "0000000000000004-bb5404c1045abbc6");

    // A hash of 2,000 JSON documents (not 10,000 as in the single-purpose
    // libraries: a persona runs eight of them, and each is covered there).
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (std::uint64_t s = 0; s < 2000; ++s)
    {
        for (const char c : gen.get_persona(persona_request{}, s).to_json())
        {
            h ^= static_cast<unsigned char>(c);
            h *= 0x100000001b3ULL;
        }
    }
    REQUIRE(h == 0xb68320266eb85006ULL);
}

// ---------------------------------------------------------------------------
// Output
// ---------------------------------------------------------------------------

TEST_CASE("the JSON has every part", "[output]")
{
    const auto json = psg::instance().get_persona(request_for("DE"), seed_a).to_json();
    for (const char* key : {"\"name\":", "\"sex\":", "\"age\":", "\"admin1\":", "\"country\":{", "\"city\":{",
                            "\"birth\":{", "\"biodata\":{", "\"personality\":{", "\"interest\":{",
                            "\"occupation\":{", "\"signature\":{"})
    {
        CAPTURE(key);
        REQUIRE(json.find(key) != std::string::npos);
    }
    REQUIRE(json.front() == '{');
    REQUIRE(json.back() == '}');
    REQUIRE(std::ranges::count(json, '{') == std::ranges::count(json, '}'));
}

TEST_CASE("a persona converts to a string and streams", "[output]")
{
    const auto p = psg::instance().get_persona(request_for("FR"), seed_a);
    const std::string text = p;
    REQUIRE(text == p.to_string());
    REQUIRE(text.find(p.name) == 0);
    REQUIRE(text.find("France") != std::string::npos);
    std::ostringstream os;
    os << p;
    REQUIRE(os.str() == text);
}

// ---------------------------------------------------------------------------
// Batch generation
// ---------------------------------------------------------------------------

TEST_CASE("persona batch: items equal the single calls for any thread count", "[batch]")
{
    auto& gen = psg::instance();
    const auto request = persona_request{};
    const auto batch = gen.get_persona_batch(request, {.count = 200, .threads = 1}, 77);
    REQUIRE(batch.size() == 200);
    for (std::uint64_t i = 0; i < batch.size(); ++i)
    {
        REQUIRE(batch[i].to_json() == gen.get_persona(request, dasmig::core::item_seed(77, i)).to_json());
    }

    // A range is computed directly, on any number of threads.
    const auto tail = gen.get_persona_batch(request, {.count = 100, .first = 100, .threads = 4}, 77);
    REQUIRE(tail.size() == 100);
    for (std::size_t i = 0; i < tail.size(); ++i)
    {
        REQUIRE(tail[i].to_json() == batch[100 + i].to_json());
    }
    for (const std::size_t threads : {2U, 7U, 0U})
    {
        const auto other = gen.get_persona_batch(request, {.count = 200, .threads = threads}, 77);
        REQUIRE(other.size() == batch.size());
        for (std::size_t i = 0; i < other.size(); ++i)
        {
            REQUIRE(other[i].to_json() == batch[i].to_json());
        }
    }

    // Without a seed the batch still has the requested size.
    REQUIRE(gen.get_persona_batch(request, {.count = 5}).size() == 5);
}

TEST_CASE("persona batch: streaming gives the same items in order", "[batch]")
{
    auto& gen = psg::instance();
    const auto request = persona_request{};
    const auto batch = gen.get_persona_batch(request, {.count = 300, .first = 10, .threads = 3}, 5);
    std::uint64_t next = 10;
    std::size_t seen = 0;
    gen.for_each_persona(request, {.count = 300, .first = 10, .threads = 3}, 5,
                        [&](std::uint64_t index, const dasmig::persona& item)
                        {
                            REQUIRE(index == next);
                            REQUIRE(item.to_json() == batch[seen].to_json());
                            ++next;
                            ++seen;
                        });
    REQUIRE(seen == batch.size());
}

TEST_CASE("persona batch: known answer of 10,000 items", "[batch][kat]")
{
    auto& gen = psg::instance();
    const auto request = persona_request{};
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    gen.for_each_persona(request, {.count = 10000, .threads = 4}, 1,
                        [&](std::uint64_t, const dasmig::persona& item)
                        { hash = dasmig::core::hash_combine(hash, dasmig::core::fnv1a64(item.to_json())); });
    REQUIRE(hash == 0x22be6a56b22800efULL);
}

// ---------------------------------------------------------------------------
// Relations
// ---------------------------------------------------------------------------

TEST_CASE("relations are off by default and then change nothing", "[relations]")
{
    const auto& gen = psg::instance();
    const auto plain = gen.get_persona(request_for("BR"), seed_a);
    REQUIRE_FALSE(plain.relatives.has_value());
    REQUIRE(plain.to_json().find("\"relatives\"") == std::string::npos);
}

TEST_CASE("relations add the family of the persona and nothing else", "[relations]")
{
    const auto& gen = psg::instance();
    std::uint64_t seed = 1;
    for (const auto* country : {"BR", "ES", "JP", "US", "RU", "IS"})
    {
        CAPTURE(country);
        auto request = request_for(country);
        const auto plain = gen.get_persona(request, seed);
        request.relations = dasmig::family_request{};
        const auto with = gen.get_persona(request, seed);

        // Every other part, and the signature, are the same.
        REQUIRE(with.name == plain.name);
        REQUIRE(with.age() == plain.age());
        REQUIRE(with.city_info.name == plain.city_info.name);
        REQUIRE(with.job.to_json() == plain.job.to_json());
        REQUIRE(with.signature().seed == plain.signature().seed);
        REQUIRE(with.signature().data == plain.signature().data);

        REQUIRE(with.relatives.has_value());
        const auto& anchor = with.relatives->anchor();
        REQUIRE(with.relatives->country_code == country);
        REQUIRE(anchor.bio_sex == with.person_sex());
        REQUIRE(anchor.age == with.age());
        REQUIRE(anchor.name == with.name);
        REQUIRE(static_cast<int>(anchor.birth.year()) == with.birthday.year);
        REQUIRE(with.to_json().find("\"relatives\":{") != std::string::npos);
        ++seed;
    }
}

TEST_CASE("relations follow the request and replay from the seed", "[relations]")
{
    const auto& gen = psg::instance();
    auto request = request_for("PT");
    request.relations = dasmig::family_request{.up = 0, .down = 0, .partners = false};
    const auto a = gen.get_persona(request, seed_a);
    REQUIRE(a.relatives->members.size() == 1);
    REQUIRE(gen.get_persona(request, seed_a).to_json() == a.to_json());

    request.relations = dasmig::family_request{.up = 1, .down = 0};
    const auto b = gen.get_persona(request, seed_a);
    REQUIRE(b.relatives->parents().size() == 2);
}

TEST_CASE("relations: stepparents and the household pass through", "[relations]")
{
    const auto& gen = psg::instance();
    auto request = request_for("BR");
    request.relations = dasmig::family_request{.stepparents = true, .home = true};
    const auto with = gen.get_persona(request, seed_a);
    REQUIRE(with.relatives.has_value());
    REQUIRE(with.relatives->home.has_value());
    REQUIRE(with.relatives->home->members.front() == with.relatives->anchor().id);
    REQUIRE(with.to_json().find("\"household\"") != std::string::npos);
    REQUIRE(gen.get_persona(request, seed_a).to_json() == with.to_json());
}

TEST_CASE("relations: a persona batch with them is the same for any thread count", "[relations][batch]")
{
    const auto& gen = psg::instance();
    auto request = persona_request{};
    request.relations = dasmig::family_request{};
    const auto one = gen.get_persona_batch(request, {.count = 200, .threads = 1}, 7);
    const auto many = gen.get_persona_batch(request, {.count = 200, .threads = 4}, 7);
    REQUIRE(one.size() == many.size());
    for (std::size_t i = 0; i < one.size(); ++i) REQUIRE(one[i].to_json() == many[i].to_json());
}

TEST_CASE("relations: known answer of 1,000 items", "[relations][kat]")
{
    const auto& gen = psg::instance();
    auto request = persona_request{};
    request.relations = dasmig::family_request{};
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    gen.for_each_persona(request, {.count = 1000, .threads = 4}, 1,
                         [&](std::uint64_t, const dasmig::persona& item)
                         { hash = dasmig::core::hash_combine(hash, dasmig::core::fnv1a64(item.to_json())); });
    REQUIRE(hash == 0xbcdc063f3a3c89b1ULL);
}

// ---------------------------------------------------------------------------
// Papers and wallet
// ---------------------------------------------------------------------------

namespace
{
std::chrono::year_month_day born_on(const dasmig::persona& p)
{
    return std::chrono::year{p.birthday.year} / std::chrono::month{p.birthday.month} /
           std::chrono::day{p.birthday.day};
}

// The JSON up to the signature, which the parts change.
std::string without_signature(const std::string& json)
{
    return json.substr(0, json.rfind(",\"signature\":"));
}

dasmig::identity_request with_documents()
{
    dasmig::identity_request r;
    r.documents = {dasmig::id_document_kind::passport};
    return r;
}

dasmig::finance_request with_cards()
{
    dasmig::finance_request r;
    r.cards = {std::nullopt};
    r.card_numbers = dasmig::card_source::test_number;
    return r;
}
} // namespace

TEST_CASE("papers and a wallet are off by default and then change nothing", "[identity][finance]")
{
    const auto& gen = psg::instance();
    const auto plain = gen.get_persona(request_for("BR"), seed_a);
    REQUIRE_FALSE(plain.papers.has_value());
    REQUIRE_FALSE(plain.wallet.has_value());
    REQUIRE(plain.to_json().find("\"identity\"") == std::string::npos);
    REQUIRE(plain.to_json().find("\"finance\"") == std::string::npos);
}

TEST_CASE("the papers are id-generator's for this persona", "[identity]")
{
    const auto& gen = psg::instance();
    const auto& ids = dasmig::idg::instance();
    for (std::uint64_t seed = 0; seed < 200; ++seed)
    {
        auto request = request_for("IT");
        request.identity = with_documents();
        const auto p = gen.get_persona(request, seed);
        REQUIRE(p.papers.has_value());
        REQUIRE(p.papers->country_code == "IT");
        REQUIRE(p.papers->birth_date == born_on(p));
        REQUIRE(p.papers->bio_sex == p.person_sex());
        for (const auto& n : p.papers->numbers) REQUIRE(ids.is_valid(n.scheme, n.value));
        REQUIRE(p.signature().data ==
                dasmig::core::hash_combine(gen.get_persona(request_for("IT"), seed).signature().data, ids.data_hash()));
    }
}

TEST_CASE("the city decides a number's place code", "[identity]")
{
    const auto& gen = psg::instance();
    // Italy's code is the comune's: the same city gives the same code.
    std::map<std::uint32_t, std::set<std::string>> codes;
    auto request = request_for("IT");
    request.identity = dasmig::identity_request{};
    request.ages = dasmig::age_range{.min = 30, .max = 60};
    for (std::uint64_t seed = 0; seed < 600; ++seed)
    {
        const auto p = gen.get_persona(request, seed);
        if (p.papers->numbers.empty()) continue;
        const auto& code = p.papers->numbers[0].place_code;
        if (!code.empty()) codes[p.city_info.geonameid].insert(code);
    }
    REQUIRE(codes.size() > 20);
    for (const auto& [city, seen] : codes) REQUIRE(seen.size() == 1);
}

TEST_CASE("the name reaches the documents", "[identity]")
{
    const auto& gen = psg::instance();
    auto mexico = request_for("MX");
    mexico.identity = with_documents();
    mexico.ages = dasmig::age_range{.min = 30, .max = 60};
    int two = 0;
    for (std::uint64_t seed = 0; seed < 200 && two < 20; ++seed)
    {
        const auto p = gen.get_persona(mexico, seed);
        if (p.papers->documents.empty()) continue;
        const auto& line = p.papers->documents[0].mrz.front();
        const auto surnames = line.substr(5, line.find("<<") - 5);
        if (surnames.find('<') != std::string::npos) ++two;
    }
    REQUIRE(two >= 20);

    auto iceland = request_for("IS");
    iceland.identity = with_documents();
    iceland.ages = dasmig::age_range{.min = 30, .max = 60};
    const auto p = gen.get_persona(iceland, 3);
    REQUIRE_FALSE(p.papers->documents.at(0).mrz.empty());
}

TEST_CASE("the wallet is finance-generator's and the survey decides who holds what", "[finance]")
{
    const auto& gen = psg::instance();
    const auto& banks = dasmig::fng::instance();
    const auto share_with_account = [&](const char* country) {
        auto request = request_for(country);
        request.ages = dasmig::age_range{.min = 25, .max = 60};
        request.finance = dasmig::finance_request{};
        int held = 0;
        for (std::uint64_t seed = 0; seed < 400; ++seed)
        {
            const auto p = gen.get_persona(request, seed);
            REQUIRE(p.wallet->birth_date == born_on(p));
            for (const auto& a : p.wallet->accounts) REQUIRE(banks.is_valid(a.scheme, a.iban.empty() ? a.account_number : a.iban));
            held += p.wallet->accounts.empty() ? 0 : 1;
        }
        return held;
    };
    // Far more Danes than Yemenis hold an account.
    REQUIRE(share_with_account("DK") > 360);
    REQUIRE(share_with_account("YE") < 100);

    // `asked` in the request does not change that.
    auto asked = request_for("YE");
    asked.finance = dasmig::finance_request{};
    asked.finance->holding = dasmig::ownership::asked;
    auto left = request_for("YE");
    left.finance = dasmig::finance_request{};
    REQUIRE(gen.get_persona(asked, 3).to_json() == gen.get_persona(left, 3).to_json());
}

TEST_CASE("papers and a wallet change nothing else", "[identity][finance][replay]")
{
    const auto& gen = psg::instance();
    for (std::uint64_t seed = 0; seed < 200; ++seed)
    {
        persona_request plain;
        const auto p = gen.get_persona(plain, seed);
        auto request = plain;
        request.identity = with_documents();
        request.finance = with_cards();
        auto with = gen.get_persona(request, seed);
        REQUIRE(with.papers.has_value());
        REQUIRE(with.wallet.has_value());
        with.papers.reset();
        with.wallet.reset();
        REQUIRE(without_signature(with.to_json()) == without_signature(p.to_json()));
    }
}

TEST_CASE("every supported country can have papers and a wallet", "[identity][finance]")
{
    const auto& gen = psg::instance();
    for (const auto code : psg::supported_countries())
    {
        auto request = request_for(std::string{code});
        request.ages = dasmig::age_range{.min = 30, .max = 60};
        request.identity = with_documents();
        request.finance = with_cards();
        const auto p = gen.get_persona(request, 21);
        REQUIRE_FALSE(p.papers->documents.empty());
    }
}

TEST_CASE("papers and a wallet in JSON come after the rest", "[identity][finance][output]")
{
    const auto& gen = psg::instance();
    auto request = request_for("FR");
    request.identity = with_documents();
    request.finance = with_cards();
    request.relations = dasmig::family_request{};
    const auto json = gen.get_persona(request, 7).to_json();
    const auto family = json.find("\"relatives\":{");
    const auto identity = json.find("\"identity\":{");
    const auto finance = json.find("\"finance\":{");
    REQUIRE(family < identity);
    REQUIRE(identity < finance);
    REQUIRE(finance < json.rfind("\"signature\":"));
}

TEST_CASE("the parts come from the generator's own instances", "[identity][finance][loading]")
{
    dasmig::idg no_ids;
    dasmig::fng no_banks;
    const psg own{psg::sources{.identity = &no_ids, .finance = &no_banks}};
    auto with = request_for("FR");
    with.identity = dasmig::identity_request{};
    REQUIRE_THROWS(own.get_persona(with, 1));
    auto money = request_for("FR");
    money.finance = dasmig::finance_request{};
    REQUIRE_THROWS(own.get_persona(money, 1));
    REQUIRE_NOTHROW(own.get_persona(request_for("FR"), 1));
}

TEST_CASE("known answers with papers and a wallet", "[identity][finance][kat]")
{
    const auto& gen = psg::instance();
    auto request = request_for("BR");
    request.person_sex = dasmig::sex::female;
    request.ages = dasmig::age_range{.min = 30, .max = 40};
    request.identity = with_documents();
    request.finance = with_cards();
    const auto p = gen.get_persona(request, seed_a);
    REQUIRE(p.name == "Yasmin Tavares");
    REQUIRE(p.papers->numbers.at(0).formatted == "754.149.568-98");
    REQUIRE(p.papers->documents.at(0).mrz.at(0) == "P<BRATAVARES<<YASMIN<<<<<<<<<<<<<<<<<<<<<<<<");

    std::uint64_t hash = 0xcbf29ce484222325ULL;
    persona_request drawn;
    drawn.identity = with_documents();
    drawn.finance = with_cards();
    for (std::uint64_t s = 0; s < 500; ++s)
    {
        hash = dasmig::core::hash_combine(hash, dasmig::core::fnv1a64(gen.get_persona(drawn, s).to_json()));
    }
    INFO(std::format("{:016x}", hash));
    REQUIRE(hash == 0x8399413cc7658b44ULL);
}

// ---------------------------------------------------------------------------
// Home (property-generator)
// ---------------------------------------------------------------------------

TEST_CASE("a home is off by default and then changes nothing", "[home]")
{
    const auto& gen = psg::instance();
    const auto plain = gen.get_persona(request_for("DE"), seed_a);
    REQUIRE_FALSE(plain.home.has_value());
    REQUIRE_FALSE(plain.household_income.has_value());
    REQUIRE(plain.to_json().find("\"home\"") == std::string::npos);
    for (std::uint64_t seed = 0; seed < 200; ++seed)
    {
        persona_request request;
        const auto p = gen.get_persona(request, seed);
        request.home = dasmig::property_request{};
        auto with = gen.get_persona(request, seed);
        REQUIRE(with.home.has_value());
        REQUIRE(with.home->country_code == p.country_info.cca2);
        with.home.reset();
        REQUIRE(without_signature(with.to_json()) == without_signature(p.to_json()));
        REQUIRE(with.signature().data != p.signature().data);
    }
}

TEST_CASE("the home's household is the family's", "[home][relations]")
{
    const auto& gen = psg::instance();
    for (std::uint64_t seed = 0; seed < 300; ++seed)
    {
        auto request = request_for("FR");
        request.relations = dasmig::family_request{};
        request.home = dasmig::property_request{};
        const auto p = gen.get_persona(request, seed);
        REQUIRE(p.relatives->home.has_value()); // asked for by the home
        const auto& household = *p.relatives->home;
        const auto type = std::string{dasmig::household_type_name(household.type)};
        if (household.type == dasmig::household_type::other) continue; // its size is drawn
        // The size is kept; the type too, unless the country's table has no
        // household of that type and size.
        REQUIRE(p.home->household_size == household.members.size());
    }
}

TEST_CASE("the household's income is everyone's earnings", "[home]")
{
    const auto& gen = psg::instance();
    int alone = 0;
    int more = 0;
    for (std::uint64_t seed = 0; seed < 400; ++seed)
    {
        auto request = request_for("US");
        request.ages = dasmig::age_range{.min = 25, .max = 60};
        request.relations = dasmig::family_request{};
        request.home = dasmig::property_request{};
        const auto p = gen.get_persona(request, seed);
        const auto own = p.job.income ? p.job.income->minor : 0;
        const auto total = p.household_income ? p.household_income->minor : 0;
        REQUIRE(total >= own);
        REQUIRE(p.home->monthly_cost.has_value() == p.household_income.has_value());
        if (p.household_income) REQUIRE(p.household_income->currency == "USD");
        if (p.relatives->home->members.size() == 1)
        {
            REQUIRE(total == own);
            ++alone;
        }
        else if (total > own)
        {
            ++more;
        }
    }
    REQUIRE(alone > 20);
    REQUIRE(more > 100); // partners and adult children earn too
}

TEST_CASE("a child's home is its parents'", "[home]")
{
    const auto& gen = psg::instance();
    for (std::uint64_t seed = 0; seed < 100; ++seed)
    {
        auto request = request_for("IT");
        request.ages = dasmig::age_range{.min = 2, .max = 12};
        request.home = dasmig::property_request{};
        const auto p = gen.get_persona(request, seed);
        REQUIRE(p.home.has_value());
        REQUIRE(p.home->household_size >= 2);
        REQUIRE_FALSE(p.job.income.has_value());
    }
}

TEST_CASE("the home request's own fields pass through", "[home]")
{
    const auto& gen = psg::instance();
    auto request = request_for("ES");
    request.home = dasmig::property_request{};
    request.home->type = dasmig::dwelling_type::flat_large;
    request.home->holding = dasmig::tenure::tenant_market;
    request.home->country = "US";          // replaced by the persona's
    request.home->anchor_age = 99;         // replaced too
    for (std::uint64_t seed = 0; seed < 50; ++seed)
    {
        const auto p = gen.get_persona(request, seed);
        REQUIRE(p.home->country_code == "ES");
        REQUIRE(p.home->type == dasmig::dwelling_type::flat_large);
        REQUIRE(p.home->holding == dasmig::tenure::tenant_market);
        REQUIRE(p.home->in_building());
        REQUIRE(gen.get_persona(request, seed).to_json() == p.to_json());
    }
}

TEST_CASE("every supported country can have a home", "[home]")
{
    const auto& gen = psg::instance();
    for (const auto code : psg::supported_countries())
    {
        auto request = request_for(std::string{code});
        request.home = dasmig::property_request{};
        request.relations = dasmig::family_request{.up = 1, .down = 2};
        for (std::uint64_t seed = 0; seed < 40; ++seed)
        {
            const auto p = gen.get_persona(request, seed);
            REQUIRE(p.home->country_code == code);
        }
    }
}

TEST_CASE("a home in JSON comes after the rest", "[home][output]")
{
    const auto& gen = psg::instance();
    auto request = request_for("GB");
    request.finance = with_cards();
    request.home = dasmig::property_request{};
    const auto json = gen.get_persona(request, 7).to_json();
    const auto finance = json.find("\"finance\":{");
    const auto home = json.find("\"home\":{");
    const auto income = json.find("\"household_income\":");
    REQUIRE(finance < home);
    REQUIRE(home < income);
    REQUIRE(income < json.rfind("\"signature\":"));
}

TEST_CASE("the home comes from the generator's own instance", "[home][loading]")
{
    dasmig::pty no_homes;
    const psg own{psg::sources{.property = &no_homes}};
    auto with = request_for("FR");
    with.home = dasmig::property_request{};
    REQUIRE_THROWS(own.get_persona(with, 1));
    REQUIRE_NOTHROW(own.get_persona(request_for("FR"), 1));
}

TEST_CASE("a home: a persona batch with one is the same for any thread count", "[home][batch]")
{
    const auto& gen = psg::instance();
    auto request = persona_request{};
    request.home = dasmig::property_request{};
    const auto one = gen.get_persona_batch(request, {.count = 200, .threads = 1}, 7);
    const auto many = gen.get_persona_batch(request, {.count = 200, .threads = 4}, 7);
    REQUIRE(one.size() == many.size());
    for (std::size_t i = 0; i < one.size(); ++i) REQUIRE(one[i].to_json() == many[i].to_json());
}

TEST_CASE("known answers with a home", "[home][kat]")
{
    const auto& gen = psg::instance();
    auto request = request_for("BR");
    request.person_sex = dasmig::sex::female;
    request.ages = dasmig::age_range{.min = 30, .max = 40};
    request.home = dasmig::property_request{};
    // The first seed whose household earns, so the home has a cost.
    std::uint64_t seed = 0;
    while (!gen.get_persona(request, seed).household_income) ++seed;
    const auto p = gen.get_persona(request, seed);
    INFO(p.to_json());
    REQUIRE(seed == 0);
    REQUIRE(p.home->to_string() == "BR, city, detached house, owner without a mortgage, 7 rooms (3 bedrooms), 173 m², built 2010, 176.47 BRL a month");
    REQUIRE(p.household_income->decimal() == "32500.00");

    std::uint64_t hash = 0xcbf29ce484222325ULL;
    persona_request drawn;
    drawn.home = dasmig::property_request{};
    for (std::uint64_t s = 0; s < 500; ++s)
    {
        hash = dasmig::core::hash_combine(hash, dasmig::core::fnv1a64(gen.get_persona(drawn, s).to_json()));
    }
    INFO(std::format("{:016x}", hash));
    REQUIRE(hash == 0xb225d3b9a2172226ULL);
}

// ---------------------------------------------------------------------------
// Employer
// ---------------------------------------------------------------------------

TEST_CASE("an employer is off by default and then changes nothing", "[employer]")
{
    const auto& gen = psg::instance();
    const auto plain = gen.get_persona(request_for("DE"), seed_a);
    REQUIRE_FALSE(plain.employer.has_value());
    REQUIRE(plain.to_json().find("\"employer\"") == std::string::npos);
    std::size_t employed = 0, not_employed = 0;
    for (std::uint64_t seed = 0; seed < 300; ++seed)
    {
        persona_request request;
        const auto p = gen.get_persona(request, seed);
        request.employer = dasmig::company_request{};
        auto with = gen.get_persona(request, seed);
        REQUIRE(with.employer.has_value() == (p.job.status == dasmig::employment_status::employed));
        // Asked for, the key is there for everyone: null when not employed.
        REQUIRE(with.to_json().find("\"employer\":") != std::string::npos);
        if (with.employer) ++employed;
        else
        {
            ++not_employed;
            REQUIRE(with.to_json().find("\"employer\":null") != std::string::npos);
        }
        // Nothing else moved: without the employer's key it is the persona without the request.
        with.employer.reset();
        auto json = without_signature(with.to_json());
        constexpr std::string_view null_employer = ",\"employer\":null";
        const auto key = json.find(null_employer);
        REQUIRE(key != std::string::npos);
        json.erase(key, null_employer.size());
        REQUIRE(json == without_signature(p.to_json()));
    }
    REQUIRE(employed > 100);
    REQUIRE(not_employed > 20);
}

TEST_CASE("the employer is in the persona's country and takes on its occupation", "[employer]")
{
    const auto& gen = psg::instance();
    for (const auto code : psg::supported_countries())
    {
        auto request = request_for(std::string{code});
        request.employer = dasmig::company_request{};
        request.employer->country = "US"; // replaced by the persona's
        request.employer->weight = dasmig::company_weight::enterprises; // replaced too
        for (std::uint64_t seed = 0; seed < 40; ++seed)
        {
            CAPTURE(code, seed);
            const auto p = gen.get_persona(request, seed);
            if (!p.employer) continue;
            REQUIRE(p.employer->country_code == code);
            REQUIRE(p.job.job.has_value());
            REQUIRE_FALSE(p.employer->name.empty());
        }
    }
}

TEST_CASE("an employer is drawn by its staff", "[employer]")
{
    const auto& gen = psg::instance();
    auto request = request_for("DE");
    request.employer = dasmig::company_request{};
    std::size_t large = 0, jobs = 0;
    for (std::uint64_t seed = 0; seed < 600; ++seed)
    {
        const auto p = gen.get_persona(request, seed);
        if (!p.employer) continue;
        ++jobs;
        if (p.employer->employees >= 50) ++large;
    }
    // By enterprises most firms are micro; by employment about half of the staff
    // work in firms of 50 or more.
    REQUIRE(jobs > 200);
    REQUIRE(large * 5 > jobs);
}

TEST_CASE("the employer request's own fields pass through", "[employer]")
{
    const auto& gen = psg::instance();
    auto request = request_for("ES");
    request.employer = dasmig::company_request{};
    request.employer->incorporated_only = true;
    request.employer->safe = true;
    request.employer->founder_surname = "Hoffmann"; // used as given
    for (std::uint64_t seed = 0; seed < 60; ++seed)
    {
        const auto p = gen.get_persona(request, seed);
        if (!p.employer) continue;
        REQUIRE(p.employer->legal_form_group != "sole_proprietorship");
        REQUIRE(p.employer->domain.ends_with(".example"));
    }
}

TEST_CASE("an employer in JSON comes after the rest", "[employer][output]")
{
    const auto& gen = psg::instance();
    auto request = request_for("GB");
    request.home = dasmig::property_request{};
    request.employer = dasmig::company_request{};
    std::uint64_t seed = 0;
    while (!gen.get_persona(request, seed).employer) ++seed;
    const auto json = gen.get_persona(request, seed).to_json();
    const auto income = json.find("\"household_income\":");
    const auto employer = json.find("\"employer\":{");
    REQUIRE(income < employer);
    REQUIRE(employer < json.rfind("\"signature\":"));
}

TEST_CASE("the employer comes from the generator's own instance", "[employer][loading]")
{
    dasmig::cpg no_companies;
    const psg own{psg::sources{.company = &no_companies}};
    auto with = request_for("FR");
    with.employer = dasmig::company_request{};
    std::uint64_t seed = 0;
    while (psg::instance().get_persona(with, seed).employer == std::nullopt) ++seed;
    REQUIRE_THROWS(own.get_persona(with, seed));
    REQUIRE_NOTHROW(own.get_persona(request_for("FR"), seed));
}

TEST_CASE("an employer: a persona batch with one is the same for any thread count", "[employer][batch]")
{
    const auto& gen = psg::instance();
    auto request = persona_request{};
    request.employer = dasmig::company_request{};
    const auto one = gen.get_persona_batch(request, {.count = 200, .threads = 1}, 7);
    const auto many = gen.get_persona_batch(request, {.count = 200, .threads = 4}, 7);
    REQUIRE(one.size() == many.size());
    for (std::size_t i = 0; i < one.size(); ++i) REQUIRE(one[i].to_json() == many[i].to_json());
}

TEST_CASE("known answers with an employer", "[employer][kat]")
{
    const auto& gen = psg::instance();
    auto request = request_for("BR");
    request.person_sex = dasmig::sex::female;
    request.ages = dasmig::age_range{.min = 30, .max = 40};
    request.employer = dasmig::company_request{};
    request.employer->safe = true;
    std::uint64_t seed = 0;
    while (!gen.get_persona(request, seed).employer) ++seed;
    const auto p = gen.get_persona(request, seed);
    INFO(p.to_json());
    REQUIRE(seed == 0);
    REQUIRE(p.employer->to_string() == "BR, Clube do Norte, sports activities and amusement and recreation activities (93), 2 persons, founded 2020-09-20");

    std::uint64_t hash = 0xcbf29ce484222325ULL;
    persona_request drawn;
    drawn.employer = dasmig::company_request{};
    for (std::uint64_t s = 0; s < 500; ++s)
    {
        hash = dasmig::core::hash_combine(hash, dasmig::core::fnv1a64(gen.get_persona(drawn, s).to_json()));
    }
    INFO(std::format("{:016x}", hash));
    REQUIRE(hash == 0x0a2d0979aa9ce719ULL);
}
