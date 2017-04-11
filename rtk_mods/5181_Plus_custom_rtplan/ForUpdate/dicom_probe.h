/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#ifndef _dcm_probe_h_
#define _dcm_probe_h_

#include "plmbase_config.h"

PLMBASE_C_API 
bool
dicom_probe_dose (const char *fn);

PLMBASE_C_API 
bool
dicom_probe_rtss (const char *fn);

PLMBASE_C_API
bool
dicom_probe_rtplan(const char *fn);

#endif
