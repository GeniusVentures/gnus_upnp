
# ------------------------------------------
# Set PROJECT_ROOT folder
get_filename_component(CURRENT_SOURCE_PARENT "${CMAKE_CURRENT_SOURCE_DIR}" DIRECTORY ABSOLUTE)
get_filename_component(PROJECT_ROOT "${CURRENT_SOURCE_PARENT}" DIRECTORY ABSOLUTE)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Package config ######################################################################################
set (CPACK_PACKAGE_VERSION_MAJOR "21")
set (CPACK_PACKAGE_VERSION_MINOR "0")
set (CPACK_PACKAGE_VERSION_PATCH "0")
set (CPACK_PACKAGE_VERSION_PRE_RELEASE "12")
set (CPACK_PACKAGE_VENDOR "Genius Ventures")

set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_BINARY_DIR})

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(GNUInstallDirs)
include(GenerateExportHeader)
include(CMakePackageConfigHelpers)
include(CheckCXXCompilerFlag)
include(${PROJECT_ROOT}/cmake/functions.cmake)
include(${PROJECT_ROOT}/cmake/install.cmake)
include(${PROJECT_ROOT}/cmake/definition.cmake)
include(${PROJECT_ROOT}/build/CompilationFlags.cmake)

check_cxx_compiler_flag(-std=c++14 CXX14_SUPPORT)
check_cxx_compiler_flag(-std=c++17 CXX17_SUPPORT)

if(EXISTS "${PROJECT_ROOT}/cmake/compile_option_by_platform/${CMAKE_SYSTEM_NAME}.cmake")
  print("add compile option: ${PROJECT_ROOT}/cmake/compile_option_by_platform/${CMAKE_SYSTEM_NAME}.cmake")
  include("${PROJECT_ROOT}/cmake/compile_option_by_platform/${CMAKE_SYSTEM_NAME}.cmake")
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -D_LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${EXTRA_CXX_FLAGS}")

if (NOT EXISTS "${CMAKE_TOOLCHAIN_FILE}")
  # https://cgold.readthedocs.io/en/latest/tutorials/toolchain/globals/cxx-standard.html#summary
  print("CMAKE_TOOLCHAIN_FILE not found, setting CMAKE_POSITION_INDEPENDENT_CODE ON")
  set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
else()
  print("Using toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
endif ()

# --------------------------------------------------------
# define third party directory
if (NOT DEFINED THIRDPARTY_DIR)
  if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../thirdparty/README.md")
    print("Setting default third party directory")
    set(THIRDPARTY_DIR "${CMAKE_CURRENT_LIST_DIR}/../../thirdparty")
    ## get absolute path
    cmake_path(SET THIRDPARTY_DIR NORMALIZE "${THIRDPARTY_DIR}")
  else()
    message( FATAL_ERROR "Cannot find thirdparty directory required to build" )
  endif()
endif()

if (NOT DEFINED CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE  "Release")
endif()

if (NOT DEFINED THIRDPARTY_BUILD_DIR)
  print("Setting third party build directory default")
  get_filename_component(BUILD_PLATFORM_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
  set(THIRDPARTY_BUILD_DIR "${THIRDPARTY_DIR}/build/${BUILD_PLATFORM_NAME}/${CMAKE_BUILD_TYPE}")
endif()

set(_THIRDPARTY_BUILD_DIR "${THIRDPARTY_BUILD_DIR}")
print("THIRDPARTY BUILD DIR: ${_THIRDPARTY_BUILD_DIR}")

set(_THIRDPARTY_DIR "${THIRDPARTY_DIR}")
print("THIRDPARTY SRC DIR: ${_THIRDPARTY_DIR}")

option(TESTING "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)
