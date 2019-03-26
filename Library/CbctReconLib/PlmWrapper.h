#ifndef PLMWRAPPER_H
#define PLMWRAPPER_H

#include <array>
#include <memory>

#undef TIMEOUT
#undef CUDA_FOUND
#include "plm_image.h"

#include "cbctrecon_config.h"
#include "cbctrecon_types.h"

class Direction_cosines;
class Volume;

class Plm_image_friend : public Plm_image {
public:
  VectorFieldType::Pointer friend_convert_to_itk(Volume *vol);
};

class CBCTRECON_API Rtss_contour_modern {
public:
  Rtss_contour_modern() = default;
  ~Rtss_contour_modern() = default;
  Rtss_contour_modern(const Rtss_contour_modern &old_contour);
  Rtss_contour_modern &operator=(const Rtss_contour_modern &contour) = default;
  Rtss_contour_modern(Rtss_contour_modern &&contour) = default;
  Rtss_contour_modern &operator=(Rtss_contour_modern &&contour) = default;
  Rtss_contour_modern &
  operator=(std::unique_ptr<Rtss_contour_modern> &&contour);

  std::vector<FloatVector> coordinates;
  /* Plastimatch specific */
  int slice_no = -1;
  std::string ct_slice_uid = "";
  size_t num_vertices = 0U;
};

class CBCTRECON_API Rtss_roi_modern {
public:
  Rtss_roi_modern() = default;
  ~Rtss_roi_modern() = default;
  Rtss_roi_modern(const Rtss_roi_modern &old_roi);
  Rtss_roi_modern(std::unique_ptr<Rtss_roi_modern> &&old_roi);
  Rtss_roi_modern &operator=(std::unique_ptr<Rtss_roi_modern> &&old_roi);

  std::vector<Rtss_contour_modern> pslist;
  std::string name = "";
  std::string color = "255 0 0";
  /* Plastimatch specific */
  size_t id = 1; /* Used for import/export (must be >= 1) */
  int bit = -1;  /* Used for ss-img (-1 for no bit) */
  size_t num_contours = 0;
};

class CBCTRECON_API Rtss_modern {
public:
  Rtss_modern() = default;
  ~Rtss_modern() = default;
  // Unique pointer, to make sure it's killed by its destructor after copy
  Rtss_modern &operator=(std::unique_ptr<Rtss_modern> &&old_rtss);
  Rtss_modern(const Rtss_modern &old_rtss);

  std::unique_ptr<Rtss_roi_modern> get_roi_by_name(const std::string &name);
  Rtss_roi_modern &get_roi_ref_by_name(const std::string &name);
  /* Output geometry */
  std::array<long, 3> m_dim{};
  std::array<float, 3> m_spacing{};
  std::array<float, 3> m_offset{};
  /* Rasterization geometry */
  std::array<long, 3> rast_dim{};
  std::array<float, 3> rast_spacing{};
  std::array<float, 3> rast_offset{};
  std::unique_ptr<Direction_cosines> rast_dc;
  /* Structures */
  std::vector<Rtss_roi_modern> slist;
  /* Plastimatch specific */
  bool have_geometry = false;
  size_t num_structures = 0;
};

#endif
