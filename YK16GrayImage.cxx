#include "YK16GrayImage.h"
#include <QPixmap>
#include <fstream>
#include <iostream>
#include <QLabel>
#include <math.h>
#include "itkImageRegionIterator.h"
#include "itkMedianImageFilter.h"
#include "itkIdentityTransform.h"
#include "itkResampleImageFilter.h"
#include "itkAffineTransform.h"
#include "itkNearestNeighborInterpolateImageFunction.h"
#include <QPainter>


using namespace std;

YK16GrayImage::YK16GrayImage(void)
{
	m_iWidth = 0;
	m_iHeight = 0;

	m_pData = NULL;
	m_pPixmap = NULL;// for display
	m_pElektaHisHeader = NULL; // VARIAN header isn't used for anything relevant

	//m_pQImage = NULL;	

	//m_pPainter = NULL;

	m_fPixelMean = 0.0;
	m_fPixelSD= 0.0;
	m_fPixelMin = 0.0;
	m_fPixelMax= 0.0;

	m_fPixelMean_ROI = 0.0;
	m_fPixelSD_ROI = 0.0;
	m_fPixelMin_ROI = 0.0;
	m_fPixelMax_ROI = 0.0;	
	m_bDrawROI = false;
	m_bShowInvert = false;

	m_fSpacingX = 0.0;
	m_fSpacingY = 0.0;


	m_ptProfileProbe.setX(0); //Mouse Clicked Position --> Data
	m_ptProfileProbe.setY(0);	

	m_bDrawProfileX = false;
	m_bDrawProfileY = false;


	m_ptFOVCenter.setX(0);
	m_ptFOVCenter.setY(0);
	m_iFOVRadius = 0;//data pixel
	m_bDrawFOVCircle = false;

	m_iTableTopPos = 0;
	m_bDrawTableLine = false;


	m_bDrawCrosshair = false;

	m_fZoom = 1.0;
	m_iOffsetX = 0;
	m_iOffsetY = 0;
	
}

YK16GrayImage::YK16GrayImage(int width, int height)
{
	m_iWidth = width;
	m_iHeight = height;

	m_pData = NULL;
	m_pPixmap = NULL;// for display
	m_pElektaHisHeader = NULL; // VARIAN header isn't used for anything relevant

	m_fPixelMean = 0.0;
	m_fPixelSD = 0.0;
	m_fPixelMin = 0.0;
	m_fPixelMax = 0.0;

	m_fPixelMean_ROI = 0.0;
	m_fPixelSD_ROI = 0.0;
	m_fPixelMin_ROI = 0.0;
	m_fPixelMax_ROI = 0.0;
	m_bDrawROI = false;
	m_bShowInvert = false;

	m_fSpacingX = 0.0;
	m_fSpacingY = 0.0;


	m_ptProfileProbe.setX(0); //Mouse Clicked Position --> Data
	m_ptProfileProbe.setY(0);

	m_bDrawProfileX = false;
	m_bDrawProfileY = false;


	m_ptFOVCenter.setX(0);
	m_ptFOVCenter.setY(0);
	m_iFOVRadius = 0;//data pixel
	m_bDrawFOVCircle = false;

	m_iTableTopPos = 0;
	m_bDrawTableLine = false;


	m_bDrawCrosshair = false;

	m_fZoom = 1.0;
	m_iOffsetX = 0;
	m_iOffsetY = 0;

	//m_pQImage = NULL;

	CreateImage(width, height, 0);			
}

YK16GrayImage::~YK16GrayImage(void)
{
	ReleaseBuffer();
}


bool YK16GrayImage::ReleaseBuffer()
{
	if (m_pData != NULL)
	{
		delete [] m_pData;
		m_pData = NULL;
	}
	if (m_pPixmap != NULL)
	{
		delete m_pPixmap; //delete  m_pPixmap??
                m_pPixmap = NULL;
	}

	m_iWidth = 0;
	m_iHeight = 0;

	if (m_pElektaHisHeader!= NULL) // VARIAN header isn't used for anything relevant
	{
		delete [] m_pElektaHisHeader;
		m_pElektaHisHeader= NULL;
	}

	return true;
}


bool YK16GrayImage::IsEmpty()
{
	if (m_pData == NULL)
		return true;
	else
		return false;
}

bool YK16GrayImage::CreateImage(int width, int height, unsigned short usVal)
{
	if (width < 1 || height < 1)
		return false;

	if (usVal < 0 || usVal > 65535)
		usVal = 0;

	//if (m_pData != NULL)
	//delete [] m_pData;

	ReleaseBuffer();

	m_iWidth = width;
	m_iHeight = height;

	int imgSize = width*height;
	m_pData = new unsigned short [imgSize];

	for (int i = 0 ; i<imgSize ; i++)
	{
		m_pData[i] = usVal;
	}

	//m_QImage = QImage(m_iWidth, m_iHeight,QImage::Format_RGB888);

	return true;
}


bool YK16GrayImage::LoadRawImage(const char *filePath, int width, int height)
{
	if (width < 1 || height < 1)
		return false;

	//if (m_pData != NULL)
	//	delete [] m_pData;	
	ReleaseBuffer();

	m_strFilePath = filePath;
	


	m_iWidth = width;
	m_iHeight = height;

	int imgSize = width*height;
	m_pData = new unsigned short [imgSize];	

	//aqprintf("ImageInfo in LoadRawImage, w: %d  h: %d   %d  %d \n",width, height, m_iWidth, m_iHeight);

	FILE* fd = NULL;
	fd = fopen(filePath, "rb");

	if (fd == NULL)
		return false;

	unsigned short buf = 0;	

	for (int i = 0 ; i<imgSize ; i++)
	{
		fread(&buf, 2, 1, fd);
		m_pData[i] = buf;
	}	

	fclose(fd);
	return true;
}

bool YK16GrayImage::CopyFromBuffer(unsigned short* pImageBuf, int width, int height)
{
	if (m_pData == NULL)
		return false;
	if (pImageBuf == NULL)
		return false;
	if (width != m_iWidth || height != m_iHeight)
		return false;

	int imgSize = m_iWidth*m_iHeight;

	for (int i = 0 ; i<imgSize ; i++)
	{
		m_pData[i] = pImageBuf[i];
	}

	//CalcImageInfo();

	return true;
}
bool YK16GrayImage::FillPixMap(int winMid, int winWidth) //0-65535 중 window level
{	
	if (m_pData == NULL)
		return false;

	if (m_pPixmap != NULL)
	{		                
		delete m_pPixmap;                
		m_pPixmap = NULL;                
	}	
	m_pPixmap = new QPixmap(QSize(m_iWidth,m_iHeight)); //something happened here!!!: w: 4289140  h: 0	        

	//8 bit gray buffer preparing

	int size = m_iWidth*m_iHeight;

	uchar* tmpData = new uchar [size*3];//RGB
	unsigned short uppVal = (int)(winMid + winWidth/2.0);
	unsigned short lowVal = (int)(winMid - winWidth/2.0);	        

	//It takes 0.4 s in Release mode

	for (int i = 0 ; i<m_iHeight ; i++) //So long time....
	{
		for (int j = 0 ; j<m_iWidth ; j++)
		{
			int tmpIdx = 3*(i*m_iWidth+j);


			if (!m_bShowInvert)
			{

				if (m_pData[i*m_iWidth+j] >= uppVal)
				{
					//QRgb rgbVal = qRgb(255, 255, 255);
					//m_QImage.setPixel(j,i,qRgb(255, 255, 255));

					tmpData[tmpIdx+0] = 255;
					tmpData[tmpIdx+1] = 255;
					tmpData[tmpIdx+2] = 255;

				}
				else if (m_pData[i*m_iWidth+j] <= lowVal)
				{			  
					tmpData[tmpIdx+0] = 0;
					tmpData[tmpIdx+1] = 0;
					tmpData[tmpIdx+2] = 0;
					//QRgb rgbVal = qRgb(0, 0, 0);
					//m_QImage.setPixel(j,i,qRgb(0, 0, 0));
				}
				else
				{
					tmpData[tmpIdx+0] = (uchar) ((m_pData[i*m_iWidth+j] - lowVal)/(double)winWidth * 255.0); //success
					tmpData[tmpIdx+1] = (uchar) ((m_pData[i*m_iWidth+j] - lowVal)/(double)winWidth * 255.0); //success
					tmpData[tmpIdx+2] = (uchar) ((m_pData[i*m_iWidth+j] - lowVal)/(double)winWidth * 255.0); //success				
				}
			}
			else
			{
				if (m_pData[i*m_iWidth+j] >= uppVal)
				{
					//QRgb rgbVal = qRgb(255, 255, 255);
					//m_QImage.setPixel(j,i,qRgb(255, 255, 255));

					tmpData[tmpIdx+0] = 0;
					tmpData[tmpIdx+1] = 0;
					tmpData[tmpIdx+2] = 0;

				}
				else if (m_pData[i*m_iWidth+j] <= lowVal)
				{
					tmpData[tmpIdx+0] = 255;
					tmpData[tmpIdx+1] = 255;
					tmpData[tmpIdx+2] = 255;
					//QRgb rgbVal = qRgb(0, 0, 0);
					//m_QImage.setPixel(j,i,qRgb(0, 0, 0));
				}
				else
				{
					tmpData[tmpIdx+0] = 255 - (uchar) ((m_pData[i*m_iWidth+j] - lowVal)/(double)winWidth * 255.0); //success
					tmpData[tmpIdx+1] = 255 - (uchar) ((m_pData[i*m_iWidth+j] - lowVal)/(double)winWidth * 255.0); //success
					tmpData[tmpIdx+2] = 255 - (uchar) ((m_pData[i*m_iWidth+j] - lowVal)/(double)winWidth * 255.0); //success				
				}
			}
		}		
	}	
	///m_QImage.save("C:\\FromFillPixmap.png");//it works well

	int iBytesPerLine = m_iWidth*3;        

	QImage tmpQImage = QImage((unsigned char*)tmpData,m_iWidth, m_iHeight,iBytesPerLine, QImage::Format_RGB888); //not deep copy!


	//image ratio (width / height) should be kept constant.
	//PAN: center of image is moving

	//m_QImage = tmpQImage.copy(0,0,m_iWidth, m_iHeight); //memory allocated here!!!

	int newWidth = qRound(m_iWidth/m_fZoom);
	int newHeight = qRound(m_iHeight/m_fZoom);

	int centerX = m_iOffsetX + qRound(m_iWidth/2.0);
	int centerY = m_iOffsetY + qRound(m_iHeight/2.0);

	int newLeftTopX = centerX - qRound(newWidth/2.0);
	int newLeftTopY = centerY - qRound(newHeight/2.0);	
	m_QImage = tmpQImage.copy(newLeftTopX,newLeftTopY, newWidth, newHeight); //memory allocated here!!!


	//copy (0,0, width, height)

	//*m_pPixmap = QPixmap::fromImage(m_QImage); //copy data to pre-allcated pixmap buffer

	delete [] tmpData;
	return true;
}

bool YK16GrayImage::FillPixMapMinMax(int winMin, int winMax) //0-65535 중 window level
{
	if (winMin < 0 || winMax > 65535 || winMin > winMax)
	{
		winMin = 0;
		winMax = 65535;
	}

	int midVal = (int)((winMin + winMax)/2.0);
	int widthVal = winMax - winMin;        

	return FillPixMap(midVal, widthVal);	
}

bool YK16GrayImage::SaveDataAsRaw (const char *filePath) //save 16 bit gray raw file
{
	if (m_pData == NULL)
		return false;

	int imgSize = m_iWidth*m_iHeight;		

	FILE* fd = NULL;
	fd = fopen(filePath, "wb");


	for (int i = 0 ; i<imgSize ; i++)
	{		
		fwrite(&m_pData[i], 2, 1, fd);		
	}	

	fclose(fd);
	return true;
}
//
//bool YK16GrayImage::DrawToLabel( QLabel* lbDisplay ) //using pixmap
//{
//	if (m_pPixmap == NULL)
//		return false;
//
//	int width = lbDisplay->width();
//	int height = lbDisplay->height();
//
//	double ratio =m_pPixmap->width() / (double)width;
//	int newHeight = m_pPixmap->height() / ratio;
//
//	//m_pPixmap->scaled(wid,showHeght,Qt::IgnoreAspectRatio)
//	//lbDisplay->setPixmap(m_pPixmap->scaled(width, height, Qt::IgnoreAspectRatio));
//	lbDisplay->setPixmap(m_pPixmap->scaled(width, newHeight, Qt::IgnoreAspectRatio));
//
//	/*int w = m_QImage.width();
//	int h = m_QImage.height();*/
//	//bool result = m_QImage.save("C:\\abcdefg.png");
//
//	return true;
//}

//
//bool YK16GrayImage::CalcImageInfo (double& meanVal, double& STDV, double& minVal, double& maxVal)
//{
//	if (m_pData == NULL)
//		return false;
//
//	int nTotal;
//	long minPixel, maxPixel;
//	int i;
//	double pixel, sumPixel;
//
//	int npixels = m_iWidth*m_iHeight;
//	nTotal = 0;
//	//minPixel = 4095;
//	minPixel = 65535;
//	maxPixel = 0;
//	sumPixel = 0.0;
//
//	for (i = 0; i < npixels; i++)
//	{
//		pixel = (double) m_pData[i];
//		sumPixel += pixel;
//		if (m_pData[i] > maxPixel)
//			maxPixel = m_pData[i];
//		if (m_pData[i] < minPixel)
//			minPixel = m_pData[i];
//		nTotal++;
//	}
//
//	double meanPixelval = sumPixel / (double)nTotal;    
//
//	double sqrSum = 0.0;
//	for (i = 0; i < npixels; i++)
//	{
//		sqrSum = sqrSum + pow(((double)m_pData[i] - meanPixelval),2.0);
//	}
//	double SD = sqrt(sqrSum/(double)nTotal);
//
//	meanVal = meanPixelval;
//	STDV = SD;
//	minVal = minPixel;
//	maxVal = maxPixel;
//
//	return true;
//}
//



 
bool YK16GrayImage::CalcImageInfo ()
{
	if (m_pData == NULL)
		return false;

	

	int nTotal;
	long minPixel, maxPixel;
	int i;
	double pixel, sumPixel;

	int npixels = m_iWidth*m_iWidth;

	if (npixels <= 0)
	  return false;

	nTotal = 0;
	//minPixel = 4095;
	minPixel = 65535;
	maxPixel = 0;
	sumPixel = 0.0;

	for (i = 0; i < npixels; i++)
	{
		pixel = (double) m_pData[i];
		sumPixel += pixel;
		if (m_pData[i] > maxPixel)
			maxPixel = m_pData[i];
		if (m_pData[i] < minPixel)
			minPixel = m_pData[i];
		nTotal++;
	}

	double meanPixelval = 0.0;

	if (nTotal <= 0)
	  meanPixelval = 0.0;
	else
	  meanPixelval = sumPixel / (double)nTotal;

	double sqrSum = 0.0;
	for (i = 0; i < npixels; i++)
	{
		sqrSum = sqrSum + pow(((double)m_pData[i] - meanPixelval),2.0);
	}
	double SD = sqrt(sqrSum/(double)nTotal);

	m_fPixelMean = meanPixelval;
	m_fPixelSD = SD;
	m_fPixelMin = minPixel;
	m_fPixelMax = maxPixel;

	return true;
}


double YK16GrayImage::CalcAveragePixelDiff(YK16GrayImage& other)
{
	if (m_pData == NULL || other.m_pData == NULL)
		return 0.0;

	int totalPixCnt = m_iWidth * m_iHeight;
	double tmpSum = 0.0;
	for (int i = 0 ; i<totalPixCnt ; i++)
	{
		tmpSum = tmpSum + fabs((double)m_pData[i] - (double)other.m_pData[i]);
	}

	return tmpSum / (double)totalPixCnt;
}

//bool YK16GrayImage::DoPixelReplacement(vector<BADPIXELMAP>& vPixelMapping )
//{
//	if (vPixelMapping.empty())
//		return false;		
//
//	if (m_pData == NULL)
//		return false;
//
//
//
//	int oriIdx, replIdx;
//
//	vector<BADPIXELMAP>::iterator it;
//
//	for (it = vPixelMapping.begin() ; it != vPixelMapping.end() ; it++)
//	{
//		BADPIXELMAP tmpData= (*it);
//		oriIdx = tmpData.BadPixY * m_iWidth + tmpData.BadPixX;
//		replIdx = tmpData.ReplPixY * m_iWidth + tmpData.ReplPixX;
//		m_pData[oriIdx] = m_pData[replIdx];
//	}
//
//
//
//	return true;
//}



//
//void YK16GrayImage::CopyYKImage2ItkImage(YK16GrayImage* pYKImage, UnsignedShortImageType::Pointer& spTarImage)
//{
//	if (pYKImage == NULL)
//		return;
//	//Raw File open	
//	//UnsignedShortImageType::SizeType tmpSize = 
//	UnsignedShortImageType::RegionType region = spTarImage->GetRequestedRegion();
//	UnsignedShortImageType::SizeType tmpSize = region.GetSize();
//
//	int sizeX = tmpSize[0];
//	int sizeY = tmpSize[1];
//
//	if (sizeX < 1 || sizeY <1)
//		return;
//
//	itk::ImageRegionIterator<UnsignedShortImageType> it(spTarImage, region);
//
//	int i = 0;
//	for (it.GoToBegin() ; !it.IsAtEnd(); it++)
//	{
//		it.Set(pYKImage->m_pData[i]);
//		i++;
//	}
//	//int totCnt = i;
//	//writerType::Pointer writer = writerType::New();	
//	//writer->SetInput(spTarImage);	
//	//writer->SetFileName("C:\\ThisImageIs_spSrcImage.png");	//It works!
//	//writer->Update();
//}
//void YK16GrayImage::CopyItkImage2YKImage(UnsignedShortImageType::Pointer& spSrcImage, YK16GrayImage* pYKImage)
//{
//	if (pYKImage == NULL)
//		return;
//	//Raw File open	
//	//UnsignedShortImageType::SizeType tmpSize = 
//	UnsignedShortImageType::RegionType region = spSrcImage->GetRequestedRegion();
//	UnsignedShortImageType::SizeType tmpSize = region.GetSize();
//
//	int sizeX = tmpSize[0];
//	int sizeY = tmpSize[1];
//
//	if (sizeX < 1 || sizeY <1)
//		return;
//
//	//itk::ImageRegionConstIterator<UnsignedShortImageType> it(spSrcImage, region);
//	itk::ImageRegionIterator<UnsignedShortImageType> it(spSrcImage, region);
//
//	int i = 0;
//	for (it.GoToBegin() ; !it.IsAtEnd() ; it++)
//	{
//		pYKImage->m_pData[i] = it.Get();
//		i++;
//	}
//	//int totCnt = i; //Total Count is OK
//
//	//int width = pYKImage->m_iWidth;
//	//int height = pYKImage->m_iHeight;
//
//
//	//writerType::Pointer writer = writerType::New();	
//	//writer->SetInput(spSrcImage);	
//	//writer->SetFileName("C:\\ThisImageIs_spSrcImage2.png");	//It works!
//	//writer->Update();
//}


void YK16GrayImage::Swap(YK16GrayImage* pImgA, YK16GrayImage* pImgB)
{
	if (pImgA == NULL || pImgB == NULL )
		return;

	if (pImgA->IsEmpty() || pImgB->IsEmpty() )
		return;

	YK16GrayImage tmpImg(pImgA->m_iWidth, pImgB->m_iHeight);
	tmpImg.CopyFromBuffer(pImgA->m_pData,pImgA->m_iWidth, pImgA->m_iHeight);
	tmpImg.m_strFilePath = pImgA->m_strFilePath;

	pImgA->CopyFromBuffer(pImgB->m_pData,pImgB->m_iWidth, pImgB->m_iHeight);
	pImgA->m_strFilePath = pImgB->m_strFilePath;


	pImgB->CopyFromBuffer(tmpImg.m_pData,tmpImg.m_iWidth, tmpImg.m_iHeight);
	pImgB->m_strFilePath = tmpImg.m_strFilePath;
}



bool YK16GrayImage::SaveDataAsHis( const char *filePath, bool bInverse )
{
	if (m_pData == NULL)
		return false;

	if (m_pElektaHisHeader ==NULL)// VARIAN header isn't used for anything relevant
		return false;


	FILE* fd = NULL;
	fd = fopen(filePath, "wb");

	fwrite(m_pElektaHisHeader, 100, 1, fd);// VARIAN header isn't used for anything relevant

	int imgSize = m_iWidth*m_iHeight;

	for (int i = 0 ; i<imgSize ; i++)
	{	
		unsigned short tmpVal =0;
		
		if (bInverse)
			tmpVal = 65535 - m_pData[i];
		else
			tmpVal = m_pData[i];			

		fwrite(&tmpVal, 2, 1, fd);		
	}	

	fclose(fd);
	
	return true;

}

void YK16GrayImage::CopyHisHeader( const char *hisFilePath )
{
	// open file
	std::ifstream file(hisFilePath, std::ios::in | std::ios::binary);

	if ( file.fail() )
		cout << "Fail to open" << "	" << hisFilePath << endl;		

	// read header
	if (m_pElektaHisHeader != NULL)// VARIAN header isn't used for anything relevant
		delete [] m_pElektaHisHeader;

	m_pElektaHisHeader = new char [DEFAULT_ELEKTA_HIS_HEADER_SIZE]; // DEFAULT_ELEKTA_HIS_HEADER_SIZE = 100
	file.read(m_pElektaHisHeader, DEFAULT_ELEKTA_HIS_HEADER_SIZE);
}
//
//void YK16GrayImage::Swap(YK16GrayImage* pImgA, YK16GrayImage* pImgB)
//{
//	if (pImgA == NULL || pImgB == NULL )
//		return;
//
//	if (pImgA->IsEmpty() || pImgB->IsEmpty() )
//		return;
//
//	YK16GrayImage tmpImg(pImgA->m_iWidth, pImgB->m_iHeight);
//	tmpImg.CopyFromBuffer(pImgA->m_pData,pImgA->m_iWidth, pImgA->m_iHeight);
//	tmpImg.m_strFilePath = pImgA->m_strFilePath;
//
//	pImgA->CopyFromBuffer(pImgB->m_pData,pImgB->m_iWidth, pImgB->m_iHeight);
//	pImgA->m_strFilePath = pImgB->m_strFilePath;
//
//
//	pImgB->CopyFromBuffer(tmpImg.m_pData,tmpImg.m_iWidth, tmpImg.m_iHeight);
//	pImgB->m_strFilePath = tmpImg.m_strFilePath;
//}




void YK16GrayImage::CopyYKImage2ItkImage(YK16GrayImage* pYKImage, UnsignedShortImageType::Pointer& spTarImage)
{
	if (pYKImage == NULL)
		return;
	//Raw File open	
	//UnsignedShortImageType::SizeType tmpSize = 
	UnsignedShortImageType::RegionType region = spTarImage->GetRequestedRegion();
	UnsignedShortImageType::SizeType tmpSize = region.GetSize();

	int sizeX = tmpSize[0];
	int sizeY = tmpSize[1];

	if (sizeX < 1 || sizeY <1)
		return;

	itk::ImageRegionIterator<UnsignedShortImageType> it(spTarImage, region);

	int i = 0;
	for (it.GoToBegin() ; !it.IsAtEnd(); ++it)
	{
		it.Set(pYKImage->m_pData[i]);
		i++;
	}
	//int totCnt = i;
	//writerType::Pointer writer = writerType::New();	
	//writer->SetInput(spTarImage);	
	//writer->SetFileName("C:\\ThisImageIs_spSrcImage.png");	//It works!
	//writer->Update();
}
void YK16GrayImage::CopyItkImage2YKImage(UnsignedShortImageType::Pointer& spSrcImage, YK16GrayImage* pYKImage)
{
	if (pYKImage == NULL)
		return;
	//Raw File open	
	//UnsignedShortImageType::SizeType tmpSize = 
	UnsignedShortImageType::RegionType region = spSrcImage->GetRequestedRegion();
	UnsignedShortImageType::SizeType tmpSize = region.GetSize();

	int sizeX = tmpSize[0];
	int sizeY = tmpSize[1];

	if (sizeX < 1 || sizeY <1)
		return;

	//itk::ImageRegionConstIterator<UnsignedShortImageType> it(spSrcImage, region);
	itk::ImageRegionIterator<UnsignedShortImageType> it(spSrcImage, region);

	int i = 0;
	for (it.GoToBegin() ; !it.IsAtEnd() ; ++it)
	{
		pYKImage->m_pData[i] = it.Get();
		i++;
	}
	//int totCnt = i; //Total Count is OK

	//int width = pYKImage->m_iWidth;
	//int height = pYKImage->m_iHeight;


	//writerType::Pointer writer = writerType::New();	
	//writer->SetInput(spSrcImage);	
	//writer->SetFileName("C:\\ThisImageIs_spSrcImage2.png");	//It works!
	//writer->Update();
}

bool YK16GrayImage::CalcImageInfo_ROI()
{
	if (m_pData == NULL)
	{		
		m_fPixelMean_ROI =-1.0;
		m_fPixelSD_ROI=-1.0;
		m_fPixelMin_ROI=-1.0;
		m_fPixelMax_ROI=-1.0;
		return false;
	}

	if (m_rtROI.width() < 1 || m_rtROI.height() < 1)
	{
		m_fPixelMean_ROI =-1.0;
		m_fPixelSD_ROI=-1.0;
		m_fPixelMin_ROI=-1.0;
		m_fPixelMax_ROI=-1.0;
		return false;
	}		

	int nTotal;
	long minPixel, maxPixel;
	
	double pixel, sumPixel;

	int npixels = m_iWidth*m_iWidth;
	nTotal = 0;
	//minPixel = 4095;
	minPixel = 65535;
	maxPixel = 0;
	sumPixel = 0.0;


	int i,j;

	for (i =m_rtROI.top()  ; i<m_rtROI.bottom() ; i++)
	{
		for (j=m_rtROI.left() ; j < m_rtROI.right() ; j++)
		{
			int idx = m_iWidth*i + j;
			pixel = (double) m_pData[idx];
			sumPixel += pixel;
			if (m_pData[idx] > maxPixel)
				maxPixel = m_pData[idx];
			if (m_pData[idx] < minPixel)
				minPixel = m_pData[idx];
			nTotal++;			
		}
	}

	double meanPixelval = sumPixel / (double)nTotal;    

	double sqrSum = 0.0;
	/*for (i = 0; i < nTotal; i++)
	{
		sqrSum = sqrSum + pow(((double)m_pData[i] - meanPixelval),2.0);
	}*/

	for (i =m_rtROI.top()  ; i<m_rtROI.bottom() ; i++)
	{
		for (j=m_rtROI.left() ; j < m_rtROI.right() ; j++)
		{
			int idx = m_iWidth*i + j;
			sqrSum = sqrSum + pow(((double)m_pData[idx] - meanPixelval),2.0);
		}
	}

	double SD = sqrt(sqrSum/(double)nTotal);

	m_fPixelMean_ROI = meanPixelval;
	m_fPixelSD_ROI = SD;
	m_fPixelMin_ROI = minPixel;
	m_fPixelMax_ROI = maxPixel;

	return true;
}

bool YK16GrayImage::setROI( int left, int top, int right, int bottom )
{	
	
	if (left >= right || top >= bottom || left < 0 || right > m_iWidth-1 || top <0 || bottom > m_iHeight-1)
	{
		m_rtROI.setLeft(0);
		m_rtROI.setTop(0);
		m_rtROI.setRight(m_iWidth-1);
		m_rtROI.setBottom(m_iHeight-1);
		return false;
	}
	m_rtROI.setLeft(left);
	m_rtROI.setTop(top);
	m_rtROI.setRight(right);
	m_rtROI.setBottom(bottom);
	return true;
}

void YK16GrayImage::DrawROIOn( bool bROI_Draw )
{
	if (bROI_Draw)
		m_bDrawROI = true;
	else
		m_bDrawROI = false;

}

bool YK16GrayImage::CloneImage( YK16GrayImage& other )
{
	if (other.m_pData == NULL)
		return false;

	//first, delete existing things
	ReleaseBuffer(); //Redundancy. CreateImage will call this too. also Create is calling this func. 

	int width = other.m_iWidth;
	int height = other.m_iHeight;

	CreateImage(width, height, 0);
	CopyFromBuffer(other.m_pData, width, height);

	if (other.m_pElektaHisHeader != NULL)// VARIAN header isn't used for anything relevant
	{
	  m_pElektaHisHeader = new char [DEFAULT_ELEKTA_HIS_HEADER_SIZE];
	  for (int i = 0 ; i<DEFAULT_ELEKTA_HIS_HEADER_SIZE ; i++ )
	  {
		  m_pElektaHisHeader[i] = other.m_pElektaHisHeader[i];
	  }
	}

	m_strFilePath = other.m_strFilePath;

	m_fPixelMean= other.m_fPixelMean;
	m_fPixelSD= other.m_fPixelSD;
	m_fPixelMin= other.m_fPixelMin;
	m_fPixelMax= other.m_fPixelMax;

	m_rtROI = other.m_rtROI;	
	m_fPixelMean_ROI=other.m_fPixelMean_ROI;
	m_fPixelSD_ROI=other.m_fPixelSD_ROI;
	m_fPixelMin_ROI=other.m_fPixelMin_ROI;
	m_fPixelMax_ROI=other.m_fPixelMax_ROI;
	m_bDrawROI= other.m_bDrawROI;	
	m_QImage = other.m_QImage;
	m_bShowInvert = other.m_bShowInvert;

	SetSpacing(other.m_fSpacingX, other.m_fSpacingY);

	m_ptProfileProbe = other.m_ptProfileProbe;
	m_bDrawProfileX = other.m_bDrawProfileX;
	m_bDrawProfileY = other.m_bDrawProfileY;

	m_ptFOVCenter = other.m_ptFOVCenter; // data pos
	m_iFOVRadius = other.m_iFOVRadius;//data pos (pixel)
	m_bDrawFOVCircle = other.m_bDrawFOVCircle;

	m_iTableTopPos = other.m_iTableTopPos;//data pos
	m_bDrawTableLine = other.m_bDrawTableLine;

	m_ptCrosshair = other.m_ptCrosshair; //data position
	m_bDrawCrosshair = other.m_bDrawCrosshair;

	m_ptSplitCenter = other.m_ptSplitCenter; //Fixed image with Moving image. center is based on dataPt.//Fixed Image: Left Top + Right Bottom, Moving: Right Top + Left Bottom
	m_enSplitOption = other.m_enSplitOption;	

	m_iOffsetX = other.m_iOffsetX;
	m_iOffsetY = other.m_iOffsetY;

	m_fZoom =other.m_fZoom;	


     return true;	
}

void YK16GrayImage::MultiplyConstant( double multiplyFactor )
{
	if (m_pData == NULL)
		return;

	for (int i = 0 ; i < m_iHeight ; i++)
	{
		for (int j = 0 ; j<m_iWidth ; j++)
		{
			m_pData[m_iWidth*i + j] = (unsigned short)(((double)m_pData[m_iWidth*i + j]) * multiplyFactor);
		}
	}

}

void YK16GrayImage::SetProfileProbePos( int dataX, int dataY )
{
	if (m_ptProfileProbe.y() >= 0 && m_ptProfileProbe.y() < m_iHeight &&
		m_ptProfileProbe.x() >= 0 && m_ptProfileProbe.x() < m_iWidth)
	{
		m_ptProfileProbe.setX(dataX);
		m_ptProfileProbe.setY(dataY);
	}
	else
	{
		m_ptProfileProbe.setX(0);
		m_ptProfileProbe.setY(0);
	}
	

}

unsigned short YK16GrayImage::GetProfileProbePixelVal()
{
	unsigned short resultVal = 0;
	if (m_pData == NULL)
		return 0;

	if (m_ptProfileProbe.y() >= 0 && m_ptProfileProbe.y() < m_iHeight &&
		m_ptProfileProbe.x() >= 0 && m_ptProfileProbe.x() < m_iWidth)
		resultVal = m_pData[m_iWidth*m_ptProfileProbe.y() + m_ptProfileProbe.x()];
	else
		resultVal = 0;		

        return resultVal;
}

void YK16GrayImage::GetProfileData( int dataX, int dataY, QVector<double>& vTarget, enProfileDirection direction )
{
	if (m_pData == NULL)
		return;

	if (dataY < 0 || dataY >= m_iHeight ||
		dataX < 0 || dataX >= m_iWidth)
		return;

	vTarget.clear();

	if (direction == DIRECTION_HOR)
	{
		int fixedY  = dataY;
		for (int j = 0 ; j< m_iWidth ; j++)
		{
			vTarget.push_back(m_pData[m_iWidth*fixedY + j]);
		}
	}
	else if (direction == DIRECTION_VER)
	{
		//Upper to Lower profile

		int fixedX  = dataX;
		for (int i = 0 ; i< m_iHeight ; i++)
		{
			vTarget.push_back(m_pData[m_iWidth*i + fixedX]);
		}
	}

        return;
}
void YK16GrayImage::GetProfileData(QVector<double>& vTarget, enProfileDirection direction )
{
	if (m_pData == NULL)
		return;

	int dataX = m_ptProfileProbe.x();
	int dataY = m_ptProfileProbe.y();

	if (dataY < 0 || dataY >= m_iHeight ||
		dataX < 0 || dataX >= m_iWidth)
		return;

	vTarget.clear();

	if (direction == DIRECTION_HOR)
	{
		int fixedY  = dataY;
		for (int j = 0 ; j< m_iWidth ; j++)
		{
			vTarget.push_back((double)(m_pData[m_iWidth*fixedY + j]));
		}
	}
	else if (direction == DIRECTION_VER)
	{
		//Upper to Lower profile

		int fixedX  = dataX;
		for (int i = 0 ; i< m_iHeight ; i++)
		{
			vTarget.push_back((double)(m_pData[m_iWidth*i + fixedX]));
		}
	}

}

bool YK16GrayImage::ConstituteFromTwo( YK16GrayImage& YKImg1,YK16GrayImage& YKImg2 )
{
    //Filtering
    if (YKImg1.IsEmpty() || YKImg2.IsEmpty() || YKImg1.m_iWidth != YKImg2.m_iWidth || YKImg1.m_iHeight != YKImg2.m_iHeight
        || YKImg1.m_iWidth * YKImg1.m_iHeight == 0)
        return false;    


    int width = YKImg1.m_iWidth;
    int height = YKImg1.m_iHeight;

    CreateImage(width, height, 0);

    int centerX = m_ptSplitCenter.x(); //data index
    int centerY = m_ptSplitCenter.y();

    int i,j;
    switch (m_enSplitOption)
    {
    case PRI_LEFT_TOP:
        for (i = 0 ; i<centerY ; i++)
        {
            for (j = 0 ; j<centerX ; j++)
            {
                m_pData[width*i + j] = YKImg1.m_pData[width*i + j];
            }
        }

        for (i = centerY ; i<height ; i++)
        {
            for (j = centerX ; j<width ; j++)
            {
                m_pData[width*i + j] = YKImg1.m_pData[width*i + j];
            }
        }

        for (i = 0 ; i < centerY ; i++)
        {
            for (j = centerX ; j<width ; j++)
            {
                m_pData[width*i + j] = YKImg2.m_pData[width*i + j];
            }
        }

        for (i = centerY ; i<height ; i++)
        {
            for (j = 0 ; j<centerX ; j++)
            {
                m_pData[width*i + j] = YKImg2.m_pData[width*i + j];
            }
        }

        break;
    default:
        break;
    }

    return true;
}

void YK16GrayImage::EditImage_Flip()
{
  if (m_pData == NULL)
	return;  

  int imgSize = m_iWidth*m_iHeight;

  if (imgSize <=0)
	return;

  //복사할 임시 buffer 생성
  unsigned short* pPrevImg = new unsigned short [imgSize]; 

  int i,j;

  for (i = 0 ; i<m_iHeight ; i++)
  {
	for (j = 0 ; j<m_iWidth ; j++)
	{	  
		pPrevImg[i*m_iWidth + j] = m_pData[i*m_iWidth + j];				 
	}
  }

  for (i = 0 ; i<m_iHeight ; i++)
  {
	for (j = 0 ; j<m_iWidth ; j++)
	{
	  int tmpX = j;
	  int tmpY = m_iHeight-i-1;	  

	  m_pData[i*m_iWidth + j] = pPrevImg[tmpY*m_iWidth + tmpX];			
	}
  }

  delete [] pPrevImg;
  return;
}

void YK16GrayImage::EditImage_Mirror()
{
  if (m_pData == NULL)
	return;  

  int imgSize = m_iWidth*m_iHeight;

  if (imgSize <= 0)
	return;

  int i = 0 ; int j = 0;  

  unsigned short* pPrevImg = new unsigned short [imgSize];  
  

  //변환 전 이미지를 copy

  for (i = 0 ; i<m_iHeight ; i++)
  {
	for (j = 0 ; j<m_iWidth ; j++)
	{	  
		pPrevImg[i*m_iWidth + j] = m_pData[i*m_iWidth + j];	  
	}
  }

  for (i = 0 ; i<m_iHeight ; i++)
  {
	for (j = 0 ; j<m_iWidth ; j++)
	{
	  int tmpX = m_iWidth-j-1;
	  int tmpY = i;
	  
	  m_pData[i*m_iWidth + j] = pPrevImg[tmpY*m_iWidth + tmpX];	  
	}
  }

  delete [] pPrevImg;

  return;
}

bool YK16GrayImage::FillPixMapDual( int winMid1, int winMid2,int winWidth1, int winWidth2 )
{
  if (m_pData == NULL)
		return false;

	if (m_pPixmap != NULL)
	{		                
		delete m_pPixmap;                
		m_pPixmap = NULL;                
	}	
	m_pPixmap = new QPixmap(QSize(m_iWidth,m_iHeight)); //something happened here!!!: w: 4289140  h: 0	        

	//8 bit gray buffer preparing
	int size = m_iWidth*m_iHeight;

	uchar* tmpData = new uchar [size*3];//RGB

	int uppVal1 = (int)(winMid1 + winWidth1/2.0);
	int lowVal1 = (int)(winMid1 - winWidth1/2.0);	        

	int uppVal2 = (int)(winMid2 + winWidth2/2.0);
	int lowVal2 = (int)(winMid2 - winWidth2/2.0);


	if (uppVal1 > 65535)
	  uppVal1 = 65535;
	if (uppVal2 > 65535)
	  uppVal2 = 65535;

	if (lowVal1 <= 0)
	  lowVal1 = 0;
	if (uppVal2 <= 0)
	  uppVal2 = 0;

	//It takes 0.4 s in Release mode

	if (m_enSplitOption != PRI_LEFT_TOP)
	  return false;


	int splitX = m_ptSplitCenter.x();
	int splitY = m_ptSplitCenter.y();


	//1/4 sector
	for (int i = 0 ; i<splitY ; i++) //So long time....
	{
		for (int j = 0 ; j< splitX ; j++)
		{
			int tmpIdx = 3*(i*m_iWidth+j);

			if (!m_bShowInvert)
			{
				if (m_pData[i*m_iWidth+j] >= uppVal1)
				{
					//QRgb rgbVal = qRgb(255, 255, 255);
					//m_QImage.setPixel(j,i,qRgb(255, 255, 255));

					tmpData[tmpIdx+0] = 255;
					tmpData[tmpIdx+1] = 255;
					tmpData[tmpIdx+2] = 255;

				}
				else if (m_pData[i*m_iWidth+j] <= lowVal1)
				{
					tmpData[tmpIdx+0] = 0;
					tmpData[tmpIdx+1] = 0;
					tmpData[tmpIdx+2] = 0;
					//QRgb rgbVal = qRgb(0, 0, 0);
					//m_QImage.setPixel(j,i,qRgb(0, 0, 0));
				}
				else
				{
					tmpData[tmpIdx+0] = (uchar) ((m_pData[i*m_iWidth+j] - lowVal1)/(double)winWidth1 * 255.0); //success
					tmpData[tmpIdx+1] = (uchar) ((m_pData[i*m_iWidth+j] - lowVal1)/(double)winWidth1 * 255.0); //success
					tmpData[tmpIdx+2] = (uchar) ((m_pData[i*m_iWidth+j] - lowVal1)/(double)winWidth1 * 255.0); //success				
				}
			}
			else
			{
				if (m_pData[i*m_iWidth+j] >= uppVal1)
				{
					//QRgb rgbVal = qRgb(255, 255, 255);
					//m_QImage.setPixel(j,i,qRgb(255, 255, 255));

					tmpData[tmpIdx+0] = 0;
					tmpData[tmpIdx+1] = 0;
					tmpData[tmpIdx+2] = 0;

				}
				else if (m_pData[i*m_iWidth+j] <= lowVal1)
				{
					tmpData[tmpIdx+0] = 255;
					tmpData[tmpIdx+1] = 255;
					tmpData[tmpIdx+2] = 255;
					//QRgb rgbVal = qRgb(0, 0, 0);
					//m_QImage.setPixel(j,i,qRgb(0, 0, 0));
				}
				else
				{
					tmpData[tmpIdx+0] = 255 - (uchar) ((m_pData[i*m_iWidth+j] - lowVal1)/(double)winWidth1 * 255.0); //success
					tmpData[tmpIdx+1] = 255 - (uchar) ((m_pData[i*m_iWidth+j] - lowVal1)/(double)winWidth1 * 255.0); //success
					tmpData[tmpIdx+2] = 255 - (uchar) ((m_pData[i*m_iWidth+j] - lowVal1)/(double)winWidth1 * 255.0); //success				
				}
			}
		}		
	}

	//2/4 sector
	for (int i = 0 ; i<splitY ; i++) //So long time....
	{
	  for (int j = splitX ; j< m_iWidth ; j++)
	  {
		int tmpIdx = 3*(i*m_iWidth+j);

		if (!m_bShowInvert)
		{

		  if (m_pData[i*m_iWidth+j] >= uppVal2)
		  {
			//QRgb rgbVal = qRgb(255, 255, 255);
			//m_QImage.setPixel(j,i,qRgb(255, 255, 255));

			tmpData[tmpIdx+0] = 255;
			tmpData[tmpIdx+1] = 255;
			tmpData[tmpIdx+2] = 255;

		  }
		  else if (m_pData[i*m_iWidth+j] <= lowVal2)
		  {
			tmpData[tmpIdx+0] = 0;
			tmpData[tmpIdx+1] = 0;
			tmpData[tmpIdx+2] = 0;
			//QRgb rgbVal = qRgb(0, 0, 0);
			//m_QImage.setPixel(j,i,qRgb(0, 0, 0));
		  }
		  else
		  {
			tmpData[tmpIdx+0] = (uchar) ((m_pData[i*m_iWidth+j] - lowVal2)/(double)winWidth2 * 255.0); //success
			tmpData[tmpIdx+1] = (uchar) ((m_pData[i*m_iWidth+j] - lowVal2)/(double)winWidth2 * 255.0); //success
			tmpData[tmpIdx+2] = (uchar) ((m_pData[i*m_iWidth+j] - lowVal2)/(double)winWidth2 * 255.0); //success				
		  }
		}
		else
		{
		  if (m_pData[i*m_iWidth+j] >= uppVal2)
		  {
			//QRgb rgbVal = qRgb(255, 255, 255);
			//m_QImage.setPixel(j,i,qRgb(255, 255, 255));

			tmpData[tmpIdx+0] = 0;
			tmpData[tmpIdx+1] = 0;
			tmpData[tmpIdx+2] = 0;

		  }
		  else if (m_pData[i*m_iWidth+j] <= lowVal2)
		  {
			tmpData[tmpIdx+0] = 255;
			tmpData[tmpIdx+1] = 255;
			tmpData[tmpIdx+2] = 255;
			//QRgb rgbVal = qRgb(0, 0, 0);
			//m_QImage.setPixel(j,i,qRgb(0, 0, 0));
		  }
		  else
		  {
			tmpData[tmpIdx+0] = 255 - (uchar) ((m_pData[i*m_iWidth+j] - lowVal2)/(double)winWidth2 * 255.0); //success
			tmpData[tmpIdx+1] = 255 - (uchar) ((m_pData[i*m_iWidth+j] - lowVal2)/(double)winWidth2 * 255.0); //success
			tmpData[tmpIdx+2] = 255 - (uchar) ((m_pData[i*m_iWidth+j] - lowVal2)/(double)winWidth2 * 255.0); //success				
		  }
		}
	  }		
	}

	//3/4 sector
	for (int i = splitY ; i<m_iHeight ; i++) //So long time....
	{
	  for (int j = 0 ; j< splitX ; j++)
	  {
		int tmpIdx = 3*(i*m_iWidth+j);

		if (!m_bShowInvert)
		{

		  if (m_pData[i*m_iWidth+j] >= uppVal2)
		  {
			//QRgb rgbVal = qRgb(255, 255, 255);
			//m_QImage.setPixel(j,i,qRgb(255, 255, 255));

			tmpData[tmpIdx+0] = 255;
			tmpData[tmpIdx+1] = 255;
			tmpData[tmpIdx+2] = 255;

		  }
		  else if (m_pData[i*m_iWidth+j] <= lowVal2)
		  {
			tmpData[tmpIdx+0] = 0;
			tmpData[tmpIdx+1] = 0;
			tmpData[tmpIdx+2] = 0;
			//QRgb rgbVal = qRgb(0, 0, 0);
			//m_QImage.setPixel(j,i,qRgb(0, 0, 0));
		  }
		  else
		  {
			tmpData[tmpIdx+0] = (uchar) ((m_pData[i*m_iWidth+j] - lowVal2)/(double)winWidth2 * 255.0); //success
			tmpData[tmpIdx+1] = (uchar) ((m_pData[i*m_iWidth+j] - lowVal2)/(double)winWidth2 * 255.0); //success
			tmpData[tmpIdx+2] = (uchar) ((m_pData[i*m_iWidth+j] - lowVal2)/(double)winWidth2 * 255.0); //success				
		  }
		}
		else
		{
		  if (m_pData[i*m_iWidth+j] >= uppVal2)
		  {
			//QRgb rgbVal = qRgb(255, 255, 255);
			//m_QImage.setPixel(j,i,qRgb(255, 255, 255));

			tmpData[tmpIdx+0] = 0;
			tmpData[tmpIdx+1] = 0;
			tmpData[tmpIdx+2] = 0;

		  }
		  else if (m_pData[i*m_iWidth+j] <= lowVal2)
		  {
			tmpData[tmpIdx+0] = 255;
			tmpData[tmpIdx+1] = 255;
			tmpData[tmpIdx+2] = 255;
			//QRgb rgbVal = qRgb(0, 0, 0);
			//m_QImage.setPixel(j,i,qRgb(0, 0, 0));
		  }
		  else
		  {
			tmpData[tmpIdx+0] = 255 - (uchar) ((m_pData[i*m_iWidth+j] - lowVal2)/(double)winWidth2 * 255.0); //success
			tmpData[tmpIdx+1] = 255 - (uchar) ((m_pData[i*m_iWidth+j] - lowVal2)/(double)winWidth2 * 255.0); //success
			tmpData[tmpIdx+2] = 255 - (uchar) ((m_pData[i*m_iWidth+j] - lowVal2)/(double)winWidth2 * 255.0); //success				
		  }
		}
	  }		
	}

	//4/4 sector
	for (int i = splitY ; i<m_iHeight ; i++) //So long time....
	{
	  for (int j = splitX ; j< m_iWidth ; j++)
	  {
		int tmpIdx = 3*(i*m_iWidth+j);

		if (!m_bShowInvert)
		{

		  if (m_pData[i*m_iWidth+j] >= uppVal1)
		  {
			//QRgb rgbVal = qRgb(255, 255, 255);
			//m_QImage.setPixel(j,i,qRgb(255, 255, 255));

			tmpData[tmpIdx+0] = 255;
			tmpData[tmpIdx+1] = 255;
			tmpData[tmpIdx+2] = 255;

		  }
		  else if (m_pData[i*m_iWidth+j] <= lowVal1)
		  {
			tmpData[tmpIdx+0] = 0;
			tmpData[tmpIdx+1] = 0;
			tmpData[tmpIdx+2] = 0;
			//QRgb rgbVal = qRgb(0, 0, 0);
			//m_QImage.setPixel(j,i,qRgb(0, 0, 0));
		  }
		  else
		  {
			tmpData[tmpIdx+0] = (uchar) ((m_pData[i*m_iWidth+j] - lowVal1)/(double)winWidth1 * 255.0); //success
			tmpData[tmpIdx+1] = (uchar) ((m_pData[i*m_iWidth+j] - lowVal1)/(double)winWidth1 * 255.0); //success
			tmpData[tmpIdx+2] = (uchar) ((m_pData[i*m_iWidth+j] - lowVal1)/(double)winWidth1 * 255.0); //success				
		  }
		}
		else
		{
		  if (m_pData[i*m_iWidth+j] >= uppVal1)
		  {
			//QRgb rgbVal = qRgb(255, 255, 255);
			//m_QImage.setPixel(j,i,qRgb(255, 255, 255));

			tmpData[tmpIdx+0] = 0;
			tmpData[tmpIdx+1] = 0;
			tmpData[tmpIdx+2] = 0;

		  }
		  else if (m_pData[i*m_iWidth+j] <= lowVal1)
		  {
			tmpData[tmpIdx+0] = 255;
			tmpData[tmpIdx+1] = 255;
			tmpData[tmpIdx+2] = 255;
			//QRgb rgbVal = qRgb(0, 0, 0);
			//m_QImage.setPixel(j,i,qRgb(0, 0, 0));
		  }
		  else
		  {
			tmpData[tmpIdx+0] = 255 - (uchar) ((m_pData[i*m_iWidth+j] - lowVal1)/(double)winWidth1 * 255.0); //success
			tmpData[tmpIdx+1] = 255 - (uchar) ((m_pData[i*m_iWidth+j] - lowVal1)/(double)winWidth1 * 255.0); //success
			tmpData[tmpIdx+2] = 255 - (uchar) ((m_pData[i*m_iWidth+j] - lowVal1)/(double)winWidth1 * 255.0); //success				
		  }
		}
	  }		
	}

	int iBytesPerLine = m_iWidth*3;        
	QImage tmpQImage = QImage((unsigned char*)tmpData,m_iWidth, m_iHeight,iBytesPerLine, QImage::Format_RGB888); //not deep copy!

	//Copy only a ROI region. All below are data-point, rather than display points
	//Outside region wiill be filled with Black by QImage inherent function

	//
	int newWidth = qRound(m_iWidth/m_fZoom);
	int newHeight = qRound(m_iHeight/m_fZoom);

	int centerX = m_iOffsetX + qRound(m_iWidth/2.0);
	int centerY = m_iOffsetY + qRound(m_iHeight/2.0);

	int newLeftTopX = centerX - qRound(newWidth/2.0);//data position
	int newLeftTopY = centerY - qRound(newHeight/2.0);	//data position
	m_QImage = tmpQImage.copy(qRound((qreal)newLeftTopX), qRound((qreal) newLeftTopY), newWidth, newHeight); //memory allocated here!!!
	

	//m_QImage = tmpQImage.copy(0,0,m_iWidth, m_iHeight); //memory allocated here!!!        
	//YKTEMP: is it needed? no it worked without below:
	//*m_pPixmap = QPixmap::fromImage(m_QImage); //copy data to pre-allcated pixmap buffer

	delete [] tmpData;
	return true;

}

bool YK16GrayImage::FillPixMapMinMaxDual( int winMin1, int winMin2, int winMax1, int winMax2 )
{
  if (winMin1 < 0 || winMax1 > 65535 || winMin1 > winMax1)
  {
	winMin1 = 0;
	winMax1 = 65535;
  }

  if (winMin2 < 0 || winMax2 > 65535 || winMin2 > winMax2)
  {
	winMin2 = 0;
	winMax2 = 65535;
  }

  int midVal1 = (int)((winMin1 + winMax1)/2.0);
  int midVal2 = (int)((winMin2 + winMax2)/2.0);

  int widthVal1 = winMax1 - winMin1;        
  int widthVal2 = winMax2 - winMin2;        

  return FillPixMapDual(midVal1, midVal2, widthVal1, widthVal2);

}

bool YK16GrayImage::isPtInFirstImage(int dataX, int dataY)
{ 
  if (dataX < 0 || dataX >= m_iWidth || dataY < 0  || dataY >= m_iHeight)
  {
	cout << "Fatal error in isPtInFirstImage! Given point is out of image point" << endl;
	return false;
  }

  if (m_enSplitOption == PRI_LEFT_TOP && !IsEmpty())
  {
	if ((dataX >= 0 && dataX < m_ptSplitCenter.x() && dataY >=0 && dataY < m_ptSplitCenter.y()) ||
		(dataX >= m_ptSplitCenter.x() && dataX < m_iWidth && dataY >= m_ptSplitCenter.y() && dataY < m_iHeight))
		return true;
	else
	  return false;
  }
  return false;
}

void YK16GrayImage::SetSplitCenter( QPoint& ptSplitCenter )
{
  if (IsEmpty())
	return;
  
  if (ptSplitCenter.x() < 0  || ptSplitCenter.x() >= m_iWidth || ptSplitCenter.y() < 0  || ptSplitCenter.y() >= m_iHeight)
  {
	m_ptSplitCenter.setX(0);
	m_ptSplitCenter.setY(0);
  }
  else
	m_ptSplitCenter = ptSplitCenter;

}

void YK16GrayImage::SetZoom( double fZoom )
{
  if (fZoom <= 1)
	m_fZoom = 1.0;
  else 
	m_fZoom = fZoom;	
}

void YK16GrayImage::MedianFilter( int iMedianSizeX, int iMedianSizeY )
{
  if (m_pData == NULL)
	return;

  UnsignedShortImageType::Pointer spTmpItkImg = UnsignedShortImageType::New();	

  UnsignedShortImageType::SizeType size;
  size[0] = m_iWidth;
  size[1] = m_iHeight;
  UnsignedShortImageType::IndexType idxStart;
  idxStart[0] = 0;
  idxStart[1] = 0;
  UnsignedShortImageType::SpacingType spacing;	
  if (m_fSpacingX*m_fSpacingY == 0)
  {
	spacing[0] = 1.0;
	spacing[1] = 1.0;
  }
  else
  {
	spacing[0] = m_fSpacingX;
	spacing[1] = m_fSpacingY;
  }

  UnsignedShortImageType::PointType origin;
  origin[0] = size[0]*spacing[0]/-2.0;
  origin[1] = size[1]*spacing[1]/-2.0;

  UnsignedShortImageType::RegionType region;
  region.SetSize(size);
  region.SetIndex(idxStart);

  spTmpItkImg->SetRegions(region);
  spTmpItkImg->SetSpacing(spacing);
  spTmpItkImg->SetOrigin(origin);
  spTmpItkImg->Allocate();

  CopyYKImage2ItkImage(this, spTmpItkImg);

  typedef itk::MedianImageFilter<UnsignedShortImageType, UnsignedShortImageType> MedianFilterType;
  MedianFilterType::Pointer medianFilter = MedianFilterType::New();
  //medianFilter->SetInput(spTmpItkImg);	

  MedianFilterType::InputSizeType radius;
  radius[0] = qRound(iMedianSizeX/2.0);
  radius[1] = qRound(iMedianSizeY/2.0);

  medianFilter->SetRadius(radius);
  medianFilter->SetInput(spTmpItkImg);
  medianFilter->Update();


  spTmpItkImg = medianFilter->GetOutput();

  CopyItkImage2YKImage(spTmpItkImg, this);

}

UnsignedShortImageType::Pointer YK16GrayImage::CloneItkImage()
{
  if (m_pData == NULL)
	return NULL;

  UnsignedShortImageType::Pointer spTmpItkImg = UnsignedShortImageType::New();	

  UnsignedShortImageType::SizeType size;
  size[0] = m_iWidth;
  size[1] = m_iHeight;
  UnsignedShortImageType::IndexType idxStart;
  idxStart[0] = 0;
  idxStart[1] = 0;
  UnsignedShortImageType::SpacingType spacing;	
  if (m_fSpacingX*m_fSpacingY == 0)
  {
	spacing[0] = 1.0;
	spacing[1] = 1.0;
  }
  else
  {
	spacing[0] = m_fSpacingX;
	spacing[1] = m_fSpacingY;
  }
  UnsignedShortImageType::PointType origin;
  origin[0] = size[0]*spacing[0]/-2.0;
  origin[1] = size[1]*spacing[1]/-2.0;

  UnsignedShortImageType::RegionType region;
  region.SetSize(size);
  region.SetIndex(idxStart);

  spTmpItkImg->SetRegions(region);
  spTmpItkImg->SetSpacing(spacing);
  spTmpItkImg->SetOrigin(origin);
  spTmpItkImg->Allocate();

  //Raw File open	
  //UnsignedShortImageType::SizeType tmpSize = 
  //UnsignedShortImageType::RegionType region = spTmpItkImg->GetRequestedRegion();
  //UnsignedShortImageType::SizeType tmpSize = region.GetSize();

  //int sizeX = tmpSize[0];
  //int sizeY = tmpSize[1];

  //if (sizeX < 1 || sizeY <1)
  //	return;

  itk::ImageRegionIterator<UnsignedShortImageType> it(spTmpItkImg, spTmpItkImg->GetRequestedRegion());

  int i = 0;
  for (it.GoToBegin() ; !it.IsAtEnd(); ++it)
  {
	it.Set(m_pData[i]);
	i++;
  }

  return spTmpItkImg;

}

void YK16GrayImage::ResampleImage( double fResampleFactor )
{
  if (m_pData == NULL)
	return;

  if (fResampleFactor <= 0)
	return;

  m_fResampleFactor = fResampleFactor;

  UnsignedShortImageType::SizeType inputSize;
  inputSize[0] = m_iWidth;
  inputSize[1] = m_iHeight;

  UnsignedShortImageType::SizeType outputSize;
  outputSize[0] = qRound(m_iWidth*fResampleFactor);
  outputSize[1] = qRound(m_iHeight*fResampleFactor);
  //m_iWidth = outputSize[0];
  //m_iHeight = outputSize[1];  	 

  UnsignedShortImageType::SpacingType outputSpacing;

  if (m_fSpacingX <=0 ||m_fSpacingY <=0)
  {
	m_fSpacingX = 1.0;
	m_fSpacingY = 1.0;
  }

  outputSpacing[0] = m_fSpacingX * (static_cast<double>(inputSize[0]) / static_cast<double>(outputSize[0]));
  outputSpacing[1] = m_fSpacingY * (static_cast<double>(inputSize[1]) / static_cast<double>(outputSize[1]));		  

  UnsignedShortImageType::Pointer input = CloneItkImage();//returns Ushort itk image from data buf
  UnsignedShortImageType::PointType outputOrigin = input->GetOrigin(); //-204.6 - 204.6  0  

  //// Resample the image
  //typedef itk::IdentityTransform<float, 2> TransformType;
  typedef itk::ResampleImageFilter<UnsignedShortImageType, UnsignedShortImageType, float> ResampleImageFilterType;
  ResampleImageFilterType::Pointer resample = ResampleImageFilterType::New();

  typedef itk::AffineTransform< float, 2 >  TransformType;
  TransformType::Pointer transform = TransformType::New();
  typedef itk::NearestNeighborInterpolateImageFunction<UnsignedShortImageType, float >  InterpolatorType;
  InterpolatorType::Pointer interpolator = InterpolatorType::New();
  transform->SetIdentity();

  resample->SetInput(input);
  resample->SetOutputDirection( input->GetDirection() );
  resample->SetInterpolator(interpolator);
  resample->SetDefaultPixelValue( 50 );
  resample->SetSize(outputSize);
  resample->SetOutputSpacing(outputSpacing);
  resample->SetOutputOrigin(outputOrigin);  
  resample->SetTransform(transform);
  resample->Update();
  
  UnsignedShortImageType::Pointer outputImg = resample->GetOutput();
  UpdateFromItkImage(outputImg); //itk --> YKImage

}

void YK16GrayImage::UpdateFromItkImage( UnsignedShortImageType::Pointer& spRefItkImg )
{
  if (!spRefItkImg)
	return;

  if (m_pData != NULL)
  {
	delete [] m_pData;
	m_pData = NULL;
  }
  if (m_pPixmap != NULL)
  {
	delete m_pPixmap;		m_pPixmap = NULL;
  }	

  UnsignedShortImageType::SizeType size = spRefItkImg->GetRequestedRegion().GetSize();
  UnsignedShortImageType::SpacingType spacing = spRefItkImg->GetSpacing();

  m_iWidth = size[0];
  m_iHeight = size[1];

  m_pData = new unsigned short [m_iWidth*m_iHeight];

  itk::ImageRegionIterator<UnsignedShortImageType> it(spRefItkImg, spRefItkImg->GetRequestedRegion());

  int i = 0;
  for (it.GoToBegin() ; !it.IsAtEnd() ; ++it)
  {
	m_pData[i] = it.Get();
	i++;
  }

}

void YK16GrayImage::UpdateFromItkImageFloat( FloatImageType2D::Pointer& spRefItkImg )
{
  if (!spRefItkImg)
	return;

  if (m_pData != NULL)
  {
	delete [] m_pData;
	m_pData = NULL;
  }
  if (m_pPixmap != NULL)
  {
	delete m_pPixmap;		m_pPixmap = NULL;
  }	

  FloatImageType2D::SizeType size = spRefItkImg->GetRequestedRegion().GetSize();
  FloatImageType2D::SpacingType spacing = spRefItkImg->GetSpacing();

  m_iWidth = size[0];
  m_iHeight = size[1];

  m_pData = new unsigned short [m_iWidth*m_iHeight];

  itk::ImageRegionIterator<FloatImageType2D> it(spRefItkImg, spRefItkImg->GetRequestedRegion());

  int i = 0;
  for (it.GoToBegin() ; !it.IsAtEnd() ; ++it)
  {
	float curVal =it.Get();
	unsigned short outVal;

	if (curVal < 0.0)
	  outVal = 0;
	else if (curVal > 65535.0)
	  outVal  = 65535;
	else
	  outVal = (unsigned short)qRound(curVal);

	m_pData[i] = outVal;		
	i++;
  }

}

void YK16GrayImage::InvertImage()
{
  if (m_pData == NULL)
	return;

  int imgSize= m_iWidth * m_iHeight;
  //Data inversion: Default for Elekta XVI system // ELEKTA VS VARIAN -> Works great like it is
  for (int i = 0 ; i<imgSize ; i++)
  {
	m_pData[i] = 65535 - m_pData[i];
  }	

}