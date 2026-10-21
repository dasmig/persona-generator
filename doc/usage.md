# persona-generator Usage Guide

persona-generator draws a complete character in one call by running the other dasmig generators in dependency order. This guide covers the request, the result, how the draws are conditioned, replay, and using your own data.

[TOC]

<!-- GitHub-rendered TOC (Doxygen uses [TOC] above instead) -->
<ul>
<li><a href="#a-first-persona">A first persona</a></li>
<li><a href="#the-request">The request</a></li>
<li><a href="#the-result">The result</a></li>
<li><a href="#how-the-draws-are-conditioned">How the draws are conditioned</a></li>
<li><a href="#seeds-and-replay">Seeds and replay</a></li>
<li><a href="#data">Data</a></li>
<li><a href="#supported-countries">Supported countries</a></li>
<li><a href="#migrating-to-v3">Migrating to v3</a></li>
<li><a href="#errors">Errors</a></li>
</ul>

## A first persona

```cpp
#include <dasmig/personagen.hpp>
#include <iostream>

int main()
{
    auto& gen = dasmig::psg::instance();
    if (!gen.has_data()) return 1;

    std::cout << gen.get_persona(dasmig::persona_request{}) << '\n';
}
```

`persona_request{}` asks for the defaults: a country drawn by population among the <a href="#supported-countries">supported countries</a>, and an age, sex, city, name, body, interests, occupation and three traits drawn for it.

## The request

| Field | Default | Meaning |
| --- | --- | --- |
| `country` | drawn | ISO 3166-1 alpha-2 code, any case |
| `person_sex` | drawn | `dasmig::sex::female` or `male` |
| `ages` | drawn from the country's age pyramid | `age_range{.min, .max}`, inclusive, in completed years |
| `reference_date` | 2023-07-01 | The date ages are computed at. The library never reads the clock |
| `traits` | 3 | How many personality traits |
| `relations` | unset | A `dasmig::family_request`: also draw the persona's family (see <a href="#relatives">Relatives</a>) |

```cpp
dasmig::persona_request request;
request.country = "pt";
request.ages = dasmig::age_range{.min = 18, .max = 30};
request.reference_date = std::chrono::year_month_day{std::chrono::year{2026}, std::chrono::October, std::chrono::day{14}};
auto p = gen.get_persona(request);
```

The request is never stored: one call cannot change the next.

## The result

A `persona` holds the typed result of every library, so no JSON parsing is needed to use it:

| Member | Type | From |
| --- | --- | --- |
| `name` | `std::string` (UTF-8) | name-generator |
| `country_info` | `dasmig::country` | country-generator |
| `city_info`, `admin1` | `dasmig::city`, state or province name | city-generator |
| `birthday` | `dasmig::birth` | birth-generator |
| `body` | `dasmig::biodata` | biodata-generator |
| `traits` | `dasmig::personality` | trait-generator |
| `interests` | `dasmig::interest` | interest-generator |
| `job` | `dasmig::occupation` | occupation-generator |

`age()` and `person_sex()` read the birth. A persona converts to `std::string` and streams as a one-line summary (`Jéssica Tomaz Cardozo, 34, female, São Paulo, Brazil, Farm labourer`). `admin1` is empty unless city-generator has its admin1 names loaded (`load_admin1()`).

`to_json()` writes `name`, `sex`, `age`, `admin1` and `signature`, and each library's result under `country`, `city`, `birth`, `biodata`, `personality`, `interest` and `occupation`, exactly as that library writes it (so a country is `country_code` in `biodata`, `interest` and `occupation`) (see its guide for the fields).

## How the draws are conditioned

```
country ──┬─ city
          ├─ birth ──┬─ biodata   (country, sex, age)
          │          ├─ name      (country's culture, sex)
          │          ├─ interest  (country, sex, age)
          │          └─ occupation (country, sex, age)
traits
```

- The **birth** gives the age and the sex, so every later part is conditioned on them: children are shorter, get no adult hobbies and no job, and older people retire.
- The **name** comes from the culture of the country and the sex.
- **Traits** are independent of the rest.

The pipeline is declared as [entity-generator](https://github.com/dasmig/entity-generator) fields (`field(...).depends_on(...)`), which checks the dependencies and orders the draws. Each part draws from a stream split from the persona's seed by the field's key, so forcing one choice does not shift the unrelated ones: the same seed with `person_sex = female` and with `male` gives the same country, city and personality.

## Seeds and replay

```cpp
auto a = gen.get_persona(request);                 // new random seed
auto b = gen.get_persona(request, a.seed());       // the same persona
auto c = gen.get_persona(request, 42);             // deterministic, const
```

`get_persona(request, seed)` is `const` and only calls `const` members of the libraries, so it may run on several threads. `get_persona(request)` draws its seed from the generator's engine and needs external synchronisation.

`signature()` is `<seed>-<data>`: `data` combines the data hashes of the eight libraries and the request, so a seed replays only against the same data and request.

## Data

The generator holds no data. It calls the eight libraries' instances, by default their singletons, so whatever they hold is the persona's:

```cpp
dasmig::psg gen;
gen.load(dasmig::dataset::full);              // every library, full tier
dasmig::bdg::instance().extend("my_countries.tsv");   // one library's layer
```

To keep different data side by side, give the generator your own instances (they must outlive it):

```cpp
dasmig::cntg country; dasmig::cg city; dasmig::bthg birth; dasmig::bdg biodata;
dasmig::ng name; dasmig::intg interest; dasmig::ocg occupation; dasmig::trtg trait;
dasmig::psg::sources own{&country, &city, &birth, &biodata, &name, &interest, &occupation, &trait};
dasmig::psg gen{own};
if (!gen.load(dasmig::dataset::lite)) return 1;
```

`has_data()` is true when all eight can generate; `data_hash()` combines their hashes.

## Supported countries

`psg::supported_countries()` lists the 79 countries a random persona is drawn from: those for which all eight libraries have data in their `lite` tier. Asking for another country is allowed; the libraries then use what they have, or reject it. South Korea, for example, is left out because name-generator's `lite` tier has no Korean male names; with the `full` tier loaded it can be requested. A country name-generator has no culture for (New Zealand, Thailand) is rejected with `std::invalid_argument`, rather than given a name from another culture.

## Batch Generation

Every seeded getter has batch forms: `get_persona_batch` return a `std::vector`, and `for_each_persona` streams the items to a sink in index order without holding the batch.

```cpp
auto& gen = dasmig::psg::instance();
const auto request = persona_request{};

// 1,000 results from one batch seed, on four threads.
auto items = gen.get_persona_batch(request, {.count = 1000, .threads = 4}, 42);

// Item 1 replays on its own: the batch seed and the index give its seed.
auto again = gen.get_persona(request, dasmig::core::item_seed(42, 1));

// A million results to a file as JSON Lines, never all in memory.
std::ofstream out{"items.jsonl"};
dasmig::core::jsonl_writer lines{out};
gen.for_each_persona(request, {.count = 1'000'000, .threads = 0}, 42, lines);
```

- **Item seeds.** Item `i` is exactly `get_persona(request, core::item_seed(batch_seed, first + i))`. A range can be generated directly (`first = 1000` starts at item 1000), two batches that follow each other equal one, and the output is the same for any `threads` (0 means the hardware count). Without a `batch_seed` one is drawn like the single getter draws its own.
- **Options.** `core::batch_options` has `count`, `first`, `threads` and `unique`. The request describes one item; the options describe the batch.
- **Uniqueness.** `.unique = core::unique_rule::by<persona>(key)` regenerates an item whose key already appeared in the batch, with a deterministic retry seed, and throws `std::runtime_error` naming the item when the attempts run out.
- **Sinks.** `core::jsonl_writer` writes one `to_json()` per line, `core::csv_writer` flattens each result's JSON into dotted columns. Any callable `(std::uint64_t index, persona&)` works.
- Batch calls use the same const getters from several threads, which the thread-safety rules above allow; the CI runs the batch tests under ThreadSanitizer.

## Relatives

Set `request.relations` to draw the people around the persona with [relationship-generator](https://github.com/dasmig/relationship-generator):

```cpp
persona_request request;
request.relations = dasmig::family_request{.up = 2, .down = 1}; // grandparents, parents, siblings, partners, children
const auto p = psg::instance().get_persona(request);
for (const auto& parent : p.relatives->parents()) std::cout << parent.name << '
';
```

The persona fills in the request's `anchor` (country, sex, birth date and name), so the parents share the persona's family name by the country's convention, and the request's `reference_date` replaces the family's. The rest of the `family_request` is yours. `relatives` is empty and `to_json()` has no `relatives` key unless the request asked for it, and the rest of the persona and its `signature()` are the same with and without it. The family is drawn from a seed split from the persona's, so it replays with the persona's seed; its own `signature` is inside `relatives`.

Set `stepparents` (a parent's new partner) and `home` (who the persona lives with, as `household` in the JSON) on the `family_request` to add them; both are off by default and need relationship-generator 1.1.0 or later.

Without `relations`, relationship-generator is not used and its data is not needed: `load()` ignores a missing tier of it. With `relations`, `get_persona` throws `std::runtime_error` if it has no data (the `lite` tier is compiled in with `DASMIG_RELATIONSHIP_EMBED`). It uses its own birth-generator and name-generator instances, the singletons by default, as the persona does.

## Migrating to v3

Version 3 composes biodata-generator 5, which draws skin, hair and eyes together (light hair goes with light eyes and fair skin, dark with dark), so the same seed gives a different skin, hair and eye colour than in v2. Everything else is unchanged: the data hash, the country, sex, age, names, jobs and traits for a seed, and the API. The libraries are now name-generator 4.2.2, city-generator 4.2.0, entity-generator 4.2.0, birth-generator and country-generator 3.1.2, interest-generator and occupation-generator 3.0.1, trait-generator 2.1.2 and biodata-generator 5.0.0. Update known answers that include the body.

## Errors

| Error | When |
| --- | --- |
| `std::runtime_error` | A library has no data loaded |
| `std::invalid_argument` | `ages.min > ages.max`, an unknown country, or another input a library rejects |
