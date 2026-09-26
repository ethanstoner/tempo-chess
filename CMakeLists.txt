cmake_minimum_required(VERSION 3.20)
project(tempo CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(TEMPO_NATIVE "Tune for the build machine's CPU" ON)

add_library(tempo_core STATIC
  src/bitboard.cpp
  src/position.cpp
  src/search.cpp
  src/eval.cpp
)
target_compile_options(tempo_core PUBLIC -Wall -Wextra)
if(TEMPO_NATIVE)
  target_compile_options(tempo_core PUBLIC -march=native)
else()
  target_compile_options(tempo_core PUBLIC -mpopcnt)
endif()

add_executable(tempo src/main.cpp)
target_link_libraries(tempo PRIVATE tempo_core)

add_executable(tempo-tune tools/tune.cpp)
target_link_libraries(tempo-tune PRIVATE tempo_core)

if(MINGW)
  target_link_options(tempo PRIVATE -static)
  target_link_options(tempo-tune PRIVATE -static)
endif()
