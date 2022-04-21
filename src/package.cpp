#include <perlbind/perlbind.h>

namespace perlbind {

void package::add_impl(const char* name, detail::function_base* function)
{
  std::string export_name = m_name + "::" + name;

  // the sv is assigned a magic metamethod table to delete the function
  // object when perl frees the sv
  SV* sv = newSViv(PTR2IV(function));
  sv_magicext(sv, nullptr, PERL_MAGIC_ext, &detail::function_base::mgvtbl, nullptr, 0);

  CV* cv = get_cv(export_name.c_str(), 0);
  if (!cv)
  {
    cv = newXS(export_name.c_str(), &package::xsub, __FILE__);
    CvXSUBANY(cv).any_ptr = function;
  }
  else // function exists, remove target to search overloads when called
  {
    CvXSUBANY(cv).any_ptr = nullptr;
  }

  // create an array with same name to store overloads in the CV's GV
  AV* av = GvAV(CvGV(cv));
  if (!av)
  {
    av = get_av(export_name.c_str(), GV_ADD);
  }

  array overloads = reinterpret_cast<AV*>(SvREFCNT_inc(av));
  overloads.push_back(sv); // giving only ref to GV array
}

void package::xsub(PerlInterpreter* my_perl, CV* cv)
{
  detail::xsub_stack stack(my_perl, cv);

  auto target = static_cast<detail::function_base*>(CvXSUBANY(cv).any_ptr);
  if (target)
  {
    return target->call(stack);
  }

  // find first compatible overload
  AV* av = GvAV(CvGV(cv));

  array functions = reinterpret_cast<AV*>(SvREFCNT_inc(av));
  for (const auto& function : functions)
  {
    auto func = INT2PTR(detail::function_base*, SvIV(function.sv()));
    if (func->is_compatible(stack))
    {
      return func->call(stack);
    }
  }

  std::string overloads;
  for (const auto& function : functions)
  {
    auto func = INT2PTR(detail::function_base*, SvIV(function.sv()));
    overloads += func->get_signature() + "\n ";
  }

  Perl_croak(aTHX_ "no overload of '%s' matched the %d argument(s), candidates:\n %s",
             stack.name().c_str(), stack.size(), overloads.c_str());
}

} // namespace perlbind
