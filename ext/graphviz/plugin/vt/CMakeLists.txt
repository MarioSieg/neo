add_library(gvplugin_vt SHARED gvplugin_vt.c)

target_include_directories(gvplugin_vt PRIVATE
  ../../lib
  ../../lib/cdt
  ../../lib/cgraph
  ../../lib/common
  ../../lib/gvc
  ../../lib/pathplan
)

target_link_libraries(gvplugin_vt cgraph gvc xdot)

# Installation location of library files
install(
  TARGETS gvplugin_vt
  RUNTIME DESTINATION ${BINARY_INSTALL_DIR}
  LIBRARY DESTINATION ${PLUGIN_INSTALL_DIR}
  ARCHIVE DESTINATION ${LIBRARY_INSTALL_DIR}
)

# Specify library version and soversion
set_target_properties(gvplugin_vt PROPERTIES
  VERSION ${GRAPHVIZ_PLUGIN_VERSION}.0.0
  SOVERSION ${GRAPHVIZ_PLUGIN_VERSION}
)

if(MINGW)
  # work around https://gitlab.kitware.com/cmake/cmake/-/issues/21716
  set_target_properties(gvplugin_vt PROPERTIES
    RUNTIME_OUTPUT_NAME gvplugin_vt-${GRAPHVIZ_PLUGIN_VERSION}
  )
endif()
