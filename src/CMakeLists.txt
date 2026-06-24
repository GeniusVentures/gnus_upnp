add_library(gnus_upnp STATIC
    upnp.cpp
    upnp_logger.cpp
    upnp_util.cpp
    upnp_igd_parser.cpp
)

target_include_directories(
    gnus_upnp
    PRIVATE ${GSL_INCLUDE_DIR} ${spdlog_INCLUDE_DIR}
    PUBLIC $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(gnus_upnp PUBLIC
    Boost::headers
    spdlog::spdlog
)
include_directories(../include)