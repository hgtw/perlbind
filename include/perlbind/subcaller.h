#pragma once

#include <stdexcept>

namespace perlbind { namespace detail {

// handles calls to perl, inherits stack::pusher to push args to perl sub
class sub_caller : public stack::pusher
{
public:
  sub_caller() = delete;
  sub_caller(PerlInterpreter* my_perl) : stack::pusher(my_perl)
  {
    ENTER; // enter scope boundary for any mortals we create
    SAVETMPS;
  }
  ~sub_caller()
  {
    PUTBACK; // set global sp back to local for any popped return values
    FREETMPS;
    LEAVE; // leave scope, decref mortals and values returned by perl
  }

  template <typename T, typename... Args, std::enable_if_t<std::is_integral<T>::value, bool> = true>
  auto call_sub(const char* subname, Args&&... args)
  {
    try
    {
      int count = call_sub_impl(subname, G_EVAL|G_SCALAR, std::forward<Args>(args)...);

      if (count != 1)
      {
        throw std::runtime_error("Expected a single result from G_SCALAR sub call");
      }
    }
    catch (...)
    {
      pop(); // top of stack holds undef on error when called with these flags
      throw;
    }

    SV* result = pop();
    if (SvTYPE(result) != SVt_IV)
    {
      throw std::runtime_error("Expected an integer result from sub call");
    }

    return static_cast<T>(SvIV(result));
  }

private:
  template <typename... Args>
  int call_sub_impl(const char* subname, int flags, Args&&... args)
  {
    PUSHMARK(SP); // notify perl of local sp (required even if not pushing args)
    push_args(std::forward<Args>(args)...);
    PUTBACK; // set global sp back to local so call will know pushed arg count

    int result_count = call_pv(subname, flags);

    SPAGAIN; // refresh local sp since call may reallocate stack for scalar returns

    SV* err = ERRSV;
    if (SvTRUE(err))
    {
      throw std::runtime_error("Perl error: " + std::string(SvPV_nolen(err)));
    }

    return result_count;
  }
};

} //namespace detail
} // namespace perlbind
