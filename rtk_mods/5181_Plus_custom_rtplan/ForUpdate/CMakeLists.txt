##-----------------------------------------------------------------------------
##  See COPYRIGHT.TXT and LICENSE.TXT for copyright and license information
##-----------------------------------------------------------------------------
project (src_plastimatch_base)

configure_file (
    ${CMAKE_CURRENT_SOURCE_DIR}/plmbase_config.h.in
    ${PLM_BUILD_ROOT}/plmbase_config.h
)

include_directories (BEFORE ${CMAKE_CURRENT_SOURCE_DIR})
include_directories (AFTER ${NKIDECOMPRESS_INCLUDE_DIR})

##-----------------------------------------------------------------------------
##  SOURCE FILES
##-----------------------------------------------------------------------------
set (PLMBASE_LIBRARY_SRC
  aperture.cxx aperture.h 
  astroid_dose.cxx
  bspline_interpolate.cxx
  bspline_warp.cxx
  bspline_xform.cxx
  cxt_extract.cxx
  cxt_io.cxx
  dicom_probe.cxx
  dicom_util.cxx
  dicom.dic
  direction_cosines.cxx 
  direction_matrices.cxx 
  exchkeys.cxx
  gaussian.cxx gaussian.h 
  hnd_io.cxx
  interpolate.cxx
  itkClampCastImageFilter.txx
  itk_dicom_load.cxx
  itk_dicom_save.cxx
  itk_directions.cxx
  itk_image.cxx
  itk_image_accumulate.cxx
  itk_image_cast.cxx
  itk_image_clone.cxx
#  itk_image_conv.cxx (broken)
  itk_image_create.cxx
  itk_image_load.txx
  itk_image_load_char.cxx
  itk_image_load_double.cxx
  itk_image_load_float.cxx
  itk_image_load_int32.cxx
  itk_image_load_short.cxx
  itk_image_load_uchar.cxx
  itk_image_load_uint32.cxx
  itk_image_load_ushort.cxx
  itk_image_load_vec.cxx
  itk_image_origin.cxx itk_image_origin.h 
  itk_image_region.cxx itk_image_region.h 
  itk_image_save.cxx
  itk_image_scale.cxx itk_image_scale.h 
  itk_image_stats.cxx
  itk_image_type.h
  itk_metadata.cxx
  itk_point.h
  itk_pointset.cxx
  itk_resample.cxx
  itk_volume_header.cxx
  itk_warp.cxx
  mc_dose.cxx
  metadata.cxx
  mha_io.cxx
  nki_io.cxx
  parameter_parser.cxx
  plm_file_format.cxx
  plm_image.cxx
  plm_image_convert.cxx
  plm_image_header.cxx
  plm_image_set.cxx
  plm_image_type.cxx
  plm_warp.cxx
  pointset.cxx
  pointset_warp.cxx
  proj_image.cxx
  proj_image_dir.cxx
  proj_matrix.cxx
  proj_volume.cxx proj_volume.h
  rasterize_slice.cxx
  rasterizer.cxx 
  raw_pointset.cxx
  ray_trace_exact.cxx
  ray_trace_probe.cxx
  ray_trace_uniform.cxx
  rpl_volume.cxx
  rtds_dcmtk.cxx
  rtds_gdcm.cxx
  rt_study.cxx rt_study.h 
  rt_study_metadata.cxx rt_study_metadata.h 
  rtog_io.cxx
  rtss.cxx rtss.h 
  rtss_contour.cxx rtss_contour.h 
  rtss_roi.cxx rtss_roi.h 
  rtplan.cxx rtplan.h 
  rtplan_beam.cxx rtplan_beam.h 
  rtplan_control_pt.cxx rtplan_control_pt.h   
  segmentation.cxx segmentation.h 
  slice_extract.cxx
  slice_list.cxx slice_list.h
  ss_img_extract.cxx
  ss_list_io.cxx
  thumbnail.cxx
  vf_convolve.cxx
  vf_jacobian.cxx
  vf_stats.cxx
  vf.cxx
  volume_conv.cxx
  volume_gaussian.cxx volume_gaussian.h 
  volume_grad.cxx volume_grad.h 
  volume_header.cxx
  volume_limit.cxx
  volume_resample.cxx
  volume_stats.cxx
  volume.cxx
  xform.cxx
  xform_convert.cxx
  xform_legacy.cxx
  xform_point.cxx
  xio_ct.cxx
  xio_ct_transform.cxx
  xio_demographic.cxx
  xio_dir.cxx
  xio_dose.cxx
  xio_patient.cxx
  xio_plan.cxx
  xio_structures.cxx
  xio_studyset.cxx
  xpm.cxx
  )

if (PLM_DCM_USE_GDCM1)
  set (PLMBASE_LIBRARY_SRC
    ${PLMBASE_LIBRARY_SRC}
    gdcm1_dose.cxx
    gdcm1_file.cxx
    gdcm1_rdd.cxx
    gdcm1_rtss.cxx
    gdcm1_series.cxx
    gdcm1_series_helper_2.cxx
    gdcm1_util.cxx
    )
endif ()

if (PLM_DCM_USE_GDCM2)
  set (PLMBASE_LIBRARY_SRC
    ${PLMBASE_LIBRARY_SRC}
    gdcm2_util.cxx
    )
endif ()

if (PLM_DCM_USE_DCMTK)
  set (PLMBASE_LIBRARY_SRC
    ${PLMBASE_LIBRARY_SRC}
    dcmtk_file.cxx
    dcmtk_image.cxx
    dcmtk_loader.cxx
    dcmtk_metadata.cxx
    dcmtk_module_general_series.cxx
    dcmtk_module_general_study.cxx
    dcmtk_module_patient.cxx
    dcmtk_rdd.cxx
    dcmtk_rtdose.cxx 
    dcmtk_rtss.cxx
	dcmtk_rtplan.cxx
    dcmtk_series.cxx
    dcmtk_sro.cxx
    dcmtk_rt_study.cxx dcmtk_rt_study.h
    dcmtk_rt_study_p.cxx dcmtk_rt_study_p.h
    dcmtk_uid.cxx
    dcmtk_util.cxx
    )
endif ()

set (PLMBASE_LIBRARY_HEADERS
  "${CMAKE_BINARY_DIR}/plmbase_config.h"
  )
foreach (ARG ${PLMBASE_LIBRARY_SRC})
  string (REGEX MATCH ".*\\.h$" TMP "${ARG}")
  if (TMP)
    list (APPEND PLMBASE_LIBRARY_HEADERS "${TMP}")
  endif ()
endforeach ()

##-----------------------------------------------------------------------------
##  LIBRARY DEPENDENCIES
##-----------------------------------------------------------------------------
set (PLMBASE_LIBRARY_DEPENDENCIES
  ${ITK_LIBRARIES}
  nkidecompress
  plmsys
  )
if (DCMTK_FOUND)
  set (PLMBASE_LIBRARY_DEPENDENCIES
    ${PLMBASE_LIBRARY_DEPENDENCIES}
    ${DCMTK_LIBRARIES})
endif ()

##-----------------------------------------------------------------------------
##  SPECIAL BUILD RULES: OpenMP & SSE2
##-----------------------------------------------------------------------------
if (OPENMP_FOUND)
  set (PLMBASE_LIBRARY_LDFLAGS "${OPENMP_LDFLAGS}")
  set_source_files_properties (bspline_warp.cxx
    PROPERTIES COMPILE_FLAGS ${OPENMP_FLAGS})
  set_source_files_properties (volume_conv.cxx
    PROPERTIES COMPILE_FLAGS ${OPENMP_FLAGS})
endif ()

if (SSE2_FOUND)
  plm_set_sse2_flags (bspline_warp.cxx)
endif ()

##-----------------------------------------------------------------------------
##  BUILD TARGETS
##-----------------------------------------------------------------------------
plm_add_library (
  plmbase
  "${PLMBASE_LIBRARY_SRC}" 
  "${PLMBASE_LIBRARY_DEPENDENCIES}"
  "${PLMBASE_LIBRARY_LDFLAGS}"
  "${PLMBASE_LIBRARY_HEADERS}")
