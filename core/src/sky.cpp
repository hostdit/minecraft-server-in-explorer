#include <mcx/sky.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <utility>

namespace mcx {

namespace {

constexpr std::array<std::pair<std::string_view, std::int64_t>, 6> times{{
    {"sunrise", 0},
    {"day", 1000},
    {"noon", 6000},
    {"sunset", 12000},
    {"night", 13000},
    {"midnight", 18000},
}};

constexpr std::array<std::string_view, 3> weathers{"clear", "rain", "thunder"};

std::string clean(std::string_view text) {
    while (!text.empty() && text.front() == ' ') text.remove_prefix(1);
    while (!text.empty() && text.back() == ' ') text.remove_suffix(1);
    std::string lower(text);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](char c) { return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c; });
    return lower;
}

}

std::string timeName(std::int64_t ticks) {
    for (auto& [name, value] : times)
        if (value == ticks) return std::string(name);
    return std::to_string(ticks);
}

std::optional<std::int64_t> parseTime(std::string_view text) {
    std::string name = clean(text);
    for (auto& [known, value] : times)
        if (known == name) return value;
    std::int64_t ticks;
    auto [end, error] = std::from_chars(name.data(), name.data() + name.size(), ticks);
    if (error != std::errc() || end != name.data() + name.size() || ticks < 0 || ticks >= dayLength) return std::nullopt;
    return ticks;
}

std::string weatherName(Weather weather) {
    return std::string(weathers.at(std::size_t(weather)));
}

std::optional<Weather> parseWeather(std::string_view text) {
    std::string name = clean(text);
    for (std::size_t i = 0; i < weathers.size(); i++)
        if (weathers[i] == name) return Weather(i);
    return std::nullopt;
}

}
