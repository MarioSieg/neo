if(with_sfdp AND ANN_FOUND)

  add_executable(mingle
    minglemain.cpp
  )

  target_include_directories(mingle
    PRIVATE
      ../../lib
      ../../lib/cdt
      ../../lib/cgraph
      ../../lib/common
  )

  if(GETOPT_FOUND)
    target_include_directories(mingle
      SYSTEM PRIVATE
        ${GETOPT_INCLUDE_DIRS}
    )
  endif()

  target_link_libraries(mingle
    cdt
    cgraph
    common
    ingraphs
    libmingle
    neatogen
    rbtree
    sfdpgen
    sparse
    ${ANN_LIBRARIES}
  )
  if(NOT HAVE_GETOPT_H)
    target_link_libraries(mingle ${GETOPT_LINK_LIBRARIES})
  endif()

  install(
    TARGETS mingle
    RUNTIME DESTINATION ${BINARY_INSTALL_DIR}
    LIBRARY DESTINATION ${LIBRARY_INSTALL_DIR}
    ARCHIVE DESTINATION ${LIBRARY_INSTALL_DIR}
  )

  if(GZIP)
    add_custom_target(man-mingle ALL DEPENDS mingle.1.gz
                      COMMENT "mingle man page")
    add_custom_command(
      OUTPUT mingle.1.gz
      COMMAND ${GZIP} -9 --no-name --to-stdout mingle.1
        >"${CMAKE_CURRENT_BINARY_DIR}/mingle.1.gz"
      MAIN_DEPENDENCY mingle.1
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMENT "compress mingle man page")
    install(
      FILES ${CMAKE_CURRENT_BINARY_DIR}/mingle.1.gz
      DESTINATION ${MAN_INSTALL_DIR}/man1)
  else()
    install(
      FILES mingle.1
      DESTINATION ${MAN_INSTALL_DIR}/man1
    )
  endif()

endif()
