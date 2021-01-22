FROM andreasga/cbctrecon:intel-opencl-runtime-cpu
MAINTAINER Andreas Gravgaard Andersen <andreasga22@gmail.com>

USER root

RUN apt update \
    && apt install -y \
        ninja-build qt5-default \
        libfftw3-dev libtinfo-dev libeigen3-dev \
        libdcmtk-dev libgdcm-dev valgrind \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

RUN cd /usr/share/cmake/Modules/ \
    && cmake -D INSTALL_DIR=/usr/lib/cmake/TBB \
        -D SYSTEM_NAME=Linux \
        -D LIB_PATH=/usr/lib/x86_64-linux-gnu \
        -D INC_PATH=/usr/include \
        -P tbb_config_installer.cmake

# Register the ROCM package repository, and install rocm-dev package
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends curl gnupg \
    && curl -sL http://repo.radeon.com/rocm/rocm.gpg.key | apt-key add - \
    && sh -c 'echo deb [arch=amd64] http://repo.radeon.com/rocm/apt/debian/ xenial main > /etc/apt/sources.list.d/rocm.list' \
    && apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        libnuma-dev libelf1 build-essential \
        qt5-default lcov openssl libgdbm-dev \
        libgdbm-compat-dev liblzma-dev libffi-dev uuid-dev \
        rocm-dev rocm-utils rocm-opencl rocm-device-libs \
        hip-base hip-rocclr llvm-amdgpu \
        libboost-dev libboost-context-dev libboost-fiber-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

RUN usermod -a -G video user \
    && echo 'export PATH=$PATH:/opt/rocm/bin:/opt/rocm/rocprofiler/bin:/opt/rocm/opencl/bin' | sudo tee -a /etc/profile.d/rocm.sh

ENV HCC_AMDGPU_TARGET=gfx900

ENV PATH=$PATH:/opt/rocm/bin:/opt/rocm/llvm/bin:/opt/rocm/rocprofiler/bin:/opt/rocm/opencl/bin
ENV ROCM_PATH="/opt/rocm/"

# Verify installation:
RUN /opt/rocm/hip/bin/hipconfig --full

# hipSYCL
RUN git clone https://github.com/illuhad/hipSYCL --recurse-submodules \
  && cd hipSYCL \
  && mkdir build && cd build \
  && cmake ../ -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_DIR=/opt/rocm/llvm/lib/cmake/ \
    -DWITH_ROCM_BACKEND=ON \
    -DCLANG_INCLUDE_PATH=/opt/rocm/llvm/lib/clang/12.0.0/ \
    -DWITH_CPU_BACKEND=ON \
  && ninja \
  && ninja install \
  && cd ../.. \
  && rm -Rf hipSYCL

ENV HIPSYCL_GPU_ARCH=gfx900

COPY ./pstl.patch ./pstl.patch

# SyclParallelSTL
RUN git clone https://github.com/KhronosGroup/SyclParallelSTL.git \
  && cd SyclParallelSTL \
  && git apply ../pstl.patch \
  && mkdir build && cd build \
  && cmake ../ -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=20 \
    -DCMAKE_CXX_FLAGS="-fopenmp" \
    -DUSE_COMPUTECPP=OFF \
    -DHIPSYCL_PLATFORM=rocm \
    -DHIPSYCL_GPU_ARCH=gfx900 \
    -DHIPSYCL_ROCM_PATH=/opt/rocm/ \
    -DHIPSYCL_SYCLCC_EXTRA_ARGS="--rocm-device-lib-path=/opt/rocm/amdgcn/bitcode/" \
    -Dhipsycl_DIR=/usr/local/lib/cmake/ \
  && ninja \
  && ninja install \
  && cd ../.. \
  && rm -Rf SyclParallelSTL

WORKDIR /home/user/

USER user

# Plastimatch won't download unless a git user is given:
RUN git config --global user.email "andreasga22@gmail.com" \
    && git config --global user.name "Andreas Gravgaard Andersen"

# Build ITK and RTK by themselves without CUDA support, although we may be able to hipify it, I doubt it's worth the effort
# "master" because 5.1rc01 are missing some set/get functions required for cuda part of RTK
# Also, we'll have to use my patched fork of RTK to forward the correct std=c++ flag to CUDA
RUN cd /home/user/ \
    && git clone --depth 1 --branch "master" https://github.com/InsightSoftwareConsortium/ITK.git ITK \
    # && sed -i 's/SimonRit/agravgaard/g' ./ITK/Modules/Remote/RTK.remote.cmake \
    && sed -i 's/GIT_TAG .*/GIT_TAG master/g' ./ITK/Modules/Remote/RTK.remote.cmake \
    && sed -i 's/<<</<< </g' ./ITK/Modules/Core/Common/include/itkArray.h \
    && sed -i 's/<<</<< </g' ./ITK/Modules/Core/Common/src/itkArrayOutputSpecialization.cxx \
    && sed -i 's/defined(H5_ZLIB_HEADER)/true/g' ./ITK/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Zdeflate.c \
    && sed -i 's/H5_ZLIB_HEADER/\"itk_zlib.h\"/g' ./ITK/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Zdeflate.c \
    && mkdir -p /home/user/ITK-build \
    && cd ITK-build \
    && export CC=/opt/rocm/hip/bin/hipcc  \
    && export CXX=/opt/rocm/hip/bin/hipcc \
    && cmake -G Ninja ../ITK \
      -D CMAKE_CXX_STANDARD=20 \
      -D CMAKE_BUILD_TYPE=Release \
      -D BUILD_TESTING=OFF -D BUILD_EXAMPLES=OFF \
      -D Module_ITKReview=ON \
      -D ITK_BUILD_DEFAULT_MODULES=OFF \
      -D ITK_USE_SYSTEM_FFTW=ON -D ITK_USE_FFTWF=ON -D ITK_USE_FFTWD=ON \
      -D Module_ITKDCMTK=ON \
      -D ITK_USE_SYSTEM_DCMTK=ON \
      -D ITK_USE_SYSTEM_GDCM=ON \
      -D Module_ITKDeprecated=ON \
      -D ITK_USE_SYSTEM_ZLIB=ON \
      -D Module_RTK=ON \
      -D Module_ITKTBB=ON \
      -D RTK_USE_CUDA=OFF \
      -D RTK_BUILD_APPLICATIONS=OFF \
      -D ITK_MINIMUM_COMPLIANCE_LEVEL=3 \
    && ninja \
    && sudo ninja install \
    && cd ../ \
    && rm -Rf ITK*

WORKDIR /home/user/
