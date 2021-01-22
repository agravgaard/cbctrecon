FROM andreasga/cbctrecon:intel-opencl-runtime-cpu
MAINTAINER Andreas Gravgaard Andersen <andreasga22@gmail.com>

USER root

# Useful for debugging docker: bash sudo vim \
RUN apt update \
    && apt remove -y gcc g++ \
    && apt autoremove -y \
    && apt-get install -y wget build-essential bash \
        pkg-config cmake ca-certificates gnupg \
        ninja-build qt5-default \
        libfftw3-dev libtinfo-dev libeigen3-dev \
        libdcmtk-dev libgdcm-dev valgrind gcc-10 g++-10

RUN wget https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS-2023.PUB \
    && apt-key add GPG-PUB-KEY-INTEL-SW-PRODUCTS-2023.PUB \
    && echo "deb https://apt.repos.intel.com/oneapi all main" > /etc/apt/sources.list.d/oneAPI.list \
    && apt-get -qqq update \
    && apt-get install -y --no-install-recommends \
        intel-oneapi-tbb-devel \
        intel-oneapi-mkl-devel \
        intel-oneapi-libdpstd-devel \
        intel-oneapi-compiler-dpcpp-cpp \
        intel-oneapi-compiler-dpcpp-cpp-and-cpp-classic \
    && apt clean -y \
    && rm -rf /var/lib/apt/lists/*

# Below may be necessary for the virtual FPGA OpenCL driver intel distributes with the above packages
# install Intel GPU drivers
RUN echo 'deb [trusted=yes arch=amd64] https://repositories.intel.com/graphics/ubuntu bionic main' \
> /etc/apt/sources.list.d/intel-graphics.list
ARG url=https://repositories.intel.com/graphics/intel-graphics.key
ADD $url /
RUN file=$(basename "$url") && \
    apt-key add "$file" && \
        rm "$file"

        RUN apt-get update -y && \
        apt-get install -y --no-install-recommends -o=Dpkg::Use-Pty=0 \
        intel-opencl \
        intel-level-zero-gpu \
        level-zero \
        level-zero-devel


RUN echo "export CC=icc" >> /etc/profile \
    && echo "export CXX=icpc" >> /etc/profile

USER user

RUN git config --global user.email "andreasga22@gmail.com" \
    && git config --global user.name "Andreas Gravgaard Andersen"

RUN echo "export CC=icc" >> ~/.bashrc \
    && echo "export CXX=icpc" >> ~/.bashrc

# Build ITK and RTK by themselves without CUDA support, although we may be able to hipify it, I doubt it's worth the effort
# "master" because 5.1rc01 are missing some set/get functions required for cuda part of RTK
# Also, we'll have to use my patched fork of RTK to forward the correct std=c++ flag to CUDA
RUN ["/bin/bash", "-c", "cd /home/user/ \
    && git clone --depth 1 --branch 'master' https://github.com/InsightSoftwareConsortium/ITK.git ITK \
    && mkdir -p /home/user/ITK-build \
    && cd ITK-build \
    && source /opt/intel/oneapi/setvars.sh \
    && export CC=icc  \
    && export CXX=icpc \
    && export CXXFLAGS='-I/opt/intel/oneapi/tbb/latest/include/ -I/opt/intel/oneapi/tbb/latest/include/tbb' \
    && export LDFLAGS='-L/opt/intel/oneapi/tbb/latest/lib/intel64/gcc4.8/ -ltbb' \
    && cmake -G Ninja ../ITK \
      -D CMAKE_CXX_STANDARD=20 \
      -D CMAKE_BUILD_TYPE=Release \
      -D BUILD_TESTING=OFF -D BUILD_EXAMPLES=OFF \
      -D Module_ITKReview=ON \
      -D ITK_BUILD_DEFAULT_MODULES=OFF \
      -D Module_ITKDCMTK=ON \
      -D ITK_USE_SYSTEM_DCMTK=ON \
      -D ITK_USE_SYSTEM_GDCM=ON \
      -D Module_ITKDeprecated=ON \
      -D ITK_USE_SYSTEM_ZLIB=ON \
      -D Module_RTK=ON \
      -D RTK_USE_CUDA=OFF \
      -D RTK_BUILD_APPLICATIONS=OFF \
      -D ITK_MINIMUM_COMPLIANCE_LEVEL=3 \
    && ninja \
    && sudo ninja install \
    && cd ../ \
    && rm -Rf ITK*"]

WORKDIR /home/user/

