// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Diego Dasso Migotto

#ifndef DASMIG_PERSONAGEN_HPP
#define DASMIG_PERSONAGEN_HPP

#include "core.hpp"
#include <algorithm>
#include <any>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <dasmig/biodatagen.hpp>
#include <dasmig/birthgen.hpp>
#include <dasmig/citygen.hpp>
#include <dasmig/countrygen.hpp>
#include <dasmig/entitygen.hpp>
#include <dasmig/interestgen.hpp>
#include <dasmig/namegen.hpp>
#include <dasmig/occupationgen.hpp>
#include <dasmig/traitgen.hpp>

/// @file personagen.hpp
/// @brief persona-generator library — complete characters composed from the other dasmig generators for C++23.
/// @author Diego Dasso Migotto (diegomigotto at hotmail dot com)
/// @see See doc/usage.md for the narrative tutorial.

DASMIG_CORE_REQUIRE(1, 2);

namespace dasmig
{

/// @brief What a persona call asks for.
///
/// Every generation call takes one; `persona_request{}` asks for the
/// defaults. Choices are never stored on the generator, so one call cannot
/// change the next. Every field is optional: what is left out is drawn, and
/// what is given conditions the draws that depend on it (the country sets the
/// city, the name culture, the body and the job; the age and sex set the rest).
struct persona_request
{
    /// @brief ISO 3166-1 alpha-2 code (any case). Unset draws one of
    ///        psg::supported_countries(), weighted by population.
    std::optional<std::string> country{}; // NOLINT(readability-redundant-member-init)

    /// @brief Biological sex; unset draws it from the birth.
    std::optional<sex> person_sex{}; // NOLINT(readability-redundant-member-init)

    /// @brief Inclusive age range in completed years; unset draws from the
    ///        country's age pyramid.
    std::optional<age_range> ages{}; // NOLINT(readability-redundant-member-init)

    /// @brief The date ages are computed at. The default is birth-generator's
    ///        (2023-07-01); the library never reads the clock.
    std::chrono::year_month_day reference_date{birth_request::default_reference_date};

    /// @brief How many personality traits to draw.
    std::size_t traits{3};
};

/// @brief Return type for persona generation.
///
/// Holds the typed result of every library in the pipeline. Supports
/// implicit conversion to std::string (a one-line summary), streaming via
/// operator<< and JSON output via to_json().
class persona
{
  public:
    std::string name;       ///< Full name (UTF-8), from the country's culture and the sex.
    dasmig::country country_info; ///< The country.
    dasmig::city city_info;       ///< The city of residence, in the country.
    std::string admin1;     ///< Name of the city's state or province; empty when city-generator has no admin1 names loaded.
    dasmig::birth birthday; ///< Birth date, completed-year age and sex.
    dasmig::biodata body;   ///< Height, weight, eyes, hair, skin and blood type, at the persona's age and sex.
    dasmig::personality traits;     ///< Personality traits and axes.
    dasmig::interest interests;     ///< Religion, hobbies and media.
    dasmig::occupation job;         ///< Employment status, education and job.

    /// @brief Retrieve the random seed used to generate this persona.
    /// @return The call seed; pass it to get_persona(request, seed) with
    ///         the same request and data to replay this persona.
    [[nodiscard]] std::uint64_t seed() const { return _signature.seed; }

    /// @brief The seed and the data this persona was generated from.
    /// @return The replay signature; `data` combines the data hashes of the
    ///         eight libraries and the request.
    [[nodiscard]] const core::seed_signature& signature() const
    {
        return _signature;
    }

    /// @brief Biological sex of the persona.
    /// @return The sex drawn with the birth.
    [[nodiscard]] sex person_sex() const { return birthday.bio_sex; }

    /// @brief Age in completed years at the request's reference date.
    /// @return The age.
    [[nodiscard]] unsigned age() const { return birthday.age; }

    /// @brief One-line summary: name, age, sex, city, country and job.
    /// @return For example `Ana Silva, 34, female, Porto, Portugal, Teacher`.
    [[nodiscard]] std::string to_string() const
    {
        std::string out = name + ", " + std::to_string(age()) + ", "
                          + std::string{core::sex_name(person_sex())} + ", "
                          + city_info.name + ", " + country_info.name_common;
        const auto text = job.to_string();
        if (!text.empty()) out += ", " + text;
        return out;
    }

    /// @brief Write this persona as a JSON object.
    ///
    /// Each library's result is written as that library writes it
    /// (`country`, `city`, `birth`, `biodata`, `personality`, `interest`,
    /// `occupation`), next to `name`, `sex`, `age`, `admin1` and `signature`.
    /// @param w Writer positioned where a value is allowed.
    void to_json(core::json_writer& w) const
    {
        w.begin_object()
            .field("name", name)
            .field("sex", core::sex_name(person_sex()))
            .field("age", age())
            .field("admin1", admin1);
        w.key("country");
        country_info.to_json(w);
        w.key("city");
        city_info.to_json(w);
        w.key("birth");
        birthday.to_json(w);
        w.key("biodata");
        body.to_json(w);
        w.key("personality");
        traits.to_json(w);
        w.key("interest");
        interests.to_json(w);
        w.key("occupation");
        job.to_json(w);
        w.key("signature");
        _signature.to_json(w);
        w.end_object();
    }

    /// @brief This persona as a JSON document.
    /// @return Compact JSON.
    [[nodiscard]] std::string to_json() const
    {
        core::json_writer w;
        to_json(w);
        return w.str();
    }

    /// @brief Implicit conversion to std::string.
    /// @return Summary string.
    operator std::string() const // NOLINT(hicpp-explicit-conversions)
    {
        return to_string();
    }

    /// @brief Stream the summary to an output stream.
    /// @param os Output stream.
    /// @param v The persona to stream.
    /// @return The stream.
    friend std::ostream& operator<<(std::ostream& os, const persona& v)
    {
        os << v.to_string();
        return os;
    }

  private:
    core::seed_signature _signature;
    friend class psg;
};

/// @brief persona-generator: runs the persona pipeline.
///
/// The pipeline draws a country, then a city, a birth, a body, a name,
/// interests, an occupation and a personality, each conditioned on what was
/// drawn before (the order and dependencies are declared as entity-generator
/// fields, so they are checked, not assumed). Each library gets its own call
/// seed, split from the persona's seed by the field's key, so the persona's
/// seed replays all of it, and forcing one choice (a country, a sex) does not
/// shift the draws of the others. Output is identical on every compiler and
/// platform (dasmig-core).
///
/// The generator holds no data of its own: it calls the instances of the eight
/// libraries it was built on (the singletons by default), so their data, their
/// layers and their `lite` or `full` tiers are the persona's.
///
/// @par Thread safety
/// get_persona(request, seed) is const and only calls const members of the
/// libraries, so it may run concurrently. get_persona(request) draws from the
/// generator's engine and needs external synchronisation on the same instance.
class psg
{
  public:
    /// @brief The library instances a persona is drawn from.
    struct sources
    {
        cntg* country{&cntg::instance()};    ///< Countries.
        cg* city{&cg::instance()};           ///< Cities.
        bthg* birth{&bthg::instance()};      ///< Birth dates and ages.
        bdg* biodata{&bdg::instance()};      ///< Bodies.
        ng* name{&ng::instance()};           ///< Names.
        intg* interest{&intg::instance()};   ///< Religion, hobbies and media.
        ocg* occupation{&ocg::instance()};   ///< Employment, education and jobs.
        trtg* trait{&trtg::instance()};      ///< Personality.
    };

    /// @brief Draws from the singletons of the eight libraries.
    psg() { build_(); }

    /// @brief Draws from the given library instances (which must outlive this
    ///        generator).
    /// @param from The instances to use.
    explicit psg(sources from) : _from{from} { build_(); }

    psg(const psg&) = delete;            ///< Not copyable.
    psg& operator=(const psg&) = delete; ///< Not copyable.
    psg(psg&&) noexcept = default;       ///< Movable.
    psg& operator=(psg&&) noexcept = default; ///< Movable.
    ~psg() = default;                    ///< Destructor.

    /// @brief Access the global singleton instance.
    /// @return A reference to the singleton.
    static psg& instance()
    {
        static psg instance;
        return instance;
    }

    /// @brief The countries a random persona is drawn from.
    ///
    /// The countries for which all eight libraries have data in their `lite`
    /// tier. A country can be asked for by `persona_request::country` outside
    /// this list; the libraries then use what they have for it, or reject it
    /// (South Korea, for example, needs name-generator's `full` tier: its
    /// `lite` tier has no Korean male names).
    /// @return ISO 3166-1 alpha-2 codes, sorted.
    [[nodiscard]] static std::span<const std::string_view> supported_countries()
    {
        return supported_;
    }

    /// @brief Check whether every library has data.
    /// @return True when all eight libraries can generate.
    [[nodiscard]] bool has_data() const
    {
        return _from.country->has_data() && _from.city->has_data() && _from.birth->has_data()
               && _from.biodata->has_data() && _from.name->has_resources()
               && _from.interest->has_data() && _from.occupation->has_data()
               && _from.trait->has_data();
    }

    /// @brief Load a tier into every library.
    /// @param tier `dataset::lite` or `dataset::full`.
    /// @return True when all eight libraries found their data.
    [[nodiscard]] bool load(dataset tier) // NOLINT(readability-make-member-function-const)
    {
        bool ok = _from.country->load(tier);
        ok = _from.city->load(tier) && ok;
        ok = _from.birth->load(tier) && ok;
        ok = _from.biodata->load(tier) && ok;
        ok = _from.name->load(tier) && ok;
        ok = _from.interest->load(tier) && ok;
        ok = _from.occupation->load(tier) && ok;
        ok = _from.trait->load(tier) && ok;
        return ok;
    }

    /// @brief Hash of the data of all eight libraries.
    /// @return A hash that changes when any library's loaded data does.
    [[nodiscard]] std::uint64_t data_hash() const
    {
        std::uint64_t h = 0;
        for (const auto part : {_from.country->data_hash(), _from.city->data_hash(),
                                _from.birth->data_hash(), _from.biodata->data_hash(),
                                _from.name->data_hash(), _from.interest->data_hash(),
                                _from.occupation->data_hash(), _from.trait->data_hash()})
        {
            h = core::hash_combine(h, part);
        }
        return h;
    }

    /// @brief Generate a persona with a new random seed.
    /// @param request What to ask for; `{}` for the defaults.
    /// @return The persona; seed() replays it.
    /// @throws std::runtime_error If a library has no data.
    /// @throws std::invalid_argument If the request is invalid.
    [[nodiscard]] persona get_persona(const persona_request& request)
    {
        return get_persona(request, _engine());
    }

    /// @brief Generate the persona for a call seed. Deterministic; does not
    ///        touch the generator's engine.
    /// @param request What to ask for.
    /// @param call_seed Any 64-bit seed.
    /// @return The same persona for the same seed, request and data.
    /// @throws std::runtime_error If a library has no data.
    /// @throws std::invalid_argument If the request is invalid.
    [[nodiscard]] persona get_persona(const persona_request& request,
                                      std::uint64_t call_seed) const
    {
        if (!has_data()) throw std::runtime_error("persona-generator: a library has no data loaded");
        if (request.ages && request.ages->min > request.ages->max)
            throw std::invalid_argument("persona_request: ages.min must be <= ages.max");

        entity_request er;
        std::string country_code;
        if (request.country)
        {
            country_code = *request.country;
            std::ranges::transform(country_code, country_code.begin(), [](char c) {
                return static_cast<char>(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
            });
            er.inputs["country"] = country_code;
        }
        if (request.person_sex)
            er.inputs["sex"] = std::string{core::sex_name(*request.person_sex)};
        if (request.ages)
        {
            er.inputs["age_min"] = static_cast<std::int64_t>(request.ages->min);
            er.inputs["age_max"] = static_cast<std::int64_t>(request.ages->max);
        }
        er.inputs["reference_date"] =
            static_cast<std::int64_t>(static_cast<int>(request.reference_date.year())) * 10000
            + static_cast<std::int64_t>(static_cast<unsigned>(request.reference_date.month())) * 100
            + static_cast<std::int64_t>(static_cast<unsigned>(request.reference_date.day()));
        er.inputs["traits"] = static_cast<std::int64_t>(request.traits);

        const auto e = _pipeline.generate(er, call_seed);

        persona p;
        p.country_info = e.get<dasmig::country>(L"country");
        p.city_info = e.get<dasmig::city>(L"city");
        p.birthday = e.get<dasmig::birth>(L"birth");
        p.body = e.get<dasmig::biodata>(L"biodata");
        p.name = e.get<std::string>(L"name");
        p.traits = e.get<dasmig::personality>(L"traits");
        p.interests = e.get<dasmig::interest>(L"interest");
        p.job = e.get<dasmig::occupation>(L"occupation");
        if (_from.city->has_admin1())
            p.admin1 = _from.city->resolve_admin1(p.city_info.country_code, p.city_info.admin1_code);
        p._signature = e.signature();
        return p;
    }

    // -- Batch generation -------------------------------------------------

    /// @brief Generate a batch of persona results from a seed drawn like
    ///        get_persona() draws its own.
    ///
    /// Item `i` is `get_persona(request, core::item_seed(batch_seed, options.first + i))`
    /// for the drawn batch seed; see the seeded overload.
    ///
    /// @param request What every item is generated from.
    /// @param options Count, first index, threads and uniqueness.
    /// @return The results, in index order.
    [[nodiscard]] std::vector<persona> get_persona_batch(const persona_request& request,
                                                  const core::batch_options& options)
    {
        return get_persona_batch(request, options, _engine());
    }

    /// @brief Generate a batch of persona results, deterministically.
    ///
    /// Item `i` is exactly `get_persona(request, core::item_seed(batch_seed,
    /// options.first + i))`, so an item replays on its own, a range can be
    /// generated directly, two batches that follow each other equal one, and
    /// the output does not depend on `options.threads`.
    ///
    /// @param request What every item is generated from.
    /// @param options Count, first index, threads and uniqueness (see
    ///        core::unique_rule; the rule must be made for `persona`).
    /// @param batch_seed Seed of the whole batch.
    /// @return The results, in index order.
    /// @throws std::invalid_argument If the request or options are invalid.
    /// @throws std::runtime_error If no data has been loaded, or a unique
    ///         batch runs out of attempts.
    [[nodiscard]] std::vector<persona> get_persona_batch(const persona_request& request,
                                                  const core::batch_options& options,
                                                  std::uint64_t batch_seed) const
    {
        return core::run_batch(options, batch_seed, [&](std::uint64_t item_seed)
                               { return get_persona(request, item_seed); });
    }

    /// @brief Stream a batch of persona results to a sink without holding the
    ///        whole batch.
    ///
    /// Generates the same items as get_persona_batch() and calls
    /// `sink(index, item)` in index order on the calling thread; `index`
    /// includes `options.first`.
    ///
    /// @tparam Sink Callable taking `(std::uint64_t, persona&)`; for example
    ///         core::jsonl_writer or core::csv_writer.
    /// @param request What every item is generated from.
    /// @param options Count, first index, threads and uniqueness.
    /// @param batch_seed Seed of the whole batch.
    /// @param sink Receives the items.
    template <typename Sink>
    void for_each_persona(const persona_request& request, const core::batch_options& options,
                        std::uint64_t batch_seed, Sink&& sink) const
    {
        core::for_each_batch(options, batch_seed,
                             [&](std::uint64_t item_seed) { return get_persona(request, item_seed); },
                             std::forward<Sink>(sink));
    }

    /// @brief Stream a batch of persona results with a batch seed drawn like
    ///        get_persona() draws its own.
    /// @tparam Sink Callable taking `(std::uint64_t, persona&)`.
    /// @param request What every item is generated from.
    /// @param options Count, first index, threads and uniqueness.
    /// @param sink Receives the items.
    template <typename Sink>
    void for_each_persona(const persona_request& request, const core::batch_options& options,
                        Sink&& sink)
    {
        for_each_persona(request, options, _engine(), std::forward<Sink>(sink));
    }


  private:
    static constexpr std::array<std::string_view, 79> supported_ = {
        "AE", "AL", "AR", "AT", "AZ", "BD", "BE", "BG", "BH", "BO", "BR", "CA", "CH", "CL", "CM", "CN",
        "CO", "CR", "CY", "CZ", "DE", "DK", "DZ", "EC", "EE", "EG", "ES", "ET", "FI", "FR", "GB", "GE",
        "GH", "GR", "HN", "HR", "HU", "ID", "IE", "IL", "IN", "IQ", "IR", "IS", "IT", "JM", "JO", "JP",
        "KZ", "LB", "LT", "LU", "LY", "MA", "MD", "MT", "MX", "MY", "NG", "NL", "NO", "PE", "PH",
        "PL", "PT", "RS", "RU", "SA", "SD", "SE", "SG", "SI", "SV", "SY", "TN", "TR", "US", "YE", "ZA"};

    static constexpr int max_country_attempts_ = 10000;

    sources _from{};
    core::rng _engine{core::rng::from_entropy()};
    eg _pipeline; // Built once, by build_().

    // One library's step in the pipeline. Unlike entity-generator's field(),
    // it reads the library's data hash at each call, so the pipeline is built
    // once and data loaded into a library later still reaches the signature;
    // and it does not format the value, which only the persona shows.
    template <typename Lib, typename Fn>
    class step_ final : public component
    {
      public:
        step_(std::wstring key, std::vector<std::wstring> depends, const Lib* lib, Fn fn)
            : _key{std::move(key)}, _depends{std::move(depends)}, _lib{lib}, _fn{std::move(fn)}
        {
        }

        [[nodiscard]] std::wstring key() const override { return _key; }
        [[nodiscard]] std::vector<std::wstring> depends_on() const override { return _depends; }
        [[nodiscard]] std::uint64_t data_hash() const override { return _lib->data_hash(); }
        [[nodiscard]] std::any generate(const generation_context& ctx) const override { return std::any{_fn(ctx)}; }
        [[nodiscard]] std::wstring to_string(const std::any& /*value*/) const override { return {}; }
        [[nodiscard]] json_value to_json(const std::any& /*value*/) const override { return {}; }

      private:
        std::wstring _key;
        std::vector<std::wstring> _depends;
        const Lib* _lib;
        Fn _fn;
    };

    template <typename Lib, typename Fn>
    void add_(std::wstring key, std::vector<std::wstring> depends, const Lib* lib, Fn fn)
    {
        _pipeline.add(std::make_unique<step_<Lib, Fn>>(std::move(key), std::move(depends), lib, std::move(fn)));
    }

    [[nodiscard]] static bool is_supported_(std::string_view cca2)
    {
        return std::ranges::binary_search(supported_, cca2);
    }

    [[nodiscard]] static std::optional<sex> sex_input_(const generation_context& ctx)
    {
        if (!ctx.has_input("sex")) return std::nullopt;
        return core::parse_sex(ctx.input<std::string>("sex")).value_or(sex::male);
    }

    /// The fields of the pipeline, in the order they depend on each other.
    void build_()
    {
        const auto from = _from;

        add_(L"country", {}, from.country, [from](const generation_context& ctx) {
                 if (ctx.has_input("country"))
                     return from.country->find_country(ctx.input<std::string>("country"));
                 for (int i = 0; i < max_country_attempts_; ++i)
                 {
                     auto c = from.country->get_country({}, ctx.random()());
                     if (is_supported_(c.cca2)) return c;
                 }
                 throw std::runtime_error("persona-generator: no supported country in the country data");
             });

        add_(L"city", {L"country"}, from.city, [from](const generation_context& ctx) {
                 return from.city->get_city(
                     {.country = ctx.get<const dasmig::country&>(L"country").cca2}, ctx.random()());
             });

        add_(L"birth", {L"country"}, from.birth, [from](const generation_context& ctx) {
                 birth_request r;
                 r.country_code = ctx.get<const dasmig::country&>(L"country").cca2;
                 r.bio_sex = sex_input_(ctx);
                 if (ctx.has_input("age_min"))
                     r.ages = age_range{.min = static_cast<std::uint8_t>(ctx.input<std::int64_t>("age_min")),
                                        .max = static_cast<std::uint8_t>(ctx.input<std::int64_t>("age_max"))};
                 const auto d = ctx.input<std::int64_t>("reference_date");
                 r.reference_date = std::chrono::year_month_day{
                     std::chrono::year{static_cast<int>(d / 10000)},
                     std::chrono::month{static_cast<unsigned>(d / 100 % 100)},
                     std::chrono::day{static_cast<unsigned>(d % 100)}};
                 return from.birth->get_birth(r, ctx.random()());
             });

        add_(L"biodata", {L"country", L"birth"}, from.biodata, [from](const generation_context& ctx) {
                 const auto& cca2 = ctx.get<const dasmig::country&>(L"country").cca2;
                 const auto& b = ctx.get<const dasmig::birth&>(L"birth");
                 return from.biodata->get_biodata(
                     {.country = cca2, .bio_sex = b.bio_sex, .age = std::min<unsigned>(b.age, 120)},
                     ctx.random()());
             });

        add_(L"name", {L"country", L"birth"}, from.name, [from](const generation_context& ctx) {
                 const auto& cca2 = ctx.get<const dasmig::country&>(L"country").cca2;
                 const auto& b = ctx.get<const dasmig::birth&>(L"birth");
                 std::string lower = cca2;
                 std::ranges::transform(lower, lower.begin(), [](char c) {
                     return static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
                 });
                 // An unknown code is culture::any, which would draw a
                 // name from a random culture: reject it instead.
                 const auto name_culture = ng::to_culture(core::from_utf8(lower));
                 if (name_culture == culture::any)
                     throw std::invalid_argument("persona-generator: name-generator has no names for country "
                                                 + cca2);
                 const auto n = from.name->get_full_name(
                     {.gender = b.bio_sex == sex::female ? gender::f : gender::m, .culture = name_culture},
                     ctx.random()());
                 return core::to_utf8(static_cast<std::wstring>(n));
             });

        add_(L"traits", {}, from.trait, [from](const generation_context& ctx) {
                 return from.trait->get_personality(
                     {.count = static_cast<std::size_t>(ctx.input<std::int64_t>("traits"))},
                     ctx.random()());
             });

        add_(L"interest", {L"country", L"birth"}, from.interest, [from](const generation_context& ctx) {
                 const auto& b = ctx.get<const dasmig::birth&>(L"birth");
                 interest_request r;
                 const auto& cca2 = ctx.get<const dasmig::country&>(L"country").cca2;
                 r.country = cca2;
                 r.age = static_cast<std::uint8_t>(std::min<unsigned>(b.age, 120));
                 r.person_sex = b.bio_sex;
                 return from.interest->get_interest(r, ctx.random()());
             });

        add_(L"occupation", {L"country", L"birth"}, from.occupation, [from](const generation_context& ctx) {
                 const auto& b = ctx.get<const dasmig::birth&>(L"birth");
                 occupation_request r;
                 const auto& cca2 = ctx.get<const dasmig::country&>(L"country").cca2;
                 r.country = cca2;
                 r.age = static_cast<std::uint8_t>(std::min<unsigned>(b.age, 120));
                 r.person_sex = b.bio_sex;
                 return from.occupation->get_occupation(r, ctx.random()());
             });
    }
};

} // namespace dasmig

#endif // DASMIG_PERSONAGEN_HPP
