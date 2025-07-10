cmake_minimum_required(VERSION 3.20)
project(SHA256 VERSION 1.0)

set(CMAKE_MACOSX_RPATH 1)
set(BUILD_SHARED_LIBS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Enable compile warnings
add_compile_options(-Wno-deprecated-declarations)

find_package(OpenSSL REQUIRED)

# Include directories
include_directories(SYSTEM ${PROJECT_SOURCE_DIR}/include ${OPENSSL_INCLUDE_DIR})

# Source files
add_executable(server src/server.c)
add_executable(client src/client.c)
add_executable(worker src/worker.c)
# Link OpenSSL to worker (digest_file uses SHA256)
target_link_libraries(worker ${OPENSSL_LIBRARIES})

