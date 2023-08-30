if(PANGOCAIRO_FOUND AND RSVG_FOUND)

  add_library(gvplugin_rsvg SHARED
    gvloadimage_rsvg.c
    gvplugin_rsvg.c
  )

  target_include_directories(gvplugin_rsvg PRIVATE
    ../../lib
    ../../lib/cdt
    ../../lib/cgraph
    ../../lib/common
    ../../lib/gvc
    ../../lib/pathplan
  )

  target_include_directories(gvplugin_rsvg SYSTEM PRIVATE
    ${PANGOCAIRO_INCLUDE_DIRS}
    ${RSVG_INCLUDE_DIRS}
  )

  target_link_libraries(gvplugin_rsvg
    gvc
    ${PANGOCAIRO_LIBRARIES}
    ${RSVG_LIBRARIES}
  )

  install(
    TARGETS gvplugin_rsvg
    RUNTIME DESTINATION ${BINARY_INSTALL_DIR}
    LIBRARY DESTINATION ${PLUGIN_INSTALL_DIR}
    ARCHIVE DESTINATION ${LIBRARY_INSTALL_DIR}
  )

  set_target_properties(gvplugin_rsvg PROPERTIES
    VERSION ${GRAPHVIZ_PLUGIN_VERSION}.0.0
    SOVERSION ${GRAPHVIZ_PLUGIN_VERSION}
  )

  if(MINGW)
    # work around https://gitlab.kitware.com/cmake/cmake/-/issues/21716
    set_target_properties(gvplugin_rsvg PROPERTIES
      RUNTIME_OUTPUT_NAME gvplugin_rsvg-${GRAPHVIZ_PLUGIN_VERSION}
    )
  endif()

endif()
