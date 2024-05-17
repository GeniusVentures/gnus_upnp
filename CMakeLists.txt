cmake_minimum_required(VERSION 3.5.1)
set(
    CMAKE_TOOLCHAIN_FILE
    "${CMAKE_SOURCE_DIR}/cmake/toolchain/cxx17.cmake"
    CACHE
    FILEPATH
    "Default toolchain"
)
add_definitions(-D_WIN32_WINNT=0x0601)
add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)

include(GNUInstallDirs)
project(gnus_upnp C CXX)

include(cmake/functions.cmake)

# find_package(GTest CONFIG REQUIRED)
# find_package(OpenSSL REQUIRED)
# include_directories(${OPENSSL_INCLUDE_DIR})
# include_directories(${GSL_INCLUDE_DIR})
# find_package(fmt CONFIG REQUIRED)
# find_package(yaml-cpp CONFIG REQUIRED)
# find_package(tsl_hat_trie CONFIG REQUIRED)
# find_package(Boost.DI CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS date_time filesystem random regex system thread log log_setup program_options)
include_directories(${Boost_INCLUDE_DIRS})

# ---------------------------
# Set version
set(VERSION_STRING 0)
set(SUBVERSION_STRING 4)

# Set source file dir
set(PROJECT.Path ${CMAKE_SOURCE_DIR}/src)
set(PROJECT.Source_DIR
    ${PROJECT.Path}
)

# -----------------------------------
# Include external plugin need
include_directories(
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/include
)

add_subdirectory(${PROJECT.Path})

#Install Headers
install(DIRECTORY "${CMAKE_SOURCE_DIR}/include/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" FILES_MATCHING  PATTERN "*.h*")

#Install LIB
install(TARGETS gnus_upnp
        EXPORT gnus_upnpTargets DESTINATION "${CMAKE_INSTALL_LIBDIR}")
		
		#Install Targets
set(gnus_upnp_CONFIG_DESTINATION_DIR "lib/cmake/gnus_upnp")
install(EXPORT gnus_upnpTargets
  FILE gnus_upnpTargets.cmake
  DESTINATION ${gnus_upnp_CONFIG_DESTINATION_DIR}
)

include(CMakePackageConfigHelpers)

# generate the config file that is includes the exports
configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/gnus_upnpConfig.cmake"
  INSTALL_DESTINATION "lib/cmake/examples"
  NO_SET_AND_CHECK_MACRO
  NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

# generate the version file for the config file
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/gnus_upnpConfigVersion.cmake"
  VERSION "${VERSION_STRING}.${SUBVERSION_STRING}"
  COMPATIBILITY AnyNewerVersion
)

# install the configuration file
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/gnus_upnpConfig.cmake
  DESTINATION ${gnus_upnp_CONFIG_DESTINATION_DIR}
)