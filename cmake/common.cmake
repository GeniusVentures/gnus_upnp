function(add_fileloader_library version soversion)
   # 1. Glob header files
   file(GLOB FILELOADER_HEADER "${CMAKE_SOURCE_DIR}/include/*.hpp")

   # 2. Glob sources files
   file(GLOB FILELOADER_SRCS "${CMAKE_SOURCE_DIR}/src/*.cpp")

   set(FILELOADER_OBJECT_LINK "")
   set(FILELOADER_TARGETS "")

   FILE(GLOB MNN_LIBS "${MNN_LIBRARY_DIR}/*")
   #list(APPEND FILELOADER_OBJECT_LINK ${FILELOADER_OBJECT_LINK} ${MNN_LIBS} ${FILELOADER_SRCS})
   list(APPEND FILELOADER_OBJECT_LINK ${FILELOADER_OBJECT_LINK} ${FILELOADER_SRCS})
   list(APPEND FILELOADER_TARGETS ${FileLoader_LIB})

   ## 3. library
   # TODO: Add flag for build SHARED or STATIC library. Currently just default is static (GNUS is not using shared libraries)
   add_library(${FileLoader_LIB} OBJECT ${FILELOADER_SRCS})
   target_link_libraries(${FileLoader_LIB} ${MNN_LIBS})

   message(STATUS "Installing FileLoader Headers ...")
   file(INSTALL ${FILELOADER_HEADER} DESTINATION ${BUILD_FILELOADER_DIR}/include)
   message(STATUS ">>> Added Library: FileLoader!")

endfunction()


function(add_fileloader_executable executable_name)
    add_executable(${executable_name} ${executable_name}.cpp)
    target_link_libraries(${executable_name} ${FileLoader_LIB} ${MNN_LIBS})
endfunction()