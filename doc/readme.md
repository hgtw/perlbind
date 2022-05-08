# Building Tests

Define the `PERLBIND_BUILD_TESTS` CMake option to build tests.

# Function Bindings

Arguments from perl to function bindings are checked for compatibility with
a function's parameters before it is called. Pointers will not be `nullptr`
(unless using `perlbind::nullable<T>`) and types will be compatible with the
parameters (depending on config options). If a function is called with invalid
arguments then it croaks with an error.

# Configuration Options

By default scalar integers and floats are not distinguished in function
arguments. This means overloads that depend on those types being different
will not work without defining `PERLBIND_STRICT_NUMERIC_TYPES` before including
the library.

Strings are treated as a separate type by default. This can be overridden
by defining the `PERLBIND_NO_STRICT_SCALAR_TYPES` option which will treat
all scalar types the same. This option overrides `PERLBIND_STRICT_NUMERIC_TYPES` if set.

> Config options should be defined before including the library everywhere it's included. Failure to do so will result in undefined behavior.

`PERLBIND_STRICT_NUMERIC_TYPES`
- Integers and floating point numbers will be treated as distinct types.
- Required for overloads that depend on int/float type differences

`PERLBIND_NO_STRICT_SCALAR_TYPES`
- Integers, floats, and strings will all be treated as the same type for function overloads
- Overrides `PERLBIND_STRICT_NUMERIC_TYPES` if set

# Overloads

Overloads work by finding the first compatible match by default. If one cannot
be found then a `std::runtime_error` is thrown. For scalar arguments this will
depend on scalar configuration options. For example if a function is overloaded
and the only difference is an int versus float argument, then whichever is
registered first will be called unless `PERLBIND_STRICT_NUMERIC_TYPES` is defined.

> Note that different integer types cannot be differentiated such as `int8_t` vs `int`

```cpp
 // for example, overloads only differing by an int and float argument registered in this order:
 void foo(int);
 void foo(float);

 // default (without PERLBIND_STRICT_NUMERIC_TYPES)
 foo(1.0); // will choose void foo(int);

 // with PERLBIND_STRICT_NUMERIC_TYPES
 foo(1.0); // will choose void foo(float);
```

# Specializing Stack Reader Types

It is possible to specialize the stack reader to allow custom types as function
binding parameters.

For example, to add support for `fmt::string_view` function parameters:
```cpp
template <>
struct perlbind::stack::read_as<fmt::string_view>
  : perlbind::stack::read_as<const char*>
{};
```

If more control is needed over the implementation, then the specialization can
implement the static `check` and `get` functions instead of inheriting from
an existing reader (see `stack_read.h`):

```cpp
template <>
struct perlbind::stack::read_as<fmt::string_view>
{
 static bool check(PerlInterpreter* my_perl, int i, int ax, int items)
 {
   return perlbind::stack::read_as<const char*>::check(my_perl, i, ax, items);
 }

 static fmt::string_view get(PerlInterpreter* my_perl, int i, int ax, int items)
 {
   return perlbind::stack::read_as<const char*>::get(my_perl, i, ax, items);
 }
};
```

# Interpreter

A `perlbind::interpreter` can either create a new perl interpreter or be constructed using
an existing one. If constructed with an existing interpreter then the object can safely be
destroyed since it will be non-owning. A non-owning interpreter object is stateless. All
binding data is stored inside perl's interpreter.

`load_script`<br/>
Load the specified script file into the package name. Throws `std::runtime_error` on error.

`call_sub<T>`<br/>
Call the specified sub with variable arguments. Expected return type is 'T'.
> Only supports `integral` and `void` return types currently

`eval`<br/>
Evaluate the specified perl string. Throws `std::runtime_error` on error.

`new_package`<br/>
Returns a `perlbind::package` interface to the specified package in perl. If
the package doesn't already exist it will be created.

`new_class<T>`<br/>
Returns a `perlbind::package` aliased as a `perlbind::class_<T>` to the specified
package. The template typename `T` is registered in an internal typemap hash on
the perl interpreter. This typemap is used for blessing object pointer return
values and to verify object pointer arguments in function bindings.

> If a type is not registered then any function bindings that return or use a
> pointer to an object of that type will be unusable. Unregistered types are
> only detectable at runtime and will croak if detected.

# Types

`perlbind::scalar`<br/>
A perl scalar that can represent a number, string, or reference

`perlbind::reference`<br/>
Inherits from scalar. Used to create references to other perl data and indicate
expected types in function bindings.

`perlbind::array`<br/>
Wrapper around a perl AV to hold a list of scalars.

`perlbind::hash`<br/>
Wrapper around a perl HV which associates key strings with scalar values.

`perlbind::nullable<T>`<br/>
Using this as a parameter in function bindings bypasses the strict type check for
an expected pointer type. If the argument is not a valid reference deriving from
the specified pointer type then a `nullptr` is passed instead. This differs from
default behavior which would either croak or treat the function as incompatible
(for overloads) due to an invalid expected argument.

> Function bindings that have a `perlbind::array` or `perlbind::hash` parameter
> may not have any other parameters. These types are variable length so the rest
> of the stack will be consumed as part of them. A `perlbind::reference` should
> be used instead to support passing array or hash references with other arguments.

# Examples

## Binding classes

```cpp
#include <perlbind/perlbind.h>
#include <string>

struct foo_base
{
  int get_number() { return 1; }
  int get_number(int a) { return a; }
  virtual std::string get_virtual_str() = 0;
};

struct derived : foo_base
{
  std::string get_string() { return m_str; }
  std::string get_virtual_str() override { return m_virtual_str; }
private:
  std::string m_str = "my string";
  std::string m_virtual_str = "my virtual string";
};

derived g_derived;

derived* get_derived()
{
  return &g_derived;
}

int main()
{
  perlbind::interpreter state;

  auto base_package = state.new_class<foo_base>("base");
  base_package.add("get_number", (int(foo_base::*)())&foo_base::get_number);
  base_package.add("get_number", (int(foo_base::*)(int))&foo_base::get_number);
  base_package.add("get_virtual_str", &foo_base::get_virtual_str);

  auto package = state.new_class<derived>("derived");
  package.add_base_class("base");
  package.add("get_string", &derived::get_string);

  auto test = state.new_package("test");
  test.add("get_derived", &get_derived);

  try
  {
    state.load_script("main", "script.pl");
    state.call_sub<void>("main::testsub");
  }
  catch (std::exception& e)
  {
    printf("error loading script: %s\n", e.what());
  }

  return 0;
}
```
```perl
# script.pl
sub testsub {
  my $derived = test::get_derived();
  print($derived->get_number() . "\n");      # 1
  print($derived->get_number(50) . "\n");    # 50
  print($derived->get_string() . "\n");      # "my string"
  print($derived->get_virtual_str() . "\n"); # "my virtual string"
}
```
