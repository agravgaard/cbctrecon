set(CMAKE_FOLDER Testing)

function(unpack_ext_data)
  cmake_parse_arguments(
    ARGS
    ""
    "TARGET;TAR_FILE;TAR_OUTPUT_PATH;TAR_SUBOUTPUT"
    ""
    ${ARGN})

  add_custom_command(
    TARGET ${ARGS_TARGET}
    POST_BUILD # Because after build the tars must have been downloaded
    COMMAND ${CMAKE_COMMAND} -E tar xzf ${ARGS_TAR_FILE} DEPENDS ${ARGS_TAR_FILE}
    WORKING_DIRECTORY ${ARGS_TAR_OUTPUT_PATH}
    COMMENT "Unpacking ${ARGS_TAR_FILE}"
    VERBATIM)
endfunction()

set(test_target cbctrecon_test)

set(${test_target}_SRC
    cbctrecon_test.cpp
    cbctrecon_test.hpp
    cbctregistration_test.cpp
    cbctregistration_test.hpp
    MyQComboBox.hxx)

if(CBCTRECON_BUILD_SHARED_LIBS)
  set(LIB_FLAG OBJECT)
endif()

add_library(${test_target} ${LIB_FLAG} ${${test_target}_SRC})

# Sets instruction set optimization flags SSE/AVX/AVX2/AVX512:
set_target_cpu_flags(${test_target})

# Only sets report flags if Intel Compiler:
set_target_vectorizer_report_flags(${test_target})

if(CBCTRECON_SANITIZE)
  sanitize_build(cbctrecon_test)
endif()

target_link_libraries(${test_target} PUBLIC CbctReconLib)

target_include_directories(
  ${test_target}
  PUBLIC ${CBCTRECON_INCLUDE_DIRS}
  PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

# Test: Reading Dicom files

set(test_target test_dcm_reader)

set(${test_target}_DATA DATA{Input/Alderson/CT.tar.gz})

add_cbctrecon_test(
  TARGET
  ${test_target}
  DATA_ARGS
  ${${test_target}_DATA})

set(TAR_OUTPUT_PATH ${CMAKE_BINARY_DIR}/ExternalData/Testing/Input/Alderson)
set(TAR_FILE ${TAR_OUTPUT_PATH}/CT.tar.gz)
set(TAR_SUBOUTPUT CT)

unpack_ext_data(
  TARGET
  ${test_target}
  TAR_FILE
  ${TAR_FILE}
  TAR_OUTPUT_PATH
  ${TAR_OUTPUT_PATH}
  TAR_SUBOUTPUT
  ${TAR_SUBOUTPUT})

# Test: CB projection reader

set(test_target test_cb_reader)

set(${test_target}_DATA DATA{Input/Alderson/CBCT.tar.gz}
                        DATA{Baseline/Alderson/CBproj.mha})

add_cbctrecon_test(
  TARGET
  ${test_target}
  DATA_ARGS
  ${${test_target}_DATA})

set(TAR_FILE ${TAR_OUTPUT_PATH}/CBCT.tar.gz)
set(TAR_SUBOUTPUT CBCT)

unpack_ext_data(
  TARGET
  ${test_target}
  TAR_FILE
  ${TAR_FILE}
  TAR_OUTPUT_PATH
  ${TAR_OUTPUT_PATH}
  TAR_SUBOUTPUT
  ${TAR_SUBOUTPUT})

# Test: OpenCL filters

set(test_target test_opencl_filter)
add_cbctrecon_test(
  TARGET
  ${test_target}
  DATA_ARGS
  add_const_filter)

add_test(
  NAME ${test_target}_add_2d
  COMMAND $<TARGET_FILE:${test_target}> add_const_2d_filter
  WORKING_DIRECTORY ${CMAKE_INSTALL_BINDIR})

add_test(
  NAME ${test_target}_add_mul
  COMMAND $<TARGET_FILE:${test_target}> add_mul_const_filter
  WORKING_DIRECTORY ${CMAKE_INSTALL_BINDIR})

add_test(
  NAME ${test_target}_min_max
  COMMAND $<TARGET_FILE:${test_target}> min_max_filter
  WORKING_DIRECTORY ${CMAKE_INSTALL_BINDIR})

add_test(
  NAME ${test_target}_min_max_2d
  COMMAND $<TARGET_FILE:${test_target}> min_max_2d_filter
  WORKING_DIRECTORY ${CMAKE_INSTALL_BINDIR})

add_test(
  NAME ${test_target}_divide_3d_by_3d_loginv
  COMMAND $<TARGET_FILE:${test_target}> divide_3Dby3D_loginv_filter
  WORKING_DIRECTORY ${CMAKE_INSTALL_BINDIR})

# add_test( NAME ${test_target}_ItoLogI_subtract_median COMMAND
# $<TARGET_FILE:${test_target}> ItoLogI_subtract_median_filter WORKING_DIRECTORY
# ${CMAKE_INSTALL_BINDIR} )

add_test(
  NAME ${test_target}_ItoLogI_subtract_median_small
  COMMAND $<TARGET_FILE:${test_target}> ItoLogI_subtract_median_filter_small
  WORKING_DIRECTORY ${CMAKE_INSTALL_BINDIR})

add_test(
  NAME ${test_target}_ItoLogI_subtract_median_gaussian
  COMMAND $<TARGET_FILE:${test_target}> ItoLogI_subtract_median_gaussian_filter
  WORKING_DIRECTORY ${CMAKE_INSTALL_BINDIR})

ExternalData_add_test(
  CbctData
  NAME
  ${test_target}_crop_by_struct
  COMMAND
  $<TARGET_FILE:${test_target}>
  crop_by_struct_filter
  DATA{Input/Alderson/CT.tar.gz}
  WORKING_DIRECTORY
  ${CMAKE_INSTALL_BINDIR})

# Test: Forward projection, for OpenCL if available

set(test_target test_forwardprojection)

add_cbctrecon_test(
  TARGET
  ${test_target}
  DATA_ARGS
  "")

# Test: End 2 End

set(test_target test_end_to_end)

set(${test_target}_DATA DATA{Input/Alderson/CT.tar.gz} DATA{Input/Alderson/CBCT.tar.gz})

add_cbctrecon_test(
  TARGET
  ${test_target}
  DATA_ARGS
  ${${test_target}_DATA})
