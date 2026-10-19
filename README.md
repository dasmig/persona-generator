# persona-generator for C++

> **Requires C++23** (e.g., `-std=c++23` for GCC/Clang, `/std:c++latest` for MSVC).

[![persona-generator for C++](https://raw.githubusercontent.com/dasmig/persona-generator/main/doc/persona-generator.png)](https://github.com/dasmig/persona-generator/releases)

[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/dasmig/persona-generator/main/LICENSE.MIT)
[![CI](https://github.com/dasmig/persona-generator/actions/workflows/ci.yml/badge.svg)](https://github.com/dasmig/persona-generator/actions/workflows/ci.yml)
[![GitHub Releases](https://img.shields.io/github/release/dasmig/persona-generator.svg)](https://github.com/dasmig/persona-generator/releases)
[![C++23](https://img.shields.io/badge/standard-C%2B%2B23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Header-only](https://img.shields.io/badge/type-header--only-green.svg)](https://github.com/dasmig/persona-generator#integration)
[![Documentation](https://img.shields.io/badge/docs-GitHub%20Pages-blue.svg)](https://dasmig.github.io/persona-generator/)

**[API Reference](https://dasmig.github.io/persona-generator/)** · **[Usage Guide](doc/usage.md)** · **[Data and Method](doc/data.md)** · **[Releases](https://github.com/dasmig/persona-generator/releases)**

Complete characters composed from the other dasmig generators: one call draws a country, a city, a birth, a body, a name, interests, an occupation and a personality, each conditioned on what was drawn before, and returns them as typed results and one JSON document.

```
Jéssica Tomaz Cardozo, 34, female, São Paulo, Brazil, Farm labourer
```

## Features

- **One Call, Eight Libraries**. A country sets the city, the name's culture, the body, the interests and the job; the birth's age and sex set the rest. The order and dependencies are declared as [entity-generator](https://github.com/dasmig/entity-generator) fields, so they are checked, not assumed.

- **Batch Generation**. Generate thousands of results from one seed, on several threads with the same output for any thread count, optionally unique by a key, and stream them as JSON Lines or CSV (see the usage guide).

- **Ask for What You Need**. A `persona_request` can fix the country, the sex, an age range, the reference date and the number of traits. What is left out is drawn.

- **Deterministic Seeding**. `get_persona(request, seed)` is a pure function of the request, the seed and the data, and `persona::seed()` replays a previous persona. Each library draws from its own stream split from the seed, so forcing one choice does not shift the others.

- **Identical Everywhere**. A seed gives the same result on every compiler, standard library and CPU (GCC, Clang, MSVC, Emscripten; x86-64 and ARM64), checked by known-answer tests.

- **JSON and Replay**. `to_json()` writes every library's result as that library writes it; `signature()` records the seed and a hash of the data of all eight libraries and the request.

- **Your Data**. The generator holds no data of its own. It calls the instances of the libraries, so their tiers (`lite`, `full`) and layers (`extend()`) are the persona's. Construct a `psg` over your own instances to use different data side by side.

## Integration

[`personagen.hpp`](https://github.com/dasmig/persona-generator/blob/main/dasmig/personagen.hpp) is the library [released here](https://github.com/dasmig/persona-generator/releases). It composes these libraries, which must be on the include path (with `dasmig-core` in each): [biodata](https://github.com/dasmig/biodata-generator), [birth](https://github.com/dasmig/birth-generator), [city](https://github.com/dasmig/city-generator), [country](https://github.com/dasmig/country-generator), [entity](https://github.com/dasmig/entity-generator), [interest](https://github.com/dasmig/interest-generator), [name](https://github.com/dasmig/name-generator), [occupation](https://github.com/dasmig/occupation-generator) and [trait](https://github.com/dasmig/trait-generator). The release notes list the versions it was tested with. Add

```cpp
#include <dasmig/personagen.hpp>
```

to the files you want to use it in. Each library finds its data as it does alone (a `resources` folder, or its compiled-in `lite` tier: define `DASMIG_COUNTRY_EMBED`, `DASMIG_CITY_EMBED`, `DASMIG_BIRTH_EMBED`, `DASMIG_BIODATA_EMBED`, `DASMIG_NAME_EMBED`, `DASMIG_INTEREST_EMBED`, `DASMIG_OCCUPATION_EMBED` and `DASMIG_TRAIT_EMBED`).

## Usage

```cpp
#include <dasmig/personagen.hpp>
#include <iostream>

auto& gen = dasmig::psg::instance();

auto p = gen.get_persona(dasmig::persona_request{});
std::cout << p << '\n';           // one line: name, age, sex, city, country, job
std::cout << p.to_json() << '\n'; // {"name":...,"country":{...},"city":{...},...}

dasmig::persona_request request;
request.country = "BR";
request.person_sex = dasmig::sex::female;
request.ages = dasmig::age_range{.min = 25, .max = 35};
auto q = gen.get_persona(request);

// The same seed gives the same result everywhere.
auto again = gen.get_persona(request, q.seed());
```

See the [usage guide](doc/usage.md) for everything else.

## JavaScript

[`@dasmig/persona`](https://www.npmjs.com/package/@dasmig/persona) is this library compiled to WebAssembly, with the `lite` data of the libraries it composes built in, for browsers, Web Workers and Node 20+: `npm install @dasmig/persona`. Its source is in [`package/`](package) and the C API it wraps in [`wasm/`](wasm). `make package` (or `python3 scripts/build_package.py`) builds it with Emscripten into `build/package`, and CI tests it in Node and in headless Chrome, benchmarks it and packs it. The package has its own version; the library versions each build contains are in its `versions`.

## Building

The libraries it composes are sibling checkouts (`LIBS=..`, the default), with their `lite` data compiled in.

```bash
make            # example
make test       # tests
make bench      # benchmarks, results in build/bench.tsv
make coverage   # code coverage
make lint       # clang-tidy
make docs       # API docs
make package    # the npm package, built with Emscripten into build/package
make test LIBS=path/to/the/libraries   # when they are elsewhere
```

## Compiler Support

Tested with:
- Clang 18+ (`-std=c++23`)
- GCC 14+ (`-std=c++23`)
- MSVC 19.38+ (`/std:c++latest`)
- Emscripten 3.1.74

## Dependencies

| Dependency | Version | Bundled | Purpose |
|-----------|---------|---------|---------|
| [dasmig-core](https://github.com/dasmig/dasmig-core) | 1.0 | Yes (`core.hpp`, `core/`) | Platform-independent random engine and distributions, JSON |
| the nine libraries above | see [CI](https://github.com/dasmig/persona-generator/blob/main/.github/actions/checkout-libs/action.yml) | No | The persona's parts |
| [Catch2](https://github.com/catchorg/Catch2) | 3.x | Yes (amalgamated) | Unit testing |

## Related Libraries

| Library | Description |
|---------|-------------|
| [address-generator](https://github.com/dasmig/address-generator) | Postal addresses with real streets by place |
| [biodata-generator](https://github.com/dasmig/biodata-generator) | Procedural human physical characteristics |
| [birth-generator](https://github.com/dasmig/birth-generator) | Demographically plausible birthdays |
| [city-generator](https://github.com/dasmig/city-generator) | Weighted city selection by population |
| [country-generator](https://github.com/dasmig/country-generator) | Weighted country selection by population |
| [email-generator](https://github.com/dasmig/email-generator) | Email addresses in each country's style |
| [entity-generator](https://github.com/dasmig/entity-generator) | ECS-based entity generation |
| [interest-generator](https://github.com/dasmig/interest-generator) | Religion, hobbies and favourite media by country, age and sex |
| [name-generator](https://github.com/dasmig/name-generator) | Culturally appropriate full names |
| [nickname-generator](https://github.com/dasmig/nickname-generator) | Gamer-style nicknames |
| [occupation-generator](https://github.com/dasmig/occupation-generator) | Employment status, education and occupation by country, age and sex |
| [phone-generator](https://github.com/dasmig/phone-generator) | Phone numbers valid in each country's numbering plan |
| [trait-generator](https://github.com/dasmig/trait-generator) | Personality traits with conflicts, synergies and Big Five axes |
| [user-generator](https://github.com/dasmig/user-generator) | Complete user records: account, username, email, phone and address |

## License

This library is released under the [MIT License](https://opensource.org/licenses/MIT).

```
MIT License

Copyright (c) 2026 Diego Dasso Migotto
```
