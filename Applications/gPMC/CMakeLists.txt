cmake_minimum_required(VERSION 3.8)

project(gPMC)

set(GPMC_VERSION_MAJOR "2")
set(GPMC_VERSION_MINOR "0")
set(GPMC_VERSION_PATCH "0")

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

find_package(OpenCL REQUIRED)
find_package(Gengetopt)
find_package(OpenMP)

if(OPENMP_FOUND)
  message(STATUS "Compiling with OPENMP option")
  add_definitions(-DUSE_OPENMP=TRUE)
endif(OPENMP_FOUND)

set(ITK_SRC_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/../ITKv4"
    CACHE PATH "ITK source directory")
set(ITK_BUILD_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/ITKv120-build"
    CACHE PATH "ITK build directory")

include(ExternalProject)
include(cmake/External_ITKv120.cmake)
include_directories(${ITK_BUILD_DIR})
include_directories(${ITK_BUILD_DIR}/Modules/Core/Common)
find_package(ITK REQUIRED)
message(STATUS "ITK_DIR = ${ITK_DIR}")
if(ITK_FOUND)
  include(${ITK_USE_FILE})
endif()

set(GPMC_LIB_DIR "${PROJECT_SOURCE_DIR}/bin")
set(GPMC_LIBRARIES "${GPMC_LIB_DIR}/goPMC.lib" "${GPMC_LIB_DIR}/dcmtk.lib"
                   "${GPMC_LIB_DIR}/libDicomRT.lib")

wrap_ggo(
  gPMC_GGO_C
  gPMC.ggo
  gPMC3Doutputdose_section.ggo
  gPMCVersion.ggo.in)

add_executable(gPMC gPMC.cpp ${gPMC_GGO_C})
include_directories("${PROJECT_SOURCE_DIR}/include")

message(STATUS "CMake of gPMC was found :: ${PROJECT_SOURCE_DIR}")
add_dependencies(gPMC ITKv120)
target_link_libraries(
  gPMC
  ${GPMC_LIBRARIES}
  ${ITK_LIBRARIES}
  OpenCL::OpenCL)

set(GPMC_SRC_DIR ${PROJECT_SOURCE_DIR})

install(TARGETS gPMC DESTINATION ${CMAKE_INSTALL_BINDIR})

set(GPMC_DLLS "${GPMC_LIB_DIR}/goPMC.dll" "${GPMC_LIB_DIR}/dcmtk.dll"
              "${GPMC_LIB_DIR}/libDicomRT.dll")
install(FILES ${GPMC_DLLS} DESTINATION ${CMAKE_INSTALL_BINDIR})

file(GLOB LUT_FILES "${PROJECT_SOURCE_DIR}/input/*.bin")
install(FILES ${LUT_FILES} DESTINATION lut)
