// Benchmarks for persona-generator; see bench.hpp for the options.
//
// The composed libraries load into their shared instances, so "load" times
// loading all of them; the lite tier is compiled in (see the Makefile). A
// library without the tier keeps what it has: trait-generator ships only
// lite, so the full tier is every other library's full data with lite traits.

#include "../dasmig/personagen.hpp"
#include "bench.hpp"

int main(int argc, char** argv)
{
    dasmig::bench::runner run{"persona-generator", argc, argv};

    run.load("load", [&] {
        dasmig::psg gen;
        (void)gen.load(run.tier());
        return gen.has_data();
    });

    dasmig::psg gen;
    (void)gen.load(run.tier());
    if (!gen.has_data()) return run.missing_tier();

    run.calls("get_persona", [&](std::uint64_t seed) { return gen.get_persona({}, seed); });
    run.calls("get_persona BR", [&](std::uint64_t seed) {
        return gen.get_persona({.country = "BR"}, seed);
    });
    run.calls("get_persona to_json", [&](std::uint64_t seed) {
        return gen.get_persona({}, seed).to_json();
    });

    run.batches("get_persona_batch", [&](std::size_t count, std::size_t threads, std::uint64_t seed) {
        return gen.get_persona_batch({}, {.count = count, .threads = threads}, seed);
    });

    return run.finish();
}
