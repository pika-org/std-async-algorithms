cmake_minimum_required(VERSION 3.22)

project(std-async-algorithms CXX)

# TODO: This is missing from STDEXEC's config file.
find_package(Threads REQUIRED)
find_package(STDEXEC REQUIRED)
find_package(pika)

# Main target
add_library(std_async_algorithms INTERFACE)
target_include_directories(std_async_algorithms INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(std_async_algorithms INTERFACE STDEXEC::stdexec)

# Test executable
add_executable(test1 tests/test1.cpp)
target_link_libraries(test1 PRIVATE std_async_algorithms $<TARGET_NAME_IF_EXISTS:pika::pika>)
target_compile_definitions(test1 PRIVATE $<$<TARGET_EXISTS:pika::pika>:STD_ASYNC_ALGOS_HAVE_PIKA>)
