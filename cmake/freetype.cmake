
set(FREETYPE_SOURCES
	"${SOURCE_ROOT}/external/freetype/builds/windows/ftdebug.c"
	"${SOURCE_ROOT}/external/freetype/src/autofit/autofit.c"
	"${SOURCE_ROOT}/external/freetype/src/bdf/bdf.c"
	"${SOURCE_ROOT}/external/freetype/src/cff/cff.c"
	"${SOURCE_ROOT}/external/freetype/src/base/ftbase.c"
	"${SOURCE_ROOT}/external/freetype/src/base/ftbitmap.c"
	"${SOURCE_ROOT}/external/freetype/src/cache/ftcache.c"
	"${SOURCE_ROOT}/external/freetype/src/base/ftfstype.c"
	"${SOURCE_ROOT}/external/freetype/src/base/ftgasp.c"
	"${SOURCE_ROOT}/external/freetype/src/base/ftglyph.c"
	"${SOURCE_ROOT}/external/freetype/src/gzip/ftgzip.c"
	"${SOURCE_ROOT}/external/freetype/src/base/ftinit.c"
	"${SOURCE_ROOT}/external/freetype/src/lzw/ftlzw.c"
	"${SOURCE_ROOT}/external/freetype/src/base/ftstroke.c"
	"${SOURCE_ROOT}/external/freetype/src/base/ftsystem.c"
	"${SOURCE_ROOT}/external/freetype/src/smooth/smooth.c"
	"${SOURCE_ROOT}/external/freetype/src/base/ftbbox.c"
	"${SOURCE_ROOT}/external/freetype/src/base/ftfntfmt.c"
	"${SOURCE_ROOT}/external/freetype/src/base/ftmm.c"
	"${SOURCE_ROOT}/external/freetype/src/base/ftpfr.c"
	"${SOURCE_ROOT}/external/freetype/src/base/ftsynth.c"
	"${SOURCE_ROOT}/external/freetype/src/base/fttype1.c"
	"${SOURCE_ROOT}/external/freetype/src/base/ftwinfnt.c"
	"${SOURCE_ROOT}/external/freetype/src/base/ftlcdfil.c"
	"${SOURCE_ROOT}/external/freetype/src/base/ftgxval.c"
	"${SOURCE_ROOT}/external/freetype/src/base/ftotval.c"
	"${SOURCE_ROOT}/external/freetype/src/base/ftpatent.c"
	"${SOURCE_ROOT}/external/freetype/src/pcf/pcf.c"
	"${SOURCE_ROOT}/external/freetype/src/pfr/pfr.c"
	"${SOURCE_ROOT}/external/freetype/src/psaux/psaux.c"
	"${SOURCE_ROOT}/external/freetype/src/pshinter/pshinter.c"
	"${SOURCE_ROOT}/external/freetype/src/psnames/psmodule.c"
	"${SOURCE_ROOT}/external/freetype/src/raster/raster.c"
	"${SOURCE_ROOT}/external/freetype/src/sfnt/sfnt.c"
	"${SOURCE_ROOT}/external/freetype/src/truetype/truetype.c"
	"${SOURCE_ROOT}/external/freetype/src/type1/type1.c"
	"${SOURCE_ROOT}/external/freetype/src/cid/type1cid.c"
	"${SOURCE_ROOT}/external/freetype/src/type42/type42.c"
	"${SOURCE_ROOT}/external/freetype/src/winfonts/winfnt.c"
)
set(FREETYPE_INCLUDE
	"${SOURCE_ROOT}/external/freetype/autofit"
	"${SOURCE_ROOT}/external/freetype/include"
	"${SOURCE_ROOT}/external/freetype/include/freetype/config"
)

add_library(freetype OBJECT ${FREETYPE_SOURCES})

target_include_directories(freetype 
	PUBLIC ${FREETYPE_INCLUDE}
)

target_compile_definitions(freetype 
	PUBLIC
		"FT2_BUILD_LIBRARY"
		"FT_DEBUG_LEVEL_TRACE"
		"FT_DEBUG_LEVEL_ERROR"
	PRIVATE 
		"_CRT_SECURE_NO_WARNINGS"
)