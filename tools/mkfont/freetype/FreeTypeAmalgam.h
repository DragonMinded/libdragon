/***************************************************************************/
/*                                                                         */
/*  FreeTypeAmalgam.h                                                      */
/*                                                                         */
/*  Copyright 2003-2007, 2011 by                                           */
/*  David Turner, Robert Wilhelm, and Werner Lemberg.                      */
/*                                                                         */
/*  This file is part of the FreeType project, and may only be used,       */
/*  modified, and distributed under the terms of the FreeType project      */
/*  license, LICENSE.TXT.  By continuing to use, modify, or distribute     */
/*  this file you indicate that you have read the license and              */
/*  understand and accept it fully.                                        */
/*                                                                         */
/***************************************************************************/

#ifdef _MSC_VER
#pragma push_macro("_CRT_SECURE_NO_WARNINGS")
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif


/*** Start of inlined file: ft2build.h ***/
  /**************************************************************************
   *
   * This is the 'entry point' for FreeType header file inclusions, to be
   * loaded before all other header files.
   *
   * A typical example is
   *
   * ```
   *   #include <ft2build.h>
   *   #include <freetype/freetype.h>
   * ```
   *
   */

#ifndef FT2BUILD_H_
#define FT2BUILD_H_


/*** Start of inlined file: ftheader.h ***/
#ifndef FTHEADER_H_
#define FTHEADER_H_

  /*@***********************************************************************/
  /*                                                                       */
  /* <Macro>                                                               */
  /*    FT_BEGIN_HEADER                                                    */
  /*                                                                       */
  /* <Description>                                                         */
  /*    This macro is used in association with @FT_END_HEADER in header    */
  /*    files to ensure that the declarations within are properly          */
  /*    encapsulated in an `extern "C" { .. }` block when included from a  */
  /*    C++ compiler.                                                      */
  /*                                                                       */
#ifndef FT_BEGIN_HEADER
#  ifdef __cplusplus
#    define FT_BEGIN_HEADER  extern "C" {
#  else
#  define FT_BEGIN_HEADER  /* nothing */
#  endif
#endif

  /*@***********************************************************************/
  /*                                                                       */
  /* <Macro>                                                               */
  /*    FT_END_HEADER                                                      */
  /*                                                                       */
  /* <Description>                                                         */
  /*    This macro is used in association with @FT_BEGIN_HEADER in header  */
  /*    files to ensure that the declarations within are properly          */
  /*    encapsulated in an `extern "C" { .. }` block when included from a  */
  /*    C++ compiler.                                                      */
  /*                                                                       */
#ifndef FT_END_HEADER
#  ifdef __cplusplus
#    define FT_END_HEADER  }
#  else
#   define FT_END_HEADER  /* nothing */
#  endif
#endif

  /**************************************************************************
   *
   * Aliases for the FreeType 2 public and configuration files.
   *
   */

  /**************************************************************************
   *
   * @section:
   *   header_file_macros
   *
   * @title:
   *   Header File Macros
   *
   * @abstract:
   *   Macro definitions used to `#include` specific header files.
   *
   * @description:
   *   In addition to the normal scheme of including header files like
   *
   *   ```
   *     #include <freetype/freetype.h>
   *     #include <freetype/ftmm.h>
   *     #include <freetype/ftglyph.h>
   *   ```
   *
   *   it is possible to used named macros instead.  They can be used
   *   directly in `#include` statements as in
   *
   *   ```
   *     #include FT_FREETYPE_H
   *     #include FT_MULTIPLE_MASTERS_H
   *     #include FT_GLYPH_H
   *   ```
   *
   *   These macros were introduced to overcome the infamous 8.3~naming rule
   *   required by DOS (and `FT_MULTIPLE_MASTERS_H` is a lot more meaningful
   *   than `ftmm.h`).
   *
   */

  /* configuration files */

  /**************************************************************************
   *
   * @macro:
   *   FT_CONFIG_CONFIG_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing
   *   FreeType~2 configuration data.
   *
   */
#ifndef FT_CONFIG_CONFIG_H
#define FT_CONFIG_CONFIG_H  <freetype/config/ftconfig.h>
#endif

  /**************************************************************************
   *
   * @macro:
   *   FT_CONFIG_STANDARD_LIBRARY_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing
   *   FreeType~2 interface to the standard C library functions.
   *
   */
#ifndef FT_CONFIG_STANDARD_LIBRARY_H
#define FT_CONFIG_STANDARD_LIBRARY_H  <freetype/config/ftstdlib.h>
#endif

  /**************************************************************************
   *
   * @macro:
   *   FT_CONFIG_OPTIONS_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing
   *   FreeType~2 project-specific configuration options.
   *
   */
#ifndef FT_CONFIG_OPTIONS_H
#define FT_CONFIG_OPTIONS_H  <freetype/config/ftoption.h>
#endif

  /**************************************************************************
   *
   * @macro:
   *   FT_CONFIG_MODULES_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   list of FreeType~2 modules that are statically linked to new library
   *   instances in @FT_Init_FreeType.
   *
   */
#ifndef FT_CONFIG_MODULES_H
#define FT_CONFIG_MODULES_H  <freetype/config/ftmodule.h>
#endif

  /* */

  /* public headers */

  /**************************************************************************
   *
   * @macro:
   *   FT_FREETYPE_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   base FreeType~2 API.
   *
   */
#define FT_FREETYPE_H  <freetype/freetype.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_ERRORS_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   list of FreeType~2 error codes (and messages).
   *
   *   It is included by @FT_FREETYPE_H.
   *
   */
#define FT_ERRORS_H  <freetype/fterrors.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_MODULE_ERRORS_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   list of FreeType~2 module error offsets (and messages).
   *
   */
#define FT_MODULE_ERRORS_H  <freetype/ftmoderr.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_SYSTEM_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   FreeType~2 interface to low-level operations (i.e., memory management
   *   and stream i/o).
   *
   *   It is included by @FT_FREETYPE_H.
   *
   */
#define FT_SYSTEM_H  <freetype/ftsystem.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_IMAGE_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing type
   *   definitions related to glyph images (i.e., bitmaps, outlines,
   *   scan-converter parameters).
   *
   *   It is included by @FT_FREETYPE_H.
   *
   */
#define FT_IMAGE_H  <freetype/ftimage.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_TYPES_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   basic data types defined by FreeType~2.
   *
   *   It is included by @FT_FREETYPE_H.
   *
   */
#define FT_TYPES_H  <freetype/fttypes.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_LIST_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   list management API of FreeType~2.
   *
   *   (Most applications will never need to include this file.)
   *
   */
#define FT_LIST_H  <freetype/ftlist.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_OUTLINE_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   scalable outline management API of FreeType~2.
   *
   */
#define FT_OUTLINE_H  <freetype/ftoutln.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_SIZES_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   API which manages multiple @FT_Size objects per face.
   *
   */
#define FT_SIZES_H  <freetype/ftsizes.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_MODULE_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   module management API of FreeType~2.
   *
   */
#define FT_MODULE_H  <freetype/ftmodapi.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_RENDER_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   renderer module management API of FreeType~2.
   *
   */
#define FT_RENDER_H  <freetype/ftrender.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_DRIVER_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing
   *   structures and macros related to the driver modules.
   *
   */
#define FT_DRIVER_H  <freetype/ftdriver.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_AUTOHINTER_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing
   *   structures and macros related to the auto-hinting module.
   *
   *   Deprecated since version~2.9; use @FT_DRIVER_H instead.
   *
   */
#define FT_AUTOHINTER_H  FT_DRIVER_H

  /**************************************************************************
   *
   * @macro:
   *   FT_CFF_DRIVER_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing
   *   structures and macros related to the CFF driver module.
   *
   *   Deprecated since version~2.9; use @FT_DRIVER_H instead.
   *
   */
#define FT_CFF_DRIVER_H  FT_DRIVER_H

  /**************************************************************************
   *
   * @macro:
   *   FT_TRUETYPE_DRIVER_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing
   *   structures and macros related to the TrueType driver module.
   *
   *   Deprecated since version~2.9; use @FT_DRIVER_H instead.
   *
   */
#define FT_TRUETYPE_DRIVER_H  FT_DRIVER_H

  /**************************************************************************
   *
   * @macro:
   *   FT_PCF_DRIVER_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing
   *   structures and macros related to the PCF driver module.
   *
   *   Deprecated since version~2.9; use @FT_DRIVER_H instead.
   *
   */
#define FT_PCF_DRIVER_H  FT_DRIVER_H

  /**************************************************************************
   *
   * @macro:
   *   FT_TYPE1_TABLES_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   types and API specific to the Type~1 format.
   *
   */
#define FT_TYPE1_TABLES_H  <freetype/t1tables.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_TRUETYPE_IDS_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   enumeration values which identify name strings, languages, encodings,
   *   etc.  This file really contains a _large_ set of constant macro
   *   definitions, taken from the TrueType and OpenType specifications.
   *
   */
#define FT_TRUETYPE_IDS_H  <freetype/ttnameid.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_TRUETYPE_TABLES_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   types and API specific to the TrueType (as well as OpenType) format.
   *
   */
#define FT_TRUETYPE_TABLES_H  <freetype/tttables.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_TRUETYPE_TAGS_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   definitions of TrueType four-byte 'tags' which identify blocks in
   *   SFNT-based font formats (i.e., TrueType and OpenType).
   *
   */
#define FT_TRUETYPE_TAGS_H  <freetype/tttags.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_BDF_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   definitions of an API which accesses BDF-specific strings from a face.
   *
   */
#define FT_BDF_H  <freetype/ftbdf.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_CID_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   definitions of an API which access CID font information from a face.
   *
   */
#define FT_CID_H  <freetype/ftcid.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_GZIP_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   definitions of an API which supports gzip-compressed files.
   *
   */
#define FT_GZIP_H  <freetype/ftgzip.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_LZW_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   definitions of an API which supports LZW-compressed files.
   *
   */
#define FT_LZW_H  <freetype/ftlzw.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_BZIP2_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   definitions of an API which supports bzip2-compressed files.
   *
   */
#define FT_BZIP2_H  <freetype/ftbzip2.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_WINFONTS_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   definitions of an API which supports Windows FNT files.
   *
   */
#define FT_WINFONTS_H   <freetype/ftwinfnt.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_GLYPH_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   API of the optional glyph management component.
   *
   */
#define FT_GLYPH_H  <freetype/ftglyph.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_BITMAP_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   API of the optional bitmap conversion component.
   *
   */
#define FT_BITMAP_H  <freetype/ftbitmap.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_BBOX_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   API of the optional exact bounding box computation routines.
   *
   */
#define FT_BBOX_H  <freetype/ftbbox.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_CACHE_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   API of the optional FreeType~2 cache sub-system.
   *
   */
#define FT_CACHE_H  <freetype/ftcache.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_MAC_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   Macintosh-specific FreeType~2 API.  The latter is used to access fonts
   *   embedded in resource forks.
   *
   *   This header file must be explicitly included by client applications
   *   compiled on the Mac (note that the base API still works though).
   *
   */
#define FT_MAC_H  <freetype/ftmac.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_MULTIPLE_MASTERS_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   optional multiple-masters management API of FreeType~2.
   *
   */
#define FT_MULTIPLE_MASTERS_H  <freetype/ftmm.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_SFNT_NAMES_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   optional FreeType~2 API which accesses embedded 'name' strings in
   *   SFNT-based font formats (i.e., TrueType and OpenType).
   *
   */
#define FT_SFNT_NAMES_H  <freetype/ftsnames.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_OPENTYPE_VALIDATE_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   optional FreeType~2 API which validates OpenType tables ('BASE',
   *   'GDEF', 'GPOS', 'GSUB', 'JSTF').
   *
   */
#define FT_OPENTYPE_VALIDATE_H  <freetype/ftotval.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_GX_VALIDATE_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   optional FreeType~2 API which validates TrueTypeGX/AAT tables ('feat',
   *   'mort', 'morx', 'bsln', 'just', 'kern', 'opbd', 'trak', 'prop').
   *
   */
#define FT_GX_VALIDATE_H  <freetype/ftgxval.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_PFR_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   FreeType~2 API which accesses PFR-specific data.
   *
   */
#define FT_PFR_H  <freetype/ftpfr.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_STROKER_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   FreeType~2 API which provides functions to stroke outline paths.
   */
#define FT_STROKER_H  <freetype/ftstroke.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_SYNTHESIS_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   FreeType~2 API which performs artificial obliquing and emboldening.
   */
#define FT_SYNTHESIS_H  <freetype/ftsynth.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_FONT_FORMATS_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   FreeType~2 API which provides functions specific to font formats.
   */
#define FT_FONT_FORMATS_H  <freetype/ftfntfmt.h>

  /* deprecated */
#define FT_XFREE86_H  FT_FONT_FORMATS_H

  /**************************************************************************
   *
   * @macro:
   *   FT_TRIGONOMETRY_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   FreeType~2 API which performs trigonometric computations (e.g.,
   *   cosines and arc tangents).
   */
#define FT_TRIGONOMETRY_H  <freetype/fttrigon.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_LCD_FILTER_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   FreeType~2 API which performs color filtering for subpixel rendering.
   */
#define FT_LCD_FILTER_H  <freetype/ftlcdfil.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_INCREMENTAL_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   FreeType~2 API which performs incremental glyph loading.
   */
#define FT_INCREMENTAL_H  <freetype/ftincrem.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_GASP_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   FreeType~2 API which returns entries from the TrueType GASP table.
   */
#define FT_GASP_H  <freetype/ftgasp.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_ADVANCES_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   FreeType~2 API which returns individual and ranged glyph advances.
   */
#define FT_ADVANCES_H  <freetype/ftadvanc.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_COLOR_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   FreeType~2 API which handles the OpenType 'CPAL' table.
   */
#define FT_COLOR_H  <freetype/ftcolor.h>

  /**************************************************************************
   *
   * @macro:
   *   FT_OTSVG_H
   *
   * @description:
   *   A macro used in `#include` statements to name the file containing the
   *   FreeType~2 API which handles the OpenType 'SVG~' glyphs.
   */
#define FT_OTSVG_H  <freetype/otsvg.h>

  /* */

  /* These header files don't need to be included by the user. */
#define FT_ERROR_DEFINITIONS_H  <freetype/fterrdef.h>
#define FT_PARAMETER_TAGS_H     <freetype/ftparams.h>

  /* Deprecated macros. */
#define FT_UNPATENTED_HINTING_H   <freetype/ftparams.h>
#define FT_TRUETYPE_UNPATENTED_H  <freetype/ftparams.h>

  /* `FT_CACHE_H` is the only header file needed for the cache subsystem. */
#define FT_CACHE_IMAGE_H          FT_CACHE_H
#define FT_CACHE_SMALL_BITMAPS_H  FT_CACHE_H
#define FT_CACHE_CHARMAP_H        FT_CACHE_H

  /* The internals of the cache sub-system are no longer exposed.  We */
  /* default to `FT_CACHE_H` at the moment just in case, but we know  */
  /* of no rogue client that uses them.                               */
  /*                                                                  */
#define FT_CACHE_MANAGER_H           FT_CACHE_H
#define FT_CACHE_INTERNAL_MRU_H      FT_CACHE_H
#define FT_CACHE_INTERNAL_MANAGER_H  FT_CACHE_H
#define FT_CACHE_INTERNAL_CACHE_H    FT_CACHE_H
#define FT_CACHE_INTERNAL_GLYPH_H    FT_CACHE_H
#define FT_CACHE_INTERNAL_IMAGE_H    FT_CACHE_H
#define FT_CACHE_INTERNAL_SBITS_H    FT_CACHE_H

/* TODO(david): Move this section below to a different header */
#ifdef FT2_BUILD_LIBRARY
#if defined( _MSC_VER )      /* Visual C++ (and Intel C++) */

  /* We disable the warning `conditional expression is constant' here */
  /* in order to compile cleanly with the maximum level of warnings.  */
  /* In particular, the warning complains about stuff like `while(0)' */
  /* which is very useful in macro definitions.  There is no benefit  */
  /* in having it enabled.                                            */
#pragma warning( disable : 4127 )

#endif /* _MSC_VER */
#endif /* FT2_BUILD_LIBRARY */

#endif /* FTHEADER_H_ */

/* END */

/*** End of inlined file: ftheader.h ***/

#endif /* FT2BUILD_H_ */

/* END */

/*** End of inlined file: ft2build.h ***/


/*** Start of inlined file: freetype.h ***/
#ifndef FREETYPE_H_
#define FREETYPE_H_


/*** Start of inlined file: ftconfig.h ***/
  /**************************************************************************
   *
   * This header file contains a number of macro definitions that are used by
   * the rest of the engine.  Most of the macros here are automatically
   * determined at compile time, and you should not need to change it to port
   * FreeType, except to compile the library with a non-ANSI compiler.
   *
   * Note however that if some specific modifications are needed, we advise
   * you to place a modified copy in your build directory.
   *
   * The build directory is usually `builds/<system>`, and contains
   * system-specific files that are always included first when building the
   * library.
   *
   * This ANSI version should stay in `include/config/`.
   *
   */

#ifndef FTCONFIG_H_
#define FTCONFIG_H_


/*** Start of inlined file: ftoption.h ***/
#ifndef FTOPTION_H_
#define FTOPTION_H_

FT_BEGIN_HEADER

  /**************************************************************************
   *
   *                USER-SELECTABLE CONFIGURATION MACROS
   *
   * This file contains the default configuration macro definitions for a
   * standard build of the FreeType library.  There are three ways to use
   * this file to build project-specific versions of the library:
   *
   * - You can modify this file by hand, but this is not recommended in
   *   cases where you would like to build several versions of the library
   *   from a single source directory.
   *
   * - You can put a copy of this file in your build directory, more
   *   precisely in `$BUILD/freetype/config/ftoption.h`, where `$BUILD` is
   *   the name of a directory that is included _before_ the FreeType include
   *   path during compilation.
   *
   *   The default FreeType Makefiles use the build directory
   *   `builds/<system>` by default, but you can easily change that for your
   *   own projects.
   *
   * - Copy the file <ft2build.h> to `$BUILD/ft2build.h` and modify it
   *   slightly to pre-define the macro `FT_CONFIG_OPTIONS_H` used to locate
   *   this file during the build.  For example,
   *
   *   ```
   *     #define FT_CONFIG_OPTIONS_H  <myftoptions.h>
   *     #include <freetype/config/ftheader.h>
   *   ```
   *
   *   will use `$BUILD/myftoptions.h` instead of this file for macro
   *   definitions.
   *
   *   Note also that you can similarly pre-define the macro
   *   `FT_CONFIG_MODULES_H` used to locate the file listing of the modules
   *   that are statically linked to the library at compile time.  By
   *   default, this file is `<freetype/config/ftmodule.h>`.
   *
   * We highly recommend using the third method whenever possible.
   *
   */

  /*************************************************************************/
  /*************************************************************************/
  /****                                                                 ****/
  /**** G E N E R A L   F R E E T Y P E   2   C O N F I G U R A T I O N ****/
  /****                                                                 ****/
  /*************************************************************************/
  /*************************************************************************/

  /*#************************************************************************
   *
   * If you enable this configuration option, FreeType recognizes an
   * environment variable called `FREETYPE_PROPERTIES`, which can be used to
   * control the various font drivers and modules.  The controllable
   * properties are listed in the section @properties.
   *
   * You have to undefine this configuration option on platforms that lack
   * the concept of environment variables (and thus don't have the `getenv`
   * function), for example Windows CE.
   *
   * `FREETYPE_PROPERTIES` has the following syntax form (broken here into
   * multiple lines for better readability).
   *
   * ```
   *   <optional whitespace>
   *   <module-name1> ':'
   *   <property-name1> '=' <property-value1>
   *   <whitespace>
   *   <module-name2> ':'
   *   <property-name2> '=' <property-value2>
   *   ...
   * ```
   *
   * Example:
   *
   * ```
   *   FREETYPE_PROPERTIES=truetype:interpreter-version=35 \
   *                       cff:no-stem-darkening=1
   * ```
   *
   */
#define FT_CONFIG_OPTION_ENVIRONMENT_PROPERTIES

  /**************************************************************************
   *
   * Uncomment the line below if you want to activate LCD rendering
   * technology similar to ClearType in this build of the library.  This
   * technology triples the resolution in the direction color subpixels.  To
   * mitigate color fringes inherent to this technology, you also need to
   * explicitly set up LCD filtering.
   *
   * When this macro is not defined, FreeType offers alternative LCD
   * rendering technology that produces excellent output.
   */
/* #define FT_CONFIG_OPTION_SUBPIXEL_RENDERING */

  /**************************************************************************
   *
   * Many compilers provide a non-ANSI 64-bit data type that can be used by
   * FreeType to speed up some computations.  However, this will create some
   * problems when compiling the library in strict ANSI mode.
   *
   * For this reason, the use of 64-bit integers is normally disabled when
   * the `__STDC__` macro is defined.  You can however disable this by
   * defining the macro `FT_CONFIG_OPTION_FORCE_INT64` here.
   *
   * For most compilers, this will only create compilation warnings when
   * building the library.
   *
   * ObNote: The compiler-specific 64-bit integers are detected in the
   *         file `ftconfig.h` either statically or through the `configure`
   *         script on supported platforms.
   */
#undef FT_CONFIG_OPTION_FORCE_INT64

  /**************************************************************************
   *
   * If this macro is defined, do not try to use an assembler version of
   * performance-critical functions (e.g., @FT_MulFix).  You should only do
   * that to verify that the assembler function works properly, or to execute
   * benchmark tests of the various implementations.
   */
/* #define FT_CONFIG_OPTION_NO_ASSEMBLER */

  /**************************************************************************
   *
   * If this macro is defined, try to use an inlined assembler version of the
   * @FT_MulFix function, which is a 'hotspot' when loading and hinting
   * glyphs, and which should be executed as fast as possible.
   *
   * Note that if your compiler or CPU is not supported, this will default to
   * the standard and portable implementation found in `ftcalc.c`.
   */
#define FT_CONFIG_OPTION_INLINE_MULFIX

  /**************************************************************************
   *
   * LZW-compressed file support.
   *
   *   FreeType now handles font files that have been compressed with the
   *   `compress` program.  This is mostly used to parse many of the PCF
   *   files that come with various X11 distributions.  The implementation
   *   uses NetBSD's `zopen` to partially uncompress the file on the fly (see
   *   `src/lzw/ftgzip.c`).
   *
   *   Define this macro if you want to enable this 'feature'.
   */
#define FT_CONFIG_OPTION_USE_LZW

  /**************************************************************************
   *
   * Gzip-compressed file support.
   *
   *   FreeType now handles font files that have been compressed with the
   *   `gzip` program.  This is mostly used to parse many of the PCF files
   *   that come with XFree86.  The implementation uses 'zlib' to partially
   *   uncompress the file on the fly (see `src/gzip/ftgzip.c`).
   *
   *   Define this macro if you want to enable this 'feature'.  See also the
   *   macro `FT_CONFIG_OPTION_SYSTEM_ZLIB` below.
   */
#define FT_CONFIG_OPTION_USE_ZLIB

  /**************************************************************************
   *
   * ZLib library selection
   *
   *   This macro is only used when `FT_CONFIG_OPTION_USE_ZLIB` is defined.
   *   It allows FreeType's 'ftgzip' component to link to the system's
   *   installation of the ZLib library.  This is useful on systems like
   *   Unix or VMS where it generally is already available.
   *
   *   If you let it undefined, the component will use its own copy of the
   *   zlib sources instead.  These have been modified to be included
   *   directly within the component and **not** export external function
   *   names.  This allows you to link any program with FreeType _and_ ZLib
   *   without linking conflicts.
   *
   *   Do not `#undef` this macro here since the build system might define
   *   it for certain configurations only.
   *
   *   If you use a build system like cmake or the `configure` script,
   *   options set by those programs have precedence, overwriting the value
   *   here with the configured one.
   *
   *   If you use the GNU make build system directly (that is, without the
   *   `configure` script) and you define this macro, you also have to pass
   *   `SYSTEM_ZLIB=yes` as an argument to make.
   */
/* #define FT_CONFIG_OPTION_SYSTEM_ZLIB */

  /**************************************************************************
   *
   * Bzip2-compressed file support.
   *
   *   FreeType now handles font files that have been compressed with the
   *   `bzip2` program.  This is mostly used to parse many of the PCF files
   *   that come with XFree86.  The implementation uses `libbz2` to partially
   *   uncompress the file on the fly (see `src/bzip2/ftbzip2.c`).  Contrary
   *   to gzip, bzip2 currently is not included and need to use the system
   *   available bzip2 implementation.
   *
   *   Define this macro if you want to enable this 'feature'.
   *
   *   If you use a build system like cmake or the `configure` script,
   *   options set by those programs have precedence, overwriting the value
   *   here with the configured one.
   */
/* #define FT_CONFIG_OPTION_USE_BZIP2 */

  /**************************************************************************
   *
   * Define to disable the use of file stream functions and types, `FILE`,
   * `fopen`, etc.  Enables the use of smaller system libraries on embedded
   * systems that have multiple system libraries, some with or without file
   * stream support, in the cases where file stream support is not necessary
   * such as memory loading of font files.
   */
/* #define FT_CONFIG_OPTION_DISABLE_STREAM_SUPPORT */

  /**************************************************************************
   *
   * PNG bitmap support.
   *
   *   FreeType now handles loading color bitmap glyphs in the PNG format.
   *   This requires help from the external libpng library.  Uncompressed
   *   color bitmaps do not need any external libraries and will be supported
   *   regardless of this configuration.
   *
   *   Define this macro if you want to enable this 'feature'.
   *
   *   If you use a build system like cmake or the `configure` script,
   *   options set by those programs have precedence, overwriting the value
   *   here with the configured one.
   */
/* #define FT_CONFIG_OPTION_USE_PNG */

  /**************************************************************************
   *
   * HarfBuzz support.
   *
   *   FreeType uses the HarfBuzz library to improve auto-hinting of OpenType
   *   fonts.  If available, many glyphs not directly addressable by a font's
   *   character map will be hinted also.
   *
   *   Define this macro if you want to enable this 'feature'.
   *
   *   If you use a build system like cmake or the `configure` script,
   *   options set by those programs have precedence, overwriting the value
   *   here with the configured one.
   */
/* #define FT_CONFIG_OPTION_USE_HARFBUZZ */

  /**************************************************************************
   *
   * Brotli support.
   *
   *   FreeType uses the Brotli library to provide support for decompressing
   *   WOFF2 streams.
   *
   *   Define this macro if you want to enable this 'feature'.
   *
   *   If you use a build system like cmake or the `configure` script,
   *   options set by those programs have precedence, overwriting the value
   *   here with the configured one.
   */
/* #define FT_CONFIG_OPTION_USE_BROTLI */

  /**************************************************************************
   *
   * Glyph Postscript Names handling
   *
   *   By default, FreeType 2 is compiled with the 'psnames' module.  This
   *   module is in charge of converting a glyph name string into a Unicode
   *   value, or return a Macintosh standard glyph name for the use with the
   *   TrueType 'post' table.
   *
   *   Undefine this macro if you do not want 'psnames' compiled in your
   *   build of FreeType.  This has the following effects:
   *
   *   - The TrueType driver will provide its own set of glyph names, if you
   *     build it to support postscript names in the TrueType 'post' table,
   *     but will not synthesize a missing Unicode charmap.
   *
   *   - The Type~1 driver will not be able to synthesize a Unicode charmap
   *     out of the glyphs found in the fonts.
   *
   *   You would normally undefine this configuration macro when building a
   *   version of FreeType that doesn't contain a Type~1 or CFF driver.
   */
#define FT_CONFIG_OPTION_POSTSCRIPT_NAMES

  /**************************************************************************
   *
   * Postscript Names to Unicode Values support
   *
   *   By default, FreeType~2 is built with the 'psnames' module compiled in.
   *   Among other things, the module is used to convert a glyph name into a
   *   Unicode value.  This is especially useful in order to synthesize on
   *   the fly a Unicode charmap from the CFF/Type~1 driver through a big
   *   table named the 'Adobe Glyph List' (AGL).
   *
   *   Undefine this macro if you do not want the Adobe Glyph List compiled
   *   in your 'psnames' module.  The Type~1 driver will not be able to
   *   synthesize a Unicode charmap out of the glyphs found in the fonts.
   */
#define FT_CONFIG_OPTION_ADOBE_GLYPH_LIST

  /**************************************************************************
   *
   * Support for Mac fonts
   *
   *   Define this macro if you want support for outline fonts in Mac format
   *   (mac dfont, mac resource, macbinary containing a mac resource) on
   *   non-Mac platforms.
   *
   *   Note that the 'FOND' resource isn't checked.
   */
#define FT_CONFIG_OPTION_MAC_FONTS

  /**************************************************************************
   *
   * Guessing methods to access embedded resource forks
   *
   *   Enable extra Mac fonts support on non-Mac platforms (e.g., GNU/Linux).
   *
   *   Resource forks which include fonts data are stored sometimes in
   *   locations which users or developers don't expected.  In some cases,
   *   resource forks start with some offset from the head of a file.  In
   *   other cases, the actual resource fork is stored in file different from
   *   what the user specifies.  If this option is activated, FreeType tries
   *   to guess whether such offsets or different file names must be used.
   *
   *   Note that normal, direct access of resource forks is controlled via
   *   the `FT_CONFIG_OPTION_MAC_FONTS` option.
   */
#ifdef FT_CONFIG_OPTION_MAC_FONTS
#define FT_CONFIG_OPTION_GUESSING_EMBEDDED_RFORK
#endif

  /**************************************************************************
   *
   * Allow the use of `FT_Incremental_Interface` to load typefaces that
   * contain no glyph data, but supply it via a callback function.  This is
   * required by clients supporting document formats which supply font data
   * incrementally as the document is parsed, such as the Ghostscript
   * interpreter for the PostScript language.
   */
#define FT_CONFIG_OPTION_INCREMENTAL

  /**************************************************************************
   *
   * The size in bytes of the render pool used by the scan-line converter to
   * do all of its work.
   */
#define FT_RENDER_POOL_SIZE  16384L

  /**************************************************************************
   *
   * FT_MAX_MODULES
   *
   *   The maximum number of modules that can be registered in a single
   *   FreeType library object.  32~is the default.
   */
#define FT_MAX_MODULES  32

  /**************************************************************************
   *
   * Debug level
   *
   *   FreeType can be compiled in debug or trace mode.  In debug mode,
   *   errors are reported through the 'ftdebug' component.  In trace mode,
   *   additional messages are sent to the standard output during execution.
   *
   *   Define `FT_DEBUG_LEVEL_ERROR` to build the library in debug mode.
   *   Define `FT_DEBUG_LEVEL_TRACE` to build it in trace mode.
   *
   *   Don't define any of these macros to compile in 'release' mode!
   *
   *   Do not `#undef` these macros here since the build system might define
   *   them for certain configurations only.
   */
/* #define FT_DEBUG_LEVEL_ERROR */
/* #define FT_DEBUG_LEVEL_TRACE */

  /**************************************************************************
   *
   * Logging
   *
   *   Compiling FreeType in debug or trace mode makes FreeType write error
   *   and trace log messages to `stderr`.  Enabling this macro
   *   automatically forces the `FT_DEBUG_LEVEL_ERROR` and
   *   `FT_DEBUG_LEVEL_TRACE` macros and allows FreeType to write error and
   *   trace log messages to a file instead of `stderr`.  For writing logs
   *   to a file, FreeType uses an the external `dlg` library (the source
   *   code is in `src/dlg`).
   *
   *   This option needs a C99 compiler.
   */
/* #define FT_DEBUG_LOGGING */

  /**************************************************************************
   *
   * Autofitter debugging
   *
   *   If `FT_DEBUG_AUTOFIT` is defined, FreeType provides some means to
   *   control the autofitter behaviour for debugging purposes with global
   *   boolean variables (consequently, you should **never** enable this
   *   while compiling in 'release' mode):
   *
   *   ```
   *     af_debug_disable_horz_hints_
   *     af_debug_disable_vert_hints_
   *     af_debug_disable_blue_hints_
   *   ```
   *
   *   Additionally, the following functions provide dumps of various
   *   internal autofit structures to stdout (using `printf`):
   *
   *   ```
   *     af_glyph_hints_dump_points
   *     af_glyph_hints_dump_segments
   *     af_glyph_hints_dump_edges
   *     af_glyph_hints_get_num_segments
   *     af_glyph_hints_get_segment_offset
   *   ```
   *
   *   As an argument, they use another global variable:
   *
   *   ```
   *     af_debug_hints_
   *   ```
   *
   *   Please have a look at the `ftgrid` demo program to see how those
   *   variables and macros should be used.
   *
   *   Do not `#undef` these macros here since the build system might define
   *   them for certain configurations only.
   */
/* #define FT_DEBUG_AUTOFIT */

  /**************************************************************************
   *
   * Memory Debugging
   *
   *   FreeType now comes with an integrated memory debugger that is capable
   *   of detecting simple errors like memory leaks or double deletes.  To
   *   compile it within your build of the library, you should define
   *   `FT_DEBUG_MEMORY` here.
   *
   *   Note that the memory debugger is only activated at runtime when when
   *   the _environment_ variable `FT2_DEBUG_MEMORY` is defined also!
   *
   *   Do not `#undef` this macro here since the build system might define it
   *   for certain configurations only.
   */
/* #define FT_DEBUG_MEMORY */

  /**************************************************************************
   *
   * Module errors
   *
   *   If this macro is set (which is _not_ the default), the higher byte of
   *   an error code gives the module in which the error has occurred, while
   *   the lower byte is the real error code.
   *
   *   Setting this macro makes sense for debugging purposes only, since it
   *   would break source compatibility of certain programs that use
   *   FreeType~2.
   *
   *   More details can be found in the files `ftmoderr.h` and `fterrors.h`.
   */
#undef FT_CONFIG_OPTION_USE_MODULE_ERRORS

  /**************************************************************************
   *
   * OpenType SVG Glyph Support
   *
   *   Setting this macro enables support for OpenType SVG glyphs.  By
   *   default, FreeType can only fetch SVG documents.  However, it can also
   *   render them if external rendering hook functions are plugged in at
   *   runtime.
   *
   *   More details on the hooks can be found in file `otsvg.h`.
   */
#define FT_CONFIG_OPTION_SVG

  /**************************************************************************
   *
   * Error Strings
   *
   *   If this macro is set, `FT_Error_String` will return meaningful
   *   descriptions.  This is not enabled by default to reduce the overall
   *   size of FreeType.
   *
   *   More details can be found in the file `fterrors.h`.
   */
/* #define FT_CONFIG_OPTION_ERROR_STRINGS */

  /*************************************************************************/
  /*************************************************************************/
  /****                                                                 ****/
  /****        S F N T   D R I V E R    C O N F I G U R A T I O N       ****/
  /****                                                                 ****/
  /*************************************************************************/
  /*************************************************************************/

  /**************************************************************************
   *
   * Define `TT_CONFIG_OPTION_EMBEDDED_BITMAPS` if you want to support
   * embedded bitmaps in all formats using the 'sfnt' module (namely
   * TrueType~& OpenType).
   */
#define TT_CONFIG_OPTION_EMBEDDED_BITMAPS

  /**************************************************************************
   *
   * Define `TT_CONFIG_OPTION_COLOR_LAYERS` if you want to support colored
   * outlines (from the 'COLR'/'CPAL' tables) in all formats using the 'sfnt'
   * module (namely TrueType~& OpenType).
   */
#define TT_CONFIG_OPTION_COLOR_LAYERS

  /**************************************************************************
   *
   * Define `TT_CONFIG_OPTION_POSTSCRIPT_NAMES` if you want to be able to
   * load and enumerate Postscript names of glyphs in a TrueType or OpenType
   * file.
   *
   * Note that if you do not compile the 'psnames' module by undefining the
   * above `FT_CONFIG_OPTION_POSTSCRIPT_NAMES` macro, the 'sfnt' module will
   * contain additional code to read the PostScript name table from a font.
   *
   * (By default, the module uses 'psnames' to extract glyph names.)
   */
#define TT_CONFIG_OPTION_POSTSCRIPT_NAMES

  /**************************************************************************
   *
   * Define `TT_CONFIG_OPTION_SFNT_NAMES` if your applications need to access
   * the internal name table in a SFNT-based format like TrueType or
   * OpenType.  The name table contains various strings used to describe the
   * font, like family name, copyright, version, etc.  It does not contain
   * any glyph name though.
   *
   * Accessing SFNT names is done through the functions declared in
   * `ftsnames.h`.
   */
#define TT_CONFIG_OPTION_SFNT_NAMES

  /**************************************************************************
   *
   * TrueType CMap support
   *
   *   Here you can fine-tune which TrueType CMap table format shall be
   *   supported.
   */
#define TT_CONFIG_CMAP_FORMAT_0
#define TT_CONFIG_CMAP_FORMAT_2
#define TT_CONFIG_CMAP_FORMAT_4
#define TT_CONFIG_CMAP_FORMAT_6
#define TT_CONFIG_CMAP_FORMAT_8
#define TT_CONFIG_CMAP_FORMAT_10
#define TT_CONFIG_CMAP_FORMAT_12
#define TT_CONFIG_CMAP_FORMAT_13
#define TT_CONFIG_CMAP_FORMAT_14

  /*************************************************************************/
  /*************************************************************************/
  /****                                                                 ****/
  /****    T R U E T Y P E   D R I V E R    C O N F I G U R A T I O N   ****/
  /****                                                                 ****/
  /*************************************************************************/
  /*************************************************************************/

  /**************************************************************************
   *
   * Define `TT_CONFIG_OPTION_BYTECODE_INTERPRETER` if you want to compile a
   * bytecode interpreter in the TrueType driver.
   *
   * By undefining this, you will only compile the code necessary to load
   * TrueType glyphs without hinting.
   *
   * Do not `#undef` this macro here, since the build system might define it
   * for certain configurations only.
   */
#define TT_CONFIG_OPTION_BYTECODE_INTERPRETER

  /**************************************************************************
   *
   * Define `TT_CONFIG_OPTION_SUBPIXEL_HINTING` if you want to compile
   * subpixel hinting support into the TrueType driver.  This modifies the
   * TrueType hinting mechanism when anything but `FT_RENDER_MODE_MONO` is
   * requested.
   *
   * In particular, it modifies the bytecode interpreter to interpret (or
   * not) instructions in a certain way so that all TrueType fonts look like
   * they do in a Windows ClearType (DirectWrite) environment.  See [1] for a
   * technical overview on what this means.  See `ttinterp.h` for more
   * details on this option.
   *
   * The new default mode focuses on applying a minimal set of rules to all
   * fonts indiscriminately so that modern and web fonts render well while
   * legacy fonts render okay.  The corresponding interpreter version is v40.
   * The so-called Infinality mode (v38) is no longer available in FreeType.
   *
   * By undefining these, you get rendering behavior like on Windows without
   * ClearType, i.e., Windows XP without ClearType enabled and Win9x
   * (interpreter version v35).  Or not, depending on how much hinting blood
   * and testing tears the font designer put into a given font.  If you
   * define one or both subpixel hinting options, you can switch between
   * between v35 and the ones you define (using `FT_Property_Set`).
   *
   * This option requires `TT_CONFIG_OPTION_BYTECODE_INTERPRETER` to be
   * defined.
   *
   * [1]
   * https://www.microsoft.com/typography/cleartype/truetypecleartype.aspx
   */
#define TT_CONFIG_OPTION_SUBPIXEL_HINTING

  /**************************************************************************
   *
   * Define `TT_CONFIG_OPTION_COMPONENT_OFFSET_SCALED` to compile the
   * TrueType glyph loader to use Apple's definition of how to handle
   * component offsets in composite glyphs.
   *
   * Apple and MS disagree on the default behavior of component offsets in
   * composites.  Apple says that they should be scaled by the scaling
   * factors in the transformation matrix (roughly, it's more complex) while
   * MS says they should not.  OpenType defines two bits in the composite
   * flags array which can be used to disambiguate, but old fonts will not
   * have them.
   *
   *   https://www.microsoft.com/typography/otspec/glyf.htm
   *   https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6glyf.html
   */
#undef TT_CONFIG_OPTION_COMPONENT_OFFSET_SCALED

  /**************************************************************************
   *
   * Define `TT_CONFIG_OPTION_GX_VAR_SUPPORT` if you want to include support
   * for Apple's distortable font technology ('fvar', 'gvar', 'cvar', and
   * 'avar' tables).  Tagged 'Font Variations', this is now part of OpenType
   * also.  This has many similarities to Type~1 Multiple Masters support.
   */
#define TT_CONFIG_OPTION_GX_VAR_SUPPORT

  /**************************************************************************
   *
   * Define `TT_CONFIG_OPTION_NO_BORING_EXPANSION` if you want to exclude
   * support for 'boring' OpenType specification expansions.
   *
   *   https://github.com/harfbuzz/boring-expansion-spec
   *
   * Right now, the following features are covered:
   *
   *   - 'avar' version 2.0
   *
   * Most likely, this is a temporary configuration option to be removed in
   * the near future, since it is assumed that eventually those features are
   * added to the OpenType standard.
   */
/* #define TT_CONFIG_OPTION_NO_BORING_EXPANSION */

  /**************************************************************************
   *
   * Define `TT_CONFIG_OPTION_BDF` if you want to include support for an
   * embedded 'BDF~' table within SFNT-based bitmap formats.
   */
#define TT_CONFIG_OPTION_BDF

  /**************************************************************************
   *
   * Option `TT_CONFIG_OPTION_MAX_RUNNABLE_OPCODES` controls the maximum
   * number of bytecode instructions executed for a single run of the
   * bytecode interpreter, needed to prevent infinite loops.  You don't want
   * to change this except for very special situations (e.g., making a
   * library fuzzer spend less time to handle broken fonts).
   *
   * It is not expected that this value is ever modified by a configuring
   * script; instead, it gets surrounded with `#ifndef ... #endif` so that
   * the value can be set as a preprocessor option on the compiler's command
   * line.
   */
#ifndef TT_CONFIG_OPTION_MAX_RUNNABLE_OPCODES
#define TT_CONFIG_OPTION_MAX_RUNNABLE_OPCODES  1000000L
#endif

  /**************************************************************************
   *
   * Option `TT_CONFIG_OPTION_GPOS_KERNING` enables a basic GPOS kerning
   * implementation (for TrueType fonts only).  With this defined, FreeType
   * is able to get kerning pair data from the GPOS 'kern' feature as well as
   * legacy 'kern' tables; without this defined, FreeType will only be able
   * to use legacy 'kern' tables.
   *
   * Note that FreeType does not support more advanced GPOS layout features;
   * even the 'kern' feature implemented here doesn't handle more
   * sophisticated kerning variants.  Use a higher-level library like
   * HarfBuzz instead for that.
   */
/* #define TT_CONFIG_OPTION_GPOS_KERNING */

  /*************************************************************************/
  /*************************************************************************/
  /****                                                                 ****/
  /****      T Y P E 1   D R I V E R    C O N F I G U R A T I O N       ****/
  /****                                                                 ****/
  /*************************************************************************/
  /*************************************************************************/

  /**************************************************************************
   *
   * `T1_MAX_DICT_DEPTH` is the maximum depth of nest dictionaries and arrays
   * in the Type~1 stream (see `t1load.c`).  A minimum of~4 is required.
   */
#define T1_MAX_DICT_DEPTH  5

  /**************************************************************************
   *
   * `T1_MAX_SUBRS_CALLS` details the maximum number of nested sub-routine
   * calls during glyph loading.
   */
#define T1_MAX_SUBRS_CALLS  16

  /**************************************************************************
   *
   * `T1_MAX_CHARSTRING_OPERANDS` is the charstring stack's capacity.  A
   * minimum of~16 is required.
   *
   * The Chinese font 'MingTiEG-Medium' (covering the CNS 11643 character
   * set) needs 256.
   */
#define T1_MAX_CHARSTRINGS_OPERANDS  256

  /**************************************************************************
   *
   * Define this configuration macro if you want to prevent the compilation
   * of the 't1afm' module, which is in charge of reading Type~1 AFM files
   * into an existing face.  Note that if set, the Type~1 driver will be
   * unable to produce kerning distances.
   */
#undef T1_CONFIG_OPTION_NO_AFM

  /**************************************************************************
   *
   * Define this configuration macro if you want to prevent the compilation
   * of the Multiple Masters font support in the Type~1 driver.
   */
#undef T1_CONFIG_OPTION_NO_MM_SUPPORT

  /**************************************************************************
   *
   * `T1_CONFIG_OPTION_OLD_ENGINE` controls whether the pre-Adobe Type~1
   * engine gets compiled into FreeType.  If defined, it is possible to
   * switch between the two engines using the `hinting-engine` property of
   * the 'type1' driver module.
   */
/* #define T1_CONFIG_OPTION_OLD_ENGINE */

  /*************************************************************************/
  /*************************************************************************/
  /****                                                                 ****/
  /****         C F F   D R I V E R    C O N F I G U R A T I O N        ****/
  /****                                                                 ****/
  /*************************************************************************/
  /*************************************************************************/

  /**************************************************************************
   *
   * Using `CFF_CONFIG_OPTION_DARKENING_PARAMETER_{X,Y}{1,2,3,4}` it is
   * possible to set up the default values of the four control points that
   * define the stem darkening behaviour of the (new) CFF engine.  For more
   * details please read the documentation of the `darkening-parameters`
   * property (file `ftdriver.h`), which allows the control at run-time.
   *
   * Do **not** undefine these macros!
   */
#define CFF_CONFIG_OPTION_DARKENING_PARAMETER_X1   500
#define CFF_CONFIG_OPTION_DARKENING_PARAMETER_Y1   400

#define CFF_CONFIG_OPTION_DARKENING_PARAMETER_X2  1000
#define CFF_CONFIG_OPTION_DARKENING_PARAMETER_Y2   275

#define CFF_CONFIG_OPTION_DARKENING_PARAMETER_X3  1667
#define CFF_CONFIG_OPTION_DARKENING_PARAMETER_Y3   275

#define CFF_CONFIG_OPTION_DARKENING_PARAMETER_X4  2333
#define CFF_CONFIG_OPTION_DARKENING_PARAMETER_Y4     0

  /**************************************************************************
   *
   * `CFF_CONFIG_OPTION_OLD_ENGINE` controls whether the pre-Adobe CFF engine
   * gets compiled into FreeType.  If defined, it is possible to switch
   * between the two engines using the `hinting-engine` property of the 'cff'
   * driver module.
   */
/* #define CFF_CONFIG_OPTION_OLD_ENGINE */

  /*************************************************************************/
  /*************************************************************************/
  /****                                                                 ****/
  /****         P C F   D R I V E R    C O N F I G U R A T I O N        ****/
  /****                                                                 ****/
  /*************************************************************************/
  /*************************************************************************/

  /**************************************************************************
   *
   * There are many PCF fonts just called 'Fixed' which look completely
   * different, and which have nothing to do with each other.  When selecting
   * 'Fixed' in KDE or Gnome one gets results that appear rather random, the
   * style changes often if one changes the size and one cannot select some
   * fonts at all.  This option makes the 'pcf' module prepend the foundry
   * name (plus a space) to the family name.
   *
   * We also check whether we have 'wide' characters; all put together, we
   * get family names like 'Sony Fixed' or 'Misc Fixed Wide'.
   *
   * If this option is activated, it can be controlled with the
   * `no-long-family-names` property of the 'pcf' driver module.
   */
/* #define PCF_CONFIG_OPTION_LONG_FAMILY_NAMES */

  /*************************************************************************/
  /*************************************************************************/
  /****                                                                 ****/
  /****    A U T O F I T   M O D U L E    C O N F I G U R A T I O N     ****/
  /****                                                                 ****/
  /*************************************************************************/
  /*************************************************************************/

  /**************************************************************************
   *
   * Compile 'autofit' module with CJK (Chinese, Japanese, Korean) script
   * support.
   */
#define AF_CONFIG_OPTION_CJK

  /**************************************************************************
   *
   * Compile 'autofit' module with fallback Indic script support, covering
   * some scripts that the 'latin' submodule of the 'autofit' module doesn't
   * (yet) handle.  Currently, this needs option `AF_CONFIG_OPTION_CJK`.
   */
#ifdef AF_CONFIG_OPTION_CJK
#define AF_CONFIG_OPTION_INDIC
#endif

  /**************************************************************************
   *
   * Use TrueType-like size metrics for 'light' auto-hinting.
   *
   * It is strongly recommended to avoid this option, which exists only to
   * help some legacy applications retain its appearance and behaviour with
   * respect to auto-hinted TrueType fonts.
   *
   * The very reason this option exists at all are GNU/Linux distributions
   * like Fedora that did not un-patch the following change (which was
   * present in FreeType between versions 2.4.6 and 2.7.1, inclusive).
   *
   * ```
   *   2011-07-16  Steven Chu  <steven.f.chu@gmail.com>
   *
   *     [truetype] Fix metrics on size request for scalable fonts.
   * ```
   *
   * This problematic commit is now reverted (more or less).
   */
/* #define AF_CONFIG_OPTION_TT_SIZE_METRICS */

  /* */

  /*
   * This macro is obsolete.  Support has been removed in FreeType version
   * 2.5.
   */
/* #define FT_CONFIG_OPTION_OLD_INTERNALS */

  /*
   * The next two macros are defined if native TrueType hinting is
   * requested by the definitions above.  Don't change this.
   */
#ifdef TT_CONFIG_OPTION_BYTECODE_INTERPRETER
#define  TT_USE_BYTECODE_INTERPRETER
#ifdef TT_CONFIG_OPTION_SUBPIXEL_HINTING
#define  TT_SUPPORT_SUBPIXEL_HINTING_MINIMAL
#endif
#endif

  /*
   * The TT_SUPPORT_COLRV1 macro is defined to indicate to clients that this
   * version of FreeType has support for 'COLR' v1 API.  This definition is
   * useful to FreeType clients that want to build in support for 'COLR' v1
   * depending on a tip-of-tree checkout before it is officially released in
   * FreeType, and while the feature cannot yet be tested against using
   * version macros.  Don't change this macro.  This may be removed once the
   * feature is in a FreeType release version and version macros can be used
   * to test for availability.
   */
#ifdef TT_CONFIG_OPTION_COLOR_LAYERS
#define  TT_SUPPORT_COLRV1
#endif

  /*
   * Check CFF darkening parameters.  The checks are the same as in function
   * `cff_property_set` in file `cffdrivr.c`.
   */
#if CFF_CONFIG_OPTION_DARKENING_PARAMETER_X1 < 0   || \
	CFF_CONFIG_OPTION_DARKENING_PARAMETER_X2 < 0   || \
	CFF_CONFIG_OPTION_DARKENING_PARAMETER_X3 < 0   || \
	CFF_CONFIG_OPTION_DARKENING_PARAMETER_X4 < 0   || \
													  \
	CFF_CONFIG_OPTION_DARKENING_PARAMETER_Y1 < 0   || \
	CFF_CONFIG_OPTION_DARKENING_PARAMETER_Y2 < 0   || \
	CFF_CONFIG_OPTION_DARKENING_PARAMETER_Y3 < 0   || \
	CFF_CONFIG_OPTION_DARKENING_PARAMETER_Y4 < 0   || \
													  \
	CFF_CONFIG_OPTION_DARKENING_PARAMETER_X1 >        \
	  CFF_CONFIG_OPTION_DARKENING_PARAMETER_X2     || \
	CFF_CONFIG_OPTION_DARKENING_PARAMETER_X2 >        \
	  CFF_CONFIG_OPTION_DARKENING_PARAMETER_X3     || \
	CFF_CONFIG_OPTION_DARKENING_PARAMETER_X3 >        \
	  CFF_CONFIG_OPTION_DARKENING_PARAMETER_X4     || \
													  \
	CFF_CONFIG_OPTION_DARKENING_PARAMETER_Y1 > 500 || \
	CFF_CONFIG_OPTION_DARKENING_PARAMETER_Y2 > 500 || \
	CFF_CONFIG_OPTION_DARKENING_PARAMETER_Y3 > 500 || \
	CFF_CONFIG_OPTION_DARKENING_PARAMETER_Y4 > 500
#error "Invalid CFF darkening parameters!"
#endif

FT_END_HEADER

#endif /* FTOPTION_H_ */

/* END */

/*** End of inlined file: ftoption.h ***/


/*** Start of inlined file: ftstdlib.h ***/
  /**************************************************************************
   *
   * This file is used to group all `#includes` to the ANSI~C library that
   * FreeType normally requires.  It also defines macros to rename the
   * standard functions within the FreeType source code.
   *
   * Load a file which defines `FTSTDLIB_H_` before this one to override it.
   *
   */

#ifndef FTSTDLIB_H_
#define FTSTDLIB_H_

#include <stddef.h>

#define ft_ptrdiff_t  ptrdiff_t

  /**************************************************************************
   *
   *                          integer limits
   *
   * `UINT_MAX` and `ULONG_MAX` are used to automatically compute the size of
   * `int` and `long` in bytes at compile-time.  So far, this works for all
   * platforms the library has been tested on.  We also check `ULLONG_MAX`
   * to see whether we can use 64-bit `long long` later on.
   *
   * Note that on the extremely rare platforms that do not provide integer
   * types that are _exactly_ 16 and 32~bits wide (e.g., some old Crays where
   * `int` is 36~bits), we do not make any guarantee about the correct
   * behaviour of FreeType~2 with all fonts.
   *
   * In these cases, `ftconfig.h` will refuse to compile anyway with a
   * message like 'couldn't find 32-bit type' or something similar.
   *
   */

#include <limits.h>

#define FT_CHAR_BIT    CHAR_BIT
#define FT_USHORT_MAX  USHRT_MAX
#define FT_INT_MAX     INT_MAX
#define FT_INT_MIN     INT_MIN
#define FT_UINT_MAX    UINT_MAX
#define FT_LONG_MIN    LONG_MIN
#define FT_LONG_MAX    LONG_MAX
#define FT_ULONG_MAX   ULONG_MAX
#ifdef LLONG_MAX
#define FT_LLONG_MAX   LLONG_MAX
#endif
#ifdef LLONG_MIN
#define FT_LLONG_MIN   LLONG_MIN
#endif
#ifdef ULLONG_MAX
#define FT_ULLONG_MAX  ULLONG_MAX
#endif

  /**************************************************************************
   *
   *                character and string processing
   *
   */

#include <string.h>

#define ft_memchr   memchr
#define ft_memcmp   memcmp
#define ft_memcpy   memcpy
#define ft_memmove  memmove
#define ft_memset   memset
#define ft_strcat   strcat
#define ft_strcmp   strcmp
#define ft_strcpy   strcpy
#define ft_strlen   strlen
#define ft_strncmp  strncmp
#define ft_strncpy  strncpy
#define ft_strrchr  strrchr
#define ft_strstr   strstr

  /**************************************************************************
   *
   *                          file handling
   *
   */

#include <stdio.h>

#define FT_FILE      FILE
#define ft_fclose    fclose
#define ft_fopen     fopen
#define ft_fread     fread
#define ft_fseek     fseek
#define ft_ftell     ftell
#define ft_snprintf  snprintf

  /**************************************************************************
   *
   *                            sorting
   *
   */

#include <stdlib.h>

#define ft_qsort  qsort

  /**************************************************************************
   *
   *                       memory allocation
   *
   */

#define ft_scalloc   calloc
#define ft_sfree     free
#define ft_smalloc   malloc
#define ft_srealloc  realloc

  /**************************************************************************
   *
   *                         miscellaneous
   *
   */

#define ft_strtol  strtol
#define ft_getenv  getenv

  /**************************************************************************
   *
   *                        execution control
   *
   */

#include <setjmp.h>

#define ft_jmp_buf     jmp_buf  /* note: this cannot be a typedef since  */
								/*       `jmp_buf` is defined as a macro */
								/*       on certain platforms            */

#define ft_longjmp     longjmp
#define ft_setjmp( b ) setjmp( *(ft_jmp_buf*) &(b) ) /* same thing here */

  /* The following is only used for debugging purposes, i.e., if   */
  /* `FT_DEBUG_LEVEL_ERROR` or `FT_DEBUG_LEVEL_TRACE` are defined. */

#include <stdarg.h>

#endif /* FTSTDLIB_H_ */

/* END */

/*** End of inlined file: ftstdlib.h ***/


/*** Start of inlined file: integer-types.h ***/
#ifndef FREETYPE_CONFIG_INTEGER_TYPES_H_
#define FREETYPE_CONFIG_INTEGER_TYPES_H_

  /* There are systems (like the Texas Instruments 'C54x) where a `char`  */
  /* has 16~bits.  ANSI~C says that `sizeof(char)` is always~1.  Since an */
  /* `int` has 16~bits also for this system, `sizeof(int)` gives~1 which  */
  /* is probably unexpected.                                              */
  /*                                                                      */
  /* `CHAR_BIT` (defined in `limits.h`) gives the number of bits in a     */
  /* `char` type.                                                         */

#ifndef FT_CHAR_BIT
#define FT_CHAR_BIT  CHAR_BIT
#endif

#ifndef FT_SIZEOF_INT

  /* The size of an `int` type. */
#if                                 FT_UINT_MAX == 0xFFFFUL
#define FT_SIZEOF_INT  ( 16 / FT_CHAR_BIT )
#elif                               FT_UINT_MAX == 0xFFFFFFFFUL
#define FT_SIZEOF_INT  ( 32 / FT_CHAR_BIT )
#elif FT_UINT_MAX > 0xFFFFFFFFUL && FT_UINT_MAX == 0xFFFFFFFFFFFFFFFFUL
#define FT_SIZEOF_INT  ( 64 / FT_CHAR_BIT )
#else
#error "Unsupported size of `int' type!"
#endif

#endif  /* !defined(FT_SIZEOF_INT) */

#ifndef FT_SIZEOF_LONG

  /* The size of a `long` type.  A five-byte `long` (as used e.g. on the */
  /* DM642) is recognized but avoided.                                   */
#if                                  FT_ULONG_MAX == 0xFFFFFFFFUL
#define FT_SIZEOF_LONG  ( 32 / FT_CHAR_BIT )
#elif FT_ULONG_MAX > 0xFFFFFFFFUL && FT_ULONG_MAX == 0xFFFFFFFFFFUL
#define FT_SIZEOF_LONG  ( 32 / FT_CHAR_BIT )
#elif FT_ULONG_MAX > 0xFFFFFFFFUL && FT_ULONG_MAX == 0xFFFFFFFFFFFFFFFFUL
#define FT_SIZEOF_LONG  ( 64 / FT_CHAR_BIT )
#else
#error "Unsupported size of `long' type!"
#endif

#endif /* !defined(FT_SIZEOF_LONG) */

#ifndef FT_SIZEOF_LONG_LONG

  /* The size of a `long long` type if available */
#if defined( FT_ULLONG_MAX ) && FT_ULLONG_MAX >= 0xFFFFFFFFFFFFFFFFULL
#define FT_SIZEOF_LONG_LONG  ( 64 / FT_CHAR_BIT )
#else
#define FT_SIZEOF_LONG_LONG  0
#endif

#endif /* !defined(FT_SIZEOF_LONG_LONG) */

  /**************************************************************************
   *
   * @section:
   *   basic_types
   *
   */

  /**************************************************************************
   *
   * @type:
   *   FT_Int16
   *
   * @description:
   *   A typedef for a 16bit signed integer type.
   */
  typedef signed short  FT_Int16;

  /**************************************************************************
   *
   * @type:
   *   FT_UInt16
   *
   * @description:
   *   A typedef for a 16bit unsigned integer type.
   */
  typedef unsigned short  FT_UInt16;

  /* */

  /* this #if 0 ... #endif clause is for documentation purposes */
#if 0

  /**************************************************************************
   *
   * @type:
   *   FT_Int32
   *
   * @description:
   *   A typedef for a 32bit signed integer type.  The size depends on the
   *   configuration.
   */
  typedef signed XXX  FT_Int32;

  /**************************************************************************
   *
   * @type:
   *   FT_UInt32
   *
   *   A typedef for a 32bit unsigned integer type.  The size depends on the
   *   configuration.
   */
  typedef unsigned XXX  FT_UInt32;

  /**************************************************************************
   *
   * @type:
   *   FT_Int64
   *
   *   A typedef for a 64bit signed integer type.  The size depends on the
   *   configuration.  Only defined if there is real 64bit support;
   *   otherwise, it gets emulated with a structure (if necessary).
   */
  typedef signed XXX  FT_Int64;

  /**************************************************************************
   *
   * @type:
   *   FT_UInt64
   *
   *   A typedef for a 64bit unsigned integer type.  The size depends on the
   *   configuration.  Only defined if there is real 64bit support;
   *   otherwise, it gets emulated with a structure (if necessary).
   */
  typedef unsigned XXX  FT_UInt64;

  /* */

#endif

#if FT_SIZEOF_INT == ( 32 / FT_CHAR_BIT )

  typedef signed int      FT_Int32;
  typedef unsigned int    FT_UInt32;

#elif FT_SIZEOF_LONG == ( 32 / FT_CHAR_BIT )

  typedef signed long     FT_Int32;
  typedef unsigned long   FT_UInt32;

#else
#error "no 32bit type found -- please check your configuration files"
#endif

  /* look up an integer type that is at least 32~bits */
#if FT_SIZEOF_INT >= ( 32 / FT_CHAR_BIT )

  typedef int            FT_Fast;
  typedef unsigned int   FT_UFast;

#elif FT_SIZEOF_LONG >= ( 32 / FT_CHAR_BIT )

  typedef long           FT_Fast;
  typedef unsigned long  FT_UFast;

#endif

  /* determine whether we have a 64-bit integer type */
#if FT_SIZEOF_LONG == ( 64 / FT_CHAR_BIT )

#define FT_INT64   long
#define FT_UINT64  unsigned long

#elif FT_SIZEOF_LONG_LONG >= ( 64 / FT_CHAR_BIT )

#define FT_INT64   long long int
#define FT_UINT64  unsigned long long int

  /**************************************************************************
   *
   * A 64-bit data type may create compilation problems if you compile in
   * strict ANSI mode.  To avoid them, we disable other 64-bit data types if
   * `__STDC__` is defined.  You can however ignore this rule by defining the
   * `FT_CONFIG_OPTION_FORCE_INT64` configuration macro.
   */
#elif !defined( __STDC__ ) || defined( FT_CONFIG_OPTION_FORCE_INT64 )

#if defined( _MSC_VER ) && _MSC_VER >= 900 /* Visual C++ (and Intel C++) */

  /* this compiler provides the `__int64` type */
#define FT_INT64   __int64
#define FT_UINT64  unsigned __int64

#elif defined( __BORLANDC__ )  /* Borland C++ */

  /* XXXX: We should probably check the value of `__BORLANDC__` in order */
  /*       to test the compiler version.                                 */

  /* this compiler provides the `__int64` type */
#define FT_INT64   __int64
#define FT_UINT64  unsigned __int64

#elif defined( __WATCOMC__ ) && __WATCOMC__ >= 1100  /* Watcom C++ */

#define FT_INT64   long long int
#define FT_UINT64  unsigned long long int

#elif defined( __MWERKS__ )    /* Metrowerks CodeWarrior */

#define FT_INT64   long long int
#define FT_UINT64  unsigned long long int

#elif defined( __GNUC__ )

  /* GCC provides the `long long` type */
#define FT_INT64   long long int
#define FT_UINT64  unsigned long long int

#endif /* !__STDC__ */

#endif /* FT_SIZEOF_LONG == (64 / FT_CHAR_BIT) */

#ifdef FT_INT64
  typedef FT_INT64   FT_Int64;
  typedef FT_UINT64  FT_UInt64;
#endif

#endif  /* FREETYPE_CONFIG_INTEGER_TYPES_H_ */

/*** End of inlined file: integer-types.h ***/


/*** Start of inlined file: public-macros.h ***/
  /*
   * The definitions in this file are used by the public FreeType headers
   * and thus should be considered part of the public API.
   *
   * Other compiler-specific macro definitions that are not exposed by the
   * FreeType API should go into
   * `include/freetype/internal/compiler-macros.h` instead.
   */
#ifndef FREETYPE_CONFIG_PUBLIC_MACROS_H_
#define FREETYPE_CONFIG_PUBLIC_MACROS_H_

  /*
   * `FT_BEGIN_HEADER` and `FT_END_HEADER` might have already been defined
   * by `freetype/config/ftheader.h`, but we don't want to include this
   * header here, so redefine the macros here only when needed.  Their
   * definition is very stable, so keeping them in sync with the ones in the
   * header should not be a maintenance issue.
   */
#ifndef FT_BEGIN_HEADER
#ifdef __cplusplus
#define FT_BEGIN_HEADER  extern "C" {
#else
#define FT_BEGIN_HEADER  /* empty */
#endif
#endif  /* FT_BEGIN_HEADER */

#ifndef FT_END_HEADER
#ifdef __cplusplus
#define FT_END_HEADER  }
#else
#define FT_END_HEADER  /* empty */
#endif
#endif  /* FT_END_HEADER */

FT_BEGIN_HEADER

  /*
   * Mark a function declaration as public.  This ensures it will be
   * properly exported to client code.  Place this before a function
   * declaration.
   *
   * NOTE: This macro should be considered an internal implementation
   * detail, and not part of the FreeType API.  It is only defined here
   * because it is needed by `FT_EXPORT`.
   */

  /* Visual C, mingw */
#if defined( _WIN32 )

#if defined( FT2_BUILD_LIBRARY ) && defined( DLL_EXPORT )
#define FT_PUBLIC_FUNCTION_ATTRIBUTE  __declspec( dllexport )
#elif defined( DLL_IMPORT )
#define FT_PUBLIC_FUNCTION_ATTRIBUTE  __declspec( dllimport )
#endif

  /* gcc, clang */
#elif ( defined( __GNUC__ ) && __GNUC__ >= 4 ) || defined( __clang__ )
#define FT_PUBLIC_FUNCTION_ATTRIBUTE \
		  __attribute__(( visibility( "default" ) ))

  /* Sun */
#elif defined( __SUNPRO_C ) && __SUNPRO_C >= 0x550
#define FT_PUBLIC_FUNCTION_ATTRIBUTE  __global
#endif

#ifndef FT_PUBLIC_FUNCTION_ATTRIBUTE
#define FT_PUBLIC_FUNCTION_ATTRIBUTE  /* empty */
#endif

  /*
   * Define a public FreeType API function.  This ensures it is properly
   * exported or imported at build time.  The macro parameter is the
   * function's return type as in:
   *
   *   FT_EXPORT( FT_Bool )
   *   FT_Object_Method( FT_Object  obj,
   *                     ... );
   *
   * NOTE: This requires that all `FT_EXPORT` uses are inside
   * `FT_BEGIN_HEADER ... FT_END_HEADER` blocks.  This guarantees that the
   * functions are exported with C linkage, even when the header is included
   * by a C++ source file.
   */
#define FT_EXPORT( x )  FT_PUBLIC_FUNCTION_ATTRIBUTE extern x

  /*
   * `FT_UNUSED` indicates that a given parameter is not used -- this is
   * only used to get rid of unpleasant compiler warnings.
   *
   * Technically, this was not meant to be part of the public API, but some
   * third-party code depends on it.
   */
#ifndef FT_UNUSED
#define FT_UNUSED( arg )  ( (arg) = (arg) )
#endif

  /*
   * Support for casts in both C and C++.
   */
#ifdef __cplusplus
#define FT_STATIC_CAST( type, var )       static_cast<type>(var)
#define FT_REINTERPRET_CAST( type, var )  reinterpret_cast<type>(var)

#define FT_STATIC_BYTE_CAST( type, var )                         \
		  static_cast<type>( static_cast<unsigned char>( var ) )
#else
#define FT_STATIC_CAST( type, var )       (type)(var)
#define FT_REINTERPRET_CAST( type, var )  (type)(var)

#define FT_STATIC_BYTE_CAST( type, var )  (type)(unsigned char)(var)
#endif

FT_END_HEADER

#endif  /* FREETYPE_CONFIG_PUBLIC_MACROS_H_ */

/*** End of inlined file: public-macros.h ***/


/*** Start of inlined file: mac-support.h ***/
#ifndef FREETYPE_CONFIG_MAC_SUPPORT_H_
#define FREETYPE_CONFIG_MAC_SUPPORT_H_

  /**************************************************************************
   *
   * Mac support
   *
   *   This is the only necessary change, so it is defined here instead
   *   providing a new configuration file.
   */
#if defined( __APPLE__ ) || ( defined( __MWERKS__ ) && defined( macintosh ) )
  /* No Carbon frameworks for 64bit 10.4.x.                         */
  /* `AvailabilityMacros.h` is available since Mac OS X 10.2,       */
  /* so guess the system version by maximum errno before inclusion. */
#include <errno.h>
#ifdef ECANCELED /* defined since 10.2 */
#include "AvailabilityMacros.h"
#endif
#if defined( __LP64__ ) && \
	( MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_4 )
#undef FT_MACINTOSH
#endif

#elif defined( __SC__ ) || defined( __MRC__ )
  /* Classic MacOS compilers */
#include "ConditionalMacros.h"
#if TARGET_OS_MAC
#define FT_MACINTOSH 1
#endif

#endif  /* Mac support */

#endif  /* FREETYPE_CONFIG_MAC_SUPPORT_H_ */

/*** End of inlined file: mac-support.h ***/

#endif /* FTCONFIG_H_ */

/* END */

/*** End of inlined file: ftconfig.h ***/


/*** Start of inlined file: fttypes.h ***/
#ifndef FTTYPES_H_
#define FTTYPES_H_


/*** Start of inlined file: ftsystem.h ***/
#ifndef FTSYSTEM_H_
#define FTSYSTEM_H_

FT_BEGIN_HEADER

  /**************************************************************************
   *
   * @section:
   *  system_interface
   *
   * @title:
   *  System Interface
   *
   * @abstract:
   *  How FreeType manages memory and i/o.
   *
   * @description:
   *  This section contains various definitions related to memory management
   *  and i/o access.  You need to understand this information if you want to
   *  use a custom memory manager or you own i/o streams.
   *
   */

  /**************************************************************************
   *
   *                 M E M O R Y   M A N A G E M E N T
   *
   */

  /**************************************************************************
   *
   * @type:
   *   FT_Memory
   *
   * @description:
   *   A handle to a given memory manager object, defined with an
   *   @FT_MemoryRec structure.
   *
   */
  typedef struct FT_MemoryRec_*  FT_Memory;

  /**************************************************************************
   *
   * @functype:
   *   FT_Alloc_Func
   *
   * @description:
   *   A function used to allocate `size` bytes from `memory`.
   *
   * @input:
   *   memory ::
   *     A handle to the source memory manager.
   *
   *   size ::
   *     The size in bytes to allocate.
   *
   * @return:
   *   Address of new memory block.  0~in case of failure.
   *
   */
  typedef void*
  (*FT_Alloc_Func)( FT_Memory  memory,
					long       size );

  /**************************************************************************
   *
   * @functype:
   *   FT_Free_Func
   *
   * @description:
   *   A function used to release a given block of memory.
   *
   * @input:
   *   memory ::
   *     A handle to the source memory manager.
   *
   *   block ::
   *     The address of the target memory block.
   *
   */
  typedef void
  (*FT_Free_Func)( FT_Memory  memory,
				   void*      block );

  /**************************************************************************
   *
   * @functype:
   *   FT_Realloc_Func
   *
   * @description:
   *   A function used to re-allocate a given block of memory.
   *
   * @input:
   *   memory ::
   *     A handle to the source memory manager.
   *
   *   cur_size ::
   *     The block's current size in bytes.
   *
   *   new_size ::
   *     The block's requested new size.
   *
   *   block ::
   *     The block's current address.
   *
   * @return:
   *   New block address.  0~in case of memory shortage.
   *
   * @note:
   *   In case of error, the old block must still be available.
   *
   */
  typedef void*
  (*FT_Realloc_Func)( FT_Memory  memory,
					  long       cur_size,
					  long       new_size,
					  void*      block );

  /**************************************************************************
   *
   * @struct:
   *   FT_MemoryRec
   *
   * @description:
   *   A structure used to describe a given memory manager to FreeType~2.
   *
   * @fields:
   *   user ::
   *     A generic typeless pointer for user data.
   *
   *   alloc ::
   *     A pointer type to an allocation function.
   *
   *   free ::
   *     A pointer type to an memory freeing function.
   *
   *   realloc ::
   *     A pointer type to a reallocation function.
   *
   */
  struct  FT_MemoryRec_
  {
	void*            user;
	FT_Alloc_Func    alloc;
	FT_Free_Func     free;
	FT_Realloc_Func  realloc;
  };

  /**************************************************************************
   *
   *                      I / O   M A N A G E M E N T
   *
   */

  /**************************************************************************
   *
   * @type:
   *   FT_Stream
   *
   * @description:
   *   A handle to an input stream.
   *
   * @also:
   *   See @FT_StreamRec for the publicly accessible fields of a given stream
   *   object.
   *
   */
  typedef struct FT_StreamRec_*  FT_Stream;

  /**************************************************************************
   *
   * @struct:
   *   FT_StreamDesc
   *
   * @description:
   *   A union type used to store either a long or a pointer.  This is used
   *   to store a file descriptor or a `FILE*` in an input stream.
   *
   */
  typedef union  FT_StreamDesc_
  {
	long   value;
	void*  pointer;

  } FT_StreamDesc;

  /**************************************************************************
   *
   * @functype:
   *   FT_Stream_IoFunc
   *
   * @description:
   *   A function used to seek and read data from a given input stream.
   *
   * @input:
   *   stream ::
   *     A handle to the source stream.
   *
   *   offset ::
   *     The offset from the start of the stream to seek to.
   *
   *   buffer ::
   *     The address of the read buffer.
   *
   *   count ::
   *     The number of bytes to read from the stream.
   *
   * @return:
   *   If count >~0, return the number of bytes effectively read by the
   *   stream (after seeking to `offset`).  If count ==~0, return the status
   *   of the seek operation (non-zero indicates an error).
   *
   */
  typedef unsigned long
  (*FT_Stream_IoFunc)( FT_Stream       stream,
					   unsigned long   offset,
					   unsigned char*  buffer,
					   unsigned long   count );

  /**************************************************************************
   *
   * @functype:
   *   FT_Stream_CloseFunc
   *
   * @description:
   *   A function used to close a given input stream.
   *
   * @input:
   *  stream ::
   *    A handle to the target stream.
   *
   */
  typedef void
  (*FT_Stream_CloseFunc)( FT_Stream  stream );

  /**************************************************************************
   *
   * @struct:
   *   FT_StreamRec
   *
   * @description:
   *   A structure used to describe an input stream.
   *
   * @input:
   *   base ::
   *     For memory-based streams, this is the address of the first stream
   *     byte in memory.  This field should always be set to `NULL` for
   *     disk-based streams.
   *
   *   size ::
   *     The stream size in bytes.
   *
   *     In case of compressed streams where the size is unknown before
   *     actually doing the decompression, the value is set to 0x7FFFFFFF.
   *     (Note that this size value can occur for normal streams also; it is
   *     thus just a hint.)
   *
   *   pos ::
   *     The current position within the stream.
   *
   *   descriptor ::
   *     This field is a union that can hold an integer or a pointer.  It is
   *     used by stream implementations to store file descriptors or `FILE*`
   *     pointers.
   *
   *   pathname ::
   *     This field is completely ignored by FreeType.  However, it is often
   *     useful during debugging to use it to store the stream's filename
   *     (where available).
   *
   *   read ::
   *     The stream's input function.
   *
   *   close ::
   *     The stream's close function.
   *
   *   memory ::
   *     The memory manager to use to preload frames.  This is set internally
   *     by FreeType and shouldn't be touched by stream implementations.
   *
   *   cursor ::
   *     This field is set and used internally by FreeType when parsing
   *     frames.  In particular, the `FT_GET_XXX` macros use this instead of
   *     the `pos` field.
   *
   *   limit ::
   *     This field is set and used internally by FreeType when parsing
   *     frames.
   *
   */
  typedef struct  FT_StreamRec_
  {
	unsigned char*       base;
	unsigned long        size;
	unsigned long        pos;

	FT_StreamDesc        descriptor;
	FT_StreamDesc        pathname;
	FT_Stream_IoFunc     read;
	FT_Stream_CloseFunc  close;

	FT_Memory            memory;
	unsigned char*       cursor;
	unsigned char*       limit;

  } FT_StreamRec;

  /* */

FT_END_HEADER

#endif /* FTSYSTEM_H_ */

/* END */

/*** End of inlined file: ftsystem.h ***/


/*** Start of inlined file: ftimage.h ***/
  /**************************************************************************
   *
   * Note: A 'raster' is simply a scan-line converter, used to render
   *       `FT_Outline`s into `FT_Bitmap`s.
   *
   */

#ifndef FTIMAGE_H_
#define FTIMAGE_H_

FT_BEGIN_HEADER

  /**************************************************************************
   *
   * @section:
   *   basic_types
   *
   */

  /**************************************************************************
   *
   * @type:
   *   FT_Pos
   *
   * @description:
   *   The type FT_Pos is used to store vectorial coordinates.  Depending on
   *   the context, these can represent distances in integer font units, or
   *   16.16, or 26.6 fixed-point pixel coordinates.
   */
  typedef signed long  FT_Pos;

  /**************************************************************************
   *
   * @struct:
   *   FT_Vector
   *
   * @description:
   *   A simple structure used to store a 2D vector; coordinates are of the
   *   FT_Pos type.
   *
   * @fields:
   *   x ::
   *     The horizontal coordinate.
   *   y ::
   *     The vertical coordinate.
   */
  typedef struct  FT_Vector_
  {
	FT_Pos  x;
	FT_Pos  y;

  } FT_Vector;

  /**************************************************************************
   *
   * @struct:
   *   FT_BBox
   *
   * @description:
   *   A structure used to hold an outline's bounding box, i.e., the
   *   coordinates of its extrema in the horizontal and vertical directions.
   *
   * @fields:
   *   xMin ::
   *     The horizontal minimum (left-most).
   *
   *   yMin ::
   *     The vertical minimum (bottom-most).
   *
   *   xMax ::
   *     The horizontal maximum (right-most).
   *
   *   yMax ::
   *     The vertical maximum (top-most).
   *
   * @note:
   *   The bounding box is specified with the coordinates of the lower left
   *   and the upper right corner.  In PostScript, those values are often
   *   called (llx,lly) and (urx,ury), respectively.
   *
   *   If `yMin` is negative, this value gives the glyph's descender.
   *   Otherwise, the glyph doesn't descend below the baseline.  Similarly,
   *   if `ymax` is positive, this value gives the glyph's ascender.
   *
   *   `xMin` gives the horizontal distance from the glyph's origin to the
   *   left edge of the glyph's bounding box.  If `xMin` is negative, the
   *   glyph extends to the left of the origin.
   */
  typedef struct  FT_BBox_
  {
	FT_Pos  xMin, yMin;
	FT_Pos  xMax, yMax;

  } FT_BBox;

  /**************************************************************************
   *
   * @enum:
   *   FT_Pixel_Mode
   *
   * @description:
   *   An enumeration type used to describe the format of pixels in a given
   *   bitmap.  Note that additional formats may be added in the future.
   *
   * @values:
   *   FT_PIXEL_MODE_NONE ::
   *     Value~0 is reserved.
   *
   *   FT_PIXEL_MODE_MONO ::
   *     A monochrome bitmap, using 1~bit per pixel.  Note that pixels are
   *     stored in most-significant order (MSB), which means that the
   *     left-most pixel in a byte has value 128.
   *
   *   FT_PIXEL_MODE_GRAY ::
   *     An 8-bit bitmap, generally used to represent anti-aliased glyph
   *     images.  Each pixel is stored in one byte.  Note that the number of
   *     'gray' levels is stored in the `num_grays` field of the @FT_Bitmap
   *     structure (it generally is 256).
   *
   *   FT_PIXEL_MODE_GRAY2 ::
   *     A 2-bit per pixel bitmap, used to represent embedded anti-aliased
   *     bitmaps in font files according to the OpenType specification.  We
   *     haven't found a single font using this format, however.
   *
   *   FT_PIXEL_MODE_GRAY4 ::
   *     A 4-bit per pixel bitmap, representing embedded anti-aliased bitmaps
   *     in font files according to the OpenType specification.  We haven't
   *     found a single font using this format, however.
   *
   *   FT_PIXEL_MODE_LCD ::
   *     An 8-bit bitmap, representing RGB or BGR decimated glyph images used
   *     for display on LCD displays; the bitmap is three times wider than
   *     the original glyph image.  See also @FT_RENDER_MODE_LCD.
   *
   *   FT_PIXEL_MODE_LCD_V ::
   *     An 8-bit bitmap, representing RGB or BGR decimated glyph images used
   *     for display on rotated LCD displays; the bitmap is three times
   *     taller than the original glyph image.  See also
   *     @FT_RENDER_MODE_LCD_V.
   *
   *   FT_PIXEL_MODE_BGRA ::
   *     [Since 2.5] An image with four 8-bit channels per pixel,
   *     representing a color image (such as emoticons) with alpha channel.
   *     For each pixel, the format is BGRA, which means, the blue channel
   *     comes first in memory.  The color channels are pre-multiplied and in
   *     the sRGB colorspace.  For example, full red at half-translucent
   *     opacity will be represented as '00,00,80,80', not '00,00,FF,80'.
   *     See also @FT_LOAD_COLOR.
   */
  typedef enum  FT_Pixel_Mode_
  {
	FT_PIXEL_MODE_NONE = 0,
	FT_PIXEL_MODE_MONO,
	FT_PIXEL_MODE_GRAY,
	FT_PIXEL_MODE_GRAY2,
	FT_PIXEL_MODE_GRAY4,
	FT_PIXEL_MODE_LCD,
	FT_PIXEL_MODE_LCD_V,
	FT_PIXEL_MODE_BGRA,

	FT_PIXEL_MODE_MAX      /* do not remove */

  } FT_Pixel_Mode;

  /* these constants are deprecated; use the corresponding `FT_Pixel_Mode` */
  /* values instead.                                                       */
#define ft_pixel_mode_none   FT_PIXEL_MODE_NONE
#define ft_pixel_mode_mono   FT_PIXEL_MODE_MONO
#define ft_pixel_mode_grays  FT_PIXEL_MODE_GRAY
#define ft_pixel_mode_pal2   FT_PIXEL_MODE_GRAY2
#define ft_pixel_mode_pal4   FT_PIXEL_MODE_GRAY4

  /* */

  /* For debugging, the @FT_Pixel_Mode enumeration must stay in sync */
  /* with the `pixel_modes` array in file `ftobjs.c`.                */

  /**************************************************************************
   *
   * @struct:
   *   FT_Bitmap
   *
   * @description:
   *   A structure used to describe a bitmap or pixmap to the raster.  Note
   *   that we now manage pixmaps of various depths through the `pixel_mode`
   *   field.
   *
   * @fields:
   *   rows ::
   *     The number of bitmap rows.
   *
   *   width ::
   *     The number of pixels in bitmap row.
   *
   *   pitch ::
   *     The pitch's absolute value is the number of bytes taken by one
   *     bitmap row, including padding.  However, the pitch is positive when
   *     the bitmap has a 'down' flow, and negative when it has an 'up' flow.
   *     In all cases, the pitch is an offset to add to a bitmap pointer in
   *     order to go down one row.
   *
   *     Note that 'padding' means the alignment of a bitmap to a byte
   *     border, and FreeType functions normally align to the smallest
   *     possible integer value.
   *
   *     For the B/W rasterizer, `pitch` is always an even number.
   *
   *     To change the pitch of a bitmap (say, to make it a multiple of 4),
   *     use @FT_Bitmap_Convert.  Alternatively, you might use callback
   *     functions to directly render to the application's surface; see the
   *     file `example2.cpp` in the tutorial for a demonstration.
   *
   *   buffer ::
   *     A typeless pointer to the bitmap buffer.  This value should be
   *     aligned on 32-bit boundaries in most cases.
   *
   *   num_grays ::
   *     This field is only used with @FT_PIXEL_MODE_GRAY; it gives the
   *     number of gray levels used in the bitmap.
   *
   *   pixel_mode ::
   *     The pixel mode, i.e., how pixel bits are stored.  See @FT_Pixel_Mode
   *     for possible values.
   *
   *   palette_mode ::
   *     This field is intended for paletted pixel modes; it indicates how
   *     the palette is stored.  Not used currently.
   *
   *   palette ::
   *     A typeless pointer to the bitmap palette; this field is intended for
   *     paletted pixel modes.  Not used currently.
   *
   * @note:
   *   `width` and `rows` refer to the *physical* size of the bitmap, not the
   *   *logical* one.  For example, if @FT_Pixel_Mode is set to
   *   `FT_PIXEL_MODE_LCD`, the logical width is a just a third of the
   *   physical one.
   */
  typedef struct  FT_Bitmap_
  {
	unsigned int    rows;
	unsigned int    width;
	int             pitch;
	unsigned char*  buffer;
	unsigned short  num_grays;
	unsigned char   pixel_mode;
	unsigned char   palette_mode;
	void*           palette;

  } FT_Bitmap;

  /**************************************************************************
   *
   * @section:
   *   outline_processing
   *
   */

  /**************************************************************************
   *
   * @struct:
   *   FT_Outline
   *
   * @description:
   *   This structure is used to describe an outline to the scan-line
   *   converter.
   *
   * @fields:
   *   n_contours ::
   *     The number of contours in the outline.
   *
   *   n_points ::
   *     The number of points in the outline.
   *
   *   points ::
   *     A pointer to an array of `n_points` @FT_Vector elements, giving the
   *     outline's point coordinates.
   *
   *   tags ::
   *     A pointer to an array of `n_points` chars, giving each outline
   *     point's type.
   *
   *     If bit~0 is unset, the point is 'off' the curve, i.e., a Bezier
   *     control point, while it is 'on' if set.
   *
   *     Bit~1 is meaningful for 'off' points only.  If set, it indicates a
   *     third-order Bezier arc control point; and a second-order control
   *     point if unset.
   *
   *     If bit~2 is set, bits 5-7 contain the drop-out mode (as defined in
   *     the OpenType specification; the value is the same as the argument to
   *     the 'SCANTYPE' instruction).
   *
   *     Bits 3 and~4 are reserved for internal purposes.
   *
   *   contours ::
   *     An array of `n_contours` shorts, giving the end point of each
   *     contour within the outline.  For example, the first contour is
   *     defined by the points '0' to `contours[0]`, the second one is
   *     defined by the points `contours[0]+1` to `contours[1]`, etc.
   *
   *   flags ::
   *     A set of bit flags used to characterize the outline and give hints
   *     to the scan-converter and hinter on how to convert/grid-fit it.  See
   *     @FT_OUTLINE_XXX.
   *
   * @note:
   *   The B/W rasterizer only checks bit~2 in the `tags` array for the first
   *   point of each contour.  The drop-out mode as given with
   *   @FT_OUTLINE_IGNORE_DROPOUTS, @FT_OUTLINE_SMART_DROPOUTS, and
   *   @FT_OUTLINE_INCLUDE_STUBS in `flags` is then overridden.
   */
  typedef struct  FT_Outline_
  {
	short       n_contours;      /* number of contours in glyph        */
	short       n_points;        /* number of points in the glyph      */

	FT_Vector*  points;          /* the outline's points               */
	char*       tags;            /* the points flags                   */
	short*      contours;        /* the contour end points             */

	int         flags;           /* outline masks                      */

  } FT_Outline;

  /* */

  /* Following limits must be consistent with */
  /* FT_Outline.{n_contours,n_points}         */
#define FT_OUTLINE_CONTOURS_MAX  SHRT_MAX
#define FT_OUTLINE_POINTS_MAX    SHRT_MAX

  /**************************************************************************
   *
   * @enum:
   *   FT_OUTLINE_XXX
   *
   * @description:
   *   A list of bit-field constants used for the flags in an outline's
   *   `flags` field.
   *
   * @values:
   *   FT_OUTLINE_NONE ::
   *     Value~0 is reserved.
   *
   *   FT_OUTLINE_OWNER ::
   *     If set, this flag indicates that the outline's field arrays (i.e.,
   *     `points`, `flags`, and `contours`) are 'owned' by the outline
   *     object, and should thus be freed when it is destroyed.
   *
   *   FT_OUTLINE_EVEN_ODD_FILL ::
   *     By default, outlines are filled using the non-zero winding rule.  If
   *     set to 1, the outline will be filled using the even-odd fill rule
   *     (only works with the smooth rasterizer).
   *
   *   FT_OUTLINE_REVERSE_FILL ::
   *     By default, outside contours of an outline are oriented in
   *     clock-wise direction, as defined in the TrueType specification.
   *     This flag is set if the outline uses the opposite direction
   *     (typically for Type~1 fonts).  This flag is ignored by the scan
   *     converter.
   *
   *   FT_OUTLINE_IGNORE_DROPOUTS ::
   *     By default, the scan converter will try to detect drop-outs in an
   *     outline and correct the glyph bitmap to ensure consistent shape
   *     continuity.  If set, this flag hints the scan-line converter to
   *     ignore such cases.  See below for more information.
   *
   *   FT_OUTLINE_SMART_DROPOUTS ::
   *     Select smart dropout control.  If unset, use simple dropout control.
   *     Ignored if @FT_OUTLINE_IGNORE_DROPOUTS is set.  See below for more
   *     information.
   *
   *   FT_OUTLINE_INCLUDE_STUBS ::
   *     If set, turn pixels on for 'stubs', otherwise exclude them.  Ignored
   *     if @FT_OUTLINE_IGNORE_DROPOUTS is set.  See below for more
   *     information.
   *
   *   FT_OUTLINE_OVERLAP ::
   *     [Since 2.10.3] This flag indicates that this outline contains
   *     overlapping contours and the anti-aliased renderer should perform
   *     oversampling to mitigate possible artifacts.  This flag should _not_
   *     be set for well designed glyphs without overlaps because it quadruples
   *     the rendering time.
   *
   *   FT_OUTLINE_HIGH_PRECISION ::
   *     This flag indicates that the scan-line converter should try to
   *     convert this outline to bitmaps with the highest possible quality.
   *     It is typically set for small character sizes.  Note that this is
   *     only a hint that might be completely ignored by a given
   *     scan-converter.
   *
   *   FT_OUTLINE_SINGLE_PASS ::
   *     This flag is set to force a given scan-converter to only use a
   *     single pass over the outline to render a bitmap glyph image.
   *     Normally, it is set for very large character sizes.  It is only a
   *     hint that might be completely ignored by a given scan-converter.
   *
   * @note:
   *   The flags @FT_OUTLINE_IGNORE_DROPOUTS, @FT_OUTLINE_SMART_DROPOUTS, and
   *   @FT_OUTLINE_INCLUDE_STUBS are ignored by the smooth rasterizer.
   *
   *   There exists a second mechanism to pass the drop-out mode to the B/W
   *   rasterizer; see the `tags` field in @FT_Outline.
   *
   *   Please refer to the description of the 'SCANTYPE' instruction in the
   *   [OpenType specification](https://learn.microsoft.com/en-us/typography/opentype/spec/tt_instructions#scantype)
   *   how simple drop-outs, smart drop-outs, and stubs are defined.
   */
#define FT_OUTLINE_NONE             0x0
#define FT_OUTLINE_OWNER            0x1
#define FT_OUTLINE_EVEN_ODD_FILL    0x2
#define FT_OUTLINE_REVERSE_FILL     0x4
#define FT_OUTLINE_IGNORE_DROPOUTS  0x8
#define FT_OUTLINE_SMART_DROPOUTS   0x10
#define FT_OUTLINE_INCLUDE_STUBS    0x20
#define FT_OUTLINE_OVERLAP          0x40

#define FT_OUTLINE_HIGH_PRECISION   0x100
#define FT_OUTLINE_SINGLE_PASS      0x200

  /* these constants are deprecated; use the corresponding */
  /* `FT_OUTLINE_XXX` values instead                       */
#define ft_outline_none             FT_OUTLINE_NONE
#define ft_outline_owner            FT_OUTLINE_OWNER
#define ft_outline_even_odd_fill    FT_OUTLINE_EVEN_ODD_FILL
#define ft_outline_reverse_fill     FT_OUTLINE_REVERSE_FILL
#define ft_outline_ignore_dropouts  FT_OUTLINE_IGNORE_DROPOUTS
#define ft_outline_high_precision   FT_OUTLINE_HIGH_PRECISION
#define ft_outline_single_pass      FT_OUTLINE_SINGLE_PASS

  /* */

#define FT_CURVE_TAG( flag )  ( flag & 0x03 )

  /* see the `tags` field in `FT_Outline` for a description of the values */
#define FT_CURVE_TAG_ON            0x01
#define FT_CURVE_TAG_CONIC         0x00
#define FT_CURVE_TAG_CUBIC         0x02

#define FT_CURVE_TAG_HAS_SCANMODE  0x04

#define FT_CURVE_TAG_TOUCH_X       0x08  /* reserved for TrueType hinter */
#define FT_CURVE_TAG_TOUCH_Y       0x10  /* reserved for TrueType hinter */

#define FT_CURVE_TAG_TOUCH_BOTH    ( FT_CURVE_TAG_TOUCH_X | \
									 FT_CURVE_TAG_TOUCH_Y )
  /* values 0x20, 0x40, and 0x80 are reserved */

  /* these constants are deprecated; use the corresponding */
  /* `FT_CURVE_TAG_XXX` values instead                     */
#define FT_Curve_Tag_On       FT_CURVE_TAG_ON
#define FT_Curve_Tag_Conic    FT_CURVE_TAG_CONIC
#define FT_Curve_Tag_Cubic    FT_CURVE_TAG_CUBIC
#define FT_Curve_Tag_Touch_X  FT_CURVE_TAG_TOUCH_X
#define FT_Curve_Tag_Touch_Y  FT_CURVE_TAG_TOUCH_Y

  /**************************************************************************
   *
   * @functype:
   *   FT_Outline_MoveToFunc
   *
   * @description:
   *   A function pointer type used to describe the signature of a 'move to'
   *   function during outline walking/decomposition.
   *
   *   A 'move to' is emitted to start a new contour in an outline.
   *
   * @input:
   *   to ::
   *     A pointer to the target point of the 'move to'.
   *
   *   user ::
   *     A typeless pointer, which is passed from the caller of the
   *     decomposition function.
   *
   * @return:
   *   Error code.  0~means success.
   */
  typedef int
  (*FT_Outline_MoveToFunc)( const FT_Vector*  to,
							void*             user );

#define FT_Outline_MoveTo_Func  FT_Outline_MoveToFunc

  /**************************************************************************
   *
   * @functype:
   *   FT_Outline_LineToFunc
   *
   * @description:
   *   A function pointer type used to describe the signature of a 'line to'
   *   function during outline walking/decomposition.
   *
   *   A 'line to' is emitted to indicate a segment in the outline.
   *
   * @input:
   *   to ::
   *     A pointer to the target point of the 'line to'.
   *
   *   user ::
   *     A typeless pointer, which is passed from the caller of the
   *     decomposition function.
   *
   * @return:
   *   Error code.  0~means success.
   */
  typedef int
  (*FT_Outline_LineToFunc)( const FT_Vector*  to,
							void*             user );

#define FT_Outline_LineTo_Func  FT_Outline_LineToFunc

  /**************************************************************************
   *
   * @functype:
   *   FT_Outline_ConicToFunc
   *
   * @description:
   *   A function pointer type used to describe the signature of a 'conic to'
   *   function during outline walking or decomposition.
   *
   *   A 'conic to' is emitted to indicate a second-order Bezier arc in the
   *   outline.
   *
   * @input:
   *   control ::
   *     An intermediate control point between the last position and the new
   *     target in `to`.
   *
   *   to ::
   *     A pointer to the target end point of the conic arc.
   *
   *   user ::
   *     A typeless pointer, which is passed from the caller of the
   *     decomposition function.
   *
   * @return:
   *   Error code.  0~means success.
   */
  typedef int
  (*FT_Outline_ConicToFunc)( const FT_Vector*  control,
							 const FT_Vector*  to,
							 void*             user );

#define FT_Outline_ConicTo_Func  FT_Outline_ConicToFunc

  /**************************************************************************
   *
   * @functype:
   *   FT_Outline_CubicToFunc
   *
   * @description:
   *   A function pointer type used to describe the signature of a 'cubic to'
   *   function during outline walking or decomposition.
   *
   *   A 'cubic to' is emitted to indicate a third-order Bezier arc.
   *
   * @input:
   *   control1 ::
   *     A pointer to the first Bezier control point.
   *
   *   control2 ::
   *     A pointer to the second Bezier control point.
   *
   *   to ::
   *     A pointer to the target end point.
   *
   *   user ::
   *     A typeless pointer, which is passed from the caller of the
   *     decomposition function.
   *
   * @return:
   *   Error code.  0~means success.
   */
  typedef int
  (*FT_Outline_CubicToFunc)( const FT_Vector*  control1,
							 const FT_Vector*  control2,
							 const FT_Vector*  to,
							 void*             user );

#define FT_Outline_CubicTo_Func  FT_Outline_CubicToFunc

  /**************************************************************************
   *
   * @struct:
   *   FT_Outline_Funcs
   *
   * @description:
   *   A structure to hold various function pointers used during outline
   *   decomposition in order to emit segments, conic, and cubic Beziers.
   *
   * @fields:
   *   move_to ::
   *     The 'move to' emitter.
   *
   *   line_to ::
   *     The segment emitter.
   *
   *   conic_to ::
   *     The second-order Bezier arc emitter.
   *
   *   cubic_to ::
   *     The third-order Bezier arc emitter.
   *
   *   shift ::
   *     The shift that is applied to coordinates before they are sent to the
   *     emitter.
   *
   *   delta ::
   *     The delta that is applied to coordinates before they are sent to the
   *     emitter, but after the shift.
   *
   * @note:
   *   The point coordinates sent to the emitters are the transformed version
   *   of the original coordinates (this is important for high accuracy
   *   during scan-conversion).  The transformation is simple:
   *
   *   ```
   *     x' = (x << shift) - delta
   *     y' = (y << shift) - delta
   *   ```
   *
   *   Set the values of `shift` and `delta` to~0 to get the original point
   *   coordinates.
   */
  typedef struct  FT_Outline_Funcs_
  {
	FT_Outline_MoveToFunc   move_to;
	FT_Outline_LineToFunc   line_to;
	FT_Outline_ConicToFunc  conic_to;
	FT_Outline_CubicToFunc  cubic_to;

	int                     shift;
	FT_Pos                  delta;

  } FT_Outline_Funcs;

  /**************************************************************************
   *
   * @section:
   *   basic_types
   *
   */

  /**************************************************************************
   *
   * @macro:
   *   FT_IMAGE_TAG
   *
   * @description:
   *   This macro converts four-letter tags to an unsigned long type.
   *
   * @note:
   *   Since many 16-bit compilers don't like 32-bit enumerations, you should
   *   redefine this macro in case of problems to something like this:
   *
   *   ```
   *     #define FT_IMAGE_TAG( value, _x1, _x2, _x3, _x4 )  value
   *   ```
   *
   *   to get a simple enumeration without assigning special numbers.
   */
#ifndef FT_IMAGE_TAG

#define FT_IMAGE_TAG( value, _x1, _x2, _x3, _x4 )                         \
		  value = ( ( FT_STATIC_BYTE_CAST( unsigned long, _x1 ) << 24 ) | \
					( FT_STATIC_BYTE_CAST( unsigned long, _x2 ) << 16 ) | \
					( FT_STATIC_BYTE_CAST( unsigned long, _x3 ) << 8  ) | \
					  FT_STATIC_BYTE_CAST( unsigned long, _x4 )         )

#endif /* FT_IMAGE_TAG */

  /**************************************************************************
   *
   * @enum:
   *   FT_Glyph_Format
   *
   * @description:
   *   An enumeration type used to describe the format of a given glyph
   *   image.  Note that this version of FreeType only supports two image
   *   formats, even though future font drivers will be able to register
   *   their own format.
   *
   * @values:
   *   FT_GLYPH_FORMAT_NONE ::
   *     The value~0 is reserved.
   *
   *   FT_GLYPH_FORMAT_COMPOSITE ::
   *     The glyph image is a composite of several other images.  This format
   *     is _only_ used with @FT_LOAD_NO_RECURSE, and is used to report
   *     compound glyphs (like accented characters).
   *
   *   FT_GLYPH_FORMAT_BITMAP ::
   *     The glyph image is a bitmap, and can be described as an @FT_Bitmap.
   *     You generally need to access the `bitmap` field of the
   *     @FT_GlyphSlotRec structure to read it.
   *
   *   FT_GLYPH_FORMAT_OUTLINE ::
   *     The glyph image is a vectorial outline made of line segments and
   *     Bezier arcs; it can be described as an @FT_Outline; you generally
   *     want to access the `outline` field of the @FT_GlyphSlotRec structure
   *     to read it.
   *
   *   FT_GLYPH_FORMAT_PLOTTER ::
   *     The glyph image is a vectorial path with no inside and outside
   *     contours.  Some Type~1 fonts, like those in the Hershey family,
   *     contain glyphs in this format.  These are described as @FT_Outline,
   *     but FreeType isn't currently capable of rendering them correctly.
   *
   *   FT_GLYPH_FORMAT_SVG ::
   *     [Since 2.12] The glyph is represented by an SVG document in the
   *     'SVG~' table.
   */
  typedef enum  FT_Glyph_Format_
  {
	FT_IMAGE_TAG( FT_GLYPH_FORMAT_NONE, 0, 0, 0, 0 ),

	FT_IMAGE_TAG( FT_GLYPH_FORMAT_COMPOSITE, 'c', 'o', 'm', 'p' ),
	FT_IMAGE_TAG( FT_GLYPH_FORMAT_BITMAP,    'b', 'i', 't', 's' ),
	FT_IMAGE_TAG( FT_GLYPH_FORMAT_OUTLINE,   'o', 'u', 't', 'l' ),
	FT_IMAGE_TAG( FT_GLYPH_FORMAT_PLOTTER,   'p', 'l', 'o', 't' ),
	FT_IMAGE_TAG( FT_GLYPH_FORMAT_SVG,       'S', 'V', 'G', ' ' )

  } FT_Glyph_Format;

  /* these constants are deprecated; use the corresponding */
  /* `FT_Glyph_Format` values instead.                     */
#define ft_glyph_format_none       FT_GLYPH_FORMAT_NONE
#define ft_glyph_format_composite  FT_GLYPH_FORMAT_COMPOSITE
#define ft_glyph_format_bitmap     FT_GLYPH_FORMAT_BITMAP
#define ft_glyph_format_outline    FT_GLYPH_FORMAT_OUTLINE
#define ft_glyph_format_plotter    FT_GLYPH_FORMAT_PLOTTER

  /*************************************************************************/
  /*************************************************************************/
  /*************************************************************************/
  /*****                                                               *****/
  /*****            R A S T E R   D E F I N I T I O N S                *****/
  /*****                                                               *****/
  /*************************************************************************/
  /*************************************************************************/
  /*************************************************************************/

  /**************************************************************************
   *
   * @section:
   *   raster
   *
   * @title:
   *   Scanline Converter
   *
   * @abstract:
   *   How vectorial outlines are converted into bitmaps and pixmaps.
   *
   * @description:
   *   A raster or a rasterizer is a scan converter in charge of producing a
   *   pixel coverage bitmap that can be used as an alpha channel when
   *   compositing a glyph with a background.  FreeType comes with two
   *   rasterizers: bilevel `raster1` and anti-aliased `smooth` are two
   *   separate modules.  They are usually called from the high-level
   *   @FT_Load_Glyph or @FT_Render_Glyph functions and produce the entire
   *   coverage bitmap at once, while staying largely invisible to users.
   *
   *   Instead of working with complete coverage bitmaps, it is also possible
   *   to intercept consecutive pixel runs on the same scanline with the same
   *   coverage, called _spans_, and process them individually.  Only the
   *   `smooth` rasterizer permits this when calling @FT_Outline_Render with
   *   @FT_Raster_Params as described below.
   *
   *   Working with either complete bitmaps or spans it is important to think
   *   of them as colorless coverage objects suitable as alpha channels to
   *   blend arbitrary colors with a background.  For best results, it is
   *   recommended to use gamma correction, too.
   *
   *   This section also describes the public API needed to set up alternative
   *   @FT_Renderer modules.
   *
   * @order:
   *   FT_Span
   *   FT_SpanFunc
   *   FT_Raster_Params
   *   FT_RASTER_FLAG_XXX
   *
   *   FT_Raster
   *   FT_Raster_NewFunc
   *   FT_Raster_DoneFunc
   *   FT_Raster_ResetFunc
   *   FT_Raster_SetModeFunc
   *   FT_Raster_RenderFunc
   *   FT_Raster_Funcs
   *
   */

  /**************************************************************************
   *
   * @struct:
   *   FT_Span
   *
   * @description:
   *   A structure to model a single span of consecutive pixels when
   *   rendering an anti-aliased bitmap.
   *
   * @fields:
   *   x ::
   *     The span's horizontal start position.
   *
   *   len ::
   *     The span's length in pixels.
   *
   *   coverage ::
   *     The span color/coverage, ranging from 0 (background) to 255
   *     (foreground).
   *
   * @note:
   *   This structure is used by the span drawing callback type named
   *   @FT_SpanFunc that takes the y~coordinate of the span as a parameter.
   *
   *   The anti-aliased rasterizer produces coverage values from 0 to 255,
   *   that is, from completely transparent to completely opaque.
   */
  typedef struct  FT_Span_
  {
	short           x;
	unsigned short  len;
	unsigned char   coverage;

  } FT_Span;

  /**************************************************************************
   *
   * @functype:
   *   FT_SpanFunc
   *
   * @description:
   *   A function used as a call-back by the anti-aliased renderer in order
   *   to let client applications draw themselves the pixel spans on each
   *   scan line.
   *
   * @input:
   *   y ::
   *     The scanline's upward y~coordinate.
   *
   *   count ::
   *     The number of spans to draw on this scanline.
   *
   *   spans ::
   *     A table of `count` spans to draw on the scanline.
   *
   *   user ::
   *     User-supplied data that is passed to the callback.
   *
   * @note:
   *   This callback allows client applications to directly render the spans
   *   of the anti-aliased bitmap to any kind of surfaces.
   *
   *   This can be used to write anti-aliased outlines directly to a given
   *   background bitmap using alpha compositing.  It can also be used for
   *   oversampling and averaging.
   */
  typedef void
  (*FT_SpanFunc)( int             y,
				  int             count,
				  const FT_Span*  spans,
				  void*           user );

#define FT_Raster_Span_Func  FT_SpanFunc

  /**************************************************************************
   *
   * @functype:
   *   FT_Raster_BitTest_Func
   *
   * @description:
   *   Deprecated, unimplemented.
   */
  typedef int
  (*FT_Raster_BitTest_Func)( int    y,
							 int    x,
							 void*  user );

  /**************************************************************************
   *
   * @functype:
   *   FT_Raster_BitSet_Func
   *
   * @description:
   *   Deprecated, unimplemented.
   */
  typedef void
  (*FT_Raster_BitSet_Func)( int    y,
							int    x,
							void*  user );

  /**************************************************************************
   *
   * @enum:
   *   FT_RASTER_FLAG_XXX
   *
   * @description:
   *   A list of bit flag constants as used in the `flags` field of a
   *   @FT_Raster_Params structure.
   *
   * @values:
   *   FT_RASTER_FLAG_DEFAULT ::
   *     This value is 0.
   *
   *   FT_RASTER_FLAG_AA ::
   *     This flag is set to indicate that an anti-aliased glyph image should
   *     be generated.  Otherwise, it will be monochrome (1-bit).
   *
   *   FT_RASTER_FLAG_DIRECT ::
   *     This flag is set to indicate direct rendering.  In this mode, client
   *     applications must provide their own span callback.  This lets them
   *     directly draw or compose over an existing bitmap.  If this bit is
   *     _not_ set, the target pixmap's buffer _must_ be zeroed before
   *     rendering and the output will be clipped to its size.
   *
   *     Direct rendering is only possible with anti-aliased glyphs.
   *
   *   FT_RASTER_FLAG_CLIP ::
   *     This flag is only used in direct rendering mode.  If set, the output
   *     will be clipped to a box specified in the `clip_box` field of the
   *     @FT_Raster_Params structure.  Otherwise, the `clip_box` is
   *     effectively set to the bounding box and all spans are generated.
   *
   *   FT_RASTER_FLAG_SDF ::
   *     This flag is set to indicate that a signed distance field glyph
   *     image should be generated.  This is only used while rendering with
   *     the @FT_RENDER_MODE_SDF render mode.
   */
#define FT_RASTER_FLAG_DEFAULT  0x0
#define FT_RASTER_FLAG_AA       0x1
#define FT_RASTER_FLAG_DIRECT   0x2
#define FT_RASTER_FLAG_CLIP     0x4
#define FT_RASTER_FLAG_SDF      0x8

  /* these constants are deprecated; use the corresponding */
  /* `FT_RASTER_FLAG_XXX` values instead                   */
#define ft_raster_flag_default  FT_RASTER_FLAG_DEFAULT
#define ft_raster_flag_aa       FT_RASTER_FLAG_AA
#define ft_raster_flag_direct   FT_RASTER_FLAG_DIRECT
#define ft_raster_flag_clip     FT_RASTER_FLAG_CLIP

  /**************************************************************************
   *
   * @struct:
   *   FT_Raster_Params
   *
   * @description:
   *   A structure to hold the parameters used by a raster's render function,
   *   passed as an argument to @FT_Outline_Render.
   *
   * @fields:
   *   target ::
   *     The target bitmap.
   *
   *   source ::
   *     A pointer to the source glyph image (e.g., an @FT_Outline).
   *
   *   flags ::
   *     The rendering flags.
   *
   *   gray_spans ::
   *     The gray span drawing callback.
   *
   *   black_spans ::
   *     Unused.
   *
   *   bit_test ::
   *     Unused.
   *
   *   bit_set ::
   *     Unused.
   *
   *   user ::
   *     User-supplied data that is passed to each drawing callback.
   *
   *   clip_box ::
   *     An optional span clipping box expressed in _integer_ pixels
   *     (not in 26.6 fixed-point units).
   *
   * @note:
   *   The @FT_RASTER_FLAG_AA bit flag must be set in the `flags` to
   *   generate an anti-aliased glyph bitmap, otherwise a monochrome bitmap
   *   is generated.  The `target` should have appropriate pixel mode and its
   *   dimensions define the clipping region.
   *
   *   If both @FT_RASTER_FLAG_AA and @FT_RASTER_FLAG_DIRECT bit flags
   *   are set in `flags`, the raster calls an @FT_SpanFunc callback
   *   `gray_spans` with `user` data as an argument ignoring `target`.  This
   *   allows direct composition over a pre-existing user surface to perform
   *   the span drawing and composition.  To optionally clip the spans, set
   *   the @FT_RASTER_FLAG_CLIP flag and `clip_box`.  The monochrome raster
   *   does not support the direct mode.
   *
   *   The gray-level rasterizer always uses 256 gray levels.  If you want
   *   fewer gray levels, you have to use @FT_RASTER_FLAG_DIRECT and reduce
   *   the levels in the callback function.
   */
  typedef struct  FT_Raster_Params_
  {
	const FT_Bitmap*        target;
	const void*             source;
	int                     flags;
	FT_SpanFunc             gray_spans;
	FT_SpanFunc             black_spans;  /* unused */
	FT_Raster_BitTest_Func  bit_test;     /* unused */
	FT_Raster_BitSet_Func   bit_set;      /* unused */
	void*                   user;
	FT_BBox                 clip_box;

  } FT_Raster_Params;

  /**************************************************************************
   *
   * @type:
   *   FT_Raster
   *
   * @description:
   *   An opaque handle (pointer) to a raster object.  Each object can be
   *   used independently to convert an outline into a bitmap or pixmap.
   *
   * @note:
   *   In FreeType 2, all rasters are now encapsulated within specific
   *   @FT_Renderer modules and only used in their context.
   *
   */
  typedef struct FT_RasterRec_*  FT_Raster;

  /**************************************************************************
   *
   * @functype:
   *   FT_Raster_NewFunc
   *
   * @description:
   *   A function used to create a new raster object.
   *
   * @input:
   *   memory ::
   *     A handle to the memory allocator.
   *
   * @output:
   *   raster ::
   *     A handle to the new raster object.
   *
   * @return:
   *   Error code.  0~means success.
   *
   * @note:
   *   The `memory` parameter is a typeless pointer in order to avoid
   *   un-wanted dependencies on the rest of the FreeType code.  In practice,
   *   it is an @FT_Memory object, i.e., a handle to the standard FreeType
   *   memory allocator.  However, this field can be completely ignored by a
   *   given raster implementation.
   */
  typedef int
  (*FT_Raster_NewFunc)( void*       memory,
						FT_Raster*  raster );

#define FT_Raster_New_Func  FT_Raster_NewFunc

  /**************************************************************************
   *
   * @functype:
   *   FT_Raster_DoneFunc
   *
   * @description:
   *   A function used to destroy a given raster object.
   *
   * @input:
   *   raster ::
   *     A handle to the raster object.
   */
  typedef void
  (*FT_Raster_DoneFunc)( FT_Raster  raster );

#define FT_Raster_Done_Func  FT_Raster_DoneFunc

  /**************************************************************************
   *
   * @functype:
   *   FT_Raster_ResetFunc
   *
   * @description:
   *   FreeType used to provide an area of memory called the 'render pool'
   *   available to all registered rasterizers.  This was not thread safe,
   *   however, and now FreeType never allocates this pool.
   *
   *   This function is called after a new raster object is created.
   *
   * @input:
   *   raster ::
   *     A handle to the new raster object.
   *
   *   pool_base ::
   *     Previously, the address in memory of the render pool.  Set this to
   *     `NULL`.
   *
   *   pool_size ::
   *     Previously, the size in bytes of the render pool.  Set this to 0.
   *
   * @note:
   *   Rasterizers should rely on dynamic or stack allocation if they want to
   *   (a handle to the memory allocator is passed to the rasterizer
   *   constructor).
   */
  typedef void
  (*FT_Raster_ResetFunc)( FT_Raster       raster,
						  unsigned char*  pool_base,
						  unsigned long   pool_size );

#define FT_Raster_Reset_Func  FT_Raster_ResetFunc

  /**************************************************************************
   *
   * @functype:
   *   FT_Raster_SetModeFunc
   *
   * @description:
   *   This function is a generic facility to change modes or attributes in a
   *   given raster.  This can be used for debugging purposes, or simply to
   *   allow implementation-specific 'features' in a given raster module.
   *
   * @input:
   *   raster ::
   *     A handle to the new raster object.
   *
   *   mode ::
   *     A 4-byte tag used to name the mode or property.
   *
   *   args ::
   *     A pointer to the new mode/property to use.
   */
  typedef int
  (*FT_Raster_SetModeFunc)( FT_Raster      raster,
							unsigned long  mode,
							void*          args );

#define FT_Raster_Set_Mode_Func  FT_Raster_SetModeFunc

  /**************************************************************************
   *
   * @functype:
   *   FT_Raster_RenderFunc
   *
   * @description:
   *   Invoke a given raster to scan-convert a given glyph image into a
   *   target bitmap.
   *
   * @input:
   *   raster ::
   *     A handle to the raster object.
   *
   *   params ::
   *     A pointer to an @FT_Raster_Params structure used to store the
   *     rendering parameters.
   *
   * @return:
   *   Error code.  0~means success.
   *
   * @note:
   *   The exact format of the source image depends on the raster's glyph
   *   format defined in its @FT_Raster_Funcs structure.  It can be an
   *   @FT_Outline or anything else in order to support a large array of
   *   glyph formats.
   *
   *   Note also that the render function can fail and return a
   *   `FT_Err_Unimplemented_Feature` error code if the raster used does not
   *   support direct composition.
   */
  typedef int
  (*FT_Raster_RenderFunc)( FT_Raster                raster,
						   const FT_Raster_Params*  params );

#define FT_Raster_Render_Func  FT_Raster_RenderFunc

  /**************************************************************************
   *
   * @struct:
   *   FT_Raster_Funcs
   *
   * @description:
   *  A structure used to describe a given raster class to the library.
   *
   * @fields:
   *   glyph_format ::
   *     The supported glyph format for this raster.
   *
   *   raster_new ::
   *     The raster constructor.
   *
   *   raster_reset ::
   *     Used to reset the render pool within the raster.
   *
   *   raster_render ::
   *     A function to render a glyph into a given bitmap.
   *
   *   raster_done ::
   *     The raster destructor.
   */
  typedef struct  FT_Raster_Funcs_
  {
	FT_Glyph_Format        glyph_format;

	FT_Raster_NewFunc      raster_new;
	FT_Raster_ResetFunc    raster_reset;
	FT_Raster_SetModeFunc  raster_set_mode;
	FT_Raster_RenderFunc   raster_render;
	FT_Raster_DoneFunc     raster_done;

  } FT_Raster_Funcs;

  /* */

FT_END_HEADER

#endif /* FTIMAGE_H_ */

/* END */

/* Local Variables: */
/* coding: utf-8    */
/* End:             */

/*** End of inlined file: ftimage.h ***/

#include <stddef.h>

FT_BEGIN_HEADER

  /**************************************************************************
   *
   * @section:
   *   basic_types
   *
   * @title:
   *   Basic Data Types
   *
   * @abstract:
   *   The basic data types defined by the library.
   *
   * @description:
   *   This section contains the basic data types defined by FreeType~2,
   *   ranging from simple scalar types to bitmap descriptors.  More
   *   font-specific structures are defined in a different section.  Note
   *   that FreeType does not use floating-point data types.  Fractional
   *   values are represented by fixed-point integers, with lower bits
   *   storing the fractional part.
   *
   * @order:
   *   FT_Byte
   *   FT_Bytes
   *   FT_Char
   *   FT_Int
   *   FT_UInt
   *   FT_Int16
   *   FT_UInt16
   *   FT_Int32
   *   FT_UInt32
   *   FT_Int64
   *   FT_UInt64
   *   FT_Short
   *   FT_UShort
   *   FT_Long
   *   FT_ULong
   *   FT_Bool
   *   FT_Offset
   *   FT_PtrDist
   *   FT_String
   *   FT_Tag
   *   FT_Error
   *   FT_Fixed
   *   FT_Pointer
   *   FT_Pos
   *   FT_Vector
   *   FT_BBox
   *   FT_Matrix
   *   FT_FWord
   *   FT_UFWord
   *   FT_F2Dot14
   *   FT_UnitVector
   *   FT_F26Dot6
   *   FT_Data
   *
   *   FT_MAKE_TAG
   *
   *   FT_Generic
   *   FT_Generic_Finalizer
   *
   *   FT_Bitmap
   *   FT_Pixel_Mode
   *   FT_Palette_Mode
   *   FT_Glyph_Format
   *   FT_IMAGE_TAG
   *
   */

  /**************************************************************************
   *
   * @type:
   *   FT_Bool
   *
   * @description:
   *   A typedef of unsigned char, used for simple booleans.  As usual,
   *   values 1 and~0 represent true and false, respectively.
   */
  typedef unsigned char  FT_Bool;

  /**************************************************************************
   *
   * @type:
   *   FT_FWord
   *
   * @description:
   *   A signed 16-bit integer used to store a distance in original font
   *   units.
   */
  typedef signed short  FT_FWord;   /* distance in FUnits */

  /**************************************************************************
   *
   * @type:
   *   FT_UFWord
   *
   * @description:
   *   An unsigned 16-bit integer used to store a distance in original font
   *   units.
   */
  typedef unsigned short  FT_UFWord;  /* unsigned distance */

  /**************************************************************************
   *
   * @type:
   *   FT_Char
   *
   * @description:
   *   A simple typedef for the _signed_ char type.
   */
  typedef signed char  FT_Char;

  /**************************************************************************
   *
   * @type:
   *   FT_Byte
   *
   * @description:
   *   A simple typedef for the _unsigned_ char type.
   */
  typedef unsigned char  FT_Byte;

  /**************************************************************************
   *
   * @type:
   *   FT_Bytes
   *
   * @description:
   *   A typedef for constant memory areas.
   */
  typedef const FT_Byte*  FT_Bytes;

  /**************************************************************************
   *
   * @type:
   *   FT_Tag
   *
   * @description:
   *   A typedef for 32-bit tags (as used in the SFNT format).
   */
  typedef FT_UInt32  FT_Tag;

  /**************************************************************************
   *
   * @type:
   *   FT_String
   *
   * @description:
   *   A simple typedef for the char type, usually used for strings.
   */
  typedef char  FT_String;

  /**************************************************************************
   *
   * @type:
   *   FT_Short
   *
   * @description:
   *   A typedef for signed short.
   */
  typedef signed short  FT_Short;

  /**************************************************************************
   *
   * @type:
   *   FT_UShort
   *
   * @description:
   *   A typedef for unsigned short.
   */
  typedef unsigned short  FT_UShort;

  /**************************************************************************
   *
   * @type:
   *   FT_Int
   *
   * @description:
   *   A typedef for the int type.
   */
  typedef signed int  FT_Int;

  /**************************************************************************
   *
   * @type:
   *   FT_UInt
   *
   * @description:
   *   A typedef for the unsigned int type.
   */
  typedef unsigned int  FT_UInt;

  /**************************************************************************
   *
   * @type:
   *   FT_Long
   *
   * @description:
   *   A typedef for signed long.
   */
  typedef signed long  FT_Long;

  /**************************************************************************
   *
   * @type:
   *   FT_ULong
   *
   * @description:
   *   A typedef for unsigned long.
   */
  typedef unsigned long  FT_ULong;

  /**************************************************************************
   *
   * @type:
   *   FT_F2Dot14
   *
   * @description:
   *   A signed 2.14 fixed-point type used for unit vectors.
   */
  typedef signed short  FT_F2Dot14;

  /**************************************************************************
   *
   * @type:
   *   FT_F26Dot6
   *
   * @description:
   *   A signed 26.6 fixed-point type used for vectorial pixel coordinates.
   */
  typedef signed long  FT_F26Dot6;

  /**************************************************************************
   *
   * @type:
   *   FT_Fixed
   *
   * @description:
   *   This type is used to store 16.16 fixed-point values, like scaling
   *   values or matrix coefficients.
   */
  typedef signed long  FT_Fixed;

  /**************************************************************************
   *
   * @type:
   *   FT_Error
   *
   * @description:
   *   The FreeType error code type.  A value of~0 is always interpreted as a
   *   successful operation.
   */
  typedef int  FT_Error;

  /**************************************************************************
   *
   * @type:
   *   FT_Pointer
   *
   * @description:
   *   A simple typedef for a typeless pointer.
   */
  typedef void*  FT_Pointer;

  /**************************************************************************
   *
   * @type:
   *   FT_Offset
   *
   * @description:
   *   This is equivalent to the ANSI~C `size_t` type, i.e., the largest
   *   _unsigned_ integer type used to express a file size or position, or a
   *   memory block size.
   */
  typedef size_t  FT_Offset;

  /**************************************************************************
   *
   * @type:
   *   FT_PtrDist
   *
   * @description:
   *   This is equivalent to the ANSI~C `ptrdiff_t` type, i.e., the largest
   *   _signed_ integer type used to express the distance between two
   *   pointers.
   */
  typedef ft_ptrdiff_t  FT_PtrDist;

  /**************************************************************************
   *
   * @struct:
   *   FT_UnitVector
   *
   * @description:
   *   A simple structure used to store a 2D vector unit vector.  Uses
   *   FT_F2Dot14 types.
   *
   * @fields:
   *   x ::
   *     Horizontal coordinate.
   *
   *   y ::
   *     Vertical coordinate.
   */
  typedef struct  FT_UnitVector_
  {
	FT_F2Dot14  x;
	FT_F2Dot14  y;

  } FT_UnitVector;

  /**************************************************************************
   *
   * @struct:
   *   FT_Matrix
   *
   * @description:
   *   A simple structure used to store a 2x2 matrix.  Coefficients are in
   *   16.16 fixed-point format.  The computation performed is:
   *
   *   ```
   *     x' = x*xx + y*xy
   *     y' = x*yx + y*yy
   *   ```
   *
   * @fields:
   *   xx ::
   *     Matrix coefficient.
   *
   *   xy ::
   *     Matrix coefficient.
   *
   *   yx ::
   *     Matrix coefficient.
   *
   *   yy ::
   *     Matrix coefficient.
   */
  typedef struct  FT_Matrix_
  {
	FT_Fixed  xx, xy;
	FT_Fixed  yx, yy;

  } FT_Matrix;

  /**************************************************************************
   *
   * @struct:
   *   FT_Data
   *
   * @description:
   *   Read-only binary data represented as a pointer and a length.
   *
   * @fields:
   *   pointer ::
   *     The data.
   *
   *   length ::
   *     The length of the data in bytes.
   */
  typedef struct  FT_Data_
  {
	const FT_Byte*  pointer;
	FT_UInt         length;

  } FT_Data;

  /**************************************************************************
   *
   * @functype:
   *   FT_Generic_Finalizer
   *
   * @description:
   *   Describe a function used to destroy the 'client' data of any FreeType
   *   object.  See the description of the @FT_Generic type for details of
   *   usage.
   *
   * @input:
   *   The address of the FreeType object that is under finalization.  Its
   *   client data is accessed through its `generic` field.
   */
  typedef void  (*FT_Generic_Finalizer)( void*  object );

  /**************************************************************************
   *
   * @struct:
   *   FT_Generic
   *
   * @description:
   *   Client applications often need to associate their own data to a
   *   variety of FreeType core objects.  For example, a text layout API
   *   might want to associate a glyph cache to a given size object.
   *
   *   Some FreeType object contains a `generic` field, of type `FT_Generic`,
   *   which usage is left to client applications and font servers.
   *
   *   It can be used to store a pointer to client-specific data, as well as
   *   the address of a 'finalizer' function, which will be called by
   *   FreeType when the object is destroyed (for example, the previous
   *   client example would put the address of the glyph cache destructor in
   *   the `finalizer` field).
   *
   * @fields:
   *   data ::
   *     A typeless pointer to any client-specified data. This field is
   *     completely ignored by the FreeType library.
   *
   *   finalizer ::
   *     A pointer to a 'generic finalizer' function, which will be called
   *     when the object is destroyed.  If this field is set to `NULL`, no
   *     code will be called.
   */
  typedef struct  FT_Generic_
  {
	void*                 data;
	FT_Generic_Finalizer  finalizer;

  } FT_Generic;

  /**************************************************************************
   *
   * @macro:
   *   FT_MAKE_TAG
   *
   * @description:
   *   This macro converts four-letter tags that are used to label TrueType
   *   tables into an `FT_Tag` type, to be used within FreeType.
   *
   * @note:
   *   The produced values **must** be 32-bit integers.  Don't redefine this
   *   macro.
   */
#define FT_MAKE_TAG( _x1, _x2, _x3, _x4 )                  \
		  ( ( FT_STATIC_BYTE_CAST( FT_Tag, _x1 ) << 24 ) | \
			( FT_STATIC_BYTE_CAST( FT_Tag, _x2 ) << 16 ) | \
			( FT_STATIC_BYTE_CAST( FT_Tag, _x3 ) <<  8 ) | \
			  FT_STATIC_BYTE_CAST( FT_Tag, _x4 )         )

  /*************************************************************************/
  /*************************************************************************/
  /*                                                                       */
  /*                    L I S T   M A N A G E M E N T                      */
  /*                                                                       */
  /*************************************************************************/
  /*************************************************************************/

  /**************************************************************************
   *
   * @section:
   *   list_processing
   *
   */

  /**************************************************************************
   *
   * @type:
   *   FT_ListNode
   *
   * @description:
   *    Many elements and objects in FreeType are listed through an @FT_List
   *    record (see @FT_ListRec).  As its name suggests, an FT_ListNode is a
   *    handle to a single list element.
   */
  typedef struct FT_ListNodeRec_*  FT_ListNode;

  /**************************************************************************
   *
   * @type:
   *   FT_List
   *
   * @description:
   *   A handle to a list record (see @FT_ListRec).
   */
  typedef struct FT_ListRec_*  FT_List;

  /**************************************************************************
   *
   * @struct:
   *   FT_ListNodeRec
   *
   * @description:
   *   A structure used to hold a single list element.
   *
   * @fields:
   *   prev ::
   *     The previous element in the list.  `NULL` if first.
   *
   *   next ::
   *     The next element in the list.  `NULL` if last.
   *
   *   data ::
   *     A typeless pointer to the listed object.
   */
  typedef struct  FT_ListNodeRec_
  {
	FT_ListNode  prev;
	FT_ListNode  next;
	void*        data;

  } FT_ListNodeRec;

  /**************************************************************************
   *
   * @struct:
   *   FT_ListRec
   *
   * @description:
   *   A structure used to hold a simple doubly-linked list.  These are used
   *   in many parts of FreeType.
   *
   * @fields:
   *   head ::
   *     The head (first element) of doubly-linked list.
   *
   *   tail ::
   *     The tail (last element) of doubly-linked list.
   */
  typedef struct  FT_ListRec_
  {
	FT_ListNode  head;
	FT_ListNode  tail;

  } FT_ListRec;

  /* */

#define FT_IS_EMPTY( list )  ( (list).head == 0 )
#define FT_BOOL( x )         FT_STATIC_CAST( FT_Bool, (x) != 0 )

  /* concatenate C tokens */
#define FT_ERR_XCAT( x, y )  x ## y
#define FT_ERR_CAT( x, y )   FT_ERR_XCAT( x, y )

  /* see `ftmoderr.h` for descriptions of the following macros */

#define FT_ERR( e )  FT_ERR_CAT( FT_ERR_PREFIX, e )

#define FT_ERROR_BASE( x )    ( (x) & 0xFF )
#define FT_ERROR_MODULE( x )  ( (x) & 0xFF00U )

#define FT_ERR_EQ( x, e )                                        \
		  ( FT_ERROR_BASE( x ) == FT_ERROR_BASE( FT_ERR( e ) ) )
#define FT_ERR_NEQ( x, e )                                       \
		  ( FT_ERROR_BASE( x ) != FT_ERROR_BASE( FT_ERR( e ) ) )

FT_END_HEADER

#endif /* FTTYPES_H_ */

/* END */

/*** End of inlined file: fttypes.h ***/


/*** Start of inlined file: fterrors.h ***/
  /**************************************************************************
   *
   * @section:
   *   error_enumerations
   *
   * @title:
   *   Error Enumerations
   *
   * @abstract:
   *   How to handle errors and error strings.
   *
   * @description:
   *   The header file `fterrors.h` (which is automatically included by
   *   `freetype.h`) defines the handling of FreeType's enumeration
   *   constants.  It can also be used to generate error message strings
   *   with a small macro trick explained below.
   *
   *   **Error Formats**
   *
   *   The configuration macro `FT_CONFIG_OPTION_USE_MODULE_ERRORS` can be
   *   defined in `ftoption.h` in order to make the higher byte indicate the
   *   module where the error has happened (this is not compatible with
   *   standard builds of FreeType~2, however).  See the file `ftmoderr.h`
   *   for more details.
   *
   *   **Error Message Strings**
   *
   *   Error definitions are set up with special macros that allow client
   *   applications to build a table of error message strings.  The strings
   *   are not included in a normal build of FreeType~2 to save space (most
   *   client applications do not use them).
   *
   *   To do so, you have to define the following macros before including
   *   this file.
   *
   *   ```
   *     FT_ERROR_START_LIST
   *   ```
   *
   *   This macro is called before anything else to define the start of the
   *   error list.  It is followed by several `FT_ERROR_DEF` calls.
   *
   *   ```
   *     FT_ERROR_DEF( e, v, s )
   *   ```
   *
   *   This macro is called to define one single error.  'e' is the error
   *   code identifier (e.g., `Invalid_Argument`), 'v' is the error's
   *   numerical value, and 's' is the corresponding error string.
   *
   *   ```
   *     FT_ERROR_END_LIST
   *   ```
   *
   *   This macro ends the list.
   *
   *   Additionally, you have to undefine `FTERRORS_H_` before #including
   *   this file.
   *
   *   Here is a simple example.
   *
   *   ```
   *     #undef FTERRORS_H_
   *     #define FT_ERRORDEF( e, v, s )  { e, s },
   *     #define FT_ERROR_START_LIST     {
   *     #define FT_ERROR_END_LIST       { 0, NULL } };
   *
   *     const struct
   *     {
   *       int          err_code;
   *       const char*  err_msg;
   *     } ft_errors[] =
   *
   *     #include <freetype/fterrors.h>
   *   ```
   *
   *   An alternative to using an array is a switch statement.
   *
   *   ```
   *     #undef FTERRORS_H_
   *     #define FT_ERROR_START_LIST     switch ( error_code ) {
   *     #define FT_ERRORDEF( e, v, s )    case v: return s;
   *     #define FT_ERROR_END_LIST       }
   *   ```
   *
   *   If you use `FT_CONFIG_OPTION_USE_MODULE_ERRORS`, `error_code` should
   *   be replaced with `FT_ERROR_BASE(error_code)` in the last example.
   */

  /* */

  /* In previous FreeType versions we used `__FTERRORS_H__`.  However, */
  /* using two successive underscores in a non-system symbol name      */
  /* violates the C (and C++) standard, so it was changed to the       */
  /* current form.  In spite of this, we have to make                  */
  /*                                                                   */
  /* ```                                                               */
  /*   #undefine __FTERRORS_H__                                        */
  /* ```                                                               */
  /*                                                                   */
  /* work for backward compatibility.                                  */
  /*                                                                   */
#if !( defined( FTERRORS_H_ ) && defined ( __FTERRORS_H__ ) )
#define FTERRORS_H_
#define __FTERRORS_H__

  /* include module base error codes */

/*** Start of inlined file: ftmoderr.h ***/
  /**************************************************************************
   *
   * This file is used to define the FreeType module error codes.
   *
   * If the macro `FT_CONFIG_OPTION_USE_MODULE_ERRORS` in `ftoption.h` is
   * set, the lower byte of an error value identifies the error code as
   * usual.  In addition, the higher byte identifies the module.  For
   * example, the error `FT_Err_Invalid_File_Format` has value 0x0003, the
   * error `TT_Err_Invalid_File_Format` has value 0x1303, the error
   * `T1_Err_Invalid_File_Format` has value 0x1403, etc.
   *
   * Note that `FT_Err_Ok`, `TT_Err_Ok`, etc. are always equal to zero,
   * including the high byte.
   *
   * If `FT_CONFIG_OPTION_USE_MODULE_ERRORS` isn't set, the higher byte of an
   * error value is set to zero.
   *
   * To hide the various `XXX_Err_` prefixes in the source code, FreeType
   * provides some macros in `fttypes.h`.
   *
   *   FT_ERR( err )
   *
   *     Add current error module prefix (as defined with the `FT_ERR_PREFIX`
   *     macro) to `err`.  For example, in the BDF module the line
   *
   *     ```
   *       error = FT_ERR( Invalid_Outline );
   *     ```
   *
   *     expands to
   *
   *     ```
   *       error = BDF_Err_Invalid_Outline;
   *     ```
   *
   *     For simplicity, you can always use `FT_Err_Ok` directly instead of
   *     `FT_ERR( Ok )`.
   *
   *   FT_ERR_EQ( errcode, err )
   *   FT_ERR_NEQ( errcode, err )
   *
   *     Compare error code `errcode` with the error `err` for equality and
   *     inequality, respectively.  Example:
   *
   *     ```
   *       if ( FT_ERR_EQ( error, Invalid_Outline ) )
   *         ...
   *     ```
   *
   *     Using this macro you don't have to think about error prefixes.  Of
   *     course, if module errors are not active, the above example is the
   *     same as
   *
   *     ```
   *       if ( error == FT_Err_Invalid_Outline )
   *         ...
   *     ```
   *
   *   FT_ERROR_BASE( errcode )
   *   FT_ERROR_MODULE( errcode )
   *
   *     Get base error and module error code, respectively.
   *
   * It can also be used to create a module error message table easily with
   * something like
   *
   * ```
   *   #undef FTMODERR_H_
   *   #define FT_MODERRDEF( e, v, s )  { FT_Mod_Err_ ## e, s },
   *   #define FT_MODERR_START_LIST     {
   *   #define FT_MODERR_END_LIST       { 0, 0 } };
   *
   *   const struct
   *   {
   *     int          mod_err_offset;
   *     const char*  mod_err_msg
   *   } ft_mod_errors[] =
   *
   *   #include <freetype/ftmoderr.h>
   * ```
   *
   */

#ifndef FTMODERR_H_
#define FTMODERR_H_

  /*******************************************************************/
  /*******************************************************************/
  /*****                                                         *****/
  /*****                       SETUP MACROS                      *****/
  /*****                                                         *****/
  /*******************************************************************/
  /*******************************************************************/

#undef  FT_NEED_EXTERN_C

#ifndef FT_MODERRDEF

#ifdef FT_CONFIG_OPTION_USE_MODULE_ERRORS
#define FT_MODERRDEF( e, v, s )  FT_Mod_Err_ ## e = v,
#else
#define FT_MODERRDEF( e, v, s )  FT_Mod_Err_ ## e = 0,
#endif

#define FT_MODERR_START_LIST  enum {
#define FT_MODERR_END_LIST    FT_Mod_Err_Max };

#ifdef __cplusplus
#define FT_NEED_EXTERN_C
  extern "C" {
#endif

#endif /* !FT_MODERRDEF */

  /*******************************************************************/
  /*******************************************************************/
  /*****                                                         *****/
  /*****               LIST MODULE ERROR BASES                   *****/
  /*****                                                         *****/
  /*******************************************************************/
  /*******************************************************************/

#ifdef FT_MODERR_START_LIST
  FT_MODERR_START_LIST
#endif

  FT_MODERRDEF( Base,      0x000, "base module" )
  FT_MODERRDEF( Autofit,   0x100, "autofitter module" )
  FT_MODERRDEF( BDF,       0x200, "BDF module" )
  FT_MODERRDEF( Bzip2,     0x300, "Bzip2 module" )
  FT_MODERRDEF( Cache,     0x400, "cache module" )
  FT_MODERRDEF( CFF,       0x500, "CFF module" )
  FT_MODERRDEF( CID,       0x600, "CID module" )
  FT_MODERRDEF( Gzip,      0x700, "Gzip module" )
  FT_MODERRDEF( LZW,       0x800, "LZW module" )
  FT_MODERRDEF( OTvalid,   0x900, "OpenType validation module" )
  FT_MODERRDEF( PCF,       0xA00, "PCF module" )
  FT_MODERRDEF( PFR,       0xB00, "PFR module" )
  FT_MODERRDEF( PSaux,     0xC00, "PS auxiliary module" )
  FT_MODERRDEF( PShinter,  0xD00, "PS hinter module" )
  FT_MODERRDEF( PSnames,   0xE00, "PS names module" )
  FT_MODERRDEF( Raster,    0xF00, "raster module" )
  FT_MODERRDEF( SFNT,     0x1000, "SFNT module" )
  FT_MODERRDEF( Smooth,   0x1100, "smooth raster module" )
  FT_MODERRDEF( TrueType, 0x1200, "TrueType module" )
  FT_MODERRDEF( Type1,    0x1300, "Type 1 module" )
  FT_MODERRDEF( Type42,   0x1400, "Type 42 module" )
  FT_MODERRDEF( Winfonts, 0x1500, "Windows FON/FNT module" )
  FT_MODERRDEF( GXvalid,  0x1600, "GX validation module" )
  FT_MODERRDEF( Sdf,      0x1700, "Signed distance field raster module" )

#ifdef FT_MODERR_END_LIST
  FT_MODERR_END_LIST
#endif

  /*******************************************************************/
  /*******************************************************************/
  /*****                                                         *****/
  /*****                      CLEANUP                            *****/
  /*****                                                         *****/
  /*******************************************************************/
  /*******************************************************************/

#ifdef FT_NEED_EXTERN_C
  }
#endif

#undef FT_MODERR_START_LIST
#undef FT_MODERR_END_LIST
#undef FT_MODERRDEF
#undef FT_NEED_EXTERN_C

#endif /* FTMODERR_H_ */

/* END */

/*** End of inlined file: ftmoderr.h ***/


  /*******************************************************************/
  /*******************************************************************/
  /*****                                                         *****/
  /*****                       SETUP MACROS                      *****/
  /*****                                                         *****/
  /*******************************************************************/
  /*******************************************************************/

#undef  FT_NEED_EXTERN_C

  /* FT_ERR_PREFIX is used as a prefix for error identifiers. */
  /* By default, we use `FT_Err_`.                            */
  /*                                                          */
#ifndef FT_ERR_PREFIX
#define FT_ERR_PREFIX  FT_Err_
#endif

  /* FT_ERR_BASE is used as the base for module-specific errors. */
  /*                                                             */
#ifdef FT_CONFIG_OPTION_USE_MODULE_ERRORS

#ifndef FT_ERR_BASE
#define FT_ERR_BASE  FT_Mod_Err_Base
#endif

#else

#undef FT_ERR_BASE
#define FT_ERR_BASE  0

#endif /* FT_CONFIG_OPTION_USE_MODULE_ERRORS */

  /* If FT_ERRORDEF is not defined, we need to define a simple */
  /* enumeration type.                                         */
  /*                                                           */
#ifndef FT_ERRORDEF

#define FT_INCLUDE_ERR_PROTOS

#define FT_ERRORDEF( e, v, s )  e = v,
#define FT_ERROR_START_LIST     enum {
#define FT_ERROR_END_LIST       FT_ERR_CAT( FT_ERR_PREFIX, Max ) };

#ifdef __cplusplus
#define FT_NEED_EXTERN_C
  extern "C" {
#endif

#endif /* !FT_ERRORDEF */

  /* this macro is used to define an error */
#define FT_ERRORDEF_( e, v, s )                                             \
		  FT_ERRORDEF( FT_ERR_CAT( FT_ERR_PREFIX, e ), v + FT_ERR_BASE, s )

  /* this is only used for <module>_Err_Ok, which must be 0! */
#define FT_NOERRORDEF_( e, v, s )                             \
		  FT_ERRORDEF( FT_ERR_CAT( FT_ERR_PREFIX, e ), v, s )

#ifdef FT_ERROR_START_LIST
  FT_ERROR_START_LIST
#endif

  /* now include the error codes */

/*** Start of inlined file: fterrdef.h ***/
  /**************************************************************************
   *
   * @section:
   *  error_code_values
   *
   * @title:
   *  Error Code Values
   *
   * @abstract:
   *  All possible error codes returned by FreeType functions.
   *
   * @description:
   *  The list below is taken verbatim from the file `fterrdef.h` (loaded
   *  automatically by including `FT_FREETYPE_H`).  The first argument of the
   *  `FT_ERROR_DEF_` macro is the error label; by default, the prefix
   *  `FT_Err_` gets added so that you get error names like
   *  `FT_Err_Cannot_Open_Resource`.  The second argument is the error code,
   *  and the last argument an error string, which is not used by FreeType.
   *
   *  Within your application you should **only** use error names and
   *  **never** its numeric values!  The latter might (and actually do)
   *  change in forthcoming FreeType versions.
   *
   *  Macro `FT_NOERRORDEF_` defines `FT_Err_Ok`, which is always zero.  See
   *  the 'Error Enumerations' subsection how to automatically generate a
   *  list of error strings.
   *
   */

  /**************************************************************************
   *
   * @enum:
   *   FT_Err_XXX
   *
   */

  /* generic errors */

  FT_NOERRORDEF_( Ok,                                        0x00,
				  "no error" )

  FT_ERRORDEF_( Cannot_Open_Resource,                        0x01,
				"cannot open resource" )
  FT_ERRORDEF_( Unknown_File_Format,                         0x02,
				"unknown file format" )
  FT_ERRORDEF_( Invalid_File_Format,                         0x03,
				"broken file" )
  FT_ERRORDEF_( Invalid_Version,                             0x04,
				"invalid FreeType version" )
  FT_ERRORDEF_( Lower_Module_Version,                        0x05,
				"module version is too low" )
  FT_ERRORDEF_( Invalid_Argument,                            0x06,
				"invalid argument" )
  FT_ERRORDEF_( Unimplemented_Feature,                       0x07,
				"unimplemented feature" )
  FT_ERRORDEF_( Invalid_Table,                               0x08,
				"broken table" )
  FT_ERRORDEF_( Invalid_Offset,                              0x09,
				"broken offset within table" )
  FT_ERRORDEF_( Array_Too_Large,                             0x0A,
				"array allocation size too large" )
  FT_ERRORDEF_( Missing_Module,                              0x0B,
				"missing module" )
  FT_ERRORDEF_( Missing_Property,                            0x0C,
				"missing property" )

  /* glyph/character errors */

  FT_ERRORDEF_( Invalid_Glyph_Index,                         0x10,
				"invalid glyph index" )
  FT_ERRORDEF_( Invalid_Character_Code,                      0x11,
				"invalid character code" )
  FT_ERRORDEF_( Invalid_Glyph_Format,                        0x12,
				"unsupported glyph image format" )
  FT_ERRORDEF_( Cannot_Render_Glyph,                         0x13,
				"cannot render this glyph format" )
  FT_ERRORDEF_( Invalid_Outline,                             0x14,
				"invalid outline" )
  FT_ERRORDEF_( Invalid_Composite,                           0x15,
				"invalid composite glyph" )
  FT_ERRORDEF_( Too_Many_Hints,                              0x16,
				"too many hints" )
  FT_ERRORDEF_( Invalid_Pixel_Size,                          0x17,
				"invalid pixel size" )
  FT_ERRORDEF_( Invalid_SVG_Document,                        0x18,
				"invalid SVG document" )

  /* handle errors */

  FT_ERRORDEF_( Invalid_Handle,                              0x20,
				"invalid object handle" )
  FT_ERRORDEF_( Invalid_Library_Handle,                      0x21,
				"invalid library handle" )
  FT_ERRORDEF_( Invalid_Driver_Handle,                       0x22,
				"invalid module handle" )
  FT_ERRORDEF_( Invalid_Face_Handle,                         0x23,
				"invalid face handle" )
  FT_ERRORDEF_( Invalid_Size_Handle,                         0x24,
				"invalid size handle" )
  FT_ERRORDEF_( Invalid_Slot_Handle,                         0x25,
				"invalid glyph slot handle" )
  FT_ERRORDEF_( Invalid_CharMap_Handle,                      0x26,
				"invalid charmap handle" )
  FT_ERRORDEF_( Invalid_Cache_Handle,                        0x27,
				"invalid cache manager handle" )
  FT_ERRORDEF_( Invalid_Stream_Handle,                       0x28,
				"invalid stream handle" )

  /* driver errors */

  FT_ERRORDEF_( Too_Many_Drivers,                            0x30,
				"too many modules" )
  FT_ERRORDEF_( Too_Many_Extensions,                         0x31,
				"too many extensions" )

  /* memory errors */

  FT_ERRORDEF_( Out_Of_Memory,                               0x40,
				"out of memory" )
  FT_ERRORDEF_( Unlisted_Object,                             0x41,
				"unlisted object" )

  /* stream errors */

  FT_ERRORDEF_( Cannot_Open_Stream,                          0x51,
				"cannot open stream" )
  FT_ERRORDEF_( Invalid_Stream_Seek,                         0x52,
				"invalid stream seek" )
  FT_ERRORDEF_( Invalid_Stream_Skip,                         0x53,
				"invalid stream skip" )
  FT_ERRORDEF_( Invalid_Stream_Read,                         0x54,
				"invalid stream read" )
  FT_ERRORDEF_( Invalid_Stream_Operation,                    0x55,
				"invalid stream operation" )
  FT_ERRORDEF_( Invalid_Frame_Operation,                     0x56,
				"invalid frame operation" )
  FT_ERRORDEF_( Nested_Frame_Access,                         0x57,
				"nested frame access" )
  FT_ERRORDEF_( Invalid_Frame_Read,                          0x58,
				"invalid frame read" )

  /* raster errors */

  FT_ERRORDEF_( Raster_Uninitialized,                        0x60,
				"raster uninitialized" )
  FT_ERRORDEF_( Raster_Corrupted,                            0x61,
				"raster corrupted" )
  FT_ERRORDEF_( Raster_Overflow,                             0x62,
				"raster overflow" )
  FT_ERRORDEF_( Raster_Negative_Height,                      0x63,
				"negative height while rastering" )

  /* cache errors */

  FT_ERRORDEF_( Too_Many_Caches,                             0x70,
				"too many registered caches" )

  /* TrueType and SFNT errors */

  FT_ERRORDEF_( Invalid_Opcode,                              0x80,
				"invalid opcode" )
  FT_ERRORDEF_( Too_Few_Arguments,                           0x81,
				"too few arguments" )
  FT_ERRORDEF_( Stack_Overflow,                              0x82,
				"stack overflow" )
  FT_ERRORDEF_( Code_Overflow,                               0x83,
				"code overflow" )
  FT_ERRORDEF_( Bad_Argument,                                0x84,
				"bad argument" )
  FT_ERRORDEF_( Divide_By_Zero,                              0x85,
				"division by zero" )
  FT_ERRORDEF_( Invalid_Reference,                           0x86,
				"invalid reference" )
  FT_ERRORDEF_( Debug_OpCode,                                0x87,
				"found debug opcode" )
  FT_ERRORDEF_( ENDF_In_Exec_Stream,                         0x88,
				"found ENDF opcode in execution stream" )
  FT_ERRORDEF_( Nested_DEFS,                                 0x89,
				"nested DEFS" )
  FT_ERRORDEF_( Invalid_CodeRange,                           0x8A,
				"invalid code range" )
  FT_ERRORDEF_( Execution_Too_Long,                          0x8B,
				"execution context too long" )
  FT_ERRORDEF_( Too_Many_Function_Defs,                      0x8C,
				"too many function definitions" )
  FT_ERRORDEF_( Too_Many_Instruction_Defs,                   0x8D,
				"too many instruction definitions" )
  FT_ERRORDEF_( Table_Missing,                               0x8E,
				"SFNT font table missing" )
  FT_ERRORDEF_( Horiz_Header_Missing,                        0x8F,
				"horizontal header (hhea) table missing" )
  FT_ERRORDEF_( Locations_Missing,                           0x90,
				"locations (loca) table missing" )
  FT_ERRORDEF_( Name_Table_Missing,                          0x91,
				"name table missing" )
  FT_ERRORDEF_( CMap_Table_Missing,                          0x92,
				"character map (cmap) table missing" )
  FT_ERRORDEF_( Hmtx_Table_Missing,                          0x93,
				"horizontal metrics (hmtx) table missing" )
  FT_ERRORDEF_( Post_Table_Missing,                          0x94,
				"PostScript (post) table missing" )
  FT_ERRORDEF_( Invalid_Horiz_Metrics,                       0x95,
				"invalid horizontal metrics" )
  FT_ERRORDEF_( Invalid_CharMap_Format,                      0x96,
				"invalid character map (cmap) format" )
  FT_ERRORDEF_( Invalid_PPem,                                0x97,
				"invalid ppem value" )
  FT_ERRORDEF_( Invalid_Vert_Metrics,                        0x98,
				"invalid vertical metrics" )
  FT_ERRORDEF_( Could_Not_Find_Context,                      0x99,
				"could not find context" )
  FT_ERRORDEF_( Invalid_Post_Table_Format,                   0x9A,
				"invalid PostScript (post) table format" )
  FT_ERRORDEF_( Invalid_Post_Table,                          0x9B,
				"invalid PostScript (post) table" )
  FT_ERRORDEF_( DEF_In_Glyf_Bytecode,                        0x9C,
				"found FDEF or IDEF opcode in glyf bytecode" )
  FT_ERRORDEF_( Missing_Bitmap,                              0x9D,
				"missing bitmap in strike" )
  FT_ERRORDEF_( Missing_SVG_Hooks,                           0x9E,
				"SVG hooks have not been set" )

  /* CFF, CID, and Type 1 errors */

  FT_ERRORDEF_( Syntax_Error,                                0xA0,
				"opcode syntax error" )
  FT_ERRORDEF_( Stack_Underflow,                             0xA1,
				"argument stack underflow" )
  FT_ERRORDEF_( Ignore,                                      0xA2,
				"ignore" )
  FT_ERRORDEF_( No_Unicode_Glyph_Name,                       0xA3,
				"no Unicode glyph name found" )
  FT_ERRORDEF_( Glyph_Too_Big,                               0xA4,
				"glyph too big for hinting" )

  /* BDF errors */

  FT_ERRORDEF_( Missing_Startfont_Field,                     0xB0,
				"`STARTFONT' field missing" )
  FT_ERRORDEF_( Missing_Font_Field,                          0xB1,
				"`FONT' field missing" )
  FT_ERRORDEF_( Missing_Size_Field,                          0xB2,
				"`SIZE' field missing" )
  FT_ERRORDEF_( Missing_Fontboundingbox_Field,               0xB3,
				"`FONTBOUNDINGBOX' field missing" )
  FT_ERRORDEF_( Missing_Chars_Field,                         0xB4,
				"`CHARS' field missing" )
  FT_ERRORDEF_( Missing_Startchar_Field,                     0xB5,
				"`STARTCHAR' field missing" )
  FT_ERRORDEF_( Missing_Encoding_Field,                      0xB6,
				"`ENCODING' field missing" )
  FT_ERRORDEF_( Missing_Bbx_Field,                           0xB7,
				"`BBX' field missing" )
  FT_ERRORDEF_( Bbx_Too_Big,                                 0xB8,
				"`BBX' too big" )
  FT_ERRORDEF_( Corrupted_Font_Header,                       0xB9,
				"Font header corrupted or missing fields" )
  FT_ERRORDEF_( Corrupted_Font_Glyphs,                       0xBA,
				"Font glyphs corrupted or missing fields" )

  /* */

/* END */

/*** End of inlined file: fterrdef.h ***/


#ifdef FT_ERROR_END_LIST
  FT_ERROR_END_LIST
#endif

  /*******************************************************************/
  /*******************************************************************/
  /*****                                                         *****/
  /*****                      SIMPLE CLEANUP                     *****/
  /*****                                                         *****/
  /*******************************************************************/
  /*******************************************************************/

#ifdef FT_NEED_EXTERN_C
  }
#endif

#undef FT_ERROR_START_LIST
#undef FT_ERROR_END_LIST

#undef FT_ERRORDEF
#undef FT_ERRORDEF_
#undef FT_NOERRORDEF_

#undef FT_NEED_EXTERN_C
#undef FT_ERR_BASE

  /* FT_ERR_PREFIX is needed internally */
#ifndef FT2_BUILD_LIBRARY
#undef FT_ERR_PREFIX
#endif

  /* FT_INCLUDE_ERR_PROTOS: Control whether function prototypes should be */
  /*                        included with                                 */
  /*                                                                      */
  /*                          #include <freetype/fterrors.h>              */
  /*                                                                      */
  /*                        This is only true where `FT_ERRORDEF` is      */
  /*                        undefined.                                    */
  /*                                                                      */
  /* FT_ERR_PROTOS_DEFINED: Actual multiple-inclusion protection of       */
  /*                        `fterrors.h`.                                 */
#ifdef FT_INCLUDE_ERR_PROTOS
#undef FT_INCLUDE_ERR_PROTOS

#ifndef FT_ERR_PROTOS_DEFINED
#define FT_ERR_PROTOS_DEFINED

FT_BEGIN_HEADER

  /**************************************************************************
   *
   * @function:
   *   FT_Error_String
   *
   * @description:
   *   Retrieve the description of a valid FreeType error code.
   *
   * @input:
   *   error_code ::
   *     A valid FreeType error code.
   *
   * @return:
   *   A C~string or `NULL`, if any error occurred.
   *
   * @note:
   *   FreeType has to be compiled with `FT_CONFIG_OPTION_ERROR_STRINGS` or
   *   `FT_DEBUG_LEVEL_ERROR` to get meaningful descriptions.
   *   'error_string' will be `NULL` otherwise.
   *
   *   Module identification will be ignored:
   *
   *   ```c
   *     strcmp( FT_Error_String(  FT_Err_Unknown_File_Format ),
   *             FT_Error_String( BDF_Err_Unknown_File_Format ) ) == 0;
   *   ```
   */
  FT_EXPORT( const char* )
  FT_Error_String( FT_Error  error_code );

  /* */

FT_END_HEADER

#endif /* FT_ERR_PROTOS_DEFINED */

#endif /* FT_INCLUDE_ERR_PROTOS */

#endif /* !(FTERRORS_H_ && __FTERRORS_H__) */

/* END */

/*** End of inlined file: fterrors.h ***/

FT_BEGIN_HEADER

  /**************************************************************************
   *
   * @section:
   *   preamble
   *
   * @title:
   *   Preamble
   *
   * @abstract:
   *   What FreeType is and isn't
   *
   * @description:
   *   FreeType is a library that provides access to glyphs in font files.  It
   *   scales the glyph images and their metrics to a requested size, and it
   *   rasterizes the glyph images to produce pixel or subpixel alpha coverage
   *   bitmaps.
   *
   *   Note that FreeType is _not_ a text layout engine.  You have to use
   *   higher-level libraries like HarfBuzz, Pango, or ICU for that.
   *
   *   Note also that FreeType does _not_ perform alpha blending or
   *   compositing the resulting bitmaps or pixmaps by itself.  Use your
   *   favourite graphics library (for example, Cairo or Skia) to further
   *   process FreeType's output.
   *
   */

  /**************************************************************************
   *
   * @section:
   *   header_inclusion
   *
   * @title:
   *   FreeType's header inclusion scheme
   *
   * @abstract:
   *   How client applications should include FreeType header files.
   *
   * @description:
   *   To be as flexible as possible (and for historical reasons), you must
   *   load file `ft2build.h` first before other header files, for example
   *
   *   ```
   *     #include <ft2build.h>
   *
   *     #include <freetype/freetype.h>
   *     #include <freetype/ftoutln.h>
   *   ```
   */

  /**************************************************************************
   *
   * @section:
   *   user_allocation
   *
   * @title:
   *   User allocation
   *
   * @abstract:
   *   How client applications should allocate FreeType data structures.
   *
   * @description:
   *   FreeType assumes that structures allocated by the user and passed as
   *   arguments are zeroed out except for the actual data.  In other words,
   *   it is recommended to use `calloc` (or variants of it) instead of
   *   `malloc` for allocation.
   *
   */

  /**************************************************************************
   *
   * @section:
   *   font_testing_macros
   *
   * @title:
   *   Font Testing Macros
   *
   * @abstract:
   *   Macros to test various properties of fonts.
   *
   * @description:
   *   Macros to test the most important font properties.
   *
   *   It is recommended to use these high-level macros instead of directly
   *   testing the corresponding flags, which are scattered over various
   *   structures.
   *
   * @order:
   *   FT_HAS_HORIZONTAL
   *   FT_HAS_VERTICAL
   *   FT_HAS_KERNING
   *   FT_HAS_FIXED_SIZES
   *   FT_HAS_GLYPH_NAMES
   *   FT_HAS_COLOR
   *   FT_HAS_MULTIPLE_MASTERS
   *   FT_HAS_SVG
   *   FT_HAS_SBIX
   *   FT_HAS_SBIX_OVERLAY
   *
   *   FT_IS_SFNT
   *   FT_IS_SCALABLE
   *   FT_IS_FIXED_WIDTH
   *   FT_IS_CID_KEYED
   *   FT_IS_TRICKY
   *   FT_IS_NAMED_INSTANCE
   *   FT_IS_VARIATION
   *
   */

  /**************************************************************************
   *
   * @section:
   *   library_setup
   *
   * @title:
   *   Library Setup
   *
   * @abstract:
   *   Functions to start and end the usage of the FreeType library.
   *
   * @description:
   *   Functions to start and end the usage of the FreeType library.
   *
   *   Note that @FT_Library_Version and @FREETYPE_XXX are of limited use
   *   because even a new release of FreeType with only documentation
   *   changes increases the version number.
   *
   * @order:
   *   FT_Library
   *   FT_Init_FreeType
   *   FT_Done_FreeType
   *
   *   FT_Library_Version
   *   FREETYPE_XXX
   *
   */

  /**************************************************************************
   *
   * @section:
   *   face_creation
   *
   * @title:
   *   Face Creation
   *
   * @abstract:
   *   Functions to manage fonts.
   *
   * @description:
   *   The functions and structures collected in this section operate on
   *   fonts globally.
   *
   * @order:
   *   FT_Face
   *   FT_FaceRec
   *   FT_FACE_FLAG_XXX
   *   FT_STYLE_FLAG_XXX
   *
   *   FT_New_Face
   *   FT_Done_Face
   *   FT_Reference_Face
   *   FT_New_Memory_Face
   *   FT_Face_Properties
   *   FT_Open_Face
   *   FT_Open_Args
   *   FT_OPEN_XXX
   *   FT_Parameter
   *   FT_Attach_File
   *   FT_Attach_Stream
   *
   */

  /**************************************************************************
   *
   * @section:
   *   sizing_and_scaling
   *
   * @title:
   *   Sizing and Scaling
   *
   * @abstract:
   *   Functions to manage font sizes.
   *
   * @description:
   *   The functions and structures collected in this section are related to
   *   selecting and manipulating the size of a font globally.
   *
   * @order:
   *   FT_Size
   *   FT_SizeRec
   *   FT_Size_Metrics
   *
   *   FT_Bitmap_Size
   *
   *   FT_Set_Char_Size
   *   FT_Set_Pixel_Sizes
   *   FT_Request_Size
   *   FT_Select_Size
   *   FT_Size_Request_Type
   *   FT_Size_RequestRec
   *   FT_Size_Request
   *
   *   FT_Set_Transform
   *   FT_Get_Transform
   *
   */

  /**************************************************************************
   *
   * @section:
   *   glyph_retrieval
   *
   * @title:
   *   Glyph Retrieval
   *
   * @abstract:
   *   Functions to manage glyphs.
   *
   * @description:
   *   The functions and structures collected in this section operate on
   *   single glyphs, of which @FT_Load_Glyph is most important.
   *
   * @order:
   *   FT_GlyphSlot
   *   FT_GlyphSlotRec
   *   FT_Glyph_Metrics
   *
   *   FT_Load_Glyph
   *   FT_LOAD_XXX
   *   FT_LOAD_TARGET_MODE
   *   FT_LOAD_TARGET_XXX
   *
   *   FT_Render_Glyph
   *   FT_Render_Mode
   *   FT_Get_Kerning
   *   FT_Kerning_Mode
   *   FT_Get_Track_Kerning
   *
   */

  /**************************************************************************
   *
   * @section:
   *   character_mapping
   *
   * @title:
   *   Character Mapping
   *
   * @abstract:
   *   Functions to manage character-to-glyph maps.
   *
   * @description:
   *   This section holds functions and structures that are related to
   *   mapping character input codes to glyph indices.
   *
   *   Note that for many scripts the simplistic approach used by FreeType
   *   of mapping a single character to a single glyph is not valid or
   *   possible!  In general, a higher-level library like HarfBuzz or ICU
   *   should be used for handling text strings.
   *
   * @order:
   *   FT_CharMap
   *   FT_CharMapRec
   *   FT_Encoding
   *   FT_ENC_TAG
   *
   *   FT_Select_Charmap
   *   FT_Set_Charmap
   *   FT_Get_Charmap_Index
   *
   *   FT_Get_Char_Index
   *   FT_Get_First_Char
   *   FT_Get_Next_Char
   *   FT_Load_Char
   *
   */

  /**************************************************************************
   *
   * @section:
   *   information_retrieval
   *
   * @title:
   *   Information Retrieval
   *
   * @abstract:
   *   Functions to retrieve font and glyph information.
   *
   * @description:
   *   Functions to retrieve font and glyph information.  Only some very
   *   basic data is covered; see also the chapter on the format-specific
   *   API for more.
   *
   *
   * @order:
   *   FT_Get_Name_Index
   *   FT_Get_Glyph_Name
   *   FT_Get_Postscript_Name
   *   FT_Get_FSType_Flags
   *   FT_FSTYPE_XXX
   *   FT_Get_SubGlyph_Info
   *   FT_SUBGLYPH_FLAG_XXX
   *
   */

  /**************************************************************************
   *
   * @section:
   *   other_api_data
   *
   * @title:
   *   Other API Data
   *
   * @abstract:
   *   Other structures, enumerations, and macros.
   *
   * @description:
   *   Other structures, enumerations, and macros.  Deprecated functions are
   *   also listed here.
   *
   * @order:
   *   FT_Face_Internal
   *   FT_Size_Internal
   *   FT_Slot_Internal
   *
   *   FT_SubGlyph
   *
   *   FT_HAS_FAST_GLYPHS
   *   FT_Face_CheckTrueTypePatents
   *   FT_Face_SetUnpatentedHinting
   *
   */

  /*************************************************************************/
  /*************************************************************************/
  /*                                                                       */
  /*                        B A S I C   T Y P E S                          */
  /*                                                                       */
  /*************************************************************************/
  /*************************************************************************/

  /**************************************************************************
   *
   * @section:
   *   glyph_retrieval
   *
   */

  /**************************************************************************
   *
   * @struct:
   *   FT_Glyph_Metrics
   *
   * @description:
   *   A structure to model the metrics of a single glyph.  The values are
   *   expressed in 26.6 fractional pixel format; if the flag
   *   @FT_LOAD_NO_SCALE has been used while loading the glyph, values are
   *   expressed in font units instead.
   *
   * @fields:
   *   width ::
   *     The glyph's width.
   *
   *   height ::
   *     The glyph's height.
   *
   *   horiBearingX ::
   *     Left side bearing for horizontal layout.
   *
   *   horiBearingY ::
   *     Top side bearing for horizontal layout.
   *
   *   horiAdvance ::
   *     Advance width for horizontal layout.
   *
   *   vertBearingX ::
   *     Left side bearing for vertical layout.
   *
   *   vertBearingY ::
   *     Top side bearing for vertical layout.  Larger positive values mean
   *     further below the vertical glyph origin.
   *
   *   vertAdvance ::
   *     Advance height for vertical layout.  Positive values mean the glyph
   *     has a positive advance downward.
   *
   * @note:
   *   If not disabled with @FT_LOAD_NO_HINTING, the values represent
   *   dimensions of the hinted glyph (in case hinting is applicable).
   *
   *   Stroking a glyph with an outside border does not increase
   *   `horiAdvance` or `vertAdvance`; you have to manually adjust these
   *   values to account for the added width and height.
   *
   *   FreeType doesn't use the 'VORG' table data for CFF fonts because it
   *   doesn't have an interface to quickly retrieve the glyph height.  The
   *   y~coordinate of the vertical origin can be simply computed as
   *   `vertBearingY + height` after loading a glyph.
   */
  typedef struct  FT_Glyph_Metrics_
  {
	FT_Pos  width;
	FT_Pos  height;

	FT_Pos  horiBearingX;
	FT_Pos  horiBearingY;
	FT_Pos  horiAdvance;

	FT_Pos  vertBearingX;
	FT_Pos  vertBearingY;
	FT_Pos  vertAdvance;

  } FT_Glyph_Metrics;

  /**************************************************************************
   *
   * @section:
   *   sizing_and_scaling
   *
   */

  /**************************************************************************
   *
   * @struct:
   *   FT_Bitmap_Size
   *
   * @description:
   *   This structure models the metrics of a bitmap strike (i.e., a set of
   *   glyphs for a given point size and resolution) in a bitmap font.  It is
   *   used for the `available_sizes` field of @FT_Face.
   *
   * @fields:
   *   height ::
   *     The vertical distance, in pixels, between two consecutive baselines.
   *     It is always positive.
   *
   *   width ::
   *     The average width, in pixels, of all glyphs in the strike.
   *
   *   size ::
   *     The nominal size of the strike in 26.6 fractional points.  This
   *     field is not very useful.
   *
   *   x_ppem ::
   *     The horizontal ppem (nominal width) in 26.6 fractional pixels.
   *
   *   y_ppem ::
   *     The vertical ppem (nominal height) in 26.6 fractional pixels.
   *
   * @note:
   *   Windows FNT:
   *     The nominal size given in a FNT font is not reliable.  If the driver
   *     finds it incorrect, it sets `size` to some calculated values, and
   *     `x_ppem` and `y_ppem` to the pixel width and height given in the
   *     font, respectively.
   *
   *   TrueType embedded bitmaps:
   *     `size`, `width`, and `height` values are not contained in the bitmap
   *     strike itself.  They are computed from the global font parameters.
   */
  typedef struct  FT_Bitmap_Size_
  {
	FT_Short  height;
	FT_Short  width;

	FT_Pos    size;

	FT_Pos    x_ppem;
	FT_Pos    y_ppem;

  } FT_Bitmap_Size;

  /*************************************************************************/
  /*************************************************************************/
  /*                                                                       */
  /*                     O B J E C T   C L A S S E S                       */
  /*                                                                       */
  /*************************************************************************/
  /*************************************************************************/

  /**************************************************************************
   *
   * @section:
   *   library_setup
   *
   */

  /**************************************************************************
   *
   * @type:
   *   FT_Library
   *
   * @description:
   *   A handle to a FreeType library instance.  Each 'library' is completely
   *   independent from the others; it is the 'root' of a set of objects like
   *   fonts, faces, sizes, etc.
   *
   *   It also embeds a memory manager (see @FT_Memory), as well as a
   *   scan-line converter object (see @FT_Raster).
   *
   *   [Since 2.5.6] In multi-threaded applications it is easiest to use one
   *   `FT_Library` object per thread.  In case this is too cumbersome, a
   *   single `FT_Library` object across threads is possible also, as long as
   *   a mutex lock is used around @FT_New_Face and @FT_Done_Face.
   *
   * @note:
   *   Library objects are normally created by @FT_Init_FreeType, and
   *   destroyed with @FT_Done_FreeType.  If you need reference-counting
   *   (cf. @FT_Reference_Library), use @FT_New_Library and @FT_Done_Library.
   */
  typedef struct FT_LibraryRec_  *FT_Library;

  /**************************************************************************
   *
   * @section:
   *   module_management
   *
   */

  /**************************************************************************
   *
   * @type:
   *   FT_Module
   *
   * @description:
   *   A handle to a given FreeType module object.  A module can be a font
   *   driver, a renderer, or anything else that provides services to the
   *   former.
   */
  typedef struct FT_ModuleRec_*  FT_Module;

  /**************************************************************************
   *
   * @type:
   *   FT_Driver
   *
   * @description:
   *   A handle to a given FreeType font driver object.  A font driver is a
   *   module capable of creating faces from font files.
   */
  typedef struct FT_DriverRec_*  FT_Driver;

  /**************************************************************************
   *
   * @type:
   *   FT_Renderer
   *
   * @description:
   *   A handle to a given FreeType renderer.  A renderer is a module in
   *   charge of converting a glyph's outline image to a bitmap.  It supports
   *   a single glyph image format, and one or more target surface depths.
   */
  typedef struct FT_RendererRec_*  FT_Renderer;

  /**************************************************************************
   *
   * @section:
   *   face_creation
   *
   */

  /**************************************************************************
   *
   * @type:
   *   FT_Face
   *
   * @description:
   *   A handle to a typographic face object.  A face object models a given
   *   typeface, in a given style.
   *
   * @note:
   *   A face object also owns a single @FT_GlyphSlot object, as well as one
   *   or more @FT_Size objects.
   *
   *   Use @FT_New_Face or @FT_Open_Face to create a new face object from a
   *   given filepath or a custom input stream.
   *
   *   Use @FT_Done_Face to destroy it (along with its slot and sizes).
   *
   *   An `FT_Face` object can only be safely used from one thread at a time.
   *   Similarly, creation and destruction of `FT_Face` with the same
   *   @FT_Library object can only be done from one thread at a time.  On the
   *   other hand, functions like @FT_Load_Glyph and its siblings are
   *   thread-safe and do not need the lock to be held as long as the same
   *   `FT_Face` object is not used from multiple threads at the same time.
   *
   * @also:
   *   See @FT_FaceRec for the publicly accessible fields of a given face
   *   object.
   */
  typedef struct FT_FaceRec_*  FT_Face;

  /**************************************************************************
   *
   * @section:
   *   sizing_and_scaling
   *
   */

  /**************************************************************************
   *
   * @type:
   *   FT_Size
   *
   * @description:
   *   A handle to an object that models a face scaled to a given character
   *   size.
   *
   * @note:
   *   An @FT_Face has one _active_ `FT_Size` object that is used by
   *   functions like @FT_Load_Glyph to determine the scaling transformation
   *   that in turn is used to load and hint glyphs and metrics.
   *
   *   A newly created `FT_Size` object contains only meaningless zero values.
   *   You must use @FT_Set_Char_Size, @FT_Set_Pixel_Sizes, @FT_Request_Size
   *   or even @FT_Select_Size to change the content (i.e., the scaling
   *   values) of the active `FT_Size`.  Otherwise, the scaling and hinting
   *   will not be performed.
   *
   *   You can use @FT_New_Size to create additional size objects for a given
   *   @FT_Face, but they won't be used by other functions until you activate
   *   it through @FT_Activate_Size.  Only one size can be activated at any
   *   given time per face.
   *
   * @also:
   *   See @FT_SizeRec for the publicly accessible fields of a given size
   *   object.
   */
  typedef struct FT_SizeRec_*  FT_Size;

  /**************************************************************************
   *
   * @section:
   *   glyph_retrieval
   *
   */

  /**************************************************************************
   *
   * @type:
   *   FT_GlyphSlot
   *
   * @description:
   *   A handle to a given 'glyph slot'.  A slot is a container that can hold
   *   any of the glyphs contained in its parent face.
   *
   *   In other words, each time you call @FT_Load_Glyph or @FT_Load_Char,
   *   the slot's content is erased by the new glyph data, i.e., the glyph's
   *   metrics, its image (bitmap or outline), and other control information.
   *
   * @also:
   *   See @FT_GlyphSlotRec for the publicly accessible glyph fields.
   */
  typedef struct FT_GlyphSlotRec_*  FT_GlyphSlot;

  /**************************************************************************
   *
   * @section:
   *   character_mapping
   *
   */

  /**************************************************************************
   *
   * @type:
   *   FT_CharMap
   *
   * @description:
   *   A handle to a character map (usually abbreviated to 'charmap').  A
   *   charmap is used to translate character codes in a given encoding into
   *   glyph indexes for its parent's face.  Some font formats may provide
   *   several charmaps per font.
   *
   *   Each face object owns zero or more charmaps, but only one of them can
   *   be 'active', providing the data used by @FT_Get_Char_Index or
   *   @FT_Load_Char.
   *
   *   The list of available charmaps in a face is available through the
   *   `face->num_charmaps` and `face->charmaps` fields of @FT_FaceRec.
   *
   *   The currently active charmap is available as `face->charmap`.  You
   *   should call @FT_Set_Charmap to change it.
   *
   * @note:
   *   When a new face is created (either through @FT_New_Face or
   *   @FT_Open_Face), the library looks for a Unicode charmap within the
   *   list and automatically activates it.  If there is no Unicode charmap,
   *   FreeType doesn't set an 'active' charmap.
   *
   * @also:
   *   See @FT_CharMapRec for the publicly accessible fields of a given
   *   character map.
   */
  typedef struct FT_CharMapRec_*  FT_CharMap;

  /**************************************************************************
   *
   * @macro:
   *   FT_ENC_TAG
   *
   * @description:
   *   This macro converts four-letter tags into an unsigned long.  It is
   *   used to define 'encoding' identifiers (see @FT_Encoding).
   *
   * @note:
   *   Since many 16-bit compilers don't like 32-bit enumerations, you should
   *   redefine this macro in case of problems to something like this:
   *
   *   ```
   *     #define FT_ENC_TAG( value, a, b, c, d )  value
   *   ```
   *
   *   to get a simple enumeration without assigning special numbers.
   */

#ifndef FT_ENC_TAG

#define FT_ENC_TAG( value, a, b, c, d )                             \
		  value = ( ( FT_STATIC_BYTE_CAST( FT_UInt32, a ) << 24 ) | \
					( FT_STATIC_BYTE_CAST( FT_UInt32, b ) << 16 ) | \
					( FT_STATIC_BYTE_CAST( FT_UInt32, c ) <<  8 ) | \
					  FT_STATIC_BYTE_CAST( FT_UInt32, d )         )

#endif /* FT_ENC_TAG */

  /**************************************************************************
   *
   * @enum:
   *   FT_Encoding
   *
   * @description:
   *   An enumeration to specify character sets supported by charmaps.  Used
   *   in the @FT_Select_Charmap API function.
   *
   * @note:
   *   Despite the name, this enumeration lists specific character
   *   repertoires (i.e., charsets), and not text encoding methods (e.g.,
   *   UTF-8, UTF-16, etc.).
   *
   *   Other encodings might be defined in the future.
   *
   * @values:
   *   FT_ENCODING_NONE ::
   *     The encoding value~0 is reserved for all formats except BDF, PCF,
   *     and Windows FNT; see below for more information.
   *
   *   FT_ENCODING_UNICODE ::
   *     The Unicode character set.  This value covers all versions of the
   *     Unicode repertoire, including ASCII and Latin-1.  Most fonts include
   *     a Unicode charmap, but not all of them.
   *
   *     For example, if you want to access Unicode value U+1F028 (and the
   *     font contains it), use value 0x1F028 as the input value for
   *     @FT_Get_Char_Index.
   *
   *   FT_ENCODING_MS_SYMBOL ::
   *     Microsoft Symbol encoding, used to encode mathematical symbols and
   *     wingdings.  For more information, see
   *     'https://www.microsoft.com/typography/otspec/recom.htm#non-standard-symbol-fonts',
   *     'http://www.kostis.net/charsets/symbol.htm', and
   *     'http://www.kostis.net/charsets/wingding.htm'.
   *
   *     This encoding uses character codes from the PUA (Private Unicode
   *     Area) in the range U+F020-U+F0FF.
   *
   *   FT_ENCODING_SJIS ::
   *     Shift JIS encoding for Japanese.  More info at
   *     'https://en.wikipedia.org/wiki/Shift_JIS'.  See note on multi-byte
   *     encodings below.
   *
   *   FT_ENCODING_PRC ::
   *     Corresponds to encoding systems mainly for Simplified Chinese as
   *     used in People's Republic of China (PRC).  The encoding layout is
   *     based on GB~2312 and its supersets GBK and GB~18030.
   *
   *   FT_ENCODING_BIG5 ::
   *     Corresponds to an encoding system for Traditional Chinese as used in
   *     Taiwan and Hong Kong.
   *
   *   FT_ENCODING_WANSUNG ::
   *     Corresponds to the Korean encoding system known as Extended Wansung
   *     (MS Windows code page 949).  For more information see
   *     'https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WindowsBestFit/bestfit949.txt'.
   *
   *   FT_ENCODING_JOHAB ::
   *     The Korean standard character set (KS~C 5601-1992), which
   *     corresponds to MS Windows code page 1361.  This character set
   *     includes all possible Hangul character combinations.
   *
   *   FT_ENCODING_ADOBE_LATIN_1 ::
   *     Corresponds to a Latin-1 encoding as defined in a Type~1 PostScript
   *     font.  It is limited to 256 character codes.
   *
   *   FT_ENCODING_ADOBE_STANDARD ::
   *     Adobe Standard encoding, as found in Type~1, CFF, and OpenType/CFF
   *     fonts.  It is limited to 256 character codes.
   *
   *   FT_ENCODING_ADOBE_EXPERT ::
   *     Adobe Expert encoding, as found in Type~1, CFF, and OpenType/CFF
   *     fonts.  It is limited to 256 character codes.
   *
   *   FT_ENCODING_ADOBE_CUSTOM ::
   *     Corresponds to a custom encoding, as found in Type~1, CFF, and
   *     OpenType/CFF fonts.  It is limited to 256 character codes.
   *
   *   FT_ENCODING_APPLE_ROMAN ::
   *     Apple roman encoding.  Many TrueType and OpenType fonts contain a
   *     charmap for this 8-bit encoding, since older versions of Mac OS are
   *     able to use it.
   *
   *   FT_ENCODING_OLD_LATIN_2 ::
   *     This value is deprecated and was neither used nor reported by
   *     FreeType.  Don't use or test for it.
   *
   *   FT_ENCODING_MS_SJIS ::
   *     Same as FT_ENCODING_SJIS.  Deprecated.
   *
   *   FT_ENCODING_MS_GB2312 ::
   *     Same as FT_ENCODING_PRC.  Deprecated.
   *
   *   FT_ENCODING_MS_BIG5 ::
   *     Same as FT_ENCODING_BIG5.  Deprecated.
   *
   *   FT_ENCODING_MS_WANSUNG ::
   *     Same as FT_ENCODING_WANSUNG.  Deprecated.
   *
   *   FT_ENCODING_MS_JOHAB ::
   *     Same as FT_ENCODING_JOHAB.  Deprecated.
   *
   * @note:
   *   When loading a font, FreeType makes a Unicode charmap active if
   *   possible (either if the font provides such a charmap, or if FreeType
   *   can synthesize one from PostScript glyph name dictionaries; in either
   *   case, the charmap is tagged with `FT_ENCODING_UNICODE`).  If such a
   *   charmap is synthesized, it is placed at the first position of the
   *   charmap array.
   *
   *   All other encodings are considered legacy and tagged only if
   *   explicitly defined in the font file.  Otherwise, `FT_ENCODING_NONE` is
   *   used.
   *
   *   `FT_ENCODING_NONE` is set by the BDF and PCF drivers if the charmap is
   *   neither Unicode nor ISO-8859-1 (otherwise it is set to
   *   `FT_ENCODING_UNICODE`).  Use @FT_Get_BDF_Charset_ID to find out which
   *   encoding is really present.  If, for example, the `cs_registry` field
   *   is 'KOI8' and the `cs_encoding` field is 'R', the font is encoded in
   *   KOI8-R.
   *
   *   `FT_ENCODING_NONE` is always set (with a single exception) by the
   *   winfonts driver.  Use @FT_Get_WinFNT_Header and examine the `charset`
   *   field of the @FT_WinFNT_HeaderRec structure to find out which encoding
   *   is really present.  For example, @FT_WinFNT_ID_CP1251 (204) means
   *   Windows code page 1251 (for Russian).
   *
   *   `FT_ENCODING_NONE` is set if `platform_id` is @TT_PLATFORM_MACINTOSH
   *   and `encoding_id` is not `TT_MAC_ID_ROMAN` (otherwise it is set to
   *   `FT_ENCODING_APPLE_ROMAN`).
   *
   *   If `platform_id` is @TT_PLATFORM_MACINTOSH, use the function
   *   @FT_Get_CMap_Language_ID to query the Mac language ID that may be
   *   needed to be able to distinguish Apple encoding variants.  See
   *
   *     https://www.unicode.org/Public/MAPPINGS/VENDORS/APPLE/Readme.txt
   *
   *   to get an idea how to do that.  Basically, if the language ID is~0,
   *   don't use it, otherwise subtract 1 from the language ID.  Then examine
   *   `encoding_id`.  If, for example, `encoding_id` is `TT_MAC_ID_ROMAN`
   *   and the language ID (minus~1) is `TT_MAC_LANGID_GREEK`, it is the
   *   Greek encoding, not Roman.  `TT_MAC_ID_ARABIC` with
   *   `TT_MAC_LANGID_FARSI` means the Farsi variant of the Arabic encoding.
   */
  typedef enum  FT_Encoding_
  {
	FT_ENC_TAG( FT_ENCODING_NONE, 0, 0, 0, 0 ),

	FT_ENC_TAG( FT_ENCODING_MS_SYMBOL, 's', 'y', 'm', 'b' ),
	FT_ENC_TAG( FT_ENCODING_UNICODE,   'u', 'n', 'i', 'c' ),

	FT_ENC_TAG( FT_ENCODING_SJIS,    's', 'j', 'i', 's' ),
	FT_ENC_TAG( FT_ENCODING_PRC,     'g', 'b', ' ', ' ' ),
	FT_ENC_TAG( FT_ENCODING_BIG5,    'b', 'i', 'g', '5' ),
	FT_ENC_TAG( FT_ENCODING_WANSUNG, 'w', 'a', 'n', 's' ),
	FT_ENC_TAG( FT_ENCODING_JOHAB,   'j', 'o', 'h', 'a' ),

	/* for backward compatibility */
	FT_ENCODING_GB2312     = FT_ENCODING_PRC,
	FT_ENCODING_MS_SJIS    = FT_ENCODING_SJIS,
	FT_ENCODING_MS_GB2312  = FT_ENCODING_PRC,
	FT_ENCODING_MS_BIG5    = FT_ENCODING_BIG5,
	FT_ENCODING_MS_WANSUNG = FT_ENCODING_WANSUNG,
	FT_ENCODING_MS_JOHAB   = FT_ENCODING_JOHAB,

	FT_ENC_TAG( FT_ENCODING_ADOBE_STANDARD, 'A', 'D', 'O', 'B' ),
	FT_ENC_TAG( FT_ENCODING_ADOBE_EXPERT,   'A', 'D', 'B', 'E' ),
	FT_ENC_TAG( FT_ENCODING_ADOBE_CUSTOM,   'A', 'D', 'B', 'C' ),
	FT_ENC_TAG( FT_ENCODING_ADOBE_LATIN_1,  'l', 'a', 't', '1' ),

	FT_ENC_TAG( FT_ENCODING_OLD_LATIN_2, 'l', 'a', 't', '2' ),

	FT_ENC_TAG( FT_ENCODING_APPLE_ROMAN, 'a', 'r', 'm', 'n' )

  } FT_Encoding;

  /* these constants are deprecated; use the corresponding `FT_Encoding` */
  /* values instead                                                      */
#define ft_encoding_none            FT_ENCODING_NONE
#define ft_encoding_unicode         FT_ENCODING_UNICODE
#define ft_encoding_symbol          FT_ENCODING_MS_SYMBOL
#define ft_encoding_latin_1         FT_ENCODING_ADOBE_LATIN_1
#define ft_encoding_latin_2         FT_ENCODING_OLD_LATIN_2
#define ft_encoding_sjis            FT_ENCODING_SJIS
#define ft_encoding_gb2312          FT_ENCODING_PRC
#define ft_encoding_big5            FT_ENCODING_BIG5
#define ft_encoding_wansung         FT_ENCODING_WANSUNG
#define ft_encoding_johab           FT_ENCODING_JOHAB

#define ft_encoding_adobe_standard  FT_ENCODING_ADOBE_STANDARD
#define ft_encoding_adobe_expert    FT_ENCODING_ADOBE_EXPERT
#define ft_encoding_adobe_custom    FT_ENCODING_ADOBE_CUSTOM
#define ft_encoding_apple_roman     FT_ENCODING_APPLE_ROMAN

  /**************************************************************************
   *
   * @struct:
   *   FT_CharMapRec
   *
   * @description:
   *   The base charmap structure.
   *
   * @fields:
   *   face ::
   *     A handle to the parent face object.
   *
   *   encoding ::
   *     An @FT_Encoding tag identifying the charmap.  Use this with
   *     @FT_Select_Charmap.
   *
   *   platform_id ::
   *     An ID number describing the platform for the following encoding ID.
   *     This comes directly from the TrueType specification and gets
   *     emulated for other formats.
   *
   *   encoding_id ::
   *     A platform-specific encoding number.  This also comes from the
   *     TrueType specification and gets emulated similarly.
   */
  typedef struct  FT_CharMapRec_
  {
	FT_Face      face;
	FT_Encoding  encoding;
	FT_UShort    platform_id;
	FT_UShort    encoding_id;

  } FT_CharMapRec;

  /*************************************************************************/
  /*************************************************************************/
  /*                                                                       */
  /*                 B A S E   O B J E C T   C L A S S E S                 */
  /*                                                                       */
  /*************************************************************************/
  /*************************************************************************/

  /**************************************************************************
   *
   * @section:
   *   other_api_data
   *
   */

  /**************************************************************************
   *
   * @type:
   *   FT_Face_Internal
   *
   * @description:
   *   An opaque handle to an `FT_Face_InternalRec` structure that models the
   *   private data of a given @FT_Face object.
   *
   *   This structure might change between releases of FreeType~2 and is not
   *   generally available to client applications.
   */
  typedef struct FT_Face_InternalRec_*  FT_Face_Internal;

  /**************************************************************************
   *
   * @section:
   *   face_creation
   *
   */

  /**************************************************************************
   *
   * @struct:
   *   FT_FaceRec
   *
   * @description:
   *   FreeType root face class structure.  A face object models a typeface
   *   in a font file.
   *
   * @fields:
   *   num_faces ::
   *     The number of faces in the font file.  Some font formats can have
   *     multiple faces in a single font file.
   *
   *   face_index ::
   *     This field holds two different values.  Bits 0-15 are the index of
   *     the face in the font file (starting with value~0).  They are set
   *     to~0 if there is only one face in the font file.
   *
   *     [Since 2.6.1] Bits 16-30 are relevant to GX and OpenType variation
   *     fonts only, holding the named instance index for the current face
   *     index (starting with value~1; value~0 indicates font access without
   *     a named instance).  For non-variation fonts, bits 16-30 are ignored.
   *     If we have the third named instance of face~4, say, `face_index` is
   *     set to 0x00030004.
   *
   *     Bit 31 is always zero (that is, `face_index` is always a positive
   *     value).
   *
   *     [Since 2.9] Changing the design coordinates with
   *     @FT_Set_Var_Design_Coordinates or @FT_Set_Var_Blend_Coordinates does
   *     not influence the named instance index value (only
   *     @FT_Set_Named_Instance does that).
   *
   *   face_flags ::
   *     A set of bit flags that give important information about the face;
   *     see @FT_FACE_FLAG_XXX for the details.
   *
   *   style_flags ::
   *     The lower 16~bits contain a set of bit flags indicating the style of
   *     the face; see @FT_STYLE_FLAG_XXX for the details.
   *
   *     [Since 2.6.1] Bits 16-30 hold the number of named instances
   *     available for the current face if we have a GX or OpenType variation
   *     (sub)font.  Bit 31 is always zero (that is, `style_flags` is always
   *     a positive value).  Note that a variation font has always at least
   *     one named instance, namely the default instance.
   *
   *   num_glyphs ::
   *     The number of glyphs in the face.  If the face is scalable and has
   *     sbits (see `num_fixed_sizes`), it is set to the number of outline
   *     glyphs.
   *
   *     For CID-keyed fonts (not in an SFNT wrapper) this value gives the
   *     highest CID used in the font.
   *
   *   family_name ::
   *     The face's family name.  This is an ASCII string, usually in
   *     English, that describes the typeface's family (like 'Times New
   *     Roman', 'Bodoni', 'Garamond', etc).  This is a least common
   *     denominator used to list fonts.  Some formats (TrueType & OpenType)
   *     provide localized and Unicode versions of this string.  Applications
   *     should use the format-specific interface to access them.  Can be
   *     `NULL` (e.g., in fonts embedded in a PDF file).
   *
   *     In case the font doesn't provide a specific family name entry,
   *     FreeType tries to synthesize one, deriving it from other name
   *     entries.
   *
   *   style_name ::
   *     The face's style name.  This is an ASCII string, usually in English,
   *     that describes the typeface's style (like 'Italic', 'Bold',
   *     'Condensed', etc).  Not all font formats provide a style name, so
   *     this field is optional, and can be set to `NULL`.  As for
   *     `family_name`, some formats provide localized and Unicode versions
   *     of this string.  Applications should use the format-specific
   *     interface to access them.
   *
   *   num_fixed_sizes ::
   *     The number of bitmap strikes in the face.  Even if the face is
   *     scalable, there might still be bitmap strikes, which are called
   *     'sbits' in that case.
   *
   *   available_sizes ::
   *     An array of @FT_Bitmap_Size for all bitmap strikes in the face.  It
   *     is set to `NULL` if there is no bitmap strike.
   *
   *     Note that FreeType tries to sanitize the strike data since they are
   *     sometimes sloppy or incorrect, but this can easily fail.
   *
   *   num_charmaps ::
   *     The number of charmaps in the face.
   *
   *   charmaps ::
   *     An array of the charmaps of the face.
   *
   *   generic ::
   *     A field reserved for client uses.  See the @FT_Generic type
   *     description.
   *
   *   bbox ::
   *     The font bounding box.  Coordinates are expressed in font units (see
   *     `units_per_EM`).  The box is large enough to contain any glyph from
   *     the font.  Thus, `bbox.yMax` can be seen as the 'maximum ascender',
   *     and `bbox.yMin` as the 'minimum descender'.  Only relevant for
   *     scalable formats.
   *
   *     Note that the bounding box might be off by (at least) one pixel for
   *     hinted fonts.  See @FT_Size_Metrics for further discussion.
   *
   *     Note that the bounding box does not vary in OpenType variation fonts
   *     and should only be used in relation to the default instance.
   *
   *   units_per_EM ::
   *     The number of font units per EM square for this face.  This is
   *     typically 2048 for TrueType fonts, and 1000 for Type~1 fonts.  Only
   *     relevant for scalable formats.
   *
   *   ascender ::
   *     The typographic ascender of the face, expressed in font units.  For
   *     font formats not having this information, it is set to `bbox.yMax`.
   *     Only relevant for scalable formats.
   *
   *   descender ::
   *     The typographic descender of the face, expressed in font units.  For
   *     font formats not having this information, it is set to `bbox.yMin`.
   *     Note that this field is negative for values below the baseline.
   *     Only relevant for scalable formats.
   *
   *   height ::
   *     This value is the vertical distance between two consecutive
   *     baselines, expressed in font units.  It is always positive.  Only
   *     relevant for scalable formats.
   *
   *     If you want the global glyph height, use `ascender - descender`.
   *
   *   max_advance_width ::
   *     The maximum advance width, in font units, for all glyphs in this
   *     face.  This can be used to make word wrapping computations faster.
   *     Only relevant for scalable formats.
   *
   *   max_advance_height ::
   *     The maximum advance height, in font units, for all glyphs in this
   *     face.  This is only relevant for vertical layouts, and is set to
   *     `height` for fonts that do not provide vertical metrics.  Only
   *     relevant for scalable formats.
   *
   *   underline_position ::
   *     The position, in font units, of the underline line for this face.
   *     It is the center of the underlining stem.  Only relevant for
   *     scalable formats.
   *
   *   underline_thickness ::
   *     The thickness, in font units, of the underline for this face.  Only
   *     relevant for scalable formats.
   *
   *   glyph ::
   *     The face's associated glyph slot(s).
   *
   *   size ::
   *     The current active size for this face.
   *
   *   charmap ::
   *     The current active charmap for this face.
   *
   * @note:
   *   Fields may be changed after a call to @FT_Attach_File or
   *   @FT_Attach_Stream.
   *
   *   For an OpenType variation font, the values of the following fields can
   *   change after a call to @FT_Set_Var_Design_Coordinates (and friends) if
   *   the font contains an 'MVAR' table: `ascender`, `descender`, `height`,
   *   `underline_position`, and `underline_thickness`.
   *
   *   Especially for TrueType fonts see also the documentation for
   *   @FT_Size_Metrics.
   */
  typedef struct  FT_FaceRec_
  {
	FT_Long           num_faces;
	FT_Long           face_index;

	FT_Long           face_flags;
	FT_Long           style_flags;

	FT_Long           num_glyphs;

	FT_String*        family_name;
	FT_String*        style_name;

	FT_Int            num_fixed_sizes;
	FT_Bitmap_Size*   available_sizes;

	FT_Int            num_charmaps;
	FT_CharMap*       charmaps;

	FT_Generic        generic;

	/* The following member variables (down to `underline_thickness`) */
	/* are only relevant to scalable outlines; cf. @FT_Bitmap_Size    */
	/* for bitmap fonts.                                              */
	FT_BBox           bbox;

	FT_UShort         units_per_EM;
	FT_Short          ascender;
	FT_Short          descender;
	FT_Short          height;

	FT_Short          max_advance_width;
	FT_Short          max_advance_height;

	FT_Short          underline_position;
	FT_Short          underline_thickness;

	FT_GlyphSlot      glyph;
	FT_Size           size;
	FT_CharMap        charmap;

	/* private fields, internal to FreeType */

	FT_Driver         driver;
	FT_Memory         memory;
	FT_Stream         stream;

	FT_ListRec        sizes_list;

	FT_Generic        autohint;   /* face-specific auto-hinter data */
	void*             extensions; /* unused                         */

	FT_Face_Internal  internal;

  } FT_FaceRec;

  /**************************************************************************
   *
   * @enum:
   *   FT_FACE_FLAG_XXX
   *
   * @description:
   *   A list of bit flags used in the `face_flags` field of the @FT_FaceRec
   *   structure.  They inform client applications of properties of the
   *   corresponding face.
   *
   * @values:
   *   FT_FACE_FLAG_SCALABLE ::
   *     The face contains outline glyphs.  Note that a face can contain
   *     bitmap strikes also, i.e., a face can have both this flag and
   *     @FT_FACE_FLAG_FIXED_SIZES set.
   *
   *   FT_FACE_FLAG_FIXED_SIZES ::
   *     The face contains bitmap strikes.  See also the `num_fixed_sizes`
   *     and `available_sizes` fields of @FT_FaceRec.
   *
   *   FT_FACE_FLAG_FIXED_WIDTH ::
   *     The face contains fixed-width characters (like Courier, Lucida,
   *     MonoType, etc.).
   *
   *   FT_FACE_FLAG_SFNT ::
   *     The face uses the SFNT storage scheme.  For now, this means TrueType
   *     and OpenType.
   *
   *   FT_FACE_FLAG_HORIZONTAL ::
   *     The face contains horizontal glyph metrics.  This should be set for
   *     all common formats.
   *
   *   FT_FACE_FLAG_VERTICAL ::
   *     The face contains vertical glyph metrics.  This is only available in
   *     some formats, not all of them.
   *
   *   FT_FACE_FLAG_KERNING ::
   *     The face contains kerning information.  If set, the kerning distance
   *     can be retrieved using the function @FT_Get_Kerning.  Otherwise the
   *     function always returns the vector (0,0).
   *
   *     Note that for TrueType fonts only, FreeType supports both the 'kern'
   *     table and the basic, pair-wise kerning feature from the 'GPOS' table
   *     (with `TT_CONFIG_OPTION_GPOS_KERNING` enabled), though FreeType does
   *     not support the more advanced GPOS layout features; use a library
   *     like HarfBuzz for those instead.
   *
   *   FT_FACE_FLAG_FAST_GLYPHS ::
   *     THIS FLAG IS DEPRECATED.  DO NOT USE OR TEST IT.
   *
   *   FT_FACE_FLAG_MULTIPLE_MASTERS ::
   *     The face contains multiple masters and is capable of interpolating
   *     between them.  Supported formats are Adobe MM, TrueType GX, and
   *     OpenType variation fonts.
   *
   *     See section @multiple_masters for API details.
   *
   *   FT_FACE_FLAG_GLYPH_NAMES ::
   *     The face contains glyph names, which can be retrieved using
   *     @FT_Get_Glyph_Name.  Note that some TrueType fonts contain broken
   *     glyph name tables.  Use the function @FT_Has_PS_Glyph_Names when
   *     needed.
   *
   *   FT_FACE_FLAG_EXTERNAL_STREAM ::
   *     Used internally by FreeType to indicate that a face's stream was
   *     provided by the client application and should not be destroyed when
   *     @FT_Done_Face is called.  Don't read or test this flag.
   *
   *   FT_FACE_FLAG_HINTER ::
   *     The font driver has a hinting machine of its own.  For example, with
   *     TrueType fonts, it makes sense to use data from the SFNT 'gasp'
   *     table only if the native TrueType hinting engine (with the bytecode
   *     interpreter) is available and active.
   *
   *   FT_FACE_FLAG_CID_KEYED ::
   *     The face is CID-keyed.  In that case, the face is not accessed by
   *     glyph indices but by CID values.  For subsetted CID-keyed fonts this
   *     has the consequence that not all index values are a valid argument
   *     to @FT_Load_Glyph.  Only the CID values for which corresponding
   *     glyphs in the subsetted font exist make `FT_Load_Glyph` return
   *     successfully; in all other cases you get an
   *     `FT_Err_Invalid_Argument` error.
   *
   *     Note that CID-keyed fonts that are in an SFNT wrapper (that is, all
   *     OpenType/CFF fonts) don't have this flag set since the glyphs are
   *     accessed in the normal way (using contiguous indices); the
   *     'CID-ness' isn't visible to the application.
   *
   *   FT_FACE_FLAG_TRICKY ::
   *     The face is 'tricky', that is, it always needs the font format's
   *     native hinting engine to get a reasonable result.  A typical example
   *     is the old Chinese font `mingli.ttf` (but not `mingliu.ttc`) that
   *     uses TrueType bytecode instructions to move and scale all of its
   *     subglyphs.
   *
   *     It is not possible to auto-hint such fonts using
   *     @FT_LOAD_FORCE_AUTOHINT; it will also ignore @FT_LOAD_NO_HINTING.
   *     You have to set both @FT_LOAD_NO_HINTING and @FT_LOAD_NO_AUTOHINT to
   *     really disable hinting; however, you probably never want this except
   *     for demonstration purposes.
   *
   *     Currently, there are about a dozen TrueType fonts in the list of
   *     tricky fonts; they are hard-coded in file `ttobjs.c`.
   *
   *   FT_FACE_FLAG_COLOR ::
   *     [Since 2.5.1] The face has color glyph tables.  See @FT_LOAD_COLOR
   *     for more information.
   *
   *   FT_FACE_FLAG_VARIATION ::
   *     [Since 2.9] Set if the current face (or named instance) has been
   *     altered with @FT_Set_MM_Design_Coordinates,
   *     @FT_Set_Var_Design_Coordinates, @FT_Set_Var_Blend_Coordinates, or
   *     @FT_Set_MM_WeightVector to select a non-default instance.
   *
   *   FT_FACE_FLAG_SVG ::
   *     [Since 2.12] The face has an 'SVG~' OpenType table.
   *
   *   FT_FACE_FLAG_SBIX ::
   *     [Since 2.12] The face has an 'sbix' OpenType table *and* outlines.
   *     For such fonts, @FT_FACE_FLAG_SCALABLE is not set by default to
   *     retain backward compatibility.
   *
   *   FT_FACE_FLAG_SBIX_OVERLAY ::
   *     [Since 2.12] The face has an 'sbix' OpenType table where outlines
   *     should be drawn on top of bitmap strikes.
   *
   */
#define FT_FACE_FLAG_SCALABLE          ( 1L <<  0 )
#define FT_FACE_FLAG_FIXED_SIZES       ( 1L <<  1 )
#define FT_FACE_FLAG_FIXED_WIDTH       ( 1L <<  2 )
#define FT_FACE_FLAG_SFNT              ( 1L <<  3 )
#define FT_FACE_FLAG_HORIZONTAL        ( 1L <<  4 )
#define FT_FACE_FLAG_VERTICAL          ( 1L <<  5 )
#define FT_FACE_FLAG_KERNING           ( 1L <<  6 )
#define FT_FACE_FLAG_FAST_GLYPHS       ( 1L <<  7 )
#define FT_FACE_FLAG_MULTIPLE_MASTERS  ( 1L <<  8 )
#define FT_FACE_FLAG_GLYPH_NAMES       ( 1L <<  9 )
#define FT_FACE_FLAG_EXTERNAL_STREAM   ( 1L << 10 )
#define FT_FACE_FLAG_HINTER            ( 1L << 11 )
#define FT_FACE_FLAG_CID_KEYED         ( 1L << 12 )
#define FT_FACE_FLAG_TRICKY            ( 1L << 13 )
#define FT_FACE_FLAG_COLOR             ( 1L << 14 )
#define FT_FACE_FLAG_VARIATION         ( 1L << 15 )
#define FT_FACE_FLAG_SVG               ( 1L << 16 )
#define FT_FACE_FLAG_SBIX              ( 1L << 17 )
#define FT_FACE_FLAG_SBIX_OVERLAY      ( 1L << 18 )

  /**************************************************************************
   *
   * @section:
   *   font_testing_macros
   *
   */

  /**************************************************************************
   *
   * @macro:
   *   FT_HAS_HORIZONTAL
   *
   * @description:
   *   A macro that returns true whenever a face object contains horizontal
   *   metrics (this is true for all font formats though).
   *
   * @also:
   *   @FT_HAS_VERTICAL can be used to check for vertical metrics.
   *
   */
#define FT_HAS_HORIZONTAL( face ) \
		  ( !!( (face)->face_flags & FT_FACE_FLAG_HORIZONTAL ) )

  /**************************************************************************
   *
   * @macro:
   *   FT_HAS_VERTICAL
   *
   * @description:
   *   A macro that returns true whenever a face object contains real
   *   vertical metrics (and not only synthesized ones).
   *
   */
#define FT_HAS_VERTICAL( face ) \
		  ( !!( (face)->face_flags & FT_FACE_FLAG_VERTICAL ) )

  /**************************************************************************
   *
   * @macro:
   *   FT_HAS_KERNING
   *
   * @description:
   *   A macro that returns true whenever a face object contains kerning data
   *   that can be accessed with @FT_Get_Kerning.
   *
   */
#define FT_HAS_KERNING( face ) \
		  ( !!( (face)->face_flags & FT_FACE_FLAG_KERNING ) )

  /**************************************************************************
   *
   * @macro:
   *   FT_IS_SCALABLE
   *
   * @description:
   *   A macro that returns true whenever a face object contains a scalable
   *   font face (true for TrueType, Type~1, Type~42, CID, OpenType/CFF, and
   *   PFR font formats).
   *
   */
#define FT_IS_SCALABLE( face ) \
		  ( !!( (face)->face_flags & FT_FACE_FLAG_SCALABLE ) )

  /**************************************************************************
   *
   * @macro:
   *   FT_IS_SFNT
   *
   * @description:
   *   A macro that returns true whenever a face object contains a font whose
   *   format is based on the SFNT storage scheme.  This usually means:
   *   TrueType fonts, OpenType fonts, as well as SFNT-based embedded bitmap
   *   fonts.
   *
   *   If this macro is true, all functions defined in @FT_SFNT_NAMES_H and
   *   @FT_TRUETYPE_TABLES_H are available.
   *
   */
#define FT_IS_SFNT( face ) \
		  ( !!( (face)->face_flags & FT_FACE_FLAG_SFNT ) )

  /**************************************************************************
   *
   * @macro:
   *   FT_IS_FIXED_WIDTH
   *
   * @description:
   *   A macro that returns true whenever a face object contains a font face
   *   that contains fixed-width (or 'monospace', 'fixed-pitch', etc.)
   *   glyphs.
   *
   */
#define FT_IS_FIXED_WIDTH( face ) \
		  ( !!( (face)->face_flags & FT_FACE_FLAG_FIXED_WIDTH ) )

  /**************************************************************************
   *
   * @macro:
   *   FT_HAS_FIXED_SIZES
   *
   * @description:
   *   A macro that returns true whenever a face object contains some
   *   embedded bitmaps.  See the `available_sizes` field of the @FT_FaceRec
   *   structure.
   *
   */
#define FT_HAS_FIXED_SIZES( face ) \
		  ( !!( (face)->face_flags & FT_FACE_FLAG_FIXED_SIZES ) )

  /**************************************************************************
   *
   * @section:
   *   other_api_data
   *
   */

  /**************************************************************************
   *
   * @macro:
   *   FT_HAS_FAST_GLYPHS
   *
   * @description:
   *   Deprecated.
   *
   */
#define FT_HAS_FAST_GLYPHS( face )  0

  /**************************************************************************
   *
   * @section:
   *   font_testing_macros
   *
   */

  /**************************************************************************
   *
   * @macro:
   *   FT_HAS_GLYPH_NAMES
   *
   * @description:
   *   A macro that returns true whenever a face object contains some glyph
   *   names that can be accessed through @FT_Get_Glyph_Name.
   *
   */
#define FT_HAS_GLYPH_NAMES( face ) \
		  ( !!( (face)->face_flags & FT_FACE_FLAG_GLYPH_NAMES ) )

  /**************************************************************************
   *
   * @macro:
   *   FT_HAS_MULTIPLE_MASTERS
   *
   * @description:
   *   A macro that returns true whenever a face object contains some
   *   multiple masters.  The functions provided by @FT_MULTIPLE_MASTERS_H
   *   are then available to choose the exact design you want.
   *
   */
#define FT_HAS_MULTIPLE_MASTERS( face ) \
		  ( !!( (face)->face_flags & FT_FACE_FLAG_MULTIPLE_MASTERS ) )

  /**************************************************************************
   *
   * @macro:
   *   FT_IS_NAMED_INSTANCE
   *
   * @description:
   *   A macro that returns true whenever a face object is a named instance
   *   of a GX or OpenType variation font.
   *
   *   [Since 2.9] Changing the design coordinates with
   *   @FT_Set_Var_Design_Coordinates or @FT_Set_Var_Blend_Coordinates does
   *   not influence the return value of this macro (only
   *   @FT_Set_Named_Instance does that).
   *
   * @since:
   *   2.7
   *
   */
#define FT_IS_NAMED_INSTANCE( face ) \
		  ( !!( (face)->face_index & 0x7FFF0000L ) )

  /**************************************************************************
   *
   * @macro:
   *   FT_IS_VARIATION
   *
   * @description:
   *   A macro that returns true whenever a face object has been altered by
   *   @FT_Set_MM_Design_Coordinates, @FT_Set_Var_Design_Coordinates,
   *   @FT_Set_Var_Blend_Coordinates, or @FT_Set_MM_WeightVector.
   *
   * @since:
   *   2.9
   *
   */
#define FT_IS_VARIATION( face ) \
		  ( !!( (face)->face_flags & FT_FACE_FLAG_VARIATION ) )

  /**************************************************************************
   *
   * @macro:
   *   FT_IS_CID_KEYED
   *
   * @description:
   *   A macro that returns true whenever a face object contains a CID-keyed
   *   font.  See the discussion of @FT_FACE_FLAG_CID_KEYED for more details.
   *
   *   If this macro is true, all functions defined in @FT_CID_H are
   *   available.
   *
   */
#define FT_IS_CID_KEYED( face ) \
		  ( !!( (face)->face_flags & FT_FACE_FLAG_CID_KEYED ) )

  /**************************************************************************
   *
   * @macro:
   *   FT_IS_TRICKY
   *
   * @description:
   *   A macro that returns true whenever a face represents a 'tricky' font.
   *   See the discussion of @FT_FACE_FLAG_TRICKY for more details.
   *
   */
#define FT_IS_TRICKY( face ) \
		  ( !!( (face)->face_flags & FT_FACE_FLAG_TRICKY ) )

  /**************************************************************************
   *
   * @macro:
   *   FT_HAS_COLOR
   *
   * @description:
   *   A macro that returns true whenever a face object contains tables for
   *   color glyphs.
   *
   * @since:
   *   2.5.1
   *
   */
#define FT_HAS_COLOR( face ) \
		  ( !!( (face)->face_flags & FT_FACE_FLAG_COLOR ) )

  /**************************************************************************
   *
   * @macro:
   *   FT_HAS_SVG
   *
   * @description:
   *   A macro that returns true whenever a face object contains an 'SVG~'
   *   OpenType table.
   *
   * @since:
   *   2.12
   */
#define FT_HAS_SVG( face ) \
		  ( !!( (face)->face_flags & FT_FACE_FLAG_SVG ) )

  /**************************************************************************
   *
   * @macro:
   *   FT_HAS_SBIX
   *
   * @description:
   *   A macro that returns true whenever a face object contains an 'sbix'
   *   OpenType table *and* outline glyphs.
   *
   *   Currently, FreeType only supports bitmap glyphs in PNG format for this
   *   table (i.e., JPEG and TIFF formats are unsupported, as are
   *   Apple-specific formats not part of the OpenType specification).
   *
   * @note:
   *   For backward compatibility, a font with an 'sbix' table is treated as
   *   a bitmap-only face.  Using @FT_Open_Face with
   *   @FT_PARAM_TAG_IGNORE_SBIX, an application can switch off 'sbix'
   *   handling so that the face is treated as an ordinary outline font with
   *   scalable outlines.
   *
   *   Here is some pseudo code that roughly illustrates how to implement
   *   'sbix' handling according to the OpenType specification.
   *
   * ```
   *   if ( FT_HAS_SBIX( face ) )
   *   {
   *     // open font as a scalable one without sbix handling
   *     FT_Face       face2;
   *     FT_Parameter  param = { FT_PARAM_TAG_IGNORE_SBIX, NULL };
   *     FT_Open_Args  args  = { FT_OPEN_PARAMS | ...,
   *                             ...,
   *                             1, &param };
   *
   *
   *     FT_Open_Face( library, &args, 0, &face2 );
   *
   *     <sort `face->available_size` as necessary into
   *      `preferred_sizes`[*]>
   *
   *     for ( i = 0; i < face->num_fixed_sizes; i++ )
   *     {
   *       size = preferred_sizes[i].size;
   *
   *       error = FT_Set_Pixel_Sizes( face, size, size );
   *       <error handling omitted>
   *
   *       // check whether we have a glyph in a bitmap strike
   *       error = FT_Load_Glyph( face,
   *                              glyph_index,
   *                              FT_LOAD_SBITS_ONLY          |
   *                              FT_LOAD_BITMAP_METRICS_ONLY );
   *       if ( error == FT_Err_Invalid_Argument )
   *         continue;
   *       else if ( error )
   *         <other error handling omitted>
   *       else
   *         break;
   *     }
   *
   *     if ( i != face->num_fixed_sizes )
   *       <load embedded bitmap with `FT_Load_Glyph`,
   *        scale it, display it, etc.>
   *
   *     if ( i == face->num_fixed_sizes  ||
   *          FT_HAS_SBIX_OVERLAY( face ) )
   *       <use `face2` to load outline glyph with `FT_Load_Glyph`,
   *        scale it, display it on top of the bitmap, etc.>
   *   }
   * ```
   *
   * [*] Assuming a target value of 400dpi and available strike sizes 100,
   * 200, 300, and 400dpi, a possible order might be [400, 200, 300, 100]:
   * scaling 200dpi to 400dpi usually gives better results than scaling
   * 300dpi to 400dpi; it is also much faster.  However, scaling 100dpi to
   * 400dpi can yield a too pixelated result, thus the preference might be
   * 300dpi over 100dpi.
   *
   * @since:
   *   2.12
   */
#define FT_HAS_SBIX( face ) \
		  ( !!( (face)->face_flags & FT_FACE_FLAG_SBIX ) )

  /**************************************************************************
   *
   * @macro:
   *   FT_HAS_SBIX_OVERLAY
   *
   * @description:
   *   A macro that returns true whenever a face object contains an 'sbix'
   *   OpenType table with bit~1 in its `flags` field set, instructing the
   *   application to overlay the bitmap strike with the corresponding
   *   outline glyph.  See @FT_HAS_SBIX for pseudo code how to use it.
   *
   * @since:
   *   2.12
   */
#define FT_HAS_SBIX_OVERLAY( face ) \
		  ( !!( (face)->face_flags & FT_FACE_FLAG_SBIX_OVERLAY ) )

  /**************************************************************************
   *
   * @section:
   *   face_creation
   *
   */

  /**************************************************************************
   *
   * @enum:
   *   FT_STYLE_FLAG_XXX
   *
   * @description:
   *   A list of bit flags to indicate the style of a given face.  These are
   *   used in the `style_flags` field of @FT_FaceRec.
   *
   * @values:
   *   FT_STYLE_FLAG_ITALIC ::
   *     The face style is italic or oblique.
   *
   *   FT_STYLE_FLAG_BOLD ::
   *     The face is bold.
   *
   * @note:
   *   The style information as provided by FreeType is very basic.  More
   *   details are beyond the scope and should be done on a higher level (for
   *   example, by analyzing various fields of the 'OS/2' table in SFNT based
   *   fonts).
   */
#define FT_STYLE_FLAG_ITALIC  ( 1 << 0 )
#define FT_STYLE_FLAG_BOLD    ( 1 << 1 )

  /**************************************************************************
   *
   * @section:
   *   other_api_data
   *
   */

  /**************************************************************************
   *
   * @type:
   *   FT_Size_Internal
   *
   * @description:
   *   An opaque handle to an `FT_Size_InternalRec` structure, used to model
   *   private data of a given @FT_Size object.
   */
  typedef struct FT_Size_InternalRec_*  FT_Size_Internal;

  /**************************************************************************
   *
   * @section:
   *   sizing_and_scaling
   *
   */

  /**************************************************************************
   *
   * @struct:
   *   FT_Size_Metrics
   *
   * @description:
   *   The size metrics structure gives the metrics of a size object.
   *
   * @fields:
   *   x_ppem ::
   *     The width of the scaled EM square in pixels, hence the term 'ppem'
   *     (pixels per EM).  It is also referred to as 'nominal width'.
   *
   *   y_ppem ::
   *     The height of the scaled EM square in pixels, hence the term 'ppem'
   *     (pixels per EM).  It is also referred to as 'nominal height'.
   *
   *   x_scale ::
   *     A 16.16 fractional scaling value to convert horizontal metrics from
   *     font units to 26.6 fractional pixels.  Only relevant for scalable
   *     font formats.
   *
   *   y_scale ::
   *     A 16.16 fractional scaling value to convert vertical metrics from
   *     font units to 26.6 fractional pixels.  Only relevant for scalable
   *     font formats.
   *
   *   ascender ::
   *     The ascender in 26.6 fractional pixels, rounded up to an integer
   *     value.  See @FT_FaceRec for the details.
   *
   *   descender ::
   *     The descender in 26.6 fractional pixels, rounded down to an integer
   *     value.  See @FT_FaceRec for the details.
   *
   *   height ::
   *     The height in 26.6 fractional pixels, rounded to an integer value.
   *     See @FT_FaceRec for the details.
   *
   *   max_advance ::
   *     The maximum advance width in 26.6 fractional pixels, rounded to an
   *     integer value.  See @FT_FaceRec for the details.
   *
   * @note:
   *   The scaling values, if relevant, are determined first during a size
   *   changing operation.  The remaining fields are then set by the driver.
   *   For scalable formats, they are usually set to scaled values of the
   *   corresponding fields in @FT_FaceRec.  Some values like ascender or
   *   descender are rounded for historical reasons; more precise values (for
   *   outline fonts) can be derived by scaling the corresponding @FT_FaceRec
   *   values manually, with code similar to the following.
   *
   *   ```
   *     scaled_ascender = FT_MulFix( face->ascender,
   *                                  size_metrics->y_scale );
   *   ```
   *
   *   Note that due to glyph hinting and the selected rendering mode these
   *   values are usually not exact; consequently, they must be treated as
   *   unreliable with an error margin of at least one pixel!
   *
   *   Indeed, the only way to get the exact metrics is to render _all_
   *   glyphs.  As this would be a definite performance hit, it is up to
   *   client applications to perform such computations.
   *
   *   The `FT_Size_Metrics` structure is valid for bitmap fonts also.
   *
   *
   *   **TrueType fonts with native bytecode hinting**
   *
   *   All applications that handle TrueType fonts with native hinting must
   *   be aware that TTFs expect different rounding of vertical font
   *   dimensions.  The application has to cater for this, especially if it
   *   wants to rely on a TTF's vertical data (for example, to properly align
   *   box characters vertically).
   *
   *   Only the application knows _in advance_ that it is going to use native
   *   hinting for TTFs!  FreeType, on the other hand, selects the hinting
   *   mode not at the time of creating an @FT_Size object but much later,
   *   namely while calling @FT_Load_Glyph.
   *
   *   Here is some pseudo code that illustrates a possible solution.
   *
   *   ```
   *     font_format = FT_Get_Font_Format( face );
   *
   *     if ( !strcmp( font_format, "TrueType" ) &&
   *          do_native_bytecode_hinting         )
   *     {
   *       ascender  = ROUND( FT_MulFix( face->ascender,
   *                                     size_metrics->y_scale ) );
   *       descender = ROUND( FT_MulFix( face->descender,
   *                                     size_metrics->y_scale ) );
   *     }
   *     else
   *     {
   *       ascender  = size_metrics->ascender;
   *       descender = size_metrics->descender;
   *     }
   *
   *     height      = size_metrics->height;
   *     max_advance = size_metrics->max_advance;
   *   ```
   */
  typedef struct  FT_Size_Metrics_
  {
	FT_UShort  x_ppem;      /* horizontal pixels per EM               */
	FT_UShort  y_ppem;      /* vertical pixels per EM                 */

	FT_Fixed   x_scale;     /* scaling values used to convert font    */
	FT_Fixed   y_scale;     /* units to 26.6 fractional pixels        */

	FT_Pos     ascender;    /* ascender in 26.6 frac. pixels          */
	FT_Pos     descender;   /* descender in 26.6 frac. pixels         */
	FT_Pos     height;      /* text height in 26.6 frac. pixels       */
	FT_Pos     max_advance; /* max horizontal advance, in 26.6 pixels */

  } FT_Size_Metrics;

  /**************************************************************************
   *
   * @struct:
   *   FT_SizeRec
   *
   * @description:
   *   FreeType root size class structure.  A size object models a face
   *   object at a given size.
   *
   * @fields:
   *   face ::
   *     Handle to the parent face object.
   *
   *   generic ::
   *     A typeless pointer, unused by the FreeType library or any of its
   *     drivers.  It can be used by client applications to link their own
   *     data to each size object.
   *
   *   metrics ::
   *     Metrics for this size object.  This field is read-only.
   */
  typedef struct  FT_SizeRec_
  {
	FT_Face           face;      /* parent face object              */
	FT_Generic        generic;   /* generic pointer for client uses */
	FT_Size_Metrics   metrics;   /* size metrics                    */
	FT_Size_Internal  internal;

  } FT_SizeRec;

  /**************************************************************************
   *
   * @section:
   *   other_api_data
   *
   */

  /**************************************************************************
   *
   * @struct:
   *   FT_SubGlyph
   *
   * @description:
   *   The subglyph structure is an internal object used to describe
   *   subglyphs (for example, in the case of composites).
   *
   * @note:
   *   The subglyph implementation is not part of the high-level API, hence
   *   the forward structure declaration.
   *
   *   You can however retrieve subglyph information with
   *   @FT_Get_SubGlyph_Info.
   */
  typedef struct FT_SubGlyphRec_*  FT_SubGlyph;

  /**************************************************************************
   *
   * @type:
   *   FT_Slot_Internal
   *
   * @description:
   *   An opaque handle to an `FT_Slot_InternalRec` structure, used to model
   *   private data of a given @FT_GlyphSlot object.
   */
  typedef struct FT_Slot_InternalRec_*  FT_Slot_Internal;

  /**************************************************************************
   *
   * @section:
   *   glyph_retrieval
   *
   */

  /**************************************************************************
   *
   * @struct:
   *   FT_GlyphSlotRec
   *
   * @description:
   *   FreeType root glyph slot class structure.  A glyph slot is a container
   *   where individual glyphs can be loaded, be they in outline or bitmap
   *   format.
   *
   * @fields:
   *   library ::
   *     A handle to the FreeType library instance this slot belongs to.
   *
   *   face ::
   *     A handle to the parent face object.
   *
   *   next ::
   *     In some cases (like some font tools), several glyph slots per face
   *     object can be a good thing.  As this is rare, the glyph slots are
   *     listed through a direct, single-linked list using its `next` field.
   *
   *   glyph_index ::
   *     [Since 2.10] The glyph index passed as an argument to @FT_Load_Glyph
   *     while initializing the glyph slot.
   *
   *   generic ::
   *     A typeless pointer unused by the FreeType library or any of its
   *     drivers.  It can be used by client applications to link their own
   *     data to each glyph slot object.
   *
   *   metrics ::
   *     The metrics of the last loaded glyph in the slot.  The returned
   *     values depend on the last load flags (see the @FT_Load_Glyph API
   *     function) and can be expressed either in 26.6 fractional pixels or
   *     font units.
   *
   *     Note that even when the glyph image is transformed, the metrics are
   *     not.
   *
   *   linearHoriAdvance ::
   *     The advance width of the unhinted glyph.  Its value is expressed in
   *     16.16 fractional pixels, unless @FT_LOAD_LINEAR_DESIGN is set when
   *     loading the glyph.  This field can be important to perform correct
   *     WYSIWYG layout.  Only relevant for scalable glyphs.
   *
   *   linearVertAdvance ::
   *     The advance height of the unhinted glyph.  Its value is expressed in
   *     16.16 fractional pixels, unless @FT_LOAD_LINEAR_DESIGN is set when
   *     loading the glyph.  This field can be important to perform correct
   *     WYSIWYG layout.  Only relevant for scalable glyphs.
   *
   *   advance ::
   *     This shorthand is, depending on @FT_LOAD_IGNORE_TRANSFORM, the
   *     transformed (hinted) advance width for the glyph, in 26.6 fractional
   *     pixel format.  As specified with @FT_LOAD_VERTICAL_LAYOUT, it uses
   *     either the `horiAdvance` or the `vertAdvance` value of `metrics`
   *     field.
   *
   *   format ::
   *     This field indicates the format of the image contained in the glyph
   *     slot.  Typically @FT_GLYPH_FORMAT_BITMAP, @FT_GLYPH_FORMAT_OUTLINE,
   *     or @FT_GLYPH_FORMAT_COMPOSITE, but other values are possible.
   *
   *   bitmap ::
   *     This field is used as a bitmap descriptor.  Note that the address
   *     and content of the bitmap buffer can change between calls of
   *     @FT_Load_Glyph and a few other functions.
   *
   *   bitmap_left ::
   *     The bitmap's left bearing expressed in integer pixels.
   *
   *   bitmap_top ::
   *     The bitmap's top bearing expressed in integer pixels.  This is the
   *     distance from the baseline to the top-most glyph scanline, upwards
   *     y~coordinates being **positive**.
   *
   *   outline ::
   *     The outline descriptor for the current glyph image if its format is
   *     @FT_GLYPH_FORMAT_OUTLINE.  Once a glyph is loaded, `outline` can be
   *     transformed, distorted, emboldened, etc.  However, it must not be
   *     freed.
   *
   *     [Since 2.10.1] If @FT_LOAD_NO_SCALE is set, outline coordinates of
   *     OpenType variation fonts for a selected instance are internally
   *     handled as 26.6 fractional font units but returned as (rounded)
   *     integers, as expected.  To get unrounded font units, don't use
   *     @FT_LOAD_NO_SCALE but load the glyph with @FT_LOAD_NO_HINTING and
   *     scale it, using the font's `units_per_EM` value as the ppem.
   *
   *   num_subglyphs ::
   *     The number of subglyphs in a composite glyph.  This field is only
   *     valid for the composite glyph format that should normally only be
   *     loaded with the @FT_LOAD_NO_RECURSE flag.
   *
   *   subglyphs ::
   *     An array of subglyph descriptors for composite glyphs.  There are
   *     `num_subglyphs` elements in there.  Currently internal to FreeType.
   *
   *   control_data ::
   *     Certain font drivers can also return the control data for a given
   *     glyph image (e.g.  TrueType bytecode, Type~1 charstrings, etc.).
   *     This field is a pointer to such data; it is currently internal to
   *     FreeType.
   *
   *   control_len ::
   *     This is the length in bytes of the control data.  Currently internal
   *     to FreeType.
   *
   *   other ::
   *     Reserved.
   *
   *   lsb_delta ::
   *     The difference between hinted and unhinted left side bearing while
   *     auto-hinting is active.  Zero otherwise.
   *
   *   rsb_delta ::
   *     The difference between hinted and unhinted right side bearing while
   *     auto-hinting is active.  Zero otherwise.
   *
   * @note:
   *   If @FT_Load_Glyph is called with default flags (see @FT_LOAD_DEFAULT)
   *   the glyph image is loaded in the glyph slot in its native format
   *   (e.g., an outline glyph for TrueType and Type~1 formats).  [Since 2.9]
   *   The prospective bitmap metrics are calculated according to
   *   @FT_LOAD_TARGET_XXX and other flags even for the outline glyph, even
   *   if @FT_LOAD_RENDER is not set.
   *
   *   This image can later be converted into a bitmap by calling
   *   @FT_Render_Glyph.  This function searches the current renderer for the
   *   native image's format, then invokes it.
   *
   *   The renderer is in charge of transforming the native image through the
   *   slot's face transformation fields, then converting it into a bitmap
   *   that is returned in `slot->bitmap`.
   *
   *   Note that `slot->bitmap_left` and `slot->bitmap_top` are also used to
   *   specify the position of the bitmap relative to the current pen
   *   position (e.g., coordinates (0,0) on the baseline).  Of course,
   *   `slot->format` is also changed to @FT_GLYPH_FORMAT_BITMAP.
   *
   *   Here is a small pseudo code fragment that shows how to use `lsb_delta`
   *   and `rsb_delta` to do fractional positioning of glyphs:
   *
   *   ```
   *     FT_GlyphSlot  slot     = face->glyph;
   *     FT_Pos        origin_x = 0;
   *
   *
   *     for all glyphs do
   *       <load glyph with `FT_Load_Glyph'>
   *
   *       FT_Outline_Translate( slot->outline, origin_x & 63, 0 );
   *
   *       <save glyph image, or render glyph, or ...>
   *
   *       <compute kern between current and next glyph
   *        and add it to `origin_x'>
   *
   *       origin_x += slot->advance.x;
   *       origin_x += slot->lsb_delta - slot->rsb_delta;
   *     endfor
   *   ```
   *
   *   Here is another small pseudo code fragment that shows how to use
   *   `lsb_delta` and `rsb_delta` to improve integer positioning of glyphs:
   *
   *   ```
   *     FT_GlyphSlot  slot           = face->glyph;
   *     FT_Pos        origin_x       = 0;
   *     FT_Pos        prev_rsb_delta = 0;
   *
   *
   *     for all glyphs do
   *       <compute kern between current and previous glyph
   *        and add it to `origin_x'>
   *
   *       <load glyph with `FT_Load_Glyph'>
   *
   *       if ( prev_rsb_delta - slot->lsb_delta >  32 )
   *         origin_x -= 64;
   *       else if ( prev_rsb_delta - slot->lsb_delta < -31 )
   *         origin_x += 64;
   *
   *       prev_rsb_delta = slot->rsb_delta;
   *
   *       <save glyph image, or render glyph, or ...>
   *
   *       origin_x += slot->advance.x;
   *     endfor
   *   ```
   *
   *   If you use strong auto-hinting, you **must** apply these delta values!
   *   Otherwise you will experience far too large inter-glyph spacing at
   *   small rendering sizes in most cases.  Note that it doesn't harm to use
   *   the above code for other hinting modes also, since the delta values
   *   are zero then.
   */
  typedef struct  FT_GlyphSlotRec_
  {
	FT_Library        library;
	FT_Face           face;
	FT_GlyphSlot      next;
	FT_UInt           glyph_index; /* new in 2.10; was reserved previously */
	FT_Generic        generic;

	FT_Glyph_Metrics  metrics;
	FT_Fixed          linearHoriAdvance;
	FT_Fixed          linearVertAdvance;
	FT_Vector         advance;

	FT_Glyph_Format   format;

	FT_Bitmap         bitmap;
	FT_Int            bitmap_left;
	FT_Int            bitmap_top;

	FT_Outline        outline;

	FT_UInt           num_subglyphs;
	FT_SubGlyph       subglyphs;

	void*             control_data;
	long              control_len;

	FT_Pos            lsb_delta;
	FT_Pos            rsb_delta;

	void*             other;

	FT_Slot_Internal  internal;

  } FT_GlyphSlotRec;

  /*************************************************************************/
  /*************************************************************************/
  /*                                                                       */
  /*                         F U N C T I O N S                             */
  /*                                                                       */
  /*************************************************************************/
  /*************************************************************************/

  /**************************************************************************
   *
   * @section:
   *   library_setup
   *
   */

  /**************************************************************************
   *
   * @function:
   *   FT_Init_FreeType
   *
   * @description:
   *   Initialize a new FreeType library object.  The set of modules that are
   *   registered by this function is determined at build time.
   *
   * @output:
   *   alibrary ::
   *     A handle to a new library object.
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @note:
   *   In case you want to provide your own memory allocating routines, use
   *   @FT_New_Library instead, followed by a call to @FT_Add_Default_Modules
   *   (or a series of calls to @FT_Add_Module) and
   *   @FT_Set_Default_Properties.
   *
   *   See the documentation of @FT_Library and @FT_Face for multi-threading
   *   issues.
   *
   *   If you need reference-counting (cf. @FT_Reference_Library), use
   *   @FT_New_Library and @FT_Done_Library.
   *
   *   If compilation option `FT_CONFIG_OPTION_ENVIRONMENT_PROPERTIES` is
   *   set, this function reads the `FREETYPE_PROPERTIES` environment
   *   variable to control driver properties.  See section @properties for
   *   more.
   */
  FT_EXPORT( FT_Error )
  FT_Init_FreeType( FT_Library  *alibrary );

  /**************************************************************************
   *
   * @function:
   *   FT_Done_FreeType
   *
   * @description:
   *   Destroy a given FreeType library object and all of its children,
   *   including resources, drivers, faces, sizes, etc.
   *
   * @input:
   *   library ::
   *     A handle to the target library object.
   *
   * @return:
   *   FreeType error code.  0~means success.
   */
  FT_EXPORT( FT_Error )
  FT_Done_FreeType( FT_Library  library );

  /**************************************************************************
   *
   * @section:
   *   face_creation
   *
   */

  /**************************************************************************
   *
   * @enum:
   *   FT_OPEN_XXX
   *
   * @description:
   *   A list of bit field constants used within the `flags` field of the
   *   @FT_Open_Args structure.
   *
   * @values:
   *   FT_OPEN_MEMORY ::
   *     This is a memory-based stream.
   *
   *   FT_OPEN_STREAM ::
   *     Copy the stream from the `stream` field.
   *
   *   FT_OPEN_PATHNAME ::
   *     Create a new input stream from a C~path name.
   *
   *   FT_OPEN_DRIVER ::
   *     Use the `driver` field.
   *
   *   FT_OPEN_PARAMS ::
   *     Use the `num_params` and `params` fields.
   *
   * @note:
   *   The `FT_OPEN_MEMORY`, `FT_OPEN_STREAM`, and `FT_OPEN_PATHNAME` flags
   *   are mutually exclusive.
   */
#define FT_OPEN_MEMORY    0x1
#define FT_OPEN_STREAM    0x2
#define FT_OPEN_PATHNAME  0x4
#define FT_OPEN_DRIVER    0x8
#define FT_OPEN_PARAMS    0x10

  /* these constants are deprecated; use the corresponding `FT_OPEN_XXX` */
  /* values instead                                                      */
#define ft_open_memory    FT_OPEN_MEMORY
#define ft_open_stream    FT_OPEN_STREAM
#define ft_open_pathname  FT_OPEN_PATHNAME
#define ft_open_driver    FT_OPEN_DRIVER
#define ft_open_params    FT_OPEN_PARAMS

  /**************************************************************************
   *
   * @struct:
   *   FT_Parameter
   *
   * @description:
   *   A simple structure to pass more or less generic parameters to
   *   @FT_Open_Face and @FT_Face_Properties.
   *
   * @fields:
   *   tag ::
   *     A four-byte identification tag.
   *
   *   data ::
   *     A pointer to the parameter data.
   *
   * @note:
   *   The ID and function of parameters are driver-specific.  See section
   *   @parameter_tags for more information.
   */
  typedef struct  FT_Parameter_
  {
	FT_ULong    tag;
	FT_Pointer  data;

  } FT_Parameter;

  /**************************************************************************
   *
   * @struct:
   *   FT_Open_Args
   *
   * @description:
   *   A structure to indicate how to open a new font file or stream.  A
   *   pointer to such a structure can be used as a parameter for the
   *   functions @FT_Open_Face and @FT_Attach_Stream.
   *
   * @fields:
   *   flags ::
   *     A set of bit flags indicating how to use the structure.
   *
   *   memory_base ::
   *     The first byte of the file in memory.
   *
   *   memory_size ::
   *     The size in bytes of the file in memory.
   *
   *   pathname ::
   *     A pointer to an 8-bit file pathname, which must be a C~string (i.e.,
   *     no null bytes except at the very end).  The pointer is not owned by
   *     FreeType.
   *
   *   stream ::
   *     A handle to a source stream object.
   *
   *   driver ::
   *     This field is exclusively used by @FT_Open_Face; it simply specifies
   *     the font driver to use for opening the face.  If set to `NULL`,
   *     FreeType tries to load the face with each one of the drivers in its
   *     list.
   *
   *   num_params ::
   *     The number of extra parameters.
   *
   *   params ::
   *     Extra parameters passed to the font driver when opening a new face.
   *
   * @note:
   *   The stream type is determined by the contents of `flags`:
   *
   *   If the @FT_OPEN_MEMORY bit is set, assume that this is a memory file
   *   of `memory_size` bytes, located at `memory_address`.  The data are not
   *   copied, and the client is responsible for releasing and destroying
   *   them _after_ the corresponding call to @FT_Done_Face.
   *
   *   Otherwise, if the @FT_OPEN_STREAM bit is set, assume that a custom
   *   input stream `stream` is used.
   *
   *   Otherwise, if the @FT_OPEN_PATHNAME bit is set, assume that this is a
   *   normal file and use `pathname` to open it.
   *
   *   If none of the above bits are set or if multiple are set at the same
   *   time, the flags are invalid and @FT_Open_Face fails.
   *
   *   If the @FT_OPEN_DRIVER bit is set, @FT_Open_Face only tries to open
   *   the file with the driver whose handler is in `driver`.
   *
   *   If the @FT_OPEN_PARAMS bit is set, the parameters given by
   *   `num_params` and `params` is used.  They are ignored otherwise.
   *
   *   Ideally, both the `pathname` and `params` fields should be tagged as
   *   'const'; this is missing for API backward compatibility.  In other
   *   words, applications should treat them as read-only.
   */
  typedef struct  FT_Open_Args_
  {
	FT_UInt         flags;
	const FT_Byte*  memory_base;
	FT_Long         memory_size;
	FT_String*      pathname;
	FT_Stream       stream;
	FT_Module       driver;
	FT_Int          num_params;
	FT_Parameter*   params;

  } FT_Open_Args;

  /**************************************************************************
   *
   * @function:
   *   FT_New_Face
   *
   * @description:
   *   Call @FT_Open_Face to open a font by its pathname.
   *
   * @inout:
   *   library ::
   *     A handle to the library resource.
   *
   * @input:
   *   pathname ::
   *     A path to the font file.
   *
   *   face_index ::
   *     See @FT_Open_Face for a detailed description of this parameter.
   *
   * @output:
   *   aface ::
   *     A handle to a new face object.  If `face_index` is greater than or
   *     equal to zero, it must be non-`NULL`.
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @note:
   *   The `pathname` string should be recognizable as such by a standard
   *   `fopen` call on your system; in particular, this means that `pathname`
   *   must not contain null bytes.  If that is not sufficient to address all
   *   file name possibilities (for example, to handle wide character file
   *   names on Windows in UTF-16 encoding) you might use @FT_Open_Face to
   *   pass a memory array or a stream object instead.
   *
   *   Use @FT_Done_Face to destroy the created @FT_Face object (along with
   *   its slot and sizes).
   */
  FT_EXPORT( FT_Error )
  FT_New_Face( FT_Library   library,
			   const char*  filepathname,
			   FT_Long      face_index,
			   FT_Face     *aface );

  /**************************************************************************
   *
   * @function:
   *   FT_New_Memory_Face
   *
   * @description:
   *   Call @FT_Open_Face to open a font that has been loaded into memory.
   *
   * @inout:
   *   library ::
   *     A handle to the library resource.
   *
   * @input:
   *   file_base ::
   *     A pointer to the beginning of the font data.
   *
   *   file_size ::
   *     The size of the memory chunk used by the font data.
   *
   *   face_index ::
   *     See @FT_Open_Face for a detailed description of this parameter.
   *
   * @output:
   *   aface ::
   *     A handle to a new face object.  If `face_index` is greater than or
   *     equal to zero, it must be non-`NULL`.
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @note:
   *   You must not deallocate the memory before calling @FT_Done_Face.
   */
  FT_EXPORT( FT_Error )
  FT_New_Memory_Face( FT_Library      library,
					  const FT_Byte*  file_base,
					  FT_Long         file_size,
					  FT_Long         face_index,
					  FT_Face        *aface );

  /**************************************************************************
   *
   * @function:
   *   FT_Open_Face
   *
   * @description:
   *   Create a face object from a given resource described by @FT_Open_Args.
   *
   * @inout:
   *   library ::
   *     A handle to the library resource.
   *
   * @input:
   *   args ::
   *     A pointer to an `FT_Open_Args` structure that must be filled by the
   *     caller.
   *
   *   face_index ::
   *     This field holds two different values.  Bits 0-15 are the index of
   *     the face in the font file (starting with value~0).  Set it to~0 if
   *     there is only one face in the font file.
   *
   *     [Since 2.6.1] Bits 16-30 are relevant to GX and OpenType variation
   *     fonts only, specifying the named instance index for the current face
   *     index (starting with value~1; value~0 makes FreeType ignore named
   *     instances).  For non-variation fonts, bits 16-30 are ignored.
   *     Assuming that you want to access the third named instance in face~4,
   *     `face_index` should be set to 0x00030004.  If you want to access
   *     face~4 without variation handling, simply set `face_index` to
   *     value~4.
   *
   *     `FT_Open_Face` and its siblings can be used to quickly check whether
   *     the font format of a given font resource is supported by FreeType.
   *     In general, if the `face_index` argument is negative, the function's
   *     return value is~0 if the font format is recognized, or non-zero
   *     otherwise.  The function allocates a more or less empty face handle
   *     in `*aface` (if `aface` isn't `NULL`); the only two useful fields in
   *     this special case are `face->num_faces` and `face->style_flags`.
   *     For any negative value of `face_index`, `face->num_faces` gives the
   *     number of faces within the font file.  For the negative value
   *     '-(N+1)' (with 'N' a non-negative 16-bit value), bits 16-30 in
   *     `face->style_flags` give the number of named instances in face 'N'
   *     if we have a variation font (or zero otherwise).  After examination,
   *     the returned @FT_Face structure should be deallocated with a call to
   *     @FT_Done_Face.
   *
   * @output:
   *   aface ::
   *     A handle to a new face object.  If `face_index` is greater than or
   *     equal to zero, it must be non-`NULL`.
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @note:
   *   Unlike FreeType 1.x, this function automatically creates a glyph slot
   *   for the face object that can be accessed directly through
   *   `face->glyph`.
   *
   *   Each new face object created with this function also owns a default
   *   @FT_Size object, accessible as `face->size`.
   *
   *   One @FT_Library instance can have multiple face objects, that is,
   *   @FT_Open_Face and its siblings can be called multiple times using the
   *   same `library` argument.
   *
   *   See the discussion of reference counters in the description of
   *   @FT_Reference_Face.
   *
   *   If `FT_OPEN_STREAM` is set in `args->flags`, the stream in
   *   `args->stream` is automatically closed before this function returns
   *   any error (including `FT_Err_Invalid_Argument`).
   *
   * @example:
   *   To loop over all faces, use code similar to the following snippet
   *   (omitting the error handling).
   *
   *   ```
   *     ...
   *     FT_Face  face;
   *     FT_Long  i, num_faces;
   *
   *
   *     error = FT_Open_Face( library, args, -1, &face );
   *     if ( error ) { ... }
   *
   *     num_faces = face->num_faces;
   *     FT_Done_Face( face );
   *
   *     for ( i = 0; i < num_faces; i++ )
   *     {
   *       ...
   *       error = FT_Open_Face( library, args, i, &face );
   *       ...
   *       FT_Done_Face( face );
   *       ...
   *     }
   *   ```
   *
   *   To loop over all valid values for `face_index`, use something similar
   *   to the following snippet, again without error handling.  The code
   *   accesses all faces immediately (thus only a single call of
   *   `FT_Open_Face` within the do-loop), with and without named instances.
   *
   *   ```
   *     ...
   *     FT_Face  face;
   *
   *     FT_Long  num_faces     = 0;
   *     FT_Long  num_instances = 0;
   *
   *     FT_Long  face_idx     = 0;
   *     FT_Long  instance_idx = 0;
   *
   *
   *     do
   *     {
   *       FT_Long  id = ( instance_idx << 16 ) + face_idx;
   *
   *
   *       error = FT_Open_Face( library, args, id, &face );
   *       if ( error ) { ... }
   *
   *       num_faces     = face->num_faces;
   *       num_instances = face->style_flags >> 16;
   *
   *       ...
   *
   *       FT_Done_Face( face );
   *
   *       if ( instance_idx < num_instances )
   *         instance_idx++;
   *       else
   *       {
   *         face_idx++;
   *         instance_idx = 0;
   *       }
   *
   *     } while ( face_idx < num_faces )
   *   ```
   */
  FT_EXPORT( FT_Error )
  FT_Open_Face( FT_Library           library,
				const FT_Open_Args*  args,
				FT_Long              face_index,
				FT_Face             *aface );

  /**************************************************************************
   *
   * @function:
   *   FT_Attach_File
   *
   * @description:
   *   Call @FT_Attach_Stream to attach a file.
   *
   * @inout:
   *   face ::
   *     The target face object.
   *
   * @input:
   *   filepathname ::
   *     The pathname.
   *
   * @return:
   *   FreeType error code.  0~means success.
   */
  FT_EXPORT( FT_Error )
  FT_Attach_File( FT_Face      face,
				  const char*  filepathname );

  /**************************************************************************
   *
   * @function:
   *   FT_Attach_Stream
   *
   * @description:
   *   'Attach' data to a face object.  Normally, this is used to read
   *   additional information for the face object.  For example, you can
   *   attach an AFM file that comes with a Type~1 font to get the kerning
   *   values and other metrics.
   *
   * @inout:
   *   face ::
   *     The target face object.
   *
   * @input:
   *   parameters ::
   *     A pointer to @FT_Open_Args that must be filled by the caller.
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @note:
   *   The meaning of the 'attach' (i.e., what really happens when the new
   *   file is read) is not fixed by FreeType itself.  It really depends on
   *   the font format (and thus the font driver).
   *
   *   Client applications are expected to know what they are doing when
   *   invoking this function.  Most drivers simply do not implement file or
   *   stream attachments.
   */
  FT_EXPORT( FT_Error )
  FT_Attach_Stream( FT_Face              face,
					const FT_Open_Args*  parameters );

  /**************************************************************************
   *
   * @function:
   *   FT_Reference_Face
   *
   * @description:
   *   A counter gets initialized to~1 at the time an @FT_Face structure is
   *   created.  This function increments the counter.  @FT_Done_Face then
   *   only destroys a face if the counter is~1, otherwise it simply
   *   decrements the counter.
   *
   *   This function helps in managing life-cycles of structures that
   *   reference @FT_Face objects.
   *
   * @input:
   *   face ::
   *     A handle to a target face object.
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @since:
   *   2.4.2
   *
   */
  FT_EXPORT( FT_Error )
  FT_Reference_Face( FT_Face  face );

  /**************************************************************************
   *
   * @function:
   *   FT_Done_Face
   *
   * @description:
   *   Discard a given face object, as well as all of its child slots and
   *   sizes.
   *
   * @input:
   *   face ::
   *     A handle to a target face object.
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @note:
   *   See the discussion of reference counters in the description of
   *   @FT_Reference_Face.
   */
  FT_EXPORT( FT_Error )
  FT_Done_Face( FT_Face  face );

  /**************************************************************************
   *
   * @section:
   *   sizing_and_scaling
   *
   */

  /**************************************************************************
   *
   * @function:
   *   FT_Select_Size
   *
   * @description:
   *   Select a bitmap strike.  To be more precise, this function sets the
   *   scaling factors of the active @FT_Size object in a face so that
   *   bitmaps from this particular strike are taken by @FT_Load_Glyph and
   *   friends.
   *
   * @inout:
   *   face ::
   *     A handle to a target face object.
   *
   * @input:
   *   strike_index ::
   *     The index of the bitmap strike in the `available_sizes` field of
   *     @FT_FaceRec structure.
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @note:
   *   For bitmaps embedded in outline fonts it is common that only a subset
   *   of the available glyphs at a given ppem value is available.  FreeType
   *   silently uses outlines if there is no bitmap for a given glyph index.
   *
   *   For GX and OpenType variation fonts, a bitmap strike makes sense only
   *   if the default instance is active (that is, no glyph variation takes
   *   place); otherwise, FreeType simply ignores bitmap strikes.  The same
   *   is true for all named instances that are different from the default
   *   instance.
   *
   *   Don't use this function if you are using the FreeType cache API.
   */
  FT_EXPORT( FT_Error )
  FT_Select_Size( FT_Face  face,
				  FT_Int   strike_index );

  /**************************************************************************
   *
   * @enum:
   *   FT_Size_Request_Type
   *
   * @description:
   *   An enumeration type that lists the supported size request types, i.e.,
   *   what input size (in font units) maps to the requested output size (in
   *   pixels, as computed from the arguments of @FT_Size_Request).
   *
   * @values:
   *   FT_SIZE_REQUEST_TYPE_NOMINAL ::
   *     The nominal size.  The `units_per_EM` field of @FT_FaceRec is used
   *     to determine both scaling values.
   *
   *     This is the standard scaling found in most applications.  In
   *     particular, use this size request type for TrueType fonts if they
   *     provide optical scaling or something similar.  Note, however, that
   *     `units_per_EM` is a rather abstract value which bears no relation to
   *     the actual size of the glyphs in a font.
   *
   *   FT_SIZE_REQUEST_TYPE_REAL_DIM ::
   *     The real dimension.  The sum of the `ascender` and (minus of) the
   *     `descender` fields of @FT_FaceRec is used to determine both scaling
   *     values.
   *
   *   FT_SIZE_REQUEST_TYPE_BBOX ::
   *     The font bounding box.  The width and height of the `bbox` field of
   *     @FT_FaceRec are used to determine the horizontal and vertical
   *     scaling value, respectively.
   *
   *   FT_SIZE_REQUEST_TYPE_CELL ::
   *     The `max_advance_width` field of @FT_FaceRec is used to determine
   *     the horizontal scaling value; the vertical scaling value is
   *     determined the same way as @FT_SIZE_REQUEST_TYPE_REAL_DIM does.
   *     Finally, both scaling values are set to the smaller one.  This type
   *     is useful if you want to specify the font size for, say, a window of
   *     a given dimension and 80x24 cells.
   *
   *   FT_SIZE_REQUEST_TYPE_SCALES ::
   *     Specify the scaling values directly.
   *
   * @note:
   *   The above descriptions only apply to scalable formats.  For bitmap
   *   formats, the behaviour is up to the driver.
   *
   *   See the note section of @FT_Size_Metrics if you wonder how size
   *   requesting relates to scaling values.
   */
  typedef enum  FT_Size_Request_Type_
  {
	FT_SIZE_REQUEST_TYPE_NOMINAL,
	FT_SIZE_REQUEST_TYPE_REAL_DIM,
	FT_SIZE_REQUEST_TYPE_BBOX,
	FT_SIZE_REQUEST_TYPE_CELL,
	FT_SIZE_REQUEST_TYPE_SCALES,

	FT_SIZE_REQUEST_TYPE_MAX

  } FT_Size_Request_Type;

  /**************************************************************************
   *
   * @struct:
   *   FT_Size_RequestRec
   *
   * @description:
   *   A structure to model a size request.
   *
   * @fields:
   *   type ::
   *     See @FT_Size_Request_Type.
   *
   *   width ::
   *     The desired width, given as a 26.6 fractional point value (with 72pt
   *     = 1in).
   *
   *   height ::
   *     The desired height, given as a 26.6 fractional point value (with
   *     72pt = 1in).
   *
   *   horiResolution ::
   *     The horizontal resolution (dpi, i.e., pixels per inch).  If set to
   *     zero, `width` is treated as a 26.6 fractional **pixel** value, which
   *     gets internally rounded to an integer.
   *
   *   vertResolution ::
   *     The vertical resolution (dpi, i.e., pixels per inch).  If set to
   *     zero, `height` is treated as a 26.6 fractional **pixel** value,
   *     which gets internally rounded to an integer.
   *
   * @note:
   *   If `width` is zero, the horizontal scaling value is set equal to the
   *   vertical scaling value, and vice versa.
   *
   *   If `type` is `FT_SIZE_REQUEST_TYPE_SCALES`, `width` and `height` are
   *   interpreted directly as 16.16 fractional scaling values, without any
   *   further modification, and both `horiResolution` and `vertResolution`
   *   are ignored.
   */
  typedef struct  FT_Size_RequestRec_
  {
	FT_Size_Request_Type  type;
	FT_Long               width;
	FT_Long               height;
	FT_UInt               horiResolution;
	FT_UInt               vertResolution;

  } FT_Size_RequestRec;

  /**************************************************************************
   *
   * @struct:
   *   FT_Size_Request
   *
   * @description:
   *   A handle to a size request structure.
   */
  typedef struct FT_Size_RequestRec_  *FT_Size_Request;

  /**************************************************************************
   *
   * @function:
   *   FT_Request_Size
   *
   * @description:
   *   Resize the scale of the active @FT_Size object in a face.
   *
   * @inout:
   *   face ::
   *     A handle to a target face object.
   *
   * @input:
   *   req ::
   *     A pointer to a @FT_Size_RequestRec.
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @note:
   *   Although drivers may select the bitmap strike matching the request,
   *   you should not rely on this if you intend to select a particular
   *   bitmap strike.  Use @FT_Select_Size instead in that case.
   *
   *   The relation between the requested size and the resulting glyph size
   *   is dependent entirely on how the size is defined in the source face.
   *   The font designer chooses the final size of each glyph relative to
   *   this size.  For more information refer to
   *   'https://www.freetype.org/freetype2/docs/glyphs/glyphs-2.html'.
   *
   *   Contrary to @FT_Set_Char_Size, this function doesn't have special code
   *   to normalize zero-valued widths, heights, or resolutions, which are
   *   treated as @FT_LOAD_NO_SCALE.
   *
   *   Don't use this function if you are using the FreeType cache API.
   */
  FT_EXPORT( FT_Error )
  FT_Request_Size( FT_Face          face,
				   FT_Size_Request  req );

  /**************************************************************************
   *
   * @function:
   *   FT_Set_Char_Size
   *
   * @description:
   *   Call @FT_Request_Size to request the nominal size (in points).
   *
   * @inout:
   *   face ::
   *     A handle to a target face object.
   *
   * @input:
   *   char_width ::
   *     The nominal width, in 26.6 fractional points.
   *
   *   char_height ::
   *     The nominal height, in 26.6 fractional points.
   *
   *   horz_resolution ::
   *     The horizontal resolution in dpi.
   *
   *   vert_resolution ::
   *     The vertical resolution in dpi.
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @note:
   *   While this function allows fractional points as input values, the
   *   resulting ppem value for the given resolution is always rounded to the
   *   nearest integer.
   *
   *   If either the character width or height is zero, it is set equal to
   *   the other value.
   *
   *   If either the horizontal or vertical resolution is zero, it is set
   *   equal to the other value.
   *
   *   A character width or height smaller than 1pt is set to 1pt; if both
   *   resolution values are zero, they are set to 72dpi.
   *
   *   Don't use this function if you are using the FreeType cache API.
   */
  FT_EXPORT( FT_Error )
  FT_Set_Char_Size( FT_Face     face,
					FT_F26Dot6  char_width,
					FT_F26Dot6  char_height,
					FT_UInt     horz_resolution,
					FT_UInt     vert_resolution );

  /**************************************************************************
   *
   * @function:
   *   FT_Set_Pixel_Sizes
   *
   * @description:
   *   Call @FT_Request_Size to request the nominal size (in pixels).
   *
   * @inout:
   *   face ::
   *     A handle to the target face object.
   *
   * @input:
   *   pixel_width ::
   *     The nominal width, in pixels.
   *
   *   pixel_height ::
   *     The nominal height, in pixels.
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @note:
   *   You should not rely on the resulting glyphs matching or being
   *   constrained to this pixel size.  Refer to @FT_Request_Size to
   *   understand how requested sizes relate to actual sizes.
   *
   *   Don't use this function if you are using the FreeType cache API.
   */
  FT_EXPORT( FT_Error )
  FT_Set_Pixel_Sizes( FT_Face  face,
					  FT_UInt  pixel_width,
					  FT_UInt  pixel_height );

  /**************************************************************************
   *
   * @section:
   *   glyph_retrieval
   *
   */

  /**************************************************************************
   *
   * @function:
   *   FT_Load_Glyph
   *
   * @description:
   *   Load a glyph into the glyph slot of a face object.
   *
   * @inout:
   *   face ::
   *     A handle to the target face object where the glyph is loaded.
   *
   * @input:
   *   glyph_index ::
   *     The index of the glyph in the font file.  For CID-keyed fonts
   *     (either in PS or in CFF format) this argument specifies the CID
   *     value.
   *
   *   load_flags ::
   *     A flag indicating what to load for this glyph.  The @FT_LOAD_XXX
   *     flags can be used to control the glyph loading process (e.g.,
   *     whether the outline should be scaled, whether to load bitmaps or
   *     not, whether to hint the outline, etc).
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @note:
   *   For proper scaling and hinting, the active @FT_Size object owned by
   *   the face has to be meaningfully initialized by calling
   *   @FT_Set_Char_Size before this function, for example.  The loaded
   *   glyph may be transformed.  See @FT_Set_Transform for the details.
   *
   *   For subsetted CID-keyed fonts, `FT_Err_Invalid_Argument` is returned
   *   for invalid CID values (that is, for CID values that don't have a
   *   corresponding glyph in the font).  See the discussion of the
   *   @FT_FACE_FLAG_CID_KEYED flag for more details.
   *
   *   If you receive `FT_Err_Glyph_Too_Big`, try getting the glyph outline
   *   at EM size, then scale it manually and fill it as a graphics
   *   operation.
   */
  FT_EXPORT( FT_Error )
  FT_Load_Glyph( FT_Face   face,
				 FT_UInt   glyph_index,
				 FT_Int32  load_flags );

  /**************************************************************************
   *
   * @section:
   *   character_mapping
   *
   */

  /**************************************************************************
   *
   * @function:
   *   FT_Load_Char
   *
   * @description:
   *   Load a glyph into the glyph slot of a face object, accessed by its
   *   character code.
   *
   * @inout:
   *   face ::
   *     A handle to a target face object where the glyph is loaded.
   *
   * @input:
   *   char_code ::
   *     The glyph's character code, according to the current charmap used in
   *     the face.
   *
   *   load_flags ::
   *     A flag indicating what to load for this glyph.  The @FT_LOAD_XXX
   *     constants can be used to control the glyph loading process (e.g.,
   *     whether the outline should be scaled, whether to load bitmaps or
   *     not, whether to hint the outline, etc).
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @note:
   *   This function simply calls @FT_Get_Char_Index and @FT_Load_Glyph.
   *
   *   Many fonts contain glyphs that can't be loaded by this function since
   *   its glyph indices are not listed in any of the font's charmaps.
   *
   *   If no active cmap is set up (i.e., `face->charmap` is zero), the call
   *   to @FT_Get_Char_Index is omitted, and the function behaves identically
   *   to @FT_Load_Glyph.
   */
  FT_EXPORT( FT_Error )
  FT_Load_Char( FT_Face   face,
				FT_ULong  char_code,
				FT_Int32  load_flags );

  /**************************************************************************
   *
   * @section:
   *   glyph_retrieval
   *
   */

  /**************************************************************************
   *
   * @enum:
   *   FT_LOAD_XXX
   *
   * @description:
   *   A list of bit field constants for @FT_Load_Glyph to indicate what kind
   *   of operations to perform during glyph loading.
   *
   * @values:
   *   FT_LOAD_DEFAULT ::
   *     Corresponding to~0, this value is used as the default glyph load
   *     operation.  In this case, the following happens:
   *
   *     1. FreeType looks for a bitmap for the glyph corresponding to the
   *     face's current size.  If one is found, the function returns.  The
   *     bitmap data can be accessed from the glyph slot (see note below).
   *
   *     2. If no embedded bitmap is searched for or found, FreeType looks
   *     for a scalable outline.  If one is found, it is loaded from the font
   *     file, scaled to device pixels, then 'hinted' to the pixel grid in
   *     order to optimize it.  The outline data can be accessed from the
   *     glyph slot (see note below).
   *
   *     Note that by default the glyph loader doesn't render outlines into
   *     bitmaps.  The following flags are used to modify this default
   *     behaviour to more specific and useful cases.
   *
   *   FT_LOAD_NO_SCALE ::
   *     Don't scale the loaded outline glyph but keep it in font units.
   *     This flag is also assumed if @FT_Size owned by the face was not
   *     properly initialized.
   *
   *     This flag implies @FT_LOAD_NO_HINTING and @FT_LOAD_NO_BITMAP, and
   *     unsets @FT_LOAD_RENDER.
   *
   *     If the font is 'tricky' (see @FT_FACE_FLAG_TRICKY for more), using
   *     `FT_LOAD_NO_SCALE` usually yields meaningless outlines because the
   *     subglyphs must be scaled and positioned with hinting instructions.
   *     This can be solved by loading the font without `FT_LOAD_NO_SCALE`
   *     and setting the character size to `font->units_per_EM`.
   *
   *   FT_LOAD_NO_HINTING ::
   *     Disable hinting.  This generally generates 'blurrier' bitmap glyphs
   *     when the glyphs are rendered in any of the anti-aliased modes.  See
   *     also the note below.
   *
   *     This flag is implied by @FT_LOAD_NO_SCALE.
   *
   *   FT_LOAD_RENDER ::
   *     Call @FT_Render_Glyph after the glyph is loaded.  By default, the
   *     glyph is rendered in @FT_RENDER_MODE_NORMAL mode.  This can be
   *     overridden by @FT_LOAD_TARGET_XXX or @FT_LOAD_MONOCHROME.
   *
   *     This flag is unset by @FT_LOAD_NO_SCALE.
   *
   *   FT_LOAD_NO_BITMAP ::
   *     Ignore bitmap strikes when loading.  Bitmap-only fonts ignore this
   *     flag.
   *
   *     @FT_LOAD_NO_SCALE always sets this flag.
   *
   *   FT_LOAD_SBITS_ONLY ::
   *     [Since 2.12] This is the opposite of @FT_LOAD_NO_BITMAP, more or
   *     less: @FT_Load_Glyph returns `FT_Err_Invalid_Argument` if the face
   *     contains a bitmap strike for the given size (or the strike selected
   *     by @FT_Select_Size) but there is no glyph in the strike.
   *
   *     Note that this load flag was part of FreeType since version 2.0.6
   *     but previously tagged as internal.
   *
   *   FT_LOAD_VERTICAL_LAYOUT ::
   *     Load the glyph for vertical text layout.  In particular, the
   *     `advance` value in the @FT_GlyphSlotRec structure is set to the
   *     `vertAdvance` value of the `metrics` field.
   *
   *     In case @FT_HAS_VERTICAL doesn't return true, you shouldn't use this
   *     flag currently.  Reason is that in this case vertical metrics get
   *     synthesized, and those values are not always consistent across
   *     various font formats.
   *
   *   FT_LOAD_FORCE_AUTOHINT ::
   *     Prefer the auto-hinter over the font's native hinter.  See also the
   *     note below.
   *
   *   FT_LOAD_PEDANTIC ::
   *     Make the font driver perform pedantic verifications during glyph
   *     loading and hinting.  This is mostly used to detect broken glyphs in
   *     fonts.  By default, FreeType tries to handle broken fonts also.
   *
   *     In particular, errors from the TrueType bytecode engine are not
   *     passed to the application if this flag is not set; this might result
   *     in partially hinted or distorted glyphs in case a glyph's bytecode
   *     is buggy.
   *
   *   FT_LOAD_NO_RECURSE ::
   *     Don't load composite glyphs recursively.  Instead, the font driver
   *     fills the `num_subglyph` and `subglyphs` values of the glyph slot;
   *     it also sets `glyph->format` to @FT_GLYPH_FORMAT_COMPOSITE.  The
   *     description of subglyphs can then be accessed with
   *     @FT_Get_SubGlyph_Info.
   *
   *     Don't use this flag for retrieving metrics information since some
   *     font drivers only return rudimentary data.
   *
   *     This flag implies @FT_LOAD_NO_SCALE and @FT_LOAD_IGNORE_TRANSFORM.
   *
   *   FT_LOAD_IGNORE_TRANSFORM ::
   *     Ignore the transform matrix set by @FT_Set_Transform.
   *
   *   FT_LOAD_MONOCHROME ::
   *     This flag is used with @FT_LOAD_RENDER to indicate that you want to
   *     render an outline glyph to a 1-bit monochrome bitmap glyph, with
   *     8~pixels packed into each byte of the bitmap data.
   *
   *     Note that this has no effect on the hinting algorithm used.  You
   *     should rather use @FT_LOAD_TARGET_MONO so that the
   *     monochrome-optimized hinting algorithm is used.
   *
   *   FT_LOAD_LINEAR_DESIGN ::
   *     Keep `linearHoriAdvance` and `linearVertAdvance` fields of
   *     @FT_GlyphSlotRec in font units.  See @FT_GlyphSlotRec for details.
   *
   *   FT_LOAD_NO_AUTOHINT ::
   *     Disable the auto-hinter.  See also the note below.
   *
   *   FT_LOAD_COLOR ::
   *     Load colored glyphs.  FreeType searches in the following order;
   *     there are slight differences depending on the font format.
   *
   *     [Since 2.5] Load embedded color bitmap images (provided
   *     @FT_LOAD_NO_BITMAP is not set).  The resulting color bitmaps, if
   *     available, have the @FT_PIXEL_MODE_BGRA format, with pre-multiplied
   *     color channels.  If the flag is not set and color bitmaps are found,
   *     they are converted to 256-level gray bitmaps, using the
   *     @FT_PIXEL_MODE_GRAY format.
   *
   *     [Since 2.12] If the glyph index maps to an entry in the face's
   *     'SVG~' table, load the associated SVG document from this table and
   *     set the `format` field of @FT_GlyphSlotRec to @FT_GLYPH_FORMAT_SVG
   *     ([since 2.13.1] provided @FT_LOAD_NO_SVG is not set).  Note that
   *     FreeType itself can't render SVG documents; however, the library
   *     provides hooks to seamlessly integrate an external renderer.  See
   *     sections @ot_svg_driver and @svg_fonts for more.
   *
   *     [Since 2.10, experimental] If the glyph index maps to an entry in
   *     the face's 'COLR' table with a 'CPAL' palette table (as defined in
   *     the OpenType specification), make @FT_Render_Glyph provide a default
   *     blending of the color glyph layers associated with the glyph index,
   *     using the same bitmap format as embedded color bitmap images.  This
   *     is mainly for convenience and works only for glyphs in 'COLR' v0
   *     tables (or glyphs in 'COLR' v1 tables that exclusively use v0
   *     features).  For full control of color layers use
   *     @FT_Get_Color_Glyph_Layer and FreeType's color functions like
   *     @FT_Palette_Select instead of setting @FT_LOAD_COLOR for rendering
   *     so that the client application can handle blending by itself.
   *
   *   FT_LOAD_NO_SVG ::
   *     [Since 2.13.1] Ignore SVG glyph data when loading.
   *
   *   FT_LOAD_COMPUTE_METRICS ::
   *     [Since 2.6.1] Compute glyph metrics from the glyph data, without the
   *     use of bundled metrics tables (for example, the 'hdmx' table in
   *     TrueType fonts).  This flag is mainly used by font validating or
   *     font editing applications, which need to ignore, verify, or edit
   *     those tables.
   *
   *     Currently, this flag is only implemented for TrueType fonts.
   *
   *   FT_LOAD_BITMAP_METRICS_ONLY ::
   *     [Since 2.7.1] Request loading of the metrics and bitmap image
   *     information of a (possibly embedded) bitmap glyph without allocating
   *     or copying the bitmap image data itself.  No effect if the target
   *     glyph is not a bitmap image.
   *
   *     This flag unsets @FT_LOAD_RENDER.
   *
   *   FT_LOAD_CROP_BITMAP ::
   *     Ignored.  Deprecated.
   *
   *   FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH ::
   *     Ignored.  Deprecated.
   *
   * @note:
   *   By default, hinting is enabled and the font's native hinter (see
   *   @FT_FACE_FLAG_HINTER) is preferred over the auto-hinter.  You can
   *   disable hinting by setting @FT_LOAD_NO_HINTING or change the
   *   precedence by setting @FT_LOAD_FORCE_AUTOHINT.  You can also set
   *   @FT_LOAD_NO_AUTOHINT in case you don't want the auto-hinter to be used
   *   at all.
   *
   *   See the description of @FT_FACE_FLAG_TRICKY for a special exception
   *   (affecting only a handful of Asian fonts).
   *
   *   Besides deciding which hinter to use, you can also decide which
   *   hinting algorithm to use.  See @FT_LOAD_TARGET_XXX for details.
   *
   *   Note that the auto-hinter needs a valid Unicode cmap (either a native
   *   one or synthesized by FreeType) for producing correct results.  If a
   *   font provides an incorrect mapping (for example, assigning the
   *   character code U+005A, LATIN CAPITAL LETTER~Z, to a glyph depicting a
   *   mathematical integral sign), the auto-hinter might produce useless
   *   results.
   *
   */
#define FT_LOAD_DEFAULT                      0x0
#define FT_LOAD_NO_SCALE                     ( 1L << 0  )
#define FT_LOAD_NO_HINTING                   ( 1L << 1  )
#define FT_LOAD_RENDER                       ( 1L << 2  )
#define FT_LOAD_NO_BITMAP                    ( 1L << 3  )
#define FT_LOAD_VERTICAL_LAYOUT              ( 1L << 4  )
#define FT_LOAD_FORCE_AUTOHINT               ( 1L << 5  )
#define FT_LOAD_CROP_BITMAP                  ( 1L << 6  )
#define FT_LOAD_PEDANTIC                     ( 1L << 7  )
#define FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH  ( 1L << 9  )
#define FT_LOAD_NO_RECURSE                   ( 1L << 10 )
#define FT_LOAD_IGNORE_TRANSFORM             ( 1L << 11 )
#define FT_LOAD_MONOCHROME                   ( 1L << 12 )
#define FT_LOAD_LINEAR_DESIGN                ( 1L << 13 )
#define FT_LOAD_SBITS_ONLY                   ( 1L << 14 )
#define FT_LOAD_NO_AUTOHINT                  ( 1L << 15 )
  /* Bits 16-19 are used by `FT_LOAD_TARGET_` */
#define FT_LOAD_COLOR                        ( 1L << 20 )
#define FT_LOAD_COMPUTE_METRICS              ( 1L << 21 )
#define FT_LOAD_BITMAP_METRICS_ONLY          ( 1L << 22 )
#define FT_LOAD_NO_SVG                       ( 1L << 24 )

  /* */

  /* used internally only by certain font drivers */
#define FT_LOAD_ADVANCE_ONLY                 ( 1L << 8  )
#define FT_LOAD_SVG_ONLY                     ( 1L << 23 )

  /**************************************************************************
   *
   * @enum:
   *   FT_LOAD_TARGET_XXX
   *
   * @description:
   *   A list of values to select a specific hinting algorithm for the
   *   hinter.  You should OR one of these values to your `load_flags` when
   *   calling @FT_Load_Glyph.
   *
   *   Note that a font's native hinters may ignore the hinting algorithm you
   *   have specified (e.g., the TrueType bytecode interpreter).  You can set
   *   @FT_LOAD_FORCE_AUTOHINT to ensure that the auto-hinter is used.
   *
   * @values:
   *   FT_LOAD_TARGET_NORMAL ::
   *     The default hinting algorithm, optimized for standard gray-level
   *     rendering.  For monochrome output, use @FT_LOAD_TARGET_MONO instead.
   *
   *   FT_LOAD_TARGET_LIGHT ::
   *     A lighter hinting algorithm for gray-level modes.  Many generated
   *     glyphs are fuzzier but better resemble their original shape.  This
   *     is achieved by snapping glyphs to the pixel grid only vertically
   *     (Y-axis), as is done by FreeType's new CFF engine or Microsoft's
   *     ClearType font renderer.  This preserves inter-glyph spacing in
   *     horizontal text.  The snapping is done either by the native font
   *     driver, if the driver itself and the font support it, or by the
   *     auto-hinter.
   *
   *     Advance widths are rounded to integer values; however, using the
   *     `lsb_delta` and `rsb_delta` fields of @FT_GlyphSlotRec, it is
   *     possible to get fractional advance widths for subpixel positioning
   *     (which is recommended to use).
   *
   *     If configuration option `AF_CONFIG_OPTION_TT_SIZE_METRICS` is
   *     active, TrueType-like metrics are used to make this mode behave
   *     similarly as in unpatched FreeType versions between 2.4.6 and 2.7.1
   *     (inclusive).
   *
   *   FT_LOAD_TARGET_MONO ::
   *     Strong hinting algorithm that should only be used for monochrome
   *     output.  The result is probably unpleasant if the glyph is rendered
   *     in non-monochrome modes.
   *
   *     Note that for outline fonts only the TrueType font driver has proper
   *     monochrome hinting support, provided the TTFs contain hints for B/W
   *     rendering (which most fonts no longer provide).  If these conditions
   *     are not met it is very likely that you get ugly results at smaller
   *     sizes.
   *
   *   FT_LOAD_TARGET_LCD ::
   *     A variant of @FT_LOAD_TARGET_LIGHT optimized for horizontally
   *     decimated LCD displays.
   *
   *   FT_LOAD_TARGET_LCD_V ::
   *     A variant of @FT_LOAD_TARGET_NORMAL optimized for vertically
   *     decimated LCD displays.
   *
   * @note:
   *   You should use only _one_ of the `FT_LOAD_TARGET_XXX` values in your
   *   `load_flags`.  They can't be ORed.
   *
   *   If @FT_LOAD_RENDER is also set, the glyph is rendered in the
   *   corresponding mode (i.e., the mode that matches the used algorithm
   *   best).  An exception is `FT_LOAD_TARGET_MONO` since it implies
   *   @FT_LOAD_MONOCHROME.
   *
   *   You can use a hinting algorithm that doesn't correspond to the same
   *   rendering mode.  As an example, it is possible to use the 'light'
   *   hinting algorithm and have the results rendered in horizontal LCD
   *   pixel mode, with code like
   *
   *   ```
   *     FT_Load_Glyph( face, glyph_index,
   *                    load_flags | FT_LOAD_TARGET_LIGHT );
   *
   *     FT_Render_Glyph( face->glyph, FT_RENDER_MODE_LCD );
   *   ```
   *
   *   In general, you should stick with one rendering mode.  For example,
   *   switching between @FT_LOAD_TARGET_NORMAL and @FT_LOAD_TARGET_MONO
   *   enforces a lot of recomputation for TrueType fonts, which is slow.
   *   Another reason is caching: Selecting a different mode usually causes
   *   changes in both the outlines and the rasterized bitmaps; it is thus
   *   necessary to empty the cache after a mode switch to avoid false hits.
   *
   */
#define FT_LOAD_TARGET_( x )   ( FT_STATIC_CAST( FT_Int32, (x) & 15 ) << 16 )

#define FT_LOAD_TARGET_NORMAL  FT_LOAD_TARGET_( FT_RENDER_MODE_NORMAL )
#define FT_LOAD_TARGET_LIGHT   FT_LOAD_TARGET_( FT_RENDER_MODE_LIGHT  )
#define FT_LOAD_TARGET_MONO    FT_LOAD_TARGET_( FT_RENDER_MODE_MONO   )
#define FT_LOAD_TARGET_LCD     FT_LOAD_TARGET_( FT_RENDER_MODE_LCD    )
#define FT_LOAD_TARGET_LCD_V   FT_LOAD_TARGET_( FT_RENDER_MODE_LCD_V  )

  /**************************************************************************
   *
   * @macro:
   *   FT_LOAD_TARGET_MODE
   *
   * @description:
   *   Return the @FT_Render_Mode corresponding to a given
   *   @FT_LOAD_TARGET_XXX value.
   *
   */
#define FT_LOAD_TARGET_MODE( x )                               \
		  FT_STATIC_CAST( FT_Render_Mode, ( (x) >> 16 ) & 15 )

  /**************************************************************************
   *
   * @section:
   *   sizing_and_scaling
   *
   */

  /**************************************************************************
   *
   * @function:
   *   FT_Set_Transform
   *
   * @description:
   *   Set the transformation that is applied to glyph images when they are
   *   loaded into a glyph slot through @FT_Load_Glyph.
   *
   * @inout:
   *   face ::
   *     A handle to the source face object.
   *
   * @input:
   *   matrix ::
   *     A pointer to the transformation's 2x2 matrix.  Use `NULL` for the
   *     identity matrix.
   *   delta ::
   *     A pointer to the translation vector.  Use `NULL` for the null
   *     vector.
   *
   * @note:
   *   This function is provided as a convenience, but keep in mind that
   *   @FT_Matrix coefficients are only 16.16 fixed-point values, which can
   *   limit the accuracy of the results.  Using floating-point computations
   *   to perform the transform directly in client code instead will always
   *   yield better numbers.
   *
   *   The transformation is only applied to scalable image formats after the
   *   glyph has been loaded.  It means that hinting is unaltered by the
   *   transformation and is performed on the character size given in the
   *   last call to @FT_Set_Char_Size or @FT_Set_Pixel_Sizes.
   *
   *   Note that this also transforms the `face.glyph.advance` field, but
   *   **not** the values in `face.glyph.metrics`.
   */
  FT_EXPORT( void )
  FT_Set_Transform( FT_Face     face,
					FT_Matrix*  matrix,
					FT_Vector*  delta );

  /**************************************************************************
   *
   * @function:
   *   FT_Get_Transform
   *
   * @description:
   *   Return the transformation that is applied to glyph images when they
   *   are loaded into a glyph slot through @FT_Load_Glyph.  See
   *   @FT_Set_Transform for more details.
   *
   * @input:
   *   face ::
   *     A handle to the source face object.
   *
   * @output:
   *   matrix ::
   *     A pointer to a transformation's 2x2 matrix.  Set this to NULL if you
   *     are not interested in the value.
   *
   *   delta ::
   *     A pointer to a translation vector.  Set this to NULL if you are not
   *     interested in the value.
   *
   * @since:
   *   2.11
   *
   */
  FT_EXPORT( void )
  FT_Get_Transform( FT_Face     face,
					FT_Matrix*  matrix,
					FT_Vector*  delta );

  /**************************************************************************
   *
   * @section:
   *   glyph_retrieval
   *
   */

  /**************************************************************************
   *
   * @enum:
   *   FT_Render_Mode
   *
   * @description:
   *   Render modes supported by FreeType~2.  Each mode corresponds to a
   *   specific type of scanline conversion performed on the outline.
   *
   *   For bitmap fonts and embedded bitmaps the `bitmap->pixel_mode` field
   *   in the @FT_GlyphSlotRec structure gives the format of the returned
   *   bitmap.
   *
   *   All modes except @FT_RENDER_MODE_MONO use 256 levels of opacity,
   *   indicating pixel coverage.  Use linear alpha blending and gamma
   *   correction to correctly render non-monochrome glyph bitmaps onto a
   *   surface; see @FT_Render_Glyph.
   *
   *   The @FT_RENDER_MODE_SDF is a special render mode that uses up to 256
   *   distance values, indicating the signed distance from the grid position
   *   to the nearest outline.
   *
   * @values:
   *   FT_RENDER_MODE_NORMAL ::
   *     Default render mode; it corresponds to 8-bit anti-aliased bitmaps.
   *
   *   FT_RENDER_MODE_LIGHT ::
   *     This is equivalent to @FT_RENDER_MODE_NORMAL.  It is only defined as
   *     a separate value because render modes are also used indirectly to
   *     define hinting algorithm selectors.  See @FT_LOAD_TARGET_XXX for
   *     details.
   *
   *   FT_RENDER_MODE_MONO ::
   *     This mode corresponds to 1-bit bitmaps (with 2~levels of opacity).
   *
   *   FT_RENDER_MODE_LCD ::
   *     This mode corresponds to horizontal RGB and BGR subpixel displays
   *     like LCD screens.  It produces 8-bit bitmaps that are 3~times the
   *     width of the original glyph outline in pixels, and which use the
   *     @FT_PIXEL_MODE_LCD mode.
   *
   *   FT_RENDER_MODE_LCD_V ::
   *     This mode corresponds to vertical RGB and BGR subpixel displays
   *     (like PDA screens, rotated LCD displays, etc.).  It produces 8-bit
   *     bitmaps that are 3~times the height of the original glyph outline in
   *     pixels and use the @FT_PIXEL_MODE_LCD_V mode.
   *
   *   FT_RENDER_MODE_SDF ::
   *     This mode corresponds to 8-bit, single-channel signed distance field
   *     (SDF) bitmaps.  Each pixel in the SDF grid is the value from the
   *     pixel's position to the nearest glyph's outline.  The distances are
   *     calculated from the center of the pixel and are positive if they are
   *     filled by the outline (i.e., inside the outline) and negative
   *     otherwise.  Check the note below on how to convert the output values
   *     to usable data.
   *
   * @note:
   *   The selected render mode only affects vector glyphs of a font.
   *   Embedded bitmaps often have a different pixel mode like
   *   @FT_PIXEL_MODE_MONO.  You can use @FT_Bitmap_Convert to transform them
   *   into 8-bit pixmaps.
   *
   *   For @FT_RENDER_MODE_SDF the output bitmap buffer contains normalized
   *   distances that are packed into unsigned 8-bit values.  To get pixel
   *   values in floating point representation use the following pseudo-C
   *   code for the conversion.
   *
   *   ```
   *   // Load glyph and render using FT_RENDER_MODE_SDF,
   *   // then use the output buffer as follows.
   *
   *   ...
   *   FT_Byte  buffer = glyph->bitmap->buffer;
   *
   *
   *   for pixel in buffer
   *   {
   *     // `sd` is the signed distance and `spread` is the current spread;
   *     // the default spread is 2 and can be changed.
   *
   *     float  sd = (float)pixel - 128.0f;
   *
   *
   *     // Convert to pixel values.
   *     sd = ( sd / 128.0f ) * spread;
   *
   *     // Store `sd` in a buffer or use as required.
   *   }
   *
   *   ```
   *
   *   FreeType has two rasterizers for generating SDF, namely:
   *
   *   1. `sdf` for generating SDF directly from glyph's outline, and
   *
   *   2. `bsdf` for generating SDF from rasterized bitmaps.
   *
   *   Depending on the glyph type (i.e., outline or bitmap), one of the two
   *   rasterizers is chosen at runtime and used for generating SDFs.  To
   *   force the use of `bsdf` you should render the glyph with any of the
   *   FreeType's other rendering modes (e.g., `FT_RENDER_MODE_NORMAL`) and
   *   then re-render with `FT_RENDER_MODE_SDF`.
   *
   *   There are some issues with stability and possible failures of the SDF
   *   renderers (specifically `sdf`).
   *
   *   1. The `sdf` rasterizer is sensitive to really small features (e.g.,
   *      sharp turns that are less than 1~pixel) and imperfections in the
   *      glyph's outline, causing artifacts in the final output.
   *
   *   2. The `sdf` rasterizer has limited support for handling intersecting
   *      contours and *cannot* handle self-intersecting contours whatsoever.
   *      Self-intersection happens when a single connected contour
   *      intersects itself at some point; having these in your font
   *      definitely poses a problem to the rasterizer and cause artifacts,
   *      too.
   *
   *   3. Generating SDF for really small glyphs may result in undesirable
   *      output; the pixel grid (which stores distance information) becomes
   *      too coarse.
   *
   *   4. Since the output buffer is normalized, precision at smaller spreads
   *      is greater than precision at larger spread values because the
   *      output range of [0..255] gets mapped to a smaller SDF range.  A
   *      spread of~2 should be sufficient in most cases.
   *
   *   Points (1) and (2) can be avoided by using the `bsdf` rasterizer,
   *   which is more stable than the `sdf` rasterizer in general.
   *
   */
  typedef enum  FT_Render_Mode_
  {
	FT_RENDER_MODE_NORMAL = 0,
	FT_RENDER_MODE_LIGHT,
	FT_RENDER_MODE_MONO,
	FT_RENDER_MODE_LCD,
	FT_RENDER_MODE_LCD_V,
	FT_RENDER_MODE_SDF,

	FT_RENDER_MODE_MAX

  } FT_Render_Mode;

  /* these constants are deprecated; use the corresponding */
  /* `FT_Render_Mode` values instead                       */
#define ft_render_mode_normal  FT_RENDER_MODE_NORMAL
#define ft_render_mode_mono    FT_RENDER_MODE_MONO

  /**************************************************************************
   *
   * @function:
   *   FT_Render_Glyph
   *
   * @description:
   *   Convert a given glyph image to a bitmap.  It does so by inspecting the
   *   glyph image format, finding the relevant renderer, and invoking it.
   *
   * @inout:
   *   slot ::
   *     A handle to the glyph slot containing the image to convert.
   *
   * @input:
   *   render_mode ::
   *     The render mode used to render the glyph image into a bitmap.  See
   *     @FT_Render_Mode for a list of possible values.
   *
   *     If @FT_RENDER_MODE_NORMAL is used, a previous call of @FT_Load_Glyph
   *     with flag @FT_LOAD_COLOR makes `FT_Render_Glyph` provide a default
   *     blending of colored glyph layers associated with the current glyph
   *     slot (provided the font contains such layers) instead of rendering
   *     the glyph slot's outline.  This is an experimental feature; see
   *     @FT_LOAD_COLOR for more information.
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @note:
   *   When FreeType outputs a bitmap of a glyph, it really outputs an alpha
   *   coverage map.  If a pixel is completely covered by a filled-in
   *   outline, the bitmap contains 0xFF at that pixel, meaning that
   *   0xFF/0xFF fraction of that pixel is covered, meaning the pixel is 100%
   *   black (or 0% bright).  If a pixel is only 50% covered (value 0x80),
   *   the pixel is made 50% black (50% bright or a middle shade of grey).
   *   0% covered means 0% black (100% bright or white).
   *
   *   On high-DPI screens like on smartphones and tablets, the pixels are so
   *   small that their chance of being completely covered and therefore
   *   completely black are fairly good.  On the low-DPI screens, however,
   *   the situation is different.  The pixels are too large for most of the
   *   details of a glyph and shades of gray are the norm rather than the
   *   exception.
   *
   *   This is relevant because all our screens have a second problem: they
   *   are not linear.  1~+~1 is not~2.  Twice the value does not result in
   *   twice the brightness.  When a pixel is only 50% covered, the coverage
   *   map says 50% black, and this translates to a pixel value of 128 when
   *   you use 8~bits per channel (0-255).  However, this does not translate
   *   to 50% brightness for that pixel on our sRGB and gamma~2.2 screens.
   *   Due to their non-linearity, they dwell longer in the darks and only a
   *   pixel value of about 186 results in 50% brightness -- 128 ends up too
   *   dark on both bright and dark backgrounds.  The net result is that dark
   *   text looks burnt-out, pixely and blotchy on bright background, bright
   *   text too frail on dark backgrounds, and colored text on colored
   *   background (for example, red on green) seems to have dark halos or
   *   'dirt' around it.  The situation is especially ugly for diagonal stems
   *   like in 'w' glyph shapes where the quality of FreeType's anti-aliasing
   *   depends on the correct display of grays.  On high-DPI screens where
   *   smaller, fully black pixels reign supreme, this doesn't matter, but on
   *   our low-DPI screens with all the gray shades, it does.  0% and 100%
   *   brightness are the same things in linear and non-linear space, just
   *   all the shades in-between aren't.
   *
   *   The blending function for placing text over a background is
   *
   *   ```
   *     dst = alpha * src + (1 - alpha) * dst    ,
   *   ```
   *
   *   which is known as the OVER operator.
   *
   *   To correctly composite an anti-aliased pixel of a glyph onto a
   *   surface,
   *
   *   1. take the foreground and background colors (e.g., in sRGB space)
   *      and apply gamma to get them in a linear space,
   *
   *   2. use OVER to blend the two linear colors using the glyph pixel
   *      as the alpha value (remember, the glyph bitmap is an alpha coverage
   *      bitmap), and
   *
   *   3. apply inverse gamma to the blended pixel and write it back to
   *      the image.
   *
   *   Internal testing at Adobe found that a target inverse gamma of~1.8 for
   *   step~3 gives good results across a wide range of displays with an sRGB
   *   gamma curve or a similar one.
   *
   *   This process can cost performance.  There is an approximation that
   *   does not need to know about the background color; see
   *   https://bel.fi/alankila/lcd/ and
   *   https://bel.fi/alankila/lcd/alpcor.html for details.
   *
   *   **ATTENTION**: Linear blending is even more important when dealing
   *   with subpixel-rendered glyphs to prevent color-fringing!  A
   *   subpixel-rendered glyph must first be filtered with a filter that
   *   gives equal weight to the three color primaries and does not exceed a
   *   sum of 0x100, see section @lcd_rendering.  Then the only difference to
   *   gray linear blending is that subpixel-rendered linear blending is done
   *   3~times per pixel: red foreground subpixel to red background subpixel
   *   and so on for green and blue.
   */
  FT_EXPORT( FT_Error )
  FT_Render_Glyph( FT_GlyphSlot    slot,
				   FT_Render_Mode  render_mode );

  /**************************************************************************
   *
   * @enum:
   *   FT_Kerning_Mode
   *
   * @description:
   *   An enumeration to specify the format of kerning values returned by
   *   @FT_Get_Kerning.
   *
   * @values:
   *   FT_KERNING_DEFAULT ::
   *     Return grid-fitted kerning distances in 26.6 fractional pixels.
   *
   *   FT_KERNING_UNFITTED ::
   *     Return un-grid-fitted kerning distances in 26.6 fractional pixels.
   *
   *   FT_KERNING_UNSCALED ::
   *     Return the kerning vector in original font units.
   *
   * @note:
   *   `FT_KERNING_DEFAULT` returns full pixel values; it also makes FreeType
   *   heuristically scale down kerning distances at small ppem values so
   *   that they don't become too big.
   *
   *   Both `FT_KERNING_DEFAULT` and `FT_KERNING_UNFITTED` use the current
   *   horizontal scaling factor (as set e.g. with @FT_Set_Char_Size) to
   *   convert font units to pixels.
   */
  typedef enum  FT_Kerning_Mode_
  {
	FT_KERNING_DEFAULT = 0,
	FT_KERNING_UNFITTED,
	FT_KERNING_UNSCALED

  } FT_Kerning_Mode;

  /* these constants are deprecated; use the corresponding */
  /* `FT_Kerning_Mode` values instead                      */
#define ft_kerning_default   FT_KERNING_DEFAULT
#define ft_kerning_unfitted  FT_KERNING_UNFITTED
#define ft_kerning_unscaled  FT_KERNING_UNSCALED

  /**************************************************************************
   *
   * @function:
   *   FT_Get_Kerning
   *
   * @description:
   *   Return the kerning vector between two glyphs of the same face.
   *
   * @input:
   *   face ::
   *     A handle to a source face object.
   *
   *   left_glyph ::
   *     The index of the left glyph in the kern pair.
   *
   *   right_glyph ::
   *     The index of the right glyph in the kern pair.
   *
   *   kern_mode ::
   *     See @FT_Kerning_Mode for more information.  Determines the scale and
   *     dimension of the returned kerning vector.
   *
   * @output:
   *   akerning ::
   *     The kerning vector.  This is either in font units, fractional pixels
   *     (26.6 format), or pixels for scalable formats, and in pixels for
   *     fixed-sizes formats.
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @note:
   *   Only horizontal layouts (left-to-right & right-to-left) are supported
   *   by this method.  Other layouts, or more sophisticated kernings, are
   *   out of the scope of this API function -- they can be implemented
   *   through format-specific interfaces.
   *
   *   Note that, for TrueType fonts only, this can extract data from both
   *   the 'kern' table and the basic, pair-wise kerning feature from the
   *   GPOS table (with `TT_CONFIG_OPTION_GPOS_KERNING` enabled), though
   *   FreeType does not support the more advanced GPOS layout features; use
   *   a library like HarfBuzz for those instead.  If a font has both a
   *   'kern' table and kern features of a GPOS table, the 'kern' table will
   *   be used.
   *
   *   Also note for right-to-left scripts, the functionality may differ for
   *   fonts with GPOS tables vs. 'kern' tables.  For GPOS, right-to-left
   *   fonts typically use both a placement offset and an advance for pair
   *   positioning, which this API does not support, so it would output
   *   kerning values of zero; though if the right-to-left font used only
   *   advances in GPOS pair positioning, then this API could output kerning
   *   values for it, but it would use `left_glyph` to mean the first glyph
   *   for that case.  Whereas 'kern' tables are always advance-only and
   *   always store the left glyph first.
   *
   *   Use @FT_HAS_KERNING to find out whether a font has data that can be
   *   extracted with `FT_Get_Kerning`.
   */
  FT_EXPORT( FT_Error )
  FT_Get_Kerning( FT_Face     face,
				  FT_UInt     left_glyph,
				  FT_UInt     right_glyph,
				  FT_UInt     kern_mode,
				  FT_Vector  *akerning );

  /**************************************************************************
   *
   * @function:
   *   FT_Get_Track_Kerning
   *
   * @description:
   *   Return the track kerning for a given face object at a given size.
   *
   * @input:
   *   face ::
   *     A handle to a source face object.
   *
   *   point_size ::
   *     The point size in 16.16 fractional points.
   *
   *   degree ::
   *     The degree of tightness.  Increasingly negative values represent
   *     tighter track kerning, while increasingly positive values represent
   *     looser track kerning.  Value zero means no track kerning.
   *
   * @output:
   *   akerning ::
   *     The kerning in 16.16 fractional points, to be uniformly applied
   *     between all glyphs.
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @note:
   *   Currently, only the Type~1 font driver supports track kerning, using
   *   data from AFM files (if attached with @FT_Attach_File or
   *   @FT_Attach_Stream).
   *
   *   Only very few AFM files come with track kerning data; please refer to
   *   Adobe's AFM specification for more details.
   */
  FT_EXPORT( FT_Error )
  FT_Get_Track_Kerning( FT_Face    face,
						FT_Fixed   point_size,
						FT_Int     degree,
						FT_Fixed*  akerning );

  /**************************************************************************
   *
   * @section:
   *   character_mapping
   *
   */

  /**************************************************************************
   *
   * @function:
   *   FT_Select_Charmap
   *
   * @description:
   *   Select a given charmap by its encoding tag (as listed in
   *   `freetype.h`).
   *
   * @inout:
   *   face ::
   *     A handle to the source face object.
   *
   * @input:
   *   encoding ::
   *     A handle to the selected encoding.
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @note:
   *   This function returns an error if no charmap in the face corresponds
   *   to the encoding queried here.
   *
   *   Because many fonts contain more than a single cmap for Unicode
   *   encoding, this function has some special code to select the one that
   *   covers Unicode best ('best' in the sense that a UCS-4 cmap is
   *   preferred to a UCS-2 cmap).  It is thus preferable to @FT_Set_Charmap
   *   in this case.
   */
  FT_EXPORT( FT_Error )
  FT_Select_Charmap( FT_Face      face,
					 FT_Encoding  encoding );

  /**************************************************************************
   *
   * @function:
   *   FT_Set_Charmap
   *
   * @description:
   *   Select a given charmap for character code to glyph index mapping.
   *
   * @inout:
   *   face ::
   *     A handle to the source face object.
   *
   * @input:
   *   charmap ::
   *     A handle to the selected charmap.
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @note:
   *   This function returns an error if the charmap is not part of the face
   *   (i.e., if it is not listed in the `face->charmaps` table).
   *
   *   It also fails if an OpenType type~14 charmap is selected (which
   *   doesn't map character codes to glyph indices at all).
   */
  FT_EXPORT( FT_Error )
  FT_Set_Charmap( FT_Face     face,
				  FT_CharMap  charmap );

  /**************************************************************************
   *
   * @function:
   *   FT_Get_Charmap_Index
   *
   * @description:
   *   Retrieve index of a given charmap.
   *
   * @input:
   *   charmap ::
   *     A handle to a charmap.
   *
   * @return:
   *   The index into the array of character maps within the face to which
   *   `charmap` belongs.  If an error occurs, -1 is returned.
   *
   */
  FT_EXPORT( FT_Int )
  FT_Get_Charmap_Index( FT_CharMap  charmap );

  /**************************************************************************
   *
   * @function:
   *   FT_Get_Char_Index
   *
   * @description:
   *   Return the glyph index of a given character code.  This function uses
   *   the currently selected charmap to do the mapping.
   *
   * @input:
   *   face ::
   *     A handle to the source face object.
   *
   *   charcode ::
   *     The character code.
   *
   * @return:
   *   The glyph index.  0~means 'undefined character code'.
   *
   * @note:
   *   If you use FreeType to manipulate the contents of font files directly,
   *   be aware that the glyph index returned by this function doesn't always
   *   correspond to the internal indices used within the file.  This is done
   *   to ensure that value~0 always corresponds to the 'missing glyph'.  If
   *   the first glyph is not named '.notdef', then for Type~1 and Type~42
   *   fonts, '.notdef' will be moved into the glyph ID~0 position, and
   *   whatever was there will be moved to the position '.notdef' had.  For
   *   Type~1 fonts, if there is no '.notdef' glyph at all, then one will be
   *   created at index~0 and whatever was there will be moved to the last
   *   index -- Type~42 fonts are considered invalid under this condition.
   */
  FT_EXPORT( FT_UInt )
  FT_Get_Char_Index( FT_Face   face,
					 FT_ULong  charcode );

  /**************************************************************************
   *
   * @function:
   *   FT_Get_First_Char
   *
   * @description:
   *   Return the first character code in the current charmap of a given
   *   face, together with its corresponding glyph index.
   *
   * @input:
   *   face ::
   *     A handle to the source face object.
   *
   * @output:
   *   agindex ::
   *     Glyph index of first character code.  0~if charmap is empty.
   *
   * @return:
   *   The charmap's first character code.
   *
   * @note:
   *   You should use this function together with @FT_Get_Next_Char to parse
   *   all character codes available in a given charmap.  The code should
   *   look like this:
   *
   *   ```
   *     FT_ULong  charcode;
   *     FT_UInt   gindex;
   *
   *
   *     charcode = FT_Get_First_Char( face, &gindex );
   *     while ( gindex != 0 )
   *     {
   *       ... do something with (charcode,gindex) pair ...
   *
   *       charcode = FT_Get_Next_Char( face, charcode, &gindex );
   *     }
   *   ```
   *
   *   Be aware that character codes can have values up to 0xFFFFFFFF; this
   *   might happen for non-Unicode or malformed cmaps.  However, even with
   *   regular Unicode encoding, so-called 'last resort fonts' (using SFNT
   *   cmap format 13, see function @FT_Get_CMap_Format) normally have
   *   entries for all Unicode characters up to 0x1FFFFF, which can cause *a
   *   lot* of iterations.
   *
   *   Note that `*agindex` is set to~0 if the charmap is empty.  The result
   *   itself can be~0 in two cases: if the charmap is empty or if the
   *   value~0 is the first valid character code.
   */
  FT_EXPORT( FT_ULong )
  FT_Get_First_Char( FT_Face   face,
					 FT_UInt  *agindex );

  /**************************************************************************
   *
   * @function:
   *   FT_Get_Next_Char
   *
   * @description:
   *   Return the next character code in the current charmap of a given face
   *   following the value `char_code`, as well as the corresponding glyph
   *   index.
   *
   * @input:
   *   face ::
   *     A handle to the source face object.
   *
   *   char_code ::
   *     The starting character code.
   *
   * @output:
   *   agindex ::
   *     Glyph index of next character code.  0~if charmap is empty.
   *
   * @return:
   *   The charmap's next character code.
   *
   * @note:
   *   You should use this function with @FT_Get_First_Char to walk over all
   *   character codes available in a given charmap.  See the note for that
   *   function for a simple code example.
   *
   *   Note that `*agindex` is set to~0 when there are no more codes in the
   *   charmap.
   */
  FT_EXPORT( FT_ULong )
  FT_Get_Next_Char( FT_Face    face,
					FT_ULong   char_code,
					FT_UInt   *agindex );

  /**************************************************************************
   *
   * @section:
   *   face_creation
   *
   */

  /**************************************************************************
   *
   * @function:
   *   FT_Face_Properties
   *
   * @description:
   *   Set or override certain (library or module-wide) properties on a
   *   face-by-face basis.  Useful for finer-grained control and avoiding
   *   locks on shared structures (threads can modify their own faces as they
   *   see fit).
   *
   *   Contrary to @FT_Property_Set, this function uses @FT_Parameter so that
   *   you can pass multiple properties to the target face in one call.  Note
   *   that only a subset of the available properties can be controlled.
   *
   *   * @FT_PARAM_TAG_STEM_DARKENING (stem darkening, corresponding to the
   *     property `no-stem-darkening` provided by the 'autofit', 'cff',
   *     'type1', and 't1cid' modules; see @no-stem-darkening).
   *
   *   * @FT_PARAM_TAG_LCD_FILTER_WEIGHTS (LCD filter weights, corresponding
   *     to function @FT_Library_SetLcdFilterWeights).
   *
   *   * @FT_PARAM_TAG_RANDOM_SEED (seed value for the CFF, Type~1, and CID
   *     'random' operator, corresponding to the `random-seed` property
   *     provided by the 'cff', 'type1', and 't1cid' modules; see
   *     @random-seed).
   *
   *   Pass `NULL` as `data` in @FT_Parameter for a given tag to reset the
   *   option and use the library or module default again.
   *
   * @input:
   *   face ::
   *     A handle to the source face object.
   *
   *   num_properties ::
   *     The number of properties that follow.
   *
   *   properties ::
   *     A handle to an @FT_Parameter array with `num_properties` elements.
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @example:
   *   Here is an example that sets three properties.  You must define
   *   `FT_CONFIG_OPTION_SUBPIXEL_RENDERING` to make the LCD filter examples
   *   work.
   *
   *   ```
   *     FT_Parameter         property1;
   *     FT_Bool              darken_stems = 1;
   *
   *     FT_Parameter         property2;
   *     FT_LcdFiveTapFilter  custom_weight =
   *                            { 0x11, 0x44, 0x56, 0x44, 0x11 };
   *
   *     FT_Parameter         property3;
   *     FT_Int32             random_seed = 314159265;
   *
   *     FT_Parameter         properties[3] = { property1,
   *                                            property2,
   *                                            property3 };
   *
   *
   *     property1.tag  = FT_PARAM_TAG_STEM_DARKENING;
   *     property1.data = &darken_stems;
   *
   *     property2.tag  = FT_PARAM_TAG_LCD_FILTER_WEIGHTS;
   *     property2.data = custom_weight;
   *
   *     property3.tag  = FT_PARAM_TAG_RANDOM_SEED;
   *     property3.data = &random_seed;
   *
   *     FT_Face_Properties( face, 3, properties );
   *   ```
   *
   *   The next example resets a single property to its default value.
   *
   *   ```
   *     FT_Parameter  property;
   *
   *
   *     property.tag  = FT_PARAM_TAG_LCD_FILTER_WEIGHTS;
   *     property.data = NULL;
   *
   *     FT_Face_Properties( face, 1, &property );
   *   ```
   *
   * @since:
   *   2.8
   *
   */
  FT_EXPORT( FT_Error )
  FT_Face_Properties( FT_Face        face,
					  FT_UInt        num_properties,
					  FT_Parameter*  properties );

  /**************************************************************************
   *
   * @section:
   *   information_retrieval
   *
   */

  /**************************************************************************
   *
   * @function:
   *   FT_Get_Name_Index
   *
   * @description:
   *   Return the glyph index of a given glyph name.  This only works
   *   for those faces where @FT_HAS_GLYPH_NAMES returns true.
   *
   * @input:
   *   face ::
   *     A handle to the source face object.
   *
   *   glyph_name ::
   *     The glyph name.
   *
   * @return:
   *   The glyph index.  0~means 'undefined character code'.
   *
   * @note:
   *   Acceptable glyph names might come from the [Adobe Glyph
   *   List](https://github.com/adobe-type-tools/agl-aglfn).  See
   *   @FT_Get_Glyph_Name for the inverse functionality.
   *
   *   This function has limited capabilities if the config macro
   *   `FT_CONFIG_OPTION_POSTSCRIPT_NAMES` is not defined in `ftoption.h`:
   *   It then works only for fonts that actually embed glyph names (which
   *   many recent OpenType fonts do not).
   */
  FT_EXPORT( FT_UInt )
  FT_Get_Name_Index( FT_Face           face,
					 const FT_String*  glyph_name );

  /**************************************************************************
   *
   * @function:
   *   FT_Get_Glyph_Name
   *
   * @description:
   *   Retrieve the ASCII name of a given glyph in a face.  This only works
   *   for those faces where @FT_HAS_GLYPH_NAMES returns true.
   *
   * @input:
   *   face ::
   *     A handle to a source face object.
   *
   *   glyph_index ::
   *     The glyph index.
   *
   *   buffer_max ::
   *     The maximum number of bytes available in the buffer.
   *
   * @output:
   *   buffer ::
   *     A pointer to a target buffer where the name is copied to.
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @note:
   *   An error is returned if the face doesn't provide glyph names or if the
   *   glyph index is invalid.  In all cases of failure, the first byte of
   *   `buffer` is set to~0 to indicate an empty name.
   *
   *   The glyph name is truncated to fit within the buffer if it is too
   *   long.  The returned string is always zero-terminated.
   *
   *   Be aware that FreeType reorders glyph indices internally so that glyph
   *   index~0 always corresponds to the 'missing glyph' (called '.notdef').
   *
   *   This function has limited capabilities if the config macro
   *   `FT_CONFIG_OPTION_POSTSCRIPT_NAMES` is not defined in `ftoption.h`:
   *   It then works only for fonts that actually embed glyph names (which
   *   many recent OpenType fonts do not).
   */
  FT_EXPORT( FT_Error )
  FT_Get_Glyph_Name( FT_Face     face,
					 FT_UInt     glyph_index,
					 FT_Pointer  buffer,
					 FT_UInt     buffer_max );

  /**************************************************************************
   *
   * @function:
   *   FT_Get_Postscript_Name
   *
   * @description:
   *   Retrieve the ASCII PostScript name of a given face, if available.
   *   This only works with PostScript, TrueType, and OpenType fonts.
   *
   * @input:
   *   face ::
   *     A handle to the source face object.
   *
   * @return:
   *   A pointer to the face's PostScript name.  `NULL` if unavailable.
   *
   * @note:
   *   The returned pointer is owned by the face and is destroyed with it.
   *
   *   For variation fonts, this string changes if you select a different
   *   instance, and you have to call `FT_Get_PostScript_Name` again to
   *   retrieve it.  FreeType follows Adobe TechNote #5902, 'Generating
   *   PostScript Names for Fonts Using OpenType Font Variations'.
   *
   *     https://download.macromedia.com/pub/developer/opentype/tech-notes/5902.AdobePSNameGeneration.html
   *
   *   [Since 2.9] Special PostScript names for named instances are only
   *   returned if the named instance is set with @FT_Set_Named_Instance (and
   *   the font has corresponding entries in its 'fvar' table or is the
   *   default named instance).  If @FT_IS_VARIATION returns true, the
   *   algorithmically derived PostScript name is provided, not looking up
   *   special entries for named instances.
   */
  FT_EXPORT( const char* )
  FT_Get_Postscript_Name( FT_Face  face );

  /**************************************************************************
   *
   * @enum:
   *   FT_SUBGLYPH_FLAG_XXX
   *
   * @description:
   *   A list of constants describing subglyphs.  Please refer to the 'glyf'
   *   table description in the OpenType specification for the meaning of the
   *   various flags (which get synthesized for non-OpenType subglyphs).
   *
   *     https://docs.microsoft.com/en-us/typography/opentype/spec/glyf#composite-glyph-description
   *
   * @values:
   *   FT_SUBGLYPH_FLAG_ARGS_ARE_WORDS ::
   *   FT_SUBGLYPH_FLAG_ARGS_ARE_XY_VALUES ::
   *   FT_SUBGLYPH_FLAG_ROUND_XY_TO_GRID ::
   *   FT_SUBGLYPH_FLAG_SCALE ::
   *   FT_SUBGLYPH_FLAG_XY_SCALE ::
   *   FT_SUBGLYPH_FLAG_2X2 ::
   *   FT_SUBGLYPH_FLAG_USE_MY_METRICS ::
   *
   */
#define FT_SUBGLYPH_FLAG_ARGS_ARE_WORDS          1
#define FT_SUBGLYPH_FLAG_ARGS_ARE_XY_VALUES      2
#define FT_SUBGLYPH_FLAG_ROUND_XY_TO_GRID        4
#define FT_SUBGLYPH_FLAG_SCALE                   8
#define FT_SUBGLYPH_FLAG_XY_SCALE             0x40
#define FT_SUBGLYPH_FLAG_2X2                  0x80
#define FT_SUBGLYPH_FLAG_USE_MY_METRICS      0x200

  /**************************************************************************
   *
   * @function:
   *   FT_Get_SubGlyph_Info
   *
   * @description:
   *   Retrieve a description of a given subglyph.  Only use it if
   *   `glyph->format` is @FT_GLYPH_FORMAT_COMPOSITE; an error is returned
   *   otherwise.
   *
   * @input:
   *   glyph ::
   *     The source glyph slot.
   *
   *   sub_index ::
   *     The index of the subglyph.  Must be less than
   *     `glyph->num_subglyphs`.
   *
   * @output:
   *   p_index ::
   *     The glyph index of the subglyph.
   *
   *   p_flags ::
   *     The subglyph flags, see @FT_SUBGLYPH_FLAG_XXX.
   *
   *   p_arg1 ::
   *     The subglyph's first argument (if any).
   *
   *   p_arg2 ::
   *     The subglyph's second argument (if any).
   *
   *   p_transform ::
   *     The subglyph transformation (if any).
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @note:
   *   The values of `*p_arg1`, `*p_arg2`, and `*p_transform` must be
   *   interpreted depending on the flags returned in `*p_flags`.  See the
   *   OpenType specification for details.
   *
   *     https://docs.microsoft.com/en-us/typography/opentype/spec/glyf#composite-glyph-description
   *
   */
  FT_EXPORT( FT_Error )
  FT_Get_SubGlyph_Info( FT_GlyphSlot  glyph,
						FT_UInt       sub_index,
						FT_Int       *p_index,
						FT_UInt      *p_flags,
						FT_Int       *p_arg1,
						FT_Int       *p_arg2,
						FT_Matrix    *p_transform );

  /**************************************************************************
   *
   * @enum:
   *   FT_FSTYPE_XXX
   *
   * @description:
   *   A list of bit flags used in the `fsType` field of the OS/2 table in a
   *   TrueType or OpenType font and the `FSType` entry in a PostScript font.
   *   These bit flags are returned by @FT_Get_FSType_Flags; they inform
   *   client applications of embedding and subsetting restrictions
   *   associated with a font.
   *
   *   See
   *   https://www.adobe.com/content/dam/Adobe/en/devnet/acrobat/pdfs/FontPolicies.pdf
   *   for more details.
   *
   * @values:
   *   FT_FSTYPE_INSTALLABLE_EMBEDDING ::
   *     Fonts with no fsType bit set may be embedded and permanently
   *     installed on the remote system by an application.
   *
   *   FT_FSTYPE_RESTRICTED_LICENSE_EMBEDDING ::
   *     Fonts that have only this bit set must not be modified, embedded or
   *     exchanged in any manner without first obtaining permission of the
   *     font software copyright owner.
   *
   *   FT_FSTYPE_PREVIEW_AND_PRINT_EMBEDDING ::
   *     The font may be embedded and temporarily loaded on the remote
   *     system.  Documents containing Preview & Print fonts must be opened
   *     'read-only'; no edits can be applied to the document.
   *
   *   FT_FSTYPE_EDITABLE_EMBEDDING ::
   *     The font may be embedded but must only be installed temporarily on
   *     other systems.  In contrast to Preview & Print fonts, documents
   *     containing editable fonts may be opened for reading, editing is
   *     permitted, and changes may be saved.
   *
   *   FT_FSTYPE_NO_SUBSETTING ::
   *     The font may not be subsetted prior to embedding.
   *
   *   FT_FSTYPE_BITMAP_EMBEDDING_ONLY ::
   *     Only bitmaps contained in the font may be embedded; no outline data
   *     may be embedded.  If there are no bitmaps available in the font,
   *     then the font is unembeddable.
   *
   * @note:
   *   The flags are ORed together, thus more than a single value can be
   *   returned.
   *
   *   While the `fsType` flags can indicate that a font may be embedded, a
   *   license with the font vendor may be separately required to use the
   *   font in this way.
   */
#define FT_FSTYPE_INSTALLABLE_EMBEDDING         0x0000
#define FT_FSTYPE_RESTRICTED_LICENSE_EMBEDDING  0x0002
#define FT_FSTYPE_PREVIEW_AND_PRINT_EMBEDDING   0x0004
#define FT_FSTYPE_EDITABLE_EMBEDDING            0x0008
#define FT_FSTYPE_NO_SUBSETTING                 0x0100
#define FT_FSTYPE_BITMAP_EMBEDDING_ONLY         0x0200

  /**************************************************************************
   *
   * @function:
   *   FT_Get_FSType_Flags
   *
   * @description:
   *   Return the `fsType` flags for a font.
   *
   * @input:
   *   face ::
   *     A handle to the source face object.
   *
   * @return:
   *   The `fsType` flags, see @FT_FSTYPE_XXX.
   *
   * @note:
   *   Use this function rather than directly reading the `fs_type` field in
   *   the @PS_FontInfoRec structure, which is only guaranteed to return the
   *   correct results for Type~1 fonts.
   *
   * @since:
   *   2.3.8
   *
   */
  FT_EXPORT( FT_UShort )
  FT_Get_FSType_Flags( FT_Face  face );

  /**************************************************************************
   *
   * @section:
   *   glyph_variants
   *
   * @title:
   *   Unicode Variation Sequences
   *
   * @abstract:
   *   The FreeType~2 interface to Unicode Variation Sequences (UVS), using
   *   the SFNT cmap format~14.
   *
   * @description:
   *   Many characters, especially for CJK scripts, have variant forms.  They
   *   are a sort of grey area somewhere between being totally irrelevant and
   *   semantically distinct; for this reason, the Unicode consortium decided
   *   to introduce Variation Sequences (VS), consisting of a Unicode base
   *   character and a variation selector instead of further extending the
   *   already huge number of characters.
   *
   *   Unicode maintains two different sets, namely 'Standardized Variation
   *   Sequences' and registered 'Ideographic Variation Sequences' (IVS),
   *   collected in the 'Ideographic Variation Database' (IVD).
   *
   *     https://unicode.org/Public/UCD/latest/ucd/StandardizedVariants.txt
   *     https://unicode.org/reports/tr37/ https://unicode.org/ivd/
   *
   *   To date (January 2017), the character with the most ideographic
   *   variations is U+9089, having 32 such IVS.
   *
   *   Three Mongolian Variation Selectors have the values U+180B-U+180D; 256
   *   generic Variation Selectors are encoded in the ranges U+FE00-U+FE0F
   *   and U+E0100-U+E01EF.  IVS currently use Variation Selectors from the
   *   range U+E0100-U+E01EF only.
   *
   *   A VS consists of the base character value followed by a single
   *   Variation Selector.  For example, to get the first variation of
   *   U+9089, you have to write the character sequence `U+9089 U+E0100`.
   *
   *   Adobe and MS decided to support both standardized and ideographic VS
   *   with a new cmap subtable (format~14).  It is an odd subtable because
   *   it is not a mapping of input code points to glyphs, but contains lists
   *   of all variations supported by the font.
   *
   *   A variation may be either 'default' or 'non-default' for a given font.
   *   A default variation is the one you will get for that code point if you
   *   look it up in the standard Unicode cmap.  A non-default variation is a
   *   different glyph.
   *
   */

  /**************************************************************************
   *
   * @function:
   *   FT_Face_GetCharVariantIndex
   *
   * @description:
   *   Return the glyph index of a given character code as modified by the
   *   variation selector.
   *
   * @input:
   *   face ::
   *     A handle to the source face object.
   *
   *   charcode ::
   *     The character code point in Unicode.
   *
   *   variantSelector ::
   *     The Unicode code point of the variation selector.
   *
   * @return:
   *   The glyph index.  0~means either 'undefined character code', or
   *   'undefined selector code', or 'no variation selector cmap subtable',
   *   or 'current CharMap is not Unicode'.
   *
   * @note:
   *   If you use FreeType to manipulate the contents of font files directly,
   *   be aware that the glyph index returned by this function doesn't always
   *   correspond to the internal indices used within the file.  This is done
   *   to ensure that value~0 always corresponds to the 'missing glyph'.
   *
   *   This function is only meaningful if
   *     a) the font has a variation selector cmap sub table, and
   *     b) the current charmap has a Unicode encoding.
   *
   * @since:
   *   2.3.6
   *
   */
  FT_EXPORT( FT_UInt )
  FT_Face_GetCharVariantIndex( FT_Face   face,
							   FT_ULong  charcode,
							   FT_ULong  variantSelector );

  /**************************************************************************
   *
   * @function:
   *   FT_Face_GetCharVariantIsDefault
   *
   * @description:
   *   Check whether this variation of this Unicode character is the one to
   *   be found in the charmap.
   *
   * @input:
   *   face ::
   *     A handle to the source face object.
   *
   *   charcode ::
   *     The character codepoint in Unicode.
   *
   *   variantSelector ::
   *     The Unicode codepoint of the variation selector.
   *
   * @return:
   *   1~if found in the standard (Unicode) cmap, 0~if found in the variation
   *   selector cmap, or -1 if it is not a variation.
   *
   * @note:
   *   This function is only meaningful if the font has a variation selector
   *   cmap subtable.
   *
   * @since:
   *   2.3.6
   *
   */
  FT_EXPORT( FT_Int )
  FT_Face_GetCharVariantIsDefault( FT_Face   face,
								   FT_ULong  charcode,
								   FT_ULong  variantSelector );

  /**************************************************************************
   *
   * @function:
   *   FT_Face_GetVariantSelectors
   *
   * @description:
   *   Return a zero-terminated list of Unicode variation selectors found in
   *   the font.
   *
   * @input:
   *   face ::
   *     A handle to the source face object.
   *
   * @return:
   *   A pointer to an array of selector code points, or `NULL` if there is
   *   no valid variation selector cmap subtable.
   *
   * @note:
   *   The last item in the array is~0; the array is owned by the @FT_Face
   *   object but can be overwritten or released on the next call to a
   *   FreeType function.
   *
   * @since:
   *   2.3.6
   *
   */
  FT_EXPORT( FT_UInt32* )
  FT_Face_GetVariantSelectors( FT_Face  face );

  /**************************************************************************
   *
   * @function:
   *   FT_Face_GetVariantsOfChar
   *
   * @description:
   *   Return a zero-terminated list of Unicode variation selectors found for
   *   the specified character code.
   *
   * @input:
   *   face ::
   *     A handle to the source face object.
   *
   *   charcode ::
   *     The character codepoint in Unicode.
   *
   * @return:
   *   A pointer to an array of variation selector code points that are
   *   active for the given character, or `NULL` if the corresponding list is
   *   empty.
   *
   * @note:
   *   The last item in the array is~0; the array is owned by the @FT_Face
   *   object but can be overwritten or released on the next call to a
   *   FreeType function.
   *
   * @since:
   *   2.3.6
   *
   */
  FT_EXPORT( FT_UInt32* )
  FT_Face_GetVariantsOfChar( FT_Face   face,
							 FT_ULong  charcode );

  /**************************************************************************
   *
   * @function:
   *   FT_Face_GetCharsOfVariant
   *
   * @description:
   *   Return a zero-terminated list of Unicode character codes found for the
   *   specified variation selector.
   *
   * @input:
   *   face ::
   *     A handle to the source face object.
   *
   *   variantSelector ::
   *     The variation selector code point in Unicode.
   *
   * @return:
   *   A list of all the code points that are specified by this selector
   *   (both default and non-default codes are returned) or `NULL` if there
   *   is no valid cmap or the variation selector is invalid.
   *
   * @note:
   *   The last item in the array is~0; the array is owned by the @FT_Face
   *   object but can be overwritten or released on the next call to a
   *   FreeType function.
   *
   * @since:
   *   2.3.6
   *
   */
  FT_EXPORT( FT_UInt32* )
  FT_Face_GetCharsOfVariant( FT_Face   face,
							 FT_ULong  variantSelector );

  /**************************************************************************
   *
   * @section:
   *   computations
   *
   * @title:
   *   Computations
   *
   * @abstract:
   *   Crunching fixed numbers and vectors.
   *
   * @description:
   *   This section contains various functions used to perform computations
   *   on 16.16 fixed-point numbers or 2D vectors.  FreeType does not use
   *   floating-point data types.
   *
   *   **Attention**: Most arithmetic functions take `FT_Long` as arguments.
   *   For historical reasons, FreeType was designed under the assumption
   *   that `FT_Long` is a 32-bit integer; results can thus be undefined if
   *   the arguments don't fit into 32 bits.
   *
   * @order:
   *   FT_MulDiv
   *   FT_MulFix
   *   FT_DivFix
   *   FT_RoundFix
   *   FT_CeilFix
   *   FT_FloorFix
   *   FT_Vector_Transform
   *   FT_Matrix_Multiply
   *   FT_Matrix_Invert
   *
   */

  /**************************************************************************
   *
   * @function:
   *   FT_MulDiv
   *
   * @description:
   *   Compute `(a*b)/c` with maximum accuracy, using a 64-bit intermediate
   *   integer whenever necessary.
   *
   *   This function isn't necessarily as fast as some processor-specific
   *   operations, but is at least completely portable.
   *
   * @input:
   *   a ::
   *     The first multiplier.
   *
   *   b ::
   *     The second multiplier.
   *
   *   c ::
   *     The divisor.
   *
   * @return:
   *   The result of `(a*b)/c`.  This function never traps when trying to
   *   divide by zero; it simply returns 'MaxInt' or 'MinInt' depending on
   *   the signs of `a` and `b`.
   */
  FT_EXPORT( FT_Long )
  FT_MulDiv( FT_Long  a,
			 FT_Long  b,
			 FT_Long  c );

  /**************************************************************************
   *
   * @function:
   *   FT_MulFix
   *
   * @description:
   *   Compute `(a*b)/0x10000` with maximum accuracy.  Its main use is to
   *   multiply a given value by a 16.16 fixed-point factor.
   *
   * @input:
   *   a ::
   *     The first multiplier.
   *
   *   b ::
   *     The second multiplier.  Use a 16.16 factor here whenever possible
   *     (see note below).
   *
   * @return:
   *   The result of `(a*b)/0x10000`.
   *
   * @note:
   *   This function has been optimized for the case where the absolute value
   *   of `a` is less than 2048, and `b` is a 16.16 scaling factor.  As this
   *   happens mainly when scaling from notional units to fractional pixels
   *   in FreeType, it resulted in noticeable speed improvements between
   *   versions 2.x and 1.x.
   *
   *   As a conclusion, always try to place a 16.16 factor as the _second_
   *   argument of this function; this can make a great difference.
   */
  FT_EXPORT( FT_Long )
  FT_MulFix( FT_Long  a,
			 FT_Long  b );

  /**************************************************************************
   *
   * @function:
   *   FT_DivFix
   *
   * @description:
   *   Compute `(a*0x10000)/b` with maximum accuracy.  Its main use is to
   *   divide a given value by a 16.16 fixed-point factor.
   *
   * @input:
   *   a ::
   *     The numerator.
   *
   *   b ::
   *     The denominator.  Use a 16.16 factor here.
   *
   * @return:
   *   The result of `(a*0x10000)/b`.
   */
  FT_EXPORT( FT_Long )
  FT_DivFix( FT_Long  a,
			 FT_Long  b );

  /**************************************************************************
   *
   * @function:
   *   FT_RoundFix
   *
   * @description:
   *   Round a 16.16 fixed number.
   *
   * @input:
   *   a ::
   *     The number to be rounded.
   *
   * @return:
   *   `a` rounded to the nearest 16.16 fixed integer, halfway cases away
   *   from zero.
   *
   * @note:
   *   The function uses wrap-around arithmetic.
   */
  FT_EXPORT( FT_Fixed )
  FT_RoundFix( FT_Fixed  a );

  /**************************************************************************
   *
   * @function:
   *   FT_CeilFix
   *
   * @description:
   *   Compute the smallest following integer of a 16.16 fixed number.
   *
   * @input:
   *   a ::
   *     The number for which the ceiling function is to be computed.
   *
   * @return:
   *   `a` rounded towards plus infinity.
   *
   * @note:
   *   The function uses wrap-around arithmetic.
   */
  FT_EXPORT( FT_Fixed )
  FT_CeilFix( FT_Fixed  a );

  /**************************************************************************
   *
   * @function:
   *   FT_FloorFix
   *
   * @description:
   *   Compute the largest previous integer of a 16.16 fixed number.
   *
   * @input:
   *   a ::
   *     The number for which the floor function is to be computed.
   *
   * @return:
   *   `a` rounded towards minus infinity.
   */
  FT_EXPORT( FT_Fixed )
  FT_FloorFix( FT_Fixed  a );

  /**************************************************************************
   *
   * @function:
   *   FT_Vector_Transform
   *
   * @description:
   *   Transform a single vector through a 2x2 matrix.
   *
   * @inout:
   *   vector ::
   *     The target vector to transform.
   *
   * @input:
   *   matrix ::
   *     A pointer to the source 2x2 matrix.
   *
   * @note:
   *   The result is undefined if either `vector` or `matrix` is invalid.
   */
  FT_EXPORT( void )
  FT_Vector_Transform( FT_Vector*        vector,
					   const FT_Matrix*  matrix );

  /**************************************************************************
   *
   * @section:
   *   library_setup
   *
   */

  /**************************************************************************
   *
   * @enum:
   *   FREETYPE_XXX
   *
   * @description:
   *   These three macros identify the FreeType source code version.  Use
   *   @FT_Library_Version to access them at runtime.
   *
   * @values:
   *   FREETYPE_MAJOR ::
   *     The major version number.
   *   FREETYPE_MINOR ::
   *     The minor version number.
   *   FREETYPE_PATCH ::
   *     The patch level.
   *
   * @note:
   *   The version number of FreeType if built as a dynamic link library with
   *   the 'libtool' package is _not_ controlled by these three macros.
   *
   */
#define FREETYPE_MAJOR  2
#define FREETYPE_MINOR  13
#define FREETYPE_PATCH  2

  /**************************************************************************
   *
   * @function:
   *   FT_Library_Version
   *
   * @description:
   *   Return the version of the FreeType library being used.  This is useful
   *   when dynamically linking to the library, since one cannot use the
   *   macros @FREETYPE_MAJOR, @FREETYPE_MINOR, and @FREETYPE_PATCH.
   *
   * @input:
   *   library ::
   *     A source library handle.
   *
   * @output:
   *   amajor ::
   *     The major version number.
   *
   *   aminor ::
   *     The minor version number.
   *
   *   apatch ::
   *     The patch version number.
   *
   * @note:
   *   The reason why this function takes a `library` argument is because
   *   certain programs implement library initialization in a custom way that
   *   doesn't use @FT_Init_FreeType.
   *
   *   In such cases, the library version might not be available before the
   *   library object has been created.
   */
  FT_EXPORT( void )
  FT_Library_Version( FT_Library   library,
					  FT_Int      *amajor,
					  FT_Int      *aminor,
					  FT_Int      *apatch );

  /**************************************************************************
   *
   * @section:
   *   other_api_data
   *
   */

  /**************************************************************************
   *
   * @function:
   *   FT_Face_CheckTrueTypePatents
   *
   * @description:
   *   Deprecated, does nothing.
   *
   * @input:
   *   face ::
   *     A face handle.
   *
   * @return:
   *   Always returns false.
   *
   * @note:
   *   Since May 2010, TrueType hinting is no longer patented.
   *
   * @since:
   *   2.3.5
   *
   */
  FT_EXPORT( FT_Bool )
  FT_Face_CheckTrueTypePatents( FT_Face  face );

  /**************************************************************************
   *
   * @function:
   *   FT_Face_SetUnpatentedHinting
   *
   * @description:
   *   Deprecated, does nothing.
   *
   * @input:
   *   face ::
   *     A face handle.
   *
   *   value ::
   *     New boolean setting.
   *
   * @return:
   *   Always returns false.
   *
   * @note:
   *   Since May 2010, TrueType hinting is no longer patented.
   *
   * @since:
   *   2.3.5
   *
   */
  FT_EXPORT( FT_Bool )
  FT_Face_SetUnpatentedHinting( FT_Face  face,
								FT_Bool  value );

  /* */

FT_END_HEADER

#endif /* FREETYPE_H_ */

/* END */

/*** End of inlined file: freetype.h ***/

/*
 * These headers are normally included manually but we add them
 * here for completeness.
 *
 */

/*** Start of inlined file: ftgasp.h ***/
#ifndef FTGASP_H_
#define FTGASP_H_

#ifdef FREETYPE_H
#error "freetype.h of FreeType 1 has been loaded!"
#error "Please fix the directory search order for header files"
#error "so that freetype.h of FreeType 2 is found first."
#endif

FT_BEGIN_HEADER

  /**************************************************************************
   *
   * @section:
   *   gasp_table
   *
   * @title:
   *   Gasp Table
   *
   * @abstract:
   *   Retrieving TrueType 'gasp' table entries.
   *
   * @description:
   *   The function @FT_Get_Gasp can be used to query a TrueType or OpenType
   *   font for specific entries in its 'gasp' table, if any.  This is mainly
   *   useful when implementing native TrueType hinting with the bytecode
   *   interpreter to duplicate the Windows text rendering results.
   */

  /**************************************************************************
   *
   * @enum:
   *   FT_GASP_XXX
   *
   * @description:
   *   A list of values and/or bit-flags returned by the @FT_Get_Gasp
   *   function.
   *
   * @values:
   *   FT_GASP_NO_TABLE ::
   *     This special value means that there is no GASP table in this face.
   *     It is up to the client to decide what to do.
   *
   *   FT_GASP_DO_GRIDFIT ::
   *     Grid-fitting and hinting should be performed at the specified ppem.
   *     This **really** means TrueType bytecode interpretation.  If this bit
   *     is not set, no hinting gets applied.
   *
   *   FT_GASP_DO_GRAY ::
   *     Anti-aliased rendering should be performed at the specified ppem.
   *     If not set, do monochrome rendering.
   *
   *   FT_GASP_SYMMETRIC_SMOOTHING ::
   *     If set, smoothing along multiple axes must be used with ClearType.
   *
   *   FT_GASP_SYMMETRIC_GRIDFIT ::
   *     Grid-fitting must be used with ClearType's symmetric smoothing.
   *
   * @note:
   *   The bit-flags `FT_GASP_DO_GRIDFIT` and `FT_GASP_DO_GRAY` are to be
   *   used for standard font rasterization only.  Independently of that,
   *   `FT_GASP_SYMMETRIC_SMOOTHING` and `FT_GASP_SYMMETRIC_GRIDFIT` are to
   *   be used if ClearType is enabled (and `FT_GASP_DO_GRIDFIT` and
   *   `FT_GASP_DO_GRAY` are consequently ignored).
   *
   *   'ClearType' is Microsoft's implementation of LCD rendering, partly
   *   protected by patents.
   *
   * @since:
   *   2.3.0
   */
#define FT_GASP_NO_TABLE               -1
#define FT_GASP_DO_GRIDFIT           0x01
#define FT_GASP_DO_GRAY              0x02
#define FT_GASP_SYMMETRIC_GRIDFIT    0x04
#define FT_GASP_SYMMETRIC_SMOOTHING  0x08

  /**************************************************************************
   *
   * @function:
   *   FT_Get_Gasp
   *
   * @description:
   *   For a TrueType or OpenType font file, return the rasterizer behaviour
   *   flags from the font's 'gasp' table corresponding to a given character
   *   pixel size.
   *
   * @input:
   *   face ::
   *     The source face handle.
   *
   *   ppem ::
   *     The vertical character pixel size.
   *
   * @return:
   *   Bit flags (see @FT_GASP_XXX), or @FT_GASP_NO_TABLE if there is no
   *   'gasp' table in the face.
   *
   * @note:
   *   If you want to use the MM functionality of OpenType variation fonts
   *   (i.e., using @FT_Set_Var_Design_Coordinates and friends), call this
   *   function **after** setting an instance since the return values can
   *   change.
   *
   * @since:
   *   2.3.0
   */
  FT_EXPORT( FT_Int )
  FT_Get_Gasp( FT_Face  face,
			   FT_UInt  ppem );

  /* */

FT_END_HEADER

#endif /* FTGASP_H_ */

/* END */

/*** End of inlined file: ftgasp.h ***/



/*** Start of inlined file: ftglyph.h ***/
  /**************************************************************************
   *
   * This file contains the definition of several convenience functions that
   * can be used by client applications to easily retrieve glyph bitmaps and
   * outlines from a given face.
   *
   * These functions should be optional if you are writing a font server or
   * text layout engine on top of FreeType.  However, they are pretty handy
   * for many other simple uses of the library.
   *
   */

#ifndef FTGLYPH_H_
#define FTGLYPH_H_

#ifdef FREETYPE_H
#error "freetype.h of FreeType 1 has been loaded!"
#error "Please fix the directory search order for header files"
#error "so that freetype.h of FreeType 2 is found first."
#endif

FT_BEGIN_HEADER

  /**************************************************************************
   *
   * @section:
   *   glyph_management
   *
   * @title:
   *   Glyph Management
   *
   * @abstract:
   *   Generic interface to manage individual glyph data.
   *
   * @description:
   *   This section contains definitions used to manage glyph data through
   *   generic @FT_Glyph objects.  Each of them can contain a bitmap,
   *   a vector outline, or even images in other formats.  These objects are
   *   detached from @FT_Face, contrary to @FT_GlyphSlot.
   *
   */

  /* forward declaration to a private type */
  typedef struct FT_Glyph_Class_  FT_Glyph_Class;

  /**************************************************************************
   *
   * @type:
   *   FT_Glyph
   *
   * @description:
   *   Handle to an object used to model generic glyph images.  It is a
   *   pointer to the @FT_GlyphRec structure and can contain a glyph bitmap
   *   or pointer.
   *
   * @note:
   *   Glyph objects are not owned by the library.  You must thus release
   *   them manually (through @FT_Done_Glyph) _before_ calling
   *   @FT_Done_FreeType.
   */
  typedef struct FT_GlyphRec_*  FT_Glyph;

  /**************************************************************************
   *
   * @struct:
   *   FT_GlyphRec
   *
   * @description:
   *   The root glyph structure contains a given glyph image plus its advance
   *   width in 16.16 fixed-point format.
   *
   * @fields:
   *   library ::
   *     A handle to the FreeType library object.
   *
   *   clazz ::
   *     A pointer to the glyph's class.  Private.
   *
   *   format ::
   *     The format of the glyph's image.
   *
   *   advance ::
   *     A 16.16 vector that gives the glyph's advance width.
   */
  typedef struct  FT_GlyphRec_
  {
	FT_Library             library;
	const FT_Glyph_Class*  clazz;
	FT_Glyph_Format        format;
	FT_Vector              advance;

  } FT_GlyphRec;

  /**************************************************************************
   *
   * @type:
   *   FT_BitmapGlyph
   *
   * @description:
   *   A handle to an object used to model a bitmap glyph image.  This is a
   *   'sub-class' of @FT_Glyph, and a pointer to @FT_BitmapGlyphRec.
   */
  typedef struct FT_BitmapGlyphRec_*  FT_BitmapGlyph;

  /**************************************************************************
   *
   * @struct:
   *   FT_BitmapGlyphRec
   *
   * @description:
   *   A structure used for bitmap glyph images.  This really is a
   *   'sub-class' of @FT_GlyphRec.
   *
   * @fields:
   *   root ::
   *     The root fields of @FT_Glyph.
   *
   *   left ::
   *     The left-side bearing, i.e., the horizontal distance from the
   *     current pen position to the left border of the glyph bitmap.
   *
   *   top ::
   *     The top-side bearing, i.e., the vertical distance from the current
   *     pen position to the top border of the glyph bitmap.  This distance
   *     is positive for upwards~y!
   *
   *   bitmap ::
   *     A descriptor for the bitmap.
   *
   * @note:
   *   You can typecast an @FT_Glyph to @FT_BitmapGlyph if you have
   *   `glyph->format == FT_GLYPH_FORMAT_BITMAP`.  This lets you access the
   *   bitmap's contents easily.
   *
   *   The corresponding pixel buffer is always owned by @FT_BitmapGlyph and
   *   is thus created and destroyed with it.
   */
  typedef struct  FT_BitmapGlyphRec_
  {
	FT_GlyphRec  root;
	FT_Int       left;
	FT_Int       top;
	FT_Bitmap    bitmap;

  } FT_BitmapGlyphRec;

  /**************************************************************************
   *
   * @type:
   *   FT_OutlineGlyph
   *
   * @description:
   *   A handle to an object used to model an outline glyph image.  This is a
   *   'sub-class' of @FT_Glyph, and a pointer to @FT_OutlineGlyphRec.
   */
  typedef struct FT_OutlineGlyphRec_*  FT_OutlineGlyph;

  /**************************************************************************
   *
   * @struct:
   *   FT_OutlineGlyphRec
   *
   * @description:
   *   A structure used for outline (vectorial) glyph images.  This really is
   *   a 'sub-class' of @FT_GlyphRec.
   *
   * @fields:
   *   root ::
   *     The root @FT_Glyph fields.
   *
   *   outline ::
   *     A descriptor for the outline.
   *
   * @note:
   *   You can typecast an @FT_Glyph to @FT_OutlineGlyph if you have
   *   `glyph->format == FT_GLYPH_FORMAT_OUTLINE`.  This lets you access the
   *   outline's content easily.
   *
   *   As the outline is extracted from a glyph slot, its coordinates are
   *   expressed normally in 26.6 pixels, unless the flag @FT_LOAD_NO_SCALE
   *   was used in @FT_Load_Glyph or @FT_Load_Char.
   *
   *   The outline's tables are always owned by the object and are destroyed
   *   with it.
   */
  typedef struct  FT_OutlineGlyphRec_
  {
	FT_GlyphRec  root;
	FT_Outline   outline;

  } FT_OutlineGlyphRec;

  /**************************************************************************
   *
   * @type:
   *   FT_SvgGlyph
   *
   * @description:
   *   A handle to an object used to model an SVG glyph.  This is a
   *   'sub-class' of @FT_Glyph, and a pointer to @FT_SvgGlyphRec.
   *
   * @since:
   *   2.12
   */
  typedef struct FT_SvgGlyphRec_*  FT_SvgGlyph;

  /**************************************************************************
   *
   * @struct:
   *   FT_SvgGlyphRec
   *
   * @description:
   *   A structure used for OT-SVG glyphs.  This is a 'sub-class' of
   *   @FT_GlyphRec.
   *
   * @fields:
   *   root ::
   *     The root @FT_GlyphRec fields.
   *
   *   svg_document ::
   *     A pointer to the SVG document.
   *
   *   svg_document_length ::
   *     The length of `svg_document`.
   *
   *   glyph_index ::
   *     The index of the glyph to be rendered.
   *
   *   metrics ::
   *     A metrics object storing the size information.
   *
   *   units_per_EM ::
   *     The size of the EM square.
   *
   *   start_glyph_id ::
   *     The first glyph ID in the glyph range covered by this document.
   *
   *   end_glyph_id ::
   *     The last glyph ID in the glyph range covered by this document.
   *
   *   transform ::
   *     A 2x2 transformation matrix to apply to the glyph while rendering
   *     it.
   *
   *   delta ::
   *     Translation to apply to the glyph while rendering.
   *
   * @note:
   *   The Glyph Management API requires @FT_Glyph or its 'sub-class' to have
   *   all the information needed to completely define the glyph's rendering.
   *   Outline-based glyphs can directly apply transformations to the outline
   *   but this is not possible for an SVG document that hasn't been parsed.
   *   Therefore, the transformation is stored along with the document.  In
   *   the absence of a 'ViewBox' or 'Width'/'Height' attribute, the size of
   *   the ViewPort should be assumed to be 'units_per_EM'.
   */
  typedef struct  FT_SvgGlyphRec_
  {
	FT_GlyphRec  root;

	FT_Byte*  svg_document;
	FT_ULong  svg_document_length;

	FT_UInt  glyph_index;

	FT_Size_Metrics  metrics;
	FT_UShort        units_per_EM;

	FT_UShort  start_glyph_id;
	FT_UShort  end_glyph_id;

	FT_Matrix  transform;
	FT_Vector  delta;

  } FT_SvgGlyphRec;

  /**************************************************************************
   *
   * @function:
   *   FT_New_Glyph
   *
   * @description:
   *   A function used to create a new empty glyph image.  Note that the
   *   created @FT_Glyph object must be released with @FT_Done_Glyph.
   *
   * @input:
   *   library ::
   *     A handle to the FreeType library object.
   *
   *   format ::
   *     The format of the glyph's image.
   *
   * @output:
   *   aglyph ::
   *     A handle to the glyph object.
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @since:
   *   2.10
   */
  FT_EXPORT( FT_Error )
  FT_New_Glyph( FT_Library       library,
				FT_Glyph_Format  format,
				FT_Glyph         *aglyph );

  /**************************************************************************
   *
   * @function:
   *   FT_Get_Glyph
   *
   * @description:
   *   A function used to extract a glyph image from a slot.  Note that the
   *   created @FT_Glyph object must be released with @FT_Done_Glyph.
   *
   * @input:
   *   slot ::
   *     A handle to the source glyph slot.
   *
   * @output:
   *   aglyph ::
   *     A handle to the glyph object.  `NULL` in case of error.
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @note:
   *   Because `*aglyph->advance.x` and `*aglyph->advance.y` are 16.16
   *   fixed-point numbers, `slot->advance.x` and `slot->advance.y` (which
   *   are in 26.6 fixed-point format) must be in the range ]-32768;32768[.
   */
  FT_EXPORT( FT_Error )
  FT_Get_Glyph( FT_GlyphSlot  slot,
				FT_Glyph     *aglyph );

  /**************************************************************************
   *
   * @function:
   *   FT_Glyph_Copy
   *
   * @description:
   *   A function used to copy a glyph image.  Note that the created
   *   @FT_Glyph object must be released with @FT_Done_Glyph.
   *
   * @input:
   *   source ::
   *     A handle to the source glyph object.
   *
   * @output:
   *   target ::
   *     A handle to the target glyph object.  `NULL` in case of error.
   *
   * @return:
   *   FreeType error code.  0~means success.
   */
  FT_EXPORT( FT_Error )
  FT_Glyph_Copy( FT_Glyph   source,
				 FT_Glyph  *target );

  /**************************************************************************
   *
   * @function:
   *   FT_Glyph_Transform
   *
   * @description:
   *   Transform a glyph image if its format is scalable.
   *
   * @inout:
   *   glyph ::
   *     A handle to the target glyph object.
   *
   * @input:
   *   matrix ::
   *     A pointer to a 2x2 matrix to apply.
   *
   *   delta ::
   *     A pointer to a 2d vector to apply.  Coordinates are expressed in
   *     1/64 of a pixel.
   *
   * @return:
   *   FreeType error code (if not 0, the glyph format is not scalable).
   *
   * @note:
   *   The 2x2 transformation matrix is also applied to the glyph's advance
   *   vector.
   */
  FT_EXPORT( FT_Error )
  FT_Glyph_Transform( FT_Glyph          glyph,
					  const FT_Matrix*  matrix,
					  const FT_Vector*  delta );

  /**************************************************************************
   *
   * @enum:
   *   FT_Glyph_BBox_Mode
   *
   * @description:
   *   The mode how the values of @FT_Glyph_Get_CBox are returned.
   *
   * @values:
   *   FT_GLYPH_BBOX_UNSCALED ::
   *     Return unscaled font units.
   *
   *   FT_GLYPH_BBOX_SUBPIXELS ::
   *     Return unfitted 26.6 coordinates.
   *
   *   FT_GLYPH_BBOX_GRIDFIT ::
   *     Return grid-fitted 26.6 coordinates.
   *
   *   FT_GLYPH_BBOX_TRUNCATE ::
   *     Return coordinates in integer pixels.
   *
   *   FT_GLYPH_BBOX_PIXELS ::
   *     Return grid-fitted pixel coordinates.
   */
  typedef enum  FT_Glyph_BBox_Mode_
  {
	FT_GLYPH_BBOX_UNSCALED  = 0,
	FT_GLYPH_BBOX_SUBPIXELS = 0,
	FT_GLYPH_BBOX_GRIDFIT   = 1,
	FT_GLYPH_BBOX_TRUNCATE  = 2,
	FT_GLYPH_BBOX_PIXELS    = 3

  } FT_Glyph_BBox_Mode;

  /* these constants are deprecated; use the corresponding */
  /* `FT_Glyph_BBox_Mode` values instead                   */
#define ft_glyph_bbox_unscaled   FT_GLYPH_BBOX_UNSCALED
#define ft_glyph_bbox_subpixels  FT_GLYPH_BBOX_SUBPIXELS
#define ft_glyph_bbox_gridfit    FT_GLYPH_BBOX_GRIDFIT
#define ft_glyph_bbox_truncate   FT_GLYPH_BBOX_TRUNCATE
#define ft_glyph_bbox_pixels     FT_GLYPH_BBOX_PIXELS

  /**************************************************************************
   *
   * @function:
   *   FT_Glyph_Get_CBox
   *
   * @description:
   *   Return a glyph's 'control box'.  The control box encloses all the
   *   outline's points, including Bezier control points.  Though it
   *   coincides with the exact bounding box for most glyphs, it can be
   *   slightly larger in some situations (like when rotating an outline that
   *   contains Bezier outside arcs).
   *
   *   Computing the control box is very fast, while getting the bounding box
   *   can take much more time as it needs to walk over all segments and arcs
   *   in the outline.  To get the latter, you can use the 'ftbbox'
   *   component, which is dedicated to this single task.
   *
   * @input:
   *   glyph ::
   *     A handle to the source glyph object.
   *
   *   mode ::
   *     The mode that indicates how to interpret the returned bounding box
   *     values.
   *
   * @output:
   *   acbox ::
   *     The glyph coordinate bounding box.  Coordinates are expressed in
   *     1/64 of pixels if it is grid-fitted.
   *
   * @note:
   *   Coordinates are relative to the glyph origin, using the y~upwards
   *   convention.
   *
   *   If the glyph has been loaded with @FT_LOAD_NO_SCALE, `bbox_mode` must
   *   be set to @FT_GLYPH_BBOX_UNSCALED to get unscaled font units in 26.6
   *   pixel format.  The value @FT_GLYPH_BBOX_SUBPIXELS is another name for
   *   this constant.
   *
   *   If the font is tricky and the glyph has been loaded with
   *   @FT_LOAD_NO_SCALE, the resulting CBox is meaningless.  To get
   *   reasonable values for the CBox it is necessary to load the glyph at a
   *   large ppem value (so that the hinting instructions can properly shift
   *   and scale the subglyphs), then extracting the CBox, which can be
   *   eventually converted back to font units.
   *
   *   Note that the maximum coordinates are exclusive, which means that one
   *   can compute the width and height of the glyph image (be it in integer
   *   or 26.6 pixels) as:
   *
   *   ```
   *     width  = bbox.xMax - bbox.xMin;
   *     height = bbox.yMax - bbox.yMin;
   *   ```
   *
   *   Note also that for 26.6 coordinates, if `bbox_mode` is set to
   *   @FT_GLYPH_BBOX_GRIDFIT, the coordinates will also be grid-fitted,
   *   which corresponds to:
   *
   *   ```
   *     bbox.xMin = FLOOR(bbox.xMin);
   *     bbox.yMin = FLOOR(bbox.yMin);
   *     bbox.xMax = CEILING(bbox.xMax);
   *     bbox.yMax = CEILING(bbox.yMax);
   *   ```
   *
   *   To get the bbox in pixel coordinates, set `bbox_mode` to
   *   @FT_GLYPH_BBOX_TRUNCATE.
   *
   *   To get the bbox in grid-fitted pixel coordinates, set `bbox_mode` to
   *   @FT_GLYPH_BBOX_PIXELS.
   */
  FT_EXPORT( void )
  FT_Glyph_Get_CBox( FT_Glyph  glyph,
					 FT_UInt   bbox_mode,
					 FT_BBox  *acbox );

  /**************************************************************************
   *
   * @function:
   *   FT_Glyph_To_Bitmap
   *
   * @description:
   *   Convert a given glyph object to a bitmap glyph object.
   *
   * @inout:
   *   the_glyph ::
   *     A pointer to a handle to the target glyph.
   *
   * @input:
   *   render_mode ::
   *     An enumeration that describes how the data is rendered.
   *
   *   origin ::
   *     A pointer to a vector used to translate the glyph image before
   *     rendering.  Can be~0 (if no translation).  The origin is expressed
   *     in 26.6 pixels.
   *
   *   destroy ::
   *     A boolean that indicates that the original glyph image should be
   *     destroyed by this function.  It is never destroyed in case of error.
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @note:
   *   This function does nothing if the glyph format isn't scalable.
   *
   *   The glyph image is translated with the `origin` vector before
   *   rendering.
   *
   *   The first parameter is a pointer to an @FT_Glyph handle that will be
   *   _replaced_ by this function (with newly allocated data).  Typically,
   *   you would do something like the following (omitting error handling).
   *
   *   ```
   *     FT_Glyph        glyph;
   *     FT_BitmapGlyph  glyph_bitmap;
   *
   *
   *     // load glyph
   *     error = FT_Load_Char( face, glyph_index, FT_LOAD_DEFAULT );
   *
   *     // extract glyph image
   *     error = FT_Get_Glyph( face->glyph, &glyph );
   *
   *     // convert to a bitmap (default render mode + destroying old)
   *     if ( glyph->format != FT_GLYPH_FORMAT_BITMAP )
   *     {
   *       error = FT_Glyph_To_Bitmap( &glyph, FT_RENDER_MODE_NORMAL,
   *                                   0, 1 );
   *       if ( error ) // `glyph' unchanged
   *         ...
   *     }
   *
   *     // access bitmap content by typecasting
   *     glyph_bitmap = (FT_BitmapGlyph)glyph;
   *
   *     // do funny stuff with it, like blitting/drawing
   *     ...
   *
   *     // discard glyph image (bitmap or not)
   *     FT_Done_Glyph( glyph );
   *   ```
   *
   *   Here is another example, again without error handling.
   *
   *   ```
   *     FT_Glyph  glyphs[MAX_GLYPHS]
   *
   *
   *     ...
   *
   *     for ( idx = 0; i < MAX_GLYPHS; i++ )
   *       error = FT_Load_Glyph( face, idx, FT_LOAD_DEFAULT ) ||
   *               FT_Get_Glyph ( face->glyph, &glyphs[idx] );
   *
   *     ...
   *
   *     for ( idx = 0; i < MAX_GLYPHS; i++ )
   *     {
   *       FT_Glyph  bitmap = glyphs[idx];
   *
   *
   *       ...
   *
   *       // after this call, `bitmap' no longer points into
   *       // the `glyphs' array (and the old value isn't destroyed)
   *       FT_Glyph_To_Bitmap( &bitmap, FT_RENDER_MODE_MONO, 0, 0 );
   *
   *       ...
   *
   *       FT_Done_Glyph( bitmap );
   *     }
   *
   *     ...
   *
   *     for ( idx = 0; i < MAX_GLYPHS; i++ )
   *       FT_Done_Glyph( glyphs[idx] );
   *   ```
   */
  FT_EXPORT( FT_Error )
  FT_Glyph_To_Bitmap( FT_Glyph*         the_glyph,
					  FT_Render_Mode    render_mode,
					  const FT_Vector*  origin,
					  FT_Bool           destroy );

  /**************************************************************************
   *
   * @function:
   *   FT_Done_Glyph
   *
   * @description:
   *   Destroy a given glyph.
   *
   * @input:
   *   glyph ::
   *     A handle to the target glyph object.  Can be `NULL`.
   */
  FT_EXPORT( void )
  FT_Done_Glyph( FT_Glyph  glyph );

  /* */

  /* other helpful functions */

  /**************************************************************************
   *
   * @section:
   *   computations
   *
   */

  /**************************************************************************
   *
   * @function:
   *   FT_Matrix_Multiply
   *
   * @description:
   *   Perform the matrix operation `b = a*b`.
   *
   * @input:
   *   a ::
   *     A pointer to matrix `a`.
   *
   * @inout:
   *   b ::
   *     A pointer to matrix `b`.
   *
   * @note:
   *   The result is undefined if either `a` or `b` is zero.
   *
   *   Since the function uses wrap-around arithmetic, results become
   *   meaningless if the arguments are very large.
   */
  FT_EXPORT( void )
  FT_Matrix_Multiply( const FT_Matrix*  a,
					  FT_Matrix*        b );

  /**************************************************************************
   *
   * @function:
   *   FT_Matrix_Invert
   *
   * @description:
   *   Invert a 2x2 matrix.  Return an error if it can't be inverted.
   *
   * @inout:
   *   matrix ::
   *     A pointer to the target matrix.  Remains untouched in case of error.
   *
   * @return:
   *   FreeType error code.  0~means success.
   */
  FT_EXPORT( FT_Error )
  FT_Matrix_Invert( FT_Matrix*  matrix );

  /* */

FT_END_HEADER

#endif /* FTGLYPH_H_ */

/* END */

/* Local Variables: */
/* coding: utf-8    */
/* End:             */

/*** End of inlined file: ftglyph.h ***/


/*** Start of inlined file: ftoutln.h ***/
#ifndef FTOUTLN_H_
#define FTOUTLN_H_

#ifdef FREETYPE_H
#error "freetype.h of FreeType 1 has been loaded!"
#error "Please fix the directory search order for header files"
#error "so that freetype.h of FreeType 2 is found first."
#endif

FT_BEGIN_HEADER

  /**************************************************************************
   *
   * @section:
   *   outline_processing
   *
   * @title:
   *   Outline Processing
   *
   * @abstract:
   *   Functions to create, transform, and render vectorial glyph images.
   *
   * @description:
   *   This section contains routines used to create and destroy scalable
   *   glyph images known as 'outlines'.  These can also be measured,
   *   transformed, and converted into bitmaps and pixmaps.
   *
   * @order:
   *   FT_Outline
   *   FT_Outline_New
   *   FT_Outline_Done
   *   FT_Outline_Copy
   *   FT_Outline_Translate
   *   FT_Outline_Transform
   *   FT_Outline_Embolden
   *   FT_Outline_EmboldenXY
   *   FT_Outline_Reverse
   *   FT_Outline_Check
   *
   *   FT_Outline_Get_CBox
   *   FT_Outline_Get_BBox
   *
   *   FT_Outline_Get_Bitmap
   *   FT_Outline_Render
   *   FT_Outline_Decompose
   *   FT_Outline_Funcs
   *   FT_Outline_MoveToFunc
   *   FT_Outline_LineToFunc
   *   FT_Outline_ConicToFunc
   *   FT_Outline_CubicToFunc
   *
   *   FT_Orientation
   *   FT_Outline_Get_Orientation
   *
   *   FT_OUTLINE_XXX
   *
   */

  /**************************************************************************
   *
   * @function:
   *   FT_Outline_Decompose
   *
   * @description:
   *   Walk over an outline's structure to decompose it into individual
   *   segments and Bezier arcs.  This function also emits 'move to'
   *   operations to indicate the start of new contours in the outline.
   *
   * @input:
   *   outline ::
   *     A pointer to the source target.
   *
   *   func_interface ::
   *     A table of 'emitters', i.e., function pointers called during
   *     decomposition to indicate path operations.
   *
   * @inout:
   *   user ::
   *     A typeless pointer that is passed to each emitter during the
   *     decomposition.  It can be used to store the state during the
   *     decomposition.
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @note:
   *   Degenerate contours, segments, and Bezier arcs may be reported.  In
   *   most cases, it is best to filter these out before using the outline
   *   for stroking or other path modification purposes (which may cause
   *   degenerate segments to become non-degenerate and visible, like when
   *   stroke caps are used or the path is otherwise outset).  Some glyph
   *   outlines may contain deliberate degenerate single points for mark
   *   attachement.
   *
   *   Similarly, the function returns success for an empty outline also
   *   (doing nothing, that is, not calling any emitter); if necessary, you
   *   should filter this out, too.
   */
  FT_EXPORT( FT_Error )
  FT_Outline_Decompose( FT_Outline*              outline,
						const FT_Outline_Funcs*  func_interface,
						void*                    user );

  /**************************************************************************
   *
   * @function:
   *   FT_Outline_New
   *
   * @description:
   *   Create a new outline of a given size.
   *
   * @input:
   *   library ::
   *     A handle to the library object from where the outline is allocated.
   *     Note however that the new outline will **not** necessarily be
   *     **freed**, when destroying the library, by @FT_Done_FreeType.
   *
   *   numPoints ::
   *     The maximum number of points within the outline.  Must be smaller
   *     than or equal to 0xFFFF (65535).
   *
   *   numContours ::
   *     The maximum number of contours within the outline.  This value must
   *     be in the range 0 to `numPoints`.
   *
   * @output:
   *   anoutline ::
   *     A handle to the new outline.
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @note:
   *   The reason why this function takes a `library` parameter is simply to
   *   use the library's memory allocator.
   */
  FT_EXPORT( FT_Error )
  FT_Outline_New( FT_Library   library,
				  FT_UInt      numPoints,
				  FT_Int       numContours,
				  FT_Outline  *anoutline );

  /**************************************************************************
   *
   * @function:
   *   FT_Outline_Done
   *
   * @description:
   *   Destroy an outline created with @FT_Outline_New.
   *
   * @input:
   *   library ::
   *     A handle of the library object used to allocate the outline.
   *
   *   outline ::
   *     A pointer to the outline object to be discarded.
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @note:
   *   If the outline's 'owner' field is not set, only the outline descriptor
   *   will be released.
   */
  FT_EXPORT( FT_Error )
  FT_Outline_Done( FT_Library   library,
				   FT_Outline*  outline );

  /**************************************************************************
   *
   * @function:
   *   FT_Outline_Check
   *
   * @description:
   *   Check the contents of an outline descriptor.
   *
   * @input:
   *   outline ::
   *     A handle to a source outline.
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @note:
   *   An empty outline, or an outline with a single point only is also
   *   valid.
   */
  FT_EXPORT( FT_Error )
  FT_Outline_Check( FT_Outline*  outline );

  /**************************************************************************
   *
   * @function:
   *   FT_Outline_Get_CBox
   *
   * @description:
   *   Return an outline's 'control box'.  The control box encloses all the
   *   outline's points, including Bezier control points.  Though it
   *   coincides with the exact bounding box for most glyphs, it can be
   *   slightly larger in some situations (like when rotating an outline that
   *   contains Bezier outside arcs).
   *
   *   Computing the control box is very fast, while getting the bounding box
   *   can take much more time as it needs to walk over all segments and arcs
   *   in the outline.  To get the latter, you can use the 'ftbbox'
   *   component, which is dedicated to this single task.
   *
   * @input:
   *   outline ::
   *     A pointer to the source outline descriptor.
   *
   * @output:
   *   acbox ::
   *     The outline's control box.
   *
   * @note:
   *   See @FT_Glyph_Get_CBox for a discussion of tricky fonts.
   */
  FT_EXPORT( void )
  FT_Outline_Get_CBox( const FT_Outline*  outline,
					   FT_BBox           *acbox );

  /**************************************************************************
   *
   * @function:
   *   FT_Outline_Translate
   *
   * @description:
   *   Apply a simple translation to the points of an outline.
   *
   * @inout:
   *   outline ::
   *     A pointer to the target outline descriptor.
   *
   * @input:
   *   xOffset ::
   *     The horizontal offset.
   *
   *   yOffset ::
   *     The vertical offset.
   */
  FT_EXPORT( void )
  FT_Outline_Translate( const FT_Outline*  outline,
						FT_Pos             xOffset,
						FT_Pos             yOffset );

  /**************************************************************************
   *
   * @function:
   *   FT_Outline_Copy
   *
   * @description:
   *   Copy an outline into another one.  Both objects must have the same
   *   sizes (number of points & number of contours) when this function is
   *   called.
   *
   * @input:
   *   source ::
   *     A handle to the source outline.
   *
   * @output:
   *   target ::
   *     A handle to the target outline.
   *
   * @return:
   *   FreeType error code.  0~means success.
   */
  FT_EXPORT( FT_Error )
  FT_Outline_Copy( const FT_Outline*  source,
				   FT_Outline        *target );

  /**************************************************************************
   *
   * @function:
   *   FT_Outline_Transform
   *
   * @description:
   *   Apply a simple 2x2 matrix to all of an outline's points.  Useful for
   *   applying rotations, slanting, flipping, etc.
   *
   * @inout:
   *   outline ::
   *     A pointer to the target outline descriptor.
   *
   * @input:
   *   matrix ::
   *     A pointer to the transformation matrix.
   *
   * @note:
   *   You can use @FT_Outline_Translate if you need to translate the
   *   outline's points.
   */
  FT_EXPORT( void )
  FT_Outline_Transform( const FT_Outline*  outline,
						const FT_Matrix*   matrix );

  /**************************************************************************
   *
   * @function:
   *   FT_Outline_Embolden
   *
   * @description:
   *   Embolden an outline.  The new outline will be at most 4~times
   *   `strength` pixels wider and higher.  You may think of the left and
   *   bottom borders as unchanged.
   *
   *   Negative `strength` values to reduce the outline thickness are
   *   possible also.
   *
   * @inout:
   *   outline ::
   *     A handle to the target outline.
   *
   * @input:
   *   strength ::
   *     How strong the glyph is emboldened.  Expressed in 26.6 pixel format.
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @note:
   *   The used algorithm to increase or decrease the thickness of the glyph
   *   doesn't change the number of points; this means that certain
   *   situations like acute angles or intersections are sometimes handled
   *   incorrectly.
   *
   *   If you need 'better' metrics values you should call
   *   @FT_Outline_Get_CBox or @FT_Outline_Get_BBox.
   *
   *   To get meaningful results, font scaling values must be set with
   *   functions like @FT_Set_Char_Size before calling FT_Render_Glyph.
   *
   * @example:
   *   ```
   *     FT_Load_Glyph( face, index, FT_LOAD_DEFAULT );
   *
   *     if ( face->glyph->format == FT_GLYPH_FORMAT_OUTLINE )
   *       FT_Outline_Embolden( &face->glyph->outline, strength );
   *   ```
   *
   */
  FT_EXPORT( FT_Error )
  FT_Outline_Embolden( FT_Outline*  outline,
					   FT_Pos       strength );

  /**************************************************************************
   *
   * @function:
   *   FT_Outline_EmboldenXY
   *
   * @description:
   *   Embolden an outline.  The new outline will be `xstrength` pixels wider
   *   and `ystrength` pixels higher.  Otherwise, it is similar to
   *   @FT_Outline_Embolden, which uses the same strength in both directions.
   *
   * @since:
   *   2.4.10
   */
  FT_EXPORT( FT_Error )
  FT_Outline_EmboldenXY( FT_Outline*  outline,
						 FT_Pos       xstrength,
						 FT_Pos       ystrength );

  /**************************************************************************
   *
   * @function:
   *   FT_Outline_Reverse
   *
   * @description:
   *   Reverse the drawing direction of an outline.  This is used to ensure
   *   consistent fill conventions for mirrored glyphs.
   *
   * @inout:
   *   outline ::
   *     A pointer to the target outline descriptor.
   *
   * @note:
   *   This function toggles the bit flag @FT_OUTLINE_REVERSE_FILL in the
   *   outline's `flags` field.
   *
   *   It shouldn't be used by a normal client application, unless it knows
   *   what it is doing.
   */
  FT_EXPORT( void )
  FT_Outline_Reverse( FT_Outline*  outline );

  /**************************************************************************
   *
   * @function:
   *   FT_Outline_Get_Bitmap
   *
   * @description:
   *   Render an outline within a bitmap.  The outline's image is simply
   *   OR-ed to the target bitmap.
   *
   * @input:
   *   library ::
   *     A handle to a FreeType library object.
   *
   *   outline ::
   *     A pointer to the source outline descriptor.
   *
   * @inout:
   *   abitmap ::
   *     A pointer to the target bitmap descriptor.
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @note:
   *   This function does **not create** the bitmap, it only renders an
   *   outline image within the one you pass to it!  Consequently, the
   *   various fields in `abitmap` should be set accordingly.
   *
   *   It will use the raster corresponding to the default glyph format.
   *
   *   The value of the `num_grays` field in `abitmap` is ignored.  If you
   *   select the gray-level rasterizer, and you want less than 256 gray
   *   levels, you have to use @FT_Outline_Render directly.
   */
  FT_EXPORT( FT_Error )
  FT_Outline_Get_Bitmap( FT_Library        library,
						 FT_Outline*       outline,
						 const FT_Bitmap  *abitmap );

  /**************************************************************************
   *
   * @function:
   *   FT_Outline_Render
   *
   * @description:
   *   Render an outline within a bitmap using the current scan-convert.
   *
   * @input:
   *   library ::
   *     A handle to a FreeType library object.
   *
   *   outline ::
   *     A pointer to the source outline descriptor.
   *
   * @inout:
   *   params ::
   *     A pointer to an @FT_Raster_Params structure used to describe the
   *     rendering operation.
   *
   * @return:
   *   FreeType error code.  0~means success.
   *
   * @note:
   *   This advanced function uses @FT_Raster_Params as an argument.
   *   The field `params.source` will be set to `outline` before the scan
   *   converter is called, which means that the value you give to it is
   *   actually ignored.  Either `params.target` must point to preallocated
   *   bitmap, or @FT_RASTER_FLAG_DIRECT must be set in `params.flags`
   *   allowing FreeType rasterizer to be used for direct composition,
   *   translucency, etc.  See @FT_Raster_Params for more details.
   */
  FT_EXPORT( FT_Error )
  FT_Outline_Render( FT_Library         library,
					 FT_Outline*        outline,
					 FT_Raster_Params*  params );

  /**************************************************************************
   *
   * @enum:
   *   FT_Orientation
   *
   * @description:
   *   A list of values used to describe an outline's contour orientation.
   *
   *   The TrueType and PostScript specifications use different conventions
   *   to determine whether outline contours should be filled or unfilled.
   *
   * @values:
   *   FT_ORIENTATION_TRUETYPE ::
   *     According to the TrueType specification, clockwise contours must be
   *     filled, and counter-clockwise ones must be unfilled.
   *
   *   FT_ORIENTATION_POSTSCRIPT ::
   *     According to the PostScript specification, counter-clockwise
   *     contours must be filled, and clockwise ones must be unfilled.
   *
   *   FT_ORIENTATION_FILL_RIGHT ::
   *     This is identical to @FT_ORIENTATION_TRUETYPE, but is used to
   *     remember that in TrueType, everything that is to the right of the
   *     drawing direction of a contour must be filled.
   *
   *   FT_ORIENTATION_FILL_LEFT ::
   *     This is identical to @FT_ORIENTATION_POSTSCRIPT, but is used to
   *     remember that in PostScript, everything that is to the left of the
   *     drawing direction of a contour must be filled.
   *
   *   FT_ORIENTATION_NONE ::
   *     The orientation cannot be determined.  That is, different parts of
   *     the glyph have different orientation.
   *
   */
  typedef enum  FT_Orientation_
  {
	FT_ORIENTATION_TRUETYPE   = 0,
	FT_ORIENTATION_POSTSCRIPT = 1,
	FT_ORIENTATION_FILL_RIGHT = FT_ORIENTATION_TRUETYPE,
	FT_ORIENTATION_FILL_LEFT  = FT_ORIENTATION_POSTSCRIPT,
	FT_ORIENTATION_NONE

  } FT_Orientation;

  /**************************************************************************
   *
   * @function:
   *   FT_Outline_Get_Orientation
   *
   * @description:
   *   This function analyzes a glyph outline and tries to compute its fill
   *   orientation (see @FT_Orientation).  This is done by integrating the
   *   total area covered by the outline. The positive integral corresponds
   *   to the clockwise orientation and @FT_ORIENTATION_POSTSCRIPT is
   *   returned. The negative integral corresponds to the counter-clockwise
   *   orientation and @FT_ORIENTATION_TRUETYPE is returned.
   *
   *   Note that this will return @FT_ORIENTATION_TRUETYPE for empty
   *   outlines.
   *
   * @input:
   *   outline ::
   *     A handle to the source outline.
   *
   * @return:
   *   The orientation.
   *
   */
  FT_EXPORT( FT_Orientation )
  FT_Outline_Get_Orientation( FT_Outline*  outline );

  /* */

FT_END_HEADER

#endif /* FTOUTLN_H_ */

/* END */

/* Local Variables: */
/* coding: utf-8    */
/* End:             */

/*** End of inlined file: ftoutln.h ***/

#ifdef _MSC_VER
#pragma pop_macro("_CRT_SECURE_NO_WARNINGS")
#endif

