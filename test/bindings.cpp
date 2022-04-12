#include <catch2/catch_test_macros.hpp>

#include <perlbind/perlbind.h>
#include <memory>

extern std::unique_ptr<perlbind::interpreter> interp;

int packagefn(int val) { return val; }
TEST_CASE("package bindings", "[package]")
{
  auto my_perl = interp->get();
  auto package = interp->new_package("foo");
  package.add("packagefn", &packagefn);

  REQUIRE_NOTHROW(interp->eval("$result = foo::packagefn(10);"));
  REQUIRE((get_sv("result", 0) != nullptr && SvIV(get_sv("result", 0)) == 10));
  REQUIRE_THROWS(interp->eval("foo::packagefn();")); // wrong arg count
  REQUIRE_THROWS(interp->eval("foo::missingfn();"));
}

TEST_CASE("static member binding", "[package][function]")
{
  struct foo
  {
    static int static_method() { return 20; }
  };

  auto my_perl = interp->get();
  auto package = interp->new_package("foo");
  package.add("static_method", &foo::static_method);

  REQUIRE_NOTHROW(interp->eval("$result = foo::static_method();"));
  REQUIRE((get_sv("result", 0) != nullptr && SvIV(get_sv("result", 0)) == 20));
}

struct fooclass
{
  static fooclass* get_fooclass();
  int foo_method() { return 30; }
};
fooclass g_fooclass;
fooclass* fooclass::get_fooclass() { return &g_fooclass; }
TEST_CASE("class member binding", "[package][function]")
{
  auto my_perl = interp->get();
  auto package = interp->new_class<fooclass>("fooclassname");
  package.add("get_fooclass", &fooclass::get_fooclass);
  package.add("foo_method", &fooclass::foo_method);

  REQUIRE_NOTHROW(interp->eval("$fooclass = fooclassname::get_fooclass();"));
  SV* rv = get_sv("fooclass", 0);
  REQUIRE(rv != nullptr);
  REQUIRE(SvROK(rv));
  REQUIRE(sv_derived_from(rv, "fooclassname"));
  REQUIRE_NOTHROW(interp->eval("$result = $fooclass->foo_method();"));
  REQUIRE((get_sv("result", 0) != nullptr && SvIV(get_sv("result", 0)) == 30));
  REQUIRE_THROWS(interp->eval("$fooclass->foo_method('invalid args');"));
}

struct derived { static derived* getinst(); };
derived g_derived;
derived* derived::getinst() { return &g_derived; }
TEST_CASE("base class inheritance", "[package]")
{
  struct base
  {
    static int base_method(base* self)
    {
      REQUIRE(static_cast<void*>(self) == &g_derived);
      return 40;
    }
  };

  // inheritance only works with object references
  auto my_perl = interp->get();
  auto package_base = interp->new_class<base>("base");
  package_base.add("base_method", &base::base_method);

  auto package = interp->new_class<derived>("derived");
  package.add_base_class("base");
  package.add("getinst", &derived::getinst);

  REQUIRE_NOTHROW(interp->eval("$derived = derived::getinst();"));
  REQUIRE_NOTHROW(interp->eval("$result = $derived->base_method();"));
  REQUIRE((get_sv("result", 0) != nullptr && SvIV(get_sv("result", 0)) == 40));
}

TEST_CASE("constants", "[package][function]")
{
  auto my_perl = interp->get();
  auto package = interp->new_package("consts");
  package.add_const("none", 0);
  package.add_const("first", 1);
  package.add_const("second", 2);

  SECTION("constant existance")
  {
    REQUIRE(get_cv("consts::none", 0) != nullptr);
    REQUIRE(get_cv("consts::first", 0) != nullptr);
    REQUIRE(get_cv("consts::second", 0) != nullptr);
    REQUIRE(get_cv("consts::missing", 0) == nullptr);
  }

  SECTION("constant values")
  {
    REQUIRE_NOTHROW(interp->eval("$const1 = consts::none;"));
    REQUIRE_NOTHROW(interp->eval("$const2 = consts::first;"));
    REQUIRE_NOTHROW(interp->eval("$const3 = consts::second;"));
    REQUIRE((get_sv("const1", 0) != nullptr && SvIV(get_sv("const1", 0)) == 0));
    REQUIRE((get_sv("const2", 0) != nullptr && SvIV(get_sv("const2", 0)) == 1));
    REQUIRE((get_sv("const3", 0) != nullptr && SvIV(get_sv("const3", 0)) == 2));
  }
}

TEST_CASE("overloads", "[package][function]")
{
  struct foo
  {
    static int bar() { return 1; }
    static int bar(int p1) { return 2; }
    static int bar(float p1) { return 3; }
    static int bar(std::string p1) { return 4; }
    static int bar(int p1, const char* p2) { return 5; }
  };

  auto my_perl = interp->get();
  auto package = interp->new_package("foo");
  package.add("bar", (int(*)())&foo::bar);
  package.add("bar", (int(*)(int))&foo::bar);
  package.add("bar", (int(*)(float))&foo::bar);
  package.add("bar", (int(*)(std::string))&foo::bar);
  package.add("bar", (int(*)(int, const char*))&foo::bar);

  REQUIRE_NOTHROW(interp->eval("$result1 = foo::bar();"));
  REQUIRE_NOTHROW(interp->eval("$result2 = foo::bar(10);"));
  REQUIRE_NOTHROW(interp->eval("$result3 = foo::bar(10.0);"));
  REQUIRE_NOTHROW(interp->eval("$result4 = foo::bar(\"10\");")); // still a string
  REQUIRE_NOTHROW(interp->eval("$result5 = foo::bar(20, \"str\");"));

  REQUIRE((get_sv("result1", 0) != nullptr && SvIV(get_sv("result1", 0)) == 1));
  REQUIRE((get_sv("result2", 0) != nullptr && SvIV(get_sv("result2", 0)) == 2));
  REQUIRE((get_sv("result3", 0) != nullptr && SvIV(get_sv("result3", 0)) == 3));
  REQUIRE((get_sv("result4", 0) != nullptr && SvIV(get_sv("result4", 0)) == 4));
  REQUIRE((get_sv("result5", 0) != nullptr && SvIV(get_sv("result5", 0)) == 5));

  SECTION("non-matching overload")
  {
    REQUIRE_THROWS(interp->eval("$overload4 = foo::bar(20, 10);"));
  }
}