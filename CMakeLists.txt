cmake_minimum_required(VERSION 3.10)
project(skeletonkey CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create shared library
add_library(skeleton_key SHARED
    src/skeletonkey.cpp
)

# Link against required libraries
target_link_libraries(skeleton_key
    PRIVATE
    pthread
    dl
)

# Set compiler flags
target_compile_options(skeleton_key PRIVATE
    -Wall
    -Wextra
    -fPIC
)

# Install the library
install(TARGETS skeleton_key
    LIBRARY DESTINATION lib
)
