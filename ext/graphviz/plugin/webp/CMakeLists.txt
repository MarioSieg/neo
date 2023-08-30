if(WEBP_FOUND)

  add_library(gvplugin_webp SHARED
    gvplugin_webp.c
    gvloadimage_webp.c
    gvdevice_webp.c
  )

  target_include_directories(gvplugin_webp PRIVATE
    ../../lib
    ../../lib/cdt
    ../../lib/cgraph
    ../../lib/common
    ../../lib/gvc
    ../../lib/pathplan
  )

  target_include_directories(gvplugin_webp SYSTEM PRIVATE
    ${WEBP_INCLUDE_DIRS}
  )

  target_link_libraries(gvplugin_webp
    gvc
    ${WEBP_LINK_LIBRARIES}
  )

  if(CAIRO_FOUND)
    target_include_directories(gvplugin_webp SYSTEM PRIVATE
      ${CAIRO_INCLUDE_DIRS}
    )
    target_link_libraries(gvplugin_webp
      ${CAIRO_LINK_LIBRARIES}
    )
    if(PANGOCAIRO_FOUND)
      target_include_directories(gvplugin_webp SYSTEM PRIVATE
        ${PANGOCAIRO_INCLUDE_DIRS}
      )

      target_link_libraries(gvplugin_webp
        ${PANGOCAIRO_LINK_LIBRARIES}
      )
    endif()
  endif()

  install(
    TARGETS gvplugin_webp
    RUNTIME DESTINATION ${BINARY_INSTALL_DIR}
    LIBRARY DESTINATION ${LIBRARY_INSTALL_DIR}
    ARCHIVE DESTINATION ${LIBRARY_INSTALL_DIR}
  )

  set_target_properties(gvplugin_webp PROPERTIES
    VERSION ${GRAPHVIZ_PLUGIN_VERSION}.0.0
    SOVERSION ${GRAPHVIZ_PLUGIN_VERSION}
  )

  if(MINGW)
    # work around https://gitlab.kitware.com/cmake/cmake/-/issues/21716
    set_target_properties(gvplugin_webp PROPERTIES
      RUNTIME_OUTPUT_NAME gvplugin_webp-${GRAPHVIZ_PLUGIN_VERSION}
    )
  endif()

endif()
