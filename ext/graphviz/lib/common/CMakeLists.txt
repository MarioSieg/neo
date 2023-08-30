BISON_TARGET(HTMLparse ${CMAKE_CURRENT_SOURCE_DIR}/htmlparse.y
                       ${CMAKE_CURRENT_BINARY_DIR}/htmlparse.c)

add_definitions(-DGVC_EXPORTS)

# Generate colortbl.h from sources
add_custom_command(
  OUTPUT svgcolor_lib
  MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/svgcolor_names
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/make_svgcolor_lib.py
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/make_svgcolor_lib.py
          ${CMAKE_CURRENT_SOURCE_DIR}/svgcolor_names
          ${CMAKE_CURRENT_BINARY_DIR}/svgcolor_lib
  COMMENT "generate SVG color library"
)
add_custom_command(
  OUTPUT brewer_lib
  MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/brewer_colors
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/make_brewer_lib.py
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/make_brewer_lib.py
          ${CMAKE_CURRENT_SOURCE_DIR}/brewer_colors
          ${CMAKE_CURRENT_BINARY_DIR}/brewer_lib
  COMMENT "generate Brewer color library"
)
add_custom_command(
  OUTPUT common/colortbl.h
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/svgcolor_lib
          ${CMAKE_CURRENT_BINARY_DIR}/brewer_lib
          ${CMAKE_CURRENT_SOURCE_DIR}/color_names
          ${CMAKE_CURRENT_SOURCE_DIR}/make_colortbl.py
  COMMAND ${CMAKE_COMMAND} -E make_directory
          "${CMAKE_CURRENT_BINARY_DIR}/common"
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/make_colortbl.py
          ${CMAKE_CURRENT_BINARY_DIR}/brewer_lib
          ${CMAKE_CURRENT_BINARY_DIR}/svgcolor_lib
          ${CMAKE_CURRENT_SOURCE_DIR}/color_names
          ${CMAKE_CURRENT_BINARY_DIR}/common/colortbl.h
  COMMENT "generate color table"
)

add_library(common_obj OBJECT
  # Header files
  arith.h
  boxes.h
  color.h
  colorprocs.h
  ${CMAKE_CURRENT_BINARY_DIR}/common/colortbl.h
  const.h
  entities.h
  geom.h
  geomprocs.h
  globals.h
  intset.h
  htmllex.h
  htmltable.h
  macros.h
  memory.h
  pointset.h
  ps_font_equiv.h
  render.h
  textspan.h
  textspan_lut.h
  types.h
  usershape.h
  utils.h

  # Source files
  args.c
  arrows.c
  colxlate.c
  ellipse.c
  emit.c
  geom.c
  globals.c
  htmllex.c
  htmltable.c
  input.c
  intset.c
  labels.c
  ns.c
  memory.c
  output.c
  pointset.c
  postproc.c
  psusershape.c
  routespl.c
  shapes.c
  splines.c
  taper.c
  textspan.c
  textspan_lut.c
  timing.c
  utils.c
  xml.c

  # Generated files
  ${BISON_HTMLparse_OUTPUTS}
)

target_include_directories(common_obj PRIVATE
  ..
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ../cdt
  ../cgraph
  ../gvc
  ../pack
  ../pathplan
  ../xdot
)

if(EXPAT_FOUND)
  target_include_directories(common_obj SYSTEM PRIVATE
    ${EXPAT_INCLUDE_DIRS}
  )
endif()

if(WIN32)
  target_include_directories(common_obj
                             PRIVATE ${WINDOWS_DEPENDENCY_DIR}/include)
endif()

if(WIN32 AND NOT MINGW)
  target_include_directories(common_obj PRIVATE ../../windows/include/unistd)
endif()

# From https://gitlab.com/graphviz/graphviz/-/issues/1613 @mdwies 20093010
if(APPLE)
  target_compile_options(common_obj PRIVATE "-fno-common")
endif()

add_library(common STATIC
  $<TARGET_OBJECTS:common_obj>
)

target_link_libraries(common PRIVATE
  cgraph
  pathplan
  label
  xdot
)

if(EXPAT_FOUND)
  target_link_libraries(common PRIVATE
    ${EXPAT_LIBRARIES}
  )
endif()

# Specify headers to be installed
install(
  FILES
    arith.h
    color.h
    geom.h
    textspan.h
    types.h
    usershape.h
  DESTINATION ${HEADER_INSTALL_DIR}
)
