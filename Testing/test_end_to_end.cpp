// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++, C#, and Java:
// http://www.viva64.com

// For testing CbctRecon

#ifdef USE_TINYREFL
#include <tinyrefl/api.hpp> // MUST BE INCLUDED FIRST, FFS!

#include "cbctrecon.h"
#include "cbctrecon.h.tinyrefl"
#else
#include "cbctrecon.h"
#endif

#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>

#include "cbctrecon_test.hpp"
#include "free_functions.h"

namespace fs = std::filesystem;

int load_dcm(CbctReconTest *cbctrecon_test, const fs::path &dcm_dir) {

  auto dcm_path = fs::absolute(dcm_dir);
  if (!fs::exists(dcm_dir)) {
    std::cerr << "Directory didn't exist: " << dcm_path << "\n";
    return -2;
  }
  if (fs::is_empty(dcm_dir)) {
    std::cerr << "Directory was empty: " << dcm_path << "\n";
    return -3;
  }

  cbctrecon_test->m_cbctrecon->m_strPathDirDefault = dcm_path;
  cbctrecon_test->test_LoadDICOMdir();
  if (cbctrecon_test->m_cbctrecon->m_spManualRigidCT.IsNull()) {
    std::cerr << "Manual Rigid CT was NULL -> Dicom dir was not read!\n";
    return -4;
  }
  return 0;
}

int load_and_recon_cb(CbctReconTest *cbctrecon_test, const fs::path &cbct_dir) {
  /* Load projections (Needs to be uploaded to girder first) */
  auto cbct_path = fs::absolute(cbct_dir);
  if (!fs::exists(cbct_dir)) {
    std::cerr << "Directory didn't exist: " << cbct_path << "\n";
    return -2;
  }
  if (fs::is_empty(cbct_dir)) {
    std::cerr << "Directory was empty: " << cbct_path << "\n";
    return -3;
  }

  // Set and guess some member variables from projection directory:
  auto proj_dir = cbct_path / "Acquisitions" / "746879825";
  if (!fs::exists(proj_dir)) {
    std::cerr << "Projection directory: " << proj_dir << " doesn't exists!\n";
    return -2;
  }
  auto proj_path = fs::absolute(proj_dir);
  auto qstr_proj_path = QString(proj_path.string().c_str());
  cbctrecon_test->test_SetHisDir(qstr_proj_path);

  const auto start_time = std::chrono::steady_clock::now();
  if (!cbctrecon_test->test_LoadSelectedProjFiles(qstr_proj_path, true)) {
    std::cerr << "Could not load or reconstruct CB projections!"
              << "\n";
    return -4;
  }
  if (cbctrecon_test->m_cbctrecon->m_spRawReconImg == nullptr) {
    std::cerr << "Raw reconstruction was Null!\n";
    return -5;
  }
  const auto end_time = std::chrono::steady_clock::now();
  std::cerr << "Proj. was loaded and reconstructed in: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                                     start_time)
                   .count()
            << " ms"
            << "\n";
  return 0;
}

int do_all_registrations(CbctReconTest *cbctrecon_test) {
  cbctrecon_test->m_dlgRegistration->UpdateListOfComboBox(0);
  cbctrecon_test->m_dlgRegistration->UpdateListOfComboBox(1);
  QString raw_str("RAW_CBCT");
  QString man_str("MANUAL_RIGID_CT");
  cbctrecon_test->m_dlgRegistration->LoadImgFromComboBox(0, raw_str);
  cbctrecon_test->m_dlgRegistration->LoadImgFromComboBox(1, man_str);

  cbctrecon_test->m_dlgRegistration->SLT_MovingImageSelected("MANUAL_RIGID_CT");
  cbctrecon_test->m_dlgRegistration
      ->SLT_PreProcessCT(); // BODY should be selected

  // Move MovingImage by -7.0, -85.0, -237.0: By emulating
  // DlgRegistration::ImageManualMove
  cbctrecon_test->m_dlgRegistration->SLT_KeyMoving(true);
  cbctrecon_test->m_dlgRegistration->ImageManualMoveOneShot(7.0f, 85.0f,
                                                            237.0f);
  cbctrecon_test->m_dlgRegistration->SLT_ConfirmManualRegistration();

  cbctrecon_test->m_dlgRegistration->SLT_DoRegistrationRigid();
  cbctrecon_test->m_dlgRegistration->SLT_DoRegistrationDeform();
  return 0;
}

int do_scatter_correction(CbctReconTest *cbctrecon_test) {
  cbctrecon_test->test_DoScatterCorrection_APRIORI();

  auto img_writer = itk::ImageFileWriter<UShortImageType>::New();
  img_writer->SetInput(cbctrecon_test->m_cbctrecon->m_spScatCorrReconImg);
  img_writer->SetFileName("plm_tmp/output_corr_fixed.mha");
  img_writer->Update();

  return 0;
}

int calculate_wepl(CbctReconTest *cbctrecon_test) {
  if (!cbctrecon_test->m_cbctrecon->m_spScatCorrReconImg) {
    std::cerr << "Scatter corrected recon image was null\n";
    return -1;
  }
  auto ss =
      cbctrecon_test->m_cbctrecon->m_structures->get_ss(ctType::DEFORM_CT);
  for (auto &structure : ss->slist) {
    std::cerr << structure.name << "\n";
  }

  const auto voi = std::string("CTV1");
  auto orig_voi = ss->get_roi_by_name(voi);
  const auto start_time = std::chrono::steady_clock::now();
  auto wepl_voi = crl::wepl::CalculateWEPLtoVOI(
      orig_voi.get(), 45, 45, cbctrecon_test->m_cbctrecon->m_spDeformedCT_Final,
      cbctrecon_test->m_cbctrecon->m_spScatCorrReconImg);
  const auto end_time = std::chrono::steady_clock::now();
  std::cerr << "WEPL was calculated in: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                                     start_time)
                   .count()
            << " ms"
            << "\n";

  const auto sum_ct = std::accumulate(
      orig_voi->pslist.begin(), orig_voi->pslist.end(), 0.0,
      [](const double val, Rtss_contour_modern &contour) {
        return val + std::accumulate(
                         contour.coordinates.begin(), contour.coordinates.end(),
                         0.0, [](const double val, const FloatVector &vec) {
                           return val + vec.magnitude();
                           /*std::sqrt(vec.x * vec.x + vec.y * vec.y +
                                     vec.z * vec.z);*/
                         });
      });
  const auto sum_wepl = std::accumulate(
      wepl_voi->pslist.begin(), wepl_voi->pslist.end(), 0.0,
      [](const double val, Rtss_contour_modern &contour) {
        return val + std::accumulate(
                         contour.coordinates.begin(), contour.coordinates.end(),
                         0.0, [](const double val, const FloatVector &vec) {
                           return val + vec.magnitude();
                           /*std::sqrt(vec.x * vec.x + vec.y * vec.y +
                                     vec.z * vec.z);*/
                         });
      });
  std::cerr << "Total difference between WEPL contour and CT contour: "
            << sum_ct - sum_wepl << "\n";
  return 0;
}

int end_to_end_test(const fs::path &dcm_dir_str, const fs::path &cbct_dir_str) {

  auto cbctrecon_test = std::make_unique<CbctReconTest>();

  auto ret_code = load_dcm(cbctrecon_test.get(), dcm_dir_str);
  if (ret_code < 0) {
    return ret_code;
  }

  ret_code = load_and_recon_cb(cbctrecon_test.get(), cbct_dir_str);
  if (ret_code < 0) {
    return ret_code;
  }

  ret_code = do_all_registrations(cbctrecon_test.get());
  if (ret_code < 0) {
    return ret_code;
  }

  /* Scatter correction algorithm "Batch" style */
  ret_code = do_scatter_correction(cbctrecon_test.get());
  if (ret_code < 0) {
    return ret_code;
  }

  /* WEPL structure test: disabled, because it's time consuming
   * and I don't want to optimize until I know if we'll actually use this tool
  ret_code = calculate_wepl(cbctrecon_test.get());
  if (ret_code < 0) {
    return ret_code;
  }*/

  /* Some verification of the WEPL results should go here */

  return 0;
}

int main(const int argc, char *argv[]) {

  if (argc < 3) {
    std::cerr << "Usage:\n"
              << argv[0]
              << " ./dicom/directory.tar.gz ./CB_proj/directory.tar.gz\n";
    return -1;
  }

  const auto dcm_dir_str = crl::split_string(argv[1], ".").at(0);
  const auto cbct_dir_str = crl::split_string(argv[2], ".").at(0);

  std::cerr << "Running cbctrecon_test!\n";
  const auto ret_code = end_to_end_test(dcm_dir_str, cbct_dir_str);
  if (ret_code < 0) {
    return ret_code;
  }

  return 0;
}
