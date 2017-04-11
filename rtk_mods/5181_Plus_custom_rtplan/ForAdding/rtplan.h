/* -----------------------------------------------------------------------
   See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
   ----------------------------------------------------------------------- */
#ifndef _rtplan_h_
#define _rtplan_h_

#include "plmbase_config.h"
#include <list>
#include <vector>

#include "direction_cosines.h"
#include "plm_int.h"
#include "rt_study_metadata.h"
#include "smart_pointer.h"

class Plm_image;
class Plm_image_header;
class Rtplan_beam;

class PLMBASE_API Rtplan {
public:
    SMART_POINTER_SUPPORT(Rtplan);
public:
    ///* Output geometry */
    //int have_geometry;
    //plm_long m_dim[3];
    //float m_spacing[3];
    //float m_offset[3];
    ///* Rasterization geometry */
    //plm_long rast_dim[3];
    //float rast_spacing[3];
    //float rast_offset[3];
    //Direction_cosines rast_dc;

    /* Patient information [to be implemented]*/
    /* std::string patient_ID;
     std::string patient_Name;
     std::string plan_name;
     std::string plan_description;
     std::string plan_date;
     std::string TPS_manufacturer;*/

    /* Structures */
    size_t num_beams;
    Rtplan_beam **beamlist;
public:
    Rtplan();
    ~Rtplan();
    void init (void);
    void clear (void);
    Rtplan_beam* add_beam(
        const std::string& beam_name,         
	int beam_id);
    void delete_beam (int index);
    Rtplan_beam* find_beam_by_id(int structure_id);
    std::string get_beam_name (size_t index);
    void set_beam_name (size_t index, const std::string& name);
    void debug (void);
    /*void adjust_structure_names (void);
    void prune_empty (void);
    static Rtss* clone_empty (Rtss* cxt_out, 
        Rtss* cxt_in);
    void find_rasterization_geometry (float offset[3], 
	float spacing[3], plm_long dims[3], Direction_cosines& dc);
    void find_rasterization_geometry (Plm_image_header *pih);
    std::string find_unused_structure_name (void);
    void fix_polyline_slice_numbers (void);
    void apply_slice_index (const Rt_study_metadata::Pointer& rsm);
    void apply_slice_list (const Slice_list *slice_list);
    void free_all_polylines (void);
    void keyholize (void);
    void set_rasterization_geometry (void);
    void set_geometry (const Plm_image_header *pih);
    void set_geometry (const Plm_image::Pointer& pli);*/
};

#endif
