#include <catch2/catch_test_macros.hpp>

#include <perlbind/perlbind.h>
#include <memory>

extern std::unique_ptr<perlbind::interpreter> interp;

TEST_CASE("push and read enums", "[stack]")
{
  enum eFoo { none = 0, first = 5 };
  enum class eFooClass { none = 0, first = 15 };
  struct foo
  {
    static eFoo read_enum(eFoo e)
    {
      REQUIRE(e == eFoo::first);
      return e;
    }
    static eFooClass read_enum_class(eFooClass e)
    {
      REQUIRE(e == eFooClass::first);
      return e;
    }
  };

  auto my_perl = interp->get();
  auto package = interp->new_package("foo");
  package.add("read_enum", &foo::read_enum);
  package.add("read_enum_class", &foo::read_enum_class);

  SECTION("enum")
  {
    auto ns = interp->new_package("eFoo");
    ns.add_const("none", eFoo::none);
    ns.add_const("first", eFoo::first);

    REQUIRE_NOTHROW(interp->eval("$result = foo::read_enum(eFoo::first);"));
    REQUIRE((get_sv("result", 0) != nullptr && SvIV(get_sv("result", 0)) == (int)eFoo::first));
  }

  SECTION("enum class")
  {
    auto ns = interp->new_package("eFooClass");
    ns.add_const("none", eFooClass::none);
    ns.add_const("first", eFooClass::first);

    REQUIRE_NOTHROW(interp->eval("$result = foo::read_enum_class(eFooClass::first);"));
    REQUIRE((get_sv("result", 0) != nullptr && SvIV(get_sv("result", 0)) == (int)eFooClass::first));
  }
}

TEST_CASE("push array to perl stack", "[stack][types]")
{
  struct foo
  {
    static perlbind::array get_array()
    {
      perlbind::array arr;
      arr.push_back(1);
      arr.push_back("two");
      arr.push_back(3.0f);
      return arr;
    }
  };

  auto my_perl = interp->get();
  auto package = interp->new_package("foo");
  package.add("get_array", &foo::get_array);

  REQUIRE_NOTHROW(interp->eval(R"script(
    @arr = foo::get_array();
    $size = @arr;
    $val1 = $arr[0];
    $val2 = $arr[1];
    $val3 = $arr[2];
  )script"));

  REQUIRE((get_sv("size", 0) != nullptr && SvIV(get_sv("size", 0)) == 3));
  REQUIRE((get_sv("val1", 0) != nullptr && SvIV(get_sv("val1", 0)) == 1));
  REQUIRE((get_sv("val2", 0) != nullptr && std::string(SvPV_nolen(get_sv("val2", 0))) == "two"));
  REQUIRE((get_sv("val3", 0) != nullptr && SvNV(get_sv("val3", 0)) == 3.0f));
}

TEST_CASE("push array reference to perl stack", "[stack][types]")
{
  struct foo
  {
    static perlbind::reference get_array_ref()
    {
      perlbind::array arr;
      arr.push_back(1);
      arr.push_back("two");
      arr.push_back(3.0f);
      return perlbind::reference(arr);
    }
  };

  auto my_perl = interp->get();
  auto package = interp->new_package("foo");
  package.add("get_array_ref", &foo::get_array_ref);

  REQUIRE_NOTHROW(interp->eval(R"script(
    $ref = foo::get_array_ref();
    $size = scalar @$ref;
    $val1 = $ref->[0];
    $val2 = $ref->[1];
    $val3 = ${ $ref }[2]
  )script"));

  REQUIRE((get_sv("size", 0) != nullptr && SvIV(get_sv("size", 0)) == 3));
  REQUIRE((get_sv("val1", 0) != nullptr && SvIV(get_sv("val1", 0)) == 1));
  REQUIRE((get_sv("val2", 0) != nullptr && std::string(SvPV_nolen(get_sv("val2", 0))) == "two"));
  REQUIRE((get_sv("val3", 0) != nullptr && SvNV(get_sv("val3", 0)) == 3.0f));
}

TEST_CASE("read auto dereferenced array from perl", "[stack][types]")
{
  struct foo
  {
    static int send_array(perlbind::array arr)
    {
      REQUIRE(arr.size() == 4);
      REQUIRE(static_cast<int>(arr[0]) == 4);
      REQUIRE(static_cast<int>(arr[1]) == 3);
      REQUIRE(static_cast<int>(arr[2]) == 2);
      REQUIRE(static_cast<int>(arr[3]) == 1);
      return 4000;
    }
  };

  auto my_perl = interp->get();
  auto package = interp->new_package("foo");
  package.add("send_array", &foo::send_array);

  REQUIRE_NOTHROW(interp->eval(R"script(
    @arr = (4, 3, 2, 1);
    $result = foo::send_array(\@arr);
  )script"));

  REQUIRE((get_sv("result", 0) != nullptr && SvIV(get_sv("result", 0)) == 4000));
}

TEST_CASE("read array reference from perl", "[stack][types]")
{
  struct foo
  {
    static int send_array_as_ref(perlbind::reference ref)
    {
      REQUIRE(ref.is_array_ref());
      REQUIRE(SvREFCNT(ref.sv()) == 2); // original RV on stack and our reference wrapper hold refs
      REQUIRE(SvREFCNT(*ref) == 2);     // underlying array should have 2 refs (original and stack RV)

      {
        perlbind::reference wrap;
        wrap.reset(SvREFCNT_inc(ref));
        REQUIRE(SvREFCNT(ref.sv()) == 3); // wrapper takes a reference to the RV
      }

      REQUIRE(SvREFCNT(ref.sv()) == 2);

      {
        perlbind::array arr;
        arr.reset(reinterpret_cast<AV*>(SvREFCNT_inc(*ref)));
        REQUIRE(SvREFCNT(ref.sv()) == 2);
        REQUIRE(SvREFCNT(*ref) == 3);
      }

      REQUIRE(SvREFCNT(ref.sv()) == 2);
      REQUIRE(SvREFCNT(*ref) == 2);

      return 5000;
    }
  };

  auto my_perl = interp->get();
  auto package = interp->new_package("foo");
  package.add("send_array_as_ref", &foo::send_array_as_ref);

  REQUIRE_NOTHROW(interp->eval(R"script(
    @arr = (4, 3, 2, 1);
    $result = foo::send_array_as_ref(\@arr);
  )script"));

  REQUIRE((get_sv("result", 0) != nullptr && SvIV(get_sv("result", 0)) == 5000));
}

TEST_CASE("push hash to perl stack", "[stack][types]")
{
  struct foo
  {
    static perlbind::hash get_hash()
    {
      perlbind::hash result;
      result["key1"] = 1;
      result["key2"] = "two";
      result.insert("key3", 3.0f);
      return result;
    }
  };

  auto my_perl = interp->get();
  auto package = interp->new_package("foo");
  package.add("get_hash", &foo::get_hash);

  REQUIRE_NOTHROW(interp->eval(R"script(
    %hash = foo::get_hash();
    $size = scalar keys %hash;
    $val1 = $hash{key1};
    $val2 = $hash{key2};
    $val3 = $hash{key3};
  )script"));

  REQUIRE((get_sv("size", 0) != nullptr && SvIV(get_sv("size", 0)) == 3));
  REQUIRE((get_sv("val1", 0) != nullptr && SvIV(get_sv("val1", 0)) == 1));
  REQUIRE((get_sv("val2", 0) != nullptr && std::string(SvPV_nolen(get_sv("val2", 0))) == "two"));
  REQUIRE((get_sv("val3", 0) != nullptr && SvNV(get_sv("val3", 0)) == 3.0f));
}

TEST_CASE("push hash reference to perl stack", "[stack][types]")
{
  struct foo
  {
    static perlbind::reference get_hash_ref()
    {
      perlbind::hash result;
      result["key1"] = 1;
      result["key2"] = "two";
      result.insert("key3", 3.0f);
      return perlbind::reference(result);
    }
  };

  auto my_perl = interp->get();
  auto package = interp->new_package("foo");
  package.add("get_hash_ref", &foo::get_hash_ref);

  REQUIRE_NOTHROW(interp->eval(R"script(
    $hashref = foo::get_hash_ref();
    $size = scalar keys %$hashref;
    $val1 = $hashref->{key1};
    $val2 = $hashref->{key2};
    $val3 = $hashref->{key3};
  )script"));

  REQUIRE((get_sv("size", 0) != nullptr && SvIV(get_sv("size", 0)) == 3));
  REQUIRE((get_sv("val1", 0) != nullptr && SvIV(get_sv("val1", 0)) == 1));
  REQUIRE((get_sv("val2", 0) != nullptr && std::string(SvPV_nolen(get_sv("val2", 0))) == "two"));
  REQUIRE((get_sv("val3", 0) != nullptr && SvNV(get_sv("val3", 0)) == 3.0f));
}

TEST_CASE("read hash from perl", "[stack][types]")
{
  struct foo
  {
    static int send_hash(perlbind::reference ref)
    {
      REQUIRE(ref.is_hash_ref());
      REQUIRE(SvREFCNT(ref.sv()) == 2); // stack RV and our wrapper
      REQUIRE(SvREFCNT(*ref) == 2); // original hash and stack RV holds one

      perlbind::hash h;
      h.reset(reinterpret_cast<HV*>(SvREFCNT_inc(*ref)));

      REQUIRE(SvREFCNT(ref.sv()) == 2);
      REQUIRE(SvREFCNT(*ref) == 3); // we wrapped a ref to the underlying hash

      REQUIRE(h.size() == 3);
      REQUIRE(static_cast<int>(h["k1"]) == 99);
      std::string s = h["k2"];
      REQUIRE(s == "val");
      REQUIRE(static_cast<float>(h["k3"]) == 5.0f);

      return 6000;
    }
  };

  auto my_perl = interp->get();
  auto package = interp->new_package("foo");
  package.add("send_hash", &foo::send_hash);

  REQUIRE_NOTHROW(interp->eval(R"script(
    %h = ('k1' => 99, 'k2' => 'val', 'k3' => 5.0);
    $result = foo::send_hash(\%h);
  )script"));

  REQUIRE((get_sv("result", 0) != nullptr && SvIV(get_sv("result", 0)) == 6000));
}