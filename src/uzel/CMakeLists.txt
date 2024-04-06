add_library(uzel
  uzel.cpp
)

target_link_libraries(uzel PRIVATE Boost::boost)

#set_target_properties(uzel PROPERTIES
#    CXX_STANDARD 20
#    CXX_STANDARD_REQUIRED ON
#)

install(TARGETS uzel LIBRARY DESTINATION lib)
