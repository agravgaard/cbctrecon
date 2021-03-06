// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++, C#, and Java:
// http://www.viva64.com

#include "DlgRegistration.h"

#include <QDialog>
#include <QFile>
#include <QString>
#include <qcombobox.h>

#include "OpenCL/ImageFilters.hpp"
#include "PlmWrapper.h"
#include "StructureSet.h"
#include "cbctrecon.h"
#include "cbctrecon_compute.h"
#include "cbctrecon_io.h"
#include "cbctrecon_mainwidget.h"
#include "cbctregistration.h"
#include "free_functions.h"
#include "qtwrap.h"

namespace fs = std::filesystem;
using namespace std::literals;

#define FIXME_BACKGROUND_MAX (-1200)

enum class enCOLOR {
  RED,
  DARKRED,
  GREEN,
  BLUE,
  YELLOW,
  MAGENTA,
};

DlgRegistration::DlgRegistration() {
  /* Sets up the GUI */
  this->ui.setupUi(this);
  m_YKImgFixed = nullptr;
  m_YKImgMoving = nullptr;
  m_YKDisp = nullptr;

  m_DoseImgFixed = nullptr;
  m_DoseImgMoving = nullptr;
  m_AGDisp_Overlay = nullptr;
}

DlgRegistration::DlgRegistration(CbctReconWidget *parent) : QDialog(parent) {
  /* Sets up the GUI */
  this->ui.setupUi(this);
  m_pParent = parent;
  m_cbctregistration =
      std::make_unique<CbctRegistration>(parent->m_cbctrecon.get());

  m_YKImgFixed = &m_cbctregistration->m_YKImgFixed[0];
  m_YKImgMoving = &m_cbctregistration->m_YKImgMoving[0];
  m_YKDisp = &m_cbctregistration->m_YKDisp[0];

  m_DoseImgFixed = &m_cbctregistration->m_DoseImgFixed[0];
  m_DoseImgMoving = &m_cbctregistration->m_DoseImgMoving[0];
  m_AGDisp_Overlay = &m_cbctregistration->m_AGDisp_Overlay[0];

  connect(this->ui.labelOverlapWnd1, SIGNAL(Mouse_Move()), this,
          SLOT(SLT_UpdateSplit1())); // added
  connect(this->ui.labelOverlapWnd2, SIGNAL(Mouse_Move()), this,
          SLOT(SLT_UpdateSplit2())); // added
  connect(this->ui.labelOverlapWnd3, SIGNAL(Mouse_Move()), this,
          SLOT(SLT_UpdateSplit3())); // added

  connect(this->ui.labelOverlapWnd1, SIGNAL(Mouse_Pressed_Left()), this,
          SLOT(SLT_MousePressedLeft1())); // added
  connect(this->ui.labelOverlapWnd2, SIGNAL(Mouse_Pressed_Left()), this,
          SLOT(SLT_MousePressedLeft2())); // added
  connect(this->ui.labelOverlapWnd3, SIGNAL(Mouse_Pressed_Left()), this,
          SLOT(SLT_MousePressedLeft3())); // added
  connect(this->ui.labelOverlapWnd1, SIGNAL(Mouse_Pressed_Right()), this,
          SLOT(SLT_MousePressedRight1())); // added
  connect(this->ui.labelOverlapWnd2, SIGNAL(Mouse_Pressed_Right()), this,
          SLOT(SLT_MousePressedRight2())); // added
  connect(this->ui.labelOverlapWnd3, SIGNAL(Mouse_Pressed_Right()), this,
          SLOT(SLT_MousePressedRight3())); // added

  connect(this->ui.labelOverlapWnd1, SIGNAL(Mouse_Released_Left()), this,
          SLOT(SLT_MouseReleasedLeft1())); // added
  connect(this->ui.labelOverlapWnd2, SIGNAL(Mouse_Released_Left()), this,
          SLOT(SLT_MouseReleasedLeft2())); // added
  connect(this->ui.labelOverlapWnd3, SIGNAL(Mouse_Released_Left()), this,
          SLOT(SLT_MouseReleasedLeft3())); // added
  connect(this->ui.labelOverlapWnd1, SIGNAL(Mouse_Released_Right()), this,
          SLOT(SLT_MouseReleasedRight1())); // added
  connect(this->ui.labelOverlapWnd2, SIGNAL(Mouse_Released_Right()), this,
          SLOT(SLT_MouseReleasedRight2())); // added
  connect(this->ui.labelOverlapWnd3, SIGNAL(Mouse_Released_Right()), this,
          SLOT(SLT_MouseReleasedRight3())); // added

  connect(this->ui.labelOverlapWnd1, SIGNAL(FocusOut()), this,
          SLOT(SLT_CancelMouseAction())); // added
  connect(this->ui.labelOverlapWnd2, SIGNAL(FocusOut()), this,
          SLOT(SLT_CancelMouseAction())); // added
  connect(this->ui.labelOverlapWnd3, SIGNAL(FocusOut()), this,
          SLOT(SLT_CancelMouseAction())); // added

  connect(this->ui.labelOverlapWnd1, SIGNAL(Mouse_Wheel()), this,
          SLOT(SLT_MouseWheelUpdate1())); // added
  connect(this->ui.labelOverlapWnd2, SIGNAL(Mouse_Wheel()), this,
          SLOT(SLT_MouseWheelUpdate2())); // added
  connect(this->ui.labelOverlapWnd3, SIGNAL(Mouse_Wheel()), this,
          SLOT(SLT_MouseWheelUpdate3())); // added

  SLT_CancelMouseAction();
}

void DlgRegistration::initDlgRegistration(std::string &strDCMUID) {
  m_cbctregistration->SetPlmOutputDir(strDCMUID);

  const UShortImageType::Pointer spNull;
  // unlink all of the pointers
  auto p_parent = m_cbctregistration->m_pParent;
  p_parent->m_spRefCTImg = spNull;
  p_parent->m_spManualRigidCT =
      spNull; // copied from RefCTImg; ID: RefCT --> Moving Img, cloned
  p_parent->m_spAutoRigidCT = spNull; // ID: AutoRigidCT
  p_parent->m_spDeformedCT1 = spNull; // Deformmation will be carried out based
                                      // on Moving IMage of GUI //AutoDeformCT1
  p_parent->m_spDeformedCT2 = spNull; // AutoDeformCT2
  p_parent->m_spDeformedCT3 = spNull; // AutoDeformCT3
  p_parent->m_spDeformedCT_Final = spNull; // AutoDeformCT3

  this->ui.checkBoxKeyMoving->setChecked(false);

  // There seems to be a problem with the cropping function:
  // this->ui.checkBoxCropBkgroundCT->setChecked(false);

  this->ui.lineEditOriginChanged->setText("");

  UpdateListOfComboBox(0);
  UpdateListOfComboBox(1);
  // if not found, just skip
  SelectComboExternal(
      0, enREGI_IMAGES::REGISTER_RAW_CBCT); // will call fixedImageSelected
  SelectComboExternal(1,
                      enREGI_IMAGES::REGISTER_MANUAL_RIGID); // WILL BE IGNORED
}

void DlgRegistration::SetMovingDose(UShortImageType::Pointer dose) {
  this->m_spMovingDose = dose;
  if (this->m_spMovingDose) {
    this->m_cbctregistration->dose_loaded = true;
  }
}
void DlgRegistration::SetFixedDose(UShortImageType::Pointer dose) {
  this->m_spFixedDose = dose;
  if (this->m_spFixedDose) {
    this->m_cbctregistration->dose_loaded = true;
  }
}

void DlgRegistration::SLT_CrntPosGo() const {
  if (m_spFixed == nullptr) {
    return;
  }

  // DICOMN position, mm
  const auto curDCMPosX = this->ui.lineEditCurPosX->text().toDouble();
  const auto curDCMPosY = this->ui.lineEditCurPosY->text().toDouble();
  const auto curDCMPosZ = this->ui.lineEditCurPosZ->text().toDouble();

  auto imgOrigin = m_spFixed->GetOrigin();
  auto imgSpacing = m_spFixed->GetSpacing();

  const auto iSliderPosIdxZ =
      qRound((curDCMPosZ - imgOrigin[2]) / static_cast<double>(imgSpacing[2]));
  const auto iSliderPosIdxY =
      qRound((curDCMPosY - imgOrigin[1]) / static_cast<double>(imgSpacing[1]));
  const auto iSliderPosIdxX =
      qRound((curDCMPosX - imgOrigin[0]) / static_cast<double>(imgSpacing[0]));

  this->ui.sliderPosDisp1->setValue(iSliderPosIdxZ);
  this->ui.sliderPosDisp2->setValue(iSliderPosIdxY);
  this->ui.sliderPosDisp3->setValue(iSliderPosIdxX);
}

template <enCOLOR color> auto get_qtpoint_vector(qyklabel *window) {
  switch (color) {
  case enCOLOR::RED:
    return &window->m_vPt;
  case enCOLOR::DARKRED:
    return &window->m_vPt_darkred;
  case enCOLOR::GREEN:
    return &window->m_vPt_green;
  case enCOLOR::BLUE:
    return &window->m_vPt_blue;
  case enCOLOR::YELLOW:
    return &window->m_vPt_yellow;
  case enCOLOR::MAGENTA:
    return &window->m_vPt_magenta;
  default:
    return &window->m_vPt_green;
  }
}

template <typename ImageBase, enPLANE plane, enCOLOR color>
auto set_points_by_slice(qyklabel *window, Rtss_roi_modern *voi,
                         std::array<double, 3> curPhysPos,
                         typename ImageBase::SpacingType imgSpacing,
                         typename ImageBase::PointType imgOriginFixed,
                         typename ImageBase::SizeType imgSize) {
  auto *Wnd_contour = get_qtpoint_vector<color>(window);
  Wnd_contour->clear();

  auto wnd_size = window->size();
  auto wnd_height = wnd_size.rheight();
  const auto wnd_width = wnd_size.rwidth();
  auto x_scale = 1.0 / static_cast<double>(wnd_width);
  auto y_scale = 1.0 / static_cast<double>(wnd_height);
  switch (plane) {
  case enPLANE::PLANE_AXIAL:
    x_scale *= imgSpacing[0] * imgSize[0];
    y_scale *= imgSpacing[1] * imgSize[1];
    break;
  case enPLANE::PLANE_FRONTAL:
    x_scale *= imgSpacing[0] * imgSize[0];
    y_scale *= imgSpacing[2] * imgSize[2];
    break;
  case enPLANE::PLANE_SAGITTAL:
    x_scale *= imgSpacing[1] * imgSize[1];
    y_scale *= imgSpacing[2] * imgSize[2];
  }

  for (auto contour : voi->pslist) {
    if (contour.coordinates.empty()) {
      continue;
    }
    const auto first_point = contour.coordinates.at(0);
    // Axial
    if (first_point[2] > curPhysPos[0] - imgSpacing[2] &&
        first_point[2] < curPhysPos[0] + imgSpacing[2] &&
        plane == enPLANE::PLANE_AXIAL) {
      for (auto point : contour.coordinates) {
        Wnd_contour->emplace_back((point[0] - imgOriginFixed[0]) / x_scale,
                                  (point[1] - imgOriginFixed[1]) / y_scale);
      }
    }
    for (auto &point : contour.coordinates) {
      // Frontal
      if (point[1] > curPhysPos[1] - imgSpacing[1] &&
          point[1] < curPhysPos[1] + imgSpacing[1] &&
          plane == enPLANE::PLANE_FRONTAL) {
        Wnd_contour->emplace_back((point[0] - imgOriginFixed[0]) / x_scale,
                                  wnd_height -
                                      (point[2] - imgOriginFixed[2]) / y_scale);
      }
      // Sagittal
      if (point[0] > curPhysPos[2] - imgSpacing[0] &&
          point[0] < curPhysPos[2] + imgSpacing[0] &&
          plane == enPLANE::PLANE_SAGITTAL) {
        Wnd_contour->emplace_back((point[1] - imgOriginFixed[1]) / x_scale,
                                  wnd_height -
                                      (point[2] - imgOriginFixed[2]) / y_scale);
      }
    }
  }

  window->m_bDrawPoints = true;
}

void DlgRegistration::SLT_DrawImageWhenSliceChange() {
  if (m_spFixed == nullptr) {
    return;
  }

  int sliderPosIdxZ, sliderPosIdxY, sliderPosIdxX;

  switch (m_enViewArrange) {
  case enViewArrange::AXIAL_FRONTAL_SAGITTAL:
    sliderPosIdxZ =
        this->ui.sliderPosDisp1
            ->value(); // Z corresponds to axial, Y to frontal, X to sagittal
    sliderPosIdxY = this->ui.sliderPosDisp2->value();
    sliderPosIdxX = this->ui.sliderPosDisp3->value();
    break;
  case enViewArrange::FRONTAL_SAGITTAL_AXIAL:
    sliderPosIdxY = this->ui.sliderPosDisp1->value();
    sliderPosIdxX = this->ui.sliderPosDisp2->value();
    sliderPosIdxZ = this->ui.sliderPosDisp3->value();

    break;
  case enViewArrange::SAGITTAL_AXIAL_FRONTAL:
    sliderPosIdxX = this->ui.sliderPosDisp1->value();
    sliderPosIdxZ = this->ui.sliderPosDisp2->value();
    sliderPosIdxY = this->ui.sliderPosDisp3->value();
    break;
  default:
    sliderPosIdxZ =
        this->ui.sliderPosDisp1
            ->value(); // Z corresponds to axial, Y to frontal, X to sagittal
    sliderPosIdxY = this->ui.sliderPosDisp2->value();
    sliderPosIdxX = this->ui.sliderPosDisp3->value();
    break;
  }

  auto imgSize = m_spFixed->GetBufferedRegion().GetSize(); // 1016x1016 x z
  auto imgOrigin = m_spFixed->GetOrigin();
  auto imgSpacing = m_spFixed->GetSpacing();

  auto curPhysPos = std::array<double, 3>{{
      imgOrigin[2] + sliderPosIdxZ * imgSpacing[2], // Z in default setting
      imgOrigin[1] + sliderPosIdxY * imgSpacing[1], // Y
      imgOrigin[0] + sliderPosIdxX * imgSpacing[0]  // Z
  }};

  const auto refIdx = 3 - static_cast<int>(m_enViewArrange);

  if (this->ui.checkBoxDrawCrosshair->isChecked()) {
    m_YKDisp[refIdx % 3].m_bDrawCrosshair = true;
    m_YKDisp[(refIdx + 1) % 3].m_bDrawCrosshair = true;
    m_YKDisp[(refIdx + 2) % 3].m_bDrawCrosshair = true;

    // m_YKDisp[0]// Left Top image, the largest

    m_YKDisp[refIdx % 3].m_ptCrosshair.setX(sliderPosIdxX); // axial
    m_YKDisp[refIdx % 3].m_ptCrosshair.setY(sliderPosIdxY);

    m_YKDisp[(refIdx + 1) % 3].m_ptCrosshair.setX(sliderPosIdxX); // Frontal
    m_YKDisp[(refIdx + 1) % 3].m_ptCrosshair.setY(static_cast<int>(imgSize[2]) -
                                                  sliderPosIdxZ - 1);

    m_YKDisp[(refIdx + 2) % 3].m_ptCrosshair.setX(sliderPosIdxY); // Sagittal
    m_YKDisp[(refIdx + 2) % 3].m_ptCrosshair.setY(static_cast<int>(imgSize[2]) -
                                                  sliderPosIdxZ - 1);

    m_YKImgFixed[0].m_bDrawCrosshair = true;
    m_YKImgFixed[1].m_bDrawCrosshair = true;
    m_YKImgFixed[2].m_bDrawCrosshair = true;

    m_YKImgFixed[0].m_ptCrosshair.setX(sliderPosIdxX); // sagittal slider
    m_YKImgFixed[0].m_ptCrosshair.setY(sliderPosIdxY);

    m_YKImgFixed[1].m_ptCrosshair.setX(sliderPosIdxX); // sagittal slider
    m_YKImgFixed[1].m_ptCrosshair.setY(static_cast<int>(imgSize[2]) -
                                       sliderPosIdxZ - 1);

    m_YKImgFixed[2].m_ptCrosshair.setX(sliderPosIdxY); // sagittal slider
    m_YKImgFixed[2].m_ptCrosshair.setY(static_cast<int>(imgSize[2]) -
                                       sliderPosIdxZ - 1);
  } else {
    m_YKDisp[0].m_bDrawCrosshair = false;
    m_YKDisp[1].m_bDrawCrosshair = false;
    m_YKDisp[2].m_bDrawCrosshair = false;

    m_YKImgFixed[0].m_bDrawCrosshair = false;
    m_YKImgFixed[1].m_bDrawCrosshair = false;
    m_YKImgFixed[2].m_bDrawCrosshair = false;
  }

  // center of the split value is passed by m_YKImgFixed;

  if (m_spMoving != nullptr) {
    m_pParent->m_cbctrecon->Draw2DFrom3DDouble(
        m_spFixed, m_spMoving, enPLANE::PLANE_AXIAL, curPhysPos[0],
        m_YKImgFixed[0], m_YKImgMoving[0]);
    m_pParent->m_cbctrecon->Draw2DFrom3DDouble(
        m_spFixed, m_spMoving, enPLANE::PLANE_FRONTAL, curPhysPos[1],
        m_YKImgFixed[1], m_YKImgMoving[1]);
    m_pParent->m_cbctrecon->Draw2DFrom3DDouble(
        m_spFixed, m_spMoving, enPLANE::PLANE_SAGITTAL, curPhysPos[2],
        m_YKImgFixed[2], m_YKImgMoving[2]);
    if (m_cbctregistration->dose_loaded) {
      m_pParent->m_cbctrecon->Draw2DFrom3DDouble(
          m_spFixedDose, m_spMovingDose, enPLANE::PLANE_AXIAL, curPhysPos[0],
          m_DoseImgFixed[0], m_DoseImgMoving[0]);
      m_pParent->m_cbctrecon->Draw2DFrom3DDouble(
          m_spFixedDose, m_spMovingDose, enPLANE::PLANE_FRONTAL, curPhysPos[1],
          m_DoseImgFixed[1], m_DoseImgMoving[1]);
      m_pParent->m_cbctrecon->Draw2DFrom3DDouble(
          m_spFixedDose, m_spMovingDose, enPLANE::PLANE_SAGITTAL, curPhysPos[2],
          m_DoseImgFixed[2], m_DoseImgMoving[2]);
    }
  } else {
    m_pParent->m_cbctrecon->Draw2DFrom3DDouble(
        m_spFixed, m_spFixed, enPLANE::PLANE_AXIAL, curPhysPos[0],
        m_YKImgFixed[0], m_YKImgMoving[0]);
    m_pParent->m_cbctrecon->Draw2DFrom3DDouble(
        m_spFixed, m_spFixed, enPLANE::PLANE_FRONTAL, curPhysPos[1],
        m_YKImgFixed[1], m_YKImgMoving[1]);
    m_pParent->m_cbctrecon->Draw2DFrom3DDouble(
        m_spFixed, m_spFixed, enPLANE::PLANE_SAGITTAL, curPhysPos[2],
        m_YKImgFixed[2], m_YKImgMoving[2]);
    if (m_cbctregistration->dose_loaded) {
      m_pParent->m_cbctrecon->Draw2DFrom3DDouble(
          m_spFixedDose, m_spFixedDose, enPLANE::PLANE_AXIAL, curPhysPos[0],
          m_DoseImgFixed[0], m_DoseImgMoving[0]);
      m_pParent->m_cbctrecon->Draw2DFrom3DDouble(
          m_spFixedDose, m_spFixedDose, enPLANE::PLANE_FRONTAL, curPhysPos[1],
          m_DoseImgFixed[1], m_DoseImgMoving[1]);
      m_pParent->m_cbctrecon->Draw2DFrom3DDouble(
          m_spFixedDose, m_spFixedDose, enPLANE::PLANE_SAGITTAL, curPhysPos[2],
          m_DoseImgFixed[2], m_DoseImgMoving[2]);
    }
  }

  // Update position lineEdit
  // QString strPos1, strPos2, strPos3;
  // strPos1.sprintf("%3.1f", curPhysPos[0]);
  // strPos2.sprintf("%3.1f", curPhysPos[1]);
  // strPos3.sprintf("%3.1f", curPhysPos[2]);

  this->ui.lineEditCurPosX->setText(to_qstr(crl::stringify(curPhysPos[0])));
  this->ui.lineEditCurPosY->setText(to_qstr(crl::stringify(curPhysPos[1])));
  this->ui.lineEditCurPosZ->setText(to_qstr(crl::stringify(curPhysPos[2])));

  ////Update Origin text box
  auto imgOriginFixed = m_spFixed->GetOrigin();
  QString strOriFixed = to_qstr(crl::make_sep_str<','>(
      imgOriginFixed[0], imgOriginFixed[1], imgOriginFixed[2]));
  this->ui.lineEditOriginFixed->setText(strOriFixed);

  if (m_spMoving != nullptr) {
    const auto imgOriginMoving = m_spMoving->GetOrigin();
    QString strOriMoving = to_qstr(crl::make_sep_str<','>(
        imgOriginMoving[0], imgOriginMoving[1], imgOriginMoving[2]));
    this->ui.lineEditOriginMoving->setText(strOriMoving);
  }

  auto arr_wnd = std::array<qyklabel *, 3>{{this->ui.labelOverlapWnd1,
                                            this->ui.labelOverlapWnd2,
                                            this->ui.labelOverlapWnd3}};

  if (m_cbctregistration->cur_voi != nullptr) {
    const auto p_cur_voi = m_cbctregistration->cur_voi.get();

    set_points_by_slice<UShortImageType, enPLANE::PLANE_AXIAL, enCOLOR::RED>(
        arr_wnd.at(refIdx % 3), p_cur_voi, curPhysPos, imgSpacing,
        imgOriginFixed, imgSize);

    set_points_by_slice<UShortImageType, enPLANE::PLANE_FRONTAL, enCOLOR::RED>(
        arr_wnd.at((refIdx + 1) % 3), p_cur_voi, curPhysPos, imgSpacing,
        imgOriginFixed, imgSize);

    set_points_by_slice<UShortImageType, enPLANE::PLANE_SAGITTAL, enCOLOR::RED>(
        arr_wnd.at((refIdx + 2) % 3), p_cur_voi, curPhysPos, imgSpacing,
        imgOriginFixed, imgSize);
  }

  if (m_cbctregistration->WEPL_voi != nullptr) {
    if (m_cbctregistration->cur_voi != nullptr) {
      const auto gantry_angle = this->ui.spinBox_GantryAngle->value();
      const auto couch_angle = this->ui.spinBox_CouchAngle->value();
      const auto &orig_voi = *(m_cbctregistration->cur_voi);
      auto p_cur_voi_distal =
          crl::roi_to_distal_only_roi(orig_voi, gantry_angle, couch_angle);

      set_points_by_slice<UShortImageType, enPLANE::PLANE_AXIAL,
                          enCOLOR::DARKRED>(
          arr_wnd.at(refIdx % 3), &p_cur_voi_distal, curPhysPos, imgSpacing,
          imgOriginFixed, imgSize);

      set_points_by_slice<UShortImageType, enPLANE::PLANE_FRONTAL,
                          enCOLOR::DARKRED>(
          arr_wnd.at((refIdx + 1) % 3), &p_cur_voi_distal, curPhysPos,
          imgSpacing, imgOriginFixed, imgSize);

      set_points_by_slice<UShortImageType, enPLANE::PLANE_SAGITTAL,
                          enCOLOR::DARKRED>(
          arr_wnd.at((refIdx + 2) % 3), &p_cur_voi_distal, curPhysPos,
          imgSpacing, imgOriginFixed, imgSize);
    }
    const auto p_wepl_voi = m_cbctregistration->WEPL_voi.get();

    set_points_by_slice<UShortImageType, enPLANE::PLANE_AXIAL, enCOLOR::GREEN>(
        arr_wnd.at(refIdx % 3), p_wepl_voi, curPhysPos, imgSpacing,
        imgOriginFixed, imgSize);

    set_points_by_slice<UShortImageType, enPLANE::PLANE_FRONTAL,
                        enCOLOR::GREEN>(arr_wnd.at((refIdx + 1) % 3),
                                        p_wepl_voi, curPhysPos, imgSpacing,
                                        imgOriginFixed, imgSize);

    set_points_by_slice<UShortImageType, enPLANE::PLANE_SAGITTAL,
                        enCOLOR::GREEN>(arr_wnd.at((refIdx + 2) % 3),
                                        p_wepl_voi, curPhysPos, imgSpacing,
                                        imgOriginFixed, imgSize);
  }
  if (m_cbctregistration->extra_voi != nullptr) {
    const auto p_extra_voi = m_cbctregistration->extra_voi.get();

    set_points_by_slice<UShortImageType, enPLANE::PLANE_AXIAL, enCOLOR::BLUE>(
        arr_wnd.at(refIdx % 3), p_extra_voi, curPhysPos, imgSpacing,
        imgOriginFixed, imgSize);

    set_points_by_slice<UShortImageType, enPLANE::PLANE_FRONTAL, enCOLOR::BLUE>(
        arr_wnd.at((refIdx + 1) % 3), p_extra_voi, curPhysPos, imgSpacing,
        imgOriginFixed, imgSize);

    set_points_by_slice<UShortImageType, enPLANE::PLANE_SAGITTAL,
                        enCOLOR::BLUE>(arr_wnd.at((refIdx + 2) % 3),
                                       p_extra_voi, curPhysPos, imgSpacing,
                                       imgOriginFixed, imgSize);
  }
  if (m_cbctregistration->extra_voi_top5 != nullptr) {
    const auto p_extra_voi_top5 = m_cbctregistration->extra_voi_top5.get();

    set_points_by_slice<UShortImageType, enPLANE::PLANE_AXIAL, enCOLOR::YELLOW>(
        arr_wnd.at(refIdx % 3), p_extra_voi_top5, curPhysPos, imgSpacing,
        imgOriginFixed, imgSize);

    set_points_by_slice<UShortImageType, enPLANE::PLANE_FRONTAL,
                        enCOLOR::YELLOW>(arr_wnd.at((refIdx + 1) % 3),
                                         p_extra_voi_top5, curPhysPos,
                                         imgSpacing, imgOriginFixed, imgSize);

    set_points_by_slice<UShortImageType, enPLANE::PLANE_SAGITTAL,
                        enCOLOR::YELLOW>(arr_wnd.at((refIdx + 2) % 3),
                                         p_extra_voi_top5, curPhysPos,
                                         imgSpacing, imgOriginFixed, imgSize);
  }
  if (m_cbctregistration->WEPL_voi_top5 != nullptr) {
    const auto p_WEPL_voi_top5 = m_cbctregistration->WEPL_voi_top5.get();

    set_points_by_slice<UShortImageType, enPLANE::PLANE_AXIAL,
                        enCOLOR::MAGENTA>(arr_wnd.at(refIdx % 3),
                                          p_WEPL_voi_top5, curPhysPos,
                                          imgSpacing, imgOriginFixed, imgSize);

    set_points_by_slice<UShortImageType, enPLANE::PLANE_FRONTAL,
                        enCOLOR::MAGENTA>(arr_wnd.at((refIdx + 1) % 3),
                                          p_WEPL_voi_top5, curPhysPos,
                                          imgSpacing, imgOriginFixed, imgSize);

    set_points_by_slice<UShortImageType, enPLANE::PLANE_SAGITTAL,
                        enCOLOR::MAGENTA>(arr_wnd.at((refIdx + 2) % 3),
                                          p_WEPL_voi_top5, curPhysPos,
                                          imgSpacing, imgOriginFixed, imgSize);
  }

  SLT_DrawImageInFixedSlice();
}

// Display is not included here
void DlgRegistration::whenFixedImgLoaded() const {
  if (m_spFixed == nullptr) {
    return;
  }

  auto imgSize = m_spFixed->GetBufferedRegion().GetSize(); // 1016x1016 x z

  // to avoid first unnecessary action.
  disconnect(this->ui.sliderPosDisp1, SIGNAL(valueChanged(int)), this,
             SLOT(SLT_DrawImageWhenSliceChange()));
  disconnect(this->ui.sliderPosDisp2, SIGNAL(valueChanged(int)), this,
             SLOT(SLT_DrawImageWhenSliceChange()));
  disconnect(this->ui.sliderPosDisp3, SIGNAL(valueChanged(int)), this,
             SLOT(SLT_DrawImageWhenSliceChange()));

  disconnect(this->ui.sliderFixedW, SIGNAL(valueChanged(int)), this,
             SLOT(SLT_DrawImageInFixedSlice()));
  disconnect(this->ui.sliderFixedL, SIGNAL(valueChanged(int)), this,
             SLOT(SLT_DrawImageInFixedSlice()));
  disconnect(this->ui.sliderMovingW, SIGNAL(valueChanged(int)), this,
             SLOT(SLT_DrawImageInFixedSlice()));
  disconnect(this->ui.sliderMovingL, SIGNAL(valueChanged(int)), this,
             SLOT(SLT_DrawImageInFixedSlice()));

  initOverlapWndSize();

  this->ui.sliderPosDisp1->setMinimum(0);
  this->ui.sliderPosDisp1->setMaximum(static_cast<int>(imgSize[2] - 1));
  const auto curPosZ = static_cast<int>(imgSize[2] / 2);
  this->ui.sliderPosDisp1->setValue(curPosZ);

  this->ui.sliderPosDisp2->setMinimum(0);
  this->ui.sliderPosDisp2->setMaximum(static_cast<int>(imgSize[1] - 1));
  const auto curPosY = static_cast<int>(imgSize[1] / 2);
  this->ui.sliderPosDisp2->setValue(curPosY);

  this->ui.sliderPosDisp3->setMinimum(0);
  this->ui.sliderPosDisp3->setMaximum(static_cast<int>(imgSize[0] - 1));
  const auto curPosX = static_cast<int>(imgSize[0] / 2);
  this->ui.sliderPosDisp3->setValue(curPosX);

  auto x_split = QPoint(static_cast<int>(imgSize[0] / 2),
                        static_cast<int>(imgSize[1] / 2));
  auto y_split = QPoint(static_cast<int>(imgSize[0] / 2),
                        static_cast<int>(imgSize[2] / 2));
  auto z_split = QPoint(static_cast<int>(imgSize[1] / 2),
                        static_cast<int>(imgSize[2] / 2));

  m_YKDisp[0].SetSplitCenter(x_split);
  m_YKDisp[1].SetSplitCenter(y_split);
  m_YKDisp[2].SetSplitCenter(z_split);

  const auto iSliderW = 2000;
  const auto iSliderL = 1024;

  this->ui.sliderFixedW->setValue(iSliderW);
  this->ui.sliderMovingW->setValue(iSliderW);

  this->ui.sliderFixedL->setValue(iSliderL);
  this->ui.sliderMovingL->setValue(iSliderL);

  connect(this->ui.sliderPosDisp1, SIGNAL(valueChanged(int)), this,
          SLOT(SLT_DrawImageWhenSliceChange()));
  connect(this->ui.sliderPosDisp2, SIGNAL(valueChanged(int)), this,
          SLOT(SLT_DrawImageWhenSliceChange()));
  connect(this->ui.sliderPosDisp3, SIGNAL(valueChanged(int)), this,
          SLOT(SLT_DrawImageWhenSliceChange()));

  connect(this->ui.sliderFixedW, SIGNAL(valueChanged(int)), this,
          SLOT(SLT_DrawImageInFixedSlice()));
  connect(this->ui.sliderFixedL, SIGNAL(valueChanged(int)), this,
          SLOT(SLT_DrawImageInFixedSlice()));
  connect(this->ui.sliderMovingW, SIGNAL(valueChanged(int)), this,
          SLOT(SLT_DrawImageInFixedSlice()));
  connect(this->ui.sliderMovingL, SIGNAL(valueChanged(int)), this,
          SLOT(SLT_DrawImageInFixedSlice()));
}

void DlgRegistration::whenMovingImgLoaded() {
  // do nothing so far
  /*if (!m_pParent->m_spRefCTImg)
        return;

  m_spMoving = m_pParent->m_spRefCTImg;*/
}

void DlgRegistration::SLT_DrawImageInFixedSlice() const
// Display Swap here!
{
  // Constitute m_YKDisp from Fixed and Moving

  if (this->ui.checkBoxDrawSplit->isChecked()) {
    for (auto i = 0; i < 3; i++) {
      auto idxAdd = static_cast<int>(m_enViewArrange); // m_iViewArrange = 0,1,2
      if (idxAdd + i >= 3) {
        idxAdd = idxAdd - 3;
      }

      m_YKDisp[i].SetSpacing(m_YKImgFixed[i + idxAdd].m_fSpacingX,
                             m_YKImgFixed[i + idxAdd].m_fSpacingY);

      m_YKDisp[i].SetSplitOption(enSplitOption::PRI_LEFT_TOP);
      // m_YKDisp[i].SetSplitCenter(QPoint dataPt);//From mouse event
      if (!m_YKDisp[i].ConstituteFromTwo(m_YKImgFixed[i + idxAdd],
                                         m_YKImgMoving[i + idxAdd])) {
        std::cout << "Image error " << i + 1 << " th view" << std::endl;
      }
    }
  } else {
    for (auto i = 0; i < 3; i++) {
      auto addedViewIdx = static_cast<int>(m_enViewArrange);
      if (i + addedViewIdx >= 3) {
        addedViewIdx = addedViewIdx - 3;
      }

      m_YKDisp[i].CloneImage(m_YKImgFixed[i + addedViewIdx]);
    }
  }

  // For dose overlay
  if (m_cbctregistration->dose_loaded) {
    if (this->ui.checkBoxDrawSplit->isChecked()) {
      for (size_t i = 0; i < 3; i++) {
        auto idxAdd =
            static_cast<size_t>(m_enViewArrange); // m_iViewArrange = 0,1,2
        if (idxAdd + i >= 3) {
          idxAdd = idxAdd - 3;
        }

        m_AGDisp_Overlay[i].SetSpacing(m_DoseImgFixed[i + idxAdd].m_fSpacingX,
                                       m_DoseImgFixed[i + idxAdd].m_fSpacingY);

        m_AGDisp_Overlay[i].SetSplitOption(enSplitOption::PRI_LEFT_TOP);
        if (!m_AGDisp_Overlay[i].ConstituteFromTwo(
                m_DoseImgFixed[i + idxAdd], m_DoseImgMoving[i + idxAdd])) {
          std::cout << "Dose Image error " << i + 1 << " th view" << std::endl;
        }
      }
    } else {
      for (size_t i = 0; i < 3; i++) {
        auto addedViewIdx = static_cast<size_t>(m_enViewArrange);
        if (i + addedViewIdx >= 3) {
          addedViewIdx = addedViewIdx - 3;
        }

        m_AGDisp_Overlay[i].CloneImage(m_DoseImgFixed[i + addedViewIdx]);
      }
    }
  }

  const auto sliderW1 = this->ui.sliderFixedW->value();
  const auto sliderW2 = this->ui.sliderMovingW->value();

  const auto sliderL1 = this->ui.sliderFixedL->value();
  const auto sliderL2 = this->ui.sliderMovingL->value();

  m_YKDisp[0].FillPixMapDual(sliderL1, sliderL2, sliderW1, sliderW2);
  m_YKDisp[1].FillPixMapDual(sliderL1, sliderL2, sliderW1, sliderW2);
  m_YKDisp[2].FillPixMapDual(sliderL1, sliderL2, sliderW1, sliderW2);

  this->ui.labelOverlapWnd1->SetBaseImage(&m_YKDisp[0]);
  this->ui.labelOverlapWnd2->SetBaseImage(&m_YKDisp[1]);
  this->ui.labelOverlapWnd3->SetBaseImage(&m_YKDisp[2]);

  // here gPMC results could be checked for and displayed, possibly with
  // modification to the qyklabel class /AGA 02/08/2017
  if (m_cbctregistration->dose_loaded) {
    m_AGDisp_Overlay[0].FillPixMapDual(sliderL1, sliderL2, sliderW1, sliderW2);
    m_AGDisp_Overlay[1].FillPixMapDual(sliderL1, sliderL2, sliderW1, sliderW2);
    m_AGDisp_Overlay[2].FillPixMapDual(sliderL1, sliderL2, sliderW1, sliderW2);

    this->ui.labelOverlapWnd1->SetOverlayImage(&m_AGDisp_Overlay[0]);
    this->ui.labelOverlapWnd2->SetOverlayImage(&m_AGDisp_Overlay[1]);
    this->ui.labelOverlapWnd3->SetOverlayImage(&m_AGDisp_Overlay[2]);
  }

  this->ui.labelOverlapWnd1->update();
  this->ui.labelOverlapWnd2->update();
  this->ui.labelOverlapWnd3->update();
}

void DlgRegistration::SLT_UpdateSplit1() // Mouse Move event
{
  const auto idx = 0;
  UpdateSplit(idx, this->ui.labelOverlapWnd1);
}

void DlgRegistration::SLT_UpdateSplit2() {
  const auto idx = 1;
  UpdateSplit(idx, this->ui.labelOverlapWnd2);
}
void DlgRegistration::SLT_UpdateSplit3() {
  const auto idx = 2;
  UpdateSplit(idx, this->ui.labelOverlapWnd3);
}

void DlgRegistration::UpdateSplit(const int viewIdx, qyklabel *pOverlapWnd) {
  const auto idx = viewIdx;

  if (pOverlapWnd == nullptr) {
    return;
  }

  if (m_YKDisp[idx].IsEmpty()) {
    return;
  }

  if (!m_bPressedLeft[idx] && !m_bPressedRight[idx]) {
    return;
  }

  const auto dspWidth = pOverlapWnd->width();
  const auto dspHeight = pOverlapWnd->height();

  const auto dataWidth = m_YKDisp[idx].m_iWidth;
  const auto dataHeight = m_YKDisp[idx].m_iHeight;
  if (dataWidth * dataHeight == 0) {
    return;
  }

  const auto dataX = pOverlapWnd->GetDataPtFromMousePos().x();
  const auto dataY = pOverlapWnd->GetDataPtFromMousePos().y();

  // only works when while the left Mouse is being clicked
  if (m_bPressedLeft[idx]) {
    auto xy_point = QPoint(dataX, dataY);
    m_YKDisp[idx].SetSplitCenter(xy_point);
    /* if (cond) {
     * calculateAngularWEPL(xy_point);
     * SLT_DrawWEPLContour(); // Radial (Spiderweb) plot
     * }
     */
    SLT_DrawImageInFixedSlice();
  } else if (m_bPressedRight[idx] && this->ui.checkBoxPan->isChecked()) {
    ////Update offset information of dispImage

    // GetOriginalDataPos (PanStart)
    // offset should be 0.. only relative distance matters. offset is in
    // realtime changing
    auto ptDataPanStartRel = pOverlapWnd->View2DataExt(
        m_ptPanStart, static_cast<int>(dspWidth), static_cast<int>(dspHeight),
        dataWidth, dataHeight, QPoint(0, 0), m_YKDisp[idx].m_fZoom);

    auto ptDataPanEndRel = pOverlapWnd->View2DataExt(
        QPoint(pOverlapWnd->x, pOverlapWnd->y), static_cast<int>(dspWidth),
        static_cast<int>(dspHeight), dataWidth, dataHeight, QPoint(0, 0),
        m_YKDisp[idx].m_fZoom);

    const auto curOffsetX = ptDataPanEndRel.x() - ptDataPanStartRel.x();
    const auto curOffsetY = ptDataPanEndRel.y() - ptDataPanStartRel.y();

    const auto prevOffsetX = m_ptTmpOriginalDataOffset.x();
    const auto prevOffsetY = m_ptTmpOriginalDataOffset.y();

    // double fZoom = m_YKDisp[idx].m_fZoom;

    m_YKDisp[idx].SetOffset(prevOffsetX - curOffsetX, prevOffsetY - curOffsetY);

    SLT_DrawImageInFixedSlice();
  } else if (m_bPressedRight[idx] &&
             !this->ui.checkBoxPan->isChecked()) // Window Level
  {
    const auto wWidth = 2.0;
    const auto wLevel = 2.0;

    const auto iAddedWidth =
        static_cast<int>((m_ptWindowLevelStart.y() - pOverlapWnd->y) * wWidth);
    const auto iAddedLevel =
        static_cast<int>((pOverlapWnd->x - m_ptWindowLevelStart.x()) * wLevel);

    // Which image is clicked first??
    auto crntDataPt = pOverlapWnd->GetDataPtFromViewPt(
        m_ptWindowLevelStart.x(), m_ptWindowLevelStart.y());

    if (pOverlapWnd->m_pYK16Image != nullptr) {
      if (m_YKDisp[idx].isPtInFirstImage(crntDataPt.x(), crntDataPt.y())) {
        this->ui.sliderFixedW->setValue(
            m_iTmpOriginalW +
            iAddedWidth); // SLT_DrawImageInFixedSlice will be called
        this->ui.sliderFixedL->setValue(m_iTmpOriginalL + iAddedLevel);
      } else {
        this->ui.sliderMovingW->setValue(m_iTmpOriginalW + iAddedWidth);
        this->ui.sliderMovingL->setValue(m_iTmpOriginalL + iAddedLevel);
      }
    }
  }
}

// Slide change by scrolling
void DlgRegistration::SLT_MouseWheelUpdate1() const {
  if (this->ui.checkBoxZoom->isChecked()) {
    const auto oldZoom = this->ui.labelOverlapWnd1->m_pYK16Image->m_fZoom;

    const auto fWeighting = 0.2;

    this->ui.labelOverlapWnd1->m_pYK16Image->SetZoom(
        oldZoom + this->ui.labelOverlapWnd1->m_iMouseWheelDelta * fWeighting);
    this->SLT_DrawImageInFixedSlice();
  } else {
    this->ui.sliderPosDisp1->setValue(
        this->ui.sliderPosDisp1->value() +
        this->ui.labelOverlapWnd1->m_iMouseWheelDelta);
  }
}

void DlgRegistration::SLT_MouseWheelUpdate2() const {
  if (this->ui.checkBoxZoom->isChecked()) {
    const auto oldZoom = this->ui.labelOverlapWnd2->m_pYK16Image->m_fZoom;
    const auto fWeighting = 0.2;

    this->ui.labelOverlapWnd2->m_pYK16Image->SetZoom(
        oldZoom + this->ui.labelOverlapWnd2->m_iMouseWheelDelta * fWeighting);
    this->SLT_DrawImageInFixedSlice();

  } else {
    this->ui.sliderPosDisp2->setValue(
        this->ui.sliderPosDisp2->value() +
        this->ui.labelOverlapWnd2->m_iMouseWheelDelta);
  }
}

void DlgRegistration::SLT_MouseWheelUpdate3() const {
  if (this->ui.checkBoxZoom->isChecked()) {
    const auto oldZoom = this->ui.labelOverlapWnd3->m_pYK16Image->m_fZoom;
    const auto fWeighting = 0.2;

    this->ui.labelOverlapWnd3->m_pYK16Image->SetZoom(
        oldZoom + this->ui.labelOverlapWnd3->m_iMouseWheelDelta * fWeighting);
    this->SLT_DrawImageInFixedSlice();

  } else {
    this->ui.sliderPosDisp3->setValue(
        this->ui.sliderPosDisp3->value() +
        this->ui.labelOverlapWnd3->m_iMouseWheelDelta);
  }
}

// release everything
void DlgRegistration::SLT_CancelMouseAction() {
  for (auto i = 0; i < 3; i++) {
    m_bPressedLeft[i] = false; // Left Mouse Pressed but not released
    m_bPressedRight[i] = false;
  }
}

void DlgRegistration::SLT_MousePressedLeft1() { m_bPressedLeft[0] = true; }

void DlgRegistration::SLT_MousePressedLeft2() { m_bPressedLeft[1] = true; }

void DlgRegistration::SLT_MousePressedLeft3() { m_bPressedLeft[2] = true; }

void DlgRegistration::MousePressedRight(const int wndIdx, qyklabel *pWnd) {
  m_bPressedRight[wndIdx] = true;

  if (this->ui.checkBoxPan->isChecked()) {
    m_ptPanStart.setX(pWnd->x);
    m_ptPanStart.setY(pWnd->y);

    m_ptTmpOriginalDataOffset.setX(m_YKDisp[wndIdx].m_iOffsetX);
    m_ptTmpOriginalDataOffset.setY(m_YKDisp[wndIdx].m_iOffsetY);
  } else {
    m_ptWindowLevelStart.setX(pWnd->x);
    m_ptWindowLevelStart.setY(pWnd->y);

    auto crntDataPt = pWnd->GetDataPtFromViewPt(m_ptWindowLevelStart.x(),
                                                m_ptWindowLevelStart.y());

    if (m_YKDisp[wndIdx].isPtInFirstImage(crntDataPt.x(), crntDataPt.y())) {
      m_iTmpOriginalL = this->ui.sliderFixedL->value();
      m_iTmpOriginalW = this->ui.sliderFixedW->value();
    } else {
      m_iTmpOriginalL = this->ui.sliderMovingL->value();
      m_iTmpOriginalW = this->ui.sliderMovingW->value();
    }
  }
}

void DlgRegistration::SLT_MousePressedRight1() {
  const auto idx = 0;
  MousePressedRight(idx, this->ui.labelOverlapWnd1);
}

void DlgRegistration::SLT_MousePressedRight2() {
  const auto idx = 1;
  MousePressedRight(idx, this->ui.labelOverlapWnd2);
}

void DlgRegistration::SLT_MousePressedRight3() {
  const auto idx = 2;
  MousePressedRight(idx, this->ui.labelOverlapWnd3);
}

void DlgRegistration::SLT_MouseReleasedLeft1() { m_bPressedLeft[0] = false; }

void DlgRegistration::SLT_MouseReleasedLeft2() { m_bPressedLeft[1] = false; }

void DlgRegistration::SLT_MouseReleasedLeft3() { m_bPressedLeft[2] = false; }

void DlgRegistration::SLT_MouseReleasedRight1() { m_bPressedRight[0] = false; }

void DlgRegistration::SLT_MouseReleasedRight2() { m_bPressedRight[1] = false; }

void DlgRegistration::SLT_MouseReleasedRight3() { m_bPressedRight[2] = false; }

void DlgRegistration::SLT_ChangeView() {
  m_enViewArrange =
      static_cast<enViewArrange>((static_cast<int>(m_enViewArrange) + 1) % 3);

  YK16GrayImage tmpBufYK[3];

  for (auto i = 0; i < 3; i++) {
    tmpBufYK[i].CloneImage(m_YKDisp[i]);
  }

  for (auto i = 0; i < 3; i++) {
    const auto nextIdx = (i + 1) % 3;
    m_YKDisp[i].CloneImage(tmpBufYK[nextIdx]);
  }

  initOverlapWndSize();
  shiftSliceSlider();
  updateSliceLabel();

  SLT_DrawImageWhenSliceChange(); // only image data will be updated. Zoom and
                                  // other things are not.
}

void DlgRegistration::initOverlapWndSize() {
  /*this->ui.labelOverlapWnd1->setFixedWidth(DEFAULT_LABEL_SIZE1);
  this->ui.labelOverlapWnd1->setFixedHeight(DEFAULT_LABEL_SIZE1);

  this->ui.labelOverlapWnd2->setFixedWidth(DEFAULT_LABEL_SIZE2);
  this->ui.labelOverlapWnd2->setFixedHeight(DEFAULT_LABEL_SIZE2);

  this->ui.labelOverlapWnd3->setFixedWidth(DEFAULT_LABEL_SIZE2);
  this->ui.labelOverlapWnd3->setFixedHeight(DEFAULT_LABEL_SIZE2);*/
}

void DlgRegistration::shiftSliceSlider() const
// shift one slice slider information
{
  // to avoid first unnecessary action.
  disconnect(this->ui.sliderPosDisp1, SIGNAL(valueChanged(int)), this,
             SLOT(SLT_DrawImageWhenSliceChange()));
  disconnect(this->ui.sliderPosDisp2, SIGNAL(valueChanged(int)), this,
             SLOT(SLT_DrawImageWhenSliceChange()));
  disconnect(this->ui.sliderPosDisp3, SIGNAL(valueChanged(int)), this,
             SLOT(SLT_DrawImageWhenSliceChange()));

  int crntMin[3];
  int crntMax[3];
  int crntValue[3];

  int newMin[3];
  int newMax[3];
  int newValue[3];

  crntMin[0] = this->ui.sliderPosDisp1->minimum();
  crntMin[1] = this->ui.sliderPosDisp2->minimum();
  crntMin[2] = this->ui.sliderPosDisp3->minimum();

  crntMax[0] = this->ui.sliderPosDisp1->maximum();
  crntMax[1] = this->ui.sliderPosDisp2->maximum();
  crntMax[2] = this->ui.sliderPosDisp3->maximum();

  crntValue[0] = this->ui.sliderPosDisp1->value();
  crntValue[1] = this->ui.sliderPosDisp2->value();
  crntValue[2] = this->ui.sliderPosDisp3->value();

  for (auto i = 0; i < 3; i++) {
    const auto newIdx = (i + 1) % 3;
    newMin[i] = crntMin[newIdx];
    newMax[i] = crntMax[newIdx];
    newValue[i] = crntValue[newIdx];
  }

  this->ui.sliderPosDisp1->setMinimum(newMin[0]);
  this->ui.sliderPosDisp2->setMinimum(newMin[1]);
  this->ui.sliderPosDisp3->setMinimum(newMin[2]);

  this->ui.sliderPosDisp1->setMaximum(newMax[0]);
  this->ui.sliderPosDisp2->setMaximum(newMax[1]);
  this->ui.sliderPosDisp3->setMaximum(newMax[2]);

  this->ui.sliderPosDisp1->setValue(newValue[0]);
  this->ui.sliderPosDisp2->setValue(newValue[1]);
  this->ui.sliderPosDisp3->setValue(newValue[2]);

  connect(this->ui.sliderPosDisp1, SIGNAL(valueChanged(int)), this,
          SLOT(SLT_DrawImageWhenSliceChange()));
  connect(this->ui.sliderPosDisp2, SIGNAL(valueChanged(int)), this,
          SLOT(SLT_DrawImageWhenSliceChange()));
  connect(this->ui.sliderPosDisp3, SIGNAL(valueChanged(int)), this,
          SLOT(SLT_DrawImageWhenSliceChange()));
}

void DlgRegistration::updateSliceLabel() const {

  switch (m_enViewArrange) {
  case enViewArrange::AXIAL_FRONTAL_SAGITTAL:
    this->ui.labelDisp1->setText("AXIAL");
    this->ui.labelDisp2->setText("FRONTAL");
    this->ui.labelDisp3->setText("SAGITTAL");
    break;

  case enViewArrange::FRONTAL_SAGITTAL_AXIAL:
    this->ui.labelDisp1->setText("FRONTAL");
    this->ui.labelDisp2->setText("SAGITTAL");
    this->ui.labelDisp3->setText("AXIAL");
    break;

  case enViewArrange::SAGITTAL_AXIAL_FRONTAL:
    this->ui.labelDisp1->setText("SAGITTAL");
    this->ui.labelDisp2->setText("AXIAL");
    this->ui.labelDisp3->setText("FRONTAL");
    break;
  default:
    std::cerr << "WTF!?" << std::endl;
    break;
  }
}

void DlgRegistration::LoadImgFromComboBox(
    const int idx,
    const QString
        &strSelectedComboTxt) // -->when fixed image loaded will be called here!
{

  UShortImageType::Pointer spTmpImg;
  if (strSelectedComboTxt.compare(QString("RAW_CBCT"), Qt::CaseSensitive) ==
      0) {
    spTmpImg = m_cbctregistration->m_pParent->m_spRawReconImg;
  } else if (strSelectedComboTxt.compare(QString("REF_CT"),
                                         Qt::CaseSensitive) == 0) {
    spTmpImg = m_cbctregistration->m_pParent->m_spRefCTImg;
  } else if (strSelectedComboTxt.compare(QString("MANUAL_RIGID_CT"),
                                         Qt::CaseSensitive) == 0) {
    spTmpImg = m_cbctregistration->m_pParent->m_spManualRigidCT;
  } else if (strSelectedComboTxt.compare(QString("AUTO_RIGID_CT"),
                                         Qt::CaseSensitive) == 0) {
    spTmpImg = m_cbctregistration->m_pParent->m_spAutoRigidCT;
  } else if (strSelectedComboTxt.compare(QString("DEFORMED_CT1"),
                                         Qt::CaseSensitive) == 0) {
    spTmpImg = m_cbctregistration->m_pParent->m_spDeformedCT1;
  } else if (strSelectedComboTxt.compare(QString("DEFORMED_CT2"),
                                         Qt::CaseSensitive) == 0) {
    spTmpImg = m_cbctregistration->m_pParent->m_spDeformedCT2;
  } else if (strSelectedComboTxt.compare(QString("DEFORMED_CT3"),
                                         Qt::CaseSensitive) == 0) {
    spTmpImg = m_cbctregistration->m_pParent->m_spDeformedCT3;
  } else if (strSelectedComboTxt.compare(QString("DEFORMED_CT_FINAL"),
                                         Qt::CaseSensitive) == 0) {
    spTmpImg = m_cbctregistration->m_pParent->m_spDeformedCT_Final;
  } else if (strSelectedComboTxt.compare(QString("COR_CBCT"),
                                         Qt::CaseSensitive) == 0) {
    spTmpImg = m_cbctregistration->m_pParent->m_spScatCorrReconImg;
  }

  if (spTmpImg == nullptr) {
    return;
  }

  if (idx == 0) {
    m_spFixed = spTmpImg.GetPointer();

    whenFixedImgLoaded();
  } else if (idx == 1) {
    m_spMoving = spTmpImg.GetPointer();
    whenMovingImgLoaded();
  }

  SLT_DrawImageWhenSliceChange();
}

void DlgRegistration::LoadVOIFromComboBox(int /*idx*/,
                                          QString &strSelectedComboTxt) {

  auto ct_type = ctType::PLAN_CT;
  const auto ct = this->ui.comboBoxImgMoving->currentText().toStdString();
  if (ct == std::string("REF_CT")) {
    // ct_type = PLAN_CT;
  } else if (ct == std::string("AUTO_RIGID_CT")) {
    ct_type = ctType::RIGID_CT;
  } else if (ct == std::string("DEFORMED_CT_FINAL")) {
    ct_type = ctType::DEFORM_CT;
  } else {
    std::cout << "This moving image does not own any VOIs" << std::endl;
    return;
  }

  auto struct_set =
      m_cbctregistration->m_pParent->m_structures->get_ss(ct_type);
  if (struct_set == nullptr) {
    return;
  }
  if (struct_set->slist.empty()) {
    std::cerr << "Structures not initialized yet" << std::endl;
    return;
  }

  for (auto voi : struct_set->slist) {
    if (strSelectedComboTxt.compare(voi.name.c_str()) == 0) {
      m_cbctregistration->cur_voi = std::make_unique<Rtss_roi_modern>(voi);
    }
  }

  SLT_DrawImageWhenSliceChange();
}

// search  for the  main data, if there  is, add  the predefined name to the
// combobox
void DlgRegistration::UpdateListOfComboBox(const int idx) const {
  QComboBox *crntCombo;

  if (idx == 0) {
    crntCombo = this->ui.comboBoxImgFixed;
  } else {
    crntCombo = this->ui.comboBoxImgMoving;
  }

  // remove all the list
  crntCombo->clear();
  const auto p_parent = m_cbctregistration->m_pParent;

  if (p_parent->m_spRawReconImg != nullptr) {
    crntCombo->addItem("RAW_CBCT");
  }

  if (p_parent->m_spRefCTImg != nullptr) {
    crntCombo->addItem("REF_CT");
  }

  if (p_parent->m_spManualRigidCT != nullptr) {
    crntCombo->addItem("MANUAL_RIGID_CT");
  }

  if (p_parent->m_spAutoRigidCT != nullptr) {
    crntCombo->addItem("AUTO_RIGID_CT");
  }

  if (p_parent->m_spDeformedCT1 != nullptr) {
    crntCombo->addItem("DEFORMED_CT1");
  }

  if (p_parent->m_spDeformedCT2 != nullptr) {
    crntCombo->addItem("DEFORMED_CT2");
  }

  if (p_parent->m_spDeformedCT3 != nullptr) {
    crntCombo->addItem("DEFORMED_CT3");
  }

  if (p_parent->m_spDeformedCT_Final != nullptr) {
    crntCombo->addItem("DEFORMED_CT_FINAL");
  }

  if (p_parent->m_spScatCorrReconImg != nullptr) {
    crntCombo->addItem("COR_CBCT");
  }
}

void DlgRegistration::SLT_RestoreImageSingle() const {
  const auto mainWndIdx = 0;
  m_YKDisp[mainWndIdx].SetZoom(1.0);
  m_YKDisp[mainWndIdx].SetOffset(0, 0);

  SLT_DrawImageInFixedSlice();
}

void DlgRegistration::SLT_RestoreImageAll() const {
  for (auto &i : m_cbctregistration->m_YKDisp) {
    i.SetZoom(1.0);
    i.SetOffset(0, 0);

    SLT_DrawImageInFixedSlice();
  }
}
// Auto registration
void DlgRegistration::SLT_DoRegistrationRigid() // plastimatch auto registration
{
  // 1) Save current image files
  if (m_spFixed == nullptr || m_spMoving == nullptr) {
    return;
  }

  // Before the registration, remove background and fill the air-bubble of the
  // cbct  1) calculate manual shift value  PreProcess_CBCT(); // remove skin
  // and fill bubble before registration

  if (m_pParent->m_cbctrecon->m_spRefCTImg == nullptr ||
      m_pParent->m_cbctrecon->m_spManualRigidCT == nullptr) {
    return;
  }

  if (this->ui.checkBoxKeyMoving->isChecked()) {
    SLT_KeyMoving(false);
  }

  /*- Make a synthetic std::vector field according to the translation
        plastimatch synth-vf --fixed [msk_skin.mha] --output
  [xf_manual_trans.mha] --xf-trans "[origin diff (raw - regi)]" plastimatch
  synth-vf --fixed E:\PlastimatchData\DicomEg\OLD\msk_skin.mha --output
  E:\PlastimatchData\DicomEg\OLD\xf_manual_trans.mha --xf-trans "21 -29 1"

        - Move the skin contour according to the std::vector field
        plastimatch warp --input [msk_skin.mha] --output-img
  [msk_skin_manRegi.mha] --xf [xf_manual_trans.mha] plastimatch warp --input
  E:\PlastimatchData\DicomEg\OLD\msk_skin.mha --output-img
  E:\PlastimatchData\DicomEg\OLD\msk_skin_manRegi.mha --xf
  E:\PlastimatchData\DicomEg\OLD\xf_manual_trans.mha*/

  std::cout << "1: writing temporary files" << std::endl;

  // Both image type: Unsigned Short
  auto filePathFixed =
      m_cbctregistration->m_strPathPlastimatch / "fixed_rigid.mha";
  auto filePathMoving =
      m_cbctregistration->m_strPathPlastimatch / "moving_rigid.mha";
  auto filePathOutput =
      m_cbctregistration->m_strPathPlastimatch / "output_rigid.mha";
  const auto filePathXform =
      m_cbctregistration->m_strPathPlastimatch / "xform_rigid.txt";
  auto filePathROI = m_cbctregistration->m_strPathPlastimatch /
                     "fixed_roi_rigid.mha"; // optional

  using writerType = itk::ImageFileWriter<UShortImageType>;

  auto writer1 = writerType::New();
  writer1->SetFileName(filePathFixed.string());
  writer1->SetUseCompression(true);
  writer1->SetInput(m_spFixed);

  auto writer2 = writerType::New();
  writer2->SetFileName(filePathMoving.string());
  writer2->SetUseCompression(true);
  writer2->SetInput(m_spMoving);

  writer1->Update();
  writer2->Update();

  if (this->ui.checkBoxUseROIForRigid->isChecked()) {
    std::cout << "Creating a ROI mask for Rigid registration " << std::endl;
    auto strFOVGeom = this->ui.lineEditFOVPos->text();

    auto strListFOV = strFOVGeom.split(",");
    if (strListFOV.count() == 3) {
      const auto FOV_DcmPosX = strListFOV.at(0).toFloat(); // mm
      const auto FOV_DcmPosY = strListFOV.at(1).toFloat();
      const auto FOV_Radius = strListFOV.at(2).toFloat();

      // Create Image using FixedImage sp

      // Image Pointer here
      UShortImageType::Pointer spRoiMask;
      crl::AllocateByRef<UShortImageType, UShortImageType>(m_spFixed,
                                                           spRoiMask);
      m_pParent->m_cbctrecon->GenerateCylinderMask(spRoiMask, FOV_DcmPosX,
                                                   FOV_DcmPosY, FOV_Radius);

      auto writer3 = writerType::New();
      writer3->SetFileName(filePathROI.string());
      writer3->SetUseCompression(true);
      writer3->SetInput(spRoiMask);
      writer3->Update();
    }
    std::cout << "2.A:  ROI-based Rigid body registration will be done"
              << std::endl;
  } else {
    filePathROI = fs::path{};
  }

  // Preprocessing
  // 2) move CT-based skin mask on CBCT based on manual shift
  //  if (m_strPathCTSkin)

  // m_strPathCTSkin is prepared after PreProcessCT()

  std::cout << "2: Creating a plastimatch command file" << std::endl;

  const auto fnCmdRegisterRigid = "cmd_register_rigid.txt";
  // QString fnCmdRegisterDeform = "cmd_register_deform.txt";
  auto pathCmdRegister =
      m_cbctregistration->m_strPathPlastimatch / fnCmdRegisterRigid;

  const auto strDummy = ""s;

  const auto mse = this->ui.radioButton_mse->isChecked();
  const auto cuda = m_pParent->ui.radioButton_UseCUDA->isChecked();
  auto GradOptionStr = this->ui.lineEditGradOption->text().toStdString();
  // For Cropped patients, FOV mask is applied.

  m_cbctregistration->GenPlastiRegisterCommandFile(
      pathCmdRegister, filePathFixed, filePathMoving, filePathOutput,
      filePathXform, enRegisterOption::PLAST_RIGID, strDummy, strDummy,
      strDummy, filePathROI, mse, cuda, GradOptionStr);

  m_cbctregistration->CallingPLMCommand(pathCmdRegister);

  std::cout << "5: Reading output image-CT" << std::endl;

  auto reader = itk::ImageFileReader<UShortImageType>::New();
  reader->SetFileName(filePathOutput.string());
  reader->Update();
  m_pParent->m_cbctrecon->m_spAutoRigidCT = reader->GetOutput();

  std::cout << "6: Reading is completed" << std::endl;

  // If rigid structure already exists copy it to plan-ct structure before
  // applying rigid again.
  if (!m_cbctregistration->m_pParent->m_structures
           ->is_ss_null<ctType::RIGID_CT>()) {
    auto rigid_ss = std::make_unique<Rtss_modern>(*(
        m_cbctregistration->m_pParent->m_structures->get_ss(ctType::RIGID_CT)));
    m_cbctregistration->m_pParent->m_structures->set_planCT_ss(
        std::move(rigid_ss));
  }

  const auto transform_success =
      m_cbctregistration->m_pParent->m_structures
          ->ApplyTransformTo<ctType::PLAN_CT>(filePathXform);
  if (transform_success) {
    std::cout << "7: Contours registered" << std::endl;
  }

  UpdateListOfComboBox(0); // combo selection signalis called
  UpdateListOfComboBox(1);

  SelectComboExternal(
      0, enREGI_IMAGES::REGISTER_RAW_CBCT); // will call fixedImageSelected
  SelectComboExternal(1, enREGI_IMAGES::REGISTER_AUTO_RIGID);

  m_cbctregistration->m_strPathXFAutoRigid = filePathXform; // for further use
}

bool DlgRegistration::eventFilter(QObject *target, QEvent *event) {
  if (event->type() == QEvent::Paint) { // eventy->type() = 12 if paint event
    return false;
  }

  if (event->type() == QEvent::ShortcutOverride) // 51
  {
    if (!this->ui.checkBoxKeyMoving->isChecked()) {
      return false;
    }

    auto *keyEvent = dynamic_cast<QKeyEvent *>(event);
    if (keyEvent == nullptr) {
      return false;
    }
    const auto iArrowKey = static_cast<int>(keyEvent->nativeVirtualKey());
    // std::cout << iArrowKey << std::endl;
    const auto resol = this->ui.lineEditMovingResol->text().toDouble(); // mm

    switch (iArrowKey) {
      // case Qt::Key_Left: //37
    case 37: // 37
      ImageManualMove(37, resol);
      break;
    case 38:                      // 38
      ImageManualMove(38, resol); // up
      break;
    case 39: // 39
      ImageManualMove(39, resol);
      break;
    case 40: // 40
      ImageManualMove(40, resol);
      break;
    case 33: //
      ImageManualMove(33, resol);
      break;
    case 34: //
      ImageManualMove(34, resol);
      break;
    default:
      break; // do nothing
    }
    // std::cout << keyEvent->->nativeVirtualKey() << std::endl;
    // event->accept();
    return true; // No more event transfering?
  }
  /*else if (event->type() == QEvent::KeyPress)
  {
        std::cout << "key pressed" << std::endl;
  }
  else if (event->type() == QEvent::KeyRelease)
  {
        std::cout << "key Released" << std::endl;
  }*/

  // return true;
  return QDialog::eventFilter(target,
                              event); // This is mandatory to deliver
                                      // the event signal to the children
                                      // components
}

void DlgRegistration::childEvent(QChildEvent *event) {
  // std::cout << "childEvent Called" << std::endl;
  if (event->added()) {
    event->child()->installEventFilter(this);
  }
}

void DlgRegistration::ImageManualMove(const int direction, const double resol) {
  if (m_spMoving == nullptr) {
    return;
  }

  // USHORT_ImageType::SizeType imgSize =
  // m_spMoving->GetRequestedRegion().GetSize(); //1016x1016 x z
  auto imgOrigin = m_spMoving->GetOrigin();
  // USHORT_ImageType::SpacingType imgSpacing = m_spFixed->GetSpacing();

  if (direction == 37) {                 // LEFT
    imgOrigin[0] = imgOrigin[0] - resol; // mm unit
  } else if (direction == 39) {          // RIGHT
    imgOrigin[0] = imgOrigin[0] + resol;
  } else if (direction == 38) { // Up
    imgOrigin[1] = imgOrigin[1] - resol;
  } else if (direction == 40) { // DOWN
    imgOrigin[1] = imgOrigin[1] + resol;
  } else if (direction == 33) { // PageUp
    imgOrigin[2] = imgOrigin[2] + resol;
  } else if (direction == 34) { // PageDown
    imgOrigin[2] = imgOrigin[2] - resol;
  }

  m_spMoving->SetOrigin(imgOrigin);

  SLT_DrawImageWhenSliceChange();

  // Display relative movement
  // Starting point? RefCT image
  // Only Valid when Moving image is the ManualMove
  auto imgOriginRef = m_pParent->m_cbctrecon->m_spRefCTImg->GetOrigin();

  QString strDelta = to_qstr(
      "delta(mm): " + crl::make_sep_str<','>(imgOrigin[0] - imgOriginRef[0],
                                             imgOrigin[1] - imgOriginRef[1],
                                             imgOrigin[2] - imgOriginRef[2]));
  this->ui.lineEditOriginChanged->setText(strDelta);

  // std::cout << "direction  " << direction << std::endl;
  // std::cout << "resolution " << resol << std::endl;
}

// change origin of moving image by shift value acquired from gradient
// registration
void DlgRegistration::ImageManualMoveOneShot(
    const float shiftX, const float shiftY,
    const float shiftZ) // DICOM coordinate
{
  if (m_spMoving == nullptr) {
    return;
  }

  // USHORT_ImageType::SizeType imgSize =
  // m_spMoving->GetRequestedRegion().GetSize(); //1016x1016 x z
  using USPointType = UShortImageType::PointType;
  auto imgOrigin = m_spMoving->GetOrigin();
  // USHORT_ImageType::SpacingType imgSpacing = m_spFixed->GetSpacing();
  imgOrigin[0] = imgOrigin[0] - static_cast<USPointType::ValueType>(shiftX);
  imgOrigin[1] = imgOrigin[1] - static_cast<USPointType::ValueType>(shiftY);
  imgOrigin[2] = imgOrigin[2] - static_cast<USPointType::ValueType>(shiftZ);

  m_spMoving->SetOrigin(imgOrigin);

  SLT_DrawImageWhenSliceChange();

  // Display relative movement
  // Starting point? RefCT image
  // Only Valid when Moving image is the ManualMove
  auto imgOriginRef = m_pParent->m_cbctrecon->m_spRefCTImg->GetOrigin();

  QString strDelta = to_qstr(
      "delta(mm): " + crl::make_sep_str<','>(imgOrigin[0] - imgOriginRef[0],
                                             imgOrigin[1] - imgOriginRef[1],
                                             imgOrigin[2] - imgOriginRef[2]));
  this->ui.lineEditOriginChanged->setText(strDelta);
}

// Bring Focus
void DlgRegistration::SLT_BringFocusToEnableArrow(const bool bChecked) const {
  if (bChecked) {
    this->ui.labelDisp1->setFocus(); // if focus is in label, Key Event will
                                     // trigger 51 (override)
  }
}

void DlgRegistration::SLT_KeyMoving(const bool bChecked) // Key Moving check box
{
  this->ui.lineEditMovingResol->setDisabled(bChecked);
  if (bChecked) {
    SelectComboExternal(1,
                        enREGI_IMAGES::REGISTER_MANUAL_RIGID); // should be
  }
  this->ui.comboBoxImgFixed->setDisabled(bChecked);
  this->ui.comboBoxImgMoving->setDisabled(bChecked);
  this->ui.pushButtonRestoreOriginal->setDisabled(bChecked);
}

QComboBox *&DlgRegistration::get_combo_box_FixMov(const int comboIdx) {
  if (comboIdx == 0) {
    return this->ui.comboBoxImgFixed;
  }
  return this->ui.comboBoxImgMoving;
}

void DlgRegistration::AddImageToCombo(const int comboIdx,
                                      const enREGI_IMAGES option)
// comboIdx 0: fixed, 1: moving
{
  auto &combo_box = get_combo_box_FixMov(comboIdx);

  switch (option) {
  case enREGI_IMAGES::REGISTER_RAW_CBCT:
    if (m_pParent->m_cbctrecon->m_spRawReconImg != nullptr) {
      combo_box->addItem("RAW_CBCT");
    }
    break;
  case enREGI_IMAGES::REGISTER_REF_CT:
    if (m_pParent->m_cbctrecon->m_spRefCTImg != nullptr) {
      combo_box->addItem("REF_CT");
    }
    break;
  case enREGI_IMAGES::REGISTER_MANUAL_RIGID:
    if (m_pParent->m_cbctrecon->m_spManualRigidCT != nullptr) {
      combo_box->addItem("MANUAL_RIGID_CT");
    }
    break;
  case enREGI_IMAGES::REGISTER_AUTO_RIGID:
    if (m_pParent->m_cbctrecon->m_spAutoRigidCT != nullptr) {
      combo_box->addItem("AUTO_RIGID_CT");
    }
    break;
  case enREGI_IMAGES::REGISTER_DEFORM1:
    if (m_pParent->m_cbctrecon->m_spDeformedCT1 != nullptr) {
      combo_box->addItem("DEFORMED_CT1");
    }
    break;
  case enREGI_IMAGES::REGISTER_DEFORM2:
    if (m_pParent->m_cbctrecon->m_spDeformedCT2 != nullptr) {
      combo_box->addItem("DEFORMED_CT2");
    }
    break;
  case enREGI_IMAGES::REGISTER_DEFORM3:
    if (m_pParent->m_cbctrecon->m_spDeformedCT3 != nullptr) {
      combo_box->addItem("DEFORMED_CT3");
    }
    break;

  case enREGI_IMAGES::REGISTER_DEFORM_FINAL:
    if (m_pParent->m_cbctrecon->m_spDeformedCT_Final != nullptr) {
      combo_box->addItem("DEFORMED_CT_FINAL");
    }
    break;
  case enREGI_IMAGES::REGISTER_COR_CBCT:
    if (m_pParent->m_cbctrecon->m_spScatCorrReconImg != nullptr) {
      combo_box->addItem("COR_CBCT");
    }
    break;
  case enREGI_IMAGES::REGISTER_DEFORM_SKIP_AUTORIGID:
    if (m_pParent->m_cbctrecon->m_spScatCorrReconImg != nullptr) {
      combo_box->addItem("REGISTER_DEFORM_SKIP_AUTORIGID");
    }
    break;
  }
}

// externally change  combo box value
void DlgRegistration::SelectComboExternal(const int idx,
                                          const enREGI_IMAGES iImage) {
  QComboBox *crntCombo;

  if (idx == 0) {
    crntCombo = this->ui.comboBoxImgFixed;
  } else if (idx == 1) {
    crntCombo = this->ui.comboBoxImgMoving;
  } else {
    std::cerr << "What did you do to get here?" << std::endl;
    return;
  }

  auto findIndx = -1;
  switch (iImage) {
  case enREGI_IMAGES::REGISTER_RAW_CBCT:
    findIndx = crntCombo->findText("RAW_CBCT");
    break;
  case enREGI_IMAGES::REGISTER_REF_CT:
    findIndx = crntCombo->findText("REF_CT");
    break;
  case enREGI_IMAGES::REGISTER_MANUAL_RIGID:
    findIndx = crntCombo->findText("MANUAL_RIGID_CT");
    break;
  case enREGI_IMAGES::REGISTER_AUTO_RIGID:
    findIndx = crntCombo->findText("AUTO_RIGID_CT");
    break;
  case enREGI_IMAGES::REGISTER_DEFORM1:
    findIndx = crntCombo->findText("DEFORMED_CT1");
    break;
  case enREGI_IMAGES::REGISTER_DEFORM2:
    findIndx = crntCombo->findText("DEFORMED_CT2");
    break;
  case enREGI_IMAGES::REGISTER_DEFORM3:
    findIndx = crntCombo->findText("DEFORMED_CT3");
    break;
  case enREGI_IMAGES::REGISTER_DEFORM_FINAL:
    findIndx = crntCombo->findText("DEFORMED_CT_FINAL");
    break;
  case enREGI_IMAGES::REGISTER_COR_CBCT:
    findIndx = crntCombo->findText("COR_CBCT");
    break;
  case enREGI_IMAGES::REGISTER_DEFORM_SKIP_AUTORIGID:
    findIndx = crntCombo->findText("REGISTER_DEFORM_SKIP_AUTORIGID");
    break;
  }
  // std::cout << "setCurrentIndx " << findIndx << std::endl;

  if (findIndx < 0) { //-1 if not found
    return;
  }

  crntCombo->setCurrentIndex(findIndx); // will call "SLT_FixedImageSelected"

  // Below are actually redendency. don't know why setCurrentIndex sometimes
  // doesn't trigger the SLT_MovingImageSelected slot func.

  const auto crntStr = crntCombo->currentText();
  if (idx == 0) {
    SLT_FixedImageSelected(crntStr);
  } else if (idx == 1) {
    SLT_MovingImageSelected(crntStr);
  }
  // If it is called inside the Dlg, SLT seemed not to conneced
}

void DlgRegistration::SLT_FixedImageSelected(QString selText) {
  // QString strCrntText = this->ui.comboBoxImgFixed->currentText();
  LoadImgFromComboBox(
      0, selText); // here, m_spMoving and Fixed images are determined
}

ctType get_ctType(const QString &selText) {
  if (selText.compare("REF_CT") == 0) {
    return ctType::PLAN_CT;
  }
  if (selText.compare("MANUAL_RIGID_CT") == 0 ||
      selText.compare("AUTO_RIGID_CT") == 0) {
    return ctType::RIGID_CT;
  }
  if (selText.compare("DEFORMED_CT1") == 0 ||
      selText.compare("DEFORMED_CT2") == 0 ||
      selText.compare("DEFORMED_CT3") == 0 ||
      selText.compare("DEFORMED_CT_FINAL") == 0) {
    return ctType::DEFORM_CT;
  }
  return ctType::PLAN_CT;
}

void DlgRegistration::SLT_MovingImageSelected(QString selText) {
  // QString strCrntText = this->ui.comboBoxImgMoving->currentText();
  // std::cout << "SLT_MovingImageSelected" << std::endl;
  LoadImgFromComboBox(1, selText);
  const auto cur_ct = get_ctType(selText);
  UpdateVOICombobox(cur_ct);
}

void DlgRegistration::UpdateVOICombobox(const ctType ct_type) const {
  auto struct_set =
      m_cbctregistration->m_pParent->m_structures->get_ss(ct_type);
  if (struct_set == nullptr) {
    return;
  }
  if (struct_set->slist.empty()) {
    std::cerr << "Structures not initialized yet" << std::endl;
    return;
  }
  this->ui.comboBox_VOI->clear();
  for (const auto &voi : struct_set->slist) {
    this->ui.comboBox_VOI->addItem(QString(voi.name.c_str()));
    this->ui.comboBox_VOItoCropBy->addItem(QString(voi.name.c_str()));
    this->ui.comboBox_VOItoCropBy_copy->addItem(QString(voi.name.c_str()));
  }
}

void DlgRegistration::SLT_RestoreMovingImg() {
  m_cbctregistration->m_pParent->RegisterImgDuplication(
      enREGI_IMAGES::REGISTER_REF_CT, enREGI_IMAGES::REGISTER_MANUAL_RIGID);
  this->ui.lineEditOriginChanged->setText(QString());

  SelectComboExternal(1, enREGI_IMAGES::REGISTER_MANUAL_RIGID);
  this->ui.pushButtonConfirmManualRegi->setDisabled(false);
}

void DlgRegistration::SLT_PreProcessCT() {
  if (!this->ui.checkBoxCropBkgroundCT->isChecked()) {
    std::cout << "Preprocessing is not selected." << std::endl;
    return;
  }

  const auto iAirThresholdShort = this->ui.spinBoxBkDetectCT->value();

  if (this->ui.comboBox_VOItoCropBy->count() < 1) {
    std::cout
        << "Reference CT DIR should be specified for structure based cropping"
        << std::endl;
    if (m_spMoving == nullptr || m_spFixed == nullptr) {
      return;
    }
    const auto fixed_size = m_spFixed->GetLargestPossibleRegion().GetSize();
    const auto moving_size = m_spMoving->GetLargestPossibleRegion().GetSize();
    if (fixed_size[0] != moving_size[0] || fixed_size[1] != moving_size[1] ||
        fixed_size[2] != moving_size[2]) {
      std::cout
          << "Fixed and moving image is not the same size, consider using "
             "a platimatch registration to solve this."
          << std::endl;
      return;
    }

    const auto reply =
        QMessageBox::question(this, "No reference structures found!",
                              "Do you wan't to attempt an auto correction "
                              "of air and excessive circumference?",
                              QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
      std::cout << "Attempting automatic air filling and skin cropping..."
                << std::endl;
      m_cbctregistration->autoPreprocessCT(iAirThresholdShort, m_spFixed,
                                           m_spMoving);
    }
    return;
  }

  const auto strRSName =
      this->ui.comboBox_VOItoCropBy->currentText().toStdString();
  const auto fill_bubble = this->ui.checkBoxFillBubbleCT->isChecked();
  const auto iBubbleFillingVal =
      this->ui.spinBoxBubFillCT->value(); // 1000 = soft tissue
  const auto iAirFillValShort = this->ui.spinBoxBkFillCT->value(); // 0 = air

  const auto cur_ct_text = ui.comboBoxImgMoving->currentText();
  const auto cur_ct = get_ctType(cur_ct_text);
  const auto &rt_structs =
      m_cbctregistration->m_pParent->m_structures->get_ss(cur_ct);

  QString image_str;
  if (ui.comboBoxImToCropFill->currentText().compare("Moving") == 0) {
    image_str = ui.comboBoxImgMoving->currentText();
  } else {
    image_str = ui.comboBoxImgFixed->currentText();
  }

  auto &image =
      m_cbctregistration->get_image_from_combotext(image_str.toStdString());

  if (!m_cbctregistration->PreprocessCT(image, iAirThresholdShort, rt_structs,
                                        strRSName, fill_bubble,
                                        iBubbleFillingVal, iAirFillValShort)) {
    std::cout
        << "Error in PreprocessCT!!!scatter correction would not work out."
        << std::endl;
    m_cbctregistration->m_pParent->m_bMacroContinue = false;
  }

  show();

  UpdateListOfComboBox(0); // combo selection signalis called
  UpdateListOfComboBox(1);
  // if not found, just skip
  SelectComboExternal(
      0, enREGI_IMAGES::REGISTER_RAW_CBCT); // will call fixedImageSelected
  SelectComboExternal(1, enREGI_IMAGES::REGISTER_MANUAL_RIGID);
  SLT_DrawImageWhenSliceChange();

  std::cout << "FINISHED!: Pre-processing of CT image" << std::endl;

  ////Load DICOM plan
  if (m_cbctregistration->m_pParent->m_strPathPlan.empty()) {
    std::cout << "No DCM plan file was found. Skipping dcm plan." << std::endl;
    return;
  }
  // QString dcmplanPath = m_pParent->m_strPathPlan;
  m_cbctregistration->LoadRTPlan(
      m_cbctregistration->m_pParent->m_strPathPlan); // fill RT_studyplan
}

void DlgRegistration::SLT_DoRegistrationDeform() {
  if (m_spFixed == nullptr || m_spMoving == nullptr) {
    return;
  }

  const auto bPrepareMaskOnly = !this->ui.checkBoxCropBkgroundCT->isChecked();

  std::cout << "0: DoRegistrationDeform: writing temporary files" << std::endl;

  // Both image type: Unsigned Short
  auto filePathFixed =
      m_cbctregistration->m_strPathPlastimatch / "fixed_deform.mha";
  auto filePathMoving =
      m_cbctregistration->m_strPathPlastimatch / "moving_deform.mha";
  auto filePathROI = m_cbctregistration->m_strPathPlastimatch /
                     "fixed_roi_DIR.mha"; // optional

  const auto filePathOutput =
      m_cbctregistration->m_strPathPlastimatch / "output_deform.mha";
  const auto filePathXform =
      m_cbctregistration->m_strPathPlastimatch / "xform_deform.txt";

  auto filePathOutputStage1 =
      m_cbctregistration->m_strPathPlastimatch / "output_deform_stage1.mha";
  auto filePathOutputStage2 =
      m_cbctregistration->m_strPathPlastimatch / "output_deform_stage2.mha";
  auto filePathOutputStage3 =
      m_cbctregistration->m_strPathPlastimatch / "output_deform_stage3.mha";

  using writerType = itk::ImageFileWriter<UShortImageType>;

  auto writer1 = writerType::New();
  writer1->SetFileName(filePathFixed.string());
  writer1->SetUseCompression(true);
  writer1->SetInput(m_spFixed);

  auto writer2 = writerType::New();
  writer2->SetFileName(filePathMoving.string());
  writer2->SetUseCompression(true);
  writer2->SetInput(m_spMoving);
  writer1->Update();
  writer2->Update();

  // Create a mask image based on the fixed sp image

  if (this->ui.checkBoxUseROIForDIR->isChecked()) {
    std::cout << "Creating a ROI mask for DIR.. " << std::endl;
    auto strFOVGeom = this->ui.lineEditFOVPos->text();

    auto strListFOV = strFOVGeom.split(",");
    if (strListFOV.count() == 3) {
      const auto FOV_DcmPosX = strListFOV.at(0).toFloat(); // mm
      const auto FOV_DcmPosY = strListFOV.at(1).toFloat();
      const auto FOV_Radius = strListFOV.at(2).toFloat();

      // Create Image using FixedImage sp

      // Image Pointer here
      UShortImageType::Pointer spRoiMask;
      crl::AllocateByRef<UShortImageType, UShortImageType>(m_spFixed,
                                                           spRoiMask);
      m_cbctregistration->m_pParent->GenerateCylinderMask(
          spRoiMask, FOV_DcmPosX, FOV_DcmPosY, FOV_Radius);

      auto writer3 = writerType::New();
      writer3->SetFileName(filePathROI.string());
      writer3->SetUseCompression(true);
      writer3->SetInput(spRoiMask);
      writer3->Update();
    }
  }

  auto filePathFixed_proc = filePathFixed;

  std::cout << "1: DoRegistrationDeform: CBCT pre-processing before deformable "
               "registration"
            << std::endl;
  // std::cout << "Air region and bubble will be removed" << std::endl;

  auto info1(m_cbctregistration->m_strPathCTSkin_autoRegi);
  auto info2(m_cbctregistration->m_strPathXFAutoRigid);

  if (!fs::exists(info1) || !fs::exists(info2)) {
    std::cout << "Fatal error! no CT skin is found or no XF auto file found. "
                 "Preprocessing will not be done. Proceeding."
              << std::endl;
  } else {
    filePathFixed_proc =
        m_cbctregistration->m_strPathPlastimatch / "fixed_deform_proc.mha";
    // skin removal and bubble filling : output file = filePathFixed_proc
    const auto bBubbleRemoval = this->ui.checkBoxFillBubbleCT->isChecked();
    const auto skinExp = this->ui.lineEditCBCTSkinCropBfDIR->text().toDouble();

    const auto iBubThresholdUshort = this->ui.spinBoxBkDetectCT->value(); // 500
    const auto iBubFillUshort = this->ui.spinBoxBubFillCT->value(); // 1000

    m_cbctregistration->ProcessCBCT_beforeDeformRegi(
        filePathFixed, m_cbctregistration->m_strPathCTSkin_autoRegi,
        filePathFixed_proc, m_cbctregistration->m_strPathXFAutoRigid,
        bBubbleRemoval, bPrepareMaskOnly, skinExp, iBubThresholdUshort,
        iBubFillUshort); // bubble filling yes

    if (bPrepareMaskOnly) {
      filePathFixed_proc = filePathFixed;
    }
  }

  std::cout << "2: DoRegistrationDeform: Creating a plastimatch command file"
            << std::endl;

  const auto fnCmdRegisterRigid = "cmd_register_deform.txt"s;
  // QString fnCmdRegisterDeform = "cmd_register_deform.txt";
  auto pathCmdRegister =
      m_cbctregistration->m_strPathPlastimatch / fnCmdRegisterRigid;

  auto strDeformableStage1 =
      this->ui.lineEditArgument1->text()
          .toStdString(); // original param: 7, add output path
  auto strDeformableStage2 = this->ui.lineEditArgument2->text().toStdString();
  auto strDeformableStage3 = this->ui.lineEditArgument3->text().toStdString();

  strDeformableStage1.append(", ").append(filePathOutputStage1.string());
  strDeformableStage2.append(", ").append(filePathOutputStage2.string());
  strDeformableStage3.append(", ").append(filePathOutputStage3.string());

  const auto mse = this->ui.radioButton_mse->isChecked();
  const auto cuda = m_pParent->ui.radioButton_UseCUDA->isChecked();
  auto GradOptionStr = this->ui.lineEditGradOption->text().toStdString();
  // For Cropped patients, FOV mask is applied.
  if (this->ui.checkBoxUseROIForDIR->isChecked()) {
    m_cbctregistration->GenPlastiRegisterCommandFile(
        pathCmdRegister, filePathFixed_proc, filePathMoving, filePathOutput,
        filePathXform, enRegisterOption::PLAST_BSPLINE, strDeformableStage1,
        strDeformableStage2, strDeformableStage3, filePathROI, mse, cuda,
        GradOptionStr);
  } else {
    m_cbctregistration->GenPlastiRegisterCommandFile(
        pathCmdRegister, filePathFixed_proc, filePathMoving, filePathOutput,
        filePathXform, enRegisterOption::PLAST_BSPLINE, strDeformableStage1,
        strDeformableStage2, strDeformableStage3, ""s, mse, cuda,
        GradOptionStr);
  }

  m_cbctregistration->CallingPLMCommand(pathCmdRegister);

  std::cout << "5: DoRegistrationDeform: Registration is done" << std::endl;
  std::cout << "6: DoRegistrationDeform: Reading output image" << std::endl;

  using readerType = itk::ImageFileReader<UShortImageType>;

  auto readerDefSt1 = readerType::New();

  auto tmpFileInfo = filePathOutputStage1;
  if (fs::exists(tmpFileInfo)) {
    readerDefSt1->SetFileName(filePathOutputStage1.string());
    readerDefSt1->Update();
    m_cbctregistration->m_pParent->m_spDeformedCT1 = readerDefSt1->GetOutput();
  }

  auto readerDefSt2 = readerType::New();
  tmpFileInfo = filePathOutputStage2;
  if (fs::exists(tmpFileInfo)) {
    readerDefSt2->SetFileName(filePathOutputStage2.string());
    readerDefSt2->Update();
    m_cbctregistration->m_pParent->m_spDeformedCT2 = readerDefSt2->GetOutput();
  }

  auto readerDefSt3 = readerType::New();
  tmpFileInfo = filePathOutputStage3;
  if (fs::exists(tmpFileInfo)) {
    readerDefSt3->SetFileName(filePathOutputStage3.string());
    readerDefSt3->Update();
    m_cbctregistration->m_pParent->m_spDeformedCT3 = readerDefSt3->GetOutput();
  }

  auto readerFinCBCT = readerType::New();
  tmpFileInfo = filePathFixed_proc; // cropped image Or not
  if (fs::exists(tmpFileInfo)) {
    readerFinCBCT->SetFileName(filePathFixed_proc.string());
    readerFinCBCT->Update();
    m_cbctregistration->m_pParent->m_spRawReconImg = readerFinCBCT->GetOutput();
  } else {
    std::cout << "No filePathFixed_proc is available. Exit the function"
              << std::endl;
    return;
  }

  // Puncturing should be done for final Deform image

  auto strPathDeformCTFinal = filePathOutput;

  auto tmpBubFileInfo = m_cbctregistration->m_strPathMskCBCTBubble;

  if (this->ui.checkBoxFillBubbleCT->isChecked() &&
      fs::exists(tmpBubFileInfo)) {
    std::cout << "6B: final puncturing according to the CBCT bubble"
              << std::endl;

    /*Mask_parms parms_fill;
    strPathDeformCTFinal = m_strPathPlastimatch + "/deformCTpuncFin.mha";

    parms_fill.mask_operation = MASK_OPERATION_FILL;
    parms_fill.input_fn = filePathOutput.toLocal8Bit().constData();
    parms_fill.mask_fn = m_strPathMskCBCTBubble.toLocal8Bit().constData();
    parms_fill.output_fn = strPathDeformCTFinal.toLocal8Bit().constData();*/

    strPathDeformCTFinal =
        m_cbctregistration->m_strPathPlastimatch / "deformCTpuncFin.mha";

    const auto enMaskOp = MASK_OPERATION_FILL;
    auto input_fn = filePathOutput;
    auto mask_fn = m_cbctregistration->m_strPathMskCBCTBubble;
    auto output_fn = strPathDeformCTFinal;

    // int iBubblePunctureVal = this->ui.lineEditBkFillCT->text().toInt(); //0 =
    // soft tissue
    const auto iBubblePunctureVal =
        0; // 0 = air. deformed CT is now already a USHORT image
    const auto mask_value = iBubblePunctureVal;
    m_cbctregistration->plm_mask_main(enMaskOp, input_fn, mask_fn, output_fn,
                                      static_cast<float>(mask_value));
  }

  auto readerDeformFinal = readerType::New();
  tmpFileInfo = strPathDeformCTFinal;
  if (fs::exists(tmpFileInfo)) {
    readerDeformFinal->SetFileName(strPathDeformCTFinal.string());
    readerDeformFinal->Update();
    m_cbctregistration->m_pParent->m_spDeformedCT_Final =
        readerDeformFinal->GetOutput();
  } else {
    std::cout << "No final output is available. Exit the function" << std::endl;
    return;
  }

  std::cout << "7: DoRegistrationDeform: Reading is completed" << std::endl;

  const auto xform_file = filePathXform;
  m_cbctregistration->m_pParent->m_structures
      ->ApplyTransformTo<ctType::RIGID_CT>(xform_file);

  std::cout << "8: Contours deformed" << std::endl;

  UpdateListOfComboBox(0); // combo selection signalis called
  UpdateListOfComboBox(1);

  SelectComboExternal(
      0, enREGI_IMAGES::REGISTER_RAW_CBCT); // will call fixedImageSelected
  SelectComboExternal(1, enREGI_IMAGES::REGISTER_DEFORM_FINAL);

  std::cout << "FINISHED!: Deformable image registration. Proceed to scatter "
               "correction"
            << std::endl;
}

void DlgRegistration::SLT_PassFixedImgForAnalysis() {
  if (m_spFixed != nullptr) {
    auto cur_fixed = this->ui.comboBoxImgFixed->currentText();
    m_pParent->UpdateReconImage(m_spFixed, cur_fixed);
  }
}

void DlgRegistration::SLT_PassMovingImgForAnalysis() {
  if (m_spMoving != nullptr) {
    auto cur_moving = this->ui.comboBoxImgMoving->currentText();
    m_pParent->UpdateReconImage(m_spMoving, cur_moving);
  }
}

void DlgRegistration::SLT_DoLowerMaskIntensity() {
  if (!this->ui.checkBoxRemoveMaskAfterCor->isChecked()) {
    std::cout << "Error. this function is not enabled" << std::endl;
    return;
  }

  const auto iDiffThreshold = this->ui.lineEditRawCorThre->text().toInt();

  const auto iNoTouchThreshold =
      this->ui.lineEditiNoTouchThreshold->text().toInt();

  const auto fInnerMargin = this->ui.lineEditThermoInner->text().toDouble();
  const auto fOuterMargin = this->ui.lineEditThermoOuter->text().toDouble();

  m_cbctregistration->ThermoMaskRemovingCBCT(m_spFixed, m_spMoving,
                                             iDiffThreshold, iNoTouchThreshold,
                                             fInnerMargin, fOuterMargin);

  UpdateListOfComboBox(0); // combo selection signalis called
  UpdateListOfComboBox(1);
  SelectComboExternal(
      0, enREGI_IMAGES::REGISTER_RAW_CBCT); // will call fixedImageSelected
  SelectComboExternal(1, enREGI_IMAGES::REGISTER_COR_CBCT);
}

void DlgRegistration::SLT_ExchangeRawRef() {}

void DlgRegistration::SLT_ManualMoveByDCMPlan() {
  if (m_spFixed == nullptr || m_spMoving == nullptr) {
    return;
  }

  const auto final_iso_pos = m_cbctregistration->ManualMoveByDCM();

  if (final_iso_pos == nullptr) {
    std::cout << "Error!  No isocenter position was found. " << std::endl;
    return;
  }

  ImageManualMoveOneShot(final_iso_pos[0], final_iso_pos[1], final_iso_pos[2]);

  UpdateListOfComboBox(0); // combo selection signalis called
  UpdateListOfComboBox(1);

  SelectComboExternal(
      0, enREGI_IMAGES::REGISTER_RAW_CBCT); // will call fixedImageSelected
  SelectComboExternal(1, enREGI_IMAGES::REGISTER_MANUAL_RIGID);
}

void DlgRegistration::SLT_ManualMoveByDCMPlanOpen() {
  const auto p_parent = m_cbctregistration->m_pParent;
  auto filePath = QFileDialog::getOpenFileName(
      this, "Open DCMRT Plan file", to_qstr(p_parent->m_strPathDirDefault),
      "DCMRT Plan (*.dcm)");

  if (filePath.length() < 1) {
    return;
  }

  const auto planIso =
      m_cbctregistration->GetIsocenterDCM_FromRTPlan(to_path(filePath));

  if (fabs(planIso.x) < 0.001 && fabs(planIso.y) < 0.001 &&
      fabs(planIso.z) < 0.001) {
    std::cout
        << "Warning!!!! Plan iso is 0 0 0. Most likely not processed properly"
        << std::endl;
  } else {
    std::cout << "isocenter was found: " << planIso.x << ", " << planIso.y
              << ", " << planIso.z << std::endl;
  }

  if (m_spFixed == nullptr || m_spMoving == nullptr) {
    return;
  }

  if (p_parent->m_spRefCTImg == nullptr ||
      p_parent->m_spManualRigidCT == nullptr) {
    return;
  }

  ImageManualMoveOneShot(static_cast<float>(planIso.x),
                         static_cast<float>(planIso.y),
                         static_cast<float>(planIso.z));

  /* Should be done in confirm manual!
  const auto trn_vec =
      FloatVector{static_cast<float>(planIso.x), static_cast<float>(planIso.y),
                  static_cast<float>(planIso.z)};
  auto &structs = m_cbctregistration->m_pParent->m_structures;
  structs->ApplyVectorTransform_InPlace<PLAN_CT>(trn_vec);
  */

  UpdateListOfComboBox(0); // combo selection signalis called
  UpdateListOfComboBox(1);

  SelectComboExternal(
      0, enREGI_IMAGES::REGISTER_RAW_CBCT); // will call fixedImageSelected
  SelectComboExternal(1, enREGI_IMAGES::REGISTER_MANUAL_RIGID);
}

void DlgRegistration::SLT_Override() const {
  auto isFixed = false;
  if (this->ui.comboBox_imToOverride->currentText().compare(
          QString("Moving")) == 0) {
    isFixed = true;
  }
  if ((isFixed && m_spFixed == nullptr) ||
      (!isFixed && m_spMoving == nullptr)) {
    std::cout << "The image you try to override is not loaded!\n";
    return;
  }

  int sliderPosIdxZ, sliderPosIdxY, sliderPosIdxX;

  switch (m_enViewArrange) {
  case enViewArrange::AXIAL_FRONTAL_SAGITTAL:
    sliderPosIdxZ =
        this->ui.sliderPosDisp1
            ->value(); // Z corresponds to axial, Y to frontal, X to sagittal
    sliderPosIdxY = this->ui.sliderPosDisp2->value();
    sliderPosIdxX = this->ui.sliderPosDisp3->value();
    break;
  case enViewArrange::FRONTAL_SAGITTAL_AXIAL:
    sliderPosIdxY = this->ui.sliderPosDisp1->value();
    sliderPosIdxX = this->ui.sliderPosDisp2->value();
    sliderPosIdxZ = this->ui.sliderPosDisp3->value();

    break;
  case enViewArrange::SAGITTAL_AXIAL_FRONTAL:
    sliderPosIdxX = this->ui.sliderPosDisp1->value();
    sliderPosIdxZ = this->ui.sliderPosDisp2->value();
    sliderPosIdxY = this->ui.sliderPosDisp3->value();
    break;
  default:
    sliderPosIdxZ =
        this->ui.sliderPosDisp1
            ->value(); // Z corresponds to axial, Y to frontal, X to sagittal
    sliderPosIdxY = this->ui.sliderPosDisp2->value();
    sliderPosIdxX = this->ui.sliderPosDisp3->value();
    break;
  }

  auto imgOrigin = m_spFixed->GetOrigin();
  auto imgSpacing = m_spFixed->GetSpacing();
  if (!isFixed) {
    imgOrigin = m_spMoving->GetOrigin();
    imgSpacing = m_spMoving->GetSpacing();
  }

  UShortImageType::PointType curPhysPos;
  curPhysPos[0] = imgOrigin[0] + sliderPosIdxX * imgSpacing[0]; // Z
  curPhysPos[1] = imgOrigin[1] + sliderPosIdxY * imgSpacing[1]; // Y
  curPhysPos[2] =
      imgOrigin[2] + sliderPosIdxZ * imgSpacing[2]; // Z in default setting

  UShortImageType::IndexType centerIdx{};
  if (!isFixed) {
    if (!m_spMoving->TransformPhysicalPointToIndex(curPhysPos, centerIdx)) {
      std::cerr << "Point not in fixed image!" << std::endl;
      return;
    }
  } else {
    if (!m_spFixed->TransformPhysicalPointToIndex(curPhysPos, centerIdx)) {
      std::cerr << "Point not in moving image!" << std::endl;
      return;
    }
  }

  const auto radius = this->ui.spinBox_overrideRadius->value();
  const auto value = static_cast<unsigned short>(
      this->ui.spinBox_overrideValue->value() + 1024);
  size_t i = 0;
  UShortImageType::IndexType curIdx{};
  for (auto curRadiusX = -radius; curRadiusX <= radius; curRadiusX++) {
    curIdx[0] = centerIdx[0] + curRadiusX;
    for (auto curRadiusY = -radius; curRadiusY <= radius; curRadiusY++) {
      curIdx[1] = centerIdx[1] + curRadiusY;
      for (auto curRadiusZ = -1; curRadiusZ <= 1; curRadiusZ++) {
        curIdx[2] = centerIdx[2] + curRadiusZ;
        if (radius >= sqrt(pow(curRadiusX, 2) + pow(curRadiusY, 2) +
                           pow(curRadiusZ, 2))) {
          if (!isFixed) {
            m_spMoving->SetPixel(curIdx, value);
            i++;
          } else {
            m_spFixed->SetPixel(curIdx, value);
            i++;
          }
        }
      }
    }
  }
  std::cout << i << " pixels were overridden." << std::endl;
}

void DlgRegistration::SLT_DoEclRegistration() {

  const auto translation_vec = DoubleVector{ui.doubleSpinBox_EclTrlX->value(),
                                            ui.doubleSpinBox_EclTrlY->value(),
                                            ui.doubleSpinBox_EclTrlZ->value()};

  const auto rotation_vec = DoubleVector{ui.doubleSpinBox_EclRotX->value(),
                                         ui.doubleSpinBox_EclRotY->value(),
                                         ui.doubleSpinBox_EclRotZ->value()};

  auto &ct_img = this->m_spFixed;

  crl::saveImageAsMHA<UShortImageType>(ct_img, "fixed_bkp.mha");
  this->m_spFixed = CbctRegistration::MoveByEclRegistration(
      translation_vec, rotation_vec, ct_img);

  this->SLT_DrawImageWhenSliceChange();
}

void DlgRegistration::SLT_ResetEclRegistration() {
  const auto filename = std::string("fixed_bkp.mha");
  this->m_spFixed = crl::loadMHAImageAs<UShortImageType>(filename);

  this->SLT_DrawImageWhenSliceChange();
}

void DlgRegistration::SLT_gPMCrecalc() {
  if (m_spFixed == nullptr) {
    return;
  }

  fs::path plan_filepath;
  // Load dcm rtplan.
  if (this->ui.spinBox_NdcmPlans->value() == 1) {
    plan_filepath = to_path(QFileDialog::getOpenFileName(
        this, "Open DCMRT Plan file",
        to_qstr(m_cbctregistration->m_pParent->m_strPathDirDefault),
        "DCMRT Plan (*.dcm)"));
  } else {
    plan_filepath = to_path(QFileDialog::getOpenFileName(
        this, "Open DCMRT Plan file",
        to_qstr(m_cbctregistration->m_pParent->m_strPathDirDefault),
        "DCMRT Plan (*.dcm)"));
    for (auto i = 1; i < this->ui.spinBox_NdcmPlans->value(); i++) {
      plan_filepath = crl::make_sep_str<','>(
          plan_filepath,
          to_path(QFileDialog::getOpenFileName(
              this, "Open DCMRT Plan file",
              to_qstr(m_cbctregistration->m_pParent->m_strPathDirDefault),
              "DCMRT Plan (*.dcm)")));
    }
  }

  /* Below MUST be done externally, through command line - due to
   * incompatibilities with compilers and library versions.. Pytrip-style but
   * with somewhat better control.. (hacks to private members might be possible
   * as well:
   * http://stackoverflow.com/questions/424104/can-i-access-private-members-from-outside-the-class-without-using-friends)
   */

  // Create gPMC command line

#ifdef USE_CUDA
  auto gPMC_device = enDevice::GPU_DEV;
#elif defined(USE_OPENCL)
  enDevice gPMC_device =
      enDevice::CPU_DEV; // because I've only tested with intel graphics
#else
  auto gPMC_device = enDevice::CPU_DEV;
#endif

  if (m_pParent->ui.radioButton_UseCPU->isChecked()) {
    gPMC_device = enDevice::CPU_DEV;
  }
  const auto n_sims = this->ui.spinBox_Nsims->value();
  const auto n_plans = this->ui.spinBox_NdcmPlans->value();

  const auto success = m_cbctregistration->CallingGPMCcommand(
      gPMC_device, n_sims, n_plans, plan_filepath, m_spFixed, m_spMoving,
      m_spFixedDose, m_spMovingDose);
  if (!success) {
    std::cerr << "Dose calc failed, due to the RNG in goPMC you may want to "
                 "just try again"
              << std::endl;
  }

  SLT_DrawImageWhenSliceChange();
}

auto get_rct_voiname(const std::string &rct_name,
                     const std::string &orig_voi_name) {

  const auto remove_space = orig_voi_name.back() == ' ' ? 1 : 0;
  // length of "pCT" is 3, so:
  const auto voi_postfix = std::string_view(
      orig_voi_name.data() + 3, orig_voi_name.size() - (3 + remove_space));
  const auto rct_voi_name = rct_name + std::string(voi_postfix);
  std::cerr << "\"" << rct_voi_name << "\"\n";

  return rct_voi_name;
}

void DlgRegistration::SLT_WEPLcalc() {
  // Get VOI
  const auto voi_name = this->ui.comboBox_VOI->currentText().toStdString();

  const auto gantry_angle = this->ui.spinBox_GantryAngle->value();
  const auto couch_angle = this->ui.spinBox_CouchAngle->value();

  constexpr auto hd_pct = 95;

  { // to keep the stack somewhat clean
    const auto ct_type = get_ctType(ui.comboBoxImgMoving->currentText());
    const auto ss = m_pParent->m_cbctrecon->m_structures->get_ss(ct_type);
    m_cbctregistration->cur_voi = ss->get_roi_by_name(voi_name);

    constexpr auto distal_only = true;
    const auto wepl_voi = crl::wepl::CalculateWEPLtoVOI<distal_only>(
        m_cbctregistration->cur_voi.get(), gantry_angle, couch_angle,
        m_spMoving, m_spFixed);

    // Calculate Hausdorff distance from the original structure to the WEPL
    // structure:
    const auto &orig_voi = *(m_cbctregistration->cur_voi);
    const auto distal_orig_voi =
        crl::roi_to_distal_only_roi(orig_voi, gantry_angle, couch_angle);
    const auto [hd, top5_wepl] =
        crl::calculate_hausdorff_and_top5<float, hd_pct>(*wepl_voi, orig_voi);
    std::cerr << hd_pct << "% Hausdorff WEPL to ORIG: " << hd.h_percent << "\n";
    m_cbctregistration->WEPL_voi_top5 =
        std::make_unique<Rtss_roi_modern>(*top5_wepl);

    m_cbctregistration->cur_voi = std::make_unique<Rtss_roi_modern>(orig_voi);
    m_cbctregistration->WEPL_voi = std::make_unique<Rtss_roi_modern>(*wepl_voi);
  }

  { // to keep the stack somewhat clean
    const auto rtss_dir = QFileDialog::getExistingDirectory(
        this, "Open extra DCMRT Plan file (optional)",
        to_qstr(m_cbctregistration->m_pParent->m_strPathDirDefault),
        QFileDialog::ShowDirsOnly);
    if (!rtss_dir.isEmpty()) {
      // We create a cbctrecon object temporarily to read a seperate dicom dir
      // and only return a single roi from the ss.
      auto cr = CbctRecon();
      if (crl::ReadDicomDir(&cr, to_path(rtss_dir))) {
        const auto xform_file = to_path(QFileDialog::getOpenFileName(
            this, "XForm file for registration of extra structures",
            to_qstr(m_cbctregistration->m_pParent->m_strPathDirDefault),
            "xform (*.txt)"));

        cr.m_structures->ApplyTransformTo<ctType::PLAN_CT>(xform_file);

        const auto extra_roi_name =
            get_rct_voiname(to_path(rtss_dir).filename().string(), voi_name);

        const auto extra_roi =
            cr.m_structures->get_ss<ctType::RIGID_CT>()->get_roi_by_name(
                extra_roi_name);
        if (extra_roi) {
          const auto distal_extra_roi = crl::roi_to_distal_only_roi(
              *extra_roi, gantry_angle, couch_angle);
          m_cbctregistration->extra_voi =
              std::make_unique<Rtss_roi_modern>(distal_extra_roi);
        }
      }
    }
  }

  if (m_cbctregistration->extra_voi) {
    const auto [hd, top5_extra] =
        crl::calculate_hausdorff_and_top5<float, hd_pct>(
            *m_cbctregistration->extra_voi, *m_cbctregistration->cur_voi);
    std::cerr << hd_pct << "% Hausdorff Extra to ORIG: " << hd.h_percent
              << "\n";
    m_cbctregistration->extra_voi_top5 =
        std::make_unique<Rtss_roi_modern>(*top5_extra);
  }
  // Draw WEPL
  SLT_DrawImageWhenSliceChange();
}

void DlgRegistration::SLT_DoRegistrationGradient() {
  // 1) Save current image files
  if (m_spFixed == nullptr || m_spMoving == nullptr) {
    return;
  }

  if (m_cbctregistration->m_pParent->m_spRefCTImg == nullptr ||
      m_cbctregistration->m_pParent->m_spManualRigidCT == nullptr) {
    return;
  }

  std::cout << "1: writing temporary files" << std::endl;
  this->ui.progressBar->setValue(5);
  // Both image type: Unsigned Short
  auto filePathFixed =
      m_cbctregistration->m_strPathPlastimatch / "fixed_gradient.mha";
  auto filePathMoving =
      m_cbctregistration->m_strPathPlastimatch / "moving_gradient.mha";
  const auto filePathOutput =
      m_cbctregistration->m_strPathPlastimatch / "output_gradient.mha";
  const auto filePathXform =
      m_cbctregistration->m_strPathPlastimatch / "xform_gradient.txt";

  using writerType = itk::ImageFileWriter<UShortImageType>;

  auto writer1 = writerType::New();
  writer1->SetFileName(filePathFixed.string());
  writer1->SetUseCompression(true);
  writer1->SetInput(m_spFixed);

  auto writer2 = writerType::New();
  writer2->SetFileName(filePathMoving.string());
  writer2->SetUseCompression(true);
  writer2->SetInput(m_spMoving);

  writer1->Update();
  writer2->Update();

  // Preprocessing
  // 2) move CT-based skin mask on CBCT based on manual shift
  //  if (m_strPathCTSkin)

  std::cout << "2: Creating a plastimatch command file" << std::endl;
  this->ui.progressBar->setValue(10);

  const auto fnCmdRegisterGradient = "cmd_register_gradient.txt"s;
  auto pathCmdRegister =
      m_cbctregistration->m_strPathPlastimatch / fnCmdRegisterGradient;

  std::cout << "For Gradient searching only, CBCT image is a moving image, CT "
               "image is fixed image"
            << std::endl;

  const auto mse = this->ui.radioButton_mse->isChecked();
  const auto cuda = m_pParent->ui.radioButton_UseCUDA->isChecked();
  auto GradOptionStr = this->ui.lineEditGradOption->text().toStdString();
  const auto dummyStr = ""s;
  m_cbctregistration->GenPlastiRegisterCommandFile(
      pathCmdRegister, filePathMoving, filePathFixed, filePathOutput,
      filePathXform, enRegisterOption::PLAST_GRADIENT, dummyStr, dummyStr,
      dummyStr, dummyStr, mse, cuda, GradOptionStr);

  // const char *command_filepath =
  // pathCmdRegister.toLocal8Bit().constDataautoconst
  auto str_command_filepath = pathCmdRegister;

  this->ui.progressBar->setValue(15);
  auto trn = m_cbctregistration->CallingPLMCommandXForm(str_command_filepath);
  this->ui.progressBar->setValue(80);

  const auto trn_vec =
      FloatVector{static_cast<float>(-trn[0]), static_cast<float>(-trn[1]),
                  static_cast<float>(-trn[-2])};

  auto &structs = m_cbctregistration->m_pParent->m_structures;
  structs->ApplyVectorTransform_InPlace<ctType::PLAN_CT>(trn_vec);

  this->ui.progressBar->setValue(99); // good ol' 99%

  ImageManualMoveOneShot(static_cast<float>(-trn[0]),
                         static_cast<float>(-trn[1]),
                         static_cast<float>(-trn[2]));
  std::cout << "6: Reading is completed" << std::endl;
  this->ui.progressBar->setValue(100);

  UpdateListOfComboBox(0); // combo selection signalis called
  UpdateListOfComboBox(1);

  SelectComboExternal(
      0, enREGI_IMAGES::REGISTER_RAW_CBCT); // will call fixedImageSelected
  SelectComboExternal(1, enREGI_IMAGES::REGISTER_MANUAL_RIGID);

  this->ui.progressBar->setValue(0);
}

void DlgRegistration::SLT_ConfirmManualRegistration() {
  if (m_spFixed == nullptr || m_spMoving == nullptr) {
    return;
  }

  auto p_parent = m_cbctregistration->m_pParent;
  if (p_parent->m_spRefCTImg == nullptr ||
      p_parent->m_spManualRigidCT == nullptr) {
    return;
  }

  if (this->ui.checkBoxKeyMoving->isChecked()) {
    SLT_KeyMoving(false); // uncheck macro
  }

  // Apply post processing for raw CBCT image and generate
  std::cout << "Preprocessing for CBCT" << std::endl;

  auto originBefore = p_parent->m_spRefCTImg->GetOrigin();
  auto originAfter = p_parent->m_spManualRigidCT->GetOrigin();

  double fShift[3];
  fShift[0] = originBefore[0] - originAfter[0]; // DICOM
  fShift[1] = originBefore[1] - originAfter[1];
  fShift[2] = originBefore[2] - originAfter[2];

  const auto trn_vec = FloatVector{static_cast<float>(-fShift[0]),
                                   static_cast<float>(-fShift[1]),
                                   static_cast<float>(-fShift[2])};

  m_cbctregistration->m_pParent->m_structures
      ->ApplyVectorTransform_InPlace<ctType::PLAN_CT>(trn_vec);

  if (this->ui.checkBoxCropBkgroundCT->isChecked()) {
    auto structures =
        m_cbctregistration->m_pParent->m_structures->get_ss(ctType::RIGID_CT);

    // RIGID_CT structs should be created above
    const auto voi_name = this->ui.comboBox_VOItoCropBy->currentText();
    if (structures != nullptr) {
      const auto voi = structures->get_roi_ref_by_name(voi_name.toStdString());
      crl::opencl::crop_by_struct_InPlace(p_parent->m_spRawReconImg, voi);
    }

    auto update_message = QString("CBCT cropped outside of ") + voi_name;
    m_pParent->UpdateReconImage(p_parent->m_spRawReconImg, update_message);

    UpdateListOfComboBox(0); // combo selection signalis called
    UpdateListOfComboBox(1);

    SelectComboExternal(
        0, enREGI_IMAGES::REGISTER_RAW_CBCT); // will call fixedImageSelected
    SelectComboExternal(1, enREGI_IMAGES::REGISTER_MANUAL_RIGID);
  }

  // Export final xform file
  auto filePathXform =
      m_cbctregistration->m_strPathPlastimatch / "xform_manual.txt";

  std::ofstream fout;
  fout.open(filePathXform.string());
  fout << "#Custom Transform" << std::endl;
  fout << "#Transform: Translation_only" << std::endl;
  fout << "Parameters: " << fShift[0] << " " << fShift[1] << " " << fShift[2]
       << std::endl;
  // fout << "FixedParameters: 0 0 0" << std::endl;

  fout.close();
  std::cout << "Writing manual registration transform info is done."
            << std::endl;
  this->ui.pushButtonConfirmManualRegi->setDisabled(true);
}

void DlgRegistration::SLT_IntensityNormCBCT() {
  const auto fROI_Radius = this->ui.lineEditNormRoiRadius->text().toFloat();

  std::cout << "Intensity is being analyzed...Please wait." << std::endl;

  float intensitySDFix = 0.0;
  float intensitySDMov = 0.0;
  const auto meanIntensityFix = m_cbctregistration->m_pParent->GetMeanIntensity(
      m_spFixed, fROI_Radius, &intensitySDFix);
  const auto meanIntensityMov = m_cbctregistration->m_pParent->GetMeanIntensity(
      m_spMoving, fROI_Radius, &intensitySDMov);

  std::cout << "Mean/SD for Fixed = " << meanIntensityFix << "/"
            << intensitySDFix << std::endl;
  std::cout << "Mean/SD for Moving = " << meanIntensityMov << "/"
            << intensitySDMov << std::endl;

  crl::AddConstHU(m_spFixed,
                  static_cast<int>(meanIntensityMov - meanIntensityFix));
  // SLT_PassMovingImgForAnalysis();

  std::cout << "Intensity shifting is done! Added value = "
            << static_cast<int>(meanIntensityMov - meanIntensityFix)
            << std::endl;
  auto update_message =
      QString("Added_%1")
          .arg(static_cast<int>(meanIntensityMov - meanIntensityFix));
  m_pParent->UpdateReconImage(m_spFixed, update_message);
  SelectComboExternal(0, enREGI_IMAGES::REGISTER_RAW_CBCT);
}
