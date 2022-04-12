#include <perlbind/perlbind.h>

namespace perlbind { namespace detail {

const MGVTBL package::mgvtbl = { 0, 0, 0, 0, 0, 0, 0, 0 };

void package::add_impl(const char* name, std::unique_ptr<function_base>&& function)
{
  std::string export_name = m_name + "::" + name;

  // todo: store cvs and cv_undef on destruction (for non-owning interpreters)
  CV* cv = get_cv(export_name.c_str(), 0);
  if (!cv)
  {
    cv = newXS(export_name.c_str(), &package::xsub, __FILE__);
    CvXSUBANY(cv).any_ptr = function.get();
  }
  else // function exists, remove target to search overloads when called
  {
    CvXSUBANY(cv).any_ptr = nullptr;
  }

  m_methods[name].emplace_back(std::move(function));
}

void package::xsub(PerlInterpreter* my_perl, CV* cv)
{
  xsub_stack stack(my_perl, cv);

  auto target = static_cast<function_base*>(CvXSUBANY(cv).any_ptr);
  if (target)
  {
    return target->call(stack);
  }

  // find first compatible overload
  auto self = detail::package::get(my_perl, stack.get_stash());
  auto it = self->m_methods.find(stack.sub_name());
  if (it != self->m_methods.end())
  {
    for (const auto& func : it->second)
    {
      if (func->is_compatible(stack))
      {
        return func->call(stack);
      }
    }
  }

  std::string overloads;
  for (const auto& func : it->second)
  {
    overloads += func->get_signature() + "\n ";
  }

  Perl_croak(aTHX_ "no overload of '%s' matched the %d argument(s), candidates:\n %s",
             stack.name().c_str(), stack.size(), overloads.c_str());
}

} // namespace detail
} // namespace perlbind
