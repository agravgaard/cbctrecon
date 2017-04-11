/* -----------------------------------------------------------------------
See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
----------------------------------------------------------------------- */
#include "plmbase_config.h"
#include <stdlib.h>
#include <stdio.h>
#include "dcmtk_config.h"
#include "dcmtk/ofstd/ofstream.h"
#include "dcmtk/dcmdata/dctk.h"

#include "dcmtk_file.h"
#include "dcmtk_metadata.h"
#include "dcmtk_loader.h"
#include "dcmtk_loader_p.h"
#include "dcmtk_rt_study.h"
#include "dcmtk_rt_study_p.h"
#include "dcmtk_rtplan.h"
//#include "dcmtk_slice_data.h"
#include "file_util.h"
#include "logfile.h"
#include "metadata.h"
#include "plm_uid_prefix.h"
#include "plm_version.h"
#include "print_and_exit.h"
//#include "rtss.h"
#include "rtplan_control_pt.h"
#include "rtplan_beam.h"
//#include "rtss_roi.h"
#include "string_util.h"

PLMBASE_C_API bool
dcmtk_rtplan_probe(const char *rtplan_fn)
{
    DcmFileFormat dfile;

    /* Suppress warning messages */
    OFLog::configure(OFLogger::FATAL_LOG_LEVEL);

    OFCondition ofrc = dfile.loadFile(rtplan_fn, EXS_Unknown, EGL_noChange);

    /* Restore error messages -- n.b. dcmtk doesn't have a way to
    query current setting, so I just set to default */
    OFLog::configure(OFLogger::WARN_LOG_LEVEL);

    if (ofrc.bad()) {
        return false;
    }

    const char *c;
    DcmDataset *dset = dfile.getDataset();
    ofrc = dset->findAndGetString(DCM_Modality, c);
    if (ofrc.bad() || !c) {
        return false;
    }

    if (strncmp(c, "RTPLAN", strlen("RTPLAN"))) {
        return false;
    }
    else {
        return true;
    }
}

void
Dcmtk_loader::rtplan_load(void)
{
    Dcmtk_series *ds_rtplan = d_ptr->ds_rtplan;    

    d_ptr->rtplan = Rtplan::New();

    /* Modality -- better be RTSTRUCT */
    std::string modality = ds_rtplan->get_modality();
    if (modality == "RTPLAN") {
        lprintf("Trying to load rt plan.\n");
    }
    else {
        print_and_exit("Oops.\n");
    }

    /* FIX: load metadata such as patient name, etc. */

    /*const char *val2 = ds_rtplan->get_cstr(DCM_PatientName);
    const char *val3 = ds_rtplan->get_cstr(DCM_PatientID);*/


    /* Load Beam sequence */

    DcmSequenceOfItems *seq = 0;
    bool rc = ds_rtplan->get_sequence(DCM_BeamSequence, seq);
    int iNumOfBeam = seq->card();
    if (rc) {
        for (unsigned long i = 0; i < iNumOfBeam; i++) {
            Rtplan_beam *curr_beam;
            OFCondition orc;
            const char *strVal = 0;
            long int iVal = 0;

            int beam_id = 0;
            std::string strBeamName;

            DcmItem *item = seq->getItem(i);
            orc = item->findAndGetLongInt(DCM_BeamNumber, iVal);
            if (!orc.good()){
                continue;
            }
            beam_id = iVal;


            orc = item->findAndGetString(DCM_BeamName, strVal);
            if (!orc.good()){
                continue;
            }

            strBeamName = strVal;            
            strVal = 0;

            curr_beam = d_ptr->rtplan->add_beam(strBeamName, beam_id);

            DcmSequenceOfItems *cp_seq = 0;
            orc = item->findAndGetSequence(DCM_ControlPointSequence, cp_seq);

            int iNumOfCP = cp_seq->card();

            for (unsigned long j = 0; j <iNumOfCP; j++) {                
                DcmItem *c_item = cp_seq->getItem(j);

                int control_pt_idx = 0;

                c_item->findAndGetLongInt(DCM_ControlPointIndex, iVal);
                control_pt_idx = (int)iVal;
                //std::string strIsocenter;
                Rtplan_control_pt* curr_cp = curr_beam->add_control_pt(control_pt_idx);

                /* ContourGeometricType */
                orc = c_item->findAndGetString(DCM_IsocenterPosition,strVal);
                if (!orc.good()){
                    continue;
                }

                float iso_pos[3];
                int rc = parse_dicom_float3(iso_pos, strVal);
                if (!rc) {
                    curr_cp->iso_pos[0] = iso_pos[0];
                    curr_cp->iso_pos[1] = iso_pos[1];
                    curr_cp->iso_pos[2] = iso_pos[2];
                }
                strVal = 0;

                /*to be implemented*/
                //Get Beam Energy
                //Get Gantry Angle
                //Get Collimator openning
                //GetTable positions
                //Get MLC positions
                //Get CumulativeMetersetWeight
            }

            if (iNumOfCP > 0){                
                if (!curr_beam->check_isocenter_identical()){
                    /* action: isonceter of the control points are not same. */
                }
            }            
        }
    }    
}

void
Dcmtk_rt_study::save_rtplan(const char *dicom_dir)
{
    //to be implemented

}
