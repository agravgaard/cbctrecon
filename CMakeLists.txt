cmake_minimum_required(VERSION 3.14) # Yes, less won't do.
cmake_policy(SET CMP0074 NEW) # For hunter to use _ROOT variables without warnings

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

set(HUNTER_PACKAGES dlib ZLIB OpenCL)

include(FetchContent)
FetchContent_Declare(SetupHunter GIT_REPOSITORY https://github.com/cpp-pm/gate)
FetchContent_MakeAvailable(SetupHunter)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif()

set(CMAKE_INCLUDE_CURRENT_DIR OFF)

# -----------------------------------------------------------------------------
# BASIC CMAKE
set(CbctRecon_VERSION_MAJOR "1") # bump at major change -> does NOT mean release version
set(CbctRecon_VERSION_MINOR "5") # bump at functionality change
set(CbctRecon_VERSION_PATCH "3") # bump at minor change

project(
  CbctRecon
  VERSION
    "${CbctRecon_VERSION_MAJOR}.${CbctRecon_VERSION_MINOR}.${CbctRecon_VERSION_PATCH}")

set(CMAKE_CXX_STANDARD
    20
    CACHE STRING "C++ standard specification")
set(CMAKE_CXX_STANDARD_REQUIRED
    ON
    CACHE STRING "Require C++ standard specification")

# Extensions are required for named union members for the opencl types which increases
# readability and safety significantly
set(CMAKE_CXX_EXTENSIONS ON)

set(CbctRecon_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
# -----------------------------------------------------------------------------

# Check Instruction set available and set intel compiler flags
include(feature_defs optional)
include(Sanitize)

option(BUILD_SHARED_LIBS
       "Build shared libs in ITK, Plastimatch and RTK (for redistribution)" OFF)
option(CBCTRECON_BUILD_SHARED_LIBS "Build shared libraries for CbctRecon"
       ${BUILD_SHARED_LIBS})
option(BUILD_TESTING "Build testing in ITK, RTK and Plastimatch" OFF)
# BUILD_TESTING is OFF by default, because ITK and RTK has decent CI tests And because ITK
# fails to download test data outside Master project source dir.

option(HUNTER_ENABLED "Enable hunter for dependency management" OFF)
option(USE_SYSTEM_QT "ON=Use local Qt, OFF=hunter downloads and configs. qtbase" ON)
option(USE_SYSTEM_OPENCL "ON=Use local OpenCL, OFF=hunter downloads and configs. OpenCL"
       ON)
option(USE_SYSTEM_ZLIB "ON=Use local Zlib, OFF=hunter downloads and configs." ON)
option(USE_SYSTEM_dlib "ON=Use local dlib, OFF=cmake downloads and configs." ON)
option(USE_SYSTEM_ITK "ON=Use local ITK, OFF=cmake downloads and configs." ON)
option(USE_SYSTEM_DCMTK "ON=Use local DCMTK, OFF=cmake downloads and configs." ON)
option(USE_SYSTEM_Plastimatch
       "ON=Use local Plastimatch, OFF=cmake downloads and configs." ON)
option(USE_HUNTER_Eigen "ON=Use Eigen from Hunter, OFF=Use Hunter in ITK." OFF)

option(USE_CUDA "Toggle CUDA on or off" OFF) # OFF by default for multiple reasons
option(USE_OPENMP "Toggle OpenMP on or off" ON)
option(USE_OPENCL "Use OpenCL as compute backend" ON)
option(USE_SYCL "Use SYCL as compute backend" OFF)
if(USE_SYCL AND USE_OPENCL)
  message(FATAL_ERROR "Only one compute backend is allowed")
elseif(NOT USE_SYCL AND NOT USE_OPENCL)
  message(FATAL_ERROR "At least one compute backend must be selected")
endif()

option(USE_ITK_DCMTK "Toggle DCMTK on or off" OFF)
option(USE_ITK_GDCM "Toggle GDCM on or off" ON)
option(USE_CLFFT "Toggle CLFFT (in development) on or off" OFF) # OFF by default because
                                                                # it doesn't yet work
option(USE_GPMC "Toggle gPMC on or off" OFF) # OFF by default because you can't expect
                                             # developers to have access to gPMC binaries
                                             # AND use VS2013 Debug when Release is much
                                             # faster for everything else.
option(USE_ITK_GDCM "Use the ITK implementation instead of downloading (Windows only)"
       OFF)
option(USE_LOWPASS_FFT "Gauss FFT as low-pass filter for scatter smoothing" OFF
)# Missing tests, so prefer not to use
option(RTK_USE_OPENCL
       "Use the custom RTK OpenCL extensions (require device image support)" OFF)

option(CBCTRECON_CLANG_COMPLETE "Generate .clang_complete for vim" ON)
option(CBCTRECON_BUILD_TESTS "Build tests for CbctRecon" ON)
option(CBCTRECON_BUILD_EXAMPLES "Build examples for CbctRecon" ON)
option(CBCTRECON_SANITIZE "Sanitize build of CbctRecon" OFF)

set(CBCTRECON_OPENCL_VERSION
    120
    CACHE STRING "Version of OpenCL to use, 1.2->120 etc.")
set(SLAB_SIZE
    16
    CACHE STRING
          "Number of projections to process simultaneously in OpenCL/CUDA fwd proj.")
add_definitions(-DCBCTRECON_OPENCL_VERSION=${CBCTRECON_OPENCL_VERSION})
if(RTK_USE_OPENCL)
  add_definitions(-DRTK_USE_OPENCL)
  add_definitions(-DSLAB_SIZE=${SLAB_SIZE})
endif()

# Windows doesn't handle /WX correctly with system (ITK) headers
option(ENABLE_PEDANTIC "Enable pedantic warnings from compiler" OFF)
if(NOT WIN32)
  set(ENABLE_PEDANTIC ON)
  option(USE_TINYREFL "Enable tinyrefl static reflection tool for CbctRecon tests" OFF
  )# ON) <- May return when TinyRefl has a stable release
  # We should prefer the official server, so we don't depend on Manu Sanchez to always
  # have a build available for us.
  set(TINYREFL_LLVM_DOWNLOAD_FROM_OFFICIAL_SERVER
      ON
      CACHE BOOL "" FORCE)
else()
  option(USE_TINYREFL "Enable tinyrefl static reflection tool for CbctRecon tests" OFF)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(USE_GPMC)
  message(STATUS "Compiling with GPMC option")
  add_definitions(-DUSE_GPMC=TRUE)
endif(USE_GPMC)

if(USE_LOWPASS_FFT)
  message(
    STATUS "Compiling with Gauss FFT as low-pass filter for scatter smoothing option")
  add_definitions(-DLOWPASS_FFT=TRUE)
endif(USE_LOWPASS_FFT)

if(USE_TINYREFL)
  message(STATUS "Compiling with tinyrefl enabled")
  add_definitions(-DUSE_TINYREFL=TRUE)
endif(USE_TINYREFL)

if(APPLE OR USE_SYSTEM_OPENCL)
  # On macOS use system OpenCL library.
  find_package(OpenCL REQUIRED)
else()
  find_package(OpenCL CONFIG REQUIRED)
endif()
if(OpenCL_FOUND)
  # add_definitions(-DUSE_OPENCL_PLM=TRUE)
  add_definitions(-DUSE_OPENCL_RTK=TRUE)
  set(OPENCL_INCLUDE_DIRS ${OpenCL_INCLUDE_DIR})
endif(OpenCL_FOUND)

if(USE_CUDA)
  message(STATUS "Compiling with CUDA option")
  if(NOT CUDACXX)
    find_package(CUDA_wrap)
    get_filename_component(CUDA_TOOLKIT_ROOT_DIR "${CUDA_TOOLKIT_INCLUDE}/.." ABSOLUTE)
  endif()
  find_package(CUDA REQUIRED)
  add_definitions(-DUSE_CUDA=TRUE)
endif(USE_CUDA)

if(ITK_USE_SYSTEM_FFTW)
  find_package(FFTW REQUIRED)
else()
  find_package(FFTW)
endif()

# set(TBB_ROOT "" CACHE PATH "Root dir of intel TBB, (one level up from lib)")
# FIND_PACKAGE(TBB) ## The world is not ready for this yet

if(USE_OPENMP)
  find_package(OpenMP REQUIRED)
  message(STATUS "Compiling with OPENMP option")
  add_definitions(-DUSE_OPENMP=TRUE)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif(USE_OPENMP)

if(USE_CLFFT)
  find_package(clFFT REQUIRED)
  message(STATUS "Compiling with clFFT option")
  add_definitions(-DUSE_CLFFT=TRUE)
endif(USE_CLFFT)

if(USE_SYSTEM_QT)
  find_package(
    Qt5
    COMPONENTS Core
               Gui
               OpenGL
               PrintSupport
               Widgets
               Test
    REQUIRED)
  if(WIN32)
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "C:\\Qt\\5.15.2\\msvc2019_64")
  endif()
else()
  hunter_add_package(Qt)
endif()
find_package(Qt5Core 5.12.3 REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(Qt5OpenGL REQUIRED)
find_package(Qt5PrintSupport REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Test REQUIRED)

if(NOT USE_SYSTEM_ZLIB)
  find_package(ZLIB CONFIG REQUIRED)
else()
  find_package(ZLIB)
endif()

# -----------------------------------------------------------------------------
# Set a default build type if none was specified
# -----------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND UNIX)
  mark_as_advanced(CMAKE_BUILD_TYPE)
  set_property(
    CACHE CMAKE_BUILD_TYPE
    PROPERTY STRINGS
             "Release"
             "Debug"
             "MinSizeRel"
             "RelWithDebInfo")
endif()

if(NOT
   CMAKE_CXX_COMPILER_ID
   STREQUAL
   "MSVC")
  include(CheckCXXSourceCompiles)
  check_cxx_source_compiles("
  #include <charconv>
  int main() {
    auto str = \"3.14\";
    auto out = 0.f;
    std::from_chars(str, str+4, out);
    return 0;
  }
  " HAS_FROM_CHARS)
  if(NOT HAS_FROM_CHARS)
    set(ABSEIL_LIBS absl::base absl::strings)
  endif()
  # Require TBB for parralel stl It should be a free choice for which implementation to
  # use, but TBB seems simplest for now
  find_package(TBB REQUIRED)
endif()
if(Module_ITKTBB AND TBB_DIR)
  include(${TBB_DIR}/TBBConfig.cmake)
elseif(NOT TBB_FOUND)
  find_package(TBB)
endif()
if(TBB_FOUND)
  set(TBB_LIBRARIES TBB::tbb)
  message(STATUS "TBB Libs: ${TBB_LIBRARIES}")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin CACHE PATH "Runtime library directory")

if(NOT USE_SYSTEM_ITK)
  # Because if we don't set it external ITK (HDF5) will set it to /bin (yes, /, as in the root dir) because they are evil like that.
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib CACHE PATH "Archive library directory")
endif()

add_subdirectory(External)

if(NOT USE_ITK_DCMTK)
  if(NOT DCMTK_FOUND AND USE_SYSTEM_DCMTK)
    find_package(DCMTK_wrap)
  else()
    include_directories("${DCMTK_BINARY_DIR}/config/include/")
    include_directories("${DCMTK_SOURCE_DIR}/dcmdata/include/")
    include_directories("${DCMTK_SOURCE_DIR}/ofstd/include/")
  endif()
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

# Plastimatch may link to an external dlib, then we'll need that:
find_package(dlib)
if(dlib_FOUND)
  set(DLIB_LIB dlib::dlib)
endif()

set(PLM_TARGETS
    plmbase
    plmutil
    plmdose
    plmbase
    plmregister
    plmreconstruct
    plmopencl
    plmsegment)

if(WIN32)
  set(WIN_LIB netapi32.lib)
endif(WIN32)

set(QT_LIBRARIES)
list(
  APPEND
  QT_LIBRARIES
  "Qt5::Core"
  "Qt5::Gui"
  "Qt5::OpenGL"
  "Qt5::PrintSupport"
  "Qt5::Widgets"
  "Qt5::Test")

set(CBCTRECON_DEP_LIBS
    ${WIN_LIB}
    ${DCMTK_LIBRARIES}
    ${RTK_LIBRARIES}
    ${CLFFT_LIBRARIES}
    ${QT_LIBRARIES}
    ${TBB_LIBRARIES}
    ${DLIB_LIB}
    ${ABSEIL_LIBS}
    OpenCL::OpenCL)

if(CBCTRECON_COVERAGE)
  # from github.com/nlohmann/json
  message(STATUS "Building test suite with coverage information")
  if(NOT
     "${CMAKE_CXX_COMPILER_ID}"
     STREQUAL
     "GNU")
    message(FATAL_ERROR "cbctrecon_Coverage requires GCC.")
  endif()

  # enable profiling
  set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} --coverage -g -O0 -fprofile-arcs -ftest-coverage")

  # from https://github.com/RWTH-HPC/CMake-codecov/blob/master/cmake/FindGcov.cmake
  get_filename_component(COMPILER_PATH "${CMAKE_CXX_COMPILER}" PATH)
  string(
    REGEX MATCH
          "^[0-9]+"
          GCC_VERSION
          "${CMAKE_CXX_COMPILER_VERSION}")
  find_program(
    GCOV_BIN
    NAMES gcov-${GCC_VERSION} gcov
    HINTS ${COMPILER_PATH})

  # collect all source files from the chosen include dir
  file(GLOB_RECURSE SOURCE_H_FILES ${CbctRecon_SOURCE_DIR}/Library/*.h*)
  file(GLOB_RECURSE SOURCE_CPP_FILES ${CbctRecon_SOURCE_DIR}/Library/*.c[xp][xp])
  set(SOURCE_FILES ${SOURCE_H_FILES} ${SOURCE_CPP_FILES})

  # add target to collect coverage information and generate HTML file (filter script from
  # https://stackoverflow.com/a/43726240/266378)
  add_custom_target(
    lcov_html
    COMMAND lcov --directory . --capture --output-file cbctrecon.info --rc
            lcov_branch_coverage=1
    COMMAND lcov -e cbctrecon.info ${SOURCE_FILES} --output-file cbctrecon.info.filtered
            --gcov-tool ${GCOV_BIN} --rc lcov_branch_coverage=1
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/Testing/thirdparty/imapdl/filterbr.py
            cbctrecon.info.filtered > cbctrecon.info.filtered.noexcept
    COMMAND genhtml --title "CbctRecon" --legend --demangle-cpp --output-directory html
            --show-details --branch-coverage cbctrecon.info.filtered.noexcept
    COMMENT "Generating HTML report html/index.html")

  add_custom_target(
    fastcov_html
    COMMAND
      ${CMAKE_CURRENT_SOURCE_DIR}/Testing/thirdparty/fastcov/fastcov.py --branch-coverage
      --lcov -o cbctrecon.info --gcov ${GCOV_BIN} --compiler-directory ${CMAKE_BINARY_DIR}
      --source-files ${SOURCE_FILES}
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/Testing/thirdparty/imapdl/filterbr.py
            cbctrecon.info > cbctrecon.info.noexcept
    COMMAND genhtml --title "CbctRecon" --legend --demangle-cpp --output-directory html
            --show-details --branch-coverage cbctrecon.info.noexcept
    COMMENT "Generating HTML report html/index.html")
endif()

include(GNUInstallDirs)
add_subdirectory(Library)
add_subdirectory(Applications)

if(CBCTRECON_BUILD_TESTS)
  include(${CbctRecon_SOURCE_DIR}/cmake/ctest_setup.cmake)
  message(STATUS "Configuring CbctRecon tests...")
  add_subdirectory(Testing)
  include(ExternalData)
  ExternalData_add_target(CbctData)

  if(CBCTRECON_BUILD_EXAMPLES)
    message(STATUS "Configuring CbctRecon examples...")
    add_subdirectory(Examples)
  endif()
else()
  if(CBCTRECON_BUILD_EXAMPLES)
    message(WARNING "Examples can't be built without tests!")
  endif()
endif()
