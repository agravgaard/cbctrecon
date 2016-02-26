# cbctrecon
CBCT Reconstruction toolkit for VARIAN
This project is a fork of the Cone Beam Reconstruction project be Yang-Kyun Park et al.

<b>Proton dose calculation on scatter-corrected CBCT image: Feasibility study for adaptive proton therapy </b>
http://dx.doi.org/10.1118/1.4923179

The aim of this fork is to make the recontruction toolkit working with Varian CBCTs, as the project mentioned above only have worked with Elekta CBCTs.

<b>Unix - branch</b> -> Ported by avoiding windows depencies and should not be expected to be stable yet! (Translated "Semaphore" part is expected to break if used at the moment)

For the moment only Windows 64bit and Unix systems (tested on up-to-date Arch-linux [26/02/2016]) with at least a cuda library file is supported.

In order to compile the software, you must have installed the following prerequisites* and compiled with Visual Studio 2013 OR gcc when compilation is needed:

<ul>
  <li>Cmake </li>
  <li>Qt 5.X </li>
  <li>FFTW </li>
  <li>CUDA (libcudart.so.X.X must at least be added to the LD_LIBRARY_PATH) </li>
  <li>DCMTK (Turn DCMTK_OVERWRITE_WIN32_COMPILER_FLAGS off)</li>
  <li>VTK </li>
  <li>ITK </li>
  <li>Plastimatch - modified**</li>
</ul>

<ul>
  <li>*   If version not explicitly stated, up-to-date version should work</li>
  <li>**  Additional and modified files will be added to the project folder..</li>
</ul>

andreasg@phys.au.dk
