#pragma once

#include <string>

namespace perlbind { namespace stack {

// perl stack reader to convert types, croaks if perl stack value isn't type compatible
template <typename T, typename = void>
struct read_as;

template <typename T>
struct read_as<T, std::enable_if_t<std::is_integral<T>::value || std::is_enum<T>::value>>
{
  static bool check(PerlInterpreter* my_perl, int i, int ax, int items)
  {
    return SvIOK(ST(i));
  }

  static T get(PerlInterpreter* my_perl, int i, int ax, int items)
  {
    if (!check(my_perl, i, ax, items))
    {
      Perl_croak(aTHX_ "expected argument %d to be an integer", i+1);
    }
    return static_cast<T>(SvIV(ST(i))); // unsigned and bools casted
  }
};

template <typename T>
struct read_as<T, std::enable_if_t<std::is_floating_point<T>::value>>
{
  static bool check(PerlInterpreter* my_perl, int i, int ax, int items)
  {
    return SvNOK(ST(i));
  }

  static T get(PerlInterpreter* my_perl, int i, int ax, int items)
  {
    if (!check(my_perl, i, ax, items))
    {
      Perl_croak(aTHX_ "expected argument %d to be a floating point", i+1);
    }
    return static_cast<T>(SvNV(ST(i)));
  }
};

template <>
struct read_as<const char*>
{
  static bool check(PerlInterpreter* my_perl, int i, int ax, int items)
  {
    return SvPOK(ST(i));
  }

  static const char* get(PerlInterpreter* my_perl, int i, int ax, int items)
  {
    if (!check(my_perl, i, ax, items))
    {
      Perl_croak(aTHX_ "expected argument %d to be a string", i+1);
    }
    return static_cast<const char*>(SvPV_nolen(ST(i)));
  }
};

template <>
struct read_as<std::string> : read_as<const char*>
{
};

template <>
struct read_as<void*>
{
  static bool check(PerlInterpreter* my_perl, int i, int ax, int items)
  {
    return SvROK(ST(i));
  }

  static void* get(PerlInterpreter* my_perl, int i, int ax, int items)
  {
    if (!check(my_perl, i, ax, items))
    {
      Perl_croak(aTHX_ "expected argument %d to be a reference to an object", i+1);
    }

    IV tmp = SvIV(SvRV(ST(i)));
    return INT2PTR(void*, tmp);
  }
};

template <typename T>
struct read_as<T, std::enable_if_t<std::is_pointer<T>::value>>
{
  static bool check(PerlInterpreter* my_perl, int i, int ax, int items)
  {
    auto typemap = detail::typemap::get(my_perl);
    auto it = typemap->find(std::type_index(typeid(T)));
    return (it != typemap->end()) && SvROK(ST(i)) && sv_derived_from(ST(i), it->second.c_str());
  }

  static T get(PerlInterpreter* my_perl, int i, int ax, int items)
  {
    if (!check(my_perl, i, ax, items))
    {
      // would prefer to check for unregistered types at compile time (not possible?)
      auto typemap = detail::typemap::get(my_perl);
      auto it = typemap->find(std::type_index(typeid(T)));
      if (it == typemap->end())
      {
        Perl_croak(aTHX_ "expected argument %d to be a reference to an unregistered type (method unusable)", i+1);
      }
      Perl_croak(aTHX_ "expected argument %d to be a reference to an object of type '%s'", i+1, it->second.c_str());
    }

    IV tmp = SvIV(SvRV(ST(i)));
    return INT2PTR(T, tmp);
  }
};

template <>
struct read_as<SV*>
{
  static bool check(PerlInterpreter* my_perl, int i, int ax, int items)
  {
    return i < items;
  }

  static SV* get(PerlInterpreter* my_perl, int i, int ax, int items)
  {
    if (!check(my_perl, i, ax, items))
    {
      Perl_croak(aTHX_ "expected argument %d to be valid scalar value", i+1);
    }
    return ST(i);
  }
};

// scalar, array, and hash readers return reference to stack items (not copies)
template <>
struct read_as<scalar>
{
  static bool check(PerlInterpreter* my_perl, int i, int ax, int items)
  {
    return (SvROK(ST(i)) && SvTYPE(SvRV(ST(i))) < SVt_PVAV) || SvTYPE(ST(i)) < SVt_PVAV;
  }

  static scalar get(PerlInterpreter* my_perl, int i, int ax, int items)
  {
    if (!check(my_perl, i, ax, items))
    {
      Perl_croak(aTHX_ "expected argument %d to be a scalar or reference to a scalar", i+1);
    }
    return SvROK(ST(i)) ? SvREFCNT_inc(SvRV(ST(i))) : SvREFCNT_inc(ST(i));
  }
};

template <>
struct read_as<reference>
{
  static bool check(PerlInterpreter* my_perl, int i, int ax, int items)
  {
    return SvROK(ST(i));
  }

  static reference get(PerlInterpreter* my_perl, int i, int ax, int items)
  {
    if (!check(my_perl, i, ax, items))
    {
      Perl_croak(aTHX_ "expected argument %d to be a reference", i+1);
    }
    // take ownership of a reference to the RV itself (avoid reference to a reference)
    reference result;
    result.reset(SvREFCNT_inc(ST(i)));
    return result;
  }
};

template <>
struct read_as<array>
{
  static bool check(PerlInterpreter* my_perl, int i, int ax, int items)
  {
    return items > 0;
  }

  static array get(PerlInterpreter* my_perl, int i, int ax, int items)
  {
    if (!check(my_perl, i, ax, items))
    {
      Perl_croak(aTHX_ "expected argument %d to be start of a perl array", i+1);
    }

    array result;
    result.reserve(items);
    for (int index = 0; index < items; ++index)
    {
      result.push_back(SvREFCNT_inc(ST(index)));
    }
    return result;
  }
};

template <>
struct read_as<hash>
{
  static bool check(PerlInterpreter* my_perl, int i, int ax, int items)
  {
    return items % 2 == 0 && SvTYPE(ST(0)) == SVt_PV;
  }

  static hash get(PerlInterpreter* my_perl, int i, int ax, int items)
  {
    if (!check(my_perl, i, ax, items))
    {
      Perl_croak(aTHX_ "expected argument %d to be start of a perl hash", i+1);
    }

    hash result;
    for (int index = 0; index < items; index += 2)
    {
      const char* key = SvPV_nolen(ST(index));
      result[key] = SvREFCNT_inc(ST(index + 1));
    }
    return result;
  }
};

} // namespace stack
} // namespace perlbind
