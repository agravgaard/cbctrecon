# ### The main application ###

set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(APPLE)
  set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_BINDIR})
endif(APPLE)

# Packaging
option(CbctRecon_PACKAGE_32BIT
       "Set this when building 32-bit packages on a 64-bit machine" OFF)
option(CbctRecon_PACKAGE_NSIS "Set to ON when packaging binaries with NSIS" OFF)
option(CbctRecon_PACKAGE_WIX "Set to ON when packaging binaries with WIX" ON)

add_definitions(-DCMAKE_EXPORT_COMPILE_COMMANDS=ON)

# ############# Qt start wrapping ##############
set(CbctReconExe_SOURCES
    cbctrecon_mainwidget.cxx
    DlgRegistration.cxx
    DlgExternalCommand.cxx
    DlgHistogram.cxx
    qt-utils/qyklabel.cpp
    qt-utils/qcustomplot.cpp
    CMakeSetup.rc)

set(CbctRecon_HEADERS
    cbctrecon_mainwidget.h
    DlgRegistration.h
    qt-utils/qyklabel.h
    qt-utils/qcustomplot.h
    qt-utils/qtwrap.h
    DlgExternalCommand.h
    DlgHistogram.h)
set(CbctRecon_FORMS
    ui/cbctrecon.ui
    ui/DlgRegistration.ui
    ui/DlgHistogram.ui # Kept for example, no longer used
    ui/DlgExternalCommand.ui)

qt5_wrap_cpp(CbctRecon_HEADERS_MOC ${CbctRecon_HEADERS})
qt5_wrap_ui(CbctRecon_FORMS_HEADERS ${CbctRecon_FORMS})
qt5_add_resources(CbctRecon_RCC ui/style.qrc)

set(CBCTRECON_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/ui
                           ${CMAKE_CURRENT_SOURCE_DIR}/qt-utils)

# Executable
add_executable(
  CbctRecon
  main.cpp
  ${CbctReconExe_SOURCES}
  ${CbctRecon_HEADERS}
  ${CbctRecon_HEADERS_MOC}
  ${CbctRecon_FORMS_HEADERS}
  ${CbctRecon_RCC})

# Sets instruction set optimization flags SSE/AVX/AVX2/AVX512:
set_target_cpu_flags(CbctRecon)

print_target_compiler_options(CbctRecon)

if(CBCTRECON_SANITIZE)
  sanitize_build(CbctRecon)
endif()

set(QT_USE_QTCORE TRUE)
set(QT_USE_QTGUI TRUE)
set(QT_USE_QTOPENGL TRUE)
# ############# Qt end wrapping ##############

set(Plastimatch_SRC ${CMAKE_BINARY_DIR}/Plastimatch-src/)
set(Plastimatch_BUILD_DIR ${CMAKE_BINARY_DIR}/Plastimatch-build/)

set(PLASTIMATCH_INCLUDE_DIRS
    ${PLASTIMATCH_INCLUDE_DIRS}
    "${Plastimatch_SRC}/src/"
    "${Plastimatch_SRC}/src/plastimatch/"
    "${Plastimatch_SRC}/src/plastimatch/util"
    "${Plastimatch_SRC}/src/plastimatch/dose"
    "${Plastimatch_SRC}/src/plastimatch/sys"
    "${Plastimatch_SRC}/src/plastimatch/base"
    "${Plastimatch_SRC}/src/plastimatch/register"
    "${Plastimatch_SRC}/src/plastimatch/reconstruct"
    "${Plastimatch_SRC}/src/plastimatch/opencl"
    "${Plastimatch_SRC}/src/plastimatch/segment"
    "${Plastimatch_BUILD_DIR}/")

# this is for "C++ General-> Add below to Additional Include Directories"
set(CBCTRECON_DEP_INCLUDES
    ${CBCTRECON_INCLUDE_DIRS}
    ${ITK_INCLUDE_DIRS}
    ${RTK_INCLUDE_DIRS}
    ${PLASTIMATCH_INCLUDE_DIRS}
    ${DCMTK_INCLUDE_DIR}
    ${CLFFT_INCLUDE_DIRS})

target_include_directories(
  CbctRecon
  PUBLIC ${CBCTRECON_DEP_INCLUDES}
  PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

# this is for "Linker->Additional dependencies"
target_link_libraries(CbctRecon PRIVATE CbctReconLib)

# ############# INSTALL ##############
# Using install/bin as storage for all dependencies before copy to INSTALL_PREFIX/bin ##

file(MAKE_DIRECTORY ${CMAKE_INSTALL_BINDIR})
file(MAKE_DIRECTORY ${CMAKE_INSTALL_PREFIX}/lut)

if(WIN32)
  set(QT_DLLS
      $<TARGET_FILE:Qt5::Widgets>
      $<TARGET_FILE:Qt5::Gui>
      $<TARGET_FILE:Qt5::OpenGL>
      $<TARGET_FILE:Qt5::Core>
      $<TARGET_FILE:Qt5::PrintSupport>)
  install(FILES ${QT_DLLS} DESTINATION ${CMAKE_INSTALL_BINDIR})
elseif(APPLE)
  foreach(target ${QT_LIBRARIES})
    get_target_property(type ${target} TYPE)
    if(NOT
       type
       STREQUAL
       "SHARED_LIBRARY")
      continue()
    endif()
    get_property(
      location
      TARGET ${target}
      PROPERTY LOCATION_RELEASE)
    # Install .so and versioned .so.x.y
    get_filename_component(QT_LIB_DIR_tmp ${location} PATH)
    get_filename_component(QT_LIB_NAME_tmp ${location} NAME)
    string(
      REPLACE ".${_qt_version}"
              ""
              QT_LIB_NAME_tmp
              ${QT_LIB_NAME_tmp})
    message(STATUS "Attmpting installing ${QT_LIB_NAME_tmp} from dir: ${QT_LIB_DIR_tmp}")
    install(
      DIRECTORY ${QT_LIB_DIR_tmp}/
      DESTINATION ${CMAKE_INSTALL_BINDIR}/${QT_LIB_NAME_tmp}.framework/
      COMPONENT Runtime # ${CMAKE_CURRENT_BINARY_DIR}/../
      FILES_MATCHING
      PATTERN "${QT_LIB_NAME_tmp}*"
      PATTERN "${QT_LIB_NAME_tmp}*.debug" EXCLUDE)
  endforeach()
endif()

install(
  TARGETS CbctRecon
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  RESOURCE DESTINATION ${CMAKE_INSTALL_BINDIR}/ui)

install(FILES ${CMAKE_CURRENT_LIST_DIR}/ui/style.qss DESTINATION ${CMAKE_INSTALL_BINDIR}/ui)

# -----------------------------------------------------------------------------
# Additional install files
# -----------------------------------------------------------------------------
if(WIN32)
  if(NOT USE_SYSTEM_QT)
    hunter_add_package(Qt COMPONENTS qttools)
  endif()
  include(${CMAKE_CURRENT_LIST_DIR}/cmake/WindeployQt.cmake)
  if($<CONFIG:Debug>)
    windeployqt(CbctRecon "debug" "${CMAKE_CURRENT_BINARY_DIR}/QtLibs")
  else()
    windeployqt(CbctRecon "release" "${CMAKE_CURRENT_BINARY_DIR}/QtLibs")
  endif()

  file(GLOB DLLFILES "${CMAKE_CURRENT_BINARY_DIR}/QtLibs/*.dll")
  install(FILES ${DLLFILES} DESTINATION ${CMAKE_INSTALL_BINDIR})
  set(QT_SUBDIRS
      "iconengines"
      "imageformats"
      "platforms"
      "styles"
      "translations")
  foreach(subdir ${QT_SUBDIRS})
    install(DIRECTORY "$<TARGET_FILE_DIR:CbctRecon>/${subdir}" DESTINATION ${CMAKE_INSTALL_BINDIR})
  endforeach()

  if(OPENMP_FOUND)
    set(CMAKE_INSTALL_OPENMP_LIBRARIES ON)
  endif(OPENMP_FOUND)
  include(InstallRequiredSystemLibraries)

endif()

# -----------------------------------------------------------------------------
# Packaging - Copied with modification from Plastimatch
# -----------------------------------------------------------------------------
# Simple version: "make package_source" to create tarball
#
# Advanced version: to generate... * zip    : cpack -G ZIP <source tree> * tar.bz2: cpack
# -G TBZ2 <source tree>
#
# ...from within your build directory
# -----------------------------------------------------------------------------
# Choose generator
if(CbctRecon_PACKAGE_WIX)
  set(CPACK_GENERATOR "WIX")
elseif(CbctRecon_PACKAGE_NSIS)
  set(CPACK_GENERATOR "NSIS")
else()
  set(CPACK_GENERATOR "ZIP")
endif()
set(CPACK_SOURCE_GENERATOR "TBZ2")

# For WiX install, and possibly for debian -dev targets, we should properly set up the
# install components. Until then, let's just kill it
set(CPACK_MONOLITHIC_INSTALL 1)

# General stuff
set(CPACK_INSTALL_PREFIX "")
set(CPACK_PACKAGE_CONTACT "agravgaard@protonmail.com")
set(CPACK_PACKAGE_NAME "cbctrecon")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "CbctRecon")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "CbctRecon - A Medical Imaging Application")
set(CPACK_PACKAGE_VENDOR "CbctRecon Dev Team")
set(CPACK_PACKAGE_VERSION_MAJOR "${CbctRecon_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${CbctRecon_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${CbctRecon_VERSION_PATCH}")
set(CPACK_PACKAGE_VERSION_TWEAK "${CbctRecon_VERSION_TWEAK}")
set(CPACK_SOURCE_IGNORE_FILES "/\\\\.git" "/\\\\.svn" "~$")

# NSIS stuff
set(CPACK_NSIS_INSTALLED_ICON_NAME "Icon1.ico")
set(CPACK_NSIS_HELP_LINK "https://gitlab.com/agravgaard/cbctrecon")
set(CPACK_NSIS_URL_INFO_ABOUT "https://gitlab.com/agravgaard/cbctrecon")
set(CPACK_NSIS_CONTACT "andreasga22@gmail.com")
set(CbctRecon_NSIS_VERSION_STRING
    "${CbctRecon_VERSION_MAJOR}.${CbctRecon_VERSION_MINOR}.${CbctRecon_VERSION_PATCH}")

# WIX stuff
set(CbctRecon_WINDOWS_INSTALL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extra/windows-install")
set(CPACK_WIX_HELP_LINK "https://gitlab.com/agravgaard/cbctrecon")
set(CPACK_WIX_UPGRADE_GUID "43DF18E5-D982-413F-834C-2865B977C145")
set(CPACK_WIX_LICENSE_RTF "${CbctRecon_WINDOWS_INSTALL_DIR}/License.rtf")
set(CPACK_WIX_UI_BANNER "${CbctRecon_WINDOWS_INSTALL_DIR}/cbctrecon_logo.jpg")
set(CPACK_WIX_UI_DIALOG "${CbctRecon_WINDOWS_INSTALL_DIR}/cbctrecon_logo_GUI.jpg")

set(CPACK_WIX_EXTRA_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/extra/wix/cmake_extra_dialog.wxs"
    "${CMAKE_CURRENT_SOURCE_DIR}/extra/wix/install_dir.wxs"
    "${CMAKE_CURRENT_SOURCE_DIR}/extra/wix/start_menu.wxs")
set(CPACK_WIX_PATCH_FILE "${CMAKE_CURRENT_SOURCE_DIR}/extra/wix/patch_path_env.xml"
                         # "${CMAKE_CURRENT_SOURCE_DIR}/extra/wix/patch_start_menu.xml"
)
set(CPACK_WIX_UI_REF "CMakeUI_InstallDir")
if(CPACK_GENERATOR STREQUAL "WIX")
  set(CPACK_PACKAGE_NAME "CbctRecon")
  if(CPACK_PACKAGE_VERSION_TWEAK AND CPACK_PACKAGE_VERSION_TWEAK GREATER "0")
    set(CPACK_PACKAGE_VERSION
        "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}.${CPACK_PACKAGE_VERSION_TWEAK}"
    )
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(CPACK_SYSTEM_NAME "win64")
    else()
      set(CPACK_SYSTEM_NAME "win32")
    endif()
    set(CPACK_PACKAGE_FILE_NAME
        "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_SYSTEM_NAME}")
  endif()
  set_property(INSTALL "bin/CbctRecon.exe" PROPERTY CPACK_START_MENU_SHORTCUTS
                                                    "CbctRecon Application")
endif()

# ZIP stuff
if(CPACK_GENERATOR STREQUAL "ZIP")
  set(CPACK_SET_DESTDIR ON)
endif()

include(CPack)

# -----------------------------------------------------------------------------
# Option to generate .clang_complete for hip Vim users using clang_complete
# -----------------------------------------------------------------------------
if(CBCTRECON_CLANG_COMPLETE)
  get_property(
    inc_dirs
    DIRECTORY
    PROPERTY INCLUDE_DIRECTORIES)
  set(file_clang_complete "${CMAKE_CURRENT_SOURCE_DIR}/.clang_complete")
  foreach(arg ${inc_dirs})
    set(inc_args "${inc_args} -I${arg}\n")
  endforeach()
  file(WRITE "${file_clang_complete}" "${inc_args}")
  message(STATUS "Generated ${file_clang_complete}")
endif()
