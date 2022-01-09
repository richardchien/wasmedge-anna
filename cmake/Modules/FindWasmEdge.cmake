find_path(WasmEdge_INCLUDE_DIR wasmedge/wasmedge.h)
find_library(WasmEdge_LIBRARY NAMES wasmedge_c)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    WasmEdge
    FOUND_VAR WasmEdge_FOUND
    REQUIRED_VARS WasmEdge_INCLUDE_DIR WasmEdge_LIBRARY)

# hide cache vars set by find_package_handle_standard_args
mark_as_advanced(WasmEdge_INCLUDE_DIR WasmEdge_LIBRARY)

if(WasmEdge_FOUND)
    set(WasmEdge_INCLUDE_DIRS ${WasmEdge_INCLUDE_DIR})
    set(WasmEdge_LIBRARIES ${WasmEdge_LIBRARY})
endif()

if(WasmEdge_FOUND AND NOT TARGET WasmEdge::wasmedge_c)
    add_library(WasmEdge::wasmedge_c UNKNOWN IMPORTED)
    set_target_properties(
        WasmEdge::wasmedge_c
        PROPERTIES IMPORTED_LOCATION ${WasmEdge_LIBRARY}
                   INTERFACE_INCLUDE_DIRECTORIES ${WasmEdge_INCLUDE_DIR})
endif()
