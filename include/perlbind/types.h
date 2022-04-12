#pragma once

namespace perlbind {

struct type_base
{
  type_base() : my_perl(PERL_GET_THX) {}
  type_base(PerlInterpreter* interp) : my_perl(interp) {}
  PerlInterpreter* my_perl = nullptr;
};

} // namespace perlbind
