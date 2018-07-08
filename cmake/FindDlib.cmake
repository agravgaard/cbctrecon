# - Find Dlib
# Find the native Dlib includes and library
#
#  DLIB_INCLUDE_DIR - where to find zlib.h, etc.
#  DLIB_LIBRARIES   - List of libraries when using zlib.
#  DLIB_FOUND       - True if zlib found.

if (NOT DLIB_DIR)
  find_path (DLIB_DIR DLIBconfig.cmake
    $ENV{DLIB_DIR}
    DOC "The build directory, containing Dlibconfig.cmake")
endif (NOT DLIB_DIR)

if (DLIB_DIR)
  if (EXISTS (${DLIB_DIR}/Dlibconfig.cmake))
    include (${DLIB_DIR}/Dlibconfig.cmake)
  endif ()
endif (DLIB_DIR)

if (DLIB_INCLUDE_DIR)
  # Already in cache, be silent
  set (Dlib_FIND_QUIETLY TRUE)
endif (DLIB_INCLUDE_DIR)

find_path (DLIB_INCLUDE_DIR "dlib/algs.h"
  PATHS "${DLIB_DIR}")

set (DLIB_NAMES dlib)
find_library (DLIB_LIBRARY NAMES ${DLIB_NAMES})

# handle the QUIETLY and REQUIRED arguments and set DLIB_FOUND to TRUE if 
# all listed variables are TRUE
include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (DLIB DEFAULT_MSG 
  DLIB_LIBRARY 
  DLIB_INCLUDE_DIR)

# Dlib on debian is linked against lapack and blas
if (DLIB_LIBRARY)
    find_package (BLAS)
    if (BLAS_FOUND)
	find_package (LAPACK)
    endif ()
    if (LAPACK_FOUND)
	list (APPEND DLIB_LIBRARY ${LAPACK_LIBRARIES})
    else ()
	set (DLIB_FOUND FALSE)
    endif ()
endif ()

if (DLIB_FOUND)
  set (DLIB_LIBRARIES ${DLIB_LIBRARY})
else (DLIB_FOUND)
  set (DLIB_LIBRARIES)
endif (DLIB_FOUND)

mark_as_advanced (DLIB_LIBRARY)
