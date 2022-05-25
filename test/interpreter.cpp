#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>

#include <perlbind/perlbind.h>

#include <fstream>
#include <memory>

// interpreter for all tests (can only create once per program due to PERL_SYS_INIT3/PERL_SYS_TERM)
std::unique_ptr<perlbind::interpreter> interp;

int main(int argc, char* argv[])
{
#ifdef PERLBIND_NO_STRICT_SCALAR_TYPES
  printf("Running tests with PERLBIND_NO_STRICT_SCALAR_TYPES\n");
#elif PERLBIND_STRICT_NUMERIC_TYPES
  printf("Running tests with PERLBIND_STRICT_NUMERIC_TYPES\n");
#endif

  interp = std::make_unique<perlbind::interpreter>();

  // setup typemaps so we can confirm same ids in another compilation unit
  perlbind::detail::usertype<int>::id();
  perlbind::detail::usertype<double>::id();
  perlbind::detail::usertype<bool>::id();

  return Catch::Session().run( argc, argv );
}

TEST_CASE("loading script files", "[interpreter]")
{
  auto my_perl = interp->get();

  SECTION("good script")
  {
    std::ofstream of("testload.pl");
    of << "$loadedvar = 1234;\n";
    of.flush();

    REQUIRE_NOTHROW(interp->load_script("testpackage", "testload.pl"));
    REQUIRE((get_sv("testpackage::loadedvar", 0) != nullptr && SvIV(get_sv("testpackage::loadedvar", 0)) == 1234));
    REQUIRE(get_sv("loadedvar", 0) == nullptr);
    REQUIRE(get_sv("testpackage::missingvar", 0) == nullptr);
  }

  SECTION("non-existent script")
  {
    REQUIRE_THROWS(interp->load_script("testpackage", "missing.pl"));
  }

  SECTION("script with syntax errors")
  {
    std::ofstream of("scripterror.pl");
    of << "1 $test_syntax_error = 1";
    of.flush();

    REQUIRE_THROWS(interp->load_script("testpackage", "scripterror.pl"));
  }
}

TEST_CASE("calling perl subs", "[subcaller]")
{
  interp->eval(R"script(
    sub testsub { return 5; }

    package callpackage;
    sub testsub { return 10; }
    sub throwsub { die "should throw"; }
  )script");

  REQUIRE(interp->call_sub<int>("testsub") == 5);
  REQUIRE(interp->call_sub<int>("callpackage::testsub") == 10);
  REQUIRE_THROWS(interp->call_sub<int>("callpackage::throwsub"));
}

TEST_CASE("non-owning interpreter stateless package bindings", "[interpreter][package]")
{
  struct stateless
  {
    static void foo() {}
    static void foo(int) {}
  };

  auto my_perl = interp->get();

  {
    perlbind::interpreter view(my_perl);
    auto package = view.new_package("stateless");
    package.add("foo", (void(*)())&stateless::foo);
    package.add("foo", (void(*)(int))&stateless::foo);
  }

  CV* cv = get_cv("stateless::foo", 0);
  REQUIRE(cv != nullptr);
  REQUIRE(GvAV(CvGV(cv)) != nullptr); // overload array should exist
}

TEST_CASE("typemap", "[interpreter][typemap]")
{
  auto my_perl = interp->get();
  auto typemap = perlbind::detail::typemap::get(my_perl);
  auto type_id = perlbind::detail::usertype<struct typemap_test*>::id();
  auto type_name = perlbind::detail::typemap::get_name<struct typemap_test*>(my_perl);
  REQUIRE(!typemap.exists(type_id));
  REQUIRE(type_name == nullptr);

  perlbind::detail::usertype<struct typemap_dummy1*>::id();
  perlbind::detail::usertype<struct typemap_dummy2*>::id();

  interp->new_class<struct typemap_test>("typemap_test");
  auto type_id_after = perlbind::detail::usertype<struct typemap_test*>::id();
  type_name = perlbind::detail::typemap::get_name<struct typemap_test*>(my_perl);
  REQUIRE(type_id == type_id_after);
  REQUIRE(typemap.exists(type_id));
  REQUIRE(type_name != nullptr);
  REQUIRE(strcmp(type_name, "typemap_test") == 0);
}

TEST_CASE("multiple calls to xsub that croaks", "[subcaller][package][function]")
{
  // this test case is for an issue in msvc release builds where xsub croak
  // unwinding would corrupt the stack due to the callback not being extern "C"
  struct multicall
  {
    static void overloaded() {}
    static void overloaded(int, int) {}
  };

  auto package = interp->new_package("multicall");
  package.add("overloaded", (void(*)())&multicall::overloaded);
  package.add("overloaded", (void(*)(int, int))&multicall::overloaded);

  interp->eval(R"script(
    sub testsub { multicall::overloaded('no compatible overload, croak'); }
  )script");

  REQUIRE_THROWS(interp->call_sub<int>("testsub"));
  REQUIRE_THROWS(interp->call_sub<int>("testsub"));
}
