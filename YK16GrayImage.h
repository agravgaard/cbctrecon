#pragma once

//v20130830 : his header buffer, itk compatible

class QPixmap;
class QLabel;
class QPainter;
//class QImage;

#define DEFAULT_WINLEVEL_MID 10000
#define DEFAULT_WINLEVEL_WIDTH 20000

#define DEFAULT_ELEKTA_HIS_HEADER_SIZE 100 // ELEKTA VS VARIAN

#include <rtkConfiguration.h>

#ifdef CUDA_FOUND
#include "itkCudaImage.h"
#else
#include "itkImage.h"
#endif

#include <QImage>
#include <vector>
#include <QVector>

struct BADPIXELMAP{
	int BadPixX;
	int BadPixY;
	int ReplPixX;
	int ReplPixY;
};

enum enProfileDirection{
	DIRECTION_HOR = 0,
	DIRECTION_VER,	
};

enum enSplitOption{
    PRI_LEFT_TOP = 0, //Primary Left Top
    PRI_RIGHT_TOP, //Primary Left Top
    PRI_LEFT,	
    PRI_RIGHT,	
    PRI_TOP,	
    PRI_BOTTOM,	
};


#ifdef CUDA_FOUND
typedef itk::CudaImage<unsigned short, 2> UnsignedShortCUDAImageType;
typedef itk::CudaImage<float, 2> FloatCUDAImageType2D;
#endif

typedef itk::Image<unsigned short, 2> UnsignedShortImageType;
typedef itk::Image<float, 2> FloatImageType2D;

using namespace std;

class YK16GrayImage
{	
public:
	YK16GrayImage(void);
	YK16GrayImage(int width, int height);
	~YK16GrayImage(void);

	int m_iWidth;
	int m_iHeight;
	//added: 20140206
	double m_fSpacingX; //[mm/px]
	double m_fSpacingY;

	unsigned short* m_pData; // 0 - 65535

	QPixmap* m_pPixmap; //Actually, no need!
	QImage m_QImage;
	//QPainter* m_pPainter;

	bool LoadRawImage(const char *filePath, int width, int height);
	bool CopyFromBuffer(unsigned short* pImageBuf, int width, int height);
	bool CloneImage(YK16GrayImage& other);

	bool CreateImage(int width, int height, unsigned short usVal);

	bool FillPixMap(int winMid, int winWidth);
	bool FillPixMapMinMax(int winMin, int winMax); //0-65535 Сп window level

	bool FillPixMapDual(int winMid1, int winMid2,int winWidth1, int winWidth2);
	bool FillPixMapMinMaxDual(int winMin1, int winMin2, int winMax1, int winMax2); //0-65535 Сп window level



	bool SaveDataAsRaw (const char *filePath);
	//bool DrawToLabel(QLabel* lbDisplay);

	bool IsEmpty();
	bool ReleaseBuffer();

	//bool CalcImageInfo (double& meanVal, double& STDV, double& minVal, double& maxVal);
	bool CalcImageInfo ();
	double CalcAveragePixelDiff(YK16GrayImage& other);

	bool DoPixelReplacement(std::vector<BADPIXELMAP>& vPixelMapping); //based on pixel mapping information, some bad pixels will be replaced with median pixel value near by

	static void CopyYKImage2ItkImage(YK16GrayImage* pYKImage, UnsignedShortImageType::Pointer& spTarImage);
	static void CopyItkImage2YKImage(UnsignedShortImageType::Pointer& spSrcImage, YK16GrayImage* pYKImage);

	QString m_strFilePath;

	double m_fPixelMean;
	double m_fPixelSD;
	double m_fPixelMin;
	double m_fPixelMax;

	static void Swap(YK16GrayImage* pImgA, YK16GrayImage* pImgB);

	QRect m_rtROI;
	bool setROI(int left, int top, int right, int bottom); //if there is error, go to default: entire image
	bool CalcImageInfo_ROI();
	double m_fPixelMean_ROI;
	double m_fPixelSD_ROI;
	double m_fPixelMin_ROI;
	double m_fPixelMax_ROI;
	bool m_bDrawROI;

	void DrawROIOn(bool bROI_Draw); //only rectangle


	//Elekta CBCT recon
	char* m_pElektaHisHeader;// VARIAN header isn't used for anything relevant
	void CopyHisHeader(const char *hisFilePath);
	//bool SaveDataAsHis (const char *filePath);
	bool SaveDataAsHis( const char *filePath, bool bInverse );
	bool m_bShowInvert;

	void MultiplyConstant(double multiplyFactor);

	void SetSpacing(double spacingX, double spacingY)
	{
		m_fSpacingX = spacingX;
		m_fSpacingY = spacingY;
	};

	QPoint m_ptProfileProbe; //Mouse Clicked Position --> Data
	bool m_bDrawProfileX;
	bool m_bDrawProfileY;

	QPoint m_ptFOVCenter; // data pos
	int m_iFOVRadius;//data pos (pixel)
	bool m_bDrawFOVCircle;

	int m_iTableTopPos;//data pos
	bool m_bDrawTableLine;

	QPoint m_ptCrosshair; //data position
	bool m_bDrawCrosshair;


	////ZOOM and PAN function. Using these information below, prepare the m_QImage for displaying 
	//in qlabel in FillPixMap function
	int m_iOffsetX; //for Pan function.. this is data based offset
	int m_iOffsetY;
	void SetOffset(int offsetX, int offsetY){m_iOffsetX = offsetX; m_iOffsetY = offsetY;}
	double m_fZoom;
	void SetZoom(double fZoom);


	//SPLIT VIEW
	QPoint m_ptSplitCenter; //Fixed image with Moving image. center is based on dataPt.//Fixed Image: Left Top + Right Bottom, Moving: Right Top + Left Bottom        
	int m_enSplitOption;
	//This cetner is moved while Left Dragging //All split and crosshair are data point based!
	void SetSplitOption(enSplitOption option) {m_enSplitOption = option;}
	void SetSplitCenter(QPoint& ptSplitCenter);	//From mouse event, data point	
	//void SetSplitCenter(int centerX, int centerY) {m_ptSplitCenter.setX(centerX); m_ptSplitCenter.setY(centerY);}//From mouse event, data point
	bool ConstituteFromTwo(YK16GrayImage& YKImg1,YK16GrayImage& YKImg2); //YKImg1 and two should be in exactly same dimension and spacing
	bool isPtInFirstImage(int dataX, int dataY);

	void SetProfileProbePos(int dataX, int dataY);
	unsigned short GetProfileProbePixelVal();	
	void GetProfileData(int dataX, int dataY, QVector<double>& vTarget, enProfileDirection direction); 
	void GetProfileData(QVector<double>& vTarget, enProfileDirection direction);

	void EditImage_Flip();
	void EditImage_Mirror();


	void MedianFilter(int iMedianSizeX, int iMedianSizeY);

	double m_fResampleFactor;//if it is not the 1.0, the data is already resampled.	

	UnsignedShortImageType::Pointer CloneItkImage();
	void ResampleImage(double fResampleFactor);

	void UpdateFromItkImage(UnsignedShortImageType::Pointer& spRefItkImg);
	void UpdateFromItkImageFloat(FloatImageType2D::Pointer& spRefItkImg);

	void InvertImage();

	//will be added later
	/*void EditImage_CW90();
	void EditImage_CCW90();
	void EditImage_Rotation(double angle);*/


        

	//should be implemented later

	//Flip
	//mirror

};
