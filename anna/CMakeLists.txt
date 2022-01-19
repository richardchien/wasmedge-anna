set(Protobuf_USE_STATIC_LIBS ON)
find_package(Protobuf REQUIRED)

include(FetchContent)
FetchContent_Declare(
    anna
    GIT_REPOSITORY git@github.com:hydro-project/anna.git
    GIT_TAG master
    GIT_SUBMODULES_RECURSE TRUE
    UPDATE_COMMAND ""
    PATCH_COMMAND sed -i "" -e "s/BUILD_SHARED_LIBS=ON/BUILD_SHARED_LIBS=OFF/"
                  common/vendor/yamlcpp/CMakeLists.txt)
set(CMAKE_BUILD_TYPE Release) # get rid of test-coverage feature
FetchContent_MakeAvailable(anna)

set(spdlog_SOURCE_DIR
    ${anna_BINARY_DIR}/common/vendor/spdlog/spdlog-prefix/src/spdlog)
set(yamlcpp_SOURCE_DIR ${anna_BINARY_DIR}/common/vendor/yamlcpp/src/yamlcpp)
set(zeromq_SOURCE_DIR ${anna_BINARY_DIR}/common/vendor/zeromq/src/zeromq)
set(zeromqcpp_SOURCE_DIR
    ${anna_BINARY_DIR}/common/vendor/zeromqcpp/zeromqcpp-prefix/src/zeromqcpp)

set(anna_proto_LIBRARY ${anna_BINARY_DIR}/libanna-proto.a)
set(hydro_zmq_LIBRARY ${anna_BINARY_DIR}/libhydro-zmq.a)
set(yamlcpp_LIBRARY ${yamlcpp_SOURCE_DIR}/libyaml-cpp.a)
set(zeromq_LIBRARY ${zeromq_SOURCE_DIR}/src/.libs/libzmq.a)

add_executable(anna_client client.cpp)
add_dependencies(anna_client anna-proto hydro-zmq zeromq zeromqcpp)
target_include_directories(anna_client PRIVATE ${anna_BINARY_DIR})
target_include_directories(anna_client PRIVATE ${anna_SOURCE_DIR}/include)
target_include_directories(anna_client
                           PRIVATE ${anna_SOURCE_DIR}/common/include)
target_include_directories(anna_client PRIVATE ${spdlog_SOURCE_DIR}/include)
target_include_directories(anna_client PRIVATE ${yamlcpp_SOURCE_DIR}/include)
target_include_directories(anna_client PRIVATE ${zeromq_SOURCE_DIR}/include)
target_include_directories(anna_client PRIVATE ${zeromqcpp_SOURCE_DIR})
target_link_libraries(
    anna_client
    PRIVATE protobuf::libprotobuf ${anna_proto_LIBRARY} ${hydro_zmq_LIBRARY}
            ${yamlcpp_LIBRARY} ${zeromq_LIBRARY})
