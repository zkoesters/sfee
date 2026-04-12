#pragma once

#include <string>
#include <vector>
#include <ranges>
#include <sstream>
#include <iterator>

inline std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> result;
    auto split_view = std::views::split(str, delimiter);
    for (auto part : split_view) {
        result.emplace_back(part.begin(), part.end());
    }
    return result;
}

template<class C>
struct char_traits_nocase : public std::char_traits<C>
{
    static bool eq(const C& c1, const C& c2)
    {
        return std::tolower(c1) == std::tolower(c2);
    }

    static bool lt(const C& c1, const C& c2)
    {
        return std::tolower(c1) < std::tolower(c2);
    }

    static int compare(const C* s1, const C* s2, size_t N)
    {
        return _strnicmp(s1, s2, N);
    }

    static const char* find(const C* s, size_t N, const C& a)
    {
        for (size_t i = 0; i < N; ++i)
        {
            if (std::tolower(s[i]) == std::tolower(a))
                return s + i;
        }
        return 0;
    }

    static bool eq_int_type(typename std::char_traits<C>::int_type c1, typename std::char_traits<C>::int_type c2)
    {
        return std::tolower(c1) == std::tolower(c2);
    }
};

namespace std {
    using istring = std::basic_string<char, char_traits_nocase<char> >;
}

template<>
struct std::hash<std::istring>
{
    std::size_t operator()(const std::istring& s) const noexcept
    {
        std::size_t _Val = std::_FNV_offset_basis;
        for (std::size_t _Idx = 0; _Idx < s.size(); ++_Idx) {
            _Val ^= std::tolower(s[_Idx]);
            _Val *= std::_FNV_prime;
        }

        return _Val;
    }
};