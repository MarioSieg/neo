if(GS_FOUND AND PANGOCAIRO_FOUND)

  add_library(gvplugin_gs SHARED
    gvloadimage_gs.c
    gvplugin_gs.c
  )

  target_include_directories(gvplugin_gs PRIVATE
    ../../lib
    ../../lib/cdt
    ../../lib/cgraph
    ../../lib/common
    ../../lib/gvc
    ../../lib/pathplan
  )

  target_include_directories(gvplugin_gs SYSTEM PRIVATE
    ${GS_INCLUDE_DIRS}
    ${PANGOCAIRO_INCLUDE_DIRS}
  )

  target_link_libraries(gvplugin_gs
    gvc
    ${GS_LIBRARIES}
    ${CAIRO_LINK_LIBRARIES}
    ${PANGOCAIRO_LINK_LIBRARIES}
  )

  install(
    TARGETS gvplugin_gs
    RUNTIME DESTINATION ${BINARY_INSTALL_DIR}
    LIBRARY DESTINATION ${PLUGIN_INSTALL_DIR}
    ARCHIVE DESTINATION ${LIBRARY_INSTALL_DIR}
  )

  set_target_properties(gvplugin_gs PROPERTIES
    VERSION ${GRAPHVIZ_PLUGIN_VERSION}.0.0
    SOVERSION ${GRAPHVIZ_PLUGIN_VERSION}
  )

  if(MINGW)
    # work around https://gitlab.kitware.com/cmake/cmake/-/issues/21716
    set_target_properties(gvplugin_gs PROPERTIES
      RUNTIME_OUTPUT_NAME gvplugin_gs-${GRAPHVIZ_PLUGIN_VERSION}
    )
  endif()

endif()
