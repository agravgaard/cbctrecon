# Download Eigen
if(USE_HUNTER_Eigen)
  hunter_add_package(Eigen)
  find_package(Eigen3 CONFIG REQUIRED)
  set(ITK_USE_SYSTEM_EIGEN
      ON
      CACHE BOOL "Use Eigen from hunter (to avoid export error)")
endif()

set(BUILD_EXAMPLES
    OFF
    CACHE BOOL "" FORCE)

# Sorry, but apparently ITK cannot figure out how to download test data.
set(BUILD_TESTING
    ${BUILD_TESTING}
    CACHE BOOL "" FORCE)

set(ITK_BUILD_DEFAULT_MODULES
    OFF
    CACHE BOOL "" FORCE)
set(Module_ITKReview
    ON
    CACHE BOOL "" FORCE)
set(ITK_WRAPPING
    OFF
    CACHE BOOL "" FORCE)
# set(ITKV4_COMPATIBILITY ON CACHE BOOL "" FORCE)
set(Module_ITKVtkGlue
    OFF
    CACHE BOOL "" FORCE)

# DCMTK
if(NOT USE_ITK_DCMTK)
  set(ITK_USE_SYSTEM_DCMTK
      ON
      CACHE BOOL "" FORCE)
  set(DCMTK_DIR
      "${DCMTK_BINARY_DIR}/../"
      CACHE PATH "" FORCE)
else()
  set(Module_ITKDCMTK
      ON
      CACHE BOOL "" FORCE)
  set(ITK_USE_SYSTEM_DCMTK
      OFF
      CACHE BOOL "" FORCE)
endif()

# ZLIB
if(WIN32)
  set(ITK_USE_SYSTEM_ZLIB
      OFF
      CACHE BOOL "" FORCE)
else()
  set(ITK_USE_SYSTEM_ZLIB
      ON
      CACHE BOOL "" FORCE)
endif()

# GDCM
if((NOT WIN32) OR USE_ITK_GDCM)
  set(ITK_USE_SYSTEM_GDCM
      OFF
      CACHE BOOL "" FORCE)
else()
  set(ITK_USE_SYSTEM_GDCM
      ON
      CACHE BOOL "" FORCE)
  if(NOT USE_SYSTEM_GDCM)
    set(GDCM_DIR ${GDCM_BINARY_DIR})
  endif()
endif()

# RTK
set(Module_RTK
    ON
    CACHE BOOL "" FORCE)
set(RTK_BUILD_APPLICATIONS
    OFF
    CACHE BOOL "")
set(RTK_USE_CUDA
    ${USE_CUDA}
    CACHE BOOL "")
set(Module_ITKCudaCommon
    ${USE_CUDA}
    CACHE BOOL "")

# The RTK tag ITK uses has a bug in some macros, fixed in master:
set(REMOTE_GIT_TAG_RTK
    "master"
    CACHE BOOL "")

# Plastimatch needs itkVectorResampleImageFilter:
set(Module_ITKDeprecated
    ON
    CACHE BOOL "")

# GPU
set(ITK_USE_GPU
    ON
    CACHE BOOL "" FORCE)
set(OPENCL_INCLUDE_DIRS
    ${OpenCL_INCLUDE_DIR}
    CACHE PATH "" FORCE)
set(OPENCL_LIBRARIES
    ${OpenCL_LIBRARY}
    CACHE FILEPATH "" FORCE)

# CUDA
set(CUDA_HAVE_GPU
    ${USE_CUDA}
    CACHE BOOL "" FORCE)
set(CUDA_SEPARABLE_COMPILATION
    OFF
    CACHE BOOL "" FORCE)
