if (HAVE_AVX2)
  set(USE_AVX_INSTRUCTIONS ON CACHE BOOL)
endif()

# CUDA
if(USE_CUDA)
  set(DLIB_USE_CUDA ON CACHE BOOL)
else()
  set(DLIB_USE_CUDA OFF CACHE BOOL)
endif()

set_directory_properties(PROPERTIES EXCLUDE_FROM_ALL YES)
FetchContent_MakeAvailable(GDCM)
