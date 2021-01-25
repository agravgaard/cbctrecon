FROM andreasga/cbctrecon:gcc-10-x86_64
MAINTAINER Andreas Gravgaard Andersen <andreasga22@gmail.com>

USER root

RUN apt-get update \
    && apt-get install -y software-properties-common \
    && add-apt-repository ppa:ubuntu-toolchain-r/test \
    && apt-get update \
    && apt-get install -y gcc-10 g++-10 lcov \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

USER user

# ITK
RUN export CC=/usr/bin/gcc-10 \
    && export CXX=/usr/bin/g++-10 \
    && export CXXFLAGS="-I/usr/include -I/usr/include/tbb -fopenmp-simd" \
    && export LDFLAGS="-L/usr/lib/x86_64-linux-gnu -ltbb" \
    && cd /home/user/ \
    && git clone --depth 1 --branch "master" https://github.com/InsightSoftwareConsortium/ITK.git ITK \
    && mkdir -p /home/user/ITK-build \
    && cd ITK-build \
    && cmake -GNinja ../ITK -DCMAKE_CXX_STANDARD=20 -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTING=OFF -DBUILD_EXAMPLES=OFF \
      -DModule_ITKReview=ON -DITK_USE_GPU=ON -DITK_BUILD_DEFAULT_MODULES=OFF \
      -DITK_USE_SYSTEM_FFTW=ON -DITK_USE_FFTWF=ON -DITK_USE_FFTWD=ON \
      -DModule_ITKDCMTK=ON -DITK_USE_SYSTEM_DCMTK=ON -DModule_ITKDeprecated=ON \
      -DITK_USE_SYSTEM_ZLIB=ON -DModule_RTK=ON \
      -DRTK_USE_CUDA=OFF -DRTK_TIME_EACH_FILTER=ON -DRTK_BUILD_APPLICATIONS=OFF \
      -DModule_RTK_GIT_TAG="master" \
      -DITK_MINIMUM_COMPLIANCE_LEVEL=3 \
    && ninja \
    && sudo ninja install \
    && cd ../ \
    && rm -Rf ITK*

WORKDIR /home/user/
