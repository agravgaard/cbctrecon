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

#ifndef __rtkOpenCLFDKBackProjectionImageFilter_h
#define __rtkOpenCLFDKBackProjectionImageFilter_h

#include "rtkFDKBackProjectionImageFilter.h"

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

namespace rtk
{

/** \class OpenCLFDKBackProjectionImageFilter
 * \brief OpenCL version of FDK backprojection.
 *
 * GPU-based implementation of the backprojection step of the
 * [Feldkamp, Davis, Kress, 1984] algorithm for filtered backprojection
 * reconstruction of cone-beam CT images with a circular source trajectory.
 *
 * \author Simon Rit
 *
 * \ingroup Projector OpenCLImageToImageFilter
 */

class ITK_EXPORT OpenCLFDKBackProjectionImageFilter :
  public FDKBackProjectionImageFilter< itk::Image<float,3>, itk::Image<float,3> >
{
public:
  /** Standard class typedefs. */
  using ImageType = itk::Image<float,3>;
  using Self = OpenCLFDKBackProjectionImageFilter;
  using SuperClass = FDKBackProjectionImageFilter<ImageType, ImageType>;
  using Pointer = itk::SmartPointer<Self>;
  using ConstPointer = itk::SmartPointer<const Self>;

  using OutputImageRegionType = ImageType::RegionType;
  using ProjectionImageType = itk::Image<float, 2>;
  using ProjectionImagePointer = ProjectionImageType::Pointer;

  /** Method for creation through the object factory. */
  itkNewMacro(Self);

  /** Run-time type information (and related methods). */
  itkTypeMacro(OpenCLFDKBackProjectionImageFilter, ImageToImageFilter);

  /** Function to allocate memory on device */
  void InitDevice();

  /** Function to sync memory from device to host and free device memory */
  void CleanUpDevice();

protected:
  OpenCLFDKBackProjectionImageFilter();
  ~OpenCLFDKBackProjectionImageFilter() override = default;

  void GenerateData() override;

private:
  OpenCLFDKBackProjectionImageFilter(const Self&); //purposely not implemented
  void operator=(const Self&);                     //purposely not implemented

  cl_context       m_Context;
  cl_command_queue m_CommandQueue;
  cl_mem           m_DeviceMatrix;
  cl_mem           m_DeviceVolume;
  cl_mem           m_DeviceProjection;
  cl_program       m_Program;
  cl_kernel        m_Kernel;
};

} // end namespace rtk

#endif
