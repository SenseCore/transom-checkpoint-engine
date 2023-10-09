# Find the mysql libraries
#
# The following variables are optionally searched for defaults
#  MYSQL_ROOT_DIR: Base directory where all mysql components are found
#  MYSQL_INCLUDE_DIR: Directory where mysql headers are found
#  MYSQL_LIB_DIR: Directory where mysql libraries are found

# The following are set after configuration is done:
#  MYSQL_FOUND
#  MYSQL_INCLUDE_DIRS
#  MYSQL_LIBRARIES

find_path(MYSQL_INCLUDE_DIRS
    NAMES mysql/mysql.h
    HINTS
    ${MYSQL_INCLUDE_DIR}
    ${MYSQL_ROOT_DIR}
    ${MYSQL_ROOT_DIR}/include)
if(NOT MYSQL_INCLUDE_DIRS)
    message(FATAL_ERROR "Fail to find mysql header")
endif()

find_library(MYSQL_LIBRARIES
    NAMES mysqlclient
    HINTS
    ${MYSQL_LIB_DIR}
    ${MYSQL_ROOT_DIR}
    "/usr/lib64/mysql"
    ${MYSQL_ROOT_DIR}/lib)
if(NOT MYSQL_LIBRARIES)
    message(FATAL_ERROR "Fail to find mysql lib")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(mysql DEFAULT_MSG MYSQL_INCLUDE_DIRS MYSQL_LIBRARIES)
mark_as_advanced(MYSQL_INCLUDE_DIRS MYSQL_LIBRARIES)