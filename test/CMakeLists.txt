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

add_executable(tests ${TEST_SOURCES})

target_link_libraries(tests PRIVATE perlbind ${PERL_LIBRARY} Catch2::Catch2)

if(UNIX)
  find_package(Threads)
  target_link_libraries(tests PRIVATE Threads::Threads)
endif()

add_test(NAME tests COMMAND tests)
