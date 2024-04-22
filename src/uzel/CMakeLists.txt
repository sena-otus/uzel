add_library(uzel
  uzel.cpp
  uconfig.cpp
)

target_link_libraries(uzel PUBLIC Boost::boost Boost::thread Boost::system)

set_target_properties(uzel PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

install(TARGETS uzel LIBRARY DESTINATION lib)
