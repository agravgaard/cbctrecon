# The main application ###
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include(CheckCXXCompilerFlag)
set(CXX_PEDANTIC_FLAGS "")
function(enable_cxx_compiler_flag_if_supported flag)
  string(FIND "${CMAKE_CXX_FLAGS}" "${flag}" flag_already_set)
  if(flag_already_set EQUAL -1)
    check_cxx_compiler_flag("${flag}" flag_supported)
    if(flag_supported)
      list(APPEND CXX_PEDANTIC_FLAGS ${flag})
      set(CXX_PEDANTIC_FLAGS
          ${CXX_PEDANTIC_FLAGS}
          PARENT_SCOPE)
    endif()
    unset(flag_supported CACHE)
  endif()
endfunction()

if(ENABLE_PEDANTIC)
  enable_cxx_compiler_flag_if_supported("-Wall")
  enable_cxx_compiler_flag_if_supported("-Wextra")
  enable_cxx_compiler_flag_if_supported("-pedantic")
  enable_cxx_compiler_flag_if_supported("-Wno-extra-semi")
  enable_cxx_compiler_flag_if_supported("-Werror")
  enable_cxx_compiler_flag_if_supported("/W4")
  enable_cxx_compiler_flag_if_supported("/WX")
endif()

add_definitions(-DCMAKE_EXPORT_COMPILE_COMMANDS=ON)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cbctrecon_config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/cbctrecon_config.h)

if(USE_OPENCL)
  if(RTK_USE_OPENCL)
    set(RTK_OPENCL_SRC
        rtkExtension/rtkOpenCLUtilities.cxx
        rtkExtension/rtkOpenCLFDKBackProjectionImageFilter.cxx
        rtkExtension/rtkOpenCLFDKConeBeamReconstructionFilter.cxx
        OpenCL/ForwardProjectionImageFilter.cpp)
  endif()

  set(OPENCLEXT_SOURCES
      ${RTK_OPENCL_SRC}
      OpenCL/ImageFilters.cxx
      OpenCL/device_picker.cpp
      OpenCL/err_code.cpp)

  set(OPENCLEXT_HEADERS
      rtkExtension/rtkOpenCLFDKConeBeamReconstructionFilter.h
      rtkExtension/rtkOpenCLFDKBackProjectionImageFilter.h
      rtkExtension/rtkOpenCLFFTRampImageFilter.h
      rtkExtension/rtkOpenCLFFTConvolutionImageFilter.h
      rtkExtension/rtkOpenCLFFTConvolutionImageFilter.hxx
      rtkExtension/itkOpenCLImageToImageFilter.h
      rtkExtension/itkOpenCLImageToImageFilter.hxx
      rtkExtension/rtkOpenCLUtilities.h
      rtkExtension/rtkOpenCLForwardProjectionImageFilter.h
      rtkExtension/rtkOpenCLForwardProjectionImageFilter.hxx
      OpenCL/ForwardProjectionImageFilter.hpp
      OpenCL/ImageFilters.hpp
      OpenCL/device_picker.hpp
      OpenCL/err_code.hpp)
elseif(USE_SYCL)
  add_subdirectory(SYCLext)
else()
  message(FATAL_ERROR "Either SYCL or OPENCL should be enabled!")
endif()

# ############# Qt start wrapping ##############
set(CbctReconLib_SOURCES
    cbctrecon.cpp
    cbctregistration.cpp
    YK16GrayImage.cxx
    AG17RGBAImage.cxx
    StructureSet.cxx
    PlmWrapper.cxx
    WEPL.cxx
    cbctrecon_io.cxx
    cbctrecon_compute.cxx
    cbctrecon_fdk.cxx
    cbctrecon_fdk.hxx
    free_functions.cpp
    ${OPENCLEXT_SOURCES})

set(CbctReconLib_HEADERS
    cbctrecon_types.h
    cbctrecon.h
    cbctregistration.h
    YK16GrayImage.h
    AG17RGBAImage.h
    PlmWrapper.h
    WEPL.h
    cbctrecon_io.h
    cbctrecon_compute.h
    StructureSet.h
    free_functions.h
    ${OPENCLEXT_HEADERS}
    ${CMAKE_CURRENT_BINARY_DIR}/cbctrecon_config.h)

set(CBCTRECON_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
                           ${CMAKE_CURRENT_SOURCE_DIR}/rtkExtension)

if(CBCTRECON_BUILD_SHARED_LIBS)
  set(SHARED_FLAG SHARED)
endif()

add_library(CbctReconLib ${SHARED_FLAG} ${CbctReconLib_SOURCES} ${CbctReconLib_HEADERS})

if(USE_SYCL)
  target_link_libraries(CbctReconLib PUBLIC sycl_ext)
endif()

# Sets instruction set optimization flags SSE/AVX/AVX2/AVX512:
set_target_cpu_flags(CbctReconLib)

# Only sets report flags if Intel Compiler:
set_target_vectorizer_report_flags(CbctReconLib)

target_compile_options(CbctReconLib PRIVATE ${CXX_PEDANTIC_FLAGS})
message(STATUS "Pedantic flags: ${CXX_PEDANTIC_FLAGS}")

if(CBCTRECON_SANITIZE)
  sanitize_build(CbctReconLib)
endif()

# target_code_coverage(CbctReconLib AUTO ALL)

set(Plastimatch_SRC ${CMAKE_BINARY_DIR}/Plastimatch-src/)
set(Plastimatch_BUILD_DIR ${CMAKE_BINARY_DIR}/Plastimatch-build/)

if(NOT USE_SYSTEM_Plastimatch)
  set(PLASTIMATCH_INCLUDE_DIRS
      ${PLASTIMATCH_INCLUDE_DIRS}
      "${Plastimatch_SRC}/src/"
      "${Plastimatch_SRC}/src/plastimatch/"
      "${Plastimatch_SRC}/src/plastimatch/util"
      "${Plastimatch_SRC}/src/plastimatch/dose"
      "${Plastimatch_SRC}/src/plastimatch/sys"
      "${Plastimatch_SRC}/src/plastimatch/base"
      "${Plastimatch_SRC}/src/plastimatch/register"
      "${Plastimatch_SRC}/src/plastimatch/reconstruct"
      "${Plastimatch_SRC}/src/plastimatch/opencl"
      "${Plastimatch_SRC}/src/plastimatch/segment"
      "${Plastimatch_BUILD_DIR}/")
endif()

# this is for "C++ General-> Add below to Additional Include Directories"
set(CBCTRECON_DEP_INCLUDES
    ${CBCTRECON_INCLUDE_DIRS}
    ${ITK_INCLUDE_DIRS}
    ${RTK_INCLUDE_DIRS}
    ${PLASTIMATCH_INCLUDE_DIRS}
    ${DCMTK_INCLUDE_DIR}
    ${CLFFT_INCLUDE_DIRS})

target_include_directories(
  CbctReconLib SYSTEM
  PUBLIC ${CBCTRECON_DEP_INCLUDES}
  PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

# this is for "Linker->Additional dependencies"
target_link_libraries(
  CbctReconLib
  PUBLIC ${PLM_TARGETS}
  PUBLIC ${CBCTRECON_DEP_LIBS})

if(USE_SYSTEM_Plastimatch)
  find_package(Plastimatch REQUIRED)

  message(STATUS "${PLASTIMATCH_LIBRARIES}")
  message(STATUS "${Plastimatch_LIBRARIES}")

  target_link_libraries(CbctReconLib PUBLIC ${Plastimatch_LIBRARIES})

  get_target_property(plmbase_includes plmbase INTERFACE_INCLUDE_DIRECTORIES)
  foreach(i_dir ${plmbase_includes})
    target_include_directories(CbctReconLib SYSTEM PUBLIC "${i_dir}/../")
  endforeach()
endif()

if(USE_TINYREFL)
  include(${TINYREFL_SOURCE_DIR}/tool/driver.cmake)
  tinyrefl_tool(
    TARGET
    CbctReconLib
    HEADERS
    cbctrecon.h
    COMPILE_OPTIONS
    -fPIC)
endif()

install(
  TARGETS CbctReconLib
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  PUBLIC_HEADER DESTINATION include)

set(RUNTIME_DIR ${CMAKE_INSTALL_PREFIX}/bin)

add_custom_command(
  TARGET CbctReconLib
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/fdk_opencl.cl" "${RUNTIME_DIR}")
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/fdk_opencl.cl DESTINATION bin)

add_custom_command(
  TARGET CbctReconLib
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/rtkExtension/forward_proj.cl" "${RUNTIME_DIR}")
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/rtkExtension/forward_proj.cl DESTINATION bin)

add_custom_command(
  TARGET CbctReconLib
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_CURRENT_SOURCE_DIR}/OpenCL/filters.cl" "${RUNTIME_DIR}")
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/OpenCL/filters.cl DESTINATION bin)

# -----------------------------------------------------------------------------
# Additional install files
# -----------------------------------------------------------------------------
if(WIN32)
  if(OPENMP_FOUND)
    set(CMAKE_INSTALL_OPENMP_LIBRARIES ON)
  endif(OPENMP_FOUND)
  include(InstallRequiredSystemLibraries)

  if(CUDA_FOUND)
    set(DLL_PATTERNS "cufft64*.dll" "cublas64*.dll" "cudart64*.dll")
    foreach(p ${DLL_PATTERNS})
      file(GLOB DLLFILES "${CUDA_TOOLKIT_ROOT_DIR}/bin/${p}")
      if(DLLFILES)
        install(FILES ${DLLFILES} DESTINATION bin)
      endif()
    endforeach()
  endif()

  if(ITK_FOUND)
    # ITK*.dll might not include RTK_AS_ITK_EXTERNAL libs
    set(DLL_PATTERNS "*.dll")
    foreach(p ${DLL_PATTERNS})
      file(GLOB DLLFILES "${ITK_DIR}/bin/release/${p}")
      if(DLLFILES)
        install(FILES ${DLLFILES} DESTINATION bin)
      endif()
    endforeach()
  endif()

  if(RTK_FOUND)
    install(FILES ${RTK_DIR}/bin/release/RTK.dll DESTINATION bin)
    install(FILES ${RTK_DIR}/bin/release/rtkcuda.dll DESTINATION bin)
    install(FILES ${RTK_DIR}/bin/release/ITKCudaCommon.dll DESTINATION bin)
    install(FILES ${RTK_DIR}/bin/release/lpsolve55.dll DESTINATION bin) # is 55 a version
                                                                        # number?
  endif()

  if(Plastimatch_FOUND)
    set(DLL_PATTERNS "plm*.dll" "")
    foreach(p ${DLL_PATTERNS})
      file(GLOB DLLFILES "${Plastimatch_BUILD_DIR}/release/${p}")
      if(DLLFILES)
        install(FILES ${DLLFILES} DESTINATION bin)
      endif()
    endforeach()
  endif()

  if(USE_CLFFT)
    install(FILES ${CLFFT_ROOT_DIR}/bin/clFFT.dll DESTINATION bin)
  endif()
  if(TBB_FOUND)
    get_target_property(TBB_DLLS TBB::tbb IMPORTED_LOCATION_RELEASE)
    install(FILES ${TBB_DLLS} DESTINATION bin)
    get_target_property(TBB_DLLS TBB::tbbmalloc IMPORTED_LOCATION_RELEASE)
    install(FILES ${TBB_DLLS} DESTINATION bin)
    get_target_property(TBB_DLLS TBB::tbbmalloc_proxy IMPORTED_LOCATION_RELEASE)
    install(FILES ${TBB_DLLS} DESTINATION bin)
  endif()

  # Add FFT dlls to Install
  if(FFTW_FOUND)
    if(EXISTS "${FFTW_DIR}/libfftw3-3.dll")
      install(FILES "${FFTW_DIR}/libfftw3-3.dll" DESTINATION bin)
    endif()
    if(EXISTS "${FFTW_DIR}/libfftw3f-3.dll")
      install(FILES "${FFTW_DIR}/libfftw3f-3.dll" DESTINATION bin)
    endif()
  endif()
endif()

# -----------------------------------------------------------------------------
# Option to generate .clang_complete for hip Vim users using clang_complete
# -----------------------------------------------------------------------------
if(CBCTRECON_CLANG_COMPLETE)
  get_property(
    inc_dirs
    DIRECTORY
    PROPERTY INCLUDE_DIRECTORIES)
  set(file_clang_complete "${CMAKE_CURRENT_SOURCE_DIR}/.clang_complete")
  foreach(arg ${inc_dirs})
    set(inc_args "${inc_args} -I${arg}\n")
  endforeach()
  file(WRITE "${file_clang_complete}" "${inc_args}")
  message(STATUS "Generated ${file_clang_complete}")
endif()
