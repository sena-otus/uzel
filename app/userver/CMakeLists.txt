add_executable(userver
  main.cpp
  netserver.cpp
  remote.cpp
  OutgoingManager.cpp
)

target_link_libraries(userver PRIVATE uzel Boost::boost Boost::system)

#set_target_properties(userver PROPERTIES
#    CXX_STANDARD 20
#    CXX_STANDARD_REQUIRED ON
#)

install(TARGETS userver RUNTIME DESTINATION bin)

target_include_directories(userver
    PRIVATE "${CMAKE_BINARY_DIR}" "${CMAKE_SOURCE_DIR}/src"
  )
