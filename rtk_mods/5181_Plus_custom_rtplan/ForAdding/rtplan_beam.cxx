/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#include "plmbase_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plm_math.h"
#include "rtplan_control_pt.h"
#include "rtplan_beam.h"
#include "string_util.h"
#include "logfile.h"

Rtplan_beam::Rtplan_beam()
{
    this->id = -1;    
    this->name = "";
    this->cplist = 0;
    this->num_cp = 0;
}

Rtplan_beam::~Rtplan_beam()
{
    this->clear ();
}

void
Rtplan_beam::clear()
{  
    for (size_t i = 0; i < this->num_cp; i++) {
        delete this->cplist[i];
    }
    free(this->cplist);

    this->name = "";    
    this->id = -1;
    this->cplist = 0;
    this->num_cp = 0;
}

Rtplan_control_pt*
Rtplan_beam::add_control_pt(int index)
{
    Rtplan_control_pt* new_control_pt;

    this->num_cp++;
    this->cplist = (Rtplan_control_pt**)realloc (this->cplist,
        this->num_cp * sizeof(Rtplan_control_pt*));

    new_control_pt
        = this->cplist[this->num_cp - 1]
        = new Rtplan_control_pt;

    new_control_pt->control_pt_no = index;
    return new_control_pt;
}

/* first beam isocenter position*/
float*
Rtplan_beam::get_isocenter_pos()
{
    float isopos[3];

    isopos[0] = 0.0;
    isopos[1] = 0.0;
    isopos[2] = 0.0;

    if (num_cp < 1)
        return isopos;

    /* Get First CP's isocenter position*/
    isopos[0] = cplist[0]->iso_pos[0];
    isopos[1] = cplist[0]->iso_pos[1];
    isopos[2] = cplist[0]->iso_pos[2];

    return isopos;
}

bool 
Rtplan_beam::check_isocenter_identical()
{
    bool bSame = true;
    if (this->num_cp < 1)
        return false;

    float firstIso[3];

    firstIso[0] = this->cplist[0]->iso_pos[0];
    firstIso[1] = this->cplist[0]->iso_pos[1];
    firstIso[2] = this->cplist[0]->iso_pos[2];

    float currIso[3];

    for (int i = 1; i < this->num_cp; i++){
        currIso[0] = this->cplist[i]->iso_pos[0];
        currIso[1] = this->cplist[i]->iso_pos[1];
        currIso[2] = this->cplist[i]->iso_pos[2];

        if (firstIso[0] != currIso[0] || firstIso[1] != currIso[1] || firstIso[2] != currIso[2]){
            bSame = false;
        }
    }
    /* list isocenter positions */
    if (!bSame){
        lprintf("Warning! Isocenter positions are not same across the control points!\n");

        for (int i = 1; i < this->num_cp; i++){
            lprintf("Control point idx: %d, isocenter: %3.2f / %3.2f / %3.2f. \n",
                cplist[i]->control_pt_no, cplist[i]->iso_pos[0], cplist[i]->iso_pos[1],
                cplist[i]->iso_pos[2]);
        }
    }
    return bSame;
}