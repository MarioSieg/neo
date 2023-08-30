add_definitions(-DEXPORT_GVPR)

add_library(gvpr STATIC
  # Header files
  actions.h
  compile.h
  gdefs.h
  gprdata.inc
  gprstate.h
  gvpr.h
  parse.h
  queue.h
  trieFA.h

  # Source files
  actions.c
  compile.c
  gvpr.c
  gprstate.c
  parse.c
  queue.c
)

target_include_directories(gvpr PRIVATE
  ..
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/.. # needed to find generated expr/exparse.h
  ${CMAKE_CURRENT_BINARY_DIR}/../common
    # needed to find generated common/colortbl.h
  ../ast
  ../cdt
  ../cgraph
  ../common
  ../expr
)

if(GETOPT_FOUND)
  target_include_directories(gvpr SYSTEM PRIVATE
    ${GETOPT_INCLUDE_DIRS}
  )
endif()

if(WIN32 AND NOT MINGW)
  target_include_directories(gvpr PRIVATE ../../windows/include/unistd)
endif()

target_link_libraries(gvpr
  ast
  cgraph
  ingraphs
  gvc
  expr
  sfio
  vmalloc
)

if(NOT HAVE_GETOPT_H)
  target_link_libraries(gvpr ${GETOPT_LINK_LIBRARIES})
endif()
