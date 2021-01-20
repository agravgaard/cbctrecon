# Copyright (c) 2018 Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not use this
# file except in compliance with the License. You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under
# the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied. See the License for the specific language governing
# permissions and limitations under the License.

# This and the two included files are taken from the OpenVINO project Modified by A.
# Gravgaard 10/10/2018

cmake_minimum_required(VERSION 2.8)

include(CPUID)
include(OptimizationFlags)

# It is easier to use find_package(OpenMP), but the below is kept for future reference
macro(enable_omp)
  if(UNIX) # Linux
    add_definitions(-fopenmp)
    find_library(intel_omp_lib iomp5
                 PATHS ${InferenceEngine_INCLUDE_DIRS}/../external/mkltiny_lnx/lib)
  elseif(WIN32) # Windows
    if(${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC)
      set(OPENMP_FLAGS "/Qopenmp /openmp")
      set(CMAKE_SHARED_LINKER_FLAGS " ${CMAKE_SHARED_LINKER_FLAGS} /nodefaultlib:vcomp")
    elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL Intel)
      set(OPENMP_FLAGS "/Qopenmp /openmp")
    else()
      message("Unknown compiler ID. OpenMP support is disabled.")
    endif()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OPENMP_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OPENMP_FLAGS}")
    find_library(
      intel_omp_lib libiomp5md
      PATHS "${InferenceEngine_INCLUDE_DIRS}/../lib/intel64/${CMAKE_BUILD_TYPE}")
  endif()
endmacro(enable_omp)
