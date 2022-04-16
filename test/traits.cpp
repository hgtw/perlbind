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
}

TEST_CASE("function traits vararg array and hash", "[traits][function]")
{
  using Fn1 = void(*)(int);
  using Fn2 = int(*)(perlbind::array);
  using Fn3 = perlbind::array(*)();
  using Fn4 = perlbind::array(*)(perlbind::hash);

  STATIC_REQUIRE(perlbind::detail::function_traits<Fn1>::has_array == false);
  STATIC_REQUIRE(perlbind::detail::function_traits<Fn2>::has_array == true);
  STATIC_REQUIRE(perlbind::detail::function_traits<Fn3>::has_array == false);
  STATIC_REQUIRE(perlbind::detail::function_traits<Fn4>::has_array == false);

  STATIC_REQUIRE(perlbind::detail::function_traits<Fn1>::has_hash == false);
  STATIC_REQUIRE(perlbind::detail::function_traits<Fn2>::has_hash == false);
  STATIC_REQUIRE(perlbind::detail::function_traits<Fn3>::has_hash == false);
  STATIC_REQUIRE(perlbind::detail::function_traits<Fn4>::has_hash == true);

  STATIC_REQUIRE(perlbind::detail::function_traits<Fn1>::is_vararg == false);
  STATIC_REQUIRE(perlbind::detail::function_traits<Fn2>::is_vararg == true);
  STATIC_REQUIRE(perlbind::detail::function_traits<Fn3>::is_vararg == false);
  STATIC_REQUIRE(perlbind::detail::function_traits<Fn4>::is_vararg == true);
}
