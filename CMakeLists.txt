cmake_minimum_required(VERSION 3.20)
project(GolfDynamics VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(GOLF_BUILD_TESTS "Build Golf Dynamics tests" ON)
option(GOLF_BUILD_TOOLS "Build the golfdemo tool" ON)
option(GOLF_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

if(MSVC)
  set(GOLF_WARNINGS /W4 /permissive- /fp:precise)
  if(GOLF_WARNINGS_AS_ERRORS)
    list(APPEND GOLF_WARNINGS /WX)
  endif()
else()
  # -ffp-contract=off: no fused multiply-adds, so native and WebAssembly builds
  # compute the same doubles and a shot replays identically everywhere
  # (multiplayer depends on it).
  set(GOLF_WARNINGS -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion -ffp-contract=off)
  if(GOLF_WARNINGS_AS_ERRORS)
    list(APPEND GOLF_WARNINGS -Werror)
  endif()
endif()

# ---- The default data files, compiled in ----
set(GOLF_EMBED_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/data/clubs/standard.json
  ${CMAKE_CURRENT_SOURCE_DIR}/data/surfaces/surfaces.json
  ${CMAKE_CURRENT_SOURCE_DIR}/data/balls/balls.json
  ${CMAKE_CURRENT_SOURCE_DIR}/data/golfers/golfers.json)
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/golf_embedded.cpp
  COMMAND ${CMAKE_COMMAND} -DDATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/data -DOUT=${CMAKE_CURRENT_BINARY_DIR}/golf_embedded.cpp
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed.cmake
  DEPENDS ${GOLF_EMBED_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed.cmake
  COMMENT "Embedding the default data files")

# ---- Engine-independent golf core (no engine or renderer headers) ----
add_library(golf_core
  ${CMAKE_CURRENT_BINARY_DIR}/golf_embedded.cpp
  src/json.cpp
  src/atmosphere.cpp
  src/ball.cpp
  src/wind.cpp
  src/surface.cpp
  src/cup.cpp
  src/shot.cpp
  src/impact.cpp
  src/club.cpp
  src/terrain.cpp
  src/course.cpp
  src/golfer.cpp
  src/rules.cpp
  src/match.cpp
  src/loader.cpp
)
add_library(golf::core ALIAS golf_core)
target_include_directories(golf_core PUBLIC include)
target_compile_options(golf_core PRIVATE ${GOLF_WARNINGS})

if(GOLF_BUILD_TOOLS)
  add_executable(golfdemo tools/golfdemo.cpp)
  target_link_libraries(golfdemo PRIVATE golf::core)
  target_compile_options(golfdemo PRIVATE ${GOLF_WARNINGS})
  target_compile_definitions(golfdemo PRIVATE GOLF_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
endif()

if(GOLF_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
