#include <catch2/catch_test_macros.hpp>

#include <perlbind/perlbind.h>

TEST_CASE("is_any traits", "[traits]")
{
  STATIC_REQUIRE(perlbind::detail::is_any<SV*, int, bool>::value == false);
  STATIC_REQUIRE(perlbind::detail::is_any<SV*, SV*, bool>::value == true);
  STATIC_REQUIRE(perlbind::detail::is_any<SV*, int, SV*>::value == true);
}

TEST_CASE("is_signed_integral trait", "[traits]")
{
  STATIC_REQUIRE(perlbind::detail::is_signed_integral<int>::value == true);
  STATIC_REQUIRE(perlbind::detail::is_signed_integral<unsigned>::value == false);
}

TEST_CASE("is_signed_integral or enum trait", "[traits]")
{
  enum eFoo { none = 0 };
  STATIC_REQUIRE(perlbind::detail::is_signed_integral_or_enum<int>::value == true);
  STATIC_REQUIRE(perlbind::detail::is_signed_integral_or_enum<unsigned>::value == false);
  STATIC_REQUIRE(perlbind::detail::is_signed_integral<eFoo>::value == false);
  STATIC_REQUIRE(perlbind::detail::is_signed_integral_or_enum<eFoo>::value == true);
}

TEST_CASE("count_of trait", "[traits]")
{
  STATIC_REQUIRE(perlbind::detail::count_of<perlbind::array>::value == 0);
  STATIC_REQUIRE(perlbind::detail::count_of<perlbind::array, int>::value == 0);
  STATIC_REQUIRE(perlbind::detail::count_of<perlbind::array, perlbind::array>::value == 1);
  STATIC_REQUIRE(perlbind::detail::count_of<perlbind::array, perlbind::array, int>::value == 1);
  STATIC_REQUIRE(perlbind::detail::count_of<perlbind::array, int, perlbind::array>::value == 1);
  STATIC_REQUIRE(perlbind::detail::count_of<perlbind::array, perlbind::array, perlbind::array>::value == 2);
  STATIC_REQUIRE(perlbind::detail::count_of<perlbind::array, int, perlbind::array, int>::value == 1);
}

TEST_CASE("is_last trait", "[traits]")
{
  STATIC_REQUIRE(perlbind::detail::is_last<perlbind::array>::value == false);
  STATIC_REQUIRE(perlbind::detail::is_last<perlbind::array, int>::value == false);
  STATIC_REQUIRE(perlbind::detail::is_last<perlbind::array, perlbind::array>::value == true);
  STATIC_REQUIRE(perlbind::detail::is_last<perlbind::array, perlbind::array, int>::value == false);
  STATIC_REQUIRE(perlbind::detail::is_last<perlbind::array, int, perlbind::array>::value == true);
  STATIC_REQUIRE(perlbind::detail::is_last<perlbind::array, perlbind::array, perlbind::array>::value == true);
  STATIC_REQUIRE(perlbind::detail::is_last<perlbind::array, int, perlbind::array, int>::value == false);
}

TEST_CASE("function traits", "[traits][function]")
{
  SECTION("regular functions")
  {
    using fn = void(*)(const char*);
    using traits = perlbind::detail::function_traits<fn>;

    STATIC_REQUIRE(std::is_same<traits::return_t, void>::value);
    STATIC_REQUIRE(traits::arity == 1);
    STATIC_REQUIRE(traits::stack_arity == 1);
    STATIC_REQUIRE(std::is_same<traits::stack_tuple, std::tuple<const char*>>::value);
  }

  SECTION("class methods")
  {
    struct Foo {};
    using fn = int(Foo::*)(int, float);
    using traits = perlbind::detail::function_traits<fn>;

    STATIC_REQUIRE(std::is_same<traits::return_t, int>::value);
    STATIC_REQUIRE(traits::arity == 2);
    STATIC_REQUIRE(traits::stack_arity == 3);
    STATIC_REQUIRE(std::is_same<traits::stack_tuple, std::tuple<Foo*, int, float>>::value);
  }

  SECTION("lambdas")
  {
    auto lambda = [](int, double) -> bool { return true; };
    using traits = perlbind::detail::function<decltype(lambda)>;

    STATIC_REQUIRE(std::is_same<traits::type, bool(*)(int, double)>::value);
    STATIC_REQUIRE(std::is_same<traits::return_t, bool>::value);
    STATIC_REQUIRE(traits::arity == 2);
    STATIC_REQUIRE(traits::stack_arity == 2);
    STATIC_REQUIRE(std::is_same<traits::stack_tuple, std::tuple<int, double>>::value);
  }
}

TEST_CASE("function traits vararg array and hash", "[traits][function]")
{
  struct Foo{};
  using Fn1 = void(*)(int);
  using Fn2 = int(*)(perlbind::array);
  using Fn3 = perlbind::array(*)();
  using Fn4 = perlbind::array(*)(perlbind::hash);
  using Fn5 = void(Foo::*)(perlbind::array);
  using Fn6 = void(*)(Foo*, perlbind::array);

  STATIC_REQUIRE(perlbind::detail::function_traits<Fn1>::is_vararg == false);
  STATIC_REQUIRE(perlbind::detail::function_traits<Fn2>::is_vararg == true);
  STATIC_REQUIRE(perlbind::detail::function_traits<Fn3>::is_vararg == false);
  STATIC_REQUIRE(perlbind::detail::function_traits<Fn4>::is_vararg == true);
  STATIC_REQUIRE(perlbind::detail::function_traits<Fn5>::is_vararg == true);
  STATIC_REQUIRE(perlbind::detail::function_traits<Fn6>::is_vararg == true);
}
