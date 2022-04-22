#pragma once

#include <string>

namespace perlbind { namespace stack {

// base class for pushing value types to perl stack
// methods use mXPUSH macros to extend the stack if necessary and mortalize SVs
// this supports pushing multiple values where (X)PUSH variants that depend on TARG do not
struct pusher
{
  virtual ~pusher() = default;

  pusher() = delete;
  pusher(PerlInterpreter* interp) : my_perl(interp), sp(PL_stack_sp) {}

  SV* pop() { return POPs; }

  void push(bool value) { XPUSHs(boolSV(value)); ++m_pushed; }
  void push(const char* value) { mXPUSHp(value, strlen(value)); ++m_pushed; }
  void push(const std::string& value) { mXPUSHp(value.c_str(), value.size()); ++m_pushed; }
  void push(scalar value) { mXPUSHs(value.release()); ++m_pushed; };
  void push(reference value) { mXPUSHs(value.release()); ++m_pushed; };

  void push(array value)
  {
    int count = static_cast<int>(value.size());
    EXTEND(sp, count);
    for (int i = 0; i < count; ++i)
    {
      // mortalizes one reference to array element to avoid copying
      PUSHs(sv_2mortal(SvREFCNT_inc(value[i].sv())));
    }
    m_pushed += count;
  }

  void push(hash value)
  {
    // hashes are pushed to the perl stack as alternating keys and values
    // this is less efficient than pushing a reference to the hash
    auto count = hv_iterinit(value) * 2;
    EXTEND(sp, count);
    while (HE* entry = hv_iternext(value))
    {
      auto val = HeVAL(entry);
      PUSHs(hv_iterkeysv(entry)); // mortalizes new key sv (keys are not stored as sv)
      PUSHs(sv_2mortal(SvREFCNT_inc(val)));
    }
    m_pushed += count;
  }

  template <typename T, std::enable_if_t<detail::is_signed_integral_or_enum<T>::value, bool> = true>
  void push(T value) { mXPUSHi(static_cast<IV>(value)); ++m_pushed; }

  template <typename T, std::enable_if_t<std::is_unsigned<T>::value, bool> = true>
  void push(T value) { mXPUSHu(value); ++m_pushed; }

  template <typename T, std::enable_if_t<std::is_floating_point<T>::value, bool> = true>
  void push(T value) { mXPUSHn(value); ++m_pushed; }

  template <typename T, std::enable_if_t<std::is_pointer<T>::value, bool> = true>
  void push(T value)
  {
    const char* type_name = detail::typemap::get_name<T>(my_perl);
    if (!type_name)
    {
      Perl_croak(aTHX_ "cannot push unregistered pointer of type '%s'", util::type_name<T>::str().c_str());
    }

    SV* sv = sv_newmortal();
    sv_setref_pv(sv, type_name, static_cast<void*>(value));
    XPUSHs(sv);
    ++m_pushed;
  };

  void push(void* value)
  {
    SV* sv = sv_newmortal();
    sv_setref_pv(sv, nullptr, value); // unblessed
    XPUSHs(sv);
    ++m_pushed;
  }

  template <typename... Args>
  void push_args(Args&&... args) {};

  template <typename T, typename... Args>
  void push_args(T value, Args&&... args)
  {
    push(value); // little inefficient, each extends
    push_args(std::forward<Args>(args)...);
  }

protected:
  PerlInterpreter* my_perl = nullptr;
  SV** sp = nullptr;
  int m_pushed = 0;
};

} // namespace stack
} // namespace perlbind
