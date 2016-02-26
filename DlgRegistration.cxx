#include "DlgRegistration.h"
//#include "cbctrecon.h"

#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>

#include "itkImageFileWriter.h"
#include "itk_image_save.h"
#include "plmcli_config.h"
#include "plm_image.h"
#include "plm_image_type.h"
#include "file_util.h"
//warp related include files
#include "rt_study.h"
//#include "rtds_warp.h"
#include "rt_study_warp.h"
#include "warp_parms.h"
#include "distance_map.h"

#include "itk_threshold.h"
#include "plm_math.h"

#include "itk_image_load.h"

#include "synthetic_vf.h"
#include "volume_header.h"

#include "plm_exception.h"
#include "plm_version.h"
#include "registration.h"
#include "registration_data.h"
#include "registration_parms.h"
#include "logfile.h"
#include "path_util.h"
#include "rt_study_metadata.h"
#include "string_util.h"

#include "segment_body.h"

#include "rtss_contour.h"
#include "rtss_roi.h"

#include "rtplan_control_pt.h"
#include "rtplan_beam.h"
#include "rtplan.h"
#include "plm_file_format.h"
#include "dcmtk_rt_study.h"

//#include "pcmd_dmap.h"

//#include "pcmd_threshold.h"
//#include "pcmd_mask.h"
//#include "pcmd_mask.cxx"
//#include "pcmd_segment.h"
//#include "pcmd_synth_vf.h"
//#include "pcmd_warp.h"
//#include "pcmd_xf_convert.h"

#include <QDir>
#include <QStringList>
#include <QMessageBox>
#include <fstream>

#define FIXME_BACKGROUND_MAX (-1200)

using namespace std;


//#include "itkImageSliceConstIteratorWithIndex.h"
//#include "itkImageSliceIteratorWithIndex.h"

DlgRegistration::DlgRegistration(): QDialog ()
{
    /* Sets up the GUI */
    ui.setupUi (this);
}

DlgRegistration::DlgRegistration(QWidget *parent): QDialog (parent)
{
    /* Sets up the GUI */
    ui.setupUi (this);
    m_pParent = (CbctRecon*)(parent);  

    connect (ui.labelOverlapWnd1, SIGNAL(Mouse_Move()), this, SLOT(SLT_UpdateSplit1())); //added
    connect (ui.labelOverlapWnd2, SIGNAL(Mouse_Move()), this, SLOT(SLT_UpdateSplit2())); //added
    connect (ui.labelOverlapWnd3, SIGNAL(Mouse_Move()), this, SLOT(SLT_UpdateSplit3())); //added

	connect (ui.labelOverlapWnd1, SIGNAL(Mouse_Pressed_Left()), this, SLOT(SLT_MousePressedLeft1())); //added
	connect (ui.labelOverlapWnd2, SIGNAL(Mouse_Pressed_Left()), this, SLOT(SLT_MousePressedLeft2())); //added
	connect (ui.labelOverlapWnd3, SIGNAL(Mouse_Pressed_Left()), this, SLOT(SLT_MousePressedLeft3())); //added
	connect (ui.labelOverlapWnd1, SIGNAL(Mouse_Pressed_Right()), this, SLOT(SLT_MousePressedRight1())); //added
	connect (ui.labelOverlapWnd2, SIGNAL(Mouse_Pressed_Right()), this, SLOT(SLT_MousePressedRight2())); //added
	connect (ui.labelOverlapWnd3, SIGNAL(Mouse_Pressed_Right()), this, SLOT(SLT_MousePressedRight3())); //added

	connect (ui.labelOverlapWnd1, SIGNAL(Mouse_Released_Left()), this, SLOT(SLT_MouseReleasedLeft1())); //added
	connect (ui.labelOverlapWnd2, SIGNAL(Mouse_Released_Left()), this, SLOT(SLT_MouseReleasedLeft2())); //added
	connect (ui.labelOverlapWnd3, SIGNAL(Mouse_Released_Left()), this, SLOT(SLT_MouseReleasedLeft3())); //added
	connect (ui.labelOverlapWnd1, SIGNAL(Mouse_Released_Right()), this, SLOT(SLT_MouseReleasedRight1())); //added
	connect (ui.labelOverlapWnd2, SIGNAL(Mouse_Released_Right()), this, SLOT(SLT_MouseReleasedRight2())); //added
	connect (ui.labelOverlapWnd3, SIGNAL(Mouse_Released_Right()), this, SLOT(SLT_MouseReleasedRight3())); //added

	connect (ui.labelOverlapWnd1, SIGNAL(FocusOut()), this, SLOT(SLT_CancelMouseAction())); //added
	connect (ui.labelOverlapWnd2, SIGNAL(FocusOut()), this, SLOT(SLT_CancelMouseAction())); //added
	connect (ui.labelOverlapWnd3, SIGNAL(FocusOut()), this, SLOT(SLT_CancelMouseAction())); //added

	connect (ui.labelOverlapWnd1, SIGNAL(Mouse_Wheel()), this, SLOT(SLT_MouseWheelUpdate1())); //added
	connect (ui.labelOverlapWnd2, SIGNAL(Mouse_Wheel()), this, SLOT(SLT_MouseWheelUpdate2())); //added
	connect (ui.labelOverlapWnd3, SIGNAL(Mouse_Wheel()), this, SLOT(SLT_MouseWheelUpdate3())); //added    

	SLT_CancelMouseAction();
	m_enViewArrange = AXIAL_FRONTAL_SAGITTAL;

	m_ptTmpOriginalDataOffset = QPoint(0,0);

	m_iTmpOriginalW = 0;
	m_iTmpOriginalL = 0;
    //void* aa = ui2.comboBoxDOF;

    /*m_dspDlgRegi1 = new YK16GrayImage();
    m_dspDlgRegi2 =  new YK16GrayImage();
    m_dspDlgRegi3 =  new YK16GrayImage();*/

    /*m_dspDlgRegi1 = NULL;
    m_dspDlgRegi2 =  NULL;
    m_dspDlgRegi3 =  NULL;*/
	//m_strPathPlastimatch = "./plastimatch"


	ui.comboBoxDeformOption->addItem("mi");//mutual info
	ui.comboBoxDeformOption->addItem("mse"); //intensity	
	ui.comboBoxDeformOption->setCurrentIndex(0);

	//QString strTest = "abcdefg";

	//const char strTest2[50];
	//const char* strTest2 = strTest.toUtf8().constData();
	//char* strTest3 = (char*)(strTest.toLocal8Bit().constData());

        m_pDcmStudyPlan = NULL;
}

DlgRegistration::~DlgRegistration()
{
    //cout << "Deleting objects"<< endl;

    for (int i = 0 ; i<3 ; i++)
    {
        m_YKImgFixed[i].ReleaseBuffer();
        m_YKImgMoving[i].ReleaseBuffer();
        m_YKDisp[i].ReleaseBuffer();
    }

    if (m_pDcmStudyPlan != NULL)
    {
        delete m_pDcmStudyPlan;
        m_pDcmStudyPlan = NULL;
    }
    /*if (m_dspDlgRegi1 != NULL)
    {
        delete m_dspDlgRegi1;
        m_dspDlgRegi1 = NULL;
    }
    if (m_dspDlgRegi2 != NULL)
    {
        delete m_dspDlgRegi2;
        m_dspDlgRegi2 = NULL;
    }
    if (m_dspDlgRegi3 != NULL)
    {
        delete m_dspDlgRegi3;
        m_dspDlgRegi3 = NULL;
    }     */   
}

void DlgRegistration::TestDraw()
{
    //m_pParent->SLT_ViewRegistration();
    //show();

}

void DlgRegistration::SLT_CrntPosGo()
{       
    //m_pParent->Draw2DFrom3D(m_pParent->m_spReconImg, PLANE_AXIAL, 0.0, m_dspDlgRegi1);
    //m_pParent->GetValueFrom3DImageUshort(5, 5, 5, m0_pParent->m_spReconImg);
    //CbctRecon* pParent= (CbctRecon*)(this->parent());
    /*whenFixedImgLoaded();
    SLT_DrawImageWhenSliceChange();    	    */
  if (!m_spFixed)
	return;


  // DICOMN position, mm
  double curDCMPosX = ui.lineEditCurPosX->text().toDouble();
  double curDCMPosY = ui.lineEditCurPosY->text().toDouble();
  double curDCMPosZ = ui.lineEditCurPosZ->text().toDouble();


  USHORT_ImageType::SizeType imgSize = m_spFixed->GetRequestedRegion().GetSize(); //1016x1016 x z
  USHORT_ImageType::PointType imgOrigin = m_spFixed->GetOrigin();
  USHORT_ImageType::SpacingType imgSpacing = m_spFixed->GetSpacing();

  //double curPhysPos[3];
  //curPhysPos[0] = imgOrigin[2] + sliderPosIdxZ*imgSpacing[2] ; //Z in default setting
  //curPhysPos[1] = imgOrigin[1] + sliderPosIdxY*imgSpacing[1]; //Y
  //curPhysPos[2] = imgOrigin[0] + sliderPosIdxX*imgSpacing[0]; //Z

  int iSliderPosIdxZ = qRound((curDCMPosZ - imgOrigin[2]) / (double)imgSpacing[2]);
  int iSliderPosIdxY = qRound((curDCMPosY - imgOrigin[1]) / (double)imgSpacing[1]);
  int iSliderPosIdxX = qRound((curDCMPosX - imgOrigin[0]) / (double)imgSpacing[0]);

  
  ui.sliderPosDisp1->setValue(iSliderPosIdxZ);
  ui.sliderPosDisp2->setValue(iSliderPosIdxY);
  ui.sliderPosDisp3->setValue(iSliderPosIdxX);
}


void DlgRegistration::SLT_DrawImageWhenSliceChange()
{
    /*if (m_pParent == NULL)
        return;	*/
	if (!m_spFixed)
	  return;	

	int sliderPosIdxZ,sliderPosIdxY, sliderPosIdxX;

	switch (m_enViewArrange)
	{
	case AXIAL_FRONTAL_SAGITTAL:
	  sliderPosIdxZ = ui.sliderPosDisp1->value();   // Z corresponds to axial, Y to frontal, X to sagittal
	  sliderPosIdxY = ui.sliderPosDisp2->value();
	  sliderPosIdxX = ui.sliderPosDisp3->value();
	  break;
	case FRONTAL_SAGITTAL_AXIAL:
	  sliderPosIdxY = ui.sliderPosDisp1->value(); 
	  sliderPosIdxX = ui.sliderPosDisp2->value(); 
	  sliderPosIdxZ = ui.sliderPosDisp3->value();   
	  
	  break;
	case SAGITTAL_AXIAL_FRONTAL:
	  sliderPosIdxX = ui.sliderPosDisp1->value(); 
	  sliderPosIdxZ = ui.sliderPosDisp2->value(); 
	  sliderPosIdxY = ui.sliderPosDisp3->value();
	  break;
	default:
	  sliderPosIdxZ = ui.sliderPosDisp1->value();   // Z corresponds to axial, Y to frontal, X to sagittal
	  sliderPosIdxY = ui.sliderPosDisp2->value();
	  sliderPosIdxX = ui.sliderPosDisp3->value();
	  break;
	}

    USHORT_ImageType::SizeType imgSize = m_spFixed->GetRequestedRegion().GetSize(); //1016x1016 x z
    USHORT_ImageType::PointType imgOrigin = m_spFixed->GetOrigin();
    USHORT_ImageType::SpacingType imgSpacing = m_spFixed->GetSpacing();

    double curPhysPos[3];
    curPhysPos[0] = imgOrigin[2] + sliderPosIdxZ*imgSpacing[2] ; //Z in default setting
    curPhysPos[1] = imgOrigin[1] + sliderPosIdxY*imgSpacing[1]; //Y
    curPhysPos[2] = imgOrigin[0] + sliderPosIdxX*imgSpacing[0]; //Z

    //This caused the problem!!!
    /*m_pParent->Draw2DFrom3D(m_pParent->m_spReconImg, PLANE_AXIAL, curPhysPos1, m_dspDlgRegi1);
    m_pParent->Draw2DFrom3D(m_pParent->m_spReconImg, PLANE_FRONTAL, curPhysPos2, m_dspDlgRegi2);
    m_pParent->Draw2DFrom3D(m_pParent->m_spReconImg, PLANE_SAGITTAL, curPhysPos3, m_dspDlgRegi3);*/

    /*m_pParent->Draw2DFrom3D(m_pParent->m_spReconImg, PLANE_AXIAL, curPhysPos1, m_dspDlgRegi1);
    m_pParent->Draw2DFrom3D(m_pParent->m_spReconImg, PLANE_FRONTAL, curPhysPos2, m_dspDlgRegi2);
    m_pParent->Draw2DFrom3D(m_pParent->m_spReconImg, PLANE_SAGITTAL, curPhysPos3, m_dspDlgRegi3);        */

	int refIdx = 3 - m_enViewArrange;

    if (ui.checkBoxDrawCrosshair->isChecked())
    {
        m_YKDisp[refIdx%3].m_bDrawCrosshair = true;
        m_YKDisp[(refIdx+1)%3].m_bDrawCrosshair = true;
        m_YKDisp[(refIdx+2)%3].m_bDrawCrosshair = true;

		//m_YKDisp[0]// Left Top image, the largest

        m_YKDisp[refIdx%3].m_ptCrosshair.setX(sliderPosIdxX); //axial
        m_YKDisp[refIdx%3].m_ptCrosshair.setY(sliderPosIdxY);        

        m_YKDisp[(refIdx+1)%3].m_ptCrosshair.setX(sliderPosIdxX); //Frontal
        m_YKDisp[(refIdx+1)%3].m_ptCrosshair.setY(imgSize[2] - sliderPosIdxZ-1);        

        m_YKDisp[(refIdx+2)%3].m_ptCrosshair.setX(sliderPosIdxY); //Sagittal
        m_YKDisp[(refIdx+2)%3].m_ptCrosshair.setY(imgSize[2] - sliderPosIdxZ-1);

        m_YKImgFixed[0].m_bDrawCrosshair = true;
        m_YKImgFixed[1].m_bDrawCrosshair = true;
        m_YKImgFixed[2].m_bDrawCrosshair = true;

        m_YKImgFixed[0].m_ptCrosshair.setX(sliderPosIdxX); //sagittal slider
        m_YKImgFixed[0].m_ptCrosshair.setY(sliderPosIdxY);        

        m_YKImgFixed[1].m_ptCrosshair.setX(sliderPosIdxX); //sagittal slider
        m_YKImgFixed[1].m_ptCrosshair.setY(imgSize[2] - sliderPosIdxZ-1);        

        m_YKImgFixed[2].m_ptCrosshair.setX(sliderPosIdxY); //sagittal slider
        m_YKImgFixed[2].m_ptCrosshair.setY(imgSize[2] - sliderPosIdxZ-1);
    }   
	else
	{
	  m_YKDisp[0].m_bDrawCrosshair = false;
	  m_YKDisp[1].m_bDrawCrosshair = false;
	  m_YKDisp[2].m_bDrawCrosshair = false;

	  m_YKImgFixed[0].m_bDrawCrosshair = false;
	  m_YKImgFixed[1].m_bDrawCrosshair = false;
	  m_YKImgFixed[2].m_bDrawCrosshair = false;
	}
    
        //center of the split value is passed by m_YKImgFixed;
    //m_pParent->Draw2DFrom3D(m_pParent->m_spReconImg, PLANE_AXIAL, curPhysPos[0], m_YKImgFixed[0]);
    //m_pParent->Draw2DFrom3D(m_pParent->m_spReconImg, PLANE_FRONTAL, curPhysPos[1], m_YKImgFixed[1]);
    //m_pParent->Draw2DFrom3D(m_pParent->m_spReconImg, PLANE_SAGITTAL, curPhysPos[2], m_YKImgFixed[2]);

	if (m_spMoving)
	{
	  m_pParent->Draw2DFrom3DDouble(m_spFixed,m_spMoving, PLANE_AXIAL, curPhysPos[0], m_YKImgFixed[0], m_YKImgMoving[0]);
	  m_pParent->Draw2DFrom3DDouble(m_spFixed, m_spMoving, PLANE_FRONTAL, curPhysPos[1], m_YKImgFixed[1], m_YKImgMoving[1]);
	  m_pParent->Draw2DFrom3DDouble(m_spFixed, m_spMoving, PLANE_SAGITTAL, curPhysPos[2], m_YKImgFixed[2], m_YKImgMoving[2]);
	}
	else
	{
	  m_pParent->Draw2DFrom3DDouble(m_spFixed,m_spFixed, PLANE_AXIAL, curPhysPos[0], m_YKImgFixed[0], m_YKImgMoving[0]);
	  m_pParent->Draw2DFrom3DDouble(m_spFixed, m_spFixed, PLANE_FRONTAL, curPhysPos[1], m_YKImgFixed[1], m_YKImgMoving[1]);
	  m_pParent->Draw2DFrom3DDouble(m_spFixed, m_spFixed, PLANE_SAGITTAL, curPhysPos[2], m_YKImgFixed[2], m_YKImgMoving[2]);
	}
   
 
    //Update position lineEdit
    QString strPos1, strPos2, strPos3;
    strPos1.sprintf("%3.1f", curPhysPos[0]);
    strPos2.sprintf("%3.1f", curPhysPos[1]);
    strPos3.sprintf("%3.1f", curPhysPos[2]);

    ui.lineEditCurPosX->setText(strPos3);
    ui.lineEditCurPosY->setText(strPos2);
    ui.lineEditCurPosZ->setText(strPos1);



	////Update Origin text box
	USHORT_ImageType::PointType imgOriginFixed = m_spFixed->GetOrigin();
	QString strOriFixed;
	strOriFixed.sprintf("%3.4f, %3.4f, %3.4f", imgOriginFixed[0], imgOriginFixed[1], imgOriginFixed[2]);
	ui.lineEditOriginFixed->setText(strOriFixed);


	if (m_spMoving)
	{
	  USHORT_ImageType::PointType imgOriginMoving = m_spMoving->GetOrigin();
	  QString strOriMoving;
	  strOriMoving.sprintf("%3.4f, %3.4f, %3.4f", imgOriginMoving[0], imgOriginMoving[1], imgOriginMoving[2]);
	  ui.lineEditOriginMoving->setText(strOriMoving);
	}

	/*qDebug() << strOriFixed;
	qDebug() << strOriMoving;*/
	//
	
	

    SLT_DrawImageInFixedSlice();

}
//Display is not included here
void DlgRegistration::whenFixedImgLoaded()
{
  if (!m_spFixed)
	return;
    /*if (!m_pParent->m_spReconImg)
        return;

	m_spFixed = m_pParent->m_spReconImg;*/

    USHORT_ImageType::SizeType imgSize = m_spFixed->GetRequestedRegion().GetSize(); //1016x1016 x z
    USHORT_ImageType::PointType imgOrigin = m_spFixed->GetOrigin();
    USHORT_ImageType::SpacingType imgSpacing = m_spFixed->GetSpacing();

	//to avoid first unnecessary action.
    disconnect(ui.sliderPosDisp1, SIGNAL(valueChanged(int)), this, SLOT(SLT_DrawImageWhenSliceChange()));
    disconnect(ui.sliderPosDisp2, SIGNAL(valueChanged(int)), this, SLOT(SLT_DrawImageWhenSliceChange()));
    disconnect(ui.sliderPosDisp3, SIGNAL(valueChanged(int)), this, SLOT(SLT_DrawImageWhenSliceChange()));

	disconnect(ui.sliderFixedW, SIGNAL(valueChanged(int)), this, SLOT(SLT_DrawImageInFixedSlice()));
	disconnect(ui.sliderFixedL, SIGNAL(valueChanged(int)), this, SLOT(SLT_DrawImageInFixedSlice()));
	disconnect(ui.sliderMovingW, SIGNAL(valueChanged(int)), this, SLOT(SLT_DrawImageInFixedSlice()));
	disconnect(ui.sliderMovingL, SIGNAL(valueChanged(int)), this, SLOT(SLT_DrawImageInFixedSlice()));

	initOverlapWndSize();	

    ui.sliderPosDisp1->setMinimum(0);
    ui.sliderPosDisp1->setMaximum(imgSize[2]-1);
    int curPosZ = imgSize[2]/2;
    ui.sliderPosDisp1->setValue(curPosZ);

    ui.sliderPosDisp2->setMinimum(0);
    ui.sliderPosDisp2->setMaximum(imgSize[1]-1);
    int curPosY = imgSize[1]/2;
    ui.sliderPosDisp2->setValue(curPosY);

    ui.sliderPosDisp3->setMinimum(0);
    ui.sliderPosDisp3->setMaximum(imgSize[0]-1);
    int curPosX = imgSize[0]/2;
    ui.sliderPosDisp3->setValue(curPosX);
	QPoint qPoint0 = QPoint(imgSize[0]/2, imgSize[1]/2);
	QPoint qPoint1 = QPoint(imgSize[0]/2, imgSize[2]/2);
	QPoint qPoint2 = QPoint(imgSize[1]/2, imgSize[2]/2);
	m_YKDisp[0].SetSplitCenter(qPoint0);
	m_YKDisp[1].SetSplitCenter(qPoint1);
	m_YKDisp[2].SetSplitCenter(qPoint2);

	//Temporarily load the Disp Image to calculate the window level
	//m_pParent->Draw2DFrom3D(m_pParent->m_spReconImg, PLANE_AXIAL, 0.0, m_YKDisp[0]);

	////Window Level based on half square
	//m_YKImgFixed[0].setROI((int)(m_YKImgFixed[0].m_iWidth/4.0),
	//						(int)(m_YKImgFixed[0].m_iHeight/4.0),
	//						(int)(m_YKImgFixed[0].m_iWidth*3.0/4.0),
	//						(int)(m_YKImgFixed[0].m_iHeight*3.0/4.0));
	//m_YKImgFixed[0].CalcImageInfo_ROI();
	//
	//int fixedImgWinMid = (int)m_YKImgFixed[0].m_fPixelMean_ROI;
	//int fixedImgWinWidth = (int)(m_YKImgFixed[0].m_fPixelSD_ROI*2.0);

	//int iSliderMin = fixedImgWinMid - fixedImgWinWidth/2.0;
	//int iSliderMax = fixedImgWinMid + fixedImgWinWidth/2.0;	

	//if (iSliderMin <= 0)
	//  iSliderMin = 0;
	//if (iSliderMax >= 65535)
	//  iSliderMax = 10000;

	int iSliderW = 2000;
	int iSliderL = 1024;	

	ui.sliderFixedW->setValue(iSliderW);
	ui.sliderMovingW->setValue(iSliderW);

	ui.sliderFixedL->setValue(iSliderL);
	ui.sliderMovingL->setValue(iSliderL);

    connect(ui.sliderPosDisp1, SIGNAL(valueChanged(int)), this, SLOT(SLT_DrawImageWhenSliceChange()));
    connect(ui.sliderPosDisp2, SIGNAL(valueChanged(int)), this, SLOT(SLT_DrawImageWhenSliceChange()));
    connect(ui.sliderPosDisp3, SIGNAL(valueChanged(int)), this, SLOT(SLT_DrawImageWhenSliceChange()));

	connect(ui.sliderFixedW, SIGNAL(valueChanged(int)), this, SLOT(SLT_DrawImageInFixedSlice()));
	connect(ui.sliderFixedL, SIGNAL(valueChanged(int)), this, SLOT(SLT_DrawImageInFixedSlice()));
	connect(ui.sliderMovingW, SIGNAL(valueChanged(int)), this, SLOT(SLT_DrawImageInFixedSlice()));
	connect(ui.sliderMovingL, SIGNAL(valueChanged(int)), this, SLOT(SLT_DrawImageInFixedSlice()));
}

void DlgRegistration::whenMovingImgLoaded()
{
  //do nothing so far
  /*if (!m_pParent->m_spRefCTImg)
	return;

  m_spMoving = m_pParent->m_spRefCTImg;*/
}

void DlgRegistration::SLT_DrawImageInFixedSlice()//Display Swap here!
{
   //Constitute m_YKDisp from Fixed and Moving 

    if (ui.checkBoxDrawSplit->isChecked())
    {
        for (int i = 0; i<3 ; i++)
        {
		  int idxAdd = m_enViewArrange;//m_iViewArrange = 0,1,2
		  if (idxAdd+i >=3)
			idxAdd = idxAdd-3;

            m_YKDisp[i].SetSpacing(m_YKImgFixed[i+idxAdd].m_fSpacingX,m_YKImgFixed[i+idxAdd].m_fSpacingY);

            m_YKDisp[i].SetSplitOption(PRI_LEFT_TOP);
            //m_YKDisp[i].SetSplitCenter(QPoint dataPt);//From mouse event
            if (!m_YKDisp[i].ConstituteFromTwo(m_YKImgFixed[i+idxAdd], m_YKImgMoving[i+idxAdd]))
                cout << "Image error " << i+1 << " th view" << endl;
        }
    }
    else
    {
        for (int i = 0 ; i<3 ; i++)
        {
		  int addedViewIdx = m_enViewArrange;
		  if (i+addedViewIdx >= 3)
			addedViewIdx = addedViewIdx-3;

            //m_YKDisp[i].CreateImage(m_YKImgFixed[i].m_iWidth, m_YKImgFixed[i].m_iHeight, 0);
            //m_YKDisp[i].CopyFromBuffer(m_YKImgFixed[i].m_pData, m_YKImgFixed[i].m_iWidth, m_YKImgFixed[i].m_iHeight);
            //cout << "width" << m_YKImgFixed[i].m_iWidth << endl;
            //cout << "height" << m_YKImgFixed[i].m_iHeight << endl;
            m_YKDisp[i].CloneImage(m_YKImgFixed[i+addedViewIdx]);
        }     
    }

    //m_dspDlgRegi1->FillPixMapMinMax(ui.sliderRawMin->value(), ui.sliderRawMax->value());	
    //m_YKDisp[0].FillPixMapMinMax(0, 1200);	        
    //m_YKDisp[1].FillPixMapMinMax(0, 1200);	       
    //m_YKDisp[2].FillPixMapMinMax(0, 1200);

    //ui.labelOverlapWnd1->SetBaseImage(&m_YKDisp[0]);
    //ui.labelOverlapWnd2->SetBaseImage(&m_YKDisp[1]);
    //ui.labelOverlapWnd3->SetBaseImage(&m_YKDisp[2]);

	int sliderW1 = ui.sliderFixedW->value();
	int sliderW2 = ui.sliderMovingW->value();

	int sliderL1 = ui.sliderFixedL->value();
	int sliderL2 = ui.sliderMovingL->value();

    m_YKDisp[0].FillPixMapDual(sliderL1,sliderL2, sliderW1, sliderW2);
    m_YKDisp[1].FillPixMapDual(sliderL1,sliderL2, sliderW1, sliderW2);
    m_YKDisp[2].FillPixMapDual(sliderL1,sliderL2, sliderW1, sliderW2);

    ui.labelOverlapWnd1->SetBaseImage(&m_YKDisp[0]);
    ui.labelOverlapWnd2->SetBaseImage(&m_YKDisp[1]);
    ui.labelOverlapWnd3->SetBaseImage(&m_YKDisp[2]);    

    ui.labelOverlapWnd1->update();	
    ui.labelOverlapWnd2->update();	
    ui.labelOverlapWnd3->update();
}

void DlgRegistration::SLT_UpdateSplit1()//Mouse Move event
{
  int idx = 0;
  UpdateSplit(idx, ui.labelOverlapWnd1);
}


void DlgRegistration::SLT_UpdateSplit2()
{
  int idx = 1;
  UpdateSplit(idx, ui.labelOverlapWnd2);

}
void DlgRegistration::SLT_UpdateSplit3()
{
  int idx = 2;  
  UpdateSplit(idx, ui.labelOverlapWnd3);
}


void DlgRegistration::UpdateSplit(int viewIdx, qyklabel* pOverlapWnd)
{
  int idx = viewIdx;

  if (pOverlapWnd == NULL)
	return;

  if (m_YKDisp[idx].IsEmpty())
	return;

  if (!m_bPressedLeft[idx] && !m_bPressedRight[idx])
	return;

  double dspWidth = pOverlapWnd->width();
  double dspHeight = pOverlapWnd->height();

  int dataWidth = m_YKDisp[idx].m_iWidth;
  int dataHeight = m_YKDisp[idx].m_iHeight;
  if (dataWidth*dataHeight == 0)
	return;

  int dataX = pOverlapWnd->GetDataPtFromMousePos().x();
  int dataY = pOverlapWnd->GetDataPtFromMousePos().y();

  //only works when while the left Mouse is being clicked
  if (m_bPressedLeft[idx])
  {
  	QPoint XYpoint = QPoint(dataX,dataY);
	m_YKDisp[idx].SetSplitCenter(XYpoint);
	SLT_DrawImageInFixedSlice();
  }
  else if (m_bPressedRight[idx] && ui.checkBoxPan->isChecked())
  {
	////Update offset information of dispImage

	//GetOriginalDataPos (PanStart)
	//offset should be 0.. only relative distance matters. offset is in realtime changing
	QPoint pointO = QPoint(0,0);
	QPoint ptDataPanStartRel = pOverlapWnd->View2DataExt(m_ptPanStart, dspWidth,
	  dspHeight, dataWidth, dataHeight, pointO, m_YKDisp[idx].m_fZoom);
	QPoint pointOverlap = QPoint(pOverlapWnd->x,pOverlapWnd->y);
	QPoint ptDataPanEndRel = pOverlapWnd->View2DataExt(pointOverlap, dspWidth,
	  dspHeight, dataWidth, dataHeight, pointO, m_YKDisp[idx].m_fZoom);

	//int dspOffsetX = pOverlapWnd->x - m_ptPanStart.x();
	//int dspOffsetY = m_ptPanStart.y() - pOverlapWnd->y;

	/*QPoint ptDataStart= pOverlapWnd->GetDataPtFromViewPt(m_ptPanStart.x(),  m_ptPanStart.y());
	QPoint ptDataEnd= pOverlapWnd->GetDataPtFromViewPt(pOverlapWnd->x, pOverlapWnd->y);*/

	int curOffsetX = ptDataPanEndRel.x() - ptDataPanStartRel.x();
	int curOffsetY = ptDataPanEndRel.y() - ptDataPanStartRel.y();

	int prevOffsetX = m_ptTmpOriginalDataOffset.x();
	int prevOffsetY = m_ptTmpOriginalDataOffset.y();

	double fZoom = m_YKDisp[idx].m_fZoom;

	m_YKDisp[idx].SetOffset(prevOffsetX - curOffsetX, prevOffsetY - curOffsetY);

	SLT_DrawImageInFixedSlice();
  }
  else if (m_bPressedRight[idx] && !ui.checkBoxPan->isChecked()) //Window Level
  {
	double wWidth = 2.0;
	double wLevel = 2.0;

	int iAddedWidth = (int)((m_ptWindowLevelStart.y() - pOverlapWnd->y)*wWidth);
	int iAddedLevel = (int)((pOverlapWnd->x - m_ptWindowLevelStart.x())*wLevel);

	//Which image is clicked first??
	QPoint crntDataPt = pOverlapWnd->GetDataPtFromViewPt(m_ptWindowLevelStart.x(),  m_ptWindowLevelStart.y());	

	if (pOverlapWnd->m_pYK16Image != NULL)
	{
	  if (m_YKDisp[idx].isPtInFirstImage(crntDataPt.x(), crntDataPt.y()))
	  {
		//ui.sliderFixedW->setValue(ui.sliderFixedW->value() + iAddedWidth); //SLT_DrawImageInFixedSlice will be called
		//ui.sliderFixedL->setValue(ui.sliderFixedL->value() + iAddedLevel);
		ui.sliderFixedW->setValue(m_iTmpOriginalW + iAddedWidth); //SLT_DrawImageInFixedSlice will be called
		ui.sliderFixedL->setValue(m_iTmpOriginalL + iAddedLevel);
	  }
	  else
	  {
		ui.sliderMovingW->setValue(m_iTmpOriginalW + iAddedWidth);
		ui.sliderMovingL->setValue(m_iTmpOriginalL + iAddedLevel);
	  }
	}
  }
}

//Slide change by scrolling
void DlgRegistration::SLT_MouseWheelUpdate1()
{  
  if (ui.checkBoxZoom->isChecked())
  {
	double oldZoom = ui.labelOverlapWnd1->m_pYK16Image->m_fZoom;

	double fWeighting = 0.2;

	ui.labelOverlapWnd1->m_pYK16Image->SetZoom(oldZoom + ui.labelOverlapWnd1->m_iMouseWheelDelta * fWeighting);
	this->SLT_DrawImageInFixedSlice();
  }
  else
	ui.sliderPosDisp1->setValue(ui.sliderPosDisp1->value() + ui.labelOverlapWnd1->m_iMouseWheelDelta);

}

void DlgRegistration::SLT_MouseWheelUpdate2()
{
  if (ui.checkBoxZoom->isChecked())
  {
	double oldZoom = ui.labelOverlapWnd2->m_pYK16Image->m_fZoom;
	double fWeighting = 0.2;


	ui.labelOverlapWnd2->m_pYK16Image->SetZoom(oldZoom + ui.labelOverlapWnd2->m_iMouseWheelDelta * fWeighting);
	this->SLT_DrawImageInFixedSlice();

  }
  else
	ui.sliderPosDisp2->setValue(ui.sliderPosDisp2->value() + ui.labelOverlapWnd2->m_iMouseWheelDelta);
}

void DlgRegistration::SLT_MouseWheelUpdate3()
{
  if (ui.checkBoxZoom->isChecked())
  {
	double oldZoom = ui.labelOverlapWnd3->m_pYK16Image->m_fZoom;
	double fWeighting = 0.2;

	ui.labelOverlapWnd3->m_pYK16Image->SetZoom(oldZoom + ui.labelOverlapWnd3->m_iMouseWheelDelta * fWeighting);
	this->SLT_DrawImageInFixedSlice();

  }
  else
	ui.sliderPosDisp3->setValue(ui.sliderPosDisp3->value() + ui.labelOverlapWnd3->m_iMouseWheelDelta);
}

//release everything
void DlgRegistration::SLT_CancelMouseAction()
{
  for (int i = 0 ; i<3; i++)
  {
	m_bPressedLeft[i] = false;//Left Mouse Pressed but not released
	m_bPressedRight[i]= false;
  }
}

void DlgRegistration::SLT_MousePressedLeft1()
{   
  m_bPressedLeft[0] = true;
}

void DlgRegistration::SLT_MousePressedLeft2()
{ 
  m_bPressedLeft[1] = true;
}

void DlgRegistration::SLT_MousePressedLeft3()
{
  m_bPressedLeft[2] = true;
}


void DlgRegistration::MousePressedRight(int wndIdx, qyklabel* pWnd)
{
  m_bPressedRight[wndIdx] = true;

  if (ui.checkBoxPan->isChecked())
  {
	m_ptPanStart.setX(pWnd->x);
	m_ptPanStart.setY(pWnd->y);

	m_ptTmpOriginalDataOffset.setX(m_YKDisp[wndIdx].m_iOffsetX);
	m_ptTmpOriginalDataOffset.setY(m_YKDisp[wndIdx].m_iOffsetY);	
  }
  else
  {
	m_ptWindowLevelStart.setX(pWnd->x);
	m_ptWindowLevelStart.setY(pWnd->y);

	QPoint crntDataPt = pWnd->GetDataPtFromViewPt(m_ptWindowLevelStart.x(),  m_ptWindowLevelStart.y());

	if (m_YKDisp[wndIdx].isPtInFirstImage(crntDataPt.x(), crntDataPt.y()))
	{
	  m_iTmpOriginalL = ui.sliderFixedL->value();
	  m_iTmpOriginalW = ui.sliderFixedW->value();
	}
	else
	{
	  m_iTmpOriginalL = ui.sliderMovingL->value();
	  m_iTmpOriginalW = ui.sliderMovingW->value();
	}	
  }
}

void DlgRegistration::SLT_MousePressedRight1()
{
  int idx = 0;
  MousePressedRight(idx, ui.labelOverlapWnd1);
  

}

void DlgRegistration::SLT_MousePressedRight2()
{
  int idx = 1;
  MousePressedRight(idx, ui.labelOverlapWnd2); 
}

void DlgRegistration::SLT_MousePressedRight3()
{
  int idx = 2;
  MousePressedRight(idx, ui.labelOverlapWnd3);
  
}

void DlgRegistration::SLT_MouseReleasedLeft1()
{  
   m_bPressedLeft[0] = false;
}

void DlgRegistration::SLT_MouseReleasedLeft2()
{ 
  m_bPressedLeft[1] = false;
}

void DlgRegistration::SLT_MouseReleasedLeft3()
{ 
  m_bPressedLeft[2] = false;
}

void DlgRegistration::SLT_MouseReleasedRight1()
{
  m_bPressedRight[0] = false;
}

void DlgRegistration::SLT_MouseReleasedRight2()
{
  m_bPressedRight[1] = false;
}

void DlgRegistration::SLT_MouseReleasedRight3()
{
  m_bPressedRight[2] = false;
}

void DlgRegistration::SLT_ChangeView()
{
  m_enViewArrange = (m_enViewArrange +1)%3;

  YK16GrayImage tmpBufYK[3];

  for (int i = 0 ; i<3 ; i++)
  {
	tmpBufYK[i].CloneImage(m_YKDisp[i]);
  }

  for (int i = 0 ; i<3 ; i++)
  {
	int nextIdx = (i+1)%3;
	m_YKDisp[i].CloneImage(tmpBufYK[nextIdx]);
  }
 // /*if (m_enViewArrange > 2)
	//m_enViewArrange = m_enViewArrange-3;*/

 // //Split center should be transfered as well.
 // for (int i = 0 ; i<3 ; i++)
 // {
	//int nextIdx = (i+1)%3;
	///*if (nextIdx >= 3)
	//  nextIdx = nextIdx -3;*/
	//m_YKDisp[i].SetSplitCenter(m_YKDisp[i+1].m_ptSplitCenter);
	////no need of data copy, but other display information (zoom and others should be copied here.
 // }  
  initOverlapWndSize();
  shiftSliceSlider();
  updateSliceLabel();

  SLT_DrawImageWhenSliceChange(); //only image data will be updated. Zoom and other things are not.
  //SLT_DrawImageInFixedSlice();
}

void DlgRegistration::initOverlapWndSize()
{
  ui.labelOverlapWnd1->setFixedWidth(DEFAULT_LABEL_SIZE1);
  ui.labelOverlapWnd1->setFixedHeight(DEFAULT_LABEL_SIZE1);

  ui.labelOverlapWnd2->setFixedWidth(DEFAULT_LABEL_SIZE2);
  ui.labelOverlapWnd2->setFixedHeight(DEFAULT_LABEL_SIZE2);

  ui.labelOverlapWnd3->setFixedWidth(DEFAULT_LABEL_SIZE2);
  ui.labelOverlapWnd3->setFixedHeight(DEFAULT_LABEL_SIZE2);

}

void DlgRegistration::shiftSliceSlider() //shift one slice slider information
{
  //to avoid first unnecessary action.
  disconnect(ui.sliderPosDisp1, SIGNAL(valueChanged(int)), this, SLOT(SLT_DrawImageWhenSliceChange()));
  disconnect(ui.sliderPosDisp2, SIGNAL(valueChanged(int)), this, SLOT(SLT_DrawImageWhenSliceChange()));
  disconnect(ui.sliderPosDisp3, SIGNAL(valueChanged(int)), this, SLOT(SLT_DrawImageWhenSliceChange()));

  int crntMin[3];
  int crntMax[3];
  int crntValue[3];

  int newMin[3];
  int newMax[3];
  int newValue[3];

  crntMin[0] = ui.sliderPosDisp1->minimum();
  crntMin[1] = ui.sliderPosDisp2->minimum();
  crntMin[2] = ui.sliderPosDisp3->minimum();

  crntMax[0] = ui.sliderPosDisp1->maximum();
  crntMax[1] = ui.sliderPosDisp2->maximum();
  crntMax[2] = ui.sliderPosDisp3->maximum();

  crntValue[0] = ui.sliderPosDisp1->value();
  crntValue[1] = ui.sliderPosDisp2->value();
  crntValue[2] = ui.sliderPosDisp3->value();

  for (int i = 0 ; i<3 ; i++)
  {
	int newIdx = (i+1)%3;	
	newMin[i] = crntMin[newIdx];
	newMax[i] = crntMax[newIdx];
	newValue[i] = crntValue[newIdx];
  }

  ui.sliderPosDisp1->setMinimum(newMin[0]);
  ui.sliderPosDisp2->setMinimum(newMin[1]);
  ui.sliderPosDisp3->setMinimum(newMin[2]);

  ui.sliderPosDisp1->setMaximum(newMax[0]);
  ui.sliderPosDisp2->setMaximum(newMax[1]);
  ui.sliderPosDisp3->setMaximum(newMax[2]);

  ui.sliderPosDisp1->setValue(newValue[0]);
  ui.sliderPosDisp2->setValue(newValue[1]);
  ui.sliderPosDisp3->setValue(newValue[2]);

  connect(ui.sliderPosDisp1, SIGNAL(valueChanged(int)), this, SLOT(SLT_DrawImageWhenSliceChange()));
  connect(ui.sliderPosDisp2, SIGNAL(valueChanged(int)), this, SLOT(SLT_DrawImageWhenSliceChange()));
  connect(ui.sliderPosDisp3, SIGNAL(valueChanged(int)), this, SLOT(SLT_DrawImageWhenSliceChange()));
}

void DlgRegistration::updateSliceLabel()
{

  switch (m_enViewArrange)
  {
  case AXIAL_FRONTAL_SAGITTAL:
	ui.labelDisp1->setText("AXIAL");
	ui.labelDisp2->setText("FRONTAL");
	ui.labelDisp3->setText("SAGITTAL");
	break;

  case FRONTAL_SAGITTAL_AXIAL:
	ui.labelDisp1->setText("FRONTAL");
	ui.labelDisp2->setText("SAGITTAL");
	ui.labelDisp3->setText("AXIAL");
	break;

  case SAGITTAL_AXIAL_FRONTAL:
	ui.labelDisp1->setText("SAGITTAL");
	ui.labelDisp2->setText("AXIAL");
	ui.labelDisp3->setText("FRONTAL");
	break;
  }
}

void DlgRegistration::SLT_RestoreImageSingle()
{
  int mainWndIdx = 0;
  m_YKDisp[mainWndIdx].SetZoom(1.0);
  m_YKDisp[mainWndIdx].SetOffset(0,0);

  SLT_DrawImageInFixedSlice();
}

void DlgRegistration::SLT_RestoreImageAll()
{
  for (int i = 0 ; i<3 ; i++)
  {
	m_YKDisp[i].SetZoom(1.0);
	m_YKDisp[i].SetOffset(0,0);

	SLT_DrawImageInFixedSlice();
  }
}
//Auto registration
void DlgRegistration::SLT_DoRegistrationRigid()//plastimatch auto registration
{
  //1) Save current image files
  if (!m_spFixed || !m_spMoving)
	return;

  //Before the registration, remove background and fill the air-bubble of the cbct
  //1) calculate manual shift value
  //PreProcess_CBCT(); // remove skin and fill bubble before registration

  if (!m_pParent->m_spRefCTImg || !m_pParent->m_spManualRigidCT)
	return;


  if (ui.checkBoxKeyMoving->isChecked())
	  SLT_KeyMoving(false);


  /*- Make a synthetic vector field according to the translation
	plastimatch synth-vf --fixed [msk_skin.mha] --output [xf_manual_trans.mha] --xf-trans "[origin diff (raw - regi)]"
	plastimatch synth-vf --fixed E:\PlastimatchData\DicomEg\OLD\msk_skin.mha --output E:\PlastimatchData\DicomEg\OLD\xf_manual_trans.mha --xf-trans "21 -29 1"

	- Move the skin contour according to the vector field
	plastimatch warp --input [msk_skin.mha] --output-img [msk_skin_manRegi.mha] --xf [xf_manual_trans.mha]
  plastimatch warp --input E:\PlastimatchData\DicomEg\OLD\msk_skin.mha --output-img E:\PlastimatchData\DicomEg\OLD\msk_skin_manRegi.mha --xf E:\PlastimatchData\DicomEg\OLD\xf_manual_trans.mha*/

  bool bPrepareMaskOnly; //prepare mask but not apply

  if (ui.checkBoxCropBkgroundCBCT->isChecked())
      bPrepareMaskOnly = false;
  else
      bPrepareMaskOnly = true;


  cout << "1: writing temporary files" << endl;

  //Both image type: Unsigned Short
  QString filePathFixed = m_strPathPlastimatch + "/" + "fixed_rigid.mha";
  QString filePathMoving = m_strPathPlastimatch + "/" + "moving_rigid.mha";
  QString filePathOutput = m_strPathPlastimatch + "/" + "output_rigid.mha";
  QString filePathXform = m_strPathPlastimatch + "/" + "xform_rigid.txt";
  QString filePathROI = m_strPathPlastimatch + "/" + "fixed_roi_rigid.mha"; //optional

  typedef itk::ImageFileWriter<USHORT_ImageType> writerType;

  writerType::Pointer writer1 = writerType::New();
  writer1->SetFileName(filePathFixed.toLocal8Bit().constData());
  writer1->SetUseCompression(true);
  writer1->SetInput(m_spFixed);

  writerType::Pointer writer2 = writerType::New();
  writer2->SetFileName(filePathMoving.toLocal8Bit().constData());
  writer2->SetUseCompression(true);
  writer2->SetInput(m_spMoving);

  writer1->Update();
  writer2->Update();

  float FOV_DcmPosX = 0.0; //mm
  float FOV_DcmPosY = 0.0;//mm
  float FOV_Radius = 0.0;

  if (ui.checkBoxUseROIForRigid->isChecked())
  {
      cout << "Creating a ROI mask for Rigid registration " << endl;
      QString strFOVGeom = ui.lineEditFOVPos->text();

      QStringList strListFOV = strFOVGeom.split(",");
      if (strListFOV.count() == 3)
      {
          FOV_DcmPosX = strListFOV.at(0).toFloat();
          FOV_DcmPosY = strListFOV.at(1).toFloat();
          FOV_Radius = strListFOV.at(2).toFloat();

          //Create Image using FixedImage sp

          //Image Pointer here
          USHORT_ImageType::Pointer spRoiMask;
          m_pParent->AllocateByRef(m_spFixed, spRoiMask);
          m_pParent->GenerateCylinderMask(spRoiMask, FOV_DcmPosX, FOV_DcmPosY, FOV_Radius);

          writerType::Pointer writer3 = writerType::New();
          writer3->SetFileName(filePathROI.toLocal8Bit().constData());
          writer3->SetUseCompression(true);
          writer3->SetInput(spRoiMask);
          writer3->Update();
      }
  }

  //Preprocessing
  //2) move CT-based skin mask on CBCT based on manual shift
//  if (m_strPathCTSkin)

  //m_strPathCTSkin is prepared after PreProcessCT()  

  //QString filePathFixed_proc = filePathFixed;


  typedef itk::ImageFileReader<USHORT_ImageType> readerType;


  QString strPathOriginalCTSkinMask;

  QString filePathFixed_proc = m_strPathPlastimatch + "/" + "fixed_rigid_proc.mha"; //After autoRigidbody Regi  
  
  QString strPath_mskSkinCT_manRegi_exp = m_strPathPlastimatch + "/msk_skin_CT_manRegi_exp.mha";//proof of preproceesing for CBCT

  QFileInfo finfoFixedProc = QFileInfo(filePathFixed_proc);
  QFileInfo finfoManMask = QFileInfo(strPath_mskSkinCT_manRegi_exp);

  //if this file already exists, no need of proprocess for CBCT (skin cropping) is needed.
//if skin cropping is not done for this case (supposed to be done during confirm manual regi)

 

  //if (!finfoFixedProc.exists() && !finfoManMask.exists() && ui.checkBoxCropBkgroundCBCT->isChecked())
  if (!finfoFixedProc.exists() && !finfoManMask.exists())
  {
      cout << "Preprocessing for CBCT is not done. It is being done here before rigid body registration" << endl;

      USHORT_ImageType::PointType originBefore = m_pParent->m_spRefCTImg->GetOrigin();
      USHORT_ImageType::PointType originAfter = m_pParent->m_spManualRigidCT->GetOrigin();

      double fShift[3];
      fShift[0] = (double)(originBefore[0] - originAfter[0]);//DICOM
      fShift[1] = (double)(originBefore[1] - originAfter[1]);
      fShift[2] = (double)(originBefore[2] - originAfter[2]);

      QFileInfo finfoSkinFile1 = QFileInfo(m_strPathCTSkin);
      QString strPathAlternateSkin = m_strPathPlastimatch + "/" + "msk_skin_CT.mha";
      QFileInfo finfoSkinFile2 = QFileInfo(strPathAlternateSkin);


      //&& ui.checkBoxCropBkgroundCBCT->isChecked()

      if (finfoSkinFile1.exists())
      {
          strPathOriginalCTSkinMask = m_strPathCTSkin;          
          //This was OK.
          ProcessCBCT_beforeAutoRigidRegi(filePathFixed, strPathOriginalCTSkinMask, filePathFixed_proc, fShift, bPrepareMaskOnly);          

          if (bPrepareMaskOnly) // currently, filePathFixed_proc == "";
              filePathFixed_proc = filePathFixed;
      }
      else if (finfoSkinFile2.exists())
      {
          cout << "alternative skin file will be used" << endl;
          strPathOriginalCTSkinMask = strPathAlternateSkin;          
          ProcessCBCT_beforeAutoRigidRegi(filePathFixed, strPathOriginalCTSkinMask, filePathFixed_proc, fShift, bPrepareMaskOnly);

          if (bPrepareMaskOnly)
              filePathFixed_proc = filePathFixed;
      }
      else
      {
          cout << "CT skin file(msk_skin_CT.mha) is not found. preprocessing will be skipped" << endl;          
          filePathFixed_proc = filePathFixed;
      }

      QFileInfo fixedProcInfo(filePathFixed_proc);
      if (!fixedProcInfo.exists())
      {
          cout << "Error! proc file doesn't exist!" << endl;
          return;
      }

      readerType::Pointer reader2 = readerType::New();
      reader2->SetFileName(filePathFixed_proc.toLocal8Bit().constData());
      reader2->Update();
      m_pParent->m_spRawReconImg = reader2->GetOutput();

      double tmpSkinMargin = ui.lineEditCBCTSkinCropBfRegid->text().toDouble();
      QString QSmarg = QString("Skin removed CBCT with margin %1 mm");
      QString QSmargarg = QSmarg.arg(tmpSkinMargin);
      m_pParent->UpdateReconImage(m_pParent->m_spRawReconImg, QSmarg);
  }

  if (!finfoFixedProc.exists())
      filePathFixed_proc = filePathFixed; //"fixed_rigid.mha";

  cout << "2: Creating a plastimatch command file" << endl;

  QString fnCmdRegisterRigid = "cmd_register_rigid.txt";
  //QString fnCmdRegisterDeform = "cmd_register_deform.txt";
  QString pathCmdRegister = m_strPathPlastimatch + "/" + fnCmdRegisterRigid;

  //GenPlastiRegisterCommandFile(pathCmdRegister, filePathFixed_proc, filePathMoving, 
	//						filePathOutput, filePathXform, PLAST_RIGID, "","","");  

  QString strDummy = "";
  //For Cropped patients, FOV mask is applied.
  if (ui.checkBoxUseROIForRigid->isChecked())
  {
      cout << "2.A:  ROI-based Rigid body registration will be done" << endl;
      GenPlastiRegisterCommandFile(pathCmdRegister, filePathFixed_proc, filePathMoving,
          filePathOutput, filePathXform, PLAST_RIGID, strDummy, strDummy, strDummy, filePathROI);
  }
  else
  {
      GenPlastiRegisterCommandFile(pathCmdRegister, filePathFixed_proc, filePathMoving,
          filePathOutput, filePathXform, PLAST_RIGID, strDummy, strDummy, strDummy);
  }

  //Sleep(1000);

	/*void DlgRegistration::GenPlastiRegisterCommandFile(QString strPathCommandFile, QString strPathFixedImg, QString strPathMovingImg,
	QString strPathOutImg, QString strPathXformOut, enRegisterOption regiOption,
	QString strStageOption1, , QString strStageOption2, QString strStageOption3)*/
 

  //const char *command_filepath = pathCmdRegister.toLocal8Bit().constData();
  std::string str_command_filepath = pathCmdRegister.toLocal8Bit().constData();

  cout << "3: calling a plastimatch command" << endl;

 
  Registration reg;
  if (reg.set_command_file(str_command_filepath) < 0) {
	printf ("Error.  could not load %s as command file.\n", 
            str_command_filepath.c_str());
  }  
  //cout << "command file path is set= " << pathCmdRegister.toLocal8Bit().constData() << endl;

  if (pathCmdRegister.length() < 3)
  {
      cout << "ERROR! pathCmdRegister is too short!" << endl;
      return;
  }

  std::string strFixed = reg.get_registration_parms()->get_fixed_fn(); //  return d_ptr->rparms;
  std::string strMoving = reg.get_registration_parms()->get_moving_fn();  

  if (strFixed.length() < 1)
  {
      cout << endl;
      cout << endl;
      cout << endl;
      cout << "ERROR! no fixed image" << endl;      
      //return;
  }
  if (strMoving.length() < 1)
  {
      cout << endl;
      cout << endl;
      cout << endl;
      cout << "ERROR!no moving image" << endl;
      //return;
  }

  ////Check that the command file is readable
  //ifstream fin;
  //fin.open(command_filepath);
  //if (fin.fail())
  //{

  //    cout << endl;
  //    cout << endl;
  //    cout << endl;

  //    fin.close();
  //    cout << "fail first.. wait 5 s" << endl;
  //    Sleep(5000);

  //    fin.open(command_filepath);
  //    if (fin.fail())
  //    {
  //        cout << endl;
  //        cout << endl;
  //        cout << endl;

  //        cout << "Second failure! Error! " << endl;
  //        fin.close();
  //    }
  //    else
  //    {
  //        cout << "Resolved after a single failure!" << endl;
  //        fin.close();
  //    }
  //}
  //else
  //{
  //    cout << "File is readable!" << endl;
  //    fin.close();
  //}

 /* if (reg.set_command_file(command_filepath) < 0) {
      printf("Error.  could not load %s as command file.\n",
          command_filepath);
  }
  cout << "command file path is set= " << pathCmdRegister.toLocal8Bit().constData() << endl;*/


  reg.do_registration ();//error occurs here

  cout << "4: Registration is done" << endl;
  cout << "5: Reading output image-CT" << endl;


  readerType::Pointer reader = readerType::New();
  reader->SetFileName(filePathOutput.toLocal8Bit().constData());		
  reader->Update();
  m_pParent->m_spAutoRigidCT = reader->GetOutput(); 

  /*readerType::Pointer reader2 = readerType::New();
  reader2->SetFileName(filePathFixed_proc.toLocal8Bit().constData());		
  reader2->Update();
  m_pParent->m_spReconImg = reader2->GetOutput();*/

  cout << "6: Reading is completed" << endl;

  UpdateListOfComboBox(0);//combo selection signalis called
  UpdateListOfComboBox(1);
  
  SelectComboExternal(0, REGISTER_RAW_CBCT); // will call fixedImageSelected 
  SelectComboExternal(1, REGISTER_AUTO_RIGID );


  m_strPathXFAutoRigid = filePathXform; //for further use
}

bool DlgRegistration::eventFilter( QObject *target, QEvent *event )
{
  if (event->type() == QEvent::Paint) //eventy->type() = 12 if paint event
	return false;

  //int aa = (int)(event->type());

  //cout << target << endl;
  //cout <<"Event Type " << aa << endl;

  if (event->type() == QEvent::ShortcutOverride)//51
  {	
	if (!ui.checkBoxKeyMoving->isChecked())
	  return false;

	QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

	int iArrowKey = (int)keyEvent->nativeVirtualKey();
	//cout << iArrowKey << endl;
	double resol = ui.lineEditMovingResol->text().toDouble(); //mm

	switch (iArrowKey)
	{
	//case Qt::Key_Left: //37
	 case 37: //37
	  ImageManualMove(37, resol);
	  break;
	case 38: //38
	  ImageManualMove(38, resol);//up
	  break;
	case 39: //39
	  ImageManualMove(39, resol);
	  break;
	case 40: //40
	  ImageManualMove(40, resol);
	  break;
	case 33: //
	  ImageManualMove(33, resol);
	  break;
	case 34: //
	  ImageManualMove(34, resol);
	  break;
	default:
	  break;//do nothing
	}	
	//cout << keyEvent->->nativeVirtualKey() << endl;
	//event->accept();	
	return true; //No more event transfering?
  } 
  /*else if (event->type() == QEvent::KeyPress)
  {
	cout << "key pressed" << endl;
  }
  else if (event->type() == QEvent::KeyRelease)
  {
	cout << "key Released" << endl;
  }*/
  
  //return true;
  return QDialog::eventFilter(target, event);//This is mandatory to deliver the event signal to the children components
}

void DlgRegistration::childEvent( QChildEvent *event )
{
  //cout << "childEvent Called" << endl;
  if (event->added()) {
	event->child()->installEventFilter(this);
  }
}

void DlgRegistration::ImageManualMove( int direction, double resol )
{
  if (!m_spMoving)
	return;  

  //USHORT_ImageType::SizeType imgSize = m_spMoving->GetRequestedRegion().GetSize(); //1016x1016 x z
  USHORT_ImageType::PointType imgOrigin = m_spMoving->GetOrigin();
  //USHORT_ImageType::SpacingType imgSpacing = m_spFixed->GetSpacing();  

  if (direction == 37)  //LEFT
	imgOrigin[0] = imgOrigin[0]-resol;   //mm unit
  else if (direction == 39) //RIGHT
	imgOrigin[0] = imgOrigin[0]+resol;  
  else if (direction == 38) //Up
	imgOrigin[1] = imgOrigin[1]-resol;  
  else if (direction == 40)//DOWN
	imgOrigin[1] = imgOrigin[1]+resol;  
  else if (direction == 33) //PageUp
	imgOrigin[2] = imgOrigin[2]+resol;  
  else if (direction == 34) //PageDown
	imgOrigin[2] = imgOrigin[2]-resol;

  m_spMoving->SetOrigin(imgOrigin);

  SLT_DrawImageWhenSliceChange();

  //Display relative movement
  //Starting point? RefCT image
  //Only Valid when Moving image is the ManualMove
  USHORT_ImageType::PointType imgOriginRef = m_pParent->m_spRefCTImg->GetOrigin(); 

  QString strDelta;
  strDelta.sprintf("delta(mm): %3.1f, %3.1f, %3.1f", (double)(imgOrigin[0]- imgOriginRef[0]),
	(double)(imgOrigin[1]- imgOriginRef[1]),
	(double)(imgOrigin[2]- imgOriginRef[2]));
  ui.lineEditOriginChanged->setText(strDelta);  
  
  //cout << "direction  " << direction << endl;
  //cout << "resolution " << resol << endl;
}

//change origin of moving image by shift value acquired from gradient registration
void DlgRegistration::ImageManualMoveOneShot(float shiftX, float shiftY, float shiftZ )//DICOM coordinate
{
    if (!m_spMoving)
        return;

    //USHORT_ImageType::SizeType imgSize = m_spMoving->GetRequestedRegion().GetSize(); //1016x1016 x z
    USHORT_ImageType::PointType imgOrigin = m_spMoving->GetOrigin();
    //USHORT_ImageType::SpacingType imgSpacing = m_spFixed->GetSpacing();  
    imgOrigin[0] = imgOrigin[0] - shiftX;
    imgOrigin[1] = imgOrigin[1] - shiftY;
    imgOrigin[2] = imgOrigin[2] - shiftZ;

    m_spMoving->SetOrigin(imgOrigin);

    SLT_DrawImageWhenSliceChange();

    //Display relative movement
    //Starting point? RefCT image
    //Only Valid when Moving image is the ManualMove
    USHORT_ImageType::PointType imgOriginRef = m_pParent->m_spRefCTImg->GetOrigin();

    QString strDelta;
    strDelta.sprintf("delta(mm): %3.1f, %3.1f, %3.1f", (double)(imgOrigin[0] - imgOriginRef[0]),
        (double)(imgOrigin[1] - imgOriginRef[1]),
        (double)(imgOrigin[2] - imgOriginRef[2]));
    ui.lineEditOriginChanged->setText(strDelta);    
}

//Bring Focus
void DlgRegistration::SLT_BringFocusToEnableArrow(bool bChecked)
{
  if (bChecked)
	ui.labelDisp1->setFocus(); //if focus is in label, Key Event will trigger 51 (override)
}

void DlgRegistration::SLT_KeyMoving( bool bChecked )//Key Moving check box
{
  ui.lineEditMovingResol->setDisabled(bChecked);
  if  (bChecked)
  {
	SelectComboExternal(1,REGISTER_MANUAL_RIGID);//should be
  }
  ui.comboBoxImgFixed->setDisabled(bChecked);
  ui.comboBoxImgMoving->setDisabled(bChecked);
  ui.pushButtonRestoreOriginal->setDisabled(bChecked);
}

void DlgRegistration::GenPlastiRegisterCommandFile(QString strPathCommandFile, QString strPathFixedImg, QString strPathMovingImg,
												   QString strPathOutImg, QString strPathXformOut, enRegisterOption regiOption,
												   QString strStageOption1, QString strStageOption2, QString strStageOption3, const QString& strPathFixedMask)
{
  ofstream fout;
  fout.open(strPathCommandFile.toLocal8Bit().constData());

  if (fout.fail())
  {
      cout << "File writing error! " << endl;
      return;
  }

  fout << "# command_file.txt" << endl;
  fout << "[GLOBAL]" << endl;
  fout << "fixed=" << strPathFixedImg.toLocal8Bit().constData() << endl;
  fout << "moving=" << strPathMovingImg.toLocal8Bit().constData() << endl;

  if (strPathFixedMask.length() > 1)
  {
      fout << "fixed_roi=" << strPathFixedMask.toLocal8Bit().constData() << endl;
  }

  fout << "img_out=" << strPathOutImg.toLocal8Bit().constData() << endl;
  fout << "xform_out=" << strPathXformOut.toLocal8Bit().constData() << endl;
  fout << endl;

//  int idx = ui.comboBoxDeformOption->currentIndex();
  QString strOptim = ui.comboBoxDeformOption->currentText();

  QStringList strListOption1,strListOption2, strListOption3;
  strListOption1 = strStageOption1.split(",");  //Subsampling rate (3), Grid size (1), Regularization(1), LandmarkPenalty(1), Max Iteration (1), StageOutput(1)
  strListOption2 = strStageOption2.split(",");
  strListOption3 = strStageOption3.split(",");

  QString optionStr;
  QStringList optionList;

  switch (regiOption)
  {
  case PLAST_RIGID:
	fout << "[STAGE]" << endl;
	fout << "xform=" << "rigid" << endl;	
	//fout << "xform=" << "affine" << endl;
	fout << "optim=" << "versor" << endl;
	//fout << "max_step=" << "0.05" << endl;
	//fout << "optim=" << "amoeba" << endl;
	//fout << "optim=" << "rsg" << endl;	
	//fout << "optim=" << "amoeba" << endl;
	//fout << "impl=" << "plastimatch" << endl;
	//fout << "metric=" << "mi" << endl; //added
	//fout << "impl=" << "itk" << endl;
	
	fout << "impl=" << "itk" << endl;
	fout << "threading=" << "openmp" << endl;
	//fout << "background_val=" << "0" << endl;
	fout << "background_val=" << "500" << endl; //-600 in HU //added
	fout << "max_its=" << "70" << endl;

	break;
  case PLAST_GRADIENT:
      fout << "#For gradient-based searching, moving image should be smaller than fixed image. So, CBCT image might move rather than CT" << endl;

      optionStr = ui.lineEditGradOption->text();
      optionList = optionStr.split(",");

      fout << "[PROCESS]" << endl;
      fout << "action=adjust" << endl;
      fout << "# only consider within this  intensity values" << endl;
      fout << "parms=-inf,0,-1000,-1000,4000,4000,inf,0" << endl;
      fout << "images=fixed,moving" << endl;              
      fout << endl;
      fout << "[STAGE]" << endl;
      fout << "metric=gm" << endl;          
      fout << "xform=translation" << endl;
      fout << "optim=grid_search" << endl;          
      fout << "gridsearch_min_overlap=" << optionList.at(0).toDouble() << " "
          << optionList.at(1).toDouble() << " "
          << optionList.at(2).toDouble() << endl;
               
      fout << "num_substages=5" << endl;
      fout << "debug_dir=" << m_strPathPlastimatch.toLocal8Bit().constData() << endl;
      break;

  case PLAST_BSPLINE:
	if (strListOption1.count() == 8)
	{
	  fout << "[STAGE]" << endl;
	  fout << "xform=" << "bspline" << endl;
	  fout << "impl=" << "plastimatch" << endl;
	  //fout <<"threading=" << "openmp" << endl;
	  //fout << "threading=" << "cuda" << endl;
	  fout << "threading=" << "openmp" << endl;
	  
	  fout << "regularization_lambda=" << strListOption1.at(4).toDouble() << endl;

	  if (strOptim.length() < 1)
		fout << "metric=" << "mi" << endl;
	  else
		fout << "metric=" << strOptim.toLocal8Bit().constData() << endl;

	  fout << "max_its=" << strListOption1.at(6).toInt() << endl;
	  fout << "grid_spac=" << strListOption1.at(3).toInt() << " " << strListOption1.at(3).toInt() << " " << strListOption1.at(3).toInt() << endl;//20 20 20 --> minimum
	  fout << "res=" << strListOption1.at(0).toInt() << " " << strListOption1.at(1).toInt() << " " << strListOption1.at(2).toInt() << endl;
	  fout << "background_val=" << "700" << endl; //-600 in HU //added

	  fout << "img_out=" <<strListOption1.at(7).toLocal8Bit().constData() << endl;
	  fout << endl;
	}
	if (strListOption2.count() == 8)
	{
	  fout << "[STAGE]" << endl;
	  fout << "xform=" << "bspline" << endl;
	  fout << "impl=" << "plastimatch" << endl;
	  fout <<"threading=" << "openmp" << endl;

	  fout << "regularization_lambda=" << strListOption2.at(4).toDouble() << endl;

	  if (strOptim.length() < 1)
		fout << "metric=" << "mi" << endl;
	  else
		fout << "metric=" << strOptim.toLocal8Bit().constData() << endl;

	  fout << "max_its=" << strListOption2.at(6).toInt() << endl;
	  fout << "grid_spac=" << strListOption2.at(3).toInt() << " " << strListOption2.at(3).toInt() << " " << strListOption2.at(3).toInt() << endl;//20 20 20 --> minimum
	  fout << "res=" << strListOption2.at(0).toInt() << " " << strListOption2.at(1).toInt() << " " << strListOption2.at(2).toInt() << endl;

	  fout << "img_out=" <<strListOption2.at(7).toLocal8Bit().constData() << endl;
	  fout << endl;
	}
	if (strListOption3.count() == 8)
	{
	  fout << "[STAGE]" << endl;
	  fout << "xform=" << "bspline" << endl;
	  fout << "impl=" << "plastimatch" << endl;
	  fout <<"threading=" << "openmp" << endl;

	  fout << "regularization_lambda=" << strListOption3.at(4).toDouble() << endl;

	  if (strOptim.length() < 1)
		fout << "metric=" << "mi" << endl;
	  else
		fout << "metric=" << strOptim.toLocal8Bit().constData() << endl;

	  fout << "max_its=" << strListOption3.at(6).toInt() << endl;
	  fout << "grid_spac=" << strListOption3.at(3).toInt() << " " << strListOption3.at(3).toInt() << " " << strListOption3.at(3).toInt() << endl;//20 20 20 --> minimum
	  fout << "res=" << strListOption3.at(0).toInt() << " " << strListOption3.at(1).toInt() << " " << strListOption3.at(2).toInt() << endl;

	  fout << "img_out=" <<strListOption3.at(7).toLocal8Bit().constData() << endl;
	  fout << endl;
	}
	
	break;
  }

  fout.close();
  //Sleep(1000); //Just in case.. it seems it helped to avoid random crash!
}

void DlgRegistration::AddImageToCombo(int comboIdx, enREGI_IMAGES option) //comboIdx 0: fixed, 1: moving
{
  switch(option)
  {
  case REGISTER_RAW_CBCT:
	if (m_pParent->m_spRawReconImg)
	{
	  if (comboIdx == 0)
		ui.comboBoxImgFixed->addItem("RAW_CBCT");
	  else if (comboIdx ==1)
		ui.comboBoxImgMoving->addItem("RAW_CBCT");
	}
	break;
  case REGISTER_REF_CT:
	if (m_pParent->m_spRefCTImg)
	{
	  if (comboIdx == 0)
		ui.comboBoxImgFixed->addItem("REF_CT");
	  else if (comboIdx ==1)
		ui.comboBoxImgMoving->addItem("REF_CT");
	}
	break;
  case REGISTER_MANUAL_RIGID:
	if (m_pParent->m_spManualRigidCT)
	{
	  if (comboIdx == 0)
		ui.comboBoxImgFixed->addItem("MANUAL_RIGID_CT");
	  else if (comboIdx ==1)
		ui.comboBoxImgMoving->addItem("MANUAL_RIGID_CT");
	}
	break;
  case REGISTER_AUTO_RIGID:
	if (m_pParent->m_spAutoRigidCT)
	{
	  if (comboIdx == 0)
		ui.comboBoxImgFixed->addItem("AUTO_RIGID_CT");
	  else if (comboIdx ==1)
		ui.comboBoxImgMoving->addItem("AUTO_RIGID_CT");
	}
	break;
  case  REGISTER_DEFORM1:
	if (m_pParent->m_spDeformedCT1)
	{
	  if (comboIdx == 0)
		ui.comboBoxImgFixed->addItem("DEFORMED_CT1");
	  else if (comboIdx ==1)
		ui.comboBoxImgMoving->addItem("DEFORMED_CT1");
	}
	break;
  case  REGISTER_DEFORM2:
	if (m_pParent->m_spDeformedCT2)
	{
	  if (comboIdx == 0)
		ui.comboBoxImgFixed->addItem("DEFORMED_CT2");
	  else if (comboIdx ==1)
		ui.comboBoxImgMoving->addItem("DEFORMED_CT2");
	}
	break;
  case  REGISTER_DEFORM3:
	if (m_pParent->m_spDeformedCT3)
	{
	  if (comboIdx == 0)
		ui.comboBoxImgFixed->addItem("DEFORMED_CT3");
	  else if (comboIdx ==1)
		ui.comboBoxImgMoving->addItem("DEFORMED_CT3");
	}
	break;

  case  REGISTER_DEFORM_FINAL:
	if (m_pParent->m_spDeformedCT_Final)
	{
	  if (comboIdx == 0)
		ui.comboBoxImgFixed->addItem("DEFORMED_CT_FINAL");
	  else if (comboIdx ==1)
		ui.comboBoxImgMoving->addItem("DEFORMED_CT_FINAL");
	}
	break;
  case  REGISTER_COR_CBCT:
	if (m_pParent->m_spScatCorrReconImg)
	{
	  if (comboIdx == 0)
		ui.comboBoxImgFixed->addItem("COR_CBCT");
	  else if (comboIdx ==1)
		ui.comboBoxImgMoving->addItem("COR_CBCT");
	}
	break;
  }
}

//externally change  combo box value 
void DlgRegistration::SelectComboExternal(int idx, enREGI_IMAGES iImage)
{
  QComboBox* crntCombo;

  if (idx  ==0)
	crntCombo = ui.comboBoxImgFixed;
  if (idx  ==1)
	crntCombo = ui.comboBoxImgMoving;


  int findIndx = -1;
  switch  (iImage)
  {
	 case REGISTER_RAW_CBCT:
	   findIndx = crntCombo->findText("RAW_CBCT");	   
	   break;
	 case REGISTER_REF_CT:
	   findIndx = crntCombo->findText("REF_CT");	   
	   break;
	 case REGISTER_MANUAL_RIGID:
	   findIndx = crntCombo->findText("MANUAL_RIGID_CT");	   
	   break;
	 case REGISTER_AUTO_RIGID:
	   findIndx = crntCombo->findText("AUTO_RIGID_CT");	   
	   break;
	 case  REGISTER_DEFORM1:
	   findIndx = crntCombo->findText("DEFORMED_CT1");	   
	   break;
	 case  REGISTER_DEFORM2:
	   findIndx = crntCombo->findText("DEFORMED_CT2");	   
	   break;
	 case  REGISTER_DEFORM3:
	   findIndx = crntCombo->findText("DEFORMED_CT3");	   
	   break;
	 case  REGISTER_DEFORM_FINAL:
	   findIndx = crntCombo->findText("DEFORMED_CT_FINAL");	   
	   break;
	 case  REGISTER_COR_CBCT:
	   findIndx = crntCombo->findText("COR_CBCT");	   
	   break;
  }
  //cout << "setCurrentIndx " << findIndx << endl;  

  if (findIndx < 0) //-1 if not found
	return;

  crntCombo->setCurrentIndex(findIndx);//will call "SLT_FixedImageSelected" 

  //Below are actually redendency. don't know why setCurrentIndex sometimes doesn't trigger the SLT_MovingImageSelected slot func.

  QString crntStr = crntCombo->currentText();
  if (idx == 0)
	SLT_FixedImageSelected(crntStr);
  else if (idx == 1)
	SLT_MovingImageSelected(crntStr);
  //If it is called inside the Dlg, SLT seemed not to conneced
}

void DlgRegistration::SLT_FixedImageSelected(QString selText)
{
  //QString strCrntText = ui.comboBoxImgFixed->currentText();
  LoadImgFromComboBox(0, selText); // here, m_spMoving and Fixed images are determined
}

void DlgRegistration::SLT_MovingImageSelected(QString selText)
{
  //QString strCrntText = ui.comboBoxImgMoving->currentText();
  //cout << "SLT_MovingImageSelected" << endl;
  LoadImgFromComboBox(1, selText); 
}


void DlgRegistration::LoadImgFromComboBox(int idx, QString& strSelectedComboTxt)// -->when fixed image loaded will be called here!
{
  //cout << "LoadImgFromComboBox " << "index " << idx << "text " << strSelectedComboTxt.toLocal8Bit().constData() << endl;

  USHORT_ImageType::Pointer spTmpImg;
  if (strSelectedComboTxt.compare(QString("RAW_CBCT"), Qt::CaseSensitive) == 0)
  {
	spTmpImg = m_pParent->m_spRawReconImg;
  }
  else if (strSelectedComboTxt.compare(QString("REF_CT"), Qt::CaseSensitive) == 0)
  {
	spTmpImg = m_pParent->m_spRefCTImg;
  }
  else if (strSelectedComboTxt.compare(QString("MANUAL_RIGID_CT"), Qt::CaseSensitive) == 0)
  {
	spTmpImg = m_pParent->m_spManualRigidCT;
  }
  else if (strSelectedComboTxt.compare(QString("AUTO_RIGID_CT"), Qt::CaseSensitive) == 0)
  {
	spTmpImg = m_pParent->m_spAutoRigidCT;
  }
  else if (strSelectedComboTxt.compare(QString("DEFORMED_CT1"), Qt::CaseSensitive) == 0)
  {
	spTmpImg = m_pParent->m_spDeformedCT1;
  } 
  else if (strSelectedComboTxt.compare(QString("DEFORMED_CT2"), Qt::CaseSensitive) == 0)
  {
	spTmpImg = m_pParent->m_spDeformedCT2;
  } 
  else if (strSelectedComboTxt.compare(QString("DEFORMED_CT3"), Qt::CaseSensitive) == 0)
  {
	spTmpImg = m_pParent->m_spDeformedCT3;
  } 
  else if (strSelectedComboTxt.compare(QString("DEFORMED_CT_FINAL"), Qt::CaseSensitive) == 0)
  {
	spTmpImg = m_pParent->m_spDeformedCT_Final;
  }
  else if (strSelectedComboTxt.compare(QString("COR_CBCT"), Qt::CaseSensitive) == 0)
  {
	spTmpImg = m_pParent->m_spScatCorrReconImg;
  }

  if (!spTmpImg)
  {
	//cout << "Selected image is not ready: " << strSelectedComboTxt.toLocal8Bit().constData() << endl;
	return;
  }
  
  if (idx == 0)
  {
	m_spFixed = spTmpImg;

	whenFixedImgLoaded();
  }
  else if (idx ==1)
  {
	m_spMoving = spTmpImg;  	
	//cout << "idx: " << idx << "m_spMoving"  << m_spMoving << endl;
	whenMovingImgLoaded();
  }

  SLT_DrawImageWhenSliceChange(); 
}

//search  for the  main data, if there  is, add  the predefined name to the combobox  
void DlgRegistration::UpdateListOfComboBox( int idx )
{
  QComboBox* crntCombo; 

  if (idx == 0)  
	crntCombo = ui.comboBoxImgFixed;  
  else
	crntCombo = ui.comboBoxImgMoving;

  //remove all the list
  crntCombo->clear();

  if (m_pParent->m_spRawReconImg)
	crntCombo->addItem("RAW_CBCT");

  if (m_pParent->m_spRefCTImg)
	crntCombo->addItem("REF_CT");

  if (m_pParent->m_spManualRigidCT)
	crntCombo->addItem("MANUAL_RIGID_CT");

  if (m_pParent->m_spAutoRigidCT)
	crntCombo->addItem("AUTO_RIGID_CT");

  if (m_pParent->m_spDeformedCT1)
	crntCombo->addItem("DEFORMED_CT1");

  if (m_pParent->m_spDeformedCT2)
		crntCombo->addItem("DEFORMED_CT2");

  if (m_pParent->m_spDeformedCT3)
	  crntCombo->addItem("DEFORMED_CT3");

  if (m_pParent->m_spDeformedCT_Final)
	crntCombo->addItem("DEFORMED_CT_FINAL");

  if (m_pParent->m_spScatCorrReconImg)
	crntCombo->addItem("COR_CBCT");
}

void DlgRegistration::SLT_RestoreMovingImg()
{
  m_pParent->RegisterImgDuplication(REGISTER_REF_CT, REGISTER_MANUAL_RIGID);  
  ui.lineEditOriginChanged->setText(QString(""));

  SelectComboExternal(1, REGISTER_MANUAL_RIGID);
}

void DlgRegistration::SLT_DoRegistrationDeform()
{  
 if (!m_spFixed || !m_spMoving)
	return;

 bool bPrepareMaskOnly = false;

 if (ui.checkBoxCropBkgroundCBCT->isChecked())
     bPrepareMaskOnly = false;
 else
     bPrepareMaskOnly = true;




 cout << "1: DoRegistrationDeform: writing temporary files" << endl;

  //Both image type: Unsigned Short
  QString filePathFixed = m_strPathPlastimatch + "/" + "fixed_deform.mha";
  QString filePathMoving = m_strPathPlastimatch + "/" + "moving_deform.mha";
  QString filePathROI = m_strPathPlastimatch + "/" + "fixed_roi_DIR.mha"; //optional

  QString filePathOutput = m_strPathPlastimatch + "/" + "output_deform.mha";
  QString filePathXform = m_strPathPlastimatch + "/" + "xform_deform.txt";


  QString filePathOutputStage1 = m_strPathPlastimatch + "/" + "output_deform_stage1.mha";
  QString filePathOutputStage2 = m_strPathPlastimatch + "/" + "output_deform_stage2.mha";
  QString filePathOutputStage3 = m_strPathPlastimatch + "/" + "output_deform_stage3.mha";

  typedef itk::ImageFileWriter<USHORT_ImageType> writerType;

  writerType::Pointer writer1 = writerType::New();
  writer1->SetFileName(filePathFixed.toLocal8Bit().constData());
  writer1->SetUseCompression(true);
  writer1->SetInput(m_spFixed);

  writerType::Pointer writer2 = writerType::New();
  writer2->SetFileName(filePathMoving.toLocal8Bit().constData());
  writer2->SetUseCompression(true);
  writer2->SetInput(m_spMoving);
  writer1->Update();
  writer2->Update();

  //Create a mask image based on the fixed sp image
  
  float FOV_DcmPosX = 0.0; //mm
  float FOV_DcmPosY = 0.0;//mm
  float FOV_Radius = 0.0;  

  if (ui.checkBoxUseROIForDIR->isChecked())
  {
      cout << "Creating a ROI mask for DIR.. " << endl;
      QString strFOVGeom = ui.lineEditFOVPos->text();

      QStringList strListFOV = strFOVGeom.split(",");
      if (strListFOV.count() == 3)
      {
          FOV_DcmPosX = strListFOV.at(0).toFloat();
          FOV_DcmPosY = strListFOV.at(1).toFloat();
          FOV_Radius = strListFOV.at(2).toFloat();

          //Create Image using FixedImage sp

          //Image Pointer here
          USHORT_ImageType::Pointer spRoiMask;
          m_pParent->AllocateByRef(m_spFixed, spRoiMask);
          m_pParent->GenerateCylinderMask(spRoiMask, FOV_DcmPosX, FOV_DcmPosY, FOV_Radius);

          writerType::Pointer writer3 = writerType::New();
          writer3->SetFileName(filePathROI.toLocal8Bit().constData());
          writer3->SetUseCompression(true);
          writer3->SetInput(spRoiMask);
          writer3->Update();
      }     
  }

  QString filePathFixed_proc = filePathFixed;  

  cout << "2: DoRegistrationDeform: CBCT pre-processing before deformable registration" << endl;
  //cout << "Air region and bubble will be removed" << endl;	

  QFileInfo info1(m_strPathCTSkin_manRegi);
  QFileInfo info2(m_strPathXFAutoRigid);

  if (!info1.exists() || !info2.exists())
  {
      cout << "Fatal error! no CT skin is found or no XF auto file found. Preprocessing will not be done. Proceeding." << endl;
  }
  else
  {
      filePathFixed_proc = m_strPathPlastimatch + "/" + "fixed_deform_proc.mha";
      //skin removal and bubble filling : output file = filePathFixed_proc
      bool bBubbleRemoval = ui.checkBoxFillBubbleCBCT->isChecked();
      ProcessCBCT_beforeDeformRegi(filePathFixed, m_strPathCTSkin_manRegi, filePathFixed_proc, m_strPathXFAutoRigid, bBubbleRemoval, bPrepareMaskOnly); //bubble filling yes

      if (bPrepareMaskOnly)
          filePathFixed_proc = filePathFixed;
  }


  cout << "3: DoRegistrationDeform: Creating a plastimatch command file" << endl;

  QString fnCmdRegisterRigid = "cmd_register_deform.txt";
  //QString fnCmdRegisterDeform = "cmd_register_deform.txt";
  QString pathCmdRegister = m_strPathPlastimatch + "/" + fnCmdRegisterRigid;

  QString strDeformableStage1 = ui.lineEditArgument1->text(); //original param: 7, add output path
  QString strDeformableStage2 = ui.lineEditArgument2->text();
  QString strDeformableStage3 = ui.lineEditArgument3->text();

  strDeformableStage1.append(", ").append(filePathOutputStage1);
  strDeformableStage2.append(", ").append(filePathOutputStage2);
  strDeformableStage3.append(", ").append(filePathOutputStage3);


  //For Cropped patients, FOV mask is applied.
  if (ui.checkBoxUseROIForDIR->isChecked())
  {
      GenPlastiRegisterCommandFile(pathCmdRegister, filePathFixed_proc, filePathMoving,
          filePathOutput, filePathXform, PLAST_BSPLINE, strDeformableStage1, strDeformableStage2, strDeformableStage3, filePathROI);
  }
  else
  {
      GenPlastiRegisterCommandFile(pathCmdRegister, filePathFixed_proc, filePathMoving,
          filePathOutput, filePathXform, PLAST_BSPLINE, strDeformableStage1, strDeformableStage2, strDeformableStage3);

  }
  	/*void DlgRegistration::GenPlastiRegisterCommandFile(QString strPathCommandFile, QString strPathFixedImg, QString strPathMovingImg,
	QString strPathOutImg, QString strPathXformOut, enRegisterOption regiOption,
	QString strStageOption1, , QString strStageOption2, QString strStageOption3)*/

  //ifstream fin;
  //fin.open(pathCmdRegister.toLocal8Bit().constData());
  //if (fin.fail())
  //{

  //    cout << endl;
  //    cout << endl;
  //    cout << endl;

  //    fin.close();
  //    cout << "fail first.. wait 5 s" << endl;
  //    Sleep(5000);

  //    fin.open(pathCmdRegister.toLocal8Bit().constData());
  //    if (fin.fail())
  //    {
  //        cout << endl;
  //        cout << endl;
  //        cout << endl;

  //        cout << "Second failure! Error! " << endl;
  //        fin.close();
  //    }
  //    else
  //    {
  //        cout << "Resolved after a single failure!" << endl;
  //        fin.close();
  //    }
  //}
  //else
  //{
  //    cout << "File is readable!" << endl;
  //    fin.close();
  //}



  //const char *command_filepath = pathCmdRegister.toLocal8Bit().constData();
  std::string str_command_filepath = pathCmdRegister.toLocal8Bit().constData();
  cout << "4: DoRegistrationDeform: calling a plastimatch command" << endl;


  Registration reg;
  if (reg.set_command_file(str_command_filepath) < 0) {
	printf ("Error.  could not load %s as command file.\n", 
            str_command_filepath.c_str());
  }

  std::string strFixed = reg.get_registration_parms()->get_fixed_fn(); //  return d_ptr->rparms;
  std::string strMoving = reg.get_registration_parms()->get_moving_fn();

  if (strFixed.length() < 1)
  {
      cout << endl;
      cout << endl;
      cout << endl;
      cout << "ERROR! no fixed image" << endl;   
  }
  if (strMoving.length() < 1)
  {
      cout << endl;
      cout << endl;
      cout << endl;
      cout << "ERROR!no moving image" << endl;      
  }




  reg.do_registration ();

 // Registration_parms *regp = new Registration_parms();
 // if (regp->parse_command_file (command_filepath) < 0) {
	//cout << "wrong use of command file" << endl;
 // }
 // do_registration (regp);
 // delete regp;

  cout << "5: DoRegistrationDeform: Registration is done" << endl;
  cout << "6: DoRegistrationDeform: Reading output image" << endl;

  typedef itk::ImageFileReader<USHORT_ImageType> readerType;
  QFileInfo tmpFileInfo;

  readerType::Pointer readerDefSt1 = readerType::New();  

  tmpFileInfo = QFileInfo(filePathOutputStage1);
  if (tmpFileInfo.exists())
  {
	readerDefSt1->SetFileName(filePathOutputStage1.toLocal8Bit().constData());
	readerDefSt1->Update();
	m_pParent->m_spDeformedCT1 = readerDefSt1->GetOutput();
  }

  readerType::Pointer readerDefSt2 = readerType::New();  
   tmpFileInfo = QFileInfo(filePathOutputStage2);
  if (tmpFileInfo.exists())
  {
	readerDefSt2->SetFileName(filePathOutputStage2.toLocal8Bit().constData());		
	readerDefSt2->Update();
	m_pParent->m_spDeformedCT2 = readerDefSt2->GetOutput();
  }

  readerType::Pointer readerDefSt3 = readerType::New();  
  tmpFileInfo = QFileInfo(filePathOutputStage3);
  if (tmpFileInfo.exists())
  {
	readerDefSt3->SetFileName(filePathOutputStage3.toLocal8Bit().constData());		
	readerDefSt3->Update();
	m_pParent->m_spDeformedCT3 = readerDefSt3->GetOutput(); 
  }

  readerType::Pointer readerFinCBCT = readerType::New();  
  tmpFileInfo = QFileInfo(filePathFixed_proc); //cropped image Or not
  if (tmpFileInfo.exists())
  {
	readerFinCBCT->SetFileName(filePathFixed_proc.toLocal8Bit().constData());		
	readerFinCBCT->Update();
	m_pParent->m_spRawReconImg = readerFinCBCT->GetOutput();
	//cout << "fixed Image Path = " << filePathFixed_proc.toLocal8Bit().constData() << endl;
  }
  else
  {
	cout << "No filePathFixed_proc is available. Exit the function" << endl;
	return;
  }

  //Puncturing should be done for final Deform image

  QString strPathDeformCTFinal = filePathOutput;

  QFileInfo tmpBubFileInfo(m_strPathMskCBCTBubble);  

  if (ui.checkBoxFillBubbleCBCT->isChecked() && tmpBubFileInfo.exists())
  {
	cout << "6B: final puncturing according to the CBCT bubble" << endl;

        /*Mask_parms parms_fill;
        strPathDeformCTFinal = m_strPathPlastimatch + "/deformCTpuncFin.mha";

        parms_fill.mask_operation = MASK_OPERATION_FILL;
        parms_fill.input_fn = filePathOutput.toLocal8Bit().constData();
        parms_fill.mask_fn = m_strPathMskCBCTBubble.toLocal8Bit().constData();
        parms_fill.output_fn = strPathDeformCTFinal.toLocal8Bit().constData();*/        

        strPathDeformCTFinal = m_strPathPlastimatch + "/deformCTpuncFin.mha";

        Mask_operation enMaskOp = MASK_OPERATION_FILL;
        QString input_fn = filePathOutput;
        QString mask_fn = m_strPathMskCBCTBubble;
        QString output_fn = strPathDeformCTFinal;

	//int iBubblePunctureVal = ui.lineEditBkFillCT->text().toInt(); //0 = soft tissue
	int iBubblePunctureVal = 0; //0 = air. deformed CT is now already a USHORT image
	int mask_value = iBubblePunctureVal;
        plm_mask_main(enMaskOp, input_fn, mask_fn, output_fn, (float)mask_value);

  }

  readerType::Pointer readerDeformFinal = readerType::New();    
  tmpFileInfo = QFileInfo(strPathDeformCTFinal);
  if (tmpFileInfo.exists())
  {
	readerDeformFinal->SetFileName(strPathDeformCTFinal.toLocal8Bit().constData());		
	readerDeformFinal->Update();
	m_pParent->m_spDeformedCT_Final = readerDeformFinal->GetOutput();  	
  }
  else
  {
	cout << "No final output is available. Exit the function" << endl;
	return;
  }

  cout << "7: DoRegistrationDeform: Reading is completed" << endl;

  UpdateListOfComboBox(0);//combo selection signalis called
  UpdateListOfComboBox(1);
  
  SelectComboExternal(0, REGISTER_RAW_CBCT); // will call fixedImageSelected 
  SelectComboExternal(1, REGISTER_DEFORM_FINAL );


  cout << "FINISHED!: Deformable image registration. Proceed to scatter correction" << endl;
}

bool DlgRegistration::PreprocessCT() //CT preparation + CBCT preparation only, then show the registration DLG
{

  if (!ui.checkBoxCropBkgroundCT->isChecked())
  {
	cout << "Preprocessing is not selected." << endl;
	return true;
  }

  if (m_pParent->m_strPathPlanCTDir.length() < 3)
  {
	cout << "Reference CT DIR should be specified" << endl;
        return false;
  }


  //All files will be saved in m_strPathPlastimatch

  //1) CT preparation
  //string strCTDir = m_pParent->m_strPathPlanCTDir.toStdString();   
  //string strPathMaskBubbleCT = m_strPathPlastimatch.append("/msk_bubbles_CT.mha").toStdString();    
  QString strPathMskBubbleCT = m_strPathPlastimatch + "/msk_bubbles_CT.mha";

  //char* strTest = (char*)(strCTDir.c_str());
  //const char* strTest2 = (strCTDir.c_str());
  //int iAirThresholdShort = -600;  
  int iAirThresholdShort = ui.lineEditBkDetectCT->text().toInt();
  //  Segment_parms parms;

  /* [1]Segment air region*/
  Plm_image in;
  Plm_image out;
  //Segment_body *sb = &parms->sb;
  Segment_body sb;
  sb.m_lower_threshold = iAirThresholdShort;

  /* Load the input image */
  in.load_native (m_pParent->m_strPathPlanCTDir.toLocal8Bit().constData()); 
  
  sb.img_in = &in;
  sb.img_out = &out;
  /* Do segmentation */
  sb.do_segmentation_air_cavity();
  /* Save output file */
  sb.img_out->save_image (strPathMskBubbleCT.toLocal8Bit().constData());

  if (m_pParent->m_strPathRS.isEmpty())
	return false;
  /* End of [1]Segment air region*/

  //plastimatch convert --input E:\PlastimatchData\DicomEg\OLD\RS.dcm --output-ss-img E:\PlastimatchData\DicomEg\OLD\ssimg_all2.mha --output-ss-list E:\PlastimatchData\DicomEg\OLD\sslist_all.txt --referenced-ct E:\PlastimatchData\DicomEg\OLD\CT

  //plm_clp_parse (&parms, &parse_fn, &usage_fn, argc, argv, 1)
  //plastimatch segment --input E:\PlastimatchData\DicomEg\OLD\CT --output-img E:\PlastimatchData\DicomEg\OLD\msk_bubbles_oldCT.mha --lower-threshold -600

 
  //do_command_warp(argc, argv);


  /* [2]Load RS file to make a Skin mask*/
  Warp_parms parms;
  Plm_file_format file_type;
  Rt_study rtds;

  parms.input_fn = m_pParent->m_strPathRS.toLocal8Bit().constData();
  parms.referenced_dicom_dir = m_pParent->m_strPathPlanCTDir.toLocal8Bit().constData();
  cout << m_pParent->m_strPathPlanCTDir.toLocal8Bit().constData() << endl;

  QString ssimg_path_all = m_strPathPlastimatch + "/ssimg_all.mha";
  QString sslist_path_all = m_strPathPlastimatch + "/sslist_all.txt";
  parms.output_ss_img_fn = ssimg_path_all.toLocal8Bit().constData();
  parms.output_ss_list_fn = sslist_path_all.toLocal8Bit().constData();

  parms.prefix_format = "mha";
  parms.use_itk = 0;
  parms.interp_lin = 1;

  //file_type = plm_file_format_deduce ((const char*) parms.input_fn);
  file_type = PLM_FILE_FMT_DICOM_RTSS;
  /*if (file_type == PLM_FILE_FMT_POINTSET) {
  warp_pointset_main (&parms);
  return;
  }*/
  /* Process warp */
  rt_study_warp(&rtds, file_type, &parms);
  printf ("Warping Finished!\n");

  //return;

  /* [3]Read outputss-list.txt and leave skin only*/
  ifstream fin;
  fin.open(sslist_path_all.toLocal8Bit().constData(), ios::in);
  if (fin.fail())
	return false;

  char str[MAX_LINE_LENGTH];

  QString strLineSkin;
  QString strLineLungLt;
  QString strLineLungRt;


  QString strRSName = ui.lineEditCropContourName->text();
  strRSName = strRSName.trimmed();

  while(!fin.eof())
  {
	memset(str, 0 ,MAX_LINE_LENGTH);
	fin.getline(str,MAX_LINE_LENGTH);
	QString strLine(str);


	QStringList strList = strLine.split('|');
	//third one is the organ name
	if (strList.length() != 3)
	{
	  cout << "abnormal file expression." << endl;
	  break;
	}
	QString organName = strList.at(2);

        organName = organName.trimmed();
        //YKTEMP20150922
	//if (organName == "Skin" || organName == "skin" || organName == "SKIN")
	//{
	//  strLineSkin = strLine;
	//}        

        //if (organName == strRSName)
        if (organName.compare(strRSName,Qt::CaseInsensitive) == 0)
        {
            strLineSkin = strLine;
            cout << "Structure for cropping was found: " << strLineSkin.toLocal8Bit().constData() << endl;
        }
  }
  fin.close();

  QString sslist_path_skin;
  if (strLineSkin.length() > 1)
  {
	ofstream fout;
	sslist_path_skin = sslist_path_all;
	sslist_path_skin.replace("all", "BODY");
	fout.open(sslist_path_skin.toLocal8Bit().constData());

	fout << strLineSkin.toLocal8Bit().constData() << endl; 

	fout.close();
  }
  else
  {
      cout << "Error: no " << strRSName.toLocal8Bit().constData() <<" RS structure is found in DICOM RS file. " << endl;
      return false;
  }
  /* End of [3]Read outputss-list.txt and leave skin only*/


  /* [4]prepare a skin mask image*/
  //plastimatch convert --input-ss-img [ssimg_all.mha] --input-ss-list [sslist_skin.txt] --output-labelmap [msk_skin.mha]

  Warp_parms parms2;
  Plm_file_format file_type2;
  Rt_study rtds2;
  //convert --input-ss-img E:\PlastimatchData\DicomEg\OLD\ssimg_all.mha --input-ss-list E:\PlastimatchData\DicomEg\OLD\sslist_skin.txt --output-labelmap E:\PlastimatchData\DicomEg\OLD\msk_skin.mha
  QString strPath_mskSkinCT = m_strPathPlastimatch + "/msk_skin_CT.mha";
  parms2.input_ss_img_fn = ssimg_path_all.toLocal8Bit().constData();
  parms2.input_ss_list_fn = sslist_path_skin.toLocal8Bit().constData();
  parms2.output_labelmap_fn = strPath_mskSkinCT.toLocal8Bit().constData(); //output 
  parms2.prefix_format = "mha";
  parms2.use_itk = 0;
  parms2.interp_lin = 1;
  //file_type = plm_file_format_deduce ((const char*) parms.input_fn);
  file_type2 = PLM_FILE_FMT_NO_FILE;
  /* Process warp */
  rt_study_warp (&rtds2, file_type2, &parms2);
  printf ("Warping2 Finished!\n");

  m_strPathCTSkin = strPath_mskSkinCT;

  /* [5] prepare a contracted skin (5 mm)*/

  //Bubble removal procedure
  QString strPath_mskSkinCT_cont;
  QString strPathMskBubbleCT_Fin;
  QString strPathBubbleRemovedCT; 

  Mask_operation mask_option;
  QString input_fn;
  QString mask_fn;
  QString output_fn;
  float mask_value = 0.0;

  if (ui.checkBoxFillBubbleCT->isChecked())
  {
	//QString strPath_mskSkinCT_dmap =  m_strPathPlastimatch + "/dmap_msk_skin_CT.mha";
	strPath_mskSkinCT_cont =  m_strPathPlastimatch + "/msk_skin_CT_cont.mha";
	plm_expansion_contract_msk(strPath_mskSkinCT, strPath_mskSkinCT_cont, -5.0);//-5mm contraction    

	//Mask_parms parms_msk;
	strPathMskBubbleCT_Fin = m_strPathPlastimatch + "/msk_bubbles_CT_fin.mha";
	/* Check if we're doing fill or mask */
	mask_option = MASK_OPERATION_MASK;
	/* Input files */
	input_fn = strPathMskBubbleCT;
        mask_fn = strPath_mskSkinCT_cont;
	output_fn = strPathMskBubbleCT_Fin;
	
	mask_value = 0.0;
	//plm_mask_main(&parms_msk);   
        plm_mask_main(mask_option, input_fn, mask_fn, output_fn, mask_value);

	//Mask_parms parms_msk2;
	strPathBubbleRemovedCT = m_strPathPlastimatch + "/bubble_filled_CT.mha";

        mask_option = MASK_OPERATION_FILL;
	input_fn = m_pParent->m_strPathPlanCTDir;   
	mask_fn = strPathMskBubbleCT_Fin;
	output_fn = strPathBubbleRemovedCT;
	int iBubbleFillingVal = ui.lineEditBubFillCT->text().toInt(); //0 = soft tissue
	mask_value = (float)iBubbleFillingVal;

        plm_mask_main(mask_option, input_fn, mask_fn, output_fn, mask_value);
  }

  /* - remove outside air*/
  //plastimatch mask --input E:\PlastimatchData\DicomEg\OLD\CT_bubble_removed.mha  --mask-value -1024 --mask E:\PlastimatchData\DicomEg\OLD\msk_skin.mha --output E:\PlastimatchData\DicomEg\OLD\CT_final.mha

  //Mask_parms parms_msk3;
  QString strPathSkinRemovedCT = m_strPathPlastimatch + "/skin_removed_CT.mha";

  //parms_msk3.mask_operation = MASK_OPERATION_MASK;  

  mask_option = MASK_OPERATION_MASK;

  QFileInfo tmpInfo(strPathBubbleRemovedCT);
  if (tmpInfo.exists())
	input_fn = strPathBubbleRemovedCT;
  else
	input_fn = m_pParent->m_strPathPlanCTDir; 

  mask_fn =  strPath_mskSkinCT;
  output_fn = strPathSkinRemovedCT;
  int iAirFillValShort = ui.lineEditBkFillCT->text().toInt(); //-1024
  mask_value = (float)iAirFillValShort;
  plm_mask_main(mask_option, input_fn, mask_fn, output_fn, mask_value);

  //strPathSkinRemovedCT .mha file is ready. this is SHORT image

  m_pParent->LoadShortImageToUshort(strPathSkinRemovedCT, m_pParent->m_spRefCTImg);
  m_pParent->RegisterImgDuplication(REGISTER_REF_CT, REGISTER_MANUAL_RIGID);
  //m_pParent->SLT_ViewRegistration();

  //Update the recon resolution (for X and Y)?--> doesn't matter, since the regidbody registration will match the resolution
  show();

  UpdateListOfComboBox(0);//combo selection signalis called
  UpdateListOfComboBox(1);
  //if not found, just skip
  SelectComboExternal(0, REGISTER_RAW_CBCT); // will call fixedImageSelected 
  SelectComboExternal(1, REGISTER_MANUAL_RIGID );

  cout << "FINISHED!: Pre-processing of CT image" << endl;

 

  //if (m_pDcmStudyPlan != NULL)
  //{
  //    Rtplan::Pointer rtplan = m_pDcmStudyPlan->get_rtplan();
  //    int iCntBeam = rtplan->num_beams;
  //    //Get Isocenter position for first beam


  //}     
  
  return true;
}


void DlgRegistration::LoadRTPlan(QString& strDCMPath)
{
    if (strDCMPath.length() < 1)
    {
        cout << "No dicom  file is found" << endl;
        return;
    }

    if (m_pDcmStudyPlan != NULL)
    {
        delete m_pDcmStudyPlan;
        m_pDcmStudyPlan = NULL;
    }

    m_pDcmStudyPlan = new Dcmtk_rt_study();

    //cout << "Before plm_file_format_deduce" << endl;
    Plm_file_format file_type_dcm_plan = plm_file_format_deduce(strDCMPath.toLocal8Bit().constData());
    //cout << "After plm_file_format_deduce" << endl;

    if (file_type_dcm_plan == PLM_FILE_FMT_DICOM_RTPLAN)
    {
        cout << "PLM_FILE_FMT_DICOM_RTPLAN " << "is found" << endl;
        m_pDcmStudyPlan->load(strDCMPath.toLocal8Bit().constData());
    }
    else
    {
        cout << "Found file is not RTPLAN. Skipping dcm plan." << endl;
        return;
    }

    //Rtplan::Pointer rtplan = m_pDcmStudyPlan->get_rtplan();
}


//void DlgRegistration::plm_dmap_main( Dmap_parms* parms )
void DlgRegistration::plm_dmap_main(QString& img_in_fn, QString& img_out_fn )
{
  if (img_in_fn.length() < 1 || img_out_fn.length() < 1)
	return;
  //dmap_main (&parms_dmap);
  Distance_map dmap;
  dmap.set_input_image (img_in_fn.toLocal8Bit().constData());
  //dmap.set_algorithm (parms_dmap.algorithm);
  //dmap.set_inside_is_positive (parms->inside_positive);
  //dmap.set_use_squared_distance (parms->squared_distance);
  dmap.run ();
  FloatImageType::Pointer dmap_image = dmap.get_output_image();
  itk_image_save(dmap_image, img_out_fn.toLocal8Bit().constData());
}

//void DlgRegistration::plm_threshold_main(Pcmd_threshold* parms)
void DlgRegistration::plm_threshold_main(QString& strRange, QString& img_in_fn, QString& img_out_fn)
{
    /*if (parms == NULL)
          return;*/

    if (img_in_fn.length() < 1 || img_out_fn.length() < 1)
        return;

  //threshold_main (&parms_thre);
  Plm_image::Pointer plm_image = plm_image_load (img_in_fn.toLocal8Bit().constData(), PLM_IMG_TYPE_ITK_FLOAT);
  FloatImageType::Pointer img_in = plm_image->m_itk_float;
  UCharImageType::Pointer img_out;

  if (strRange != "") {
      img_out = itk_threshold(img_in, std::string(strRange.toLocal8Bit().constData()));
  }

  Plm_image pli (img_out);
  pli.save_image (img_out_fn.toLocal8Bit().constData());

 // bool output_dicom = false;
 // enum output_type = PLM_IMG_TYPE_UNDEFINED; //

 // if (output_dicom) {
	//itk_image_save_short_dicom (
	//  img_out, img_out_fn.toLocal8Bit().constData(), 0);
 // } else {
	//Plm_image pli (img_out);

	//if (output_type) {
 //           pli.convert(output_type);
	//}
	//pli.save_image (img_out_fn);
 // }
  cout << "contracted skin mask is ready" << endl;
}

//void DlgRegistration::plm_mask_main(Mask_parms* parms)
void DlgRegistration::plm_mask_main(Mask_operation mask_option, QString& input_fn, QString& mask_fn, QString& output_fn, float mask_value)
{
  Plm_image::Pointer img
      = plm_image_load_native(input_fn.toLocal8Bit().constData());
  if (!img) {
	printf("Error: could not open '%s' for read\n",
            input_fn.toLocal8Bit().constData());
        return;
  }

  UCharImageType::Pointer mask = itk_image_load_uchar(mask_fn.toLocal8Bit().constData(), 0);

  switch (img->m_type) {
	case PLM_IMG_TYPE_ITK_UCHAR:
	  img->m_itk_uchar = mask_image (img->m_itk_uchar, mask, 
              mask_option, mask_value);
	  break;
	case PLM_IMG_TYPE_ITK_SHORT:
	  img->m_itk_short = mask_image (img->m_itk_short, mask, 
              mask_option, mask_value);
	  break;
	case PLM_IMG_TYPE_ITK_USHORT:
	  img->m_itk_ushort = mask_image (img->m_itk_ushort, mask, 
              mask_option, mask_value);
	  break;
	case PLM_IMG_TYPE_ITK_ULONG:
	  img->m_itk_uint32 = mask_image (img->m_itk_uint32, mask, 
              mask_option, mask_value);
	  break;
	case PLM_IMG_TYPE_GPUIT_FLOAT:
	case PLM_IMG_TYPE_ITK_FLOAT:
	  img->m_itk_float = mask_image (img->itk_float(), mask, 
              mask_option, mask_value);
	  break;
	default:
	  printf ("Unhandled conversion in mask_main\n");
	  break;
  }

  bool output_dicom = false; //default: comes from Mask_param header
  Plm_image_type output_type = PLM_IMG_TYPE_UNDEFINED; //default: comes from Mask_param header

  if (output_dicom) {
      img->save_short_dicom(output_fn.toLocal8Bit().constData(), 0);
  } else {
	if (output_type) {
	  img->convert (output_type);
	}
	img->save_image (output_fn.toLocal8Bit().constData());
  }
}


void DlgRegistration::plm_expansion_contract_msk( QString& strPath_msk, QString& strPath_msk_exp_cont, double fExpVal )
{
#if defined (commentout)
  Dmap_parms parms_dmap; //orignally, Dmap_parms is defined in pcmd_dmap.cxx, supposed to be in any .h  
  QString strPath_mskSkinCT_dmap =  strPath_msk + "_dmap.mha";
  parms_dmap.img_in_fn = strPath_msk.toLocal8Bit().constData();
  parms_dmap.img_out_fn = strPath_mskSkinCT_dmap.toLocal8Bit().constData();  
  plm_dmap_main(&parms_dmap);  
#endif

  QString strPath_mskSkinCT_dmap = strPath_msk + "_dmap.mha";
  plm_dmap_main(strPath_msk, strPath_mskSkinCT_dmap);

  //Thresholding
  /* Pcmd_threshold parms_thre;
   
   parms_thre.range_string = string_format ("-inf,%f", fExpVal);
   parms_thre.img_in_fn = strPath_mskSkinCT_dmap.toLocal8Bit().constData();
   parms_thre.img_out_fn = strPath_mskSkinCT_mod.toLocal8Bit().constData();   */

  QString strPath_mskSkinCT_mod = strPath_msk_exp_cont;
  QString range_string = QString(string_format("-inf,%f", fExpVal).c_str());
  QString img_in_fn = strPath_mskSkinCT_dmap;
  QString img_out_fn = strPath_mskSkinCT_mod;

  //plm_threshold_main(&parms_thre);
  plm_threshold_main(range_string, img_in_fn, img_out_fn);
}


//NoBubble filling is included. bubble is only needed to be filled during deformable regi
void DlgRegistration::ProcessCBCT_beforeAutoRigidRegi(QString& strPathRawCBCT, QString& strPath_mskSkinCT, QString& strPathOutputCBCT, double* manualTrans3d, bool bPrepareMaskOnly)
{ 
  //Calc. Origin difference

  //1) Move CT mask according to the manual shift
  //plastimatch synth-vf --fixed [msk_skin.mha] --output [xf_manual_trans.mha] --xf-trans "[origin diff (raw - regi)]"
  if (!m_pParent->m_spManualRigidCT)
	return;
  
  //QString strPath_mskSkinCT = m_strPathPlastimatch + "/msk_skin_CT.mha";
  QString strPath_outputXF_manualTrans = m_strPathPlastimatch + "/xf_manual_trans.mha";

  //USHORT_ImageType::PointType rawOrigin = m_pParent->m_spReconImg->GetOrigin();
  //USHORT_ImageType::PointType rigidMovedOrigin = m_pParent->m_spManualRigidCT->GetOrigin();      
  //
  //double trans[3];
  //trans[0] = (double)(rawOrigin[0] - rigidMovedOrigin[0]);
  //trans[1] = (double)(rawOrigin[1] - rigidMovedOrigin[1]);
  //trans[2] = (double)(rawOrigin[2] - rigidMovedOrigin[2]);

  plm_synth_trans_xf(strPath_mskSkinCT, strPath_outputXF_manualTrans,  manualTrans3d[0],  manualTrans3d[1],  manualTrans3d[2]);

  //2) Move CT mask according to the manual shift
  /*Move the skin contour according to the vector field
	plastimatch warp --input [msk_skin.mha] --output-img [msk_skin_manRegi.mha] --xf [xf_manual_trans.mha]
  plastimatch warp --input E:\PlastimatchData\DicomEg\OLD\msk_skin.mha --output-img E:\PlastimatchData\DicomEg\OLD\msk_skin_manRegi.mha --xf E:\PlastimatchData\DicomEg\OLD\xf_manual_trans.mha*/

 
  //parms->output_img_fn = parser->get_string("output-img").c_str();
  //parms->xf_in_fn = parser->get_string("xf").c_str();

  //convert --input-ss-img E:\PlastimatchData\DicomEg\OLD\ssimg_all.mha --input-ss-list E:\PlastimatchData\DicomEg\OLD\sslist_skin.txt --output-labelmap E:\PlastimatchData\DicomEg\OLD\msk_skin.mha
  //QString strPath_mskSkinCT = m_strPathPlastimatch + "/msk_skin_CT.mha";
  QString strPath_mskSkinCT_manRegi = m_strPathPlastimatch + "/msk_skin_CT_manRegi.mha";  


  Warp_parms parms;
  Plm_file_format file_type;  
  Rt_study rtds;
  
  parms.input_fn = strPath_mskSkinCT.toLocal8Bit().constData(); 
  parms.output_img_fn = strPath_mskSkinCT_manRegi.toLocal8Bit().constData();
  parms.xf_in_fn = strPath_outputXF_manualTrans.toLocal8Bit().constData();  
  parms.fixed_img_fn = strPathRawCBCT.toLocal8Bit().constData(); //add this to fit Mask to CBCT image

  parms.prefix_format = "mha";
  parms.use_itk = 0;
  parms.interp_lin = 1;//linear
  //file_type = plm_file_format_deduce ((const char*) parms.input_fn);
  file_type = PLM_FILE_FMT_IMG;

  cout << "Entering plm rt_study_warp..." << endl;

  rt_study_warp(&rtds, file_type, &parms);
  printf ("Warping_rigid_trans Finished!\n");
  
  //3) Expand 10mm skin contour
	//plastimatch synth-vf --fixed E:\PlastimatchData\DicomEg\OLD\msk_skin_manRegi.mha --radial-mag "0 0 0" --xf-radial --output E:\PlastimatchData\DicomEg\NEW\xf_exp_CB.mha
	//plastimatch synth-vf --fixed E:\PlastimatchData\DicomEg\OLD\msk_skin_manRegi.mha --radial-mag "0.1 0.1 0.1" --xf-radial --output E:\PlastimatchData\DicomEg\NEW\xf_exp_CB.mha

  QString strPath_mskSkinCT_manRegi_exp = m_strPathPlastimatch + "/msk_skin_CT_manRegi_exp.mha";

  double skinExp = ui.lineEditCBCTSkinCropBfRegid->text().toDouble();
  if (skinExp < 0.0 || skinExp > 100.0)
	skinExp = 0.0;

  plm_expansion_contract_msk(strPath_mskSkinCT_manRegi, strPath_mskSkinCT_manRegi_exp, skinExp);//10 mm expansion for a mask image

  //4) eliminate the air region (temporarily)
  //plastimatch mask --input E:\PlastimatchData\DicomEg\NEW\rawCBCT2.mha  --mask-value 0 --mask E:\PlastimatchData\DicomEg\OLD\msk_skin_autoRegi_exp.mha --output E:\PlastimatchData\DicomEg\NEW\rawCBCT4.mha  
 
  //cout << "bPrepareMaskOnly=" << bPrepareMaskOnly << endl;

  int bkGroundValUshort = ui.lineEditBkFillCBCT->text().toInt(); //0

  //Mask_parms parms_msk;
  //QString strPath_CBCT_skinRemovedTemp = m_strPathPlastimatch + "/skin_removed_CBCT_tmp.mha";

  Mask_operation mask_option = MASK_OPERATION_MASK;   
  QString input_fn = strPathRawCBCT;   
  QString mask_fn = strPath_mskSkinCT_manRegi_exp;
  QString output_fn = strPathOutputCBCT;
  //parms_msk.mask_value = 0.0; //unsigned short
  float mask_value = bkGroundValUshort; //unsigned short


  if (!bPrepareMaskOnly) //actual cropping is controled by checkBoxCropBkgroundCBCT. But mask files are always prepared.
  {      
      cout << "Entering plm_mask_main to crop the skin image." << endl;
      plm_mask_main(mask_option, input_fn, mask_fn, output_fn, mask_value);
  }
  else
  {
      cout << "bPrepareMaskOnly flag is on. Skipping plm_mask_main.. " << endl;
      strPathOutputCBCT = "";
  }

  m_strPathCTSkin_manRegi = strPath_mskSkinCT_manRegi; //for further use  

  cout << "CBCT preprocessing is done! " << endl;

}

//called after the auto rigid regi. 1) accurate skin clipping 2) air bubble filling inside of the CBCT
void DlgRegistration::ProcessCBCT_beforeDeformRegi(QString& strPathRawCBCT, QString& strPath_mskSkinCT_manRegi, QString& strPathOutputCBCT, QString& strPathXFAutoRigid, bool bBubbleFilling, bool bPrepareMaskOnly)
{
  if (!m_pParent->m_spAutoRigidCT)
	return;  

  Warp_parms parms;
  Plm_file_format file_type;  
  Rt_study rtds;

  QString strPath_mskSkinCT_autoRegi = m_strPathPlastimatch + "/msk_skin_CT_autoRegi.mha";

  parms.input_fn = strPath_mskSkinCT_manRegi.toLocal8Bit().constData(); 
  parms.output_img_fn = strPath_mskSkinCT_autoRegi.toLocal8Bit().constData();
  parms.xf_in_fn = strPathXFAutoRigid.toLocal8Bit().constData();  
  parms.fixed_img_fn = strPathRawCBCT.toLocal8Bit().constData(); //add this to fit Mask to CBCT image

  parms.prefix_format = "mha";
  parms.use_itk = 0;
  parms.interp_lin = 1;//linear
  //file_type = plm_file_format_deduce ((const char*) parms.input_fn);
  file_type = PLM_FILE_FMT_IMG;

  rt_study_warp(&rtds, file_type, &parms);
  printf ("Skin mask based on auto regi is ready!\n");

  QString strPath_mskSkinCT_autoRegi_exp = m_strPathPlastimatch + "/msk_skin_CT_autoRegi_exp.mha";

  double skinExp = ui.lineEditCBCTSkinCropBfDIR->text().toDouble();
  if (skinExp < 0.0 || skinExp > 100.0)
	skinExp = 0.0;

  plm_expansion_contract_msk(strPath_mskSkinCT_autoRegi, strPath_mskSkinCT_autoRegi_exp, skinExp);//8 mm expansion for a mask image

  //4) eliminate the air region (temporarily)
  //plastimatch mask --input E:\PlastimatchData\DicomEg\NEW\rawCBCT2.mha  --mask-value 0 --mask E:\PlastimatchData\DicomEg\OLD\msk_skin_autoRegi_exp.mha --output E:\PlastimatchData\DicomEg\NEW\rawCBCT4.mha

  //Mask_parms parms_msk;
  //QString strPath_CBCT_skinRemovedTemp = m_strPathPlastimatch + "/skin_removed_CBCT_tmp.mha";

  Mask_operation mask_option = MASK_OPERATION_MASK;   
  QString input_fn = strPathRawCBCT;   
  QString mask_fn = strPath_mskSkinCT_autoRegi_exp;
  QString output_fn = strPathOutputCBCT;
  float mask_value = 0.0; //unsigned short

  if (!bPrepareMaskOnly)
      plm_mask_main(mask_option, input_fn, mask_fn, output_fn, mask_value);
  else
      strPathOutputCBCT = strPathRawCBCT;

  m_strPathCTSkin_autoRegi = strPath_mskSkinCT_autoRegi; //for further use. this is not expanded one!

  //****************OPTIONAL
  if (!bBubbleFilling)
	return; //exit

  //Bubble filling
  //- With the latest air-removed image (rawCBCT4), find inside-body-bubbles.
  //plastimatch segment --input [rawCBCT.mha] --output-img [msk_bubbles_CBCT.mha] --lower-threshold [air_thre_ushort]
  //plastimatch segment --input E:\PlastimatchData\DicomEg\NEW\rawCBCT4.mha --output-img E:\PlastimatchData\DicomEg\NEW\msk_bubbles_CBCT500.mha --lower-threshold 500

  /*- fill skin regions in bubble mask image
	plastimatch mask --input E:\PlastimatchData\DicomEg\NEW\msk_bubbles_CBCT500.mha --mask-value 0 --mask E:\PlastimatchData\DicomEg\OLD\msk_skin_autoRegi_cont.mha --output E:\PlastimatchData\DicomEg\NEW\msk_bubbles_CBCT_final.mha*/

  /*- (optional) fill lung regions 
	plastimatch fill --input E:\PlastimatchData\DicomEg\NEW\msk_bubbles_CBCT_final.mha --mask-value 0 --mask E:\PlastimatchData\DicomEg\OLD\msk_lungs_autoRegi_exp.mha --output E:\PlastimatchData\DicomEg\NEW\msk_bubbles_CBCT_final.mha*/

  QString strPathMskBubbleCBCT = m_strPathPlastimatch + "/msk_bubbles_CBCT.mha";  
  QString strPathMskBubbleCBCT_final = m_strPathPlastimatch + "/msk_bubbles_CBCT_final.mha";

  //int iAirThresholdUShort = 500; //depends on the CBCT image
  int iBubThresholdUshort = ui.lineEditBubDetectCBCT->text().toInt();
  int iBubFillUshort = ui.lineEditBubFillCBCT->text().toInt(); // 700
  
  Plm_image in;
  Plm_image out;  
  Segment_body sb;
  sb.m_lower_threshold = iBubThresholdUshort;  
  in.load_native (strPathOutputCBCT.toLocal8Bit().constData());

  sb.img_in = &in;
  sb.img_out = &out;
  /* Do segmentation */
  sb.do_segmentation_air_cavity ();
  /* Save output file */
  sb.img_out->save_image (strPathMskBubbleCBCT.toLocal8Bit().constData());

  QString strPath_mskSkinCT_autoRegi_cont = m_strPathPlastimatch + "/msk_skin_CT_autoRegi_cont.mha";
  plm_expansion_contract_msk(strPath_mskSkinCT_autoRegi, strPath_mskSkinCT_autoRegi_cont, -10.0);//-10 mm expansion for a mask image

  //Mask_parms parms_msk_bubble;  
  mask_option = MASK_OPERATION_MASK;
  input_fn = strPathMskBubbleCBCT;   
  mask_fn =  strPath_mskSkinCT_autoRegi_cont;
  output_fn = strPathMskBubbleCBCT_final;
  mask_value = 0.0; //unsigned short
  plm_mask_main(mask_option, input_fn, mask_fn, output_fn, mask_value);  

  //Fill CBCT bubbles with the above-bubble mask
  //plastimatch fill --input [air-removed_autoRegistered CBCT] --mask-value [dummy CBCT soft-tissue value in ushort -->depending on the scan region?] --mask [segmented air-bubble] --output [CBCT_final.mha --> holes and outside(with margin) region-removed CBCT. ready for deformable registration]
  //plastimatch fill --input E:\PlastimatchData\DicomEg\NEW\rawCBCT4.mha --mask-value 700 --mask E:\PlastimatchData\DicomEg\NEW\msk_bubbles_CBCT_final.mha --output E:\PlastimatchData\DicomEg\NEW\rawCBCT_final.mha

  //Mask_parms parms_fill;
  QString strPathBubbleRemovedCBCT = m_strPathPlastimatch + "/bubble_filled_CBCT.mha";  //tmp
  //QString strPathBubbleRemovedCBCT = strPathOutputCBCT;  //OVERWRITTING

  mask_option = MASK_OPERATION_FILL;
  //parms_fill.input_fn = strPathRawCBCT.toLocal8Bit().constData();   
  input_fn = strPathOutputCBCT;   //CBCT after air correction
  mask_fn = strPathMskBubbleCBCT_final;
  //parms_fill.output_fn = strPathBubbleRemovedCBCT.toLocal8Bit().constData(); 
  output_fn = strPathOutputCBCT; //overwriting
  //parms_fill.mask_value = 700.0; //compromised softtissue value
  mask_value = (float)iBubFillUshort; //compromised softtissue value
  plm_mask_main(mask_option, input_fn, mask_fn, output_fn, mask_value);

  if (bBubbleFilling) //if bubble was segmented
	m_strPathMskCBCTBubble = strPathMskBubbleCBCT_final; //save this for further use
}

void DlgRegistration::plm_synth_trans_xf( QString& strPath_fixed, QString& strPath_out_xf, double transX, double transY, double transZ)
{
  Synthetic_vf_parms sv_parms;
  sv_parms.pattern = Synthetic_vf_parms::PATTERN_TRANSLATION;
  sv_parms.translation[0] = transX;
  sv_parms.translation[1] = transY;
  sv_parms.translation[2] = transZ;

  FloatImageType::Pointer fixed = itk_image_load_float (strPath_fixed.toLocal8Bit().constData(), 0);
  sv_parms.pih.set_from_itk_image (fixed);

  //Synthetic_vf_parms *sv_parms = &parms->sv_parms;  

  DeformationFieldType::Pointer vf = synthetic_vf (&sv_parms);  
  itk_image_save (vf, strPath_out_xf.toLocal8Bit().constData());
}

void DlgRegistration::SLT_PreProcessCT()
{    
     if (!PreprocessCT())
     {
     cout << "Error in PreprocessCT!!!scatter correction would not work out." << endl;
     m_pParent->m_bMacroContinue = false;
     }  
    ////Load DICOM plan
    if (m_pParent->m_strPathPlan.isEmpty())
    {
        cout << "No DCM plan file was found. Skipping dcm plan." << endl;
        return;
    }    
    //QString dcmplanPath = m_pParent->m_strPathPlan;
    LoadRTPlan(m_pParent->m_strPathPlan);//fill RT_studyplan  
}

void DlgRegistration::SetPlmOutputDir(QString& endFix)
{
  QDir crntDir = QDir::current(); //folder where current exe file exists.
  QString crntPathStr = crntDir.absolutePath();
  QString dirName = crntPathStr.append("/").append("plm_").append(endFix);
  
  QDir tmpDir = QDir(dirName);
  if (!tmpDir.exists())
	tmpDir.mkpath(dirName);
  m_strPathPlastimatch = dirName;
}

void DlgRegistration::init( QString& strDCMUID )
{
  SetPlmOutputDir(strDCMUID);

  USHORT_ImageType::Pointer spNull;
  //unlink all of the pointers
  //m_pParent->m_spReconImg->Delete(); //fixed image // ID: RawCBCT
  m_pParent->m_spRefCTImg = spNull;
  m_pParent->m_spManualRigidCT= spNull;//copied from RefCTImg; ID: RefCT --> Moving Img, cloned
  m_pParent->m_spAutoRigidCT= spNull; // ID: AutoRigidCT    
  m_pParent->m_spDeformedCT1= spNull; //Deformmation will be carried out based on Moving IMage of GUI //AutoDeformCT1
  m_pParent->m_spDeformedCT2= spNull; //AutoDeformCT2
  m_pParent->m_spDeformedCT3= spNull; //AutoDeformCT3
  m_pParent->m_spDeformedCT_Final= spNull; //AutoDeformCT3  

  ui.checkBoxKeyMoving->setChecked(false);
  ui.lineEditOriginChanged->setText("");
  int findIndx = -1;  
  findIndx = ui.comboBoxDeformOption->findText("mi");
  ui.comboBoxDeformOption->setCurrentIndex(findIndx);//will call "SLT_FixedImageSelected" 

  //show();

  UpdateListOfComboBox(0);
  UpdateListOfComboBox(1);
  //if not found, just skip
  SelectComboExternal(0, REGISTER_RAW_CBCT); // will call fixedImageSelected 
  SelectComboExternal(1, REGISTER_MANUAL_RIGID ); //WILL BE IGNORED
}

void DlgRegistration::SLT_PassFixedImgForAnalysis()
{
  if (m_spFixed){
  	QString curText = ui.comboBoxImgFixed->currentText();
	m_pParent->UpdateReconImage(m_spFixed, curText);
  }
}

void DlgRegistration::SLT_PassMovingImgForAnalysis()
{
  if (m_spMoving){
  	QString curText = ui.comboBoxImgMoving->currentText();
	m_pParent->UpdateReconImage(m_spMoving, curText);
  }
}

void DlgRegistration::PostSkinRemovingCBCT( USHORT_ImageType::Pointer& spCBCT )
{
  if (!spCBCT)
  {
	cout << "Error! No CBCT image is available" << endl;
  } 

  //find the closest skin contour for CBCT: !8 mm expansion from auto rigid body

  QString strPath_mskSkinCT_final;
  QString strPath_mskSkinCT_autoRegi_exp = m_strPathPlastimatch + "/msk_skin_CT_autoRegi_exp.mha";  
  QFileInfo maskInfoAuto(strPath_mskSkinCT_autoRegi_exp);

  QString strPath_mskSkinCT_manualRegi_exp = m_strPathPlastimatch + "/msk_skin_CT_manRegi_exp.mha";
  QFileInfo maskInfoManual(strPath_mskSkinCT_manualRegi_exp);
  if (maskInfoAuto.exists()) //if the mask file is not prepared, give up the skin removal
  {
      strPath_mskSkinCT_final = strPath_mskSkinCT_autoRegi_exp;
  }
  else
  {
      cout << "Mask file of auto-registration is not prepared. Use manual regi-mask instead" << endl;

      if (maskInfoManual.exists())
      {
          strPath_mskSkinCT_final = strPath_mskSkinCT_manualRegi_exp;
      }
      else
      {
          cout << "Mask file of manual registration is not prepared. Skip skin removal!" << endl;
          return;
      }
  }	

  //cout << "Plastimatch Path " << m_strPathPlastimatch.toLocal8Bit().constData() << endl;
    
  if (m_strPathPlastimatch.length() < 1)
  {
	  cout << "NO plastimatch Dir was defined. CorrCBCT will not be saved automatically" << endl;
	  return;   
  }
  //1) Export current CBCT file
  QString filePathCBCT = m_strPathPlastimatch + "/" + "CorrCBCT.mha"; //usually corrected one
  QString filePathCBCT_noSkin = m_strPathPlastimatch + "/" + "CorrCBCT_final.mha"; //usually corrected one
  	  
  typedef itk::ImageFileWriter<USHORT_ImageType> writerType;
  writerType::Pointer writer = writerType::New();
  writer->SetFileName(filePathCBCT.toLocal8Bit().constData());
  writer->SetUseCompression(true);
  writer->SetInput(spCBCT);

  cout << "Writing the CBCT file" << endl;
  writer->Update();

  QFileInfo CBCTInfo(filePathCBCT);
  if (!CBCTInfo.exists())
  {
	  cout << "No CBCT file to read. Maybe prior writing failed" << endl;
	  return;
  }

  //ERROR HERE! delete the temporry folder.
  cout << "Delete the temporary folder if it crashes" << endl;
  
   //4) eliminate the air region (temporarily)
  //Mask_parms parms_msk;
  //DIMENSION SHOULD BE MATCHED!!!! BETWEEN raw CBCT and Mask files
  Mask_operation mask_option = MASK_OPERATION_MASK;
  QString input_fn = filePathCBCT.toLocal8Bit().constData();
  QString mask_fn = strPath_mskSkinCT_final.toLocal8Bit().constData();
  QString output_fn = filePathCBCT_noSkin.toLocal8Bit().constData();
  float mask_value = 0.0; //unsigned short  
  plm_mask_main(mask_option, input_fn, mask_fn, output_fn, mask_value);

  //m_strPathCTSkin_autoRegi = strPath_mskSkinCT_autoRegi; //for further use. this is not expanded one!

  typedef itk::ImageFileReader<USHORT_ImageType> readerType;
  readerType::Pointer readerCBCT = readerType::New();    
  QFileInfo tmpFileInfo(filePathCBCT_noSkin);

  if (tmpFileInfo.exists())
  {
	readerCBCT->SetFileName(filePathCBCT_noSkin.toLocal8Bit().constData());		
	cout << "Reading the corrected file" << endl;
	readerCBCT->Update();
	spCBCT = readerCBCT->GetOutput();  	
  }
  else
  {
	cout << "Error! No skin-removed file is available for reading" << endl;
	return;
  }
}

void DlgRegistration::CropSkinUsingRS( USHORT_ImageType::Pointer& spImgUshort, QString& strPathRS, double cropMargin )
{
  if (cropMargin != 0.0)
  {
	cout << "margin has not been implemented yet. regarded as 0.0 in this version" << endl;
  }
  if (!spImgUshort)
	return;

   /* Load RS file to make a Skin mask*/
  Warp_parms parms;
  Plm_file_format file_type;
  Rt_study rtds;

  //Export cur image first
  QString filePathCurImg = m_strPathPlastimatch + "/" + "SkinCropRS_curImg.mha"; //usually corrected one

  typedef itk::ImageFileWriter<USHORT_ImageType> writerType;

  writerType::Pointer writer = writerType::New();
  writer->SetFileName(filePathCurImg.toLocal8Bit().constData());
  writer->SetUseCompression(true);
  writer->SetInput(spImgUshort);
  cout << "Writing the current image file" << endl;
  writer->Update();


  parms.input_fn = strPathRS.toLocal8Bit().constData();  
  parms.fixed_img_fn = filePathCurImg.toLocal8Bit().constData();  

  QString ssimg_path_all = m_strPathPlastimatch + "/ssimg_all_cstm.mha";
  QString sslist_path_all = m_strPathPlastimatch + "/sslist_all_cstm.txt";
  parms.output_ss_img_fn = ssimg_path_all.toLocal8Bit().constData();
  parms.output_ss_list_fn = sslist_path_all.toLocal8Bit().constData();

  parms.prefix_format = "mha";
  parms.use_itk = 0;
  parms.interp_lin = 1;

  //file_type = plm_file_format_deduce ((const char*) parms.input_fn);
  file_type = PLM_FILE_FMT_DICOM_RTSS;
  /*if (file_type == PLM_FILE_FMT_POINTSET) {
  warp_pointset_main (&parms);
  return;
  }*/
  /* Process warp */
  rt_study_warp(&rtds, file_type, &parms);
  printf ("Warping Finished!\n");

  //return;

  /* [3]Read outputss-list.txt and leave skin only*/
  ifstream fin;
  fin.open(sslist_path_all.toLocal8Bit().constData(), ios::in);
  if (fin.fail())
	return;

  char str[MAX_LINE_LENGTH];

  QString strLineSkin;  

  while(!fin.eof())
  {
	memset(str, 0 ,MAX_LINE_LENGTH);
	fin.getline(str,MAX_LINE_LENGTH);
	QString strLine(str);


	QStringList strList = strLine.split('|');
	//third one is the organ name
	if (strList.length() != 3)
	{
	  cout << "abnormal file expression." << endl;
	  break;
	}
	QString organName = strList.at(2);

	organName.trimmed();
	if (organName == "Body" || organName == "body" || organName == "BODY")
	{
	  strLineSkin = strLine;
	}
	/*if (organName == "LeftLung")
	{
	strLineLungLt = strLine;
	}
	if (organName == "RightLung")
	{
	strLineLungRt = strLine;
	}*/
  }
  fin.close();

  QString sslist_path_skin;
  if (strLineSkin.length() > 1)
  {
	ofstream fout;
	sslist_path_skin = sslist_path_all;
	sslist_path_skin.replace("all", "BODY");
	fout.open(sslist_path_skin.toLocal8Bit().constData());

	fout << strLineSkin.toLocal8Bit().constData() << endl; 

	fout.close();
  }
  else
  {
	cout << "Error: no skin contour is found in DICOM RS file. " << endl;
	return;
  }
  /* End of [3]Read outputss-list.txt and leave skin only*/


  /* [4]prepare a skin mask image*/
  //plastimatch convert --input-ss-img [ssimg_all.mha] --input-ss-list [sslist_skin.txt] --output-labelmap [msk_skin.mha]

  Warp_parms parms2;
  Plm_file_format file_type2;
  Rt_study rtds2;
  //convert --input-ss-img E:\PlastimatchData\DicomEg\OLD\ssimg_all.mha --input-ss-list E:\PlastimatchData\DicomEg\OLD\sslist_skin.txt --output-labelmap E:\PlastimatchData\DicomEg\OLD\msk_skin.mha
  QString strPath_mskSkinCT = m_strPathPlastimatch + "/msk_skin_CT_cstm.mha";
  parms2.input_ss_img_fn = ssimg_path_all.toLocal8Bit().constData();
  parms2.input_ss_list_fn = sslist_path_skin.toLocal8Bit().constData();
  parms2.output_labelmap_fn = strPath_mskSkinCT.toLocal8Bit().constData(); //output 
  parms2.prefix_format = "mha";
  parms2.use_itk = 0;
  parms2.interp_lin = 1;
  //file_type = plm_file_format_deduce ((const char*) parms.input_fn);
  file_type2 = PLM_FILE_FMT_NO_FILE;
  /* Process warp */
  rt_study_warp(&rtds2, file_type2, &parms2);
  printf ("Warping2 Finished!\n");
  //m_strPathCTSkin = strPath_mskSkinCT;


  //Mask_parms parms_msk3;
  QString strPathSkinRemovedCT = m_strPathPlastimatch + "/skin_removed_CT_cstm.mha";
  Mask_operation mask_option = MASK_OPERATION_MASK;
  QFileInfo tmpInfo(filePathCurImg);

  QString input_fn;
  if (tmpInfo.exists())
      input_fn = filePathCurImg;
  else
	return;

  QString mask_fn = strPath_mskSkinCT;
  QString output_fn = strPathSkinRemovedCT;
  int iAirFillValShort = 0;
  float mask_value = (float)iAirFillValShort;
  plm_mask_main(mask_option, input_fn, mask_fn, output_fn, mask_value);  
  //strPathSkinRemovedCT .mha file is ready. this is SHORT image

  typedef itk::ImageFileReader<USHORT_ImageType> readerType;
  readerType::Pointer reader = readerType::New();  

  QFileInfo tmpFileInfo = QFileInfo(strPathSkinRemovedCT); //cropped image
  if (tmpFileInfo.exists())
  {
	reader->SetFileName(strPathSkinRemovedCT.toLocal8Bit().constData());		
	reader->Update();

	spImgUshort = reader->GetOutput();
	//cout << "fixed Image Path = " << filePathFixed_proc.toLocal8Bit().constData() << endl;
  }
  else
  {
	cout << "No strPathSkinRemovedCT is available. Exit the function" << endl;
	return;
  }
}

void DlgRegistration::SLT_Macro()
{
  //1) DIR
  if (!m_pParent->m_spAutoRigidCT)
	return;

  SLT_DoRegistrationDeform();
  m_pParent->SLT_DoScatterCorrection_APRIORI();
  //will reconstrct automatically

  //then, Crop skin
  if (m_pParent->ui.lineEdit_PathCBCTSkinPath->text().length() > 1)
  {
	m_pParent->SLT_CropSkinUsingRS();
  }
  //Message with alarming
  QMessageBox::information(0, "Warning", "Reconstruction is done. Export the file");
  

}

void DlgRegistration::SLT_ExchangeRawRef()
{
	

}

void DlgRegistration::SLT_ManualMoveByDCMPlan()
{
    if (!m_spFixed || !m_spMoving)
        return;

    if (!m_pParent->m_spRefCTImg || !m_pParent->m_spManualRigidCT)
        return;

    if (m_pDcmStudyPlan == NULL)
    {
        cout << "Error! no dcmRTStudy is loaded" << endl;
        return;
    }

    Rtplan::Pointer rtplan = m_pDcmStudyPlan->get_rtplan();

    if (!rtplan)
    {
        cout << "Error! no dcm plan is loaded" << endl;
        return;
    }
    int iCntBeam = rtplan->num_beams;

    if (iCntBeam < 1)
    {
        cout << "Error! no beam is found" << endl;
        return;
    }

    float* final_iso_pos = NULL;


    for (int i = 0; i < iCntBeam; i++)
    {
        Rtplan_beam *curBeam = rtplan->beamlist[i];

        int iCntCP = curBeam->num_cp;

        for (int j = 0; j < iCntCP; j++)
        {           
            float* cur_iso_pos = curBeam->cplist[j]->get_isocenter();
            cout << "Beam ID: " << curBeam->id << ", Control point ID: " << curBeam->cplist[j]->control_pt_no <<
                ", Isocenter pos : " << cur_iso_pos[0] << "/" << cur_iso_pos[1] << "/" << cur_iso_pos[2] << endl;

            if (i == 0 && j == 0)
                final_iso_pos = curBeam->cplist[j]->get_isocenter();
        }        
    }
    //VEC3D shiftVal;// = GetShiftValueFromGradientXForm(filePathXform, true); //true: inverse trans should be applied if CBCT was moving image //in mm    

    if (final_iso_pos == NULL)
    {
        cout << "Error!  No isocenter position was found. " << endl;
        return;
    }
     

    //ImageManualMoveOneShot(-iso_pos[0], -iso_pos[1], -iso_pos[2]);    
    ImageManualMoveOneShot(final_iso_pos[0], final_iso_pos[1], final_iso_pos[2]);

    UpdateListOfComboBox(0);//combo selection signalis called
    UpdateListOfComboBox(1);

    SelectComboExternal(0, REGISTER_RAW_CBCT); // will call fixedImageSelected 
    SelectComboExternal(1, REGISTER_MANUAL_RIGID);
}

void DlgRegistration::SLT_DoRegistrationGradient()
{
    //1) Save current image files
    if (!m_spFixed || !m_spMoving)
        return;

    if (!m_pParent->m_spRefCTImg || !m_pParent->m_spManualRigidCT)
        return;

    cout << "1: writing temporary files" << endl;

    //Both image type: Unsigned Short
    QString filePathFixed = m_strPathPlastimatch + "/" + "fixed_gradient.mha";
    QString filePathMoving = m_strPathPlastimatch + "/" + "moving_gradient.mha";
    QString filePathOutput = m_strPathPlastimatch + "/" + "output_gradient.mha";
    QString filePathXform = m_strPathPlastimatch + "/" + "xform_gradient.txt";

    typedef itk::ImageFileWriter<USHORT_ImageType> writerType;

    writerType::Pointer writer1 = writerType::New();
    writer1->SetFileName(filePathFixed.toLocal8Bit().constData());
    writer1->SetUseCompression(true);
    writer1->SetInput(m_spFixed);

    writerType::Pointer writer2 = writerType::New();
    writer2->SetFileName(filePathMoving.toLocal8Bit().constData());
    writer2->SetUseCompression(true);
    writer2->SetInput(m_spMoving);

    writer1->Update();
    writer2->Update();

    //Preprocessing
    //2) move CT-based skin mask on CBCT based on manual shift
    //  if (m_strPathCTSkin)
    
   cout << "2: Creating a plastimatch command file" << endl;

    QString fnCmdRegisterGradient = "cmd_register_gradient.txt";
    //QString fnCmdRegisterDeform = "cmd_register_deform.txt";
    QString pathCmdRegister = m_strPathPlastimatch + "/" + fnCmdRegisterGradient;

    /*GenPlastiRegisterCommandFile(pathCmdRegister, filePathFixed, filePathMoving,
        filePathOutput, filePathXform, PLAST_GRADIENT, "", "", "");    */

    cout << "For Gradient searching only, CBCT image is a moving image, CT image is fixed image" << endl;
    GenPlastiRegisterCommandFile(pathCmdRegister, filePathMoving, filePathFixed,
         filePathOutput, filePathXform, PLAST_GRADIENT, "", "", "");

    /*void DlgRegistration::GenPlastiRegisterCommandFile(QString strPathCommandFile, QString strPathFixedImg, QString strPathMovingImg,
    QString strPathOutImg, QString strPathXformOut, enRegisterOption regiOption,
    QString strStageOption1, , QString strStageOption2, QString strStageOption3)*/

    //const char *command_filepath = pathCmdRegister.toLocal8Bit().constData();
    std::string str_command_filepath = pathCmdRegister.toLocal8Bit().constData();

    cout << "3: calling a plastimatch command" << endl;

    Registration reg;
    if (reg.set_command_file(str_command_filepath) < 0) {
        printf("Error.  could not load %s as command file.\n",
            str_command_filepath.c_str());
    }
    reg.do_registration();
    cout << "4: Registration is done" << endl; 

    cout << "5: Load shift values" << endl;

    VEC3D shiftVal = GetShiftValueFromGradientXForm(filePathXform, true); //true: inverse trans should be applied if CBCT was moving image //in mm    

    ImageManualMoveOneShot(shiftVal.x, shiftVal.y, shiftVal.z);

    cout << "6: Reading is completed" << endl;

    UpdateListOfComboBox(0);//combo selection signalis called
    UpdateListOfComboBox(1);

    SelectComboExternal(0, REGISTER_RAW_CBCT); // will call fixedImageSelected 
    SelectComboExternal(1, REGISTER_MANUAL_RIGID);
}

VEC3D DlgRegistration::GetShiftValueFromGradientXForm(QString& filePath, bool bInverse)
{
    VEC3D resVal;
    resVal.x = 0.0;
    resVal.y = 0.0;
    resVal.z = 0.0;

    ifstream fin;
    fin.open(filePath.toLocal8Bit().constData());

    if (fin.fail())
        return resVal;

    char str[MAX_LINE_LENGTH];
    //memset(str, 0, MAX_LINE_LENGTH);

    QString tmpStr;
    while (!fin.eof())
    {
        memset(str, 0, MAX_LINE_LENGTH);
        fin.getline(str, MAX_LINE_LENGTH);
        tmpStr = QString(str);

        if (tmpStr.contains("Parameters") && !tmpStr.contains("#"))
            break;
    }
    QStringList strList = tmpStr.split(" ");
    if (strList.count() < 4)
    {
        cout << "error: not enough shift information" << endl;
        return resVal;
    }
    
    resVal.x = strList.at(1).toDouble();
    resVal.y = strList.at(2).toDouble();
    resVal.z = strList.at(3).toDouble();

    if (bInverse)
    {
        resVal.x = -resVal.x;
        resVal.y = -resVal.y;
        resVal.z = -resVal.z;
    }
    
    fin.close();
    

    return resVal;

//#Insight Transform File V1.0 
//#Transform 0
//Transform: TranslationTransform_double_3_3
//Parameters: 1.407247543334961 45.48719024658203 -125.87542724609375
//         FixedParameters :
//

}

void DlgRegistration::SLT_ConfirmManualRegistration()
{
    if (!m_spFixed || !m_spMoving)
        return;

    if (!m_pParent->m_spRefCTImg || !m_pParent->m_spManualRigidCT)
        return;

    if (ui.checkBoxKeyMoving->isChecked())
        SLT_KeyMoving(false); // uncheck macro
    
    //Apply post processing for raw CBCT image and generate
    cout << "Preprocessing for CBCT" << endl;

    bool bPrepareMaskOnly = false;

    if (ui.checkBoxCropBkgroundCBCT->isChecked())
        bPrepareMaskOnly = false;
    else
        bPrepareMaskOnly = true;

    USHORT_ImageType::PointType originBefore = m_pParent->m_spRefCTImg->GetOrigin();
    USHORT_ImageType::PointType originAfter = m_pParent->m_spManualRigidCT->GetOrigin();

    double fShift[3];
    fShift[0] = (double)(originBefore[0] - originAfter[0]);//DICOM
    fShift[1] = (double)(originBefore[1] - originAfter[1]);
    fShift[2] = (double)(originBefore[2] - originAfter[2]);

    cout << "1: writing temporary files" << endl;

    //Both image type: Unsigned Short
    QString filePathFixed = m_strPathPlastimatch + "/" + "fixed_rigid.mha"; //CBCT image //redundant
    QString filePathFixed_proc = m_strPathPlastimatch + "/" + "fixed_rigid_proc.mha"; //After autoRigidbody Regi  

    //writing
    typedef itk::ImageFileWriter<USHORT_ImageType> writerType;
    writerType::Pointer writer = writerType::New();
    writer->SetFileName(filePathFixed.toLocal8Bit().constData());
    writer->SetUseCompression(true);
    writer->SetInput(m_spFixed); 
    writer->Update();


    cout << "1.A: Writing temporary files is done" << endl;

    QFileInfo finfoSkinFile1 = QFileInfo(m_strPathCTSkin);
    QString strPathAlternateSkin = m_strPathPlastimatch + "/" + "msk_skin_CT.mha";
    QFileInfo finfoSkinFile2 = QFileInfo(strPathAlternateSkin);

    QString strPathOriginalCTSkinMask;

    if (finfoSkinFile1.exists())
    {
        strPathOriginalCTSkinMask = m_strPathCTSkin;
        ProcessCBCT_beforeAutoRigidRegi(filePathFixed, strPathOriginalCTSkinMask, filePathFixed_proc, fShift, bPrepareMaskOnly);

        if (bPrepareMaskOnly) // currently, filePathFixed_proc == "";
            filePathFixed_proc = filePathFixed;

    }
    else if (finfoSkinFile2.exists())
    {
        cout << "alternative skin file will be used" << endl;
        strPathOriginalCTSkinMask = strPathAlternateSkin;        
        ProcessCBCT_beforeAutoRigidRegi(filePathFixed, strPathOriginalCTSkinMask, filePathFixed_proc, fShift, bPrepareMaskOnly);

        if (bPrepareMaskOnly) // currently, filePathFixed_proc == "";
            filePathFixed_proc = filePathFixed;
    }

    QFileInfo fInfo = QFileInfo(filePathFixed_proc);

    if (fInfo.exists() && !bPrepareMaskOnly) //if fixed_rigid_proc.mha is generated successfully.
    {

        cout << "Trying to read file: filePathFixed_proc" << endl;
        //Update RawReconImg
        typedef itk::ImageFileReader<USHORT_ImageType> readerType;
        readerType::Pointer reader = readerType::New();
        reader->SetFileName(filePathFixed_proc.toLocal8Bit().constData());
        reader->Update();
        m_pParent->m_spRawReconImg = reader->GetOutput();

        double tmpSkinMargin = ui.lineEditCBCTSkinCropBfRegid->text().toDouble();
        QString QSmarg = QString("Skin removed CBCT with margin %1 mm").arg(tmpSkinMargin);
        m_pParent->UpdateReconImage(m_pParent->m_spRawReconImg, QSmarg);

        cout << "Reading is completed" << endl;

        UpdateListOfComboBox(0);//combo selection signalis called
        UpdateListOfComboBox(1);

        SelectComboExternal(0, REGISTER_RAW_CBCT); // will call fixedImageSelected 
        SelectComboExternal(1, REGISTER_MANUAL_RIGID);

    }
	//Export final xform file
	QString filePathXform = m_strPathPlastimatch + "/" + "xform_manual.txt";
	
	ofstream fout;
	fout.open(filePathXform.toLocal8Bit().constData());
	fout << "#Custom Transform" << endl;
	fout << "#Transform: Translation_only" << endl;
	fout << "Parameters: " << fShift[0] << " " << fShift[1] << " " << fShift[2] << endl;
	//fout << "FixedParameters: 0 0 0" << endl;
	
	fout.close();
	cout << "Writing manual registration transform info is done." << endl;
}

void DlgRegistration::SLT_IntensityNormCBCT()
{
    float fROI_Radius = ui.lineEditNormRoiRadius->text().toFloat();


    cout << "Intensity is being analyzed...Please wait." << endl;

    float intensitySDFix =0.0;
    float intensitySDMov = 0.0;
    float meanIntensityFix = m_pParent->GetMeanIntensity(m_spFixed, fROI_Radius, &intensitySDFix);
    float meanIntensityMov = m_pParent->GetMeanIntensity(m_spMoving, fROI_Radius, &intensitySDMov);
    
    cout << "Mean/SD for Fixed = " << meanIntensityFix << "/" << intensitySDFix << endl;
    cout << "Mean/SD for Moving = " << meanIntensityMov << "/" << intensitySDMov << endl;

    //m_pParent->ExportReconSHORT_HU(m_spMoving, QString("D:/tmpExport.mha"));   

    m_pParent->AddConstHU(m_spFixed, (int)(meanIntensityMov - meanIntensityFix));
    //SLT_PassMovingImgForAnalysis();

    cout << "Intensity shifting is done! Added value = " << (int)(meanIntensityMov - meanIntensityFix) << endl;
    QString QSadded = QString("Added_%1").arg((int)(meanIntensityMov - meanIntensityFix));
    m_pParent->UpdateReconImage(m_spFixed, QSadded);
    SelectComboExternal(0, REGISTER_RAW_CBCT);
}
