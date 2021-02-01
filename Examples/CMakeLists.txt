set(CMAKE_FOLDER Examples)

function(add_cbctrecon_example)
  cmake_parse_arguments(
    ARGS
    ""
    "TARGET"
    "GGO_FILES"
    ${ARGN})

  set(SRC_FILES ${ARGS_TARGET}.cpp ${ARGS_GGO_FILES})

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

endfunction()

add_cbctrecon_example(TARGET bigart)

add_cbctrecon_example(TARGET apply_affine)

# On The Fly CB reconstruction:
find_package(Gengetopt REQUIRED)

# Common revision info between applications
include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC GIT_SHA1)
if(GIT_SHA1 MATCHES ".*NOTFOUND")
  set(CBCTRECON_VERSION_HASH "")
else()
  set(CBCTRECON_VERSION_HASH ", git hash ${GIT_SHA1}")
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/CbctReconVersion.ggo.in
               ${CMAKE_CURRENT_BINARY_DIR}/CbctReconVersion.ggo)

wrap_ggo(
  onthefly_recon_GGO_C
  onthefly_recon.ggo
  cbctreconinputprojections_section.ggo
  CbctRecon3Doutputimage_section.ggo
  ${CMAKE_CURRENT_BINARY_DIR}/CbctReconVersion.ggo)

add_cbctrecon_example(
  TARGET
  onthefly_recon
  GGO_FILES
  ${onthefly_recon_GGO_C})
