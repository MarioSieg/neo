find_package(Freetype)
find_package(PANGOCAIRO)
find_package(PkgConfig)
if(PkgConfig_FOUND)
  pkg_check_modules(LASI lasi)
endif()

if(Freetype_FOUND AND LASI_FOUND AND PANGOCAIRO_FOUND)

  add_library(gvplugin_lasi SHARED
    gvloadimage_lasi.c
    gvplugin_lasi.c
    gvrender_lasi.cpp
  )

  target_include_directories(gvplugin_lasi PRIVATE
    ../../lib
    ../../lib/cdt
    ../../lib/cgraph
    ../../lib/common
    ../../lib/gvc
    ../../lib/pathplan
  )

  target_include_directories(gvplugin_lasi SYSTEM PRIVATE
    ${FREETYPE_INCLUDE_DIRS}
    ${LASI_INCLUDE_DIRS}
    ${PANGOCAIRO_INCLUDE_DIRS}
  )

  target_link_libraries(gvplugin_lasi
    ${FREETYPE_LIBRARIES}
    ${LASI_LIBRARIES}
    ${PANGOCAIRO_LIBRARIES}
  )

  install(
    TARGETS gvplugin_lasi
    RUNTIME DESTINATION ${BINARY_INSTALL_DIR}
    LIBRARY DESTINATION ${PLUGIN_INSTALL_DIR}
    ARCHIVE DESTINATION ${LIBRARY_INSTALL_DIR}
  )

  set_target_properties(gvplugin_lasi PROPERTIES
    VERSION ${GRAPHVIZ_PLUGIN_VERSION}.0.0
    SOVERSION ${GRAPHVIZ_PLUGIN_VERSION}
  )

  if(MINGW)
    # work around https://gitlab.kitware.com/cmake/cmake/-/issues/21716
    set_target_properties(gvplugin_lasi PROPERTIES
      RUNTIME_OUTPUT_NAME gvplugin_lasi-${GRAPHVIZ_PLUGIN_VERSION}
    )
  endif()

endif()
