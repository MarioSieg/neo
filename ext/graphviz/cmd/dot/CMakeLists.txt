add_definitions(-DDEMAND_LOADING=1)

include_directories(
  ../../lib
  ${CMAKE_CURRENT_BINARY_DIR}
  ../../lib/cdt
  ../../lib/cgraph
  ../../lib/common
  ../../lib/gvc
  ../../lib/pathplan
)

add_executable(dot
  # Source files
  dot.c
  no_builtins.c
)

target_link_libraries(dot
  cgraph
  gvc
)

if(APPLE)
  set_target_properties(dot PROPERTIES LINK_FLAGS -Wl,-stack_size,0x2000000)
elseif(MINGW)
  set_target_properties(dot PROPERTIES LINK_FLAGS -Wl,--stack,0x2000000)
elseif(WIN32)
  set_target_properties(dot PROPERTIES LINK_FLAGS /STACK:"33554432")
endif()

# Installation location of executables
install(
  TARGETS dot
  RUNTIME DESTINATION ${BINARY_INSTALL_DIR}
  LIBRARY DESTINATION ${LIBRARY_INSTALL_DIR}
  ARCHIVE DESTINATION ${LIBRARY_INSTALL_DIR}
)

add_executable(dot_builtins
  dot.c
  dot_builtins.cpp
)

# teach dot_builtins how to find plugins at run time
if(APPLE)
  set_target_properties(dot_builtins PROPERTIES
    INSTALL_RPATH "@executable_path/../${PLUGIN_INSTALL_DIR}")
else()
  set_target_properties(dot_builtins PROPERTIES
    INSTALL_RPATH "\$ORIGIN/../${PLUGIN_INSTALL_DIR}")
endif()

target_link_libraries(dot_builtins PRIVATE
  cdt
  cgraph
  gvc
  gvplugin_core
  gvplugin_dot_layout
  gvplugin_neato_layout
  pathplan
  xdot
)

if(EXPAT_FOUND)
  target_include_directories(dot_builtins SYSTEM PRIVATE ${EXPAT_INCLUDE_DIRS})
  target_link_libraries(dot_builtins PRIVATE ${EXPAT_LIBRARIES})
endif()

if(GD_FOUND)
  target_link_libraries(dot_builtins PRIVATE gvplugin_gd)
endif()

find_package(PkgConfig)
if(PkgConfig_FOUND)
  pkg_check_modules(GTS gts)
  if(GTS_FOUND)
    target_include_directories(dot_builtins SYSTEM PRIVATE ${GTS_INCLUDE_DIRS})
    target_link_libraries(dot_builtins PRIVATE ${GTS_LINK_LIBRARIES})
  endif()

  pkg_check_modules(LASI lasi)
  if(LASI_FOUND)
    target_link_libraries(dot_builtins PRIVATE gvplugin_lasi)
  endif()
endif()

if(PANGOCAIRO_FOUND)
  target_link_libraries(dot_builtins PRIVATE gvplugin_pango)
  if(PkgConfig_FOUND)
    pkg_check_modules(WEBP libwebp)
    if(WEBP_FOUND)
      target_link_libraries(dot_builtins PRIVATE gvplugin_webp)
    endif()
  endif()
endif()

find_library(SOCKET socket)
if(SOCKET)
  target_link_libraries(dot_builtins PRIVATE ${SOCKET})
endif()

install(
  TARGETS dot_builtins
  RUNTIME DESTINATION ${BINARY_INSTALL_DIR}
  LIBRARY DESTINATION ${LIBRARY_INSTALL_DIR}
  ARCHIVE DESTINATION ${LIBRARY_INSTALL_DIR}
)

# Aliases to the dot executable (not including '.exe' suffix)
list(APPEND dot_aliases circo fdp neato osage patchwork sfdp twopi)

# We use copying instead of symlinking for Cygwin to avoid
# https://gitlab.com/graphviz/graphviz/-/issues/2123

foreach(cmd_alias IN LISTS dot_aliases)
  set(DOTCOPY
      "${CMAKE_CURRENT_BINARY_DIR}/${cmd_alias}${CMAKE_EXECUTABLE_SUFFIX}")
  if(WIN32 OR CYGWIN)
    # Copy dot executable to each alias name then install copies to bindir
    add_custom_command(
      TARGET dot
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:dot> ${DOTCOPY}
      COMMENT "Copying dot to ${DOTCOPY}"
    )
    install(
      PROGRAMS ${DOTCOPY}
      DESTINATION ${BINARY_INSTALL_DIR}
    )
  else()
    # For not-WIN32, install symlinks from dot_aliases -> dot_executable in
    # bindir. Note: This may be brittle. This builds a symlink from
    # ./cmd_alias -> ./dot in ${CMAKE_CURRENT_BINARY_DIR}, then installs that
    # symlink into ${BINARY_INSTALL_DIR}. This presumes
    # ${CMAKE_CURRENT_BINARY_DIR}/dot is installed to ${BINARY_INSTALL_DIR}/dot.
    # There is a (small?) risk of dangling symlinks
    add_custom_command(
      TARGET dot
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE_NAME:dot>
        ${cmd_alias}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Linking dot as ${cmd_alias}"
    )
    install(
      FILES ${DOTCOPY}
      DESTINATION ${BINARY_INSTALL_DIR}
    )
  endif()
endforeach()

# Specify man pages to be installed
if(GZIP)
  add_custom_target(man-dot ALL DEPENDS dot.1.gz
                    COMMENT "dot man page")
  add_custom_command(
    OUTPUT dot.1.gz
    COMMAND ${GZIP} -9 --no-name --to-stdout dot.1
      >"${CMAKE_CURRENT_BINARY_DIR}/dot.1.gz"
    MAIN_DEPENDENCY dot.1
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "compress dot man page")
  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/dot.1.gz
    DESTINATION ${MAN_INSTALL_DIR}/man1)
  add_custom_target(man-osage ALL DEPENDS osage.1.gz
                    COMMENT "osage man page")
  add_custom_command(
    OUTPUT osage.1.gz
    COMMAND ${GZIP} -9 --no-name --to-stdout osage.1
      >"${CMAKE_CURRENT_BINARY_DIR}/osage.1.gz"
    MAIN_DEPENDENCY osage.1
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "compress osage man page")
  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/osage.1.gz
    DESTINATION ${MAN_INSTALL_DIR}/man1)
  add_custom_target(man-patchwork ALL DEPENDS patchwork.1.gz
                    COMMENT "patchwork man page")
  add_custom_command(
    OUTPUT patchwork.1.gz
    COMMAND ${GZIP} -9 --no-name --to-stdout patchwork.1
      >"${CMAKE_CURRENT_BINARY_DIR}/patchwork.1.gz"
    MAIN_DEPENDENCY patchwork.1
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "compress patchwork man page")
  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/patchwork.1.gz
    DESTINATION ${MAN_INSTALL_DIR}/man1)
else()
  install(
    FILES dot.1 osage.1 patchwork.1
    DESTINATION ${MAN_INSTALL_DIR}/man1
  )
endif()

if(NOT CMAKE_CROSSCOMPILING)
  configure_file(
    ../../cmake/configure_plugins.cmake.in configure_plugins.cmake @ONLY)

  install(SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/configure_plugins.cmake)
endif()
