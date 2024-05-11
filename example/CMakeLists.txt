#add_subdirectory(application)
add_executable(UPNPExample UPNPExample.cpp)

add_dependencies(UPNPExample UPNP)
target_link_libraries(UPNPExample PRIVATE UPNP)
include_directories(../include)
