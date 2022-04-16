#pragma once

namespace perlbind { namespace detail {

template<typename T, typename... Rest>
struct is_any : std::false_type {};
template<typename T, typename Last>
struct is_any<T, Last> : std::is_same<T, Last> {};
template<typename T, typename First, typename... Rest>
struct is_any<T, First, Rest...> : std::integral_constant<bool, std::is_same<T, First>::value || is_any<T, Rest...>::value> {};

template <typename T>
struct is_signed_integral : std::integral_constant<bool, std::is_integral<T>::value && std::is_signed<T>::value> {};

template <typename T>
struct is_signed_integral_or_enum : std::integral_constant<bool, is_signed_integral<T>::value || std::is_enum<T>::value> {};

} // namespace detail
} // namespace perlbind
