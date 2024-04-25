add_executable(uclient
  main.cpp
  netapp.cpp
)

target_link_libraries(uclient PRIVATE uzel Boost::boost Boost::system)

install(TARGETS uclient RUNTIME DESTINATION bin)

target_include_directories(uclient
    PRIVATE "${CMAKE_BINARY_DIR}" "${CMAKE_SOURCE_DIR}/src"
  )
