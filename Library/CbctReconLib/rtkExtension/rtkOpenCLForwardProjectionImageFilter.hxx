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

#ifndef rtkOpenCLForwardProjectionImageFilter_hxx
#define rtkOpenCLForwardProjectionImageFilter_hxx

#include "rtkConfiguration.h"
// Conditional definition of the class to pass ITKHeaderTest
#ifdef RTK_USE_OPENCL

#include "rtkOpenCLForwardProjectionImageFilter.h"

#include "rtkMacro.h"
#include <itkCastImageFilter.h>
#include <itkImageRegionConstIterator.h>
#include <itkImageRegionIteratorWithIndex.h>
#include <itkLinearInterpolateImageFunction.h>
#include <itkMacro.h>

#include "itkStatisticsImageFilter.h"

namespace rtk {

template <class TInputImage, class TOutputImage>
OpenCLForwardProjectionImageFilter<
    TInputImage, TOutputImage>::OpenCLForwardProjectionImageFilter()
    : m_StepSize(1) {}

template <class TInputImage, class TOutputImage>
void OpenCLForwardProjectionImageFilter<TInputImage,
                                        TOutputImage>::GenerateData() {
  if (this->GetGeometry()->GetSourceToDetectorDistances().size() &&
      this->GetGeometry()->GetSourceToDetectorDistances()[0] == 0) {
    itkGenericExceptionMacro(
        << "Parallel geometry is not handled by OpenCL forward projector.");
  }

  const typename Superclass::GeometryType *geometry = this->GetGeometry();
  const unsigned int Dimension = TInputImage::ImageDimension;
  const unsigned int iFirstProj =
      this->GetInput(0)->GetRequestedRegion().GetIndex(Dimension - 1);
  const unsigned int nProj =
      this->GetInput(0)->GetRequestedRegion().GetSize(Dimension - 1);
  const unsigned int nPixelsPerProj =
      this->GetOutput()->GetBufferedRegion().GetSize(0) *
      this->GetOutput()->GetBufferedRegion().GetSize(1) *
      itk::NumericTraits<typename TInputImage::PixelType>::GetLength();

  auto largestReg = this->GetOutput()->GetLargestPossibleRegion();
  this->GetOutput()->SetRegions(largestReg);
  this->GetOutput()->Allocate();

  OpenCL_forwardProject_options fwd_opts;
  // Setting BoxMin and BoxMax
  // SR: we are using textures (read_imagef sampling) where the pixel definition
  // is not center but corner. Therefore, we set the box limits from index to
  // index+size instead of, for ITK, index-0.5 to index+size-0.5.
  for (unsigned int i = 0; i < 3; i++) {
    fwd_opts.box_min.at(i) =
        this->GetInput(1)->GetBufferedRegion().GetIndex()[i] + 0.5;
    fwd_opts.box_max.at(i) =
        fwd_opts.box_min.at(i) +
        this->GetInput(1)->GetBufferedRegion().GetSize()[i] - 1.0;
  }

  // Getting Spacing
  for (unsigned int i = 0; i < 3; i++) {
    fwd_opts.spacing.at(i) = this->GetInput(1)->GetSpacing()[i];
  }
  fwd_opts.t_step = m_StepSize;

  // OpenCL convenient format for dimensions
  fwd_opts.projSize.at(0) = this->GetOutput()->GetBufferedRegion().GetSize()[0];
  fwd_opts.projSize.at(1) = this->GetOutput()->GetBufferedRegion().GetSize()[1];

  fwd_opts.volSize.at(0) = this->GetInput(1)->GetBufferedRegion().GetSize()[0];
  fwd_opts.volSize.at(1) = this->GetInput(1)->GetBufferedRegion().GetSize()[1];
  fwd_opts.volSize.at(2) = this->GetInput(1)->GetBufferedRegion().GetSize()[2];

  // auto *pin = this->GetInput(0)->GetBufferPointer();
  auto cast_filter_0 =
      itk::CastImageFilter<TInputImage, itk::Image<float, 3>>::New();
  cast_filter_0->SetInput(this->GetInput(0));
  cast_filter_0->Update();
  auto *pin = cast_filter_0->GetOutput()->GetBufferPointer();

  // auto *pvol = this->GetInput(1)->GetBufferPointer();
  auto cast_filter_1 =
      itk::CastImageFilter<TInputImage, itk::Image<float, 3>>::New();
  cast_filter_1->SetInput(this->GetInput(1));
  cast_filter_1->Update();
  auto *pvol = cast_filter_1->GetOutput()->GetBufferPointer();

  auto *pout = this->GetOutput()->GetBufferPointer();

  // Account for system rotations
  typename Superclass::GeometryType::ThreeDHomogeneousMatrixType volPPToIndex;
  volPPToIndex = GetPhysicalPointToIndexMatrix(this->GetInput(1));

  // Compute matrix to translate the pixel indices on the volume and the
  // detector if the Requested region has non-zero index
  typename Superclass::GeometryType::ThreeDHomogeneousMatrixType
      projIndexTranslation,
      volIndexTranslation;
  projIndexTranslation.SetIdentity();
  volIndexTranslation.SetIdentity();
  for (unsigned int i = 0; i < 3; i++) {
    projIndexTranslation[i][3] =
        this->GetOutput()->GetRequestedRegion().GetIndex(i);
    volIndexTranslation[i][3] =
        -this->GetInput(1)->GetBufferedRegion().GetIndex(i);

    // Adding 0.5 offset to change from the centered pixel convention (ITK)
    // to the corner pixel convention (OpenCL).
    volPPToIndex[i][3] += 0.5;
  }

  // Compute matrices to transform projection index to volume index, one per
  // projection
  auto translatedProjectionIndexTransformMatrices =
      std::vector<std::array<float, 12>>(nProj);
  auto translatedVolumeTransformMatrices =
      std::vector<std::array<float, 12>>(nProj);
  auto source_positions = std::vector<std::array<float, 3>>(nProj);

  fwd_opts.radiusCylindricalDetector = geometry->GetRadiusCylindricalDetector();

  // Go over each projection
  for (unsigned int iProj = iFirstProj; iProj < iFirstProj + nProj; iProj++) {
    typename Superclass::GeometryType::ThreeDHomogeneousMatrixType
        translatedProjectionIndexTransformMatrix;
    typename Superclass::GeometryType::ThreeDHomogeneousMatrixType
        translatedVolumeTransformMatrix;
    translatedVolumeTransformMatrix.Fill(0);

    // The matrices required depend on the type of detector
    if (fwd_opts.radiusCylindricalDetector == 0) {
      translatedProjectionIndexTransformMatrix =
          volIndexTranslation.GetVnlMatrix() * volPPToIndex.GetVnlMatrix() *
          geometry->GetProjectionCoordinatesToFixedSystemMatrix(iProj)
              .GetVnlMatrix() *
          rtk::GetIndexToPhysicalPointMatrix(this->GetInput()).GetVnlMatrix() *
          projIndexTranslation.GetVnlMatrix();
      for (int j = 0; j < 3; j++) { // Ignore the 4th row
        for (int k = 0; k < 4; k++) {
          translatedProjectionIndexTransformMatrices.at(iProj - iFirstProj)
              .at(j * 4 + k) = static_cast<float>(
              translatedProjectionIndexTransformMatrix[j][k]);
        }
      }
    } else {
      translatedProjectionIndexTransformMatrix =
          geometry->GetProjectionCoordinatesToDetectorSystemMatrix(iProj)
              .GetVnlMatrix() *
          rtk::GetIndexToPhysicalPointMatrix(this->GetInput()).GetVnlMatrix() *
          projIndexTranslation.GetVnlMatrix();
      for (int j = 0; j < 3; j++) { // Ignore the 4th row
        for (int k = 0; k < 4; k++) {
          translatedProjectionIndexTransformMatrices.at(iProj - iFirstProj)
              .at(j * 4 + k) = static_cast<float>(
              translatedProjectionIndexTransformMatrix[j][k]);
        }
      }

      translatedVolumeTransformMatrix =
          volIndexTranslation.GetVnlMatrix() * volPPToIndex.GetVnlMatrix() *
          geometry->GetRotationMatrices()[iProj].GetInverse();
      for (int j = 0; j < 3; j++) { // Ignore the 4th row
        for (int k = 0; k < 4; k++) {
          translatedVolumeTransformMatrices.at(iProj - iFirstProj)
              .at(j * 4 + k) =
              static_cast<float>(translatedVolumeTransformMatrix[j][k]);
        }
      }
    }

    // Compute source position in volume indices
    auto source_position = volPPToIndex * geometry->GetSourcePosition(iProj);

    // Copy it into a single large array
    for (unsigned int d = 0; d < 3; d++)
      source_positions.at(iProj - iFirstProj).at(d) =
          source_position[d]; // Ignore the 4th component
  }

  int projectionOffset = 0;
  fwd_opts.vectorLength =
      itk::PixelTraits<typename TInputImage::PixelType>::Dimension;

  for (unsigned int i = 0; i < nProj; i += SLAB_SIZE) {
    // If nProj is not a multiple of SLAB_SIZE, the last slab will contain less
    // than SLAB_SIZE projections
    fwd_opts.projSize[2] = std::min(nProj - i, (unsigned int)SLAB_SIZE);
    projectionOffset =
        iFirstProj + i - this->GetOutput()->GetBufferedRegion().GetIndex(2);

    fwd_opts.translatedProjectionIndexTransformMatrices =
        std::vector<float>(fwd_opts.projSize[2] * 12);
    fwd_opts.translatedVolumeTransformMatrices =
        std::vector<float>(fwd_opts.projSize[2] * 12);
    fwd_opts.source_positions = std::vector<float>(fwd_opts.projSize[2] * 3);
    for (unsigned int i_proj = 0; i_proj < fwd_opts.projSize[2]; ++i_proj) {
      for (unsigned int jk = 0; jk < 12; ++jk) {
        fwd_opts.translatedProjectionIndexTransformMatrices.at(i_proj * 12 +
                                                               jk) =
            translatedProjectionIndexTransformMatrices.at(i + i_proj).at(jk);
        fwd_opts.translatedVolumeTransformMatrices.at(i_proj * 12 + jk) =
            translatedVolumeTransformMatrices.at(i + i_proj).at(jk);
      }
      for (unsigned int j = 0; j < 3; ++j) {
        fwd_opts.source_positions.at(i_proj * 3 + j) =
            source_positions.at(i + i_proj).at(j);
      }
    }
    // Run the forward projection with a slab of SLAB_SIZE or less projections
    OpenCL_forward_project(pin + nPixelsPerProj * projectionOffset,
                           pout + nPixelsPerProj * projectionOffset, pvol,
                           fwd_opts);
  }
}

} // end namespace rtk

#endif // end conditional definition of the class

#endif
