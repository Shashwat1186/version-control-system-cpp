cmake_minimum_required(VERSION 3.10)
project(git_clone CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)

# Add source files
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
list(FILTER SOURCES EXCLUDE REGEX "src/git_commands\.cpp$")

# Create the executable
add_executable(git ${SOURCES})

# Link libraries
target_link_libraries(git PRIVATE
    ZLIB::ZLIB
    OpenSSL::Crypto
    CURL::libcurl
)

# Optional: Add strict compilation warnings to keep the project clean
target_compile_options(git PRIVATE -Wall -Wextra -Wpedantic)