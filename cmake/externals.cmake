include(FetchContent)

macro(external_dependency NAME URL COMMIT)
    if(${NAME} STREQUAL "Plastimatch")
      set(PATCH_CMD PATCH_COMMAND "") # git apply ${CMAKE_SOURCE_DIR}/External/patches/plm.patch)
    else()
      set(PATCH_CMD PATCH_COMMAND "")
    endif()
    if(${NAME} STREQUAL "TINYREFL")
      set(SHALLOW_CMD GIT_SHALLOW OFF)
    else()
      set(SHALLOW_CMD GIT_SHALLOW ON)
    endif()

    if(NOT TARGET ${NAME})
        message(STATUS "external dependency ${NAME} from ${URL} at ${COMMIT}")

	FetchContent_Declare(
          PROJ "${NAME}"
          GIT_REPOSITORY "${URL}"
          GIT_TAG "${COMMIT}"
          ${SHALLOW_CMD}
          ${PATCH_CMD}
        )
        
       endif()

    else()
        message(STATUS "external dependency ${NAME} already satisfied")
    endif()
endmacro()
