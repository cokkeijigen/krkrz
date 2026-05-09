
set(TURBOJPEG_C_SOURCES
	"${SOURCE_ROOT}/external/libjpeg-turbo/jcapimin.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jcapistd.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jccoefct.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jccolor.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jcdctmgr.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jchuff.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jcinit.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jcmainct.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jcmarker.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jcmaster.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jcomapi.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jcparam.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jcphuff.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jcprepct.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jcsample.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jctrans.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jdapimin.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jdapistd.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jdatadst.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jdatasrc.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jdcoefct.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jdcolor.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jddctmgr.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jdhuff.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jdinput.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jdmainct.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jdmarker.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jdmaster.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jdmerge.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jdphuff.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jdpostct.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jdsample.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jdtrans.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jerror.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jfdctflt.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jfdctfst.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jfdctint.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jidctflt.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jidctfst.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jidctint.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jidctred.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jquant1.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jquant2.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jutils.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jmemmgr.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jmemnobs.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jaricom.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jcarith.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jdarith.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/turbojpeg.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/transupp.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jdatadst-tj.c"
	"${SOURCE_ROOT}/external/libjpeg-turbo/jdatasrc-tj.c"
)

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
	set(TURBOJPEG_SIMD_SOURCES 
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jccolor-mmx.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jccolor-sse2.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jcgray-mmx.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jcgray-sse2.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jchuff-sse2.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jcsample-mmx.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jcsample-sse2.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jdcolor-mmx.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jdcolor-sse2.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jdmerge-mmx.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jdmerge-sse2.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jdsample-mmx.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jdsample-sse2.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jfdctflt-3dn.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jfdctflt-sse.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jfdctfst-mmx.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jfdctfst-sse2.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jfdctint-mmx.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jfdctint-sse2.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jidctflt-3dn.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jidctflt-sse.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jidctflt-sse2.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jidctfst-mmx.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jidctfst-sse2.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jidctint-mmx.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jidctint-sse2.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jidctred-sse2.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jidctred-mmx.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jquant-3dn.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jquant-mmx.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jquant-sse.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jquantf-sse2.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jquanti-sse2.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jsimdcpu.asm"
	)
	list(APPEND TURBOJPEG_C_SOURCES 
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jsimd_i386.c"
	)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(TURBOJPEG_SIMD_SOURCES
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jquanti-sse2-64.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jccolor-sse2-64.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jcgray-sse2-64.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jchuff-sse2-64.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jcsample-sse2-64.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jdcolor-sse2-64.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jdmerge-sse2-64.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jdsample-sse2-64.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jfdctflt-sse-64.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jfdctfst-sse2-64.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jfdctint-sse2-64.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jidctflt-sse2-64.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jidctfst-sse2-64.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jidctint-sse2-64.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jidctred-sse2-64.asm"
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jquantf-sse2-64.asm"
	)
	list(APPEND TURBOJPEG_C_SOURCES 
		"${SOURCE_ROOT}/external/libjpeg-turbo/simd/jsimd_x86_64.c"
	)
endif()

set_source_files_properties(${TURBOJPEG_SIMD_SOURCES} 
	PROPERTIES LANGUAGE ASM_NASM
)

set(TURBOJPEG_INCLUDE 
	"${SOURCE_ROOT}/external/libjpeg-turbo/"
	"${SOURCE_ROOT}/external/libjpeg-turbo/simd"
	"${SOURCE_ROOT}/external/libjpeg-turbo/win"
	"${SOURCE_ROOT}/external/libjpeg-turbo/vcproj"
)

add_library(turbojpeg OBJECT 
	${TURBOJPEG_C_SOURCES} 
	${TURBOJPEG_SIMD_SOURCES}
)

target_include_directories(turbojpeg 
	PUBLIC "${TURBOJPEG_INCLUDE}"
)

target_compile_definitions(turbojpeg 
	PRIVATE "_CRT_SECURE_NO_WARNINGS"
)