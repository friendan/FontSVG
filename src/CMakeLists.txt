cmake_minimum_required(VERSION 3.10)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(FontSVG VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_definitions(-DCMAKE)

if(MSVC OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(/W3 /WX- /utf-8)
endif()

foreach(_flag_var
    CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_DEBUG
    CMAKE_C_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELEASE
    CMAKE_C_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_MINSIZEREL
    CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_RELWITHDEBINFO)
    if(DEFINED ${_flag_var})
        string(REGEX REPLACE "/MD" "/MT" ${_flag_var} "${${_flag_var}}")
    endif()
endforeach()

set(CMAKE_DEBUG_POSTFIX "_mtd")
set(CMAKE_RELEASE_POSTFIX "_mt")
set(CMAKE_MINSIZEREL_POSTFIX "_mt")
set(CMAKE_RELWITHDEBINFO_POSTFIX "_mt")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../bin)

set(FONTSVG_SRC_DIR "${CMAKE_SOURCE_DIR}/FontSVG")
set(FONTSVG_SKIN_DIR "${CMAKE_SOURCE_DIR}/../bin/skin/FontSVG")
set(FONTSVG_SKIN_ZIP "${CMAKE_CURRENT_BINARY_DIR}/FontSVGSkin.zip")
file(TO_CMAKE_PATH "${FONTSVG_SKIN_ZIP}" FONTSVG_SKIN_ZIP_RC_PATH)
configure_file(
    ${FONTSVG_SRC_DIR}/FontSVGZip.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/FontSVGZip.rc
    @ONLY
)

# ---------- DUIX（DuiLib Ultimate）----------
if(DEFINED ENV{DUIX} AND NOT "$ENV{DUIX}" STREQUAL "")
    set(DUIX_ROOT "$ENV{DUIX}")
else()
    set(DUIX_ROOT "H:/sdk/DuiLib/trunk/3rd/DuiLib_Ultimate-master")
endif()
get_filename_component(DUIX_ROOT "${DUIX_ROOT}" ABSOLUTE)
if(NOT EXISTS "${DUIX_ROOT}/src/DuiLib/UIlib.h")
    message(FATAL_ERROR "DUIX not found (set env DUIX). Tried: ${DUIX_ROOT}")
endif()
message(STATUS "DUIX_ROOT=${DUIX_ROOT}")

set(DUIX_INCLUDE "${DUIX_ROOT}/src/DuiLib")
set(DUIX_BIN "${DUIX_ROOT}/bin")
set(DUIX_WEBVIEW2_ROOT "${DUIX_ROOT}/src/3rd/webview2/pkg/build/native")
set(WEBVIEW2_ROOT "${DUIX_WEBVIEW2_ROOT}")
message(STATUS "WEBVIEW2_ROOT=${WEBVIEW2_ROOT}")

add_library(DuiLib STATIC IMPORTED GLOBAL)
set_target_properties(DuiLib PROPERTIES
    IMPORTED_LOCATION_DEBUG "${DUIX_BIN}/DuiLib_mtd.lib"
    IMPORTED_LOCATION_RELEASE "${DUIX_BIN}/DuiLib_mt.lib"
    IMPORTED_LOCATION_MINSIZEREL "${DUIX_BIN}/DuiLib_mt.lib"
    IMPORTED_LOCATION_RELWITHDEBINFO "${DUIX_BIN}/DuiLib_mt.lib"
    IMPORTED_LOCATION "${DUIX_BIN}/DuiLib_mtd.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${DUIX_INCLUDE};${WEBVIEW2_ROOT}/include"
    INTERFACE_COMPILE_DEFINITIONS "UILIB_STATIC;DUILIB_RENDER_BACKEND=1;DUILIB_HAS_WEBVIEW2=1"
)

add_library(lunasvg STATIC IMPORTED GLOBAL)
set_target_properties(lunasvg PROPERTIES
    IMPORTED_LOCATION_DEBUG "${DUIX_BIN}/lunasvg_mtd.lib"
    IMPORTED_LOCATION_RELEASE "${DUIX_BIN}/lunasvg_mt.lib"
    IMPORTED_LOCATION_MINSIZEREL "${DUIX_BIN}/lunasvg_mt.lib"
    IMPORTED_LOCATION_RELWITHDEBINFO "${DUIX_BIN}/lunasvg_mt.lib"
    IMPORTED_LOCATION "${DUIX_BIN}/lunasvg_mtd.lib"
)

add_library(plutovg STATIC IMPORTED GLOBAL)
set_target_properties(plutovg PROPERTIES
    IMPORTED_LOCATION_DEBUG "${DUIX_BIN}/plutovg_mtd.lib"
    IMPORTED_LOCATION_RELEASE "${DUIX_BIN}/plutovg_mt.lib"
    IMPORTED_LOCATION_MINSIZEREL "${DUIX_BIN}/plutovg_mt.lib"
    IMPORTED_LOCATION_RELWITHDEBINFO "${DUIX_BIN}/plutovg_mt.lib"
    IMPORTED_LOCATION "${DUIX_BIN}/plutovg_mtd.lib"
)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(_WV2_ARCH x64)
else()
    set(_WV2_ARCH x86)
endif()
set(_WV2_LOADER "${WEBVIEW2_ROOT}/${_WV2_ARCH}/WebView2LoaderStatic.lib")
if(NOT EXISTS "${_WV2_LOADER}")
    message(FATAL_ERROR "WebView2LoaderStatic.lib not found: ${_WV2_LOADER}")
endif()

set(FONTSVG_SOURCES
    ${FONTSVG_SRC_DIR}/main.cpp
    ${FONTSVG_SRC_DIR}/StdAfx.cpp
    ${FONTSVG_SRC_DIR}/Logger.cpp
    ${FONTSVG_SRC_DIR}/MainWnd.cpp
    ${FONTSVG_SRC_DIR}/FontCatalog.cpp
    ${FONTSVG_SRC_DIR}/GlyphSvg.cpp
    ${FONTSVG_SRC_DIR}/IconExportWnd.cpp
    ${FONTSVG_SRC_DIR}/ImageExportWnd.cpp
    ${FONTSVG_SRC_DIR}/AvatarExportWnd.cpp
    ${FONTSVG_SRC_DIR}/GlyphCellUI.cpp
    ${FONTSVG_SRC_DIR}/GlyphPath.cpp
    ${FONTSVG_SRC_DIR}/OutlineCanvasUI.cpp
    ${FONTSVG_SRC_DIR}/OutlineEditWnd.cpp
    ${FONTSVG_SRC_DIR}/FontSVG.rc
    ${CMAKE_CURRENT_BINARY_DIR}/FontSVGZip.rc
)

file(GLOB_RECURSE FONTSVG_SKIN_FILES CONFIGURE_DEPENDS
    "${FONTSVG_SKIN_DIR}/*"
)

add_custom_target(FontSVGSkinZip ALL
    BYPRODUCTS "${FONTSVG_SKIN_ZIP}"
    COMMAND ${CMAKE_COMMAND} -E rm -f "${FONTSVG_SKIN_ZIP}"
    COMMAND ${CMAKE_COMMAND} -E chdir "${FONTSVG_SKIN_DIR}"
        ${CMAKE_COMMAND} -E tar cf "${FONTSVG_SKIN_ZIP}" --format=zip .
    DEPENDS ${FONTSVG_SKIN_FILES}
    COMMENT "Pack FontSVG skin zip"
    VERBATIM
)

add_executable(FontSVG WIN32 ${FONTSVG_SOURCES})
add_dependencies(FontSVG FontSVGSkinZip)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/FontSVGZip.rc PROPERTIES
    OBJECT_DEPENDS "${FONTSVG_SKIN_ZIP}"
)
set_source_files_properties(${FONTSVG_SRC_DIR}/FontSVG.rc PROPERTIES
    OBJECT_DEPENDS "${FONTSVG_SRC_DIR}/App.ico"
)
target_include_directories(FontSVG PRIVATE ${FONTSVG_SRC_DIR})
target_compile_definitions(FontSVG PRIVATE UNICODE _UNICODE)
target_compile_features(FontSVG PRIVATE cxx_std_17)
target_link_libraries(FontSVG PRIVATE
    DuiLib
    lunasvg
    plutovg
    "${_WV2_LOADER}"
    d2d1 dwrite d3d11 dxgi dcomp windowscodecs
    version
    shell32
    comdlg32
    gdi32
    user32
    ole32
)

set_target_properties(FontSVG PROPERTIES
    DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}"
    RELEASE_POSTFIX "${CMAKE_RELEASE_POSTFIX}"
    MINSIZEREL_POSTFIX "${CMAKE_MINSIZEREL_POSTFIX}"
    RELWITHDEBINFO_POSTFIX "${CMAKE_RELWITHDEBINFO_POSTFIX}"
)
