#ifndef ForwardProjectionImageFilter_hpp
#define ForwardProjectionImageFilter_hpp

// Conditional definition of the function
#ifdef RTK_USE_OPENCL

#include <array>
#include <vector>

#include "cbctrecon_config.h"

struct CBCTRECON_API OpenCL_forwardProject_options {
  std::array<unsigned int, 3> projSize{};
  std::array<unsigned int, 3> volSize{};
  std::vector<float> translatedProjectionIndexTransformMatrices;
  std::vector<float> translatedVolumeTransformMatrices;
  std::vector<float> source_positions;
  float t_step{};
  float radiusCylindricalDetector{};
  unsigned int vectorLength{};
  std::array<float, 3> box_min{};
  std::array<float, 3> box_max{};
  std::array<float, 3> spacing{};
};

void CBCTRECON_API
OpenCL_forward_project(float *h_proj_in, float *h_proj_out, float *h_vol,
                       OpenCL_forwardProject_options &fwd_opts);

#endif // end conditional definition of the class

#endif
