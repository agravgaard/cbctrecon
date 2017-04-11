/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#ifndef _dcmtk_rt_study_p_h_
#define _dcmtk_rt_study_p_h_

#include "plmbase_config.h"
#include "plm_image.h"
//#include "plm_image_set.h"
#include "rt_study_metadata.h"
#include "rtss.h"
#include "rtplan.h"

class Dcmtk_series;
class Dcmtk_slice_data;

class Dcmtk_rt_study_private {
public:
    OFString date_string;
    OFString time_string;
    char ct_series_uid[100];
    char dose_instance_uid[100];
    char dose_series_uid[100];
    char for_uid[100];
    char plan_instance_uid[100];
    char rtss_instance_uid[100];
    char rtss_series_uid[100];
    char study_uid[100];
    std::vector<Dcmtk_slice_data>* slice_data;

    Dcmtk_series *ds_rtdose; //not used in anywhere?
    Dcmtk_series *ds_rtss; //not used in anywhere?
    Dcmtk_series *ds_rtplan; //not used in anywhere?

    Rtss::Pointer cxt;
    Rtplan::Pointer plan;
    Metadata *cxt_metadata;
    //Plm_image_set::Pointer img;
    Plm_image::Pointer img;
    Plm_image::Pointer dose;

    Rt_study_metadata::Pointer rt_study_metadata;

    bool filenames_with_uid;

public:
    Dcmtk_rt_study_private ();
    ~Dcmtk_rt_study_private ();
};

#endif
