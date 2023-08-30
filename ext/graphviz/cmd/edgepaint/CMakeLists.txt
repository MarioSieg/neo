if(with_sfdp)

  add_executable(edgepaint edgepaintmain.c)

  target_include_directories(edgepaint PRIVATE
    ../../lib
    ../../lib/common
    ../../lib/cgraph
    ../../lib/cdt
  )

  if(GETOPT_FOUND)
    target_include_directories(edgepaint SYSTEM PRIVATE
      ${GETOPT_INCLUDE_DIRS}
    )
  endif()

  target_link_libraries(edgepaint PRIVATE
    cdt
    cgraph
    edgepaintlib
    gvc
    ingraphs
    neatogen
    rbtree
    sfdpgen
    sparse
  )

  if(NOT HAVE_GETOPT_H)
    target_link_libraries(edgepaint PRIVATE ${GETOPT_LINK_LIBRARIES})
  endif()

  install(
    TARGETS edgepaint
    RUNTIME DESTINATION ${BINARY_INSTALL_DIR}
    LIBRARY DESTINATION ${LIBRARY_INSTALL_DIR}
    ARCHIVE DESTINATION ${LIBRARY_INSTALL_DIR}
  )

  if(GZIP)
    add_custom_target(man-edgepaint ALL DEPENDS edgepaint.1.gz
                      COMMENT "edgepaint man page")
    add_custom_command(
      OUTPUT edgepaint.1.gz
      COMMAND ${GZIP} -9 --no-name --to-stdout edgepaint.1
        >"${CMAKE_CURRENT_BINARY_DIR}/edgepaint.1.gz"
      MAIN_DEPENDENCY edgepaint.1
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMENT "compress edgepaint man page")
    install(
      FILES ${CMAKE_CURRENT_BINARY_DIR}/edgepaint.1.gz
      DESTINATION ${MAN_INSTALL_DIR}/man1)
  else()
    install(
      FILES edgepaint.1
      DESTINATION ${MAN_INSTALL_DIR}/man1
    )
  endif()

endif()
