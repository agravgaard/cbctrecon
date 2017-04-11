#include "DlgExternalCommand.h"
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QStringList>
#include <QMessageBox>
#include <QProcess>
#include "DlgRegistration.h"

using namespace std;

DlgExternalCommand::DlgExternalCommand() : QDialog()
{
    /* Sets up the GUI */
    ui.setupUi (this);
}

DlgExternalCommand::DlgExternalCommand(QWidget *parent) : QDialog(parent)
{
    /* Sets up the GUI */
    ui.setupUi (this);
    m_pParent = (CbctRecon*)(parent);	

	int len = BuildRTKCommandFilter();
}

DlgExternalCommand::~DlgExternalCommand()
{
 
}

void DlgExternalCommand::SLT_SetRTKPath()
{
	QString dirPath = QFileDialog::getExistingDirectory(this, tr("Open RTK bin Directory"),
		"", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);


	if (dirPath.length() > 1)
	{
		SetRTKBinPath(dirPath);
	}
}

void DlgExternalCommand::SetRTKBinPath(QString& strDirPath)
{
	//If DIr is exist
	QDir dirInfo = QDir(strDirPath);

	if (!dirInfo.exists())
	{
		cout << "ERROR! " << strDirPath.toLocal8Bit().constData() << " doesn't exist." << endl;
		return;
	}

	ui.comboBoxRTKOption->clear();

	m_strDirRTKBin = strDirPath;
	ui.plainTextRTKPath->setPlainText(m_strDirRTKBin);

	//Search for available commands
	if (m_strDirRTKBin.isEmpty())
		return;
	
	int cnt = m_listRTKCommandFilter.length();

	QString tmpStrPath;

	for (int i = 0; i < cnt; i++)
	{
		QString strCommandFilter = m_listRTKCommandFilter.at(i);
		tmpStrPath = m_strDirRTKBin;
		tmpStrPath = tmpStrPath.append("/").append(strCommandFilter).append(".exe");

		cout << m_strDirRTKBin.toLocal8Bit().constData() << endl;
		cout << tmpStrPath.toLocal8Bit().constData() << endl;

		QFileInfo fInfo = QFileInfo(tmpStrPath);
		if (fInfo.exists()) //add combo
		{
			ui.comboBoxRTKOption->addItem(strCommandFilter);
		}
	}
}

void DlgExternalCommand::SLT_GenRTKCommand()
{
	QString crntPath = ui.plainTextRTKPath->toPlainText();
	QString crntCommand = ui.comboBoxRTKOption->currentText();
	
	QString strFinalCommandText;

	if (crntCommand.length() > 0 && crntPath.length() > 0)
	{
		strFinalCommandText = crntPath.append("/").append(crntCommand);
	}
	else
		return;

	//QString strGeometry;
	QString strOutput; //this should be member and unique //this is float format
	//QString strHardware;
	//QString strTruncation;
	//QString strHann;
	//QString strHannY;

	//QString strProjPath;
	//QString strRegExp;	
	//QString strOutOrigin; //default: centered
	//QString strOutDimension;
	//QString strOutSpacing;
	//QString strOutDirection; //no clue about what it is	
	
	//geometry
	QString str_mainGeometry = m_pParent->ui.lineEdit_GeomPath->text();

	if (str_mainGeometry.length() < 1)
		cout << "Command will not be valid. set geometry file path in the main UI first." << endl;

	QString str_mainHardware;
	if (m_pParent->ui.radioButton_UseCPU->isChecked())
		str_mainHardware = "cpu";
	else if (m_pParent->ui.radioButton_UseCUDA->isChecked())
		str_mainHardware = "cuda";
	else if (m_pParent->ui.radioButton_UseOpenCL->isChecked())
		str_mainHardware = "opencl";

	//QString str_mainTruncation;
	double f_mainTrunc = m_pParent->ui.lineEdit_Ramp_TruncationCorrection->text().toDouble();
	double f_mainHann = m_pParent->ui.lineEdit_Ramp_HannCut->text().toDouble();
	double f_mainHannY = m_pParent->ui.lineEdit_Ramp_HannCutY->text().toDouble();

	QString str_mainProjPath = m_pParent->ui.lineEdit_HisDirPath->text();

	QString str_mainProjRegExp = ".*.his";

	QString str_mainDimension = QString("%1,%2,%3")
		.arg(m_pParent->ui.lineEdit_outImgDim_AP->text())
		.arg(m_pParent->ui.lineEdit_outImgDim_SI->text())
		.arg(m_pParent->ui.lineEdit_outImgDim_LR->text());

	QString str_mainSpacing = QString("%1,%2,%3")
		.arg(m_pParent->ui.lineEdit_outImgSp_AP->text())
		.arg(m_pParent->ui.lineEdit_outImgSp_SI->text())
		.arg(m_pParent->ui.lineEdit_outImgSp_LR->text());



	QTime curTime = QTime::currentTime();
	QString strTimeStamp = curTime.toString("hhmmss");
	QDir tmpPlmDir = QDir(m_pParent->m_pDlgRegistration->m_strPathPlastimatch);
	
	if (!tmpPlmDir.exists())
	{
		cout << "Error! No tmp plm path is available." << tmpPlmDir.absolutePath().toLocal8Bit().constData() << endl;		
		return;
	}

	strOutput = tmpPlmDir.absolutePath() + "/" + "ExternalRtk_" + crntCommand + "_" + strTimeStamp + ".mha";

	

	if (crntCommand == "rtkfdk")
	{
		//For FDK, this is sure!	
		strFinalCommandText = strFinalCommandText +
			" --geometry " + str_mainGeometry +
			" --path " + str_mainProjPath +
			" --verbos" +
			" --regexp " + str_mainProjRegExp +
			" --output " + strOutput +
			" --spacing " + str_mainSpacing +
			" --dimension " + str_mainDimension +

			" --hardware " + str_mainHardware +
			" --pad " + QString("%1").arg(f_mainTrunc) +
			" --hann " + QString("%1").arg(f_mainHann) +
			" --hannY " + QString("%1").arg(f_mainHannY);
	}
	//ui
	/*lineEditIteration
		lineEditSARTlamda
		lineEditSARTpositivity
		lineEditSARTsubsetproj*/

	
	else if (crntCommand == "rtksart")
	{		
		QString strIteration = ui.lineEditIteration->text().trimmed(); //niterations default 5
		QString strLamda = ui.lineEditSARTlamda->text().trimmed(); //Convergence factor : default 0.3
		QString strPositivity = ui.lineEditSARTpositivity->text().trimmed();   //Enforces positivity during the reconstruction (default=off)",
		QString strNprojpersubset = ui.lineEditSARTsubsetproj->text().trimmed(); //Number of projections processed between each update of the reconstructed volume (1 for SART, several for OSSART, all for SIRT)  (default=`1')",
		QString strFwdMethod = "Joseph";
		if (str_mainHardware == "cuda")
			strFwdMethod = "CudaRayCast";
			//			--fp = ENUM Forward projection method(possible values = \"Joseph\", \"RayCastInterpolator\", "CudaRayCast\" default=`Joseph')",

		QString strBackMethod = "VoxelBasedBackProjection";
		if (str_mainHardware == "cuda")
			strBackMethod = "CudaVoxelBased";		

		strFinalCommandText = strFinalCommandText +
			" --geometry " + str_mainGeometry +
			" --path " + str_mainProjPath +
			" --verbos" +
			" --regexp " + str_mainProjRegExp +			
			" --spacing " + str_mainSpacing +
			" --dimension " + str_mainDimension +

			" --niterations " + strIteration +
			" --lambda " + strLamda +
			" --positivity " + strPositivity +
			" --nprojpersubset " + strNprojpersubset +
			" --fp " + strFwdMethod +
			" --bp " + strBackMethod +
			" --time " + "on" +
			" --output " + strOutput;
	}
	else if (crntCommand == "rtkadmmtotalvariation")
	{
		//lineEditIteration
		//lineEditTValpha
//			lineEditTVbeta
	//		lineEditTVCGiter* /

		QString strIteration = ui.lineEditIteration->text().trimmed(); //niterations default 5
		QString strTValpha = ui.lineEditTValpha->text().trimmed(); //Convergence factor : default 0.3
		QString strTVbeta = ui.lineEditTVbeta->text().trimmed();   //Enforces positivity during the reconstruction (default=off)",
		QString strTVCGiter = ui.lineEditTVCGiter->text().trimmed();   //Enforces positivity during the reconstruction (default=off)",		
		QString strFwdMethod = "Joseph";
		if (str_mainHardware == "cuda")
			strFwdMethod = "CudaRayCast";
		//			--fp = ENUM Forward projection method(possible values = \"Joseph\", \"RayCastInterpolator\", "CudaRayCast\" default=`Joseph')",

		QString strBackMethod = "VoxelBasedBackProjection";
		if (str_mainHardware == "cuda")
			strBackMethod = "CudaVoxelBased";

		strFinalCommandText = strFinalCommandText +
			" --geometry " + str_mainGeometry +
			" --path " + str_mainProjPath +
			" --verbos" +
			" --regexp " + str_mainProjRegExp +			
			" --spacing " + str_mainSpacing +
			" --dimension " + str_mainDimension +
			" --niterations " + strIteration +
			" --alpha " + strTValpha +
			" --beta " + strTVbeta +
			" --CGiter " + strTVCGiter +
			" --fp " + strFwdMethod +
			" --bp " + strBackMethod +
			" --time " + "on" +
			" --output " + strOutput;
	}
	else if (crntCommand == "rtkadmmwavelets")
	{
		QString strIteration = ui.lineEditIteration->text().trimmed(); //niterations default 5
		QString strTValpha = ui.lineEditTValpha->text().trimmed(); //Convergence factor : default 0.3
		QString strTVbeta = ui.lineEditTVbeta->text().trimmed();   //Enforces positivity during the reconstruction (default=off)",
		QString strTVCGiter = ui.lineEditTVCGiter->text().trimmed();   //Enforces positivity during the reconstruction (default=off)",		

		QString strWVorder = ui.lineEditWVorder->text().trimmed();
		QString strWVlevel = ui.lineEditWVlevel->text().trimmed();
		//"      --order=INT         The order of the Daubechies wavelets  (default=`3')",
		//"      --levels=INT        The number of decomposition levels in the wavelets \n                            transform  (default=`5')",	


		QString strFwdMethod = "Joseph";
		if (str_mainHardware == "cuda")
			strFwdMethod = "CudaRayCast";
		//			--fp = ENUM Forward projection method(possible values = \"Joseph\", \"RayCastInterpolator\", "CudaRayCast\" default=`Joseph')",

		QString strBackMethod = "VoxelBasedBackProjection";
		if (str_mainHardware == "cuda")
			strBackMethod = "CudaVoxelBased";

		strFinalCommandText = strFinalCommandText +
			" --geometry " + str_mainGeometry +
			" --path " + str_mainProjPath +
			" --verbos" +
			" --regexp " + str_mainProjRegExp +		
			" --spacing " + str_mainSpacing +
			" --dimension " + str_mainDimension +
			" --niterations " + strIteration +
			" --alpha " + strTValpha +
			" --beta " + strTVbeta +
			" --CGiter " + strTVCGiter +
			" --fp " + strFwdMethod +
			" --bp " + strBackMethod +
			" --order " + strWVorder +
			" --levels " + strWVlevel +
			" --time " + "on" +
			" --output " + strOutput;
	}


	m_strRecentOutputPath = strOutput;
	ui.plainTextRTKCommandLine->setPlainText(strFinalCommandText);
}

void DlgExternalCommand::SLT_RunRTKCommand()
{
	QString strFinalExternalCommand = ui.plainTextRTKCommandLine->toPlainText();
	if (QProcess::execute(strFinalExternalCommand) < 0)
		qDebug() << "Failed to run";

	cout << "External RTK reconstruction is done" << endl;
	cout << "File is being loaded" << endl;

	m_pParent->LoadExternalFloatImage(m_strRecentOutputPath, true); //true: conversion (float, direction
	
	if (m_pParent->ui.checkBox_PostMedianOn->isChecked())
		m_pParent->MedianFilterByGUI();//applied to raw image

	m_pParent->FileExportByGUI();//applied to raw image
}

int DlgExternalCommand::BuildRTKCommandFilter() //called when it is created
{
	//Search for some preset option file, later.
	//Temporarily it is hard coded.	
	m_listRTKCommandFilter.clear();

	m_listRTKCommandFilter.push_back("rtksart");
	m_listRTKCommandFilter.push_back("rtkfdk");
	m_listRTKCommandFilter.push_back("rtkadmmtotalvariation");
	m_listRTKCommandFilter.push_back("rtkadmmwavelets");
	//m_listRTKCommandFilter.push_back("rtktotalvariationdenoising");
	//m_listRTKCommandFilter.push_back("rtkwaveletsdenoising");
	

	return (m_listRTKCommandFilter.length());
}

void DlgExternalCommand::SLT_SetRTKPathManual()//apply button
{
	QString tmpPlainText = ui.plainTextRTKPath->toPlainText();
	SetRTKBinPath(tmpPlainText);

}


