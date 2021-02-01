macro(add_cbctrecon_app)
  cmake_parse_arguments(
    ARGS
    ""
    "TARGET"
    ""
    ${ARGN})

  set(SRC_FILES ${ARGS_TARGET}.cpp)

  if(CBCTRECON_BUILD_SHARED_LIBS)
    add_executable(${ARGS_TARGET} $<TARGET_OBJECTS:cbctrecon_test> ${SRC_FILES})
    target_link_libraries(${ARGS_TARGET} CbctReconLib)
  else()
    add_executable(${ARGS_TARGET} ${SRC_FILES})
    target_link_libraries(${ARGS_TARGET} cbctrecon_test)
  endif()

  # Sets instruction set optimization flags SSE/AVX/AVX2/AVX512:
  set_target_cpu_flags(${ARGS_TARGET})

  # Only sets report flags if Intel Compiler:
  set_target_vectorizer_report_flags(${ARGS_TARGET})

  target_include_directories(
    ${ARGS_TARGET}
    PUBLIC ${CBCTRECON_INCLUDE_DIRS}
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../Testing/"
    PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

  install(TARGETS ${ARGS_TARGET} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endmacro()

add_cbctrecon_app(TARGET opencl_tester)
