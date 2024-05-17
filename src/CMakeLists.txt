add_library(gnus_upnp STATIC
    upnp.cpp
    )
include_directories(
	${PROJECT_SOURCE_DIR}/src
	)
target_include_directories(gnus_upnp PRIVATE ${GSL_INCLUDE_DIR}) 
target_link_libraries(gnus_upnp PRIVATE
	${Boost_LIBRARIES}
)
	
include_directories(../include )