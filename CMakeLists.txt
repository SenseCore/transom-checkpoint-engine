cmake_minimum_required(VERSION 3.1)
project(crud)

set(CMAKE_CXX_STANDARD 17)
add_compile_definitions(SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)

## find pb
find_package(Protobuf REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIRS})

## compile proto file
SET(PROTO_DIR ${CMAKE_SOURCE_DIR}/transom_snapshot_server/include/communicator/http)
include(cmake/CompileProto.cmake)
set(PROTO_FILES http.proto)
set(PROTOC_FLAGS ${PROTOC_FLAGS} -I${PROTOBUF_INCLUDE_DIR})
compile_proto(PROTO_HDRS PROTO_SRCS ${PROTO_DIR} # DESTDIR
                                    ${PROTO_DIR} # HDR_OUTPUT_DIR
                                    ${PROTO_DIR} # PROTO_DIR
                                    ${PROTO_FILES} )

## find dependent libraries
include(cmake/findBrpc.cmake)
include(cmake/findIbverbs.cmake)
include(cmake/findMysql.cmake)

find_package(spdlog         REQUIRED)
find_package(Threads        REQUIRED)
find_package(gflags         REQUIRED)

# MAIN program

## include directories
include_directories(transom_snapshot_server/include)
include_directories($(IBVERBS_INCLUDE_DIRS))
include_directories($(MYSQL_INCLUDE_DIRS))

## recursively add source files
file(GLOB_RECURSE MAIN_SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "transom_snapshot_server/*.cpp")
list(REMOVE_ITEM MAIN_SOURCES transom_snapshot_server/src/backend/main.cpp)
list(REMOVE_ITEM MAIN_SOURCES transom_snapshot_server/tests/coordinator_test.cpp)
list(REMOVE_ITEM MAIN_SOURCES transom_snapshot_server/tests/operator_test.cpp)
list(REMOVE_ITEM MAIN_SOURCES transom_snapshot_server/tests/metaclient_test.cpp)
list(APPEND MAIN_SOURCES ${PROTO_SRCS} ${PROTO_HDRS})
list(APPEND MAIN_SOURCES ${GENERATED_SOURCES})

link_libraries(
    spdlog
    ibverbs
    rt
    ${MYSQL_LIBRARIES}
    dl

    ${BRPC_LIB}
    ${PROTOBUF_LIBRARIES}
    ${LEVELDB_LIB}
    snappy
    ${THRIFT_LIB}
    ${OPENSSL_CRYPTO_LIBRARY}
    ${OPENSSL_SSL_LIBRARY}
    gflags
    z
)

## define targets
add_executable(transom_snapshot_server ${MAIN_SOURCES} "transom_snapshot_server/src/backend/main.cpp")
add_executable(coordinator-test ${MAIN_SOURCES} "transom_snapshot_server/tests/coordinator_test.cpp")
add_executable(operator-test ${MAIN_SOURCES} "transom_snapshot_server/tests/operator_test.cpp")
add_executable(metaclient-test ${MAIN_SOURCES} "transom_snapshot_server/tests/metaclient_test.cpp")
