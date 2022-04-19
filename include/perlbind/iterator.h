#pragma once

namespace perlbind { namespace detail {

struct array_iterator
{
  array_iterator() = default;
  array_iterator(PerlInterpreter* interp, AV* av, size_t index)
    : my_perl(interp), m_av(av), m_index(index), m_scalar(interp)
  {}

  bool operator!=(const array_iterator& other) const
  {
    return m_index != other.m_index;
  }

  array_iterator& operator++()
  {
    ++m_index;
    return *this;
  }

  scalar* operator->()
  {
    return &(**this);
  }

  scalar& operator*()
  {
    SV** sv = av_fetch(m_av, m_index, 0);
    if (sv)
      m_scalar = SvREFCNT_inc(*sv);

    return m_scalar;
  }

private:
  PerlInterpreter* my_perl;
  AV* m_av;
  size_t m_index;
  scalar m_scalar;
};

struct hash_iterator
{
  hash_iterator() = default;
  hash_iterator(PerlInterpreter* interp, HV* hv, HE* he)
    : my_perl(interp), m_hv(hv), m_he(he)
  {}

  bool operator==(const hash_iterator& other) const
  {
    return m_he == other.m_he;
  }

  bool operator!=(const hash_iterator& other) const
  {
    return !(*this == other);
  }

  hash_iterator& operator++()
  {
    m_he = hv_iternext(m_hv);
    return *this;
  }

  std::pair<const char*, scalar>* operator->()
  {
    return &(**this);
  }

  std::pair<const char*, scalar>& operator*()
  {
    m_pair = { HePV(m_he, PL_na), scalar(my_perl, SvREFCNT_inc(HeVAL(m_he))) };
    return m_pair;
  }

private:
  PerlInterpreter* my_perl;
  HV* m_hv;
  HE* m_he;
  std::pair<const char*, scalar> m_pair;
};

} // namespace detail
} // namespace perlbind
