add_library(gvplugin_dot_layout SHARED
  # Source files
  gvplugin_dot_layout.c
  gvlayout_dot_layout.c
)

target_include_directories(gvplugin_dot_layout PRIVATE
  ../../lib
  ${CMAKE_CURRENT_SOURCE_DIR}
  ../../lib/cdt
  ../../lib/cgraph
  ../../lib/common
  ../../lib/gvc
  ../../lib/pathplan
)

target_link_libraries(gvplugin_dot_layout
  dotgen
  gvc
)

# Installation location of library files
install(
  TARGETS gvplugin_dot_layout
  RUNTIME DESTINATION ${BINARY_INSTALL_DIR}
  LIBRARY DESTINATION ${PLUGIN_INSTALL_DIR}
  ARCHIVE DESTINATION ${LIBRARY_INSTALL_DIR}
)

# Specify library version and soversion
set_target_properties(gvplugin_dot_layout PROPERTIES
  VERSION ${GRAPHVIZ_PLUGIN_VERSION}.0.0
  SOVERSION ${GRAPHVIZ_PLUGIN_VERSION}
)

if(MINGW)
  # work around https://gitlab.kitware.com/cmake/cmake/-/issues/21716
  set_target_properties(gvplugin_dot_layout PROPERTIES
    RUNTIME_OUTPUT_NAME gvplugin_dot_layout-${GRAPHVIZ_PLUGIN_VERSION}
  )
endif()
