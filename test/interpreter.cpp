#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>

#include <perlbind/perlbind.h>

#include <fstream>
#include <memory>

// interpreter for all tests (can only create once per program due to PERL_SYS_INIT3/PERL_SYS_TERM)
std::unique_ptr<perlbind::interpreter> interp;

int main(int argc, char* argv[])
{
  interp = std::make_unique<perlbind::interpreter>();

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
    perlbind::interpreter view(my_perl, false);
    auto package = view.new_package("stateless");
    package.add("foo", (void(*)())&stateless::foo);
    package.add("foo", (void(*)(int))&stateless::foo);
  }

  CV* cv = get_cv("stateless::foo", 0);
  REQUIRE(cv != nullptr);
  REQUIRE(GvAV(CvGV(cv)) != nullptr); // overload array should exist
}
