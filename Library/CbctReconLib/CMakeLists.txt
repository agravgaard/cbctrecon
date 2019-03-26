### The main application ###
set(CMAKE_CXX_STARDARD 14) # only for this sub directory
# dlib headers are not ready for 17

set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_definitions(-DCMAKE_EXPORT_COMPILE_COMMANDS=ON)

configure_file (
  ${CMAKE_CURRENT_SOURCE_DIR}/cbctrecon_config.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/cbctrecon_config.h
)

############## Qt start wrapping ##############
SET(CbctReconLib_SOURCES
  cbctrecon.cpp
  cbctregistration.cpp
  YK16GrayImage.cxx
  AG17RGBAImage.cxx
  StructureSet.cxx
  PlmWrapper.cxx
  WEPL.cxx
  cbctrecon_io.cxx
  cbctrecon_compute.cxx
  # cbctrecon_fdk.cxx Only contains PLM reconstruction
  cbctrecon_fdk.hxx
  rtkExtension/rtkOpenCLUtilities.cxx
  rtkExtension/rtkOpenCLFDKBackProjectionImageFilter.cxx
  rtkExtension/rtkOpenCLFDKConeBeamReconstructionFilter.cxx
  OpenCLFFTFilter.cxx
)

SET(CbctReconLib_HEADERS
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
  rtkExtension/rtkOpenCLFDKConeBeamReconstructionFilter.h
  rtkExtension/rtkOpenCLFDKBackProjectionImageFilter.h
  rtkExtension/rtkOpenCLFFTRampImageFilter.h
  rtkExtension/rtkOpenCLFFTConvolutionImageFilter.h
  rtkExtension/rtkOpenCLFFTConvolutionImageFilter.hxx
  rtkExtension/itkOpenCLImageToImageFilter.h
  rtkExtension/itkOpenCLImageToImageFilter.hxx
  rtkExtension/rtkOpenCLUtilities.h
  OpenCLFFTFilter.h
  ${CMAKE_CURRENT_BINARY_DIR}/cbctrecon_config.h
)

set(CBCTRECON_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/rtkExtension
  )

if(CBCTRECON_BUILD_SHARED_LIBS)
  set(SHARED_FLAG SHARED)
endif()

add_library(CbctReconLib ${SHARED_FLAG}
  ${CbctReconLib_SOURCES}
  ${CbctReconLib_HEADERS}
  )

# Sets instruction set optimization flags SSE/AVX/AVX2/AVX512:
set_target_cpu_flags(CbctReconLib)

# Only sets report flags if Intel Compiler:
set_target_vectorizer_report_flags(CbctReconLib)

if(CBCTRECON_SANITIZE)
    sanitize_build(CbctReconLib)
endif()

set(Plastimatch_SRC ${CMAKE_BINARY_DIR}/Plastimatch-src/)
set(Plastimatch_BUILD_DIR ${CMAKE_BINARY_DIR}/Plastimatch-build/)

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

#this is for "C++ General-> Add below to Additional Include Directories"
set(CBCTRECON_DEP_INCLUDES
  ${CBCTRECON_INCLUDE_DIRS}
  ${ITK_INCLUDE_DIRS}
  ${RTK_INCLUDE_DIRS}
  ${PLASTIMATCH_INCLUDE_DIRS}
  ${DCMTK_INCLUDE_DIR}
  ${CLFFT_INCLUDE_DIRS}
)

target_include_directories(CbctReconLib
  PUBLIC ${CBCTRECON_DEP_INCLUDES}
  PUBLIC ${CMAKE_CURRENT_BINARY_DIR}
)

#this is for "Linker->Additional dependencies"
target_link_libraries(CbctReconLib
  PUBLIC ${PLM_TARGETS}
  PUBLIC ${CBCTRECON_DEP_LIBS}
)
if(USE_TINYREFL)
  include(${TINYREFL_SOURCE_DIR}/tool/driver.cmake)
  tinyrefl_tool(
    TARGET CbctReconLib
    HEADERS cbctrecon.h
    COMPILE_OPTIONS -fPIC
    )
endif()

install( TARGETS CbctReconLib
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  PUBLIC_HEADER DESTINATION include
  )

get_target_property(RUNTIME_DIR CbctReconLib RUNTIME_OUTPUT_DIRECTORY)
if (${RUNTIME_DIR} STREQUAL RUNTIME_DIR-NOTFOUND)
  set(RUNTIME_DIR ${CMAKE_BINARY_DIR}/bin)
endif()
add_custom_command(TARGET CbctReconLib
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/fdk_opencl.cl" "${RUNTIME_DIR}"
  )
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/fdk_opencl.cl DESTINATION bin)

##-----------------------------------------------------------------------------
##  Additional install files
##-----------------------------------------------------------------------------
if(WIN32)
  if(OPENMP_FOUND)
    set(CMAKE_INSTALL_OPENMP_LIBRARIES ON)
  endif(OPENMP_FOUND)
  include(InstallRequiredSystemLibraries)

  if(CUDA_FOUND)
    set (DLL_PATTERNS "cufft64*.dll" "cublas64*.dll" "cudart64*.dll")
    foreach (p ${DLL_PATTERNS})
    file (GLOB DLLFILES "${CUDA_TOOLKIT_ROOT_DIR}/bin/${p}")
    if(DLLFILES)
      install (FILES ${DLLFILES} DESTINATION bin)
    endif()
    endforeach ()
  endif()

  if(ITK_FOUND)
    # ITK*.dll might not include RTK_AS_ITK_EXTERNAL libs
    set (DLL_PATTERNS "*.dll")
    foreach (p ${DLL_PATTERNS})
      file (GLOB DLLFILES "${ITK_DIR}/bin/release/${p}")
      if(DLLFILES)
        install (FILES ${DLLFILES} DESTINATION bin)
      endif()
    endforeach ()
  endif()

  if(RTK_FOUND)
    install (FILES ${RTK_DIR}/bin/release/RTK.dll DESTINATION bin)
    install (FILES ${RTK_DIR}/bin/release/rtkcuda.dll DESTINATION bin)
    install (FILES ${RTK_DIR}/bin/release/ITKCudaCommon.dll DESTINATION bin)
    install (FILES ${RTK_DIR}/bin/release/lpsolve55.dll DESTINATION bin) #is 55 a version number?
  endif()

  if(Plastimatch_FOUND)
    set (DLL_PATTERNS "plm*.dll" "")
    foreach (p ${DLL_PATTERNS})
      file (GLOB DLLFILES "${Plastimatch_BUILD_DIR}/release/${p}")
      if(DLLFILES)
        install (FILES ${DLLFILES} DESTINATION bin)
      endif()
    endforeach ()
  endif()

  if(USE_CLFFT)
    install (FILES ${CLFFT_ROOT_DIR}/bin/clFFT.dll DESTINATION bin)
  endif()

  #Add FFT dlls to Install
  if(FFTW_FOUND)
    if(EXISTS "${FFTW_DIR}/libfftw3-3.dll")
      install (FILES "${FFTW_DIR}/libfftw3-3.dll" DESTINATION bin)
    endif()
    if(EXISTS "${FFTW_DIR}/libfftw3f-3.dll")
      install (FILES "${FFTW_DIR}/libfftw3f-3.dll" DESTINATION bin)
    endif()
  endif()
endif()

##-----------------------------------------------------------------------------
## Option to generate .clang_complete for hip Vim users using clang_complete
##-----------------------------------------------------------------------------
if (CBCTRECON_CLANG_COMPLETE)
  get_property(inc_dirs DIRECTORY PROPERTY INCLUDE_DIRECTORIES)
  set (file_clang_complete "${CMAKE_CURRENT_SOURCE_DIR}/.clang_complete")
  foreach (arg ${inc_dirs})
    set (inc_args "${inc_args} -I${arg}\n")
  endforeach ()
  file (WRITE "${file_clang_complete}" "${inc_args}")
  message (STATUS "Generated ${file_clang_complete}")
endif ()

message(STATUS "Generate vim YCM config file for CbctRecon at ${CMAKE_CURRENT_SOURCE_DIR} ...")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/.ycm_extra_conf.py.in
    ${CMAKE_CURRENT_SOURCE_DIR}/.ycm_extra_conf.py
)
