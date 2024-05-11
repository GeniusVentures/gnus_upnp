add_library(UPNP STATIC
    upnp.cpp

    )
include_directories(
	${PROJECT_SOURCE_DIR}/src
	)
target_include_directories(UPNP PRIVATE ${GSL_INCLUDE_DIR}) 
target_link_libraries(UPNP PRIVATE
	${Boost_LIBRARIES}
)
	
include_directories(../include )