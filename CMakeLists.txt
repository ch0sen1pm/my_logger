cmake_minimum_required(VERSION 3.16)
project(my_logger VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(main main.cpp)

# benchmark: my_logger vs spdlog
find_package(spdlog REQUIRED)
add_executable(bench_vs_spdlog examples/bench_vs_spdlog.cpp)
target_include_directories(bench_vs_spdlog PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(bench_vs_spdlog spdlog::spdlog)
