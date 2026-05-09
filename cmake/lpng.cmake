
add_library(libpng OBJECT 
	"${SOURCE_ROOT}/external/lpng/png.c"
	"${SOURCE_ROOT}/external/lpng/pngerror.c"
	"${SOURCE_ROOT}/external/lpng/pngget.c"
	"${SOURCE_ROOT}/external/lpng/pngmem.c"
	"${SOURCE_ROOT}/external/lpng/pngpread.c"
	"${SOURCE_ROOT}/external/lpng/pngread.c"
	"${SOURCE_ROOT}/external/lpng/pngrio.c"
	"${SOURCE_ROOT}/external/lpng/pngrtran.c"
	"${SOURCE_ROOT}/external/lpng/pngrutil.c"
	"${SOURCE_ROOT}/external/lpng/pngset.c"
	"${SOURCE_ROOT}/external/lpng/pngtrans.c"
	"${SOURCE_ROOT}/external/lpng/pngwio.c"
	"${SOURCE_ROOT}/external/lpng/pngwrite.c"
	"${SOURCE_ROOT}/external/lpng/pngwtran.c"
	"${SOURCE_ROOT}/external/lpng/pngwutil.c"
)

target_link_libraries(libpng PUBLIC zlib)

target_include_directories(libpng PUBLIC
	"${SOURCE_ROOT}/external/lpng"
)
target_compile_definitions(libpng PRIVATE 
	"_CRT_SECURE_NO_WARNINGS"
)