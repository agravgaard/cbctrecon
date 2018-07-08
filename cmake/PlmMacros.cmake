##-----------------------------------------------------------------------------
##  See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
##-----------------------------------------------------------------------------
##-----------------------------------------------------------------------------
##  A few constants used to control build & install
##-----------------------------------------------------------------------------
set (INSTALL_NEVER 0)
set (INSTALL_ALWAYS 1)
if (PLM_CONFIG_DEBIAN_BUILD)
  set (INSTALL_IF_NOT_DEBIAN 0)
else ()
  set (INSTALL_IF_NOT_DEBIAN 1)
endif ()

set (BUILD_NEVER 0)
set (BUILD_ALWAYS 1)

##-----------------------------------------------------------------------------
##  Create enum options
##-----------------------------------------------------------------------------
macro (option_enum
    option_name
    option_descr
    option_value
    )
  set (${option_name} ${option_value} CACHE STRING ${option_descr})
  set_property (CACHE ${option_name} PROPERTY STRINGS ${ARGN})
endmacro ()

##-----------------------------------------------------------------------------
##  Save and restore variables
##-----------------------------------------------------------------------------
macro (push_var _var)
  set (${_var}_BACKUP ${${_var}})
endmacro ()

macro (push_vars)
  foreach (_var IN ITEMS ${ARGN})
    push_var (${_var})
  endforeach ()
endmacro ()

macro (pop_var _var)
  set (${_var} ${${_var}_BACKUP})
endmacro ()

macro (pop_vars)
  foreach (_var IN ITEMS ${ARGN})
    pop_var (${_var})
  endforeach ()
endmacro ()

##-----------------------------------------------------------------------------
##  Macros for creating targets
##-----------------------------------------------------------------------------
macro (PLM_ADD_LIBRARY
    TARGET_NAME
    TARGET_SRC
    TARGET_LIBS
    TARGET_LDFLAGS
    TARGET_INCLUDE_DIRECTORIES
    TARGET_INCLUDE_FILES
    )

  add_library (${TARGET_NAME} ${TARGET_SRC})
  set_target_properties (${TARGET_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${PLM_BINARY_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${PLM_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${PLM_BINARY_DIR}"
    PUBLIC_HEADER "${TARGET_INCLUDE_FILES}")
  if (PLM_CONFIG_INSTALL_LIBRARIES)
    if (NOT PLM_PACKAGE_LEGACY_CMAKE_CONFIG)
      target_include_directories(${TARGET_NAME} INTERFACE
	${TARGET_INCLUDE_DIRECTORIES})
    endif ()
    install (TARGETS ${TARGET_NAME}
      EXPORT PlastimatchLibraryDepends
      RUNTIME DESTINATION "${PLM_INSTALL_BIN_DIR}"
      LIBRARY DESTINATION "${PLM_INSTALL_LIB_DIR}"
      ARCHIVE DESTINATION "${PLM_INSTALL_LIB_DIR}"
      PUBLIC_HEADER DESTINATION "${PLM_INSTALL_INCLUDE_DIR}"
      )
  endif ()
  target_link_libraries (${TARGET_NAME} ${TARGET_LIBS})
  if (NOT ${TARGET_LDFLAGS} STREQUAL "")
    set_target_properties(${TARGET_NAME}
      PROPERTIES LINK_FLAGS ${TARGET_LDFLAGS})
  endif ()

  # Decorate .so on unix
  set_target_properties(${TARGET_NAME}
      PROPERTIES SOVERSION "${PLM_VERSION_MAJOR}.${PLM_VERSION_MINOR}")
endmacro ()

# Static libraries used when they aren't properly decorated for windows
macro (PLM_ADD_STATIC_LIBRARY
    TARGET_NAME TARGET_SRC TARGET_LIBS TARGET_LDFLAGS TARGET_INCLUDES)

  add_library (${TARGET_NAME} STATIC ${TARGET_SRC})

  set_target_properties (${TARGET_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${PLM_BINARY_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${PLM_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${PLM_BINARY_DIR}"
    PUBLIC_HEADER "${TARGET_INCLUDES}")

  if (PLM_CONFIG_INSTALL_LIBRARIES)
    install (TARGETS ${TARGET_NAME}
      EXPORT PlastimatchLibraryDepends
      RUNTIME DESTINATION "${PLM_INSTALL_BIN_DIR}"
      LIBRARY DESTINATION "${PLM_INSTALL_LIB_DIR}"
      ARCHIVE DESTINATION "${PLM_INSTALL_LIB_DIR}"
      PUBLIC_HEADER DESTINATION "${PLM_INSTALL_INCLUDE_DIR}"
      )
  endif ()

  target_link_libraries (${TARGET_NAME} ${TARGET_LIBS})
  if (NOT ${TARGET_LDFLAGS} STREQUAL "")
    set_target_properties(${TARGET_NAME}
      PROPERTIES LINK_FLAGS ${TARGET_LDFLAGS})
  endif ()
endmacro ()

macro (PLM_ADD_GPU_PLUGIN_LIBRARY TARGET_NAME TARGET_SRC)

  # Add library target
  cuda_add_library (${TARGET_NAME} SHARED ${TARGET_SRC})

  # Set output directory.  No PUBLIC_HEADER directory is needed,
  # because they don't have a public API.
  set_target_properties (${TARGET_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${PLM_BINARY_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${PLM_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${PLM_BINARY_DIR}")

  # Set installation diretory and export definition.  No PUBLIC_HEADER needed.
  if (PLM_CONFIG_INSTALL_LIBRARIES)
    install (TARGETS ${TARGET_NAME}
      EXPORT PlastimatchLibraryDepends
      RUNTIME DESTINATION "${PLM_INSTALL_BIN_DIR}"
      LIBRARY DESTINATION "${PLM_INSTALL_LIB_DIR}"
      ARCHIVE DESTINATION "${PLM_INSTALL_LIB_DIR}"
      )
  endif ()

  # Decorate .so on unix
  set_target_properties(${TARGET_NAME}
      PROPERTIES SOVERSION "${PLM_VERSION_MAJOR}.${PLM_VERSION_MINOR}")
endmacro ()

## New version, excludes TARGET_BUILD
macro (plm_add_executable_v3
    TARGET_NAME
    TARGET_SRC
    TARGET_INCLUDES
    TARGET_LIBS
    TARGET_LDFLAGS
    TARGET_INSTALL)

  add_executable (${TARGET_NAME} ${TARGET_SRC})
  target_link_libraries (${TARGET_NAME} ${TARGET_LIBS})
  target_include_directories (${TARGET_NAME} PRIVATE ${TARGET_INCLUDES})
  set_target_properties (${TARGET_NAME}
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PLM_BINARY_DIR}")
  if (NOT ${TARGET_LDFLAGS} STREQUAL "")
    set_target_properties(${TARGET_NAME}
      PROPERTIES LINK_FLAGS ${TARGET_LDFLAGS})
  endif ()
  # CXX linkage required for nlopt
  set_target_properties (${TARGET_NAME} PROPERTIES LINKER_LANGUAGE CXX)
  if (${TARGET_INSTALL})
    install (TARGETS ${TARGET_NAME} DESTINATION "${PLM_INSTALL_BIN_DIR}")
  endif ()
endmacro ()

macro (PLM_ADD_OPENCL_FILE SRCS CL_FILE)
  # I don't yet know how to bundle the .cl file within the executable.
  # Therefore, copy the .cl into binary directory.
  set (${SRCS} ${${SRCS}} "${PLM_BINARY_DIR}/${CL_FILE}")
  add_custom_command (
    OUTPUT "${PLM_BINARY_DIR}/${CL_FILE}"
    COMMAND ${CMAKE_COMMAND} "-E" "copy"
    "${CMAKE_CURRENT_SOURCE_DIR}/${CL_FILE}"
    "${PLM_BINARY_DIR}/${CL_FILE}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${CL_FILE}")
  # Need in the testing directory too :(
  set (${SRCS} ${${SRCS}} "${PLM_BUILD_TESTING_DIR}/${CL_FILE}")
  add_custom_command (
    OUTPUT "${PLM_BUILD_TESTING_DIR}/${CL_FILE}"
    COMMAND ${CMAKE_COMMAND} "-E" "copy"
    "${CMAKE_CURRENT_SOURCE_DIR}/${CL_FILE}"
    "${PLM_BUILD_TESTING_DIR}/${CL_FILE}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${CL_FILE}")
endmacro ()

macro (PLM_ADD_TARGET_COPY TARGET SRC DEST DEPENDENCY)
  add_custom_target (${TARGET} ALL DEPENDS "${DEST}")
  add_custom_command (
      OUTPUT "${DEST}"
      COMMAND ${CMAKE_COMMAND} "-E" "copy" "${SRC}" "${DEST}"
      DEPENDS ${DEPENDENCY}
      )
endmacro ()

macro (PLM_SET_SSE2_FLAGS)
  foreach (SRC ${ARGN})
    # JAS 08.19.2010 - Unfortunately, this doesn't work.
    #  SET_PROPERTY(
    #      SOURCE bspline.c
    #      APPEND PROPERTY COMPILE_DEFINITIONS ${SSE2_FLAGS}
    #      )
    # So, we ask CMake more forcefully to add additional compile flags
    get_source_file_property (FILE_FLAGS ${SRC} COMPILE_FLAGS)
    if (FILE_FLAGS AND NOT FILE_FLAGS MATCHES "NONE")
      set (FILE_FLAGS "${FILE_FLAGS} -msse2")
    else ()
      set (FILE_FLAGS "-msse2")
    endif ()
    set_source_files_properties (
      ${SRC} PROPERTIES COMPILE_FLAGS "${FILE_FLAGS}")
  endforeach ()
endmacro ()

macro (PLM_SET_PIC_FLAGS)
    if (CMAKE_VERSION VERSION_LESS "2.8.9")
	if ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
	    set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fPIC")
	    set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fPIC")
	    set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fPIC")
	    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fPIC")
	endif ()
    else ()
	set (CMAKE_POSITION_INDEPENDENT_CODE TRUE)
    endif ()
endmacro ()
