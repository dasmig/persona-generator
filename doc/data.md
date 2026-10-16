# Data and Method

persona-generator has no data of its own. This page records what that means for a persona's data, and what the composition adds (rule 6 of the dasmig standard).

## Where the data comes from

Each part of a persona comes from one library, and each library documents its own data, sources and limits:

| Part | Library | Data notes |
| --- | --- | --- |
| Country | [country-generator](https://github.com/dasmig/country-generator) | `doc/data.md` |
| City | [city-generator](https://github.com/dasmig/city-generator) | GeoNames |
| Birth | [birth-generator](https://github.com/dasmig/birth-generator) | UN World Population Prospects age pyramids |
| Body | [biodata-generator](https://github.com/dasmig/biodata-generator) | Several sources, partly curated; the library says which |
| Name | [name-generator](https://github.com/dasmig/name-generator) | `philipperemy/name-dataset`; see the provenance note in its `doc/data.md` |
| Interests | [interest-generator](https://github.com/dasmig/interest-generator) | CIA World Factbook, Eurostat HETUS, curated catalogue |
| Job | [occupation-generator](https://github.com/dasmig/occupation-generator) | ILOSTAT with a curated catalogue |
| Traits | [trait-generator](https://github.com/dasmig/trait-generator) | Authored catalogue |

A persona's data is the union of these, and its `signature().data` is a hash of all eight libraries' loaded data and the request.

## What the composition decides

- **The supported countries** (`psg::supported_countries()`): the 79 countries for which all eight libraries have data in their `lite` tier, found by generating a persona of each sex for each. It is curated, not computed at run time.
- **The conditioning**: the birth's age and sex are passed to biodata, interest and occupation; the country's name culture and the sex are passed to name. Nothing else is correlated. In particular the personality is independent of the rest, and there is no correlation between, say, a job and a body beyond the age and sex both depend on.
- **Biodata is age-adjusted**: the persona passes its age to biodata-generator, so children and the elderly get an age-appropriate body.

## Known limits

- The `lite` tiers are top-rank slices of the full data, so names in a small culture repeat more.
- Names are only as clean as name-generator's data. Before name-generator 4.1.0 its Iranian lists held Czech names and several cultures' lists held initials and fragments (Md, Om, El); 4.1.0 removed them, and persona-generator 1.1.0 requires it. Two-letter tokens outside the Arabic-script cultures and longer abbreviations such as Mohd remain (see name-generator's `doc/data.md`).
- `admin1` (the state or province name) needs city-generator's admin1 file; it is not part of the replay signature.
