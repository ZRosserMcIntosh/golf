add_executable(golf_tests
  test_main.cpp
  test_atmosphere.cpp
  test_flight.cpp
  test_ground.cpp
  test_cup.cpp
  test_impact.cpp
  test_course.cpp
  test_game.cpp
)
target_link_libraries(golf_tests PRIVATE golf::core)
target_compile_options(golf_tests PRIVATE ${GOLF_WARNINGS})
target_compile_definitions(golf_tests PRIVATE GOLF_DATA_DIR="${PROJECT_SOURCE_DIR}/data")
add_test(NAME golf_tests COMMAND golf_tests)
