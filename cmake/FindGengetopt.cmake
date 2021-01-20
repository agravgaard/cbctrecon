# Attempt to find gengetopt. If not found, compile it.
find_program(GENGETOPT gengetopt)
if((GENGETOPT STREQUAL "GENGETOPT-NOTFOUND") OR (GENGETOPT STREQUAL ""))
  get_filename_component(CLITK_CMAKE_DIR ${CMAKE_CURRENT_LIST_FILE} PATH)
  add_subdirectory(${CLITK_CMAKE_DIR}/../Applications/gengetopt
                   ${CMAKE_CURRENT_BINARY_DIR}/gengetopt)
else((GENGETOPT STREQUAL "GENGETOPT-NOTFOUND") OR (GENGETOPT STREQUAL ""))
  if(EXISTS ${GENGETOPT})
    add_executable(gengetopt IMPORTED)
    set_property(TARGET gengetopt PROPERTY IMPORTED_LOCATION ${GENGETOPT})
  else(EXISTS ${GENGETOPT})
    set(GENGETOPT
        "GENGETOPT-NOTFOUND"
        CACHE FILEPATH "Path to a program." FORCE)
    message(FATAL_ERROR "No gengetopt executable found at the specified location")
  endif(EXISTS ${GENGETOPT})
endif((GENGETOPT STREQUAL "GENGETOPT-NOTFOUND") OR (GENGETOPT STREQUAL ""))

# Create a cmake script to cat a list of files
file(
  WRITE ${CMAKE_BINARY_DIR}/cat.cmake
  "
FILE(WRITE \${OUTPUT} \"\")
SET(LIST_INPUTS \${INPUTS})
SEPARATE_ARGUMENTS(LIST_INPUTS)
FOREACH(INPUT \${LIST_INPUTS})
  FILE(READ \${INPUT} CONTENT)
  FILE(APPEND \${OUTPUT} \"\${CONTENT}\")
ENDFOREACH()
")

macro(WRAP_GGO GGO_SRCS)

  # Set current list of files to zero for a new target
  set(GGO_FILES_ABS "")

  # Convert list of a file in a list with absolute file names
  foreach(GGO_FILE ${ARGN})
    get_filename_component(GGO_FILE_ABS ${GGO_FILE} ABSOLUTE)
    list(APPEND GGO_FILES_ABS ${GGO_FILE_ABS})
  endforeach()

  # Append to a new ggo file containing all files
  list(
    GET
    GGO_FILES_ABS
    0
    FIRST_GGO_FILE)
  get_filename_component(FIRST_GGO_BASEFILENAME ${FIRST_GGO_FILE} NAME)
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${FIRST_GGO_BASEFILENAME}"
    COMMAND
      ${CMAKE_COMMAND} -D INPUTS="${GGO_FILES_ABS}" -D
      OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/${FIRST_GGO_BASEFILENAME} -P
      ${CMAKE_BINARY_DIR}/cat.cmake
    DEPENDS ${GGO_FILES_ABS})
  set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/${FIRST_GGO_BASEFILENAME}
                              PROPERTIES GENERATED TRUE)

  # Now add ggo command
  get_filename_component(GGO_BASEFILENAME ${FIRST_GGO_FILE} NAME_WE)
  set(GGO_H ${GGO_BASEFILENAME}_ggo.h)
  set(GGO_C ${GGO_BASEFILENAME}_ggo.c)
  set(GGO_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${GGO_H}
                 ${CMAKE_CURRENT_BINARY_DIR}/${GGO_C})
  add_custom_command(
    OUTPUT ${GGO_OUTPUT}
    COMMAND
      gengetopt ARGS < ${CMAKE_CURRENT_BINARY_DIR}/${FIRST_GGO_BASEFILENAME}
      --output-dir=${CMAKE_CURRENT_BINARY_DIR}
      --arg-struct-name=args_info_${GGO_BASEFILENAME}
      --func-name=cmdline_parser_${GGO_BASEFILENAME} --file-name=${GGO_BASEFILENAME}_ggo
      --unamed-opts --conf-parser --include-getopt
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${FIRST_GGO_BASEFILENAME})
  set(${GGO_SRCS} ${${GGO_SRCS}} ${GGO_OUTPUT})
  include_directories(${CMAKE_CURRENT_BINARY_DIR})

  set_source_files_properties(${${GGO_SRCS}} PROPERTIES GENERATED TRUE)
  if(CMAKE_COMPILER_IS_GNUCXX)
    find_program(DEFAULT_GCC gcc)
    exec_program(
      ${DEFAULT_GCC} ARGS
      "-dumpversion"
      OUTPUT_VARIABLE GCCVER)
    if("${GCCVER}" VERSION_GREATER "4.5.2")
      set_source_files_properties(${${GGO_SRCS}}
                                  PROPERTIES COMPILE_FLAGS "-Wno-unused-but-set-variable")
    endif("${GCCVER}" VERSION_GREATER "4.5.2")
  endif(CMAKE_COMPILER_IS_GNUCXX)
  if(MSVC)
    set_source_files_properties(${${GGO_SRCS}} PROPERTIES COMPILE_FLAGS "/W0")
  endif(MSVC)
endmacro(WRAP_GGO)
