#pragma once

#include <string>
#include <typeindex>
#include <unordered_map>

namespace perlbind { namespace detail {

namespace typemap
{
  extern const MGVTBL mgvtbl;
  using typemap_t = std::unordered_map<std::type_index, std::string>;

  inline void store(PerlInterpreter* my_perl, const typemap_t* p_typemap)
  {
    sv_unmagicext((SV*)PL_defstash, PERL_MAGIC_ext, const_cast<MGVTBL*>(&typemap::mgvtbl));
    sv_magicext((SV*)PL_defstash, nullptr, PERL_MAGIC_ext, &typemap::mgvtbl, (const char*)p_typemap, 0);
  }

  inline typemap_t* get(PerlInterpreter* my_perl)
  {
    MAGIC* mg = mg_findext((SV*)PL_defstash, PERL_MAGIC_ext, &typemap::mgvtbl);
    if (!mg || !mg->mg_ptr)
    {
      Perl_croak(aTHX_ "unexpected error accessing interpreter typemap");
    }
    return reinterpret_cast<typemap_t*>(mg->mg_ptr);
  }
} // namespace typemap

} // namespace detail
} // namespace perlbind
