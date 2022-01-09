cmake_minimum_required(VERSION 3.14)
project(WasmEdgeAnna C)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules)

find_package(WasmEdge REQUIRED)

add_executable(wasmedge_anna main.c)
target_link_libraries(wasmedge_anna PRIVATE WasmEdge::wasmedge_c)
