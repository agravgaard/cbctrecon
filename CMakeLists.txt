CMAKE_MINIMUM_REQUIRED (VERSION 3.14) # Yes, less won't do.

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

include(HunterGate)
HunterGate(  # Latest version: 27/01/2019
    URL "https://github.com/ruslo/hunter/archive/v0.23.89.tar.gz"
    SHA1 "a370290a8b32987755a36d64807bf8c7f5b61730"
)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

#-----------------------------------------------------------------------------
# BASIC CMAKE
set(CbctRecon_VERSION_MAJOR "1") # bump at major change -> does NOT mean release version
set(CbctRecon_VERSION_MINOR "4") # bump at functionality change
set(CbctRecon_VERSION_PATCH "3") # bump at minor change

project(CbctRecon
  VERSION "${CbctRecon_VERSION_MAJOR}.${CbctRecon_VERSION_MINOR}.${CbctRecon_VERSION_PATCH}"
  )

SET(CMAKE_CXX_STANDARD          14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        ON)
# EXT Required for opencl named types cl_float3 val -> val.x .y & .z

set(CbctRecon_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
#-----------------------------------------------------------------------------

# Check Instruction set available and set intel compiler flags
include(feature_defs OPTIONAL)
include(Sanitize)

option(USE_SYSTEM_QT "ON=Use local Qt, OFF=hunter downloads and configs. qtbase" ON)
option(USE_SYSTEM_OPENCL "ON=Use local OpenCL, OFF=hunter downloads and configs. OpenCL" ON)
option(USE_SYSTEM_ZLIB "ON=Use local Zlib, OFF=hunter downloads and configs. Zlib" OFF)
OPTION(USE_CUDA "Toggle CUDA on or off" OFF) # OFF by default for multiple reasons
OPTION(USE_OPENMP "Toggle OpenMP on or off" ON)
OPTION(USE_DCMTK "Toggle DCMTK on or off" ON) # ON by default because PLM can't figure out what to do by itself.
OPTION(USE_CLFFT "Toggle CLFFT (in development) on or off" OFF) # OFF by default because it doesn't yet work
OPTION(USE_GPMC "Toggle gPMC on or off" OFF) # OFF by default because you can't expect developers to have access to gPMC binaries AND use VS2013 Debug when Release is much faster for everything else.
option(USE_ITK_GDCM OFF "Use the ITK implementation instead of downloading (Windows only)")
OPTION(USE_LOWPASS_FFT "Gauss FFT as low-pass filter for scatter smoothing" ON)
OPTION(CBCTRECON_CLANG_COMPLETE "Generate .clang_complete for vim" ON)
OPTION(CBCTRECON_BUILD_TESTS "Build tests for CbctRecon" ON)
option(CBCTRECON_SANITIZE "Sanitize build of CbctRecon" ON)

if(CMAKE_COMPILER_IS_GNUCXX AND ${CBCTRECON_BUILD_TESTS} AND ${USE_CUDA})
  set(TMP_COVERAGE ON)  # gcov requires gcc and tests to be build.
  # CUDA is to only do this on private GitLab runners and to enable most routes
endif()
OPTION(CBCTRECON_COVERAGE "Get test coverage report for CbctReconLib" ${TMP_COVERAGE})


OPTION(BUILD_SHARED_LIBS "Build shared libs in ITK, Plastimatch and RTK (for redistribution)" ON)
if(NOT WIN32)
  set(BUILD_SHARED_LIBS OFF)
  OPTION(USE_TINYREFL "Enable tinyrefl static reflection tool for CbctRecon tests" OFF) # ON) <- May return when TinyRefl has a stable release
  # We should prefer the official server, so we don't depend on Manu Sanchez to always have a build available for us.
  set(TINYREFL_LLVM_DOWNLOAD_FROM_OFFICIAL_SERVER ON CACHE BOOL "" FORCE)
else()
  OPTION(USE_TINYREFL "Enable tinyrefl static reflection tool for CbctRecon tests" OFF)
endif()
OPTION(CBCTRECON_BUILD_SHARED_LIBS "Build shared libraries for CbctRecon" ${BUILD_SHARED_LIBS})

OPTION(BUILD_TESTING "Build testing in ITK, RTK and Plastimatch" OFF)
# BUILD_TESTING is OFF by default, because ITK and RTK has decent CI tests
# And because ITK fails to download test data outside Master project source dir.

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

IF(USE_GPMC)
  message (STATUS "Compiling with GPMC option")
  add_definitions(-DUSE_GPMC=TRUE)
ENDIF(USE_GPMC)

IF(USE_LOWPASS_FFT)
  message (STATUS "Compiling with Gauss FFT as low-pass filter for scatter smoothing option")
  ADD_DEFINITIONS(-DLOWPASS_FFT=TRUE)
ENDIF(USE_LOWPASS_FFT)

IF(USE_TINYREFL)
  message (STATUS "Compiling with tinyrefl enabled")
  ADD_DEFINITIONS(-DUSE_TINYREFL=TRUE)
ENDIF(USE_TINYREFL)

if(APPLE OR USE_SYSTEM_OPENCL)
    # On macOS use system OpenCL library.
    find_package(OpenCL REQUIRED)
else()
    hunter_add_package(OpenCL)  # Requires mesa-common-dev on ubuntu
    find_package(OpenCL CONFIG REQUIRED)
endif()
if(OpenCL_FOUND)
  add_definitions(-DUSE_OPENCL_PLM=TRUE)
  add_definitions(-DUSE_OPENCL_RTK=TRUE)
  set(OPENCL_INCLUDE_DIRS ${OpenCL_INCLUDE_DIR})
endif(OpenCL_FOUND)

IF(USE_CUDA)
  message (STATUS "Compiling with CUDA option")
  if (NOT CUDACXX)
    FIND_PACKAGE(CUDA_wrap)
    GET_FILENAME_COMPONENT(CUDA_TOOLKIT_ROOT_DIR "${CUDA_TOOLKIT_INCLUDE}/.." ABSOLUTE)
  endif ()
  FIND_PACKAGE(CUDA REQUIRED)
  add_definitions(-DUSE_CUDA=TRUE)
ENDIF(USE_CUDA)

FIND_PACKAGE(FFTW)

# set(TBB_ROOT "" CACHE PATH "Root dir of intel TBB, (one level up from lib)")
# FIND_PACKAGE(TBB) ## The world is not ready for this yet

IF(USE_OPENMP)
  FIND_PACKAGE(OpenMP REQUIRED)
  message(STATUS "Compiling with OPENMP option")
  add_definitions(-DUSE_OPENMP=TRUE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
ENDIF(USE_OPENMP)

IF(USE_CLFFT)
  FIND_PACKAGE(clFFT REQUIRED)
  message(STATUS "Compiling with clFFT option")
  add_definitions(-DUSE_CLFFT=TRUE)
ENDIF(USE_CLFFT)

if(USE_SYSTEM_QT)
  find_package(Qt5 COMPONENTS Core Gui OpenGL PrintSupport Widgets Test REQUIRED)
  if(WIN32)
    set(CMAKE_PREFIX_PATH
      ${CMAKE_PREFIX_PATH}
      "C:\\Qt\\5.12.1\\msvc2015_64"
      )
  endif()
else()
  hunter_add_package(Qt)
endif()
find_package(Qt5Core 5.9 REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(Qt5OpenGL REQUIRED)
find_package(Qt5PrintSupport REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Test REQUIRED)

if(NOT USE_SYSTEM_ZLIB)
  hunter_add_package(ZLIB)
  find_package(ZLIB CONFIG REQUIRED)
else()
  find_package(ZLIB)
endif()

#-----------------------------------------------------------------------------
# Set a default build type if none was specified
#-----------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND UNIX)
  mark_as_advanced(CMAKE_BUILD_TYPE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Release" "Debug"
    "MinSizeRel" "RelWithDebInfo")
endif()

set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}")
set(CMAKE_INSTALL_BINDIR "${CMAKE_BINARY_DIR}/bin" CACHE PATH "")
set(CMAKE_INSTALL_DATAROOTDIR "${CMAKE_BINARY_DIR}/share" CACHE PATH "")
set(CMAKE_INSTALL_INCLUDEDIR "${CMAKE_BINARY_DIR}/include" CACHE PATH "")
set(CMAKE_INSTALL_LIBDIR "${CMAKE_BINARY_DIR}/lib64" CACHE PATH "")
set(LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib" CACHE PATH "")
set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/lib" CACHE PATH "")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/bin" CACHE PATH "")
set(EXECUTEABLE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" CACHE PATH "")

add_subdirectory(External)

if(NOT DCMTK_FOUND AND USE_DCMTK)
  if(NOT USE_SYSTEM_DCMTK)
    set(DCMTK_DIR "${CMAKE_BINARY_DIR}")
  endif()
  find_package(DCMTK_wrap)
endif()

if(NOT ITK_FOUND)
  if(NOT USE_SYSTEM_ITK)
    set(ITK_DIR "${CMAKE_BINARY_DIR}/ITK-build")
  endif()
  find_package(ITK REQUIRED)
  include(${ITK_USE_FILE})
endif()

if(USE_SYSTEM_RTK)
  find_package(RTK REQUIRED)
  include(${RTK_USE_FILE})
endif()

set(PLM_TARGETS
  plmbase
  plmutil
  plmdose
  plmbase
  plmregister
  plmreconstruct
  plmopencl
  plmsegment
)

if(WIN32)
  set(WIN_LIB netapi32.lib)
endif(WIN32)

set(QT_LIBRARIES)
list(APPEND QT_LIBRARIES
  "Qt5::Core"
  "Qt5::Gui"
  "Qt5::OpenGL"
  "Qt5::PrintSupport"
  "Qt5::Widgets"
  "Qt5::Test"
  )

set(CBCTRECON_DEP_LIBS
  ${WIN_LIB}
  ${DCMTK_LIBRARIES}
  ${RTK_LIBRARIES}
  ${CLFFT_LIBRARIES}
  ${QT_LIBRARIES}
  ${TBB_LIBRARIES}
  OpenCL::OpenCL
)
if(CBCTRECON_COVERAGE)
  include(CodeCoverage)
  APPEND_COVERAGE_COMPILER_FLAGS()
endif()

add_subdirectory(Library)
add_subdirectory(Applications)

if(CBCTRECON_BUILD_TESTS)
  include(${CbctRecon_SOURCE_DIR}/cmake/ctest_setup.cmake)
  message(STATUS "Configuring CbctRecon tests...")
  add_subdirectory(Testing)
  include(ExternalData)
  ExternalData_Add_Target(CbctData)
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    message(STATUS "Generate vim YCM config file for CbctRecon at ${CbctRecon_SOURCE_DIR} ...")
    configure_file(
        ${CbctRecon_SOURCE_DIR}/.ycm_extra_conf.py.in
        ${CbctRecon_SOURCE_DIR}/.ycm_extra_conf.py
    )
endif()
