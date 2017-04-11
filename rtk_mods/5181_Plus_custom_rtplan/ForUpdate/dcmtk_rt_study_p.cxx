/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plmbase_config.h"
#include "dcmtk_config.h"
#include "dcmtk/ofstd/ofstream.h"
#include "dcmtk/dcmdata/dctk.h"

#include "dcmtk_rt_study_p.h"
#include "dcmtk_slice_data.h"
#include "dcmtk_uid.h"
#include "plm_uid_prefix.h"

Dcmtk_rt_study_private::Dcmtk_rt_study_private ()
{
    DcmDate::getCurrentDate (date_string);
    DcmTime::getCurrentTime (time_string);
    dcmtk_uid (study_uid, PLM_UID_PREFIX);
    dcmtk_uid (for_uid, PLM_UID_PREFIX);
    dcmtk_uid (ct_series_uid, PLM_UID_PREFIX);
    dcmtk_uid (plan_instance_uid, PLM_UID_PREFIX);
    dcmtk_uid (rtss_instance_uid, PLM_UID_PREFIX);
    dcmtk_uid (rtss_series_uid, PLM_UID_PREFIX);
    dcmtk_uid (dose_series_uid, PLM_UID_PREFIX);
    dcmtk_uid (dose_instance_uid, PLM_UID_PREFIX);
    slice_data = new std::vector<Dcmtk_slice_data>;

    rt_study_metadata = Rt_study_metadata::New ();
    filenames_with_uid = true;
}

Dcmtk_rt_study_private::~Dcmtk_rt_study_private ()
{
    delete slice_data;
}
