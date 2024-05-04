add_executable(usender
  main.cpp
)

target_link_libraries(usender PRIVATE uzel Boost::boost Boost::system)

install(TARGETS usender RUNTIME DESTINATION bin)

target_include_directories(usender
    PRIVATE "${CMAKE_BINARY_DIR}" "${CMAKE_SOURCE_DIR}/src"
  )
