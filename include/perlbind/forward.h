#pragma once

namespace perlbind {

namespace detail {

class xsub_stack;
struct function_base;
struct array_iterator;
struct hash_iterator;
namespace typemap
{
  template <typename T>
  const char* get_name(PerlInterpreter* my_perl);
}

} // namespace detail

class interpreter;
class package;
struct scalar;
struct scalar_proxy;
struct reference;
struct array;
struct hash;

} // namespace perlbind
