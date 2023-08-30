add_library(ingraphs STATIC
  # Header files
  ingraphs.h

  # Source files
  ingraphs.c
)

target_include_directories(ingraphs PRIVATE
  ..
  ../cdt
  ../cgraph
)

target_link_libraries(ingraphs PRIVATE
  cgraph
)
