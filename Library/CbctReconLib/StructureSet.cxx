// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++, C#, and Java:
// http://www.viva64.com

#include <memory>

#include <QFile>

#undef TIMEOUT
#include "xform.h"
#include "xform_convert.h"

#include "StructureSet.h"

StructureSet::StructureSet() = default;

StructureSet::~StructureSet() = default;

void StructureSet::set_planCT_ss(std::unique_ptr<Rtss_modern> &&struct_set) {
  m_plan_ss = std::move(struct_set);
}

Rtss_modern *StructureSet::get_ss(const ctType struct_set) const {
  switch (struct_set) {
  case PLAN_CT:
    return m_plan_ss.get();
  case RIGID_CT:
    return m_rigid_ss.get();
  case DEFORM_CT:
    return m_deform_ss.get();
  }
  std::cerr << "Invalid CT type" << std::endl;
  return nullptr;
}

std::unique_ptr<Rtss_modern>
StructureSet::transform_by_vector(const ctType struct_set,
                                  FloatVector &vec) const {
  const auto ss = get_ss(struct_set);
  auto out_ss = std::make_unique<Rtss_modern>(*ss);

  for (auto &roi : out_ss->slist) {
    for (auto &contour : roi.pslist) {
      for (auto &coord : contour.coordinates) { // should SIMD
        coord.x += vec.x;
        coord.y += vec.y;
        coord.z += vec.z;
      }
    }
  }
  return out_ss;
}

std::unique_ptr<Rtss_modern> StructureSet::transform_by_vectorField(
    const ctType struct_set, const VectorFieldType::Pointer &vf) const {

  const auto ss = get_ss(struct_set);
  auto out_ss = std::make_unique<Rtss_modern>(*ss);

  for (auto &roi : out_ss->slist) {
    for (auto &contour : roi.pslist) {
      for (auto &coord : contour.coordinates) {
        VectorFieldType::PointType physIndex;
        physIndex[0] = coord.x;
        physIndex[1] = coord.y;
        physIndex[2] = coord.z;

        VectorFieldType::IndexType index{};
        if (!vf->TransformPhysicalPointToIndex(physIndex, index)) {
          std::cerr << "Index: " << index << " out of bounds: " << physIndex
                    << std::endl;
          continue;
        }

        coord.x += vf->GetPixel(index)[0];
        coord.y += vf->GetPixel(index)[1];
        coord.z += vf->GetPixel(index)[2];
      }
    }
  }
  return out_ss;
}

std::unique_ptr<Rtss_modern> StructureSet::transform_by_Lambda(
    const ctType struct_set, const TransformType &transform_function) const {

  const auto ss = get_ss(struct_set);
  auto out_ss = std::make_unique<Rtss_modern>(*ss);

  for (auto &roi : out_ss->slist) {
    for (auto &contour : roi.pslist) {
      for (auto &coord : contour.coordinates) {
        VectorFieldType::PointType physIndex;
        physIndex[0] = coord.x;
        physIndex[1] = coord.y;
        physIndex[2] = coord.z;

        auto new_point = transform_function(physIndex);

        coord.x = new_point[0];
        coord.y = new_point[1];
        coord.z = new_point[2];
      }
    }
  }
  return out_ss;
}

bool StructureSet::ApplyRigidTransformToPlan(QFile& rigid_transform_file) {
  auto xform = std::make_unique<Xform>();
  xform->load(rigid_transform_file.fileName().toStdString());

  const auto xform_type = xform->get_type();
  // First we make sure, it was a rigid-transform.
  switch (xform_type) {
  case XFORM_ITK_TRANSLATION:
  case XFORM_ITK_VERSOR:
  case XFORM_ITK_QUATERNION:
  case XFORM_ITK_AFFINE:
  case XFORM_ITK_SIMILARITY:
    std::cout << "Warning, rotation (and scaling) not yet implemented"
              << std::endl;
    break;
  default:
    std::cerr << "\a"
              << "You are passing the wrong type of transform file to the "
                 "rigid transform"
              << std::endl;
    return false;
  }

  // Now we get the translation of the transform
  // params is equivalent to itk::Array<T>
  itk::TransformBase::ParametersType params;

  switch (xform_type) {
  case XFORM_ITK_TRANSLATION: {
    const auto trn = xform->get_trn();
    params = trn->GetParameters();
    break;
  }
  case XFORM_ITK_VERSOR: {
    const auto trn = xform->get_vrs();
    params = trn->GetParameters();
    break;
  }
  case XFORM_ITK_QUATERNION: {
    const auto trn = xform->get_quat();
    params = trn->GetParameters();
    break;
  }
  case XFORM_ITK_AFFINE: {
    const auto trn = xform->get_aff();
    params = trn->GetParameters();
    break;
  }
  case XFORM_ITK_SIMILARITY: {
    const auto trn = xform->get_similarity();
    params = trn->GetParameters();
    break;
  }
  default:
    std::cerr << "we should never get here" << std::endl;
    return false;
  }

  FloatVector trn_vec{static_cast<float>(params.x()),
                      static_cast<float>(params.y()),
                      static_cast<float>(params.z())};

  m_rigid_ss = transform_by_vector(PLAN_CT, trn_vec);

  return true;
}

bool StructureSet::ApplyDeformTransformToRigid(QFile& deform_transform_file) {
  auto xform = Xform::New();
  xform->load(deform_transform_file.fileName().toStdString());

  const auto xform_type = xform->get_type();

  TransformType transform;
  VectorFieldType::Pointer vf;

  switch (xform_type) {
  case XFORM_ITK_TRANSLATION:
    transform = [&xform](const itk::Point<double, 3U> point) {
      return xform->get_trn()->TransformPoint(point);
    };
    break;
  case XFORM_ITK_VERSOR:
    transform = [&xform](const itk::Point<double, 3U> point) {
      return xform->get_vrs()->TransformPoint(point);
    };
    break;
  case XFORM_ITK_QUATERNION:
    transform = [&xform](const itk::Point<double, 3U> point) {
      return xform->get_quat()->TransformPoint(point);
    };
    break;
  case XFORM_ITK_AFFINE:
    transform = [&xform](const itk::Point<double, 3U> point) {
      return xform->get_aff()->TransformPoint(point);
    };
    break;
  case XFORM_ITK_BSPLINE:
    transform = [&xform](const itk::Point<double, 3U> point) {
      return xform->get_itk_bsp()->TransformPoint(point);
    };
    break;
  case XFORM_ITK_TPS:
    transform = [&xform](itk::Point<double, 3U> point) {
      return xform->get_itk_tps()->TransformPoint(point);
    };
    break;
  case XFORM_ITK_SIMILARITY:
    transform = [&xform](const itk::Point<double, 3U> point) {
      return xform->get_similarity()->TransformPoint(point);
    };
    break;
  case XFORM_ITK_VECTOR_FIELD: {
    vf = xform->get_itk_vf();
    break;
  }
  case XFORM_GPUIT_BSPLINE: {
    auto xform_converter = std::make_unique<Xform_convert>();
    xform_converter->set_input_xform(xform);
    xform_converter->m_xf_out_type = XFORM_ITK_VECTOR_FIELD;
    xform_converter->run();
    const auto out_xform = xform_converter->get_output_xform();
    vf = out_xform->get_itk_vf();
    break;
  }
  case XFORM_GPUIT_VECTOR_FIELD: {
    auto plm_img = std::make_unique<Plm_image_friend>();
    const auto vol = xform->get_gpuit_vf().get();
    // The below function is expensive compared to the other options
    // consider methods to avoid using this format in DlgRegistration.
    vf = plm_img->friend_convert_to_itk(vol);
    break;
  }
  default:
    std::cerr << "\a"
              << "You are passing the wrong type of transform file to the "
                 "deform transform\n";
    return false;
  }

  if (transform != nullptr) {
    m_deform_ss = transform_by_Lambda(RIGID_CT, transform);
    return true;
  }
  if (vf.IsNotNull()) {
    m_deform_ss = transform_by_vectorField(RIGID_CT, vf);
    return true;
  }

  std::cerr << "\a"
            << "Transform function were not created and no vector field were "
               "applied\n";
  return false;
}
