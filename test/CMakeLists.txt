find_package(GTest REQUIRED)

enable_testing()


add_executable(gtest_uzel gtest_uzel.cpp)

set_target_properties(gtest_uzel PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
)

target_include_directories(gtest_uzel
  PRIVATE "${CMAKE_BINARY_DIR}" "${CMAKE_SOURCE_DIR}/src"
)


target_link_libraries(gtest_uzel
  PRIVATE
  GTest::gtest_main
  uzel
)

gtest_discover_tests(gtest_uzel)
