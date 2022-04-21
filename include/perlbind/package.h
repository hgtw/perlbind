#pragma once

#include <string>

namespace perlbind { namespace detail {

class package
{
public:
  package() = delete;
  package(const package& other) = delete;
  package(package&& other) = delete;
  package& operator=(const package& other) = delete;
  package& operator=(package&& other) = delete;
  package(PerlInterpreter* interp, const char* name)
    : my_perl(interp), m_name(name), m_stash(gv_stashpv(name, GV_ADD))
  {}

  template <typename T>
  void add(const char* name, T&& func)
  {
    // ownership of function object is given to perl
    auto function = new detail::function<T>(my_perl, func);
    add_impl(name, static_cast<detail::function_base*>(function));
  }

  void add_base_class(const char* name)
  {
    std::string package_isa = m_name + "::ISA";
    array isa_array = get_av(package_isa.c_str(), GV_ADD);
    isa_array.push_back(name);
  }

  template <typename T>
  void add_const(const char* name, T&& value)
  {
    newCONSTSUB(m_stash, name, scalar(value).release());
  }

private:
  static void xsub(PerlInterpreter* my_perl, CV* cv);
  void add_impl(const char* name, detail::function_base* function);

  std::string m_name;
  PerlInterpreter* my_perl = nullptr;
  HV* m_stash = nullptr;
};

} // namespace detail

// public package interface for internal packages stored on interpreter
struct package
{
  virtual ~package() = default;
  package() = delete;

  // bind a function pointer to a function name in the package
  // overloading is supported with the following restrictions
  // 1) overloaded functions cannot have default arguments
  // 2) function pointers must be explicitly casted for C++ functions with the same name
  // 3) overloading has a runtime lookup cost and will choose the first compatible overload
  template <typename T>
  void add(const char* name, T&& func)
  {
    m_package->add<T>(name, std::forward<T>(func));
  }

  // specify a base class name for object inheritance (must be registered)
  // calling object methods missing from the package will search parent classes
  // base classes are searched in registered order and include any grandparents
  void add_base_class(const char* name)
  {
    m_package->add_base_class(name);
  }

  // add a constant value to this package namespace
  template <typename T>
  void add_const(const char* name, T&& value)
  {
    m_package->add_const<T>(name, std::forward<T>(value));
  }

private:
  friend class interpreter;
  package(detail::package* package) : m_package(package) {}

  detail::package* m_package = nullptr; // non-owning, stored on interpreter
};

template <typename T>
struct class_ : public package
{
  friend class interpreter;
  using package::package;
};

} // namespace perlbind
