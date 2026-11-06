#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <functional>
struct RutaHash {
    using is_transparent=void;
    size_t operator()(std::wstring_view s) const noexcept { return std::hash<std::wstring_view>{}(s); }
};
