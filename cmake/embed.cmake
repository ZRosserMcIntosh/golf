# Generate a C++ source that holds the default data files as string literals.
# Usage: cmake -DDATA_DIR=... -DOUT=... -P cmake/embed.cmake
set(PAIRS
  "clubsStandard|clubs/standard.json"
  "surfaces|surfaces/surfaces.json"
  "balls|balls/balls.json"
  "golfers|golfers/golfers.json")
set(BODY "// Generated from data/ by cmake/embed.cmake. Do not edit.\n#include \"golf/embedded.hpp\"\nnamespace golf::embedded {\n")
foreach(P ${PAIRS})
  string(REPLACE "|" ";" PL "${P}")
  list(GET PL 0 FN)
  list(GET PL 1 FILE)
  file(READ "${DATA_DIR}/${FILE}" CONTENT)
  string(APPEND BODY "const char* ${FN}() {\n    return R\"golfjson(${CONTENT})golfjson\";\n}\n")
endforeach()
string(APPEND BODY "}  // namespace golf::embedded\n")
file(WRITE "${OUT}.tmp" "${BODY}")
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${OUT}.tmp" "${OUT}")
