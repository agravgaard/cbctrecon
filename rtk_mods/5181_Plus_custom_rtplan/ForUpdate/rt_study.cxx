/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plm_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if PLM_DCM_USE_GDCM1
#include "gdcm1_dose.h"
#include "gdcm1_series.h"
#endif
#include "astroid_dose.h"
#include "file_util.h"
#include "itk_resample.h"
#include "logfile.h"
#include "mc_dose.h"
#include "path_util.h"
#include "plm_image.h"
#include "print_and_exit.h"
#include "rt_study.h"
#include "rt_study_p.h"
#include "rtss.h"
#include "segmentation.h"
#include "volume.h"
#include "xio_ct.h"
#include "xio_ct_transform.h"
#include "xio_demographic.h"
#include "xio_dir.h"
#include "xio_dose.h"
#include "xio_patient.h"
#include "xio_structures.h"

Rt_study::Rt_study ()
{
    d_ptr = new Rt_study_private;
}

Rt_study::~Rt_study ()
{
    delete d_ptr;
}


void 
Rt_study::load (const char* input_path, 
    Plm_file_format file_type)
{
    if (file_type == PLM_FILE_FMT_UNKNOWN) {
        file_type = plm_file_format_deduce (input_path);
    }

    switch (file_type) {
    case PLM_FILE_FMT_NO_FILE:
        print_and_exit ("Could not open input file %s for read\n",
            input_path);
        break;
    case PLM_FILE_FMT_UNKNOWN:
    case PLM_FILE_FMT_IMG:
        this->load_image (input_path);
        break;
    case PLM_FILE_FMT_DICOM_DIR:
        this->load_dicom_dir ((const char*) input_path);
        break;
    case PLM_FILE_FMT_XIO_DIR:
        this->load_xio ((const char*) input_path);
        break;
    case PLM_FILE_FMT_DIJ:
        print_and_exit (
            "Warping dij files requires ctatts_in, dif_in files\n");
        break;
    case PLM_FILE_FMT_DICOM_RTSS:
        this->load_dicom_rtss ((const char*) input_path);
        break;
    case PLM_FILE_FMT_DICOM_DOSE:
        this->load_dicom_dose ((const char*) input_path);
        break;
    case PLM_FILE_FMT_DICOM_RTPLAN:
        this->load_dicom_rtplan((const char*)input_path);
        break;
    case PLM_FILE_FMT_CXT:
        this->load_cxt (input_path);
        break;
    case PLM_FILE_FMT_SS_IMG_VEC:
    default:
        print_and_exit (
            "Sorry, don't know how to load/convert/warp/segment "
            "input type %s (%s)\n",
            plm_file_format_string (file_type), input_path);
        break;
    }
}

void 
Rt_study::load (const std::string& path,
    Plm_file_format file_type)
{
    this->load (path.c_str(), file_type);
}

void
Rt_study::load_image (const char *fn)
{
    d_ptr->m_img = Plm_image::New (fn);
}

void
Rt_study::load_image (const std::string& fn)
{
    this->load_image (fn.c_str());
}

void
Rt_study::load_dicom_dir (const char *dicom_dir)
{
    const char *dicom_dir_tmp;  /* In case dicom_dir is a file, not dir */

    if (is_directory (dicom_dir)) {
        dicom_dir_tmp = dicom_dir;
    } else {
        dicom_dir_tmp = file_util_dirname (dicom_dir);
    }

    this->load_dicom (dicom_dir_tmp);

    if (dicom_dir_tmp != dicom_dir) {
        free ((void*) dicom_dir_tmp);
    }
}

void
Rt_study::load_dicom (const char *dicom_dir)
{
    if (!dicom_dir) {
        return;
    }

#if PLM_DCM_USE_DCMTK
    this->load_dcmtk (dicom_dir);
#else
    this->load_gdcm (dicom_dir);
#endif
}

void
Rt_study::load_dicom_rtss (const char *dicom_path)
{
    d_ptr->m_rtss.reset ();
#if PLM_DCM_USE_DCMTK
    this->load_dcmtk (dicom_path);
#elif PLM_DCM_USE_GDCM1
    d_ptr->m_rtss = Segmentation::New ();
    d_ptr->m_rtss->load_gdcm_rtss (dicom_path, d_ptr->m_drs.get());
#else
    /* Do nothing */
#endif
}

void
Rt_study::load_dicom_dose (const char *dicom_path)
{
#if PLM_DCM_USE_DCMTK
    this->load_dcmtk (dicom_path);
#elif PLM_DCM_USE_GDCM1
    d_ptr->m_dose.reset (gdcm1_dose_load (0, dicom_path));
#else
    /* Do nothing */
#endif
}


void
Rt_study::load_dicom_rtplan(const char *dicom_path)
{
    //d_ptr->m_rtplan.reset();//currently no need of priviate structure but metadata.
#if PLM_DCM_USE_DCMTK
    this->load_dcmtk(dicom_path);
#elif PLM_DCM_USE_GDCM1
    //not yet implemented
#else
    /* Do nothing */
#endif
}

void
Rt_study::load_xio (const char *xio_dir)
{
    Xio_dir xd (xio_dir);
    Xio_patient *xpd;
    std::string xio_studyset_dir;
    std::string xio_plan_dir;

    if (xd.num_patients() <= 0) {
        print_and_exit ("Error, xio num_patient_dir = %d\n", 
            xd.num_patients());
    }
    xpd = xd.patient_dir[0];
    if (xd.num_patients() > 1) {
        printf ("Warning: multiple patients found in xio directory.\n"
            "Defaulting to first directory: %s\n", 
            xpd->m_path.c_str());
    }

    if (xpd->plan_dirs.empty()) {
        /* No plans exist, load only studyset */
        if (xpd->studyset_dirs.empty()) {
            print_and_exit ("Error, xio patient has no studyset.");
        }

        printf ("Warning: no plans found, only loading studyset.");

        xio_studyset_dir = xpd->studyset_dirs.front();
        if (xpd->studyset_dirs.size() > 1) {
            printf (
                "Warning: multiple studyset found in xio patient directory.\n"
                "Defaulting to first directory: %s\n", 
                xio_studyset_dir.c_str());
        }
    } else {
        /* Plans exist, so load the first plan */
        const std::string& xio_plan_dir = xpd->plan_dirs.front();
        if (xpd->plan_dirs.size() > 1) {
            printf ("Warning: multiple plans found in xio patient directory.\n"
                "Defaulting to first directory: %s\n", xio_plan_dir.c_str());
        }

        /* Load the summed XiO dose file */
        d_ptr->m_dose = Plm_image::New ();
        printf ("calling xio_dose_load\n");
        d_ptr->m_xio_dose_filename = xio_plan_dir + "/dose.1";
        xio_dose_load (d_ptr->m_dose.get(), 
            d_ptr->m_drs->get_dose_metadata (),
            d_ptr->m_xio_dose_filename.c_str());

        /* Find studyset associated with plan */
        xio_studyset_dir = xio_plan_dir_get_studyset_dir (xio_plan_dir);
    }

    printf("path is :: %s\n", xio_studyset_dir.c_str());

    /* Load the XiO studyset slice list */
    Xio_studyset xst (xio_studyset_dir);

    /* Load the XiO studyset CT images */
    d_ptr->m_img = Plm_image::New();
    xio_ct_load (d_ptr->m_img.get(), &xst);

    /* Load the XiO studyset structure set */
    d_ptr->m_rtss = Segmentation::New ();
    d_ptr->m_rtss->load_xio (xst);

    /* Apply XiO CT geometry to structures */
    if (d_ptr->m_rtss->have_structure_set()) {
        Rtss *rtss_ss = d_ptr->m_rtss->get_structure_set_raw ();
        rtss_ss->set_geometry (d_ptr->m_img);
    }

    /* Load demographics */
    if (xpd->m_demographic_fn != "") {
        Xio_demographic demographic (xpd->m_demographic_fn.c_str());
        if (demographic.m_patient_name != "") {
            d_ptr->m_drs->set_study_metadata (0x0010, 0x0010, 
                demographic.m_patient_name);
        }
        if (demographic.m_patient_id != "") {
            d_ptr->m_drs->set_study_metadata (0x0010, 0x0020, 
                demographic.m_patient_id);
        }
    }

    /* If referenced DICOM CT is provided,  the coordinates will be
       transformed from XiO to DICOM LPS  with the same origin as the
       original CT.

       Otherwise, the XiO CT will be saved as DICOM and the structures
       will be associated to those slices. The coordinates will be
       transformed to DICOM LPS based on the patient position metadata
       and the origin will remain the same. */

    if (d_ptr->m_img) {
        if (d_ptr->m_drs->slice_list_complete()) {
            /* Determine transformation based on original DICOM */
            d_ptr->m_xio_transform->set_from_rdd (d_ptr->m_img.get(), 
                d_ptr->m_drs.get());
        }
    }

    if (d_ptr->m_img) {
        xio_ct_apply_transform (d_ptr->m_img.get(), d_ptr->m_xio_transform);
    }
    if (d_ptr->m_rtss->have_structure_set()) {
        xio_structures_apply_transform (d_ptr->m_rtss->get_structure_set_raw(),
            d_ptr->m_xio_transform);
    }
    if (d_ptr->m_dose) {
        xio_dose_apply_transform (d_ptr->m_dose.get(), d_ptr->m_xio_transform);
    }
}

void
Rt_study::load_ss_img (const char *ss_img, const char *ss_list)
{
    d_ptr->m_rtss = Segmentation::New ();
    d_ptr->m_rtss->load (ss_img, ss_list);
}

void
Rt_study::load_dose_img (const char *dose_img)
{
    if (d_ptr->m_dose) {
        d_ptr->m_dose.reset();
    }
    if (dose_img) {
        d_ptr->m_dose = plm_image_load_native (dose_img);
    }
}

void
Rt_study::load_rdd (const char *image_directory)
{
    d_ptr->m_drs = Rt_study_metadata::load (image_directory);

    /* GCS FIX: I think the below is not needed any more, but there 
       might be some edge cases, such as converting image with referenced 
       ct, which should copy patient name but not slice uids */
#if defined (commentout)
    Rt_study_metadata *rsm = d_ptr->m_drs.get ();
    Metadata *meta = rsm->get_study_metadata ();
    if (rsm->slice_list_complete()) {
        /* Default to patient position in referenced DICOM */
        d_ptr->m_meta->set_metadata(0x0018, 0x5100,
            si->m_demographics.get_metadata(0x0018, 0x5100));
        d_ptr->m_xio_transform->set (d_ptr->m_meta);

        /* Default to patient name/ID/sex in referenced DICOM */
        d_ptr->m_meta->set_metadata(0x0010, 0x0010,
            si->m_demographics.get_metadata(0x0010, 0x0010));
        d_ptr->m_meta->set_metadata(0x0010, 0x0020,
            si->m_demographics.get_metadata(0x0010, 0x0020));
        d_ptr->m_meta->set_metadata(0x0010, 0x0040,
            si->m_demographics.get_metadata(0x0010, 0x0040));
    }
#endif
}

void
Rt_study::load_dose_xio (const char *dose_xio)
{
    if (d_ptr->m_dose) {
        d_ptr->m_dose.reset();
    }
    if (dose_xio) {
        d_ptr->m_xio_dose_filename = dose_xio;
        d_ptr->m_dose = Plm_image::New ();
        Metadata::Pointer& dose_meta = d_ptr->m_drs->get_dose_metadata ();
        xio_dose_load (d_ptr->m_dose.get(), dose_meta, dose_xio);
        xio_dose_apply_transform (d_ptr->m_dose.get(), d_ptr->m_xio_transform);
    }
}

void
Rt_study::load_dose_astroid (const char *dose_astroid)
{
    if (d_ptr->m_dose) {
        d_ptr->m_dose.reset();
    }
    if (dose_astroid) {
        d_ptr->m_dose = Plm_image::New ();
        Metadata::Pointer& dose_meta = d_ptr->m_drs->get_dose_metadata ();
        astroid_dose_load (d_ptr->m_dose.get(), dose_meta, dose_astroid);
        astroid_dose_apply_transform (d_ptr->m_dose.get(), 
            d_ptr->m_xio_transform);
    }
}

void
Rt_study::load_dose_mc (const char *dose_mc)
{
    if (d_ptr->m_dose) {
        d_ptr->m_dose.reset();
    }
    if (dose_mc) {
        d_ptr->m_dose = Plm_image::New ();
        mc_dose_load (d_ptr->m_dose.get(), dose_mc);
        mc_dose_apply_transform (d_ptr->m_dose.get(), d_ptr->m_xio_transform);
    }
}

void 
Rt_study::load_cxt (const char *input_fn)
{
    d_ptr->m_rtss = Segmentation::New ();
    d_ptr->m_rtss->load_cxt (input_fn, d_ptr->m_drs.get());
}

void 
Rt_study::load_prefix (const char *input_fn)
{
    d_ptr->m_rtss = Segmentation::New ();
    d_ptr->m_rtss->load_prefix (input_fn);
}

void
Rt_study::save_dicom (const char *dicom_dir, bool filenames_with_uid)
{
    if (!dicom_dir) {
        return;
    }

    if (d_ptr->m_img) {
        d_ptr->m_drs->set_image_header (d_ptr->m_img);
    }
    if (d_ptr->m_rtss) {
        d_ptr->m_rtss->cxt_extract ();
    }

#if PLM_DCM_USE_DCMTK
    this->save_dcmtk (dicom_dir, filenames_with_uid);
#else
    this->save_gdcm (dicom_dir);
#endif
}

void
Rt_study::save_dicom (const std::string& dicom_dir, bool filenames_with_uid)
{
    this->save_dicom (dicom_dir.c_str(), filenames_with_uid);
}

void
Rt_study::save_dicom_dose (const char *dicom_dir)
{
    if (!dicom_dir) {
        return;
    }

#if PLM_DCM_USE_DCMTK
    this->save_dcmtk_dose (dicom_dir);
#else
    /* Not yet supported -- this function is only used by topas, 
       which uses dcmtk. */
#endif
}

void
Rt_study::save_dose (const std::string& fname)
{
    if (fname != "") {
        d_ptr->m_dose->save_image (fname);
    }
}

void
Rt_study::save_dose (const char* fname)
{
    if (d_ptr->m_dose) {
        d_ptr->m_dose->save_image (fname);
    }
}

void
Rt_study::save_dose (const char* fname, Plm_image_type image_type)
{
    if (d_ptr->m_dose) {
        d_ptr->m_dose->convert_and_save (fname, image_type);
    }
}

void
Rt_study::save_prefix (
    const std::string& output_prefix, 
    const std::string& extension)
{
    d_ptr->m_rtss->save_prefix (output_prefix, extension);
}

const Rt_study_metadata::Pointer&
Rt_study::get_rt_study_metadata () const
{
    return d_ptr->m_drs;
}

Rt_study_metadata::Pointer&
Rt_study::get_rt_study_metadata ()
{
    return d_ptr->m_drs;
}

void 
Rt_study::set_study_metadata (std::vector<std::string>& metadata)
{
    Metadata::Pointer& study_metadata = d_ptr->m_drs->get_study_metadata ();

    std::vector<std::string>::iterator it = metadata.begin();
    while (it < metadata.end()) {
        const std::string& str = (*it);
        size_t eq_pos = str.find_first_of ('=');
        if (eq_pos != std::string::npos) {
            std::string key = str.substr (0, eq_pos);
            std::string val = str.substr (eq_pos+1);
#if defined (commentout)
            /* Set older-style metadata, used by gdcm */
            d_ptr->m_meta->set_metadata (key, val);
#endif
            /* Set newer-style metadata, used by dcmtk */
            study_metadata->set_metadata (key, val);
        }
        ++it;
    }

    d_ptr->m_xio_transform->set (d_ptr->m_drs->get_image_metadata());
}

bool
Rt_study::have_image ()
{
    return (bool) d_ptr->m_img 
        && d_ptr->m_img->m_type != PLM_IMG_TYPE_UNDEFINED;
}

Plm_image::Pointer
Rt_study::get_image ()
{
    return d_ptr->m_img;
}

void 
Rt_study::set_image (ShortImageType::Pointer& itk_image)
{
    d_ptr->m_img = Plm_image::New (itk_image);
}

void 
Rt_study::set_image (FloatImageType::Pointer& itk_image)
{
    d_ptr->m_img = Plm_image::New (itk_image);
}

void 
Rt_study::set_image (Plm_image* pli)
{
    d_ptr->m_img.reset (pli);
}

void 
Rt_study::set_image (const Plm_image::Pointer& pli)
{
    d_ptr->m_img = pli;
}

bool
Rt_study::have_dose ()
{
    return (bool) d_ptr->m_dose;
}

void 
Rt_study::set_dose (Plm_image *pli)
{
    d_ptr->m_dose.reset (pli);
}

void 
Rt_study::set_dose (FloatImageType::Pointer itk_dose)
{
    d_ptr->m_dose.reset (new Plm_image (itk_dose));
}

void 
Rt_study::set_dose (Volume *vol)
{
    if (!vol) return;
    d_ptr->m_dose = Plm_image::New();

    /* GCS FIX: Make a copy */
    d_ptr->m_dose->set_volume (vol->clone_raw());
}

void 
Rt_study::set_dose (const Plm_image::Pointer& pli)
{
    d_ptr->m_dose = pli;
}

Plm_image::Pointer
Rt_study::get_dose ()
{
    return d_ptr->m_dose;
}

bool
Rt_study::have_rtss ()
{
    return (bool) d_ptr->m_rtss;
}

Segmentation::Pointer
Rt_study::get_rtss ()
{
    return d_ptr->m_rtss;
}

void 
Rt_study::set_rtss (Segmentation::Pointer rtss)
{
    d_ptr->m_rtss = rtss;
}

void 
Rt_study::add_structure (
    const UCharImageType::Pointer& itk_image,
    const char *structure_name,
    const char *structure_color)
{
    if (!have_rtss()) {
        d_ptr->m_rtss = Segmentation::New ();
    }
    d_ptr->m_rtss->add_structure (itk_image, structure_name, structure_color);
}

Xio_ct_transform*
Rt_study::get_xio_ct_transform ()
{
    return d_ptr->m_xio_transform;
}

const std::string&
Rt_study::get_xio_dose_filename (void) const
{
    return d_ptr->m_xio_dose_filename;
}

Metadata::Pointer&
Rt_study::get_metadata (void)
{
    return d_ptr->m_drs->get_study_metadata();
}

Volume::Pointer
Rt_study::get_image_volume_short ()
{
    if (!d_ptr->m_img) {
        return Volume::Pointer();
    }
    return d_ptr->m_img->get_volume_short ();
}

Volume::Pointer
Rt_study::get_image_volume_float (void)
{
    if (!d_ptr->m_img) {
        return Volume::Pointer();
    }
    return d_ptr->m_img->get_volume_float ();
}

bool
Rt_study::has_dose ()
{
    return (d_ptr->m_dose != 0);
}

Volume::Pointer
Rt_study::get_dose_volume_float ()
{
    if (!d_ptr->m_dose) {
        return Volume::Pointer();
    }
    return d_ptr->m_dose->get_volume_float ();
}

/* Resample image and ss_img */
void
Rt_study::resample (float spacing[3])
{
    d_ptr->m_img->set_itk (resample_image (
            d_ptr->m_img->itk_float(), spacing));
    d_ptr->m_rtss->resample (spacing);
}
