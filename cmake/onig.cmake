
include(CheckFunctionExists)
include(CheckIncludeFiles)
include(CheckTypeSize)

set(HAVE_PROTOTYPES 1)
check_function_exists("alloca"    HAVE_ALLOCA)
check_include_files("alloca.h"    HAVE_ALLOCA_H)
check_include_files("stdarg.h"    HAVE_STDARG_PROTOTYPES)
check_include_files("stdint.h"    HAVE_STDINT_H)
check_include_files("stdlib.h"    HAVE_STDLIB_H)
check_include_files("strings.h"   HAVE_STRINGS_H)
check_include_files("string.h"    HAVE_STRING_H)
check_include_files("sys/times.h" HAVE_SYS_TIMES_H)
check_include_files("sys/time.h"  HAVE_SYS_TIME_H)
check_include_files("sys/types.h" HAVE_SYS_TYPES_H)
check_include_files("unistd.h"    HAVE_UNISTD_H)
check_type_size("int"   SIZEOF_INT)
check_type_size("long"  SIZEOF_LONG)
check_type_size("short" SIZEOF_SHORT)
check_include_files("stdlib.h;stdarg.h;string.h;float.h" STDC_HEADERS)

configure_file(
	"${SOURCE_ROOT}/external/onig/src/config.h.cmake.in"
	"${CMAKE_BINARY_DIR}/onig/config.h"
)

set(ONIG_SOURCES
	"${SOURCE_ROOT}/external/onig/src/regerror.c"
	"${SOURCE_ROOT}/external/onig/src/regparse.c"
	"${SOURCE_ROOT}/external/onig/src/regext.c"
	"${SOURCE_ROOT}/external/onig/src/regcomp.c"
	"${SOURCE_ROOT}/external/onig/src/regexec.c"
	"${SOURCE_ROOT}/external/onig/src/reggnu.c"
	"${SOURCE_ROOT}/external/onig/src/regenc.c"
	"${SOURCE_ROOT}/external/onig/src/regsyntax.c"
	"${SOURCE_ROOT}/external/onig/src/regtrav.c"
	"${SOURCE_ROOT}/external/onig/src/regversion.c"
	"${SOURCE_ROOT}/external/onig/src/st.c"
	"${SOURCE_ROOT}/external/onig/src/regposix.c"
	"${SOURCE_ROOT}/external/onig/src/regposerr.c"
	"${SOURCE_ROOT}/external/onig/src/onig_init.c"
	"${SOURCE_ROOT}/external/onig/src/unicode.c"
	"${SOURCE_ROOT}/external/onig/src/ascii.c"
	"${SOURCE_ROOT}/external/onig/src/utf8.c"
	"${SOURCE_ROOT}/external/onig/src/utf16_be.c"
	"${SOURCE_ROOT}/external/onig/src/utf16_le.c"
	"${SOURCE_ROOT}/external/onig/src/utf32_be.c"
	"${SOURCE_ROOT}/external/onig/src/utf32_le.c"
	"${SOURCE_ROOT}/external/onig/src/euc_jp.c"
	"${SOURCE_ROOT}/external/onig/src/sjis.c"
	"${SOURCE_ROOT}/external/onig/src/iso8859_1.c"
	"${SOURCE_ROOT}/external/onig/src/iso8859_2.c"
	"${SOURCE_ROOT}/external/onig/src/iso8859_3.c"
	"${SOURCE_ROOT}/external/onig/src/iso8859_4.c"
	"${SOURCE_ROOT}/external/onig/src/iso8859_5.c"
	"${SOURCE_ROOT}/external/onig/src/iso8859_6.c"
	"${SOURCE_ROOT}/external/onig/src/iso8859_7.c"
	"${SOURCE_ROOT}/external/onig/src/iso8859_8.c"
	"${SOURCE_ROOT}/external/onig/src/iso8859_9.c"
	"${SOURCE_ROOT}/external/onig/src/iso8859_10.c"
	"${SOURCE_ROOT}/external/onig/src/iso8859_11.c"
	"${SOURCE_ROOT}/external/onig/src/iso8859_13.c"
	"${SOURCE_ROOT}/external/onig/src/iso8859_14.c"
	"${SOURCE_ROOT}/external/onig/src/iso8859_15.c"
	"${SOURCE_ROOT}/external/onig/src/iso8859_16.c"
	"${SOURCE_ROOT}/external/onig/src/euc_tw.c"
	"${SOURCE_ROOT}/external/onig/src/euc_kr.c"
	"${SOURCE_ROOT}/external/onig/src/big5.c"
	"${SOURCE_ROOT}/external/onig/src/gb18030.c"
	"${SOURCE_ROOT}/external/onig/src/koi8_r.c"
	"${SOURCE_ROOT}/external/onig/src/cp1251.c"
	"${SOURCE_ROOT}/external/onig/src/euc_jp_prop.c"
	"${SOURCE_ROOT}/external/onig/src/sjis_prop.c"
	"${SOURCE_ROOT}/external/onig/src/unicode_unfold_key.c"
	"${SOURCE_ROOT}/external/onig/src/unicode_fold1_key.c"
	"${SOURCE_ROOT}/external/onig/src/unicode_fold2_key.c"
	"${SOURCE_ROOT}/external/onig/src/unicode_fold3_key.c"
)

add_library(onig OBJECT ${ONIG_SOURCES})

set_target_properties(onig PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
target_include_directories(onig PUBLIC 
	"${SOURCE_ROOT}/external/onig/src/"
	"${CMAKE_BINARY_DIR}/onig/"
)

target_compile_definitions(onig PRIVATE
    "_CRT_SECURE_NO_WARNINGS"
    "ONIG_EXTERN=extern"
)