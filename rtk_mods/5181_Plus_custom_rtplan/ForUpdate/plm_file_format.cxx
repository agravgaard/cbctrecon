/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plmbase_config.h"
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <itksys/SystemTools.hxx>
#include <itkImageIOBase.h>

#include "dicom_probe.h"
#include "file_util.h"
#include "gdcm1_dose.h"
#include "itk_image.h"
#include "path_util.h"
#include "plm_file_format.h"
#include "string_util.h"
#include "xio_dir.h"

static int
is_xio_directory (const char* path)
{
    Xio_dir xd (path);
    if (xd.num_patients () > 0) {
	printf ("Found an XiO directory!!!!\n");
	return 1;
    } else {
	return 0;
    }
}

Plm_file_format
plm_file_format_deduce (const char* path)
{
    std::string ext;

    if (!path || !path[0]) {
	return PLM_FILE_FMT_NO_FILE;
    }
    
    if (itksys::SystemTools::FileIsDirectory (path)) {

	if (is_xio_directory (path)) {
	    return PLM_FILE_FMT_XIO_DIR;
	}

	/* GCS TODO:  Distinguish rtog directories */
	return PLM_FILE_FMT_DICOM_DIR;
    }

    if (!file_exists (path)) {
	return PLM_FILE_FMT_NO_FILE;
    }
    
    ext = itksys::SystemTools::GetFilenameLastExtension (std::string (path));

    if (!itksys::SystemTools::Strucmp (ext.c_str(), ".fcsv")) {
	return PLM_FILE_FMT_POINTSET;
    }

    if (!itksys::SystemTools::Strucmp (ext.c_str(), ".txt")) {
	/* Probe for pointset */
	int rc;
	const int MAX_LINE = 2048;
	char line[MAX_LINE];
	float f[4];
	FILE* fp = fopen (path, "rb");
	if (!fp) return PLM_FILE_FMT_NO_FILE;

	fgets (line, MAX_LINE, fp);
	fclose (fp);

	rc = sscanf (line, "%g %g %g %g", &f[0], &f[1], &f[2], &f[3]);
	if (rc == 3) {
	    return PLM_FILE_FMT_POINTSET;
	}

	/* Not sure, assume image */
	return PLM_FILE_FMT_IMG;
    }

    if (!itksys::SystemTools::Strucmp (ext.c_str(), ".cxt")) {
	return PLM_FILE_FMT_CXT;
    }

    if (!itksys::SystemTools::Strucmp (ext.c_str(), ".dij")) {
	return PLM_FILE_FMT_DIJ;
    }

    if (!itksys::SystemTools::Strucmp (ext.c_str(), ".pfm")) {
	return PLM_FILE_FMT_PROJ_IMG;
    }
    if (!itksys::SystemTools::Strucmp (ext.c_str(), ".hnd")) {
	return PLM_FILE_FMT_PROJ_IMG;
    }
    if (!itksys::SystemTools::Strucmp (ext.c_str(), ".scan")) {
	return PLM_FILE_FMT_IMG;
    }

    //Check DICOM first
    /* Maybe dicom rtss? */
    if (dicom_probe_rtss(path)) {
        return PLM_FILE_FMT_DICOM_RTSS;
    }

    /* Maybe dicom dose */
    if (dicom_probe_dose(path)) {
        return PLM_FILE_FMT_DICOM_DOSE;
    }

    /* Maybe dicom plan */
    if (dicom_probe_rtplan(path)) {
        return PLM_FILE_FMT_DICOM_RTPLAN;
    }


    itk::ImageIOBase::IOPixelType pixel_type;
    itk::ImageIOBase::IOComponentType component_type;
    int num_dimensions, num_components;
    itk_image_get_props (std::string (path), &num_dimensions, &pixel_type, 
	&component_type, &num_components);//this tries to read the file with itkReader
    if (pixel_type == itk::ImageIOBase::VECTOR) {
	/* Maybe vector field? */
	if (component_type == itk::ImageIOBase::FLOAT
            || component_type == itk::ImageIOBase::DOUBLE) {
	    return PLM_FILE_FMT_VF;
	}
	/* Maybe ss_image? */
	if (num_components >= 2 
	    && component_type == itk::ImageIOBase::UCHAR)
	{
	    return PLM_FILE_FMT_SS_IMG_VEC;
	}
	/* else fall through */
    }   

    return PLM_FILE_FMT_IMG;
}

Plm_file_format
plm_file_format_deduce (const std::string& path)
{
    return plm_file_format_deduce (path.c_str());
}

char*
plm_file_format_string (Plm_file_format file_type)
{
    switch (file_type) {
    case PLM_FILE_FMT_NO_FILE:
	return "No file";
    case PLM_FILE_FMT_UNKNOWN:
	return "Unknown";
    case PLM_FILE_FMT_IMG:
	return "Image";
    case PLM_FILE_FMT_VF:
	return "Vector field";
    case PLM_FILE_FMT_DIJ:
	return "Dij matrix";
    case PLM_FILE_FMT_POINTSET:
	return "Pointset";
    case PLM_FILE_FMT_CXT:
	return "Cxt file";
    case PLM_FILE_FMT_DICOM_DIR:
	return "DICOM directory";
    case PLM_FILE_FMT_XIO_DIR:
	return "XiO directory";
    case PLM_FILE_FMT_RTOG_DIR:
	return "RTOG directory";
    case PLM_FILE_FMT_PROJ_IMG:
	return "Projection image";
    case PLM_FILE_FMT_DICOM_RTSS:
	return "DICOM-RT SS";
    case PLM_FILE_FMT_DICOM_DOSE:
	return "DICOM-RT dose";
    case PLM_FILE_FMT_SS_IMG_VEC:
	return "Structure set image";
    default:
	return "Unknown/default";
    }
}

Plm_file_format 
plm_file_format_parse (const char* string)
{
    if (!strcmp (string, "mha")) {
	return PLM_FILE_FMT_IMG;
    }
    else if (!strcmp (string, "vf")) {
	return PLM_FILE_FMT_VF;
    }
    else if (!strcmp (string, "dij")) {
	return PLM_FILE_FMT_DIJ;
    }
    else if (!strcmp (string, "pointset")) {
	return PLM_FILE_FMT_POINTSET;
    }
    else if (!strcmp (string, "cxt")) {
	return PLM_FILE_FMT_CXT;
    }
    else if (!strcmp (string, "dicom") || !strcmp (string, "dicom-dir")) {
	return PLM_FILE_FMT_DICOM_DIR;
    }
    else if (!strcmp (string, "xio")) {
	return PLM_FILE_FMT_XIO_DIR;
    }
    else if (!strcmp (string, "rtog")) {
	return PLM_FILE_FMT_RTOG_DIR;
    }
    else if (!strcmp (string, "proj")) {
	return PLM_FILE_FMT_PROJ_IMG;
    }
    else if (!strcmp (string, "rtss") || !strcmp (string, "dicom-rtss")) {
	return PLM_FILE_FMT_DICOM_RTSS;
    }
    else if (!strcmp (string, "ssimg")) {
	return PLM_FILE_FMT_SS_IMG_VEC;
    }
    else {
	return PLM_FILE_FMT_UNKNOWN;
    }
}


Plm_file_format 
plm_file_format_from_extension (const char* filename)
{
    if (extension_is (filename, ".dcm")) {
	return PLM_FILE_FMT_DICOM_DIR;
    }
    else if (extension_is (filename, ".cxt")) {
	return PLM_FILE_FMT_CXT;
    }
    else {
	return PLM_FILE_FMT_IMG;
    }
}
