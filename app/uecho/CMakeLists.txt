add_executable(uecho
  main.cpp
)

target_link_libraries(uecho PRIVATE uzel Boost::boost Boost::system)

install(TARGETS uecho RUNTIME DESTINATION bin)

target_include_directories(uecho
    PRIVATE "${CMAKE_BINARY_DIR}" "${CMAKE_SOURCE_DIR}/src"
  )
