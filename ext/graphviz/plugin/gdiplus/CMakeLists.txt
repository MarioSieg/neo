if(WIN32)

  add_library(gvplugin_gdiplus SHARED
    # Header files
    FileStream.h
    gvplugin_gdiplus.h

    # Source files
    FileStream.cpp
    gvdevice_gdiplus.cpp
    gvloadimage_gdiplus.cpp
    gvplugin_gdiplus.cpp
    gvrender_gdiplus.cpp
    gvtextlayout_gdiplus.cpp
  )

  target_include_directories(gvplugin_gdiplus PRIVATE
    ../../lib
    ${CMAKE_CURRENT_SOURCE_DIR}
    ../../lib/cdt
    ../../lib/cgraph
    ../../lib/common
    ../../lib/gvc
    ../../lib/pathplan
  )

  target_link_libraries(gvplugin_gdiplus
    gvc
    gdiplus
  )

  # Installation location of library files
  install(
    TARGETS gvplugin_gdiplus
    RUNTIME DESTINATION ${BINARY_INSTALL_DIR}
    LIBRARY DESTINATION ${PLUGIN_INSTALL_DIR}
    ARCHIVE DESTINATION ${LIBRARY_INSTALL_DIR}
  )

  # Specify library version and soversion
  set_target_properties(gvplugin_gdiplus PROPERTIES
    VERSION ${GRAPHVIZ_PLUGIN_VERSION}.0.0
    SOVERSION ${GRAPHVIZ_PLUGIN_VERSION}
  )

  if(MINGW)
    # work around https://gitlab.kitware.com/cmake/cmake/-/issues/21716
    set_target_properties(gvplugin_gdiplus PROPERTIES
        RUNTIME_OUTPUT_NAME gvplugin_gdiplus-${GRAPHVIZ_PLUGIN_VERSION}
    )
  endif()

endif()
