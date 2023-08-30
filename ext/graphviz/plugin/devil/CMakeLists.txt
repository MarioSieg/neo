find_package(PANGOCAIRO)
find_package(DevIL)

if(PANGOCAIRO_FOUND AND DevIL_FOUND)

  add_library(gvplugin_devil SHARED
    gvdevice_devil.c
    gvplugin_devil.c
  )

  target_include_directories(gvplugin_devil PRIVATE
    ../../lib
    ../../lib/cdt
    ../../lib/cgraph
    ../../lib/common
    ../../lib/gvc
    ../../lib/pathplan
  )

  target_include_directories(gvplugin_devil SYSTEM PRIVATE
    ${PANGOCAIRO_INCLUDE_DIRS}
    ${IL_INCLUDE_DIR}
  )

  target_link_libraries(gvplugin_devil
    ${PANGOCAIRO_LIBRARIES}
    ${IL_LIBRARIES}
    ${ILU_LIBRARIES}
  )

  install(
    TARGETS gvplugin_devil
    RUNTIME DESTINATION ${BINARY_INSTALL_DIR}
    LIBRARY DESTINATION ${PLUGIN_INSTALL_DIR}
    ARCHIVE DESTINATION ${LIBRARY_INSTALL_DIR}
  )

  set_target_properties(gvplugin_devil PROPERTIES
    VERSION ${GRAPHVIZ_PLUGIN_VERSION}.0.0
    SOVERSION ${GRAPHVIZ_PLUGIN_VERSION}
  )

  if(MINGW)
    # work around https://gitlab.kitware.com/cmake/cmake/-/issues/21716
    set_target_properties(gvplugin_devil PROPERTIES
      RUNTIME_OUTPUT_NAME gvplugin_devil-${GRAPHVIZ_PLUGIN_VERSION}
    )
  endif()

endif()
