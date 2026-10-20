#include "catch_amalgamated.hpp"
#include "../dasmig/personagen.hpp"

#include <algorithm>
#include <chrono>
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
    REQUIRE(gen.data_hash() == 0x12083193eb329143ULL);

    const auto one = gen.get_persona(persona_request{}, 1);
    REQUIRE(one.to_string() == "Nigel Phillips, 13, male, Gosport, United Kingdom, student");
    REQUIRE(one.signature().to_string() == "0000000000000001-d29a76dea8889ccb");
    const auto two = gen.get_persona(persona_request{}, 2);
    REQUIRE(two.to_string() == "Sadaf Esmaeili, 16, female, Mashhad, Iran, student");
    REQUIRE(two.signature().to_string() == "0000000000000002-d29a76dea8889ccb");

    persona_request brazil;
    brazil.country = "BR";
    brazil.person_sex = dasmig::sex::female;
    brazil.ages = dasmig::age_range{.min = 25, .max = 35};
    brazil.traits = 5;
    const auto three = gen.get_persona(brazil, 3);
    REQUIRE(three.to_string() == "Jéssica Tomaz Cardozo, 34, female, São Paulo, Brazil, Farm labourer");
    REQUIRE(three.signature().to_string() == "0000000000000003-00a99a1fc2c48775");

    persona_request japan;
    japan.country = "jp";
    japan.reference_date = std::chrono::year_month_day{std::chrono::year{2030}, std::chrono::March, std::chrono::day{9}};
    const auto four = gen.get_persona(japan, 4);
    REQUIRE(four.to_string() == "Iwasaki Yasushi, 70, male, Edogawe, Japan, Shop assistant");
    REQUIRE(four.signature().to_string() == "0000000000000004-a458d5cf038b5c6f");

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
    REQUIRE(h == 0x9b965a26df7f7238ULL);
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
    REQUIRE(hash == 0x78805ecdd844e9fcULL);
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
    REQUIRE(hash == 0xdea1b993d5948d6bULL);
}
