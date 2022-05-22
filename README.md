
# Perlbind Library
[![linux](https://github.com/hgtw/perlbind/actions/workflows/linux.yml/badge.svg)](https://github.com/hgtw/perlbind/actions/workflows/linux.yml)
[![windows](https://github.com/hgtw/perlbind/actions/workflows/windows.yml/badge.svg)](https://github.com/hgtw/perlbind/actions/workflows/windows.yml)

This is a C++14 binding library for perl. It can be used as an alternative to writing XS for embedding perl in C++ projects.

# Basic Usage

```cpp
#include <perlbind/perlbind.h>

int get_sum(int a, int b)
{
  return a + b;
}

int main()
{
  perlbind::interpreter state;

  auto package = state.new_package("mypackage");
  package.add("get_sum", &get_sum);

  try
  {
    state.load_script("main", "script.pl");
    int result = state.call_sub<int>("main::testsub");
    printf("result: %d\n", result); // result: 15
  }
  catch (std::exception& e)
  {
    printf("error: %s\n", e.what());
  }

  return 0;
}
```

```perl
# script.pl
sub testsub {
  my $sum = mypackage::get_sum(5, 10);
  return $sum;
}
```
