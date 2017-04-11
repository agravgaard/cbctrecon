/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#ifndef _dcmtk_loader_p_h_
#define _dcmtk_loader_p_h_

#include "plmbase_config.h"
#include <map>
#include <string>
#include "dcmtk_series.h"
#include "plm_image.h"
#include "rt_study_metadata.h"
#include "rtss.h"
#include "rtplan.h"

class Dcmtk_rt_study;

/* Map from SeriesInstanceUID to Dcmtk_series */
typedef std::map<std::string, Dcmtk_series*> Dcmtk_series_map;
typedef std::pair<std::string, Dcmtk_series*> Dcmtk_series_map_pair;

class Dcmtk_loader_private {
public:
    Rt_study_metadata::Pointer m_drs;

    Plm_image::Pointer img;
    Plm_image::Pointer dose;
    Rtss::Pointer cxt;
    Rtplan::Pointer rtplan;

    //Rtplan::Pointer plan; //to be implemented

public:
    Dcmtk_series_map m_smap;

    Dcmtk_series *ds_image;
    Dcmtk_series *ds_rtss;
    Dcmtk_series *ds_rtdose;
    Dcmtk_series *ds_rtplan;

public:
    Dcmtk_loader_private () {
        ds_image = 0;
        ds_rtss = 0;
        ds_rtdose = 0;
        ds_rtplan = 0;
    }
    ~Dcmtk_loader_private () {
        /* Delete Dicom_series objects in map */
        Dcmtk_series_map::iterator it;
        for (it = m_smap.begin(); it != m_smap.end(); ++it) {
            delete (*it).second;
        }
    }
};

#endif
