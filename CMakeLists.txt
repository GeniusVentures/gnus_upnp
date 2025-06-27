cmake_minimum_required(VERSION 3.20)

if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0A00)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)

include(GNUInstallDirs)
project(gnus_upnp LANGUAGES C CXX)

include(cmake/functions.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS date_time filesystem random regex system thread log program_options)
include_directories(${Boost_INCLUDE_DIRS})

add_subdirectory(src)

install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/include/"
    TYPE INCLUDE
)

install(
    TARGETS gnus_upnp
    EXPORT gnus_upnpConfig
)

set(gnus_upnp_CONFIG_DESTINATION_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/gnus_upnp")
install(
    EXPORT gnus_upnpConfig
    DESTINATION ${gnus_upnp_CONFIG_DESTINATION_DIR}
)
