#ifndef CBCTRECON_COMPUTE_H
#define CBCTRECON_COMPUTE_H

#include "cbctrecon_config.h"

#include <algorithm> // for std::max
#include <charconv>
#include <iostream>
#include <optional>
#include <string>
#include <type_traits>

#include "itkImage.h"
#include "itkImageSliceIteratorWithIndex.h"

#include "rtkFieldOfViewImageFilter.h"
#include "rtkThreeDCircularProjectionGeometry.h"

#include "cbctrecon_types.h"
#include "free_functions.h"

namespace fs = std::filesystem;

namespace crl {

CBCTRECON_API void ApplyBowtie(FloatImageType::Pointer &projections,
                               const FloatImage2DType::Pointer &bowtie_proj);

CBCTRECON_API double GetMaxAndMinValueOfProjectionImage(
    double &fProjImgValueMax, double &fProjImgValueMin,
    const FloatImageType::Pointer &projImage); // , double theoreticalMin);

CBCTRECON_API void Get2DFrom3D(FloatImageType::Pointer &spSrcImg3D,
                               FloatImage2DType::Pointer &spTargetImg2D,
                               int idx, enPLANE iDirection);

CBCTRECON_API double
CalculateIntensityScaleFactorFromMeans(UShortImageType::Pointer &spProjRaw3D,
                                       UShortImageType::Pointer &spProjCT3D);
CBCTRECON_API double GetRawIntensityScaleFactor(std::string &strRef_mAs,
                                                std::string &strCur_mAs);
CBCTRECON_API void TransformationRTK2IEC(FloatImageType::Pointer &spSrcTarg);

CBCTRECON_API void AddConstHU(UShortImageType::Pointer &spImg, int HUval);

// This function came from the tracking project. trans values are all in mm,
// DICOM x, y, z
CBCTRECON_API void
ImageTransformUsingCouchCorrection(UShortImageType::Pointer &spUshortInput,
                                   UShortImageType::Pointer &spUshortOutput,
                                   const VEC3D &couch_trans,
                                   const VEC3D &couch_rot);

CBCTRECON_API void RotateImgBeforeFwd(UShortImageType::Pointer &spInputImgUS,
                                      UShortImageType::Pointer &spOutputImgUS);

CBCTRECON_API void
ConvertUshort2AttFloat(UShortImageType::Pointer &spImgUshort,
                       FloatImageType::Pointer &spAttImgFloat);

CBCTRECON_API
void CropFOV3D(UShortImageType::Pointer &sp_Img, const float physPosX,
               const float physPosY, const float physRadius,
               const float physTablePosY);

CBCTRECON_API
UShortImageType::Pointer
ConvertLineInt2Intensity_ushort(FloatImageType::Pointer &spProjLineInt3D);

CBCTRECON_API
FloatImageType::Pointer
ConvertIntensity2LineInt_ushort(UShortImageType::Pointer &spProjIntensity3D);

CBCTRECON_API
FloatImageType::Pointer
ConvertIntensity2LineInt_ushort(FloatImageType::Pointer &spProjIntensity3D);

CBCTRECON_API
void RenameFromHexToDecimal(const std::vector<fs::path> &filenameList);

/// Templates:

template <typename RefImageType, typename TargetImageType>
void AllocateByRef(typename RefImageType::Pointer &spRefImg3D,
                   typename TargetImageType::Pointer &spTarImg3D) {
  const auto sizeSrc = spRefImg3D->GetBufferedRegion().GetSize();
  const auto startSrc = spRefImg3D->GetBufferedRegion().GetIndex();

  const auto spacingSrc = spRefImg3D->GetSpacing();
  const auto originSrc = spRefImg3D->GetOrigin();

  typename RefImageType::RegionType region;
  region.SetSize(sizeSrc);
  region.SetIndex(startSrc);

  spTarImg3D = TargetImageType::New();

  spTarImg3D->SetRegions(region);
  spTarImg3D->SetSpacing(spacingSrc);
  spTarImg3D->SetOrigin(originSrc);

  spTarImg3D->Allocate();
  spTarImg3D->FillBuffer(0);
}

template <typename T>
T GetValueFrom3DImage(const unsigned int reqX, const unsigned int reqY,
                      const unsigned int reqZ,
                      typename itk::Image<T, 3>::Pointer &sp3DImage) {
  if (sp3DImage == nullptr) {
    return 0;
  }

  const auto idx = FloatImageType::IndexType{reqX, reqY, reqZ};
  if (!sp3DImage->GetBufferedRegion().IsInside(idx)) {
    return static_cast<T>(-1); // underflow on purpose
  }

  return sp3DImage->GetPixel(idx);
}

template <typename ImageType>
double
GetFOVRadius(const rtk::ThreeDCircularProjectionGeometry::Pointer &geometry,
             const typename ImageType::Pointer &ProjStack) {

  using FOVfilterType = rtk::FieldOfViewImageFilter<ImageType, ImageType>;
  typename FOVfilterType::Pointer FOVfilter = FOVfilterType::New();
  FOVfilter->SetGeometry(geometry);
  FOVfilter->SetProjectionsStack(ProjStack.GetPointer());
  double x, z;
  auto r_inf = -1.0;
  bool hasOverlap = FOVfilter->ComputeFOVRadius(
      FOVfilterType::FOVRadiusType::RADIUSINF, x, z, r_inf);
  // halffan gives r(BOTH)~25, r(SUP)~25, r(INF)~232 -> RADIUSINF also seems
  // to work for fullfan, so we'll use that.

  if (hasOverlap) {
    std::cout << "FOV (inf) radius was found: r=" << r_inf << std::endl;
  }
  auto r_sup = -1.0;
  hasOverlap = FOVfilter->ComputeFOVRadius(
      FOVfilterType::FOVRadiusType::RADIUSSUP, x, z, r_sup);
  if (hasOverlap) {
    std::cout << "FOV (sup) radius was found: r=" << r_sup << std::endl;
  }
  return std::max(r_inf, r_sup);
}

template <typename T, typename ImageType>
bool GetOutputResolutionFromFOV(
    typename T::SizeType &sizeOutput, typename T::SpacingType &spacing,
    const rtk::ThreeDCircularProjectionGeometry::Pointer &geometry,
    const typename ImageType::Pointer &ProjStack,
    const fs::path &outputFilePath) {

  if (outputFilePath.empty() || !fs::exists(outputFilePath)) {
    const double radius = GetFOVRadius<ImageType>(geometry, ProjStack);
    if (radius > 0.0) {
      sizeOutput[0] = 512; // AP
      sizeOutput[1] = 200; // SI
      sizeOutput[2] = 512; // LR
      spacing[0] = 2.0 * radius / sizeOutput[0];
      spacing[1] = 1.0;
      spacing[2] = 2.0 * radius / sizeOutput[2];
      return true;
    }
  }

  return false;
}

template <class T, std::enable_if_t<std::is_unsigned<T>::value, int> = 0>
constexpr auto float_to_(const float input) {
  const auto max_ushort = std::numeric_limits<T>::max();
  if (input < 0.0F) {
    return static_cast<T>(0);
  }
  if (input > static_cast<float>(max_ushort)) {
    return static_cast<T>(max_ushort -
                          1); // - 1 to avoid implicit cast overflow
  }
  return static_cast<T>(crl::ce_round(input));
}

template <class T, std::enable_if_t<std::is_floating_point<T>::value, int> = 0>
constexpr auto float_to_(const float input) {
  return static_cast<T>(input);
}

template <typename OutputImageType>
void Set2DTo3D(FloatImage2DType::Pointer &spSrcImg2D,
               typename OutputImageType::Pointer &spTargetImg3D, const int idx,
               const enPLANE iDirection) {
  if (spSrcImg2D == nullptr ||
      spTargetImg3D == nullptr) { // Target image should be also ready.
    return;
  }

  auto idxHor = 0, idxVer = 0, idxZ = 0;

  switch (iDirection) {
  case enPLANE::PLANE_AXIAL:
    idxHor = 0;
    idxVer = 1;
    idxZ = 2;
    break;
  case enPLANE::PLANE_FRONTAL:
    idxHor = 0;
    idxVer = 2;
    idxZ = 1;
    break;
  case enPLANE::PLANE_SAGITTAL:
    idxHor = 1;
    idxVer = 2;
    idxZ = 0;
    break;
  }

  auto imgDim2D = spSrcImg2D->GetBufferedRegion().GetSize();
  auto imgDim3D = spTargetImg3D->GetBufferedRegion().GetSize();

  // Filtering
  if (imgDim2D[0] != imgDim3D[idxHor] || imgDim2D[1] != imgDim3D[idxVer] ||
      idx < 0 || idx >= static_cast<int>(imgDim3D[idxZ])) {
    std::cout << "Error: image dimensions is not matching" << std::endl;
    std::cout << "2D= " << imgDim2D << std::endl;
    std::cout << "3D= " << imgDim3D << std::endl;
    return;
  }

  itk::ImageRegionConstIterator<FloatImage2DType> it_2D(
      spSrcImg2D, spSrcImg2D->GetBufferedRegion());
  itk::ImageSliceIteratorWithIndex<OutputImageType> it_3D(
      spTargetImg3D, spTargetImg3D->GetBufferedRegion());

  it_3D.SetFirstDirection(idxHor);
  it_3D.SetSecondDirection(idxVer);
  it_3D.GoToBegin();

  const int zSize = imgDim3D[idxZ];

  it_2D.GoToBegin();

  for (auto i = 0; i < zSize && !it_3D.IsAtEnd(); i++) {
    // Search matching slice using slice iterator for m_spProjCTImg
    if (i == idx) {
      while (!it_3D.IsAtEndOfSlice()) {
        while (!it_3D.IsAtEndOfLine()) {

          it_3D.Set(
              float_to_<typename OutputImageType::ValueType>(it_2D.Get()));
          // float tmpVal = (float)(it_3D.Get()); //in proj image case, this is
          // intensity  it_2D.Set(tmpVal);
          ++it_2D;
          ++it_3D;
        } // while2
        it_3D.NextLine();
      } // while1
      break;
    }
    //
    it_3D.NextSlice();
  } // end of for
}

} // namespace crl

#endif // CBCTRECON_COMPUTE_H
