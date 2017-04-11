/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#ifndef _plm_file_format_h_
#define _plm_file_format_h_

#include "plmbase_config.h"

class Pstring;

enum Plm_file_format {
    PLM_FILE_FMT_NO_FILE,
    PLM_FILE_FMT_UNKNOWN,
    PLM_FILE_FMT_IMG,
    PLM_FILE_FMT_VF,
    PLM_FILE_FMT_DIJ,
    PLM_FILE_FMT_POINTSET,
    PLM_FILE_FMT_CXT,
    PLM_FILE_FMT_DICOM_DIR,
    PLM_FILE_FMT_XIO_DIR,
    PLM_FILE_FMT_RTOG_DIR,
    PLM_FILE_FMT_PROJ_IMG,
    PLM_FILE_FMT_DICOM_RTSS,
    PLM_FILE_FMT_DICOM_DOSE,
    PLM_FILE_FMT_DICOM_RTPLAN,
    PLM_FILE_FMT_SS_IMG_VEC
};

PLMBASE_API Plm_file_format plm_file_format_deduce (const char* path);
PLMBASE_API Plm_file_format plm_file_format_deduce (const std::string& path);
PLMBASE_API char* plm_file_format_string (Plm_file_format file_type);
PLMBASE_API Plm_file_format plm_file_format_parse (const char* string);
PLMBASE_API Plm_file_format plm_file_format_from_extension (const char* filename);

#endif
