/*=========================================================================
 *
 *  Copyright RTK Consortium
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/

#ifndef RTKOPENCLFDKCONEBEAMRECONSTRUCTIONFILTER_H
#define RTKOPENCLFDKCONEBEAMRECONSTRUCTIONFILTER_H
#include "cbctrecon_config.h"
#include "rtkFDKConeBeamReconstructionFilter.h"
// #include "rtkOpenCLFDKWeightProjectionFilter.h" <- hard, because cpu is not
// bad and cuda is full of textures

#ifdef RTK_USE_OPENCL

#include "rtkOpenCLFDKBackProjectionImageFilter.h"

namespace rtk {

/** \class OpenCLFDKConeBeamReconstructionFilter
 * \brief Implements [Feldkamp, Davis, Kress, 1984] algorithm using OpenCL
 *
 * Replaces ramp and backprojection in FDKConeBeamReconstructionFilter with
 * - OpenCLFDKBackProjectionImageFilter.
 * Also take care to create the reconstructed volume on the GPU at the beginning
 * and transfers it at the end.
 *
 * \test rtkfdktest.cxx
 *
 * \author Simon Rit
 *
 * \ingroup ReconstructionAlgorithm OpenCLImageToImageFilter
 */
class CBCTRECON_API OpenCLFDKConeBeamReconstructionFilter
    : public FDKConeBeamReconstructionFilter<itk::Image<float, 3>,
                                             itk::Image<float, 3>, float> {
public:
  /** Standard class typedefs. */
  using Self = OpenCLFDKConeBeamReconstructionFilter;
  using Superclass =
      FDKConeBeamReconstructionFilter<itk::Image<float, 3>,
                                      itk::Image<float, 3>, float>;
  using Pointer = itk::SmartPointer<Self>;
  using ConstPointer = itk::SmartPointer<const Self>;

  /** Typedefs of subfilters which have been implemented with OpenCL */
  // typedef rtk::OpenCLFDKWeightProjectionFilter    WeightFilterType;
  using BackProjectionFilterType = OpenCLFDKBackProjectionImageFilter;

  /** Standard New method. */
  itkNewMacro(Self)

      /** Runtime information support. */
      itkTypeMacro(OpenCLFDKConeBeamReconstructionFilter, ImageToImageFilter);

protected:
  OpenCLFDKConeBeamReconstructionFilter();
  ~OpenCLFDKConeBeamReconstructionFilter() override = default;

  void GenerateData() override;

public:
  // purposely not implemented
  OpenCLFDKConeBeamReconstructionFilter(const Self &) = delete;
  void operator=(const Self &) = delete;
  OpenCLFDKConeBeamReconstructionFilter(Self &&) = delete;
  void operator=(Self &&) = delete;
}; // end of class

} // end namespace rtk

//#ifndef ITK_MANUAL_INSTANTIATION
//#include "rtkOpenCLFDKConeBeamReconstructionFilter.hxx"
//#endif

#endif
#endif
