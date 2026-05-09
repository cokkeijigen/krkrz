
include(CheckTypeSize)
include(CheckIncludeFile)

check_include_file("unistd.h" Z_HAVE_UNISTD_H)
check_type_size   ("off64_t"  HAVE_OFF64_T)

configure_file(
	"${SOURCE_ROOT}/external/zlib/zconf.h.cmakein" 
	"${CMAKE_BINARY_DIR}/zlib/zconf.h" @ONLY
)

set(ZLIB_C_SOURCES
    "${SOURCE_ROOT}/external/zlib/adler32.c"
    "${SOURCE_ROOT}/external/zlib/compress.c"
    "${SOURCE_ROOT}/external/zlib/crc32.c"
    "${SOURCE_ROOT}/external/zlib/deflate.c"
    "${SOURCE_ROOT}/external/zlib/gzclose.c"
    "${SOURCE_ROOT}/external/zlib/gzlib.c"
    "${SOURCE_ROOT}/external/zlib/gzread.c"
    "${SOURCE_ROOT}/external/zlib/gzwrite.c"
    "${SOURCE_ROOT}/external/zlib/inflate.c"
    "${SOURCE_ROOT}/external/zlib/infback.c"
    "${SOURCE_ROOT}/external/zlib/inftrees.c"
    "${SOURCE_ROOT}/external/zlib/inffast.c"
    "${SOURCE_ROOT}/external/zlib/trees.c"
    "${SOURCE_ROOT}/external/zlib/uncompr.c"
    "${SOURCE_ROOT}/external/zlib/zutil.c"
)

if(MSVC)
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(ZLIB_ASM_SOURCES
			"${SOURCE_ROOT}/external/zlib/contrib/masmx86/inffas32.asm"
			"${SOURCE_ROOT}/external/zlib/contrib/masmx86/match686.asm"
		)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(ZLIB_ASM_SOURCES
			"${SOURCE_ROOT}/external/zlib/contrib/masmx64/gvmat64.asm"
			"${SOURCE_ROOT}/external/zlib/contrib/masmx64/inffasx64.asm"
		)
    endif()
endif()

if(ZLIB_ASM_SOURCES)
    set_source_files_properties(${ZLIB_ASM_SOURCES} PROPERTIES
        LANGUAGE ASM_MASM
    )
endif()

add_library(zlib OBJECT ${ZLIB_C_SOURCES} ${ZLIB_ASM_SOURCES})

target_include_directories(zlib PUBLIC 
    "${SOURCE_ROOT}/external/zlib" 
    "${CMAKE_BINARY_DIR}/zlib"
)

if(HAVE_OFF64_T)
    target_compile_definitions(zlib PUBLIC 
        "_LARGEFILE64_SOURCE=1"
    )
endif()

if(ZLIB_ASM_SOURCES)
    target_compile_definitions(zlib PUBLIC 
        "ASMV" "ASMINF"
    )
endif()

target_compile_definitions(zlib PRIVATE 
    "_CRT_SECURE_NO_DEPRECATE"
    "_CRT_SECURE_NO_WARNINGS"
)