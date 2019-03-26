#ifndef CBCTREGISTRATION_H
#define CBCTREGISTRATION_H

#include <itkImage.h>

//#undef TIMEOUT
//#undef CUDA_FOUND
//#include "itk_mask.h"

#include "cbctrecon_config.h"

#include "AG17RGBAImage.h"
#include "StructureSet.h"
#include "YK16GrayImage.h"
#include "cbctrecon_types.h"

class QString;
class CbctRecon;
class Dmap_parms;
class Pcmd_threshold;
class Mask_parms;
class Plm_image_header;
class Dcmtk_rt_study;
class Mask_operation;
class DoubleVector3DType;

enum enDevice {
  CPU_DEV,
  GPU_DEV,
};

enum enViewArrange {
  AXIAL_FRONTAL_SAGITTAL = 0,
  FRONTAL_SAGITTAL_AXIAL,
  SAGITTAL_AXIAL_FRONTAL,
};

enum enRegisterOption {
  PLAST_RIGID = 0,
  PLAST_GRADIENT,
  PLAST_AFFINE,
  PLAST_BSPLINE,
};

// #define DEFAULT_LABEL_SIZE1 512
// #define DEFAULT_LABEL_SIZE2 256
// #define DEFAULT_LABEL_SIZE3 256

class CBCTRECON_API CbctRegistration {

public:
  explicit CbctRegistration(CbctRecon *parent);
  ~CbctRegistration();
  CbctRegistration(const CbctRegistration &) = delete;
  void operator=(const CbctRegistration &) = delete;
  CbctRegistration(CbctRegistration &&) = delete;
  void operator=(CbctRegistration &&) = delete;

  void GenPlastiRegisterCommandFile(
      const QString &strPathCommandFile, const QString &strPathFixedImg,
      const QString &strPathMovingImg, const QString &strPathOutImg,
      const QString &strPathXformOut, enRegisterOption regiOption,
      const QString &strStageOption1, const QString &strStageOption2,
      const QString &strStageOption3, const QString &strPathFixedMask,
      bool optim_mse, bool use_cuda, QString &GradOptionStr) const;

  // get val mm
  static VEC3D GetShiftValueFromGradientXForm(QString &file_path,
                                              bool b_inverse);

  bool PreprocessCT(int iAirThresholdShort, QString strRSName, bool fill_bubble,
                    int iBubbleFillingVal, int iAirFillValShort);
  static void autoPreprocessCT(int iAirThresholdShort,
                               UShortImageType::Pointer &spFixed,
                               UShortImageType::Pointer &spMoving);
  void CalculateWEPLtoVOI(const std::string &voi_name, int gantry_angle,
                          int couch_angle, UShortImageType::Pointer &spMoving,
                          UShortImageType::Pointer &spFixed);
  float *ManualMoveByDCM() const;
  void LoadRTPlan(QString &strDCMPath);

  static void CallingPLMCommand(std::string &command_filepath);
  static DoubleVector3DType
  CallingPLMCommandXForm(std::string &command_filepath);
  bool CallingGPMCcommand(enDevice device, int n_sims, int n_plans,
                          QString &comma_sep_planfilepath,
                          UShortImageType::Pointer &spFixed,
                          UShortImageType::Pointer &spMoving,
                          UShortImageType::Pointer &spFixedDose,
                          UShortImageType::Pointer &spMovingDose);

  // void plm_dmap_main (Dmap_parms* parms);
  void plm_dmap_main(QString &img_in_fn, QString &img_out_fn) const;
  // void plm_threshold_main (Pcmd_threshold* parms);
  // void plm_threshold_main (Pcmd_threshold* parms);
  // plm_threshold_main(range_string, img_in_fn, img_out_fn);
  void plm_threshold_main(QString &strRange, QString &img_in_fn,
                          QString &img_out_fn) const;
  // void plm_mask_main (Mask_parms* parms);
  void plm_mask_main(Mask_operation mask_option, QString &input_fn,
                     QString &mask_fn, QString &output_fn,
                     float mask_value) const;

  void plm_expansion_contract_msk(QString &strPath_msk,
                                  QString &strPath_msk_exp_cont,
                                  double fExpVal) const;

  void plm_synth_trans_xf(QString &strPath_fixed, QString &strPath_out_xf,
                          double transX, double transY, double transZ) const;
  void ProcessCBCT_beforeAutoRigidRegi(QString &strPathRawCBCT,
                                       QString &strPath_mskSkinCT,
                                       QString &strPathOutputCBCT,
                                       double *manualTrans3d,
                                       bool bPrepareMaskOnly, double skinExp,
                                       int bkGroundValUshort);

  // void ProcessCBCT_beforeDeformRegi(QString& strPathRawCBCT, QString&
  // strPath_mskSkinCT_, QString& strPathOutputCBCT, double* manualTrans3d);
  void ProcessCBCT_beforeDeformRegi(
      QString &strPathRawCBCT, QString &strPath_mskSkinCT_manRegi,
      QString &strPathOutputCBCT, QString &strPathXFAutoRigid,
      bool bBubbleFilling, bool bPrepareMaskOnly, double skinExp,
      int bubbleThresh,
      int bubbleFill); // 8 mm skin cut + fill air bubbles inside CBCT
  void SetPlmOutputDir(QString &endFix);

  void PostSkinRemovingCBCT(
      UShortImageType::Pointer &spCBCT) const; // this function
                                               // will be called
                                               // from main Dlg.

  void CropSkinUsingRS(UShortImageType::Pointer &spImgUshort,
                       QString &strPathRS, double cropMargin) const;

  // void ThermoMaskRemovingCBCT(USHORT_ImageType::Pointer& spCBCTraw,
  // USHORT_ImageType::Pointer& spCBCTcor, int threshold);

  void ThermoMaskRemovingCBCT(UShortImageType::Pointer &spCBCTraw,
                              UShortImageType::Pointer &spCBCTcor,
                              int diffThreshold,
                              int noTouchThreshold /*= 1100*/,
                              double innerMargin, double outerMargin) const;

  void GenShellMask(QString &strPathInputMask, QString &strPathOutputMask,
                    double fInnerMargin, double fOuterMargin) const;

  VEC3D GetIsocenterDCM_FromRTPlan(QString &strFilePath) const;

  // void keyPressEvent ( QKeyEvent * e );
  // void Draw2DFrom3D(USHORT_ImageType::Pointer& pImg, enPLANE direction,
  // double pos, YK16GrayImage* pOutput2D);  void
  // Draw2DFrom3D(USHORT_ImageType::Pointer& pImg, enPLANE direction, double
  // pos, YK16GrayImage& Output2D);

  // still public:
  CbctRecon *m_pParent{};            // to pull 3D images
  YK16GrayImage m_YKImgFixed[3];     // CBCT in this study
  YK16GrayImage m_YKImgMoving[3];    // CBCT in this study
  YK16GrayImage m_YKDisp[3];         // CBCT in this study
  AG17RGBAImage m_DoseImgFixed[3];   // CBCT in this study
  AG17RGBAImage m_DoseImgMoving[3];  // CBCT in this study
  AG17RGBAImage m_AGDisp_Overlay[3]; // CBCT in this study
  std::unique_ptr<Rtss_roi_modern> WEPL_voi;
  std::unique_ptr<Rtss_roi_modern> cur_voi;
  bool dose_loaded = false;

  // UShortImageType::Pointer m_spFixed;  // pointer only, for display
  // UShortImageType::Pointer m_spMoving; // pointer only, for display

  // UShortImageType::Pointer m_spFixedDose;  // pointer only, for display
  // UShortImageType::Pointer m_spMovingDose; // pointer only, for display

  QString m_strPathPlastimatch;     // full path
  QString m_strPathCTSkin;          // shared data among functions
  QString m_strPathCTSkin_manRegi;  // shared data among functions, manually
                                    // registered to CBCT, w/ margin ( 10mm)
  QString m_strPathCTSkin_autoRegi; // shared data among functions w/ margin (10
                                    // mm), registered to CBCT
  QString m_strPathCTSkin_deformRegi; // this is for WEPL calculation
  QString m_strPathXFAutoRigid;       // Updated during SLT_DoRegistrationRigid
  QString m_strPathMskCBCTBubble;     // Updated during SLT_DoRegistrationRigid

  Dcmtk_rt_study *m_pDcmStudyPlan{};
};

#endif // CBCTREGISTRATION_H
