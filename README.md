# cbctrecon
CBCT Reconstruction toolkit for VARIAN type cone beam projections
This project is a fork of the Cone Beam Reconstruction project be Yang-Kyun Park et al.

<b>Proton dose calculation on scatter-corrected CBCT image: Feasibility study for adaptive proton therapy </b>
http://dx.doi.org/10.1118/1.4923179

The aim of this fork is to make the recontruction toolkit working with Varian CBCTs (hnd and xim format), as the project mentioned above only have worked with Elekta CBCTs.

For the moment Windows 64bit is supported.

In order to compile the software, you must have installed the following prerequisites and compiled with Visual Studio 2013 (or 2015 with CUDA 8.0 or above) when compilation is needed:

<ul>
  <li>Cmake - Latest</li>
  <li>Qt 5.X</li>
  <li>FFTW (optional but recommended)</li>
  <li>CUDA [Only the cudart library is mandatory]</li>
  <li>DCMTK - Latest: https://github.com/commontk/DCMTK </li>
  <li>ITK - Latest: https://github.com/InsightSoftwareConsortium/ITK </li>
  <li>RTK - Latest: https://github.com/SimonRit/RTK </li>
  <li>Plastimatch - https://gitlab.com/agravgaard/plastimatch </li>
</ul>
<b>Build and Install order and important flags*:</b>
<ul>
  <li>Your favorite: Qt 5.X, CUDA and OpenCL SDK</li>
  <li>DCMTK, `DCMTK_OVERWRITE_WIN32_COMPILER_FLAGS OFF`</li>
  <li>ITK, `ITKReview ON`, `ITKDCMTK` and `USE_SYSTEM_DCMTK ON`</li>
  <li>RTK</li>
  <li>Plastimatch</li>
  <li>CBCTRecon</li>
</ul>

*Before installation consider:
If you have nVidia hardware be sure to enable the `USE_CUDA` and `CUDA_HAS_GPU` flag whenever possible.
If you do not have nVidia hardware be sure to enable the `USE_OPENCL` and `OPENCL_HAS_GPU` flag whenever possible AND use https://github.com/agravgaard/RTK instead of the main fork OpenCL functionality will unfortunately be deprecated eventually.
If you want to use CUDA without nVidia hardware, then use the PGI compiler (pgc, pgcc) with nvcc from the CUDA SDK and copy the cuda dll's from the pgi bin directory to your working directory.
If you want to use Visual Studio 2015, then add `#if _MSC_VER < 1900 / #endif` around snprintf and vsnprintf definitions in Plastimatch (will be added to the fork later)
Also if you want to use Visual Studio 2015, you must use CUDA 8.0 or above due to compiler incompatibilities
If you want to use the PGI compiler, remember to match the cuda versions i.e. update to version 16 to use CUDA 8.0
If you have VTK, then you can link that with ITK and Plastimatch, but it is optional and not used in this project.
Avoid using HDF5 when compiling dependencies or deal with the compile linking problems yourself.
For older versions of Qt, ITK, CUDA check the RTK and Plastimatch compilation instructions.

Avoid using HDF5 when compiling dependencies or deal with the compile linking problems consequences yourself.
As a rule of thumb: if RTK and Plastimatch compiles this will too.

andreasg@phys.au.dk
