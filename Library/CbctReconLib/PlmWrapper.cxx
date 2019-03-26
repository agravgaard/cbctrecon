// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++, C#, and Java:
// http://www.viva64.com

#include <memory>

#include "direction_cosines.h"

#include "PlmWrapper.h"
#include <itkImageRegionIterator.h>

VectorFieldType::Pointer Plm_image_friend::friend_convert_to_itk(Volume *vol) {
  /*return this->convert_gpuit_to_itk<VectorFieldType::Pointer, float(*)[3]>(
    vol);*/

  using ImageType = VectorFieldType;
  const auto img = reinterpret_cast<FloatVector *>(vol->img);
  ImageType::SizeType sz;
  ImageType::IndexType st;
  ImageType::RegionType rg;
  ImageType::PointType og;
  ImageType::SpacingType sp;
  ImageType::DirectionType dc;

  /* Copy header & allocate data for itk */
  for (auto d1 = 0U; d1 < 3U; d1++) {
    st[d1] = 0U;
    sz[d1] = static_cast<ImageType::SizeValueType>(vol->dim[d1]);
    sp[d1] = static_cast<ImageType::SpacingValueType>(vol->spacing[d1]);
    og[d1] = static_cast<ImageType::PointValueType>(vol->origin[d1]);
    for (auto d2 = 0U; d2 < 3U; d2++) {
      dc[d1][d2] = static_cast<ImageType::DirectionType::ValueType>(
          vol->direction_cosines[d1 * 3 + d2]);
    }
  }
  rg.SetSize(sz);
  rg.SetIndex(st);

  auto itk_img = ImageType::New();
  itk_img->SetRegions(rg);
  itk_img->SetOrigin(og);
  itk_img->SetSpacing(sp);
  itk_img->SetDirection(dc);

  itk_img->Allocate();

  /* Copy data into itk */
  using IteratorType = itk::ImageRegionIterator<ImageType>;
  IteratorType it(itk_img, rg);
  int i;
  for (it.GoToBegin(), i = 0; !it.IsAtEnd(); ++it, ++i) {
    auto vec = itk::Vector<float, 3>();
    vec.SetElement(0, img[i].x);
    vec.SetElement(1, img[i].y);
    vec.SetElement(2, img[i].z);
    it.Set(vec);
  }

  /* Free input volume */
  this->free_volume();

  /* Return the new image; caller will assign to correct member
  and set type */
  return itk_img;
}

Rtss_contour_modern::Rtss_contour_modern(
    const Rtss_contour_modern &old_contour) {
  ct_slice_uid = old_contour.ct_slice_uid;
  slice_no = old_contour.slice_no;
  num_vertices = old_contour.num_vertices;
  coordinates.resize(num_vertices);
  std::copy(old_contour.coordinates.begin(), old_contour.coordinates.end(),
            coordinates.begin());
}

Rtss_contour_modern &Rtss_contour_modern::
operator=(std::unique_ptr<Rtss_contour_modern> &&old_contour) {
  this->num_vertices = old_contour->num_vertices;
  this->coordinates = std::move(old_contour->coordinates);
  return *this;
}

Rtss_roi_modern::Rtss_roi_modern(const Rtss_roi_modern &old_roi) {
  name = old_roi.name;
  color = old_roi.color;
  id = old_roi.id;   /* Used for import/export (must be >= 1) */
  bit = old_roi.bit; /* Used for ss-img (-1 for no bit) */
  num_contours = old_roi.num_contours;
  pslist.resize(num_contours);
  std::copy(old_roi.pslist.begin(), old_roi.pslist.end(), pslist.begin());
}

Rtss_roi_modern &Rtss_roi_modern::
operator=(std::unique_ptr<Rtss_roi_modern> &&old_roi) {
  this->name = std::move(old_roi->name);
  this->num_contours = old_roi->num_contours;
  this->color = std::move(old_roi->color);
  this->id = old_roi->id;
  this->bit = old_roi->bit;
  this->pslist = std::move(old_roi->pslist);
  return *this;
}

Rtss_roi_modern::Rtss_roi_modern(std::unique_ptr<Rtss_roi_modern> &&old_roi) {
  this->name = std::move(old_roi->name);
  this->num_contours = old_roi->num_contours;
  this->color = std::move(old_roi->color);
  this->id = old_roi->id;
  this->bit = old_roi->bit;
  this->pslist = std::move(old_roi->pslist);
}

// Move assignment constructor
Rtss_modern &Rtss_modern::operator=(std::unique_ptr<Rtss_modern> &&old_rtss) {
  this->m_dim = old_rtss->m_dim;
  this->m_spacing = old_rtss->m_spacing;
  this->m_offset = old_rtss->m_offset;
  this->rast_dim = old_rtss->rast_dim;
  this->rast_spacing = old_rtss->rast_spacing;
  this->rast_offset = old_rtss->rast_offset;
  this->have_geometry = old_rtss->have_geometry;

  this->rast_dc = std::move(old_rtss->rast_dc);

  this->num_structures = old_rtss->num_structures;
  this->slist = std::move(old_rtss->slist);
  return *this;
}

// Copy constructor
Rtss_modern::Rtss_modern(const Rtss_modern &old_rtss) {
  m_dim = old_rtss.m_dim;
  m_spacing = old_rtss.m_spacing;
  m_offset = old_rtss.m_offset;
  rast_dim = old_rtss.rast_dim;
  rast_spacing = old_rtss.rast_spacing;
  rast_offset = old_rtss.rast_offset;
  have_geometry = old_rtss.have_geometry;

  auto unique_rast_dc =
      std::make_unique<Direction_cosines>(old_rtss.rast_dc->get_matrix());
  rast_dc = std::move(unique_rast_dc);

  num_structures = old_rtss.num_structures;
  slist.resize(num_structures);
  std::copy(old_rtss.slist.begin(), old_rtss.slist.end(), slist.begin());
}

std::unique_ptr<Rtss_roi_modern>
Rtss_modern::get_roi_by_name(const std::string &name) {
  for (auto &roi : slist) {
    if (roi.name == name) {
      return std::make_unique<Rtss_roi_modern>(roi);
    }
  }
  std::cerr << "VOI name was not in structure set !?" << std::endl;
  return nullptr;
}

Rtss_roi_modern &Rtss_modern::get_roi_ref_by_name(const std::string &name) {
  for (auto &roi : slist) {
    if (roi.name == name) {
      return roi;
    }
  }
  std::cerr << "VOI name was not in structure set !?\n";
  std::cerr << "Warning: Returning " << slist.at(0).name << " instead of "
            << name << "\n";
  return slist.at(0);
}
