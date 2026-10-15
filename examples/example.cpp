#include "../dasmig/personagen.hpp"
#include <cstdint>
#include <iostream>

int main()
{
    // The singleton draws from the eight libraries' singletons, which load
    // the lite tier (compiled in, or from their resources/).
    auto& gen = dasmig::psg::instance();
    if (!gen.has_data())
    {
        std::cerr << "Could not load the data of all eight libraries.\n";
        return 1;
    }

    std::cout << "Random:\n";
    for (int i = 0; i < 5; ++i)
    {
        std::cout << "  " << gen.get_persona(dasmig::persona_request{}) << '\n';
    }

    std::cout << "\nA woman of 25 to 35 in Brazil:\n";
    dasmig::persona_request request;
    request.country = "BR";
    request.person_sex = dasmig::sex::female;
    request.ages = dasmig::age_range{.min = 25, .max = 35};
    for (int i = 0; i < 3; ++i)
    {
        std::cout << "  " << gen.get_persona(request) << '\n';
    }

    std::cout << "\nSeed 42, twice:\n";
    const auto a = gen.get_persona(dasmig::persona_request{}, std::uint64_t{42});
    const auto b = gen.get_persona(dasmig::persona_request{}, std::uint64_t{42});
    std::cout << "  " << a.to_string() << '\n'
              << "  " << b.to_string() << '\n'
              << "  signature " << a.signature().to_string() << '\n';

    std::cout << "\nAs JSON:\n  " << a.to_json() << '\n';
    return 0;
}
