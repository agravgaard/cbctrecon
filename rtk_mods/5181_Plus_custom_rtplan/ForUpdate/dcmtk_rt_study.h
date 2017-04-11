/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#ifndef _dcmtk_rt_study_h_
#define _dcmtk_rt_study_h_

#include "plmbase_config.h"
#include <list>

#include "itk_image.h"
#include "plm_image.h"
#include "plm_int.h"
#include "rt_study_metadata.h"
#include "rtss.h"
#include "rtplan.h"
#include "volume.h"

class Dcmtk_rt_study_private;
class Dcmtk_slice_data;

class PLMBASE_API Dcmtk_rt_study {
public:
    Dcmtk_rt_study_private *d_ptr;
public:
    Dcmtk_rt_study ();
    ~Dcmtk_rt_study ();
public:
    void load (const char *dicom_path);
    void save (const char *dicom_path);
public:
    const char* get_ct_series_uid () const;
    const char* get_dose_instance_uid () const;
    const char* get_dose_series_uid () const;
    const char* get_frame_of_reference_uid () const;
    const char* get_plan_instance_uid () const;
    const char* get_rtss_instance_uid () const;
    const char* get_rtss_series_uid () const;
    const char* get_study_date () const;
    const char* get_study_time () const;
    const char* get_study_uid () const;
    std::vector<Dcmtk_slice_data>* get_slice_data();

public:
    Plm_image::Pointer get_image ();
    Volume::Pointer get_image_volume_float ();
    void set_image (Plm_image::Pointer image);

    Rtss::Pointer& get_rtss ();
    void set_rtss (Rtss::Pointer rtss);

    Rtplan::Pointer& get_rtplan();
    void set_rtplan(Rtplan::Pointer rtplan);

    Plm_image::Pointer get_dose ();
    void set_dose (Plm_image::Pointer image);

    void set_rt_study_metadata (Rt_study_metadata::Pointer rt_study_metadata);
    void set_filenames_with_uid (bool filenames_with_uid);

public:
    void save_image (const char *dicom_path);
    void save_dose (const char *dicom_path);
    void save_rtss (const char *dicom_path);
    void save_rtplan(const char *dicom_path);//not yet implemented

};

#endif
