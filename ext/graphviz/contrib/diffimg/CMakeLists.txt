find_package(GD)

if(GD_FOUND)

  add_executable(diffimg diffimg.c)
  target_include_directories(diffimg PRIVATE ../../lib)
  target_include_directories(diffimg SYSTEM PRIVATE ${GD_INCLUDE_DIRS})
  target_link_libraries(diffimg ${GD_LIBRARIES})

  install(
    TARGETS diffimg
    RUNTIME DESTINATION ${BINARY_INSTALL_DIR}
    LIBRARY DESTINATION ${LIBRARY_INSTALL_DIR}
    ARCHIVE DESTINATION ${LIBRARY_INSTALL_DIR}
  )

  if(GZIP)
    add_custom_target(man-diffimg ALL DEPENDS diffimg.1.gz
                      COMMENT "diffimg man page")
    add_custom_command(
      OUTPUT diffimg.1.gz
      COMMAND ${GZIP} -9 --no-name --to-stdout diffimg.1
        >"${CMAKE_CURRENT_BINARY_DIR}/diffimg.1.gz"
      MAIN_DEPENDENCY diffimg.1
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMENT "compress diffimg man page")
    install(
      FILES ${CMAKE_CURRENT_BINARY_DIR}/diffimg.1.gz
      DESTINATION ${MAN_INSTALL_DIR}/man1)
  else()
    install(
      FILES diffimg.1
      DESTINATION ${MAN_INSTALL_DIR}/man1
    )
  endif()

endif()
