#/bin/bash
set -xe

# Show available OpenCL driver & device:
clinfo

mkdir -p build && cd build

export COMMON_FLAGS=".. -GNinja -DCMAKE_CXX_STANDARD=20 -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/home/user/cbctrecon_install -DBUILD_TESTING=OFF -DCBCTRECON_BUILD_TESTS=ON -DRTK_USE_OPENCL=ON"
# Eigen should be included in ITK if necessary:
export COMMON_SYSTEM_LIBS="-DUSE_SYSTEM_ZLIB=ON -DUSE_SYSTEM_dlib=ON -DUSE_SYSTEM_DCMTK=ON -DHUNTER_ENABLED=OFF -DUSE_SYSTEM_Plastimatch=OFF"

if [ -d /usr/local/lib/cmake/ITK-5.2 ]; then # Use system ITK and RTK
    export COMMON_NONSYSTEM_ITK="-DUSE_SYSTEM_ITK=ON -DITK_DIR=/usr/local/lib/cmake/ITK-5.2/"
else
    export COMMON_NONSYSTEM_ITK="-DUSE_SYSTEM_ITK=OFF -DITK_USE_SYSTEM_DCMTK=ON -DUSE_ITK_DCMTK=OFF -DUSE_HUNTER_Eigen=OFF -DModule_RTK_GIT_TAG=master"
fi

if [[ "$CUDA_AVAILABLE" = "YES" ]]; then
    export CUDA_FLAGS="-DUSE_CUDA=ON -DEXACT_GCC=/usr/bin/gcc-10 -DCUDA_HOST_COMPILER=/usr/bin/gcc-10"
    nvidia-smi
    # Ubuntu Bionic:
    export DLIBDIR="-Ddlib_DIR=/usr/lib/cmake/dlib/"
else
    export CUDA_FLAGS="-DUSE_CUDA=OFF"
    # Ubuntu Eoan:
    export DLIBDIR="-Ddlib_DIR=/usr/lib/x86_64-linux-gnu/cmake/dlib/"
fi

if [[ "$COVERAGE" = "YES" ]]; then
    export COVERAGE_FLAGS="-DCBCTRECON_COVERAGE=ON"
else
    export COVERAGE_FLAGS="-DCBCTRECON_COVERAGE=OFF"
fi

# TBB
if [[ "$CC" = "icc" ]]; then
    export CXXFLAGS="$CXXFLAGS -fopenmp"
    export LDFLAGS="$LDFLAGS -ltbb"
elif [ -d /usr/include/tbb ]; then # Use system ITK and RTK
    export CXXFLAGS="$CXXFLAGS -I/usr/include -I/usr/include/tbb -fopenmp-simd"
    export LDFLAGS="$LDFLAGS -L/usr/lib/x86_64-linux-gnu -ltbb"
fi

cmake $COMMON_FLAGS \
    $COMMON_SYSTEM_LIBS \
    $COMMON_NONSYSTEM_ITK \
    $CUDA_FLAGS \
    $COVERAGE_FLAGS \
    $DLIBDIR \
    $CUSTOM_CMAKE_FLAGS

cmake --build . --target CbctData
cmake --build .

# Make sure the OpenCL files are in the exe path (CMake is being quirky)
cmake --build . --target install

ctest -VV

if [[ "$COVERAGE" = "YES" ]]; then
    cmake --build . --target fastcov_html
#  lcov --directory Testing --base-directory ../Library/CbctReconLib/ --capture --output-file coverage.info
#  lcov --summary coverage.info
fi
