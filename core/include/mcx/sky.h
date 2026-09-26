#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace mcx {

enum class Weather : std::uint8_t { clear, rain, thunder };

constexpr std::int64_t dayLength = 24000;
constexpr std::int64_t noon = 6000;

std::string timeName(std::int64_t ticks);
std::optional<std::int64_t> parseTime(std::string_view text);
std::string weatherName(Weather weather);
std::optional<Weather> parseWeather(std::string_view text);

}
