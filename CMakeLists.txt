cmake_minimum_required(VERSION 3.20)
project(tempo CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(TEMPO_NATIVE "Tune for the build machine's CPU" ON)

add_executable(tempo
  src/main.cpp
  src/bitboard.cpp
  src/position.cpp
  src/search.cpp
)
target_compile_options(tempo PRIVATE -Wall -Wextra)
if(TEMPO_NATIVE)
  target_compile_options(tempo PRIVATE -march=native)
else()
  target_compile_options(tempo PRIVATE -mpopcnt)
endif()
if(MINGW)
  target_link_options(tempo PRIVATE -static)
endif()
