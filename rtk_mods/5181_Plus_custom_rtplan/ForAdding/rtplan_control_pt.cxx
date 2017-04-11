/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plmbase_config.h"
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>

#include "logfile.h"
#include "plm_math.h"
#include "rtplan_control_pt.h"

Rtplan_control_pt::Rtplan_control_pt()
{
    this->control_pt_no = -1;    
    
    this->iso_pos[0] = 0.0;
    this->iso_pos[1] = 0.0;
    this->iso_pos[2] = 0.0;
    this->fCumulativeMeterSet = 0.0;


    this->fDoseRate = 0.0;
    this->fGantryAngle = 0.0;

    this->bGantryRotDirection = false;

    this->fTablePosVert = 0.0;
    this->fTablePosLong = 0.0;
    this->fTablePosLat = 0.0;    

    this->fCollimatorAngle = 0.0;
    this->bCollimatorRotDir = false;       

    this->fPatientSupportAngle = 0.0;
    this->bPatientSupportRotDir = false;    
}

Rtplan_control_pt::~Rtplan_control_pt()
{

}