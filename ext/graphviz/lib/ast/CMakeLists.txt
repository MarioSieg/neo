add_library(ast STATIC
  # Header files
  ast.h
  error.h

  # Source files
  pathpath.c
  chresc.c
  chrtoi.c
  error.c
  fmtbuf.c
  fmtesc.c
  pathaccess.c
  pathcanon.c
  pathcat.c
  pathfind.c
  strcopy.c
  stresc.c
  strmatch.c
)

target_include_directories(ast PRIVATE
  ..
)

if(WIN32 AND NOT MINGW)
  target_include_directories(ast PRIVATE ../../windows/include/unistd)
endif()
