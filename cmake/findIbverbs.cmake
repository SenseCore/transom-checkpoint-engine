# Find the ibverbs libraries
#
# The following variables are optionally searched for defaults
#  IBVERBS_ROOT_DIR: Base directory where all ibverbs components are found
#  IBVERBS_INCLUDE_DIR: Directory where ibverbs headers are found
#  IBVERBS_LIB_DIR: Directory where ibverbs libraries are found

# The following are set after configuration is done:
#  IBVERBS_FOUND
#  IBVERBS_INCLUDE_DIRS
#  IBVERBS_LIBRARIES

find_path(IBVERBS_INCLUDE_DIRS
    NAMES infiniband/verbs.h
    HINTS
    ${IBVERBS_INCLUDE_DIR}
    ${IBVERBS_ROOT_DIR}
    ${IBVERBS_ROOT_DIR}/include)
if(NOT IBVERBS_INCLUDE_DIRS)
    message(FATAL_ERROR "Fail to find ibverbs header")
endif()

find_library(IBVERBS_LIBRARIES
    NAMES ibverbs
    HINTS
    ${IBVERBS_LIB_DIR}
    ${IBVERBS_ROOT_DIR}
    ${IBVERBS_ROOT_DIR}/lib)
if(NOT IBVERBS_LIBRARIES)
    message(FATAL_ERROR "Fail to find ibverbs lib")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ibverbs DEFAULT_MSG IBVERBS_INCLUDE_DIRS IBVERBS_LIBRARIES)
mark_as_advanced(IBVERBS_INCLUDE_DIRS IBVERBS_LIBRARIES)