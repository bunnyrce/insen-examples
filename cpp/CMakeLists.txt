cmake_minimum_required(VERSION 3.12)
project(insen_controller_client)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add executable
add_executable(insen_client insen_client.cpp)

# Platform-specific libraries
if(WIN32)
    # Windows doesn't need additional libraries for serial communication
    target_compile_definitions(insen_client PRIVATE _WIN32)
else()
    # Linux/Unix - no additional libraries needed for POSIX serial
    target_compile_definitions(insen_client PRIVATE UNIX)
endif()

# Compiler-specific options
if(MSVC)
    target_compile_options(insen_client PRIVATE /W4)
else()
    target_compile_options(insen_client PRIVATE -Wall -Wextra -pedantic)
endif()

# Install target
install(TARGETS insen_client DESTINATION bin)