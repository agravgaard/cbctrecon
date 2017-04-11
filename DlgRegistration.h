#pragma once
#include <QDialog>
#include "ui_DlgRegistration.h"
#include "itkImage.h"
#include "itk_mask.h"
#include "YK16GrayImage.h"
#include <QString>
#include "cbctrecon.h"



//#include "pcmd_synth_vf.h"
//class CbctRecon;
//class YK16GrayImage;
class qyklabel;

class Dmap_parms;
class Pcmd_threshold;
class Mask_parms;

class Plm_image_header;

class Dcmtk_rt_study;
//class Synthetic_vf_main_parms;


//typedef unsigned short USHORT_PixelType;
//typedef itk::Image< USHORT_PixelType, 3 > USHORT_ImageType;

enum enViewArrange{
  AXIAL_FRONTAL_SAGITTAL = 0,
  FRONTAL_SAGITTAL_AXIAL,
  SAGITTAL_AXIAL_FRONTAL,
};

enum enRegisterOption{
  PLAST_RIGID = 0,
  PLAST_GRADIENT,
  PLAST_AFFINE,
  PLAST_BSPLINE,
};

#define DEFAULT_LABEL_SIZE1 512
#define DEFAULT_LABEL_SIZE2 256
#define DEFAULT_LABEL_SIZE3 256


class DlgRegistration : public QDialog,
    public Ui::DlgRegistrationClass
{
    Q_OBJECT
    ;

public slots:
    void SLT_CrntPosGo();
    void SLT_DrawImageWhenSliceChange(); //upper level drawing: big calculation
    void SLT_DrawImageInFixedSlice();//lower level Drawing func.

    void SLT_UpdateSplit1();//lower level Drawing func. //Mouse Move even
    void SLT_UpdateSplit2();//lower level Drawing func.//Mouse Move even
    void SLT_UpdateSplit3();//lower level Drawing func.//Mouse Move even

	void SLT_CancelMouseAction();

	void SLT_MouseWheelUpdate1();
	void SLT_MouseWheelUpdate2();
	void SLT_MouseWheelUpdate3();

	void SLT_MousePressedLeft1();
	void SLT_MousePressedLeft2();
	void SLT_MousePressedLeft3();

	void SLT_MousePressedRight1();
	void SLT_MousePressedRight2();
	void SLT_MousePressedRight3();

	void SLT_MouseReleasedLeft1();
	void SLT_MouseReleasedLeft2();
	void SLT_MouseReleasedLeft3();

	void SLT_MouseReleasedRight1();
	void SLT_MouseReleasedRight2();
	void SLT_MouseReleasedRight3();
	void SLT_ChangeView();//3 toggle button

	void SLT_RestoreImageSingle();
	void SLT_RestoreImageAll();

	void SLT_DoRegistrationRigid();
	void SLT_DoRegistrationDeform();
        void SLT_DoRegistrationGradient();
        void SLT_ManualMoveByDCMPlan();

	void SLT_BringFocusToEnableArrow(bool bChecked);

	void SLT_KeyMoving(bool bChecked);

	//Image selection event from Combobox
	void SLT_FixedImageSelected(QString selText); //here, when fixed_image_loaded function will be called
	void SLT_MovingImageSelected(QString selText);//here, when mvoing_image_loaded function will be called

	void SLT_RestoreMovingImg();//copy refCT to Manual Moving Image.

	void SLT_PreProcessCT();

	void SLT_PassFixedImgForAnalysis();
	void SLT_PassMovingImgForAnalysis();
	void SLT_Macro();

	void SLT_ExchangeRawRef();

        /* After manual move: this will trigger skin cropping and uncheck the key moving*/
        void SLT_ConfirmManualRegistration();
        void SLT_IntensityNormCBCT();
        


public:
    DlgRegistration();    
    DlgRegistration(QWidget *parent);
    ~DlgRegistration();
     void TestDraw();
     void whenFixedImgLoaded(); //should be called by comboBox
     void whenMovingImgLoaded();
	 void initOverlapWndSize();
	 void shiftSliceSlider();
	 void updateSliceLabel();
	 void UpdateSplit(int viewIdx, qyklabel* pOverlapWnd);
	 void MousePressedRight(int wndIdx, qyklabel* pWnd);

	 void childEvent(QChildEvent *event);
	 bool eventFilter(QObject *target, QEvent *event);

	 void ImageManualMove(int direction, double resol);
         void ImageManualMoveOneShot(float shiftX, float shiftY, float shiftZ);//DICOM coordinate

	// void GenPlastiRegisterCommandFile(QString strPathCommandFile, enRegisterOption regiOption);
	 /*void GenPlastiRegisterCommandFile(QString strPathCommandFile, QString strPathFixedImg, QString strPathMovingImg,
	   QString strPathOutImg, QString strPathXformOut, enRegisterOption regiOption,
	   QString strStageOption1, QString strStageOption2, QString strStageOption3);*/

         void GenPlastiRegisterCommandFile(QString strPathCommandFile, QString strPathFixedImg, QString strPathMovingImg,
             QString strPathOutImg, QString strPathXformOut, enRegisterOption regiOption,
             QString strStageOption1, QString strStageOption2, QString strStageOption3, const QString& strPathFixedMask = QString(""));

         //VEC3D GetShiftValueFromGradientXForm(QString& filePath); //get val mm
         VEC3D GetShiftValueFromGradientXForm(QString& filePath, bool bInverse = false);


	 void AddImageToCombo(int comboIdx, enREGI_IMAGES option); //comboIdx 0: fixed, 1: moving
	 void LoadImgFromComboBox(int idx, QString& strSelectedComboTxt);

	 void SelectComboExternal(int idx, enREGI_IMAGES iImage);

	 void UpdateListOfComboBox(int idx);
	 bool PreprocessCT();
         void LoadRTPlan(QString& strDCMPath);

	 //void plm_dmap_main (Dmap_parms* parms);
         void plm_dmap_main(QString& img_in_fn, QString& img_out_fn);
	 //void plm_threshold_main (Pcmd_threshold* parms);
         //void plm_threshold_main (Pcmd_threshold* parms);
         //plm_threshold_main(range_string, img_in_fn, img_out_fn);
         void plm_threshold_main(QString& strRange, QString& img_in_fn, QString& img_out_fn);
	 //void plm_mask_main (Mask_parms* parms);                  
         void plm_mask_main(Mask_operation mask_option, QString& input_fn, QString& mask_fn, QString& output_fn, float mask_value);


	 void plm_expansion_contract_msk(QString& strPath_msk, QString& strPath_msk_exp_cont, double fExpVal);

	 void plm_synth_trans_xf( QString& strPath_fixed, QString& strPath_out_xf, double transX, double transY, double transZ);
	  void ProcessCBCT_beforeAutoRigidRegi(QString& strPathRawCBCT, QString& strPath_mskSkinCT, QString& strPathOutputCBCT, double* manualTrans3d, bool bPrepareMaskOnly = false);

	 //void ProcessCBCT_beforeDeformRegi(QString& strPathRawCBCT, QString& strPath_mskSkinCT_, QString& strPathOutputCBCT, double* manualTrans3d); 
          void ProcessCBCT_beforeDeformRegi(QString& strPathRawCBCT, QString& strPath_mskSkinCT_manRegi, QString& strPathOutputCBCT, QString& strPathXFAutoRigid, bool bBubbleFilling, bool bPrepareMaskOnly = false);//8 mm skin cut + fill air bubbles inside CBCT

	 void SetPlmOutputDir(QString& endFix);
	 void init(QString& strDCMUID);

	 void PostSkinRemovingCBCT(USHORT_ImageType::Pointer& spCBCT); //this function will be called from main Dlg.

	 void CropSkinUsingRS(USHORT_ImageType::Pointer& spImgUshort, QString& strPathRS, double cropMargin );

	 //void keyPressEvent ( QKeyEvent * e );
	 
     //void Draw2DFrom3D(USHORT_ImageType::Pointer& pImg, enPLANE direction, double pos, YK16GrayImage* pOutput2D);
     //void Draw2DFrom3D(USHORT_ImageType::Pointer& pImg, enPLANE direction, double pos, YK16GrayImage& Output2D);
    

public: 
    CbctRecon* m_pParent; //to pull 3D images
    YK16GrayImage m_YKImgFixed[3]; //CBCT in this study
    YK16GrayImage m_YKImgMoving[3]; //CBCT in this study
    YK16GrayImage m_YKDisp[3]; //CBCT in this study
	int m_enViewArrange;
    //YK16GrayImage m_YKImgMoving[3];    //RefCT

	bool m_bPressedLeft[3];//Left Mouse Pressed but not released
	bool m_bPressedRight[3];
	QPoint m_ptWindowLevelStart;//data point

	QPoint m_ptPanStart;//data point

	QPoint m_ptTmpOriginalDataOffset;
	int m_iTmpOriginalW;
	int m_iTmpOriginalL;

	USHORT_ImageType::Pointer m_spFixed;//pointer only, for display
	USHORT_ImageType::Pointer m_spMoving;//pointer only, for display


	QString m_strPathPlastimatch;//full path
	QString m_strPathCTSkin; // shared data among functions
	QString m_strPathCTSkin_manRegi; // shared data among functions, manually registered to CBCT, w/ margin ( 10mm)
	QString m_strPathCTSkin_autoRegi; // shared data among functions w/ margin (10 mm), registered to CBCT
	QString m_strPathCTSkin_deformRegi; //this is for WEPL calculation
	QString m_strPathXFAutoRigid; //Updated during SLT_DoRegistrationRigid
	QString m_strPathMskCBCTBubble; //Updated during SLT_DoRegistrationRigid



        Dcmtk_rt_study* m_pDcmStudyPlan;

      


private:
    Ui::DlgRegistrationClass ui;
	
};
