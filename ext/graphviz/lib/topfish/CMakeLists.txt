if(with_smyrna)

  add_library(topfish STATIC
    # Header files
    hierarchy.h

    # Source files
    hierarchy.c
    rescale_layout.c
  )

  target_include_directories(topfish PRIVATE
    ..
    ../common
  )

  target_link_libraries(topfish PRIVATE
    gvc
    neatogen
  )

endif()
