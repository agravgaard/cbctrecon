// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++, C#, and Java:
// http://www.viva64.com

#if USE_TINYREFL
#include "cbctrecon.h"
#include "cbctrecon.h.tinyrefl"
#include <tinyrefl/api.hpp>
#else
#include "cbctrecon.h"
#endif

#define USE_AVX false
#if USE_AVX
#include <immintrin.h>
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#include <cstdio>
#include <valarray>
#include <memory>

// Qt
#include <qdir.h>
#include <qinputdialog.h>
#include <qxmlstream.h>

// ITK
#include <itkAbsImageFilter.h>
#include <itkAddImageFilter.h>
#include <itkBinaryBallStructuringElement.h>
#include <itkBinaryDilateImageFilter.h>
#include <itkBinaryErodeImageFilter.h>
#include <itkBinaryFillholeImageFilter.h>
#include <itkBinaryThresholdImageFilter.h>
#include <itkFlipImageFilter.h>
#include <itkImageDuplicator.h>
#include <itkImageSliceConstIteratorWithIndex.h>
#include <itkImageSliceIteratorWithIndex.h>
#include <itkMaskImageFilter.h>
#include <itkMedianImageFilter.h>
#include <itkMinimumMaximumImageCalculator.h>
#include <itkMultiplyImageFilter.h>
#include <itkNearestNeighborInterpolateImageFunction.h>
#include <itkRegularExpressionSeriesFileNames.h>
#include <itkResampleImageFilter.h>
#include <itkSmoothingRecursiveGaussianImageFilter.h>
#include <itkTimeProbe.h>

#ifdef LOWPASS_FFT
// ITK Low-pass fourier filter
#include <itkFFTShiftImageFilter.h>
#include <itkForwardFFTImageFilter.h>
#include <itkGaussianImageSource.h>
#include <itkInverseFFTImageFilter.h>
#include <itkWrapPadImageFilter.h>
#endif // LOWPASS_FFT

// RTK includes
#include <rtkConstantImageSource.h>
#include <rtkFieldOfViewImageFilter.h>
#include <rtkProjectionsReader.h>
#include <rtkThreeDCircularProjectionGeometry.h>
#include <rtkThreeDCircularProjectionGeometryXMLFile.h>
#include <rtkVarianObiGeometryReader.h>
#include <rtkVarianProBeamGeometryReader.h>

#if USE_CUDA
#include <itkCudaImage.h>
using CUDAFloatImageType = itk::CudaImage<float, 3U>;
#endif // USE_CUDA

// Plastimatch
#undef TIMEOUT
#undef CUDA_FOUND
/* #include <beam_calc.h> // used to be rt_beam in earlier plm
#include <dcmtk_rt_study.h>
#include <itk_image_type.h>
#include <plan_calc.h> // used to be rt_plan in earlier plm
#include <proj_matrix.h>
#include <proj_volume.h>
#include <ray_data.h>
#include <volume_adjust.h> */

// Local
#include "AG17RGBAImage.h"
#include "YK16GrayImage.h"
#include "OpenCLFFTFilter.h"
#include "StructureSet.h"
#include "WEPL.h"
#include "YK16GrayImage.h"
#include "cbctrecon_compute.h"
#include "cbctrecon_io.h"

CbctRecon::CbctRecon() {

  m_dspYKReconImage = std::make_unique<YK16GrayImage>();
  m_dspYKImgProj = std::make_unique<YK16GrayImage>();

  // Badpixmap;
  m_pImgOffset = std::make_unique<YK16GrayImage>(DEFAULT_ELEKTA_PROJ_WIDTH,
                                                 DEFAULT_ELEKTA_PROJ_HEIGHT);
  m_pImgGain = std::make_unique<YK16GrayImage>(DEFAULT_ELEKTA_PROJ_WIDTH,
                                               DEFAULT_ELEKTA_PROJ_HEIGHT);
  // Prepare Raw image

  m_bScanDirectionCW = true;

  m_iTmpIdx = 60;

  m_fProjImgValueMax = 0.0; // value of float image
  m_fProjImgValueMin = 0.0;

  m_arrYKBufProj.clear();
  m_iCntSelectedProj = 0;

  m_structures = std::make_unique<StructureSet>();

  m_iFixedOffset_ScatterMap = 10000; // fixed! allows negative value of scatter
  // m_iFixedOffset_ScatterMap = 0;//fixed! allows negative value of scatter
  m_fResampleF = 1.0;
  m_fProjSpacingX = 0.4; // DEFAULT, will be updated during Load Proj selected
  m_fProjSpacingY = 0.4;

  m_strPathDirDefault = QDir::currentPath();
  std::cout << "Current Default Dir: "
            << m_strPathDirDefault.toLocal8Bit().constData() << std::endl;

  // shell test
  // QString strCurFolder = "H:\\lib\\rtk\\NightlyBUILD64\\bin\\Release";
  // QString strCommand = QString("explorer %1").arg(strCurFolder);
  // std::cout << strCommand.toLocal8Bit().constData() << std::endl;
  //::system(strCommand.toLocal8Bit().constData());
  //::system("rtkfdk");

  //	if
  //(QProcess::execute(QString("H:\\lib\\rtk\\NightlyBUILD64\\bin\\Release\\rtkfdk"))
  //< 0) 	qDebug() << "Failed to run";
  m_bMacroContinue = true;
}

CbctRecon::~CbctRecon() { ReleaseMemory(); }

void CbctRecon::ReleaseMemory() {
  if (!m_arrYKImage.empty()) {
    // m_iImgCnt = 0;
    m_arrYKImage.clear();
  }

  if (!m_arrYKBufProj.empty()) {
    m_arrYKBufProj.clear();
    m_iCntSelectedProj = 0;
  }
}

// Hexa name ->decimal name

void CbctRecon::RenameFromHexToDecimal(QStringList &filenameList) const {
  const auto size = filenameList.size();

  for (auto i = 0; i < size; i++) {
    const auto &crntFilePath = filenameList.at(i);
    auto fileInfo = QFileInfo(crntFilePath);
    auto dir = fileInfo.absoluteDir();
    auto fileBase = fileInfo.baseName();
    auto newBaseName = HexStr2IntStr(fileBase);
    auto extStr = fileInfo.completeSuffix();

    auto newFileName = newBaseName.append(".").append(extStr);
    auto newPath = dir.absolutePath() + "/" + newFileName;

    // extract former part
    QFile::rename(crntFilePath, newPath);
  }
  // Extract
}

bool CbctRecon::FillProjForDisplay(const int slice_number) {
  // Using slice iterator,
  // 1) Find the slice requested
  // 2) get dimension to create 2DYK16Image
  // 3) copy slice region to YK16 Image --> Cating: float to USHORT

  if (m_spProjImg3DFloat == nullptr) {
    return false;
  }

  itk::ImageSliceConstIteratorWithIndex<FloatImageType> it(
      m_spProjImg3DFloat, m_spProjImg3DFloat->GetRequestedRegion());

  auto imgSize =
      m_spProjImg3DFloat->GetRequestedRegion().GetSize(); // 1016x1016 x z

  const int width = imgSize[0];
  const int height = imgSize[1];
  m_dspYKImgProj->CreateImage(width, height, 0);

  it.SetFirstDirection(0);  // x?
  it.SetSecondDirection(1); // y?
  it.GoToBegin();

  const auto realValGap = m_fProjImgValueMax - m_fProjImgValueMin;
  m_multiplyFactor = 0.0;

  if (realValGap > 0.0) {
    m_multiplyFactor = 65535.0 / realValGap;
  }

  auto i_num_slice = 0;
  while (!it.IsAtEnd()) {
    if (i_num_slice == slice_number) {
      auto i_num_height = 0;
      while (!it.IsAtEndOfSlice()) {
        auto i_num_width = 0;
        while (!it.IsAtEndOfLine()) {
          // double tmpVal = it.Get()*multiplyFactor;
          const double tmp_val = it.Get();

          m_dspYKImgProj->m_pData[i_num_width + width * i_num_height] =
              static_cast<unsigned short>((tmp_val - m_fProjImgValueMin) *
                                          m_multiplyFactor);
          // it.Set() doesn't exist in the Const Iterator
          ++it;
          i_num_width++;
        }
        it.NextLine();
        i_num_height++;
      }
      // break;
    }
    it.NextSlice();
    i_num_slice++;
  }

  return true;
}

int hex_to_int(const char ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  if (ch >= 'A' && ch <= 'F') {
    return ch - 'A' + 10;
  }
  if (ch >= 'a' && ch <= 'f') {
    return ch - 'a' + 10;
  }
  return -1;
}

QString CbctRecon::HexStr2IntStr(QString &str_hex) const {
  auto hex_str = str_hex.toStdString();
  std::reverse(std::begin(hex_str), std::end(hex_str));
  auto tmpDecimal = 0;
  // int cnt = 0;
  auto inv_i = hex_str.size() - 1;
  for (auto &hex : hex_str) {
    const auto tmp_num = hex_to_int(hex);
    tmpDecimal += tmp_num * static_cast<int>(pow(16.0, inv_i--));
  }

  auto int_str = QString("%1").arg(tmpDecimal);

  return int_str;
  // return tmpDecimal;
  // m_str_10.Format("%d", tmpDecimal);
}

void CbctRecon::LoadCalibData(std::string &filepath,
                              const enCalibType calib_type) {
  switch (calib_type) {
  case GAIN_CALIB:
    m_pImgGain->LoadRawImage(filepath.c_str(), DEFAULT_ELEKTA_PROJ_WIDTH,
                             DEFAULT_ELEKTA_PROJ_HEIGHT);
    break;
  case OFFSET_CALIB:
    m_pImgOffset->LoadRawImage(filepath.c_str(), DEFAULT_ELEKTA_PROJ_WIDTH,
                               DEFAULT_ELEKTA_PROJ_HEIGHT);
    break;
  case BADPIXEL_CALIB:
    LoadBadPixelMap(filepath.c_str());
    break;
  default:
    break;
  }
}

std::unique_ptr<YK16GrayImage>
CbctRecon::ApplyCalibrationMaps(YK16GrayImage *const &rawImg,
                                const bool DarkCorr, const bool GainCorr,
                                const bool DefectCorr) {
  auto corrImg = std::make_unique<YK16GrayImage>(DEFAULT_ELEKTA_PROJ_WIDTH,
                                                 DEFAULT_ELEKTA_PROJ_HEIGHT);

  // m_pParent->m_pCurrImageRaw->m_pData[i]

  if (!DarkCorr && !GainCorr) {
    for (auto i = 0; i < DEFAULT_ELEKTA_PROJ_WIDTH * DEFAULT_ELEKTA_PROJ_HEIGHT;
         i++) {
      corrImg->m_pData[i] = rawImg->m_pData[i]; // raw image
    }
  } else if (DarkCorr && !GainCorr) {
    if (m_pImgOffset->IsEmpty()) {
      for (auto i = 0;
           i < DEFAULT_ELEKTA_PROJ_WIDTH * DEFAULT_ELEKTA_PROJ_HEIGHT; i++) {
        corrImg->m_pData[i] = rawImg->m_pData[i]; // raw image
      }
    } else {
      for (auto i = 0;
           i < DEFAULT_ELEKTA_PROJ_WIDTH * DEFAULT_ELEKTA_PROJ_HEIGHT; i++) {
        if (rawImg->m_pData[i] > m_pImgOffset->m_pData[i]) {
          corrImg->m_pData[i] = rawImg->m_pData[i] - m_pImgOffset->m_pData[i];
        } else {
          corrImg->m_pData[i] = 0;
        }
      }
    }
  } else if (!DarkCorr && GainCorr) {
    if (m_pImgGain->IsEmpty()) {
      for (auto i = 0;
           i < DEFAULT_ELEKTA_PROJ_WIDTH * DEFAULT_ELEKTA_PROJ_HEIGHT; i++) {
        corrImg->m_pData[i] = rawImg->m_pData[i]; // raw image
      }
    } else {
      // get a mean value for m_pGainImage
      auto sum = 0.0;
      for (auto i = 0;
           i < DEFAULT_ELEKTA_PROJ_WIDTH * DEFAULT_ELEKTA_PROJ_HEIGHT; i++) {
        sum = sum + m_pImgGain->m_pData[i];
      }
      const auto MeanVal =
          sum / static_cast<double>(DEFAULT_ELEKTA_PROJ_WIDTH *
                                    DEFAULT_ELEKTA_PROJ_HEIGHT);

      for (auto i = 0;
           i < DEFAULT_ELEKTA_PROJ_WIDTH * DEFAULT_ELEKTA_PROJ_HEIGHT; i++) {
        if (m_pImgGain->m_pData[i] == 0) {
          corrImg->m_pData[i] = rawImg->m_pData[i];
        } else {
          corrImg->m_pData[i] = static_cast<unsigned short>(
              static_cast<double>(rawImg->m_pData[i]) /
              static_cast<double>(m_pImgGain->m_pData[i]) * MeanVal);
        }
      }
    }
  }

  else if (DarkCorr && GainCorr) {
    auto bRawImage = false;
    if (m_pImgOffset->IsEmpty()) {
      bRawImage = true;
    }
    if (m_pImgGain->IsEmpty()) {
      bRawImage = true;
    }

    if (bRawImage) {
      for (auto i = 0;
           i < DEFAULT_ELEKTA_PROJ_WIDTH * DEFAULT_ELEKTA_PROJ_HEIGHT; i++) {
        corrImg->m_pData[i] = rawImg->m_pData[i]; // raw image
      }
    } else // if not raw image
    {
      // get a mean value for m_pGainImage
      auto sum = 0.0;
      for (auto i = 0;
           i < DEFAULT_ELEKTA_PROJ_WIDTH * DEFAULT_ELEKTA_PROJ_HEIGHT; i++) {
        sum = sum + (m_pImgGain->m_pData[i] - m_pImgOffset->m_pData[i]);
      }
      const auto MeanVal =
          sum / static_cast<double>(DEFAULT_ELEKTA_PROJ_WIDTH *
                                    DEFAULT_ELEKTA_PROJ_HEIGHT);

      auto iDenomLessZero = 0;
      auto iDenomLessZero_RawIsGreaterThanDark = 0;
      auto iDenomLessZero_RawIsSmallerThanDark = 0;
      auto iDenomOK_RawValueMinus = 0;
      auto iValOutOfRange = 0;

      for (auto i = 0;
           i < DEFAULT_ELEKTA_PROJ_WIDTH * DEFAULT_ELEKTA_PROJ_HEIGHT; i++) {
        const auto denom = static_cast<double>(m_pImgGain->m_pData[i] -
                                               m_pImgOffset->m_pData[i]);

        if (denom <= 0) {
          iDenomLessZero++;

          if (rawImg->m_pData[i] > m_pImgOffset->m_pData[i]) {
            corrImg->m_pData[i] = rawImg->m_pData[i] - m_pImgOffset->m_pData[i];
            iDenomLessZero_RawIsGreaterThanDark++;
          } else {
            corrImg->m_pData[i] = 0;
            iDenomLessZero_RawIsSmallerThanDark++;
          }
        } else {
          const auto tmpVal =
              (rawImg->m_pData[i] - m_pImgOffset->m_pData[i]) / denom * MeanVal;

          if (tmpVal < 0) {
            corrImg->m_pData[i] = 0;
            iDenomOK_RawValueMinus++;
          } else {
            if (tmpVal > 65535) { // 16bit max value
              iValOutOfRange++;
            }

            corrImg->m_pData[i] = static_cast<unsigned short>(tmpVal);
          }
        }
      } // end of for
    }   // end if not bRawImage

  } // else if (m_bDarkCorrApply && m_bGainCorrApply)

  if (DefectCorr && !m_vPixelReplMap.empty()) // pixel replacement
  {
    corrImg = BadPixReplacement(std::move(corrImg));
  }

  return corrImg;
}

QString CbctRecon::CorrectSingleFile(const char *filePath, const bool DarkCorr,
                                     const bool GainCorr,
                                     const bool DefectCorr) {
  // Load raw file
  auto rawImg = std::make_unique<YK16GrayImage>(DEFAULT_ELEKTA_PROJ_WIDTH,
                                                DEFAULT_ELEKTA_PROJ_HEIGHT);
  rawImg->LoadRawImage(filePath, DEFAULT_ELEKTA_PROJ_WIDTH,
                       DEFAULT_ELEKTA_PROJ_HEIGHT);

  const auto corrImg =
      ApplyCalibrationMaps(rawImg.get(), DarkCorr, GainCorr, DefectCorr);
  // filePath
  // QString exportName = filePath;
  // corrImg.SaveDataAsRaw();
  const auto endFix = QString("_CORR");

  auto srcFileInfo = QFileInfo(filePath);
  auto dir = srcFileInfo.absoluteDir();
  auto baseName = srcFileInfo.baseName();
  const auto extName = srcFileInfo.completeSuffix();

  const auto newFileName = baseName.append(endFix).append(".").append(extName);
  auto newPath = dir.absolutePath() + "/" + newFileName;

  if (!corrImg->SaveDataAsRaw(newPath.toLocal8Bit().constData())) {
    std::cerr << "Could not save as Raw in: " << newPath.toStdString()
              << std::endl;
  }

  return newPath;
  // corrImg.ReleaseBuffer();
}

void CbctRecon::CorrectSingleFile(YK16GrayImage *pYKRawImg, const bool DarkCorr,
                                  const bool GainCorr, const bool DefectCorr) {
  if (pYKRawImg == nullptr) {
    return;
  }

  const auto corrImg =
      ApplyCalibrationMaps(pYKRawImg, DarkCorr, GainCorr, DefectCorr);

  // Replace old buffer with new one.
  pYKRawImg->CopyFromBuffer(corrImg->m_pData, corrImg->m_iWidth,
                            corrImg->m_iHeight);
}

void CbctRecon::LoadBadPixelMap(const char *filePath) {
  m_vPixelReplMap.clear();

  std::ifstream fin;
  fin.open(filePath);

  if (fin.fail()) {
    return;
  }

  char str[MAX_LINE_LENGTH];
  // memset(str, 0, MAX_LINE_LENGTH);

  while (!fin.eof()) {
    memset(&str[0], 0, MAX_LINE_LENGTH);
    fin.getline(&str[0], MAX_LINE_LENGTH);
    auto tmpStr = QString(&str[0]);

    if (tmpStr.contains("#ORIGINAL_X")) {
      break;
    }
  }

  while (!fin.eof()) {
    memset(&str[0], 0, MAX_LINE_LENGTH);
    fin.getline(&str[0], MAX_LINE_LENGTH);
    auto tmpStr = QString(&str[0]);

    auto strList = tmpStr.split("	");

    if (strList.size() == 4) {
      BADPIXELMAP tmpData{};
      tmpData.BadPixX = strList.at(0).toInt();
      tmpData.BadPixY = strList.at(1).toInt();
      tmpData.ReplPixX = strList.at(2).toInt();
      tmpData.ReplPixY = strList.at(3).toInt();
      m_vPixelReplMap.push_back(tmpData);
    }
  }
  fin.close();
}

std::unique_ptr<YK16GrayImage>
CbctRecon::BadPixReplacement(std::unique_ptr<YK16GrayImage> targetImg) {
  if (m_vPixelReplMap.empty()) {
    return targetImg;
  }

  for (auto &it : m_vPixelReplMap) {
    const auto tmpData = it;
    const auto oriIdx =
        tmpData.BadPixY * DEFAULT_ELEKTA_PROJ_WIDTH + tmpData.BadPixX;
    const auto replIdx =
        tmpData.ReplPixY * DEFAULT_ELEKTA_PROJ_WIDTH + tmpData.ReplPixX;
    targetImg->m_pData[oriIdx] = targetImg->m_pData[replIdx];
  }

  return targetImg;
}

void CbctRecon::SetProjDir(QString &strProjPath) {
  m_strPathGeomXML.clear();
  m_strPathDirDefault = strProjPath;

  const UShortImageType::Pointer spNull;

  m_spCrntReconImg = spNull;     // fixed image // ID: RawCBCT
  m_spRawReconImg = spNull;      // just added --> when file is loaded
  m_spScatCorrReconImg = spNull; // just added --> after scatter correction

  FindAllRelevantPaths(strProjPath);
}

std::vector<std::string>
CbctRecon::GetProjFileNames(QString &dirPath) // main loading fuction for
                                              // projection images
{

  m_iImgCnt = 0; // should be reset
  m_iCntSelectedProj = 0;
  ReleaseMemory(); // only reset mem for indepent projection images

  std::string regexp;
  switch (m_projFormat) {
  case HIS_FORMAT:
    regexp = ".[0-9a-fA-F].his";
    break;
  case HND_FORMAT:
    regexp = "Proj_.*.hnd";
    break;
  case XIM_FORMAT:
    regexp = "Proj_.*.xim";
    break;
  }
  auto regexpnames = itk::RegularExpressionSeriesFileNames::New();
  regexpnames->SetDirectory(dirPath.toLocal8Bit().constData());
  // regexpnames->SetNumericSort(false);
  regexpnames->SetNumericSort(true); // doesn't work with hexadecimal. and
                                     // [true/false] doesn't mean ascending or
                                     // descending
  regexpnames->SetRegularExpression(regexp);
  regexpnames->SetSubMatch(
      1); // SetSubMatch(0) led to sorting from last digit of the name

  auto names = regexpnames->GetFileNames();

  rtk::RegisterIOFactories();
  std::vector<size_t> idxtopop;
  for (auto &fn : names) {
    auto imageio = itk::ImageIOFactory::CreateImageIO(
        fn.c_str(), itk::ImageIOFactory::ReadMode);

    if (imageio.IsNull()) {
      idxtopop.push_back(&fn - &names[0]);
    }
  }
  std::reverse(idxtopop.begin(), idxtopop.end());
  for (auto &id : idxtopop) {
    names.erase(names.begin() + id);
  }

  return names;
}

bool CbctRecon::LoadGeometry(QFileInfo &geomFileInfo,
                             std::vector<std::string> &names) {

  if (!geomFileInfo.exists()) {
    std::cout << "Critical Error! geometry file is not existing. Please retry."
              << std::endl;
    return false;
  }

  if (geomFileInfo.fileName() == "_Frames.xml") // this is XVI XML.
  {
    std::cout << "XVI Geometry File was found. This will be temporarily used:"
              << geomFileInfo.fileName().toLocal8Bit().constData() << std::endl;
    const auto success =
        LoadXVIGeometryFile(geomFileInfo.absoluteFilePath()
                                .toLocal8Bit()
                                .constData()); // will generate m_spFullGeometry
    if (!success) {
      return false;
    }
  } else if (geomFileInfo.fileName() ==
             "ProjectionInfo.xml") // this is OBI XML.
  {
    std::cout
        << "Varian XML Geometry File was found. This will be temporarily used:"
        << geomFileInfo.fileName().toLocal8Bit().constData() << std::endl;
    auto reader = rtk::VarianObiGeometryReader::New();
    reader->SetXMLFileName(geomFileInfo.fileName().toLocal8Bit().constData());
    reader->SetProjectionsFileNames(names);
    reader->UpdateOutputData();
    // Write
    auto xmlWriter = rtk::ThreeDCircularProjectionGeometryXMLFileWriter::New();
    xmlWriter->SetFilename("RTKgeometry.xml");
    xmlWriter->SetObject(reader->GetGeometry());
    TRY_AND_EXIT_ON_ITK_EXCEPTION(xmlWriter->WriteFile());
    std::cout << "RTK standard Geometry XML File was created:"
              << "RTKgeometry.xml" << std::endl;
    LoadRTKGeometryFile("RTKgeometry.xml");
    // ::::::::::::::::::::::::::::LoadXMLGeometryFile(geomPath.toLocal8Bit().constData());
    // //will generate m_spFullGeometry
  } else if (geomFileInfo.fileName() == "Scan.xml") // this is XIM XML.
  {
    std::cout << "Varian Xim XML Geometry File was found. This will be "
                 "temporarily used:"
              << geomFileInfo.fileName().toLocal8Bit().constData() << std::endl;
    auto reader = rtk::VarianProBeamGeometryReader::New();
    reader->SetXMLFileName(
        geomFileInfo.absoluteFilePath().toLocal8Bit().constData());
    reader->SetProjectionsFileNames(names);
    reader->UpdateOutputData();
    // Write
    auto xmlWriter = rtk::ThreeDCircularProjectionGeometryXMLFileWriter::New();
    xmlWriter->SetFilename("RTKgeometry.xml");
    xmlWriter->SetObject(reader->GetGeometry());
    xmlWriter->WriteFile();
    std::cout << "RTK standard Geometry XML File was created:"
              << "RTKgeometry.xml" << std::endl;
    LoadRTKGeometryFile("RTKgeometry.xml");
    std::cout << "Done!";
    // ::::::::::::::::::::::::::::LoadXMLGeometryFile(geomPath.toLocal8Bit().constData());
    // //will generate m_spFullGeometry
  } else {
    std::cout << "RTK standard Geometry XML File was found:"
              << geomFileInfo.absoluteFilePath().toLocal8Bit().constData()
              << std::endl;
    LoadRTKGeometryFile(geomFileInfo.absoluteFilePath()
                            .toLocal8Bit()
                            .constData()); // will generate m_spFullGeometry
  }
  return true;
}

std::vector<int>
CbctRecon::GetExcludeProjFiles(const bool bManAngleGap,
                               const double gantryAngleInterval) {
  ///////////////////////////////////Exclude outlier projection files
  auto angle_gaps =
      m_spFullGeometry->GetAngularGaps(m_spFullGeometry->GetSourceAngles());

  auto sum_gap =
      std::accumulate(std::begin(angle_gaps), std::end(angle_gaps), 0.0);
  sum_gap /= itk::Math::pi * 180.0;

  auto &gantry_angles = m_spFullGeometry->GetGantryAngles();
  std::vector<int> vSelectedIdx;
  std::vector<int> vSelectedIdx_final;
  std::vector<int> vExcludeIdx;

  if (bManAngleGap) {
    // Select indices for recon
    // Generate norminal gantry values from the first angle
    const auto firstAngle = gantry_angles.at(0);
    const auto lastAngle = gantry_angles.at(gantry_angles.size() - 1);

    std::vector<double> vNormAngles;

    const auto multiSize = std::lround(sum_gap / gantryAngleInterval) + 2;

    // CW only (179.xx -> 181.xx -> 359.xx --> 1.xx --> 179.xx), CCW should be
    // checked later
    for (auto i = 0; i < multiSize; i++) {
      auto curAngle = 0.0;

      if (m_bScanDirectionCW) {
        curAngle = firstAngle + i * gantryAngleInterval;
        if (curAngle >= 360.0) {
          curAngle = curAngle - 360.0;
        }
      } else {
        curAngle = firstAngle - i * gantryAngleInterval;
        if (curAngle < 0.0) {
          curAngle = curAngle + 360.0;
        }
      }
      // Don't add last gantry angle if their intervals are too small.

      // Last data will be added at the last part
      if (i > multiSize - 5) // last parts of the data
      {
        if (m_bScanDirectionCW) {
          if (curAngle <=
              lastAngle - gantryAngleInterval / 2.0) // from 5 latest indices,
          {
            vNormAngles.push_back(curAngle);
          }
        } else {
          if (curAngle >=
              lastAngle - gantryAngleInterval / 2.0) // from 5 latest indices,
          {
            vNormAngles.push_back(curAngle);
          }
        }
        // gantryAngleInterval/2.0 is given to remove "very near" value to the
        // last value
      } else {
        vNormAngles.push_back(curAngle);
      }
    }
    vNormAngles.push_back(lastAngle);

    for (auto vNormAngle : vNormAngles) {
      std::cout << "Nominal proj. angle: ";
      std::cout << vNormAngle << std::endl;
    }

    // Collect appropriate indices
    GetSelectedIndices(gantry_angles, vNormAngles, vSelectedIdx,
                       m_bScanDirectionCW, vExcludeIdx);

    for (auto &it_idx : vSelectedIdx) {
      std::cout << "Index: " << it_idx << "     "
                << "GantryAngle: " << gantry_angles.at(it_idx) << std::endl;
    }
  } else // not manual
  {
    for (auto i = 0U; i < gantry_angles.size(); i++) {
      if (std::find(vExcludeIdx.begin(), vExcludeIdx.end(), i) ==
          vExcludeIdx.end()) { // if i is not included in vExcludeIdx
        vSelectedIdx.push_back(i);
      }
    }
  }
  // Another exlusion for kV off images

  vSelectedIdx_final.clear();

  // std::vector<int>::iterator itExclude;
  // for (itExclude = m_vExcludeProjIdx.begin(); itExclude !=
  // m_vExcludeProjIdx.end(); ++itExclude)
  //{
  //    int idx = (*itExclude);
  //    //if (std::find(m_vExcludeProjIdx.begin(), m_vExcludeProjIdx.end(),
  //    curIdx) == m_vExcludeProjIdx.end()) // if i is not included in
  //    vExcludeIdx
  //    //    vSelectedIdx_final.push_back(curIdx);

  //    std::cout << "Exclude " << idx << std::endl;
  //}

  m_vExcludeProjIdx.clear();

  for (auto &it_final : vSelectedIdx) {
    if (std::find(m_vExcludeProjIdx.begin(), m_vExcludeProjIdx.end(),
                  it_final) ==
        m_vExcludeProjIdx.end()) { // if i is not included in vExcludeIdx
      vSelectedIdx_final.push_back(it_final);
    }
  }

  std::cout << "Total proj count: " << vSelectedIdx.size() << std::endl;

  return vSelectedIdx_final;
}

void CbctRecon::LoadSelectedProj(const std::vector<int> &exclude_ids,
                                 const std::vector<std::string> &names) {
  // 3) Seletively load projection file

  // Regenerate geometry object
  m_spCustomGeometry = GeometryType::New();

  // for (itIdx =vSelectedIdx.begin() ; itIdx != vSelectedIdx.end() ; itIdx++ )
  // #pragma omp parallel for private(itIdx) shared(m_spCustomGeometry)
  // schedule(static)
  for (auto &it_idx : exclude_ids) {
    // 9 parameters are required
    const auto curSID =
        m_spFullGeometry->GetSourceToIsocenterDistances().at(it_idx);
    const auto curSDD =
        m_spFullGeometry->GetSourceToDetectorDistances().at(it_idx);
    auto curGantryAngle = m_spFullGeometry->GetGantryAngles().at(it_idx);
    const auto kVAng =
        curGantryAngle * 180.0 *
        itk::Math::one_over_pi; // 360 / 2 = 180 radians to degrees
    auto MVAng = kVAng - (m_projFormat == HIS_FORMAT ? 0.0 : 90.0);
    if (MVAng < 0.0) {
      MVAng = MVAng + 360.0;
    }
    curGantryAngle = MVAng;

    const auto curProjOffsetX =
        m_spFullGeometry->GetProjectionOffsetsX().at(it_idx);
    const auto curProjOffsetY =
        m_spFullGeometry->GetProjectionOffsetsY().at(it_idx);

    const auto curOutOfPlaneAngles =
        m_spFullGeometry->GetOutOfPlaneAngles().at(it_idx);
    const auto curInPlaneAngles =
        m_spFullGeometry->GetInPlaneAngles().at(it_idx);

    const auto curSrcOffsetX = m_spFullGeometry->GetSourceOffsetsX().at(it_idx);
    const auto curSrcOffsetY = m_spFullGeometry->GetSourceOffsetsY().at(it_idx);

    m_spCustomGeometry->AddProjection(
        curSID, curSDD, curGantryAngle, curProjOffsetX,
        curProjOffsetY, // Flexmap, For Xim, these are 0
        curOutOfPlaneAngles,
        curInPlaneAngles,              // In elekta and varian, these are 0
        curSrcOffsetX, curSrcOffsetY); // In elekta and varian, these are 0
  }

  std::cout << "Excluded proj count: " << m_vExcludeProjIdx.size() << std::endl;
  std::cout << "Final proj count: " << exclude_ids.size() << std::endl;

  // Regenerate fileNames and geometry object based on the selected indices.

  if (!m_vSelectedFileNames.empty()) {
    m_vSelectedFileNames.clear();
  }

  std::ofstream fout;
  fout.open("D:/DebugFileNames.txt");

  for (auto &it_idx : exclude_ids) {
    const auto &cur_str = names.at(it_idx);
    m_vSelectedFileNames.push_back(cur_str);
    fout << cur_str.c_str() << std::endl;
  }

  fout.close();

  m_iCntSelectedProj =
      m_vSelectedFileNames.size(); // Used to check in ScatterCorr_PrioriCT
}

void CbctRecon::saveHisHeader() {
  if (m_projFormat == HIS_FORMAT) {
    std::cout << "Copying the HIS info to buffer." << std::endl;
    m_arrYKBufProj.resize(m_vSelectedFileNames.size());
    auto it_selected = m_vSelectedFileNames.begin();
    for (auto it = m_arrYKBufProj.begin();
         it != m_arrYKBufProj.end() &&
         it_selected != m_vSelectedFileNames.end();
         ++it, ++it_selected) {
      it->m_strFilePath = it_selected->c_str();
      it->CopyHisHeader(it_selected->c_str());
    }
  }
}

void CbctRecon::NormalizeProjections(ProjReaderType::Pointer &reader) {

  auto originalMax = -1.0;
  auto originalMin = -1.0;
  //                   µ/rho (cm^2/g)  * rho (g/cm^3) * path (cm)
  const auto theoreticalMin =
      0.1541 * 1.225e-3 *
      sqrt(pow(m_spCustomGeometry->GetSourceToDetectorDistances()[0], 2) +
           pow(m_spCustomGeometry->GetSourceToDetectorDistances()[1], 2) +
           pow(m_spCustomGeometry->GetSourceToDetectorDistances()[2], 2)) *
      0.1; // mm -> cm

  const auto correctionValue = GetMaxAndMinValueOfProjectionImage(
      originalMax, originalMin, reader->GetOutput()); // , theoreticalMin);
  std::cout << "Reader Max, Min=" << originalMax << "	" << originalMin
            << std::endl;

  if (correctionValue > 1000.0) {
    if (originalMax - originalMin > log(65535.0f) - theoreticalMin) {
      OpenCL_AddConst_MulConst_InPlace(
          static_cast<cl_float *>(reader->GetOutput()->GetBufferPointer()),
          reader->GetOutput()->GetLargestPossibleRegion().GetSize(),
          static_cast<cl_float>(-correctionValue), // 2048th lowest value
                                                   // (avoiding outliers simply)
          static_cast<cl_float>((log(65535.0f) - theoreticalMin) /
                                (originalMax - originalMin)));
    } else {
      OpenCL_AddConst_InPlace(
          static_cast<cl_float *>(reader->GetOutput()->GetBufferPointer()),
          reader->GetOutput()->GetLargestPossibleRegion().GetSize(),
          static_cast<cl_float>(-correctionValue)); // 2048th lowest value
                                                    // (avoiding outliers
                                                    // simply)
    }
    // Reset min max:
    originalMax = -1.0;
    originalMin = -1.0;
    if (GetMaxAndMinValueOfProjectionImage(originalMax, originalMin,
                                           reader->GetOutput()) > -1000.0) {
      std::cout << "Reader Max, Min=" << originalMax << "	" << originalMin
                << std::endl;
    }
  }
  m_spProjImg3DFloat = reader->GetOutput(); // 1024 1024, line integ image
}

// True if projections were resampled
bool CbctRecon::ResampleProjections(double &resample_factor) {

  std::cout << "ProjectionReader Get Spacing : "
            << m_spProjImg3DFloat->GetSpacing() << std::endl;

  m_fProjSpacingX = m_spProjImg3DFloat->GetSpacing()[0];
  m_fProjSpacingY = m_spProjImg3DFloat->GetSpacing()[1];

  if (resample_factor > 1 || resample_factor <= 0) {
    std::cout << "wrong resample factor. reset to 1.0" << std::endl;
    resample_factor = 1.0;
    return false;
  }

  if (fabs(resample_factor - 1.0) > 0.001) {
    ResampleItkImage(
        m_spProjImg3DFloat, m_spProjImg3DFloat,
        resample_factor); // was! BROKEN AF for .his where input size
                          // != 1024 (tested with 1016) -> outputs
                          // offset -inputoffset/refactor^2 and 4
                          // pixels too few in x and y
  }
  return true;
}

void CbctRecon::GetSelectedIndices(const std::vector<double> &vFullAngles,
                                   std::vector<double> &vNormAngles,
                                   std::vector<int> &vTargetIdx, const bool bCW,
                                   std::vector<int> &vExcludingIdx) const {
  // projection time. Begins with 179.xxx (CW)
  auto latest_Idx = 0;

  const int sizeNom = vNormAngles.size();
  const int sizeFull = vFullAngles.size();

  for (auto i = 0; i < sizeNom; i++) {
    const auto tmpNominalValue = vNormAngles.at(i);

    for (auto j = latest_Idx + 1; j < sizeFull - 1; j++) {
      auto enExcludingMode = 0; // 0: safe,1: right is outlier, 2: left is
                                // outlier, 3: both are outlier

      // 1) Left point is outlier
      if (std::find(vExcludingIdx.begin(), vExcludingIdx.end(), j) !=
              vExcludingIdx.end() &&
          std::find(vExcludingIdx.begin(), vExcludingIdx.end(), j + 1) ==
              vExcludingIdx.end()) {
        enExcludingMode = 2;
      }
      // 2) Right point is outlier
      else if (std::find(vExcludingIdx.begin(), vExcludingIdx.end(), j) ==
                   vExcludingIdx.end() &&
               std::find(vExcludingIdx.begin(), vExcludingIdx.end(), j + 1) !=
                   vExcludingIdx.end()) {
        enExcludingMode = 1;
      }
      // 2) No outlier
      else if (std::find(vExcludingIdx.begin(), vExcludingIdx.end(), j) ==
                   vExcludingIdx.end() &&
               std::find(vExcludingIdx.begin(), vExcludingIdx.end(), j + 1) ==
                   vExcludingIdx.end()) {
        enExcludingMode = 0;
      }
      // 3) Both are outliers
      else if (std::find(vExcludingIdx.begin(), vExcludingIdx.end(), j) !=
                   vExcludingIdx.end() &&
               std::find(vExcludingIdx.begin(), vExcludingIdx.end(), j + 1) !=
                   vExcludingIdx.end()) {
        enExcludingMode = 3;
      }

      auto cur_val = vFullAngles.at(j);
      auto next_val = vFullAngles.at(j + 1);

      if (bCW) {
        // for full gantry angle value of 359.0 - 1.0 interface in CW
        if (cur_val >
            next_val + 0.2) // e.g.359)  - 0.5,, 0.2-->minimum angle diff.
        {
          if (tmpNominalValue < 100) {
            cur_val = cur_val - 360.0;
          } else if (tmpNominalValue > 260) {
            next_val = next_val + 360.0;
          }
        }
        if (tmpNominalValue >= cur_val && tmpNominalValue <= next_val) {

          // Add filtering
          // if j is among the excluding index list (e.g. outlier), just pass
          // it.

          const auto diff_cur = fabs(tmpNominalValue - cur_val);
          const auto diff_next = fabs(tmpNominalValue - next_val);

          if (diff_cur <= diff_next || enExcludingMode == 0 ||
              enExcludingMode == 1) {
            latest_Idx = j;
            vTargetIdx.push_back(latest_Idx);
          } else if (j != sizeFull - 2 && enExcludingMode == 2) {
            latest_Idx = j + 1;
            vTargetIdx.push_back(latest_Idx);
          } else {
            latest_Idx = j;
            // Skip to pushback
          }

          break;
        }
      } else // in CCW case
      {
        // for full gantry angle value of 1.0 - 359.0 interface in CCW
        if (cur_val <
            next_val + 0.01) // e.g.359)  - 0.5,, 0.2-->minimum angle diff.
        {
          if (tmpNominalValue < 100) { // for redundant check
            next_val = next_val - 360.0;
          } else if (tmpNominalValue > 260) { // for redundant check
            cur_val = cur_val + 360.0;
          }
        }

        // in CCW, next value should be smaller than curVal
        if (tmpNominalValue >= next_val && tmpNominalValue <= cur_val) {
          const auto diffCur = fabs(tmpNominalValue - cur_val);
          const auto diffNext = fabs(tmpNominalValue - next_val);

          if (diffCur <= diffNext || enExcludingMode == 0 ||
              enExcludingMode == 1) {
            latest_Idx = j;
            vTargetIdx.push_back(latest_Idx);
          } else if (j != sizeFull - 2 && enExcludingMode == 2) {
            latest_Idx = j + 1;
            vTargetIdx.push_back(latest_Idx);
          } else {
            latest_Idx = j;
            // Skip to pushback
          }
          break;
        }
      }
    }
  }
  // vTargetIdx.push_back(sizeFull-1); //omit the last image --> should be same
  // as first...
}

void CbctRecon::GetExcludeIndexByNames(
    const QString &outlierListPath, std::vector<std::string> &vProjFileFullPath,
    std::vector<int> &vExcludeIdx) const {
  std::ifstream fin;
  fin.open(outlierListPath.toLocal8Bit().constData(), std::ios::in);
  if (static_cast<int>(fin.fail()) == 1) {
    return;
  }

  char str[MAX_LINE_LENGTH];

  while (!fin.eof()) {
    memset(&str[0], 0, MAX_LINE_LENGTH);
    fin.getline(&str[0], MAX_LINE_LENGTH);
    if (strlen(&str[0]) < 1) {
      continue;
    }

    std::cout << "Outlier file: " << &str[0] << std::endl;

    auto curIdx = 0;
    for (auto &it : vProjFileFullPath) {
      if (strstr(it.c_str(), static_cast<const char *>(str)) != nullptr) {
        std::cout << "Detected in the list. Index = " << curIdx << std::endl;
        vExcludeIdx.push_back(curIdx);
        break;
      }
      curIdx++;
    }
  }
  fin.close();
}

void CbctRecon::SetMaxAndMinValueOfProjectionImage() // should be called
                                                     // whenever proj image is
                                                     // changed
{
  if (m_iImgCnt > 0) {
    m_fProjImgValueMax = 65535;
    m_fProjImgValueMin = 0;
    return;
  }

  if (m_spProjImg3DFloat == nullptr) {
    return;
  }

  itk::ImageSliceConstIteratorWithIndex<FloatImageType> it(
      m_spProjImg3DFloat, m_spProjImg3DFloat->GetRequestedRegion());

  it.SetFirstDirection(0);  // x?
  it.SetSecondDirection(1); // y?
  it.GoToBegin();

  m_fProjImgValueMin = 65535.0;
  m_fProjImgValueMax = -9999.0;

  while (!it.IsAtEnd()) {
    while (!it.IsAtEndOfSlice()) {
      while (!it.IsAtEndOfLine()) {
        const double tmpVal = it.Get();

        if (m_fProjImgValueMax < tmpVal) {
          m_fProjImgValueMax = tmpVal;
        }

        if (m_fProjImgValueMin > tmpVal) {
          m_fProjImgValueMin = tmpVal;
        }

        ++it;
      }
      it.NextLine();
    }
    it.NextSlice();
  }
}

bool CbctRecon::IsFileNameOrderCorrect(
    std::vector<std::string> &vFileNames) const {
  // regardless of whether number or hexa codes,
  // we can convert it from number to hexa number and compare the order

  const int size = vFileNames.size();

  if (size < 2) {
    return false;
  }

  std::vector<int> arrNum(size);

  auto index = 0;
  while (index < size) {
    QString crntFilePath = vFileNames.at(index++).c_str();
    auto fileInfo = QFileInfo(crntFilePath);
    auto dir = fileInfo.absoluteDir();
    auto file_basename = fileInfo.baseName();
    auto newBaseName = HexStr2IntStr(file_basename);
    arrNum.push_back(newBaseName.toInt());
  }

  /*bool bOrderOK = true;
  for (int i = 0; i < size - 1; i++)
  {
      if (arrNum[i] >= arrNum[i + 1]) {
          bOrderOK = false;
              }
  }*/

  const auto index_of_nonascending =
      std::adjacent_find(arrNum.begin(), arrNum.end(), std::greater<>());

  return index_of_nonascending == arrNum.end();
}

void CbctRecon::PostApplyFOVDispParam(const float physPosX,
                                      const float physPosY,
                                      const float physRadius,
                                      const float physTablePosY) const {
  if (m_dspYKReconImage == nullptr) {
    return;
  }

  auto origin = m_spCrntReconImg->GetOrigin();
  auto spacing = m_spCrntReconImg->GetSpacing();
  // UShortImageType::SizeType size =
  // m_spCrntReconImg->GetBufferedRegion().GetSize();

  const auto pixPosX = qRound((physPosX - static_cast<double>(origin[0])) /
                              static_cast<double>(spacing[0]));
  const auto pixPosY = qRound((physPosY - static_cast<double>(origin[1])) /
                              static_cast<double>(spacing[1]));

  const auto pixRadius = qRound(physRadius / static_cast<double>(spacing[0]));

  // int pixWidth = qRound((qreal) size[0]);
  // int pixHeight = qRound((qreal) size[1]);

  const auto pixTableY =
      qRound((physTablePosY - static_cast<double>(origin[1])) /
             static_cast<double>(spacing[1]));

  if (pixPosX >= 0 && pixPosY < m_dspYKReconImage->m_iWidth && pixPosY >= 0 &&
      pixPosY < m_dspYKReconImage->m_iHeight && pixRadius > 0 &&
      pixRadius < m_dspYKReconImage->m_iWidth && pixTableY >= 0 &&
      pixTableY < m_dspYKReconImage->m_iHeight) {
    m_dspYKReconImage->m_ptFOVCenter.setX(pixPosX); // data pos
    m_dspYKReconImage->m_ptFOVCenter.setY(pixPosY);

    m_dspYKReconImage->m_iFOVRadius = pixRadius;
    m_dspYKReconImage->m_iTableTopPos = pixTableY;
  }
}

void CbctRecon::CropSupInf(UShortImageType::Pointer &sp_Img,
                           float physPosInfCut, float physPosSupCut) {
  if (sp_Img == nullptr) {
    return;
  }
  // 1) region iterator, set 0 for all pixels outside the circle and below the
  // table top, based on physical position
  auto origin = sp_Img->GetOrigin();
  auto spacing = sp_Img->GetSpacing();
  auto size = sp_Img->GetBufferedRegion().GetSize();

  std::cout << "Old Origin" << origin << std::endl;
  std::cout << "Old spacing" << spacing << std::endl;
  std::cout << "Old size" << size << std::endl;

  UShortImageType::SizeType sizeLower{}, sizeUpper{}; // not index. this is
                                                      // width of pixels that
                                                      // will be taken away.
  sizeLower[0] = 0;
  sizeLower[1] = 0;
  sizeLower[2] = 0;
  /*indexUpper[0] = size[0] - 1;
  indexUpper[1] = size[1] - 1;
  indexUpper[2] = size[2] - 1;*/
  sizeUpper[0] = 0;
  sizeUpper[1] = 0;
  sizeUpper[2] = 0;

  const auto minPosSI = origin[2];
  const auto maxPosSI = origin[2] + (size[2] - 1) * spacing[2];

  if (minPosSI >= physPosInfCut) {
    physPosInfCut = minPosSI;
  }
  if (maxPosSI <= physPosSupCut) {
    physPosSupCut = maxPosSI;
  }

  if (physPosSupCut <= physPosInfCut) {
    return;
  }

  ////calc index
  sizeLower[2] = qRound((physPosInfCut - minPosSI) / spacing[2]);
  sizeUpper[2] = qRound((maxPosSI - physPosSupCut) / spacing[2]);
  //
  using CropImageFilterType =
      itk::CropImageFilter<UShortImageType, UShortImageType>;
  auto CropFilter = CropImageFilterType::New();

  CropFilter->SetInput(sp_Img);
  CropFilter->SetLowerBoundaryCropSize(sizeLower);
  CropFilter->SetUpperBoundaryCropSize(sizeUpper);

  CropFilter->Update();

  if (sp_Img == m_spRawReconImg) {
    sp_Img = CropFilter->GetOutput();
    m_spRawReconImg = sp_Img;
  }

  if (sp_Img == m_spRefCTImg) {
    sp_Img = CropFilter->GetOutput();
    m_spRefCTImg = sp_Img;
    m_spManualRigidCT = sp_Img;
  }

  const auto origin_new = sp_Img->GetOrigin();
  const auto spacing_new = sp_Img->GetSpacing();
  const auto size_new = sp_Img->GetBufferedRegion().GetSize();

  // origin_new[2] = physPosInfCut;
  // sp_Img->SetOrigin(origin_new);

  std::cout << "New Origin" << origin_new << std::endl;
  std::cout << "New spacing" << spacing_new << std::endl;
  std::cout << "New size" << size_new << std::endl;

  std::cout << "LowPos[mm, index] = " << physPosInfCut << ", " << sizeLower[2]
            << std::endl;
  std::cout << "UpperPos[mm, index] = " << physPosSupCut << ", " << sizeUpper[2]
            << std::endl;
  std::cout << "Cropping SI has been successfully done." << std::endl;

  // Result: same image after cropping
  /*
      sizeDiff[0] = CropFilter->GetOutput()->GetBufferedRegion().GetSize()[0] -
     pYKImageROI->m_iWidth; sizeDiff[1] =
     CropFilter->GetOutput()->GetBufferedRegion().GetSize()[1] -
     pYKImageROI->m_iHeight;

      if (sizeDiff[0] != 0 || sizeDiff[1] != 0)
      {
      std::cout << "Cross-correlation error! template size is not matching ROI
     image even after cropping" << std::endl; return;
      }

      rescaleFilter->SetInput(CropFilter->GetOutput());*/
}

// mm
void CbctRecon::CropFOV3D(UShortImageType::Pointer &sp_Img,
                          const float physPosX, const float physPosY,
                          const float physRadius,
                          const float physTablePosY) const {
  if (sp_Img == nullptr) {
    return;
  }
  // 1) region iterator, set 0 for all pixels outside the circle and below the
  // table top, based on physical position
  auto origin = sp_Img->GetOrigin();
  auto spacing = sp_Img->GetSpacing();

  itk::ImageSliceIteratorWithIndex<UShortImageType> it(
      sp_Img, sp_Img->GetBufferedRegion());

  it.SetFirstDirection(0);  // x?
  it.SetSecondDirection(1); // y?
  it.GoToBegin();

  auto iNumSlice = 0;
  while (!it.IsAtEnd()) {
    auto iPosY = 0;
    while (!it.IsAtEndOfSlice()) {
      auto iPosX = 0;
      while (!it.IsAtEndOfLine()) {
        // Calculate physical position

        const auto crntPhysX = iPosX * static_cast<double>(spacing[0]) +
                               static_cast<double>(origin[0]);
        const auto crntPhysY = iPosY * static_cast<double>(spacing[1]) +
                               static_cast<double>(origin[1]);

        if (pow(crntPhysX - physPosX, 2.0) + pow(crntPhysY - physPosY, 2.0) >=
            pow(physRadius, 2.0)) {
          //(*it) = (unsigned short)0; //air value
          it.Set(0);
        }

        if (crntPhysY >= physTablePosY) {
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

void CbctRecon::CopyDictionary(itk::MetaDataDictionary &fromDict,
                               itk::MetaDataDictionary &toDict) const {
  using DictionaryType = itk::MetaDataDictionary;

  DictionaryType::ConstIterator itr = fromDict.Begin();
  const DictionaryType::ConstIterator end = fromDict.End();
  using MetaDataStringType = itk::MetaDataObject<std::string>;

  while (itr != end) {
    auto entry = itr->second;

    MetaDataStringType::Pointer entryvalue =
        dynamic_cast<MetaDataStringType *>(entry.GetPointer());
    if (entryvalue != nullptr) {
      auto tagkey = itr->first;
      auto tagvalue = entryvalue->GetMetaDataObjectValue();
      itk::EncapsulateMetaData<std::string>(toDict, tagkey, tagvalue);
    }
    ++itr;
  }
}

inline float BeamHardModel(const double val, const double a, const double b,
                           const double c, const double d) {
  return a * pow(val, 3.0) + b * pow(val, 2.0) + c * val + d;
}

inline double HndBeamHardModel(const double val) {
  // a * x^3 + b * x^2 + c * x + d
  return 6.0e-08 * pow(val, 3.0) - 1.0e-08 * pow(val, 2.0) - 5.0e-07 * val +
         8.0e-01;
}

inline double HisBeamHardModel(const double val) {
  // a * x^3 + b * x^2 + c * x + d
  return 9.321e-05 * pow(val, 3.0) - 2.609e-03 * pow(val, 2.0) +
         3.374e-02 * val + 9.691e-01;
}

inline double
XimBeamHardModel(const double val) { // a * x^3 + b * x^2 + c * x + d
  return 6.0e-08 * pow(val, 3.0) + 9.0e-5 * pow(val, 2.0) + 1.0e-2 * val + 0.8;
}

void hndBeamHardening(float *pBuffer, const int nPix) {
#pragma omp parallel for
  for (auto i = 0; i < nPix; i++) {
    pBuffer[i] = static_cast<float>(
        (pBuffer[i] < 1.189 ? pBuffer[i]
                            : pBuffer[i] * HndBeamHardModel(pBuffer[i])) +
        1.47);
  }
}
void hisBeamHardening(float *pBuffer, const int nPix) {
#pragma omp parallel for
  for (auto i = 0; i < nPix; i++) {
    pBuffer[i] = static_cast<float>(
        (pBuffer[i] < 1.189 ? pBuffer[i]
                            : pBuffer[i] * HisBeamHardModel(pBuffer[i])));
  }
}
void ximBeamHardening(float *pBuffer, const int nPix) {
#pragma omp parallel for
  for (auto i = 0; i < nPix; i++) {
    pBuffer[i] = static_cast<float>(
        (pBuffer[i] < 1.189 ? pBuffer[i]
                            : pBuffer[i] * XimBeamHardModel(pBuffer[i])) -
        1.47);
  }
}
void CbctRecon::DoBeamHardeningCorrection() const {
  if (m_spProjImg3DFloat == nullptr) {
    return;
  }

  // FloatImageType m_spProjImg3D: float image

  // double crntVal = 0.0;
  // double corrF = 0.0;

  // REMEMBER to change in above inlined functions, here is only for
  // debug !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

  // HND_FORMAT:
  auto poly3_a = 6.0e-08;
  auto poly3_b = -1.0e-08;
  auto poly3_c = -5.0e-07;
  auto poly3_d = 8.0e-01;
  auto poly3_e = 1.47;

  switch (m_projFormat) {
  case HIS_FORMAT:
    poly3_a = 9.321e-05;
    poly3_b = -2.609e-03;
    poly3_c = 3.374e-02;
    poly3_d = 9.691e-01;
    poly3_e = 0.0;
    break;
  case HND_FORMAT:
    break; // used to initialize
  case XIM_FORMAT:
    poly3_a = 6.0e-8;
    poly3_b = 9.0e-5;
    poly3_c = 1.0e-2;
    poly3_d = 0.8;
    poly3_e = -1.47;
    break;
  }
  // If Hnd:

  // double corrVal = 0.0;

  // Shortening factor 0.9 is applied
  /*double poly3_a = 11.24e-05;
  double poly3_b = -29.28e-04;
  double poly3_c = 35.48e-03;
  double poly3_d = 9.701e-01;*/

  // Shortening factor 0.7 is applied
  /*double poly3_a = 1.660e-04;
  double poly3_b = -3.699e-03;
  double poly3_c = 3.923e-02;
  double poly3_d = 9.727e-01;*/

  std::cout << "Beam hardening corrF poly curve:" << poly3_a << "	"
            << poly3_b << "	" << poly3_c << "	" << poly3_d << "  "
            << poly3_e << std::endl;

  const auto pImgBuffer = m_spProjImg3DFloat->GetBufferPointer();
  const auto pImgSize =
      m_spProjImg3DFloat->GetLargestPossibleRegion().GetSize();
  const unsigned int nPix = pImgSize[0] * pImgSize[1] * pImgSize[2];

  switch (m_projFormat) {
  case HIS_FORMAT:
    hisBeamHardening(pImgBuffer, nPix);
    break;
  case HND_FORMAT:
    hndBeamHardening(pImgBuffer, nPix);
    break;
  case XIM_FORMAT:
    ximBeamHardening(pImgBuffer, nPix);
    break;
  }
}

double heaviside(const double x) { return .5 * sgn(x) + 0.5; }

double fullFan_subFunction(const double a, const double b, const double c,
                           const double d, const double x) {
  return c - sqrt(abs(pow(a, 2) - pow(x * d - b, 2))) *
                 heaviside(x * d - b + a) * heaviside(-(x * d - b - a));
}

double fullFan_Function(const double a, const double b, const double c,
                        const double d, const double e, const double x) {
  return (fullFan_subFunction(a, b, c, d, x - 3. * e) +
          fullFan_subFunction(a, b, c, d, x - 2. * e) +
          fullFan_subFunction(a, b, c, d, x - e) +
          fullFan_subFunction(a, b, c, d, x) +
          fullFan_subFunction(a, b, c, d, x + e) +
          fullFan_subFunction(a, b, c, d, x + 2. * e) +
          fullFan_subFunction(a, b, c, d, x + 3. * e)) *
         .142857; // = 1/7
}

void CbctRecon::BowtieByFit(const bool fullfan,
                            const QStringList &params) const {
  if (params.length() != 4 && !fullfan) {
    std::cout << "Wrong number of arguments!" << std::endl
              << "Must be a;b;c;d -> d. / (1 + exp(-b.*(x - a))) + c"
              << std::endl;
    return;
  }
  if (params.length() != 5 && fullfan) {
    std::cout << "Wrong number of arguments!" << std::endl
              << "Must be a;b;c;d;e -> c - sqrt(abs(a^2-((x+/-e)*d-b)^2)) * "
                 "heaviside((x+/-e)*d-b+a) * heaviside(-((x+/-e)*d-b-a))"
              << std::endl;
    return;
  }
  const auto poly3_a = params.at(0).toDouble(); // 264.6; //comboBox_fBTcor
  const auto poly3_b = params.at(1).toDouble(); // 0.06258;
  const auto poly3_c = params.at(2).toDouble(); // 2.502;
  const auto poly3_d = params.at(3).toDouble(); // 1.455;
  auto poly3_e = 0.0;
  if (fullfan) {
    poly3_e = params.at(4).toDouble();
  }

  auto imgSize = m_spProjImg3DFloat->GetLargestPossibleRegion().GetSize();
  if (fullfan) {
    std::cout << "Bow-tie correction curve:" << poly3_d << " / ( 1 + exp(-"
              << poly3_b << " * (x - " << poly3_a << "))) + " << poly3_c
              << " + " << poly3_d << " / ( 1 + exp(" << poly3_b << " * (x - "
              << poly3_e << ")))" << std::endl;
  } else {
    std::cout << "Bow-tie correction curve:" << poly3_d << " / ( 1 + exp(-"
              << poly3_b << " * (x - " << poly3_a << "))) + " << poly3_c
              << std::endl;
  }

  using iteratorType = itk::ImageRegionIteratorWithIndex<FloatImageType>;
  iteratorType it(m_spProjImg3DFloat, m_spProjImg3DFloat->GetRequestedRegion());

  it.GoToBegin();
  if (fullfan) {
    while (!it.IsAtEnd()) {
      const auto crntVal =
          static_cast<double>(it.Get()); // (65535 / exp(it.Get())); //raw
                                         // mu_t = ln(65535/I) <-> I =
                                         // 65535 / exp(mu_t)
      const auto x_idx =
          static_cast<double>(it.GetIndex()[0]) *
          (512.0 / imgSize[0]); // 512 from current fit -> conversion
                                // to be consistent with downResFactor
      // if (crntVal > (poly3_c - poly3_a)), negative values are fine, don't
      // worry
      const auto corrF =
          fullFan_Function(poly3_a, poly3_b, poly3_c, poly3_d, poly3_e, x_idx);
      it.Set(static_cast<float>(crntVal -
                                corrF)); // (log(65535 / (crntVal - corrF))));
      ++it;
    }
  } else {
    while (!it.IsAtEnd()) {
      const auto crntVal =
          static_cast<double>(it.Get()); // (65535 / exp(it.Get()));
                                         // //raw mu_t = ln(65535/I)
                                         // <-> I = 65535 / exp(mu_t)
      const auto x_idx =
          static_cast<double>(it.GetIndex()[0]) *
          (512.0 / imgSize[0]); // 512 from current fit -> conversion
                                // to be consistent with downResFactor
      // if (crntVal > poly3_c), negative values are fine, don't worry
      const auto corrF =
          poly3_d / (1.0 + exp(-poly3_b * (x_idx - poly3_a))) + poly3_c;
      it.Set(static_cast<float>(crntVal -
                                corrF)); // (log(65535 / (crntVal - corrF))));
      ++it;
    }
  }
}

void CbctRecon::Draw2DFrom3DDouble(UShortImageType::Pointer &spFixedImg,
                                   UShortImageType::Pointer &spMovingImg,
                                   const enPLANE enPlane, const double pos,
                                   YK16GrayImage &YKFixed,
                                   YK16GrayImage &YKMoving) const {
  if (spFixedImg == nullptr || spMovingImg == nullptr) {
    return;
  }

  itk::ImageSliceConstIteratorWithIndex<UShortImageType> it(
      spFixedImg, spFixedImg->GetRequestedRegion());

  auto imgSize = spFixedImg->GetRequestedRegion().GetSize(); // 1016x1016 x z
  // UShortImageType::SizeType imgSizeBuf =
  // spFixedImg->GetBufferedRegion().GetSize(); //1016x1016 x z
  // UShortImageType::SizeType imgSizeLargest =
  // spFixedImg->GetLargestPossibleRegion().GetSize(); //1016x1016 x z

  auto imgOrigin = spFixedImg->GetOrigin();
  auto imgSpacing = spFixedImg->GetSpacing();

  int width = imgSize[0];
  int height = imgSize[1];
  auto i_req_slice = qRound((pos - imgOrigin[2]) / imgSpacing[2]);
  int i_cnt_slice = imgSize[2];

  // For moving image
  using ResampleFilterType =
      itk::ResampleImageFilter<UShortImageType, UShortImageType>;
  auto filter = ResampleFilterType::New();

  filter->SetInput(spMovingImg);

  using TransformType = itk::AffineTransform<double, 3>;
  const auto transform = TransformType::New();
  filter->SetTransform(transform);

  using InterpolatorType =
      itk::NearestNeighborInterpolateImageFunction<UShortImageType, double>;
  const auto interpolator = InterpolatorType::New();
  filter->SetInterpolator(interpolator);
  filter->SetDefaultPixelValue(0);

  UShortImageType::DirectionType direction;
  direction.SetIdentity();
  filter->SetOutputDirection(direction);

  auto movingSpacing = imgSpacing;
  auto movingOrigin = imgOrigin;
  auto movingSize = imgSize;

  switch (enPlane) {
  case PLANE_AXIAL:
    it.SetFirstDirection(0);  // x?
    it.SetSecondDirection(1); // y?

    movingSpacing[2] = 1.0;
    movingOrigin[2] = pos;
    movingSize[2] = 1;
    // Resample Here! make corresponding 2D image for Moving image
    YKFixed.SetSpacing(imgSpacing[0], imgSpacing[1]);

    break;
  case PLANE_FRONTAL:
    width = imgSize[0];
    height = imgSize[2];
    i_cnt_slice = imgSize[1];
    i_req_slice = qRound((pos - imgOrigin[1]) / imgSpacing[1]);
    it.SetFirstDirection(0);  // x?
    it.SetSecondDirection(2); // y?

    movingSpacing[1] = 1.0;
    movingOrigin[1] = pos;
    movingSize[1] = 1;

    YKFixed.SetSpacing(imgSpacing[0], imgSpacing[2]);
    break;
  case PLANE_SAGITTAL:
    width = imgSize[1];
    height = imgSize[2];
    i_cnt_slice = imgSize[0];
    i_req_slice = qRound((pos - imgOrigin[0]) / imgSpacing[0]);
    it.SetFirstDirection(1);  // x?
    it.SetSecondDirection(2); // y?

    movingSpacing[0] = 1.0;
    movingOrigin[0] = pos;
    movingSize[0] = 1;

    YKFixed.SetSpacing(imgSpacing[1], imgSpacing[2]);
    break;

  default:
    std::cout << "default should not passed by" << std::endl;
    it.SetFirstDirection(0);  // x?
    it.SetSecondDirection(1); // y?
    YKFixed.SetSpacing(imgSpacing[0], imgSpacing[1]);
    break;
  }

  filter->SetOutputSpacing(movingSpacing);
  filter->SetOutputOrigin(movingOrigin);
  filter->SetSize(movingSize);
  filter->Update();

  YKFixed.CreateImage(width, height, 0);
  // std::cout << "Before MovingImg Creation " << std::endl;

  YKMoving.CreateImage(width, height, 0); // exactly same dimension

  itk::ImageRegionConstIterator<UShortImageType> itMoving(
      filter->GetOutput(), filter->GetOutput()->GetBufferedRegion());
  auto cnt = 0;

  // this simple code will cause flip of the image in frontal and sagittal image
  for (itMoving.GoToBegin(); !itMoving.IsAtEnd(); ++itMoving) {
    YKMoving.m_pData[cnt] = itMoving.Get();
    cnt++;
  }
  if (enPlane != PLANE_AXIAL) {
    YKMoving.EditImage_Flip();
  }

  // std::cout << "tot pixel no: " << cnt << std::endl;

  // YK16GrayImage::CopyItkImage2YKImage(filter->GetOutput(), &YKMoving);

  // std::cout << "After MovingImg Creation " << std::endl;

  it.GoToBegin();

  auto iNumSlice = 0;

  if (i_req_slice < 0 || i_req_slice >= i_cnt_slice) {
    return;
  }

  while (!it.IsAtEnd()) {
    if (iNumSlice == i_req_slice) {
      auto iNumHeight = 0;

      while (!it.IsAtEndOfSlice()) {
        auto iNumWidth = 0;
        while (!it.IsAtEndOfLine()) {
          const auto fixedImgVal = it.Get();

          if (enPlane == PLANE_AXIAL) {
            YKFixed.m_pData[iNumWidth + width * iNumHeight] = fixedImgVal;
          } else {
            YKFixed.m_pData[iNumWidth + width * (height - iNumHeight - 1)] =
                fixedImgVal;
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

  //    std::cout << "YK Images were filled" << std::endl;
}

// Actually just an overload of the above function
void CbctRecon::Draw2DFrom3DDouble(UShortImageType::Pointer &spFixedImg,
                                   UShortImageType::Pointer &spMovingImg,
                                   const enPLANE enPlane, const double pos,
                                   AG17RGBAImage &YKFixed,
                                   AG17RGBAImage &YKMoving) const {
  if (spFixedImg == nullptr || spMovingImg == nullptr) {
    return;
  }

  itk::ImageSliceConstIteratorWithIndex<UShortImageType> it(
      spFixedImg, spFixedImg->GetRequestedRegion());

  auto imgSize = spFixedImg->GetRequestedRegion().GetSize(); // 1016x1016 x z
  // UShortImageType::SizeType imgSizeBuf =
  // spFixedImg->GetBufferedRegion().GetSize(); //1016x1016 x z
  // UShortImageType::SizeType imgSizeLargest =
  // spFixedImg->GetLargestPossibleRegion().GetSize(); //1016x1016 x z

  auto imgOrigin = spFixedImg->GetOrigin();
  auto imgSpacing = spFixedImg->GetSpacing();

  auto width = imgSize[0];
  auto height = imgSize[1];
  auto iCntSlice = imgSize[2];
  auto iReqSlice = qRound((pos - imgOrigin[2]) / imgSpacing[2]);

  // For moving image
  using ResampleFilterType =
      itk::ResampleImageFilter<UShortImageType, UShortImageType>;
  auto filter = ResampleFilterType::New();

  filter->SetInput(spMovingImg);

  using TransformType = itk::AffineTransform<double, 3>;
  const auto transform = TransformType::New();
  filter->SetTransform(transform);

  using InterpolatorType =
      itk::NearestNeighborInterpolateImageFunction<UShortImageType, double>;
  const auto interpolator = InterpolatorType::New();
  filter->SetInterpolator(interpolator);
  filter->SetDefaultPixelValue(0);

  // const double outputSpacing[2] = { 1.0, 1.0 };
  // const double outputOrigin[2] = { 0.0, 0.0 };

  UShortImageType::DirectionType direction;
  direction.SetIdentity();
  filter->SetOutputDirection(direction);

  // ResampledImgType2D::SizeType outSize;

  auto movingSpacing = imgSpacing;
  auto movingOrigin = imgOrigin;
  auto movingSize = imgSize;

  switch (enPlane) {
  case PLANE_AXIAL:
    it.SetFirstDirection(0);  // x?
    it.SetSecondDirection(1); // y?

    movingSpacing[2] = 1.0;
    movingOrigin[2] = pos;
    movingSize[2] = 1;
    // Resample Here! make corresponding 2D image for Moving image
    YKFixed.SetSpacing(imgSpacing[0], imgSpacing[1]);

    break;
  case PLANE_FRONTAL:
    width = imgSize[0];
    height = imgSize[2];
    iCntSlice = imgSize[1];
    iReqSlice = qRound((pos - imgOrigin[1]) / imgSpacing[1]);
    it.SetFirstDirection(0);  // x?
    it.SetSecondDirection(2); // y?

    movingSpacing[1] = 1.0;
    movingOrigin[1] = pos;
    movingSize[1] = 1;

    YKFixed.SetSpacing(imgSpacing[0], imgSpacing[2]);
    break;
  case PLANE_SAGITTAL:
    width = imgSize[1];
    height = imgSize[2];
    iCntSlice = imgSize[0];
    iReqSlice = qRound((pos - imgOrigin[0]) / imgSpacing[0]);
    it.SetFirstDirection(1);  // x?
    it.SetSecondDirection(2); // y?

    movingSpacing[0] = 1.0;
    movingOrigin[0] = pos;
    movingSize[0] = 1;

    YKFixed.SetSpacing(imgSpacing[1], imgSpacing[2]);
    break;

  default:
    std::cout << "default should not passed by" << std::endl;
    it.SetFirstDirection(0);  // x?
    it.SetSecondDirection(1); // y?
    YKFixed.SetSpacing(imgSpacing[0], imgSpacing[1]);
    break;
  }

  filter->SetOutputSpacing(movingSpacing);
  filter->SetOutputOrigin(movingOrigin);
  filter->SetSize(movingSize);
  filter->Update();

  YKFixed.CreateImage(width, height, 0);
  // std::cout << "Before MovingImg Creation " << std::endl;

  YKMoving.CreateImage(width, height, 0); // exactly same dimension

  itk::ImageRegionConstIterator<UShortImageType> itMoving(
      filter->GetOutput(), filter->GetOutput()->GetBufferedRegion());
  auto cnt = 0;

  // this simple code will cause flip of the image in frontal and sagittal image
  for (itMoving.GoToBegin(); !itMoving.IsAtEnd(); ++itMoving) {
    YKMoving.m_pData[cnt] = itMoving.Get();
    cnt++;
  }
  if (enPlane != PLANE_AXIAL) {
    YKMoving.EditImage_Flip();
  }

  it.GoToBegin();

  auto iNumSlice = 0;

  if (iReqSlice < 0 || iReqSlice >= static_cast<int>(iCntSlice)) {
    return;
  }

  while (!it.IsAtEnd()) {
    if (iNumSlice == iReqSlice) {
      auto iNumHeight = 0;

      while (!it.IsAtEndOfSlice()) {
        auto iNumWidth = 0;
        while (!it.IsAtEndOfLine()) {
          const auto fixedImgVal = it.Get();

          if (enPlane == PLANE_AXIAL) {
            YKFixed.m_pData[iNumWidth + width * iNumHeight] = fixedImgVal;
          } else {
            YKFixed.m_pData[iNumWidth + width * (height - iNumHeight - 1)] =
                fixedImgVal;
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

  //    std::cout << "YK Images were filled" << std::endl;
}

void CbctRecon::Draw2DFrom3D(UShortImageType::Pointer &pImg,
                             const enPLANE direction, const double pos,
                             YK16GrayImage &Output2D) const {
  if (pImg == nullptr) {
    return;
  }

  itk::ImageSliceConstIteratorWithIndex<UShortImageType> it(
      pImg, pImg->GetRequestedRegion());

  auto imgSize = pImg->GetRequestedRegion().GetSize(); // 1016x1016 x z
  // UShortImageType::SizeType imgSizeBuf = pImg->GetBufferedRegion().GetSize();
  // //1016x1016 x z UShortImageType::SizeType imgSizeLargest =
  // pImg->GetLargestPossibleRegion().GetSize(); //1016x1016 x z

  auto imgOrigin = pImg->GetOrigin();
  auto imgSpacing = pImg->GetSpacing();

  auto width = imgSize[0];
  auto height = imgSize[1];
  auto iCntSlice = imgSize[2];
  auto iReqSlice = qRound((pos - imgOrigin[2]) / imgSpacing[2]);

  switch (direction) {
  case PLANE_AXIAL:
    it.SetFirstDirection(0);  // x?
    it.SetSecondDirection(1); // y?
    Output2D.SetSpacing(imgSpacing[0], imgSpacing[1]);
    break;
  case PLANE_FRONTAL:
    width = imgSize[0];
    height = imgSize[2];
    iCntSlice = imgSize[1];
    iReqSlice = qRound((pos - imgOrigin[1]) / imgSpacing[1]);
    it.SetFirstDirection(0);  // x?
    it.SetSecondDirection(2); // y?
    Output2D.SetSpacing(imgSpacing[0], imgSpacing[2]);
    break;
  case PLANE_SAGITTAL:
    width = imgSize[1];
    height = imgSize[2];
    iCntSlice = imgSize[0];
    iReqSlice = qRound((pos - imgOrigin[0]) / imgSpacing[0]);
    it.SetFirstDirection(1);  // x?
    it.SetSecondDirection(2); // y?
    Output2D.SetSpacing(imgSpacing[1], imgSpacing[2]);
    break;
  default:
    std::cout << "default should not be passed by" << std::endl;
    it.SetFirstDirection(0);  // x?
    it.SetSecondDirection(1); // y?
    Output2D.SetSpacing(imgSpacing[0], imgSpacing[1]);
    break;
  }

  Output2D.CreateImage(width, height, 0);
  it.GoToBegin();

  auto iNumSlice = 0;

  if (iReqSlice < 0 || iReqSlice >= static_cast<int>(iCntSlice)) {
    return;
  }

  while (!it.IsAtEnd()) {
    if (iNumSlice == iReqSlice) {
      auto iNumHeight = 0;
      while (!it.IsAtEndOfSlice()) {
        auto iNumWidth = 0;
        while (!it.IsAtEndOfLine()) {
          // double tmpVal = it.Get()*multiplyFactor;
          const double tmpVal = it.Get();

          if (direction == PLANE_AXIAL) {
            Output2D.m_pData[iNumWidth + width * iNumHeight] = tmpVal;
          } else {
            Output2D.m_pData[iNumWidth + width * (height - iNumHeight - 1)] =
                tmpVal;
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
}

void CbctRecon::RegisterImgDuplication(const enREGI_IMAGES src,
                                       const enREGI_IMAGES target) {

  UShortImageType::Pointer tmpSrc;

  switch (src) {
  case REGISTER_REF_CT:
    tmpSrc = m_spRefCTImg;
    break;
  default:
    std::cerr << "You are using a non-valid target!" << std::endl;
    return;
  }

  if (tmpSrc == nullptr) {
    std::cout << "src image is empty" << std::endl;
    return;
  }

  // Duplication for registration. Starting point is manual Rigid CT image
  using DuplicatorType = itk::ImageDuplicator<UShortImageType>;
  auto duplicator = DuplicatorType::New();
  duplicator->SetInputImage(tmpSrc);
  duplicator->Update();

  switch (target) {
  case REGISTER_MANUAL_RIGID:
    m_spManualRigidCT = duplicator->GetOutput();
    break;
  default:
    std::cerr << "You are using a non-valid target!" << std::endl;
  }
  // Duplication for : End
}
void CbctRecon::FindAllRelevantPaths(
    const QString &pathProjHisDir) // called following SLT_SetHisDir
{
  // in case of eletka, img_UID
  // QString aa;
  // std::cout<< "ddd " << aa.toLocal8Bit().constData() << std::endl;

  m_strDCMUID.clear();
  m_strPathPatientDir.clear();
  m_strPatientDirName.clear();
  m_strPathFRAME_DBF.clear();
  m_strPathIMAGE_DBF.clear();
  m_strPathGeomXML.clear();
  m_strPathPlanCTDir.clear();
  m_strPathRS.clear();
  m_strPathRS_CBCT.clear();
  m_strPathElektaINI.clear();
  m_strPathElektaINIXVI2.clear();

  m_strPathPlan.clear();

  m_strPathIMAGES.clear();

  QDir curHisDir(pathProjHisDir);
  QDir movingDir(pathProjHisDir);
  m_projFormat = HIS_FORMAT;

  const auto cur_his_path = curHisDir.dirName();
  if (!cur_his_path.contains("img_", Qt::CaseSensitive) &&
      !cur_his_path.contains("fwd_", Qt::CaseSensitive) &&
      !cur_his_path.contains("sca_", Qt::CaseSensitive) &&
      !cur_his_path.contains("cor_", Qt::CaseSensitive)) {
    if (cur_his_path.contains("Scan0", Qt::CaseSensitive)) {
      std::cout << "XML set by guessing: Scan0/../ProjectionInfo.xml"
                << std::endl;
      m_strPathGeomXML =
          curHisDir.absolutePath() + "/../" + "ProjectionInfo.xml";
      std::cout << "Patient DIR set to: Scan0/../../" << std::endl;
      m_projFormat = HND_FORMAT;
      return;
    }
    if (curHisDir.absolutePath().contains("Acquisitions", Qt::CaseSensitive)) {
      std::cout << "XML set by guessing: Acquisitions/../Scan.xml" << std::endl;
      m_strPathGeomXML = curHisDir.absolutePath() + "/../../" + "Scan.xml";
      std::cout << "Patient DIR set to: Acquisitions/../../../" << std::endl;
      m_projFormat = XIM_FORMAT;
      return;
    }

    std::cout << "Projection folder should have format [img_UID]" << std::endl;
    std::cout << "XML file cannot be made" << std::endl;
    return;
  }

  const auto &tmpStr = cur_his_path;
  auto strListDir = tmpStr.split("_");
  m_strDCMUID = strListDir.at(1);

  // m_strDCMUID = cur_his_path.right(cur_his_path.length() - 4);

  if (!movingDir.cdUp()) // projDir ==> IMAGES
  {
    std::cout << "no upper dir" << std::endl;
    return;
  }
  QDir tmpDir_IMAGES(movingDir.absolutePath());
  m_strPathIMAGES = tmpDir_IMAGES.absolutePath();

  if (!movingDir.cdUp()) // IMAGES ==> patient_402-02-78
  {
    std::cout << "no upper dir" << std::endl;
    return;
  }
  QDir tmpDir_PatientFolder(movingDir.absolutePath());

  if (!movingDir
           .cdUp()) // patient_402-02-78 ==> Data folder where DBF files are.
  {
    std::cout << "no upper dir" << std::endl;
    return;
  }

  m_strPatientDirName = tmpDir_PatientFolder.dirName();
  m_strPathPatientDir = tmpDir_PatientFolder.absolutePath();

  QDir tmpDir_RootFolder(movingDir.absolutePath()); // root folder

  if (tmpDir_RootFolder.absolutePath().length() > 1) {
    m_strPathDirDefault = tmpDir_RootFolder.absolutePath();
  }

  // option 1: already made rtk xml file
  const auto tmpPathRTKGeometry = tmpDir_RootFolder.absolutePath() + "/" +
                                  "ElektaGeom_" + m_strDCMUID + ".xml";
  QFileInfo rtkGeomInfo(tmpPathRTKGeometry);

  // option 2
  const auto pathXVIGeometryXML =
      curHisDir.absolutePath() + "/" + "_Frames.xml";
  QFileInfo xviGeomInfo(pathXVIGeometryXML);

  if (rtkGeomInfo
          .exists()) // The best option: rtk geometry file is already existing
  {
    std::cout << "RTK XLM file is found" << std::endl;
    m_strPathGeomXML = tmpPathRTKGeometry;
  } else if (xviGeomInfo.exists()) // 2nd option:_Frames.xml already exists in
                                   // each projection folder for > XVI5.0.2
  {
    std::cout << "XVI XLM file is found" << std::endl;
    // YKdebug: Later, it
    m_strPathGeomXML = pathXVIGeometryXML;

    // Later, it should be

    // XVIXMLReader.SetFile(pathXVIGeometryXML)
    // XVIXMLReader.GenerateData()

    // rtk::ThreeDCircularProjectionGeometryXMLFileWriter::Pointer xmlWriter
    // xmlWriter->SetFilename( tmpPathRTKGeometry ) //as option 1
    // xmlWriter->SetObject(XVIXMLReader->GetGeometry());
    // TRY_AND_EXIT_ON_ITK_EXCEPTION(xmlWriter->WriteFile());
    // m_strPathGeomXML = tmpPathRTKGeometry;
  } else {
    auto tmpStrPath1 = m_strPathPatientDir;
    auto tmpStrPath2 = m_strPathPatientDir;

    // 1st priority: DBF files saved in each "patient" folder --> just in case
    // data are collected separately
    auto fInfo_FrameDBF = QFileInfo(tmpStrPath1.append("/FRAME.DBF"));
    auto fInfo_ImageDBF = QFileInfo(tmpStrPath2.append("/IMAGE.DBF"));

    if (!fInfo_FrameDBF.exists() || !fInfo_ImageDBF.exists()) {
      std::cout << "No found in the patient folder. DBF files can be saved in "
                   "each individual patient as well. Continues to search them "
                   "again in root folder(standard)"
                << std::endl;

      fInfo_FrameDBF =
          QFileInfo(tmpDir_RootFolder.absolutePath().append("/FRAME.DBF"));
      fInfo_ImageDBF =
          QFileInfo(tmpDir_RootFolder.absolutePath().append("/IMAGE.DBF"));

      if (!fInfo_FrameDBF.exists() || !fInfo_ImageDBF.exists()) {
        std::cout << "DBF files were not found" << std::endl;
        std::cout << "XML file cannot be made" << std::endl;
        return;
      }
    } else {
      std::cout << "DBF files are found in the individual patient directory."
                << std::endl;
    }
    m_strPathFRAME_DBF = fInfo_FrameDBF.absoluteFilePath();
    m_strPathIMAGE_DBF = fInfo_ImageDBF.absoluteFilePath();

    m_strPathGeomXML.clear();
    m_strPathGeomXML = MakeElektaXML(
        m_strPathIMAGE_DBF, m_strPathFRAME_DBF,
        m_strDCMUID); // if DBF files exist but UID is not found, it will crash

    if (m_strPathGeomXML.length() < 1) {
      std::cout << "No releated data in DBF file" << std::endl;
      return;
    }
  }

  // std::cout << "Root folder: " <<
  // tmpDir_RootFolder.absolutePath().toLocal8Bit().constData() << std::endl;
  // std::cout << "Root folder2: " <<
  // tmpDir_RootFolder.absolutePath().append("\\FRAME.DBF").toLocal8Bit().constData()
  // << std::endl;  std::cout <<
  // fInfo_FrameDBF.absoluteFilePath().toLocal8Bit().constData() << std::endl;

  // GenerateXMLFunc.

  // Search for the geometry XML file: naming convention:
  // ElektaGeom_DICOMUID.xml  after Generation of the XML from DBF files
  //  movingDir = tmpDir_IMAGES;
  // tmpDir_PatientFolder;

  auto enDirStructure_Type = 2;
  // 0: in patient DIR --> 3 folders(CT_SET, DICOM_PLAN, IMAGES)
  // 1: // Patient DIR ==> IMAGES --> CT_SET / DICOM_PLAN
  // 2: NO CT image

  auto tmpDIR_CTSET =
      QDir(tmpDir_PatientFolder.absolutePath().append("/CT_SET"));

  if (tmpDIR_CTSET.exists()) {
    enDirStructure_Type = 0;
  } else {
    auto tmpStrPathCTSET = m_strPathIMAGES;
    tmpDIR_CTSET = QDir(tmpStrPathCTSET.append("/CT_SET"));

    if (tmpDIR_CTSET.exists()) {
      enDirStructure_Type = 1;
    }
  }

  // QString strPathCTSet = m_strPathIMAGES.append("/CT_SET");

  // switch (enDirStructure_Type)
  // {
  // case 0:
  //  movingDir = tmpDir_IMAGES;
  // break;
  // case 1:
  // movingDir = tmpDir_IMAGES;
  // break;
  // case 2:
  // std::cout << "No CT DICOM folder exist. Proceeding w/o CT" << std::endl;
  // break;
  // }
  //

  if (enDirStructure_Type != 2) {
    auto listDir = tmpDIR_CTSET.entryInfoList(QDir::Dirs, QDir::Name);
    if (listDir.size() <= 2) // only /. and /.. exist
    {
      std::cout << "No CT DICOM folder exist. Proceeding w/o CT" << std::endl;
    } else {
      m_strPathPlanCTDir =
          listDir.at(2).absoluteFilePath(); // . , .. , real DICOM folder
    }
  }

  // for (int i = 0 ; i < listDir.size() ; i++)
  // {
  ////std::cout << listDir.at(i).absolutePath().toLocal8Bit().constData() <<
  /// std::endl; //this returns Dir, not itself
  // QString tmpPath = listDir.at(i).absoluteFilePath();
  // }

  auto listFile = tmpDIR_CTSET.entryInfoList(
      QDir::Files, QDir::Name); // search for DICOM RS file

  if (listFile.empty()) {
    std::cout << "No CT DICOM RS file exist. proceeding w/o RS" << std::endl;
    // return;
  } else {
    for (const auto &i : listFile) {
      if (i.suffix().contains("DCM", Qt::CaseInsensitive)) {
        m_strPathRS = i.absoluteFilePath();
        break;
      }
    }
  }

  auto tmpDIR_DCM_Plan =
      QDir(tmpDir_PatientFolder.absolutePath().append("/DICOM_PLAN"));

  if (tmpDIR_DCM_Plan.exists()) {
    enDirStructure_Type = 0;
  } else {
    auto tmpStrPathCTSET = m_strPathIMAGES;
    tmpDIR_DCM_Plan = QDir(tmpStrPathCTSET.append("/DICOM_PLAN"));

    if (tmpDIR_DCM_Plan.exists()) {
      enDirStructure_Type = 1;
    } else {
      enDirStructure_Type = 2;
    }
  }

  if (enDirStructure_Type != 2) {
    auto listFileDCMPlan =
        tmpDIR_DCM_Plan.entryInfoList(QDir::Files, QDir::Name);
    if (listFileDCMPlan.empty()) // should be /. and /.. and one dcm file
    {
      std::cout << "No DCM plan file exists. Proceeding w/o dicom plan"
                << std::endl;
    } else if (listFileDCMPlan.size() > 1) {
      std::cout << "Warning! More than one DCM plan file exists. First DCM "
                   "plan file will be used"
                << std::endl;
    } else {
    }

    for (const auto &i : listFileDCMPlan) {
      if (i.suffix().contains("DCM", Qt::CaseInsensitive)) {
        m_strPathPlan = i.absoluteFilePath();
        break; // get fisrt one only
      }
    }
  }

  QDir movingDirCBCTRS;

  if (enDirStructure_Type == 0) {
    movingDirCBCTRS = std::move(tmpDir_PatientFolder);
  } else if (enDirStructure_Type == 1) {
    movingDirCBCTRS = std::move(tmpDir_IMAGES);
  }

  if (!movingDirCBCTRS.cd("CBCT_RS")) {
    std::cout << "no CBCT_RS dir exists. Proceed with out CBCT RS image"
              << std::endl;
  } else {
    auto listFile2 = movingDirCBCTRS.entryInfoList(QDir::Files, QDir::Name);

    if (listFile2.empty()) {
      std::cout << "No CBCT DICOM RS file exist. proceeding w/o RS"
                << std::endl;
      // return;
      m_strPathRS_CBCT.clear();
    } else {
      for (const auto &i : listFile2) {
        if (i.suffix().contains("DCM", Qt::CaseInsensitive)) {
          m_strPathRS_CBCT = i.absoluteFilePath();
          break;
        }
      }
    }
  }

  const auto strPathAcqParamDir = pathProjHisDir + "/Reconstruction";
  auto tmpAcqParamDir = QDir(strPathAcqParamDir);

  if (tmpAcqParamDir.exists()) {
    auto listFileAcqParam = tmpAcqParamDir.entryInfoList(
        QDir::Files, QDir::Name); // search for DICOM RS file

    auto iMinNameLength = 9999;

    auto iMaxNameLength = 0;
    auto iCnt_INIXVI = 0;

    QString strPathINIXVI_long;
    for (const auto &i : listFileAcqParam) {
      // suffix:*.tar.gz ==> gz only
      if (i.suffix().contains("INI", Qt::CaseInsensitive)) {
        auto tmpPath = i.absoluteFilePath();

        if (tmpPath.length() < iMinNameLength) {
          iMinNameLength = tmpPath.length();
          m_strPathElektaINI = tmpPath;
        }
      }

      auto StrSuffix = i.completeSuffix();

      if (StrSuffix.contains("INI.XVI", Qt::CaseInsensitive)) {
        iCnt_INIXVI++;

        auto tmpPath2 = i.absoluteFilePath();

        if (tmpPath2.length() > iMaxNameLength) {
          iMaxNameLength = tmpPath2.length();
          strPathINIXVI_long = tmpPath2;
        }
      }
    }

    if (iCnt_INIXVI == 2) {
      m_strPathElektaINIXVI2 = strPathINIXVI_long;
    }
  }

  std::cout << "m_strDCMUID: " << m_strDCMUID.toLocal8Bit().constData()
            << std::endl;
  std::cout << "m_strPathPatientDir: "
            << m_strPathPatientDir.toLocal8Bit().constData() << std::endl;
  std::cout << "m_strPatientDirName: "
            << m_strPatientDirName.toLocal8Bit().constData() << std::endl;
  std::cout << "m_strPathFRAME_DBF: "
            << m_strPathFRAME_DBF.toLocal8Bit().constData() << std::endl;
  std::cout << "m_strPathIMAGE_DBF: "
            << m_strPathIMAGE_DBF.toLocal8Bit().constData() << std::endl;
  std::cout << "m_strPathGeomXML: "
            << m_strPathGeomXML.toLocal8Bit().constData() << std::endl;
  std::cout << "m_strPathPlanCTDir: "
            << m_strPathPlanCTDir.toLocal8Bit().constData() << std::endl;
  std::cout << "m_strPathRS: " << m_strPathRS.toLocal8Bit().constData()
            << std::endl;
  std::cout << "m_strPathRS_CBCT: "
            << m_strPathRS_CBCT.toLocal8Bit().constData() << std::endl;
  std::cout << "m_strPathPlan: " << m_strPathPlan.toLocal8Bit().constData()
            << std::endl;
  std::cout << "m_strPathElektaINI: "
            << m_strPathElektaINI.toLocal8Bit().constData() << std::endl;
  std::cout << "m_strPathElektaINIXVI2: "
            << m_strPathElektaINIXVI2.toLocal8Bit().constData() << std::endl;
}

void CbctRecon::SaveProjImageAsHIS(UShortImageType::Pointer &spProj3D,
                                   std::vector<YK16GrayImage> arrYKImage,
                                   QString &strSavingFolder,
                                   const double resampleF) const {
  std::cout << "Starting Saving files" << std::endl;
  FILE *fd = nullptr;

  UShortImageType::Pointer targetImg3D;
  const auto restoreResampleF = 1.0 / resampleF;

  if (fabs(resampleF - 1.0) > 0.001) {
    std::cout << "restore the  resampled image by applying a factor of "
              << restoreResampleF << std::endl;
    ResampleItkImage(spProj3D, targetImg3D, restoreResampleF);
  } else {
    targetImg3D = spProj3D;
  }

  itk::ImageSliceConstIteratorWithIndex<UShortImageType> it_FwdProj(
      targetImg3D, targetImg3D->GetRequestedRegion());

  it_FwdProj.SetFirstDirection(0);
  it_FwdProj.SetSecondDirection(1);
  it_FwdProj.GoToBegin();

  for (auto it = arrYKImage.begin();
       it != arrYKImage.end() && !it_FwdProj.IsAtEnd(); ++it) {
    QFileInfo crntFileInfo(it->m_strFilePath);

    auto crntFileName = crntFileInfo.fileName();
    auto crntPath = strSavingFolder + "/" + crntFileName;

#ifdef WIN32
    if (fopen_s(&fd, crntPath.toLocal8Bit().constData(), "wb") == 0) {
      std::cerr << "Could not open file: " << crntPath.toLocal8Bit().constData()
                << " for writing!" << std::endl;
      return;
    }
#else
    fd = fopen(crntPath.toLocal8Bit().constData(), "wb");
#endif
    fwrite(it->m_pElektaHisHeader, 100, 1,
           fd); // this buffer only include header info

    // int imgSize = m_arrYKBufProj[i].m_iWidth * m_arrYKImage[i].m_iHeight;

    // Search matching slice using slice iterator for m_spProjCTImg
    while (!it_FwdProj.IsAtEndOfSlice()) {
      while (!it_FwdProj.IsAtEndOfLine()) {
        auto tmpVal = static_cast<unsigned short>(it_FwdProj.Get());
        tmpVal = 65535 - tmpVal; // inverse is done here

        fwrite(&tmpVal, 2, 1, fd);
        ++it_FwdProj;
      }
      it_FwdProj.NextLine();
    }
    fclose(fd);

    it_FwdProj.NextSlice();
    // std::cout << "Now saving " << i+1 << " th file: " <<
    // crntFileName.toLocal8Bit().constData() << std::endl;
  }

  std::cout << "Saving completed" << std::endl;
}

int divisible_by_235_const(const int size) {
  auto input_size = size;
  auto ok = true;
  while (ok) {
    if (input_size % 2 == 0) {
      // ok so far
      input_size /= 2; // compiler should optimize so division and modulo is
                       // calculated simultaneously
    } else if (input_size % 3 == 0) {
      // ok so far
      input_size /= 3;
    } else if (input_size % 5 == 0) {
      // ok so far
      input_size /= 5;
    } else {
      ok = false;
    }
  }
  return input_size;
}

int get_padding(const int input_size) {
  auto cur_padding = 0;
  while (true) {
    // Padding necessary
    if (divisible_by_235_const(input_size + cur_padding) != 1) {
      // While is broken if divisible, else more padding needed.
      cur_padding += 2;
    } else {
      break;
    }
  }
  return cur_padding;
}

#ifdef LOWPASS_FFT
// template<typename T>
FloatImage2DType::Pointer LowPassFFT(FloatImage2DType::Pointer &input,
                                     const double sigmaValue) {
  const auto Dimension = input->GetImageDimension();
  using RealImageType = FloatImage2DType;

  // Some FFT filter implementations, like VNL's, need the image size to be a
  // multiple of small prime numbers.
  using PadFilterType = itk::WrapPadImageFilter<RealImageType, RealImageType>;
  auto padFilter = PadFilterType::New();
  padFilter->SetInput(input);
  PadFilterType::SizeType padding{};
  auto input_size = input->GetLargestPossibleRegion().GetSize();

  for (unsigned int dim = 0; dim < Dimension; dim++) {
    padding[dim] = get_padding(input_size[dim]);
    // Even though the size is usually 512 or 384 which gives padding 0,
    // it would not really speed up the code much to have the checks for these
    // sizes.
  }
  padFilter->SetPadUpperBound(padding);

  using ForwardFFTFilterType = itk::ForwardFFTImageFilter<RealImageType>;
  using ComplexImageType = ForwardFFTFilterType::OutputImageType;
  auto forwardFFTFilter = ForwardFFTFilterType::New();
  forwardFFTFilter->SetInput(padFilter->GetOutput());
  forwardFFTFilter->UpdateOutputInformation();

  // A Gaussian is used here to create a low-pass filter.
  using GaussianSourceType = itk::GaussianImageSource<RealImageType>;
  auto gaussianSource = GaussianSourceType::New();
  gaussianSource->SetNormalized(false);
  gaussianSource->SetScale(1.0);
  const ComplexImageType::ConstPointer transformedInput =
      forwardFFTFilter->GetOutput();
  const auto inputRegion(transformedInput->GetLargestPossibleRegion());
  const auto inputSize = inputRegion.GetSize();
  const auto inputSpacing = transformedInput->GetSpacing();
  const auto inputOrigin = transformedInput->GetOrigin();
  const auto inputDirection = transformedInput->GetDirection();
  gaussianSource->SetSize(inputSize);
  gaussianSource->SetSpacing(inputSpacing);
  gaussianSource->SetOrigin(inputOrigin);
  gaussianSource->SetDirection(inputDirection);
  GaussianSourceType::ArrayType sigma;
  GaussianSourceType::PointType mean;
  sigma.Fill(sigmaValue);
  for (unsigned int ii = 0; ii < Dimension; ++ii) {
    const auto halfLength = inputSize[ii] * inputSpacing[ii] / 2.0;
    sigma[ii] *= halfLength;
    mean[ii] = inputOrigin[ii] + halfLength;
  }
  mean = inputDirection * mean;
  gaussianSource->SetSigma(sigma);
  gaussianSource->SetMean(mean);

  using FFTShiftFilterType =
      itk::FFTShiftImageFilter<RealImageType, RealImageType>;
  auto fftShiftFilter = FFTShiftFilterType::New();
  fftShiftFilter->SetInput(gaussianSource->GetOutput());

  using MultiplyFilterType =
      itk::MultiplyImageFilter<ComplexImageType, RealImageType,
                               ComplexImageType>;
  auto multiplyFilter = MultiplyFilterType::New();
  multiplyFilter->SetInput1(forwardFFTFilter->GetOutput());
  multiplyFilter->SetInput2(fftShiftFilter->GetOutput());

  using InverseFilterType =
      itk::InverseFFTImageFilter<ComplexImageType, RealImageType>;
  auto inverseFFTFilter = InverseFilterType::New();
  inverseFFTFilter->SetInput(multiplyFilter->GetOutput());
  inverseFFTFilter->Update();
  return inverseFFTFilter->GetOutput();
}
#endif

// a * x - y
class axmy {
public:
  float a = 1.0;
  explicit axmy(const float a_val) { this->a = a_val; };
  axmy() = default;
  ~axmy() = default;

  // I think these two operators are required for the SetFunctor
  bool operator!=(const axmy &) const { return false; }
  bool operator==(const axmy &other) const { return !(*this != other); }

  float operator()(const float val1, const float val2) const {
    return val1 * a - val2;
  }
};

// spProjRaw3D: raw intensity value (0-65535), spProjCT3D: raw intensity value
// (0-65535)
void CbctRecon::GenScatterMap_PriorCT(UShortImageType::Pointer &spProjRaw3D,
                                      UShortImageType::Pointer &spProjCT3D,
                                      UShortImageType::Pointer &spProjScat3D,
                                      double /*medianRadius*/,
                                      const double gaussianSigma,
                                      const int nonNegativeScatOffset,
                                      const bool bSave) {
  // Scatter map: should be 2D to use 2D median, Gaussian filters
  if (m_iCntSelectedProj < 1) {
    std::cout << "error: no count of proj image" << std::endl;
    return;
  }

  if (spProjRaw3D == nullptr || spProjCT3D == nullptr) {
    std::cout << "error: proj image 3D is not ready" << std::endl;
    return;
  }

  using SizeType = UShortImageType::SizeType;
  auto size1 = spProjRaw3D->GetRequestedRegion().GetSize();
  auto size2 = spProjCT3D->GetRequestedRegion().GetSize();

  std::cout << "Raw3DProj Size= " << size1 << std::endl;
  std::cout << "spProjCT Size= " << size2 << std::endl;

  auto bHighResolMacro = false; // raw imag= 1024, scattermap = 512
  if (size1[0] != size2[0] || size1[1] != size2[1] || size1[2] != size2[2]) {
    std::cout << "Raw and CT projection dimension are not matching. under the "
                 "high resolution macro?"
              << std::endl;
    // Why 2.0 shouldn't it be 1/downresfactor ?
    if (size1[0] ==
            static_cast<SizeType::SizeValueType>(qRound(size2[0] * 2.0)) &&
        size1[1] ==
            static_cast<SizeType::SizeValueType>(qRound(size2[1] * 2.0)) &&
        size1[2] == size2[2]) {
      bHighResolMacro = true;
    } else {
      return;
    }
  }

  UShortImageType::Pointer spTmpProjRaw3D;

  if (bHighResolMacro) {
    std::cout << "bHighResolMacro is unexpectedly on" << std::endl;
    ResampleItkImage(spProjRaw3D, spTmpProjRaw3D, 0.5);
  } else {
    spTmpProjRaw3D = spProjRaw3D;
  }

  AllocateByRef<UShortImageType, UShortImageType>(spTmpProjRaw3D, spProjScat3D);
  // AllocateByRef(spProjCT3D, spProjScat3D);

  // std::cout << "Scat3D size = " <<
  // spProjScat3D->GetRequestedRegion().GetSize() << std::endl;

  auto imgSize = spTmpProjRaw3D->GetRequestedRegion().GetSize();
  // UShortImageType::SizeType imgSize =
  // spProjCT3D->GetRequestedRegion().GetSize();

  // UShortImageType::SizeType imgSize =
  // spSrcImg3D->GetBufferedRegion().GetSize();  Create spProjScat3D with same
  // dimension of the spProjRaw3D
  const int iSizeZ = imgSize[2];

  // std::cout << "resample factor " << resF2D << std::endl;
  const auto scaling =
      CalculateIntensityScaleFactorFromMeans(spProjRaw3D, spProjCT3D);

  m_strCur_mAs = QString("%1,20").arg(64.0 * 40.0 / 20.0 / scaling);

  m_strRef_mAs = QString("64,40");

  const auto mAs_correctionFactor =
      GetRawIntensityScaleFactor(m_strRef_mAs, m_strCur_mAs);
  for (auto i = 0; i < iSizeZ; i++) {
    FloatImage2DType::Pointer spImg2DRaw;
    FloatImage2DType::Pointer spImg2DPrim;
    FloatImage2DType::Pointer spImg2DScat;

    Get2DFrom3D(spTmpProjRaw3D, spImg2DRaw, i,
                PLANE_AXIAL); // simple conversion between ushort 3D to float 2D
                              // (using casting, not log): input/output: 0-65535
    Get2DFrom3D(spProjCT3D, spImg2DPrim, i, PLANE_AXIAL);

    // Dimension should be matched
    AllocateByRef<FloatImage2DType, FloatImage2DType>(spImg2DRaw, spImg2DScat);
    auto axmy_functor = axmy(mAs_correctionFactor);
    auto axmy_filter =
        itk::BinaryFunctorImageFilter<FloatImage2DType, FloatImage2DType,
                                      FloatImage2DType, axmy>::New();
    axmy_filter->SetFunctor(axmy_functor);
    axmy_filter->SetInput1(spImg2DRaw);
    axmy_filter->SetInput2(spImg2DPrim);
    axmy_filter->Update();
    spImg2DScat = axmy_filter->GetOutput();

#ifdef LOWPASS_FFT
    spImg2DScat = LowPassFFT(spImg2DScat, gaussianSigma);
#else
    // ResampleItkImage2D(spImg2DScat, spImg2DScat, resF2D);
    using MedianFilterType =
        itk::MedianImageFilter<FloatImage2DType, FloatImage2DType>;

    MedianFilterType::Pointer medianFilterX = MedianFilterType::New();
    MedianFilterType::InputSizeType radiusX{};
    radiusX[0] = medianRadius;
    radiusX[1] = 0;
    medianFilterX->SetRadius(radiusX);
    medianFilterX->SetInput(spImg2DScat);
    // medianFilterX->Update();
    // spImg2DScat = medianFilterX->GetOutput();

    MedianFilterType::Pointer medianFilterY = MedianFilterType::New();
    MedianFilterType::InputSizeType radiusY{};
    radiusY[0] = 0;
    radiusY[1] = medianRadius;
    medianFilterY->SetRadius(radiusY);
    medianFilterY->SetInput(medianFilterX->GetOutput());
    medianFilterY->Update();
    spImg2DScat = medianFilterY->GetOutput();

    using SmoothingFilterType =
        itk::SmoothingRecursiveGaussianImageFilter<FloatImage2DType,
                                                   FloatImage2DType>;
    SmoothingFilterType::Pointer gaussianFilter = SmoothingFilterType::New();
    // gaussianFilter->SetInput(medianFilter->GetOutput());
    gaussianFilter->SetInput(spImg2DScat);
    if (hisIsUsed) {
      gaussianFilter->SetSigma(
          gaussianSigma); // filter specific setting for 512x 512 image
    } else {
      SmoothingFilterType::SigmaArrayType gaussianSigmaArray;
      gaussianSigmaArray[0] = gaussianSigma;
      gaussianSigmaArray[1] = .75 * gaussianSigma;
      gaussianFilter->SetSigmaArray(
          gaussianSigmaArray); // filter specific setting for 512x384 (varian/2)
    }
    // gaussianFilter->Update();
    // spImg2DScat = gaussianFilter->GetOutput();
#endif

    using AddImageFilterType =
        itk::AddImageFilter<FloatImage2DType, FloatImage2DType,
                            FloatImage2DType>;
    auto addFilter = AddImageFilterType::New();
#ifdef LOWPASS_FFT
    addFilter->SetInput1(spImg2DScat);
#else
    addFilter->SetInput1(gaussianFilter->GetOutput());
#endif
    addFilter->SetConstant2(static_cast<float>(nonNegativeScatOffset));
    addFilter->Update();
    spImg2DScat = addFilter->GetOutput(); // even after the offset applied, -
                                          // value is still possible

    // float to unsigned short
    Set2DTo3D(spImg2DScat, spProjScat3D, i,
              PLANE_AXIAL); // input/Output: 0-65535 intensity valuesno mu_t to
                            // intensity converion is involved

    const auto unit = qRound(iSizeZ / 10.0);
    if (i % unit == 0) {
      std::cout << "Generating scatter map: "
                << i / static_cast<double>(unit) * 10.0 << " % is done"
                << std::endl;
    }
  } // end of for

  if (bSave) {
    // Saving part: save as his file in sub-folder of raw image
    std::cout << "Files are being saved" << std::endl;
    std::cout << "Patient DIR Path: "
              << m_strPathPatientDir.toLocal8Bit().constData() << std::endl;

    if (m_strPathPatientDir.isEmpty() && m_projFormat == HIS_FORMAT) {
      std::cout << "File save error!: No patient DIR name" << std::endl;
      return;
    }

    // Get current folder
    QString strCrntDir;
    if (m_projFormat == HIS_FORMAT) {
      strCrntDir = m_strPathPatientDir + "/" + "IMAGES"; // current Proj folder
    } else {
      // stolen from registration class: m_strPathPlastimatch definition
      auto crntDir = QDir::current(); // folder where current exe file exists.
      auto crntPathStr = crntDir.absolutePath();
      auto dirName = crntPathStr.append("/plm_tmp");

      auto tmpDir = QDir(dirName);
      if (!tmpDir.exists()) {
        if (!tmpDir.mkpath(dirName)) {
          std::cerr << "Could not create " << dirName.toStdString()
                    << std::endl;
          // Not enough reason to fail
        }
      }
      strCrntDir = dirName;
    }

    // Make a sub directory
    QDir crntDir(strCrntDir);

    if (!crntDir.exists()) {
      std::cout << "File save error: The specified folder does not exist."
                << std::endl;
      return;
    }

    const auto scatDirName = "sca_" + m_strDCMUID;

    const auto tmpResult = crntDir.mkdir(scatDirName); // what if the directory
                                                       // exists?

    if (!tmpResult) {
      std::cout << "Scatter map directory seems to exist already. Files will "
                   "be overwritten."
                << std::endl;
    }

    auto strSavingFolder = strCrntDir + "/" + scatDirName;
    if (m_projFormat == HIS_FORMAT) {
      SaveProjImageAsHIS(spProjScat3D, m_arrYKBufProj, strSavingFolder,
                         m_fResampleF);
    } else {
      using imagewritertype = itk::ImageFileWriter<UShortImageType>;
      auto imagewriter = imagewritertype::New();
      imagewriter->SetInput(spProjScat3D);
      imagewriter->SetFileName(
          QString(strSavingFolder + "/scatter.mha").toStdString());
      imagewriter->Update();
    }
  }
}

class corr_functor {
public:
  corr_functor(const float mAs_corr, const int nonNegOffset) {
    this->mAs_correctionFactor = mAs_corr;
    this->nonNegativeScatOffset = nonNegOffset;
  }
  corr_functor() = default;
  ~corr_functor() = default;
  float mAs_correctionFactor = 1.0f;
  int nonNegativeScatOffset = 0;

  // I think these two operators are required for the SetFunctor
  bool operator!=(const corr_functor &) const { return false; }
  bool operator==(const corr_functor &other) const { return !(*this != other); }

  float operator()(const float val1, const float val2) const {
    const auto rawVal = val1 * mAs_correctionFactor;
    const auto scatVal = val2 - static_cast<float>(nonNegativeScatOffset);
    auto corrVal = rawVal - scatVal;

    if (corrVal < 1.0f) {
      corrVal = 1.0f; // underflow control
    }
    // max unsigned short - 1, just so we don't overflow
    const auto max_ushort =
        static_cast<float>(std::numeric_limits<unsigned short>::max() - 1);
    if (corrVal >
        max_ushort) { // 65535 -->(inversion) --> 0 --> LOg (65536 / 0) = ERROR!
      corrVal = max_ushort;
    }

    return corrVal; // later, add customSPR
                    // corrVal = (float)(rawVal - customSPR*scatterVal);
  }
};

void CbctRecon::ScatterCorr_PrioriCT(UShortImageType::Pointer &spProjRaw3D,
                                     UShortImageType::Pointer &spProjScat3D,
                                     UShortImageType::Pointer &m_spProjCorr3D,
                                     const int nonNegativeScatOffset,
                                     int postMedian, const bool bSave) {
  // Scatter map: should be 2D to use 2D median, Gaussian filters
  if (m_iCntSelectedProj < 1) {
    std::cout << "error: no count of proj image" << std::endl;
    return;
  }

  if (spProjRaw3D == nullptr || spProjScat3D == nullptr) {
    std::cout << "Error: proj image 3D is not ready" << std::endl;
    return;
  }

  auto size1 = spProjRaw3D->GetRequestedRegion().GetSize();
  auto size2 = spProjScat3D->GetRequestedRegion().GetSize();

  std::cout << "Raw3DProj Size= " << size1 << std::endl;
  std::cout << "spProjScat3D Size= " << size2 << std::endl;

  auto bHighResolMacro = false;

  if (size1[0] != size2[0] || size1[1] != size2[1] || size1[2] != size2[2]) {
    std::cout << "Raw and scatter projection dimension are not matching. under "
                 "the high resolution macro?"
              << std::endl;

    if (static_cast<int>(size1[0]) == qRound(size2[0] * 2.0) &&
        static_cast<int>(size1[1]) == qRound(size2[1] * 2.0) &&
        size1[2] == size2[2]) {
      bHighResolMacro = true;
    } else {
      return;
    }
  }

  UShortImageType::Pointer spTmpProjScat3D;

  if (bHighResolMacro) {
    ResampleItkImage(spProjScat3D, spTmpProjScat3D, 2.0);
  } else {
    spTmpProjScat3D = spProjScat3D;
  }

  AllocateByRef<UShortImageType, UShortImageType>(spProjRaw3D, m_spProjCorr3D);

  auto imgSize = spProjRaw3D->GetRequestedRegion().GetSize();

  // UShortImageType::SizeType imgSize =
  // spSrcImg3D->GetBufferedRegion().GetSize();  Create spProjScat3D with same
  // dimension of the spProjRaw3D
  const int iSizeZ = imgSize[2];

  // std::cout << "resample factor " << resF2D << std::endl;.

  const auto mAs_correctionFactor =
      GetRawIntensityScaleFactor(m_strRef_mAs, m_strCur_mAs);
  for (auto i = 0; i < iSizeZ; i++) {
    FloatImage2DType::Pointer spImg2DRaw;
    FloatImage2DType::Pointer spImg2DScat;
    FloatImage2DType::Pointer spImg2DCorr;

    Get2DFrom3D(spProjRaw3D, spImg2DRaw, i, PLANE_AXIAL);
    Get2DFrom3D(spTmpProjScat3D, spImg2DScat, i, PLANE_AXIAL);

    auto corr_fun = corr_functor(mAs_correctionFactor, nonNegativeScatOffset);
    auto corr_filter =
        itk::BinaryFunctorImageFilter<FloatImage2DType, FloatImage2DType,
                                      FloatImage2DType, corr_functor>::New();
    corr_filter->SetFunctor(corr_fun);
    corr_filter->SetInput1(spImg2DRaw);
    corr_filter->SetInput2(spImg2DScat);
    corr_filter->Update();
    spImg2DCorr = corr_filter->GetOutput();

    // Post Median filtering

    if (bHighResolMacro) {
      postMedian = postMedian * 2.0;
    }

    if (postMedian >= 2) // YK2015
    {
      using MedianFilterType =
          itk::MedianImageFilter<FloatImage2DType, FloatImage2DType>;
      auto medianFilter = MedianFilterType::New();
      MedianFilterType::InputSizeType radius{};

      radius[0] = qRound(postMedian / 2.0);
      radius[1] = radius[0];

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

    Set2DTo3D(spImg2DCorr, m_spProjCorr3D, i, PLANE_AXIAL); // float2D to USHORT

    const auto unit = qRound(iSizeZ / 10.0);
    if (i % unit == 0) {
      std::cout << "Applying scatter correction: "
                << i / static_cast<double>(unit) * 10.0 << " % is done"
                << std::endl;
    }
  }

  if (bSave) {
    // Saving part: save as his file in sub-folder of raw image
    std::cout << "Files are being saved" << std::endl;
    std::cout << "Patient DIR Path: "
              << m_strPathPatientDir.toLocal8Bit().constData() << std::endl;

    if (m_strPathPatientDir.isEmpty()) {
      std::cout << "File save error!: No patient DIR name" << std::endl;
      return;
    }

    // Get current folder
    const auto strCrntDir =
        m_strPathPatientDir + "/" + "IMAGES"; // current Proj folder

    // Make a sub directory
    QDir crntDir(strCrntDir);

    if (!crntDir.exists()) {
      std::cout << "File save error: The specified folder does not exist."
                << std::endl;
      return;
    }

    const auto scatDirName = "cor_" + m_strDCMUID;

    const auto tmpResult = crntDir.mkdir(scatDirName); // what if the directory
                                                       // exists?

    if (!tmpResult) {
      std::cout << "Corrected projection directory seems to exist already. "
                   "Files will be overwritten."
                << std::endl;
    }

    auto strSavingFolder = strCrntDir + "/" + scatDirName;

    if (!bHighResolMacro) {
      SaveProjImageAsHIS(m_spProjCorr3D, m_arrYKBufProj, strSavingFolder,
                         m_fResampleF);
    } else {
      SaveProjImageAsHIS(m_spProjCorr3D, m_arrYKBufProj, strSavingFolder, 1.0);
    }
  }
  // spProjScat3D->Initialize(); //memory release
}

void CbctRecon::Set2DTo3D(FloatImage2DType::Pointer &spSrcImg2D,
                          UShortImageType::Pointer &spTargetImg3D,
                          const int idx, const enPLANE iDirection) const {
  if (spSrcImg2D == nullptr ||
      spTargetImg3D == nullptr) { // Target image should be also ready.
    return;
  }

  auto idxHor = 0, idxVer = 0, idxZ = 0;

  switch (iDirection) {
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

  auto imgDim2D = spSrcImg2D->GetBufferedRegion().GetSize();
  // FloatImage2DType::SpacingType spacing2D = spSrcImg2D->GetSpacing();
  // FloatImage2DType::PointType origin2D = spSrcImg2D->GetOrigin();

  auto imgDim3D = spTargetImg3D->GetBufferedRegion().GetSize();
  // UShortImageType::SpacingType spacing3D = spTargetImg3D->GetSpacing();
  // UShortImageType::PointType origin3D = spTargetImg3D->GetOrigin();

  // Filtering
  if (imgDim2D[0] != imgDim3D[idxHor] || imgDim2D[1] != imgDim3D[idxVer] ||
      idx < 0 || idx >= static_cast<int>(imgDim3D[idxZ])) {
    std::cout << "Error: image dimensions is not matching" << std::endl;
    std::cout << "2D= " << imgDim2D << std::endl;
    std::cout << "3D= " << imgDim3D << std::endl;
    return;
  }
  /*int width = imgDim[idxHor];
  int height  = imgDim[idxVer];*/

  // itk::ImageRegionConstIteratorWithIndex<FloatImageType2D> it_2D (spSrcImg2D,
  // spSrcImg2D->GetRequestedRegion());
  itk::ImageRegionConstIterator<FloatImage2DType> it_2D(
      spSrcImg2D, spSrcImg2D->GetRequestedRegion());
  itk::ImageSliceIteratorWithIndex<UShortImageType> it_3D(
      spTargetImg3D, spTargetImg3D->GetRequestedRegion());

  it_3D.SetFirstDirection(idxHor);
  it_3D.SetSecondDirection(idxVer);
  it_3D.GoToBegin();

  const int zSize = imgDim3D[idxZ];

  it_2D.GoToBegin();

  unsigned short outputVal = 0;
  const auto max_ushort = std::numeric_limits<unsigned short>::max();
  for (auto i = 0; i < zSize && !it_3D.IsAtEnd(); i++) {
    /*QFileInfo crntFileInfo(arrYKImage[i].m_strFilePath);
    QString crntFileName = crntFileInfo.fileName();
    QString crntPath = strSavingFolder + "/" + crntFileName;*/
    // Search matching slice using slice iterator for m_spProjCTImg
    if (i == idx) {
      while (!it_3D.IsAtEndOfSlice()) {
        while (!it_3D.IsAtEndOfLine()) {
          const auto fVal2D = it_2D.Get();

          if (fVal2D < 0.0f) {
            outputVal = 0U;
          } else if (fVal2D > static_cast<float>(max_ushort)) {
            outputVal = max_ushort;
          } else {
            outputVal = static_cast<unsigned short>(qRound(fVal2D));
          }

          it_3D.Set(outputVal);
          // float tmpVal = (float)(it_3D.Get()); //in proj image case, this is
          // intensity  it_2D.Set(tmpVal);
          ++it_2D;
          ++it_3D;
        } // while2
        it_3D.NextLine();
      } // while1
      break;
    }
    //
    it_3D.NextSlice();
  } // end of for
}

// void CbctRecon::Get2DFrom3D( FloatImageType::Pointer& spSrcImg3D,
// FloatImageType2D::Pointer& spTargetImg2D, enPLANE iDirection)
//{
//
//}
class LineInt2Intensity {
public:
  LineInt2Intensity() = default;
  ~LineInt2Intensity() = default;
  float operator()(const float val) const {
    const auto max_ushort = std::numeric_limits<unsigned short>::max();
    float intensityVal =
        exp(static_cast<double>(val) * -1.0) * static_cast<double>(max_ushort);

    if (intensityVal <= 1.0) {
      intensityVal = 1.0;
    }
    if (intensityVal >= (max_ushort - 1)) {
      intensityVal = static_cast<double>(max_ushort - 1);
    }

    return static_cast<unsigned short>(intensityVal);
  }
};
// From line integral to raw intensity
// bkIntensity is usually 65535
UShortImageType::Pointer
CbctRecon::ConvertLineInt2Intensity(FloatImageType::Pointer &spProjLineInt3D) {
  if (spProjLineInt3D == nullptr) {
    return nullptr;
  }
  // FloatImageType::IMageRegionIteratorWithIndex

  auto convert_filter =
      itk::UnaryFunctorImageFilter<FloatImageType, UShortImageType,
                                   LineInt2Intensity>::New();
  convert_filter->SetInput(spProjLineInt3D);
  convert_filter->Update();
  return convert_filter->GetOutput();
}

class Intensity2LineInt {
public:
  Intensity2LineInt() = default;
  ~Intensity2LineInt() = default;
  float operator()(const unsigned short val) const {
    const auto max_ushort = std::numeric_limits<unsigned short>::max();
    // mu = ln(I_0/I) OR mu = ln(I/I0)
    const float mu_t_val =
        log(static_cast<double>(max_ushort) / static_cast<double>(val));

    return mu_t_val;
  }
};

FloatImageType::Pointer CbctRecon::ConvertIntensity2LineInt(
    UShortImageType::Pointer &spProjIntensity3D) {
  if (spProjIntensity3D == nullptr) {
    return nullptr;
  }
  auto convert_filter =
      itk::UnaryFunctorImageFilter<UShortImageType, FloatImageType,
                                   Intensity2LineInt>::New();
  convert_filter->SetInput(spProjIntensity3D);
  convert_filter->Update();
  return convert_filter->GetOutput();
}

// it works! new memory will be allocated for spTarImg
void CbctRecon::ResampleItkImage(FloatImageType::Pointer &spSrcImg,
                                 FloatImageType::Pointer &spTarImg,
                                 const double resFactor) const {
  if (spSrcImg == nullptr) {
    return;
  }

  //  std::cout << "original Origin: " << spSrcImg2D->GetOrigin() << std::endl;
  using ResampleImageFilterType =
      itk::ResampleImageFilter<FloatImageType, FloatImageType, float>;
  auto resample = ResampleImageFilterType::New();

  resample->SetOutputDirection(spSrcImg->GetDirection());

  using TransformType = itk::AffineTransform<float, 3>;
  auto transform = TransformType::New();

  using InterpolatorType =
      itk::NearestNeighborInterpolateImageFunction<FloatImageType, float>;
  const auto interpolator = InterpolatorType::New();
  resample->SetInterpolator(interpolator);
  if ((m_projFormat == HIS_FORMAT &&
       DEFAULT_ELEKTA_PROJ_HEIGHT ==
           spSrcImg->GetBufferedRegion().GetSize()[1]) ||
      (m_projFormat != HIS_FORMAT &&
       DEFAULT_VARIAN_PROJ_HEIGHT ==
           spSrcImg->GetBufferedRegion().GetSize()[1])) {
    resample->SetDefaultPixelValue(50);
  } else {
    resample->SetDefaultPixelValue(0);
  }

  auto inputSize = spSrcImg->GetLargestPossibleRegion().GetSize();
  FloatImageType::SizeType outputSize{};
  outputSize[0] = qRound(inputSize[0] * resFactor);
  outputSize[1] = qRound(inputSize[1] * resFactor);
  outputSize[2] = inputSize[2];
  resample->SetSize(outputSize);

  FloatImageType::SpacingType outputSpacing;
  outputSpacing[0] =
      spSrcImg->GetSpacing()[0] *
      (static_cast<double>(inputSize[0]) / static_cast<double>(outputSize[0]));
  outputSpacing[1] =
      spSrcImg->GetSpacing()[1] *
      (static_cast<double>(inputSize[1]) / static_cast<double>(outputSize[1]));
  outputSpacing[2] = spSrcImg->GetSpacing()[2];
  resample->SetOutputSpacing(outputSpacing);

  const auto outputOrigin = spSrcImg->GetOrigin(); // Float image
  resample->SetOutputOrigin(outputOrigin);

  resample->SetInput(spSrcImg);
  transform->SetIdentity();
  resample->SetTransform(transform);

  resample->Update();

  // resample->GetOutput()->SetOrigin(prevOrigin);
  spTarImg = resample->GetOutput(); // is it copied? or replaced?
}

void CbctRecon::ResampleItkImage(UShortImageType::Pointer &spSrcImg,
                                 UShortImageType::Pointer &spTarImg,
                                 const double resFactor) const {
  if (spSrcImg == nullptr) {
    return;
  }

  //  std::cout << "original Origin: " << spSrcImg2D->GetOrigin() << std::endl;
  using ResampleImageFilterType =
      itk::ResampleImageFilter<UShortImageType, UShortImageType, float>;
  auto resample = ResampleImageFilterType::New();

  resample->SetOutputDirection(spSrcImg->GetDirection());

  using TransformType = itk::AffineTransform<float, 3>;
  auto transform = TransformType::New();

  using InterpolatorType =
      itk::NearestNeighborInterpolateImageFunction<UShortImageType, float>;
  const auto interpolator = InterpolatorType::New();
  resample->SetInterpolator(interpolator);

  resample->SetDefaultPixelValue(50);

  auto inputSize = spSrcImg->GetLargestPossibleRegion().GetSize();
  UShortImageType::SizeType outputSize{};
  outputSize[0] = qRound(inputSize[0] * resFactor);
  outputSize[1] = qRound(inputSize[1] * resFactor);
  outputSize[2] = inputSize[2];
  resample->SetSize(outputSize);

  UShortImageType::SpacingType outputSpacing;
  outputSpacing[0] =
      spSrcImg->GetSpacing()[0] *
      (static_cast<double>(inputSize[0]) / static_cast<double>(outputSize[0]));
  outputSpacing[1] =
      spSrcImg->GetSpacing()[1] *
      (static_cast<double>(inputSize[1]) / static_cast<double>(outputSize[1]));
  outputSpacing[2] = spSrcImg->GetSpacing()[2];
  resample->SetOutputSpacing(outputSpacing);

  const auto outputOrigin = spSrcImg->GetOrigin(); // Float
                                                   // image
  resample->SetOutputOrigin(outputOrigin);

  resample->SetInput(spSrcImg);
  transform->SetIdentity();
  resample->SetTransform(transform);

  resample->Update();

  // resample->GetOutput()->SetOrigin(prevOrigin);
  spTarImg = resample->GetOutput(); // is it copied? or replaced?
}

void CbctRecon::ResampleItkImage2D(FloatImage2DType::Pointer &spSrcImg2D,
                                   FloatImage2DType::Pointer &spTarImg2D,
                                   const double resFactor) const {
  if (spSrcImg2D == nullptr) {
    std::cout << "ERROR! SrcImage is empty" << std::endl;
    return;
  }

  //  std::cout << "original Origin: " << spSrcImg2D->GetOrigin() << std::endl;
  using ResampleImageFilterType =
      itk::ResampleImageFilter<FloatImage2DType, FloatImage2DType, float>;
  auto resample = ResampleImageFilterType::New();

  resample->SetOutputDirection(spSrcImg2D->GetDirection());

  // outputSpacing[2] = spSrcImg2D->GetSpacing()[2];

  // std::cout << "Output spacing: " << outputSpacing << std::endl;

  // FloatImageType2D::Pointer input = spSrcImg2D;
  // FloatImageType2D::PointType prevOrigin = input->GetOrigin(); //-204.6 -
  // 204.6  0

  // input->SetOrigin(outputOrigin);

  /* std::cout << "outputSize " << outputSize << std::endl;
   std::cout << "OutputSpacing " << outputSpacing << std::endl;
   std::cout << "OutputOrigin " << outputOrigin << std::endl;*/

  // Resample the image
  // typedef itk::IdentityTransform<float, 2> TransformType;

  using TransformType = itk::AffineTransform<float, 2>;
  auto transform = TransformType::New();

  using InterpolatorType =
      itk::NearestNeighborInterpolateImageFunction<FloatImage2DType, float>;
  const auto interpolator = InterpolatorType::New();
  resample->SetInterpolator(interpolator);

  resample->SetDefaultPixelValue(50);

  auto inputSize = spSrcImg2D->GetLargestPossibleRegion().GetSize();
  FloatImage2DType::SizeType outputSize{};
  outputSize[0] = qRound(inputSize[0] * resFactor);
  outputSize[1] = qRound(inputSize[1] * resFactor);
  resample->SetSize(outputSize);

  FloatImage2DType::SpacingType outputSpacing;
  outputSpacing[0] =
      spSrcImg2D->GetSpacing()[0] *
      (static_cast<double>(inputSize[0]) / static_cast<double>(outputSize[0]));
  outputSpacing[1] =
      spSrcImg2D->GetSpacing()[1] *
      (static_cast<double>(inputSize[1]) / static_cast<double>(outputSize[1]));
  resample->SetOutputSpacing(outputSpacing);

  const auto outputOrigin = spSrcImg2D->GetOrigin(); // Float image
  resample->SetOutputOrigin(outputOrigin);

  resample->SetInput(spSrcImg2D);
  transform->SetIdentity();
  resample->SetTransform(transform);

  resample->Update();

  // resample->GetOutput()->SetOrigin(prevOrigin);
  spTarImg2D = resample->GetOutput(); // is it copied? or replaced?

  // std::cout << "resampled Origin: " << spTarImg2D->GetOrigin() << std::endl;
}

void CbctRecon::AfterScatCorrectionMacro(const bool use_cuda,
                                         const bool use_opencl,
                                         const bool save_dicom,
                                         FDK_options &fdk_options) {
  // Original projection file can be replaced by the corrected one
  // Current projection map (float) used for the reconstruction is:
  // m_spProjImg3DFloat and this is resampled one
  m_spProjImg3DFloat = ConvertIntensity2LineInt(m_spProjImgCorr3D);

  // Do reconstruction

  // Regardeless of previous setting, The Truncation should not be applied!

  // Truncation is invalidated inside the function
  if (use_cuda) {
    DoReconstructionFDK<CUDA_DEVT>(REGISTER_COR_CBCT, fdk_options);
  } else if (use_opencl) {
    DoReconstructionFDK<OPENCL_DEVT>(REGISTER_COR_CBCT, fdk_options);
  } else {
    DoReconstructionFDK<CPU_DEVT>(REGISTER_COR_CBCT, fdk_options);
  }

  // Save Image as DICOM
  if (save_dicom) {
    // Get current folder
    const auto strCrntDir = m_strPathPatientDir + "/" + "IMAGES" + "/" +
                            "cor_" + m_strDCMUID; // current Proj folder
    QDir crntDir(strCrntDir);
    const auto SubDirName = QString("Reconstruction");
    const auto tmpResult =
        crntDir.mkdir(SubDirName); // what if the directory exists?
    if (!tmpResult) {
      std::cout
          << "DICOM dir seems to exist already. Files will be overwritten."
          << std::endl;
    }
    auto strSavingFolder = strCrntDir + "/" + SubDirName;
    auto updated_text_ct = QString("PriorCT_ScatterCorr");
    SaveUSHORTAsSHORT_DICOM(m_spScatCorrReconImg, m_strDCMUID, updated_text_ct,
                            strSavingFolder);
    // Export as DICOM (using plastimatch) folder?
  }
  std::cout << "Exiting AfterScatCorrectionMacro.";
}

int CbctRecon::CropSkinUsingThreshold(const int threshold,
                                      const int erode_radius,
                                      const int dilate_radius) {
  std::cout << "Overwriting of values below threshold to air ";
  if (m_spCrntReconImg == nullptr) {
    return -1;
  }

  using threshFilterType =
      itk::BinaryThresholdImageFilter<UShortImageType, UShortImageType>;
  auto threshFilter = threshFilterType::New();
  threshFilter->SetInput(m_spCrntReconImg);

  threshFilter->SetOutsideValue(0);
  threshFilter->SetInsideValue(1);
  threshFilter->SetLowerThreshold(threshold);
  threshFilter->Update();
  const UShortImageType::Pointer spCrntImgMask = threshFilter->GetOutput();
  using iteratorType = itk::ImageRegionIteratorWithIndex<UShortImageType>;
  iteratorType it(spCrntImgMask, spCrntImgMask->GetRequestedRegion());
  auto imgDims = spCrntImgMask->GetBufferedRegion().GetSize();

  it.GoToBegin();
  while (!it.IsAtEnd()) {
    const int z_idx = it.GetIndex()[2];
    if (z_idx == static_cast<int>(imgDims[2] - 10) || z_idx == 10) {
      it.Set(1.0f);
    }
    ++it;
  }

  using HoleFillingFilterType = itk::BinaryFillholeImageFilter<UShortImageType>;
  auto HoleFillingFilter = HoleFillingFilterType::New();
  HoleFillingFilter->SetForegroundValue(1);
  HoleFillingFilter->SetFullyConnected(false);
  std::cout << "Threshold filtering.. ";
  HoleFillingFilter->SetInput(spCrntImgMask);

  using StructElementType =
      itk::BinaryBallStructuringElement<USHORT_PixelType, 3>;
  using ErodeFilterType =
      itk::BinaryErodeImageFilter<UShortImageType, UShortImageType,
                                  StructElementType>;
  using DilateFilterType =
      itk::BinaryDilateImageFilter<UShortImageType, UShortImageType,
                                   StructElementType>;
  auto binaryErode = ErodeFilterType::New();
  binaryErode->SetErodeValue(1);
  StructElementType erodeStructElement;
  erodeStructElement.SetRadius(erode_radius);
  erodeStructElement.CreateStructuringElement();
  binaryErode->SetKernel(erodeStructElement);
  std::cout << "filling holes.. ";
  binaryErode->SetInput(HoleFillingFilter->GetOutput());

  auto binaryDilate = DilateFilterType::New();
  binaryDilate->SetDilateValue(1);
  StructElementType dilateStructElement;
  dilateStructElement.SetRadius(dilate_radius);
  dilateStructElement.CreateStructuringElement();
  binaryDilate->SetKernel(dilateStructElement);
  std::cout << "eroding dirt.. ";
  binaryDilate->SetInput(binaryErode->GetOutput());
  std::cout << "Skin mask is being created..." << std::endl;

  using MaskFilterType =
      itk::MaskImageFilter<UShortImageType, UShortImageType, UShortImageType>;
  auto MaskFilter = MaskFilterType::New();
  MaskFilter->SetMaskingValue(0);
  std::cout << "Dilating.. ";
  MaskFilter->SetMaskImage(binaryDilate->GetOutput());
  if (m_spCrntReconImg == m_spRawReconImg) {
    MaskFilter->SetInput(m_spRawReconImg);
    MaskFilter->Update();
    m_spRawReconImg = MaskFilter->GetOutput();
    return 1;
  }
  if (m_spCrntReconImg == m_spRefCTImg) {
    MaskFilter->SetInput(m_spRefCTImg);
    MaskFilter->Update();
    m_spRefCTImg = MaskFilter->GetOutput();
    return 2;
  }
  if (m_spCrntReconImg == m_spScatCorrReconImg) {
    MaskFilter->SetInput(m_spScatCorrReconImg);
    MaskFilter->Update();
    m_spScatCorrReconImg = MaskFilter->GetOutput();
    return 3;
  }
  return 0;
}

// Below version is optimized for many points and much faster
void CbctRecon::ExportAngularWEPL_byFile(QString &strPathOutput,
                                         const double fAngleStart,
                                         const double fAngleEnd,
                                         const double fAngleGap) {
  if (strPathOutput.length() < 1) {
    return;
  }

  if (m_vPOI_DCM.empty()) {
    std::cout << "No POI data is prepared. Load them first" << std::endl;
    return;
  }

  if (m_spRawReconImg == nullptr) {
    std::cout << "Error: no Raw Recon image is found" << std::endl;
    return;
  }

  if (m_spScatCorrReconImg == nullptr) {
    std::cout << "Warning: no ScatCorrReconImg is found" << std::endl;
  }

  if (m_spManualRigidCT == nullptr) {
    std::cout << "Warning: no ManualRigidCT is found" << std::endl;
  }
  if (m_spAutoRigidCT == nullptr) {
    std::cout << "Warning: no AutoRigidCT is found" << std::endl;
  }
  if (m_spDeformedCT_Final == nullptr) {
    std::cout << "Warning: no DeformedCT is found" << std::endl;
  }

  std::vector<WEPLData> vOutputWEPL_manual;
  std::vector<WEPLData> vOutputWEPL_auto_rigid;
  std::vector<WEPLData> vOutputWEPL_deform;
  std::vector<WEPLData> vOutputWEPL_rawCBCT;
  std::vector<WEPLData> vOutputWEPL_corCBCT;
#pragma omp parallel sections
  {
#pragma omp section
    {
      GetAngularWEPL_window(m_spRawReconImg, fAngleGap, fAngleStart, fAngleEnd,
                            vOutputWEPL_rawCBCT, true); // mandatory
      std::cout << "Done: (RAW)";
    }
#pragma omp section
    {
      if (m_spScatCorrReconImg != nullptr) {
        try {
          GetAngularWEPL_window(m_spScatCorrReconImg, fAngleGap, fAngleStart,
                                fAngleEnd, vOutputWEPL_corCBCT, true);
          std::cout << " (COR)";
        } catch (std::exception &e) {
          std::cout << " (COR) failed!!: e=" << e.what() << std::endl;
        }
      }
    }
#pragma omp section
    {
      if (m_spManualRigidCT != nullptr) {
        try {
          GetAngularWEPL_window(m_spManualRigidCT, fAngleGap, fAngleStart,
                                fAngleEnd, vOutputWEPL_manual, true);
          std::cout << " (MAN)";
        } catch (std::exception &e) {
          std::cout << " (MAN) failed!!: e=" << e.what() << std::endl;
        }
      }
    }
#pragma omp section
    {
      if (m_spAutoRigidCT != nullptr) {
        try {
          GetAngularWEPL_window(m_spAutoRigidCT, fAngleGap, fAngleStart,
                                fAngleEnd, vOutputWEPL_auto_rigid, true);
          std::cout << " (AUT)";
        } catch (std::exception &e) {
          std::cout << " (AUT) failed!!: e=" << e.what() << std::endl;
        }
      }
    }
#pragma omp section
    {
      if (m_spDeformedCT_Final != nullptr) {
        try {
          GetAngularWEPL_window(m_spDeformedCT_Final, fAngleGap, fAngleStart,
                                fAngleEnd, vOutputWEPL_deform, true);
          std::cout << " (DEF)";
        } catch (std::exception &e) {
          std::cout << " (DEF) failed!!: e=" << e.what() << std::endl;
        }
      }
    }
  }
  std::cout << std::endl;

  std::cout << "Saving results...";

  std::ofstream fout;
  fout.open(strPathOutput.toLocal8Bit().constData());

  const int cntWEPL = vOutputWEPL_rawCBCT.size();

  fout << "Point Index"
       << "\t"
       << "Gantry Angle"
       << "\t"
       << "Sample Number"
       << "\t"
       << "RawCBCT"
       << "\t";

  if (m_spScatCorrReconImg != nullptr &&
      static_cast<int>(vOutputWEPL_corCBCT.size()) == cntWEPL) {
    fout << "CorrCBCT"
         << "\t";
  }
  if (m_spManualRigidCT != nullptr &&
      static_cast<int>(vOutputWEPL_manual.size()) == cntWEPL) {
    fout << "ManualRigidCT"
         << "\t";
  }
  if (m_spAutoRigidCT != nullptr &&
      static_cast<int>(vOutputWEPL_auto_rigid.size()) == cntWEPL) {
    fout << "AutoRigidCT"
         << "\t";
  }
  if (m_spDeformedCT_Final != nullptr &&
      static_cast<int>(vOutputWEPL_deform.size()) == cntWEPL) {
    fout << "DeformedCT"
         << "\t";
  }
  fout << std::endl;

  for (auto i = 0; i < cntWEPL; i++) {
    const auto cur_rawpoint = vOutputWEPL_rawCBCT.at(i);
    fout << cur_rawpoint.ptIndex << "\t" << cur_rawpoint.fGanAngle << "\t" << i
         << "\t" << cur_rawpoint.fWEPL << "\t";

    if (m_spScatCorrReconImg != nullptr &&
        static_cast<int>(vOutputWEPL_corCBCT.size()) == cntWEPL) {
      fout << vOutputWEPL_corCBCT.at(i).fWEPL << "\t";
    }
    if (m_spManualRigidCT != nullptr &&
        static_cast<int>(vOutputWEPL_manual.size()) == cntWEPL) {
      fout << vOutputWEPL_manual.at(i).fWEPL << "\t";
    }
    if (m_spAutoRigidCT != nullptr &&
        static_cast<int>(vOutputWEPL_auto_rigid.size()) == cntWEPL) {
      fout << vOutputWEPL_auto_rigid.at(i).fWEPL << "\t";
    }
    if (m_spDeformedCT_Final != nullptr &&
        static_cast<int>(vOutputWEPL_deform.size()) == cntWEPL) {
      fout << vOutputWEPL_deform.at(i).fWEPL << "\t";
    }

    fout << std::endl;
  }
  fout.close();
  std::cout << "done!" << std::endl;
}

void CbctRecon::GetAngularWEPL_window(UShortImageType::Pointer &spUshortImage,
                                      const float fAngleGap,
                                      const float fAngleStart,
                                      const float fAngleEnd,
                                      std::vector<WEPLData> &vOutputWEPLData,
                                      const bool bAppend) {
  if (spUshortImage == nullptr) {
    return;
  }

  if (fAngleGap <= 0) {
    return;
  }

  if (!bAppend) {
    vOutputWEPLData.clear();
  }

  const auto wepl_image = ConvertUshort2WeplFloat(spUshortImage);

  const double fullAngle = fAngleEnd - fAngleStart;
  const auto sizeAngles = qRound(fullAngle / fAngleGap);

  const std::array<double, 3> pixel_size = {{spUshortImage->GetSpacing()[0],
                                             spUshortImage->GetSpacing()[1],
                                             spUshortImage->GetSpacing()[2]}};

  const auto couch = 0.0;

  for (auto i = 0; i < sizeAngles; ++i) {
    const double gantry = fAngleStart + i * fAngleGap;
    const auto basis = get_basis_from_angles(gantry, couch);
    size_t loop_idx = 0;
    // int z_slice = -5000; // for progress and debug
    for (auto &poi_it : m_vPOI_DCM) {
      loop_idx++;
      UShortImageType::PointType cur_point;
      cur_point[0] = poi_it.x;
      cur_point[1] = poi_it.y;
      cur_point[2] = poi_it.z;

      UShortImageType::IndexType cur_idx{};
      if (!spUshortImage->TransformPhysicalPointToIndex(cur_point, cur_idx)) {
        // cur_point not in image
        continue;
      }
      /* // Turn on when debugging
      if (cur_idx[2] != z_slice) {
              z_slice = cur_idx[2];
              std::cout << z_slice << ", " << std::endl;
      }
      */
      const std::array<size_t, 3> point_id = {
          {static_cast<size_t>(cur_idx[0]), static_cast<size_t>(cur_idx[1]),
           static_cast<size_t>(cur_idx[2])}};

      WEPLData wepl_data{};
      wepl_data.fWEPL =
          WEPL_from_point(point_id, basis, pixel_size, wepl_image);
      wepl_data.ptIndex = loop_idx;
      wepl_data.fGanAngle = gantry;

      vOutputWEPLData.push_back(wepl_data);
    }
  }
}

void CbctRecon::GetAngularWEPL_SinglePoint(
    UShortImageType::Pointer &spUshortImage, const float fAngleGap,
    const float fAngleStart, const float fAngleEnd, const VEC3D &calcPt,
    const int curPtIdx, std::vector<WEPLData> &vOutputWEPLData,
    const bool bAppend) const {
  if (spUshortImage == nullptr) {
    return;
  }

  if (fAngleGap <= 0) {
    return;
  }

  const auto wepl_image = ConvertUshort2WeplFloat(spUshortImage);

  const double fullAngle = fAngleEnd - fAngleStart;
  const auto sizeAngles = static_cast<size_t>(qRound(fullAngle / fAngleGap));

  UShortImageType::PointType calc_point;
  calc_point[0] = calcPt.x;
  calc_point[1] = calcPt.y;
  calc_point[2] = calcPt.z;
  UShortImageType::IndexType calc_idx{};

  if (!spUshortImage->TransformPhysicalPointToIndex(calc_point, calc_idx)) {
    std::cerr << "Point was outside image!" << std::endl;
  }

  // 1) Generate parms according to the angle e.g 360 parms
  const std::array<size_t, 3> isoTarget = {{static_cast<size_t>(calc_idx[0]),
                                            static_cast<size_t>(calc_idx[1]),
                                            static_cast<size_t>(calc_idx[2])}};

  std::cout << "Target: ( " << isoTarget[0] << ", " << isoTarget[1] << ", "
            << isoTarget[2] << " ), sizeAngles: " << sizeAngles << std::endl;

  const std::string stdout_file = "WEPL_stdout.txt";

  std::ofstream ofs(stdout_file); // Open stdout_file for writing
  if (ofs.is_open()) {
    std::cerr
        << "couldn't open file: " << stdout_file << " for writing!\n"
        << "Are you running this app from a folder without write permissions?"
        << std::endl;
    return;
  }
  const std::array<double, 3> pixel_size = {{wepl_image->GetSpacing()[0],
                                             wepl_image->GetSpacing()[1],
                                             wepl_image->GetSpacing()[2]}};

  auto region = wepl_image->GetLargestPossibleRegion();

  std::vector<WEPLData> stArrWEPL(sizeAngles);

  size_t i = 0;
  for (auto &it : stArrWEPL) {
    // YKTEMP Should be updated according to recent update of plastimatch
    const auto curAngle = fAngleStart + i * fAngleGap;

    const auto basis = get_basis_from_angles(curAngle, 0.0);

    ofs << std::fixed << std::setprecision(3) << curAngle << ", [" << basis[0]
        << ", " << basis[1] << ", " << basis[2] << "]: ";

    it.fWEPL = WEPL_from_point(isoTarget, basis, pixel_size,
                               wepl_image); // get_rgdepth
    it.fGanAngle = curAngle;
    it.ptIndex = curPtIdx;
    ofs << std::fixed << std::setprecision(5) << it.fWEPL << "\n";
    i++;
  }

  ofs.close();

  if (!bAppend) {
    vOutputWEPLData.clear();
  }
  vOutputWEPLData.insert(vOutputWEPLData.end(), stArrWEPL.begin(),
                         stArrWEPL.end());
  /*
for (int i = 0; i < sizeAngles; i++)
{
  vOutputWEPLData.push_back(stArrWEPL[i]);
}*/

  // delete[] stArrWEPL;
}

void CbctRecon::GetAngularWEPL_MultiPoint(
    UShortImageType::Pointer &spUshortImage, const float fAngleGap,
    const float fAngleStart, const float fAngleEnd,
    std::vector<WEPLData> &vOutputWEPLData, const bool bAppend) {
  if (spUshortImage == nullptr) {
    return;
  }

  if (fAngleGap <= 0) {
    return;
  }

  ShortImageType::Pointer spShortImg;
  ConvertUshort2Short(spUshortImage, spShortImg);

  using CastFilterType = itk::CastImageFilter<ShortImageType, FloatImageType>;
  auto castFilter = CastFilterType::New();
  castFilter->SetInput(spShortImg);
  castFilter->Update();

  // Plm_image::Pointer ct_vol = Plm_image::New (spShortImg);
  auto ct_vol = Plm_image::New(
      castFilter->GetOutput()); // Oh ohh.. not actually a pointer now is it?

  const double fullAngle = fAngleEnd - fAngleStart;
  const auto sizeAngles = qRound(fullAngle / fAngleGap);

  // 1) Generate parms according to the angle e.g 360 parms
  double isoTarget[3];
  // isoTarget[0] = ui.lineEdit_ForcedProbePosX->text().toDouble(); //in mm
  // isoTarget[1] = ui.lineEdit_ForcedProbePosY->text().toDouble();
  // isoTarget[2] = ui.lineEdit_ForcedProbePosZ->text().toDouble();

  const auto srcDistance = 2200.0f; // in mm, 2.2 m
  float srcProton[3] = {0.0f, 0.0f, 0.0f};

  const auto ap_distance = 1900.0f; // 300 mm from the target //offset from the
                                    // source float ap_distance = 1000.0; //

  float ap_spacing[2] = {1.0f, 1.0f}; // resolution
  const plm_long ap_dim[2] = {1, 1};
  float ap_center[2] = {1.0f, 1.0f};

  const auto ray_step = 1.0f; // mm

  const auto sizePOI = m_vPOI_DCM.size();
  std::vector<WEPLData> stArrWEPL;
  stArrWEPL.reserve(sizePOI * sizeAngles);

  // std::cout << "Everything is OK!? Let's calculate WEPL...\n" << std::endl;

  const std::string stdout_file = "WEPL_stdout.txt";

  std::ofstream ofs(stdout_file); // Open stdout_file for writing
  if (ofs.is_open()) {
    std::cerr
        << "couldn't open file: " << stdout_file << " for writing!\n"
        << "Are you running this app from a folder without write permissions?"
        << std::endl;
    return;
  }

#pragma omp parallel for
  for (auto p = 0; p < static_cast<int>(sizePOI); p++) {
    const auto curPOI = m_vPOI_DCM.at(p);
    isoTarget[0] = curPOI.x; // Sorry, might do better later..
    isoTarget[1] = curPOI.y;
    isoTarget[2] = curPOI.z;

    itk::Point<double, 3U> itk_isoTarget;
    itk_isoTarget[0] = curPOI.x;
    itk_isoTarget[1] = curPOI.y;
    itk_isoTarget[2] = curPOI.z;

    auto region = spShortImg->GetLargestPossibleRegion();

    auto target_bool = UCharImageType::New();
    target_bool->SetRegions(region);
    target_bool->SetDirection(spShortImg->GetDirection());
    target_bool->SetSpacing(spShortImg->GetSpacing());
    target_bool->SetOrigin(spShortImg->GetOrigin());
    target_bool->Allocate();
    UCharImageType::IndexType pixelIndex{};
    if (!target_bool->TransformPhysicalPointToIndex(itk_isoTarget,
                                                    pixelIndex)) {
      // itk_isoTarget not in image
      continue;
    }
    target_bool->SetPixel(pixelIndex, 255);
    // GetAngularWEPL_SinglePoint(m_spCrntReconImg, fAngleGap, curPOI, i,
    // vOutputWEPL, true);  if (m_spRawReconImg)
    // GetAngularWEPL_MultiPoint(m_spRawReconImg, fAngleGap, fAngleStart,
    // fAngleEnd, curPOI, i, vOutputWEPL_rawCBCT, true);//mandatory

    ofs << "POI idx: " << p
        << ", calculation for all given angles startng...\n";

    // for (int i = qRound(fAngleStart / fAngleGap); i < (sizeAngles +
    // qRound(fAngleStart / fAngleGap)); i++)
    for (auto i = 0; i < sizeAngles; i++) {
      // YKTEMP Should be updated according to recent update of plastimatch
      const auto curAngle = fAngleStart + i * fAngleGap;

      srcProton[0] = isoTarget[0] + srcDistance * sin(curAngle * M_PI / 180.0);
      srcProton[1] = isoTarget[1] - srcDistance * cos(curAngle * M_PI / 180.0);
      srcProton[2] = isoTarget[2];

      // std::cout << "\nDefine scene.." << std::endl;
      auto scene = std::make_shared<Plan_calc>(); // Rt_plan>();
      // std::cout << "New beam.." << std::endl;
      // Rt_beam *newBeam = scene->append_beam(); // . to ->
      scene->append_beam();
      scene->set_patient(ct_vol); // . to ->
      auto newBeam = scene->get_last_rt_beam();
      auto p_aperture = newBeam->get_aperture().get();
      p_aperture->set_distance(ap_distance);
      p_aperture->set_distance(ap_distance);
      p_aperture->set_spacing(&ap_spacing[0]);
      p_aperture->set_dim(&ap_dim[0]);
      p_aperture->set_center(&ap_center[0]);

      newBeam->set_step_length(ray_step);
      newBeam->set_isocenter_position(&isoTarget[0]);
      newBeam->set_source_position(&srcProton[0]);

      // std::cout << "\nPrep beam.." << std::endl;
      // scene.prepare_beam_for_calc(newBeam);

      scene->set_target(target_bool);
      // std::string flavor("b");
      // newBeam->set_flavor(flavor); // b for beta to get fastest dose calc
      // scene->compute_dose(newBeam);

      auto ct_hu = Plm_image::New();
      ct_hu->set_volume(scene->get_patient_volume());

      using Float_pair_list = std::list<std::pair<float, float>>;
      Float_pair_list lookup;
      lookup.emplace_back(NLMIN(float), 0.0f);
      lookup.emplace_back(-1000.0f, 0.00106f);
      lookup.emplace_back(0.0f, 1.0f);
      lookup.emplace_back(41.46f, 1.048674f);
      lookup.emplace_back(NLMAX(float), 0.005011f);

      auto psp = volume_adjust(ct_hu->get_volume(), lookup);
      auto patient_psp = Plm_image::New(psp);

      newBeam->prepare_for_calc(ct_hu, patient_psp, newBeam->get_target());

      // scene->create_patient_psp(); // hu to stopping power
      // std::string flavor("b");
      // newBeam->set_flavor(flavor); // b for beta to get fastest dose calc
      // scene->compute_dose(newBeam); // ONLY way to call prepare_for_calc from
      // the "outside" (without ugly tricks)
      // newBeam->prepare_for_calc(ct_vol, newBeam->get_ct_psp(),
      // newBeam->get_target());
      // wed_ct_compute in wed_main

      auto rpl_vol = newBeam->rsp_accum_vol; // rpl_vol;

      // rpl_vol->compute_proj_wed_volume()

      auto proj_vol = rpl_vol->get_proj_volume();
      // float *proj_wed_vol_img = (float*) rpl_vol->proj_wed_vol->img;
      // std::cout << "\nA lot of const doubles..";

      const auto src = proj_vol->get_src();
      const auto iso = proj_vol->get_iso();
      const auto sid_length =
          proj_vol->get_proj_matrix()->sid; // distance from source to aperture
      double src_iso_vec[3];
      vec3_sub3(&src_iso_vec[0], src, iso);
      const auto src_iso_distance = vec3_len(&src_iso_vec[0]);
      const auto ap_iso_distance = src_iso_distance - sid_length;
      // std::cout << " front clipping plane..";
      const auto base_rg_dist =
          ap_iso_distance - rpl_vol->get_front_clipping_plane();

      const auto base_dist =
          proj_vol->get_proj_matrix()->sid; // distance from source to aperture

      // const plm_long *ires = proj_vol->get_image_dim();

      // int ap_ij[2]; //ray index of rvol
      // plm_long ap_idx = 0;  //ray number always 0 here

      double ray_ap[3]; // std::vector from src to ray intersection with ap
      // std::cout << " get ray data.. " << std::endl;
      rpl_vol->compute_ray_data(); // computes p2 <- called in compute dose for
                                   // some flavors of beam
      const auto ray_data = rpl_vol->get_ray_data();

      const auto ray_data_single = &ray_data[0]; // ap_idx];

      /* Coordinate of ray intersection with aperture plane */
      const auto ap_xyz = &ray_data_single->p2[0];
      vec3_sub3(&ray_ap[0], ap_xyz, src);
      const auto ray_ap_length = vec3_len(&ray_ap[0]);
      const auto rglength = base_rg_dist * (ray_ap_length / base_dist);

      plm_long ap_idx_default[2] = {0, 0};
      WEPLData curWEPLData{};
      curWEPLData.fWEPL =
          rpl_vol->get_value(ap_idx_default, rglength); // rgdepth
      curWEPLData.fGanAngle = curAngle;
      curWEPLData.ptIndex = p;

      stArrWEPL.push_back(curWEPLData);
      // This should be in the Rt_beam destructor, but it isn't
      delete newBeam->rsp_accum_vol;
      delete newBeam->hu_samp_vol;
      delete newBeam->sigma_vol;
      delete newBeam->rpl_vol_lg;
      delete newBeam->rpl_vol_samp_lg;
      delete newBeam->sigma_vol_lg;
      delete newBeam->dose_rv; // rpl_dose_vol;

      delete newBeam;
      // delete scene; not necessary for shared_ptr
    }
  }

  // std::cout << "Return results for saving.." << std::endl;

  if (!bAppend) {
    vOutputWEPLData.clear();
  }
  vOutputWEPLData.insert(vOutputWEPLData.end(), stArrWEPL.begin(),
                         stArrWEPL.end());
  /*
  for (int p = 0; p < sizePOI; p++)
  {
          for (auto i = int(fAngleStart / fAngleGap); i < sizeAngles; i++)
          {
                  vOutputWEPLData.push_back(stArrWEPL[i - qRound(fAngleStart /
  fAngleGap) + p * sizeAngles]);
          }
  }

  delete[] stArrWEPL;
  */
  ct_vol->free();
  ofs.close();
}

void CbctRecon::GeneratePOIData(const bool AnteriorToPosterior,
                                const double table_posY) // it fills m_vPOI_DCM
{
  if (!m_vPOI_DCM.empty()) {
    m_vPOI_DCM.clear();
  }

  auto imgSize = m_spCrntReconImg->GetLargestPossibleRegion().GetSize();
  const VEC3D imgDims = {static_cast<double>(imgSize[0]),
                         static_cast<double>(imgSize[1]),
                         static_cast<double>(imgSize[2])};
  const VEC3D imgSpac{m_spCrntReconImg->GetSpacing()[0],
                      m_spCrntReconImg->GetSpacing()[1],
                      m_spCrntReconImg->GetSpacing()[2]};

  if (AnteriorToPosterior) {
    for (size_t k = 2; k < imgDims.z - 2; k++) {
      for (size_t i = 2; i < imgDims.x - 2; i++) {
        VEC3D fPOI = {i * imgSpac.x - (imgSpac.x * imgDims.x) / 2., table_posY,
                      k * imgSpac.z - (imgSpac.z * imgDims.z) / 2.};
        m_vPOI_DCM.push_back(fPOI);
      }
    }
  } else {
    for (size_t k = 2; k < imgDims.z - 2; k++) {
      for (size_t j = 2; j < imgDims.y - 2; j++) {
        VEC3D fPOI = {2, j * imgSpac.y - (imgSpac.y * imgDims.y) / 2.,
                      k * imgSpac.z - (imgSpac.z * imgDims.z) / 2.};
        m_vPOI_DCM.push_back(fPOI);
      }
    }
  }
  const auto last_point = m_vPOI_DCM.back();
  std::cout << "POI data generated! last value: [" << last_point.x << ", "
            << last_point.y << ", " << last_point.z << "]" << std::endl;
}

void CbctRecon::LoadExternalFloatImage(QString &strPath,
                                       const bool bConversion) {
  using ReaderType = itk::ImageFileReader<FloatImageType>;
  auto reader = ReaderType::New();

  // QString filePath = strPath;

  reader->SetFileName(strPath.toLocal8Bit().constData());
  reader->Update();

  FloatImageType::Pointer spCrntImg = reader->GetOutput();

  // Float image
  std::cout << "Float image has been loaded" << std::endl;

  if (bConversion) {
    TransformationRTK2IEC(spCrntImg);
  }

  using AbsImageFilterType =
      itk::AbsImageFilter<FloatImageType, FloatImageType>;
  auto absImgFilter = AbsImageFilterType::New();
  absImgFilter->SetInput(spCrntImg); // 20140206 modified it was a buug

  using MultiplyImageFilterType =
      itk::MultiplyImageFilter<FloatImageType, FloatImageType, FloatImageType>;
  auto multiplyImageFilter = MultiplyImageFilterType::New();
  multiplyImageFilter->SetInput(absImgFilter->GetOutput());
  multiplyImageFilter->SetConstant(65536); // calculated already

  using CastFilterType = itk::CastImageFilter<FloatImageType, UShortImageType>;
  auto castFilter = CastFilterType::New();
  castFilter->SetInput(multiplyImageFilter->GetOutput());
  castFilter->Update();
  m_spRawReconImg = castFilter->GetOutput();
}

// Only can be used for m_spRawRecon
void CbctRecon::MedianFilterByGUI(
    const UShortImageType::SizeType &indexRadius) {
  using FilterType = itk::MedianImageFilter<UShortImageType, UShortImageType>;
  auto medFilter = FilterType::New();

  // this is radius. 1 --> median window 3
  std::cout << "Post median(3D) filtering is under progress..Size(radius X Y "
               "Z) is = "
            << indexRadius << std::endl;

  medFilter->SetRadius(indexRadius);
  medFilter->SetInput(m_spCrntReconImg);
  medFilter->Update();

  m_spCrntReconImg = medFilter->GetOutput();
  std::cout << "median filtering has been done" << std::endl;
}

void CbctRecon::Export2DDoseMapAsMHA(QString &strPath) const {
  if (m_dspYKReconImage == nullptr) {
    return;
  }

  if (m_spCrntReconImg == nullptr) {
    return;
  }

  if (strPath.length() <= 1) {
    return;
  }

  const auto originLeft = static_cast<double>(m_spCrntReconImg->GetOrigin()[0]);
  const auto originTop =
      static_cast<double>(m_spCrntReconImg->GetOrigin()[1]); // not sure...

  const auto spacingX = static_cast<double>(m_spCrntReconImg->GetSpacing()[0]);
  const auto spacingY =
      static_cast<double>(m_spCrntReconImg->GetSpacing()[1]); // not sure...

  // Export float 2D image
  auto doseImg2D = FloatImage2DType::New();
  FloatImage2DType::SizeType doseSize{};
  doseSize[0] = m_dspYKReconImage->m_iWidth;
  doseSize[1] = m_dspYKReconImage->m_iHeight;

  FloatImage2DType::IndexType doseStart{};
  doseStart[0] = 0;
  doseStart[1] = 0;

  FloatImage2DType::RegionType doseRegion;
  doseRegion.SetSize(doseSize);
  doseRegion.SetIndex(doseStart);

  FloatImage2DType::SpacingType doseSpacing;
  doseSpacing[0] = spacingX;
  doseSpacing[1] = spacingY;

  FloatImage2DType::PointType doseOrigin;
  doseOrigin[0] = originLeft;
  doseOrigin[1] = originTop;

  doseImg2D->SetRegions(doseRegion);
  doseImg2D->SetSpacing(doseSpacing);
  doseImg2D->SetOrigin(doseOrigin);

  doseImg2D->Allocate();
  doseImg2D->FillBuffer(0);

  const auto factor_ushort2float = 0.01; // cGy --> Gy

  itk::ImageRegionIterator<FloatImage2DType> it(
      doseImg2D, doseImg2D->GetLargestPossibleRegion());

  auto pixel_val = 0.0f;
  auto i = 0;
  for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
    pixel_val = static_cast<double>(m_dspYKReconImage->m_pData[i]) *
                factor_ushort2float;
    it.Set(pixel_val);
    i++;
  }
  // YK201502
  using WriterType = itk::ImageFileWriter<FloatImage2DType>;
  auto writer = WriterType::New();
  writer->SetFileName(strPath.toLocal8Bit().constData());
  writer->SetUseCompression(true);
  writer->SetInput(doseImg2D);
  writer->Update();

  std::cout << "File was exported successfully" << std::endl;
}

void CbctRecon::ExportProjGeometryTXT(QString &strPath) const {
  // if (!m_spFullGeometry)
  //	return;

  if (m_spCustomGeometry ==
      nullptr) { // will be filled after Projection load button is pushed
    return;
  }

  if (strPath.length() <= 1) {
    return;
  }

  const int cntAngle = m_spCustomGeometry->GetGantryAngles().size();
  const int cntShiftX = m_spCustomGeometry->GetProjectionOffsetsX().size();
  const int cntShiftY = m_spCustomGeometry->GetProjectionOffsetsY().size();

  if (cntAngle <= 0) {
    std::cout << "Error! no angle std::vector is found" << std::endl;
    return;
  }

  if (cntAngle != cntShiftX || cntAngle != cntShiftY) {
    std::cout << "Error! Angle number and shift number are not matching."
              << std::endl;
    return;
  }

  auto itShiftX = m_spCustomGeometry->GetProjectionOffsetsX().begin();
  auto itShiftY = m_spCustomGeometry->GetProjectionOffsetsY().begin();

  std::ofstream fout;
  fout.open(strPath.toLocal8Bit().constData());

  fout << "MV_Gantry_Angle"
       << "	"
       << "PanelShiftX(mm)"
       << "	"
       << "PanelShiftY(mm)" << std::endl;

  for (auto &itAng : m_spCustomGeometry->GetGantryAngles()) {
    fout << itAng << "	" << *itShiftX << "	" << *itShiftY << std::endl;

    ++itShiftX;
    ++itShiftY;
  }

  fout.close();
}

bool CbctRecon::LoadXVIGeometryFile(const char *filePath) {
  const auto strFilePath = QString(filePath);

  m_spFullGeometry = GeometryType::New();

  /* We'll parse the example.xml */
  auto *file = new QFile(strFilePath);
  /* If we can't open it, let's show an error message. */
  if (!file->open(QIODevice::ReadOnly | QIODevice::Text)) {
    return false;
  }
  /* QXmlStreamReader takes any QIODevice. */
  QXmlStreamReader xml(file);
  // QList< QMap<QString, QString> > persons;
  /* We'll parse the XML until we reach end of it.*/

  m_vExcludeProjIdx.clear();
  auto iIdx = 0;

  while (!xml.atEnd() && !xml.hasError()) {
    /* Read next element.*/
    const auto token = xml.readNext();
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
      if (xml.name() == "Frame") {
        auto flxData = XML_parseFrameForXVI5(xml);
        // m_vRetroFlexmap.push_back(flxData);

        if (flxData.fGanAngle < 0) {
          flxData.fGanAngle = flxData.fGanAngle + 360.0;
        }

        flxData.fPanelOffsetX = -flxData.fPanelOffsetX;
        flxData.fPanelOffsetY = -flxData.fPanelOffsetY;

        if (!flxData.bKV_On) {
          m_vExcludeProjIdx.push_back(iIdx);
        }

        /*               if (flxData.bKV_On)
                           m_vExcludeProjIdx.push_back(iIdx);*/

        ////Image qual test
        // flxData.fGanAngle = -flxData.fGanAngle;
        // if (flxData.fGanAngle < 0)
        //	flxData.fGanAngle = flxData.fGanAngle + 360.0;
        // flxData.fPanelOffsetX = -flxData.fPanelOffsetX;
        // flxData.fPanelOffsetY = -flxData.fPanelOffsetY;

        m_spFullGeometry->AddProjection(1000.0, 1536.0, flxData.fGanAngle,
                                        flxData.fPanelOffsetX,
                                        flxData.fPanelOffsetY, // Flexmap
                                        0.0, 0.0,  // In elekta, these are 0
                                        0.0, 0.0); // In elekta, these are 0

        iIdx++;
      }
    }
  }
  /* Error handling. */
  if (xml.hasError()) {
    m_strError = xml.errorString();
    return false;
  }
  /* Removes any device() or data from the reader
   * and resets its internal state to the initial state. */
  xml.clear();
  return true;
}

void CbctRecon::GenerateCylinderMask(UShortImageType::Pointer &spImgCanvas,
                                     const float fDcmPosX, const float fDcmPosY,
                                     const float fRadius) const {
  if (spImgCanvas == nullptr) {
    return;
  }
  // 1) region iterator, set 0 for all pixels outside the circle and below the
  // table top, based on physical position
  auto origin = spImgCanvas->GetOrigin();
  auto spacing = spImgCanvas->GetSpacing();
  // UShortImageType::SizeType size =
  // spImgCanvas->GetBufferedRegion().GetSize();

  // itk::ImageSliceConstIteratorWithIndex<FloatImageType> it (m_spReconImg,
  // m_spReconImg->GetRequestedRegion());
  itk::ImageSliceIteratorWithIndex<UShortImageType> it(
      spImgCanvas, spImgCanvas->GetRequestedRegion());

  // ImageSliceConstIteratorWithIndex<ImageType> it( image,
  // image->GetRequestedRegion() );  UShortImageType::SizeType imgSize =
  // spImgCanvas->GetRequestedRegion().GetSize(); //1016x1016 x z

  // int width = imgSize[0];
  // int height = imgSize[1];

  it.SetFirstDirection(0);  // x?
  it.SetSecondDirection(1); // y?
  it.GoToBegin();

  auto iNumSlice = 0;

  // int i = 0;//height
  // int j = 0; // width

  while (!it.IsAtEnd()) {
    auto iPosY = 0;
    while (!it.IsAtEndOfSlice()) {
      auto iPosX = 0;
      while (!it.IsAtEndOfLine()) {
        // Calculate physical position

        const auto crntPhysX = iPosX * static_cast<double>(spacing[0]) +
                               static_cast<double>(origin[0]);
        const auto crntPhysY = iPosY * static_cast<double>(spacing[1]) +
                               static_cast<double>(origin[1]);

        if (pow(crntPhysX - fDcmPosX, 2.0) + pow(crntPhysY - fDcmPosY, 2.0) >=
            pow(fRadius, 2.0)) {
          //(*it) = (unsigned short)0; //air value
          it.Set(0);
        } else {
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

float CbctRecon::GetMeanIntensity(UShortImageType::Pointer &spImg,
                                  const float sphereR,
                                  float *sdIntensity) const {
  if (spImg == nullptr) {
    return -1.0;
  }

  // 1) region iterator, set 0 for all pixels outside the circle and below the
  // table top, based on physical position
  auto origin = spImg->GetOrigin();
  auto spacing = spImg->GetSpacing();
  // UShortImageType::SizeType size = spImg->GetBufferedRegion().GetSize();

  itk::ImageSliceIteratorWithIndex<UShortImageType> it(
      spImg, spImg->GetRequestedRegion());
  // UShortImageType::SizeType imgSize = spImg->GetRequestedRegion().GetSize();
  // //1016x1016 x z

  // int width = imgSize[0];
  // int height = imgSize[1];

  it.SetFirstDirection(0);  // x?
  it.SetSecondDirection(1); // y?
  it.GoToBegin();

  auto iNumSlice = 0;

  auto pixSum = 0.0;
  auto iCnt = 0;

  while (!it.IsAtEnd()) {
    auto iPosY = 0;
    while (!it.IsAtEndOfSlice()) {
      auto iPosX = 0;
      while (!it.IsAtEndOfLine()) {
        // Calculate physical position

        const auto crntPhysX = iPosX * static_cast<double>(spacing[0]) +
                               static_cast<double>(origin[0]);
        const auto crntPhysY = iPosY * static_cast<double>(spacing[1]) +
                               static_cast<double>(origin[1]);
        const auto crntPhysZ = iNumSlice * static_cast<double>(spacing[2]) +
                               static_cast<double>(origin[2]);

        if (pow(crntPhysX, 2.0) + pow(crntPhysY, 2.0) + pow(crntPhysZ, 2.0) <
            pow(sphereR, 2.0)) {
          pixSum = pixSum + static_cast<double>(it.Get());
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

  float meanIntensity;
  if (iCnt > 0) {
    meanIntensity = pixSum / static_cast<double>(iCnt);
  } else {
    meanIntensity = -1.0;
  }

  if (sdIntensity == nullptr) {
    return meanIntensity;
  }

  auto devSum = 0.0;
  it.GoToBegin();

  iNumSlice = 0;

  while (!it.IsAtEnd()) {
    auto iPosY = 0;
    while (!it.IsAtEndOfSlice()) {
      auto iPosX = 0;
      while (!it.IsAtEndOfLine()) {
        // Calculate physical position

        const auto crntPhysX = iPosX * static_cast<double>(spacing[0]) +
                               static_cast<double>(origin[0]);
        const auto crntPhysY = iPosY * static_cast<double>(spacing[1]) +
                               static_cast<double>(origin[1]);
        const auto crntPhysZ = iNumSlice * static_cast<double>(spacing[2]) +
                               static_cast<double>(origin[2]);

        if (pow(crntPhysX, 2.0) + pow(crntPhysY, 2.0) + pow(crntPhysZ, 2.0) <
            pow(sphereR, 2.0)) {
          devSum =
              devSum + pow(static_cast<double>(it.Get()) - meanIntensity, 2.0);
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

  if (iCnt > 0) {
    *sdIntensity = sqrt(devSum / static_cast<double>(iCnt));
  } else {
    *sdIntensity = -1.0;
  }

  return meanIntensity;
}

bool CbctRecon::ResortCBCTProjection(
    std::vector<int> &vIntPhaseBinSelected, QString &strPathForXML,
    QString &strPathProjRoot, QString &strUID,
    std::vector<float> &vFloatPhaseFull, GeometryType::Pointer &spGeomFull,
    std::vector<std::string> &vProjPathsFull) const {
  if (vIntPhaseBinSelected.empty()) {
    return false;
  }

  const int NumOfPhaseFull = vFloatPhaseFull.size();
  const int NumOfGeomFull = spGeomFull->GetGantryAngles().size();
  const int NumOfProjFileFull = vProjPathsFull.size();

  if (NumOfPhaseFull != NumOfGeomFull || NumOfGeomFull != NumOfProjFileFull) {
    std::cout << "Num of data is not matching:"
              << " NumOfPhaseFull= " << NumOfPhaseFull
              << " NumOfGeomFull= " << NumOfGeomFull
              << " NumOfProjFileFull= " << NumOfProjFileFull << std::endl;
    return false;
  }

  // Check Root dir is set

  if (strUID.length() < 1) {
    return false;
  }

  QDir dirSaveXML(strPathForXML);
  QDir dirSaveProj(strPathProjRoot);

  if (!dirSaveXML.exists() || !dirSaveProj.exists()) {
    std::cout << "Error! Directories don't exist" << std::endl;
    return false;
  }
  const int iNumOfSelPhase = vIntPhaseBinSelected.size();

  QString strUID_Endfix = "P";
  const QChar zero('0');
  for (auto i = 0; i < iNumOfSelPhase; i++) {
    QString strNum;
    strNum = QString("%1").arg(vIntPhaseBinSelected.at(i), 2, 10, zero);
    strUID_Endfix = strUID_Endfix + strNum;
  }
  strUID_Endfix = strUID_Endfix + "P"; // UID...P00102030405060P
  const auto strNewUID = strUID + strUID_Endfix;

  // Create a subDir
  QDir curProjRoot(strPathProjRoot);
  const auto strSubDirName = "img_" + strNewUID;
  if (!curProjRoot.mkdir(strSubDirName)) {
    std::cerr << "Could not make subdir" << std::endl;
    return false;
  }

  const auto strPathProj = strPathProjRoot + "/" + strSubDirName;

  QDir projDir(strPathProj);
  if (!projDir.exists()) {
    std::cout << "no Proj Dir exists" << std::endl;
    return false;
  }

  QDir xmlDir(strPathForXML);
  if (!xmlDir.exists()) {
    std::cout << "no XML Dir exists" << std::endl;
    return false;
  }

  // strPathProj
  // strPathForXML
  std::vector<int> vSelectedIdxTemp;
  std::vector<int> vSelectedIdxFin;

  for (auto i = 0; i < iNumOfSelPhase; i++) {
    AppendInPhaseIndex(vIntPhaseBinSelected.at(i), vFloatPhaseFull,
                       vSelectedIdxTemp);
  }
  // Remove redandancy

  sort(vSelectedIdxTemp.begin(), vSelectedIdxTemp.end()); // hopefully,
                                                          // ascending
  std::cout << "sorting check" << std::endl;
  std::cout << "0 " << vSelectedIdxTemp.at(0) << std::endl;
  std::cout << "1 " << vSelectedIdxTemp.at(1) << std::endl;

  auto prevVal = -1;
  for (auto &it : vSelectedIdxTemp) {
    if (it > prevVal) {
      vSelectedIdxFin.push_back(it);
    }

    prevVal = it;
  }

  auto spSubGeometry = GeometryType::New();

  for (auto &itIdx : vSelectedIdxFin) {
    std::cout << "cur Idx=" << itIdx << std::endl;
    // 9 parameters are required
    const auto curSID = spGeomFull->GetSourceToIsocenterDistances().at(itIdx);
    const auto curSDD = spGeomFull->GetSourceToDetectorDistances().at(itIdx);
    const auto curGantryAngle = spGeomFull->GetGantryAngles().at(itIdx);

    const auto curProjOffsetX = spGeomFull->GetProjectionOffsetsX().at(itIdx);
    const auto curProjOffsetY = spGeomFull->GetProjectionOffsetsY().at(itIdx);

    const auto curOutOfPlaneAngles =
        spGeomFull->GetOutOfPlaneAngles().at(itIdx);
    const auto curInPlaneAngles = spGeomFull->GetInPlaneAngles().at(itIdx);

    const auto curSrcOffsetX = spGeomFull->GetSourceOffsetsX().at(itIdx);
    const auto curSrcOffsetY = spGeomFull->GetSourceOffsetsY().at(itIdx);

    spSubGeometry->AddProjection(
        curSID, curSDD, curGantryAngle, curProjOffsetX,
        curProjOffsetY,                        // Flexmap
        curOutOfPlaneAngles, curInPlaneAngles, // In elekta, these are 0
        curSrcOffsetX, curSrcOffsetY);         // In elekta, these are 0
  }
  // Export spSubGeometry
  auto xmlWriter = rtk::ThreeDCircularProjectionGeometryXMLFileWriter::New();

  const auto geomFileName = "ElektaGeom_" + strNewUID + ".xml";
  auto geomFilePath = strPathForXML + "/" + geomFileName;

  xmlWriter->SetFilename(geomFilePath.toLocal8Bit().constData());
  xmlWriter->SetObject(spSubGeometry);
  TRY_AND_EXIT_ON_ITK_EXCEPTION(xmlWriter->WriteFile());
  // Copy selected his files to a different folder

  for (auto &itIdx : vSelectedIdxFin) {
    QString strPathProjOriginal = vProjPathsFull.at(itIdx).c_str();
    // Copy this file to target dir

    QFileInfo fInfo(strPathProjOriginal);
    auto strPathProjNew = strPathProj + "/" + fInfo.fileName();
    QFile::copy(fInfo.absoluteFilePath(), strPathProjNew);
  }

  //    std::vector<float>& vFloatPhaseFull, GeometryType::Pointer& spGeomFull,
  //    std::vector<std::string>& vProjPathsFull
  std::cout << vSelectedIdxFin.size() << " files were copied." << std::endl;

  return true;
}

void CbctRecon::AppendInPhaseIndex(const int iPhase,
                                   std::vector<float> &vFloatPhaseFull,
                                   std::vector<int> &vOutputIndex,
                                   const int margin) const {

  const int iNumOfPhase = vFloatPhaseFull.size();

  int startPhase1;
  int endPhase1;

  int startPhase2;
  int endPhase2;

  for (auto i = 0; i < iNumOfPhase; i++) {
    const auto iCurPhase = qRound(vFloatPhaseFull.at(i) * 100.0);
    // determine wether it is within the range

    if (iPhase < margin) // if 5 --> 0 ~ 10%, IF 4--> 99 ~ 09
    {
      startPhase2 = iPhase + 100 - margin;
      endPhase2 = 100;

      startPhase1 = 0;
      endPhase1 = iPhase + margin;
    } else {
      startPhase1 = iPhase - margin;
      endPhase1 = iPhase + margin;

      startPhase2 = 1; // reverse
      endPhase2 = 0;
    }

    if ((iCurPhase >= startPhase1 && iCurPhase <= endPhase1) ||
        (iCurPhase >= startPhase2 && iCurPhase <= endPhase2)) {
      vOutputIndex.push_back(i);
    }
  }
}

void CbctRecon::LoadShort3DImage(QString &filePath,
                                 const enREGI_IMAGES enTarget) {
  QFileInfo fInfo(filePath);
  if (!fInfo.exists()) {
    return;
  }

  UShortImageType::Pointer spImg;

  if (!LoadShortImageToUshort(filePath, spImg)) {
    std::cout << "error! in LoadShortImageToUshort" << std::endl;
  }

  switch (enTarget) {
  case REGISTER_RAW_CBCT:
    m_spRawReconImg = spImg;
    break;
  case REGISTER_COR_CBCT:
    m_spScatCorrReconImg = spImg;
    break;
  case REGISTER_MANUAL_RIGID:
    m_spManualRigidCT = spImg;
    break;
  case REGISTER_AUTO_RIGID:
    m_spAutoRigidCT = spImg;
    break;
  case REGISTER_DEFORM_FINAL:
    m_spDeformedCT_Final = spImg;
    break;
  default:
    m_spRawReconImg = spImg;
    break;
  }

  using ImageCalculatorFilterType2 =
      itk::MinimumMaximumImageCalculator<UShortImageType>;

  auto imageCalculatorFilter2 = ImageCalculatorFilterType2::New();
  // imageCalculatorFilter2->SetImage(m_spReconImg);
  imageCalculatorFilter2->SetImage(spImg);
  imageCalculatorFilter2->Compute();

  const auto minVal2 =
      static_cast<double>(imageCalculatorFilter2->GetMinimum());
  const auto maxVal2 =
      static_cast<double>(imageCalculatorFilter2->GetMaximum());

  std::cout << "Min and Max Values are	" << minVal2 << "	" << maxVal2
            << std::endl;

  // Update UI
  auto imgDim = spImg->GetBufferedRegion().GetSize();
  auto spacing = spImg->GetSpacing();

  std::cout << "Image Dimension:	" << imgDim[0] << "	" << imgDim[1]
            << "	" << imgDim[2] << std::endl;
  std::cout << "Image Spacing (mm):	" << spacing[0] << "	" << spacing[1]
            << "	" << spacing[2] << std::endl;

  m_spCrntReconImg = spImg.GetPointer();

  m_dspYKReconImage->CreateImage(imgDim[0], imgDim[1], 0);
}

void CbctRecon::GetWEPLDataFromSingleFile(const QString &filePath,
                                          std::vector<VEC3D> &vPOI,
                                          std::vector<WEPLData> &vOutputWEPL,
                                          const double fAngleStart,
                                          const double fAngleEnd) const {

  const int iCntPOI = vPOI.size();

  if (iCntPOI < 1) {
    return;
  }

  const auto fAngleGap = 1.0f;

  UShortImageType::Pointer spImg;

  auto strFilePath = filePath;
  if (!LoadShortImageToUshort(strFilePath, spImg)) {
    std::cout << "error! in LoadShortImageToUshort" << std::endl;
    return;
  }

  for (auto i = 0; i < iCntPOI; i++) {
    const auto curPOI = vPOI.at(i);
    // append mode
    GetAngularWEPL_SinglePoint(spImg, fAngleGap, fAngleStart, fAngleEnd, curPOI,
                               i, vOutputWEPL, true); // mandatory
  }
}

void CbctRecon::ScatterCorPerProjRef(const double scaMedian,
                                     const double scaGaussian,
                                     const int postScatMedianSize,
                                     const bool use_cuda, const bool use_opencl,
                                     const bool save_dicom,
                                     FDK_options &fdk_options) // load text file
{
  if (m_strListPerProjRefVol.empty()) {
    std::cout << "Error! Ref Vol list is not ready yet. Load it first"
              << std::endl;
    return;
  }

  // Find the mask file
  // QString strPath_mskSkinCT_final;
  // QString strPath_mskSkinCT_autoRegi_exp =
  // m_pDlgRegistration->m_strPathPlastimatch + "/msk_skin_CT_autoRegi_exp.mha";
  // QFileInfo maskInfoAuto(strPath_mskSkinCT_autoRegi_exp);

  // QString strPath_mskSkinCT_manualRegi_exp =
  // m_pDlgRegistration->m_strPathPlastimatch + "/msk_skin_CT_manRegi_exp.mha";
  // QFileInfo maskInfoManual(strPath_mskSkinCT_manualRegi_exp);

  // if (maskInfoAuto.exists()) //if the mask file is not prepared, give up the
  // skin removal
  //{
  //    strPath_mskSkinCT_final = strPath_mskSkinCT_autoRegi_exp;
  //}
  // else
  //{
  //    std::cout << "Mask file of auto-registration is not prepared. Use manual
  //    regi-mask instead" << std::endl;

  //    if (maskInfoManual.exists())
  //    {
  //        strPath_mskSkinCT_final = strPath_mskSkinCT_manualRegi_exp;
  //    }
  //    else
  //    {
  //        std::cout << "Mask file of manual registration is not prepared. Skip
  //        skin removal!" << std::endl; return;
  //    }
  //}

  ////std::cout << "Plastimatch Path " <<
  /// m_strPathPlastimatch.toLocal8Bit().constData() << std::endl;

  // if (m_pDlgRegistration->m_strPathPlastimatch.length() < 1)
  //{
  //    std::cout << "NO plastimatch Dir was defined. CorrCBCT will not be saved
  //    automatically" << std::endl; return;
  //}
  // Forward proj

  // Make a canvas

  if (m_spProjImgRaw3D == nullptr) {
    std::cout << "ERRORRR! m_spProjImgRaw3D" << std::endl;
    return;
  }

  m_spProjImgCT3D = UShortImageType::New(); // later
  const auto projCT_size =
      m_spProjImgRaw3D->GetLargestPossibleRegion().GetSize(); // 1024 1024 350
  const auto projCT_idxStart =
      m_spProjImgRaw3D->GetLargestPossibleRegion().GetIndex(); // 0 0 0
  const auto projCT_spacing = m_spProjImgRaw3D->GetSpacing();  // 0.4 0.4 1.0
  const auto projCT_origin =
      m_spProjImgRaw3D->GetOrigin(); //-204.6 -204.6 -174.5

  UShortImageType::RegionType projCT_region;
  projCT_region.SetSize(projCT_size);
  projCT_region.SetIndex(projCT_idxStart);

  m_spProjImgCT3D->SetRegions(projCT_region);
  m_spProjImgCT3D->SetSpacing(projCT_spacing);
  m_spProjImgCT3D->SetOrigin(projCT_origin);

  m_spProjImgCT3D->Allocate();
  m_spProjImgCT3D->FillBuffer(0);

  // YKTEMP
  const auto proj_size = m_spProjImgCT3D->GetBufferedRegion().GetSize();
  std::cout << "ProjImgCT Size = " << proj_size[0] << ", " << proj_size[1]
            << ", " << proj_size[2] << std::endl;
  std::cout << "ProjImgCT origin = " << m_spProjImgCT3D->GetOrigin()[0] << ", "
            << m_spProjImgCT3D->GetOrigin()[1] << ", "
            << m_spProjImgCT3D->GetOrigin()[2] << std::endl;
  std::cout << "ProjImgCT spacing = " << m_spProjImgCT3D->GetSpacing()[0]
            << ", " << m_spProjImgCT3D->GetSpacing()[1] << ", "
            << m_spProjImgCT3D->GetSpacing()[2] << std::endl;

  const int iCntRefVol = m_strListPerProjRefVol.count();

  if (iCntRefVol < 1) {
    std::cout << "Error! no volume data for loading" << std::endl;
    return;
  }

  const auto flexCnt =
      static_cast<int>(m_spCustomGeometry->GetGantryAngles().size());
  if (flexCnt != iCntRefVol) {
    std::cout << "Error! flex count doesn't match" << std::endl;
    return;
  }

  for (auto i = 0; i < iCntRefVol; i++) {
    // Load volume: Short image
    auto spOutputShort_raw = ShortImageType::New();
    // ShortImageType::Pointer spOutputShort_threshold = ShortImageType::New();
    auto spOutputUshort = UShortImageType::New();
    // UShortImageType::Pointer spOutputUshort_register =
    // UShortImageType::New();
    auto spUshortRotated = UShortImageType::New();
    auto spAttFloat = FloatImageType::New();

    auto strDirPath = m_strListPerProjRefVol.at(i);

    if (!LoadShortImageDirOrFile(strDirPath, spOutputShort_raw)) {
      std::cout << "Error! in " << i
                << " th image. File couldn't be found. Path= "
                << strDirPath.toLocal8Bit().constData() << std::endl;
      return;
    }

    ConvertShort2Ushort(spOutputShort_raw, spOutputUshort);

    RotateImgBeforeFwd(spOutputUshort,
                       spUshortRotated); // IEC to RTK w/ kVGantry
    ConvertUshort2AttFloat(spUshortRotated, spAttFloat);

    const auto curMVAngle = m_spCustomGeometry->GetGantryAngles().at(i);
    const auto curPanelOffsetX =
        m_spCustomGeometry->GetProjectionOffsetsX().at(i);
    const auto curPanelOffsetY =
        m_spCustomGeometry->GetProjectionOffsetsY().at(i);

#if USE_CUDA
    if (use_cuda) {
      SingleForwardProjection<CUDAFloatImageType>(
          spAttFloat, curMVAngle, curPanelOffsetX, curPanelOffsetY,
          m_spProjImgCT3D, i);
    } else
#endif
    {
      SingleForwardProjection<FloatImageType>(spAttFloat, curMVAngle,
                                              curPanelOffsetX, curPanelOffsetY,
                                              m_spProjImgCT3D, i);
    }
    std::cout << "Proj: " << i << "/" << iCntRefVol << std::endl;
  }

  /* typedef itk::ImageFileWriter<UShortImageType> WriterType;
   WriterType::Pointer writer = WriterType::New();
   writer->SetFileName("D:/TmpProjCT3D.mha");
   writer->SetUseCompression(true);
   writer->SetInput(m_spProjImgCT3D);
   writer->Update();
   */

  std::cout << "Generating scatter map is ongoing..." << std::endl;

  std::cout << "To account for the mAs values, the intensity scale factor of "
            << GetRawIntensityScaleFactor(m_strRef_mAs, m_strCur_mAs)
            << "will be multiplied during scatter correction to avoid negative "
               "scatter"
            << std::endl;

  GenScatterMap_PriorCT(m_spProjImgRaw3D, m_spProjImgCT3D, m_spProjImgScat3D,
                        scaMedian, scaGaussian, m_iFixedOffset_ScatterMap,
                        false);  // void GenScatterMap2D_PriorCT()
  m_spProjImgCT3D->Initialize(); // memory saving

  std::cout << "Scatter correction is in progress..." << std::endl;

  ScatterCorr_PrioriCT(m_spProjImgRaw3D, m_spProjImgScat3D, m_spProjImgCorr3D,
                       m_iFixedOffset_ScatterMap, postScatMedianSize, true);
  m_spProjImgScat3D->Initialize(); // memory saving

  std::cout << "AfterCorrectionMacro is ongoing..." << std::endl;
  AfterScatCorrectionMacro(use_cuda, use_opencl, save_dicom, fdk_options);
  std::cout << "FINISHED!Scatter correction: CBCT DICOM files are saved"
            << std::endl;

  ////1) Export current CBCT file
  // QString filePathCBCT = m_strPathPlastimatch + "/" + "CorrCBCT.mha";
  // //usually corrected one  QString filePathCBCT_noSkin = m_strPathPlastimatch
  // +
  // "/" + "CorrCBCT_final.mha"; //usually corrected one

  // typedef itk::ImageFileWriter<UShortImageType> writerType;
  // writerType::Pointer writer = writerType::New();
  // writer->SetFileName(filePathCBCT.toLocal8Bit().constData());
  // writer->SetUseCompression(true);
  // writer->SetInput(spCBCT);

  // std::cout << "Writing the CBCT file" << std::endl;
  // writer->Update();

  // QFileInfo CBCTInfo(filePathCBCT);
  // if (!CBCTInfo.exists())
  //{
  //    std::cout << "No CBCT file to read. Maybe prior writing failed" <<
  //    std::endl; return;
  //}

  ////ERROR HERE! delete the temporry folder.
  // std::cout << "Delete the temporary folder if it crashes" << std::endl;

  ////4) eliminate the air region (temporarily)
  ////Mask_parms parms_msk;
  ////DIMENSION SHOULD BE MATCHED!!!! BETWEEN raw CBCT and Mask files
  // Mask_operation mask_option = MASK_OPERATION_MASK;
  // QString input_fn = filePathCBCT.toLocal8Bit().constData();
  // QString mask_fn = strPath_mskSkinCT_final.toLocal8Bit().constData();
  // QString output_fn = filePathCBCT_noSkin.toLocal8Bit().constData();
  // float mask_value = 0.0; //unsigned short
  // plm_mask_main(mask_option, input_fn, mask_fn, output_fn, mask_value);
}

// double CbctRecon::CropSkinUsingRS(UShortImageType::Pointer& spImgUshort,
// QString& strPathRS, double cropMargin )
//{
//   //if (m_pParent->m_strPathRS.isEmpty())
//	//return;
//  /* End of [1]Segment air region*/
//
//  //plastimatch convert --input E:\PlastimatchData\DicomEg\OLD\RS.dcm
//  --output-ss-img E:\PlastimatchData\DicomEg\OLD\ssimg_all2.mha
//  --output-ss-list E:\PlastimatchData\DicomEg\OLD\sslist_all.txt
//  --referenced-ct E:\PlastimatchData\DicomEg\OLD\CT
//
//  //plm_clp_parse (&parms, &parse_fn, &usage_fn, argc, argv, 1)
//  //plastimatch segment --input E:\PlastimatchData\DicomEg\OLD\CT --output-img
//  E:\PlastimatchData\DicomEg\OLD\msk_bubbles_oldCT.mha --lower-threshold -600
//
//  //do_command_warp(argc, argv);
//
//
//
//
//}

bool SaveDoseGrayImage(
    const char *filePath, const int width, const int height,
    const double spacingX, const double spacingY, const double originLeft_mm,
    const double originTop_mm,
    unsigned short *pData) // export dose array to a specified file (16bit TIF)
{
  // Global variables
  const long m_iSubFileType = 0;
  const short m_iWidth = width;
  const short m_iHeight = height;
  const short m_iBitsPerSample = 16;
  const short m_iCompression = 1;
  const short m_iPhotometric = 0;
  const long m_iStripOffset = 1024;
  const short m_iSamplePerPixel = 1;
  const long m_iRowsPerStrip = height;
  const long m_iStripByteCnts = qRound(width * height * 2.0);

  const short m_iResolUnit = 2;
  const short m_iPgNum = 0; // or 1?
  const unsigned short m_iMinSampleVal = 0;
  const unsigned short m_iMaxSampleVal = 65535U; // old: 255
  const auto ten_mill = 10000000;
  RATIONAL m_rXResol{static_cast<long>(qRound(1 / spacingX * 25.4 *
                                              ten_mill)), // spacingX in dpi
                     ten_mill};
  RATIONAL m_rYResol{static_cast<long>(qRound(1 / spacingY * 25.4 * ten_mill)),
                     ten_mill}; // spacingY

  // double fLeftPosMM = -dataPt.x()*spacingX;
  // double fTopPosMM = dataPt.y()*spacingY;
  const auto fLeftPosMM = originLeft_mm;
  const auto fTopPosMM = -originTop_mm;

  RATIONAL m_rXPos{static_cast<long>(qRound(fLeftPosMM / 25.4 * ten_mill)),
                   ten_mill};
  RATIONAL m_rYPos{static_cast<long>(qRound(fTopPosMM / 25.4 * ten_mill)),
                   ten_mill};

  auto m_iNextOffset = 0;

  if (pData == nullptr) {
    return false;
  }

  // Set Center
  QPoint dataPt;
  dataPt.setX(qRound(m_iWidth / 2.0));
  dataPt.setY(qRound(m_iHeight / 2.0));

  FILE *fd = nullptr;

#ifdef WIN32
  if (fopen_s(&fd, filePath, "wb") == 0) {
    std::cerr << "Could not open file: " << filePath << " for writing!"
              << std::endl;
    return false;
  }
#else
  fd = fopen(filePath, "wb");
  if (fd == nullptr) {
    std::cerr << "Could not open file: " << filePath << " for writing!"
              << std::endl;
    return false;
  }
#endif

  long MarkerUpper;
  long MarkerLower;

  MarkerUpper = 0x002A4949;
  MarkerLower = 0x00000008;

  fwrite(&MarkerUpper, sizeof(long), 1, fd); // 4
  fwrite(&MarkerLower, sizeof(long), 1, fd); // 8

  // int IFDSize = GetValidIFDCnt();
  auto IFDSize = 18;

  fwrite(&IFDSize, sizeof(unsigned short), 1, fd); // 10

  // auto* IFDArr = new TIFIFD[IFDSize];
  std::vector<TIFIFD> IFDarr;
  IFDarr.reserve(IFDSize);

  int offsetX;
  auto offsetY = 0;

  /*int idx = 0;
  int TagID = 0;
  int dataType = 0;
  int DataCnt = 0;
  int dataVal = 0;
      */

  const unsigned short data_type = 3;
  const auto data_cnt = 1;

  if (m_iSubFileType >= 0) {
    const auto tififd_tmp = TIFIFD{254, data_type, data_cnt, m_iSubFileType};
    IFDarr.push_back(tififd_tmp);
  }

  if (m_iWidth >= 0) {
    const auto tififd_tmp = TIFIFD{256, data_type, data_cnt, m_iWidth};
    IFDarr.push_back(tififd_tmp); // dataVal이 초기값이면 insert 안함
  }
  if (m_iHeight >= 0) {
    const auto tififd_tmp = TIFIFD{257, data_type, data_cnt, m_iHeight};
    IFDarr.push_back(tififd_tmp); // dataVal이 초기값이면 insert 안함
  }
  if (m_iBitsPerSample >= 0) {
    const auto tififd_tmp = TIFIFD{258, data_type, data_cnt, m_iBitsPerSample};
    IFDarr.push_back(tififd_tmp); // dataVal이 초기값이면 insert 안함
  }
  if (m_iCompression >= 0) {
    const auto tififd_tmp = TIFIFD{259, data_type, data_cnt, m_iCompression};
    IFDarr.push_back(tififd_tmp); // dataVal이 초기값이면 insert 안함
  }
  if (m_iPhotometric >= 0) {
    const auto tififd_tmp = TIFIFD{262, data_type, data_cnt, m_iPhotometric};
    IFDarr.push_back(tififd_tmp); // 1로 강제 지정
    // dataVal = 0; //0으로 강제 지정
    // dataVal이 초기값이면 insert 안함
  }
  if (m_iStripOffset >= 0) {
    const auto tififd_tmp =
        TIFIFD{273, 4, data_cnt, static_cast<int>(m_iStripOffset)};
    IFDarr.push_back(tififd_tmp); // dataVal이 초기값이면 insert 안함
  }
  if (m_iSamplePerPixel >= 0) {
    const auto tififd_tmp = TIFIFD{277, data_type, data_cnt, m_iSamplePerPixel};
    IFDarr.push_back(tififd_tmp);
    // 1로강제지정
    // dataVal = 1;
    // dataVal이 초기값이면 insert 안함
  }
  if (m_iRowsPerStrip >= 0) {
    const auto tififd_tmp =
        TIFIFD{278, data_type, data_cnt, static_cast<int>(m_iRowsPerStrip)};
    IFDarr.push_back(tififd_tmp); // dataVal이 초기값이면 insert 안함
  }
  if (m_iStripByteCnts >= 0) {
    const auto tififd_tmp =
        TIFIFD{279, 4, data_cnt, static_cast<int>(m_iStripByteCnts)};
    IFDarr.push_back(tififd_tmp);
    /*if (m_iSamplePerPixel == 1)
    dataVal = m_iStripByteCnts;
    else if (m_iSamplePerPixel == 3)
    dataVal = (int)(m_iStripByteCnts/3.0);
    */
    // dataVal이 초기값이면 insert 안함
  }
  if (m_rXResol.a != 0) {
    offsetX = 8 + 2 + 12 * IFDSize + 4;

    const auto tififd_tmp = TIFIFD{282, 5, data_cnt, offsetX}; // maximum
    IFDarr.push_back(tififd_tmp); // dataVal이 초기값이면 insert 안함
  }
  if (m_rYResol.a != 0) {
    offsetY = 8 + 2 + 12 * IFDSize + 4 + 8;

    const auto tififd_tmp = TIFIFD{283, 5, data_cnt, offsetY};
    IFDarr.push_back(tififd_tmp); // dataVal이 초기값이면 insert 안함
  }

  // IFDSize 단위 데이터 몇개인지 나타냄
  // 20111226추가 //center를 표시
  if (m_rXPos.a != 0) {
    offsetX = 8 + 2 + 12 * IFDSize + 4 + 8 + 8;

    const auto tififd_tmp = TIFIFD{286, 5, data_cnt, offsetX}; // maximum
    IFDarr.push_back(tififd_tmp); // dataVal이 초기값이면 insert 안함
  }
  if (m_rYPos.a != 0) {
    offsetY = 8 + 2 + 12 * IFDSize + 4 + 8 + 8 + 8;

    const auto tififd_tmp = TIFIFD{287, 5, data_cnt, offsetY};
    IFDarr.push_back(tififd_tmp); // dataVal이 초기값이면 insert 안함
  }

  //////
  if (m_iMinSampleVal >= 0) {
    const auto tififd_tmp = TIFIFD{280, data_type, data_cnt, m_iMinSampleVal};
    IFDarr.push_back(tififd_tmp); // dataVal이 초기값이면 insert 안함
  }
  if (m_iMaxSampleVal >= 0) {
    const auto tififd_tmp = TIFIFD{281, data_type, data_cnt, m_iMaxSampleVal};
    IFDarr.push_back(tififd_tmp); // dataVal이 초기값이면 insert 안함
  }
  if (m_iResolUnit >= 0) {
    const auto tififd_tmp = TIFIFD{296, data_type, data_cnt, m_iResolUnit};
    IFDarr.push_back(tififd_tmp); // dataVal이 초기값이면 insert 안함
  }
  if (m_iPgNum >= 0) {
    const auto tififd_tmp = TIFIFD{297, data_type, 2, m_iPgNum};
    IFDarr.push_back(tififd_tmp); // dataVal이 초기값이면 insert 안함
  }
  /*
for (int i = 0; i < IFDSize; i++)
{
  fwrite(&IFDArr[i], sizeof(TIFIFD), 1, fd);
}
  */
  for (auto &&it : IFDarr) {
    fwrite(&it, sizeof(TIFIFD), 1, fd);
  }
  fwrite(&m_iNextOffset, 4, 1, fd);

  fwrite(&m_rXResol, 8, 1, fd);
  fwrite(&m_rYResol, 8, 1, fd);

  fwrite(&m_rXPos, 8, 1,
         fd); // Used to be 10 instead of 1, but that must've been a mistake
  fwrite(&m_rYPos, 8, 1, fd);

  const auto iDummySize = static_cast<size_t>(1024 - (offsetY + 8));

  // char tmpDummy [802]; // 1024 -222

  // auto *tmpDummy = new char[iDummySize];
  auto tmpDummy = std::valarray<char>(static_cast<char>(0), iDummySize);
  // memset(tmpDummy, 0, iDummySize);
  fwrite(&tmpDummy[0], sizeof(char), iDummySize, fd); //`까지 0으로 채움

  // delete[] tmpDummy;
  // delete[] IFDArr;

  const auto imgSize = m_iWidth * m_iHeight;
  // fwrite(m_pImage, imgSize, 1, fd);

  //쓰기용 버퍼 생성
  // unsigned short* writeBuf = new unsigned short[imgSize];

  // for (int i = 0 ; i<imgSize ; i++)
  //{
  //	//fread(&m_pImage[i], 2, 1, fd);
  //	if (pData[i] < 0)
  //		writeBuf[i] = 0;
  //	else if  (pData[i] > 65535)
  //		writeBuf[i] = 65535;
  //	else
  //		writeBuf[i] = pData[i];  //gray 이미지를 건드는 것이다!
  //}

  for (auto i = 0; i < imgSize; i++) {
    fwrite(&pData[i], 2, 1, fd);
  }
  fclose(fd);

  return true;
}
