cmake_minimum_required(VERSION 3.17)
project(WasmEdgeAnna C CXX)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(anna)

find_package(WasmEdge REQUIRED)

add_executable(wasmedge_anna main.cpp)
target_link_libraries(wasmedge_anna PRIVATE WasmEdge::wasmedge_c client_wrapper)
