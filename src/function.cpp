#include <perlbind/perlbind.h>

namespace perlbind { namespace detail {

const MGVTBL function_base::mgvtbl = { 0, 0, 0, 0, gc, 0, 0, 0 };

int function_base::gc(pTHX_ SV* sv, MAGIC* mg)
{
  auto pfunc = INT2PTR(perlbind::detail::function_base*, SvIV(sv));
  delete pfunc;
  return 1;
}

} // namespace detail
} // namespace perlbind
