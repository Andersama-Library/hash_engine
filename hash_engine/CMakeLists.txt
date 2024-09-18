# CMakeList.txt : CMake project for hash_engine, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (hash_engine "hash_engine.cpp" "hash_engine.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET hash_engine PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
