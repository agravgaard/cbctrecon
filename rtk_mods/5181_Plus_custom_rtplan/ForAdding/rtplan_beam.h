/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#ifndef _rtplan_beam_h_
#define _rtplan_beam_h_

#include "plmbase_config.h"
#include <string>

class Rtplan_control_pt;

class PLMBASE_API Rtplan_beam {
public:
    std::string name;    
    int id;                    /* Used for import/export (must be >= 1) */
    size_t num_cp;
    Rtplan_control_pt** cplist; //control point list

    //int bit;                   /* Used for ss-img (-1 for no bit) */
    //size_t num_contours;
    //Rtss_contour** pslist;

    //isocenter position
    //Beam weighting
    //metersetb

    //

    //control points
    


public:
    Rtplan_beam();
    ~Rtplan_beam();

    void clear ();
    Rtplan_control_pt* add_control_pt(int index);
    bool check_isocenter_identical();
    float* get_isocenter_pos(); //float[3], dicom coordinate    
};


#endif
