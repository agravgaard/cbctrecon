/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plmbase_config.h"

#include "dicom_probe.h"

#if PLM_DCM_USE_DCMTK
#include "dcmtk_rtdose.h"
#include "dcmtk_rtss.h"
#include "dcmtk_rtplan.h"
#elif PLM_DCM_USE_GDCM1
#include "gdcm1_dose.h"
#include "gdcm1_rtss.h"
#else
/* Nothing */
#endif
#include "make_string.h"

/* Return true if the file is a dicom rtstruct */
bool
dicom_probe_rtss (const char *rtss_fn)
{
#if PLM_DCM_USE_DCMTK
    return dcmtk_rtss_probe (rtss_fn);
#elif PLM_DCM_USE_GDCM1
    return gdcm_rtss_probe (rtss_fn);
#else
    return false;
#endif
}

/* Return true if the file is a dicom rt dose */
bool
dicom_probe_dose (const char *fn)
{
#if PLM_DCM_USE_DCMTK
    return dcmtk_dose_probe (fn);
#elif PLM_DCM_USE_GDCM1
    return gdcm1_dose_probe (fn);
#else
    return false;
#endif
}


/* Return true if the file is a dicom rtplan */
bool
dicom_probe_rtplan(const char *rtplan_fn)
{
#if PLM_DCM_USE_DCMTK
    return dcmtk_rtplan_probe(rtplan_fn);
#elif PLM_DCM_USE_GDCM1
    //return gdcm_rtss_probe(rtplan_fn);//not yet implemented
    return false;
#else
    return false;
#endif
}
