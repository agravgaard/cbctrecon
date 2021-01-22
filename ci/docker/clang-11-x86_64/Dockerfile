FROM andreasga/cbctrecon:intel-opencl-runtime-cpu
MAINTAINER Andreas Gravgaard Andersen <andreasga22@gmail.com>

USER root

# Useful for debugging docker: bash sudo vim \
RUN apt update \
    && apt install -y \
        ninja-build qt5-default \
        libfftw3-dev libtinfo-dev libeigen3-dev \
        libdcmtk-dev libgdcm-dev valgrind \
        g++-10 gnupg software-properties-common \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

RUN cd /usr/share/cmake/Modules/ \
    && cmake -D INSTALL_DIR=/usr/lib/cmake/TBB \
        -D SYSTEM_NAME=Linux \
        -D LIB_PATH=/usr/lib/x86_64-linux-gnu \
        -D INC_PATH=/usr/include \
        -P tbb_config_installer.cmake

RUN wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add - \
    && apt-add-repository "deb http://apt.llvm.org/groovy/ llvm-toolchain-groovy-11 main" \
    && apt-add-repository "deb-src http://apt.llvm.org/groovy/ llvm-toolchain-groovy-11 main" \
    && apt update \
    && apt -y install \
        clang clangd \
        libc++-dev libc++1 libc++abi-dev libc++abi1 \
        libclang-dev libclang1 \
        libomp-dev libomp5 \
        lld llvm-dev llvm-runtime llvm \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# The flag -DLIBCXX_ENABLE_PARALLEL_ALGORITHMS=ON  creates problems in cmake config.
        # -D LLVM_ENABLE_PROJECTS="clang;lld;libcxx;libcxxabi;openmp;libclc;pstl"
        # -D LIBCXX_ENABLE_PARALLEL_ALGORITHMS=ON \
#RUN git clone https://github.com/RadeonOpenCompute/llvm-project.git -b amd-stg-open --depth=1 --shallow-submodules \
#    && cd llvm-project \
#    && mkdir build \
#    && cd build \
#    && CC=gcc-10 CXX=g++-10 cmake -GNinja ../llvm \
#        -D LLVM_ENABLE_PROJECTS="all" \
#        -D CMAKE_BUILD_TYPE=Release \
#        -D PSTL_PARALLEL_BACKEND="tbb" \
#        -D CMAKE_CXX_STANDARD=20 \
#    && ninja \
#    && ninja install \
#    && cd ../.. \
#    && rm -Rf ./llvm-project \
#    && ldconfig \
#    && /usr/local/bin/clang --version

RUN echo "export CC=/usr/bin/clang" >> /etc/profile \
    && echo "export CXX=/usr/bin/clang++" >> /etc/profile

USER user

RUN git config --global user.email "andreasga22@gmail.com" \
    && git config --global user.name "Andreas Gravgaard Andersen"

RUN echo "export CC=/usr/bin/clang" >> ~/.bashrc \
    && echo "export CXX=/usr/bin/clang++" >> ~/.bashrc

WORKDIR /home/user/
