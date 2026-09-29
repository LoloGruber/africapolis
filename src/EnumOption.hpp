#pragma once
#include <CLI/CLI.hpp>
#include <magic_enum.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace Africapolis {

/**
 * @brief Restricts an option to the symbols of an enumeration, matched regardless of case.
 * The symbols are reflected by magic_enum, so extending the enumeration is enough to make a new
 * value acceptable and to list it in the help output.
 */
template<typename E> requires std::is_enum_v<E>
CLI::Validator enumSymbols() {
    std::string symbols;
    for(const auto & symbol: magic_enum::enum_names<E>()) {
        symbols += (symbols.empty() ? "" : ",") + std::string(symbol);
    }
    return CLI::Validator(
        [](std::string & input) -> std::string {
            auto value = magic_enum::enum_cast<E>(input, magic_enum::case_insensitive);
            if(not value.has_value())
                return "'" + input + "' is not a symbol of " + std::string(magic_enum::enum_type_name<E>());
            input = std::to_string(magic_enum::enum_integer(value.value())); // CLI11 reads an enumeration from its underlying value
            return {};
        },
        "{" + symbols + "}",
        std::string(magic_enum::enum_type_name<E>())
    );
}

/**
 * @brief Unwraps an option which is only required by a subset of the modes of a task.
 * @throws std::invalid_argument when the option was omitted although the selected mode needs it
 */
template<typename T, typename E> requires std::is_enum_v<E>
const T & requiredFor(const std::optional<T> & value, std::string_view option, E mode) {
    if(not value.has_value()) {
        throw std::invalid_argument("Option '" + std::string(option) + "' is required for mode " + std::string(magic_enum::enum_name(mode)));
    }
    return value.value();
}

}
