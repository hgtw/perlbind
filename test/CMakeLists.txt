cmake_minimum_required(VERSION 3.14)

Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.0.0-preview4
)

FetchContent_MakeAvailable(Catch2)

set(TEST_SOURCES
  interpreter.cpp
  bindings.cpp
  stack.cpp
  traits.cpp
  types.cpp
)

if(MSVC)
  set(TEST_SOURCES ${TEST_SOURCES} ../src/perlbind.natvis)
endif()

function(add_tests)
  set(target_name "tests")
  if(ARGC GREATER 0)
    set(target_name "${target_name}-${ARGV0}")
  endif()

  add_executable(${target_name} ${TEST_SOURCES})
  add_test(NAME ${target_name} COMMAND ${target_name})

  if(PERLBIND_ENABLE_ASAN)
    target_compile_options(${target_name} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(${target_name} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
  endif()

  target_link_libraries(${target_name} PRIVATE perlbind ${PERL_LIBRARY} Catch2::Catch2)
  target_compile_definitions(${target_name} PRIVATE ${ARGV0})

  if(UNIX)
    find_package(Threads)
    target_link_libraries(${target_name} PRIVATE Threads::Threads)
  endif()
endfunction()

add_tests()
add_tests(PERLBIND_STRICT_NUMERIC_TYPES)
add_tests(PERLBIND_NO_STRICT_SCALAR_TYPES)
