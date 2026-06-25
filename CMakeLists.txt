cmake_minimum_required(VERSION 3.10)
project(git_clone CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)

# Add source files
set(SOURCES
    main.cpp
    utils.cpp
    git_objects.cpp
    git_commands.cpp
    network.cpp
    packfile.cpp
    checkout.cpp
)

# Create the executable
add_executable(mygit ${SOURCES})

# Link libraries
target_link_libraries(mygit PRIVATE
    ZLIB::ZLIB
    OpenSSL::Crypto
    CURL::libcurl
)

# Optional: Add strict compilation warnings to keep the project clean
target_compile_options(mygit PRIVATE -Wall -Wextra -Wpedantic)