/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plmbase_config.h"
#include "dcmtk_config.h"
#include "dcmtk/ofstd/ofstream.h"
#include "dcmtk/dcmdata/dctk.h"

#include "dcmtk_loader.h"
#include "dcmtk_rt_study.h"
#include "dcmtk_rt_study_p.h"
#include "dcmtk_rtss.h"
#include "dcmtk_series.h"
#include "dcmtk_slice_data.h"
#include "logfile.h"
#include "plm_image.h"
#include "plm_version.h"
#include "rt_study_metadata.h"
#include "rtss.h"
#include "smart_pointer.h"
#include "volume.h"

Dcmtk_rt_study::Dcmtk_rt_study ()
{
    this->d_ptr = new Dcmtk_rt_study_private;
}

Dcmtk_rt_study::~Dcmtk_rt_study ()
{
    delete this->d_ptr;
}

const char*
Dcmtk_rt_study::get_ct_series_uid () const
{
    return d_ptr->ct_series_uid;
}

const char*
Dcmtk_rt_study::get_dose_instance_uid () const
{
    return d_ptr->dose_instance_uid;
}

const char*
Dcmtk_rt_study::get_dose_series_uid () const
{
    return d_ptr->dose_series_uid;
}

const char*
Dcmtk_rt_study::get_frame_of_reference_uid () const
{
    return d_ptr->for_uid;
}

const char*
Dcmtk_rt_study::get_plan_instance_uid () const
{
    return d_ptr->plan_instance_uid;
}

const char*
Dcmtk_rt_study::get_rtss_instance_uid () const
{
    return d_ptr->rtss_instance_uid;
}

const char*
Dcmtk_rt_study::get_rtss_series_uid () const
{
    return d_ptr->rtss_series_uid;
}

const char*
Dcmtk_rt_study::get_study_date () const
{
    return d_ptr->date_string.c_str();
}

const char*
Dcmtk_rt_study::get_study_time () const
{
    return d_ptr->time_string.c_str();
}

const char*
Dcmtk_rt_study::get_study_uid () const
{
    return d_ptr->study_uid;
}

std::vector<Dcmtk_slice_data>* 
Dcmtk_rt_study::get_slice_data ()
{
    return d_ptr->slice_data;
}

Plm_image::Pointer
Dcmtk_rt_study::get_image ()
{
    return d_ptr->img;
}

Volume::Pointer
Dcmtk_rt_study::get_image_volume_float ()
{
    return d_ptr->img->get_volume_float ();
}

void 
Dcmtk_rt_study::set_image (Plm_image::Pointer image)
{
    d_ptr->img = image;
}

Rtss::Pointer&
Dcmtk_rt_study::get_rtss ()
{
    return d_ptr->cxt;
}

void 
Dcmtk_rt_study::set_rtss (Rtss::Pointer rtss)
{
    d_ptr->cxt = rtss;
}

Rtplan::Pointer&
Dcmtk_rt_study::get_rtplan()
{
    return d_ptr->plan;
}

void
Dcmtk_rt_study::set_rtplan(Rtplan::Pointer rtplan)
{
    d_ptr->plan = rtplan;
}

Plm_image::Pointer 
Dcmtk_rt_study::get_dose ()
{
    return d_ptr->dose;
}

void 
Dcmtk_rt_study::set_dose (Plm_image::Pointer image)
{
    d_ptr->dose = image;
}

void 
Dcmtk_rt_study::set_rt_study_metadata (
    Rt_study_metadata::Pointer rt_study_metadata)
{
    d_ptr->rt_study_metadata = rt_study_metadata;
}

void 
Dcmtk_rt_study::set_filenames_with_uid (bool filenames_with_uid)
{
    d_ptr->filenames_with_uid = filenames_with_uid;
}

void 
Dcmtk_rt_study::load (const char *dicom_path)
{
    Dcmtk_loader dss (dicom_path);
    dss.set_rt_study_metadata (d_ptr->rt_study_metadata);    
    dss.parse_directory ();    

    d_ptr->img = dss.get_image ();
    d_ptr->cxt = dss.get_rtss ();
    d_ptr->dose = dss.get_dose ();
    d_ptr->plan = dss.get_rtplan();
    //d_ptr->plan = dss.get_plan();//Not yet implemented
}

void 
Dcmtk_rt_study::save (const char *dicom_dir)
{
    /* GCS FIX: If we're writing an image, we always want new metadata;
       but this should probably be handled by somewhere else in the 
       code. */
    if (d_ptr->img) {
        d_ptr->rt_study_metadata->generate_new_uids ();
    }
    if (d_ptr->img) {
        this->save_image (dicom_dir);
    }
    if (d_ptr->cxt) {
        this->save_rtss (dicom_dir);
    }
    if (d_ptr->dose) {
        this->save_dose (dicom_dir);
    }
    if (d_ptr->plan) {
        this->save_rtplan(dicom_dir);
    }
}
