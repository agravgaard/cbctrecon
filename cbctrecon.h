#ifndef CBCTRECON_H
#define CBCTRECON_H

#include <QtWidgets/QMainWindow>
//#include <QTimer>
#include "ui_cbctrecon.h"
#include "YK16GrayImage.h"

//#include "itkImage.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkTileImageFilter.h"
//
////class YK16GrayImage;
////
////#define BLACK_VALUE 0
////#define NORMAL_VALUE 1000
////#define WHITE_VALUE 4095
////#define CENTERLINE_VALUE 30000
////
////#define DEFAULT_CROSSHAIR_MARGIN 20
////#define DEFAULT_SAMPLING_PIXELS 3
////#define DEFAULT_ROI_RATIO_X 0.5
////#define DEFAULT_ROI_RATIO_Y 0.5
//#define DEFAULT_PERCENT_BADPIX_ON_COLUMN 30
//#define DEFAULT_PERCENT_BADPIX_ON_ROW 30
//

// RTK includes
#include <rtkHndImageIO.h>
#include <rtkXimImageIO.h>
#include <rtkVarianObiGeometryReader.h> // ELEKTASynergy VS VARIANObi
#include <rtkVarianProBeamGeometryReader.h> // ELEKTASynergy VS VARIANObi
#include <rtkThreeDCircularProjectionGeometryXMLFile.h>
#include <rtkThreeDCircularProjectionGeometry.h>

#include <itkConfigure.h>
#include <rtkConfiguration.h>
#include <rtkFDKBackProjectionImageFilter.h>
#include <rtkFDKConeBeamReconstructionFilter.h>
#include <rtkADMMTotalVariationConeBeamReconstructionFilter.h> // ADDED BY AGRAVGAARD


#ifdef CUDA_FOUND
#include <rtkCudaTotalVariationDenoisingBPDQImageFilter.h> // ADDED BY AGRAVGAARD
#else
#include <rtkTotalVariationDenoisingBPDQImageFilter.h> // ADDED BY AGRAVGAARD
#endif

#include <rtkTotalVariationImageFilter.h> // ADDED BY AGRAVGAARD
#include <itkStatisticsImageFilter.h> // ADDED BY AGRAVGAARD
#include <rtkConstantImageSource.h>


#include <rtkDisplacedDetectorImageFilter.h>
#include <rtkParkerShortScanImageFilter.h>
#include <rtkProjectionsReader.h>
#include <rtkFieldOfViewImageFilter.h>

#include <MMSystem.h>

// ITK includes
#include <itkStreamingImageFilter.h>
#include <itkRegularExpressionSeriesFileNames.h>
#include <itkMemoryProbesCollectorBase.h>
#include <itkEuler3DTransform.h>
#include <itkResampleImageFilter.h>
#include <itkFlipImageFilter.h>
#include <itkMultiplyImageFilter.h>
#include "itkCastImageFilter.h"
#include "itkAbsImageFilter.h"
#include "itkBinaryThresholdImageFilter.h"
#include "itkBinaryDilateImageFilter.h"
#include "itkBinaryErodeImageFilter.h"
#include "itkBinaryBallStructuringElement.h"
#include "itkBinaryFillholeImageFilter.h"
#include "itkMaskImageFilter.h"

//#include "plmreconstruct_config.h" // <- Are these still necessarY?
//#include "plm_int.h"
//#include "threading.h"
//#include "volume.h"

typedef float FloatPixelType;
typedef signed short SHORT_PixelType;
typedef unsigned short USHORT_PixelType;
#ifdef CUDA_FOUND
#include <cuda_runtime.h>
#include <itkCudaImageToImageFilter.h>
#include <rtkCudaFFTRampImageFilter.h>
#include <rtkCudaDisplacedDetectorImageFilter.h>
#include <rtkCudaParkerShortScanImageFilter.h>
typedef itk::CudaImage< FloatPixelType, 3 > CUDAOutputImageType;
typedef itk::CudaImage< itk::CovariantVector< FloatPixelType, 3 >, 3 > GradientCUDAOutputImageType;
typedef itk::CudaImage< FloatPixelType, 2 > CUDAOutputImageType2D;
typedef itk::CudaImage< USHORT_PixelType, 3 > USHORT_CUDAImageType;
typedef itk::CudaImage< USHORT_PixelType, 2 > USHORT_CUDAImageType2D;
typedef itk::CudaImage< SHORT_PixelType, 3 > SHORT_CUDAImageType;
typedef itk::CudaImage< SHORT_PixelType, 2 > SHORT_CUDAImageType2D;
#endif

typedef itk::Image< FloatPixelType, 3 >     OutputImageType;
typedef itk::Image< itk::CovariantVector
	< FloatPixelType, 3 >, 3 >                GradientOutputImageType;
typedef itk::Image< FloatPixelType, 2 > OutputImageType2D;

typedef itk::Image< USHORT_PixelType, 3 > USHORT_ImageType;
typedef itk::Image< USHORT_PixelType, 2 > USHORT_ImageType2D;

typedef itk::Image< SHORT_PixelType, 3 > SHORT_ImageType;
typedef itk::Image< SHORT_PixelType, 2 > SHORT_ImageType2D;


// typedef itk::Image< FloatPixelType, 3 > OutputImageType; //
typedef itk::ImageFileReader< OutputImageType > ReaderType;
typedef itk::ImageFileWriter< OutputImageType > WriterType;

typedef rtk::ThreeDCircularProjectionGeometry GeometryType;



#define DEFAULT_VARIAN_PROJ_WIDTH 1024 // ELEKTA VS VARIAN
#define DEFAULT_VARIAN_PROJ_HEIGHT 768 //1024 // ELEKTA VS VARIAN because why not use a random value, varian!?
#define MAX_LINE_LENGTH 1024

//lineEdit_scaMedian, when downsampling =1.0
#define DEFAULT_SCA_MEDIAN 25.0
//lineEdit_scaGaussian, when downsampling =1.0
#define DEFAULT_SCA_GAUSSIAN 3.0
//lineEdit_sca, when downsampling =1.0
#define DEFAULT_SCA_POST_PROJ_MEDIAN 6.0


enum enPLANE{
    PLANE_AXIAL = 0,
    PLANE_FRONTAL,
    PLANE_SAGITTAL,
};

enum enREGI_IMAGES{
  REGISTER_RAW_CBCT = 0,  
  REGISTER_REF_CT, //manual moving image
  REGISTER_MANUAL_RIGID, //manual moving image
  REGISTER_AUTO_RIGID,
  REGISTER_DEFORM1,
  REGISTER_DEFORM2,
  REGISTER_DEFORM3,
  REGISTER_DEFORM_FINAL,
  REGISTER_COR_CBCT,
  REGISTER_DEFORM_SKIP_AUTORIGID,
};

enum enMachineType{
  MACHINE_VARIAN = 0, // ELEKTA VS VARIAN ? <- Double replace done
  MACHINE_ELEKTA,
};

enum FWD_METHOD{
  en_Joseph = 0,
  en_CudaRayCast,
  en_RayCastInterpolator,		
};

struct WEPLData{
  double fWEPL;
  int ptIndex;
  double fGanAngle;
};
struct VEC3D{
  double x;
  double y;
  double z;  
};


struct FLEXDATA{
	float fGanAngle; //MV beam gantry angle
	float fPanelOffsetX; //MV beam gantry angle
	float fPanelOffsetY; //MV beam gantry angle	
};


class QStandardItemModel;

class DlgRegistration;
class DlgExternalCommand;
class QTimer;
class QXmlStreamReader;

using namespace std;

class CbctRecon : public QMainWindow
{
	Q_OBJECT

public:
	CbctRecon(QWidget *parent = 0, Qt::WindowFlags flags = 0);
	~CbctRecon();			
	//void DoRecon();
	void ReleaseMemory();
	void RenameFromHexToDecimal(QStringList& filenameList);
	QString HexStr2IntStr(QString& strHex);

	QString CorrectSingleFile(const char* filePath);	
	void CorrectSingleFile(YK16GrayImage* pYKRawImg);

	void LoadBadPixelMap(const char* filePath);
	void BadPixReplacement(YK16GrayImage* targetImg);

	void LoadRTKGeometryFile(const char* filePath);

	//void GetSelectedIndices(const vector<double>& vFullAngles, vector<double>& vNormAngles, vector<int>& vTargetIdx, bool bCW);
	void GetExcludeIndexByNames(QString outlierListPath, vector<string>& vProjFileFullPath, vector<int>& vExcludeIdx);
	void GetSelectedIndices(const vector<double>& vFullAngles, vector<double>& vNormAngles, vector<int>& vTargetIdx, bool bCW, vector<int>& vExcludingIdx);

	void SetMaxAndMinValueOfProjectionImage(); // scan m_spProjImg3D and update m_fProjImgValueMin, max
	void GetMaxAndMinValueOfProjectionImage(double& fProjImgValueMax, double& fProjImgValueMin, OutputImageType::Pointer projImage);

	double GetValueFrom3DImageFloat(int reqX, int reqY, int reqZ, OutputImageType::Pointer& sp3DFloatImage);
	double GetValueFrom3DImageUshort(int reqX, int reqY, int reqZ, USHORT_ImageType::Pointer& sp3DUshortImage);

	bool IsFileNameOrderCorrect(vector<string>& vFileNames);

	void PostApplyFOVDispParam();

	//void ExportDICOM_SHORT(SHORT_ImageType::Pointer& sp3DshortImage);//NOT COMPLETED YET!! Export DICOM without Source DICOM is not possible
	void CopyDictionary (itk::MetaDataDictionary &fromDict, itk::MetaDataDictionary &toDict);//NOT COMPLETED YET!! Export DICOM without Source DICOM is not possible

	void DoBeamHardeningCorrection();

	void Draw2DFrom3D(USHORT_ImageType::Pointer& pImg, enPLANE direction, double pos, YK16GrayImage& pOutput2D);
	void Draw2DFrom3DDouble(USHORT_ImageType::Pointer& spFixedImg, USHORT_ImageType::Pointer& spMovingImg, enPLANE direction, double pos, YK16GrayImage& YKFixed, YK16GrayImage& YKMoving);

	void RegisterImgDuplication(enREGI_IMAGES src, enREGI_IMAGES target);

	
	//plastimatch skin / bubble-remover
	//QString getPathCTDir(enMachineType enType);//DICOM Dir
	//QString getPathRS(enMachineType enType);//RS path
	//bool loadPlanCTFromDCM(QString& strCTDirPath, QString& strRSPath);//using plastimatch, prepare m_spRefCTImg. Remove air, RS is needed
	//Skin will be removed, bubble will be filled	

	void FindAllRelevantPaths(QString pathProjHisDir);
	QString MakeVarianXML(QString filePath_ImageXML, QString DICOM_UID); // ELEKTA VS VARIAN

	bool LoadShortImageToUshort(QString& strPath, USHORT_ImageType::Pointer& pUshortImage);
	void init_DlgRegistration(QString& strDCM_UID);

	//using RTK forward projection algorithm, generate 2D projection image files (as line integral, mu_t)
	void ForwardProjection (USHORT_ImageType::Pointer& spVolImg3D, GeometryType::Pointer& spGeometry, USHORT_ImageType::Pointer& spProjCT3D, bool bSave);
	//to be implemented: Save projection3D to *.his files	
	void GenScatterMap_PriorCT(USHORT_ImageType::Pointer& spProjRaw3D, USHORT_ImageType::Pointer& spProjCT3D, USHORT_ImageType::Pointer& spProjScat3D, double medianRadius, double gaussianSigma, int nonNegativeScatOffset, bool bSave);
	void ScatterCorr_PrioriCT(USHORT_ImageType::Pointer& spProjRaw3D, USHORT_ImageType::Pointer& spProjScat3D, USHORT_ImageType::Pointer& m_spProjCorr3D, int nonNegativeScatOffset, int postMedian, bool bSave);
	void AfterScatCorrectionMacro();

	//His file export from 3D proj file
	void SaveProjImageAsHIS(USHORT_ImageType::Pointer& m_spProj3D, YK16GrayImage* arrYKImage, QString& strSavingFolder, int iCnt, double resampleF); //arrYKImage include HIS header and original file name	
	
	void ConvertLineInt2Intensity( OutputImageType::Pointer& spProjLineInt3D, USHORT_ImageType::Pointer& spProjIntensity3D, int bkIntensity );
	void ConvertIntensity2LineInt(USHORT_ImageType::Pointer& spProjIntensity3D, OutputImageType::Pointer& spProjLineInt3D, int bkIntensity);
	
	void Get2DFrom3D( USHORT_ImageType::Pointer& spSrcImg3D, OutputImageType2D::Pointer& spTargetImg2D, int idx, enPLANE iDirection);	
	void Set2DTo3D( OutputImageType2D::Pointer& spSrcImg2D, USHORT_ImageType::Pointer& spTargetImg3D, int idx, enPLANE iDirection);	
	
	void AllocateByRef( USHORT_ImageType::Pointer& spRefImg3D, USHORT_ImageType::Pointer& spTarImg3D );
	void AllocateByRef(OutputImageType2D::Pointer& spRefImg2D, OutputImageType2D::Pointer& spTarImg2D);
	void AllocateByRef( OutputImageType::Pointer& spRefImg3D, OutputImageType::Pointer& spTarImg3D );
	void AllocateByRef(USHORT_ImageType::Pointer& spRefImg3D, OutputImageType::Pointer& spTarImg3D);
	void AllocateByRef(OutputImageType::Pointer& spRefImg3D, USHORT_ImageType::Pointer& spTarImg3D);

	//void ResampleItkImage(OutputImageType::Pointer& spImgFloat, double resampleF);
	//Resample proj images
	void ResampleItkImage( OutputImageType::Pointer& spSrcImg, OutputImageType::Pointer& spTarImg, double resampleF );
	void ResampleItkImage( USHORT_ImageType::Pointer& spSrcImg, USHORT_ImageType::Pointer& spTarImg, double resFactor );
	void ResampleItkImage2D(OutputImageType2D::Pointer& spSrcImg2D, OutputImageType2D::Pointer& spTarImg2D, double resFactor); //using slice iterator

	void DoReconstructionFDK(enREGI_IMAGES target);
	void CudaDoReconstructionFDK(enREGI_IMAGES target);
	void CudaDoReconstructionTV(enREGI_IMAGES target); // ADDED BY AGRAVGAARD
	void DoReconstructionTV(enREGI_IMAGES target); // ADDED BY AGRAVGAARD

	
	void UpdateReconImage(USHORT_ImageType::Pointer& spNewImg, QString& fileName);

	//void SaveUSHORTAsSHORT_DICOM (USHORT_ImageType::Pointer& spImg, QString& strPatientID, QString& strPatientName);//ushort image --> short image --> 
	void SaveUSHORTAsSHORT_DICOM( USHORT_ImageType::Pointer& spImg, QString& strPatientID, QString& strPatientName, QString& strPathTargetDir);

	void ConvertUshort2Short(USHORT_ImageType::Pointer& spImgUshort, SHORT_ImageType::Pointer& spImgShort);

	double GetRawIntensityScaleFactor();

	void GetAngularWEPL_SinglePoint(USHORT_ImageType::Pointer& spUshortImage, float fAngleGap, float fAngleStart, float fAngleEnd, VEC3D calcPt, int curPtIdx, vector<WEPLData>& vOutputWEPLData, bool bAppend);

	void GetAngularWEPL_MultiPoint(USHORT_ImageType::Pointer& spUshortImage, float fAngleGap, float fAngleStart, float fAngleEnd, vector<WEPLData>& vOutputWEPLData, bool bAppend);

//	void UpdateUIAfterLoading(QString& imgName);

	void LoadExternalFloatImage(QString& strPath, bool bConversion);
	void TransformationRTK2IEC(OutputImageType::Pointer& spSrc);

	void MedianFilterByGUI(); //params are given at the UI
	void FileExportByGUI();//params are given at the UI


	/*Temporary implementation for XVI5 xml*/
	void LoadXVIGeometryFile(const char* filePath); //temporary implenetation using QT XML. This is for XVI v >5.0.2. _Frames.xml is in every projection folder 
	
	FLEXDATA XML_parseFrameForXVI5(QXmlStreamReader& xml);
	QString XML_GetSingleItemString(QXmlStreamReader& xml);

	bool GetXrayParamFromINI(QString& strPathINI, float& kVp, float& mA, float& ms);

	void SetProjDir(QString& strProjPath);

	bool FullScatterCorrectionMacroSingle(QString& outputDirPath, enREGI_IMAGES enFwdRefImg, bool bFullResolRecon, bool bExportImages = false, bool bCBCT_IntensityShift = false);

	void ExportAngularWEPL_byFile(QString& strPathOutput);

	void OptimizedExportAngularWEPL_byFile(QString& strPathOutput);

	void ExportReconSHORT_HU(USHORT_ImageType::Pointer& spUsImage, QString& outputFilePath);
	/*Temporary implementation for XVI5 xml*/


	//using itkimage and cuda do median filter faster. Output will be overwritten
//	void cudaMedianFilter2DITK(OutputImageType2D::Pointer& spUshortImage, int wndSizeX, int wndSizeY);

	//double CropSkinUsingRS(USHORT_ImageType::Pointer& spImgUshort, QString& strPathRS, double cropMargin);

	//void AuditMemory();

	void CropFOV3D(USHORT_ImageType::Pointer& sp_Img, float physPosX, float physPosY, float physRadius, float physTablePosY);

	void GenerateCylinderMask(USHORT_ImageType::Pointer& spImgCanvas, float fDcmPosX, float fDcmPosY, float fRadius);

	float GetMeanIntensity(USHORT_ImageType::Pointer& spImg, float sphereR, float* sdIntensity = NULL);

	void AddConstHU(USHORT_ImageType::Pointer& spImg, int HUval);

	bool ResortCBCTProjection(vector<int>& vIntPhaseBinSelected, QString& strPathForXML, QString& strPathProjRoot, QString& strUID, vector<float>& vFloatPhaseFull, GeometryType::Pointer& spGeomFull, vector<string>& vProjPathsFull);

	void AppendInPhaseIndex(int iPhase, vector<float>& vFloatPhaseFull, vector<int>& vOutputIndex, int margin = 5);

	//using RTK forward projection algorithm, generate 2D projection image files (as line integral, mu_t)
	public slots:			
		void SLT_LoadRawImages(); //independent 2d projection files //not used in clinical case
		void SLT_LoadRawHndImages(); 
		void SLT_LoadRawXimImages(); 
		void SLT_Load3DImage(); //indenepndent 3D mha file. UshortFormat. Do reconstruction is an antoher way to make m_spReconImg
		void SLT_Load3DImageShort();
		void SLT_LoadNKIImage();
		void SLT_LoadSelectedProjFiles(); //based on presetting values on GUI, including geometry files
		void SLT_ExportHis();
		void SLT_LoadPlanCT_mha();
		void SLT_LoadPlanCT_USHORT();
		void SLT_LoadImageFloat3D();  //Dose file
		void SLTM_LoadDICOMdir(); //independent 2d projection files //not used in clinical case
		void SLTM_LoadRTKoutput();
		void SLT_DrawRawImages(); //external *.his images
		void SLT_DrawProjImages(); // draw images from HIS FILE READER or filtered image before going into recon.
		void SLT_DrawReconImage();
		//tools
		void SLT_FileNameHex2Dec();	
		void SLT_MakeVarianXML();		// ELEKTA VS VARIAN
		//Gain/ Offset correction
		void SLT_OpenOffsetFile();
		void SLT_OpenGainFile();
		void SLT_OpenBadpixelFile();
		void SLT_ApplyCalibration();
		//Gain/ Offset correction
		void SLT_SetHisDir();
		void SLT_OpenVarianGeomFile(); // ELEKTA VS VARIAN
		void SLT_SetOutputPath();
		void SLT_DoReconstruction();
		//Profile table
		//void SLT_GetProjectionProfile();
		//void SLT_GetReconImgProfile();
		void SLT_CopyTableToClipBoard();
		void SLT_DataProbeProj();
		void SLT_DataProbeRecon();
		void SLT_DrawGraph();
		void SLT_InitializeGraphLim();
		void SLT_UpdateTable();
		void SLT_CalculateROI_Recon();
		void SLT_CalculateROI_Proj();
		void SLT_GoForcedProbePos();
		void SLT_PostApplyFOVDispParam();
		void SLT_DoPostProcessing(); //cropping Circle
        void SLT_PostProcCropInv();
		void SLT_ExportReconUSHORT();
		void SLT_ExportReconSHORT_HU();
		void SLT_DoBHC();
		void SLT_DoBTC();
		void SLT_Export2DDose_TIF();
		void SLTM_Export2DDoseMapAsMHA();
		void SLT_ViewRegistration();
		void SLT_DoScatterCorrection_APRIORI();
		void SLT_TempAudit();
		void SLT_CalcAndSaveAngularWEPL();
		void SLT_DoScatterCorrectionUniform();
		void SLT_FileExportShortDICOM_CurrentImg();
		void SLT_AddConstHUToCurImg();
		void SLT_SetCBCTSkinRSPath();
		void SLT_CropSkinUsingRS();
		void SLT_CropSkinUsingThreshold();
		void SLT_ExportAngularWEPL_byFile();
		void SLT_OptExportAngularWEPL_byFile();
		void SLT_GeneratePOIData();
		void SLT_LoadPOIData();
		void SLT_StartSyncFromSharedMem();
		void SLT_StopSyncFromSharedMem();
		void SLT_TimerEvent();
		void SLTM_ViewExternalCommand();
		void SLT_MedianFilterDoNow();
		void SLTM_ExportProjGeometryTXT();
        void SLTM_ForwardProjection();
        void SLTM_FineResolScatterCorrectrionMacro();//projection: full, scatter map:512x512
        void SLTM_FullScatterCorrectionMacroAP();
        void SLTM_BatchScatterCorrectionMacroAP();
		void SLT_OpenPhaseData(); //fill lineEdit_PhaseTxtPath
		void SLT_Export4DCBCT(); //phase resorting

public:

	//ReaderType::Pointer m_reader;
	//WriterType::Pointer m_writer;

	YK16GrayImage* m_arrYKImage; //independent raw images
	int m_iImgCnt; //for independent raw images --> no relation to Directroy based projections

	YK16GrayImage* m_arrYKBufProj;
	int m_iCntSelectedProj;

	YK16GrayImage* m_pImgOffset;
	YK16GrayImage* m_pImgGain;
	//Badpixmap;
	vector<BADPIXELMAP> m_vPixelReplMap;	

	//RTK recon
	GeometryType::Pointer m_spFullGeometry; //sp = smart pointer
	GeometryType::Pointer m_spCustomGeometry;
	bool ximIsUsed = true;
	bool m_bScanDirectionCW;

	OutputImageType::Pointer m_spProjImg3DFloat; //This is float image loaded by RTK. line integral (mu_t value). To convert this to Intensity, use mu t = ln(65535/I)

	USHORT_ImageType::Pointer m_spProjImgRaw3D;//raw intensity value converted from line integral (mu_t). 0-65535
	USHORT_ImageType::Pointer m_spProjImgCT3D;//1.5G // release this after Scatter Generation
	USHORT_ImageType::Pointer m_spProjImgScat3D; //scatter map proj file using any scatter-estimation method//1.5G //release this after Cor gen
	USHORT_ImageType::Pointer m_spProjImgCorr3D; //proj file-scatter corrected one using either priori CT or any other method//1.5G
	
	USHORT_ImageType::Pointer m_spCrntReconImg; //fixed image // ID: RawCBCT
	USHORT_ImageType::Pointer m_spRawReconImg; //just added --> when file is loaded
	USHORT_ImageType::Pointer m_spScatCorrReconImg;//just added --> after scatter correction

    USHORT_ImageType::Pointer m_spRefCTImg;//filled by SLT_LoadPlanCT_mha(); ID: RefCT_Original
	USHORT_ImageType::Pointer m_spManualRigidCT;//copied from RefCTImg; ID: RefCT --> Moving Img, cloned
	USHORT_ImageType::Pointer m_spAutoRigidCT; // ID: AutoRigidCT    
	USHORT_ImageType::Pointer m_spDeformedCT1; //Deformmation will be carried out based on Moving IMage of GUI //AutoDeformCT1
	USHORT_ImageType::Pointer m_spDeformedCT2; //AutoDeformCT2
	USHORT_ImageType::Pointer m_spDeformedCT3; //AutoDeformCT3
	USHORT_ImageType::Pointer m_spDeformedCT_Final; //AutoDeformCT3

	YK16GrayImage* m_dspYKReconImage;
	YK16GrayImage*	m_dspYKImgProj;
	int m_iTmpIdx;

	double m_fProjImgValueMax; //value of float image
	double m_fProjImgValueMin;

	double m_multiplyFactor;
	QStandardItemModel *m_pTableModel;
    DlgRegistration* m_pDlgRegistration;
	DlgExternalCommand* m_pDlgExternalCommand;

	//Automatically detected relavant file/Dir path when load the projection files (SLT_SetHisDir)

	//Belows are ELEKTA specific // ELEKTA VS VARIAN !!!
	//1) Find DICOM UID by subtracting "img_" from whole proj path name

	//Below paths will be decided after the Find... Func.
	QString m_strDCMUID;
	QString m_strPathPatientDir; //full path of patient Directory
	QString m_strPatientDirName; //just the name --> later I can extract the patient ID from here
	QString m_strPathFRAME_DBF;
	QString m_strPathIMAGE_XML;
	QString m_strPathGeomXML; //after Generation of the XML from DBF files
	QString m_strPathPlanCTDir;
	QString m_strPathRS; //for body and lung contours
	QString m_strPathPlan; //for isocenter position
	QString m_strPathDirDefault; //QFileDialog default starting point
	QString m_strPathRS_CBCT; //QFileDialog default starting point
	QString m_strPathVarianINI; //for mAs values
	QString m_strPathIMAGES;//upper folder of projection files (His)
	int m_iFixedOffset_ScatterMap;//fixed! allows negative value of scatter
	double m_fResampleF; //typically 0.5. this is updated during LoadSelectedProj image and ui.lineEdit_DownResolFactor.//also affects all other scatter correction method
	double m_fProjSpacingX; //updated from SelectedProjLoad
	double m_fProjSpacingY; //updated from SelectedProjLoad

	vector<VEC3D> m_vPOI_DCM;//initialized by file Load

	QTimer* m_Timer;
	bool m_busyTimer;

	vector<string> m_vSelectedFileNames;

	bool m_bMacroContinue;
	
	vector<float> m_vPhaseFloat;

//private:
public:
	Ui::CbctReconClass ui;
};



#endif // BADPIXELDETECTOR_H 
