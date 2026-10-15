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

`age()` and `person_sex()` read the birth. A persona converts to `std::string` and streams as a one-line summary (`Thayna Brandao, 34, female, São Paulo, Brazil, Farm labourer`). `admin1` is empty unless city-generator has its admin1 names loaded (`load_admin1()`).

`to_json()` writes `name`, `sex`, `age`, `admin1` and `signature`, and each library's result under `country`, `city`, `birth`, `biodata`, `personality`, `interest` and `occupation`, exactly as that library writes it (see its guide for the fields).

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

## Errors

| Error | When |
| --- | --- |
| `std::runtime_error` | A library has no data loaded |
| `std::invalid_argument` | `ages.min > ages.max`, an unknown country, or another input a library rejects |
