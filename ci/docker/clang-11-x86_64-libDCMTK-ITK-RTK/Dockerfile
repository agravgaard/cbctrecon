FROM andreasga/cbctrecon:clang-11-x86_64
MAINTAINER Andreas Gravgaard Andersen <andreasga22@gmail.com>

USER user

# DCMTK
#RUN cd /home/user/ \
#    && git clone --depth 1 --branch "DCMTK-3.6.3" https://github.com/DCMTK/DCMTK.git DCMTK \
#    && mkdir -p /home/user/DCMTK-build \
#    && cd DCMTK-build \
#    && export CC=clang && export CXX=clang++ \
#    && cmake -GNinja ../DCMTK \
#      -DDCMTK_WITH_DOXYGEN=OFF -DDCMTK_ENABLE_CXX11=ON -DDCMTK_ENABLE_STL=ON \
#      -DDCMTK_OVERWRITE_WIN32_COMPILER_FLAGS=OFF -DDCMTK_WITH_XML=OFF -DDCMTK_WITH_PNG=OFF \
#      -DDCMTK_WITH_TIFF=OFF -DDCMTK_WITH_OPENSSL=OFF -DDCMTK_WITH_SNDFILE=OFF -DDCMTK_WITH_ICONV=OFF \
#    && cmake --build .

# ITK
RUN cd /home/user/ \
    && git clone --depth 1 --branch "master" https://github.com/InsightSoftwareConsortium/ITK.git ITK \
    && mkdir -p /home/user/ITK-build \
    && cd ITK-build \
    && export CC=clang && export CXX=clang++ \
    && export CXXFLAGS="-I/usr/include -I/usr/include/tbb -fopenmp-simd" \
    && export LDFLAGS="-L/usr/lib/x86_64-linux-gnu -ltbb" \
    && cmake -G Ninja ../ITK \
      -D CMAKE_CXX_STANDARD=20 \
      -D CMAKE_BUILD_TYPE=Release \
      -D BUILD_TESTING=OFF -D BUILD_EXAMPLES=OFF \
      -D Module_ITKReview=ON \
      -D ITK_BUILD_DEFAULT_MODULES=OFF \
      -D ITK_USE_SYSTEM_FFTW=ON \
      -D ITK_USE_FFTWF=ON -D ITK_USE_FFTWD=ON \
      -D Module_ITKDCMTK=ON \
      -D ITK_USE_SYSTEM_DCMTK=ON \
      -D ITK_USE_SYSTEM_GDCM=ON \
      -D ITK_USE_SYSTEM_ZLIB=ON \
      -D Module_ITKDeprecated=ON \
      -D Module_RTK=ON \
      -D RTK_USE_CUDA=OFF \
      -D RTK_BUILD_APPLICATIONS=OFF \
      -D Module_RTK_GIT_TAG="master" \
      -D ITK_MINIMUM_COMPLIANCE_LEVEL=3 \
    && ninja \
    && sudo ninja install \
    && cd ../ \
    && rm -Rf ITK*


WORKDIR /home/user/
