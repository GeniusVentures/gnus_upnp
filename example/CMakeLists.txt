#add_subdirectory(application)
add_executable(UPNPExample UPNPExample.cpp)

add_dependencies(UPNPExample gnus_upnp)
target_link_libraries(UPNPExample PRIVATE gnus_upnp)
include_directories(../include)
