/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#ifndef _rtplan_control_pt_h_
#define _rtplan_control_pt_h_

#include "plmbase_config.h"
#include <string>

class PLMBASE_API Rtplan_control_pt {
public:
    int control_pt_no;
    
    float fCumulativeMeterSet;
    float iso_pos[3];//DICOM coordinate, x,y,z

    float fDoseRate;
    float fGantryAngle;
    bool bGantryRotDirection;
    
    float fTablePosVert;
    float fTablePosLong;
    float fTablePosLat;

    float fCollimatorAngle;
    bool bCollimatorRotDir;

    float fPatientSupportAngle;
    bool bPatientSupportRotDir;

    //MLC information here?
    
public:
    Rtplan_control_pt();
    ~Rtplan_control_pt();
    float* get_isocenter(){ return iso_pos; }

public:    
};

#endif
