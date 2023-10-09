# Find the brpc libraries
#
# The following variables are optionally searched for defaults
#  BRPC_ROOT_DIR: Base directory where all brpc components are found
#  BRPC_INCLUDE_DIR: Directory where brpc headers are found
#  BRPC_LIB_DIR: Directory where brpc library is found
#
# The following are set after configuration is done:
#  BRPC_FOUND
#  BRPC_INCLUDE_DIRS
#  BRPC_LIBRARIES

find_package(OpenSSL)
include_directories(${OPENSSL_INCLUDE_DIR})

# Search for libthrift* by best effort. If it is not found and brpc is
# compiled with thrift protocol enabled, a link error would be reported.
find_library(THRIFT_LIB NAMES thrift)
if (NOT THRIFT_LIB)
    set(THRIFT_LIB "")
endif()

find_path(BRPC_INCLUDE_PATH
    NAMES brpc/server.h
    HINTS
    ${BRPC_INCLUDE_DIR}
    ${BRPC_ROOT_DIR}
    ${BRPC_ROOT_DIR}/include)
if(NOT BRPC_INCLUDE_PATH)
    message(FATAL_ERROR "Fail to find brpc header")
endif()

find_library(BRPC_LIB
    NAMES libbrpc.a
    HINTS
    ${BRPC_LIB_DIR}
    ${BRPC_ROOT_DIR}
    ${BRPC_ROOT_DIR}/lib)
message(STATUS ${BRPC_LIB})
if(NOT BRPC_LIB)
    message(FATAL_ERROR "Fail to find brpc lib")
endif()

include_directories(${BRPC_INCLUDE_PATH})

find_path(LEVELDB_INCLUDE_PATH NAMES leveldb/db.h)
find_library(LEVELDB_LIB NAMES leveldb libleveldb.a)
if ((NOT LEVELDB_INCLUDE_PATH) OR (NOT LEVELDB_LIB))
    message(FATAL_ERROR "Fail to find leveldb")
endif()
include_directories(${LEVELDB_INCLUDE_PATH})
