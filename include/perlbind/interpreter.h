#pragma once

#include <typeinfo>

namespace perlbind {

class interpreter
{
public:
  interpreter();
  interpreter(PerlInterpreter* interp, bool store_typemap) : my_perl(interp)
  {
    if (store_typemap)
      detail::typemap::store(my_perl, &m_typemap);
  }
  interpreter(int argc, const char** argv);
  interpreter(const interpreter& other) = delete;
  interpreter(interpreter&& other) = delete;
  interpreter& operator=(const interpreter& other) = delete;
  interpreter& operator=(interpreter&& other) = delete;
  ~interpreter();

  PerlInterpreter* get() const { return my_perl; }

  void load_script(std::string packagename, std::string filename);
  void eval(const char* str);

  template <typename T, typename... Args>
  T call_sub(const char* subname, Args&&... args) const
  {
    detail::sub_caller caller(my_perl);
    return caller.call_sub<T>(subname, std::forward<Args>(args)...);
  }

  // returns interface to add bindings to package name
  package new_package(const char* name)
  {
    auto it = m_packages.find(name);
    if (it == m_packages.end())
    {
      auto res = m_packages.emplace(std::piecewise_construct,
                                    std::forward_as_tuple(name),
                                    std::forward_as_tuple(my_perl, name));
      it = res.first;
    }
    return package(&it->second);
  }

  // registers type for blessing objects, returns interface
  template <typename T>
  class_<T> new_class(const char* name)
  {
    static_assert(!std::is_pointer<T>::value && !std::is_reference<T>::value,
                  "new_class<T> 'T' should not be a pointer or reference");

    m_typemap[std::type_index(typeid(T*))] = name;
    auto result = new_package(name);
    return class_<T>(result.m_package);
  }

  // helper to bind functions in default main:: package
  template <typename T>
  void add(const char* name, T&& func)
  {
    new_package("main").add(name, std::forward<T>(func));
  }

private:
  void init(int argc, const char** argv);

  bool m_is_owner = false;
  PerlInterpreter* my_perl = nullptr;
  std::unordered_map<std::string, detail::package> m_packages;
  detail::typemap::typemap_t m_typemap;
};

} // namespace perlbind
