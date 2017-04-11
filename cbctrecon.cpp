#include "cbctrecon.h"
#include "YK16GrayImage.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>
#include <algorithm>
#include <fstream>
#include <QInputDialog>

#include <windows.h>

#include "itkImageDuplicator.h"
#include "itkImageSliceConstIteratorWithIndex.h"
#include "itkImageSliceIteratorWithIndex.h"


#include <QStandardItemModel>
#include <QClipBoard>
#include <QVector>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QProcess>

// ITK includes
#include "itkIdentityTransform.h"
//#include "itkResampleImageFilter.h" //AGRAVGAARD
#include "itkRescaleIntensityImageFilter.h"
#include "itkAddImageFilter.h"
#include "itkThresholdImageFilter.h"
#include "itkMinimumMaximumImageCalculator.h"
#include "itkMedianImageFilter.h"
#include "itkMeanImageFilter.h" //YKTEMP20141218
#include "itkGPUMeanImageFilter.h"

// #include "itkGDCMImageIO.h" //AGRAVGAARD
// #include "itkGDCMSeriesFileNames.h" //AGRAVGAARD
#include "itkNumericSeriesFileNames.h"
#include "itkImageSeriesReader.h"
#include "itkImageSeriesWriter.h"
#include "itkShiftScaleImageFilter.h"
#include "DlgRegistration.h"
#include "DlgExternalCommand.h"

#include "itkAffineTransform.h"
#include "itkNearestNeighborInterpolateImageFunction.h"
#include <itksys/SystemTools.hxx>

#include "mha_io.h"
#include "nki_io.h"
#include "plm_image.h"
#include "rt_study_metadata.h"

#include "aperture.h"
#include "Rt_beam.h"
#include "Rt_plan.h"
#include "plm_math.h"
#include "proj_volume.h"
#include "ray_trace_probe.h"
#include "rpl_volume.h"
#include "volume.h"
#include "volume_limit.h"
#include "wed_parms.h"
#include "proj_matrix.h"
#include "ray_data.h"


#if ITK_VERSION_MAJOR >= 4
#include "gdcmUIDGenerator.h"
#else
#include "gdcm/src/gdcmFile.h"
#include "gdcm/src/gdcmUtil.h"
#endif



#if CUDA_FOUND
# include "cuda_runtime.h" // Don't know why, but it's included in some cuda-test files. AGRAVGAARD
# include "itkCudaImageToImageFilter.h"
# include "rtkCudaFFTRampImageFilter.h"
# include "rtkCudaDisplacedDetectorImageFilter.h"
# include "rtkCudaParkerShortScanImageFilter.h"
# include "rtkCudaFDKConeBeamReconstructionFilter.h"
# include "rtkCudaTotalVariationDenoisingBPDQImageFilter.h" // ADDED BY AGRAVGAARD
# include "rtkCudaForwardProjectionImageFilter.h"
#endif

# include "rtkADMMWaveletsConeBeamReconstructionFilter.h" // ADDED BY AGRAVGAARD
# include "rtkConjugateGradientConeBeamReconstructionFilter.h" // ADDED BY AGRAVGAARD
# include "rtkSARTConeBeamReconstructionFilter.h" // ADDED BY AGRAVGAARD
# include "rtkTotalVariationImageFilter.h" // ADDED BY AGRAVGAARD
# include "itkStatisticsImageFilter.h" // ADDED BY AGRAVGAARD
# include "rtkDownsampleImageFilter.h"


#if OPENCL_FOUND
#include "rtkOpenCLFDKConeBeamReconstructionFilter.h"
#endif


//related includes for fowward projection
//#include "itkMedianImageFilter.h"
#include "itkIdentityTransform.h"
#include "itkResampleImageFilter.h"
#include "itkRescaleIntensityImageFilter.h"
#include <itkEuler3DTransform.h>
#include <itkFlipImageFilter.h>
#include "itkSmoothingRecursiveGaussianImageFilter.h"
#include "rtkJosephForwardProjectionImageFilter.h"
#include "rtkRayCastInterpolatorForwardProjectionImageFilter.h"
#include "rtkBoellaardScatterCorrectionImageFilter.h"
// #include "rtkFieldOfViewImageFilter.h"

#include <QXmlStreamReader>


#define round(dTemp) (long(dTemp+(dTemp>0? .5:-.5)))

using namespace std;
//extern "C" void YKCudaMedianWrapFloat(float* CPUinput, float* CPUoutput, const int width, const int height, const int fWidth, const int fHeight);

struct TIFIFD{
	unsigned short TagID;
	unsigned short DataType;
	int DataCnt;
	int DataOrOffset ;
};

struct RATIONAL
{
	long a;
	long b;
};

void InsertHeaderToArray(TIFIFD* IFDArr, int insertIdx, int TagID, int dataType, int DataCnt, int dataVal);
bool SaveDoseGrayImage(const char* filePath, int width, int height, double spacingX, double spacingY, double originLeft_mm, double originTop_mm, unsigned short* pData);
double vectorMean(const vector<double>& vDouble);
double vectorSum(const vector<double>& vDouble);

CbctRecon::CbctRecon(QWidget *parent, Qt::WindowFlags flags)
: QMainWindow(parent, flags)
{
	int aaa = 5;
	ui.setupUi(this);

	//m_iWidth = 2304;
	//m_iHeight = 3200;

	//m_pImageYKDark = new YK16GrayImage(2304,3200);
	//m_pImageYKGain = new YK16GrayImage(2304, 3200);
	////m_pMedianYKImg = new YK16GrayImage(2304, 3200);
	//m_fPercentThre = 30.0; //30% increase is mandatory
	//m_iMedianSize = 3;
	m_arrYKImage = NULL;

	m_dspYKReconImage = new YK16GrayImage();
	m_dspYKImgProj = new YK16GrayImage();

	m_pImgOffset = NULL;
	m_pImgGain = NULL;
	//Badpixmap;
	m_pImgOffset = new YK16GrayImage(DEFAULT_VARIAN_PROJ_WIDTH, DEFAULT_VARIAN_PROJ_HEIGHT); // ELEKTA VS VARIAN
	m_pImgGain = new YK16GrayImage(DEFAULT_VARIAN_PROJ_WIDTH, DEFAULT_VARIAN_PROJ_HEIGHT); // ELEKTA VS VARIAN
	//Prepare Raw image

	//ConvertImg_4030eToElektaProjRaw("C:\\4030eGain_2048_3200.raw", "C:\\TestGain_1024_1024.raw");
	//ConvertImg_4030eToElektaProjRaw("C:\\4030eOffset_2048_3200.raw", "C:\\TestOffset_1024_1024.raw");

	//m_reader = ReaderType::New();
	//m_writer = WriterType::New();
	m_bScanDirectionCW = true;

	//Disable cuda & opencl as defaults
	ui.radioButton_UseCPU->setChecked(true);
	ui.radioButton_UseCUDA->setDisabled(true);
	ui.radioButton_UseOpenCL->setDisabled(true);

	ui.pushButton_DoRecon->setDisabled(true);

	m_iTmpIdx = 60;

	m_fProjImgValueMax = 0.0; //value of float image
	m_fProjImgValueMin = 0.0;

	m_arrYKBufProj = NULL;
	m_iCntSelectedProj = 0;

	connect(ui.labelImageRaw, SIGNAL(Mouse_Move()), this, SLOT(SLT_DataProbeProj()));
	connect(ui.labelImageRaw, SIGNAL(Mouse_Pressed_Left()), this, SLOT(SLT_CalculateROI_Proj())); //added

	connect(ui.labelReconImage, SIGNAL(Mouse_Move()), this, SLOT(SLT_DataProbeRecon()));
	connect(ui.labelReconImage, SIGNAL(Mouse_Pressed_Left()), this, SLOT(SLT_CalculateROI_Recon()));


	//Mouse_Left

	//connect(ui.MyLabel, SIGNAL(Mouse_Pos()), this, SLOT(MouseCurrPos()));


#if OPENCL_FOUND
	ui.radioButton_UseOpenCL->setDisabled(false);
	ui.radioButton_UseCUDA->setChecked(true);
#endif

#if CUDA_FOUND
	ui.radioButton_UseCUDA->setDisabled(false);
	ui.radioButton_UseCUDA->setChecked(true);
#endif

	m_pTableModel = NULL;
	m_pDlgRegistration = new DlgRegistration(this);
	m_pDlgExternalCommand = new DlgExternalCommand(this);


	m_iFixedOffset_ScatterMap = 10000;//fixed! allows negative value of scatter
	//m_iFixedOffset_ScatterMap = 0;//fixed! allows negative value of scatter // HEY AGRAVGAARD TODO HERE!!
	m_fResampleF = 1.0;
	m_fProjSpacingX = 0.4; //DEFAULT, will be updated during Load Proj selected
	m_fProjSpacingY = 0.4;


	//20141017 QTIMER for sync
	m_Timer = new QTimer(this);
	connect(m_Timer, SIGNAL(timeout()), this, SLOT(SLT_TimerEvent()));
	m_busyTimer = false;

	m_strPathDirDefault = QDir::currentPath();
	cout << "Current Default Dir: " << m_strPathDirDefault.toLocal8Bit().constData() << endl;

	init_DlgRegistration(QString("tmp"));//to Setup plastimatch folder. this is useful if registration will be only done

	//shell test
	//QString strCurFolder = "H:\\lib\\rtk\\NightlyBUILD64\\bin\\Release";
	//QString strCommand = QString("explorer %1").arg(strCurFolder);
	//cout << strCommand.toLocal8Bit().constData() << endl;
	//::system(strCommand.toLocal8Bit().constData());
	//::system("rtkfdk");

//	if (QProcess::execute(QString("H:\\lib\\rtk\\NightlyBUILD64\\bin\\Release\\rtkfdk")) < 0)
	//	qDebug() << "Failed to run";

	//m_strPathDirDefault = "D:\\Program_data\\01_20140827_CBCT_All\\04_patient_phan_pelvis_M\\IMAGES\\img_1.3.46.423632.135786.1409186054.9_M20mAs6440";
	//m_strPathDirDefault = "C:\\Users\\PC\\Documents\\Alderson_phantom\\k\\Raw projections";
	m_strPathDirDefault = "C:\\Users\\PC\\Documents\\Scripps_CBCT\\CBCT_Scatter_Project\\CBCT\\RawData\\CatPhan";

        m_bMacroContinue = true;

}

CbctRecon::~CbctRecon()
{
	ReleaseMemory();
	delete m_dspYKReconImage;
	delete m_dspYKImgProj;
    delete m_pDlgRegistration;
	delete m_Timer;
	delete m_pDlgExternalCommand;
}

void CbctRecon::ReleaseMemory()
{
	if (m_arrYKImage != NULL)
	{
		delete [] m_arrYKImage;
		m_arrYKImage = NULL;
		//m_iImgCnt = 0;
	}

	if (m_pTableModel != NULL)
	{
		delete m_pTableModel;
		m_pTableModel = NULL;
	}

	if (m_arrYKBufProj != NULL)
	{
	  delete [] m_arrYKBufProj;
	  m_arrYKBufProj = NULL;
	  m_iCntSelectedProj = 0;
	}
}

void CbctRecon::SLT_LoadRawImages(){
	if (ximIsUsed)
		SLT_LoadRawXimImages();
	else
		SLT_LoadRawHndImages();
}

//Function for independent projection his images
void CbctRecon::SLT_LoadRawHndImages()
{
	//QStringList files = QFileDialog::getOpenFileNames(this,"Select one or more files to open",
	//	"/home","projection images (*.his)");

	QStringList files = QFileDialog::getOpenFileNames(this,"Select one or more files to open",
		m_strPathDirDefault,"projection images (*.hnd)"); // ELEKTA vs VARIAN

	m_iImgCnt = files.size();

	if (m_iImgCnt < 1)
		return;

	ReleaseMemory();

	m_arrYKImage = new YK16GrayImage [m_iImgCnt];


	typedef rtk::HndImageIO readerType;
	readerType::Pointer reader = readerType::New();

	for (int i = 0; i < m_iImgCnt; ++i)
	{
		QString strFile = files.at(i);

		reader->SetFileName(strFile.toLocal8Bit().constData());
		reader->ReadImageInformation();

		int width = reader->GetDimensions(0); // width
		int height = reader->GetDimensions(1); // height
		int sizePix = reader->GetImageSizeInPixels();
		int sizeBuf = reader->GetImageSizeInBytes();
		int bytesPerPix = qRound(sizeBuf / (double)sizePix);

		if (bytesPerPix != 2)
			break;

		m_arrYKImage[i].CreateImage(width, height, 0);

		reader->Read(m_arrYKImage[i].m_pData);

		m_arrYKImage[i].m_strFilePath = strFile;
		//Copy his header - 100 bytes
		m_arrYKImage[i].CopyHisHeader(strFile.toLocal8Bit().constData());
	}

	m_multiplyFactor = 1.0;

	ui.spinBoxImgIdx->setMinimum(0);
	ui.spinBoxImgIdx->setMaximum(m_iImgCnt-1);
	ui.spinBoxImgIdx->setValue(0);

	SetMaxAndMinValueOfProjectionImage();
	SLT_InitializeGraphLim();

	SLT_DrawRawImages(); //Change FileName as well.. read spinbox value and draw image
}

//Function for independent projection his images
void CbctRecon::SLT_LoadRawXimImages()
{
	//QStringList files = QFileDialog::getOpenFileNames(this,"Select one or more files to open",
	//	"/home","projection images (*.his)");

	QStringList files = QFileDialog::getOpenFileNames(this, "Select one or more files to open",
		m_strPathDirDefault, "projection images (*.xim)"); // ELEKTA vs VARIAN

	m_iImgCnt = files.size();

	if (m_iImgCnt < 1)
		return;

	ReleaseMemory();

	m_arrYKImage = new YK16GrayImage[m_iImgCnt];


	typedef rtk::XimImageIO readerType;
	readerType::Pointer reader = readerType::New();

	for (int i = 0; i < m_iImgCnt; ++i)
	{
		QString strFile = files.at(i);

		reader->SetFileName(strFile.toLocal8Bit().constData());
		reader->ReadImageInformation();

		int width = reader->GetDimensions(0); // width
		int height = reader->GetDimensions(1); // height
		int sizePix = reader->GetImageSizeInPixels();
		int sizeBuf = reader->GetImageSizeInBytes();
		int bytesPerPix = qRound(sizeBuf / (double)sizePix);

		if (bytesPerPix != 2)
			break;

		m_arrYKImage[i].CreateImage(width, height, 0);

		reader->Read(m_arrYKImage[i].m_pData);

		m_arrYKImage[i].m_strFilePath = strFile;
		//Copy his header - 100 bytes
		m_arrYKImage[i].CopyHisHeader(strFile.toLocal8Bit().constData());
	}

	m_multiplyFactor = 1.0;

	ui.spinBoxImgIdx->setMinimum(0);
	ui.spinBoxImgIdx->setMaximum(m_iImgCnt - 1);
	ui.spinBoxImgIdx->setValue(0);

	SetMaxAndMinValueOfProjectionImage();
	SLT_InitializeGraphLim();

	SLT_DrawRawImages(); //Change FileName as well.. read spinbox value and draw image
}

//Hexa name ->decimal name

void CbctRecon::RenameFromHexToDecimal(QStringList& filenameList)
{
	int size = filenameList.size();

	QString crntFilePath;

	for (int i = 0 ; i<size ; i++)
	{
		crntFilePath = filenameList.at(i);
		QFileInfo fileInfo = QFileInfo(crntFilePath);
		QDir dir = fileInfo.absoluteDir();

		QString newBaseName = HexStr2IntStr(fileInfo.baseName());
		QString extStr = fileInfo.completeSuffix();

		QString newFileName = newBaseName.append(".").append(extStr);
		QString newPath = dir.absolutePath() + "/" + newFileName;

		//extract former part
		QFile::rename(crntFilePath, newPath);
	}
	//Extract

}
void CbctRecon::SLT_LoadImageFloat3D() //Dose image for JPhillips
{
	typedef itk::ImageFileReader<OutputImageType> ReaderType;
	ReaderType::Pointer reader = ReaderType::New();

	QString fileName = QFileDialog::getOpenFileName(this, "Open Image",m_strPathDirDefault, "3D dose float file (*.mha)",0,0);

	if (fileName.length() < 1)
		return;

	reader->SetFileName(fileName.toLocal8Bit().constData());
	reader->Update();


	//YK TEMP 20140507 //
	//Make a mask
	//Get information to create a mask image that has different resolution
	//double spacing_mm = 0.5; //used for x,y,z alike
	//QInputDialog inputDlg;

	//bool ok;
	//QString text = QInputDialog::getText(this, "Input Dialog","mask resolution in mm, same for all 3D", QLineEdit::Normal, "mask resolution", &ok);

	//if (ok && !text.isEmpty())
	//	spacing_mm = text.toLocal8Bit().toDouble();
	////typedef itk::Image< float, 3 > ImageType;

	//OutputImageType::Pointer img1 = OutputImageType::New();
	//
	////start index: What is the index of Left Top Inferior corner in DICOM coordinate?
	//
	//OutputImageType::SizeType oldSize = reader->GetOutput()->GetBufferedRegion().GetSize();
	//OutputImageType::SpacingType oldSpacing = reader->GetOutput()->GetSpacing();

	//OutputImageType::SizeType size1;
	//size1[0] = (double)oldSize[0]*(double)oldSpacing[0] / spacing_mm; // X --> determines sagittal slice num
	//size1[1] = (double)oldSize[1]*(double)oldSpacing[1] / spacing_mm;
	//size1[2] = (double)oldSize[2]*(double)oldSpacing[2] / spacing_mm;

	////cout << "mask size = " << size1 << endl;

	//OutputImageType::IndexType idxStart; //always should be 0 -->want to shift center? Use Origin
	//idxStart[0] = 0;
	//idxStart[1] = 0;
	//idxStart[2] = 0;

	//OutputImageType::SpacingType spacing1;
	//spacing1[0] = spacing_mm;
	//spacing1[1] = spacing_mm;
	//spacing1[2] = spacing_mm;

	////cout << "mask spacing = " << spacing1 << endl;

	//OutputImageType::PointType origin1; //Top Left Inferior most position

//Unit of Origin: mm
	//origin1 = reader->GetOutput()->GetOrigin();
	//
	//OutputImageType::RegionType region1;
	//region1.SetSize(size1);
	//region1.SetIndex(idxStart);
	//
	//img1->SetRegions(region1);
	//img1->SetSpacing(spacing1);
	//img1->SetOrigin(origin1);

	//img1->Allocate();
	//img1->FillBuffer(0);

	 //typedef itk::ImageFileWriter<OutputImageType> WriterType;
	 //WriterType::Pointer writer = WriterType::New();
	 //writer->SetFileName("E:\\floatImgMask.mha");
	 //writer->SetUseCompression(true);
	 //writer->SetInput(img1);
	 //
	 //writer->Update();
	 ////YK TEMP 20140507 END//


	//Multiply: Gy to mGy
	typedef itk::MultiplyImageFilter<OutputImageType, OutputImageType, OutputImageType> MultiplyImageFilterType;
	MultiplyImageFilterType::Pointer multiplyImageFilter = MultiplyImageFilterType::New();
	multiplyImageFilter->SetInput(reader->GetOutput());
	multiplyImageFilter->SetConstant(100.0); //calculated already //Gy to cGy

	//typedef unsigned short FinalPixelType;
	//typedef itk::Image< FinalPixelType, 3 > FinalImageType;

	typedef itk::CastImageFilter< OutputImageType, USHORT_ImageType > CastFilterType;
	CastFilterType::Pointer castFilter = CastFilterType::New();
	castFilter->SetInput(multiplyImageFilter->GetOutput());

	castFilter->Update();

	m_spRawReconImg = castFilter->GetOutput();
	m_spCrntReconImg = m_spRawReconImg;

	//Update UI
	USHORT_ImageType::SizeType imgDim = m_spRawReconImg->GetBufferedRegion().GetSize();
	USHORT_ImageType::SpacingType spacing = m_spRawReconImg->GetSpacing();

	cout << "Image Dimension:	" << imgDim[0] << "	" << imgDim[1] << "	" << imgDim[2] << endl;
	cout << "Image Spacing (mm):	" << spacing[0] << "	" << spacing[1] << "	" << spacing[2] << endl;

	ui.lineEdit_Cur3DFileName->setText(fileName);

	m_dspYKReconImage->CreateImage(imgDim[0], imgDim[1],0);

	ui.spinBoxReconImgSliceNo->setMinimum(0);
	ui.spinBoxReconImgSliceNo->setMaximum(imgDim[2]-1);
	int initVal = qRound((imgDim[2]-1)/2.0);

	SLT_InitializeGraphLim();
	ui.spinBoxReconImgSliceNo->setValue(initVal); //DrawRecon Imge is called, but sometimes skipped
	ui.radioButton_graph_recon->setChecked(true);

	/*QString strMsg = "Do you want to create a ?";
	QMessageBox msgBox;
	msgBox.setText(strMsg);
	msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);

	int res = msgBox.exec();
	if (res == QMessageBox::Yes)
	RenameFromHexToDecimal(files);*/

	SLT_DrawReconImage();
}

void CbctRecon::SLT_Load3DImage() // mha reconstructed file, from external source
{
	//typedef itk::ImageFileWriter<OutputImageType> WriterType;
	typedef itk::ImageFileReader<USHORT_ImageType> ReaderType;
	ReaderType::Pointer reader = ReaderType::New();

	QString fileName = QFileDialog::getOpenFileName(this, "Open Image",m_strPathDirDefault, "Projection file (*.mha)",0,0);

	if (fileName.length() < 1)
		return;

	reader->SetFileName(fileName.toLocal8Bit().constData());
	reader->Update();

	m_spRawReconImg = reader->GetOutput();
	m_spCrntReconImg = m_spRawReconImg;


	/*UpdateUIAfterLoading(fileName);
	return;*/
	//Update UIAFter Loading

	USHORT_ImageType::SizeType imgDim = m_spRawReconImg->GetBufferedRegion().GetSize();
	USHORT_ImageType::SpacingType spacing = m_spRawReconImg->GetSpacing();

	cout << "Image Dimension:	" << imgDim[0] << "	" << imgDim[1] << "	" << imgDim[2] << endl;
	cout << "Image Spacing (mm):	" << spacing[0] << "	" << spacing[1] << "	" << spacing[2] << endl;

	ui.lineEdit_Cur3DFileName->setText(fileName);

	m_dspYKReconImage->CreateImage(imgDim[0], imgDim[1],0);

	ui.spinBoxReconImgSliceNo->setMinimum(0);
	ui.spinBoxReconImgSliceNo->setMaximum(imgDim[2]-1);
	int initVal = qRound((imgDim[2]-1)/2.0);

	SLT_InitializeGraphLim();
	ui.spinBoxReconImgSliceNo->setValue(initVal); //DrawRecon Imge is called, but sometimes skipped
	ui.radioButton_graph_recon->setChecked(true);

	SLT_DrawReconImage();
}

void CbctRecon::SLT_Load3DImageShort()
{
  QString fileName = QFileDialog::getOpenFileName(this, "Open Image",m_strPathDirDefault, "short mha file (*.mha)",0,0);

  if (fileName.length() < 1)
	return;

  if (!LoadShortImageToUshort(fileName, m_spRawReconImg))
  {
	cout << "error! in LoadShortImageToUshort" << endl;
  }

  typedef itk::MinimumMaximumImageCalculator <USHORT_ImageType>
	ImageCalculatorFilterType2;

  ImageCalculatorFilterType2::Pointer imageCalculatorFilter2
	= ImageCalculatorFilterType2::New ();
  //imageCalculatorFilter2->SetImage(m_spReconImg);
  imageCalculatorFilter2->SetImage(m_spRawReconImg);
  imageCalculatorFilter2->Compute();

  double minVal2 = (double)(imageCalculatorFilter2->GetMinimum());
  double maxVal2 = (double)(imageCalculatorFilter2->GetMaximum());
  cout <<"Min and Max Values are	" << minVal2 << "	" << maxVal2 << endl;

  //Update UI
  m_spCrntReconImg = m_spRawReconImg;

  USHORT_ImageType::SizeType imgDim = m_spCrntReconImg->GetBufferedRegion().GetSize();
  USHORT_ImageType::SpacingType spacing = m_spCrntReconImg->GetSpacing();

  cout << "Image Dimension:	" << imgDim[0] << "	" << imgDim[1] << "	" << imgDim[2] << endl;
  cout << "Image Spacing (mm):	" << spacing[0] << "	" << spacing[1] << "	" << spacing[2] << endl;

  ui.lineEdit_Cur3DFileName->setText(fileName);

  m_dspYKReconImage->CreateImage(imgDim[0], imgDim[1],0);

  ui.spinBoxReconImgSliceNo->setMinimum(0);
  ui.spinBoxReconImgSliceNo->setMaximum(imgDim[2]-1);
  int initVal = qRound((imgDim[2]-1)/2.0);

  SLT_InitializeGraphLim();
  ui.spinBoxReconImgSliceNo->setValue(initVal); //DrawRecon Imge is called, but sometimes skipped
  ui.radioButton_graph_recon->setChecked(true);

  SLT_DrawReconImage();
}

void CbctRecon::SLT_LoadNKIImage()
{
	QString filePath = QFileDialog::getOpenFileName(this, "Open Image", m_strPathDirDefault, "NKI file (*.SCAN)", 0, 0);

	if (filePath.length() < 1)
		return;

	Volume *v = nki_load(filePath.toLocal8Bit().constData()); //NKI is unsigned short!!!
	if (!v)
	{
		printf("file reading error\n");
		return;
	}

	QString endFix = "_conv";
	QFileInfo srcFileInfo = QFileInfo(filePath);
	QDir dir = srcFileInfo.absoluteDir();
	QString baseName = srcFileInfo.completeBaseName();
	QString extName = "mha";

	QString newFileName = baseName.append(endFix).append(".").append(extName);
	QString newPath = dir.absolutePath() + "/" + newFileName;

	write_mha(newPath.toLocal8Bit().constData(), v);
	cout << "File conversion is done. Trying to read mha file.." << endl;
	//corrImg.ReleaseBuffer();
	//NKI to mha


	if (!LoadShortImageToUshort(newPath, m_spRawReconImg))
	{
		cout << "error! in LoadShortImageToUshort" << endl;
	}

	typedef itk::MinimumMaximumImageCalculator <USHORT_ImageType>
		ImageCalculatorFilterType2;

	ImageCalculatorFilterType2::Pointer imageCalculatorFilter2
		= ImageCalculatorFilterType2::New();
	//imageCalculatorFilter2->SetImage(m_spReconImg);
	imageCalculatorFilter2->SetImage(m_spRawReconImg);
	imageCalculatorFilter2->Compute();

	double minVal2 = (double)(imageCalculatorFilter2->GetMinimum());
	double maxVal2 = (double)(imageCalculatorFilter2->GetMaximum());
	cout << "Min and Max Values are	" << minVal2 << "	" << maxVal2 << endl;

	//Update UI
	m_spCrntReconImg = m_spRawReconImg;

	USHORT_ImageType::SizeType imgDim = m_spCrntReconImg->GetBufferedRegion().GetSize();
	USHORT_ImageType::SpacingType spacing = m_spCrntReconImg->GetSpacing();

	cout << "Image Dimension:	" << imgDim[0] << "	" << imgDim[1] << "	" << imgDim[2] << endl;
	cout << "Image Spacing (mm):	" << spacing[0] << "	" << spacing[1] << "	" << spacing[2] << endl;

	ui.lineEdit_Cur3DFileName->setText(newFileName);

	m_dspYKReconImage->CreateImage(imgDim[0], imgDim[1], 0);

	ui.spinBoxReconImgSliceNo->setMinimum(0);
	ui.spinBoxReconImgSliceNo->setMaximum(imgDim[2] - 1);
	int initVal = qRound((imgDim[2] - 1) / 2.0);

	SLT_InitializeGraphLim();
	ui.spinBoxReconImgSliceNo->setValue(initVal); //DrawRecon Imge is called, but sometimes skipped
	ui.radioButton_graph_recon->setChecked(true);

	SLT_DrawReconImage();


}


void CbctRecon::SLT_DrawRawImages()
{
	int crntIdx = ui.spinBoxImgIdx->value();

	if (crntIdx >= m_iImgCnt)
		return;

	int windowMin = ui.sliderRawMin->value();
	int windowMax = ui.sliderRawMax->value();

	QFileInfo tmpInfo = QFileInfo(m_arrYKImage[crntIdx].m_strFilePath);
	ui.lineEditFileName->setText(tmpInfo.fileName());


	int width = m_arrYKImage[crntIdx].m_iWidth;
	int height = m_arrYKImage[crntIdx].m_iHeight;
	m_dspYKImgProj->CreateImage(width, height, 0);
	m_dspYKImgProj->CopyFromBuffer(m_arrYKImage[crntIdx].m_pData, width, height);

	m_dspYKImgProj->FillPixMapMinMax(windowMin, windowMax);
	ui.labelImageRaw->SetBaseImage(m_dspYKImgProj);
	ui.labelImageRaw->update();
}


void CbctRecon::SLT_DrawProjImages()
{
	if (m_dspYKImgProj == NULL)
		return;


	if (m_iImgCnt > 0)
	{
		SLT_DrawRawImages();
//		SLT_DrawGraph();
		SLT_UpdateTable();
		return;
	}
	//Using slice iterator,
	//1) Find the slice requested
	//2) get dimension to create 2DYK16Image
	//3) copy slice region to YK16 Image --> Cating: float to USHORT

	if (!m_spProjImg3DFloat)
		return;

	itk::ImageSliceConstIteratorWithIndex<OutputImageType> it (m_spProjImg3DFloat, m_spProjImg3DFloat->GetRequestedRegion());

	OutputImageType::SizeType imgSize = m_spProjImg3DFloat->GetRequestedRegion().GetSize(); //1016x1016 x z
	OutputImageType::SizeType imgSizeBuf = m_spProjImg3DFloat->GetBufferedRegion().GetSize(); //1016x1016 x z
	OutputImageType::SizeType imgSizeLargest = m_spProjImg3DFloat->GetLargestPossibleRegion().GetSize(); //1016x1016 x z

	int width = imgSize[0];
	int height = imgSize[1];
	m_dspYKImgProj->CreateImage(width, height, 0);

	//cout << "img Size_Requested: " << imgSize[0] << "	" << imgSize[1] <<"	" << imgSize[2] << endl; //same
	//cout << "img Size_Buffered: " << imgSizeBuf[0] << "	" << imgSizeBuf[1] <<"	" << imgSizeBuf[2] << endl; //same
	//cout << "img Size_LargestPossible: " << imgSizeLargest[0] << "	" << imgSizeLargest[1] <<"	" << imgSizeLargest[2] << endl; //same
	//-->this means that projection Reader modifies the original image a little bit (1024 --> 1016)

	it.SetFirstDirection(0); //x?
	it.SetSecondDirection(1); //y?
	it.GoToBegin();

	int iNumSlice = 0;
	int iNumWidth = 0;
	int iNumHeight = 0;

	double realValGap = m_fProjImgValueMax - m_fProjImgValueMin;
	m_multiplyFactor = 0.0;

	if (realValGap > 0.0)
	{
		m_multiplyFactor = 65535.0 / realValGap;
	}


	int iReqSlice = ui.spinBoxImgIdx->value();

	while( !it.IsAtEnd() )
	{
		if (iNumSlice == iReqSlice)
		{
			iNumHeight = 0;
			while( !it.IsAtEndOfSlice() )
			{
				iNumWidth = 0;
				while( !it.IsAtEndOfLine() )
				{
					//double tmpVal = it.Get()*multiplyFactor;
					double tmpVal = it.Get();


					m_dspYKImgProj->m_pData[iNumWidth + width*iNumHeight] = (unsigned short)((tmpVal- m_fProjImgValueMin)*m_multiplyFactor);
					// it.Set() doesn't exist in the Const Iterator
					++it;
					iNumWidth++;
				}
				it.NextLine();
				iNumHeight++;
			}
			//break;
		}
		it.NextSlice();
		iNumSlice++;
	}

	m_dspYKImgProj->FillPixMapMinMax(ui.sliderRawMin->value(), ui.sliderRawMax->value());

	ui.labelImageRaw->SetBaseImage(m_dspYKImgProj);
	ui.labelImageRaw->update();

	SLT_UpdateTable();
}
//
//void CbctRecon::SLT_Draw3DImage()
//{
//	//if (m_pImageYKGain->IsEmpty())
//	//	return;
//
//	//m_pImageYKGain->FillPixMapMinMax(ui.sliderGainMin->value(), ui.sliderGainMax->value());
//	////m_pImageYKGain->DrawToLabel(ui.labelImageGain);
//
//	//ui.labelImageGain->SetBaseImage(m_pImageYKGain);
//	//ui.labelImageGain->update();
//}
QString CbctRecon::HexStr2IntStr(QString& strHex)
{
	int len = strHex.length();

	int tmpDecimal = 0;
	int cnt = 0;
	int i = 0;

	for (i = len-1 ;i >= 0 ; i--)
	{
		int tmpNum = 0;

		if (strHex.at(i) == 'a' || strHex.at(i) == 'A')
			tmpNum = 10;
		else if (strHex.at(i) == 'b' || strHex.at(i) == 'B')
			tmpNum = 11;
		else if (strHex.at(i) == 'c' || strHex.at(i) == 'C')
			tmpNum = 12;
		else if (strHex.at(i) == 'd' || strHex.at(i) == 'D')
			tmpNum = 13;
		else if (strHex.at(i) == 'e' || strHex.at(i) == 'E')
			tmpNum = 14;
		else if (strHex.at(i) == 'f' || strHex.at(i) == 'F')
			tmpNum = 15;

		else
		{
			QString tmpStr;
			tmpStr = strHex.at(i);
			tmpNum = tmpStr.toInt();
		}
		tmpDecimal = tmpDecimal + tmpNum*(int)pow(16.0, len-1-i);
	}

	QString intStr = QString("%1").arg(tmpDecimal);

	return intStr;
	//return tmpDecimal;
	//m_str_10.Format("%d", tmpDecimal);
}


void CbctRecon::SLT_FileNameHex2Dec()
{
	QStringList files = QFileDialog::getOpenFileNames(this,"Select one or more files to open",
		m_strPathDirDefault,"projection images (*.his)");

	int cnt = files.size();
	if (cnt <= 0 )
		return;

	QString strMsg = "Original file names will be gone. Backup is strongly recommended. Continue?";

	QMessageBox msgBox;
	msgBox.setText(strMsg);
	msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);

	int res = msgBox.exec();

	if (res == QMessageBox::Yes)
		RenameFromHexToDecimal(files);
}

void CbctRecon::SLT_MakeVarianXML() // ELEKTA VS VARIAN
{
	//Define IMAGE.DBF path
	QString filePath_ImageXML = QFileDialog::getOpenFileName(this, "Select IMAGE.XML file", m_strPathDirDefault, "Varian Obi file (*.xml)", 0,0); // ELEKTA VS VARIAN

	if (filePath_ImageXML.length() < 2)
		return;

//	QString filePath_FrameDBF = QFileDialog::getOpenFileName(this, "Select FRAME.DBF file", m_strPathDirDefault, "Varian DB file (*.dbf)", 0,0); // ELEKTA VS VARIAN

//	if (filePath_FrameDBF.length() < 2)
//		return;

	QString DICOM_UID;
	QString XMLFileName;
	QInputDialog inputDlg;

	bool ok;
	QString text = QInputDialog::getText(this, "Input Dialog","DICOM_UID:", QLineEdit::Normal, "DICOM_UID", &ok); //Needed?
	/*However   void SetProjectionsFileNames (const FileNamesContainer &name) might be needed instead*/
	if (ok && !text.isEmpty())
		DICOM_UID = text;

	if (DICOM_UID.length() < 2)
		return;

	QString genFilePath = MakeVarianXML (filePath_ImageXML, DICOM_UID); // ELEKTA VS VARIAN
	cout << "Generated VarianXML path: " << genFilePath.toLocal8Bit().constData() << endl; // ELEKTA VS VARIAN

	return;
}

QString CbctRecon::MakeVarianXML(QString filePath_ImageXML, QString DICOM_UID) // ELEKTA VS VARIAN
{
  cout << "Varian geometry XML file is being generated." << endl; // ELEKTA VS VARIAN
  //Define FRAME.DBF path
  rtk::VarianObiGeometryReader::Pointer reader = rtk::VarianObiGeometryReader::New(); // ELEKTA VS VARIAN
  //string strDicomUID = DICOM_UID.toLocal8Bit().constData(); //DICOM_UID.toStdString()
  //string strDicomUID = DICOM_UID.toStdString();
  //string strDbfImg = filePath_ImageDBF.toStdString();
  //string strDbfFrame = filePath_FrameDBF.toStdString();

  QFileInfo info = QFileInfo(filePath_ImageXML);
  QString dirPath = info.absolutePath();

  QString fileName = "VarianGeom_" + DICOM_UID + ".xml"; // ELEKTA VS VARIAN

  QString strOutput = dirPath + "/" + fileName;

//  reader->SetDicomUID(DICOM_UID.toLocal8Bit().constData());
//  reader->SetImageDbfFileName(filePath_ImageDBF.toLocal8Bit().constData());
//  reader->SetFrameDbfFileName(filePath_FrameDBF.toLocal8Bit().constData());
  reader->SetXMLFileName(filePath_ImageXML.toLocal8Bit().constData());

  TRY_AND_EXIT_ON_ITK_EXCEPTION( reader->UpdateOutputData() );

  // Write
  rtk::ThreeDCircularProjectionGeometryXMLFileWriter::Pointer xmlWriter =
	rtk::ThreeDCircularProjectionGeometryXMLFileWriter::New();
  xmlWriter->SetFilename(strOutput.toLocal8Bit().constData());
  xmlWriter->SetObject( reader->GetGeometry() );
  TRY_AND_EXIT_ON_ITK_EXCEPTION( xmlWriter->WriteFile() )

  cout << "Reading succeed" << endl;

  return strOutput;
}


void CbctRecon::SLT_ExportHis()
{
	if (m_iImgCnt < 1)
	{
		cout << "Error: Load raw his images first" << endl;
		return;
	}

	//Get Folder Name!

	//For displaying Dir only..
	QString dir = QFileDialog::getExistingDirectory(this, "Open Directory",
		"/home", QFileDialog::ShowDirsOnly
		| QFileDialog::DontResolveSymlinks);

	//FileName should be same, only selected folder

	for (int i = 0 ;i<m_iImgCnt ; i++)
	{
		QFileInfo tmpInfo = QFileInfo(m_arrYKImage[i].m_strFilePath);
		QString newPath = dir + "/" + tmpInfo.fileName();
		m_arrYKImage[i].SaveDataAsHis(newPath.toLocal8Bit().constData(), false);
	}

	cout << "File export was done successfully" << endl;
}



void CbctRecon::SLT_OpenOffsetFile()
{
	//QString strPath = QFileDialog::getOpenFileNames(this,"Select one or more files to open","/home","Images (*.raw)");
	QString strPath = QFileDialog::getOpenFileName(this,"Select a single file to open",m_strPathDirDefault,"raw image (*.raw)");

	if (strPath.length()<=1)
		return;

	ui.lineEdit_offsetPath->setText(strPath);
	m_pImgOffset->LoadRawImage(strPath.toLocal8Bit().constData(),DEFAULT_VARIAN_PROJ_WIDTH, DEFAULT_VARIAN_PROJ_HEIGHT);

}

void CbctRecon::SLT_OpenGainFile()
{
	QString strPath = QFileDialog::getOpenFileName(this,"Select a single file to open",m_strPathDirDefault,"raw image (*.raw)");

	if (strPath.length()<=1)
		return;

	ui.lineEdit_gainPath->setText(strPath);
	m_pImgGain->LoadRawImage(strPath.toLocal8Bit().constData(),DEFAULT_VARIAN_PROJ_WIDTH, DEFAULT_VARIAN_PROJ_HEIGHT);
}

void CbctRecon::SLT_OpenBadpixelFile()
{
	QString strPath = QFileDialog::getOpenFileName(this,"Select a single file to open",m_strPathDirDefault,"bad pixel map file (*.pmf)");

	if (strPath.length()<=1)
		return;

	ui.lineEdit_badpixelPath->setText(strPath);
	LoadBadPixelMap(strPath.toLocal8Bit().constData());
	//m_pImgGain->LoadRawImage(strPath.toLocal8Bit(),IMG_WIDTH, IMG_HEIGHT);
}

QString CbctRecon::CorrectSingleFile(const char* filePath)
{
	//Load raw file
	YK16GrayImage rawImg(DEFAULT_VARIAN_PROJ_WIDTH, DEFAULT_VARIAN_PROJ_HEIGHT);
	rawImg.LoadRawImage(filePath, DEFAULT_VARIAN_PROJ_WIDTH, DEFAULT_VARIAN_PROJ_HEIGHT);

	YK16GrayImage corrImg(DEFAULT_VARIAN_PROJ_WIDTH, DEFAULT_VARIAN_PROJ_HEIGHT);

	bool bDarkCorrApply = ui.checkBox_offsetOn->isChecked();
	bool bGainCorrApply = ui.checkBox_gainOn->isChecked();
	bool bDefectMapApply = ui.checkBox_badpixelOn->isChecked();

	//m_pParent->m_pCurrImageRaw->m_pData[i]

	if (!bDarkCorrApply && !bGainCorrApply)
	{
		for (int i = 0; i < DEFAULT_VARIAN_PROJ_WIDTH * DEFAULT_VARIAN_PROJ_HEIGHT; i++)
		{
			corrImg.m_pData[i] = rawImg.m_pData[i]; //raw image
		}
	}
	else if (bDarkCorrApply && !bGainCorrApply)
	{
		if (m_pImgOffset->IsEmpty())
		{
			for (int i = 0; i < DEFAULT_VARIAN_PROJ_WIDTH * DEFAULT_VARIAN_PROJ_HEIGHT; i++)
			{
				corrImg.m_pData[i] = rawImg.m_pData[i]; //raw image
			}
		}
		else
		{
			for (int i = 0; i < DEFAULT_VARIAN_PROJ_WIDTH * DEFAULT_VARIAN_PROJ_HEIGHT; i++) {
				if (rawImg.m_pData[i] > m_pImgOffset->m_pData[i])
					corrImg.m_pData[i] = rawImg.m_pData[i] - m_pImgOffset->m_pData[i];
				else
					corrImg.m_pData[i] = 0;
			}
		}
	}
	else if (!bDarkCorrApply && bGainCorrApply)
	{
		if (m_pImgGain->IsEmpty())
		{
			for (int i = 0; i < DEFAULT_VARIAN_PROJ_WIDTH * DEFAULT_VARIAN_PROJ_HEIGHT; i++) {
				corrImg.m_pData[i] = rawImg.m_pData[i]; //raw image
			}
		}
		else
		{
			//get a mean value for m_pGainImage
			double sum = 0.0;
			double MeanVal = 0.0;
			for (int i = 0; i < DEFAULT_VARIAN_PROJ_WIDTH * DEFAULT_VARIAN_PROJ_HEIGHT; i++) {
				sum = sum + m_pImgGain->m_pData[i];
			}
			MeanVal = sum/(double)(DEFAULT_VARIAN_PROJ_WIDTH*DEFAULT_VARIAN_PROJ_HEIGHT);

			for (int i = 0; i < DEFAULT_VARIAN_PROJ_WIDTH * DEFAULT_VARIAN_PROJ_HEIGHT; i++) {
				if (m_pImgGain->m_pData[i] == 0)
					corrImg.m_pData[i] = rawImg.m_pData[i];
				else
					corrImg.m_pData[i]  = (USHORT)((double)rawImg.m_pData[i]/(double)(m_pImgGain->m_pData[i])*MeanVal);
			}
		}

		/*double CalibF = 0.0;

		for (int i = 0; i < xSize * ySize; i++) {
			CalibF = m_pParent->m_dlgControl->lineEditSingleCalibFactor->text().toDouble();
			pImageCorr[i] = (unsigned short)(pImageCorr[i] * CalibF);
		}*/
	}

	else if (bDarkCorrApply && bGainCorrApply)
	{
		bool bRawImage = false;
		if (m_pImgOffset->IsEmpty())		{
			bRawImage = true;
		}
		if (m_pImgGain->IsEmpty())
		{
			bRawImage = true;
		}

		if (bRawImage)
		{
			for (int i = 0; i < DEFAULT_VARIAN_PROJ_WIDTH* DEFAULT_VARIAN_PROJ_HEIGHT; i++) {
				corrImg.m_pData[i] = rawImg.m_pData[i] ; //raw image
			}
		}
		else //if not raw image
		{
			//get a mean value for m_pGainImage
			double sum = 0.0;
			double MeanVal = 0.0;
			for (int i = 0; i < DEFAULT_VARIAN_PROJ_WIDTH *DEFAULT_VARIAN_PROJ_HEIGHT; i++) {
				sum = sum + (m_pImgGain->m_pData[i] - m_pImgOffset->m_pData[i]);
			}
			MeanVal = sum/(double)(DEFAULT_VARIAN_PROJ_WIDTH*DEFAULT_VARIAN_PROJ_HEIGHT);

			double denom = 0.0;
			int iDenomLessZero = 0;
			int iDenomLessZero_RawIsGreaterThanDark = 0;
			int iDenomLessZero_RawIsSmallerThanDark = 0;
			int iDenomOK_RawValueMinus = 0;
			int iValOutOfRange = 0;

			for (int i = 0; i < DEFAULT_VARIAN_PROJ_WIDTH * DEFAULT_VARIAN_PROJ_HEIGHT; i++)
			{
				denom = (double)(m_pImgGain->m_pData[i] - m_pImgOffset->m_pData[i]);

				if (denom <= 0)
				{
					iDenomLessZero++;

					if (rawImg.m_pData[i] > m_pImgOffset->m_pData[i])
					{
						corrImg.m_pData[i] = rawImg.m_pData[i] - m_pImgOffset->m_pData[i];
						iDenomLessZero_RawIsGreaterThanDark++;
					}
					else
					{
						corrImg.m_pData[i] = 0;
						iDenomLessZero_RawIsSmallerThanDark++;
					}
				}
				else
				{
					double tmpVal = 0.0;
					tmpVal = (rawImg.m_pData[i] - m_pImgOffset->m_pData[i]) / denom * MeanVal;

					if (tmpVal < 0)
					{
						corrImg.m_pData[i] = 0;
						iDenomOK_RawValueMinus++;
					}
					else
					{
						if (tmpVal > 65535) //16bit max value
							iValOutOfRange++;

						corrImg.m_pData[i] = (USHORT)tmpVal;
					}
				}
			}//end of for
		}//end if not bRawImage

		//Apply Single Calib. Factor
		/*double CalibF = 0.0;

		for (int i = 0; i < xSize * ySize; i++) {
			CalibF = m_pParent->m_dlgControl->lineEditSingleCalibFactor->text().toDouble();
			pImageCorr[i] = (unsigned short)(pImageCorr[i] * CalibF);
		}*/

	} // else if (m_bDarkCorrApply && m_bGainCorrApply)

	/*******************Customized Thresholding after Gain Corr	****************/
	//unsigned short customThreVal = ui.lineEdit_customThre->text().toDouble();  //13000
	//bool enableCustomThre = ui.checkBox_customThre->isChecked();

	/*if (enableCustomThre)
	{
		for (int i = 0; i < IMG_WIDTH* IMG_HEIGHT; i++)
		{
			if (corrImg.m_pData[i] >= customThreVal)
				corrImg.m_pData[i] = customThreVal;
		}
	}*/

	if (bDefectMapApply && !m_vPixelReplMap.empty()) //pixel replacement
	{
		BadPixReplacement(&corrImg);
	}

	//file naming & export
	//file end-fix

	//filePath
	//QString exportName = filePath;
	//corrImg.SaveDataAsRaw();
	QString endFix = "_CORR";

	QFileInfo srcFileInfo = QFileInfo(filePath);
	QDir dir = srcFileInfo.absoluteDir();
	QString baseName = srcFileInfo.baseName();
	QString extName = srcFileInfo.completeSuffix();

	QString newFileName = baseName.append(endFix).append(".").append(extName);
	QString newPath = dir.absolutePath() + "/" + newFileName;

	corrImg.SaveDataAsRaw(newPath.toLocal8Bit().constData());

	return newPath;
	//corrImg.ReleaseBuffer();
}

void CbctRecon::CorrectSingleFile( YK16GrayImage* pYKRawImg )
{
	if (pYKRawImg == NULL)
		return;

	//YK16GrayImage rawImg(IMG_WIDTH, IMG_HEIGHT);
	//rawImg.LoadRawImage(filePath, IMG_WIDTH, IMG_HEIGHT);

	YK16GrayImage corrImg(DEFAULT_VARIAN_PROJ_WIDTH, DEFAULT_VARIAN_PROJ_HEIGHT);

	bool bDarkCorrApply = ui.checkBox_offsetOn->isChecked();
	bool bGainCorrApply = ui.checkBox_gainOn->isChecked();
	bool bDefectMapApply = ui.checkBox_badpixelOn->isChecked();

	//m_pParent->m_pCurrImageRaw->m_pData[i]

	if (!bDarkCorrApply && !bGainCorrApply)
	{
		for (int i = 0; i < DEFAULT_VARIAN_PROJ_WIDTH * DEFAULT_VARIAN_PROJ_HEIGHT; i++)
		{
			corrImg.m_pData[i] = pYKRawImg->m_pData[i]; //raw image
		}
	}
	else if (bDarkCorrApply && !bGainCorrApply)
	{
		if (m_pImgOffset->IsEmpty())
		{
			for (int i = 0; i < DEFAULT_VARIAN_PROJ_WIDTH * DEFAULT_VARIAN_PROJ_HEIGHT; i++)
			{
				corrImg.m_pData[i] = pYKRawImg->m_pData[i]; //raw image
			}
		}
		else
		{
			for (int i = 0; i < DEFAULT_VARIAN_PROJ_WIDTH * DEFAULT_VARIAN_PROJ_HEIGHT; i++) {
				if (pYKRawImg->m_pData[i] > m_pImgOffset->m_pData[i])
					corrImg.m_pData[i] = pYKRawImg->m_pData[i] - m_pImgOffset->m_pData[i];
				else
					corrImg.m_pData[i] = 0;
			}
		}
	}
	else if (!bDarkCorrApply && bGainCorrApply)
	{
		if (m_pImgGain->IsEmpty())
		{
			for (int i = 0; i < DEFAULT_VARIAN_PROJ_WIDTH * DEFAULT_VARIAN_PROJ_HEIGHT; i++) {
				corrImg.m_pData[i] = pYKRawImg->m_pData[i]; //raw image
			}
		}
		else
		{
			//get a mean value for m_pGainImage
			double sum = 0.0;
			double MeanVal = 0.0;
			for (int i = 0; i < DEFAULT_VARIAN_PROJ_WIDTH * DEFAULT_VARIAN_PROJ_HEIGHT; i++) {
				sum = sum + m_pImgGain->m_pData[i];
			}
			MeanVal = sum/(double)(DEFAULT_VARIAN_PROJ_WIDTH*DEFAULT_VARIAN_PROJ_HEIGHT);

			for (int i = 0; i < DEFAULT_VARIAN_PROJ_WIDTH * DEFAULT_VARIAN_PROJ_HEIGHT; i++) {
				if (m_pImgGain->m_pData[i] == 0)
					corrImg.m_pData[i] = pYKRawImg->m_pData[i];
				else
					corrImg.m_pData[i]  = (USHORT)((double)pYKRawImg->m_pData[i]/(double)(m_pImgGain->m_pData[i])*MeanVal);
			}

		}

		/*double CalibF = 0.0;

		for (int i = 0; i < xSize * ySize; i++) {
			CalibF = m_pParent->m_dlgControl->lineEditSingleCalibFactor->text().toDouble();
			pImageCorr[i] = (unsigned short)(pImageCorr[i] * CalibF);
		}*/
	}

	else if (bDarkCorrApply && bGainCorrApply)
	{
		bool bRawImage = false;
		if (m_pImgOffset->IsEmpty())		{
			bRawImage = true;
		}
		if (m_pImgGain->IsEmpty())
		{
			bRawImage = true;
		}

		if (bRawImage)
		{
			for (int i = 0; i < DEFAULT_VARIAN_PROJ_WIDTH* DEFAULT_VARIAN_PROJ_HEIGHT; i++) {
				corrImg.m_pData[i] = pYKRawImg->m_pData[i] ; //raw image
			}
		}
		else //if not raw image
		{
			//get a mean value for m_pGainImage
			double sum = 0.0;
			double MeanVal = 0.0;
			for (int i = 0; i < DEFAULT_VARIAN_PROJ_WIDTH *DEFAULT_VARIAN_PROJ_HEIGHT; i++) {
				sum = sum + (m_pImgGain->m_pData[i] - m_pImgOffset->m_pData[i]);
			}
			MeanVal = sum/(double)(DEFAULT_VARIAN_PROJ_WIDTH*DEFAULT_VARIAN_PROJ_HEIGHT);

			double denom = 0.0;
			int iDenomLessZero = 0;
			int iDenomLessZero_RawIsGreaterThanDark = 0;
			int iDenomLessZero_RawIsSmallerThanDark = 0;
			int iDenomOK_RawValueMinus = 0;
			int iValOutOfRange = 0;

			for (int i = 0; i < DEFAULT_VARIAN_PROJ_WIDTH * DEFAULT_VARIAN_PROJ_HEIGHT; i++)
			{
				denom = (double)(m_pImgGain->m_pData[i] - m_pImgOffset->m_pData[i]);

				if (denom <= 0)
				{
					iDenomLessZero++;

					if (pYKRawImg->m_pData[i] > m_pImgOffset->m_pData[i])
					{
						corrImg.m_pData[i] = pYKRawImg->m_pData[i] - m_pImgOffset->m_pData[i];
						iDenomLessZero_RawIsGreaterThanDark++;
					}
					else
					{
						corrImg.m_pData[i] = 0;
						iDenomLessZero_RawIsSmallerThanDark++;
					}
				}
				else
				{
					double tmpVal = 0.0;
					tmpVal = (pYKRawImg->m_pData[i] - m_pImgOffset->m_pData[i]) / denom * MeanVal;

					if (tmpVal < 0)
					{
						corrImg.m_pData[i] = 0;
						iDenomOK_RawValueMinus++;
					}
					else
					{
						if (tmpVal > 65535) //16bit max value
							iValOutOfRange++;

						corrImg.m_pData[i] = (USHORT)tmpVal;
					}
				}
			}//end of for
		}//end if not bRawImage

		//Apply Single Calib. Factor
		/*double CalibF = 0.0;

		for (int i = 0; i < xSize * ySize; i++) {
			CalibF = m_pParent->m_dlgControl->lineEditSingleCalibFactor->text().toDouble();
			pImageCorr[i] = (unsigned short)(pImageCorr[i] * CalibF);
		}*/

	} // else if (m_bDarkCorrApply && m_bGainCorrApply)

	/*******************Customized Thresholding after Gain Corr	****************/
	//unsigned short customThreVal = ui.lineEdit_customThre->text().toDouble();  //13000
	//bool enableCustomThre = ui.checkBox_customThre->isChecked();

	/*if (enableCustomThre)
	{
		for (int i = 0; i < IMG_WIDTH* IMG_HEIGHT; i++)
		{
			if (corrImg.m_pData[i] >= customThreVal)
				corrImg.m_pData[i] = customThreVal;
		}
	}*/
	if (bDefectMapApply && !m_vPixelReplMap.empty()) //pixel replacement
	{
		BadPixReplacement(&corrImg);
	}

	//Replace old buffer with new one.
	pYKRawImg->CopyFromBuffer(corrImg.m_pData, corrImg.m_iWidth, corrImg.m_iHeight);

	//file naming & export
	//file end-fix

	//filePath
	//QString exportName = filePath;
	//corrImg.SaveDataAsRaw();
	//QString endFix = "_CORR";

	//QFileInfo srcFileInfo = QFileInfo(filePath);
	//QDir dir = srcFileInfo.absoluteDir();
	//QString baseName = srcFileInfo.baseName();
	//QString extName = srcFileInfo.completeSuffix();

	//QString newFileName = baseName.append(endFix).append(".").append(extName);
	//QString newPath = dir.absolutePath() + "/" + newFileName;

	//corrImg.SaveDataAsRaw(newPath.toLocal8Bit().constData());

	return;
}


void CbctRecon::LoadBadPixelMap(const char* filePath)
{
	m_vPixelReplMap.clear();

	ifstream fin;
	fin.open(filePath);

	if (fin.fail())
		return;

	char str[MAX_LINE_LENGTH];
	//memset(str, 0, MAX_LINE_LENGTH);

	while (!fin.eof())
	{
		memset(str, 0, MAX_LINE_LENGTH);
		fin.getline(str, MAX_LINE_LENGTH);
		QString tmpStr = QString(str);

		if (tmpStr.contains("#ORIGINAL_X"))
			break;
	}

	while (!fin.eof())
	{
		memset(str, 0, MAX_LINE_LENGTH);
		fin.getline(str, MAX_LINE_LENGTH);
		QString tmpStr = QString(str);

		QStringList strList = tmpStr.split("	");

		if (strList.size() == 4)
		{
			BADPIXELMAP tmpData;
			tmpData.BadPixX = strList.at(0).toInt();
			tmpData.BadPixY = strList.at(1).toInt();
			tmpData.ReplPixX = strList.at(2).toInt();
			tmpData.ReplPixY = strList.at(3).toInt();
			m_vPixelReplMap.push_back(tmpData);
		}
	}
	fin.close();
}

void CbctRecon::BadPixReplacement(YK16GrayImage* targetImg)
{
	if (m_vPixelReplMap.empty())
		return;

	int oriIdx, replIdx;

	vector<BADPIXELMAP>::iterator it;

	for (it = m_vPixelReplMap.begin() ; it != m_vPixelReplMap.end(); it++)
	{
		BADPIXELMAP tmpData= (*it);
		oriIdx = tmpData.BadPixY * DEFAULT_VARIAN_PROJ_WIDTH + tmpData.BadPixX;
		replIdx = tmpData.ReplPixY * DEFAULT_VARIAN_PROJ_WIDTH + tmpData.ReplPixX;
		targetImg->m_pData[oriIdx] = targetImg->m_pData[replIdx];
	}
}

void CbctRecon::SLT_ApplyCalibration()
{
	if (m_iImgCnt < 1)
		return;

	for (int i = 0 ; i<m_iImgCnt ; i++)
	{
		CorrectSingleFile(&m_arrYKImage[i]); //pixel value will be changed
	}
	//	for (int i = 0 ; i<listSize ; i++)
	//	{
	//		QString filePath = m_strlistPath.at(i);
	//		QString corrFilePath = CorrectSingleFile(filePath.toLocal8Bit().constData());
	//		ui.plainTextEdit_Corrected->appendPlainText(corrFilePath);
	//	}
	SLT_DrawRawImages();
}

void CbctRecon::SLT_DrawReconImage()
{
	if (m_dspYKReconImage == NULL)
		return;

	if (!m_spCrntReconImg)
	{
	  cout << "no recon image to be displayed" << endl;
	  return;
	}

	//  The ExtractImageFilter type is instantiated using the input and
	//  output image types. A filter object is created with the New()
	//  method and assigned to a SmartPointer.

	typedef itk::ExtractImageFilter<USHORT_ImageType, USHORT_ImageType2D> ExtractFilterType;
	ExtractFilterType::Pointer extractFilter = ExtractFilterType::New();

	typedef itk::ImageDuplicator< USHORT_ImageType > DuplicatorType;
	DuplicatorType::Pointer duplicator = DuplicatorType::New();
	duplicator->SetInputImage(m_spCrntReconImg);
	duplicator->Update();
	USHORT_ImageType::Pointer clonedImage = duplicator->GetOutput();


	//USHORT_ImageType::Pointer cloned3DImg = USHORT_ImageType::New();

	//cloned3DImg->Clone()
	//m_spReconImg->Clone();

	//InPlaceImageFilter: This overwrites output on input buffer.
	// so original reconImg will be= gone after running once.
	//use less memory than standard ImageToImageFilters

	//cout <<"is InPlace going in extractFilter?  " << extractFilter->GetInPlace() << endl;
	//cout << "InPlace Possible? " << extractFilter->CanRunInPlace() << endl;
	extractFilter->SetDirectionCollapseToSubmatrix();
	//cout <<"is InPlace still going?  " << extractFilter->GetInPlace() << endl;

	//return;


	//memory check:
	//USHORT_ImageType::RegionType crntRegion3D = m_spReconImg->GetBufferedRegion();
	USHORT_ImageType::RegionType crntRegion3D = clonedImage->GetBufferedRegion();

	crntRegion3D = clonedImage->GetBufferedRegion();
	//cout << crntRegion3D.GetSize()[2] << endl; // 01


	//cout << crntRegion3D.size[0] << endl;
	//cout << crntRegion3D.size[1] << endl;
	//USHORT_ImageType::RegionType crntRegion3D;

	//  We take the size from the region and collapse the size in the $Z$
	//  component by setting its value to $1$.


	//Get Image Size and Extraction Index info.
	USHORT_ImageType::SizeType size = crntRegion3D.GetSize();
	size[2] = 0; // z size number = 0 --> should not be 1

	//cout << "size is " << size[0] << "	" << size[1] << "	" << size[2] << endl;

	USHORT_ImageType::IndexType start = crntRegion3D.GetIndex();
	const int iSliceNumber = ui.spinBoxReconImgSliceNo->value();
	//const int iSliceNumber = m_iTmpIdx;
	start[2] = iSliceNumber;//60

	double originZ = m_spCrntReconImg->GetOrigin()[2];
	double spacingZ = m_spCrntReconImg->GetSpacing()[2];
	double posZ = originZ + iSliceNumber*spacingZ;

	QString strPosZ;
	strPosZ.sprintf("%4.2f", posZ);
	ui.lineEdit_CurrentPosZ->setText(strPosZ);

	//cout << "start point is " << start[0] << "	" << start[1] << "	" << start[2] << endl;


	//Define a region to generate
	USHORT_ImageType::RegionType desiredRegion;
	desiredRegion.SetSize(  size  );//410 410 0
	desiredRegion.SetIndex( start );	// 0 0 60


	//Error occurred here --> sloved by crntRegion3D = m_spReconImg->GetBufferedRegion();
	extractFilter->SetExtractionRegion( desiredRegion );//error

	//  Below we connect the reader, filter and writer to form the data
	//  processing pipeline.
	extractFilter->SetInput( clonedImage );

	//bool bCanRunInPlace = extractFilter->CanRunInPlace();
	//Error Here Again! --> solved by extractFilter->SetInput(

	//ui.spinBoxReconImg_Min->setMinimum(0);
	//ui.spinBoxReconImg_Max->setMaximum(119);
	extractFilter->Update();

	/*typedef itk::ImageFileWriter<USHORT_ImageType2D> WriterType;
	WriterType::Pointer writer = WriterType::New();
	writer->SetFileName("D:\\tmpExtractedImg.mha");
	writer->SetUseCompression(true);
	writer->SetInput(extractFilter->GetOutput());
	writer->Update();	*/

	//crntRegion3D = m_spReconImg->GetLargestPossibleRegion();
	//cout << crntRegion3D.GetSize()[2] << endl; // 120
	//crntRegion3D = m_spReconImg->GetBufferedRegion();
	//cout << crntRegion3D.GetSize()[2] << endl; // 01

	//cout <<m_dspYKReconImage->m_iWidth << "	" << m_dspYKReconImage->m_iHeight << endl;
	USHORT_ImageType2D::Pointer pCrnt2D = extractFilter->GetOutput();
	YK16GrayImage::CopyItkImage2YKImage(pCrnt2D, m_dspYKReconImage); //dimension should be same automatically.

	//m_dspYKReconImage->SaveDataAsRaw("D:\\RawFile.raw"); //410 410 OK

	PostApplyFOVDispParam();
	//SLT_UpdatePostProcDispObj();

	if (ui.checkBox_PostDispObjOn->isChecked())
	{
		m_dspYKReconImage->m_bDrawFOVCircle = true;
		m_dspYKReconImage->m_bDrawTableLine = true;
	}

	else
	{
		m_dspYKReconImage->m_bDrawFOVCircle = false;
		m_dspYKReconImage->m_bDrawTableLine = false;
	}



	m_dspYKReconImage->FillPixMapMinMax(ui.sliderReconImgMin->value(), ui.sliderReconImgMax->value());
	//m_dspYKReconImage->m_bDrawProfileX = true;
	//m_dspYKReconImage->m_bDrawProfileY = true;
	//m_dspYKReconImage->DrawToLabel(ui.labelReconImage);
	ui.labelReconImage->SetBaseImage(m_dspYKReconImage);
	ui.labelReconImage->update();

	//SLT_DrawGraph();
	SLT_UpdateTable();
}

// Get the projection geometry
void CbctRecon::LoadRTKGeometryFile( const char* filePath )
{
	rtk::ThreeDCircularProjectionGeometryXMLFileReader::Pointer geometryReader;
	geometryReader = rtk::ThreeDCircularProjectionGeometryXMLFileReader::New();
	geometryReader->SetFilename(filePath);
	TRY_AND_EXIT_ON_ITK_EXCEPTION( geometryReader->GenerateOutputInformation() )
		std::cout << "Geometry reading succeed" << std::endl;

	m_spFullGeometry = geometryReader->GetOutputObject();

	//fullGeometry->GetGantryAngles();
	int geoDataSize = m_spFullGeometry->GetGantryAngles().size();
	cout << "Geometry data size(projection gantry angles): " << geoDataSize << endl;
	if (geoDataSize < 1)
		return;

	// CW: continuously ascending except 360 - 0 interface, no negative value
	// CCW: continuously descending except 0 - 360 interface, no negative value

	//m_bScanDirectionCW

	/*for (int i = 0 ; i<geoDataSize ; i++)
	{
		cout << "Projection gantry angle " << i << ": " << m_spFullGeometry->GetGantryAngles().at(i) << endl;
	}
	cout << "Coordination is following IEC coordinate (e.g. 190 deg = RPO, 170 LPO, 30 = LAO supposing HFS) no and kV source position (not MV gantry)" << endl;*/


	vector<double> vTempConvAngles;

	vector<double>::const_iterator it;
	vector<double>::const_iterator itBegin = (m_spFullGeometry->GetGantryAngles()).begin();
	vector<double>::const_iterator itEnd = (m_spFullGeometry->GetGantryAngles()).end();

	for (it =  itBegin; it !=itEnd ; it++)
	{
		double tmpAngle = (*it);

		if (tmpAngle > 180.0)
			tmpAngle = tmpAngle - 360.0;

		vTempConvAngles.push_back(tmpAngle);
	}

	//compare 2 points in the middle of the angle list
	int iLowerIdx = (int)(geoDataSize* 1.0 / 3.0);
	int iUpperIdx = (int)(geoDataSize* 2.0 / 3.0);

	if (vTempConvAngles.at(iLowerIdx) < vTempConvAngles.at(iUpperIdx)) // ascending
	{
		m_bScanDirectionCW = true;
		cout << "The scan direction is CW" << endl;
	}

	else
	{
		m_bScanDirectionCW = false;
		cout << "The scan direction is CCW" << endl;
	}
	//cout << "AngularGapsWithNext Size: " << custGeometry->GetAngularGapsWithNext().size() << endl;
	//cout << "AngularGapsWithNext Mean (deg): " << vectorMean(custGeometry->GetAngularGapsWithNext())/itk::Math::pi * 180.0<< endl;
	cout << "AngularGaps Size: " << m_spFullGeometry->GetAngularGaps(m_spFullGeometry->GetSourceAngles()).size() << endl;

	//SLT_DoReconstruction();

	//double meanGap = vectorMean(m_spFullGeometry->GetAngularGaps())/itk::Math::pi * 180.0;
	//cout << "AngularGaps Mean (deg): " << meanGap << endl;
	//double angleSum = vectorSum(m_spFullGeometry->GetAngularGaps())/itk::Math::pi * 180.0;//total rot. range from first angle
	//cout << "AngularGaps Sum (deg): " << angleSum  << endl;

}
void CbctRecon::SLT_OpenVarianGeomFile()
{
	QString strPath = QFileDialog::getOpenFileName(this,"Select a single file to open",m_strPathDirDefault,"Varian geometry file (*.xml)");

	//	QFileDialog::getOpenFileName(this, "Select FRAME.DBF file", "", "Varian DB file (*.dbf)", 0,0);

	if (strPath.length()<=1)
		return;

	ui.lineEdit_VarianGeomPath->setText(strPath);
	//m_pImgOffset->LoadRawImage(strPath.toLocal8Bit().constData(),IMG_WIDTH, IMG_HEIGHT);
	//LoadElektaGeometryFile(strPath.toLocal8Bit().constData());
	//SLT_DoReconstruction();
}

void CbctRecon::SLT_SetOutputPath()
{
	QString strPath = QFileDialog::getSaveFileName(this, "File path to save", "D:\\", "meta 3D image data (*.mha)",0,0); //Filename don't need to exist

	if (strPath.length()<=1)
		return;

	ui.lineEdit_OutputFilePath->setText(strPath);
}

#if CUDA_FOUND
void CbctRecon::CudaDoReconstructionTV(enREGI_IMAGES target)  // ADDED BY AGRAVGAARD
{

	if (!m_spProjImg3DFloat)
	{
		cout << "processed Projection image is not ready yet" << endl;
		return;
	}
	//Resampling first --> to save the recon time. 1024 --> 512

	//cout << m_spProjImg3DFloat->GetRequestedRegion().GetSize() << endl;
	//cout << spTmpImage->GetRequestedRegion().GetSize() << endl;
	//return;
#if CUDA_FOUND
	typedef itk::CastImageFilter< OutputImageType, CUDAOutputImageType > castToCUDAType;
	castToCUDAType::Pointer  castToCuda = castToCUDAType::New();
	castToCuda->SetInput(m_spProjImg3DFloat);
	castToCuda->Update();
	typedef itk::ImageDuplicator< CUDAOutputImageType > DuplicatorType;
	DuplicatorType::Pointer duplicator = DuplicatorType::New();
	duplicator->SetInputImage(castToCuda->GetOutput());
	duplicator->Update();
#else
	cout << "Not compiled with CUDA option! Required for Total Variation method!" << endl;
	return;
#endif

	CUDAOutputImageType::Pointer spCurImg = duplicator->GetOutput(); //already down sampled
	//OutputImageType::Pointer spCurImg = m_spProjImg3D;
	//spCurImg = duplicator->GetOutput();

	//ResampleItkImage(m_spProjImg3DFloat, spCurImg, resampleFactor);

	//Displaced detector weighting // set pipeline //inplace filter
	typedef rtk::CudaDisplacedDetectorImageFilter DDFType;
	DDFType::Pointer ddf = DDFType::New();

	if (ui.checkBox_UseDDF->isChecked())
	{
		ddf->SetInput(spCurImg);
		//ddf->SetGeometry( geometryReader->GetOutputObject() );
		ddf->SetGeometry(m_spCustomGeometry);
		cout << "DDF was set in pipeline" << endl;

		if (ui.checkBox_UpdateAfterFiltering->isChecked())
			ddf->Update();//no mememory increas: InPlace Filter

		spCurImg = ddf->GetOutput();
	}

	typedef rtk::CudaParkerShortScanImageFilter PSSFType;
	PSSFType::Pointer pssf = PSSFType::New();

	if (ui.checkBox_UsePSSF->isChecked())
	{
		// Short scan image filter
		pssf->SetInput(spCurImg);
		//pssf->SetGeometry( geometryReader->GetOutputObject() );
		pssf->SetGeometry(m_spCustomGeometry);
		//pssf->InPlaceOff(); //YKComments: Do not overwrite input image buffer for output
		cout << "short scan image filter success" << endl;

		if (ui.checkBox_UpdateAfterFiltering->isChecked())
			pssf->Update(); //no mememory increas: InPlace Filter

		spCurImg = pssf->GetOutput();
	}

	//Just Before going to Total Variation recon,
	//Update Projection data and delete old data.

	//Let's duplicate this
	//Original m_spProjImg3D will be deleted after update

	if (ui.checkBox_UpdateAfterFiltering->isChecked())
	{
		typedef itk::ImageDuplicator< CUDAOutputImageType > DuplicatorType;
		DuplicatorType::Pointer duplicator = DuplicatorType::New();
		duplicator->SetInputImage(spCurImg);
		duplicator->Update();
		m_spProjImg3DFloat = duplicator->GetOutput();

		SetMaxAndMinValueOfProjectionImage(); // scan m_spProjImg3D and update m_fProjImgValueMin, max
		SLT_DrawProjImages();
	}


	// Generate image sources for cone beam CT reconstruction
	typedef rtk::ConstantImageSource< CUDAOutputImageType > ConstantImageSourceType;

	ConstantImageSourceType::PointType origin;
	ConstantImageSourceType::SpacingType spacing;
	ConstantImageSourceType::SizeType sizeOutput;

	sizeOutput[0] = ui.lineEdit_outImgDim_AP->text().toInt(); //pixel
	sizeOutput[1] = ui.lineEdit_outImgDim_SI->text().toInt();  //Caution!: direction is different in NKI SCAN FIle
	sizeOutput[2] = ui.lineEdit_outImgDim_LR->text().toInt();

	spacing[0] = ui.lineEdit_outImgSp_AP->text().toDouble();
	spacing[1] = ui.lineEdit_outImgSp_SI->text().toDouble();
	spacing[2] = ui.lineEdit_outImgSp_LR->text().toDouble();

	origin[0] = -0.5*sizeOutput[0] * spacing[0]; //Y in DCM?
	origin[1] = -0.5*sizeOutput[1] * spacing[1];  //Z in DCM?
	origin[2] = -0.5*sizeOutput[2] * spacing[2]; //X in DCM?

	ConstantImageSourceType::Pointer constantImageSource = ConstantImageSourceType::New();
	constantImageSource->SetOrigin(origin);
	constantImageSource->SetSpacing(spacing);
	constantImageSource->SetSize(sizeOutput);
	constantImageSource->SetConstant(0.0);  //initial value

	/* First IMAGE source then PROJECTION source:  (Iz projection just 2ximg?)*/

	double fTruncCorFactor = ui.lineEdit_Ramp_TruncationCorrection->text().toDouble();
	double tvBeta = 5; // ui.lineEdit_TVbeta->text().toFloat();
	double tvAlpha = 5; //ui.lineEdit_TValpha->text().toFloat();
	double tvAL_iter = 5; //ui.lineEdit_AL_iter->text().toUInt();
	double tvCG_iter = 5; //ui.lineEdit_CG_iter->text().toUInt();

	if (fTruncCorFactor > 0.0 && target == REGISTER_COR_CBCT)
	{
		cout << "Warning! Truncation factor is " << fTruncCorFactor << ". Regardless of previous setting, this factor should not be 0 for scatter corrected CBCT. Now zero value is applied." << endl;
		fTruncCorFactor = 0.0;
		if (ui.checkBox_UseTrunc->isChecked())
			fTruncCorFactor = ui.lineEdit_Ramp_TruncationCorrection->text().toDouble();
	}

	cout << "Admm Total variation options are being set.." << endl;
	
	// ADMMTotalVariation reconstruction filtering
	typedef rtk::ADMMTotalVariationConeBeamReconstructionFilter
		<CUDAOutputImageType, GradientCUDAOutputImageType> ADMMTotalVariationType;
	ADMMTotalVariationType::Pointer admmtotalvariation = ADMMTotalVariationType::New();
	admmtotalvariation->SetInput( constantImageSource->GetOutput() );
	admmtotalvariation->SetInput(1, spCurImg);
	admmtotalvariation->SetGeometry(m_spCustomGeometry);
	admmtotalvariation->SetAlpha(tvAlpha);
	admmtotalvariation->SetBeta(tvBeta);
	admmtotalvariation->SetAL_iterations(tvAL_iter);
	admmtotalvariation->SetCG_iterations(tvCG_iter);
	

	std::cout << "\n\n****** CUDA Voxel-Based Backprojector and CUDA Forward projector ******" << std::endl;

	//admmtotalvariation->SetForwardProjectionFilter(2);
	//admmtotalvariation->SetBackProjectionFilter(2);
	// TRY_AND_EXIT_ON_ITK_EXCEPTION( admmtotalvariation->Update() ); //SHOULD BE DONE AT END OF STREAMING ONLY

	//std::cout << "\n\n PASSED! " << std::endl;

	typedef itk::CudaImage< FloatPixelType, 3 >     OutputImageType;
	typedef itk::CudaImage< USHORT_PixelType, 3 > USHORT_ImageType;

	std::cout << "Cone beam reconstruction pipeline is ready" << std::endl;

	// Streaming depending on streaming capability of writer --> not affect the calc. speed
	typedef itk::StreamingImageFilter<OutputImageType, OutputImageType> StreamerType;
	StreamerType::Pointer streamerBP = StreamerType::New();
	streamerBP->SetInput(admmtotalvariation->GetOutput());
	streamerBP->SetNumberOfStreamDivisions(1); // YK: 1 in example code from "rtkfdk" AG: 8 in test code from "rtkfdk"

	cout << "Euler 3D Transformation: from RTK-procuded volume to standard DICOM coordinate" << endl;

	/* RTK-produced 3D Volume should be changed in coordination of itk */
	/* Coordination transformation using Euler 3D transformation */

	// 1) Prepare Canvas parameter
	//OutputImageType::Pointer fixedImg = OutputImageType::New();
	//start index: What is the index of Left Top Inferior corner in DICOM coordinate?


	//Same image type from original image -3D & float
	OutputImageType::IndexType start_trans;
	start_trans[0] = 0;
	start_trans[1] = 0;
	start_trans[2] = 0;

	OutputImageType::SizeType size_trans;
	size_trans[0] = sizeOutput[0]; // X //410
	size_trans[1] = sizeOutput[2]; //Y  // 410
	size_trans[2] = sizeOutput[1]; //Z // 120?

	OutputImageType::SpacingType spacing_trans;
	spacing_trans[0] = spacing[0];
	spacing_trans[1] = spacing[2];
	spacing_trans[2] = spacing[1];

	OutputImageType::PointType Origin_trans;
	Origin_trans[0] = -0.5* size_trans[0] * spacing_trans[0];
	Origin_trans[1] = -0.5* size_trans[1] * spacing_trans[1];
	Origin_trans[2] = -0.5* size_trans[2] * spacing_trans[2];

	OutputImageType::RegionType region_trans;
	region_trans.SetSize(size_trans);
	region_trans.SetIndex(start_trans);

	/* 2) Prepare Target image */
	OutputImageType::Pointer targetImg = streamerBP->GetOutput();

	/* 3) Configure transform */
	typedef itk::Euler3DTransform< double > TransformType;
	TransformType::Pointer transform = TransformType::New();

	TransformType::ParametersType param;
	param.SetSize(6);
	//MAXIMUM PARAM NUMBER: 6!!!
	//if (target == REGISTER_COR_CBCT){
	//	param.put(0, 0.0);
	//}
	//else{
	param.put(0, 0.0); //rot X // 0.5 = PI/2 //Actuly is  rot Z ELEKTA VS VARIAN <-- THIS MIGHT BE THE SOLUTION
	//}
	param.put(1, itk::Math::pi / 2.0);//rot Y //Actuly is rot Y
	param.put(2, itk::Math::pi / -2.0);//rot Z //Actuly is rot X
	param.put(3, 0.0); // Trans X mm
	param.put(4, 0.0); // Trans Y mm
	param.put(5, 0.0); // Trans Z mm

	TransformType::ParametersType fixedParam(3); //rotation center
	fixedParam.put(0, 0);
	fixedParam.put(1, 0);
	fixedParam.put(2, 0);

	transform->SetParameters(param);
	transform->SetFixedParameters(fixedParam); //Center of the Transform

	cout << "Transform matrix:" << "	" << endl;
	cout << transform->GetMatrix() << std::endl;

	typedef itk::ResampleImageFilter<OutputImageType, OutputImageType> ResampleFilterType;
	ResampleFilterType::Pointer resampler = ResampleFilterType::New();
	//OutputImageType::RegionType fixedImg_Region = fixedImg->GetLargestPossibleRegion().GetSize();

	resampler->SetInput(targetImg);
	resampler->SetSize(size_trans);
	resampler->SetOutputOrigin(Origin_trans); //Lt Top Inf of Large Canvas
	resampler->SetOutputSpacing(spacing_trans); // 1 1 1
	resampler->SetOutputDirection(targetImg->GetDirection()); //image normal?
	resampler->SetTransform(transform);
	//resampler->Update();//yktemp Error 2

	cout << "Flip filter is being applied" << endl;
	typedef itk::FlipImageFilter< OutputImageType >  FilterType;
	FilterType::Pointer flipFilter = FilterType::New();
	typedef FilterType::FlipAxesArrayType FlipAxesArrayType;
	FlipAxesArrayType arrFlipAxes;
	arrFlipAxes[0] = 1;
	arrFlipAxes[1] = 0;
	arrFlipAxes[2] = 0;

	flipFilter->SetFlipAxes(arrFlipAxes);
	flipFilter->SetInput(resampler->GetOutput());

	/*OutputImageType::Pointer floatImg = flipFilter->GetOutput();*/
	//const unsigned int Dimension = 3;
	//FinalImageType::Pointer finalImg ;

	typedef itk::AbsImageFilter<OutputImageType, OutputImageType> AbsImageFilterType;
	AbsImageFilterType::Pointer absImgFilter = AbsImageFilterType::New();
	absImgFilter->SetInput(flipFilter->GetOutput()); // 20140206 modified it was a bug
	//absImgFilter->SetInput(resampler->GetOutput());

	typedef itk::MultiplyImageFilter<OutputImageType, OutputImageType, CUDAOutputImageType> MultiplyImageFilterType;
	MultiplyImageFilterType::Pointer multiplyImageFilter = MultiplyImageFilterType::New();
	multiplyImageFilter->SetInput(absImgFilter->GetOutput());
	multiplyImageFilter->SetConstant(65536); //calculated already


	//typedef unsigned short FinalPixelType;
	//typedef itk::Image< FinalPixelType, 3 > FinalImageType;

	typedef itk::CastImageFilter< OutputImageType, USHORT_ImageType > CastFilterType;
	CastFilterType::Pointer castFilter = CastFilterType::New();
	castFilter->SetInput(multiplyImageFilter->GetOutput());
	//castFilter->Update(); //YK20150109

	USHORT_ImageType::SizeType indexRadius;
	indexRadius[0] = ui.lineEdit_PostMedSizeX->text().toInt(); // radius along x
	indexRadius[1] = ui.lineEdit_PostMedSizeY->text().toInt(); // radius along y
	indexRadius[2] = ui.lineEdit_PostMedSizeZ->text().toInt(); // radius along y

	USHORT_ImageType::Pointer tmpReconImg;
	//if all 0 0 0 don't do the median filtering

	itk::TimeProbe reconTimeProbe;
	reconTimeProbe.Start();

	cout << "Reconstructing the image.. please wait..." << endl;

	if (ui.checkBox_PostMedianOn->isChecked() && (indexRadius[0] != 0 || indexRadius[1] != 0 || indexRadius[2] != 0))
	{
		typedef itk::MedianImageFilter<USHORT_ImageType, USHORT_ImageType >  FilterType;
		FilterType::Pointer medFilter = FilterType::New();

		//YKTEMP20141218 S
		// typedef itk::MeanImageFilter<USHORT_ImageType, USHORT_ImageType >  FilterType;
		// FilterType::Pointer medFilter = FilterType::New();
		//YKTEMP20141218 E

		//this is radius. 1 --> median window 3
		cout << "Post median(3D) filtering is in the pipeline..Size(radius X Y Z) is = " << indexRadius << endl;

		medFilter->SetRadius(indexRadius);
		medFilter->SetInput(castFilter->GetOutput());
		medFilter->Update(); //Error here!g

		tmpReconImg = medFilter->GetOutput();
		cout << "median filtering has been done" << endl;
	}
	else
	{
		cout << "No post median filtering is used" << endl;
		castFilter->Update();
		tmpReconImg = castFilter->GetOutput();
	}

	typedef itk::ThresholdImageFilter <USHORT_ImageType> ThresholdImageFilterType;
	ThresholdImageFilterType::Pointer thresholdFilterAbove = ThresholdImageFilterType::New();
	thresholdFilterAbove->SetInput(tmpReconImg);
	thresholdFilterAbove->ThresholdAbove(4095);
	thresholdFilterAbove->SetOutsideValue(4095);

	ThresholdImageFilterType::Pointer thresholdFilterBelow = ThresholdImageFilterType::New();
	thresholdFilterBelow->SetInput(thresholdFilterAbove->GetOutput());
	thresholdFilterBelow->ThresholdBelow(0);
	thresholdFilterBelow->SetOutsideValue(0);
	thresholdFilterBelow->Update();
	//tmpReconImg = thresholdFilterBelow->GetOutput();

	cout << "After Filtering" << endl;

	reconTimeProbe.Stop();
	std::cout << "It took " << reconTimeProbe.GetMean() << ' ' << reconTimeProbe.GetUnit() << std::endl;
	ui.lineEdit_ReconstructionTime->setText(QString("%1").arg(reconTimeProbe.GetMean()));

	//cout << "TestYK" << endl;
	//cout << tmpReconImg->GetRequestedRegion().GetSize() << endl;
	//cout << "Before InPlace Off: " <<  castFilter->GetInPlace() << endl;	//0
	//castFilter->InPlaceOff();
	//cout << "After InPlace Off: " <<  castFilter->GetInPlace() << endl; //0
	//Because Input Output format are different, this filter cannot do InPlace function.

	itk::Image<USHORT_PixelType, 3>::Pointer tmpReconImgOutCuda;
	typedef itk::CastImageFilter <USHORT_CUDAImageType, itk::Image<USHORT_PixelType, 3> > castBackType;
	castBackType::Pointer castBack = castBackType::New();
	castBack->SetInput(thresholdFilterBelow->GetOutput());
	castBack->Update();
	tmpReconImgOutCuda = castBack->GetOutput();
	//By default CanRunInPlace checks whether the input and output image type match.
	switch (target)
	{
	case REGISTER_RAW_CBCT:
		m_spRawReconImg = tmpReconImgOutCuda; //Checked.. successfully alive.
		m_spCrntReconImg = m_spRawReconImg;
		break;
	case REGISTER_COR_CBCT:
		m_spScatCorrReconImg = tmpReconImgOutCuda; //Checked.. successfully alive.
		m_spCrntReconImg = m_spScatCorrReconImg;
		break;
	}
	QString outputFilePath = ui.lineEdit_OutputFilePath->text();

	QFileInfo outFileInfo(outputFilePath);
	QDir outFileDir = outFileInfo.absoluteDir();

	if (outputFilePath.length() < 2 || !outFileDir.exists())
	{
		cout << "No available output path. Should be exported later" << endl;
		ui.lineEdit_Cur3DFileName->setText("FDK-reconstructed volume");
	}
	else
	{
		typedef itk::ImageFileWriter<itk::Image< USHORT_PixelType, 3 >> WriterType;
		WriterType::Pointer writer = WriterType::New();
		writer->SetFileName(outputFilePath.toLocal8Bit().constData());
		writer->SetUseCompression(true); //not exist in original code (rtkfdk)
		writer->SetInput(m_spCrntReconImg);

		cout << "Writing the image to: " << outputFilePath.toLocal8Bit().constData() << endl;

		TRY_AND_EXIT_ON_ITK_EXCEPTION(writer->Update());

		ui.lineEdit_Cur3DFileName->setText(outputFilePath);
		std::cout << std::endl;
		std::cout << "Output generation was succeeded" << std::endl;
	}


	m_dspYKReconImage->CreateImage(size_trans[0], size_trans[1], 0);

	ui.spinBoxReconImgSliceNo->setMinimum(0);
	ui.spinBoxReconImgSliceNo->setMaximum(size_trans[2] - 1);
	ui.spinBoxReconImgSliceNo->setValue(qRound(size_trans[2] / 2.0)); //DrawReconImage is called automatically

	//For 2D image display
	//SLT_DrawReconImage();
	//m_spReconImg = writer->GetOutput();

	/*OutputImageType::SizeType AfterReconSize = m_spProjImg3D->GetBufferedRegion().GetSize();
	cout << "AfterReconSize  " << AfterReconSize[0] << "	"
	<< AfterReconSize[1] << "	"
	<< AfterReconSize[2] << endl;*/

	/*ui.radioButton_graph_recon->setChecked(true);
	SLT_InitializeGraphLim();
	SLT_DrawReconImage();	*/

	//SLT_ViewRegistration();
	if (target == REGISTER_COR_CBCT)
	{
		UpdateReconImage(m_spCrntReconImg, QString("SCATTER_COR_CBCT"));
	}
	else if (target == REGISTER_RAW_CBCT)
	{
		UpdateReconImage(m_spCrntReconImg, QString("RAW_CBCT"));
	}

	cout << "FINISHED!: Total Variation CBCT reconstruction" << endl;
	//if not found, just skip

	//SLT_DrawGraph();
	//2) Load Geometry file.
	//3) Prepare all parameters from GUI components
}
#else
void CbctRecon::CudaDoReconstructionTV(enREGI_IMAGES target)  // ADDED BY AGRAVGAARD
{
	cout << "This program were not compiled with the CUDA option, total variation only implemented with CUDA." << endl;
	return;
}
#endif

#if CUDA_FOUND
void CbctRecon::CudaDoReconstructionFDK(enREGI_IMAGES target)
{
	typedef itk::CastImageFilter <OutputImageType, CUDAOutputImageType > castToCudaType;
	castToCudaType::Pointer castToCuda = castToCudaType::New();
	castToCuda->SetInput(m_spProjImg3DFloat);
	castToCuda->Update();
	CUDAOutputImageType::Pointer cuda_spProjImg3DFloat = castToCuda->GetOutput();

	if (!cuda_spProjImg3DFloat)
	{
		cout << "processed Projection image is not ready yet" << endl;
		return;
	}

#if CUDA_FOUND
	cout << "CUDA method will be used..." << endl;
#else
	cout << "Not compiled with CUDA option..." << endl;
	return;
#endif
	//Resampling first --> to save the recon time. 1024 --> 512

	
	
	typedef itk::ImageDuplicator< CUDAOutputImageType > DuplicatorType;
	DuplicatorType::Pointer duplicator = DuplicatorType::New();
	duplicator->SetInputImage(cuda_spProjImg3DFloat);
	duplicator->Update();

	CUDAOutputImageType::Pointer spCurImg = duplicator->GetOutput(); //already down sampled

	//OutputImageType::Pointer spCurImg = m_spProjImg3D;
	//spCurImg = duplicator->GetOutput();

	//ResampleItkImage(m_spProjImg3DFloat, spCurImg, resampleFactor);

	//Displaced detector weighting // set pipeline //inplace filter

	typedef rtk::CudaDisplacedDetectorImageFilter DDFType;
	DDFType::Pointer ddf = DDFType::New();

	if (ui.checkBox_UseDDF->isChecked())
	{
		ddf->SetInput(spCurImg);
		//ddf->SetGeometry( geometryReader->GetOutputObject() );
		ddf->SetGeometry(m_spCustomGeometry);
		cout << "DDF was set in pipeline" << endl;

		if (ui.checkBox_UpdateAfterFiltering->isChecked())
			ddf->Update();//no mememory increas: InPlace Filter

		spCurImg = ddf->GetOutput();
	}

	typedef rtk::CudaParkerShortScanImageFilter PSSFType;
	PSSFType::Pointer pssf = PSSFType::New();


	if (ui.checkBox_UsePSSF->isChecked())
	{
		// Short scan image filter
		pssf->SetInput(spCurImg);
		//pssf->SetGeometry( geometryReader->GetOutputObject() );
		pssf->SetGeometry(m_spCustomGeometry);
		//pssf->InPlaceOff(); //YKComments: Do not overwrite input image buffer for output
		cout << "short scan image filter success" << endl;

		if (ui.checkBox_UpdateAfterFiltering->isChecked())
			pssf->Update(); //no mememory increas: InPlace Filter

		spCurImg = pssf->GetOutput();
	}

	//Just Before going to FDK recon,
	//Update Projection data and delete old data.

	//Let's duplicate this
	//Original m_spProjImg3D will be deleted after update

	if (ui.checkBox_UpdateAfterFiltering->isChecked())
	{
		typedef itk::ImageDuplicator< CUDAOutputImageType > DuplicatorType;
		DuplicatorType::Pointer duplicator = DuplicatorType::New();
		duplicator->SetInputImage(spCurImg);
		duplicator->Update();
		m_spProjImg3DFloat = duplicator->GetOutput();

		SetMaxAndMinValueOfProjectionImage(); // scan m_spProjImg3D and update m_fProjImgValueMin, max
		SLT_DrawProjImages();
	}


	// Generate image sources for cone beam CT reconstruction
	typedef rtk::ConstantImageSource< CUDAOutputImageType > ConstantImageSourceType;

	ConstantImageSourceType::PointType origin;
	ConstantImageSourceType::SpacingType spacing;
	ConstantImageSourceType::SizeType sizeOutput;

	sizeOutput[0] = ui.lineEdit_outImgDim_AP->text().toInt(); //pixel
	sizeOutput[1] = ui.lineEdit_outImgDim_SI->text().toInt();  //Caution!: direction is different in NKI SCAN FIle
	sizeOutput[2] = ui.lineEdit_outImgDim_LR->text().toInt();

	spacing[0] = ui.lineEdit_outImgSp_AP->text().toDouble();
	spacing[1] = ui.lineEdit_outImgSp_SI->text().toDouble();
	spacing[2] = ui.lineEdit_outImgSp_LR->text().toDouble();

	origin[0] = -0.5*sizeOutput[0] * spacing[0]; //Y in DCM?
	origin[1] = -0.5*sizeOutput[1] * spacing[1];  //Z in DCM?
	origin[2] = -0.5*sizeOutput[2] * spacing[2]; //X in DCM?

	ConstantImageSourceType::Pointer constantImageSource = ConstantImageSourceType::New();
	constantImageSource->SetOrigin(origin);
	constantImageSource->SetSpacing(spacing);
	constantImageSource->SetSize(sizeOutput);
	constantImageSource->SetConstant(0.0);  //initial value

	double fTruncCorFactor = ui.lineEdit_Ramp_TruncationCorrection->text().toDouble();
	double fHannCut = ui.lineEdit_Ramp_HannCut->text().toDouble();
	double fCosineCut = ui.lineEdit_Ramp_CosineCut->text().toDouble();
	double fHamming = ui.lineEdit_Ramp_Hamming->text().toDouble();
	double fHannCutY = ui.lineEdit_Ramp_HannCutY->text().toDouble();
	double fRamLak = ui.lineEdit_Ramp_RamLakCut->text().toDouble();

	if (fTruncCorFactor > 0.0 && target == REGISTER_COR_CBCT)
	{
		cout << "Warning! Truncation factor is " << fTruncCorFactor << ". Regardless of previous setting, this factor should not be 0 for scatter corrected CBCT. Now zero value is applied." << endl;
		fTruncCorFactor = 0.0;
		if (ui.checkBox_UseTrunc->isChecked())
			fTruncCorFactor = ui.lineEdit_Ramp_TruncationCorrection->text().toDouble();
	}

	//YKTEMP
	cout << "fTruncCorFactor =" << fTruncCorFactor << endl;
	// This macro sets options for fdk filter which I can not see how to do better
	// because TFFTPrecision is not the same, e.g. for CPU and CUDA (SR)
	/*
#define SET_FELDKAMP_OPTIONS(f) \
	f->SetInput( 0, constantImageSource->GetOutput() ); \
	f->SetInput( 1, spCurImg ); \
	f->SetGeometry( m_spCustomGeometry ); \
	f->GetRampFilter()->SetTruncationCorrection(fTruncCorFactor); \
	f->GetRampFilter()->SetHannCutFrequency(fHannCut); \
	f->GetRampFilter()->SetCosineCutFrequency(fCosineCut); \
	f->GetRampFilter()->SetHammingFrequency(fHamming); \
	f->GetRampFilter()->SetHannCutFrequencyY(fHannCutY); 
	*/


	// FDK reconstruction filtering
	// itk::ImageToImageFilter<CUDAOutputImageType, CUDAOutputImageType>::Pointer CUDAfeldkamp;
	typedef rtk::CudaFDKConeBeamReconstructionFilter FDKCUDAType;
	FDKCUDAType::Pointer CUDAfeldkamp = FDKCUDAType::New();

	cout << "CUDA will be used for FDK reconstruction" << endl;
	// CUDAfeldkamp = FDKCUDAType::New();
	CUDAfeldkamp->SetInput(0, constantImageSource->GetOutput());
	CUDAfeldkamp->SetInput(1, spCurImg);
	CUDAfeldkamp->SetGeometry(m_spCustomGeometry);
	CUDAfeldkamp->GetRampFilter()->SetTruncationCorrection(fTruncCorFactor);
	CUDAfeldkamp->GetRampFilter()->SetHannCutFrequency(fHannCut);
	CUDAfeldkamp->GetRampFilter()->SetCosineCutFrequency(fCosineCut);
	CUDAfeldkamp->GetRampFilter()->SetHammingFrequency(fHamming);
	CUDAfeldkamp->GetRampFilter()->SetHannCutFrequencyY(fHannCutY);
	CUDAfeldkamp->GetRampFilter()->SetRamLakCutFrequency(fRamLak);
	// SET_CUDA_FELDKAMP_OPTIONS(static_cast<FDKCUDAType*>(CUDAfeldkamp.GetPointer()));

	typedef rtk::FieldOfViewImageFilter <CUDAOutputImageType, CUDAOutputImageType> FOVFilterType;
	FOVFilterType::Pointer fieldofviewFilter = FOVFilterType::New();
	if (ui.checkBox_UpdateAfterFiltering->isChecked())
	{
		CUDAfeldkamp->Update();
		std::cout << "FOV filter is being added.." << std::endl;
		fieldofviewFilter->SetInput(0, CUDAfeldkamp->GetOutput());
		fieldofviewFilter->SetGeometry(m_spCustomGeometry);
		if (!ui.checkBox_UseDDF->isChecked())
		{
			fieldofviewFilter->SetDisplacedDetector(TRUE);
		}
		fieldofviewFilter->SetProjectionsStack(duplicator->GetOutput());
		fieldofviewFilter->Update();
	}



	cout << "Cone beam reconstruction pipeline is ready" << std::endl;

	// Streaming depending on streaming capability of writer --> not affect the calc. speed

	typedef itk::StreamingImageFilter <CUDAOutputImageType, CUDAOutputImageType> StreamerType;
	StreamerType::Pointer streamerBP = StreamerType::New();

	if (ui.checkBox_UpdateAfterFiltering->isChecked())
	{
		streamerBP->SetInput(fieldofviewFilter->GetOutput());
	}
	else
	{
		streamerBP->SetInput(CUDAfeldkamp->GetOutput());
	}
	streamerBP->SetNumberOfStreamDivisions(4); // YK: 1 in example code from "rtkfdk" //AG: stated in test: 4 for ITK MAJOR >= 4

	cout << "Euler 3D Transformation: from RTK-procuded volume to standard DICOM coordinate" << endl;

	/* RTK-produced 3D Volume should be changed in coordination of itk */
	/* Coordination transformation using Euler 3D transformation */

	// 1) Prepare Canvas parameter
	//OutputImageType::Pointer fixedImg = OutputImageType::New();
	//start index: What is the index of Left Top Inferior corner in DICOM coordinate?


	//Same image type from original image -3D & float
	OutputImageType::IndexType start_trans;
	start_trans[0] = 0;
	start_trans[1] = 0;
	start_trans[2] = 0;

	OutputImageType::SizeType size_trans;
	size_trans[0] = sizeOutput[0]; // X //410
	size_trans[1] = sizeOutput[2]; //Y  // 410
	size_trans[2] = sizeOutput[1]; //Z // 120?

	OutputImageType::SpacingType spacing_trans;
	spacing_trans[0] = spacing[0];
	spacing_trans[1] = spacing[2];
	spacing_trans[2] = spacing[1];

	OutputImageType::PointType Origin_trans;
	Origin_trans[0] = -0.5* size_trans[0] * spacing_trans[0];
	Origin_trans[1] = -0.5* size_trans[1] * spacing_trans[1];
	Origin_trans[2] = -0.5* size_trans[2] * spacing_trans[2];

	OutputImageType::RegionType region_trans;
	region_trans.SetSize(size_trans);
	region_trans.SetIndex(start_trans);

	/* 2) Prepare Target image */
	CUDAOutputImageType::Pointer targetImg = streamerBP->GetOutput();

	/* 3) Configure transform */
	typedef itk::Euler3DTransform< double > TransformType;
	TransformType::Pointer transform = TransformType::New();

	TransformType::ParametersType param;
	param.SetSize(6);
	//MAXIMUM PARAM NUMBER: 6!!!
	//if (target == REGISTER_COR_CBCT){
	//	param.put(0, 0.0);
	//}
	//else{
	param.put(0, 0.0); //rot X // 0.5 = PI/2 //Actuly is  rot Z ELEKTA VS VARIAN <-- THIS MIGHT BE THE SOLUTION
	//}
	param.put(1, itk::Math::pi / 2.0);//rot Y //Actuly is rot Y
	param.put(2, itk::Math::pi / -2.0);//rot Z //Actuly is rot X
	param.put(3, 0.0); // Trans X mm
	param.put(4, 0.0); // Trans Y mm
	param.put(5, 0.0); // Trans Z mm

	TransformType::ParametersType fixedParam(3); //rotation center
	fixedParam.put(0, 0);
	fixedParam.put(1, 0);
	fixedParam.put(2, 0);

	transform->SetParameters(param);
	transform->SetFixedParameters(fixedParam); //Center of the Transform

	cout << "Transform matrix:" << "	" << endl;
	cout << transform->GetMatrix() << std::endl;

	typedef itk::ResampleImageFilter <CUDAOutputImageType, CUDAOutputImageType> ResampleFilterType;
	ResampleFilterType::Pointer resampler = ResampleFilterType::New();
	//OutputImageType::RegionType fixedImg_Region = fixedImg->GetLargestPossibleRegion().GetSize();

	resampler->SetInput(targetImg);
	resampler->SetSize(size_trans);
	resampler->SetOutputOrigin(Origin_trans); //Lt Top Inf of Large Canvas
	resampler->SetOutputSpacing(spacing_trans); // 1 1 1
	resampler->SetOutputDirection(targetImg->GetDirection()); //image normal?
	resampler->SetTransform(transform);
	//resampler->Update();//yktemp Error 2

	//LR flip

	cout << "Flip filter is being applied" << endl;
	typedef itk::FlipImageFilter< OutputImageType >  FilterType;
	FilterType::Pointer flipFilter = FilterType::New();
	typedef FilterType::FlipAxesArrayType FlipAxesArrayType;
	FlipAxesArrayType arrFlipAxes;
	arrFlipAxes[0] = 1;
	arrFlipAxes[1] = 0;
	arrFlipAxes[2] = 0;
	flipFilter->SetFlipAxes(arrFlipAxes);
	flipFilter->SetInput(resampler->GetOutput());

	/*OutputImageType::Pointer floatImg = flipFilter->GetOutput();*/
	//const unsigned int Dimension = 3;
	//FinalImageType::Pointer finalImg ;

	typedef itk::AbsImageFilter<OutputImageType, OutputImageType> AbsImageFilterType;
	AbsImageFilterType::Pointer absImgFilter = AbsImageFilterType::New();
	absImgFilter->SetInput(flipFilter->GetOutput()); // 20140206 modified it was a bug
	//absImgFilter->SetInput(resampler->GetOutput());

	typedef itk::MultiplyImageFilter<OutputImageType, OutputImageType, CUDAOutputImageType> MultiplyImageFilterType;
	MultiplyImageFilterType::Pointer multiplyImageFilter = MultiplyImageFilterType::New();
	multiplyImageFilter->SetInput(absImgFilter->GetOutput());
	multiplyImageFilter->SetConstant(65536); //calculated already


	//typedef unsigned short FinalPixelType;
	//typedef itk::Image< FinalPixelType, 3 > FinalImageType;
	typedef itk::CastImageFilter< CUDAOutputImageType, USHORT_ImageType > CastFilterType;
	CastFilterType::Pointer castFilter = CastFilterType::New();
	/*Total Variation Filter*/
	if (ui.checkBox_TV->isChecked()){
		/*To check TV is doing its job:*/
		typedef itk::StatisticsImageFilter < CUDAOutputImageType > MeanAlgorithmType;
		MeanAlgorithmType::Pointer meanAlgorithm = MeanAlgorithmType::New();
		meanAlgorithm->SetInput(multiplyImageFilter->GetOutput());
		meanAlgorithm->Update();
		std::cout << "Sample mean = " << meanAlgorithm->GetMean() << std::endl;
		meanAlgorithm->ResetPipeline(); // Make sure not to get trivial output after TV filter.

		typedef rtk::TotalVariationImageFilter < CUDAOutputImageType > TotalVariationFilterType;
		TotalVariationFilterType::Pointer tv = TotalVariationFilterType::New();
		double totalVariationBefore;
		double totalVariationAfter;
		tv->SetInput(multiplyImageFilter->GetOutput());
		tv->Update();
		totalVariationBefore = tv->GetTotalVariation();
		std::cout << "Total variation before denoising is " << totalVariationBefore << std::endl;

		/*Now GO:*/
		typedef rtk::CudaTotalVariationDenoisingBPDQImageFilter  tvFilterType;
		tvFilterType::Pointer tvFilter = tvFilterType::New();
		cout << "Post total variation filtering is in the pipeline.." << endl;

		tvFilter->SetGamma(ui.lineEdit_TVgamma->text().toDouble());
		tvFilter->SetNumberOfIterations(ui.lineEdit_TV_iter->text().toInt());
		tvFilter->SetInput(multiplyImageFilter->GetOutput());
		tvFilter->Update();

		meanAlgorithm->SetInput(tvFilter->GetOutput());
		meanAlgorithm->Update();
		std::cout << "Sample mean = " << meanAlgorithm->GetMean() << std::endl;
		tv->SetInput(tvFilter->GetOutput());
		tv->Update();
		totalVariationAfter = tv->GetTotalVariation();
		std::cout << "Total variation after denoising is " << totalVariationAfter << std::endl;
		typedef itk::MultiplyImageFilter<CUDAOutputImageType, CUDAOutputImageType, CUDAOutputImageType> MultiplyImageFilterType;
		MultiplyImageFilterType::Pointer multiplyImageFilter = MultiplyImageFilterType::New();
		multiplyImageFilter->SetInput(tvFilter->GetOutput());
		if (ui.checkBox_Magic->isChecked()){
			multiplyImageFilter->SetConstant(1. / 0.9); // needed because m_beta, which is defined in in the TV denoising filter
		}
		else
		{
			multiplyImageFilter->SetConstant(0.9409); // needed because m_beta, which is defined in in the TV denoising filter.. the 0.0409 is a calculated guess, if it works, the factor will be investigated further..
		}


		castFilter->SetInput(multiplyImageFilter->GetOutput());

		castFilter->Update();
	}
	else
	{
		castFilter->SetInput(multiplyImageFilter->GetOutput());

		castFilter->Update(); //YK20150109
	}


	USHORT_ImageType::Pointer tmpReconImg;
	//if all 0 0 0 don't do the median filtering

	itk::TimeProbe reconTimeProbe;
	reconTimeProbe.Start();

	cout << "Reconstructing the image.. please wait..." << endl;
	USHORT_ImageType::SizeType indexRadius;
	indexRadius[0] = ui.lineEdit_PostMedSizeX->text().toInt(); // radius along x
	indexRadius[1] = ui.lineEdit_PostMedSizeY->text().toInt(); // radius along y
	indexRadius[2] = ui.lineEdit_PostMedSizeZ->text().toInt(); // radius along y
	if (ui.checkBox_PostMedianOn->isChecked() && (indexRadius[0] != 0 || indexRadius[1] != 0 || indexRadius[2] != 0))
	{
		typedef itk::MedianImageFilter<USHORT_ImageType, USHORT_ImageType >  FilterType; //TODO: CUDA THIS!!
		
		FilterType::Pointer medFilter = FilterType::New();

		//YKTEMP20141218 S
		// typedef itk::MeanImageFilter<USHORT_ImageType, USHORT_ImageType >  FilterType;
		// FilterType::Pointer medFilter = FilterType::New();
		//YKTEMP20141218 E

		//this is radius. 1 --> median window 3
		cout << "Post median(3D) filtering is in the pipeline..Size(radius X Y Z) is = " << indexRadius << endl;

		medFilter->SetRadius(indexRadius);
		medFilter->SetInput(castFilter->GetOutput());
		medFilter->Update(); //Error here!g

		tmpReconImg = medFilter->GetOutput();
		cout << "median filtering has been done" << endl;
	}
	else
	{
		cout << "No post median filtering is used" << endl;
		castFilter->Update();
		tmpReconImg = castFilter->GetOutput();
	}

	typedef itk::ThresholdImageFilter <USHORT_ImageType> ThresholdImageFilterType;
	ThresholdImageFilterType::Pointer thresholdFilterAbove = ThresholdImageFilterType::New();
	thresholdFilterAbove->SetInput(tmpReconImg);
	thresholdFilterAbove->ThresholdAbove(4095);
	thresholdFilterAbove->SetOutsideValue(4095);

	ThresholdImageFilterType::Pointer thresholdFilterBelow = ThresholdImageFilterType::New();
	thresholdFilterBelow->SetInput(thresholdFilterAbove->GetOutput());
	thresholdFilterBelow->ThresholdBelow(0);
	thresholdFilterBelow->SetOutsideValue(0);
	thresholdFilterBelow->Update();
	tmpReconImg = thresholdFilterBelow->GetOutput();

	cout << "After Filtering" << endl;

	reconTimeProbe.Stop();
	std::cout << "It took " << reconTimeProbe.GetMean() << ' ' << reconTimeProbe.GetUnit() << std::endl;
	ui.lineEdit_ReconstructionTime->setText(QString("%1").arg(reconTimeProbe.GetMean()));

	//cout << "TestYK" << endl;
	//cout << tmpReconImg->GetRequestedRegion().GetSize() << endl;
	//cout << "Before InPlace Off: " <<  castFilter->GetInPlace() << endl;	//0
	//castFilter->InPlaceOff();
	//cout << "After InPlace Off: " <<  castFilter->GetInPlace() << endl; //0
	//Because Input Output format are different, this filter cannot do InPlace function.

	//By default CanRunInPlace checks whether the input and output image type match.
	switch (target)
	{
	case REGISTER_RAW_CBCT:
		m_spRawReconImg = tmpReconImg; //Checked.. successfully alive.
		m_spCrntReconImg = m_spRawReconImg;
		break;
	case REGISTER_COR_CBCT:
		m_spScatCorrReconImg = tmpReconImg; //Checked.. successfully alive.
		m_spCrntReconImg = m_spScatCorrReconImg;
		break;
	}
	QString outputFilePath = ui.lineEdit_OutputFilePath->text();

	QFileInfo outFileInfo(outputFilePath);
	QDir outFileDir = outFileInfo.absoluteDir();

	if (outputFilePath.length() < 2 || !outFileDir.exists())
	{
		cout << "No available output path. Should be exported later" << endl;
		ui.lineEdit_Cur3DFileName->setText("FDK-reconstructed volume");
	}
	else
	{
		typedef itk::ImageFileWriter<USHORT_ImageType> WriterType;
		WriterType::Pointer writer = WriterType::New();
		writer->SetFileName(outputFilePath.toLocal8Bit().constData());
		writer->SetUseCompression(true); //not exist in original code (rtkfdk)
		writer->SetInput(m_spCrntReconImg);

		cout << "Writing the image to: " << outputFilePath.toLocal8Bit().constData() << endl;

		TRY_AND_EXIT_ON_ITK_EXCEPTION(writer->Update());

		ui.lineEdit_Cur3DFileName->setText(outputFilePath);
		std::cout << std::endl;
		std::cout << "Output generation was succeeded" << std::endl;
	}

	static_cast<FDKCUDAType*>(CUDAfeldkamp.GetPointer())->PrintTiming(std::cout);


	m_dspYKReconImage->CreateImage(size_trans[0], size_trans[1], 0);

	ui.spinBoxReconImgSliceNo->setMinimum(0);
	ui.spinBoxReconImgSliceNo->setMaximum(size_trans[2] - 1);
	ui.spinBoxReconImgSliceNo->setValue(qRound(size_trans[2] / 2.0)); //DrawReconImage is called automatically

	//SLT_ViewRegistration();
	if (target == REGISTER_COR_CBCT)
	{
		UpdateReconImage(m_spCrntReconImg, QString("SCATTER_COR_CBCT"));
	}
	else if (target == REGISTER_RAW_CBCT)
	{
		UpdateReconImage(m_spCrntReconImg, QString("RAW_CBCT"));
	}

	typedef itk::CastImageFilter <CUDAOutputImageType, OutputImageType > castBackType;
	castBackType::Pointer castBack = castBackType::New();
	castBack->SetInput(cuda_spProjImg3DFloat);
	castBack->Update();
	OutputImageType::Pointer m_spProjImg3DFloat = castBack->GetOutput();

	cout << "FINISHED!: FDK CBCT reconstruction" << endl;
	//if not found, just skip

	//SLT_DrawGraph();
	//2) Load Geometry file.
	//3) Prepare all parameters from GUI components
}
#else
void CbctRecon::CudaDoReconstructionFDK(enREGI_IMAGES target)
{
	cout << "This program were not compiled with the CUDA option, please select OpenCL or CPU instead!" << endl;
	return;
}
#endif

void CbctRecon::DoReconstructionFDK(enREGI_IMAGES target)
{
	//Hardware Type
	const char* strHardware = "";

	if (ui.radioButton_UseCPU->isChecked())
	{
		strHardware = "cpu";
	}
	else if (ui.radioButton_UseOpenCL->isChecked())
	{
		strHardware = "opencl";
#if OPENCL_FOUND
		typedef itk::Image< FloatPixelType, 3 > OutputImageType; // sorry, but why not cuda?
		OutputImageType::Pointer m_spProjImg3DFloat; //This is float image loaded by RTK. line integral (mu_t value). To convert this to Intensity, use mu t = ln(65535/I)
#else
		std::cerr << "The program has not been compiled with opencl option" << std::endl;
		return;
#endif
	}
  // typedef itk::Image< FloatPixelType, 3 > OutputImageType; // sorry, but why not cuda?
  // OutputImageType::Pointer m_spProjImg3DFloat; //This is float image loaded by RTK. line integral (mu_t value). To convert this to Intensity, use mu t = ln(65535/I)

  if (!m_spProjImg3DFloat)
  {
	cout << "processed Projection image is not ready yet" << endl;
	return;
  }
  //Resampling first --> to save the recon time. 1024 --> 512

  //cout << m_spProjImg3DFloat->GetRequestedRegion().GetSize() << endl;
  //cout << spTmpImage->GetRequestedRegion().GetSize() << endl;
  //return;
  typedef itk::ImageDuplicator< OutputImageType > DuplicatorType;
  DuplicatorType::Pointer duplicator = DuplicatorType::New();
  duplicator->SetInputImage(m_spProjImg3DFloat);
  duplicator->Update();

  OutputImageType::Pointer spCurImg = duplicator->GetOutput(); //already down sampled
	//OutputImageType::Pointer spCurImg = m_spProjImg3D;
	//spCurImg = duplicator->GetOutput();

	//ResampleItkImage(m_spProjImg3DFloat, spCurImg, resampleFactor);

	//Displaced detector weighting // set pipeline //inplace filter
	typedef rtk::DisplacedDetectorImageFilter< OutputImageType > DDFType;
	DDFType::Pointer ddf = DDFType::New();

	if (ui.checkBox_UseDDF->isChecked())
	{
		ddf->SetInput(spCurImg);
		//ddf->SetGeometry( geometryReader->GetOutputObject() );
		ddf->SetGeometry( m_spCustomGeometry );
		cout << "DDF was set in pipeline" << endl;

		if (ui.checkBox_UpdateAfterFiltering->isChecked())
			ddf->Update();//no mememory increas: InPlace Filter

		spCurImg = ddf->GetOutput();
	}
	typedef rtk::ParkerShortScanImageFilter< OutputImageType > PSSFType;
	PSSFType::Pointer pssf = PSSFType::New();
	

	if (ui.checkBox_UsePSSF->isChecked())
	{
		// Short scan image filter
		pssf->SetInput( spCurImg );
		//pssf->SetGeometry( geometryReader->GetOutputObject() );
		pssf->SetGeometry( m_spCustomGeometry );
		//pssf->InPlaceOff(); //YKComments: Do not overwrite input image buffer for output
		cout << "short scan image filter success" << endl;

		if (ui.checkBox_UpdateAfterFiltering->isChecked())
			pssf->Update(); //no mememory increas: InPlace Filter

		spCurImg = pssf->GetOutput();
	}

	//Just Before going to FDK recon,
	//Update Projection data and delete old data.

	//Let's duplicate this
	//Original m_spProjImg3D will be deleted after update
	
	if (ui.checkBox_UpdateAfterFiltering->isChecked())
	{
		typedef itk::ImageDuplicator< OutputImageType > DuplicatorType;
		DuplicatorType::Pointer duplicator = DuplicatorType::New();
		duplicator->SetInputImage(spCurImg);
		duplicator->Update();
		m_spProjImg3DFloat = duplicator->GetOutput();

		SetMaxAndMinValueOfProjectionImage(); // scan m_spProjImg3D and update m_fProjImgValueMin, max
		SLT_DrawProjImages();
	}


	// Generate image sources for cone beam CT reconstruction
	typedef rtk::ConstantImageSource< OutputImageType > ConstantImageSourceType;

	ConstantImageSourceType::PointType origin;
	ConstantImageSourceType::SpacingType spacing;
	ConstantImageSourceType::SizeType sizeOutput;

	sizeOutput[0] = ui.lineEdit_outImgDim_AP->text().toInt(); //pixel
	sizeOutput[1] = ui.lineEdit_outImgDim_SI->text().toInt();  //Caution!: direction is different in NKI SCAN FIle
	sizeOutput[2] = ui.lineEdit_outImgDim_LR->text().toInt();

	spacing[0] = ui.lineEdit_outImgSp_AP->text().toDouble();
	spacing[1] = ui.lineEdit_outImgSp_SI->text().toDouble();
	spacing[2] = ui.lineEdit_outImgSp_LR->text().toDouble();

	origin[0] = -0.5*sizeOutput[0]*spacing[0]; //Y in DCM?
	origin[1] = -0.5*sizeOutput[1]*spacing[1];  //Z in DCM?
	origin[2] = -0.5*sizeOutput[2]*spacing[2]; //X in DCM?

	ConstantImageSourceType::Pointer constantImageSource = ConstantImageSourceType::New();
	constantImageSource->SetOrigin( origin );
	constantImageSource->SetSpacing( spacing );
	constantImageSource->SetSize( sizeOutput );
	constantImageSource->SetConstant( 0.0 );  //initial value

	double fTruncCorFactor = ui.lineEdit_Ramp_TruncationCorrection->text().toDouble();
	double fHannCut = ui.lineEdit_Ramp_HannCut->text().toDouble();
	double fCosineCut = ui.lineEdit_Ramp_CosineCut->text().toDouble();
	double fHamming = ui.lineEdit_Ramp_Hamming->text().toDouble();
	double fHannCutY = ui.lineEdit_Ramp_HannCutY->text().toDouble();
	double fRamLak = ui.lineEdit_Ramp_RamLakCut->text().toDouble();

	if (fTruncCorFactor > 0.0 && target == REGISTER_COR_CBCT)
        {
            cout << "Warning! Truncation factor is " << fTruncCorFactor << ". Regardless of previous setting, this factor should not be 0 for scatter corrected CBCT. Now zero value is applied." << endl;
			fTruncCorFactor = 0.0;
			if (ui.checkBox_UseTrunc->isChecked())
				fTruncCorFactor = ui.lineEdit_Ramp_TruncationCorrection->text().toDouble();
        }

	// This macro sets options for fdk filter which I can not see how to do better
	// because TFFTPrecision is not the same, e.g. for CPU and CUDA (SR)
#define SET_FELDKAMP_OPTIONS(f) \
	f->SetInput( 0, constantImageSource->GetOutput() ); \
	f->SetInput( 1, spCurImg ); \
	f->SetGeometry( m_spCustomGeometry ); \
	f->GetRampFilter()->SetTruncationCorrection(fTruncCorFactor); \
	f->GetRampFilter()->SetHannCutFrequency(fHannCut); \
	f->GetRampFilter()->SetCosineCutFrequency(fCosineCut); \
	f->GetRampFilter()->SetHammingFrequency(fHamming); \
	f->GetRampFilter()->SetHannCutFrequencyY(fHannCutY); \
	f->GetRampFilter()->SetRamLakCutFrequency(fRamLak);

	

	// FDK reconstruction filtering
	itk::ImageToImageFilter<OutputImageType, OutputImageType>::Pointer feldkamp;
	typedef rtk::FDKConeBeamReconstructionFilter< OutputImageType > FDKCPUType;

#if OPENCL_FOUND
	typedef rtk::OpenCLFDKConeBeamReconstructionFilter              FDKOPENCLType;
#endif

	if(!strcmp(strHardware, "cpu") )
	{
		feldkamp = FDKCPUType::New();
		SET_FELDKAMP_OPTIONS( dynamic_cast<FDKCPUType*>(feldkamp.GetPointer()) );

		//// Motion compensated CBCT settings
		//if(args_info.signal_given && args_info.dvf_given)
		//{
		// dvfReader->SetFileName(args_info.dvf_arg);
		// def->SetSignalFilename(args_info.signal_arg);
		// dynamic_cast<FDKCPUType*>(feldkamp.GetPointer())->SetBackProjectionFilter( bp.GetPointer() );
		//}
	}
	
	else if(!strcmp(strHardware, "opencl") )
	{
#if OPENCL_FOUND
		feldkamp = FDKOPENCLType::New();
		SET_FELDKAMP_OPTIONS( static_cast<FDKOPENCLType*>(feldkamp.GetPointer()) );
#else
		std::cerr << "The program has not been compiled with opencl option" << std::endl;
		return;
#endif
	}
	typedef rtk::FieldOfViewImageFilter <OutputImageType, OutputImageType> FOVFilterType;
	FOVFilterType::Pointer fieldofviewFilter = FOVFilterType::New();
	if (ui.checkBox_UpdateAfterFiltering->isChecked())
	{
		feldkamp->Update();
		cout << "FOV filter is being added.." << endl;
		fieldofviewFilter->SetInput(0, feldkamp->GetOutput());
		fieldofviewFilter->SetGeometry(m_spCustomGeometry);
		if (!ui.checkBox_UseDDF->isChecked()) 
		{
			fieldofviewFilter->SetDisplacedDetector(TRUE);
		}
		fieldofviewFilter->SetProjectionsStack(duplicator->GetOutput());
		fieldofviewFilter->Update();
		
	}
	
	cout << "Cone beam reconstruction pipeline is ready" << endl;

	// Streaming depending on streaming capability of writer --> not affect the calc. speed

	typedef itk::StreamingImageFilter<OutputImageType, OutputImageType> StreamerType;
	StreamerType::Pointer streamerBP = StreamerType::New();
	
	if (ui.checkBox_UpdateAfterFiltering->isChecked())
	{
		streamerBP->SetInput(fieldofviewFilter->GetOutput());
	}
	else
	{
		streamerBP->SetInput(feldkamp->GetOutput());
	}
	streamerBP->SetNumberOfStreamDivisions(4); // YK: 1 in example code from "rtkfdk" //AG: stated in test: 4 for ITK MAJOR >= 4

	cout << "Euler 3D Transformation: from RTK-procuded volume to standard DICOM coordinate" << endl;

	/* RTK-produced 3D Volume should be changed in coordination of itk */
	/* Coordination transformation using Euler 3D transformation */

	// 1) Prepare Canvas parameter
	//OutputImageType::Pointer fixedImg = OutputImageType::New();
	//start index: What is the index of Left Top Inferior corner in DICOM coordinate?


	//Same image type from original image -3D & float
	OutputImageType::IndexType start_trans;
	start_trans[0] = 0;
	start_trans[1] = 0;
	start_trans[2] = 0;

	OutputImageType::SizeType size_trans;
	size_trans[0] = sizeOutput[0]; // X //410
	size_trans[1] = sizeOutput[2]; //Y  // 410
	size_trans[2] = sizeOutput[1]; //Z // 120?

	OutputImageType::SpacingType spacing_trans;
	spacing_trans[0] = spacing[0];
	spacing_trans[1] = spacing[2];
	spacing_trans[2] = spacing[1];

	OutputImageType::PointType Origin_trans;
	Origin_trans[0] = -0.5* size_trans[0]*spacing_trans[0];
	Origin_trans[1] = -0.5* size_trans[1]*spacing_trans[1];
	Origin_trans[2] = -0.5* size_trans[2]*spacing_trans[2];

	OutputImageType::RegionType region_trans;
	region_trans.SetSize(size_trans);
	region_trans.SetIndex(start_trans);

	/* 2) Prepare Target image */
	OutputImageType::Pointer targetImg = streamerBP->GetOutput();

	/* 3) Configure transform */
	typedef itk::Euler3DTransform< double > TransformType;
	TransformType::Pointer transform = TransformType::New();

	TransformType::ParametersType param;
	param.SetSize(6);
	//MAXIMUM PARAM NUMBER: 6!!!
	//if (target == REGISTER_COR_CBCT){
	//	param.put(0, 0.0);
	//}
	//else{
	param.put(0, 0.0); //rot X // 0.5 = PI/2 //Actuly is  rot Z ELEKTA VS VARIAN <-- THIS MIGHT BE THE SOLUTION
	//}
	param.put(1, itk::Math::pi_over_2);//rot Y //Actuly is rot Y
	param.put(2, -itk::Math::pi_over_2);//rot Z //Actuly is rot X
	param.put(3, 0.0); // Trans X mm
	param.put(4, 0.0); // Trans Y mm
	param.put(5, 0.0); // Trans Z mm

	TransformType::ParametersType fixedParam(3); //rotation center
	fixedParam.put(0,0);
	fixedParam.put(1,0);
	fixedParam.put(2,0);

	transform->SetParameters(param);
	transform->SetFixedParameters(fixedParam); //Center of the Transform

	cout << "Transform matrix:" << "	" << endl;
	cout << transform->GetMatrix() << std::endl;

	typedef itk::ResampleImageFilter<OutputImageType, OutputImageType> ResampleFilterType;
	ResampleFilterType::Pointer resampler = ResampleFilterType::New();
	//OutputImageType::RegionType fixedImg_Region = fixedImg->GetLargestPossibleRegion().GetSize();

	resampler->SetInput(targetImg);
	resampler->SetSize(size_trans);
	resampler->SetOutputOrigin( Origin_trans); //Lt Top Inf of Large Canvas
	resampler->SetOutputSpacing( spacing_trans ); // 1 1 1
	resampler->SetOutputDirection( targetImg->GetDirection() ); //image normal?
	resampler->SetTransform(transform);
	//resampler->Update();//yktemp Error 2
	typedef itk::FlipImageFilter< OutputImageType >  FilterType;
	FilterType::Pointer flipFilter = FilterType::New();

	cout << "Flip filter is being applied" << endl;
	typedef FilterType::FlipAxesArrayType FlipAxesArrayType;
	FlipAxesArrayType arrFlipAxes;
	arrFlipAxes[0] = 1;
	arrFlipAxes[1] = 0;
	arrFlipAxes[2] = 0;
	flipFilter->SetFlipAxes(arrFlipAxes);
	flipFilter->SetInput(resampler->GetOutput());
	
	/*OutputImageType::Pointer floatImg = flipFilter->GetOutput();*/
	//const unsigned int Dimension = 3;
	//FinalImageType::Pointer finalImg ;
	
	typedef itk::AbsImageFilter<OutputImageType, OutputImageType> AbsImageFilterType;
	AbsImageFilterType::Pointer absImgFilter = AbsImageFilterType::New();
	absImgFilter->SetInput(flipFilter->GetOutput()); // 20140206 modified it was a bug
	//absImgFilter->SetInput(resampler->GetOutput());

	typedef itk::MultiplyImageFilter<OutputImageType, OutputImageType, OutputImageType> MultiplyImageFilterType;
	
	MultiplyImageFilterType::Pointer multiplyImageFilter = MultiplyImageFilterType::New();
	multiplyImageFilter->SetInput(absImgFilter->GetOutput());
	multiplyImageFilter->SetConstant(65536); //calculated already
	
	
	//typedef unsigned short FinalPixelType;
	//typedef itk::Image< FinalPixelType, 3 > FinalImageType;
	typedef itk::CastImageFilter< OutputImageType, USHORT_ImageType > CastFilterType;
	CastFilterType::Pointer castFilter = CastFilterType::New();
	/*Total Variation Filter*/
	if (ui.checkBox_TV->isChecked()){
		/*To check TV is doing its job:*/
		typedef itk::StatisticsImageFilter < OutputImageType > MeanAlgorithmType;
		MeanAlgorithmType::Pointer meanAlgorithm = MeanAlgorithmType::New();
		meanAlgorithm->SetInput(multiplyImageFilter->GetOutput());
		meanAlgorithm->Update();
		std::cout << "Sample mean = " << meanAlgorithm->GetMean() << std::endl;
		meanAlgorithm->ResetPipeline(); // Make sure not to get trivial output after TV filter.

		typedef rtk::TotalVariationImageFilter < OutputImageType > TotalVariationFilterType;
		TotalVariationFilterType::Pointer tv = TotalVariationFilterType::New();
		double totalVariationBefore;
		double totalVariationAfter;
		tv->SetInput(multiplyImageFilter->GetOutput());
		tv->Update();
		totalVariationBefore = tv->GetTotalVariation();
		std::cout << "Total variation before denoising is " << totalVariationBefore << std::endl;

		/*Now GO:*/
		typedef rtk::TotalVariationDenoisingBPDQImageFilter<OutputImageType, GradientOutputImageType>  tvFilterType;
		
		tvFilterType::Pointer tvFilter = tvFilterType::New();
		cout << "Post total variation filtering is in the pipeline.." << endl;

		tvFilter->SetGamma(ui.lineEdit_TVgamma->text().toDouble());
		tvFilter->SetNumberOfIterations(ui.lineEdit_TV_iter->text().toInt());
		tvFilter->SetInput(multiplyImageFilter->GetOutput());
		tvFilter->Update(); 

		meanAlgorithm->SetInput(tvFilter->GetOutput());
		meanAlgorithm->Update();
		std::cout << "Sample mean = " << meanAlgorithm->GetMean() << std::endl;
		tv->SetInput(tvFilter->GetOutput());
		tv->Update();
		totalVariationAfter = tv->GetTotalVariation();
		std::cout << "Total variation after denoising is " << totalVariationAfter << std::endl;
		typedef itk::MultiplyImageFilter<OutputImageType, OutputImageType, OutputImageType> MultiplyImageFilterType;
		MultiplyImageFilterType::Pointer multiplyImageFilter = MultiplyImageFilterType::New();
		multiplyImageFilter->SetInput(tvFilter->GetOutput());
		if (ui.checkBox_Magic->isChecked()){
			multiplyImageFilter->SetConstant(1. / 0.9); // needed because m_beta, which is defined in in the TV denoising filter
		}
		else
		{
			multiplyImageFilter->SetConstant(0.9409); // needed because m_beta, which is defined in in the TV denoising filter.. the 0.0409 is a calculated guess, if it works, the factor will be investigated further..
		}
		

		castFilter->SetInput(multiplyImageFilter->GetOutput());
		
		castFilter->Update(); 
	}
	else
	{
		castFilter->SetInput(multiplyImageFilter->GetOutput());
		
		castFilter->Update(); //YK20150109
	}


	USHORT_ImageType::Pointer tmpReconImg;
	//if all 0 0 0 don't do the median filtering

	itk::TimeProbe reconTimeProbe;
	reconTimeProbe.Start();

	cout << "Reconstructing the image.. please wait..." << endl;
	USHORT_ImageType::SizeType indexRadius;
	indexRadius[0] = ui.lineEdit_PostMedSizeX->text().toInt(); // radius along x
	indexRadius[1] = ui.lineEdit_PostMedSizeY->text().toInt(); // radius along y
	indexRadius[2] = ui.lineEdit_PostMedSizeZ->text().toInt(); // radius along y
	if (ui.checkBox_PostMedianOn->isChecked() && (indexRadius[0] != 0 || indexRadius[1] != 0 || indexRadius[2] != 0))
	{
		typedef itk::MedianImageFilter<USHORT_ImageType, USHORT_ImageType >  FilterType;
		FilterType::Pointer medFilter = FilterType::New();

	  //YKTEMP20141218 S
	 // typedef itk::MeanImageFilter<USHORT_ImageType, USHORT_ImageType >  FilterType;
	 // FilterType::Pointer medFilter = FilterType::New();
	  //YKTEMP20141218 E

	  //this is radius. 1 --> median window 3
		cout << "Post median(3D) filtering is in the pipeline..Size(radius X Y Z) is = " << indexRadius<< endl;

		medFilter->SetRadius( indexRadius );
		medFilter->SetInput(castFilter->GetOutput());
		medFilter->Update(); //Error here!g
		tmpReconImg = medFilter->GetOutput();
	  //cout << "median filtering has been done" << endl;
	}
	else 
	{
		tmpReconImg = castFilter->GetOutput();
	}
	
	typedef itk::ThresholdImageFilter <USHORT_ImageType> ThresholdImageFilterType;
	ThresholdImageFilterType::Pointer thresholdFilterAbove = ThresholdImageFilterType::New();
	thresholdFilterAbove->SetInput(tmpReconImg);
	thresholdFilterAbove->ThresholdAbove(4095);
	thresholdFilterAbove->SetOutsideValue(4095);
	
	ThresholdImageFilterType::Pointer thresholdFilterBelow = ThresholdImageFilterType::New();
	thresholdFilterBelow->SetInput(thresholdFilterAbove->GetOutput());
	thresholdFilterBelow->ThresholdBelow(0);
	thresholdFilterBelow->SetOutsideValue(0);
	thresholdFilterBelow->Update();
	tmpReconImg = thresholdFilterBelow->GetOutput();

	cout << "After Filtering" << endl;

	reconTimeProbe.Stop();
	std::cout << "It took " << reconTimeProbe.GetMean() << ' ' << reconTimeProbe.GetUnit() << std::endl;
	ui.lineEdit_ReconstructionTime->setText(QString("%1").arg(reconTimeProbe.GetMean()));

	//cout << "TestYK" << endl;
	//cout << tmpReconImg->GetRequestedRegion().GetSize() << endl;
	//cout << "Before InPlace Off: " <<  castFilter->GetInPlace() << endl;	//0
	//castFilter->InPlaceOff();
	//cout << "After InPlace Off: " <<  castFilter->GetInPlace() << endl; //0
	//Because Input Output format are different, this filter cannot do InPlace function.

	//By default CanRunInPlace checks whether the input and output image type match.
	switch (target)
	{
	case REGISTER_RAW_CBCT:
	  m_spRawReconImg = tmpReconImg; //Checked.. successfully alive.
	  m_spCrntReconImg = m_spRawReconImg;
	  break;
	case REGISTER_COR_CBCT:
	  m_spScatCorrReconImg = tmpReconImg; //Checked.. successfully alive.
	  m_spCrntReconImg = m_spScatCorrReconImg;
	  break;
	}
	QString outputFilePath = ui.lineEdit_OutputFilePath->text();

	QFileInfo outFileInfo(outputFilePath);
	QDir outFileDir = outFileInfo.absoluteDir();

	if (outputFilePath.length() < 2 || !outFileDir.exists())
	{
	  cout << "No available output path. Should be exported later" << endl;
	  ui.lineEdit_Cur3DFileName->setText("FDK-reconstructed volume");
	}
	else
	{
	  typedef itk::ImageFileWriter<USHORT_ImageType> WriterType;
	  WriterType::Pointer writer = WriterType::New();
	  writer->SetFileName(outputFilePath.toLocal8Bit().constData());
	  writer->SetUseCompression(true); //not exist in original code (rtkfdk)
	  writer->SetInput(m_spCrntReconImg);

	  cout << "Writing the image to: " << outputFilePath.toLocal8Bit().constData() << endl;

	  TRY_AND_EXIT_ON_ITK_EXCEPTION(writer->Update());

	  ui.lineEdit_Cur3DFileName->setText(outputFilePath);
	  std::cout << std::endl;
	  std::cout << "Output generation was succeeded" << std::endl;
	}

	if(!strcmp(strHardware, "cpu") )
		static_cast<FDKCPUType* >(feldkamp.GetPointer())->PrintTiming(std::cout);
#if OPENCL_FOUND
	else if(!strcmp(strHardware, "opencl") )
		static_cast<FDKOPENCLType*>(feldkamp.GetPointer())->PrintTiming(std::cout);
#endif

	m_dspYKReconImage->CreateImage(size_trans[0], size_trans[1],0);

	ui.spinBoxReconImgSliceNo->setMinimum(0);
	ui.spinBoxReconImgSliceNo->setMaximum(size_trans[2]-1);
	ui.spinBoxReconImgSliceNo->setValue(qRound(size_trans[2]/2.0)); //DrawReconImage is called automatically

	//For 2D image display
	//SLT_DrawReconImage();
	//m_spReconImg = writer->GetOutput();

	/*OutputImageType::SizeType AfterReconSize = m_spProjImg3D->GetBufferedRegion().GetSize();
	cout << "AfterReconSize  " << AfterReconSize[0] << "	"
		<< AfterReconSize[1] << "	"
		<< AfterReconSize[2] << endl;*/

	/*ui.radioButton_graph_recon->setChecked(true);
	SLT_InitializeGraphLim();
	SLT_DrawReconImage();	*/
	

	//SLT_ViewRegistration();
	if (target == REGISTER_COR_CBCT)
	{
	  UpdateReconImage(m_spCrntReconImg, QString("SCATTER_COR_CBCT"));
	}
	else if (target == REGISTER_RAW_CBCT)
	{
	  UpdateReconImage(m_spCrntReconImg, QString("RAW_CBCT"));
	}

	cout << "FINISHED!: FDK CBCT reconstruction" << endl;
	//if not found, just skip
	
	//SLT_DrawGraph();
	//2) Load Geometry file.
	//3) Prepare all parameters from GUI components
}

#ifdef CUDA_FOUND
void CbctRecon::DoReconstructionTV(enREGI_IMAGES target)
{
	cout << "You shouldn't use the CPU for iterative when compiled on cuda option!" << endl;
}
#else
void CbctRecon::DoReconstructionTV(enREGI_IMAGES target)
{
	//Hardware Type
	if (!ui.radioButton_UseCPU->isChecked())
		cout << "no OpenCL version of Total Variation Cone Beam Reconstruction available -> using CPU!" << endl;

	if (!m_spProjImg3DFloat)
	{
		cout << "processed Projection image is not ready yet" << endl;
		return;
	}
	//Resampling first --> to save the recon time. 1024 --> 512

	typedef itk::ImageDuplicator< OutputImageType > DuplicatorType;
	DuplicatorType::Pointer duplicator = DuplicatorType::New();
	duplicator->SetInputImage(m_spProjImg3DFloat);
	duplicator->Update();

	OutputImageType::Pointer spCurImg = duplicator->GetOutput(); //already down sampled

	//Displaced detector weighting // set pipeline //inplace filter
	typedef rtk::DisplacedDetectorImageFilter< OutputImageType > DDFType;
	DDFType::Pointer ddf = DDFType::New();
	if (ui.checkBox_UseDDF->isChecked())
	{
		ddf->SetInput(spCurImg);
		//ddf->SetGeometry( geometryReader->GetOutputObject() );
		ddf->SetGeometry(m_spCustomGeometry);
		cout << "DDF was set in pipeline" << endl;

		if (ui.checkBox_UpdateAfterFiltering->isChecked())
			ddf->Update();//no mememory increas: InPlace Filter

		spCurImg = ddf->GetOutput();
	}

	typedef rtk::ParkerShortScanImageFilter< OutputImageType > PSSFType;
	PSSFType::Pointer pssf = PSSFType::New();
	if (ui.checkBox_UsePSSF->isChecked())
	{
		// Short scan image filter
		pssf->SetInput(spCurImg);
		pssf->SetGeometry(m_spCustomGeometry);
		cout << "short scan image filter success" << endl;

		if (ui.checkBox_UpdateAfterFiltering->isChecked())
			pssf->Update(); //no mememory increas: InPlace Filter

		spCurImg = pssf->GetOutput();
	}

	if (ui.checkBox_UpdateAfterFiltering->isChecked())
	{
		typedef itk::ImageDuplicator< OutputImageType > DuplicatorType;
		DuplicatorType::Pointer duplicator = DuplicatorType::New();
		duplicator->SetInputImage(spCurImg);
		duplicator->Update();
		m_spProjImg3DFloat = duplicator->GetOutput();

		SetMaxAndMinValueOfProjectionImage(); // scan m_spProjImg3D and update m_fProjImgValueMin, max
		SLT_DrawProjImages();
	}


	// Generate image sources for cone beam CT reconstruction
	typedef rtk::ConstantImageSource< OutputImageType > ConstantImageSourceType;

	ConstantImageSourceType::PointType origin;
	ConstantImageSourceType::SpacingType spacing;
	ConstantImageSourceType::SizeType sizeOutput;

	sizeOutput[0] = ui.lineEdit_outImgDim_AP->text().toInt(); //pixel
	sizeOutput[1] = ui.lineEdit_outImgDim_SI->text().toInt();  //Caution!: direction is different in NKI SCAN FIle
	sizeOutput[2] = ui.lineEdit_outImgDim_LR->text().toInt();

	spacing[0] = ui.lineEdit_outImgSp_AP->text().toDouble();
	spacing[1] = ui.lineEdit_outImgSp_SI->text().toDouble();
	spacing[2] = ui.lineEdit_outImgSp_LR->text().toDouble();

	origin[0] = -0.5*sizeOutput[0] * spacing[0]; //Y in DCM?
	origin[1] = -0.5*sizeOutput[1] * spacing[1]; //Z in DCM?
	origin[2] = -0.5*sizeOutput[2] * spacing[2]; //X in DCM?

	ConstantImageSourceType::Pointer constantImageSource = ConstantImageSourceType::New();
	constantImageSource->SetOrigin(origin);
	constantImageSource->SetSpacing(spacing);
	constantImageSource->SetSize(sizeOutput);
	constantImageSource->SetConstant(0.0);  //initial value
	

	float Beta = 1000; // ui.lineEdit_TVbeta->text().toFloat();
	float Alpha = 10; //ui.lineEdit_TValpha->text().toFloat();
	float AL_iter = 3; //ui.lineEdit_AL_iter->text().toUInt();
	float CG_iter = 3; //ui.lineEdit_CG_iter->text().toUInt();


	cout << "Admm options are being set.." << endl;
	/*
	// ADMMTotalVariation reconstruction filtering
	typedef rtk::ADMMWaveletsConeBeamReconstructionFilter
		<OutputImageType> ADMMType;
	ADMMType::Pointer admm = ADMMType::New();
	admm->SetInput( 0, constantImageSource->GetOutput()); // buffered region of [0, 0, 0]?
	admm->SetInput(1, spCurImg);
	admm->SetGeometry( m_spCustomGeometry );
	admm->SetAlpha( Alpha );
	admm->SetBeta( Beta );
	admm->SetAL_iterations( AL_iter );
	admm->SetCG_iterations( CG_iter );
	admm->SetNumberOfLevels( 3 ); 
	admm->SetOrder( 3 );
	admm->SetForwardProjectionFilter( 0 );
	admm->SetBackProjectionFilter( 0 );
	*/
	/*
	typedef rtk::ConjugateGradientConeBeamReconstructionFilter <OutputImageType> IterReconType;
	IterReconType::Pointer iterRecon = IterReconType::New();
	iterRecon->SetInput(0, constantImageSource->GetOutput());
	iterRecon->SetInput(1, spCurImg);
	iterRecon->SetGeometry(m_spCustomGeometry);
	iterRecon->SetNumberOfIterations(10);
	// In all cases, use the Joseph forward projector
	iterRecon->SetForwardProjectionFilter(0);
	ConstantImageSourceType::Pointer uniformWeightsSource = ConstantImageSourceType::New();
	uniformWeightsSource->SetInformationFromImage(spCurImg);
	uniformWeightsSource->SetConstant(1.0);
	iterRecon->SetBackProjectionFilter(0);
	iterRecon->SetInput(2, uniformWeightsSource->GetOutput());
	*/
	typedef rtk::SARTConeBeamReconstructionFilter <OutputImageType, OutputImageType> ReconType;
	ReconType::Pointer Recon = ReconType::New();
	Recon->SetInput(0, constantImageSource->GetOutput());
	Recon->SetInput(1, spCurImg);
	Recon->SetGeometry(m_spCustomGeometry);
	Recon->SetLambda(0.3); //Default value
	Recon->SetNumberOfIterations(10); //Default = 3
	Recon->SetNumberOfProjectionsPerSubset(1); // default value
	Recon->SetBackProjectionFilter(1); // backProj=0, Joseph=1, Cuda=2, normJoseph=3, CudaRay=4
	Recon->SetForwardProjectionFilter(0); // Joseph=0, RayCast=1, Cuda=2

	typedef rtk::FieldOfViewImageFilter <OutputImageType, OutputImageType> FOVFilterType;
	FOVFilterType::Pointer fieldofviewFilter = FOVFilterType::New();
	if (ui.checkBox_UpdateAfterFiltering->isChecked())
	{
		cout << "Wait for iterative reconstruction to finish.." << endl;
		try
		{
			Recon->Update();
		}
		catch (itk::ExceptionObject & e)
		{
			cout << e.what() << ", error: " << e << endl;
			system("pause");
			cerr << e << endl;
		}
		cout << "FOV filter is being added.." << endl;
		fieldofviewFilter->SetInput(0, Recon->GetOutput());
		fieldofviewFilter->SetGeometry(m_spCustomGeometry);
		if (!ui.checkBox_UseDDF->isChecked())
		{
			fieldofviewFilter->SetDisplacedDetector(TRUE);
		}
		fieldofviewFilter->SetProjectionsStack(duplicator->GetOutput());
		fieldofviewFilter->Update();

	}

	cout << "Cone beam reconstruction pipeline is ready" << endl;

	// Streaming depending on streaming capability of writer --> not affect the calc. speed

	typedef itk::StreamingImageFilter<OutputImageType, OutputImageType> StreamerType;
	StreamerType::Pointer streamerBP = StreamerType::New();

	if (ui.checkBox_UpdateAfterFiltering->isChecked())
	{
		streamerBP->SetInput(fieldofviewFilter->GetOutput());
	}
	else
	{
		streamerBP->SetInput(Recon->GetOutput());
	}
	streamerBP->SetNumberOfStreamDivisions(4); // YK: 1 in example code from "rtkfdk" //AG: stated in test: 4 for ITK MAJOR >= 4

	cout << "Euler 3D Transformation: from RTK-procuded volume to standard DICOM coordinate" << endl;
	//Same image type from original image -3D & float
	OutputImageType::IndexType start_trans;
	start_trans[0] = 0;
	start_trans[1] = 0;
	start_trans[2] = 0;

	OutputImageType::SizeType size_trans;
	size_trans[0] = sizeOutput[0]; // X //410
	size_trans[1] = sizeOutput[2]; //Y  // 410
	size_trans[2] = sizeOutput[1]; //Z // 120?

	OutputImageType::SpacingType spacing_trans;
	spacing_trans[0] = spacing[0];
	spacing_trans[1] = spacing[2];
	spacing_trans[2] = spacing[1];

	OutputImageType::PointType Origin_trans;
	Origin_trans[0] = -0.5* size_trans[0] * spacing_trans[0];
	Origin_trans[1] = -0.5* size_trans[1] * spacing_trans[1];
	Origin_trans[2] = -0.5* size_trans[2] * spacing_trans[2];

	OutputImageType::RegionType region_trans;
	region_trans.SetSize(size_trans);
	region_trans.SetIndex(start_trans);

	/* 2) Prepare Target image */
	OutputImageType::Pointer targetImg = streamerBP->GetOutput();

	/* 3) Configure transform */
	typedef itk::Euler3DTransform< double > TransformType;
	TransformType::Pointer transform = TransformType::New();

	TransformType::ParametersType param;
	param.SetSize(6);
	param.put(0, 0.0); //rot X // 0.5 = PI/2 //Actuly is  rot Z ELEKTA VS VARIAN <-- THIS MIGHT BE THE SOLUTION
	param.put(1, itk::Math::pi_over_2);//rot Y //Actuly is rot Y
	param.put(2, -itk::Math::pi_over_2);//rot Z //Actuly is rot X
	param.put(3, 0.0); // Trans X mm
	param.put(4, 0.0); // Trans Y mm
	param.put(5, 0.0); // Trans Z mm

	TransformType::ParametersType fixedParam(3); //rotation center
	fixedParam.put(0, 0);
	fixedParam.put(1, 0);
	fixedParam.put(2, 0);

	transform->SetParameters(param);
	transform->SetFixedParameters(fixedParam); //Center of the Transform

	cout << "Transform matrix:" << "	" << endl;
	cout << transform->GetMatrix() << std::endl;

	typedef itk::ResampleImageFilter<OutputImageType, OutputImageType> ResampleFilterType;
	ResampleFilterType::Pointer resampler = ResampleFilterType::New();
	resampler->SetInput(targetImg);
	resampler->SetSize(size_trans);
	resampler->SetOutputOrigin(Origin_trans); //Lt Top Inf of Large Canvas
	resampler->SetOutputSpacing(spacing_trans); // 1 1 1
	resampler->SetOutputDirection(targetImg->GetDirection()); //image normal?
	resampler->SetTransform(transform);

	typedef itk::FlipImageFilter< OutputImageType >  FilterType;
	FilterType::Pointer flipFilter = FilterType::New();

	cout << "Flip filter is being applied" << endl;
	typedef FilterType::FlipAxesArrayType FlipAxesArrayType;
	FlipAxesArrayType arrFlipAxes;
	arrFlipAxes[0] = 1;
	arrFlipAxes[1] = 0;
	arrFlipAxes[2] = 0;
	flipFilter->SetFlipAxes(arrFlipAxes);
	flipFilter->SetInput(resampler->GetOutput());
	
	typedef itk::AbsImageFilter<OutputImageType, OutputImageType> AbsImageFilterType;
	AbsImageFilterType::Pointer absImgFilter = AbsImageFilterType::New();
	absImgFilter->SetInput(flipFilter->GetOutput()); 

	typedef itk::MultiplyImageFilter<OutputImageType, OutputImageType, OutputImageType> MultiplyImageFilterType;

	MultiplyImageFilterType::Pointer multiplyImageFilter = MultiplyImageFilterType::New();
	multiplyImageFilter->SetInput(absImgFilter->GetOutput());
	multiplyImageFilter->SetConstant(65536); //calculated already

	//typedef unsigned short FinalPixelType;
	//typedef itk::Image< FinalPixelType, 3 > FinalImageType;
	typedef itk::CastImageFilter< OutputImageType, USHORT_ImageType > CastFilterType;
	CastFilterType::Pointer castFilter = CastFilterType::New();
	castFilter->SetInput(multiplyImageFilter->GetOutput());
	castFilter->Update(); 

	USHORT_ImageType::Pointer tmpReconImg;
	//if all 0 0 0 don't do the median filtering

	itk::TimeProbe reconTimeProbe;
	reconTimeProbe.Start();

	cout << "Reconstructing the image.. please wait..." << endl;
	USHORT_ImageType::SizeType indexRadius;
	indexRadius[0] = ui.lineEdit_PostMedSizeX->text().toInt(); // radius along x
	indexRadius[1] = ui.lineEdit_PostMedSizeY->text().toInt(); // radius along y
	indexRadius[2] = ui.lineEdit_PostMedSizeZ->text().toInt(); // radius along y
	if (ui.checkBox_PostMedianOn->isChecked() && (indexRadius[0] != 0 || indexRadius[1] != 0 || indexRadius[2] != 0))
	{
		typedef itk::MedianImageFilter<USHORT_ImageType, USHORT_ImageType >  FilterType;
		FilterType::Pointer medFilter = FilterType::New();

		//this is radius. 1 --> median window 3
		cout << "Post median(3D) filtering is in the pipeline..Size(radius X Y Z) is = " << indexRadius << endl;

		medFilter->SetRadius(indexRadius);
		medFilter->SetInput(castFilter->GetOutput());
		medFilter->Update(); //Error here!g
		tmpReconImg = medFilter->GetOutput();
		//cout << "median filtering has been done" << endl;
	}
	else
	{
		tmpReconImg = castFilter->GetOutput();
	}

	typedef itk::ThresholdImageFilter <USHORT_ImageType> ThresholdImageFilterType;
	ThresholdImageFilterType::Pointer thresholdFilterAbove = ThresholdImageFilterType::New();
	thresholdFilterAbove->SetInput(tmpReconImg);
	thresholdFilterAbove->ThresholdAbove(4095);
	thresholdFilterAbove->SetOutsideValue(4095);

	ThresholdImageFilterType::Pointer thresholdFilterBelow = ThresholdImageFilterType::New();
	thresholdFilterBelow->SetInput(thresholdFilterAbove->GetOutput());
	thresholdFilterBelow->ThresholdBelow(0);
	thresholdFilterBelow->SetOutsideValue(0);
	thresholdFilterBelow->Update();
	tmpReconImg = thresholdFilterBelow->GetOutput();

	cout << "After Filtering" << endl;

	reconTimeProbe.Stop();
	std::cout << "It took " << reconTimeProbe.GetMean() << ' ' << reconTimeProbe.GetUnit() << std::endl;
	ui.lineEdit_ReconstructionTime->setText(QString("%1").arg(reconTimeProbe.GetMean()));

	//By default CanRunInPlace checks whether the input and output image type match.
	switch (target)
	{
	case REGISTER_RAW_CBCT:
		m_spRawReconImg = tmpReconImg; //Checked.. successfully alive.
		m_spCrntReconImg = m_spRawReconImg;
		break;
	case REGISTER_COR_CBCT:
		m_spScatCorrReconImg = tmpReconImg; //Checked.. successfully alive.
		m_spCrntReconImg = m_spScatCorrReconImg;
		break;
	}
	QString outputFilePath = ui.lineEdit_OutputFilePath->text();

	QFileInfo outFileInfo(outputFilePath);
	QDir outFileDir = outFileInfo.absoluteDir();

	if (outputFilePath.length() < 2 || !outFileDir.exists())
	{
		cout << "No available output path. Should be exported later" << endl;
		ui.lineEdit_Cur3DFileName->setText("FDK-reconstructed volume");
	}
	else
	{
		typedef itk::ImageFileWriter<USHORT_ImageType> WriterType;
		WriterType::Pointer writer = WriterType::New();
		writer->SetFileName(outputFilePath.toLocal8Bit().constData());
		writer->SetUseCompression(true); //not exist in original code (rtkfdk)
		writer->SetInput(m_spCrntReconImg);

		cout << "Writing the image to: " << outputFilePath.toLocal8Bit().constData() << endl;

		TRY_AND_EXIT_ON_ITK_EXCEPTION(writer->Update());

		ui.lineEdit_Cur3DFileName->setText(outputFilePath);
		std::cout << std::endl;
		std::cout << "Output generation was succeeded" << std::endl;
	}


	m_dspYKReconImage->CreateImage(size_trans[0], size_trans[1], 0);

	ui.spinBoxReconImgSliceNo->setMinimum(0);
	ui.spinBoxReconImgSliceNo->setMaximum(size_trans[2] - 1);
	ui.spinBoxReconImgSliceNo->setValue(qRound(size_trans[2] / 2.0)); 
	//SLT_ViewRegistration();
	if (target == REGISTER_COR_CBCT)
	{
		UpdateReconImage(m_spCrntReconImg, QString("SCATTER_COR_CBCT"));
	}
	else if (target == REGISTER_RAW_CBCT)
	{
		UpdateReconImage(m_spCrntReconImg, QString("RAW_CBCT"));
	}

	cout << "FINISHED!: Total Variation CBCT reconstruction" << endl;
}
#endif

void CbctRecon::SLT_DoReconstruction()
{
	if (ui.checkBox_FDK->isChecked())
	{
		if (ui.radioButton_UseCUDA->isChecked())
		{
			CudaDoReconstructionFDK(REGISTER_RAW_CBCT);
		} else {
			DoReconstructionFDK(REGISTER_RAW_CBCT);
		}
		m_pDlgRegistration->UpdateListOfComboBox(0);//combo selection signalis called
		m_pDlgRegistration->UpdateListOfComboBox(1);
		//m_pDlgRegistration->SelectComboExternal(0, REGISTER_RAW_CBCT); // will call fixedImageSelected
		//m_pDlgRegistration->SelectComboExternal(1, REGISTER_RAW_CBCT );

		//After first reconstruction, set Median size to 0 0 1 for scatter corrected solution
		/* ui.lineEdit_PostMedSizeX->setText(QString("%1").arg(0.0));
		ui.lineEdit_PostMedSizeY->setText(QString("%1").arg(0.0));
		ui.lineEdit_PostMedSizeZ->setText(QString("%1").arg(1.0));*/
	}
	else if (ui.checkBox_TV->isChecked() && !ui.checkBox_FDK->isChecked())
	{
#if CUDA_FOUND
		CudaDoReconstructionTV(REGISTER_RAW_CBCT);
#else
		DoReconstructionTV(REGISTER_RAW_CBCT);
#endif
		m_pDlgRegistration->UpdateListOfComboBox(0);//combo selection signalis called
		m_pDlgRegistration->UpdateListOfComboBox(1);

	}
  
}

void CbctRecon::SLT_InitializeGraphLim()
{
	//Set Max Min at graph
	if (ui.radioButton_graph_proj->isChecked())
	{
		if (m_iImgCnt > 0) //if indep raw his images are loaded
		{
			int horLen = m_dspYKImgProj->m_iWidth;
			int verLen = m_dspYKImgProj->m_iHeight;

			//set edit maxium min
			QString strXMin;
			strXMin.sprintf("%d", horLen);
			ui.lineEditXMin->setText("0");
			ui.lineEditXMax->setText(strXMin);

			QString strYMin, strYMax;
			strYMin.sprintf("%2.1f", m_fProjImgValueMin);
			strYMax.sprintf("%2.1f", m_fProjImgValueMax);

			ui.lineEditYMin->setText(strYMin);
			ui.lineEditYMax->setText(strYMax);
		}


		if (!m_spProjImg3DFloat)
			return;

		int horLen = m_spProjImg3DFloat->GetBufferedRegion().GetSize()[0];
		int verLen = m_spProjImg3DFloat->GetBufferedRegion().GetSize()[1];

		//set edit maxium min
		QString strXMin;
		strXMin.sprintf("%d", horLen);
		ui.lineEditXMin->setText("0");
		ui.lineEditXMax->setText(strXMin);

		QString strYMin, strYMax;
		strYMin.sprintf("%2.1f", m_fProjImgValueMin);
		strYMax.sprintf("%2.1f", m_fProjImgValueMax);

		ui.lineEditYMin->setText(strYMin);
		ui.lineEditYMax->setText(strYMax);
	}
	else if (ui.radioButton_graph_recon->isChecked())
	{
		if (!m_spCrntReconImg)
			return;

		int horLen = m_spCrntReconImg->GetBufferedRegion().GetSize()[0];
		int verLen = m_spCrntReconImg->GetBufferedRegion().GetSize()[1];

		//set edit maxium min
		QString strXMax;
		strXMax.sprintf("%d", horLen);
		ui.lineEditXMin->setText("0");
		ui.lineEditXMax->setText(strXMax);

		QString strYMin, strYMax;
		strYMin.sprintf("%4.1f", 0.0);
		strYMax.sprintf("%4.1f", 2000.0);

		ui.lineEditYMin->setText(strYMin);
		ui.lineEditYMax->setText(strYMax);
	}

	return;
}

//
//void CbctRecon::SLT_GetProjectionProfile()
//{
//		m_dspYKReconImage->CreateImage(410,410,0);
//		ui.spinBoxReconImgSliceNo->setMinimum(0);
//		ui.spinBoxReconImgSliceNo->setMaximum(119);
//
//		m_iTmpIdx++;
//		ui.spinBoxReconImgSliceNo->setValue(m_iTmpIdx);
//
//
//		SLT_DrawReconImage();
//
//}
//
//void CbctRecon::SLT_GetReconImgProfile()
//{
//
//}

void CbctRecon::SLT_CopyTableToClipBoard()
{
	qApp->clipboard()->clear();

	QStringList list;

	int rowCnt = m_pTableModel->rowCount();
	int columnCnt = m_pTableModel->columnCount();


	list << "\n";
	//for (int i = 0 ; i < columnCnt ; i++)
	//{
	QFileInfo tmpInfo = QFileInfo(ui.lineEdit_Cur3DFileName->text());
		//list << "Index";
	list << tmpInfo.baseName();
	list << "\n";

	list << "Pos(mm)";
	list << "Intensity";
	list << "\n";

	for (int j = 0 ; j<rowCnt ; j++)
	{
		for (int i = 0 ; i < columnCnt ; i++)
		{
			QStandardItem* item = m_pTableModel->item(j,i);
			list << item->text();
		}
		list << "\n";
	}

	qApp->clipboard()->setText(list.join("\t"));
}

void CbctRecon::SetProjDir(QString& strProjPath)
{
    m_strPathGeomXML = "";
    m_strPathDirDefault = strProjPath;
    ui.lineEdit_HisDirPath->setText(strProjPath);

    USHORT_ImageType::Pointer spNull;

    m_spCrntReconImg = spNull; //fixed image // ID: RawCBCT
    m_spRawReconImg = spNull; //just added --> when file is loaded
    m_spScatCorrReconImg = spNull;//just added --> after scatter correction

    FindAllRelevantPaths(strProjPath);
    init_DlgRegistration(m_strDCMUID);
}

void CbctRecon::SLT_SetHisDir() //Initialize all image buffer
{
  //Initializing..


	//Set folder --> then use RTK HIS Reader
	QString dirPath = QFileDialog::getExistingDirectory(this, tr("Open Directory"),
		m_strPathDirDefault,QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	if (dirPath.length()<=1)
		return;

        SetProjDir(dirPath);

        m_vSelectedFileNames.clear();

        cout << "Push Load button to load projection images" << endl;
	return;

	//if (m_strPathGeomXML.length() > 1) //if geometry file was found
	//{
	//  SLT_LoadSelectedProjFiles();
	//}
	//else
	//{
	//  cout << "Geometry file is not ready. Find XML geometry file manually" << endl;
	//  ui.lineEdit_ElektaGeomPath->setText(QString(""));
	//}
}


void CbctRecon::SLT_LoadSelectedProjFiles()//main loading fuction for projection images
{
	ui.pushButton_DoRecon->setDisabled(true);
	//1) Get all projection file names
	QString dirPath = ui.lineEdit_HisDirPath->text();
		//.toLocal8Bit().constData();

	if (!QFile::exists(dirPath))
	{
		cout << "Projection file directory was not found. Retry." << endl;
		return;
	}

	m_iImgCnt = 0; //should be reset
	m_iCntSelectedProj = 0;
	ReleaseMemory(); //only reset mem for indepent projection images
	char * regexp;
	if (ximIsUsed)
		regexp = "Proj_.*.xim"; //".[0-9a-fA-F].hnd"; //is"; VARIAN vs ELEKTA
	else
		regexp = "Proj_.*.hnd"; //".[0-9a-fA-F].hnd"; //is"; VARIAN vs ELEKTA
	//char * regexp = ".*.his";
	vector <string> names;
	itk::RegularExpressionSeriesFileNames::Pointer regexpnames = itk::RegularExpressionSeriesFileNames::New();
	regexpnames->SetDirectory(dirPath.toLocal8Bit().constData());
	//regexpnames->SetNumericSort(false);
	regexpnames->SetNumericSort(true); //doesn't work with hexadecimal. and [true/false] doesn't mean ascending or descending
	regexpnames->SetRegularExpression(regexp);
	regexpnames->SetSubMatch(1); //SetSubMatch(0) led to sorting from last digit of the name
	//regexpnames->SetSubMatch(0); //SetSubMatch(0) led to sorting from last digit of the name
	
	TRY_AND_EXIT_ON_ITK_EXCEPTION( names = regexpnames->GetFileNames() );

	if (!IsFileNameOrderCorrect(names))
	{
		cout << "Check the file name order" << endl;
		QMessageBox::warning(this,"warning","Error on File Name Sorting!");
		return;
	}
	else
	{
		cout << "File name order was cross-checked and found to be OK!" << endl;
	}

	int fullCnt = names.size();
	if (fullCnt <= 0)
	{
		cout << "No projection file was found. Retry." << endl;
		return;
	}
	else
		cout << fullCnt << "  projection files were found." << endl;

	//2) Elekta Geometry file
	QString geomPath = ui.lineEdit_VarianGeomPath->text(); // VARIAN VS ELEKTA
	QFileInfo geomFileInfo(geomPath);
	//!QFile::exists(geomPath)

	if (!geomFileInfo.exists())
	{
		cout << "Critical Error! geometry file is not existing. Please retry." << endl;
		return;
	}
	else if (geomFileInfo.fileName() == "_Frames.xml")//this is XVI XML. this code will be deleted later ,instead RTK XML will be used as a sole solution
	{
		cout << "XVI Geometry File was found. This will be temporarily used:" << geomPath.toLocal8Bit().constData() << endl;
		LoadXVIGeometryFile(geomPath.toLocal8Bit().constData()); //will generate m_spFullGeometry
	}
	else if (geomFileInfo.fileName() == "ProjectionInfo.xml")//this is OBI XML. this code will be deleted later ,instead RTK XML will be used as a sole solution
	{
		cout << "Varian XML Geometry File was found. This will be temporarily used:" << geomPath.toLocal8Bit().constData() << endl;
		// ADDED BY AGRAVGAARD :::::::: Thief'd from RTK/applications/ rtkvarianobigemetry.cxx
		rtk::VarianObiGeometryReader::Pointer reader;
		reader = rtk::VarianObiGeometryReader::New();
		reader->SetXMLFileName(geomPath.toLocal8Bit().constData());
		reader->SetProjectionsFileNames(regexpnames->GetFileNames());
		TRY_AND_EXIT_ON_ITK_EXCEPTION(reader->UpdateOutputData());
		// Write
		rtk::ThreeDCircularProjectionGeometryXMLFileWriter::Pointer xmlWriter =
			rtk::ThreeDCircularProjectionGeometryXMLFileWriter::New();
		xmlWriter->SetFilename("RTKgeometry.xml");
		xmlWriter->SetObject(reader->GetGeometry());
		TRY_AND_EXIT_ON_ITK_EXCEPTION(xmlWriter->WriteFile());
		cout << "RTK standard Geometry XML File was created:" << "RTKgeometry.xml" << endl;
		LoadRTKGeometryFile("RTKgeometry.xml");
		// ::::::::::::::::::::::::::::LoadXMLGeometryFile(geomPath.toLocal8Bit().constData()); //will generate m_spFullGeometry
	}
	else if (geomFileInfo.fileName() == "Scan.xml")//this is XIM XML. this code will be deleted later ,instead RTK XML will be used as a sole solution
	{
		cout << "Varian ProBeam XML Geometry File was found. This will be temporarily used:" << geomPath.toLocal8Bit().constData() << endl;
		// ADDED BY AGRAVGAARD :::::::: Thief'd from RTK/applications/ rtkvarianobigemetry.cxx
		rtk::VarianProBeamGeometryReader::Pointer reader;
		reader = rtk::VarianProBeamGeometryReader::New();
		reader->SetXMLFileName(geomPath.toLocal8Bit().constData());
		reader->SetProjectionsFileNames(regexpnames->GetFileNames());
		TRY_AND_EXIT_ON_ITK_EXCEPTION(reader->UpdateOutputData());
		// Write
		rtk::ThreeDCircularProjectionGeometryXMLFileWriter::Pointer xmlWriter =
			rtk::ThreeDCircularProjectionGeometryXMLFileWriter::New();
		xmlWriter->SetFilename("RTKgeometry.xml");
		xmlWriter->SetObject(reader->GetGeometry());
		TRY_AND_EXIT_ON_ITK_EXCEPTION(xmlWriter->WriteFile());
		cout << "RTK standard Geometry XML File was created:" << "RTKgeometry.xml" << endl;
		LoadRTKGeometryFile("RTKgeometry.xml");
		cout << "Done!";
		// ::::::::::::::::::::::::::::LoadXMLGeometryFile(geomPath.toLocal8Bit().constData()); //will generate m_spFullGeometry
	}
	else
	{
		cout << "RTK standard Geometry XML File was found:" << geomPath.toLocal8Bit().constData() << endl;
		LoadRTKGeometryFile(geomPath.toLocal8Bit().constData()); //will generate m_spFullGeometry
	}

	int iFullGeoDataSize  = m_spFullGeometry->GetGantryAngles().size() ;
	if (iFullGeoDataSize< 1)
	{
		cout << "Not enough projection image (should be > 0)" << endl;
		return;
	}

	if (iFullGeoDataSize != fullCnt && iFullGeoDataSize-1 != fullCnt)
	{
		QMessageBox msgBox;
		msgBox.setText("Size of geometry data and file numbers are not same!");
		msgBox.exec();
	}

	//3) Seletively load projection file

	double meanGap = vectorMean(m_spFullGeometry->GetAngularGaps(m_spFullGeometry->GetSourceAngles())) / itk::Math::pi * 180.0;
	double angleSum = vectorSum(m_spFullGeometry->GetAngularGaps(m_spFullGeometry->GetSourceAngles())) / itk::Math::pi * 180.0;//total rot. range from first angle
	cout << "AngularGaps Sum (deg): " << angleSum << ", Mean (deg): " << meanGap << endl;


	double gantryAngleInterval = ui.lineEdit_ManualProjAngleGap->text().toDouble();

	//if (ui.Radio_KeepOriginalAngles->isChecked())
	if (ui.Radio_ManualProjAngleGap->isChecked())
	{
		//bManualGap = true;
		//cout << "Input angle gap in deg: " ;
		//cin >> gantryAngleInterval;

		if (gantryAngleInterval < meanGap)
		{
			cout << "Angle gap size is too small. Terminating the app" << endl;
			return;
			//bManualGap = false;
		}
	}

///////////////////////////////////Exclude outlier projection files

	vector<int> vExcludeIdx;
	/*
	if (ximIsUsed){ // Possibly due to the slow acceleration the first and last few projections make artefacts
		int n_excl = 4; // exclude from each side
		int last_offset = iFullGeoDataSize - 2 * n_excl; //new last index + 1
		for (int i = 0; i < 8; i++)
		{
			vExcludeIdx.push_back(i < n_excl ? i : (last_offset + i ));
		}
	}
	*/
	// cout << "[Excluding-files-function] has been omitted. To reactivaite it, please edit SLT_LoadSelectedProjFiles" << endl;


///////////////////////////////////Exclude outlier projection files
  vector<int> vSelectedIdx;
  vector<int>::iterator itIdx;

  //double gantryAngleInterval = ui.lineEdit_ManualProjAngleGap->text().toDouble();

  if (ui.Radio_ManualProjAngleGap->isChecked())
  {
	  //Select indices for recon
	  //Generate norminal gantry values from the first angle
	  double firstAngle = m_spFullGeometry->GetGantryAngles().at(0);
	  double lastAngle = m_spFullGeometry->GetGantryAngles().at(iFullGeoDataSize-1);

	  vector<double> vNormAngles;

	  int multiSize  = round(angleSum / gantryAngleInterval)+2;

	  //CW only (179.xx -> 181.xx -> 359.xx --> 1.xx --> 179.xx), CCW should be checked later
	  for (int i  = 0 ; i< multiSize ; i++)
	  {
		  double curAngle = 0.0;

		  if (m_bScanDirectionCW)
		  {
			curAngle = firstAngle + i*gantryAngleInterval;
			if (curAngle >= 360.0)
				curAngle  = curAngle - 360.0;
		  }
		  else
		  {
			curAngle = firstAngle - i*gantryAngleInterval;
			if (curAngle < 0.0)
				curAngle  = curAngle + 360.0;
		  }


		  //Don't add last gantry angle if their intervals are too small.

		  //Last data will be added at the last part
		  if (i > multiSize - 5) // last parts of the data
		  {
			  if (m_bScanDirectionCW)
			  {
				  if (curAngle <= lastAngle - gantryAngleInterval/2.0)//from 5 latest indices,
				  {
					  vNormAngles.push_back(curAngle);
				  }
			  }
			  else
			  {
				  if (curAngle >= lastAngle - gantryAngleInterval/2.0)//from 5 latest indices,
				  {
					  vNormAngles.push_back(curAngle);
				  }
			  }
				  //gantryAngleInterval/2.0 is given to remove "very near" value to the last value
		  }
		  else
			  vNormAngles.push_back(curAngle);
	  }
	  vNormAngles.push_back(lastAngle);

	  for (int i = 0 ; i<vNormAngles.size() ; i++)
	  {
		  cout << "Nominal proj. angle: " ;
			cout << vNormAngles.at(i) << endl;
	  }

	  //Collect appropriate indices
	  GetSelectedIndices(m_spFullGeometry->GetGantryAngles(), vNormAngles, vSelectedIdx, m_bScanDirectionCW, vExcludeIdx);

	  for (itIdx =vSelectedIdx.begin() ; itIdx != vSelectedIdx.end() ; itIdx++ )
	  {
		  cout << "Index: " << *itIdx << "     " << "GantryAngle: " << m_spFullGeometry->GetGantryAngles().at(*itIdx) << endl;
	  }
  }
  else // not manual
  {
	  for (int i = 0 ; i< iFullGeoDataSize ; i++)
	  {
		  if (std::find(vExcludeIdx.begin(), vExcludeIdx.end(),i) == vExcludeIdx.end())
			vSelectedIdx.push_back(i);
	  }
  }

  //Regenerate geometry object
  //cout << "Gantry (deg): ";
  m_spCustomGeometry = GeometryType::New();
  for (itIdx =vSelectedIdx.begin() ; itIdx != vSelectedIdx.end() ; itIdx++ )
  {
	  //9 parameters are required
	  double curSID = m_spFullGeometry->GetSourceToIsocenterDistances().at(*itIdx);
	  double curSDD = m_spFullGeometry->GetSourceToDetectorDistances().at(*itIdx);
	  double curGantryAngle = m_spFullGeometry->GetGantryAngles().at(*itIdx);
	  double kVAng = curGantryAngle * 180.0 * itk::Math::one_over_pi; // 360 / 2 = 180 radians to degrees
	  double MVAng = kVAng - 90.0;
	  if (MVAng < 0.0)
		  MVAng = MVAng + 360.0;
	  curGantryAngle = MVAng;
	  //cout << curGantryAngle << ", ";

	  double curProjOffsetX = m_spFullGeometry->GetProjectionOffsetsX().at(*itIdx);
	  double curProjOffsetY = m_spFullGeometry->GetProjectionOffsetsY().at(*itIdx);
	  double curOutOfPlaneAngles = m_spFullGeometry->GetOutOfPlaneAngles().at(*itIdx);
	  double curInPlaneAngles = m_spFullGeometry->GetInPlaneAngles().at(*itIdx);
	  double curSrcOffsetX = m_spFullGeometry->GetSourceOffsetsX().at(*itIdx);
	  double curSrcOffsetY = m_spFullGeometry->GetSourceOffsetsY().at(*itIdx);

	  m_spCustomGeometry->AddProjection(curSID, curSDD,curGantryAngle,
		  curProjOffsetX,curProjOffsetY, //Flexmap. For Xim, these are 0
		  curOutOfPlaneAngles, curInPlaneAngles, //In elekta and varian, these are 0
		  curSrcOffsetX, curSrcOffsetY); //In elekta and varian, these are 0
  }
  //cout << endl;
  //Regenerate fileNames and geometry object based on the selected indices.

  if (!m_vSelectedFileNames.empty())
      m_vSelectedFileNames.clear();

  for (itIdx = vSelectedIdx.begin(); itIdx != vSelectedIdx.end(); itIdx++)
  {
      string curStr = names.at(*itIdx);
      m_vSelectedFileNames.push_back(curStr);
  }

  // Reads the cone beam projections
  typedef rtk::ProjectionsReader< OutputImageType > ReaderType;
  ReaderType::Pointer reader = ReaderType::New();
  reader->SetFileNames(m_vSelectedFileNames);
  TRY_AND_EXIT_ON_ITK_EXCEPTION(reader->GenerateOutputInformation());
  cout << "Reading..." << endl;
  TRY_AND_EXIT_ON_ITK_EXCEPTION(reader->Update());
  //After reading the whole file,
  //HIS header should be saved

  /*
  cout << "Copying the HIS info to buffer." << endl;

  m_iCntSelectedProj = m_vSelectedFileNames.size();
  m_arrYKBufProj = new YK16GrayImage [m_iCntSelectedProj];
  
  for (int i = 0 ; i< m_iCntSelectedProj; i++)
  {
      m_arrYKBufProj[i].m_strFilePath = m_vSelectedFileNames.at(i).c_str();
      m_arrYKBufProj[i].CopyHisHeader(m_vSelectedFileNames.at(i).c_str());
  }
  */ // AG: THIS DOES NOT SEEM USEFUL FOR HND FILES ITS GONNA FAIL AT SAVING LATER ANYWAY?
  m_iCntSelectedProj = m_vSelectedFileNames.size(); // EXCEPT THIS

  //QString strMsg = "Do you want to save the projection files as meta file(*.mha)";

  //QMessageBox msgBox;
  //msgBox.setText(strMsg);
  //msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);

  //int res = msgBox.exec();

  //if (res == QMessageBox::Yes)
  //{
	 // QString filePath = QFileDialog::getSaveFileName(this, "Save Image", "", "meta image file (*.mha)",0,0); //Filename don't need to exist
	 //
	 // typedef itk::ImageFileWriter<  OutputImageType > WriterType2;
	 // WriterType2::Pointer writer2 = WriterType2::New();
	 // writer2->SetFileName(filePath.toLocal8Bit().constData());
	 // writer2->SetUseCompression(true);
	 // writer2->SetInput( reader->GetOutput() );
	 // TRY_AND_EXIT_ON_ITK_EXCEPTION( writer2->Update() );
	 // cout << "proj meta file was saved " << endl;
  //}

  cout << "ProjectionReader Get Spacing : "<< reader->GetOutput()->GetSpacing() << endl;
  //0.4 0.4 1 (spacing[2] --> means nothing)
  //To reduce the size,resample the image
  //// Calculate output size and spacing

  m_fProjSpacingX = reader->GetOutput()->GetSpacing()[0];
  m_fProjSpacingY = reader->GetOutput()->GetSpacing()[1];

  double originalMax = -1.0;
  double originalMin = -1.0;

  GetMaxAndMinValueOfProjectionImage(originalMax, originalMin, reader->GetOutput());
  cout << "Reader Max, Min=" << originalMax << "	" << originalMin << endl;

  m_spProjImg3DFloat = reader->GetOutput(); // 1024 1024, line integ image 

  m_fResampleF = ui.lineEdit_DownResolFactor->text().toDouble(); //0.5

  if (m_fResampleF > 1 && m_fResampleF <= 0)
  {
	cout << "wrong resample factor. reset to 1.0" << endl;
	ui.lineEdit_DownResolFactor->setText("1.0");
	m_fResampleF = 1.0;
  }
  //UPdate default scatter correction params
  //double defaultScaResampleF = m_fResampleF;
  //double defaultScaMedian = DEFAULT_SCA_MEDIAN * defaultScaResampleF;//25.0 --> 12.5
  //double defaultScaGaussian = DEFAULT_SCA_GAUSSIAN * defaultScaResampleF;//25.0 --> 12.5
  //double defaultScaPostProjMedian = DEFAULT_SCA_POST_PROJ_MEDIAN * defaultScaResampleF; // 6 --> 3

  //ui.lineEdit_scaResam->setText(QString("%1").arg(defaultScaResampleF));
  //ui.lineEdit_scaMedian->setText(QString("%1").arg(defaultScaMedian));
  //ui.lineEdit_scaGaussian->setText(QString("%1").arg(defaultScaGaussian));
  //ui.lineEdit_scaPostMedian->setText(QString("%1").arg(defaultScaPostProjMedian));
  if (m_fResampleF != 1.0)
  {
	cout << "Resampling by a factor: " << m_fResampleF << endl;
	ResampleItkImage(m_spProjImg3DFloat, m_spProjImg3DFloat, m_fResampleF); // AG: Uses much time!!
  }
  cout << "ConvertLineInt2Intensity.. " << endl;
  ConvertLineInt2Intensity(m_spProjImg3DFloat, m_spProjImgRaw3D, 65535); 

  cout << "Raw3DProj dimension " << m_spProjImgRaw3D->GetRequestedRegion().GetSize() << endl;

  //m_spProjImgRaw3D is Ushort

  //20140214: Norminal I_0 --> I_0_user : No effect in image quality of Recon.image
   //m_spProjImg3D = mu_t value
  //double wanted_I0 = 40000;
  //double addingValue = log(1/(65536/wanted_I0)); //if 50000: -0.27058
  //typedef itk::AddImageFilter<OutputImageType> AddImageFilterType;
  //AddImageFilterType::Pointer addFilter = AddImageFilterType::New();
  //typedef itk::AddImageFilter <OutputImageType, OutputImageType, OutputImageType> AddImageFilterType;
  //AddImageFilterType::Pointer addImageFilter = AddImageFilterType::New();
  //addImageFilter->SetInput1(m_spProjImg3D);
  //cout << "adding value=" << addingValue << endl;
  //addImageFilter->SetConstant2(addingValue); // + ~0.5
  //addImageFilter->Update();
  //m_spProjImg3D = addImageFilter->GetOutput();
 //Error occurss. tmpe inactivated

  //std::cout << vSelectedFileNames.size() << std::endl;
  std::cout << "Projection reading succeeded: " << m_vSelectedFileNames.size() << " files were read" << std::endl;

  //Following macro
  //set default 3D median filter
  /*ui.lineEdit_PostMedSizeX->setText(QString("%1").arg(1.0));
  ui.lineEdit_PostMedSizeY->setText(QString("%1").arg(1.0));
  ui.lineEdit_PostMedSizeZ->setText(QString("%1").arg(1.0));*/

 // cout << "before button enabled" << endl;
  ui.pushButton_DoRecon->setEnabled(true);

  //cout << "before setMinMax" << endl;
  ui.spinBoxImgIdx->setMinimum(0);
  ui.spinBoxImgIdx->setMaximum(m_vSelectedFileNames.size() - 1);
  ui.spinBoxImgIdx->setValue(0); //it doesn't call Draw Event .. don't know why.


 // cout << "before SetMaxAndMinValueOfProjectionImage" << endl;

  SetMaxAndMinValueOfProjectionImage(); //update min max projection image

  //cout << "before SLT_InitializeGraphLim" << endl;
  SLT_InitializeGraphLim();


  //cout << "before SLT_DrawProjImages" << endl;
  this->SLT_DrawProjImages();  //Update Table is called
  //SLT_DrawGraph();
  //SLT_UpdateTable();

  if (!ui.checkBox_autoBT->isChecked() && !ui.checkBox_autoBH->isChecked() && !ui.checkBox_autoRecon->isChecked())
  {
	  cout << "FINISHED!: Loading projection files. Proceed to reconstruction" << endl;
  }
  else if (ui.checkBox_autoBT->isChecked())
  {
	  SLT_DoBTC();
	  cout << "FINISHED: Bow-Tie correction. ";
	  if (!ui.checkBox_autoRecon->isChecked() && !ui.checkBox_autoBH->isChecked()){
		  cout << "Proceed to reconstruction";
	  }
	  cout << endl;
  }
  if (ui.checkBox_autoBH->isChecked())
  {
	  SLT_DoBHC();
	  cout << "FINISHED: Beam Hardening correction. ";
	  if (!ui.checkBox_autoRecon->isChecked()){
		  cout << "Proceed to reconstruction";
	  }
	  cout << endl;
  }
  if (ui.checkBox_autoRecon->isChecked())
  {
	  SLT_DoReconstruction();
	  cout << "FINISHED!: Reconstruction. Load pCT and Proceed to registration" << endl;
  }

  PlaySound(TEXT("chime.wav"), NULL, SND_SYNC); //WINDOWS
}

//
//void CbctRecon::SLT_DrawGainImage()
//{
//	if (m_pImgGain->IsEmpty())
//		return;
//
//	m_pImgGain->FillPixMapMinMax(ui.sliderGainMin->value(), ui.sliderGainMax->value());
//	//m_pImageYKGain->DrawToLabel(ui.labelImageGain);
//
//	ui.labelImageGain->SetBaseImage(m_pImgGain);
//	ui.labelImageGain->update();
//
//}

void CbctRecon::GetSelectedIndices(const vector<double>& vFullAngles, vector<double>& vNormAngles, vector<int>& vTargetIdx, bool bCW, vector<int>& vExcludingIdx)
{
	//vector<double>::iterator nomIt;

	//vector<double>::const_iterator full_It;//assumption, sorted by projection time. Begins with 179.xxx (CW)
	int latest_Idx = 0;

	double curVal =0.0;
	double nextVal =0.0;

	int sizeNom = vNormAngles.size();
	int sizeFull = vFullAngles.size();

	for (int i = 0 ; i< sizeNom ; i++)
	{
		double tmpNominalValue = vNormAngles.at(i);

		for (int j= latest_Idx+1 ; j<sizeFull-1; j++)
		{
			int enExcludingMode = 0; //0: safe,1: right is outlier, 2: left is outlier, 3: both are outlier

			//1) Left point is outlier
			if (std::find(vExcludingIdx.begin(), vExcludingIdx.end(), j) != vExcludingIdx.end()
				&& std::find(vExcludingIdx.begin(), vExcludingIdx.end(), j+1) == vExcludingIdx.end())
			{
				enExcludingMode = 2;
			}
			//2) Right point is outlier
			else if (std::find(vExcludingIdx.begin(), vExcludingIdx.end(), j) == vExcludingIdx.end()
				&& std::find(vExcludingIdx.begin(), vExcludingIdx.end(), j+1) != vExcludingIdx.end())
			{
				enExcludingMode = 1;
			}
			//2) No outlier
			else if (std::find(vExcludingIdx.begin(), vExcludingIdx.end(), j) == vExcludingIdx.end()
				&& std::find(vExcludingIdx.begin(), vExcludingIdx.end(), j+1) == vExcludingIdx.end())
			{
				enExcludingMode = 0;
			}
			//3) Both are outliers
			else if (std::find(vExcludingIdx.begin(), vExcludingIdx.end(), j) != vExcludingIdx.end()
				&& std::find(vExcludingIdx.begin(), vExcludingIdx.end(), j+1) != vExcludingIdx.end())
			{
				enExcludingMode = 3;
			}

			curVal = vFullAngles.at(j);
			nextVal = vFullAngles.at(j+1);

			if (bCW)
			{
				//for full gantry angle value of 359.0 - 1.0 interface in CW
				if (curVal > nextVal + 0.2) // e.g.359)  - 0.5,, 0.2-->minimum angle diff.
				{
					if (tmpNominalValue < 100)
						curVal = curVal - 360.0;
					else if (tmpNominalValue > 260)
						nextVal = nextVal + 360.0;
				}
				if (tmpNominalValue >= curVal && tmpNominalValue <= nextVal)
				{

					//Add filtering
					//if j is among the excluding index list (e.g. outlier), just pass it.


					double diffCur = fabs(tmpNominalValue - curVal);
					double diffNext = fabs(tmpNominalValue - nextVal);

					if (diffCur <= diffNext || enExcludingMode == 0 || enExcludingMode == 1)
					{
						latest_Idx = j;
						vTargetIdx.push_back(latest_Idx);
					}
					else if (j != sizeFull-2 && (enExcludingMode == 0 || enExcludingMode == 2))
					{
						latest_Idx = j+1;
						vTargetIdx.push_back(latest_Idx);
					}
					else
					{
						latest_Idx = j;
						//Skip to pushback
					}

					break;
				}
			}
			else // in CCW case
			{
				//for full gantry angle value of 1.0 - 359.0 interface in CCW
				if (curVal < nextVal + 0.01) // e.g.359)  - 0.5,, 0.2-->minimum angle diff.
				{
					if (tmpNominalValue < 100) //for redundant check
						nextVal = nextVal - 360.0;
					else if (tmpNominalValue > 260) //for redundant check
						curVal = curVal + 360.0;
				}

				// in CCW, next value should be smaller than curVal
				if (tmpNominalValue >= nextVal && tmpNominalValue <= curVal)
				{
					double diffCur = fabs(tmpNominalValue - curVal);
					double diffNext = fabs(tmpNominalValue - nextVal);

					latest_Idx = j;

				/*	if (diffCur <= diffNext)
					{
						vTargetIdx.push_back(j);
					}
					else
					{
						if (j != sizeFull-2)
						{
							vTargetIdx.push_back(j+1);
						}
					}*/

					if (diffCur <= diffNext || enExcludingMode == 0 || enExcludingMode == 1)
					{
						latest_Idx = j;
						vTargetIdx.push_back(latest_Idx);
					}
					else if (j != sizeFull-2 && (enExcludingMode == 0 || enExcludingMode == 2))
					{
						latest_Idx = j+1;
						vTargetIdx.push_back(latest_Idx);
					}
					else
					{
						latest_Idx = j;
						//Skip to pushback
					}
					break;
				}
			}
		}
	}
	//vTargetIdx.push_back(sizeFull-1); //omit the last image --> should be same as first...
}

void CbctRecon::GetExcludeIndexByNames( QString outlierListPath, vector<string>& vProjFileFullPath, vector<int>& vExcludeIdx )
{
	ifstream fin;
	fin.open(outlierListPath.toLocal8Bit().constData(), ios::in);
	if (fin.fail() ==1)
		return;

	char str[MAX_LINE_LENGTH];

	while (!fin.eof())
	{
		memset(str, 0, MAX_LINE_LENGTH);
		fin.getline(str, MAX_LINE_LENGTH);
		if (strlen(str) < 1)
			continue;

		cout << "Outlier file: " << str << endl;
		vector<string>::iterator it;
		int curIdx = 0;
		for (it = vProjFileFullPath.begin() ; it != vProjFileFullPath.end() ; it++)
		{
			if (strstr((*it).c_str(), (const char*)str) != NULL)
			{
				cout << "Detected in the list. Index = " << curIdx << endl;
				vExcludeIdx.push_back(curIdx);
				break;
			}
			curIdx++;
		}
	}
	fin.close();
}


void CbctRecon::SetMaxAndMinValueOfProjectionImage()//should be called whenever proj image is changed
{
	if (m_iImgCnt > 0)
	{
		m_fProjImgValueMax = 65535;
		m_fProjImgValueMin = 0;
		return;
	}

	if (!m_spProjImg3DFloat)
		return;

	itk::ImageSliceConstIteratorWithIndex<OutputImageType> it (m_spProjImg3DFloat, m_spProjImg3DFloat->GetRequestedRegion());

	it.SetFirstDirection(0); //x?
	it.SetSecondDirection(1); //y?
	it.GoToBegin();

	m_fProjImgValueMin = 65535.0;
	m_fProjImgValueMax = -9999.0;

	while( !it.IsAtEnd() )
	{
		while( !it.IsAtEndOfSlice() )
		{
			while( !it.IsAtEndOfLine() )
			{
				double tmpVal = it.Get();

				if (m_fProjImgValueMax < tmpVal)
					m_fProjImgValueMax = tmpVal;

				if (m_fProjImgValueMin > tmpVal)
					m_fProjImgValueMin = tmpVal;

				++it;
			}
			it.NextLine();
		}
		it.NextSlice();
	}
}

void CbctRecon::GetMaxAndMinValueOfProjectionImage(double& fProjImgValueMax, double& fProjImgValueMin, OutputImageType::Pointer projImage)
{
	/*if (m_iImgCnt > 0)
	{
		m_fProjImgValueMax = 65535;
		m_fProjImgValueMin = 0;
		return;
	}*/

	if (!projImage)
	{
		fProjImgValueMax = -1.0;
		fProjImgValueMin = -1.0;
		return;
	}

	itk::ImageSliceConstIteratorWithIndex<OutputImageType> it (projImage, projImage->GetRequestedRegion());

	it.SetFirstDirection(0); //x?
	it.SetSecondDirection(1); //y?
	it.GoToBegin();

	fProjImgValueMin = 65535.0;
	fProjImgValueMax = -9999.0;

	while( !it.IsAtEnd() )
	{
		while( !it.IsAtEndOfSlice() )
		{
			while( !it.IsAtEndOfLine() )
			{
				double tmpVal = it.Get();

				if (fProjImgValueMax < tmpVal)
					fProjImgValueMax = tmpVal;

				if (fProjImgValueMin > tmpVal)
					fProjImgValueMin = tmpVal;

				++it;
			}
			it.NextLine();
		}
		it.NextSlice();
	}
}

void CbctRecon::SLT_DataProbeProj()
{
	double dspWidth = ui.labelImageRaw->width();
	double dspHeight = ui.labelImageRaw->height();
	int dataWidth = 0 ;  int dataHeight = 0;
	int dataX = 0;
	int dataY = 0;
	int dataZ = 0;
	double fProbeValue = 0.0;


	if (m_iImgCnt > 0) //there is indep loaded projection files
	{
		dataWidth = m_dspYKImgProj->m_iWidth;
		dataHeight = m_dspYKImgProj->m_iHeight;

		dataX = qRound(ui.labelImageRaw->x / dspWidth * dataWidth);
		dataY = qRound(ui.labelImageRaw->y / dspHeight * dataHeight);
		dataZ =  ui.spinBoxImgIdx->value();
		fProbeValue = m_dspYKImgProj->m_pData[dataWidth*dataY + dataX];
	}
	else
	{
		 if(!m_spProjImg3DFloat)
			return;

		 dataWidth = (int)(m_spProjImg3DFloat->GetBufferedRegion().GetSize()[0]);
		 dataHeight = (int)(m_spProjImg3DFloat->GetBufferedRegion().GetSize()[1]);

		 //int crntIdx = ui.spinBoxImgIdx->value();
		 //These are displayed data (just index data)
		 dataX = qRound(ui.labelImageRaw->x / dspWidth * dataWidth);
		 dataY = qRound(ui.labelImageRaw->y / dspHeight * dataHeight);
		 dataZ =  ui.spinBoxImgIdx->value();

		 //fProbeValue = m_dspYKImgProj->m_pData[dataWidth*dataY + dataX]/m_multiplyFactor;
		 fProbeValue = m_dspYKImgProj->m_pData[dataWidth*dataY + dataX]/m_multiplyFactor + m_fProjImgValueMin;
	}

	QString dspText;
	dspText.sprintf("(%d, %d, %d): %3.2f",dataX, dataY, dataZ, fProbeValue);
	ui.lineEdit_DataProbe_Proj->setText(dspText);
}


void CbctRecon::SLT_DataProbeRecon()
{
	if (!m_spCrntReconImg)
		return;

	double dspWidth = ui.labelReconImage->width();
	double dspHeight = ui.labelReconImage->height();

	int dataWidth = (int)(m_spCrntReconImg->GetBufferedRegion().GetSize()[0]);
	int dataHeight = (int)(m_spCrntReconImg->GetBufferedRegion().GetSize()[1]);

	//int crntIdx = ui.spinBoxImgIdx->value();
	//These are displayed data (just index data)

	int dataX = qRound(ui.labelReconImage->x / dspWidth * dataWidth);
	int dataY = qRound(ui.labelReconImage->y / dspHeight * dataHeight);
	int dataZ =  ui.spinBoxReconImgSliceNo->value();


	unsigned short iProbeValue = m_dspYKReconImage->m_pData[dataWidth*dataY + dataX];
	//unsigned short iProbeValue = GetValueFrom3DImageUshort(dataX, dataY, dataZ, m_spReconImg);

	QString dspText;
	dspText.sprintf("(%03d, %03d, %03d): %d",dataX, dataY, dataZ, iProbeValue);
	ui.lineEdit_DataProbe_Recon->setText(dspText);
}


double CbctRecon::GetValueFrom3DImageFloat(int reqX, int reqY, int reqZ, OutputImageType::Pointer& sp3DFloatImage)
{
	if (!sp3DFloatImage)
		return -1.0;

	itk::ImageSliceConstIteratorWithIndex<OutputImageType> it (sp3DFloatImage, sp3DFloatImage->GetBufferedRegion());

	it.SetFirstDirection(0); //x?
	it.SetSecondDirection(1); //y?
	it.GoToBegin();

	int idxX, idxY,idxZ;
	idxX = 0;
	idxY = 0;
	idxZ = 0;

	while( !it.IsAtEnd())
	{
		if (idxZ == reqZ)
		{
			idxY = 0;
			while( !it.IsAtEndOfSlice() )
			{
				if (idxY == reqY)
				{
					idxX = 0;
					while( !it.IsAtEndOfLine() )
					{
						if (idxX == reqX)
						{
							double tmpVal = it.Get();
							return tmpVal;
						}
						++it;
						idxX++;
					}
					break;
				}
				it.NextLine();
				idxY++;
			}
			break;
		}
		it.NextSlice();
		idxZ++;
	}

	return -2.0;
}

double CbctRecon::GetValueFrom3DImageUshort( int reqX, int reqY, int reqZ, USHORT_ImageType::Pointer& sp3DUshortImage )
{
	if (!sp3DUshortImage)
		return 0;

	itk::ImageSliceConstIteratorWithIndex<USHORT_ImageType> it (sp3DUshortImage, sp3DUshortImage->GetBufferedRegion());

	it.SetFirstDirection(0); //x?
	it.SetSecondDirection(1); //y?
	it.GoToBegin();

	int idxX, idxY,idxZ;
	idxX = 0;
	idxY = 0;
	idxZ = 0;

	while( !it.IsAtEnd())
	{
		if (idxZ == reqZ)
		{
			idxY = 0;
			while( !it.IsAtEndOfSlice() )
			{
				if (idxY == reqY)
				{

					idxX =0;
					while( !it.IsAtEndOfLine() )
					{
						if (idxX == reqX)
						{
							unsigned short tmpVal = it.Get();
							return tmpVal;
						}
						++it;
						idxX++;
					}
					break;
				}
				it.NextLine();
				idxY++;
			}
			break;
		}
		it.NextSlice();
		idxZ++;
	}
	return 65535;
}


void CbctRecon::SLT_DrawGraph() //based on profile
{
	if (m_pTableModel == NULL)
		return;

	//Draw only horizontal, center

	QVector<double> vAxisX; //can be rows or columns
	QVector<double> vAxisY;

	//QStandardItemModel 	m_pTableModel.item()
	int dataLen = m_pTableModel->rowCount();

	if (dataLen < 1)
		return;

        //cout << "check graph 1" << endl;
	ui.customPlot->clearGraphs();


	double minX = 9999.0;
	double maxX = -1.0;

	for (int i = 0 ; i< dataLen ; i++)
	{
		QStandardItem* tableItem1 = m_pTableModel->item(i,0);
		QStandardItem* tableItem2 = m_pTableModel->item(i,1);
		double tableVal1 = tableItem1->text().toDouble();
		double tableVal2 = tableItem2->text().toDouble();

		if (minX > tableVal1)
			minX = tableVal1;
		if (maxX < tableVal1)
			maxX = tableVal1;

		vAxisX.push_back(tableVal1);
		vAxisY.push_back(tableVal2);
	}

        //cout << "check graph 2" << endl;

	ui.customPlot->addGraph();
	ui.customPlot->graph(0)->setData(vAxisX, vAxisY);
	ui.customPlot->graph(0)->setPen(QPen(Qt::blue));
	ui.customPlot->graph(0)->setName("Image profile");

	 ui.lineEditXMin->setText(QString("%1").arg(minX));
	 ui.lineEditXMax->setText(QString("%1").arg(maxX));

	double tmpXMin = ui.lineEditXMin->text().toDouble();
	double tmpXMax = ui.lineEditXMax->text().toDouble();
	double tmpYMin = ui.lineEditYMin->text().toDouble();
	double tmpYMax = ui.lineEditYMax->text().toDouble();

        //cout << "check graph 3" << endl;

	ui.customPlot->xAxis->setRange(tmpXMin, tmpXMax);
	ui.customPlot->yAxis->setRange(tmpYMin, tmpYMax);

	ui.customPlot->xAxis->setLabel("mm");
	ui.customPlot->yAxis->setLabel("Intensity");
	ui.customPlot->setTitle("Image Profile");
	QFont titleFont = font();
	titleFont.setPointSize(10);
	ui.customPlot->setTitleFont(titleFont);

        //cout << "check graph 4" << endl;

	ui.customPlot->legend->setVisible(false);
	QFont legendFont = font();  // start out with MainWindow's font..
	legendFont.setPointSize(9); // and make a bit smaller for legend
	ui.customPlot->legend->setFont(legendFont);
	ui.customPlot->legend->setPositionStyle(QCPLegend::psTopRight);
	ui.customPlot->legend->setBrush(QBrush(QColor(255,255,255,200)));

        //cout << "check graph 5" << endl;
	ui.customPlot->replot();

	//SLT_UpdateTable();
}


void CbctRecon::SLT_UpdateTable()
{
    if (!m_spCrntReconImg)
        ui.radioButton_graph_proj->setChecked(true);

    //cout << "check 1" << endl;
	YK16GrayImage* pYKImg = NULL;
	double fMultiPlyFactor = 1.0;
	double fMinValue  = 0.0;

	if (ui.radioButton_graph_proj->isChecked())
	{
		pYKImg = m_dspYKImgProj;

		if (m_iImgCnt > 0) //if indep image
			fMultiPlyFactor = 1.0;
		else
		{
			fMultiPlyFactor = m_multiplyFactor;
			fMinValue = m_fProjImgValueMin;
		}
	}
	else
	{
		pYKImg = m_dspYKReconImage;
		fMultiPlyFactor = 1.0;
		fMinValue = 0.0;
	}
	if (pYKImg == NULL)
		return;

        //cout << "check 2" << endl;

	if (m_pTableModel != NULL)
	{
		delete m_pTableModel;
		m_pTableModel = NULL;
	}
        //cout << "check 3" << endl;
        int columnSize = 1;
        int rowSize = 0;

	///int rowSize = pYKImg->m_iWidth;

	if (ui.radioButton_Profile_Hor->isChecked())
	{
		columnSize = 2;
		rowSize = pYKImg->m_iWidth;
	}
	else
	{
		columnSize = 2;
		rowSize = pYKImg->m_iHeight;
	}

        //cout << "check 4" << endl;
	m_pTableModel = new QStandardItemModel(rowSize,columnSize,this); //2 Rows and 3 Columns

	//for (int i = 0 ; i<columnSize ; i++)
	//{
	//QFileInfo tmpInfo = QFileInfo(m_arrYKImage[i].m_strFilePath);
	//m_pTableModel->setHorizontalHeaderItem(0, new QStandardItem(QString("Index")));
	//m_pTableModel->setHorizontalHeaderItem(0, new QStandardItem(QString("Profile")));
	m_pTableModel->setHorizontalHeaderItem(0, new QStandardItem(QString("Index"))); //is not mm
	m_pTableModel->setHorizontalHeaderItem(1, new QStandardItem(QString("Value")));
	//}


        //cout << "check 5" << endl;
	//int width = pYKImg->m_iWidth;
	//int height = pYKImg->m_iHeight;
	//int fixedY = qRound(height / 2.0);

	double originX, originY;
	double spacingX, spacingY;

	if (ui.radioButton_graph_proj->isChecked())
	{
		originX = 0.0;
		originY = 0.0;
		spacingX = 1.0;
		spacingY = 1.0;
	}
	else
	{
            if (m_spCrntReconImg)
            {
                USHORT_ImageType::PointType tmpOrigin = m_spCrntReconImg->GetOrigin();
                USHORT_ImageType::SpacingType tmpSpacing = m_spCrntReconImg->GetSpacing();
                originX = tmpOrigin[0];
                originY = tmpOrigin[1];
                spacingX = tmpSpacing[0];
                spacingY = tmpSpacing[1];
            }
	}

        //cout << "check 6" << endl;

	QVector<double> vPos;
	if (ui.radioButton_Profile_Hor->isChecked())
	{
		for (int i = 0; i<rowSize ; i++)
		{
			vPos.push_back(originX + i*spacingX);
		}
	}
	else
	{
		for (int i = 0; i<rowSize ; i++)
		{
			vPos.push_back(originY + i*spacingY);
		}
	}

	QVector<double> vProfile;
	if (ui.radioButton_Profile_Hor->isChecked())
	{
		pYKImg->GetProfileData(vProfile, DIRECTION_HOR);
	}
	else
	{
		pYKImg->GetProfileData(vProfile, DIRECTION_VER);
	}

		//int i = fixedY;
	for (int i = 0; i<rowSize ; i++)
	{
		qreal tmpVal1 = (qreal)(vPos[i]);
		m_pTableModel->setItem(i,0,new QStandardItem(QString("%1").arg(tmpVal1)));

		qreal tmpVal2 = (qreal)(vProfile[i]) / fMultiPlyFactor + fMinValue;
		m_pTableModel->setItem(i,1,new QStandardItem(QString("%1").arg(tmpVal2)));
	}

        ui.tableViewReconImgProfile->setModel(m_pTableModel); //also for proj

        //cout << "check 7" << endl;
	SLT_DrawGraph();
}


bool CbctRecon::IsFileNameOrderCorrect(vector<string>& vFileNames)
{
	//regardless of whether number or hexa codes,
	//we can convert it from number to hexa number and compare the order

	int size = vFileNames.size();

	if (size < 2)
		return false;

	int* arrNum = new int [size];


	QString crntFilePath;

	for (int i = 0 ; i<size ; i++)
	{
		crntFilePath = vFileNames.at(i).c_str();
		QFileInfo fileInfo = QFileInfo(crntFilePath);
		QDir dir = fileInfo.absoluteDir();

		QString newBaseName = HexStr2IntStr(fileInfo.baseName());
		arrNum[i]= newBaseName.toInt();
	}

	bool bOrderOK = true;
	for (int i = 0 ; i<size-1 ; i++)
	{
		if (arrNum[i] >= arrNum[i+1])
			bOrderOK = false;
	}

	return bOrderOK;

}

//Mouse Left Click
void CbctRecon::SLT_CalculateROI_Recon()
{
	if (m_dspYKReconImage == NULL)
		return;

	if (!m_spCrntReconImg)
		return;

	double dspWidth = ui.labelReconImage->width();
	double dspHeight = ui.labelReconImage->height();

	int dataWidth = m_dspYKReconImage->m_iWidth;
	int dataHeight = m_dspYKReconImage->m_iHeight;
	if (dataWidth*dataHeight == 0)
		return;

	//int crntIdx = ui.spinBoxImgIdx->value();
	//These are displayed data (just index data)

	int dataX = qRound(ui.labelReconImage->x / dspWidth * dataWidth);
	int dataY = qRound(ui.labelReconImage->y / dspHeight * dataHeight);
	int dataZ =  ui.spinBoxReconImgSliceNo->value();

	double originX = (double)(m_spCrntReconImg->GetOrigin()[0]);
	double originY = (double)(m_spCrntReconImg->GetOrigin()[1]);
	double originZ = (double)(m_spCrntReconImg->GetOrigin()[2]);

	double spacingX = (double)(m_spCrntReconImg->GetSpacing()[0]);
	double spacingY = (double)(m_spCrntReconImg->GetSpacing()[1]);
	double spacingZ = (double)(m_spCrntReconImg->GetSpacing()[2]);

	double posX =  originX + dataX*spacingX;
	double posY =  originY + dataY*spacingY;
	double posZ =  originZ + dataZ*spacingZ;

	//ui.lineEdit_ForcedProbePosX->setText(QString("%1").arg(dataX));
	//ui.lineEdit_ForcedProbePosY->setText(QString("%1").arg(dataY));
	//ui.lineEdit_ForcedProbePosZ->setText(QString("%1").arg(dataZ));

	QString tmpStr1,tmpStr2, tmpStr3;
	tmpStr1.sprintf("%4.2f", posX);
	tmpStr2.sprintf("%4.2f", posY);
	tmpStr3.sprintf("%4.2f", posZ);
	ui.lineEdit_ForcedProbePosX->setText(tmpStr1);
	ui.lineEdit_ForcedProbePosY->setText(tmpStr2);
	ui.lineEdit_ForcedProbePosZ->setText(tmpStr3);

	m_dspYKReconImage->SetProfileProbePos(dataX, dataY);
	if (ui.radioButton_Profile_Hor->isChecked())
	{
		m_dspYKReconImage->m_bDrawProfileX = true;
		m_dspYKReconImage->m_bDrawProfileY = false;
	}
	else
	{
		m_dspYKReconImage->m_bDrawProfileX = false;
		m_dspYKReconImage->m_bDrawProfileY = true;
	}

	// m_dspYKReconImage value itself
	int ROI_size = ui.lineEdit_ROI_size->text().toInt();
	if (ROI_size < 0)
		return;

	if (ROI_size > 0)
	{
		m_dspYKReconImage->setROI(qRound(dataX - ROI_size/2.0),qRound( dataY - ROI_size/2.0),qRound(dataX + ROI_size/2.0), qRound(dataY + ROI_size/2.0));
		m_dspYKReconImage->CalcImageInfo_ROI();
		m_dspYKReconImage->DrawROIOn(true);

		//m_dspYKImgProj->m_pData[iNumWidth + width*iNumHeight] = (unsigned short)((tmpVal- m_fProjImgValueMin)*m_multiplyFactor);

		QString strMean;
		strMean.sprintf("%5.2f", m_dspYKReconImage->m_fPixelMean_ROI);
		QString strSD;
		strSD.sprintf("%5.2f", m_dspYKReconImage->m_fPixelSD_ROI);
		ui.lineEdit_ROI_mean->setText(strMean);
		ui.lineEdit_ROI_SD->setText(strSD);
	}
	else
		m_dspYKReconImage->DrawROIOn(false);

	SLT_DrawReconImage();
}

void CbctRecon::SLT_CalculateROI_Proj()
{
	if (m_dspYKImgProj == NULL)
		return;

	double dspWidth = ui.labelImageRaw->width();
	double dspHeight = ui.labelImageRaw->height();

	int dataWidth = m_dspYKImgProj->m_iWidth;
	int dataHeight = m_dspYKImgProj->m_iHeight;
	if (dataWidth*dataHeight == 0)
		return;

	//int crntIdx = ui.spinBoxImgIdx->value();
	//These are displayed data (just index data)
	int dataX = qRound(ui.labelImageRaw->x / dspWidth * dataWidth);
	int dataY = qRound(ui.labelImageRaw->y / dspHeight * dataHeight);
	int dataZ =  ui.spinBoxImgIdx->value();


	//double originX = (double)(m_spProjImg3D->GetOrigin()[0]);//0
	//double originY = (double)(m_spProjImg3D->GetOrigin()[1]);//0
	//double originZ = (double)(m_spProjImg3D->GetOrigin()[2]);//0

	//double spacingX = (double)(m_spProjImg3D->GetSpacing()[0]);//1
	//double spacingY = (double)(m_spProjImg3D->GetSpacing()[1]);//1
	//double spacingZ = (double)(m_spProjImg3D->GetSpacing()[2]);	//1

	double originX = 0.0;//0
	double originY = 0.0;//0
	double originZ = 0.0;//0

	double spacingX = 1.0;//1
	double spacingY = 1.0;//1
	double spacingZ = 1.0;	//1

	double posX =  originX + dataX*spacingX;
	double posY =  originY + dataY*spacingY;
	double posZ =  originZ + dataZ*spacingZ;

	ui.lineEdit_ForcedProbePosX->setText(QString("%1").arg(posX));
	ui.lineEdit_ForcedProbePosY->setText(QString("%1").arg(posY));
	ui.lineEdit_ForcedProbePosZ->setText(QString("%1").arg(posZ));

	//ui.lineEdit_ForcedProbePosX->setText(QString("%1").arg(dataX));
	//ui.lineEdit_ForcedProbePosY->setText(QString("%1").arg(dataY));

	m_dspYKImgProj->SetProfileProbePos(dataX, dataY);
	//m_dspYKImgProj->m_bDrawProfileX = true;
	//m_dspYKImgProj->m_bDrawProfileY = true;

	if (ui.radioButton_Profile_Hor->isChecked())
	{
		m_dspYKImgProj->m_bDrawProfileX = true;
		m_dspYKImgProj->m_bDrawProfileY = false;
	}
	else
	{
		m_dspYKImgProj->m_bDrawProfileX = false;
		m_dspYKImgProj->m_bDrawProfileY = true;
	}

	// m_dspYKReconImage value itself
	int ROI_size = ui.lineEdit_ROI_size->text().toInt();
	if (ROI_size < 0)
		return;

	if (ROI_size > 0)
	{
		m_dspYKImgProj->setROI(qRound(dataX - ROI_size/2.0),qRound( dataY - ROI_size/2.0),qRound(dataX + ROI_size/2.0), qRound(dataY + ROI_size/2.0));
		m_dspYKImgProj->CalcImageInfo_ROI();
		m_dspYKImgProj->DrawROIOn(true);
		QString strMean;
		//strMean.sprintf("%5.1f", m_dspYKImgProj->m_fPixelMean_ROI);
		strMean.sprintf("%5.2f", (m_dspYKImgProj->m_fPixelMean_ROI / m_multiplyFactor) + m_fProjImgValueMin);

		QString strSD;
		strSD.sprintf("%5.2f", m_dspYKImgProj->m_fPixelSD_ROI/ m_multiplyFactor);
		ui.lineEdit_ROI_mean->setText(strMean);
		ui.lineEdit_ROI_SD->setText(strSD);
	}
	else
		m_dspYKImgProj->DrawROIOn(false);

	SLT_DrawProjImages();
}

bool CbctRecon::LoadShortImageToUshort(QString& strPath, USHORT_ImageType::Pointer& pUshortImage)
{
  typedef itk::ImageFileReader<SHORT_ImageType> ReaderType;
  ReaderType::Pointer reader = ReaderType::New();

  //QString fileName = QFileDialog::getOpenFileName(this, "Open Image","", "Plan CT file (*.mha)",0,0);

  if (strPath.length() < 1)
	return false;

  reader->SetFileName(strPath.toLocal8Bit().constData());
  reader->Update();

  //Figure out whether this is NKI
  typedef itk::MinimumMaximumImageCalculator <SHORT_ImageType> ImageCalculatorFilterType;
  ImageCalculatorFilterType::Pointer imageCalculatorFilter = ImageCalculatorFilterType::New();
  imageCalculatorFilter->SetImage(reader->GetOutput());
  imageCalculatorFilter->Compute();

  double minVal0 = (double)(imageCalculatorFilter->GetMinimum());
  double maxVal0 = (double)(imageCalculatorFilter->GetMaximum());

  cout << "Original Min and Max Values are	" << minVal0 << "	" << maxVal0 << endl;

  bool bNKI = false;
  if (minVal0 > -600) //impossible for normal Short image. IN NKI, always -512. don't know why
  {
	  bNKI = true;
  }

  //Thresholding
  typedef itk::ThresholdImageFilter <SHORT_ImageType> ThresholdImageFilterType;
  ThresholdImageFilterType::Pointer thresholdFilter = ThresholdImageFilterType::New();

  if (!bNKI)
  {
	  thresholdFilter->SetInput(reader->GetOutput());
	  thresholdFilter->ThresholdOutside(-1024, 3072); //--> 0 ~ 4095
	  thresholdFilter->SetOutsideValue(-1024);
	  thresholdFilter->Update();
  }
  else
  {
	  thresholdFilter->SetInput(reader->GetOutput());
	  thresholdFilter->ThresholdOutside(0, 4095); //--> 0 ~ 4095
	  thresholdFilter->SetOutsideValue(0);
	  thresholdFilter->Update();
  }

  imageCalculatorFilter->SetImage(thresholdFilter->GetOutput());
  imageCalculatorFilter->Compute();

  double minVal= (double)(imageCalculatorFilter->GetMinimum());
  double maxVal= (double)(imageCalculatorFilter->GetMaximum());

  cout <<"Current Min and Max Values are	" << minVal << "	" << maxVal << endl;

  //Min value is always 3024 --> outside the FOV
  //USHORT_PixelType outputMinVal = (USHORT_PixelType)(minVal - minVal);
  //USHORT_PixelType outputMaxVal = (USHORT_PixelType) (maxVal - minVal);
  //USHORT_PixelType outputMinVal = (USHORT_PixelType)(minVal + 1024);
  //USHORT_PixelType outputMaxVal = (USHORT_PixelType) (maxVal + 1024);


  USHORT_PixelType outputMinVal, outputMaxVal;
  if (!bNKI)
  {
	  outputMinVal = (USHORT_PixelType)(minVal + 1024);
	  outputMaxVal = (USHORT_PixelType)(maxVal + 1024);
  }
  else
  {
	  outputMinVal = (USHORT_PixelType)(minVal);
	  outputMaxVal = (USHORT_PixelType)(maxVal);

  }

  typedef itk::RescaleIntensityImageFilter<SHORT_ImageType, USHORT_ImageType> RescaleFilterType;
  RescaleFilterType::Pointer spRescaleFilter = RescaleFilterType::New();
  spRescaleFilter->SetInput(thresholdFilter->GetOutput());
  spRescaleFilter->SetOutputMinimum(outputMinVal);
  spRescaleFilter->SetOutputMaximum(outputMaxVal);
  spRescaleFilter->Update();
  pUshortImage = spRescaleFilter->GetOutput();

   return true;
}

void CbctRecon::SLT_LoadPlanCT_mha() //m_spRecon -->m_spRefCT
{
	//typedef itk::ImageFileReader<SHORT_ImageType> ReaderType;
	//ReaderType::Pointer reader = ReaderType::New();

	QString fileName = QFileDialog::getOpenFileName(this, "Open Image",m_strPathDirDefault, "Plan CT file (*.mha)",0,0);

	if (fileName.length() < 1)
		return;

	//reader->SetFileName(fileName.toLocal8Bit().constData());
	//reader->Update();


	////Thresholding

	//typedef itk::ThresholdImageFilter <SHORT_ImageType> ThresholdImageFilterType;

	//ThresholdImageFilterType::Pointer thresholdFilter = ThresholdImageFilterType::New();
	//thresholdFilter->SetInput(reader->GetOutput());
	//thresholdFilter->ThresholdOutside(-1024, 3072); //--> 0 ~ 4095
	//thresholdFilter->SetOutsideValue(-1024);
	//thresholdFilter->Update();

	//typedef itk::MinimumMaximumImageCalculator <SHORT_ImageType>
	//	ImageCalculatorFilterType;

	//ImageCalculatorFilterType::Pointer imageCalculatorFilter
	//	= ImageCalculatorFilterType::New ();
	//imageCalculatorFilter->SetImage(thresholdFilter->GetOutput());
	//imageCalculatorFilter->Compute();

	//double minVal= (double)(imageCalculatorFilter->GetMinimum());
	//double maxVal= (double)(imageCalculatorFilter->GetMaximum());

	//cout <<"Min and Max Values are	" << minVal << "	" << maxVal << endl;

	////Min value is always 3024 --> outside the FOV
	////USHORT_PixelType outputMinVal = (USHORT_PixelType)(minVal - minVal);
	////USHORT_PixelType outputMaxVal = (USHORT_PixelType) (maxVal - minVal);
	//USHORT_PixelType outputMinVal = (USHORT_PixelType)(minVal + 1024);
	//USHORT_PixelType outputMaxVal = (USHORT_PixelType) (maxVal + 1024);

	//typedef itk::RescaleIntensityImageFilter<SHORT_ImageType, USHORT_ImageType> RescaleFilterType;
	//RescaleFilterType::Pointer spRescaleFilter = RescaleFilterType::New();
	//spRescaleFilter->SetInput(thresholdFilter->GetOutput());
	//spRescaleFilter->SetOutputMinimum(outputMinVal);
	//spRescaleFilter->SetOutputMaximum(outputMaxVal);

	//spRescaleFilter->Update();
	////m_spReconImg = spRescaleFilter->GetOutput();
 //   m_spRefCTImg = spRescaleFilter->GetOutput();

  if (!LoadShortImageToUshort(fileName, m_spRefCTImg))
  {
	cout << "error! in LoadShortImageToUshort" << endl;
  }

	typedef itk::MinimumMaximumImageCalculator <USHORT_ImageType>
		ImageCalculatorFilterType2;

	ImageCalculatorFilterType2::Pointer imageCalculatorFilter2
		= ImageCalculatorFilterType2::New ();
	//imageCalculatorFilter2->SetImage(m_spReconImg);
        imageCalculatorFilter2->SetImage(m_spRefCTImg);
	imageCalculatorFilter2->Compute();

	double minVal2 = (double)(imageCalculatorFilter2->GetMinimum());
	double maxVal2 = (double)(imageCalculatorFilter2->GetMaximum());

	cout <<"Min and Max Values are	" << minVal2 << "	" << maxVal2 << endl;

	//Update UI
	USHORT_ImageType::SizeType imgDim = m_spRefCTImg->GetBufferedRegion().GetSize();
	USHORT_ImageType::SpacingType spacing = m_spRefCTImg->GetSpacing();

	cout << "Image Dimension:	" << imgDim[0] << "	" << imgDim[1] << "	" << imgDim[2] << endl;
	cout << "Image Spacing (mm):	" << spacing[0] << "	" << spacing[1] << "	" << spacing[2] << endl;

	RegisterImgDuplication(REGISTER_REF_CT, REGISTER_MANUAL_RIGID);
	m_spCrntReconImg = m_spRefCTImg;

	ui.lineEdit_Cur3DFileName->setText(fileName);
	m_dspYKReconImage->CreateImage(imgDim[0], imgDim[1],0);

	ui.spinBoxReconImgSliceNo->setMinimum(0);
	ui.spinBoxReconImgSliceNo->setMaximum(imgDim[2]-1);
	int initVal = qRound((imgDim[2]-1)/2.0);

	SLT_InitializeGraphLim();
	ui.spinBoxReconImgSliceNo->setValue(initVal); //DrawRecon Imge is called
	ui.radioButton_graph_recon->setChecked(true);

	//Duplication for registration. Starting point is manual Rigid CT image
	/*typedef itk::ImageDuplicator<USHORT_ImageType> DuplicatorType;
	DuplicatorType::Pointer duplicator = DuplicatorType::New();
	duplicator->SetInputImage(m_spRefCTImg);
	duplicator->Update();
	m_spManualRigidCT = duplicator->GetOutput();*/
	//Duplication for : End


	/*if (!m_spReconImg)
	{
	m_spReconImg = m_spRefCTImg;
	SLT_DrawReconImage();
	}*/

}

void CbctRecon::SLT_GoForcedProbePos()// when forced probe button was clicked
{
	double fForcedProbePosX = ui.lineEdit_ForcedProbePosX->text().toDouble(); //data is the reference
	double fForcedProbePosY = ui.lineEdit_ForcedProbePosY->text().toDouble();
	double fForcedProbePosZ = ui.lineEdit_ForcedProbePosZ->text().toDouble();

	double dspWidth =0.0;
	double dspHeight=0.0;
	int dataWidth =0;
	int dataHeight =0;

	//First change the scene acc to Z value
	double originX, originY, originZ;
	double spacingX, spacingY, spacingZ;
	int sliceIdx = 0;

	int dataX, dataY;

	if (ui.radioButton_graph_proj->isChecked())
	{
		if (!m_spProjImg3DFloat)
			return;

		originX = m_spProjImg3DFloat->GetOrigin()[0];
		originY = m_spProjImg3DFloat->GetOrigin()[1];
		originZ = m_spProjImg3DFloat->GetOrigin()[2];

		spacingX = m_spProjImg3DFloat->GetSpacing()[0];
		spacingY = m_spProjImg3DFloat->GetSpacing()[1];
		spacingZ = m_spProjImg3DFloat->GetSpacing()[2];

		sliceIdx = qRound((fForcedProbePosZ - originZ)/spacingZ);

		if (sliceIdx < 0 || sliceIdx >= m_iImgCnt)
			return;

		ui.spinBoxImgIdx->setValue(sliceIdx); //Draw function is called

		dspWidth = ui.labelImageRaw->width();
		dspHeight = ui.labelImageRaw->height();

		dataWidth = m_dspYKImgProj->m_iWidth;
		dataHeight = m_dspYKImgProj->m_iHeight;

		dataX = qRound((fForcedProbePosX - originX)/spacingX);
		dataY = qRound((fForcedProbePosY - originY)/spacingY);

		if (dataX < 0 || dataX >= dataWidth ||
			dataY < 0 || dataY >= dataHeight)
			return;

		ui.labelImageRaw->x = qRound(dataX / (double)dataWidth * dspWidth);
		ui.labelImageRaw->y = qRound(dataY / (double)dataHeight * dspHeight);

		SLT_CalculateROI_Proj();
	}
	else if (ui.radioButton_graph_recon->isChecked())
	{
		if(!m_spCrntReconImg)
			return;

		originX = m_spCrntReconImg->GetOrigin()[0];
		originY = m_spCrntReconImg->GetOrigin()[1];
		originZ = m_spCrntReconImg->GetOrigin()[2];

		spacingX = m_spCrntReconImg->GetSpacing()[0];
		spacingY = m_spCrntReconImg->GetSpacing()[1];
		spacingZ = m_spCrntReconImg->GetSpacing()[2];

		sliceIdx = qRound((fForcedProbePosZ - originZ)/spacingZ);

		if (sliceIdx < 0 || sliceIdx >= m_spCrntReconImg->GetBufferedRegion().GetSize()[2])
			return;

		ui.spinBoxReconImgSliceNo->setValue(sliceIdx); //Draw function is called


		dspWidth = ui.labelReconImage->width();
		dspHeight = ui.labelReconImage->height();

		dataWidth = m_dspYKReconImage->m_iWidth;
		dataHeight = m_dspYKReconImage->m_iHeight;


		dataX = qRound((fForcedProbePosX - originX)/spacingX);
		dataY = qRound((fForcedProbePosY - originY)/spacingY);

		if (dataX < 0 || dataX >= dataWidth ||
			dataY < 0 || dataY >= dataHeight)
			return;

		ui.labelReconImage->x = qRound(dataX / (double)dataWidth * dspWidth);
		ui.labelReconImage->y = qRound(dataY / (double)dataHeight * dspHeight);

		SLT_CalculateROI_Recon();
	}
}

void CbctRecon::PostApplyFOVDispParam()
{
	if (m_dspYKReconImage == NULL)
		return;

        float physPosX = ui.lineEdit_PostFOV_X->text().toFloat();
        float physPosY = ui.lineEdit_PostFOV_Y->text().toFloat();

        float physRadius = ui.lineEdit_PostFOV_R->text().toFloat();
        float physTablePosY = ui.lineEdit_PostTablePosY->text().toFloat();

	USHORT_ImageType::PointType origin = m_spCrntReconImg->GetOrigin();
	USHORT_ImageType::SpacingType spacing = m_spCrntReconImg->GetSpacing();
	USHORT_ImageType::SizeType size = m_spCrntReconImg->GetBufferedRegion().GetSize();


	int pixPosX = qRound((physPosX - (double)origin[0]) /(double)spacing[0]);
	int pixPosY = qRound((physPosY - (double)origin[1]) /(double)spacing[1]);

	int pixRadius = qRound(physRadius / (double)spacing[0]);

	int pixWidth = qRound((qreal) size[0]);
	int pixHeight = qRound((qreal) size[1]);

	int pixTableY = qRound((physTablePosY - (double)origin[1])/(double)spacing[1]);


	if (pixPosX >= 0  && pixPosY < m_dspYKReconImage->m_iWidth && pixPosY >= 0
		&& pixPosY < m_dspYKReconImage->m_iHeight && pixRadius>0 && pixRadius <  m_dspYKReconImage->m_iWidth
		&& pixTableY >=0 && pixTableY <  m_dspYKReconImage->m_iHeight)
	{
		m_dspYKReconImage->m_ptFOVCenter.setX(pixPosX);// data pos
		m_dspYKReconImage->m_ptFOVCenter.setY(pixPosY);

		m_dspYKReconImage->m_iFOVRadius = pixRadius;
		m_dspYKReconImage->m_iTableTopPos = pixTableY;
	}
}


void CbctRecon::SLT_PostApplyFOVDispParam()
{
	//PostApplyFOVDispParam();
	SLT_DrawReconImage();
}


// mm
void CbctRecon::CropFOV3D(USHORT_ImageType::Pointer& sp_Img, float physPosX, float physPosY, float physRadius, float physTablePosY)
{
    if (!sp_Img)
        return;
    //1) region iterator, set 0 for all pixels outside the circle and below the table top, based on physical position
    USHORT_ImageType::PointType origin = sp_Img->GetOrigin();
    USHORT_ImageType::SpacingType spacing = sp_Img->GetSpacing();
    USHORT_ImageType::SizeType size = sp_Img->GetBufferedRegion().GetSize();

    //itk::ImageSliceConstIteratorWithIndex<OutputImageType> it (m_spReconImg, m_spReconImg->GetRequestedRegion());
    itk::ImageSliceIteratorWithIndex<USHORT_ImageType> it(sp_Img, sp_Img->GetRequestedRegion());

    //ImageSliceConstIteratorWithIndex<ImageType> it( image, image->GetRequestedRegion() );
    USHORT_ImageType::SizeType imgSize = sp_Img->GetRequestedRegion().GetSize(); //1016x1016 x z

    int width = imgSize[0];
    int height = imgSize[1];

    it.SetFirstDirection(0); //x?
    it.SetSecondDirection(1); //y?
    it.GoToBegin();

    int iNumSlice = 0;
    int iPosX = 0;
    int iPosY = 0;

    int i = 0;//height
    int j = 0; // width

    double crntPhysX = 0.0;
    double crntPhysY = 0.0;

    while (!it.IsAtEnd())
    {
        iPosY = 0;
        while (!it.IsAtEndOfSlice())
        {
            iPosX = 0;
            while (!it.IsAtEndOfLine())
            {
                //Calculate physical position

                crntPhysX = iPosX*(double)spacing[0] + (double)origin[0];
                crntPhysY = iPosY*(double)spacing[1] + (double)origin[1];

                if (pow(crntPhysX - physPosX, 2.0) + pow(crntPhysY - physPosY, 2.0) >= pow(physRadius, 2.0))
                {
                    //(*it) = (unsigned short)0; //air value
                    it.Set(0);
                }

                if (crntPhysY >= physTablePosY)
                {
                    it.Set(0);
                }
                ++it;
                iPosX++;
            }
            it.NextLine();
            iPosY++;
        }
        it.NextSlice();
        iNumSlice++;
    }
}


void CbctRecon::SLT_DoPostProcessing()
{
	if (!m_spCrntReconImg)
		return;
	//1) region iterator, set 0 for all pixels outside the circle and below the table top, based on physical position

	float physPosX = ui.lineEdit_PostFOV_X->text().toFloat();
        float physPosY = ui.lineEdit_PostFOV_Y->text().toFloat();

        float physRadius = ui.lineEdit_PostFOV_R->text().toFloat();
        float physTablePosY = ui.lineEdit_PostTablePosY->text().toFloat();

        CropFOV3D(m_spCrntReconImg, physPosX, physPosY, physRadius, physTablePosY);

	SLT_DrawReconImage();
}

void CbctRecon::SLT_PostProcCropInv()
{
    if (!m_spCrntReconImg)
        return;
    //1) region iterator, set 0 for all pixels outside the circle and below the table top, based on physical position

    double physPosX = ui.lineEdit_PostFOV_X->text().toDouble();
    double physPosY = ui.lineEdit_PostFOV_Y->text().toDouble();

    double physRadius = ui.lineEdit_PostFOV_R->text().toDouble();
    double physTablePosY = ui.lineEdit_PostTablePosY->text().toDouble();

    USHORT_ImageType::PointType origin = m_spCrntReconImg->GetOrigin();
    USHORT_ImageType::SpacingType spacing = m_spCrntReconImg->GetSpacing();
    USHORT_ImageType::SizeType size = m_spCrntReconImg->GetBufferedRegion().GetSize();

    //itk::ImageSliceConstIteratorWithIndex<OutputImageType> it (m_spReconImg, m_spReconImg->GetRequestedRegion());
    itk::ImageSliceIteratorWithIndex<USHORT_ImageType> it(m_spCrntReconImg, m_spCrntReconImg->GetRequestedRegion());

    //ImageSliceConstIteratorWithIndex<ImageType> it( image, image->GetRequestedRegion() );
    USHORT_ImageType::SizeType imgSize = m_spCrntReconImg->GetRequestedRegion().GetSize(); //1016x1016 x z

    int width = imgSize[0];
    int height = imgSize[1];

    it.SetFirstDirection(0); //x?
    it.SetSecondDirection(1); //y?
    it.GoToBegin();

    int iNumSlice = 0;
    int iPosX = 0;
    int iPosY = 0;

    int i = 0;//height
    int j = 0; // width

    double crntPhysX = 0.0;
    double crntPhysY = 0.0;

    while (!it.IsAtEnd())
    {
        iPosY = 0;
        while (!it.IsAtEndOfSlice())
        {
            iPosX = 0;
            while (!it.IsAtEndOfLine())
            {
                //Calculate physical position

                crntPhysX = iPosX*(double)spacing[0] + (double)origin[0];
                crntPhysY = iPosY*(double)spacing[1] + (double)origin[1];

                //crop inside of FOV
                if (pow(crntPhysX - physPosX, 2.0) + pow(crntPhysY - physPosY, 2.0) < pow(physRadius, 2.0))
                {
                    it.Set(0);
                }
                //if (crntPhysY >= physTablePosY) //table cropping = same
                //{
                //    it.Set(0);
                //}
                ++it;
                iPosX++;
            }
            it.NextLine();
            iPosY++;
        }
        it.NextSlice();
        iNumSlice++;
    }

    SLT_DrawReconImage();

}


void CbctRecon::SLT_ExportReconUSHORT()
{
	if (!m_spCrntReconImg)
	{
		cout <<" no image to export" << endl;
		return;
	}

	QString strPath = QFileDialog::getSaveFileName(this,"Save Image","", "unsigned short meta image (*.mha)",0,0);
	if (strPath.length()<=1)
		return;

	typedef itk::ImageFileWriter<USHORT_ImageType> WriterType;
	WriterType::Pointer writer = WriterType::New();
	writer->SetFileName(strPath.toLocal8Bit().constData());
	writer->SetUseCompression(true); //not exist in original code (rtkfdk)
	writer->SetInput(m_spCrntReconImg);

	cout << "Writing is under progress...: " << strPath.toLocal8Bit().constData() << endl;
	writer->Update();
	cout << "Writing was successfully done" << endl;

	QString msgStr = QString("USHORT File Writing was successfully done");
	QMessageBox::information(this, "Procedure Done",msgStr);
}

void CbctRecon::ExportReconSHORT_HU(USHORT_ImageType::Pointer& spUsImage, QString& outputFilePath)
{
    /*if (!m_spCrntReconImg)
    {
    cout << " no image to export" << endl;
    return;
    }
    */
    if (!spUsImage)
    {
        cout << " no image to export" << endl;
        return;
    }

    typedef itk::ImageDuplicator< USHORT_ImageType > DuplicatorType;
    DuplicatorType::Pointer duplicator = DuplicatorType::New();
    duplicator->SetInputImage(spUsImage);
    duplicator->Update();
    USHORT_ImageType::Pointer clonedReconImage = duplicator->GetOutput();
    SHORT_ImageType::Pointer clonedReconImageSHORT;

    int tissueCBCT = 0;
    int tissueCT_USHORT = 0;
    int HighDensityCBCT = 0;
    int HighDensityHU = 0;
    bool bCBCT2HU_mapping = false;
    int crntTissueVal = 0;

    typedef itk::ThresholdImageFilter <USHORT_ImageType> ThresholdImageFilterType;
    ThresholdImageFilterType::Pointer thresholdFilterAbove = ThresholdImageFilterType::New();
	thresholdFilterAbove->SetInput(clonedReconImage);
	thresholdFilterAbove->ThresholdAbove(4095);
	thresholdFilterAbove->SetOutsideValue(4095);
	
	ThresholdImageFilterType::Pointer thresholdFilterBelow = ThresholdImageFilterType::New();
	thresholdFilterBelow->SetInput(thresholdFilterAbove->GetOutput());
	thresholdFilterBelow->ThresholdBelow(0);
	thresholdFilterBelow->SetOutsideValue(0);
	thresholdFilterBelow->Update();
	
	////thresholdFilter->ThresholdOutside(0, 4096); //--> 0 ~ 4095
	//
	//thresholdFilter->Set
	////	thresholdFilter->ThresholdOutside(-1024, 3072); //--> 0 ~ 4095
	//thresholdFilter->SetOutsideValue(0);
	//thresholdFilter->Update();

    typedef itk::MinimumMaximumImageCalculator <USHORT_ImageType> ImageCalculatorFilterType;
    ImageCalculatorFilterType::Pointer imageCalculatorFilter = ImageCalculatorFilterType::New();
    imageCalculatorFilter->SetImage(thresholdFilterBelow->GetOutput());
    imageCalculatorFilter->Compute();
    double minVal = (double)(imageCalculatorFilter->GetMinimum());
    double maxVal = (double)(imageCalculatorFilter->GetMaximum());
    //cout << "Min and Max Values are	" << minVal << "	" << maxVal << endl; //should be 0 and 4096
    //cout <<"Min and Max Values are	" << minVal << "	" << maxVal << endl;

    //Min value is always 3024 --> outside the FOV
    SHORT_PixelType outputMinVal = (SHORT_PixelType)(minVal - 1024);
    SHORT_PixelType outputMaxVal = (SHORT_PixelType)(maxVal - 1024);

    //USHORT_PixelType outputMinVal = (USHORT_PixelType)(minVal - minVal);
    //USHORT_PixelType outputMaxVal = (USHORT_PixelType) (maxVal - minVal);

    typedef itk::RescaleIntensityImageFilter<USHORT_ImageType, SHORT_ImageType> RescaleFilterType;
    RescaleFilterType::Pointer spRescaleFilter = RescaleFilterType::New();
    spRescaleFilter->SetInput(thresholdFilterBelow->GetOutput());
    spRescaleFilter->SetOutputMinimum(outputMinVal);
    spRescaleFilter->SetOutputMaximum(outputMaxVal);

    /*typedef itk::RescaleIntensityImageFilter<SHORT_ImageType, USHORT_ImageType> RescaleFilterType;
    RescaleFilterType::Pointer spRescaleFilter = RescaleFilterType::New();
    spRescaleFilter->SetInput(thresholdFilter->GetOutput());
    spRescaleFilter->SetOutputMinimum(outputMinVal);
    spRescaleFilter->SetOutputMaximum(outputMaxVal);	*/
    spRescaleFilter->Update();
    //clonedReconImageSHORT = spRescaleFilter->GetOutput();

    //waterHU = 1024;

    typedef itk::AddImageFilter <SHORT_ImageType, SHORT_ImageType, SHORT_ImageType> AddImageFilterType;
    AddImageFilterType::Pointer addImageFilter = AddImageFilterType::New();
    addImageFilter->SetInput1(spRescaleFilter->GetOutput());

    int addingVal = 0;//1024-680
    //if (bCBCT2HU_mapping)
    //{
    //    addingVal = tissueCT_USHORT - tissueCBCT;//1024-680
    //}
    //else
    //{

    //}
    addImageFilter->SetConstant2(addingVal);
    addImageFilter->Update();
    //m_spReconImg = spRescaleFilter->GetOutput();

    typedef itk::MinimumMaximumImageCalculator <SHORT_ImageType>
        ImageCalculatorFilterType2;

    ImageCalculatorFilterType2::Pointer imageCalculatorFilter2
        = ImageCalculatorFilterType2::New();
    imageCalculatorFilter2->SetImage(addImageFilter->GetOutput());
    imageCalculatorFilter2->Compute();

    double minVal2 = (double)(imageCalculatorFilter2->GetMinimum());
    double maxVal2 = (double)(imageCalculatorFilter2->GetMaximum());

    //cout << "Short image Min and Max Values are	" << minVal2 << "	" << maxVal2 << endl;

    typedef itk::ImageFileWriter<SHORT_ImageType> WriterType;
    WriterType::Pointer writer = WriterType::New();
    writer->SetFileName(outputFilePath.toLocal8Bit().constData());
    //writer->SetUseCompression(true);
    writer->SetUseCompression(false); //for plastimatch
    writer->SetInput(addImageFilter->GetOutput());

    cout << "Writing is under progress...: " << outputFilePath.toLocal8Bit().constData() << endl;
    writer->Update();
    cout << "Writing was successfully done" << endl;

  //  QString msgStr = QString("SHORT File Writing was successfully done");
//    QMessageBox::information(this, "Procedure Done", msgStr);


    //----------------------------------------------------------------------------export DICOM--------------------------------------------

    /*QString strMsg2 = "Do you want to export DICOM slices as well?";
    msgBox.setText(strMsg2);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);

    int res2 = msgBox.exec();

    if (res2 == QMessageBox::Yes)
    {
    cout << "DICOM export is under progress." << endl;
    SHORT_ImageType::Pointer spTmpImgResult = spRescaleFilter->GetOutput();
    ExportDICOM_SHORT(spTmpImgResult);

    cout << "DICOM export is completed." << endl;
    }*/

    return;


}
void CbctRecon::SLT_ExportReconSHORT_HU()
{
    /*if (!m_spCrntReconImg)
    {
    cout << " no image to export" << endl;
    return;
    }
    */
    QString strPath = QFileDialog::getSaveFileName(this, "Save Image", "", "signed short meta image (*.mha)", 0, 0);
    if (strPath.length() <= 1)
        return;
    ExportReconSHORT_HU(m_spCrntReconImg, strPath);
    return;



    //if (!m_spCrntReconImg)
    //{
    //    cout << " no image to export" << endl;
    //    return;
    //}

    //typedef itk::ImageDuplicator< USHORT_ImageType > DuplicatorType;
    //DuplicatorType::Pointer duplicator = DuplicatorType::New();
    //duplicator->SetInputImage(m_spCrntReconImg);
    //duplicator->Update();
    //USHORT_ImageType::Pointer clonedReconImage = duplicator->GetOutput();
    //SHORT_ImageType::Pointer clonedReconImageSHORT;

    ////Convert image first...
    ////to keep the original daa,

    //QString strMsg = "Do you want to set soft-tissue to HU=0?";

    //QMessageBox msgBox;
    //msgBox.setText(strMsg);
    //msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);

    //int res = msgBox.exec();

    //int tissueCBCT = 0;
    //int tissueCT_USHORT = 0;
    //int HighDensityCBCT = 0;
    //int HighDensityHU = 0;
    //bool bCBCT2HU_mapping = false;

    //int crntTissueVal = 0;
    //if (res == QMessageBox::Yes)
    //{
    //    bool ok = false;
    //    //QString text = QInputDialog::getText(this, "Input Dialog","CBCT to CT Mapping Value (e.g. [WaterCBCT#, WaterHU, HighDensityCBCT#, HighDensityHU]",
    //    //QLineEdit::Normal, "775,0,871,370",&ok);

    //    QString text = QInputDialog::getText(this, "Input Dialog", "CBCT to CT Mapping Value",
    //        QLineEdit::Normal, "1012,1036", &ok);

    //    //Parsing
    //    QStringList strlistParam = text.split(",");
    //    if (strlistParam.length() != 2)
    //    {
    //        cout << "Error! 2 mapping values are required." << endl;
    //        return;
    //    }
    //    bCBCT2HU_mapping = true;
    //    //waterCBCT = text.toInt();
    //    tissueCBCT = strlistParam.at(0).toInt();
    //    tissueCT_USHORT = strlistParam.at(1).toInt();
    //    //HighDensityCBCT = strlistParam.at(2).toInt();// bone or something
    //    //HighDensityHU = strlistParam.at(3).toInt();
    //}



    //typedef itk::AddImageFilter<OutputImageType> AddImageFilterType;
    //AddImageFilterType::Pointer addFilter = AddImageFilterType::New();
    //typedef itk::AddImageFilter <OutputImageType, OutputImageType, OutputImageType> AddImageFilterType;
    //AddImageFilterType::Pointer addImageFilter = AddImageFilterType::New();
    //addImageFilter->SetInput1(m_spProjImg3D);

    //Thresholding

    //typedef itk::ThresholdImageFilter <USHORT_ImageType> ThresholdImageFilterType;
    //ThresholdImageFilterType::Pointer thresholdFilter = ThresholdImageFilterType::New();
    //thresholdFilter->SetInput(clonedReconImage);
    //thresholdFilter->ThresholdOutside(0, 4096); //--> 0 ~ 4095
    ////	thresholdFilter->ThresholdOutside(-1024, 3072); //--> 0 ~ 4095
    //thresholdFilter->SetOutsideValue(0);
    //thresholdFilter->Update();

    //typedef itk::MinimumMaximumImageCalculator <USHORT_ImageType> ImageCalculatorFilterType;
    //ImageCalculatorFilterType::Pointer imageCalculatorFilter = ImageCalculatorFilterType::New();
    //imageCalculatorFilter->SetImage(thresholdFilter->GetOutput());
    //imageCalculatorFilter->Compute();
    //double minVal = (double)(imageCalculatorFilter->GetMinimum());
    //double maxVal = (double)(imageCalculatorFilter->GetMaximum());
    //cout << "Min and Max Values are	" << minVal << "	" << maxVal << endl; //should be 0 and 4096
    ////cout <<"Min and Max Values are	" << minVal << "	" << maxVal << endl;

    ////Min value is always 3024 --> outside the FOV
    //SHORT_PixelType outputMinVal = (SHORT_PixelType)(minVal - 1024);
    //SHORT_PixelType outputMaxVal = (SHORT_PixelType)(maxVal - 1024);

    ////USHORT_PixelType outputMinVal = (USHORT_PixelType)(minVal - minVal);
    ////USHORT_PixelType outputMaxVal = (USHORT_PixelType) (maxVal - minVal);

    //typedef itk::RescaleIntensityImageFilter<USHORT_ImageType, SHORT_ImageType> RescaleFilterType;
    //RescaleFilterType::Pointer spRescaleFilter = RescaleFilterType::New();
    //spRescaleFilter->SetInput(thresholdFilter->GetOutput());
    //spRescaleFilter->SetOutputMinimum(outputMinVal);
    //spRescaleFilter->SetOutputMaximum(outputMaxVal);

    ///*typedef itk::RescaleIntensityImageFilter<SHORT_ImageType, USHORT_ImageType> RescaleFilterType;
    //RescaleFilterType::Pointer spRescaleFilter = RescaleFilterType::New();
    //spRescaleFilter->SetInput(thresholdFilter->GetOutput());
    //spRescaleFilter->SetOutputMinimum(outputMinVal);
    //spRescaleFilter->SetOutputMaximum(outputMaxVal);	*/
    //spRescaleFilter->Update();
    ////clonedReconImageSHORT = spRescaleFilter->GetOutput();

    ////waterHU = 1024;

    //typedef itk::AddImageFilter <SHORT_ImageType, SHORT_ImageType, SHORT_ImageType> AddImageFilterType;
    //AddImageFilterType::Pointer addImageFilter = AddImageFilterType::New();
    //addImageFilter->SetInput1(spRescaleFilter->GetOutput());

    //int addingVal = 0;//1024-680
    //if (bCBCT2HU_mapping)
    //{
    //    addingVal = tissueCT_USHORT - tissueCBCT;//1024-680
    //}
    //else
    //{

    //}
    //addImageFilter->SetConstant2(addingVal);
    //addImageFilter->Update();
    ////m_spReconImg = spRescaleFilter->GetOutput();

    //typedef itk::MinimumMaximumImageCalculator <SHORT_ImageType>
    //    ImageCalculatorFilterType2;

    //ImageCalculatorFilterType2::Pointer imageCalculatorFilter2
    //    = ImageCalculatorFilterType2::New();
    //imageCalculatorFilter2->SetImage(addImageFilter->GetOutput());
    //imageCalculatorFilter2->Compute();

    //double minVal2 = (double)(imageCalculatorFilter2->GetMinimum());
    //double maxVal2 = (double)(imageCalculatorFilter2->GetMaximum());

    //cout << "Short image Min and Max Values are	" << minVal2 << "	" << maxVal2 << endl;

    //typedef itk::ImageFileWriter<SHORT_ImageType> WriterType;
    //WriterType::Pointer writer = WriterType::New();
    //writer->SetFileName(strPath.toLocal8Bit().constData());
    ////writer->SetUseCompression(true);
    //writer->SetUseCompression(false); //for plastimatch
    //writer->SetInput(addImageFilter->GetOutput());

    //cout << "Writing is under progress...: " << strPath.toLocal8Bit().constData() << endl;
    //writer->Update();
    //cout << "Writing was successfully done" << endl;

    //QString msgStr = QString("SHORT File Writing was successfully done");
    //QMessageBox::information(this, "Procedure Done", msgStr);


    //----------------------------------------------------------------------------export DICOM--------------------------------------------

    /*QString strMsg2 = "Do you want to export DICOM slices as well?";
    msgBox.setText(strMsg2);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);

    int res2 = msgBox.exec();

    if (res2 == QMessageBox::Yes)
    {
    cout << "DICOM export is under progress." << endl;
    SHORT_ImageType::Pointer spTmpImgResult = spRescaleFilter->GetOutput();
    ExportDICOM_SHORT(spTmpImgResult);

    cout << "DICOM export is completed." << endl;
    }*/

    //return;

}

//void CbctRecon::ExportDICOM_SHORT( SHORT_ImageType::Pointer& sp3DshortImage ) //NOT COMPLETED YET!! Export DICOM without Source DICOM is not possible
//{
//	typedef itk::ImageSeriesReader< SHORT_ImageType >ReaderType;
//	typedef itk::GDCMImageIO ImageIOType;
//	typedef itk::GDCMSeriesFileNames InputNamesGeneratorType;
//	typedef itk::NumericSeriesFileNames OutputNamesGeneratorType;
//
//	typedef itk::ImageSeriesWriter< SHORT_ImageType, SHORT_ImageType2D > SeriesWriterType;
//
//	/*typedef itk::IdentityTransform< double, InputDimension > 		TransformType;
//	typedef itk::LinearInterpolateImageFunction< InputImageType, double > InterpolatorType;
//	typedef itk::ResampleImageFilter< InputImageType, InputImageType > ResampleFilterType;
//	typedef itk::ShiftScaleImageFilter< InputImageType, InputImageType > ShiftScaleType;*/
//
//	typedef itk::ShiftScaleImageFilter< SHORT_ImageType, SHORT_ImageType > ShiftScaleType;
//
//	// 1) Read the input series
//	ImageIOType::Pointer gdcmIO = ImageIOType::New();
//	InputNamesGeneratorType::Pointer inputNames = InputNamesGeneratorType::New();
//	inputNames->SetInputDirectory( "D:\\DICOMINPUT" );
//	const ReaderType::FileNamesContainer & filenames = inputNames->GetInputFileNames();
//
//	ReaderType::Pointer reader = ReaderType::New();
//
//	reader->SetImageIO( gdcmIO );
//	reader->SetFileNames( filenames );
//
//	reader->Update();
//
//	ReaderType::DictionaryRawPointer inputDict = (*(reader->GetMetaDataDictionaryArray()))[0];
//	ReaderType::DictionaryArrayType outputArray;
//
//	// To keep the new series in the same study as the original we need
//	// to keep the same study UID. But we need new series and frame of
//	// reference UID's.
//#if ITK_VERSION_MAJOR >= 4
//	gdcm::UIDGenerator suid;
//	std::string seriesUID = suid.Generate();
//	gdcm::UIDGenerator fuid;
//	std::string frameOfReferenceUID = fuid.Generate();
//#else
//	std::string seriesUID = gdcm::Util::CreateUniqueUID( gdcmIO->GetUIDPrefix());
//	std::string frameOfReferenceUID = gdcm::Util::CreateUniqueUID( gdcmIO->GetUIDPrefix());
//#endif
//	std::string studyUID;
//	std::string sopClassUID;
//	itk::ExposeMetaData<std::string>(*inputDict, "0020|000d", studyUID);
//	itk::ExposeMetaData<std::string>(*inputDict, "0008|0016", sopClassUID);
//
//	gdcmIO->KeepOriginalUIDOn();
//
//	SHORT_ImageType::SizeType outputSize = sp3DshortImage->GetBufferedRegion().GetSize();
//	for (unsigned int f = 0; f < outputSize[2]; f++)
//	{
//		// Create a new dictionary for this slice
//		ReaderType::DictionaryRawPointer dict = new ReaderType::DictionaryType;
//
//		// Copy the dictionary from the first slice
//		CopyDictionary (*inputDict, *dict);
//
//		// Set the UID's for the study, series, SOP  and frame of reference
//		itk::EncapsulateMetaData<std::string>(*dict,"0020|000d", studyUID);
//		itk::EncapsulateMetaData<std::string>(*dict,"0020|000e", seriesUID);
//		itk::EncapsulateMetaData<std::string>(*dict,"0020|0052", frameOfReferenceUID);
//
//#if ITK_VERSION_MAJOR >= 4
//		gdcm::UIDGenerator sopuid;
//		std::string sopInstanceUID = sopuid.Generate();
//#else
//		std::string sopInstanceUID = gdcm::Util::CreateUniqueUID( gdcmIO->GetUIDPrefix());
//#endif
//		itk::EncapsulateMetaData<std::string>(*dict,"0008|0018", sopInstanceUID);
//		itk::EncapsulateMetaData<std::string>(*dict,"0002|0003", sopInstanceUID);
//
//		// Change fields that are slice specific
//		itksys_ios::ostringstream value;
//		value.str("");
//		value << f + 1;
//
//		// Image Number
//		itk::EncapsulateMetaData<std::string>(*dict,"0020|0013", value.str());
//
//		// Series Description - Append new description to current series
//		// description
//		std::string oldSeriesDesc;
//		itk::ExposeMetaData<std::string>(*inputDict, "0008|103e", oldSeriesDesc);
//
//		value.str("");
//		value << "Test";
//		/*value << oldSeriesDesc
//			<< ": Resampled with pixel spacing "
//			<< outputSpacing[0] << ", "
//			<< outputSpacing[1] << ", "
//			<< outputSpacing[2];*/
//		// This is an long string and there is a 64 character limit in the
//		// standard
//		unsigned lengthDesc = value.str().length();
//
//		std::string seriesDesc( value.str(), 0,
//			lengthDesc > 64 ? 64
//			: lengthDesc);
//		itk::EncapsulateMetaData<std::string>(*dict,"0008|103e", seriesDesc);
//
//		// Series Number
//		value.str("");
//		value << 1001;
//		itk::EncapsulateMetaData<std::string>(*dict,"0020|0011", value.str());
//
//		// Derivation Description - How this image was derived
//		value.str("");
//		value << "Derivation Description" << endl;
//
//		/*for (int i = 0; i < argc; i++)
//		{
//			value << argv[i] << " ";
//		}*/
//		value << ": " << ITK_SOURCE_VERSION;
//
//		lengthDesc = value.str().length();
//		std::string derivationDesc( value.str(), 0,
//			lengthDesc > 1024 ? 1024
//			: lengthDesc);
//		itk::EncapsulateMetaData<std::string>(*dict,"0008|2111", derivationDesc);
//
//		// Image Position Patient: This is calculated by computing the
//		// physical coordinate of the first pixel in each slice.
//		SHORT_ImageType::PointType position;
//		SHORT_ImageType::IndexType index;
//		index[0] = 0;
//		index[1] = 0;
//		index[2] = f;
//		sp3DshortImage->TransformIndexToPhysicalPoint(index, position);
//
//		value.str("");
//		value << position[0] << "/" << position[1] << "/" << position[2];
//		itk::EncapsulateMetaData<std::string>(*dict,"0020|0032", value.str());
//		// Slice Location: For now, we store the z component of the Image
//		// Position Patient.
//		value.str("");
//		value << position[2];
//		itk::EncapsulateMetaData<std::string>(*dict,"0020|1041", value.str());
//
//
//		SHORT_ImageType::SpacingType outputSpacing = sp3DshortImage->GetSpacing();
//
//		// Slice Thickness: For now, we store the z spacing
//		value.str("");
//		value << outputSpacing[2];
//		itk::EncapsulateMetaData<std::string>(*dict,"0018|0050",value.str());
//		// Spacing Between Slices
//		itk::EncapsulateMetaData<std::string>(*dict,"0018|0088",value.str());
//
//		// Save the dictionary
//		outputArray.push_back(dict);
//	}
//
//	////////////////////////////////////////////////
//	// 4) Shift data to undo the effect of a rescale intercept by the
//	//    DICOM reader
//	std::string interceptTag("0028|1052");
//	typedef itk::MetaDataObject< std::string > MetaDataStringType;
//	itk::MetaDataObjectBase::Pointer entry = (*inputDict)[interceptTag];
//
//	MetaDataStringType::ConstPointer interceptValue =
//		dynamic_cast<const MetaDataStringType *>( entry.GetPointer() ) ;
//
//	int interceptShift = 0;
//	if( interceptValue )
//	{
//		std::string tagValue = interceptValue->GetMetaDataObjectValue();
//		interceptShift = -atoi ( tagValue.c_str() );
//	}
//
//	ShiftScaleType::Pointer shiftScale = ShiftScaleType::New();
//	shiftScale->SetInput( sp3DshortImage);
//	shiftScale->SetShift( interceptShift );
//
//	////////////////////////////////////////////////
//	// 5) Write the new DICOM series
//
//	// Make the output directory and generate the file names.
//	itksys::SystemTools::MakeDirectory("D:\\testITKDICOM" );
//
//	// Generate the file names
//	OutputNamesGeneratorType::Pointer outputNames = OutputNamesGeneratorType::New();
//	std::string seriesFormat("test");
//	seriesFormat = seriesFormat + "/" + "IM%d.dcm";
//	outputNames->SetSeriesFormat (seriesFormat.c_str());
//	outputNames->SetStartIndex (1);
//	outputNames->SetEndIndex (outputSize[2]);
//
//	SeriesWriterType::Pointer seriesWriter = SeriesWriterType::New();
//	seriesWriter->SetInput( shiftScale->GetOutput() );
//	seriesWriter->SetImageIO( gdcmIO );
//	seriesWriter->SetFileNames( outputNames->GetFileNames() );
//	seriesWriter->SetMetaDataDictionaryArray( &outputArray );
//	try
//	{
//		seriesWriter->Update();
//	}
//	catch( itk::ExceptionObject & excp )
//	{
//		std::cerr << "Exception thrown while writing the series " << std::endl;
//		std::cerr << excp << std::endl;
//		return;
//	}
//	/*std::cout << "The output series in directory " << argv[2]
//	<< " has " << outputSize[2] << " files with spacing "
//		<< outputSpacing
//		<< std::endl;*/
//
//}



void CbctRecon::CopyDictionary (itk::MetaDataDictionary &fromDict, itk::MetaDataDictionary &toDict)
{
	typedef itk::MetaDataDictionary DictionaryType;

	DictionaryType::ConstIterator itr = fromDict.Begin();
	DictionaryType::ConstIterator end = fromDict.End();
	typedef itk::MetaDataObject< std::string > MetaDataStringType;

	while( itr != end )
	{
		itk::MetaDataObjectBase::Pointer  entry = itr->second;

		MetaDataStringType::Pointer entryvalue =
			dynamic_cast<MetaDataStringType *>( entry.GetPointer() ) ;
		if( entryvalue )
		{
			std::string tagkey   = itr->first;
			std::string tagvalue = entryvalue->GetMetaDataObjectValue();
			itk::EncapsulateMetaData<std::string>(toDict, tagkey, tagvalue);
		}
		++itr;
	}
}



void CbctRecon::DoBeamHardeningCorrection()
{
	if (!m_spProjImg3DFloat)
		return;

	//OutputImageType m_spProjImg3D: float image

	typedef itk::ImageRegionIteratorWithIndex<OutputImageType> iteratorType;
	iteratorType it(m_spProjImg3DFloat, m_spProjImg3DFloat->GetRequestedRegion());

	double crntVal = 0.0;
	double corrF = 0.0;
	/* Below were used for Elekta
	double poly3_a = 9.321e-05;
	double poly3_b = -2.609e-03;
	double poly3_c = 3.374e-02;
	double poly3_d = 9.691e-01;
	double poly3_e = 0.0; */

	double poly3_a = 6.0e-08; // Acc 5 :: Catphan calibration (after ugly full-fan bowtie corr.)
	double poly3_b = -1.0e-08;
	double poly3_c = -5.0e-07;
	double poly3_d = 8.0e-01;
	double poly3_e = 1.47;

	//double corrVal = 0.0;
	if (ximIsUsed){
	  poly3_a = 6.0e-8;
	  poly3_b = 9.0e-5;
	  poly3_c = 1.0e-2;
	  poly3_d = 0.8;
	  poly3_e = -1.47;
	}

	//Shortening factor 0.9 is applied
	/*double poly3_a = 11.24e-05;
	double poly3_b = -29.28e-04;
	double poly3_c = 35.48e-03;
	double poly3_d = 9.701e-01;*/

	//Shortening factor 0.7 is applied
	/*double poly3_a = 1.660e-04;
	double poly3_b = -3.699e-03;
	double poly3_c = 3.923e-02;
	double poly3_d = 9.727e-01;*/

	cout << "Beam hardening corrF poly curve: " << poly3_a <<", " << poly3_b << ", " <<poly3_c << ", "<<poly3_d << endl;

	it.GoToBegin();

	while (!it.IsAtEnd())
	{
		crntVal = (double)(it.Get()); //raw mu_t -> tau in Varian Confidential

		if (crntVal < 1.189) //corresponding to 5 cm of water depth
		{
			//corrF = 1+(1-crntVal);
			corrF = 1.0;
		}
		else
		{
			corrF = poly3_a*pow(crntVal,3.0) +
				poly3_b*pow(crntVal,2.0) +
				poly3_c*pow(crntVal,1.0) +
				poly3_d;
		}
		it.Set((float)(crntVal*corrF + poly3_e));
		++it;
	}
}

void CbctRecon::SLT_DoBHC()
{
  if (!m_spProjImg3DFloat)
	return;

  DoBeamHardeningCorrection();//only for m_spProjImg3D
  SetMaxAndMinValueOfProjectionImage();
  SLT_DrawProjImages();
}
template <typename T> int sgn(T val) {
	return (T(0) < val) - (val < T(0));
}
double heaviside(double x) {
	return .5 * sgn(x) + 0.5;
}

double fullFan_subFunction(double a, double b, double c, double d, double x){
	return c - sqrt(abs(pow(a, 2) - pow(x * d - b, 2))) *
		heaviside(x * d - b + a) * heaviside(-( x * d - b - a));
}

double fullFan_Function(double a, double b, double c, double d, double e, double x){
	return (
		fullFan_subFunction(a, b, c, d, x - 3. * e) +
		fullFan_subFunction(a, b, c, d, x - 2. * e) +
		fullFan_subFunction(a, b, c, d, x - e) +
		fullFan_subFunction(a, b, c, d, x) +
		fullFan_subFunction(a, b, c, d, x + e) +
		fullFan_subFunction(a, b, c, d, x + 2. * e) +
		fullFan_subFunction(a, b, c, d, x + 3. * e)
		) * .142857;
}

void CbctRecon::SLT_DoBTC()
{
	if (!m_spProjImg3DFloat)
	  return;

	if (ximIsUsed){
	  cout << "Bow tie filtering should not be used for Xim data (from Scripps) !!" << endl;
	  return;
	}
 	//OutputImageType m_spProjImg3D: float image

	typedef itk::ImageRegionIteratorWithIndex<OutputImageType> iteratorType;
	iteratorType it(m_spProjImg3DFloat, m_spProjImg3DFloat->GetRequestedRegion());

	double crntVal = 0.0;
	double corrF = 0.0;
	//double corrVal = 0.0;
	/*curve(x) = d. / (1 + exp(-b.*(x - a))) + c From MATLAB:
		Coefficients(with 95 % confidence bounds) :
		a = 264.6  (264.3, 264.8)
		b = 0.06258  (0.06177, 0.0634)
		c = 2.502  (2.5, 2.505)
		d = 1.455  (1.452, 1.458)
		HEAD DATA:
		a =       255.2  (254.8, 255.6)
		b =     0.04582  (0.0451, 0.04655)
		c =       2.131  (2.128, 2.135)
		d =       1.721  (1.715, 1.726)
	*/
	/*FULLFAN:
	model = @(a, b, c, d, e, x) ...
	c-sqrt(abs(a.^2-power(x.*d-b,2))).*...%heaviside(x.*d-b+a).*heaviside(-(x.*d-b-a));
	1 ./ (1 + exp(- e .* (x.*d-b+a))).*...
	1 ./ (1 + exp(- e .* (-(x.*d-b-a))));
	FULLFAN HEAD:
	a =       2.022  (2.021, 2.024)
	b =       3.243  (3.241, 3.245)
	c =       4.187  (4.186, 4.188)
	d =     0.01284  (0.01283, 0.01285)
	e =       10.55  (10.46, 10.64)
	2.0225;3.2433;4.1869;0.0128;10.5462
	1.4419;2.3382;2.6395;0.0092;18.1504 Acc 5 Catphan
	*/
	
	QStringList strList = ui.lineEdit_fBTcor->text().split(';');
	//third one is the organ name
	if (strList.length() != 4 && !ui.checkBox_Fullfan->isChecked())
	{
		cout << "Wrong number of arguments!" << endl << "Must be a;b;c;d -> d. / (1 + exp(-b.*(x - a))) + c" << endl;
		return;
	}
	else if (strList.length() != 5 && ui.checkBox_Fullfan->isChecked())
	{
		cout << "Wrong number of arguments!" << endl
			<< "Must be a;b;c;d;e -> c - sqrt(abs(a^2-((x+/-e)*d-b)^2)) * heaviside((x+/-e)*d-b+a) * heaviside(-((x+/-e)*d-b-a))" << endl;
			// << "Must be a;b;c;d;e -> d. / (1 + exp(-b.*(x - a))) + c + d. / (1 + exp(b.*(x - e)))" << endl;
		return;
	}
	double poly3_a = strList.at(0).toDouble();  //264.6; //lineEdit_fBTcor
	double poly3_b = strList.at(1).toDouble();  //0.06258;
	double poly3_c = strList.at(2).toDouble();  //2.502;
	double poly3_d = strList.at(3).toDouble();  //1.455;
	double poly3_e;
	if (ui.checkBox_Fullfan->isChecked())
	{
		poly3_e = strList.at(4).toDouble();
	}

	FloatImageType::SizeType imgSize = m_spProjImg3DFloat->GetLargestPossibleRegion().GetSize();
	if (ui.checkBox_Fullfan->isChecked())
	{
		cout << "Bow-tie correction curve:" 
			 << poly3_d << " / ( 1 + exp(-" << poly3_b << " * (x - " << poly3_a << "))) + " << poly3_c 
			 << " + " << poly3_d << " / ( 1 + exp(" << poly3_b << " * (x - " << poly3_e << ")))" << endl;
	}
	else {
		cout << "Bow-tie correction curve:" << poly3_d << " / ( 1 + exp(-" << poly3_b << " * (x - " << poly3_a << "))) + " << poly3_c << endl;
	}
	
	
	double x_idx;
	it.GoToBegin();
	while (!it.IsAtEnd())
	{
		crntVal = (double)(it.Get()); // (65535 / exp(it.Get())); //raw mu_t = ln(65535/I) <-> I = 65535 / exp(mu_t)
		x_idx = (double)(it.GetIndex()[0]) * (512.0 / imgSize[0]); // 512 from current fit -> conversion to be consistent with downResFactor
		if (ui.checkBox_Fullfan->isChecked())
		{
			if (crntVal > (poly3_c - poly3_a))
			{
				corrF = fullFan_Function(poly3_a, poly3_b, poly3_c, poly3_d, poly3_e, x_idx);
				/* corrF = poly3_c -
					sqrt(abs(pow(poly3_a, 2) - pow(x_idx * poly3_d - poly3_b, 2))) /
					(1 + exp(-poly3_e * (x_idx * poly3_d - poly3_b + poly3_a))) /
					(1 + exp(poly3_e * (x_idx * poly3_d - poly3_b - poly3_a))); */
			}
			else
			{
				corrF = crntVal - 1e-2; //approximate precision of fit 
			}
			//poly3_d / (1.0 + exp(-poly3_b * (x_idx - poly3_a))) + poly3_c + poly3_d / (1.0 + exp(poly3_b * (x_idx - poly3_e)));
		}
		else{
			if (crntVal > poly3_c)
			{
				corrF = poly3_d / (1.0 + exp(-poly3_b * (x_idx - poly3_a))) + poly3_c;
			}
			else
			{
				corrF = crntVal - 1e-2; //approximate precition of fit 
			}
		}
		it.Set((float)(crntVal - corrF)); // (log(65535 / (crntVal - corrF))));
		++it;
	}
	SetMaxAndMinValueOfProjectionImage();
	SLT_DrawProjImages();
	cout << "Bow-tie correction done." << endl;
}

void CbctRecon::SLT_ViewRegistration() //default showing function
{
  //if (!m_spReconImg)
	/*{
	  cout << "No fixed image is ready" << endl;
	  return;
	}*/
	m_pDlgRegistration->UpdateListOfComboBox(0);//combo selection signalis called
	m_pDlgRegistration->UpdateListOfComboBox(1);

	//if not found, just skip
	//m_pDlgRegistration->SelectComboExternal(0, REGISTER_RAW_CBCT); // will call fixedImageSelected
	//m_pDlgRegistration->SelectComboExternal(1, REGISTER_MANUAL_RIGID );
	m_pDlgRegistration->show();

}


void CbctRecon::Draw2DFrom3DDouble(USHORT_ImageType::Pointer& spFixedImg, USHORT_ImageType::Pointer& spMovingImg, enPLANE enPlane, double pos, YK16GrayImage& YKFixed, YK16GrayImage& YKMoving)
{
    if (!spFixedImg || !spMovingImg)
        return;

    itk::ImageSliceConstIteratorWithIndex<USHORT_ImageType> it (spFixedImg, spFixedImg->GetRequestedRegion());

    USHORT_ImageType::SizeType imgSize = spFixedImg->GetRequestedRegion().GetSize(); //1016x1016 x z
    USHORT_ImageType::SizeType imgSizeBuf = spFixedImg->GetBufferedRegion().GetSize(); //1016x1016 x z
    USHORT_ImageType::SizeType imgSizeLargest = spFixedImg->GetLargestPossibleRegion().GetSize(); //1016x1016 x z

    USHORT_ImageType::PointType imgOrigin = spFixedImg->GetOrigin();
    USHORT_ImageType::SpacingType imgSpacing = spFixedImg->GetSpacing();

    int width = 0;
    int height = 0;
    int iReqSlice = 0;
    int iCntSlice = 0;

    //For moving image
    typedef itk::ResampleImageFilter<USHORT_ImageType,USHORT_ImageType> ResampleFilterType;
    ResampleFilterType::Pointer filter = ResampleFilterType::New();

	filter->SetInput(spMovingImg);

    typedef itk::AffineTransform< double, 3 > TransformType;
    TransformType::Pointer transform = TransformType::New();
    filter->SetTransform( transform );

    typedef itk::NearestNeighborInterpolateImageFunction<USHORT_ImageType, double > InterpolatorType;
    InterpolatorType::Pointer interpolator = InterpolatorType::New();
    filter->SetInterpolator( interpolator );
    filter->SetDefaultPixelValue( 0 );

    //const double outputSpacing[2] = { 1.0, 1.0 };
    //const double outputOrigin[2] = { 0.0, 0.0 };

    USHORT_ImageType::DirectionType direction;
    direction.SetIdentity();
    filter->SetOutputDirection( direction );

    //ResampledImgType2D::SizeType outSize;

    USHORT_ImageType::SpacingType movingSpacing = imgSpacing;
    USHORT_ImageType::PointType movingOrigin = imgOrigin;
    USHORT_ImageType::SizeType movingSize = imgSize;

    switch (enPlane)
    {
    case PLANE_AXIAL:
        width = imgSize[0];
        height = imgSize[1];
        iCntSlice = imgSize[2];
        iReqSlice = qRound((pos - imgOrigin[2])/imgSpacing[2]);
        it.SetFirstDirection(0); //x?
        it.SetSecondDirection(1); //y?

        movingSpacing[2] = 1.0;
        movingOrigin[2] = pos;
        movingSize[2] = 1;
        //Resample Here! make corresponding 2D image for Moving image
        YKFixed.SetSpacing(imgSpacing[0], imgSpacing[1]);

        break;
    case PLANE_FRONTAL:
        width = imgSize[0];
        height = imgSize[2];
        iCntSlice = imgSize[1];
        iReqSlice = qRound((pos - imgOrigin[1])/imgSpacing[1]);
        it.SetFirstDirection(0); //x?
        it.SetSecondDirection(2); //y?

		movingSpacing[1] = 1.0;
		movingOrigin[1] = pos;
		movingSize[1] = 1;

        YKFixed.SetSpacing(imgSpacing[0], imgSpacing[2]);
        break;
    case PLANE_SAGITTAL:
        width = imgSize[1];
        height = imgSize[2];
        iCntSlice = imgSize[0];
        iReqSlice = qRound((pos - imgOrigin[0])/imgSpacing[0]);
        it.SetFirstDirection(1); //x?
        it.SetSecondDirection(2); //y?


		movingSpacing[0] = 1.0;
		movingOrigin[0] = pos;
		movingSize[0] = 1;

        YKFixed.SetSpacing(imgSpacing[1], imgSpacing[2]);
        break;

    default:
        cout << "default should not passed by" << endl;
        width = imgSize[0];
        height = imgSize[1];
        iCntSlice = imgSize[2];
        iReqSlice = qRound((pos - imgOrigin[2])/imgSpacing[2]);
        it.SetFirstDirection(0); //x?
        it.SetSecondDirection(1); //y?
        YKFixed.SetSpacing(imgSpacing[0], imgSpacing[1]);
        break;
    }

    filter->SetOutputSpacing( movingSpacing );
    filter->SetOutputOrigin(movingOrigin);
    filter->SetSize(movingSize);
    filter->Update();


	//typedef itk::ResampleImageFilter<OutputImageType, OutputImageType> ResampleFilterType;
	//ResampleFilterType::Pointer resampler = ResampleFilterType::New();
	////OutputImageType::RegionType fixedImg_Region = fixedImg->GetLargestPossibleRegion().GetSize();

	//resampler->SetInput(targetImg);
	//resampler->SetSize(size_trans);
	//resampler->SetOutputOrigin( Origin_trans); //Lt Top Inf of Large Canvas
	//resampler->SetOutputSpacing( spacing_trans ); // 1 1 1
	//resampler->SetOutputDirection( targetImg->GetDirection() ); //image normal?
	//resampler->SetTransform(transform);



    YKFixed.CreateImage(width, height, 0);
    //cout << "Before MovingImg Creation " << endl;

    YKMoving.CreateImage(width, height, 0);//exactly same dimension

    itk::ImageRegionConstIterator<USHORT_ImageType> itMoving (filter->GetOutput(), filter->GetOutput()->GetBufferedRegion());
    int cnt = 0;

	//this simple code will cause flip of the image in frontal and sagittal image
    for (itMoving.GoToBegin() ; !itMoving.IsAtEnd() ; ++itMoving)
    {
        YKMoving.m_pData[cnt] = itMoving.Get();
        cnt++;
    }
	if (enPlane != PLANE_AXIAL)
	{
	  YKMoving.EditImage_Flip();
	}


	//cout << "tot pixel no: " << cnt << endl;

    //YK16GrayImage::CopyItkImage2YKImage(filter->GetOutput(), &YKMoving);

    //cout << "After MovingImg Creation " << endl;

    it.GoToBegin();

    int iNumSlice = 0;
    int iNumWidth = 0;
    int iNumHeight = 0;

    if (iReqSlice < 0 || iReqSlice >=iCntSlice)
        return;

    while( !it.IsAtEnd() )
    {
        if (iNumSlice == iReqSlice)
        {
            iNumHeight = 0;

            while( !it.IsAtEndOfSlice())
            {
                iNumWidth = 0;
                while( !it.IsAtEndOfLine())
                {
                    //double tmpVal = it.Get()*multiplyFactor;
                    SHORT_ImageType::PixelType fixedImgVal = it.Get();
                    USHORT_ImageType::IndexType pixelIdxFixed;
                    USHORT_ImageType::PointType pixelPhysPt;
                    pixelIdxFixed = it.GetIndex();

                   // spFixedImg->TransformIndexToPhysicalPoint (pixelIdxFixed, pixelPhysPt);
                    //spMovingImg->TransformPhysicalPointToIndex(pixelPhysPt,pixelIdxMoving);

                    //Fill YKMoving image
                    //calculate the position of this iterator

                    //double movingImgVal = spMovingImg->GetPixel()

                    //double movingImgVal = (double)spMovingImg->GetPixel( pixelIdxMoving );
                    //unsigned short movingImgVal = 0;

                    if (enPlane == PLANE_AXIAL)
                    {
                        YKFixed.m_pData[iNumWidth + width*iNumHeight] = fixedImgVal;
                       // YKMoving.m_pData[iNumWidth + width*iNumHeight] = movingImgVal;
                    }
                    else
                    {
                        YKFixed.m_pData[iNumWidth + width*(height - iNumHeight-1)] = fixedImgVal;
                        //YKMoving.m_pData[iNumWidth + width*(height - iNumHeight-1)] = movingImgVal;
                    }

                    ++it;
                    iNumWidth++;
                }
                it.NextLine();
                iNumHeight++;
            }
            break;
        }
        it.NextSlice();
        iNumSlice++;
    }

//    cout << "YK Images were filled" << endl;
}

void CbctRecon::Draw2DFrom3D(USHORT_ImageType::Pointer& pImg, enPLANE direction, double pos, YK16GrayImage& Output2D)
{
    if (!pImg)
        return;

    itk::ImageSliceConstIteratorWithIndex<USHORT_ImageType> it (pImg, pImg->GetRequestedRegion());

    USHORT_ImageType::SizeType imgSize = pImg->GetRequestedRegion().GetSize(); //1016x1016 x z
    USHORT_ImageType::SizeType imgSizeBuf = pImg->GetBufferedRegion().GetSize(); //1016x1016 x z
    USHORT_ImageType::SizeType imgSizeLargest = pImg->GetLargestPossibleRegion().GetSize(); //1016x1016 x z

    USHORT_ImageType::PointType imgOrigin = pImg->GetOrigin();
    USHORT_ImageType::SpacingType imgSpacing = pImg->GetSpacing();

    int width = 0;
    int height = 0;
    int iReqSlice = 0;
    int iCntSlice = 0;


    switch (direction)
    {
    case PLANE_AXIAL:
        width = imgSize[0];
        height = imgSize[1];
        iCntSlice = imgSize[2];
        iReqSlice = qRound((pos - imgOrigin[2])/imgSpacing[2]);
        it.SetFirstDirection(0); //x?
        it.SetSecondDirection(1); //y?
        Output2D.SetSpacing(imgSpacing[0], imgSpacing[1]);
        break;
    case PLANE_FRONTAL:
        width = imgSize[0];
        height = imgSize[2];
        iCntSlice = imgSize[1];
        iReqSlice = qRound((pos - imgOrigin[1])/imgSpacing[1]);
        it.SetFirstDirection(0); //x?
        it.SetSecondDirection(2); //y?
        Output2D.SetSpacing(imgSpacing[0], imgSpacing[2]);
        break;
    case PLANE_SAGITTAL:
        width = imgSize[1];
        height = imgSize[2];
        iCntSlice = imgSize[0];
        iReqSlice = qRound((pos - imgOrigin[0])/imgSpacing[0]);
        it.SetFirstDirection(1); //x?
        it.SetSecondDirection(2); //y?
        Output2D.SetSpacing(imgSpacing[1], imgSpacing[2]);
        break;
    default:
        cout << "default should not be passed by" << endl;
        width = imgSize[0];
        height = imgSize[1];
        iCntSlice = imgSize[2];
        iReqSlice = qRound((pos - imgOrigin[2])/imgSpacing[2]);
        it.SetFirstDirection(0); //x?
        it.SetSecondDirection(1); //y?
        Output2D.SetSpacing(imgSpacing[0], imgSpacing[1]);
        break;
    }

    Output2D.CreateImage(width, height, 0);
    it.GoToBegin();

    int iNumSlice = 0;
    int iNumWidth = 0;
    int iNumHeight = 0;

    if (iReqSlice < 0 || iReqSlice >=iCntSlice)
        return;

    while( !it.IsAtEnd() )
    {
        if (iNumSlice == iReqSlice)
        {
            iNumHeight = 0;
            while( !it.IsAtEndOfSlice() )
            {
                iNumWidth = 0;
                while( !it.IsAtEndOfLine() )
                {
                    //double tmpVal = it.Get()*multiplyFactor;
                    double tmpVal = it.Get();

                    if (direction == PLANE_AXIAL)
                        Output2D.m_pData[iNumWidth + width*iNumHeight] = tmpVal;
                    else
                        Output2D.m_pData[iNumWidth + width*(height - iNumHeight-1)] = tmpVal;

                    ++it;
                    iNumWidth++;
                }
                it.NextLine();
                iNumHeight++;
            }
            break;
        }
        it.NextSlice();
        iNumSlice++;
    }
}



void CbctRecon::RegisterImgDuplication( enREGI_IMAGES src, enREGI_IMAGES target )
{

  USHORT_ImageType::Pointer tmpSrc;


  switch (src)
  {
  case REGISTER_REF_CT:
	tmpSrc = m_spRefCTImg;
	break;
  }

  if (!tmpSrc)
  {
	cout <<"src image is empty" << endl;
	return;
  }

  //Duplication for registration. Starting point is manual Rigid CT image
  typedef itk::ImageDuplicator<USHORT_ImageType> DuplicatorType;
  DuplicatorType::Pointer duplicator = DuplicatorType::New();
  duplicator->SetInputImage(tmpSrc);
  duplicator->Update();

  switch (target)
  {
  case REGISTER_MANUAL_RIGID:
	 m_spManualRigidCT = duplicator->GetOutput();
  break;
  }
  //Duplication for : End
}
void CbctRecon::FindAllRelevantPaths(QString pathProjHisDir)//called following SLT_SetHisDir
{
  //in case of eletka, img_UID
  //QString aa;
  //cout<< "ddd " << aa.toLocal8Bit().constData() << endl;

  m_strDCMUID = "";
  m_strPathPatientDir = "";
  m_strPatientDirName = "";
  m_strPathFRAME_DBF = "";
  m_strPathIMAGE_XML = "";
  m_strPathGeomXML = "";
  m_strPathPlanCTDir = "";
  m_strPathRS = "";
  m_strPathRS_CBCT = "";
  m_strPathVarianINI = "";

  m_strPathPlan = "";
  m_strPathIMAGES = "";

  QDir curHisDir(pathProjHisDir);
  QDir movingDir(pathProjHisDir);


  if (!curHisDir.dirName().contains("img_", Qt::CaseSensitive) &&
	!curHisDir.dirName().contains("fwd_", Qt::CaseSensitive) &&
	 !curHisDir.dirName().contains("sca_", Qt::CaseSensitive) &&
	 !curHisDir.dirName().contains("cor_", Qt::CaseSensitive))
  {
	if (curHisDir.dirName().contains("Scan0", Qt::CaseSensitive))
	{
		cout << "XML set by guessing: Scan0/../ProjectionInfo.xml" << endl;
		m_strPathGeomXML = curHisDir.absolutePath() + "/../" + "ProjectionInfo.xml";
		ui.lineEdit_VarianGeomPath->setText(m_strPathGeomXML);
		cout << "Patient DIR set to: Scan0/../../" << endl;
		m_strPathGeomXML = curHisDir.absolutePath() + "/../"; // <- Might break shit, dunno..
		ximIsUsed = false;
		return;
	}
	else if (curHisDir.dirName().contains("Acquisitions", Qt::CaseSensitive))
	{
		cout << "XML set by guessing: Acquisitions/../Scan.xml" << endl;
		m_strPathGeomXML = curHisDir.absolutePath() + "/../../" + "Scan.xml";
		ui.lineEdit_VarianGeomPath->setText(m_strPathGeomXML);
		cout << "Patient DIR set to: Acquisitions/../../../" << endl;
		m_strPathGeomXML = curHisDir.absolutePath() + "/../../"; // <- Might break shit, dunno..
		ximIsUsed = true;
		return;
	}
	else
	{
		cout << "Projection folder should have format [img_UID]" << endl;
		cout << "XML file cannot be made" << endl;
		return;
	}
  }

  QString tmpStr = curHisDir.dirName();
  QStringList strListDir = tmpStr.split("_");
  m_strDCMUID = strListDir.at(1);




  //m_strDCMUID = curHisDir.dirName().right(curHisDir.dirName().length() - 4);

  if (!movingDir.cdUp()) //projDir ==> IMAGES
  {
	cout << "no upper dir" <<endl;
	return;
  }
  QDir tmpDir_IMAGES(movingDir.absolutePath());
  m_strPathIMAGES = tmpDir_IMAGES.absolutePath();



  if (!movingDir.cdUp()) //IMAGES ==> patient_402-02-78
  {
	cout << "no upper dir" <<endl;
	return;
  }
  QDir tmpDir_PatientFolder(movingDir.absolutePath());

  if (!movingDir.cdUp()) //patient_402-02-78 ==> Data folder where DBF files are.
  {
	cout << "no upper dir" <<endl;
	return;
  }

  m_strPatientDirName = tmpDir_PatientFolder.dirName();
  m_strPathPatientDir = tmpDir_PatientFolder.absolutePath();

  QDir tmpDir_RootFolder(movingDir.absolutePath()); //root folder

  if (tmpDir_RootFolder.absolutePath().length() > 1)
	m_strPathDirDefault = tmpDir_RootFolder.absolutePath();

  //option 1: already made rtk xml file
  QString tmpPathRTKGeometry = tmpDir_RootFolder.absolutePath() +"/" + "VarianGeom_" + m_strDCMUID + ".xml";
  QFileInfo rtkGeomInfo(tmpPathRTKGeometry);

  //option 2
  QString pathXVIGeometryXML = curHisDir.absolutePath() + "/../" + "ProjectionInfo.xml";
  QFileInfo xviGeomInfo(pathXVIGeometryXML);

  if (rtkGeomInfo.exists()) //The best option: rtk geometry file is already existing
  {
	cout << "RTK XLM file is found" << endl;
	m_strPathGeomXML = tmpPathRTKGeometry;
  }
  else if (xviGeomInfo.exists()) //2nd option:_Frames.xml already exists in each projection folder for > XVI5.0.2
  {
	  cout << "XVI XLM file is found" << endl;
	  //YKdebug: Later, it
	  m_strPathGeomXML = pathXVIGeometryXML;

	  //Later, it should be

	  // XVIXMLReader.SetFile(pathXVIGeometryXML)
	  //XVIXMLReader.GenerateData()

	  //rtk::ThreeDCircularProjectionGeometryXMLFileWriter::Pointer xmlWriter
	  //xmlWriter->SetFilename( tmpPathRTKGeometry ) //as option 1
	  //xmlWriter->SetObject(XVIXMLReader->GetGeometry());
	  //TRY_AND_EXIT_ON_ITK_EXCEPTION(xmlWriter->WriteFile());
	  //m_strPathGeomXML = tmpPathRTKGeometry;
  }
  else
  {
	  /*I have altered the code below, pray I do not alter it further*/
//	QString tmpStrPath1 = m_strPathPatientDir;
	QString tmpStrPath2 = m_strPathPatientDir;

	//1st priority: DBF files saved in each "patient" folder --> just in case data are collected separately
//	QFileInfo fInfo_FrameDBF = QFileInfo(tmpStrPath1.append("/FRAME.DBF"));
	QFileInfo fInfo_ImageXML = QFileInfo(tmpStrPath2.append("/IMAGE.XML"));

	if (!fInfo_ImageXML.exists())
	{
	  cout << "No found in the patient folder. DBF files can be saved in each individual patient as well. Continues to search them again in root folder(standard)" << endl;

//	  fInfo_FrameDBF = QFileInfo(tmpDir_RootFolder.absolutePath().append("/FRAME.DBF"));
	  fInfo_ImageXML = QFileInfo(tmpDir_RootFolder.absolutePath().append("/IMAGE.XML"));

	  if (!fInfo_ImageXML.exists())
	  {
		cout << "XML files were not found" << endl;
		cout << "XML file cannot be made" << endl;
		return;
	  }
	}
	else
	{
	  cout << "DBF files are found in the individual patient directory." << endl;
	}
//	m_strPathFRAME_DBF = fInfo_FrameDBF.absoluteFilePath();
	m_strPathIMAGE_XML = fInfo_ImageXML.absoluteFilePath();

	m_strPathGeomXML = "";
	m_strPathGeomXML = MakeVarianXML(m_strPathIMAGE_XML, m_strDCMUID);//if DBF files exist but UID is not found, it will crash

	/*I have altered the code above, pray I do not alter it further*/
	if (m_strPathGeomXML.length() < 1)
	{
	  cout << "No releated data in DBF file" << endl;
	  return;
	}
  }

  //cout << "Root folder: " << tmpDir_RootFolder.absolutePath().toLocal8Bit().constData() << endl;
  //cout << "Root folder2: " << tmpDir_RootFolder.absolutePath().append("\\FRAME.DBF").toLocal8Bit().constData() << endl;
  //cout << fInfo_FrameDBF.absoluteFilePath().toLocal8Bit().constData() << endl;

  //GenerateXMLFunc.

  //Search for the geometry XML file: naming convention: ElektaGeom_DICOMUID.xml
  //after Generation of the XML from DBF files
//  movingDir = tmpDir_IMAGES;
  //tmpDir_PatientFolder;

  int enDirStructure_Type = 0;
  //0: in patient DIR --> 3 folders(CT_SET, DICOM_PLAN, IMAGES)
  //1: // Patient DIR ==> IMAGES --> CT_SET / DICOM_PLAN
  //2: NO CT image

  QDir tmpDIR_CTSET = QDir(tmpDir_PatientFolder.absolutePath().append("/CT_SET"));

  if (tmpDIR_CTSET.exists())
	enDirStructure_Type = 0;
  else
  {
	QString tmpStrPathCTSET = m_strPathIMAGES;
	tmpDIR_CTSET = QDir(tmpStrPathCTSET.append("/CT_SET"));

	if (tmpDIR_CTSET.exists())
	  enDirStructure_Type = 1;
	else
	  enDirStructure_Type = 2;
  }

  //QString strPathCTSet = m_strPathIMAGES.append("/CT_SET");

 // switch (enDirStructure_Type)
 // {
 // case 0:
	//  movingDir = tmpDir_IMAGES;
	//break;
 // case 1:
	//movingDir = tmpDir_IMAGES;
	//break;
 // case 2:
	//cout << "No CT DICOM folder exist. Proceeding w/o CT" << endl;
	//break;
 // }
 //

  QFileInfoList listDir;
  if (enDirStructure_Type != 2)
  {
	listDir = tmpDIR_CTSET.entryInfoList(QDir::Dirs, QDir::Name);
	if (listDir.size() <= 2) //only /. and /.. exist
	{
	  cout << "No CT DICOM folder exist. Proceeding w/o CT" << endl;
	}
	else
	   m_strPathPlanCTDir = listDir.at(2).absoluteFilePath(); // . , .. , real DICOM folder
  }

 // for (int i = 0 ; i < listDir.size() ; i++)
 // {
	////cout << listDir.at(i).absolutePath().toLocal8Bit().constData() << endl; //this returns Dir, not itself
	//QString tmpPath = listDir.at(i).absoluteFilePath();
 // }

  QFileInfoList listFile = tmpDIR_CTSET.entryInfoList(QDir::Files, QDir::Name); //search for DICOM RS file

  if (listFile.size() <= 0)
  {
	cout << "No CT DICOM RS file exist. proceeding w/o RS" << endl;
	//return;
  }
  else
  {
	for (int i = 0 ; i < listFile.size() ; i++)
	{
	  if (listFile.at(i).suffix().contains("DCM",Qt::CaseInsensitive))
	  {
		m_strPathRS = listFile.at(i).absoluteFilePath();
		break;
	  }
	}
  }

  QDir tmpDIR_DCM_Plan = QDir(tmpDir_PatientFolder.absolutePath().append("/DICOM_PLAN"));

  if (tmpDIR_DCM_Plan.exists())
      enDirStructure_Type = 0;
  else
  {
      QString tmpStrPathCTSET = m_strPathIMAGES;
      tmpDIR_DCM_Plan = QDir(tmpStrPathCTSET.append("/DICOM_PLAN"));

      if (tmpDIR_DCM_Plan.exists())
          enDirStructure_Type = 1;
      else
          enDirStructure_Type = 2;
  }

  QFileInfoList listFileDCMPlan;
  if (enDirStructure_Type != 2)
  {
      listFileDCMPlan = tmpDIR_DCM_Plan.entryInfoList(QDir::Files, QDir::Name);
      if (listFileDCMPlan.size() < 1) //should be /. and /.. and one dcm file
      {
          cout << "No DCM plan file exists. Proceeding w/o dicom plan" << endl;
      }
      else if (listFileDCMPlan.size() > 1)
      {
          cout << "Warning! More than one DCM plan file exists. First DCM plan file will be used" << endl;
      }
      else{

      }

      for (int i = 0; i < listFileDCMPlan.size(); i++)
      {
          if (listFileDCMPlan.at(i).suffix().contains("DCM", Qt::CaseInsensitive))
          {
              m_strPathPlan = listFileDCMPlan.at(i).absoluteFilePath();
              break; //get fisrt one only
          }
      }
  }

  QDir movingDirCBCTRS;

  if (enDirStructure_Type == 0)
	movingDirCBCTRS = tmpDir_PatientFolder;
  else if (enDirStructure_Type == 1)
	movingDirCBCTRS = tmpDir_IMAGES;


  if (!movingDirCBCTRS.cd("CBCT_RS"))
  {
	cout << "no CBCT_RS dir exists. Proceed with out CBCT RS image" << endl;
  }
  else
  {
	QFileInfoList listFile2 = movingDirCBCTRS.entryInfoList(QDir::Files, QDir::Name);

	if (listFile2.size() <= 0)
	{
	  cout << "No CBCT DICOM RS file exist. proceeding w/o RS" << endl;
	  //return;
	  m_strPathRS_CBCT = "";
	}
	else
	{
	  for (int i = 0 ; i < listFile2.size() ; i++)
	  {
		if (listFile2.at(i).suffix().contains("DCM",Qt::CaseInsensitive))
		{
		  m_strPathRS_CBCT = listFile2.at(i).absoluteFilePath();
		  break;
		}
	  }
	}
  }
  ui.lineEdit_PathCBCTSkinPath->setText(m_strPathRS_CBCT);

  QString strPathAcqParamDir = pathProjHisDir + "/Reconstruction";
  QDir tmpAcqParamDir = QDir(strPathAcqParamDir);

  if (tmpAcqParamDir.exists())
  {
      QFileInfoList listFileAcqParam = tmpAcqParamDir.entryInfoList(QDir::Files, QDir::Name); //search for DICOM RS file

      int iMinNameLength = 9999;
      for (int i = 0; i < listFileAcqParam.size(); i++)
      {
          //suffix:*.tar.gz ==> gz only
          if (listFileAcqParam.at(i).suffix().contains("INI", Qt::CaseInsensitive))
          {
              QString tmpPath = listFileAcqParam.at(i).absoluteFilePath();

              if (tmpPath.length() < iMinNameLength)
              {
                  iMinNameLength = tmpPath.length();
                  m_strPathVarianINI = tmpPath;
              }
          }
      }

  }

  cout << "m_strDCMUID: " << m_strDCMUID.toLocal8Bit().constData() << endl;
  cout << "m_strPathPatientDir: " << m_strPathPatientDir.toLocal8Bit().constData() << endl;
  cout << "m_strPatientDirName: " << m_strPatientDirName.toLocal8Bit().constData() << endl;
  cout << "m_strPathFRAME_DBF: " << m_strPathFRAME_DBF.toLocal8Bit().constData() << endl;
  cout << "m_strPathIMAGE_XML: " << m_strPathIMAGE_XML.toLocal8Bit().constData() << endl;
  cout << "m_strPathGeomXML: " << m_strPathGeomXML.toLocal8Bit().constData() << endl;
  cout << "m_strPathPlanCTDir: " << m_strPathPlanCTDir.toLocal8Bit().constData() << endl;
  cout << "m_strPathRS: " << m_strPathRS.toLocal8Bit().constData() << endl;
  cout << "m_strPathRS_CBCT: " << m_strPathRS_CBCT.toLocal8Bit().constData() << endl;
  cout << "m_strPathPlan: " << m_strPathPlan.toLocal8Bit().constData() << endl;
  cout << "m_strPathVarianINI: " << m_strPathVarianINI.toLocal8Bit().constData() << endl;

  float kVp = 0.0;
  float mA = 0.0;
  float ms = 0.0;
  GetXrayParamFromINI(m_strPathVarianINI, kVp, mA, ms);

  if (kVp*mA*ms != 0)
  {
      //update GUI
      cout << "Updating current mAs setting from INI file: " << "kVp= " << kVp << ", mA= " << mA << ", ms= " << ms << endl;
      ui.lineEdit_CurmAs->setText(QString("%1, %2").arg(mA).arg(ms));
  }

  //YKTEMP: delete if the UI is changed
  ui.lineEdit_VarianGeomPath->setText(m_strPathGeomXML);
}

void CbctRecon::init_DlgRegistration(QString& strDCM_UID) //init dlgRegistrations
{
  m_pDlgRegistration->init(strDCM_UID); //NULLing all temporary spImage
}

//output spProjCT3D => intensity value, not line integral
void CbctRecon::ForwardProjection( USHORT_ImageType::Pointer& spVolImg3D, GeometryType::Pointer& spGeometry, USHORT_ImageType::Pointer& spProjCT3D, bool bSave )
{
  if (!spVolImg3D)
  {
	cout << "ERROR! No 3D-CT file. Load 3D CT file first" << endl;
	return;
  }

  if (m_iCntSelectedProj < 1)
  {
	cout << "Error! No projection image is loaded" << endl;
	return;
  }

  if (spGeometry->GetGantryAngles().size() < 1)
  {
	cout << "No geometry!" << endl;
	return;
  }

  //m_spProjCTImg --> spProjCT3D

  OutputImageType::Pointer spResultProjImageFloat;
  //Euler Transformation for RTK's weird orientation

  int iNumOfProjections = 0;

  if(true)
  {
	//0) CT image Transformation
	USHORT_ImageType::SizeType size_original = spVolImg3D->GetLargestPossibleRegion().GetSize();
	USHORT_ImageType::SpacingType spacing_original = spVolImg3D->GetSpacing();

	//Same image type from original image -3D & float
	USHORT_ImageType::IndexType start_trans;
	start_trans[0] = 0;
	start_trans[1] = 0;
	start_trans[2] = 0;

	USHORT_ImageType::SizeType size_trans;
	size_trans[0] = size_original[1]; // X //512
	size_trans[1] = size_original[2]; //Y  //512
	size_trans[2] = size_original[0]; //Z //300

	cout << " size_trans" << size_trans << endl;

	USHORT_ImageType::SpacingType spacing_trans;
	spacing_trans[0] = spacing_original[1];
	spacing_trans[1] = spacing_original[2];
	spacing_trans[2] = spacing_original[0];

	cout << " spacing_trans" << spacing_trans << endl;

	USHORT_ImageType::PointType Origin_trans;
	Origin_trans[0] = -0.5* size_trans[0]*spacing_trans[0];
	Origin_trans[1] = -0.5* size_trans[1]*spacing_trans[1];
	Origin_trans[2] = -0.5* size_trans[2]*spacing_trans[2];

	USHORT_ImageType::RegionType region_trans;
	region_trans.SetSize(size_trans);
	region_trans.SetIndex(start_trans);

	typedef itk::FlipImageFilter< USHORT_ImageType >  FilterType;
	FilterType::Pointer flipFilter = FilterType::New();
	typedef FilterType::FlipAxesArrayType FlipAxesArrayType;
    FlipAxesArrayType arrFlipAxes;
	arrFlipAxes[0] = 1;
	arrFlipAxes[1] = 0;
	arrFlipAxes[2] = 0;
	flipFilter->SetFlipAxes(arrFlipAxes);
	flipFilter->SetInput(spVolImg3D); //plan CT, USHORT image
	
	typedef itk::Euler3DTransform< double > TransformType;
	TransformType::Pointer transform = TransformType::New();

	TransformType::ParametersType param;
	param.SetSize(6);
	param.put(0, -itk::Math::pi_over_2); //rot X 
	param.put(1, 0);//rot Y
	param.put(2, itk::Math::pi_over_2);//rot Z
	param.put(3, 0.0); // Trans X mm
	param.put(4, 0.0); // Trans Y mm
	param.put(5, 0.0); // Trans Z mm

	TransformType::ParametersType fixedParam(3); //rotation center
	fixedParam.put(0,0);
	fixedParam.put(1,0);
	fixedParam.put(2,0);

	transform->SetParameters(param);
	transform->SetFixedParameters(fixedParam); //Center of the Transform

	cout << "Transform matrix:" << "	" << endl;
	cout << transform->GetMatrix() << std::endl;

	typedef itk::ResampleImageFilter<USHORT_ImageType, USHORT_ImageType> ResampleFilterType;
	ResampleFilterType::Pointer resampler = ResampleFilterType::New();

	resampler->SetInput(flipFilter->GetOutput());
	resampler->SetSize(size_trans);
	resampler->SetOutputOrigin( Origin_trans); //Lt Top Inf of Large Canvas
	resampler->SetOutputSpacing( spacing_trans ); // 1 1 1
	resampler->SetOutputDirection(flipFilter->GetOutput()->GetDirection() ); //image normal?
	resampler->SetTransform(transform);

	typedef itk::CastImageFilter< USHORT_ImageType, OutputImageType> CastFilterType; //Maybe not inplace filter
	CastFilterType::Pointer castFilter = CastFilterType::New();
	castFilter->SetInput(resampler->GetOutput());

	//double ref_mAs = 2560.0; //64 40 in Linac5
	//double crnt_mA = 40;
	//double crnt_ms = 40;

	//double calibF_A = ui.lineEdit_scaCT2CBCTCalA->text().toDouble();//Works well with Prostate patient in Linac4


	//Default value
	double calibF_A = 1.0;
	double calibF_B = 0.0;

	/*if (ui.lineEdit_CalibCoeff_a->text().length() > 0 && ui.lineEdit_CalibCoeff_b->text().length() > 0)
	{
	  calibF_A = ui.lineEdit_CalibCoeff_a->text().toDouble();
	  calibF_B = ui.lineEdit_CalibCoeff_b->text().toDouble();
	}*/

	//For CBCT autoRef:
	//SOft tissue: 0 HU--> -400 HU
	//Bone: 1126 HU --> 140 HU
	// aftter 1024 shift
	// 1024 = 624a +b
	//2150 = 1164a + b
	//Therefore: a = 2.0851, b = -277.1

//YKTEMP For CBCT calibration
	//calibF_A = 2.0851;
	//calibF_B = -277.1;

	cout << "Temporary forcing CT# applied for tissue" << endl;

	cout << "CBCT calibration Factor(Recommended: 1, 0): A = " << calibF_A << "  B= " << calibF_B << endl;
	typedef itk::MultiplyImageFilter<OutputImageType, OutputImageType, OutputImageType> MultiplyImageFilterType;
	MultiplyImageFilterType::Pointer multiplyImageFilter = MultiplyImageFilterType::New();
	multiplyImageFilter->SetInput(castFilter->GetOutput());
	multiplyImageFilter->SetConstant(calibF_A/65535.0);

#if CUDA_FOUND
	typedef itk::AddImageFilter <OutputImageType, OutputImageType, CUDAOutputImageType> AddImageFilterType;
#else
	typedef itk::AddImageFilter <OutputImageType, OutputImageType, OutputImageType> AddImageFilterType;
#endif
	AddImageFilterType::Pointer addImageFilter = AddImageFilterType::New();
	addImageFilter->SetInput1(multiplyImageFilter->GetOutput());
	double addingVal = calibF_B/65535.0;
	addImageFilter->SetConstant2(addingVal);
	addImageFilter->Update(); //will generate map of real_mu (att.coeff)

#if CUDA_FOUND
	CUDAOutputImageType::Pointer spCTImg_mu;
#else
	OutputImageType::Pointer spCTImg_mu;
#endif
	spCTImg_mu = addImageFilter->GetOutput();
	//2) Prepare empty projection images //Should be corresonponding to raw projection images

	// Create a stack of empty projection images
#if CUDA_FOUND
	typedef rtk::ConstantImageSource< CUDAOutputImageType > ConstantImageSourceType; //Output: FLoat image = may be mu_t = log(I_0/I)
#else
	typedef rtk::ConstantImageSource< OutputImageType > ConstantImageSourceType; //Output: FLoat image = may be mu_t = log(I_0/I)
#endif
	ConstantImageSourceType::Pointer constantImageSource = ConstantImageSourceType::New();

	ConstantImageSourceType::SizeType size;
	ConstantImageSourceType::SpacingType spacing;
	ConstantImageSourceType::PointType origin;

	cout << "Setting-up vacant projection image data" << endl;

	//a) size
	  //cout << "chk1" << endl;
	size[0] = qRound((double)DEFAULT_VARIAN_PROJ_WIDTH * m_fResampleF);
	size[1] = qRound((double)DEFAULT_VARIAN_PROJ_HEIGHT* m_fResampleF);
	size[2] = spGeometry->GetGantryAngles().size();
	iNumOfProjections = size[2];

	//b) spacing
	spacing[0] = m_fProjSpacingX / m_fResampleF;//typical HIS file
	spacing[1] = m_fProjSpacingY / m_fResampleF;
	spacing[2] = 1.0;

	//c) Origin: can center be the image center? or should be related to the CT image???
	origin[0] = spacing[0] * (size[0]-1) * -0.5;
	origin[1] = spacing[1] * (size[1]-1) * -0.5;
	origin[2] = 0.0;

	constantImageSource->SetOrigin( origin );
	constantImageSource->SetSpacing( spacing );

	OutputImageType::DirectionType imageDirection;
	imageDirection.SetIdentity(); //no effect
	constantImageSource->SetDirection( imageDirection );
	constantImageSource->SetSize( size );
	constantImageSource->SetConstant( 1.0 );
	constantImageSource->UpdateOutputInformation();
	cout << "Canvas for projection image is ready to write" << endl;

	//4) Prepare CT image to be projected
#if CUDA_FOUND
	int fwdMethod = en_CudaRayCast; // en_RayCastInterpolator; // en_CudaRayCast; //later, it will be coming from the GUI
#else
	int fwdMethod = en_Joseph; // en_RayCastInterpolator; // en_CudaRayCast; //later, it will be coming from the GUI
#endif
	cout << "projection algorithm (0:Joseph, 1: CUDA, 2:RayCast ): " << fwdMethod << endl;

	// Create forward projection image filter
#if CUDA_FOUND
	typedef rtk::CudaForwardProjectionImageFilter<CUDAOutputImageType, CUDAOutputImageType> CudaforwardProjectionType; //Float to Float
	CudaforwardProjectionType::Pointer CudaforwardProjection = CudaforwardProjectionType::New(); 
#else
	rtk::ForwardProjectionImageFilter<OutputImageType, OutputImageType>::Pointer forwardProjection; //Float to Float

	switch(fwdMethod)
	{
	case (en_Joseph):
	  forwardProjection = rtk::JosephForwardProjectionImageFilter<OutputImageType, OutputImageType>::New();
	  break;
	case (en_CudaRayCast):
#if CUDA_FOUND
	  std::cerr << "You should not be able to end up here!" << std::endl;
#else
	  std::cerr << "The program has not been compiled with cuda option" << std::endl;
	  return;
#endif
	  break;
	case(en_RayCastInterpolator):
	  forwardProjection = rtk::RayCastInterpolatorForwardProjectionImageFilter<OutputImageType, OutputImageType>::New();
	  break;

	default:
	  std::cerr << "Unhandled --method value." << std::endl;
	  return;
	}
#endif
	itk::TimeProbe projProbe;
	cout << "Forward projection is now ongoing" << endl;
#if CUDA_FOUND
	CudaforwardProjection->SetInput(constantImageSource->GetOutput());
	CudaforwardProjection->SetInput(1, spCTImg_mu); //reference plan CT image
	CudaforwardProjection->SetGeometry(spGeometry);
	projProbe.Start();
	TRY_AND_EXIT_ON_ITK_EXCEPTION(CudaforwardProjection->Update());
	projProbe.Stop();
	spResultProjImageFloat = CudaforwardProjection->GetOutput();
#else
	forwardProjection->SetInput( constantImageSource->GetOutput() ); //Canvas. projection image will be saved here.
	forwardProjection->SetInput( 1, spCTImg_mu ); //reference plan CT image
	forwardProjection->SetGeometry(spGeometry);

	projProbe.Start();
	TRY_AND_EXIT_ON_ITK_EXCEPTION(forwardProjection->Update());
	projProbe.Stop();

	spResultProjImageFloat = forwardProjection->GetOutput();
#endif
	
	cout << "Forward projection done by in method ID = " << fwdMethod << " in:	" << projProbe.GetMean() << ' ' << projProbe.GetUnit() << '.' << std::endl;
  }//release all the memory

  //From Float to USHORT and line integral to intensity

  spProjCT3D = USHORT_ImageType::New(); //later
  USHORT_ImageType::SizeType projCT_size = spResultProjImageFloat->GetLargestPossibleRegion().GetSize(); //1024 1024 350
  USHORT_ImageType::IndexType projCT_idxStart= spResultProjImageFloat->GetLargestPossibleRegion().GetIndex(); //0 0 0
  USHORT_ImageType::SpacingType projCT_spacing = spResultProjImageFloat->GetSpacing(); // 0.4 0.4 1.0
  USHORT_ImageType::PointType  projCT_origin = spResultProjImageFloat->GetOrigin(); //-204.6 -204.6 -174.5

  //Copy informations from spResultProjImageFloat
  OutputImageType::RegionType projCT_region;
  projCT_region.SetSize(projCT_size);
  projCT_region.SetIndex(projCT_idxStart);

  spProjCT3D->SetRegions(projCT_region);
  spProjCT3D->SetSpacing(projCT_spacing);
  spProjCT3D->SetOrigin(projCT_origin);

  spProjCT3D->Allocate();
  spProjCT3D->FillBuffer(0);

  //Calculation process
  itk::ImageRegionConstIterator<OutputImageType> itSrc (spResultProjImageFloat, spResultProjImageFloat->GetRequestedRegion());
  itk::ImageRegionIterator<USHORT_ImageType> itTarg (spProjCT3D, spProjCT3D->GetRequestedRegion()); //writing

  itSrc.GoToBegin();
  itTarg.GoToBegin();

  float fProjVal = 0.0;
  double tmpConvVal = 0.0;

  //Convert line integral to intensity value (I0/I = exp(mu_t)) --> I = I0/exp(mu_t)
  while(!itSrc.IsAtEnd() && !itTarg.IsAtEnd())
  {
	fProjVal = itSrc.Get(); // mu_t //63.5 --> 6.35
	tmpConvVal = (65535.0/exp(fProjVal)); //physically true

	if (tmpConvVal <= 0.0)
	  itTarg.Set(0);
	else if (tmpConvVal > 65535.0)
	  itTarg.Set(65535);
	else
	{
	  itTarg.Set((unsigned short)tmpConvVal);
	}
	++itSrc;
	++itTarg;
  }

  //spProjCT3D: USHORT IMAGE of intensity. Not inverted (physical intensity)

  if (bSave)
  {
	//Saving part: save as his file in sub-folder of raw image
	cout << "Files are being saved" << endl;
	cout << " Patient DIR Path: " << m_strPathPatientDir.toLocal8Bit().constData() << endl;

	if (m_strPathPatientDir.isEmpty())
	{
	  cout << "File save error!: No patient DIR name" << endl;
	  return;
	}

	//Get current folder
	QString strCrntDir = m_strPathPatientDir + "/" + "IMAGES"; //current Proj folder


	//Make a sub directory
	QDir crntDir(strCrntDir);

	if (!crntDir.exists())
	{
	  cout << "File save error: The specified folder does not exist." << endl;
	  return;
	}

	QString fwdDirName = "fwd_" + m_strDCMUID;

	bool tmpResult = crntDir.mkdir(fwdDirName); //what if the directory exists?

	if (!tmpResult)
	{
	  cout << "FwdProj directory seems to exist already. Files will be overwritten." << endl;
	}

	QString strSavingFolder = strCrntDir + "/" + fwdDirName;
	SaveProjImageAsHIS(spProjCT3D, m_arrYKBufProj,strSavingFolder, m_iCntSelectedProj, m_fResampleF);
  }
}

void CbctRecon::SaveProjImageAsHIS( USHORT_ImageType::Pointer& spProj3D, YK16GrayImage* arrYKImage, QString& strSavingFolder, int iCnt, double resampleF )
{
  cout << "Starting Saving files" << endl;
  FILE* fd = NULL;

   USHORT_ImageType::Pointer targetImg3D;
   double restoreResampleF =  1.0/resampleF;

  if (resampleF != 1.0)
  {
	cout << "restore the resampled image by applying a factor of " << restoreResampleF << endl;
	ResampleItkImage(spProj3D, targetImg3D,restoreResampleF);
  }
  else
  {
	targetImg3D = spProj3D;
  }

  itk::ImageSliceConstIteratorWithIndex<USHORT_ImageType> it_FwdProj (targetImg3D, targetImg3D->GetRequestedRegion());

  it_FwdProj.SetFirstDirection(0);
  it_FwdProj.SetSecondDirection(1);
  it_FwdProj.GoToBegin();

  for (int i = 0 ; i< iCnt && !it_FwdProj.IsAtEnd(); i++)
  {
	QFileInfo crntFileInfo(arrYKImage[i].m_strFilePath);

	QString crntFileName = crntFileInfo.fileName();
	QString crntPath = strSavingFolder + "/" + crntFileName;

	fd = fopen(crntPath.toLocal8Bit().constData(), "wb");
	fwrite(arrYKImage[i].m_pElektaHisHeader, 100, 1, fd); //this buffer only include header info

	//int imgSize = m_arrYKBufProj[i].m_iWidth * m_arrYKImage[i].m_iHeight;

	//Search matching slice using slice iterator for m_spProjCTImg
	while( !it_FwdProj.IsAtEndOfSlice() )
	{
	  while( !it_FwdProj.IsAtEndOfLine() )
	  {
		unsigned short tmpVal = (unsigned short)(it_FwdProj.Get());
		tmpVal = 65535 - tmpVal; //inverse is done here

		fwrite(&tmpVal, 2, 1, fd);
		++it_FwdProj;
	  }
	  it_FwdProj.NextLine();
	}
	fclose(fd);

	it_FwdProj.NextSlice();
	//cout << "Now saving " << i+1 << " th file: " << crntFileName.toLocal8Bit().constData() << endl;
  }

  cout << "Saving completed" << endl;
}

void CbctRecon::SLT_DoScatterCorrection_APRIORI()
{

  bool bExportProj_Fwd = ui.checkBox_ExportFwd->isChecked();
  bool bExportProj_Scat = ui.checkBox_ExportScat->isChecked();
  bool bExportProj_Cor = ui.checkBox_ExportCor->isChecked();


  //ForwardProjection(m_spRefCTImg, m_spCustomGeometry, m_spProjImgCT3D, false); //final moving image
  if (m_pDlgRegistration->m_spMoving)
	ForwardProjection(m_pDlgRegistration->m_spMoving, m_spCustomGeometry, m_spProjImgCT3D, bExportProj_Fwd); //final moving image
  else if (m_spRefCTImg)
  {
	cout << "No Moving image in Registration is found. Ref CT image will be used instead" << endl;
	ForwardProjection(m_spRefCTImg, m_spCustomGeometry, m_spProjImgCT3D, bExportProj_Fwd); //final moving image
  }
  else
  {
	cout << "Error!: No ref image for forward projection is found." << endl;
	return;
  }

  //double scaResam = ui.lineEdit_scaResam->text().toDouble();
  double scaMedian = ui.lineEdit_scaMedian->text().toDouble();
  double scaGaussian = ui.lineEdit_scaGaussian->text().toDouble();

  cout << "Generating scatter map is ongoing..." << endl;

  cout << "To account for the mAs values, the intensity scale factor of "<< GetRawIntensityScaleFactor() << "will be multiplied during scatter correction to avoid negative scatter" << endl;
  GenScatterMap_PriorCT(m_spProjImgRaw3D, m_spProjImgCT3D, m_spProjImgScat3D, scaMedian,scaGaussian,m_iFixedOffset_ScatterMap, bExportProj_Scat );	//void GenScatterMap2D_PriorCT()
  m_spProjImgCT3D->Initialize(); //memory saving

  cout << "Scatter correction is in progress..." << endl;

  int postScatMedianSize = ui.lineEdit_scaPostMedian->text().toInt();
  ScatterCorr_PrioriCT(m_spProjImgRaw3D, m_spProjImgScat3D, m_spProjImgCorr3D, m_iFixedOffset_ScatterMap,postScatMedianSize, bExportProj_Cor);
  m_spProjImgScat3D->Initialize(); //memory saving

  cout << "AfterCorrectionMacro is ongoing..." << endl;
  AfterScatCorrectionMacro();
  cout << "FINISHED!Scatter correction: CBCT DICOM files are saved" << endl;
}

//spProjRaw3D: raw intensity value (0-65535), spProjCT3D: raw intensity value (0-65535)
void CbctRecon::GenScatterMap_PriorCT(USHORT_ImageType::Pointer& spProjRaw3D, USHORT_ImageType::Pointer& spProjCT3D, USHORT_ImageType::Pointer& spProjScat3D, double medianRadius, double gaussianSigma, int nonNegativeScatOffset, bool bSave)
{
  //Scatter map: should be 2D to use 2D median, Gaussian filters
  if (m_iCntSelectedProj < 1)
  {
	cout << "error: no count of proj image" << endl;
	return;
  }

  if (!spProjRaw3D || !spProjCT3D)
  {
	cout << "error: proj image 3D is not ready" << endl;
	return;
  }

  USHORT_ImageType::SizeType size1 = spProjRaw3D->GetRequestedRegion().GetSize();
  USHORT_ImageType::SizeType size2 = spProjCT3D->GetRequestedRegion().GetSize();

  cout << "Raw3DProj Size= " << size1 << endl;
  cout << "spProjCT Size= " << size2 << endl;

  bool bHighResolMacro = false; // raw imag= 1024, scattermap = 512

  if (size1[0] != size2[0] ||
	  size1[1] != size2[1] ||
	  size1[2] != size2[2])
  {
	cout << "Raw and CT projection dimension are not matching. under the high resolution macro?" << endl;

        if (size1[0] == qRound(size2[0] * 2.0) && size1[1] == qRound(size2[1] * 2.0) && size1[2] == size2[2])
        {
            bHighResolMacro = true;
        }
        else
        {
            return;
        }
  }

  USHORT_ImageType::Pointer spTmpProjRaw3D;

  if (bHighResolMacro)
  {
	  cout << "bHighResolMacro is unexpectedly on" << endl;
	  ResampleItkImage(spProjRaw3D, spTmpProjRaw3D, 0.5);
  }
  else
  {
      spTmpProjRaw3D = spProjRaw3D;
  }



  AllocateByRef(spTmpProjRaw3D, spProjScat3D);
  //AllocateByRef(spProjCT3D, spProjScat3D);

 // cout << "Scat3D size = " << spProjScat3D->GetRequestedRegion().GetSize() << endl;

  USHORT_ImageType::SizeType imgSize = spTmpProjRaw3D->GetRequestedRegion().GetSize();
  //USHORT_ImageType::SizeType imgSize = spProjCT3D->GetRequestedRegion().GetSize();

  //USHORT_ImageType::SizeType imgSize = spSrcImg3D->GetBufferedRegion().GetSize();
  //Create spProjScat3D with same dimension of the spProjRaw3D
  int iSizeZ = imgSize[2];

 // cout << "resample factor " << resF2D << endl;

  double mAs_correctionFactor = GetRawIntensityScaleFactor();
  for (int i = 0 ; i < iSizeZ ; i++)
  {
	OutputImageType2D::Pointer spImg2DRaw;
	OutputImageType2D::Pointer spImg2DPrim;
	OutputImageType2D::Pointer spImg2DScat;

        Get2DFrom3D(spTmpProjRaw3D, spImg2DRaw, i, PLANE_AXIAL); //simple conversion between ushort 3D to float 2D (using casting, not log): input/output: 0-65535
	Get2DFrom3D(spProjCT3D, spImg2DPrim, i, PLANE_AXIAL);

	//YK16GrayImage tmpYKRaw, tmpYKPrim;
	//tmpYKRaw.UpdateFromItkImageFloat(spImg2DRaw);
	////tmpYKPrim.UpdateFromItkImageFloat(spImg2DPrim);

	//QString str1 = QString("D:\\testYK\\imageRaw_%1.raw").arg(i);
	////QString str2 = QString("D:\\testYK\\imagePrim_%1.raw").arg(i);
	//
	//tmpYKRaw.SaveDataAsRaw(str1.toLocal8Bit().constData());
	////tmpYKRaw.SaveDataAsRaw(str2.toLocal8Bit().constData());

	//cout << "i = " << i << endl;
	//continue;

	//Dimension should be matched
	AllocateByRef(spImg2DRaw, spImg2DScat);

	itk::ImageRegionConstIteratorWithIndex<OutputImageType2D> it_Src1(spImg2DRaw, spImg2DRaw->GetRequestedRegion());
	itk::ImageRegionConstIteratorWithIndex<OutputImageType2D> it_Src2(spImg2DPrim, spImg2DPrim->GetRequestedRegion());
	itk::ImageRegionIteratorWithIndex<OutputImageType2D> it_Tar(spImg2DScat, spImg2DScat->GetRequestedRegion());

	int cnt1=0; int cnt2 = 0;
	for (it_Src1.GoToBegin(), it_Src2.GoToBegin(), it_Tar.GoToBegin(); !it_Src1.IsAtEnd() && !it_Src2.IsAtEnd() && !it_Tar.IsAtEnd() ; ++it_Src1, ++it_Src2, ++it_Tar)
	{
	  float intensityValScat = it_Src1.Get()*mAs_correctionFactor - it_Src2.Get(); //raw intensity * mAs_CF - primary intensity (
	  it_Tar.Set(intensityValScat);//float 	  //allow minus value
	}

	//If CUDA is selected, do this using CUDA

	//Resampling to speed-up

	//ResampleItkImage2D(spImg2DScat, spImg2DScat, resF2D);
	typedef itk::MedianImageFilter<OutputImageType2D, OutputImageType2D> MedianFilterType;

	MedianFilterType::Pointer medianFilterX = MedianFilterType::New();
	MedianFilterType::InputSizeType radiusX;
	radiusX[0] = medianRadius;
	radiusX[1] = 0;
	medianFilterX->SetRadius(radiusX);
	medianFilterX->SetInput(spImg2DScat);
	//medianFilterX->Update();
	//spImg2DScat = medianFilterX->GetOutput();

	MedianFilterType::Pointer medianFilterY = MedianFilterType::New();
	MedianFilterType::InputSizeType radiusY;
	radiusY[0] = 0;
	radiusY[1] = medianRadius;
	medianFilterY->SetRadius(radiusY);
	medianFilterY->SetInput(medianFilterX->GetOutput());
	medianFilterY->Update();
	spImg2DScat = medianFilterY->GetOutput();

	typedef itk::SmoothingRecursiveGaussianImageFilter<OutputImageType2D, OutputImageType2D>  SmoothingFilterType;
	SmoothingFilterType::Pointer gaussianFilter = SmoothingFilterType::New();
	//gaussianFilter->SetInput(medianFilter->GetOutput());
	gaussianFilter->SetInput(spImg2DScat);
	//gaussianFilter->SetSigma(gaussianSigma); //filter specific setting for 512x 512 image (elekta/2)
	SmoothingFilterType::SigmaArrayType gaussianSigmaArray;
	gaussianSigmaArray[0] = gaussianSigma;
	gaussianSigmaArray[1] = .75 * gaussianSigma;
	gaussianFilter->SetSigmaArray(gaussianSigmaArray); //filter specific setting for 512x384 (varian/2)
	//spImg2DScat = gaussianFilter->GetOutput();
	typedef itk::AddImageFilter<OutputImageType2D, OutputImageType2D, OutputImageType2D> AddImageFilterType;
	AddImageFilterType::Pointer addFilter = AddImageFilterType::New();
	addFilter->SetInput1(gaussianFilter->GetOutput());
	addFilter->SetConstant2((float)nonNegativeScatOffset);
	addFilter->Update();
	spImg2DScat = addFilter->GetOutput(); //even after the offset applied, - value is still possible

	//float to unsigned short
	Set2DTo3D(spImg2DScat, spProjScat3D, i, PLANE_AXIAL); //input/Output: 0-65535 intensity valuesno mu_t to intensity converion is involved

	int unit = qRound(iSizeZ/10.0);
	if (i%unit == 0)
	{
	  cout << "Generating scatter map: " << (i/(double)unit)*10.0  << " % is done" << endl;
	}
  }//end of for

  if (bSave)
  {
	//Saving part: save as his file in sub-folder of raw image
	cout << "Files are being saved" << endl;
	cout << "Patient DIR Path: " << m_strPathPatientDir.toLocal8Bit().constData() << endl;

	if (m_strPathPatientDir.isEmpty())
	{
	  cout << "File save error!: No patient DIR name" << endl;
	  return;
	}

	//Get current folder
	QString strCrntDir = m_strPathPatientDir + "/" + "IMAGES"; //current Proj folder


	//Make a sub directory
	QDir crntDir(strCrntDir);

	if (!crntDir.exists())
	{
	  cout << "File save error: The specified folder does not exist." << endl;
	  return;
	}

	QString scatDirName = "sca_" + m_strDCMUID;

	bool tmpResult = crntDir.mkdir(scatDirName); //what if the directory exists?

	if (!tmpResult)
	{
	  cout << "Scatter map directory seems to exist already. Files will be overwritten." << endl;
	}

	QString strSavingFolder = strCrntDir + "/" + scatDirName;
	SaveProjImageAsHIS(spProjScat3D, m_arrYKBufProj,strSavingFolder, m_iCntSelectedProj, m_fResampleF);
  }
}

void CbctRecon::ScatterCorr_PrioriCT(USHORT_ImageType::Pointer& spProjRaw3D, USHORT_ImageType::Pointer& spProjScat3D, USHORT_ImageType::Pointer& m_spProjCorr3D, int nonNegativeScatOffset, int postMedian, bool bSave)
{
  //Scatter map: should be 2D to use 2D median, Gaussian filters
  if (m_iCntSelectedProj < 1)
  {
	cout << "error: no count of proj image" << endl;
	return;
  }

  if (!spProjRaw3D || !spProjScat3D)
  {
	cout << "Error: proj image 3D is not ready" << endl;
	return;
  }

  USHORT_ImageType::SizeType size1 = spProjRaw3D->GetRequestedRegion().GetSize();
  USHORT_ImageType::SizeType size2 = spProjScat3D->GetRequestedRegion().GetSize();

  cout << "Raw3DProj Size= " << size1 << endl;
  cout << "spProjScat3D Size= " << size2 << endl;

  bool bHighResolMacro = false;

  if (size1[0] != size2[0] ||
      size1[1] != size2[1] ||
      size1[2] != size2[2])
  {
      cout << "Raw and scatter projection dimension are not matching. under the high resolution macro?" << endl;

      if (size1[0] == qRound(size2[0] * 2.0) && size1[1] == qRound(size2[1] * 2.0) && size1[2] == size2[2])
      {
          bHighResolMacro = true;
      }
      else
      {
          return;
      }
  }

  USHORT_ImageType::Pointer spTmpProjScat3D;

  if (bHighResolMacro)
  {
      ResampleItkImage(spProjScat3D, spTmpProjScat3D, 2.0);
  }
  else
  {
      spTmpProjScat3D = spProjScat3D;
  }


  AllocateByRef(spProjRaw3D, m_spProjCorr3D);


  USHORT_ImageType::SizeType imgSize = spProjRaw3D->GetRequestedRegion().GetSize();

  //USHORT_ImageType::SizeType imgSize = spSrcImg3D->GetBufferedRegion().GetSize();
  //Create spProjScat3D with same dimension of the spProjRaw3D
  int iSizeZ = imgSize[2];

  // cout << "resample factor " << resF2D << endl;.

  double mAs_correctionFactor = GetRawIntensityScaleFactor();
  for (int i = 0 ; i < iSizeZ ; i++)
  {
	OutputImageType2D::Pointer spImg2DRaw;
	OutputImageType2D::Pointer spImg2DScat;
	OutputImageType2D::Pointer spImg2DCorr;

	Get2DFrom3D(spProjRaw3D, spImg2DRaw, i, PLANE_AXIAL);
        Get2DFrom3D(spTmpProjScat3D, spImg2DScat, i, PLANE_AXIAL);

	//Dimension should be matched
	AllocateByRef(spImg2DRaw, spImg2DCorr);

	itk::ImageRegionConstIteratorWithIndex<OutputImageType2D> it_Src1(spImg2DRaw, spImg2DRaw->GetRequestedRegion());
	itk::ImageRegionConstIteratorWithIndex<OutputImageType2D> it_Src2(spImg2DScat, spImg2DScat->GetRequestedRegion());
	itk::ImageRegionIteratorWithIndex<OutputImageType2D> it_Tar(spImg2DCorr, spImg2DCorr->GetRequestedRegion());

	float rawVal, scatVal, corrVal;

	for (it_Src1.GoToBegin(), it_Src2.GoToBegin(), it_Tar.GoToBegin(); !it_Src1.IsAtEnd() && !it_Src2.IsAtEnd() && !it_Tar.IsAtEnd() ; ++it_Src1, ++it_Src2, ++it_Tar)
	{
	  rawVal = it_Src1.Get()*mAs_correctionFactor;
	  scatVal = it_Src2.Get() - nonNegativeScatOffset;
	  corrVal = rawVal - scatVal;

	  if (corrVal < 1.0)
		corrVal = 1.0;
	  if (corrVal > 65534.0) //65535 -->(inversion) --> 0 --> LOg (65536 / 0) = ERROR!
		corrVal = 65534.0; // HEY AGRAVGAARD TODO HERE

	  it_Tar.Set(corrVal);//float // later, add customSPR
	  		//corrVal = (float)(rawVal - customSPR*scatterVal);
	}
	//Post Median filtering

        if (bHighResolMacro)
            postMedian = postMedian*2.0;

	if (postMedian >= 2)//YK2015
	{
		typedef itk::MedianImageFilter<OutputImageType2D, OutputImageType2D> MedianFilterType;
		MedianFilterType::Pointer medianFilter = MedianFilterType::New();
		MedianFilterType::InputSizeType radius;

		radius[0] = qRound(postMedian / 2.0);
		radius[1] = qRound(postMedian / 2.0);

	/*	if (ui.radioButton_UseCUDA->isChecked())
		{
			int wndX = radius[0] * 2 + 1;
			int wndY = radius[1] * 2 + 1;

			cudaMedianFilter2DITK(spImg2DCorr, wndX, wndY);
		}
		else
		{*/
		medianFilter->SetRadius(radius);
		medianFilter->SetInput(spImg2DCorr);
		medianFilter->Update();
		spImg2DCorr = medianFilter->GetOutput();
		//}
	}

	Set2DTo3D(spImg2DCorr, m_spProjCorr3D, i, PLANE_AXIAL);//float2D to USHORT

	int unit = qRound(iSizeZ/10.0);
	if (i%unit == 0)
	{
	  cout << "Applying scatter correction: " << (i/(double)unit)*10.0  << " % is done" << endl;
	}
  }

  if (bSave)
  {
	//Saving part: save as his file in sub-folder of raw image
	cout << "Files are being saved" << endl;
	cout << "Patient DIR Path: " << m_strPathPatientDir.toLocal8Bit().constData() << endl;

	if (m_strPathPatientDir.isEmpty())
	{
	  cout << "File save error!: No patient DIR name" << endl;
	  return;
	}

	//Get current folder
	QString strCrntDir = m_strPathPatientDir + "/" + "IMAGES"; //current Proj folder


	//Make a sub directory
	QDir crntDir(strCrntDir);

	if (!crntDir.exists())
	{
	  cout << "File save error: The specified folder does not exist." << endl;
	  return;
	}

	QString scatDirName = "cor_" + m_strDCMUID;

	bool tmpResult = crntDir.mkdir(scatDirName); //what if the directory exists?

	if (!tmpResult)
	{
	  cout << "Corrected projection directory seems to exist already. Files will be overwritten." << endl;
	}

	QString strSavingFolder = strCrntDir + "/" + scatDirName;

        if (!bHighResolMacro)
            SaveProjImageAsHIS(m_spProjCorr3D, m_arrYKBufProj,strSavingFolder, m_iCntSelectedProj, m_fResampleF);
        else
            SaveProjImageAsHIS(m_spProjCorr3D, m_arrYKBufProj, strSavingFolder, m_iCntSelectedProj, 1.0);
  }
  //spProjScat3D->Initialize(); //memory release
}



//spSrcImg3D: usually projImage in USHORT type
void CbctRecon::Get2DFrom3D( USHORT_ImageType::Pointer& spSrcImg3D, OutputImageType2D::Pointer& spTargetImg2D, int idx, enPLANE iDirection)
{
  if (!spSrcImg3D)
	return;

  int idxHor,idxVer, idxZ;

  switch (iDirection)
  {
  case PLANE_AXIAL:
	idxHor = 0;
	idxVer = 1;
	idxZ = 2;
	break;
  case PLANE_FRONTAL:
	idxHor = 0;
	idxVer = 2;
	idxZ = 1;
	break;
  case PLANE_SAGITTAL:
	idxHor = 1;
	idxVer = 2;
	idxZ = 0;
	break;
  }

  //Create 2D target image based on geometry of 3D
  USHORT_ImageType::SizeType imgDim = spSrcImg3D->GetBufferedRegion().GetSize();
  USHORT_ImageType::SpacingType spacing = spSrcImg3D->GetSpacing();
  USHORT_ImageType::PointType origin = spSrcImg3D->GetOrigin();

  int width = imgDim[idxHor];
  int height  = imgDim[idxVer];
  int zSize = imgDim[idxZ];
  //cout << "Get2DFrom3D zSize = " << zSize << endl;



  if (idx < 0 || idx >= zSize)
  {
	cout << "Error! idx is out of the range" << endl;
	return;
  }

  OutputImageType2D::IndexType idxStart;
  idxStart[0] = 0;
  idxStart[1] = 0;

  OutputImageType2D::SizeType size2D;
  size2D[0] = imgDim[idxHor];
  size2D[1] = imgDim[idxVer];

  OutputImageType2D::SpacingType spacing2D;
  spacing2D[0] = spacing[idxHor];
  spacing2D[1] = spacing[idxVer];

  OutputImageType2D::PointType origin2D;
//  origin2D[0] = origin[idxHor];
//  origin2D[1] = origin[idxVer];
  origin2D[0] = size2D[0]*spacing2D[0]/-2.0;
  origin2D[1] = size2D[1]*spacing2D[1]/-2.0;


  OutputImageType2D::RegionType region;
  region.SetSize(size2D);
  region.SetIndex(idxStart);

  //spTargetImg2D is supposed to be empty.
  if (spTargetImg2D)
  {
	cout << "something is here in target image. is it gonna be overwritten?" << endl;
  }

  spTargetImg2D = OutputImageType2D::New();
  spTargetImg2D->SetRegions(region);
  spTargetImg2D->SetSpacing(spacing2D);
  spTargetImg2D->SetOrigin(origin2D);

  spTargetImg2D->Allocate();
  spTargetImg2D->FillBuffer(0);

  //cout << "src size = " << spSrcImg3D->GetRequestedRegion().GetSize() << " " << endl;
  //cout << "target image size = " << spTargetImg2D->GetRequestedRegion().GetSize() << " " << endl;


  itk::ImageSliceConstIteratorWithIndex<USHORT_ImageType> it_3D (spSrcImg3D, spSrcImg3D->GetRequestedRegion());
  //itk::ImageRegionIteratorWithIndex<OutputImageType2D> it_2D (spTargetImg2D, spTargetImg2D->GetRequestedRegion());
  itk::ImageRegionIterator<OutputImageType2D> it_2D (spTargetImg2D, spTargetImg2D->GetRequestedRegion());

  it_3D.SetFirstDirection(idxHor);
  it_3D.SetSecondDirection(idxVer);

  it_3D.GoToBegin();
  it_2D.GoToBegin();


  for (int i = 0 ; i< zSize && !it_3D.IsAtEnd();i++)
  {
	/*QFileInfo crntFileInfo(arrYKImage[i].m_strFilePath);
	QString crntFileName = crntFileInfo.fileName();
	QString crntPath = strSavingFolder + "/" + crntFileName;*/
	//Search matching slice using slice iterator for m_spProjCTImg
	//cout << "Get2DFrom3D: Slide= " << i  << " ";

	if (i == idx)
	{
	  while( !it_3D.IsAtEndOfSlice() ) //Error here why?
	  {
		while( !it_3D.IsAtEndOfLine() )
		{
		  float tmpVal = (float)(it_3D.Get()); //in proj image case, this is intensity
		  it_2D.Set(tmpVal);
		  ++it_2D;
		  ++it_3D;
		}//while2
		it_3D.NextLine();
	  }//while1
	  break;
	}	// end if
	it_3D.NextSlice();
  }	//end of for

  //cout << "cnt = " << cnt << " TotCnt " << cntTot << endl;
  /*YK16GrayImage tmpYK;
  tmpYK.UpdateFromItkImageFloat(spTargetImg2D);
  QString str = QString("D:\\testYK\\InsideFunc_%1.raw").arg(idx);
  tmpYK.SaveDataAsRaw(str.toLocal8Bit().constData());*/
}


void CbctRecon::Set2DTo3D( OutputImageType2D::Pointer& spSrcImg2D, USHORT_ImageType::Pointer& spTargetImg3D, int idx, enPLANE iDirection )
{
  if (!spSrcImg2D || !spTargetImg3D) //Target image should be also ready.
	return;

  int idxHor,idxVer, idxZ;

  switch (iDirection)
  {
  case PLANE_AXIAL:
	idxHor = 0;
	idxVer = 1;
	idxZ = 2;
	break;
  case PLANE_FRONTAL:
	idxHor = 0;
	idxVer = 2;
	idxZ = 1;
	break;
  case PLANE_SAGITTAL:
	idxHor = 1;
	idxVer = 2;
	idxZ = 0;
	break;
  }

  OutputImageType2D::SizeType imgDim2D = spSrcImg2D->GetBufferedRegion().GetSize();
  OutputImageType2D::SpacingType spacing2D = spSrcImg2D->GetSpacing();
  OutputImageType2D::PointType origin2D = spSrcImg2D->GetOrigin();

  USHORT_ImageType::SizeType imgDim3D = spTargetImg3D->GetBufferedRegion().GetSize();
  USHORT_ImageType::SpacingType spacing3D = spTargetImg3D->GetSpacing();
  USHORT_ImageType::PointType origin3D = spTargetImg3D->GetOrigin();

  //Filtering
  if (imgDim2D[0] != imgDim3D[idxHor] ||
	imgDim2D[1] != imgDim3D[idxVer] || idx < 0 || idx >= imgDim3D[idxZ])
  {
	cout << "Error: image dimensions is not matching" << endl;
	cout << "2D= "  << imgDim2D << endl;
	cout << "3D= "  << imgDim3D << endl;
	return;
  }
  /*int width = imgDim[idxHor];
  int height  = imgDim[idxVer];*/



  //itk::ImageRegionConstIteratorWithIndex<OutputImageType2D> it_2D (spSrcImg2D, spSrcImg2D->GetRequestedRegion());
  itk::ImageRegionConstIterator<OutputImageType2D> it_2D (spSrcImg2D, spSrcImg2D->GetRequestedRegion());
  itk::ImageSliceIteratorWithIndex<USHORT_ImageType> it_3D (spTargetImg3D, spTargetImg3D->GetRequestedRegion());

  it_3D.SetFirstDirection(idxHor);
  it_3D.SetSecondDirection(idxVer);
  it_3D.GoToBegin();

  int zSize = imgDim3D[idxZ];

  it_2D.GoToBegin();

  float fVal2D = 0.0;
  unsigned short outputVal =0;

  for (int i = 0 ; i< zSize && !it_3D.IsAtEnd(); i++)
  {
	/*QFileInfo crntFileInfo(arrYKImage[i].m_strFilePath);
	QString crntFileName = crntFileInfo.fileName();
	QString crntPath = strSavingFolder + "/" + crntFileName;*/
	//Search matching slice using slice iterator for m_spProjCTImg
	if (i == idx)
	{
	  while( !it_3D.IsAtEndOfSlice() )
	  {
		while( !it_3D.IsAtEndOfLine() )
		{
		  fVal2D = it_2D.Get();

		  if (fVal2D < 0.0)
			outputVal = 0;
		  else if (fVal2D > 65535.0)
			outputVal = 65535;
		  else
			outputVal = (unsigned short)qRound(fVal2D);

		  it_3D.Set(outputVal);
		  //float tmpVal = (float)(it_3D.Get()); //in proj image case, this is intensity
		  //it_2D.Set(tmpVal);
		  ++it_2D;
		  ++it_3D;
		}//while2
		it_3D.NextLine();
	  }//while1
	  break;
	}
	//
	it_3D.NextSlice();
  }//end of for
}

//void CbctRecon::Get2DFrom3D( OutputImageType::Pointer& spSrcImg3D, OutputImageType2D::Pointer& spTargetImg2D, enPLANE iDirection)
//{
//
//}



//From line integral to raw intensity
//bkIntensity is usually 65535
void CbctRecon::ConvertLineInt2Intensity( OutputImageType::Pointer& spProjLineInt3D, USHORT_ImageType::Pointer& spProjIntensity3D, int bkIntensity )
{
  if (!spProjLineInt3D)
	return;
  //OutputImageType::IMageRegionIteratorWithIndex

  AllocateByRef(spProjLineInt3D, spProjIntensity3D);

  itk::ImageRegionConstIteratorWithIndex<OutputImageType> it_Src(spProjLineInt3D, spProjLineInt3D->GetRequestedRegion());
  itk::ImageRegionIteratorWithIndex<USHORT_ImageType> it_Tar(spProjIntensity3D, spProjIntensity3D->GetRequestedRegion());

  for (it_Src.GoToBegin(), it_Tar.GoToBegin(); !it_Src.IsAtEnd() && !it_Tar.IsAtEnd() ; ++it_Src, ++it_Tar)
  {
	float intensityVal = exp((double)it_Src.Get() * (-1.0)) * (double)bkIntensity;

	if (intensityVal <= 1.0)
	  intensityVal = 1.0;
	if (intensityVal >= 65534)
	  intensityVal = 65534.0;

	it_Tar.Set((unsigned short)intensityVal);
  }
}


void CbctRecon::ConvertIntensity2LineInt( USHORT_ImageType::Pointer& spProjIntensity3D, OutputImageType::Pointer& spProjLineInt3D, int bkIntensity)
{
  if (!spProjIntensity3D)
	return;
  //OutputImageType::IMageRegionIteratorWithIndex

  AllocateByRef(spProjIntensity3D, spProjLineInt3D);

  itk::ImageRegionConstIteratorWithIndex<USHORT_ImageType> it_Src(spProjIntensity3D, spProjIntensity3D->GetRequestedRegion());
  itk::ImageRegionIteratorWithIndex<OutputImageType> it_Tar(spProjLineInt3D, spProjLineInt3D->GetRequestedRegion());



  for (it_Src.GoToBegin(), it_Tar.GoToBegin(); !it_Src.IsAtEnd() && !it_Tar.IsAtEnd() ; ++it_Src, ++it_Tar)
  {
	//mu = ln(I_0/I) OR mu = ln(I/I0)
	float mu_t_val = log((double)bkIntensity/(double)it_Src.Get());
	it_Tar.Set(mu_t_val);
  }
}


void CbctRecon::AllocateByRef( OutputImageType::Pointer& spRefImg3D, OutputImageType::Pointer& spTarImg3D )
{
  OutputImageType::SizeType sizeSrc = spRefImg3D->GetBufferedRegion().GetSize();
  OutputImageType::IndexType startSrc = spRefImg3D->GetBufferedRegion().GetIndex();

  OutputImageType::SpacingType spacingSrc = spRefImg3D->GetSpacing();
  OutputImageType::PointType originSrc = spRefImg3D->GetOrigin();

  OutputImageType::RegionType region;
  region.SetSize(sizeSrc);
  region.SetIndex(startSrc);

  spTarImg3D = OutputImageType::New();

  spTarImg3D->SetRegions(region);
  spTarImg3D->SetSpacing(spacingSrc);
  spTarImg3D->SetOrigin(originSrc);

  spTarImg3D->Allocate();
  spTarImg3D->FillBuffer(0);
}

void CbctRecon::AllocateByRef( USHORT_ImageType::Pointer& spRefImg3D, USHORT_ImageType::Pointer& spTarImg3D )
{
  USHORT_ImageType::SizeType sizeSrc = spRefImg3D->GetBufferedRegion().GetSize();
  USHORT_ImageType::IndexType startSrc = spRefImg3D->GetBufferedRegion().GetIndex();

  USHORT_ImageType::SpacingType spacingSrc = spRefImg3D->GetSpacing();
  USHORT_ImageType::PointType originSrc = spRefImg3D->GetOrigin();

  USHORT_ImageType::RegionType region;
  region.SetSize(sizeSrc);
  region.SetIndex(startSrc);

  spTarImg3D = USHORT_ImageType::New();

  spTarImg3D->SetRegions(region);
  spTarImg3D->SetSpacing(spacingSrc);
  spTarImg3D->SetOrigin(originSrc);

  spTarImg3D->Allocate();
  spTarImg3D->FillBuffer(0);
}


void CbctRecon::AllocateByRef( OutputImageType2D::Pointer& spRefImg2D, OutputImageType2D::Pointer& spTarImg2D )
{
  OutputImageType2D::SizeType sizeSrc = spRefImg2D->GetBufferedRegion().GetSize();
  OutputImageType2D::IndexType startSrc = spRefImg2D->GetBufferedRegion().GetIndex();

  OutputImageType2D::SpacingType spacingSrc = spRefImg2D->GetSpacing();
  OutputImageType2D::PointType originSrc = spRefImg2D->GetOrigin();

  OutputImageType2D::RegionType region;
  region.SetSize(sizeSrc);
  region.SetIndex(startSrc);

  spTarImg2D = OutputImageType2D::New();

  spTarImg2D->SetRegions(region);
  spTarImg2D->SetSpacing(spacingSrc);
  spTarImg2D->SetOrigin(originSrc);

  spTarImg2D->Allocate();
  spTarImg2D->FillBuffer(0);
}

void CbctRecon::AllocateByRef( USHORT_ImageType::Pointer& spRefImg3D, OutputImageType::Pointer& spTarImg3D )
{
  USHORT_ImageType::SizeType sizeSrc = spRefImg3D->GetBufferedRegion().GetSize();
  USHORT_ImageType::IndexType startSrc = spRefImg3D->GetBufferedRegion().GetIndex();
  USHORT_ImageType::SpacingType spacingSrc = spRefImg3D->GetSpacing();
  USHORT_ImageType::PointType originSrc = spRefImg3D->GetOrigin();


  OutputImageType::SizeType size =  (OutputImageType::SizeType)sizeSrc;
  OutputImageType::IndexType start = (OutputImageType::IndexType)startSrc;
  OutputImageType::SpacingType spacing = (OutputImageType::SpacingType)spacingSrc;
  OutputImageType::PointType origin = (OutputImageType::PointType)originSrc;


  OutputImageType::RegionType region;
  region.SetSize(size);
  region.SetIndex(start);

  spTarImg3D = OutputImageType::New();

  spTarImg3D->SetRegions(region);
  spTarImg3D->SetSpacing(spacing);
  spTarImg3D->SetOrigin(origin);

  spTarImg3D->Allocate();
  spTarImg3D->FillBuffer(0);

}

void CbctRecon::AllocateByRef( OutputImageType::Pointer& spRefImg3D, USHORT_ImageType::Pointer& spTarImg3D )
{
  OutputImageType::SizeType sizeSrc = spRefImg3D->GetBufferedRegion().GetSize();
  OutputImageType::IndexType startSrc = spRefImg3D->GetBufferedRegion().GetIndex();
  OutputImageType::SpacingType spacingSrc = spRefImg3D->GetSpacing();
  OutputImageType::PointType originSrc = spRefImg3D->GetOrigin();


  USHORT_ImageType::SizeType size =  (USHORT_ImageType::SizeType)sizeSrc;
  USHORT_ImageType::IndexType start = (USHORT_ImageType::IndexType)startSrc;
  USHORT_ImageType::SpacingType spacing = (USHORT_ImageType::SpacingType)spacingSrc;
  USHORT_ImageType::PointType origin = (USHORT_ImageType::PointType)originSrc;

  USHORT_ImageType::RegionType region;

  region.SetSize(size);
  region.SetIndex(start);

  spTarImg3D = USHORT_ImageType::New();

  spTarImg3D->SetRegions(region);
  spTarImg3D->SetSpacing(spacing);
  spTarImg3D->SetOrigin(origin);

  spTarImg3D->Allocate();
  spTarImg3D->FillBuffer(0);

}
//it works! new memory will be allocated for spTarImg
void CbctRecon::ResampleItkImage( OutputImageType::Pointer& spSrcImg, OutputImageType::Pointer& spTarImg, double resFactor )
{
  if (!spSrcImg)
	return;

//  cout << "original Origin: " << spSrcImg2D->GetOrigin() << endl;
  typedef itk::ResampleImageFilter<OutputImageType, OutputImageType, float> ResampleImageFilterType;
  ResampleImageFilterType::Pointer resample = ResampleImageFilterType::New();

  resample->SetOutputDirection( spSrcImg->GetDirection() );

  typedef itk::AffineTransform< float, 3 >  TransformType;
  TransformType::Pointer transform = TransformType::New();

  typedef itk::NearestNeighborInterpolateImageFunction<OutputImageType, float >  InterpolatorType;
  InterpolatorType::Pointer interpolator = InterpolatorType::New();
  resample->SetInterpolator( interpolator );

  resample->SetDefaultPixelValue( 50 );

  OutputImageType::SizeType inputSize = spSrcImg->GetLargestPossibleRegion().GetSize();
  OutputImageType::SizeType outputSize;
  outputSize[0] = qRound(inputSize[0] * resFactor);
  outputSize[1] = qRound(inputSize[1] * resFactor);
  outputSize[2] = inputSize[2];
  resample->SetSize(outputSize);

  OutputImageType::SpacingType outputSpacing;
  outputSpacing[0] = spSrcImg->GetSpacing()[0] * (static_cast<double>(inputSize[0]) / static_cast<double>(outputSize[0]));
  outputSpacing[1] = spSrcImg->GetSpacing()[1] * (static_cast<double>(inputSize[1]) / static_cast<double>(outputSize[1]));
  outputSpacing[2] = spSrcImg->GetSpacing()[2];
  resample->SetOutputSpacing(outputSpacing);

  OutputImageType::PointType outputOrigin = spSrcImg->GetOrigin(); //Float image
  resample->SetOutputOrigin( outputOrigin );

  resample->SetInput(spSrcImg);
  transform->SetIdentity();
  resample->SetTransform(transform);

  resample->Update();

  //resample->GetOutput()->SetOrigin(prevOrigin);
  spTarImg = resample->GetOutput(); //is it copied? or replaced?

}

void CbctRecon::ResampleItkImage( USHORT_ImageType::Pointer& spSrcImg, USHORT_ImageType::Pointer& spTarImg, double resFactor )
{
  if (!spSrcImg)
	return;

  //  cout << "original Origin: " << spSrcImg2D->GetOrigin() << endl;
  typedef itk::ResampleImageFilter<USHORT_ImageType, USHORT_ImageType, float> ResampleImageFilterType;
  ResampleImageFilterType::Pointer resample = ResampleImageFilterType::New();

  resample->SetOutputDirection( spSrcImg->GetDirection() );

  typedef itk::AffineTransform< float, 3 >  TransformType;
  TransformType::Pointer transform = TransformType::New();

  typedef itk::NearestNeighborInterpolateImageFunction<USHORT_ImageType, float >  InterpolatorType;
  InterpolatorType::Pointer interpolator = InterpolatorType::New();
  resample->SetInterpolator( interpolator );

  resample->SetDefaultPixelValue( 50 );

  USHORT_ImageType::SizeType inputSize = spSrcImg->GetLargestPossibleRegion().GetSize();
  USHORT_ImageType::SizeType outputSize;
  outputSize[0] = qRound(inputSize[0] * resFactor);
  outputSize[1] = qRound(inputSize[1] * resFactor);
  outputSize[2] = inputSize[2];
  resample->SetSize(outputSize);

  USHORT_ImageType::SpacingType outputSpacing;
  outputSpacing[0] = spSrcImg->GetSpacing()[0] * (static_cast<double>(inputSize[0]) / static_cast<double>(outputSize[0]));
  outputSpacing[1] = spSrcImg->GetSpacing()[1] * (static_cast<double>(inputSize[1]) / static_cast<double>(outputSize[1]));
  outputSpacing[2] = spSrcImg->GetSpacing()[2];
  resample->SetOutputSpacing(outputSpacing);

  USHORT_ImageType::PointType outputOrigin = spSrcImg->GetOrigin(); //Float image
  resample->SetOutputOrigin( outputOrigin );

  resample->SetInput(spSrcImg);
  transform->SetIdentity();
  resample->SetTransform(transform);

  resample->Update();

  //resample->GetOutput()->SetOrigin(prevOrigin);
  spTarImg = resample->GetOutput(); //is it copied? or replaced?

}

void CbctRecon::ResampleItkImage2D ( OutputImageType2D::Pointer& spSrcImg2D, OutputImageType2D::Pointer& spTarImg2D, double resFactor )
{
  if (!spSrcImg2D)
  {
	cout << "ERROR! SrcImage is empty" << endl;
	return;
  }

//  cout << "original Origin: " << spSrcImg2D->GetOrigin() << endl;
  typedef itk::ResampleImageFilter<OutputImageType2D, OutputImageType2D, float> ResampleImageFilterType;
  ResampleImageFilterType::Pointer resample = ResampleImageFilterType::New();

  resample->SetOutputDirection( spSrcImg2D->GetDirection() );

  //outputSpacing[2] = spSrcImg2D->GetSpacing()[2];

  //cout << "Output spacing: " << outputSpacing << endl;

  //OutputImageType2D::Pointer input = spSrcImg2D;
  //OutputImageType2D::PointType prevOrigin = input->GetOrigin(); //-204.6 - 204.6  0

  //input->SetOrigin(outputOrigin);

 /* cout << "outputSize " << outputSize << endl;
  cout << "OutputSpacing " << outputSpacing << endl;
  cout << "OutputOrigin " << outputOrigin << endl;*/

  // Resample the image
  //typedef itk::IdentityTransform<float, 2> TransformType;

  typedef itk::AffineTransform< float, 2 >  TransformType;
  TransformType::Pointer transform = TransformType::New();

  typedef itk::NearestNeighborInterpolateImageFunction<OutputImageType2D, float >  InterpolatorType;
  InterpolatorType::Pointer interpolator = InterpolatorType::New();
  resample->SetInterpolator( interpolator );


  resample->SetDefaultPixelValue( 50 );

  OutputImageType2D::SizeType inputSize = spSrcImg2D->GetLargestPossibleRegion().GetSize();
  OutputImageType2D::SizeType outputSize;
  outputSize[0] = qRound(inputSize[0] * resFactor);
  outputSize[1] = qRound(inputSize[1] * resFactor);
  resample->SetSize(outputSize);

  OutputImageType2D::SpacingType outputSpacing;
  outputSpacing[0] = spSrcImg2D->GetSpacing()[0] * (static_cast<double>(inputSize[0]) / static_cast<double>(outputSize[0]));
  outputSpacing[1] = spSrcImg2D->GetSpacing()[1] * (static_cast<double>(inputSize[1]) / static_cast<double>(outputSize[1]));
  resample->SetOutputSpacing(outputSpacing);

  OutputImageType2D::PointType outputOrigin = spSrcImg2D->GetOrigin(); //Float image
  resample->SetOutputOrigin( outputOrigin );


  resample->SetInput(spSrcImg2D);
  transform->SetIdentity();
  resample->SetTransform(transform);

  resample->Update();

  //resample->GetOutput()->SetOrigin(prevOrigin);
  spTarImg2D = resample->GetOutput(); //is it copied? or replaced?

 // cout << "resampled Origin: " << spTarImg2D->GetOrigin() << endl;
}

void CbctRecon::AfterScatCorrectionMacro()
{
  //Original projection file can be replaced by the corrected one
  //Current projection map (float) used for the reconstruction is: m_spProjImg3DFloat and this is resampled one
  ConvertIntensity2LineInt(m_spProjImgCorr3D, m_spProjImg3DFloat, 65535);

  int iSizeZ = m_spProjImg3DFloat->GetRequestedRegion().GetSize()[2];

  //Update UI
  ui.pushButton_DoRecon->setEnabled(true);
  ui.spinBoxImgIdx->setMinimum(0);
  ui.spinBoxImgIdx->setMaximum(iSizeZ-1);
  ui.spinBoxImgIdx->setValue(0);
  SetMaxAndMinValueOfProjectionImage(); //update min max projection image
  SLT_InitializeGraphLim();
  SLT_DrawProjImages();  //Update Table is called

  //return;

  //Do reconstruction + //Update GUI


  //Regardeless of previous setting, The Truncation should not be applied!

  //Truncation is invalidated inside the function
  if (ui.radioButton_UseCUDA->isChecked())
  {
	  CudaDoReconstructionFDK(REGISTER_COR_CBCT);
  }
  else{
	  DoReconstructionFDK(REGISTER_COR_CBCT);
  }
  
  //Skin removal (using CT contour w/ big margin)

  cout << "Post  FDK reconstruction is done. Moving on to post skin removal" << endl;


  m_pDlgRegistration->PostSkinRemovingCBCT(m_spRawReconImg);
  m_pDlgRegistration->PostSkinRemovingCBCT(m_spScatCorrReconImg);
  //cout << "skin removal is done" << endl;

  m_pDlgRegistration->UpdateListOfComboBox(0);//combo selection signalis called
  m_pDlgRegistration->UpdateListOfComboBox(1);
  m_pDlgRegistration->SelectComboExternal(0, REGISTER_RAW_CBCT); // will call fixedImageSelected
  m_pDlgRegistration->SelectComboExternal(1, REGISTER_COR_CBCT );

  UpdateReconImage(m_spScatCorrReconImg, QString("Scatter corrected CBCT")); //main GUI update

  //Save Image as DICOM

  if (ui.checkBox_ExportVolDICOM->isChecked())
  {
      //Get current folder
      QString strCrntDir = m_strPathPatientDir + "/" + "IMAGES" + "/" + "cor_" + m_strDCMUID; //current Proj folder
      QDir crntDir(strCrntDir);
      QString SubDirName = "Reconstruction";
      bool tmpResult = crntDir.mkdir(SubDirName); //what if the directory exists?
      if (!tmpResult)
      {
          cout << "DICOM dir seems to exist already. Files will be overwritten." << endl;
      }
      QString strSavingFolder = strCrntDir + "/" + SubDirName;
      SaveUSHORTAsSHORT_DICOM(m_spScatCorrReconImg, m_strDCMUID, QString("PriorCT_ScatterCorr"), strSavingFolder);
      //Export as DICOM (using plastimatch) folder?
  }
}

//called whenver recon 3D image for display changes.
void CbctRecon::UpdateReconImage(USHORT_ImageType::Pointer& spNewImg, QString& fileName)
{
	m_spCrntReconImg = spNewImg;
	ui.lineEdit_Cur3DFileName->setText(fileName);

	USHORT_ImageType::SizeType size = m_spCrntReconImg->GetRequestedRegion().GetSize();

	m_dspYKReconImage->CreateImage(size[0], size[1],0);

	ui.spinBoxReconImgSliceNo->setMinimum(0);
	ui.spinBoxReconImgSliceNo->setMaximum(size[2]-1);

	int initVal = qRound((size[2]-1)/2.0);
	//SLT_DrawReconImage(); //Update Table, Update Graph

	//m_dspYKReconImage->CreateImage(size_trans[0], size_trans[1],0);
	SLT_InitializeGraphLim();
	ui.spinBoxReconImgSliceNo->setValue(initVal); //DrawRecon Imge is called, but sometimes skipped
	ui.radioButton_graph_recon->setChecked(true);

	SLT_DrawReconImage();
}


void CbctRecon::SLT_TempAudit()
{
  if (m_spRawReconImg)
	cout << "m_spRawReconImg " << m_spRawReconImg << endl;

  if (m_spRefCTImg)
	cout << "m_spRefCTImg " << m_spRefCTImg << endl;

  if (m_spCrntReconImg)
	cout << "m_spCrntReconImg " << m_spCrntReconImg << endl;


}

void CbctRecon::SaveUSHORTAsSHORT_DICOM( USHORT_ImageType::Pointer& spImg, QString& strPatientID, QString& strPatientName, QString& strPathTargetDir)
{
  if (!spImg)
	return;

  SHORT_ImageType::Pointer spShortImg;
  ConvertUshort2Short(spImg, spShortImg);
  typedef itk::CastImageFilter <SHORT_ImageType, itk::Image< SHORT_PixelType, 3 > > CastFilterType;
  CastFilterType::Pointer castFilter = CastFilterType::New();
  castFilter->SetInput(spShortImg);
  castFilter->Update();
  Plm_image plm_img(castFilter->GetOutput());

  QString endFix = "_DCM";

  QString newDirPath = strPathTargetDir + "/" + strPatientID + "_DCM";


  QDir dirNew(newDirPath);
  if (!dirNew.exists()){
	dirNew.mkdir(".");
  }

  Rt_study_metadata rsm;
  rsm.set_patient_id(strPatientID.toLocal8Bit().constData());
  rsm.set_patient_name(strPatientName.toLocal8Bit().constData());

  plm_img.save_short_dicom(newDirPath.toLocal8Bit().constData(), &rsm);
}


void CbctRecon::ConvertUshort2Short(USHORT_ImageType::Pointer& spImgUshort, SHORT_ImageType::Pointer& spImgShort)
{
	//cout << "Before filter spImgUshort" << spImgUshort << endl;

	typedef itk::ThresholdImageFilter <USHORT_ImageType> ThresholdImageFilterType;
	ThresholdImageFilterType::Pointer thresholdFilter = ThresholdImageFilterType::New();
	thresholdFilter->SetInput(spImgUshort);
	thresholdFilter->ThresholdOutside(0, 4096); //--> 0 ~ 4095
	thresholdFilter->SetOutsideValue(0);
	thresholdFilter->Update();

	typedef itk::MinimumMaximumImageCalculator <USHORT_ImageType> ImageCalculatorFilterType;
	ImageCalculatorFilterType::Pointer imageCalculatorFilter = ImageCalculatorFilterType::New ();
	imageCalculatorFilter->SetImage(thresholdFilter->GetOutput());
	imageCalculatorFilter->Compute();
	double minVal= (double)(imageCalculatorFilter->GetMinimum());
	double maxVal= (double)(imageCalculatorFilter->GetMaximum());
	//cout <<"Min and Max Values are	" << minVal << "	" << maxVal << endl; //should be 0 and 4096
	//cout <<"Min and Max Values are	" << minVal << "	" << maxVal << endl;

	//Min value is always 3024 --> outside the FOV
	SHORT_PixelType outputMinVal = (SHORT_PixelType)(minVal-1024);
	SHORT_PixelType outputMaxVal = (SHORT_PixelType)(maxVal-1024);

	//USHORT_PixelType outputMinVal = (USHORT_PixelType)(minVal - minVal);
	//USHORT_PixelType outputMaxVal = (USHORT_PixelType) (maxVal - minVal);

	typedef itk::RescaleIntensityImageFilter<USHORT_ImageType, SHORT_ImageType> RescaleFilterType;
	RescaleFilterType::Pointer spRescaleFilter = RescaleFilterType::New();
	spRescaleFilter->SetInput(thresholdFilter->GetOutput());
	spRescaleFilter->SetOutputMinimum(outputMinVal);
	spRescaleFilter->SetOutputMaximum(outputMaxVal);
	spRescaleFilter->Update();

	spImgShort = spRescaleFilter->GetOutput();

	//cout << "After filter spImgUshort" << spImgUshort << endl;
	//cout << "After filter spImgShort" << spImgShort << endl;
}

void CbctRecon::SLT_LoadPlanCT_USHORT()
{
  //typedef itk::ImageFileWriter<OutputImageType> WriterType;
  typedef itk::ImageFileReader<USHORT_ImageType> ReaderType;
  ReaderType::Pointer reader = ReaderType::New();

  QString fileName = QFileDialog::getOpenFileName(this, "Open Image",m_strPathDirDefault, "Projection file (*.mha)",0,0);

  if (fileName.length() < 1)
	return;

  reader->SetFileName(fileName.toLocal8Bit().constData());
  reader->Update();

  m_spRefCTImg = reader->GetOutput();

  UpdateReconImage(m_spRefCTImg, QString("RefCT"));

  RegisterImgDuplication(REGISTER_REF_CT, REGISTER_MANUAL_RIGID);
}

void CbctRecon::SLT_CalcAndSaveAngularWEPL()//single point
{
  vector<WEPLData> vOutputWEPL;

  double fAngleGap = ui.lineEdit_WEPL_AngRes->text().toDouble();
  double fAngleStart = ui.lineEdit_AngStart->text().toDouble();
  double fAngleEnd = ui.lineEdit_AngEnd->text().toDouble();


  VEC3D curPOI;
  curPOI.x = ui.lineEdit_ForcedProbePosX->text().toDouble(); //in mm
  curPOI.y = ui.lineEdit_ForcedProbePosY->text().toDouble();
  curPOI.z = ui.lineEdit_ForcedProbePosZ->text().toDouble();

  GetAngularWEPL_SinglePoint(m_spCrntReconImg, fAngleGap, fAngleStart, fAngleEnd, curPOI, 0, vOutputWEPL, true);
  cout << "Computed WEPL points: " << vOutputWEPL.size() << endl;

  //export arrWEPL
   QString filePath = QFileDialog::getSaveFileName(this, "Save data", "", "txt image file (*.txt)",0,0); //Filename don't need to exist

   if (filePath.length() < 1)
	 return;

   ofstream fout;
   fout.open (filePath.toLocal8Bit().constData());

   fout << "Angle" << "	" << "WEPL(mm)" << endl;

   for (int i = 0; i < vOutputWEPL.size(); i++)
   {
	   fout << vOutputWEPL.at(i).fGanAngle << "	" << vOutputWEPL.at(i).fWEPL << endl;
   }

   fout.close();
   cout << "Saving angular WEPL is completed" << endl;
}

//This function deals with the current projection image converted by ElektaProjReader (intensity to lineintegral is already done. now the type is float, 3D)
void CbctRecon::SLT_DoScatterCorrectionUniform()
{
  if (!m_spProjImg3DFloat)
	return;

  USHORT_ImageType::Pointer spIntensityRaw;
  ConvertLineInt2Intensity( m_spProjImg3DFloat, spIntensityRaw,65535);

  typedef rtk::BoellaardScatterCorrectionImageFilter<USHORT_ImageType, USHORT_ImageType> ScatterFilterType;

  ScatterFilterType::Pointer spScatFilter = ScatterFilterType::New();

  double airThre = ui.lineEdit_uniAirThre->text().toDouble();
  double scat2PrimRatio = ui.lineEdit_uniSPR->text().toDouble();
  double nonNagativity = ui.lineEdit_uniNegativity->text().toDouble();

  cout << "Boallaard uniform scatter correction is being applied" << endl;
  cout << "Air threshold: " << airThre << endl;
  cout << "Scatter to Primary ratio(will be automatically adjusted by the minimum intensity): " << scat2PrimRatio << endl;
  cout << "NonNegativityConstraintThreshold: " << nonNagativity << endl;

  spScatFilter->SetInput(spIntensityRaw);
  spScatFilter->SetAirThreshold(airThre);
  spScatFilter->SetScatterToPrimaryRatio(scat2PrimRatio);
  spScatFilter->SetNonNegativityConstraintThreshold(nonNagativity);

  spScatFilter->Update(); //errir occured

  USHORT_ImageType::Pointer spIntensityUniformCorr = spScatFilter->GetOutput();

  ConvertIntensity2LineInt(spIntensityUniformCorr, m_spProjImg3DFloat, 65535);


  //ConvertLineInt2Intensity(m_spProjImg3DFloat, m_spProjImgRaw3D, 65535);

  m_spProjImgRaw3D = spIntensityUniformCorr;



  ui.spinBoxImgIdx->setValue(0);
  SetMaxAndMinValueOfProjectionImage(); //update min max projection image
  SLT_InitializeGraphLim();

  this->SLT_DrawProjImages();  //Update Table is called

  cout << "FINISHED!: Uniform Scatter correction for raw projection images (Boallaard method) is completed. Proceed to reconstruction" << endl;
}

void CbctRecon::SLT_FileExportShortDICOM_CurrentImg()
{

  if (!m_spCrntReconImg)
	return;

  QString dirPath = QFileDialog::getExistingDirectory(this, tr("Open Directory"),
	m_strPathDirDefault,QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

  if (dirPath.isEmpty())
	return;

  //Get current folder
  QDir crntDir(dirPath);

  QInputDialog inputDlg;

  bool ok;
  QString text = QInputDialog::getText(this, "Input Dialog","set End Fix for Patient UID", QLineEdit::Normal, "PatientID_EndFix", &ok);

  QString strEndFix = "YKP";
  QString strPatientID;

  if (ok && !text.isEmpty())
  {
	strEndFix = text;
	strPatientID = m_strDCMUID + "_" + strEndFix;
  }
  else
  {
	strPatientID = m_strDCMUID;
  }

  if (strPatientID.isEmpty())
	return;

  bool tmpResult = crntDir.mkdir(strPatientID); //what if the directory exists?
  if (!tmpResult)
  {
	cout << "DICOM dir seems to exist already. Files will be overwritten." << endl;
  }

  QString strSavingFolder = dirPath + "/" + strPatientID;
  SaveUSHORTAsSHORT_DICOM(m_spCrntReconImg, strPatientID, strEndFix, strSavingFolder);
}

void CbctRecon::SLT_AddConstHUToCurImg()
{
  if (!m_spCrntReconImg)
	return;
  int addingVal = ui.lineEdit_AddConstHU->text().toInt();
  AddConstHU(m_spCrntReconImg, addingVal);
  UpdateReconImage(m_spCrntReconImg, QString("Added%1").arg(addingVal));
}

double CbctRecon::GetRawIntensityScaleFactor()
{
  //GetRawIntensity Scale Factor
  double rawIntensityScaleF = 1.0;

  double fRef_mAs =0.0;
  double fCur_mAs=0.0;
  QString strRef_mAs = ui.lineEdit_RefmAs->text();
  QStringList listmAsRef = strRef_mAs.split(",");
  if (listmAsRef.length() == 2)
  {
	fRef_mAs = listmAsRef.at(0).toDouble() * listmAsRef.at(1).toDouble();
  }
  QString strCur_mAs = ui.lineEdit_CurmAs->text();
  QStringList listmAsCur = strCur_mAs.split(",");
  if (listmAsCur.length() == 2)
  {
	fCur_mAs = listmAsCur.at(0).toDouble() * listmAsCur.at(1).toDouble();
  }
  if (fRef_mAs*fCur_mAs != 0)
  {
	rawIntensityScaleF = fRef_mAs / fCur_mAs;
  }

  return rawIntensityScaleF;
  //if 64 40 ref, 40 40 cur --> scaleF = 1.6
  //raw intensity X scaleF ==> raw intensity increased --> this avoids negative scatter map

}

void CbctRecon::SLT_SetCBCTSkinRSPath()
{
  QString strPath = QFileDialog::getOpenFileName(this, "Open RS file",m_strPathDirDefault, "DICOM RS (*.dcm)",0,0);

  if (strPath.length()<=1)
	return;

  ui.lineEdit_PathCBCTSkinPath->setText(strPath);

}

void CbctRecon::SLT_CropSkinUsingThreshold()
{
	cout << "Overwriting of values below threshold to air ";
	if (!m_spCrntReconImg) {
		return;
	}

	USHORT_ImageType::SizeType dims; 
	dims = m_spCrntReconImg->GetBufferedRegion().GetSize();
	cout << "initiating.. dim" << dims << endl;
	typedef itk::BinaryThresholdImageFilter <USHORT_ImageType, USHORT_ImageType> threshFilterType;
	threshFilterType::Pointer threshFilter = threshFilterType::New();
	cout << "Adding input.. ";
	threshFilter->SetInput( m_spCrntReconImg );
	/*
	if (m_spCrntReconImg == m_spRawReconImg)
	{
		threshFilter->SetInput(m_spRawReconImg);
	}
	else if (m_spCrntReconImg == m_spRefCTImg)
	{
		threshFilter->SetInput(m_spRefCTImg);
	}
	else if (m_spCrntReconImg == m_spScatCorrReconImg)
	{
		threshFilter->SetInput(m_spScatCorrReconImg);
	}
	*/
	cout << "input added.. "; //

	threshFilter->SetOutsideValue( 0 );
	threshFilter->SetInsideValue( 1 );
	threshFilter->SetLowerThreshold( ui.lineEdit_Threshold->text().toInt() );
	threshFilter->Update();
	USHORT_ImageType::Pointer spCrntImgMask = threshFilter->GetOutput();
	typedef itk::ImageRegionIteratorWithIndex<USHORT_ImageType> iteratorType;
	iteratorType it(spCrntImgMask, spCrntImgMask->GetRequestedRegion());
	USHORT_ImageType::SizeType imgDims = spCrntImgMask->GetBufferedRegion().GetSize();
	double crntVal = 0.0;
	int z_idx;
	it.GoToBegin();
	while (!it.IsAtEnd())
	{
		crntVal = (double)(it.Get()); 
		z_idx = it.GetIndex()[2]; 
		if (z_idx == imgDims[2] - 10 || z_idx == 10)
		{
			it.Set((float)(1));
		}
		++it;
	}
	
	typedef itk::BinaryFillholeImageFilter <USHORT_ImageType> HoleFillingFilterType;
	HoleFillingFilterType::Pointer HoleFillingFilter = HoleFillingFilterType::New();
	HoleFillingFilter->SetForegroundValue( 1 );
	HoleFillingFilter->SetFullyConnected( false );
	cout << "Threshold filtering.. ";
	HoleFillingFilter->SetInput( spCrntImgMask );
	dims = threshFilter->GetOutput()->GetBufferedRegion().GetSize();
	cout << "dim" << dims << endl;


	typedef itk::BinaryBallStructuringElement< USHORT_PixelType, 3> StructElementType;
	typedef itk::BinaryErodeImageFilter< USHORT_ImageType, USHORT_ImageType, StructElementType > ErodeFilterType;
	typedef itk::BinaryDilateImageFilter< USHORT_ImageType, USHORT_ImageType, StructElementType > DilateFilterType;
	ErodeFilterType::Pointer binaryErode = ErodeFilterType::New();
	binaryErode->SetErodeValue( 1 );
	StructElementType erodeStructElement;
	erodeStructElement.SetRadius( ui.lineEdit_ErodeRadius->text().toInt() );
	erodeStructElement.CreateStructuringElement();
	binaryErode->SetKernel( erodeStructElement );
	cout << "filling holes.. ";
	binaryErode->SetInput(HoleFillingFilter->GetOutput());
	dims = HoleFillingFilter->GetOutput()->GetBufferedRegion().GetSize();
	cout << "dim" << dims << endl;

	DilateFilterType::Pointer binaryDilate = DilateFilterType::New();
	binaryDilate->SetDilateValue( 1 );
	StructElementType dilateStructElement;
	dilateStructElement.SetRadius( ui.lineEdit_DilateRadius->text().toInt() );
	dilateStructElement.CreateStructuringElement();
	binaryDilate->SetKernel(dilateStructElement);
	cout << "eroding dirt.. ";
	binaryDilate->SetInput(binaryErode->GetOutput());
	dims = binaryErode->GetOutput()->GetBufferedRegion().GetSize();
	cout << "dim" << dims << endl;
	cout << "Skin mask is being created..." << endl;
	//binaryDilate->Update();

	typedef itk::MaskImageFilter <USHORT_ImageType, USHORT_ImageType, USHORT_ImageType> MaskFilterType;
	MaskFilterType::Pointer MaskFilter = MaskFilterType::New();
	MaskFilter->SetMaskingValue(0);
	cout << "Dilating.. ";
	MaskFilter->SetMaskImage(binaryDilate->GetOutput());
	dims = binaryDilate->GetOutput()->GetBufferedRegion().GetSize();
	cout << "dim" << dims << endl;

	if (m_spCrntReconImg == m_spRawReconImg)
	{
		MaskFilter->SetInput( m_spRawReconImg );
		MaskFilter->Update();
		m_spRawReconImg = MaskFilter->GetOutput();
		dims = MaskFilter->GetOutput()->GetBufferedRegion().GetSize();
		cout << "dim" << dims << endl;
		UpdateReconImage(m_spRawReconImg, QString("Thresh-based skin cropped image"));
	}
	else if (m_spCrntReconImg == m_spRefCTImg)
	{
		MaskFilter->SetInput(m_spRefCTImg);
		cout << "Update mask filter.. ";
		MaskFilter->Update();
		cout << "And return output." << endl;
		m_spRefCTImg = MaskFilter->GetOutput();
		dims = MaskFilter->GetOutput()->GetBufferedRegion().GetSize();
		cout << "dim" << dims << endl;
		UpdateReconImage(m_spRefCTImg, QString("Thresh-based skin cropped image"));
	}
	else if (m_spCrntReconImg == m_spScatCorrReconImg)
	{
		MaskFilter->SetInput(m_spScatCorrReconImg);
		MaskFilter->Update();
		m_spScatCorrReconImg = MaskFilter->GetOutput();
		UpdateReconImage(m_spScatCorrReconImg, QString("Thresh-based skin cropped image"));
	}
}

void CbctRecon::SLT_CropSkinUsingRS() // CHANGE HERE
{
  QString strPathRS =  ui.lineEdit_PathCBCTSkinPath->text();
  if (strPathRS.length() < 1)
	return;

  double croppingMargin = ui.lineEdit_SkinMargin->text().toDouble();

  if (m_spCrntReconImg == m_spRawReconImg)
  {
	m_pDlgRegistration->CropSkinUsingRS(m_spRawReconImg, strPathRS, croppingMargin);
	UpdateReconImage(m_spRawReconImg,QString("RS-based skin cropped image"));
  }
  else if (m_spCrntReconImg == m_spRefCTImg)
  {
	m_pDlgRegistration->CropSkinUsingRS(m_spRefCTImg, strPathRS, croppingMargin);
	UpdateReconImage(m_spRefCTImg,QString("RS-based skin cropped image"));
  }
  else if (m_spCrntReconImg == m_spScatCorrReconImg)
  {
	m_pDlgRegistration->CropSkinUsingRS(m_spScatCorrReconImg, strPathRS, croppingMargin);
	UpdateReconImage(m_spScatCorrReconImg,QString("RS-based skin cropped image"));
  }
}

void CbctRecon::ExportAngularWEPL_byFile(QString& strPathOutput)
{
    if (strPathOutput.length() < 1)
        return;

    double fAngleGap = ui.lineEdit_WEPL_AngRes->text().toDouble();
	double fAngleStart = ui.lineEdit_AngStart->text().toDouble();
	double fAngleEnd = ui.lineEdit_AngEnd->text().toDouble();

    if (m_vPOI_DCM.empty())
    {
        cout << "No POI data is prepared. Load them first" << endl;
        return;
    }

    if (!m_spRawReconImg)
    {
        cout << "Error: no Raw Recon image is found" << endl;
        return;
    }

    if (!m_spScatCorrReconImg)
    {
        cout << "Warning: no ScatCorrReconImg is found" << endl;
    }

    if (!m_spManualRigidCT)
    {
        cout << "Warning: no ManualRigidCT is found" << endl;
    }
    if (!m_spAutoRigidCT)
    {
        cout << "Warning: no AutoRigidCT is found" << endl;
    }
    if (!m_spDeformedCT_Final)
    {
        cout << "Warning: no DeformedCT is found" << endl;
    }
    int sizePOI = m_vPOI_DCM.size();

    vector<WEPLData> vOutputWEPL_manual;
    vector<WEPLData> vOutputWEPL_auto_rigid;
    vector<WEPLData> vOutputWEPL_deform;
    vector<WEPLData> vOutputWEPL_rawCBCT;
    vector<WEPLData> vOutputWEPL_corCBCT;

	cout << "Everything is OK!? Let's calculate WEPL...\n" << endl;

    for (int i = 0; i < sizePOI; i++)
    {
        VEC3D curPOI = m_vPOI_DCM.at(i);
        //GetAngularWEPL_SinglePoint(m_spCrntReconImg, fAngleGap, curPOI, i, vOutputWEPL, true);
        //if (m_spRawReconImg)
		GetAngularWEPL_SinglePoint(m_spRawReconImg, fAngleGap, fAngleStart, fAngleEnd, curPOI, i, vOutputWEPL_rawCBCT, true);//mandatory
        if (m_spScatCorrReconImg)
			GetAngularWEPL_SinglePoint(m_spScatCorrReconImg, fAngleGap, fAngleStart, fAngleEnd, curPOI, i, vOutputWEPL_corCBCT, true);
        if (m_spManualRigidCT)
			GetAngularWEPL_SinglePoint(m_spManualRigidCT, fAngleGap, fAngleStart, fAngleEnd, curPOI, i, vOutputWEPL_manual, true);
        if (m_spAutoRigidCT)
			GetAngularWEPL_SinglePoint(m_spAutoRigidCT, fAngleGap, fAngleStart, fAngleEnd, curPOI, i, vOutputWEPL_auto_rigid, true);
        if (m_spDeformedCT_Final)
			GetAngularWEPL_SinglePoint(m_spDeformedCT_Final, fAngleGap, fAngleStart, fAngleEnd, curPOI, i, vOutputWEPL_deform, true);
		cout << "OK, point: " << i << " succeeded. Next..\n" << endl;
    }
    //cout << "Computed WEPL points: " << vOutputWEPL.size() << endl;
	cout << "OK calculation is done, time to save the results...\n" << endl;

    ofstream fout;
    fout.open(strPathOutput.toLocal8Bit().constData());

//    double curAngle2;

    //fout << "POI_Index" << "," << "Gantry_Angle" << "," << "WEPL(mm)" << endl;

    int cntWEPL = vOutputWEPL_rawCBCT.size();

    fout << "Point Index" << "\t" << "Gantry Angle" << "\t" << "Sample Number" << "\t" << "RawCBCT" << "\t";

    if (m_spScatCorrReconImg && vOutputWEPL_corCBCT.size() == cntWEPL)
        fout << "CorrCBCT" << "\t";
    if (m_spManualRigidCT && vOutputWEPL_manual.size() == cntWEPL)
        fout << "ManualRigidCT" << "\t";
    if (m_spAutoRigidCT && vOutputWEPL_auto_rigid.size() == cntWEPL)
        fout << "AutoRigidCT" << "\t";
    if (m_spDeformedCT_Final && vOutputWEPL_deform.size() == cntWEPL)
        fout << "DeformedCT" << "\t";
    fout << endl;

    for (int i = 0; i < cntWEPL; i++)
    {
        fout << vOutputWEPL_rawCBCT.at(i).ptIndex << "\t" << vOutputWEPL_rawCBCT.at(i).fGanAngle << "\t" << i << "\t" << vOutputWEPL_rawCBCT.at(i).fWEPL << "\t";

        if (m_spScatCorrReconImg && vOutputWEPL_corCBCT.size() == cntWEPL)
            fout << vOutputWEPL_corCBCT.at(i).fWEPL << "\t";
        if (m_spManualRigidCT && vOutputWEPL_manual.size() == cntWEPL)
            fout << vOutputWEPL_manual.at(i).fWEPL << "\t";
        if (m_spAutoRigidCT && vOutputWEPL_auto_rigid.size() == cntWEPL)
            fout << vOutputWEPL_auto_rigid.at(i).fWEPL << "\t";
        if (m_spDeformedCT_Final && vOutputWEPL_deform.size() == cntWEPL)
            fout << vOutputWEPL_deform.at(i).fWEPL << "\t";

        fout << endl;
    }
    fout.close();
    cout << "Saving angular WEPL is completed\n" << endl;

}


void CbctRecon::OptimizedExportAngularWEPL_byFile(QString& strPathOutput)
{
	if (strPathOutput.length() < 1)
		return;

	double fAngleGap = ui.lineEdit_WEPL_AngRes->text().toDouble();
	double fAngleStart = ui.lineEdit_AngStart->text().toDouble();
	double fAngleEnd = ui.lineEdit_AngEnd->text().toDouble();

	if (m_vPOI_DCM.empty())
	{
		cout << "No POI data is prepared. Load them first" << endl;
		return;
	}

	if (!m_spRawReconImg)
	{
		cout << "Error: no Raw Recon image is found" << endl;
		return;
	}

	if (!m_spScatCorrReconImg)
	{
		cout << "Warning: no ScatCorrReconImg is found" << endl;
	}

	if (!m_spManualRigidCT)
	{
		cout << "Warning: no ManualRigidCT is found" << endl;
	}
	if (!m_spAutoRigidCT)
	{
		cout << "Warning: no AutoRigidCT is found" << endl;
	}
	if (!m_spDeformedCT_Final)
	{
		cout << "Warning: no DeformedCT is found" << endl;
	}
	int sizePOI = m_vPOI_DCM.size();

	vector<WEPLData> vOutputWEPL_manual;
	vector<WEPLData> vOutputWEPL_auto_rigid;
	vector<WEPLData> vOutputWEPL_deform;
	vector<WEPLData> vOutputWEPL_rawCBCT;
	vector<WEPLData> vOutputWEPL_corCBCT;

	cout << "Everything is OK!? Let's calculate WEPL...\n" << endl;

	//for (int i = 0; i < sizePOI; i++)
	//{
	//	VEC3D curPOI = m_vPOI_DCM.at(i);
		//GetAngularWEPL_SinglePoint(m_spCrntReconImg, fAngleGap, curPOI, i, vOutputWEPL, true);
		//if (m_spRawReconImg)
	GetAngularWEPL_MultiPoint(m_spRawReconImg, fAngleGap, fAngleStart, fAngleEnd, vOutputWEPL_rawCBCT, true);//mandatory
	cout << "Done: (RAW)";
	if (m_spScatCorrReconImg)
	{
		try{
			GetAngularWEPL_MultiPoint(m_spScatCorrReconImg, fAngleGap, fAngleStart, fAngleEnd, vOutputWEPL_corCBCT, true);
			cout << " (COR)";
		}
		catch (exception e){
			cout << " (COR) failed!!: e=" << e.what() << endl;
			vOutputWEPL_corCBCT.~vector();
		}
	}
	if (m_spManualRigidCT)
	{
		try{
			GetAngularWEPL_MultiPoint(m_spManualRigidCT, fAngleGap, fAngleStart, fAngleEnd, vOutputWEPL_manual, true);
			cout << " (MAN)";
		}
		catch(exception e){
			cout << " (MAN) failed!!: e=" << e.what() << endl;
			vOutputWEPL_manual.~vector();
		}
	}
	if (m_spAutoRigidCT)
	{
		try {
			GetAngularWEPL_MultiPoint(m_spAutoRigidCT, fAngleGap, fAngleStart, fAngleEnd, vOutputWEPL_auto_rigid, true);
			cout << " (AUT)";
		}
		catch (exception e){
			cout << " (AUT) failed!!: e=" << e.what() << endl;
			vOutputWEPL_auto_rigid.~vector();
		}
	}
	if (m_spDeformedCT_Final)
	{
		try{
			GetAngularWEPL_MultiPoint(m_spDeformedCT_Final, fAngleGap, fAngleStart, fAngleEnd, vOutputWEPL_deform, true);
			cout << " (DEF)";
		}
		catch (exception e){
			cout << " (DEF) failed!!: e=" << e.what() << endl;
			vOutputWEPL_deform.~vector();
		}
	}
	cout << endl;
	
	cout << "Saving results...";

	ofstream fout;
	fout.open(strPathOutput.toLocal8Bit().constData());

	int cntWEPL = vOutputWEPL_rawCBCT.size();

	fout << "Point Index" << "\t" << "Gantry Angle" << "\t" << "Sample Number" << "\t" << "RawCBCT" << "\t";

	if (m_spScatCorrReconImg && vOutputWEPL_corCBCT.size() == cntWEPL)
		fout << "CorrCBCT" << "\t";
	if (m_spManualRigidCT && vOutputWEPL_manual.size() == cntWEPL)
		fout << "ManualRigidCT" << "\t";
	if (m_spAutoRigidCT && vOutputWEPL_auto_rigid.size() == cntWEPL)
		fout << "AutoRigidCT" << "\t";
	if (m_spDeformedCT_Final && vOutputWEPL_deform.size() == cntWEPL)
		fout << "DeformedCT" << "\t";
	fout << endl;

	for (int i = 0; i < cntWEPL; i++)
	{
		fout << vOutputWEPL_rawCBCT.at(i).ptIndex << "\t" << vOutputWEPL_rawCBCT.at(i).fGanAngle << "\t" << i << "\t" << vOutputWEPL_rawCBCT.at(i).fWEPL << "\t";

		if (m_spScatCorrReconImg && vOutputWEPL_corCBCT.size() == cntWEPL)
			fout << vOutputWEPL_corCBCT.at(i).fWEPL << "\t";
		if (m_spManualRigidCT && vOutputWEPL_manual.size() == cntWEPL)
			fout << vOutputWEPL_manual.at(i).fWEPL << "\t";
		if (m_spAutoRigidCT && vOutputWEPL_auto_rigid.size() == cntWEPL)
			fout << vOutputWEPL_auto_rigid.at(i).fWEPL << "\t";
		if (m_spDeformedCT_Final && vOutputWEPL_deform.size() == cntWEPL)
			fout << vOutputWEPL_deform.at(i).fWEPL << "\t";

		fout << endl;
	}
	fout.close();
	cout << "done!" << endl;

}


void CbctRecon::SLT_ExportAngularWEPL_byFile()
{
    //export arrWEPL
    QString filePath = QFileDialog::getSaveFileName(this, "Save data", m_strPathDirDefault, "txt image file (*.txt)", 0, 0); //Filename don't need to exist

    if (filePath.length() < 1)
        return;

    ExportAngularWEPL_byFile(filePath);
}

void CbctRecon::SLT_OptExportAngularWEPL_byFile()
{
	//export arrWEPL
	QString filePath = QFileDialog::getSaveFileName(this, "Save data", m_strPathDirDefault, "txt image file (*.txt)", 0, 0); //Filename don't need to exist

	if (filePath.length() < 1)
		return;

	OptimizedExportAngularWEPL_byFile(filePath);
}

void CbctRecon::GetAngularWEPL_SinglePoint(USHORT_ImageType::Pointer& spUshortImage, float fAngleGap, float fAngleStart, float fAngleEnd, VEC3D calcPt, int curPtIdx, vector<WEPLData>& vOutputWEPLData, bool bAppend)
{
	if (!spUshortImage)
		return;

	if (fAngleGap <= 0)
		return;

  SHORT_ImageType::Pointer spShortImg;
  ConvertUshort2Short(spUshortImage, spShortImg); 

  typedef itk::CastImageFilter< SHORT_ImageType, OutputImageType > CastFilterType;
  CastFilterType::Pointer castFilter = CastFilterType::New();
  castFilter->SetInput(spShortImg);
  castFilter->Update();

  //Plm_image::Pointer ct_vol = Plm_image::New (spShortImg);
  Plm_image::Pointer ct_vol = Plm_image::New (castFilter->GetOutput()); // Oh ohh.. not actually a pointer now is it?

  double fullAngle = fAngleEnd - fAngleStart;
  int sizeAngles = qRound(fullAngle / fAngleGap);

  //1) Generate parms according to the angle e.g 360 parms
  double isoTarget[3];
  //isoTarget[0] = ui.lineEdit_ForcedProbePosX->text().toDouble(); //in mm
  //isoTarget[1] = ui.lineEdit_ForcedProbePosY->text().toDouble();
  //isoTarget[2] = ui.lineEdit_ForcedProbePosZ->text().toDouble();

  isoTarget[0] = calcPt.x;
  isoTarget[1] = calcPt.y;
  isoTarget[2] = calcPt.z;

  float srcDistance = 2200.0; //in mm, 2.2 m
  float srcProton[3] = {0.0,0.0,0.0};

  float ap_distance = 1900.0; // 300 mm from the target //offset from the source
  //float ap_distance = 1000.0; //


  float ap_spacing[2] = {1.0, 1.0};  //resolution
  int ap_dim[2] = {1, 1};
  float ap_center[2] = {1,1};

  float ray_step = 1.0;            //mm

  WEPLData* stArrWEPL = new WEPLData [sizeAngles];

  double curAngle = 0.0;

  //cout << "Definition in place, calculation for all given angles startng..." << endl;

  for (int i = (fAngleStart / fAngleGap) ; i < sizeAngles ; i++)
  {
      //YKTEMP Should be updated according to recent update of plastimatch
	curAngle = i * fAngleGap;

	srcProton[0] = isoTarget[0] + (srcDistance * sin(curAngle * M_PI / 180.0));
	srcProton[1] = isoTarget[1] - (srcDistance*cos(curAngle * M_PI / 180.0));
	srcProton[2] = isoTarget[2];

	//Rt_plan* scene = new Rt_plan; // Oh Ohhh
	Rt_plan scene;

	//scene.beam = new Rt_beam;
        Rt_beam* newBeam = scene.append_beam(); // . to ->
	scene.set_patient(ct_vol); // . to ->
        newBeam->get_aperture()->set_distance(ap_distance);
        newBeam->get_aperture()->set_distance(ap_distance);
        newBeam->get_aperture()->set_spacing(ap_spacing);
        newBeam->get_aperture()->set_dim(ap_dim);
        newBeam->get_aperture()->set_center(ap_center);


        newBeam->set_step_length(ray_step);
        newBeam->set_isocenter_position(isoTarget);
        newBeam->set_source_position(srcProton);


	//if (!scene.init()) {
        /*if (!scene.compute_plan()) {
		cout << "Error in scene initRSP" << endl;
		return;
	}*/
        scene.prepare_beam_for_calc(newBeam); // . to ->

	//wed_ct_compute in wed_main


        Rpl_volume* rpl_vol = newBeam->rpl_vol;

        //rpl_vol->compute_proj_wed_volume()

	Proj_volume *proj_vol = rpl_vol->get_proj_volume();
	//float *proj_wed_vol_img = (float*) rpl_vol->proj_wed_vol->img;

	const double *src = proj_vol->get_src();
	const double *iso = proj_vol->get_iso();
	const double sid_length = proj_vol->get_proj_matrix()->sid; //distance from source to aperture
	double src_iso_vec[3];
	vec3_sub3(src_iso_vec,src,iso);
	const double src_iso_distance = vec3_len(src_iso_vec);
	const double ap_iso_distance = src_iso_distance - sid_length;

	double base_rg_dist = ap_iso_distance - rpl_vol->get_front_clipping_plane();

	const double base_dist = proj_vol->get_proj_matrix()->sid; //distance from source to aperture

	const int *ires = proj_vol->get_image_dim();

	//int ap_ij[2]; //ray index of rvol
	plm_long ap_idx = 0;  //ray number always 0 here

	Ray_data *ray_data;
	double ray_ap[3]; //vector from src to ray intersection with ap plane
	double ray_ap_length; //length of vector from src to ray intersection with ap plane
	double rglength; //length that we insert into get_rgdepth for each ray

	ray_data = rpl_vol->get_Ray_data();

	Ray_data *ray_data_single = &ray_data[ap_idx];

	/* Coordinate of ray intersection with aperture plane */
	double *ap_xyz = ray_data_single->p2;
	vec3_sub3 (ray_ap, ap_xyz, src);
	ray_ap_length = vec3_len(ray_ap);
	rglength = base_rg_dist*(ray_ap_length/base_dist);

	int ap_idx_default[2] = {0,0};
	stArrWEPL[i].fWEPL = (float) (rpl_vol->get_rgdepth(ap_idx_default,rglength));
	stArrWEPL[i].fGanAngle = curAngle;
	stArrWEPL[i].ptIndex = curPtIdx;

        delete newBeam;
  }

  //cout << "Phew, we got through this point too.. now we clean up and get ready for the next.." << endl;

  if (!bAppend)
	vOutputWEPLData.clear();

  for (int i = 0 ; i< sizeAngles ; i++)
  {
	vOutputWEPLData.push_back(stArrWEPL[i]);
  }

  delete [] stArrWEPL;
  ct_vol->free();


  //export arrWEPL
 // QString filePath = QFileDialog::getSaveFileName(this, "Save data", "", "txt image file (*.txt)",0,0); //Filename don't need to exist

 // if (filePath.length() < 1)
	//return;

 // ofstream fout;
 // fout.open (filePath.toLocal8Bit().constData());

 // double curAngle2;

 // fout << "Angle" << "	" << "WEPL(mm)" << endl;

 // for (int i = 0 ; i < sizeAngles ; i++)
 // {
	//curAngle2 = i * fAngleGap;
	//fout << curAngle2 << "	" << arrWEPL[i] << endl;
 // }

 // delete [] arrWEPL;

 // fout.close();
 // cout << "Saving angular WEPL is completed" << endl;
}


void CbctRecon::GetAngularWEPL_MultiPoint(USHORT_ImageType::Pointer& spUshortImage, float fAngleGap, float fAngleStart, float fAngleEnd, vector<WEPLData>& vOutputWEPLData, bool bAppend)
{
	if (!spUshortImage)
		return;

	if (fAngleGap <= 0)
		return;

	SHORT_ImageType::Pointer spShortImg;
	ConvertUshort2Short(spUshortImage, spShortImg);

	typedef itk::CastImageFilter< SHORT_ImageType, OutputImageType > CastFilterType;
	CastFilterType::Pointer castFilter = CastFilterType::New();
	castFilter->SetInput(spShortImg);
	castFilter->Update();

	//Plm_image::Pointer ct_vol = Plm_image::New (spShortImg);
	Plm_image::Pointer ct_vol = Plm_image::New(castFilter->GetOutput()); // Oh ohh.. not actually a pointer now is it?

	double fullAngle = fAngleEnd - fAngleStart;
	int sizeAngles = qRound(fullAngle / fAngleGap);

	//1) Generate parms according to the angle e.g 360 parms
	double isoTarget[3];
	//isoTarget[0] = ui.lineEdit_ForcedProbePosX->text().toDouble(); //in mm
	//isoTarget[1] = ui.lineEdit_ForcedProbePosY->text().toDouble();
	//isoTarget[2] = ui.lineEdit_ForcedProbePosZ->text().toDouble();

	float srcDistance = 2200.0; //in mm, 2.2 m
	float srcProton[3] = { 0.0, 0.0, 0.0 };

	float ap_distance = 1900.0; // 300 mm from the target //offset from the source
	//float ap_distance = 1000.0; //


	float ap_spacing[2] = { 1.0, 1.0 };  //resolution
	int ap_dim[2] = { 1, 1 };
	float ap_center[2] = { 1, 1 };

	float ray_step = 1.0;            //mm

	double curAngle = 0.0;
	VEC3D curPOI;

	int sizePOI = m_vPOI_DCM.size();
	WEPLData* stArrWEPL = new WEPLData[sizePOI * sizeAngles];

	// cout << "Everything is OK!? Let's calculate WEPL...\n" << endl;
	
	for (int p = 0; p < sizePOI; p++)
	{
		curPOI = m_vPOI_DCM.at(p);
		isoTarget[0] = curPOI.x;  // Sorry, might do better later..
		isoTarget[1] = curPOI.y;
		isoTarget[2] = curPOI.z;
		//GetAngularWEPL_SinglePoint(m_spCrntReconImg, fAngleGap, curPOI, i, vOutputWEPL, true);
		//if (m_spRawReconImg)
		//GetAngularWEPL_MultiPoint(m_spRawReconImg, fAngleGap, fAngleStart, fAngleEnd, curPOI, i, vOutputWEPL_rawCBCT, true);//mandatory
		
#ifdef DEBUG
			cout << "POI idx: " << p << ", calculation for all given angles startng...\n";
#endif
		
		for (int i = qRound(fAngleStart / fAngleGap); i < (sizeAngles + qRound(fAngleStart / fAngleGap)); i++)
		{
			//YKTEMP Should be updated according to recent update of plastimatch
			curAngle = i * fAngleGap;

			srcProton[0] = isoTarget[0] + (srcDistance * sin(curAngle * M_PI / 180.0));
			srcProton[1] = isoTarget[1] - (srcDistance * cos(curAngle * M_PI / 180.0));
			srcProton[2] = isoTarget[2];

			//cout << "\nDefine scene.." << endl;
			Rt_plan scene;
			//cout << "New beam.." << endl;
			Rt_beam* newBeam = scene.append_beam(); // . to ->
			scene.set_patient(ct_vol); // . to ->
			newBeam->get_aperture()->set_distance(ap_distance);
			newBeam->get_aperture()->set_distance(ap_distance);
			newBeam->get_aperture()->set_spacing(ap_spacing);
			newBeam->get_aperture()->set_dim(ap_dim);
			newBeam->get_aperture()->set_center(ap_center);


			newBeam->set_step_length(ray_step);
			newBeam->set_isocenter_position(isoTarget);
			newBeam->set_source_position(srcProton);

			//cout << "\nPrep beam.." << endl;
			scene.prepare_beam_for_calc(newBeam);
			//wed_ct_compute in wed_main

			Rpl_volume* rpl_vol = newBeam->rpl_vol;

			//rpl_vol->compute_proj_wed_volume()

			Proj_volume *proj_vol = rpl_vol->get_proj_volume();
			//float *proj_wed_vol_img = (float*) rpl_vol->proj_wed_vol->img;
			//cout << "\nA lot of const doubles..";

			const double *src = proj_vol->get_src();
			const double *iso = proj_vol->get_iso();
			const double sid_length = proj_vol->get_proj_matrix()->sid; //distance from source to aperture
			double src_iso_vec[3];
			vec3_sub3(src_iso_vec, src, iso);
			const double src_iso_distance = vec3_len(src_iso_vec);
			const double ap_iso_distance = src_iso_distance - sid_length;
			//cout << " front clipping plane..";
			double base_rg_dist = ap_iso_distance - rpl_vol->get_front_clipping_plane();

			const double base_dist = proj_vol->get_proj_matrix()->sid; //distance from source to aperture

			const int *ires = proj_vol->get_image_dim();

			//int ap_ij[2]; //ray index of rvol
			plm_long ap_idx = 0;  //ray number always 0 here

			Ray_data *ray_data;
			double ray_ap[3]; //vector from src to ray intersection with ap plane
			double ray_ap_length; //length of vector from src to ray intersection with ap plane
			double rglength; //length that we insert into get_rgdepth for each ray
			//cout << " get ray data.. " << endl;
			ray_data = rpl_vol->get_Ray_data();

			Ray_data *ray_data_single = &ray_data[ap_idx];

			/* Coordinate of ray intersection with aperture plane */
			double *ap_xyz = ray_data_single->p2;
			vec3_sub3(ray_ap, ap_xyz, src);
			ray_ap_length = vec3_len(ray_ap);
			rglength = base_rg_dist*(ray_ap_length / base_dist);

			int ap_idx_default[2] = { 0, 0 };
			stArrWEPL[i - qRound(fAngleStart / fAngleGap) + p * sizeAngles].fWEPL = (float)(rpl_vol->get_rgdepth(ap_idx_default, rglength));
			stArrWEPL[i - qRound(fAngleStart / fAngleGap) + p * sizeAngles].fGanAngle = curAngle;
			stArrWEPL[i - qRound(fAngleStart / fAngleGap) + p * sizeAngles].ptIndex = p;

			delete newBeam;
		}
	}

	// cout << "Return results for saving.." << endl;

	if (!bAppend)
		vOutputWEPLData.clear();

	for (int p = 0; p < sizePOI; p++)
	{
		for (int i = int(fAngleStart / fAngleGap); i < sizeAngles; i++)
		{
			vOutputWEPLData.push_back(stArrWEPL[i - qRound(fAngleStart / fAngleGap) + p * sizeAngles]);
		}
	}
	
	delete [] stArrWEPL; 
	ct_vol->free(); 
}

void CbctRecon::SLT_GeneratePOIData()//it fills m_vPOI_DCM
{
	if (!m_vPOI_DCM.empty())
		m_vPOI_DCM.clear();
	VEC3D imgDims;
	imgDims.x = ui.lineEdit_outImgDim_LR->text().toInt();
	imgDims.y = ui.lineEdit_outImgDim_AP->text().toInt();
	imgDims.z = ui.lineEdit_outImgDim_SI->text().toInt();
	VEC3D imgSpac;
	imgSpac.x = ui.lineEdit_outImgSp_LR->text().toDouble();
	imgSpac.y = ui.lineEdit_outImgSp_AP->text().toDouble();
	imgSpac.z = ui.lineEdit_outImgSp_SI->text().toDouble();
	VEC3D fPOI;
	if (ui.checkBox_AP->isChecked())
	{
		for (size_t k = 2; k < (imgDims.z - 2); k++)
		{
			for (size_t i = 2; i < (imgDims.x - 2); i++)
			{
				fPOI.x = i * imgSpac.x - ((imgSpac.x * imgDims.x) / 2.);
				fPOI.y = ui.lineEdit_PostTablePosY->text().toDouble() + 10;
				fPOI.z = k * imgSpac.z - ((imgSpac.z * imgDims.z) / 2.);
				m_vPOI_DCM.push_back(fPOI);
			}
		}
	}
	else
	{
		for (size_t k = 2; k < (imgDims.z - 2); k++)
		{
			for (size_t j = 2; j < (imgDims.y - 2); j++)
			{
				fPOI.x = 2;
				fPOI.y = j * imgSpac.y - ((imgSpac.y * imgDims.y) / 2.);
				fPOI.z = k * imgSpac.z - ((imgSpac.z * imgDims.z) / 2.);
				m_vPOI_DCM.push_back(fPOI);
			}
		}
	}
	cout << "POI data generated! last value: [" << fPOI.x << ", " << fPOI.y << ", " << fPOI.z << "]" << endl;

}

void CbctRecon::SLT_LoadPOIData()//it fills m_vPOI_DCM
{
  if (!m_vPOI_DCM.empty())
	m_vPOI_DCM.clear();


  QString filePath = QFileDialog::getOpenFileName(this, "POI data file",m_strPathDirDefault, "POI data file (*.txt)",0,0);

  if (filePath.length() < 1)
	return;

  ifstream fin;
  fin.open(filePath.toLocal8Bit().constData(), ios::in);
  if (fin.fail())
	return;

  char str[MAX_LINE_LENGTH];
  //File format:
  //no header

  //x1	y1	z1, in mm
  //x2	y2	z2,
  //..

  while(!fin.eof())
  {
	VEC3D fPOI;
	memset(str, 0 ,MAX_LINE_LENGTH);
	fin.getline(str,MAX_LINE_LENGTH);
	QString strLine(str);

	//QStringList strList = strLine.split(' ');//tab
	QStringList strList = strLine.split('\t');//tab
	//third one is the organ name
	if (strList.length() < 3)
	{
	  cout << "abnormal file expression." << endl;
	  break;
	}
	fPOI.x = strList.at(0).toDouble();
	fPOI.y = strList.at(1).toDouble();
	fPOI.z = strList.at(2).toDouble();

	m_vPOI_DCM.push_back(fPOI);
  }
  for (int i = 0 ; i< m_vPOI_DCM.size() ; i++)
  {
	cout << "Data "  << i << "	" << m_vPOI_DCM.at(i).x << ", " << m_vPOI_DCM.at(i).y << ", " << m_vPOI_DCM.at(i).z << endl;
  }
  cout << "POI data has been loaded. " << m_vPOI_DCM.size() << " data points are read" << endl;
  fin.close();
}

void CbctRecon::SLT_StartSyncFromSharedMem()
{
  //int msInterval = ui.lineEditTimerInterval->text().toInt();

  //if (msInterval > 0 && msInterval < 5000)
  //{
	 // if (m_arrYKImage != NULL)
	 // {
		//delete[] m_arrYKImage;
		//m_arrYKImage = NULL;
		//m_iImgCnt = 0;
	 // }

	 // m_iImgCnt = 1;
	 // m_arrYKImage = new YK16GrayImage [m_iImgCnt];
	 // m_arrYKImage[0].CreateImage(1024,1024,0);
	 //
	 // m_busyTimer =false;
	 // m_Timer->start(msInterval);
  //}

}

void CbctRecon::SLT_StopSyncFromSharedMem()
{
  //m_Timer->stop();

  //HANDLE hSemaphore = OpenSemaphore(SYNCHRONIZE ,FALSE, "YKSemaphore");
  //Option SYNCHRONIZE doesn't work! you cannot release Semaphore due to the access is denied (GetLastError 5)
  HANDLE hSemaphore = OpenSemaphore(SEMAPHORE_ALL_ACCESS ,FALSE, "YKSemaphore");
  //increase counter
//  LONG prev_counter;
 // ReleaseSemaphore(hSemaphore, 1, &prev_counter);
  //decrease counter
  //ReleaseSemaphore(hSemaphore, 1, &prev_counter);

  ofstream fout;
  fout.open("E:\\SemphoreLogC++.txt");


  int max_cnt = 200;

  int cnt = 0;
  while(cnt < max_cnt)
  {
	cnt++;

	DWORD dwWaitResult  = WaitForSingleObject(hSemaphore, 5);

	switch (dwWaitResult)
	{
	 // The semaphore object was signaled.
	case WAIT_OBJECT_0: //0 -->This got the semaphore token
	  //PERFORM TASK
	  printf("Thread %d: wait succeeded\n", GetCurrentThreadId());
	  fout << "Thread %d: wait succeeded " << GetCurrentThreadId() << endl;
	  Sleep(10);

	  if (!ReleaseSemaphore(
		hSemaphore,  // handle to semaphore
		1,            // increase count by one
		NULL) )       // not interested in previous count
	  {
		printf("ReleaseSemaphore error: %d\n", GetLastError()); //Access is denied.
	  }
	  break;

	case WAIT_TIMEOUT: //no need of release //258
	  //Sleep(50);
	  printf("Thread %d: wait timed out\n", GetCurrentThreadId());
	  fout << "Thread %d: wait timed out " << GetCurrentThreadId() << endl;
	  break;
	}
	//	ReleaseSemaphore(hSemaphore, 1, &prev_counter);
  }

  fout.close();
}

void CbctRecon::SLT_TimerEvent()
{
  if (m_busyTimer)
	return;

  if (m_arrYKImage == NULL || m_iImgCnt != 1)
	return;

  m_busyTimer = true;

  //Look into the shared mem
  TCHAR szName[] = TEXT("YKSharedMemory");
  HANDLE handle = OpenFileMapping(FILE_MAP_READ, FALSE, szName);

  if (handle == NULL)
  {
	cout << "Cannot open Mapped file" << endl;
	SLT_StopSyncFromSharedMem();
	return;
  }

  int size = 1024*1024*2;
  int pix_size = (int)(size/2.0);
  unsigned char* charBuf = NULL;
  charBuf = (unsigned char*)MapViewOfFile(handle, FILE_MAP_READ, 0, 0, size);

  if (charBuf == NULL)
  {
	cout << "Shared memory was not read. Timer will be stopped" << endl;
	SLT_StopSyncFromSharedMem();
	CloseHandle(handle);
	delete[] charBuf;
	return;
  }

  //byte array to unsigned short
  //assuming little endian

  //unsigned short* imgBuf = new unsigned short [pix_size];
  int idxA = 0;
  int idxB = 0;

  for (int i = 0 ; i<pix_size ; i++)
  {
	idxA = i*2+1;
	idxB = i*2;
	//0: 1,0  1: 3,2 ...
	m_arrYKImage[0].m_pData[i] = ((charBuf[idxA]<< 8)|charBuf[idxB]); //little endian
  }

  ui.spinBoxImgIdx->setValue(0);
  SLT_DrawRawImages();

  CloseHandle(handle);

  m_busyTimer = false;
}

void CbctRecon::SLTM_ViewExternalCommand()
{
	m_pDlgExternalCommand->show();
}


void CbctRecon::LoadExternalFloatImage(QString& strPath, bool bConversion)
{
	typedef itk::ImageFileReader<OutputImageType> ReaderType;
	ReaderType::Pointer reader = ReaderType::New();

	//QString filePath = strPath;

	reader->SetFileName(strPath.toLocal8Bit().constData());
	reader->Update();

	OutputImageType::Pointer spCrntImg = reader->GetOutput();

	//Float image
	cout << "Float image has been loaded" << endl;

	if (bConversion)
	{
		TransformationRTK2IEC(spCrntImg);
	}

	typedef itk::AbsImageFilter<OutputImageType, OutputImageType> AbsImageFilterType;
	AbsImageFilterType::Pointer absImgFilter = AbsImageFilterType::New();
	absImgFilter->SetInput(spCrntImg); // 20140206 modified it was a bug

	typedef itk::MultiplyImageFilter<OutputImageType, OutputImageType, OutputImageType> MultiplyImageFilterType;
	MultiplyImageFilterType::Pointer multiplyImageFilter = MultiplyImageFilterType::New();
	multiplyImageFilter->SetInput(absImgFilter->GetOutput());
	multiplyImageFilter->SetConstant(65536); //calculated already

	typedef itk::CastImageFilter< OutputImageType, USHORT_ImageType > CastFilterType;
	CastFilterType::Pointer castFilter = CastFilterType::New();
	castFilter->SetInput(multiplyImageFilter->GetOutput());
	castFilter->Update();
	m_spRawReconImg = castFilter->GetOutput();

	QString strCrntFileName;
	QFileInfo outFileInfo(strPath);
	strCrntFileName = outFileInfo.fileName();

	UpdateReconImage(m_spRawReconImg, strCrntFileName);
}

void CbctRecon::TransformationRTK2IEC(OutputImageType::Pointer& spSrcTarg)
{
	OutputImageType::SizeType sizeOutput = spSrcTarg->GetBufferedRegion().GetSize();
	OutputImageType::SpacingType spacingOutput = spSrcTarg->GetSpacing();

	//Transformation is applied
	cout << "Euler 3D Transformation: from RTK-procuded volume to standard DICOM coordinate" << endl;
	//Same image type from original image -3D & float
	OutputImageType::IndexType start_trans;
	start_trans[0] = 0;
	start_trans[1] = 0;
	start_trans[2] = 0;

	OutputImageType::SizeType size_trans;
	size_trans[0] = sizeOutput[0]; // X //410
	size_trans[1] = sizeOutput[2]; //Y  // 410
	size_trans[2] = sizeOutput[1]; //Z // 120?

	OutputImageType::SpacingType spacing_trans;
	spacing_trans[0] = spacingOutput[0];
	spacing_trans[1] = spacingOutput[2];
	spacing_trans[2] = spacingOutput[1];

	OutputImageType::PointType Origin_trans;
	Origin_trans[0] = -0.5* size_trans[0] * spacing_trans[0];
	Origin_trans[1] = -0.5* size_trans[1] * spacing_trans[1];
	Origin_trans[2] = -0.5* size_trans[2] * spacing_trans[2];

	OutputImageType::RegionType region_trans;
	region_trans.SetSize(size_trans);
	region_trans.SetIndex(start_trans);

	/* 2) Prepare Target image */
	OutputImageType::Pointer targetImg = spSrcTarg;

	/* 3) Configure transform */
	typedef itk::Euler3DTransform< double > TransformType;
	TransformType::Pointer transform = TransformType::New();

	TransformType::ParametersType param;
	param.SetSize(6);
	//MAXIMUM PARAM NUMBER: 6!!!
	param.put(0, 0.0); //rot X // 0.5 = PI/2
	param.put(1, itk::Math::pi / 2.0);//rot Y
	param.put(2, itk::Math::pi / -2.0);//rot Z
	param.put(3, 0.0); // Trans X mm
	param.put(4, 0.0); // Trans Y mm
	param.put(5, 0.0); // Trans Z mm

	TransformType::ParametersType fixedParam(3); //rotation center
	fixedParam.put(0, 0);
	fixedParam.put(1, 0);
	fixedParam.put(2, 0);

	transform->SetParameters(param);
	transform->SetFixedParameters(fixedParam); //Center of the Transform

	cout << "Transform matrix:" << "	" << endl;
	cout << transform->GetMatrix() << std::endl;

	typedef itk::ResampleImageFilter<OutputImageType, OutputImageType> ResampleFilterType;
	ResampleFilterType::Pointer resampler = ResampleFilterType::New();
	//OutputImageType::RegionType fixedImg_Region = fixedImg->GetLargestPossibleRegion().GetSize();

	resampler->SetInput(targetImg);
	resampler->SetSize(size_trans);
	resampler->SetOutputOrigin(Origin_trans); //Lt Top Inf of Large Canvas
	resampler->SetOutputSpacing(spacing_trans); // 1 1 1
	resampler->SetOutputDirection(targetImg->GetDirection()); //image normal?
	resampler->SetTransform(transform);

	cout << "Flip filter is being applied" << endl;

	typedef itk::FlipImageFilter< OutputImageType >  FilterType;
	FilterType::Pointer flipFilter = FilterType::New();
	typedef FilterType::FlipAxesArrayType FlipAxesArrayType;
	FlipAxesArrayType arrFlipAxes;
	arrFlipAxes[0] = 1;
	arrFlipAxes[1] = 0;
	arrFlipAxes[2] = 0;

	flipFilter->SetFlipAxes(arrFlipAxes);
	flipFilter->SetInput(resampler->GetOutput());

	flipFilter->Update();

	spSrcTarg = flipFilter->GetOutput();
}

void CbctRecon::SLTM_LoadDICOMdir()
{
	QString dirPath = QFileDialog::getExistingDirectory(this, tr("Open Directory"), m_strPathDirDefault, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

	if (dirPath.length() <= 1)
		return;

	Plm_image plmImg;
	plmImg.load_native(dirPath.toLocal8Bit().constData());
	itk::Image<SHORT_PixelType, 3>::Pointer spPlmShortImg = plmImg.itk_short();
	typedef itk::CastImageFilter <itk::Image< SHORT_PixelType, 3 >, SHORT_ImageType> CastFilterType;
	CastFilterType::Pointer castFilter = CastFilterType::New();
	castFilter->SetInput(spPlmShortImg);
	castFilter->Update();
	SHORT_ImageType::Pointer spShortImg = castFilter->GetOutput();
	//Figure out whether this is NKI
	typedef itk::MinimumMaximumImageCalculator <SHORT_ImageType> ImageCalculatorFilterType;
	ImageCalculatorFilterType::Pointer imageCalculatorFilter = ImageCalculatorFilterType::New();
	imageCalculatorFilter->SetImage(spShortImg);
	imageCalculatorFilter->Compute();

	double minVal0 = (double)(imageCalculatorFilter->GetMinimum());
	double maxVal0 = (double)(imageCalculatorFilter->GetMaximum());


	//Thresholding
	typedef itk::ThresholdImageFilter <SHORT_ImageType> ThresholdImageFilterType;
	ThresholdImageFilterType::Pointer thresholdFilter = ThresholdImageFilterType::New();

	thresholdFilter->SetInput(spShortImg);
	thresholdFilter->ThresholdOutside(-1024, 3072); //--> 0 ~ 4095
	thresholdFilter->SetOutsideValue(-1024);
	thresholdFilter->Update();

	imageCalculatorFilter->SetImage(thresholdFilter->GetOutput());
	imageCalculatorFilter->Compute();

	double minVal = (double)(imageCalculatorFilter->GetMinimum());
	double maxVal = (double)(imageCalculatorFilter->GetMaximum());

	cout << "Current Min and Max Values are	" << minVal << "	" << maxVal << endl;

	unsigned short outputMinVal = (USHORT_PixelType)(minVal + 1024);
	unsigned short outputMaxVal = (USHORT_PixelType)(maxVal + 1024);

	typedef itk::RescaleIntensityImageFilter<SHORT_ImageType, USHORT_ImageType> RescaleFilterType;
	RescaleFilterType::Pointer spRescaleFilter = RescaleFilterType::New();
	spRescaleFilter->SetInput(thresholdFilter->GetOutput());
	spRescaleFilter->SetOutputMinimum(outputMinVal);
	spRescaleFilter->SetOutputMaximum(outputMaxVal);
	spRescaleFilter->Update();

	//m_spRawReconImg = spRescaleFilter->GetOutput();
	m_spRefCTImg = spRescaleFilter->GetOutput();

	UpdateReconImage(m_spRefCTImg, QString("DICOM reference image"));

	RegisterImgDuplication(REGISTER_REF_CT, REGISTER_MANUAL_RIGID);
}

void CbctRecon::SLTM_LoadRTKoutput()
{
	QString filePath = QFileDialog::getOpenFileName(this, "Open Image", m_strPathDirDefault, "rtk output float image (*.mha)", 0, 0);
	LoadExternalFloatImage(filePath, true);
}
//Only can be used for m_spRawRecon
void CbctRecon::MedianFilterByGUI()
{
	if (!m_spCrntReconImg)
		return;

	/*QString strCrntFileName;
	QFileInfo outFileInfo(strPath);
	strCrntFileName = outFileInfo.fileName();*/

	USHORT_ImageType::SizeType indexRadius;
	indexRadius[0] = ui.lineEdit_PostMedSizeX->text().toInt(); // radius along x
	indexRadius[1] = ui.lineEdit_PostMedSizeY->text().toInt(); // radius along y
	indexRadius[2] = ui.lineEdit_PostMedSizeZ->text().toInt(); // radius along y

	if (indexRadius[0] != 0 || indexRadius[1] != 0 || indexRadius[2] != 0)
	{
		typedef itk::MedianImageFilter<USHORT_ImageType, USHORT_ImageType >  FilterType;
		FilterType::Pointer medFilter = FilterType::New();

		//this is radius. 1 --> median window 3
		cout << "Post median(3D) filtering is under progress..Size(radius X Y Z) is = " << indexRadius << endl;

		medFilter->SetRadius(indexRadius);
                medFilter->SetInput(m_spCrntReconImg);
		medFilter->Update();

                m_spCrntReconImg = medFilter->GetOutput();
		cout << "median filtering has been done" << endl;

		QString prevFileName = ui.lineEdit_Cur3DFileName->text();

                UpdateReconImage(m_spCrntReconImg, prevFileName.append("_med"));
	}
	else
	{
		cout << "Not valid median window" << endl;
	}
}
//Only can be used for m_spRawRecon
void CbctRecon::FileExportByGUI()//USHORT
{
	QString outputFilePath = ui.lineEdit_OutputFilePath->text();
	QFileInfo outFileInfo(outputFilePath);
	QDir outFileDir= outFileInfo.absoluteDir();

	//bool b = outFileDir.exists();
	//QString tmpPath = outFileDir.absolutePath();

	if (outputFilePath.length() < 2 || !outFileDir.exists())
	{
		cout << "No available output path. Should be exported later" << endl;
	}
	else
	{
		typedef itk::ImageFileWriter<USHORT_ImageType> WriterType;
		WriterType::Pointer writer = WriterType::New();
		writer->SetFileName(outputFilePath.toLocal8Bit().constData());
		writer->SetUseCompression(true); //not exist in original code (rtkfdk)
		writer->SetInput(m_spRawReconImg);

		cout << "Writing the image to: " << outputFilePath.toLocal8Bit().constData() << endl;

		TRY_AND_EXIT_ON_ITK_EXCEPTION(writer->Update());

		std::cout << std::endl;
		std::cout << "The output image was successfully saved" << std::endl;
	}
}

void CbctRecon::SLT_MedianFilterDoNow()
{
	MedianFilterByGUI();
}

void CbctRecon::SLT_Export2DDose_TIF() //2D dose from current displayed image of reconstruction
{
	if (m_dspYKReconImage == NULL)
		return;

	if (!m_spCrntReconImg)
		return;

	QString strPath = QFileDialog::getSaveFileName(this, "Save Image", "", "signed short meta image (*.tif)", 0, 0);
	if (strPath.length() <= 1)
		return;

	double originLeft = (double)(m_spCrntReconImg->GetOrigin()[0]);
	double originTop = (double)(m_spCrntReconImg->GetOrigin()[1]);//not sure...

	double spacingX = (double)(m_spCrntReconImg->GetSpacing()[0]);
	double spacingY = (double)(m_spCrntReconImg->GetSpacing()[1]);//not sure...

	if (!SaveDoseGrayImage(strPath.toLocal8Bit().constData(), m_dspYKReconImage->m_iWidth, m_dspYKReconImage->m_iHeight, spacingX, spacingY, originLeft, originTop, m_dspYKReconImage->m_pData))
	{
		cout << "Failed in save gray dose file" << endl;
	}
	else
	{
		cout << "image exported successfully." << endl;
	}

}

void CbctRecon::SLTM_Export2DDoseMapAsMHA()
{
	if (m_dspYKReconImage == NULL)
		return;

	if (!m_spCrntReconImg)
		return;

	QString strPath = QFileDialog::getSaveFileName(this, "Save Image", "", "itk compatible meta image (*.mha)", 0, 0);
	if (strPath.length() <= 1)
		return;


	double originLeft = (double)(m_spCrntReconImg->GetOrigin()[0]);
	double originTop = (double)(m_spCrntReconImg->GetOrigin()[1]);//not sure...

	double spacingX = (double)(m_spCrntReconImg->GetSpacing()[0]);
	double spacingY = (double)(m_spCrntReconImg->GetSpacing()[1]);//not sure...

	//Export float 2D image
	OutputImageType2D::Pointer doseImg2D = OutputImageType2D::New();
	OutputImageType2D::SizeType doseSize;
	doseSize[0] = m_dspYKReconImage->m_iWidth;
	doseSize[1] = m_dspYKReconImage->m_iHeight;

	OutputImageType2D::IndexType doseStart;
	doseStart[0] = 0;
	doseStart[1] = 0;

	OutputImageType2D::RegionType doseRegion;
	doseRegion.SetSize(doseSize);
	doseRegion.SetIndex(doseStart);

	OutputImageType2D::SpacingType doseSpacing;
	doseSpacing[0] = spacingX;
	doseSpacing[1] = spacingY;

	OutputImageType2D::PointType doseOrigin;
	doseOrigin[0] = originLeft;
	doseOrigin[1] = originTop;

	doseImg2D->SetRegions(doseRegion);
	doseImg2D->SetSpacing(doseSpacing);
	doseImg2D->SetOrigin(doseOrigin);

	doseImg2D->Allocate();
	doseImg2D->FillBuffer(0);

	double factor_ushort2float = 0.01; // cGy --> Gy

	itk::ImageRegionIterator<OutputImageType2D> it(doseImg2D, doseImg2D->GetLargestPossibleRegion());

	float pixel_val = 0.0f;
	int i = 0;
	for (it.GoToBegin(); !it.IsAtEnd(); ++it)
	{
		pixel_val = (double)m_dspYKReconImage->m_pData[i] * factor_ushort2float;
		it.Set(pixel_val);
		i++;
	}
	//YK201502
	typedef itk::ImageFileWriter<OutputImageType2D> WriterType;
	WriterType::Pointer writer = WriterType::New();
	writer->SetFileName(strPath.toLocal8Bit().constData());
	writer->SetUseCompression(true);
	writer->SetInput(doseImg2D);
	writer->Update();

	cout << "File was exported successfully" << endl;
}

void CbctRecon::SLTM_ExportProjGeometryTXT()
{
	//if (!m_spFullGeometry)
	//	return;

	if (!m_spCustomGeometry) //will be filled after Projection load button is pushed
		return;

	QString strPath = QFileDialog::getSaveFileName(this, "Save text file", "", "text (*.txt)", 0, 0);

	if (strPath.length() <= 1)
		return;

	vector<double>::const_iterator itAng, itShiftX, itShiftY;

	int cntAngle = m_spCustomGeometry->GetGantryAngles().size();
	int cntShiftX = m_spCustomGeometry->GetProjectionOffsetsX().size();
	int cntShiftY = m_spCustomGeometry->GetProjectionOffsetsY().size();

	if (cntAngle <= 0)
	{
		cout << "Error! no angle vector is found" << endl;
		return;
	}


	if (cntAngle != cntShiftX || cntAngle != cntShiftY)
	{
		cout << "Error! Angle number and shift number are not matching." << endl;
		return;
	}

	itShiftX = m_spCustomGeometry->GetProjectionOffsetsX().begin();
	itShiftY = m_spCustomGeometry->GetProjectionOffsetsY().begin();

	ofstream fout;
	fout.open(strPath.toLocal8Bit().constData());

	fout << "MV_Gantry_Angle" << "	" << "PanelShiftX(mm)" << "	" << "PanelShiftY(mm)" << endl;

	for (itAng = m_spCustomGeometry->GetGantryAngles().begin(); itAng != m_spCustomGeometry->GetGantryAngles().end(); itAng++)
	{
		fout << (*itAng) << "	" << (*itShiftX) << "	" << (*itShiftY) << endl;

		itShiftX++;
		itShiftY++;
	}

	fout.close();
}

void CbctRecon::LoadXVIGeometryFile(const char* filePath)
{
	QString strFilePath = filePath;

	m_spFullGeometry = GeometryType::New();

	FLEXDATA flxData;

	/* We'll parse the example.xml */
	QFile* file = new QFile(strFilePath);
	/* If we can't open it, let's show an error message. */
	if (!file->open(QIODevice::ReadOnly | QIODevice::Text)) {
		return;
	}
	/* QXmlStreamReader takes any QIODevice. */
	QXmlStreamReader xml(file);
	//QList< QMap<QString, QString> > persons;
	/* We'll parse the XML until we reach end of it.*/
	while (!xml.atEnd() &&
		!xml.hasError()) {
		/* Read next element.*/
		QXmlStreamReader::TokenType token = xml.readNext();
		/* If token is just StartDocument, we'll go to next.*/
		if (token == QXmlStreamReader::StartDocument) {
			continue;
		}
		/* If token is StartElement, we'll see if we can read it.*/
		if (token == QXmlStreamReader::StartElement) {
			/* If it's named persons, we'll go to the next.*/
			if (xml.name() == "Frames") {
				continue;
			}
			/* If it's named person, we'll dig the information from there.*/
			if (xml.name() == "Frame")
			{
				flxData = XML_parseFrameForXVI5(xml);
				//m_vRetroFlexmap.push_back(flxData);

				if (flxData.fGanAngle < 0)
					flxData.fGanAngle = flxData.fGanAngle + 360.0;

				flxData.fPanelOffsetX = -flxData.fPanelOffsetX;
				flxData.fPanelOffsetY = -flxData.fPanelOffsetY;

				////Image qual test
				//flxData.fGanAngle = -flxData.fGanAngle;
				//if (flxData.fGanAngle < 0)
				//	flxData.fGanAngle = flxData.fGanAngle + 360.0;
				//flxData.fPanelOffsetX = -flxData.fPanelOffsetX;
				//flxData.fPanelOffsetY = -flxData.fPanelOffsetY;

				m_spFullGeometry->AddProjection(1000.0, 1536.0, flxData.fGanAngle,
					flxData.fPanelOffsetX, flxData.fPanelOffsetY, //Flexmap
					0.0, 0.0, //In elekta, these are 0
					0.0, 0.0); //In elekta, these are 0
			}
		}
	}
	/* Error handling. */
	if (xml.hasError()) {
		QMessageBox::critical(this,
			"SLT_SetFlexmap",
			xml.errorString(),
			QMessageBox::Ok);
	}
	/* Removes any device() or data from the reader
	* and resets its internal state to the initial state. */
	xml.clear();

}



FLEXDATA CbctRecon::XML_parseFrameForXVI5(QXmlStreamReader& xml)
{
	FLEXDATA tmpResult;
	tmpResult.fGanAngle = 0.0;
	tmpResult.fPanelOffsetX = 0.0;
	tmpResult.fPanelOffsetY = 0.0;

	/* Let's check that we're really getting a person. */
	if (xml.tokenType() != QXmlStreamReader::StartElement &&
		xml.name() == "Frame") {
		return tmpResult;
	}
	/* Let's get the attributes for person */
	//QXmlStreamAttributes attributes = xml.attributes();
	/* Let's check that person has id attribute. */
	//if (attributes.hasAttribute("id")) {
	//	/* We'll add it to the map. */
	//	person["id"] = attributes.value("id").toString();
	//}
	/* Next element... */
	xml.readNext();
	/*
	* We're going to loop over the things because the order might change.
	* We'll continue the loop until we hit an EndElement named person.
	*/
	while (!(xml.tokenType() == QXmlStreamReader::EndElement &&
		xml.name() == "Frame")) {
		QStringRef tmpXmlName = (xml.name());
		QString strTmpXMLName = QString(tmpXmlName.toLocal8Bit().constData());
		int tmpType = (int)(xml.tokenType());

		QString tmpStr;
		if (xml.tokenType() == QXmlStreamReader::StartElement) {
			/* We've found first name. */
			if (xml.name() == "Seq") {
				tmpStr = XML_GetSingleItemString(xml);
			}
			/* We've found surname. */
			else if (xml.name() == "DeltaMS") {
				tmpStr = XML_GetSingleItemString(xml);
			}
			/* We've found email. */
			else if (xml.name() == "HasPixelFactor") {
				tmpStr = XML_GetSingleItemString(xml);
			}
			/* We've found website. */
			else if (xml.name() == "PixelFactor") {
				tmpStr = XML_GetSingleItemString(xml);
			}
			else if (xml.name() == "GantryAngle") {
				tmpStr = XML_GetSingleItemString(xml);
				tmpResult.fGanAngle = tmpStr.toDouble();
			}
			else if (xml.name() == "Exposed") {
				tmpStr = XML_GetSingleItemString(xml);
			}
			else if (xml.name() == "MVOn") {
				tmpStr = XML_GetSingleItemString(xml);
			}
			else if (xml.name() == "UCentre") {
				tmpStr = XML_GetSingleItemString(xml);
				tmpResult.fPanelOffsetX = tmpStr.toDouble();
			}
			else if (xml.name() == "VCentre") {
				tmpStr = XML_GetSingleItemString(xml);
				tmpResult.fPanelOffsetY = tmpStr.toDouble();
			}
			else if (xml.name() == "Inactive") {
				tmpStr = XML_GetSingleItemString(xml);
			}
		}
		xml.readNext();
	}
	return tmpResult;

}

QString CbctRecon::XML_GetSingleItemString(QXmlStreamReader& xml)
{
	QString strResult = "";
	/* We need a start element, like <foo> */
	if (xml.tokenType() != QXmlStreamReader::StartElement) {
		return strResult;
	}

	/* Let's read the name... */
	QString elementName = xml.name().toString();
	/* ...go to the next. */
	xml.readNext();
	/*
	* This elements needs to contain Characters so we know it's
	* actually data, if it's not we'll leave.
	*/
	if (xml.tokenType() != QXmlStreamReader::Characters) {
		return strResult;
	}
	strResult = xml.text().toString();
	return strResult;

}

void CbctRecon::SLTM_ForwardProjection()
{
    GeometryType::Pointer crntGeometry = GeometryType::New();

    int cntProj = m_spCustomGeometry->GetGantryAngles().size();

    if (cntProj < 1)
    {
        cout << "ERROR: geometry is not ready" << endl;
        return;
    }

    QMessageBox msgBox;
    msgBox.setText("Do you want to override panel shifts with 0 before forward projection?");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);

    int bOverridePanelShift = msgBox.exec();

    //if (res == QMessageBox::Yes)

    //Regenerate geometry object

    for (int i = 0; i < cntProj; i++)
    {
        double curSID = m_spCustomGeometry->GetSourceToIsocenterDistances().at(i);
        double curSDD = m_spCustomGeometry->GetSourceToDetectorDistances().at(i);

        double curGantryAngle = m_spCustomGeometry->GetGantryAngles().at(i);
		double kVAng = curGantryAngle * 360. / (2.*itk::Math::pi);
		double MVAng = kVAng - 90.0;
		if (MVAng < 0.0)
			MVAng = MVAng + 360;
		curGantryAngle = MVAng;

        double curProjOffsetX = m_spCustomGeometry->GetProjectionOffsetsX().at(i);
        double curProjOffsetY = m_spCustomGeometry->GetProjectionOffsetsY().at(i);

        double curOutOfPlaneAngles = m_spCustomGeometry->GetOutOfPlaneAngles().at(i);
        double curInPlaneAngles = m_spCustomGeometry->GetInPlaneAngles().at(i);

        double curSrcOffsetX = m_spCustomGeometry->GetSourceOffsetsX().at(i);
        double curSrcOffsetY = m_spCustomGeometry->GetSourceOffsetsY().at(i);

        if (bOverridePanelShift)
        {
            /*curProjOffsetX = 0.0;
            curProjOffsetY = 0.0;*/
            curProjOffsetX = -100.0;//half fan, shifted toward patient right when kV source is 0
            curProjOffsetY = -20.0;//shifted toward superior
        }

        crntGeometry->AddProjection(curSID, curSDD, curGantryAngle,
            curProjOffsetX, curProjOffsetY, //Flexmap
            curOutOfPlaneAngles, curInPlaneAngles, //In elekta, these are 0
            curSrcOffsetX, curSrcOffsetY); //In elekta, these are 0
    }

    ForwardProjection(m_spRawReconImg, crntGeometry, m_spProjImgRaw3D, true); //final moving image

    //Export geometry txt
    QString strPath = QFileDialog::getSaveFileName(this, "Save geometry file for forward projection", "", "text (*.txt)", 0, 0);

    if (strPath.length() <= 1)
        return;

    vector<double>::const_iterator itAng, itShiftX, itShiftY;

    int cntAngle = crntGeometry->GetGantryAngles().size();
    int cntShiftX = crntGeometry->GetProjectionOffsetsX().size();
    int cntShiftY = crntGeometry->GetProjectionOffsetsY().size();

    if (cntAngle <= 0)
    {
        cout << "Error! no angle vector is found" << endl;
        return;
    }


    if (cntAngle != cntShiftX || cntAngle != cntShiftY)
    {
        cout << "Error! Angle number and shift number are not matching." << endl;
        return;
    }

    itShiftX = crntGeometry->GetProjectionOffsetsX().begin();
    itShiftY = crntGeometry->GetProjectionOffsetsY().begin();

    ofstream fout;
    fout.open(strPath.toLocal8Bit().constData());

    fout << "MV_Gantry_Angle" << "	" << "PanelShiftX(mm)" << "	" << "PanelShiftY(mm)" << endl;

    for (itAng = crntGeometry->GetGantryAngles().begin(); itAng != crntGeometry->GetGantryAngles().end(); itAng++)
    {
        fout << (*itAng) << "	" << (*itShiftX) << "	" << (*itShiftY) << endl;

        itShiftX++;
        itShiftY++;
    }

    fout.close();

}

void CbctRecon::SLTM_FineResolScatterCorrectrionMacro()
{
    float curResampleF = m_fResampleF;
    ui.lineEdit_DownResolFactor->setText("1.0");
    SLT_LoadSelectedProjFiles();
    m_fResampleF = curResampleF;
    ui.lineEdit_DownResolFactor->setText(QString("%1").arg(m_fResampleF));

    //Scatter correction

    SLT_DoScatterCorrection_APRIORI();
}

void CbctRecon::SLTM_FullScatterCorrectionMacroAP() //single. should be called after HIS folder is defined
{
    if (m_strPathPatientDir.length() < 2)
        return;

    enREGI_IMAGES enRegImg = REGISTER_DEFORM_FINAL;
    bool bFullResolForFinalRecon = false;

    bool bIntensityShift = true;

    FullScatterCorrectionMacroSingle(m_strPathPatientDir, enRegImg, bFullResolForFinalRecon, false, bIntensityShift);
}


void CbctRecon::SLTM_BatchScatterCorrectionMacroAP()
{
    //Scatter parameters
    QTime batchmodeTime = QTime::currentTime();

    //1) Get img_ file lists
    QString dirPath = QFileDialog::getExistingDirectory(this, tr("Open IMAGES Directory"),
        m_strPathDirDefault, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    QDir dirIMAGES = QDir(dirPath);

    if (!dirIMAGES.exists())
        return;

    QFileInfoList listDir;
    listDir = dirIMAGES.entryInfoList(QDir::Dirs, QDir::Name);

    int iCntProjDir = listDir.size();

    cout << "Found directory number= " << iCntProjDir - 2 << endl;

    QString curProjDirPath;
    if (iCntProjDir <= 2) //only /. and /.. exist
    {
        cout << "Error! No projection directory exists." << endl;
        return;
    }
    //Several questions to set params
    //1) Output Dir
    QString strOutDirPath = QFileDialog::getExistingDirectory(this, tr("Open Output Directory"),
        m_strPathDirDefault, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (strOutDirPath.length() < 1)
        return;

    //2) fwd reference: manual or auto-rigid or deformed

    bool ok;
    QString text = QInputDialog::getText(this, "Input Dialog",
        "Reference for forward projection([0] Deformed CT, [1] auto-rigid CT, [2] manual-aligned CT(dcm_plan), [3] DeformedCT_skipAutoRigid",
        QLineEdit::Normal, "0", &ok);

    enREGI_IMAGES enRegImg;

    if (ok && !text.isEmpty())
    {
        int iRefImgVal = text.toInt();

        if (iRefImgVal == 0)
            enRegImg = REGISTER_DEFORM_FINAL;
        else if (iRefImgVal == 1)
            enRegImg = REGISTER_AUTO_RIGID;
        else if (iRefImgVal == 2)
            enRegImg = REGISTER_MANUAL_RIGID;
        else if (iRefImgVal == 3)
            enRegImg = REGISTER_DEFORM_SKIP_AUTORIGID;
        else
            enRegImg = REGISTER_DEFORM_FINAL;


    }

    int res;
    //3) Fine resol option
    bool bFullResolForFinalRecon = false;
    bool bIntensityShift = false;
    /*QMessageBox msgBox;
    QString strMsg = "Full-resolution reconstruction after scatter generation?";
    msgBox.setText(strMsg);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    int res = msgBox.exec();
    if (res == QMessageBox::Yes)
    {
        bFullResolForFinalRecon = true;
    }*/

    /* QMessageBox msgBox;
     QString strMsg = "Apply truncation for raw CBCT?";
     msgBox.setText(strMsg);
     msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
     res = msgBox.exec();

     if (res == QMessageBox::Yes)
     {
     bTrancOnlyRaw = true;
     }*/

    QMessageBox msgBox;
    QString strMsg = "Intensity shift for raw CBCT?";
    msgBox.setText(strMsg);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    res = msgBox.exec();

    if (res == QMessageBox::Yes)
    {
        bIntensityShift = true;
    }

    /*QMessageBox msgBox;
    QString strMsg = "Full-resolution reconstruction after scatter generation?";
    msgBox.setText(strMsg);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    int res = msgBox.exec();
    if (res == QMessageBox::Yes)
    {
    bFullResolForFinalRecon = true;
    }*/


    bool bExportShortImages = false;
    QMessageBox msgBox2;
    QString strMsg2 = "Export short images after correction?";
    msgBox2.setText(strMsg2);
    msgBox2.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    res = msgBox2.exec();
    if (res == QMessageBox::Yes)
    {
        bExportShortImages = true;
    }

    int cntHisDir = 0;
    for (int i = 2; i < iCntProjDir; i++)// start from 2
    {
        curProjDirPath = listDir.at(i).absoluteFilePath();

        if (curProjDirPath.contains("img_"))
        {
            cout << "Found projection dir number: " << cntHisDir << ", Current Proj Path: " << curProjDirPath.toLocal8Bit().constData() << endl;
            SetProjDir(curProjDirPath);
            FullScatterCorrectionMacroSingle(strOutDirPath, enRegImg, bFullResolForFinalRecon, bExportShortImages, bIntensityShift);
            cntHisDir++;
        }
    }

    /*QMessageBox msgBoxFinal;
    msgBoxFinal.setText("All Done!");
    msgBoxFinal.exec();*/

    float elapsedSec = batchmodeTime.elapsed() / 1000.0;

    cout << "Batch mode calculation is done! " << QString::number(elapsedSec, 'f', 2).toLocal8Bit().constData() << " seconds was spent for " << cntHisDir << " cases" << endl;
}

bool CbctRecon::FullScatterCorrectionMacroSingle(QString& outputDirPath, enREGI_IMAGES enFwdRefImg, bool bFullResolRecon, bool bExportImages, bool bCBCT_IntensityShift)
{
    if (m_strDCMUID.length() < 1)
        return false;

    m_bMacroContinue = true;

    bool bFOVCropping = ui.checkBox_PostDispObjOn->isChecked(); //this button is for display, but use it for cropping option in macro mode
    float physPosX = ui.lineEdit_PostFOV_X->text().toFloat();
    float physPosY = ui.lineEdit_PostFOV_Y->text().toFloat();
    float physRadius = ui.lineEdit_PostFOV_R->text().toFloat();
    float physTablePosY = ui.lineEdit_PostTablePosY->text().toFloat();

    //Load Pushbutton
    SLT_LoadSelectedProjFiles();

    float fOldValTruncation = ui.lineEdit_Ramp_TruncationCorrection->text().toFloat();;


    SLT_DoReconstruction();
    //ui.lineEdit_Ramp_TruncationCorrection->setText(QString("0.0"));


    int addingVal = ui.lineEdit_AddConstHU->text().toInt();

    if (addingVal != 0)
    {
        cout << "Raw CBCT is being added by HU of: " << addingVal << endl;
        SLT_AddConstHUToCurImg();
    }

    if (bFOVCropping)
    {
        //Crop CBCT with predetermined FOV/ Table
		cout << "FOV cropping is under way..." << endl;
        CropFOV3D(m_spRawReconImg, physPosX, physPosY, physRadius, physTablePosY);
    }

    SLT_ViewRegistration();

    m_pDlgRegistration->SLT_PreProcessCT();
    if (!m_bMacroContinue)
    {
        cout << "Stopped during MacroSingle due to error in PreProcessCT" << endl;
        return false;
    }


    QString strSuffix;
    switch (enFwdRefImg)
    {
    case REGISTER_MANUAL_RIGID:
        m_pDlgRegistration->SLT_ManualMoveByDCMPlan();
        m_pDlgRegistration->SLT_ConfirmManualRegistration();//skin cropping for CBCT. only works when CBCT_skin crop is on
        strSuffix = strSuffix + "_man";

        if (bCBCT_IntensityShift)
            m_pDlgRegistration->SLT_IntensityNormCBCT();

        break;
    case REGISTER_AUTO_RIGID:
        m_pDlgRegistration->SLT_ManualMoveByDCMPlan();
        m_pDlgRegistration->SLT_ConfirmManualRegistration();//skin cropping

        if (bCBCT_IntensityShift)
            m_pDlgRegistration->SLT_IntensityNormCBCT();

        //OPtional
        if (bFOVCropping)
            CropFOV3D(m_spManualRigidCT, physPosX, physPosY, physRadius, physTablePosY);

        m_pDlgRegistration->SLT_DoRegistrationRigid();
        strSuffix = strSuffix + "_rigid";
        break;
    case REGISTER_DEFORM_FINAL:
		cout << "REGISTER_DEFORM_FINAL was chosen." << endl;
        m_pDlgRegistration->SLT_ManualMoveByDCMPlan();
        m_pDlgRegistration->SLT_ConfirmManualRegistration();//skin cropping

		if (bCBCT_IntensityShift)
		{
			cout << "IntensityShift is underway" << endl;
			m_pDlgRegistration->SLT_IntensityNormCBCT();
		}

        if (bFOVCropping)
            CropFOV3D(m_spManualRigidCT, physPosX, physPosY, physRadius, physTablePosY);

        m_pDlgRegistration->SLT_DoRegistrationRigid();
        m_pDlgRegistration->SLT_DoRegistrationDeform();
        strSuffix = strSuffix + "_defrm";
        break;

    case REGISTER_DEFORM_SKIP_AUTORIGID:
        m_pDlgRegistration->SLT_ManualMoveByDCMPlan();
        m_pDlgRegistration->SLT_ConfirmManualRegistration();//skin cropping
        //m_pDlgRegistration->SLT_DoRegistrationRigid();

        if (bCBCT_IntensityShift)
            m_pDlgRegistration->SLT_IntensityNormCBCT();

        if (bFOVCropping)
            CropFOV3D(m_spManualRigidCT, physPosX, physPosY, physRadius, physTablePosY);

        m_pDlgRegistration->SLT_DoRegistrationDeform();
        strSuffix = strSuffix + "_defrm_skipRigid";
        break;

    default:
        break;
    }

    if (bFullResolRecon) //if fullResol recon is on, load original proj files again
    {
        float curResampleF = m_fResampleF;
        ui.lineEdit_DownResolFactor->setText("1.0");
        SLT_LoadSelectedProjFiles();
        m_fResampleF = curResampleF;
        ui.lineEdit_DownResolFactor->setText(QString("%1").arg(m_fResampleF));

        strSuffix = strSuffix + "_HD";
    }
    else
    {
        //strSuffix = strSuffix + "_HD"
    }
    SLT_DoScatterCorrection_APRIORI();
    //1) Save the corrCBCT image as signed short
    QString outputPath_rawCBCT = outputDirPath + "/" + m_strDCMUID + strSuffix + "_rawCBCT.mha";
    QString outputPath_corrCBCT = outputDirPath + "/" + m_strDCMUID + strSuffix + "_corrCBCT.mha";
    QString outputPath_manCT = outputDirPath + "/" + m_strDCMUID + strSuffix + "_manCT.mha";
    QString outputPath_rigidCT = outputDirPath + "/" + m_strDCMUID + strSuffix + "_rigidCT.mha";
    QString outputPath_deformCT = outputDirPath + "/" + m_strDCMUID + strSuffix + "_deformCT.mha";

    if (bExportImages)
    {
        ExportReconSHORT_HU(m_spRawReconImg, outputPath_rawCBCT);
        ExportReconSHORT_HU(m_spScatCorrReconImg, outputPath_corrCBCT);
        ExportReconSHORT_HU(m_spManualRigidCT, outputPath_manCT);
        ExportReconSHORT_HU(m_spAutoRigidCT, outputPath_rigidCT);
        ExportReconSHORT_HU(m_spDeformedCT_Final, outputPath_deformCT);
    }

    //2) Calculate batched WEPL points
    if (!m_vPOI_DCM.empty())
    {
        QString outputTxtPath = outputDirPath + "/" + m_strDCMUID +strSuffix +  "_WEPL.txt";
        ExportAngularWEPL_byFile(outputTxtPath);
    }
    return true;
}

bool CbctRecon::GetXrayParamFromINI(QString& strPathINI, float& kVp, float& mA, float& ms)
{
    QFileInfo info = QFileInfo(strPathINI);

    kVp = 0.0;
    mA = 0.0;
    ms = 0.0;

    if (!info.exists())
    {
        return false;
    }

    //TubeMA=64.0000
    //TubeKV = 120.0000
    //TubeKVLength = 40.0000
    ifstream fin;
    fin.open(strPathINI.toLocal8Bit().constData());

    if (fin.fail())
        return false;

    char str[MAX_LINE_LENGTH];

    while (!fin.eof())
    {
        memset(str, 0, MAX_LINE_LENGTH);
        fin.getline(str, MAX_LINE_LENGTH);
        QString tmpStr = QString(str);
        QStringList strListParam = tmpStr.split("=");

        QString tagName;
        QString strVal;

        if (strListParam.count() == 2)
        {
            tagName = strListParam.at(0);
            strVal = strListParam.at(1);
            tagName = tagName.trimmed();
            strVal = strVal.trimmed();

            if (tagName == "TubeMA")
                mA = strVal.toFloat();
            if (tagName == "TubeKVLength")
                ms = strVal.toFloat();
            if (tagName == "TubeKV")
                kVp = strVal.toFloat();
        }
    }
    fin.close();

    if (kVp == 0 || mA == 0 || ms == 0)
        return false;

    return true;
}

void CbctRecon::GenerateCylinderMask(USHORT_ImageType::Pointer& spImgCanvas, float fDcmPosX, float fDcmPosY, float fRadius)
{
    if (!spImgCanvas)
        return;
    //1) region iterator, set 0 for all pixels outside the circle and below the table top, based on physical position
    USHORT_ImageType::PointType origin = spImgCanvas->GetOrigin();
    USHORT_ImageType::SpacingType spacing = spImgCanvas->GetSpacing();
    USHORT_ImageType::SizeType size = spImgCanvas->GetBufferedRegion().GetSize();

    //itk::ImageSliceConstIteratorWithIndex<OutputImageType> it (m_spReconImg, m_spReconImg->GetRequestedRegion());
    itk::ImageSliceIteratorWithIndex<USHORT_ImageType> it(spImgCanvas, spImgCanvas->GetRequestedRegion());

    //ImageSliceConstIteratorWithIndex<ImageType> it( image, image->GetRequestedRegion() );
    USHORT_ImageType::SizeType imgSize = spImgCanvas->GetRequestedRegion().GetSize(); //1016x1016 x z

    int width = imgSize[0];
    int height = imgSize[1];

    it.SetFirstDirection(0); //x?
    it.SetSecondDirection(1); //y?
    it.GoToBegin();

    int iNumSlice = 0;
    int iPosX = 0;
    int iPosY = 0;

    int i = 0;//height
    int j = 0; // width

    double crntPhysX = 0.0;
    double crntPhysY = 0.0;

    while (!it.IsAtEnd())
    {
        iPosY = 0;
        while (!it.IsAtEndOfSlice())
        {
            iPosX = 0;
            while (!it.IsAtEndOfLine())
            {
                //Calculate physical position

                crntPhysX = iPosX*(double)spacing[0] + (double)origin[0];
                crntPhysY = iPosY*(double)spacing[1] + (double)origin[1];

                if (pow(crntPhysX - fDcmPosX, 2.0) + pow(crntPhysY - fDcmPosY, 2.0) >= pow(fRadius, 2.0))
                {
                    //(*it) = (unsigned short)0; //air value
                    it.Set(0);
                }
                else
                {
                    it.Set(1);
                }

                ++it;
                iPosX++;
            }
            it.NextLine();
            iPosY++;
        }
        it.NextSlice();
        iNumSlice++;
    }
}


float CbctRecon::GetMeanIntensity(USHORT_ImageType::Pointer& spImg, float sphereR, float* sdIntensity)
{
    if (!spImg)
        return -1.0;

    float meanIntensity = 0.0;

    //1) region iterator, set 0 for all pixels outside the circle and below the table top, based on physical position
    USHORT_ImageType::PointType origin = spImg->GetOrigin();
    USHORT_ImageType::SpacingType spacing = spImg->GetSpacing();
    USHORT_ImageType::SizeType size = spImg->GetBufferedRegion().GetSize();

    itk::ImageSliceIteratorWithIndex<USHORT_ImageType> it(spImg, spImg->GetRequestedRegion());
    USHORT_ImageType::SizeType imgSize = spImg->GetRequestedRegion().GetSize(); //1016x1016 x z

    int width = imgSize[0];
    int height = imgSize[1];

    it.SetFirstDirection(0); //x?
    it.SetSecondDirection(1); //y?
    it.GoToBegin();

    int iNumSlice = 0;
    int iPosX = 0;
    int iPosY = 0;

    int i = 0;//height
    int j = 0; // width

    double crntPhysX = 0.0;
    double crntPhysY = 0.0;
    double crntPhysZ = 0.0;

    double pixSum = 0.0;
    int iCnt = 0;

    while (!it.IsAtEnd())
    {
        iPosY = 0;
        while (!it.IsAtEndOfSlice())
        {
            iPosX = 0;
            while (!it.IsAtEndOfLine())
            {
                //Calculate physical position

                crntPhysX = iPosX*(double)spacing[0] + (double)origin[0];
                crntPhysY = iPosY*(double)spacing[1] + (double)origin[1];
                crntPhysZ = iNumSlice*(double)spacing[2] + (double)origin[2];

                if (pow(crntPhysX, 2.0) + pow(crntPhysY, 2.0) + pow(crntPhysZ, 2.0) < pow(sphereR, 2.0))
                {
                    pixSum = pixSum + (double)(it.Get());
                    iCnt++;
                }
                ++it;
                iPosX++;
            }
            it.NextLine();
            iPosY++;
        }
        it.NextSlice();
        iNumSlice++;
    }

    if (iCnt > 0)
        meanIntensity = pixSum / (double)iCnt;
    else
        meanIntensity = -1.0;


    if (sdIntensity == NULL)
        return meanIntensity;


    double devSum = 0.0;
    it.GoToBegin();

    iNumSlice = 0;
    iPosX = 0;
    iPosY = 0;

    i = 0;//height
    j = 0; // width

    crntPhysX = 0.0;
    crntPhysY = 0.0;
    crntPhysZ = 0.0;

    while (!it.IsAtEnd())
    {
        iPosY = 0;
        while (!it.IsAtEndOfSlice())
        {
            iPosX = 0;
            while (!it.IsAtEndOfLine())
            {
                //Calculate physical position

                crntPhysX = iPosX*(double)spacing[0] + (double)origin[0];
                crntPhysY = iPosY*(double)spacing[1] + (double)origin[1];
                crntPhysZ = iNumSlice*(double)spacing[2] + (double)origin[2];

                if (pow(crntPhysX, 2.0) + pow(crntPhysY, 2.0) + pow(crntPhysZ, 2.0) < pow(sphereR, 2.0))
                {
                    devSum = devSum + pow(((double)(it.Get()) - meanIntensity), 2.0);
                }
                ++it;
                iPosX++;
            }
            it.NextLine();
            iPosY++;
        }
        it.NextSlice();
        iNumSlice++;
    }

    if (iCnt > 0)
    {
        *sdIntensity = sqrt(devSum / (double)iCnt);
    }
    else
        *sdIntensity = -1.0;

    return meanIntensity;
}

void CbctRecon::AddConstHU(USHORT_ImageType::Pointer& spImg, int HUval)
{

    typedef itk::ImageRegionIteratorWithIndex<USHORT_ImageType> iteratorType;
    iteratorType it(spImg, spImg->GetRequestedRegion());

    it.GoToBegin();

    int crntVal = 0;
    int newVal = 0;

    while (!it.IsAtEnd())
    {
        crntVal = (int)(it.Get());

        newVal = HUval + crntVal;

        if (newVal <= 0)
            newVal = 0;

        if (newVal >= 4095)
            newVal = 4095;

        it.Set((unsigned short)newVal);
        ++it;
    }
}



void CbctRecon::SLT_OpenPhaseData()
{
	if (!m_vPhaseFloat.empty())
		m_vPhaseFloat.clear();
	
		//Open file
	QString filePath = QFileDialog::getOpenFileName(this, "Open phase text", m_strPathDirDefault, "Phase text file (*.txt)", 0, 0);
	
	if (filePath.length() < 1)
		return;
	
	
	
	ifstream fin;
	fin.open(filePath.toLocal8Bit().constData(), ios::in);
	if (fin.fail())
		return;
	
	ui.lineEdit_PhaseTxtPath->setText(filePath);
	
	char str[MAX_LINE_LENGTH];
	
	
	float tmpPhase = 0.0;
	
	float phaseSum = 0.0;
	int phaseCnt = 0;
	while (!fin.eof())
	{
		memset(str, 0, MAX_LINE_LENGTH);
		fin.getline(str, MAX_LINE_LENGTH);
		QString strLine(str);
		
		if (strLine.length() < 1)
			break;
		
		tmpPhase = strLine.toFloat();
		m_vPhaseFloat.push_back(tmpPhase);
		phaseCnt++;
		phaseSum = phaseSum + tmpPhase;
	}
	fin.close();
	cout << "NumOfPhaseData[Float]= " << phaseCnt << "  Mean Phase value= " << phaseSum / (double)phaseCnt << endl;
}

void CbctRecon::SLT_Export4DCBCT()
{ 
	if (!m_spCustomGeometry)
	{
		cout << "Error! no Geometry information loaded yet" << endl;
		return;
	
	}
	
	int NumOfGanAngle = m_spCustomGeometry->GetGantryAngles().size();
	int NumOfPhase = m_vPhaseFloat.size();
	
	if (NumOfGanAngle != NumOfPhase)
	{
		cout << "Size not matched. NumOfProjection= " << NumOfGanAngle << " NumOfProjection= " << NumOfPhase << endl;
		return;
	}
	    //build phase bins
	QString strPhaseTextFull = ui.lineEdit_PhaseExportString->text();
	QStringList strlistPhaseFull = strPhaseTextFull.split(";");
	
	int cntGroup = strlistPhaseFull.count();
	
	vector<int> vPhaseBinsSelected;
	    //m_strPatientDirName = tmpDir_PatientFolder.dirName();
		    //m_strPathPatientDir = tmpDir_PatientFolder.absolutePath();
		
		    //QDir tmpDir_RootFolder(movingDir.absolutePath()); //root folder
		
		    //if (tmpDir_RootFolder.absolutePath().length() > 1)
		    //    m_strPathDirDefault = tmpDir_RootFolder.absolutePath();
		
		    ////option 1: already made rtk xml file
		    //QString tmpPathRTKGeometry = tmpDir_RootFolder.absolutePath() + "/" + "ElektaGeom_" + m_strDCMUID + ".xml";
		    //QFileInfo rtkGeomInfo(tmpPathRTKGeometry);
	
	QString strDirForXML = m_strPathDirDefault; //where xml file is located
	//QString strUID ;//P00102030P + m_strDCMUID
	QString strDirForProj = m_strPathIMAGES;
	
	for (int i = 0; i < cntGroup; i++)
	{
		        //for a single group
		QStringList strListGroup = strlistPhaseFull.at(i).split(",");
		int iPhaseCnt = strListGroup.count();
		vPhaseBinsSelected.clear();
		
		for (int j = 0; j < iPhaseCnt; j++)
		{
			vPhaseBinsSelected.push_back(strListGroup.at(j).toInt());
		}
		        //m_vSelectedFileNames: full file paths of projections
		        //m_spCustomGeometry: full information
		        //m_vPhaseFloat: full data of phase
		
		        //Create Dir, xml, etc
		if (!ResortCBCTProjection(vPhaseBinsSelected, strDirForXML, strDirForProj, m_strDCMUID, m_vPhaseFloat, m_spCustomGeometry, m_vSelectedFileNames))
		{
			cout << "Error in ResortCBCTProjection " << strlistPhaseFull.at(i).toLocal8Bit().constData() << endl;
			return;
		}
	}
			
		    //mkdir
		    //QString strCrntDir = m_strPathPatientDir + "/" + "IMAGES"; //current Proj folder    
		    //QString strCrntDir = ui.lineEdit_HisDirPath->text();
		
		    ////Make a sub directory
		    //QDir crntDir(strCrntDir);
		
		    //if (!crntDir.exists())
		    //{
		    //    cout << "File save error: The specified folder does not exist." << endl;
		    //    return;
		    //}
		
		    ////QString fwdDirName = "fwd_" + m_strDCMUID;
		
		    //QString str4D = "4DCBCT";
		    //bool tmpResult = crntDir.mkdir(str4D);
		
		    //QString strSubDir = strCrntDir + "/" + str4D;
		    //QDir crntSubDir(strSubDir);
		    //if (!crntSubDir.exists())
		    //{
		    //    cout << "File save error" << endl;
		    //    return;
		    //}
		
		    //crntSubDir.mkdir();
}

bool CbctRecon::ResortCBCTProjection(vector<int>& vIntPhaseBinSelected, QString& strPathForXML, QString& strPathProjRoot, QString& strUID, vector<float>& vFloatPhaseFull, GeometryType::Pointer& spGeomFull, vector<string>& vProjPathsFull)
{
	if (vIntPhaseBinSelected.empty())
		return false;
	
	int NumOfPhaseFull = vFloatPhaseFull.size();
	int NumOfGeomFull = spGeomFull->GetGantryAngles().size();
	int NumOfProjFileFull = vProjPathsFull.size();
	
	if (NumOfPhaseFull != NumOfGeomFull || NumOfGeomFull != NumOfProjFileFull)
	{
		cout << "Num of data is not matching:" << " NumOfPhaseFull= " << NumOfPhaseFull << " NumOfGeomFull= " << NumOfGeomFull << " NumOfProjFileFull= " << NumOfProjFileFull << endl;
		return false;
	}
	
	    //Check Root dir is set
	
	if (strUID.length() < 1)
		return false;
	
	QDir dirSaveXML(strPathForXML);
	QDir dirSaveProj(strPathProjRoot);
	
	if (!dirSaveXML.exists() || !dirSaveProj.exists())
	{
		cout << "Error! Directories don't exist" << endl;
		return false;
	}
	    //Generate a new UID
	QString strUID_Endfix;
	int iNumOfSelPhase = vIntPhaseBinSelected.size();
	
	strUID_Endfix = "P";
	for (int i = 0; i < iNumOfSelPhase; i++)
	{
		QString strNum;
		strNum = strNum.sprintf("%02d", vIntPhaseBinSelected.at(i));
		strUID_Endfix = strUID_Endfix + strNum;
	}
	strUID_Endfix = strUID_Endfix + "P"; //UID...P00102030405060P
	QString strNewUID = strUID + strUID_Endfix;
	
	    //Create a subDir
	QDir curProjRoot(strPathProjRoot);
	QString strSubDirName = "img_" + strNewUID;
	curProjRoot.mkdir(strSubDirName);
	
	QString strPathProj = strPathProjRoot + "/" + strSubDirName;
	
	QDir projDir(strPathProj);
	if (!projDir.exists())
	{
		cout << "no Proj Dir exists" << endl;
		return false;
	}
	
		QDir xmlDir(strPathForXML);
	if (!xmlDir.exists())
	{
		cout << "no XML Dir exists" << endl;
		return false;
	}
	
	    //strPathProj
	    //strPathForXML
	vector<int> vSelectedIdxTemp;
	vector<int> vSelectedIdxFin;
	
	for (int i = 0; i < iNumOfSelPhase; i++)
	{
		AppendInPhaseIndex(vIntPhaseBinSelected.at(i), vFloatPhaseFull, vSelectedIdxTemp);
	}
	//Remove redandancy
	
	sort(vSelectedIdxTemp.begin(), vSelectedIdxTemp.end()); //hopefully, ascending
	cout << "sorting check" << endl;
	cout << "0 " << vSelectedIdxTemp.at(0) << endl;
	cout << "1 " << vSelectedIdxTemp.at(1) << endl;
	
	vector<int>::iterator it;
	
	int prevVal = -1;
	for (it = vSelectedIdxTemp.begin(); it != vSelectedIdxTemp.end(); ++it)
	{
		if ((*it) > prevVal)
		{
			vSelectedIdxFin.push_back(*it);
		}
		
		
		prevVal = (*it);
	}
	
	GeometryType::Pointer spSubGeometry = GeometryType::New();
	
	vector<int>::iterator itIdx;

	for (itIdx = vSelectedIdxFin.begin(); itIdx != vSelectedIdxFin.end(); itIdx++)
	{
		cout << "cur Idx=" << (*itIdx) << endl;
		        //9 parameters are required
		double curSID = spGeomFull->GetSourceToIsocenterDistances().at(*itIdx);
		double curSDD = spGeomFull->GetSourceToDetectorDistances().at(*itIdx);
		double curGantryAngle = spGeomFull->GetGantryAngles().at(*itIdx) * 360. / (2.*itk::Math::pi);
		
		double curProjOffsetX = spGeomFull->GetProjectionOffsetsX().at(*itIdx);
		double curProjOffsetY = spGeomFull->GetProjectionOffsetsY().at(*itIdx);
		
		double curOutOfPlaneAngles = spGeomFull->GetOutOfPlaneAngles().at(*itIdx);
		double curInPlaneAngles = spGeomFull->GetInPlaneAngles().at(*itIdx);
		
		double curSrcOffsetX = spGeomFull->GetSourceOffsetsX().at(*itIdx);
		double curSrcOffsetY = spGeomFull->GetSourceOffsetsY().at(*itIdx);
		
		spSubGeometry->AddProjection(curSID, curSDD, curGantryAngle,
		curProjOffsetX, curProjOffsetY, //Flexmap 
		curOutOfPlaneAngles, curInPlaneAngles, //In elekta, these are 0
		curSrcOffsetX, curSrcOffsetY); //In elekta, these are 0
	}
	//Export spSubGeometry
	rtk::ThreeDCircularProjectionGeometryXMLFileWriter::Pointer xmlWriter =
	rtk::ThreeDCircularProjectionGeometryXMLFileWriter::New();
	
	QString geomFileName = "ElektaGeom_" + strNewUID + ".xml";
	QString geomFilePath = strPathForXML + "/" + geomFileName;
	
	xmlWriter->SetFilename(geomFilePath.toLocal8Bit().constData());
	xmlWriter->SetObject(spSubGeometry);
	TRY_AND_EXIT_ON_ITK_EXCEPTION(xmlWriter->WriteFile());
	//Copy selected his files to a different folder
	
	for (itIdx = vSelectedIdxFin.begin(); itIdx != vSelectedIdxFin.end(); itIdx++)
	{
		QString strPathProjOriginal = vProjPathsFull.at(*itIdx).c_str();
		//Copy this file to target dir
		
		QFileInfo fInfo(strPathProjOriginal);
		QString strPathProjNew = strPathProj + "/" + fInfo.fileName();
		QFile::copy(fInfo.absoluteFilePath(), strPathProjNew);
	}
	
	//    vector<float>& vFloatPhaseFull, GeometryType::Pointer& spGeomFull, vector<string>& vProjPathsFull       
	cout << vSelectedIdxFin.size() << " files were copied." << endl;
	
	return true;
}

void CbctRecon::AppendInPhaseIndex(int iPhase, vector<float>& vFloatPhaseFull, vector<int>& vOutputIndex, int margin)
{
	
	int iNumOfPhase = vFloatPhaseFull.size();
	
	int iCurPhase = 0;
	
	int startPhase1;
	int endPhase1;
	
	int startPhase2;
	int endPhase2;
	
	for (int i = 0; i < iNumOfPhase; i++)
	{
		iCurPhase = qRound(vFloatPhaseFull.at(i)*100.0);
		//determine wether it is within the range 
		
		if (iPhase < margin) //if 5 --> 0 ~ 10%, IF 4--> 99 ~ 09
		{
			startPhase2 = iPhase + 100 - margin;
			endPhase2 = 100;
			
			startPhase1 = 0;
			endPhase1 = iPhase + margin;
		}
		else
		{
			startPhase1 = iPhase - margin;
			endPhase1 = iPhase + margin;
			
			startPhase2 = 1; //reverse
			endPhase2 = 0;
			}

			if ((iCurPhase >= startPhase1 && iCurPhase <= endPhase1) ||
			(iCurPhase >= startPhase2 && iCurPhase <= endPhase2))
			{
			vOutputIndex.push_back(i);
		}
	}
}

//
//void CbctRecon::cudaMedianFilter2DITK(OutputImageType2D::Pointer& spFloatImage2D, int wndSizeX, int wndSizeY)
//{
//	cout << "cudaMedianFilter2DITK is being used" << endl;
//
//	OutputImageType2D::SizeType sizeSrc = spFloatImage2D->GetBufferedRegion().GetSize();
//	OutputImageType2D::IndexType startSrc = spFloatImage2D->GetBufferedRegion().GetIndex();
//	OutputImageType2D::SpacingType spacingSrc = spFloatImage2D->GetSpacing();
//	OutputImageType2D::PointType originSrc = spFloatImage2D->GetOrigin();
//
//	int imgWidth = (int)(sizeSrc[0]);
//	int imgHeight = (int)(sizeSrc[1]);
//	//1) make a 1D array buffer and copy image from itk to this buff.
//	float* pCpuImgInput = new float[imgWidth*imgHeight];
//	float* pCpuImgOutput = new float[imgWidth*imgHeight];
//
//	itk::ImageRegionConstIterator<OutputImageType2D> itSrc(spFloatImage2D, spFloatImage2D->GetRequestedRegion()); //writing
//
//	//itSrc.GoToBegin();
//
//	int arrIdx = 0;
//	for (itSrc.GoToBegin(); !itSrc.IsAtEnd(); itSrc++)
//	{
//		pCpuImgInput [arrIdx]= itSrc.Get();
//		arrIdx++;
//	}
//
//	//Run CUDA median filter at .cu file using 1) as input
//
//	//C Extern function
//	YKCudaMedianWrapFloat(pCpuImgInput, pCpuImgOutput, imgWidth, imgHeight, wndSizeX, wndSizeY);
//
//	//output result is copied to the original ITK image
//	itk::ImageRegionIterator<OutputImageType2D> itTarg(spFloatImage2D, spFloatImage2D->GetRequestedRegion()); //writing
//
//	itTarg.GoToBegin();
//
//	arrIdx = 0;
//	//while (!itSrc.IsAtEnd() && !itTarg.IsAtEnd())
//	for (itTarg.GoToBegin(); !itTarg.IsAtEnd(); itTarg++)
//	{
//		itTarg.Set(pCpuImgOutput[arrIdx]);
//		arrIdx++;
//	}
//
//	delete[] pCpuImgInput;
//	delete[] pCpuImgOutput;
//}

//double CbctRecon::CropSkinUsingRS(USHORT_ImageType::Pointer& spImgUshort, QString& strPathRS, double cropMargin )
//{
//   //if (m_pParent->m_strPathRS.isEmpty())
//	//return;
//  /* End of [1]Segment air region*/
//
//  //plastimatch convert --input E:\PlastimatchData\DicomEg\OLD\RS.dcm --output-ss-img E:\PlastimatchData\DicomEg\OLD\ssimg_all2.mha --output-ss-list E:\PlastimatchData\DicomEg\OLD\sslist_all.txt --referenced-ct E:\PlastimatchData\DicomEg\OLD\CT
//
//  //plm_clp_parse (&parms, &parse_fn, &usage_fn, argc, argv, 1)
//  //plastimatch segment --input E:\PlastimatchData\DicomEg\OLD\CT --output-img E:\PlastimatchData\DicomEg\OLD\msk_bubbles_oldCT.mha --lower-threshold -600
//
//  //do_command_warp(argc, argv);
//
//
//
//
//}





//TIF export
void InsertHeaderToArray(TIFIFD* IFDArr, int insertIdx,
						 int TagID, int dataType, int DataCnt, int dataVal)//dataVal이 초기값이면 insert 안함
{
	IFDArr[insertIdx].TagID = TagID;
	IFDArr[insertIdx].DataType = dataType;
	IFDArr[insertIdx].DataCnt = DataCnt;
	IFDArr[insertIdx].DataOrOffset = dataVal;
	return;
}



bool SaveDoseGrayImage(const char* filePath, int width, int height, double spacingX, double spacingY, double originLeft_mm, double originTop_mm, unsigned short* pData) //export dose array to a specified file (16bit TIF)
{
	//Global variables
	long m_iSubFileType = 0;
	short m_iWidth = width;
	short m_iHeight = height;
	short m_iBitsPerSample  = 16;
	short m_iCompression = 1;
	short m_iPhotometric = 0;
	long m_iStripOffset = 1024;
	short m_iSamplePerPixel = 1;
	long m_iRowsPerStrip = height;
	long m_iStripByteCnts = qRound(width*height*2.0);

	short m_iResolUnit = 2;
	short m_iPgNum = 0;//or 1?
	short m_iMinSampleVal = 0;
	short m_iMaxSampleVal = 65535; //old: 255

	RATIONAL m_rXResol;//spacingX in dpi
	RATIONAL m_rYResol;//spacingY

	RATIONAL m_rXPos;
	RATIONAL m_rYPos;

	m_rXResol.b = 10000000;
	m_rYResol.b = 10000000;

	m_rXResol.a = (long)qRound(1/spacingX *25.4*m_rXResol.b) ; //dpi
	m_rYResol.a = (long)qRound(1/spacingY *25.4*m_rYResol.b);

	int m_iNextOffset = 0;

	if (pData == NULL)
		return false;

	//Set Center
	QPoint dataPt;
	dataPt.setX(qRound(m_iWidth / 2.0));
	dataPt.setY(qRound(m_iHeight / 2.0));

	m_rXPos.b = 10000000;
	m_rYPos.b = 10000000;

	//double fLeftPosMM = -dataPt.x()*spacingX;
	//double fTopPosMM = dataPt.y()*spacingY;
	double fLeftPosMM = originLeft_mm;
	double fTopPosMM = -originTop_mm;

	m_rXPos.a = (long)(qRound(fLeftPosMM / 25.4 * m_rXPos.b));
	m_rYPos.a = (long)(qRound(fTopPosMM / 25.4 * m_rYPos.b));
	//Set Center

	FILE* fd = NULL;

	fd = fopen(filePath, "wb");

	long MarkerUpper;
	long MarkerLower;

	MarkerUpper = 0x002A4949;
	MarkerLower = 0x00000008;

	fwrite(&MarkerUpper, sizeof(long), 1, fd); //4
	fwrite(&MarkerLower, sizeof(long), 1, fd); //8

	//int IFDSize = GetValidIFDCnt();
	int IFDSize = 18;

	fwrite(&IFDSize, sizeof(unsigned short), 1, fd); //10

	TIFIFD* IFDArr = new TIFIFD [IFDSize];

	int offsetX;
	int offsetY;

	int idx =0;
	int TagID = 0;
	int dataType = 0;
	int DataCnt = 0;
	int dataVal = 0;

	if (m_iSubFileType >= 0)
	{
		TagID = 254;
		dataType = 3;
		DataCnt = 1;
		dataVal = m_iSubFileType;
		InsertHeaderToArray(IFDArr, idx, TagID, dataType, DataCnt, dataVal);//dataVal이 초기값이면 insert 안함
		idx++;
	}

	if (m_iWidth >=0)
	{
		TagID = 256;
		dataType = 3;
		DataCnt = 1;
		dataVal = m_iWidth;
		InsertHeaderToArray(IFDArr, idx, TagID, dataType, DataCnt, dataVal);//dataVal이 초기값이면 insert 안함
		idx++;
	}
	if (m_iHeight >= 0)
	{
		TagID = 257;
		dataType = 3;
		DataCnt = 1;
		dataVal = m_iHeight;
		InsertHeaderToArray(IFDArr, idx, TagID, dataType, DataCnt, dataVal);//dataVal이 초기값이면 insert 안함
		idx++;
	}
	if (m_iBitsPerSample >= 0)
	{
		TagID = 258;
		dataType = 3;
		DataCnt = 1;
		dataVal = m_iBitsPerSample;
		//dataVal = 16;
		InsertHeaderToArray(IFDArr, idx, TagID, dataType, DataCnt, dataVal);//dataVal이 초기값이면 insert 안함
		idx++;
	}
	if (m_iCompression >= 0)
	{
		TagID = 259;
		dataType = 3;
		DataCnt = 1;
		dataVal = m_iCompression;
		InsertHeaderToArray(IFDArr, idx, TagID, dataType, DataCnt, dataVal);//dataVal이 초기값이면 insert 안함
		idx++;
	}
	if (m_iPhotometric >= 0)
	{
		TagID = 262;
		dataType = 3;
		DataCnt = 1;
		dataVal = m_iPhotometric; //1로 강제 지정
		//dataVal = 0; //0으로 강제 지정
		InsertHeaderToArray(IFDArr, idx, TagID, dataType, DataCnt, dataVal);//dataVal이 초기값이면 insert 안함
		idx++;
	}
	if (m_iStripOffset >= 0)
	{
		TagID = 273;
		dataType = 4;
		DataCnt = 1;
		//dataVal = 1024;
		dataVal = m_iStripOffset;
		InsertHeaderToArray(IFDArr, idx, TagID, dataType, DataCnt, dataVal);//dataVal이 초기값이면 insert 안함
		idx++;
	}
	if (m_iSamplePerPixel >= 0)
	{
		TagID = 277;
		dataType = 3;
		DataCnt = 1;
		dataVal = m_iSamplePerPixel;

		//1로강제지정
		//dataVal = 1;
		InsertHeaderToArray(IFDArr, idx, TagID, dataType, DataCnt, dataVal);//dataVal이 초기값이면 insert 안함
		idx++;
	}
	if (m_iRowsPerStrip >= 0)
	{
		TagID = 278;
		dataType = 3;
		DataCnt = 1;
		dataVal = m_iRowsPerStrip;
		InsertHeaderToArray(IFDArr, idx, TagID, dataType, DataCnt, dataVal);//dataVal이 초기값이면 insert 안함
		idx++;
	}
	if (m_iStripByteCnts >= 0)
	{
		TagID = 279;
		dataType = 4;
		DataCnt = 1;
		dataVal = m_iStripByteCnts;
		/*if (m_iSamplePerPixel == 1)
		dataVal = m_iStripByteCnts;
		else if (m_iSamplePerPixel == 3)
		dataVal = (int)(m_iStripByteCnts/3.0);
		*/
		InsertHeaderToArray(IFDArr, idx, TagID, dataType, DataCnt, dataVal);//dataVal이 초기값이면 insert 안함
		idx++;
	}
	if (m_rXResol.a != 0)
	{
		offsetX = 0;
		offsetX = 8+2+(12* IFDSize)+4;

		TagID = 282;
		dataType = 5;
		DataCnt = 1;
		dataVal = offsetX;//maximum
		InsertHeaderToArray(IFDArr, idx, TagID, dataType, DataCnt, dataVal);//dataVal이 초기값이면 insert 안함
		idx++;
	}
	if (m_rYResol.a != 0)
	{
		offsetY = 0;
		offsetY = 8+2+(12* IFDSize)+4+8;

		TagID = 283;
		dataType = 5;
		DataCnt = 1;
		dataVal = offsetY;
		InsertHeaderToArray(IFDArr, idx, TagID, dataType, DataCnt, dataVal);//dataVal이 초기값이면 insert 안함
		idx++;
	}

	//IFDSize 단위 데이터 몇개인지 나타냄
	//20111226추가 //center를 표시
	if (m_rXPos.a != 0)
	{
		offsetX = 0;
		offsetX = 8+2+(12* IFDSize)+4+8+8;

		TagID = 286;
		dataType = 5;
		DataCnt = 1;
		dataVal = offsetX;//maximum
		InsertHeaderToArray(IFDArr, idx, TagID, dataType, DataCnt, dataVal);//dataVal이 초기값이면 insert 안함
		idx++;
	}
	if (m_rYPos.a != 0)
	{
		offsetY = 0;
		offsetY = 8+2+(12* IFDSize)+4+8+8+8;

		TagID = 287;
		dataType = 5;
		DataCnt = 1;
		dataVal = offsetY;
		InsertHeaderToArray(IFDArr, idx, TagID, dataType, DataCnt, dataVal);//dataVal이 초기값이면 insert 안함
		idx++;
	}

	//////
	if (m_iMinSampleVal >= 0)
	{
		TagID = 280;
		dataType = 3;
		DataCnt = 1;
		dataVal = m_iMinSampleVal;
		InsertHeaderToArray(IFDArr, idx, TagID, dataType, DataCnt, dataVal);//dataVal이 초기값이면 insert 안함
		idx++;
	}
	if (m_iMaxSampleVal >= 0)
	{
		TagID = 281;
		dataType = 3;
		DataCnt = 1;
		dataVal = m_iMaxSampleVal;
		InsertHeaderToArray(IFDArr, idx, TagID, dataType, DataCnt, dataVal);//dataVal이 초기값이면 insert 안함
		idx++;
	}
	if (m_iResolUnit >= 0)
	{
		TagID = 296;
		dataType = 3;
		DataCnt = 1;
		dataVal = m_iResolUnit;
		InsertHeaderToArray(IFDArr, idx, TagID, dataType, DataCnt, dataVal);//dataVal이 초기값이면 insert 안함
		idx++;
	}
	if (m_iPgNum >= 0)
	{
		TagID = 297;
		dataType = 3;
		DataCnt = 2;
		dataVal = m_iPgNum;
		InsertHeaderToArray(IFDArr, idx, TagID, dataType, DataCnt, dataVal);//dataVal이 초기값이면 insert 안함
		idx++;
	}

	for (int i = 0 ; i<IFDSize ; i++)
	{
		fwrite(&IFDArr[i], sizeof(TIFIFD), 1, fd);
	}
	fwrite(&m_iNextOffset, 4, 1, fd);

	fwrite(&m_rXResol, 8, 1, fd);
	fwrite(&m_rYResol, 8, 1, fd);

	fwrite(&m_rXPos, 8, 10, fd);
	fwrite(&m_rYPos, 8, 1, fd);

	int iDummySize = 0;
	iDummySize = 1024 - (offsetY+8);

	//char tmpDummy [802]; // 1024 -222

	char* tmpDummy = new char [iDummySize];
	memset(tmpDummy, 0, iDummySize);
	fwrite(tmpDummy, iDummySize, 1, fd); //`까지 0으로 채움

	delete [] tmpDummy;
	delete [] IFDArr;

	int imgSize = m_iWidth*m_iHeight;
	//fwrite(m_pImage, imgSize, 1, fd);


	//쓰기용 버퍼 생성
	unsigned short* writeBuf = new unsigned short [imgSize];

	//for (int i = 0 ; i<imgSize ; i++)
	//{
	//	//fread(&m_pImage[i], 2, 1, fd);
	//	if (pData[i] < 0)
	//		writeBuf[i] = 0;
	//	else if  (pData[i] > 65535)
	//		writeBuf[i] = 65535;
	//	else
	//		writeBuf[i] = pData[i];  //gray 이미지를 건드는 것이다!
	//}

	for (int i = 0 ; i<imgSize ; i++)
	{
		fwrite(&pData[i], 2, 1, fd);
	}
	fclose(fd);

	return true;
}



double vectorMean(const vector<double>& vDouble)
{
	int size = vDouble.size();
	if (size <= 0)
		return 0.0;

	double sum = 0.0;
	vector<double>::const_iterator it;

	for (it = vDouble.begin() ; it != vDouble.end() ; it++)
	{
		sum = sum + (*it);
	}

	return sum / (double)size;
}

double vectorSum(const vector<double>& vDouble)
{
	int size = vDouble.size();
	if (size <= 0)
		return 0.0;

	double sum = 0.0;
	vector<double>::const_iterator it;

	for (it = vDouble.begin() ; it != vDouble.end() ; it++)
	{
		sum = sum + (*it);
	}

	return sum;
}

//Projection image Median filtering using CUDA

