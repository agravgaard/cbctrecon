FROM andreasga/cbctrecon:intel-opencl-runtime-cpu
MAINTAINER Andreas Gravgaard Andersen <andreasga22@gmail.com>

USER root

# Useful for debugging docker: bash sudo vim \
RUN apt update \
    && apt install -y g++-10 gcc-10 ninja-build qt5-default \
        libfftw3-dev libtinfo-dev \
        libdcmtk-dev libeigen3-dev valgrind \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

RUN cd /usr/share/cmake/Modules/ \
    && cmake -D INSTALL_DIR=/usr/lib/cmake/TBB \
        -D SYSTEM_NAME=Linux \
        -D LIB_PATH=/usr/lib/x86_64-linux-gnu \
        -D INC_PATH=/usr/include \
        -P tbb_config_installer.cmake

USER user

RUN git config --global user.email "andreasga22@gmail.com" \
    && git config --global user.name "Andreas Gravgaard Andersen"

WORKDIR /home/user/
