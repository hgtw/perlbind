#include <catch2/catch_test_macros.hpp>

#include <perlbind/perlbind.h>
#include <memory>

extern std::unique_ptr<perlbind::interpreter> interp;

TEST_CASE("scalar construction", "[types]")
{
  perlbind::scalar value;
  REQUIRE(value.is_null());
  value = 1;
  REQUIRE(value.is_integer());
  value = true;
  REQUIRE(value.is_integer());
  value = "str";
  REQUIRE(value.is_string());
  value = 10.0f;
  REQUIRE(value.is_float());

  enum A { none = 0, first = 64 };
  value = A::first;
  REQUIRE((value.is_integer() && static_cast<int>(value) == A::first));

  REQUIRE(SvREFCNT(value.sv()) == 1);
}

TEST_CASE("typecasts from scalar", "[types]")
{
  perlbind::scalar value;

  value = 100;
  int i = value;
  REQUIRE(i == 100);

  value = false;
  bool b = value;
  REQUIRE(b == false);

  value = "foo";
  std::string s = value;
  REQUIRE(s == "foo");

  value = 20.0f;
  float f = value;
  REQUIRE(f == 20.0f);

  enum A { none = 0, first = 64 };
  value = A::first;
  A e1 = value;
  REQUIRE(e1 == A::first);

  enum class B { none = 0, first = 128 };
  value = B::first;
  B e2 = value;
  REQUIRE(e2 == B::first);

  REQUIRE(SvREFCNT(value.sv()) == 1);
}

TEST_CASE("scalar as<T>", "[types]")
{
  perlbind::scalar value = 123;
  REQUIRE(static_cast<int>(value) == value.as<int>());
  value = "str";
  REQUIRE(strcmp(static_cast<const char*>(value), value.as<const char*>()) == 0);
  REQUIRE(strcmp(value.as<const char*>(), value.c_str()) == 0);
}

TEST_CASE("reference", "[types]")
{
  perlbind::scalar foo = 1;
  REQUIRE(SvREFCNT(foo.sv()) == 1);
  {
    perlbind::reference ref(foo);
    REQUIRE(ref.is_reference());
    REQUIRE(SvREFCNT(foo.sv()) == 2);
    REQUIRE(SvREFCNT(ref.sv()) == 1);
    REQUIRE(ref.deref() == foo.sv());
  }
  REQUIRE(SvREFCNT(foo.sv()) == 1);
}

TEST_CASE("reference reset", "[types]")
{
  auto my_perl = interp->get();

  HV* hashval = newHV();
  SV* hashref = newRV_inc((SV*)hashval);
  REQUIRE(SvREFCNT(hashval) == 2);
  REQUIRE(SvREFCNT(hashref) == 1);

  perlbind::reference ref;
  ref.reset(hashref);
  REQUIRE(SvREFCNT(hashval) == 2);
  REQUIRE(SvREFCNT(hashref) == 1);

  SvREFCNT_dec(hashval);
}

TEST_CASE("reference release", "[types]")
{
  perlbind::scalar foo = 1;
  perlbind::reference ref(foo);

  SV* refsv = ref.release(); // unowned reference dangling
  REQUIRE(ref.sv() != refsv);
  REQUIRE(ref.deref() != foo.sv());
  REQUIRE(SvREFCNT(foo.sv()) == 2);
  REQUIRE(SvREFCNT(ref.sv()) == 1);
  REQUIRE(SvREFCNT(refsv) == 1);

  {
    perlbind::reference ref2;
    ref2.reset(refsv); // reacquire ownership of the ref and it should not inc
    REQUIRE(ref2.deref() == foo.sv());
    REQUIRE(SvREFCNT(foo.sv()) == 2);
    REQUIRE(SvREFCNT(ref.sv()) == 1);
    REQUIRE(SvREFCNT(ref2.sv()) == 1);
    REQUIRE(SvREFCNT(refsv) == 1);
  }

  REQUIRE(SvREFCNT(foo.sv()) == 1);
}

TEST_CASE("reference copy construction", "[types]")
{
  perlbind::scalar foo = 1;
  perlbind::reference ref = foo; // creating 1 reference to foo
  REQUIRE(ref.deref() == foo.sv());
  REQUIRE(SvREFCNT(foo.sv()) == 2);
  REQUIRE(SvREFCNT(ref.sv()) == 1);

  SECTION("with arrays")
  {
    perlbind::array arr;
    perlbind::reference arrref = arr;
    REQUIRE(arrref.deref() == arr.sv());
    REQUIRE(SvREFCNT(arr.sv()) == 2);
    REQUIRE(SvREFCNT(arrref.sv()) == 1);
  }

  SECTION("with hash")
  {
    perlbind::hash h;
    perlbind::reference hashref(h);
    REQUIRE(hashref.deref() == h.sv());
    REQUIRE(SvREFCNT(h.sv()) == 2);
    REQUIRE(SvREFCNT(hashref.sv()) == 1);
  }

  SECTION("with raw SV")
  {
    auto my_perl = interp->get();
    SV* value = newSViv(10);
    perlbind::reference ref(value);
    REQUIRE(SvREFCNT(value) == 2);
    REQUIRE(SvREFCNT(ref.sv()) == 1);
    REQUIRE(ref.deref() == value);
    SvREFCNT_dec(value);
  }
}

TEST_CASE("reference rvalue construction", "[types]")
{
  auto my_perl = interp->get();

  SECTION("scalar object")
  {
    perlbind::scalar v(5);
    auto ref = perlbind::reference(std::move(v));
    REQUIRE(SvREFCNT(v.sv()) == 2);
    REQUIRE(SvREFCNT(ref.sv()) == 1);
    REQUIRE(ref.deref() == v.sv());
  }

  SECTION("raw SV")
  {
    HV* hashval = newHV();
    {
      // moving 1 new reference to the hash
      auto ref = perlbind::reference(SvREFCNT_inc(hashval));
      REQUIRE(SvREFCNT(hashval) == 2);
      REQUIRE(ref.deref() == (SV*)hashval);

      // this ref should consume the last non-owned HV's reference
      // bug: this is calling the scalar object constructor instead of SV*
      auto ref2 = perlbind::reference(std::move(hashval));
      REQUIRE(SvREFCNT(hashval) == 2);
      REQUIRE(SvREFCNT(ref.sv()) == 1);
      REQUIRE(ref2.deref() == (SV*)hashval);
    }
    REQUIRE(SvREFCNT(hashval) == 0);
  }
}

TEST_CASE("reference from move constructed scalars should not leak", "[types]")
{
  auto my_perl = interp->get();

  SECTION("scalar object")
  {
    auto ref = perlbind::reference(perlbind::scalar(3).release());
    SV* sv = ref.deref();
    REQUIRE(SvREFCNT(ref.sv()) == 1);
    REQUIRE(SvREFCNT(sv) == 1);
  }

  SECTION("SV*")
  {
    auto ref = perlbind::reference(newSViv(10));
    SV* sv = ref.deref();
    REQUIRE(SvREFCNT(ref.sv()) == 1);
    REQUIRE(SvREFCNT(sv) == 1);
  }
}

TEST_CASE("reference copying, moving, and releasing ref counts", "[types]")
{
  perlbind::scalar foo = 1;
  perlbind::reference ref(foo);

  SV* refsv = ref.release(); // unowned reference dangling
  REQUIRE(ref.deref() != foo.sv());
  REQUIRE(ref.sv() != refsv);
  REQUIRE(SvREFCNT(foo.sv()) == 2);
  REQUIRE(SvREFCNT(ref.sv()) == 1);
  REQUIRE(SvREFCNT(refsv) == 1);

  {
    perlbind::reference ref2;
    ref2.reset(refsv); // take ownership of the released ref and should not incref
    REQUIRE(ref2.deref() == foo.sv());
    REQUIRE(SvREFCNT(foo.sv()) == 2);
    REQUIRE(SvREFCNT(ref.sv()) == 1);
    REQUIRE(SvREFCNT(ref2.sv()) == 1);
  }

  REQUIRE(SvREFCNT(foo.sv()) == 1);
}

TEST_CASE("reference to a reference", "[types]")
{
  perlbind::scalar foo = 1;
  perlbind::reference ref(foo);
  perlbind::reference ref2(ref);
  REQUIRE(ref.is_reference());
  REQUIRE(ref2.is_reference());
  REQUIRE(SvROK(*ref2));
  REQUIRE(*ref2 == ref.sv());
  REQUIRE(SvREFCNT(foo.sv()) == 2);
  REQUIRE(SvREFCNT(ref.sv()) == 2);
  REQUIRE(SvREFCNT(ref2.sv()) == 1);
}

TEST_CASE("scalar reference holding reference to unscoped scalar", "[types]")
{
  perlbind::reference ref;
  {
    perlbind::scalar foo = 1;
    ref = perlbind::reference(foo);
    REQUIRE(SvREFCNT(foo.sv()) == 2);
    REQUIRE(SvREFCNT(ref.sv()) == 1);
    REQUIRE(SvREFCNT(ref.deref()) == 2);
  }
  REQUIRE(SvREFCNT(ref.sv()) == 1);
  REQUIRE(SvREFCNT(ref.deref()) == 1);
}

TEST_CASE("arrays", "[types]")
{
  perlbind::array arr;
  arr.push_back(1);
  arr.push_back("two");
  arr.push_back(3.0f);

  REQUIRE(arr.size() == 3);
  REQUIRE(static_cast<int>(arr[0]) == 1);
  REQUIRE(arr[0].as<int>() == 1);
  REQUIRE(strcmp(static_cast<const char*>(arr[1]), "two") == 0);
  REQUIRE(strcmp(arr[1].as<const char*>(), "two") == 0);
  REQUIRE(static_cast<float>(arr[2]) == 3.0f);
  REQUIRE(arr[2].as<float>() == 3.0f);
}

TEST_CASE("array index proxy", "[types]")
{
  auto my_perl = interp->get();

  perlbind::array arr;
  arr.push_back(1);
  arr.push_back("two");
  arr.push_back(3.0f);

  int          copy_int = arr[0];
  std::string  copy_str = arr[1];
  float        copy_float = arr[2];
  perlbind::scalar first = arr[0];

  REQUIRE(arr[0].sv() == first.sv());  // scalar should own a reference to source

  // array and temp proxy index hold reference to array elements
  // only the scalar should hold another reference (not a copy)
  REQUIRE(SvREFCNT(arr[0].sv()) == 3); //SvREFCNT(first.sv()) == 2
  REQUIRE(SvREFCNT(arr[1].sv()) == 2);
  REQUIRE(SvREFCNT(arr[2].sv()) == 2);

  REQUIRE(copy_int == 1);
  REQUIRE(copy_str == "two");
  REQUIRE(copy_float == 3.0f);

  first = 20;
  REQUIRE(static_cast<int>(arr[0]) == 20); // should be modified through scalar
}

TEST_CASE("array index proxy assignment", "[types]")
{
  auto my_perl = interp->get();

  perlbind::array arr;
  arr.push_back(1);
  arr.push_back("two");
  arr.push_back(3.0f);

  arr[0] = 99;
  arr[1] = "bar";
  arr[2] = 99.0f;

  std::string s = arr[1];
  REQUIRE(static_cast<int>(arr[0]) == 99);
  REQUIRE(s == "bar");
  REQUIRE(static_cast<float>(arr[2]) == 99.0f);

  REQUIRE(SvREFCNT(arr[0].sv()) == 2);
  REQUIRE(SvREFCNT(arr[1].sv()) == 2);
  REQUIRE(SvREFCNT(arr[2].sv()) == 2);
}

TEST_CASE("array from reference", "[types]")
{
  perlbind::scalar s;
  perlbind::reference sref(s);
  REQUIRE_THROWS(perlbind::array(sref));

  perlbind::array a;
  perlbind::reference aref(a);
  REQUIRE_NOTHROW(perlbind::array(aref));

  perlbind::hash h;
  perlbind::reference href(h);
  REQUIRE_THROWS(perlbind::array(href));

  SECTION("reference counts")
  {
    perlbind::array src;
    {
      perlbind::reference ref(src);

      perlbind::array arr = ref;
      REQUIRE(arr.sv() == src.sv());
      REQUIRE(SvREFCNT(src.sv()) == 3);
    }
    REQUIRE(SvREFCNT(src.sv()) == 1);
  }
}

TEST_CASE("array from index proxy", "[types]")
{
  perlbind::array arr1;
  arr1.push_back(1);

  perlbind::array arr2;
  arr2.push_back(perlbind::reference(arr1));

  REQUIRE_THROWS(perlbind::array(arr1[0]));
  REQUIRE_NOTHROW(perlbind::array(arr2[0]));
}

TEST_CASE("array with reference element", "[types]")
{
  perlbind::scalar v = 1000;

  perlbind::array arr;
  arr.push_back(1);
  arr.push_back(perlbind::reference(v));

  REQUIRE(arr[1].sv());
}

TEST_CASE("nested array reference", "[types]")
{
  perlbind::array arr1;
  arr1.push_back(100);
  arr1.push_back(200);

  {
    perlbind::array arr2;
    arr2.push_back(perlbind::reference(arr1));

    {
      perlbind::array read = arr2[0];
      REQUIRE(read.sv() == arr1.sv());
      REQUIRE(static_cast<int>(read[0]) == 100);
      REQUIRE(static_cast<int>(read[1]) == 200);
    }
  }

  REQUIRE(SvREFCNT(arr1.sv()) == 1);
}

TEST_CASE("hash", "[types]")
{
  perlbind::hash h;
  h.insert("Key1", "value");
  h.insert("Key2", 1);
  h["Key3"] = 2.0f;

  REQUIRE(h.size() == 3);

  std::string s = h["Key1"];
  REQUIRE(s == "value");
}

TEST_CASE("hash construction from reference", "[types]")
{
  perlbind::scalar s;
  perlbind::reference sref(s);
  REQUIRE_THROWS(perlbind::hash(sref));

  perlbind::array a;
  perlbind::reference aref(a);
  REQUIRE_THROWS(perlbind::hash(aref));

  perlbind::hash h;
  perlbind::reference href(h);
  REQUIRE_NOTHROW(perlbind::hash(href));

  SECTION("reference counts")
  {
    perlbind::hash src;
    {
      perlbind::reference ref(src);

      perlbind::hash hash = ref;
      REQUIRE(hash.sv() == src.sv());
      REQUIRE(SvREFCNT(src.sv()) == 3);
    }
    REQUIRE(SvREFCNT(src.sv()) == 1);
  }
}

TEST_CASE("hash construction from proxy index", "[types]")
{
  perlbind::hash dummy;
  dummy.insert("key", perlbind::reference(perlbind::hash()));
  dummy.insert("bad", perlbind::reference(perlbind::array()));

  REQUIRE_NOTHROW(perlbind::hash(dummy["key"]));
  REQUIRE_THROWS(perlbind::hash(dummy["bad"]));
  REQUIRE_THROWS(perlbind::hash(dummy["missing"]));
}

TEST_CASE("hash index proxy reference counts", "[types]")
{
  perlbind::hash h;
  h["Key1"] = "value";

  REQUIRE(SvREFCNT(h["Key1"].sv()) == 2); // underlying scalar and temporary index proxy

  perlbind::scalar v = h["Key1"]; // scalar should hold a ref
  REQUIRE(SvREFCNT(h["Key1"].sv()) == 3);
  REQUIRE(SvREFCNT(v.sv()) == 2);

  auto v2 = h["Key1"]; // auto is another proxy and should hold a ref
  REQUIRE(SvREFCNT(h["Key1"].sv()) == 4);
  REQUIRE(SvREFCNT(v2.sv()) == 3);
}

TEST_CASE("nested hash reference counts", "[types]")
{
  perlbind::hash h1;
  h1.insert("foo1", 50);

  {
    perlbind::hash h2;
    h2.insert("foo2", 100);
    h2.insert("nest1", perlbind::reference(h1));
    h2["nest2"] = perlbind::reference(h1);

    REQUIRE(SvREFCNT(h1.sv()) == 3);
  }

  REQUIRE(SvREFCNT(h1.sv()) == 1);
}

TEST_CASE("hash inserts and reads", "[types]")
{
  SECTION("scalars")
  {
    perlbind::hash h;
    h["foo1"] = 50;
    h.insert("foo2", 100);

    perlbind::scalar v1 = h["foo1"]; // should be +1 on foo1
    perlbind::scalar v2 = h["foo2"]; // should be +1 on foo2
    REQUIRE(v1.type() == v2.type());
    REQUIRE(SvREFCNT(v1.sv()) == SvREFCNT(v2.sv()));
    REQUIRE(SvREFCNT(v1.sv()) == 2);
  }

  SECTION("references", "[types]")
  {
    perlbind::scalar s1 = 50;
    perlbind::scalar s2 = 100;
    perlbind::hash src;

    perlbind::hash h;
    h["foo1"] = perlbind::reference(s1);
    h.insert("foo2", perlbind::reference(s2));
    h["foo3"] = perlbind::reference(src);

    perlbind::scalar v1 = h["foo1"];
    perlbind::scalar v2 = h["foo2"];
    perlbind::scalar v3 = h["foo3"];
    perlbind::reference v4;
    v4.reset(SvREFCNT_inc(h["foo3"]));

    REQUIRE(v1.is_scalar_ref());
    REQUIRE(v1.type() == v2.type());
    REQUIRE(v3.is_hash_ref());
    REQUIRE(v4.is_hash_ref());

    REQUIRE(SvREFCNT(v1.sv()) == 2); // hash element and read object
    REQUIRE(SvREFCNT(v2.sv()) == 2);
    REQUIRE(SvREFCNT(v3.sv()) == 3); // hash element, 2x read objects
    REQUIRE(SvREFCNT(v4.sv()) == 3);
  }
}

TEST_CASE("creating array and hash references through proxy index")
{
  SECTION("array")
  {
    perlbind::hash all;
    if (!all.exists("key"))
    {
      all["key"] = perlbind::reference(perlbind::array());
      REQUIRE(all.exists("key"));
    }
    perlbind::scalar entry = all["key"];
    REQUIRE(entry.is_array_ref());
  }

  SECTION("hash")
  {
    perlbind::hash all;
    if (!all.exists("key"))
    {
      all["key"] = perlbind::reference(perlbind::hash());
      REQUIRE(all.exists("key"));
    }
    perlbind::scalar entry = all["key"];
    REQUIRE(entry.is_hash_ref());
  }
}

TEST_CASE("insert into nested hash proxy index during iteration")
{
  perlbind::hash all;

  for (int i = 0; i < 3; ++i)
  {
    perlbind::scalar key = all["key"];
    if (key.is_null()) // all.exists("key")
    {
      all["key"] = perlbind::reference(perlbind::hash());
    }

    perlbind::hash nested = all["key"];
    nested["k" + std::to_string(i)] = i + 1;
  }

  REQUIRE(all.exists("key"));

  SV* refsv = nullptr;
  {
    perlbind::scalar ref = all["key"];
    REQUIRE(ref.is_hash_ref());
    REQUIRE(SvREFCNT(ref.sv()) == 2);
    refsv = ref.sv();
  }

  {
    perlbind::hash nested = all["key"];
    REQUIRE(nested.size() == 3);
    REQUIRE(static_cast<int>(nested["k0"]) == 1);
    REQUIRE(static_cast<int>(nested["k1"]) == 2);
    REQUIRE(static_cast<int>(nested["k2"]) == 3);
    REQUIRE(SvREFCNT(nested.sv()) == 2); // underlying hash and our object
    REQUIRE(SvREFCNT(all.sv()) == 1);
  }

  REQUIRE(SvREFCNT(all.sv()) == 1);
}

TEST_CASE("reading nested hash", "[types]")
{
  perlbind::hash h1;
  h1.insert("foo1", 50);

  {
    perlbind::hash h2;
    h2.insert("foo2", 100);
    h2.insert("nest1", perlbind::reference(h1));
    h2.insert("nest2", perlbind::reference(h1));

    {
      // ref1 should be reference to the hashref which increments the underlying hash
      perlbind::hash hash1access = h2["nest1"];
      REQUIRE(hash1access.sv() == h1.sv());
      REQUIRE(SvREFCNT(h1.sv()) == 4); // should be: orig, 2x ref in h2, hash1access
      REQUIRE(hash1access.exists("foo1"));
      REQUIRE(static_cast<int>(hash1access["foo1"]) == 50);
    }
  }

  REQUIRE(SvREFCNT(h1.sv()) == 1);
}

TEST_CASE("hash proxy with non-existent keys", "[types]")
{
  // hv_fetch/av_fetch will return an undefined value for non-existent keys
  // but this should default to 0/empty through scalar typecast operators
  perlbind::hash h;
  h["foo0"] = 3;
  int v1 = h["foo0"];
  int v2 = h["foo1"];
  float f = h["foo2"];
  std::string s = h["foo3"];

  REQUIRE(v1 == 3);
  REQUIRE(v2 == 0);
  REQUIRE(f == 0.0f);
  REQUIRE(s.empty());
}

TEST_CASE("array iterator ref count", "[types]")
{
  perlbind::array arr;
  arr.push_back(100);
  SV* src = arr[0].sv();

  REQUIRE(SvREFCNT(src) == 1);
  {
    auto it = arr.begin();
    REQUIRE(SvREFCNT(src) == 2); // begin() fetches first item
    REQUIRE((*it).sv() == src);
  }
  REQUIRE(SvREFCNT(src) == 1);
}

TEST_CASE("array range loops", "[types]")
{
  perlbind::array arr;
  arr.push_back(100);
  SV* src = arr[0].sv();

  SECTION("copy")
  {
    for (auto it : arr)
    {
      REQUIRE(SvREFCNT(it.sv()) == 1);
      REQUIRE(it.sv() != src);
      it = 300;
    }
    REQUIRE(static_cast<int>(arr[0]) == 100);
  }

  SECTION("reference")
  {
    for (auto& it : arr)
    {
      REQUIRE(it.sv() == src);
      it = 300;
    }
    REQUIRE(static_cast<int>(arr[0]) == 300);
  }

  SECTION("array containing references to hash", "[types]")
  {
    perlbind::hash h;
    {
      perlbind::array src;
      src.push_back(1);
      src.push_back(perlbind::reference(h));
      src.push_back(perlbind::reference(h));
      src.push_back(perlbind::reference(h));
      src.push_back("str");

      int count = 0;
      for (perlbind::scalar item : src)
      {
        if (item.is_hash_ref())
        {
          REQUIRE(SvREFCNT(*item) == 5); // each should be 1x orig, 3x refs, 1x loop
          ++count;
        }
      }
      REQUIRE(count == 3);
    }
    REQUIRE(SvREFCNT(h.sv()) == 1);
  }

  REQUIRE(SvREFCNT(src) == 1);
}

TEST_CASE("hash iterator ref count", "[types]")
{
  perlbind::hash table;
  table["foo"] = 100;
  SV* src = table["foo"].sv();

  REQUIRE(SvREFCNT(src) == 1);
  {
    auto it = table.begin();
    REQUIRE(SvREFCNT(src) == 2); // begin() fetches first item
    REQUIRE((*it).second.sv() == src);
  }
  REQUIRE(SvREFCNT(src) == 1);
}

TEST_CASE("hash iterator range loop", "[types]")
{
  perlbind::hash table;
  table["foo"] = 100;
  SV* src = table["foo"].sv();

  SECTION("copy")
  {
    for (auto it : table)
    {
      REQUIRE(SvREFCNT(it.second.sv()) == 1);
      REQUIRE(it.second.sv() != src);
      it.second = 300;
    }
    REQUIRE(static_cast<int>(table["foo"]) == 100);
  }

  SECTION("reference")
  {
    for (auto& it : table)
    {
      REQUIRE(it.second.sv() == src);
      REQUIRE(static_cast<int>(it.second) == 100);
      it.second = 300;
    }
    REQUIRE(static_cast<int>(table["foo"]) == 300);
  }

  REQUIRE(SvREFCNT(src) == 1);
}

TEST_CASE("hash find")
{
  perlbind::hash table;
  table["foo"] = 100;
  SV* src = table["foo"].sv();

  SECTION("exists")
  {
    auto it = table.find("foo");
    REQUIRE(it != table.end());
    REQUIRE(strcmp(static_cast<const char*>(it->first), "foo") == 0);
    REQUIRE(static_cast<int>(it->second) == 100);
  }

  SECTION("missing")
  {
    auto it = table.find("missing");
    REQUIRE(it == table.end());
  }

  REQUIRE(SvREFCNT(src) == 1);
}

TEST_CASE("scalar cast from object reference to pointer", "[types]")
{
  struct foo {};
  foo f;
  foo* foo_ptr = &f;

  auto my_perl = interp->get();
  interp->new_class<foo>("fooclass");

  SV* rv = sv_newmortal();
  SV* sv = sv_setref_pv(rv, "fooclass", static_cast<void*>(foo_ptr));

  perlbind::scalar scalar = SvREFCNT_inc(sv);
  foo* from = static_cast<foo*>(scalar);
  REQUIRE(from == foo_ptr);
}

TEST_CASE("proxy assignment to another proxy", "[types]")
{
  perlbind::array a;
  a.push_back(1111);
  SV* orig = a[0].sv();

  perlbind::array b;
  b[0] = a[0]; // this should copy the scalar proxy value

  REQUIRE(static_cast<int>(b[0]) == 1111);
  REQUIRE(b[0].sv() != a[0].sv());
  REQUIRE(SvREFCNT(orig) == 1);
}
