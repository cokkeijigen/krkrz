

set(BASECLASSES_SOURCES
	"${SOURCE_ROOT}/external/baseclasses/amextra.cpp"
	"${SOURCE_ROOT}/external/baseclasses/amfilter.cpp"
	"${SOURCE_ROOT}/external/baseclasses/amvideo.cpp"
	"${SOURCE_ROOT}/external/baseclasses/arithutil.cpp"
	"${SOURCE_ROOT}/external/baseclasses/combase.cpp"
	"${SOURCE_ROOT}/external/baseclasses/cprop.cpp"
	"${SOURCE_ROOT}/external/baseclasses/ctlutil.cpp"
	"${SOURCE_ROOT}/external/baseclasses/ddmm.cpp"
	"${SOURCE_ROOT}/external/baseclasses/dllentry.cpp"
	"${SOURCE_ROOT}/external/baseclasses/dllsetup.cpp"
	"${SOURCE_ROOT}/external/baseclasses/mtype.cpp"
	"${SOURCE_ROOT}/external/baseclasses/outputq.cpp"
	"${SOURCE_ROOT}/external/baseclasses/perflog.cpp"
	"${SOURCE_ROOT}/external/baseclasses/pstream.cpp"
	"${SOURCE_ROOT}/external/baseclasses/pullpin.cpp"
	"${SOURCE_ROOT}/external/baseclasses/refclock.cpp"
	"${SOURCE_ROOT}/external/baseclasses/renbase.cpp"
	"${SOURCE_ROOT}/external/baseclasses/schedule.cpp"
	"${SOURCE_ROOT}/external/baseclasses/seekpt.cpp"
	"${SOURCE_ROOT}/external/baseclasses/source.cpp"
	"${SOURCE_ROOT}/external/baseclasses/strmctl.cpp"
	"${SOURCE_ROOT}/external/baseclasses/sysclock.cpp"
	"${SOURCE_ROOT}/external/baseclasses/transfrm.cpp"
	"${SOURCE_ROOT}/external/baseclasses/transip.cpp"
	"${SOURCE_ROOT}/external/baseclasses/videoctl.cpp"
	"${SOURCE_ROOT}/external/baseclasses/vtrans.cpp"
	"${SOURCE_ROOT}/external/baseclasses/winctrl.cpp"
	"${SOURCE_ROOT}/external/baseclasses/winutil.cpp"
	"${SOURCE_ROOT}/external/baseclasses/wxlist.cpp"
	"${SOURCE_ROOT}/external/baseclasses/wxutil.cpp"
)

set(ORIGINAL_WXDEBUG "${SOURCE_ROOT}/external/baseclasses/wxdebug.cpp")
set(FIXED_WXDEBUG    "${CMAKE_BINARY_DIR}/baseclasses/wxdebug.cpp")

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	list(APPEND BASECLASSES_SOURCES ${ORIGINAL_WXDEBUG})
else()
	if(EXISTS ${ORIGINAL_WXDEBUG})
		file(READ ${ORIGINAL_WXDEBUG} FILE_CONTENTS)
		string(
			REPLACE "CDisp::CDisp(pp)" "(void)CDisp(pp)" 
			MODIFIED_CONTENTS "${FILE_CONTENTS}"
		)
		file(WRITE ${FIXED_WXDEBUG} "${MODIFIED_CONTENTS}")
	endif()

	if(EXISTS ${FIXED_WXDEBUG})
		list(APPEND BASECLASSES_SOURCES ${FIXED_WXDEBUG})
	endif()
endif()

add_library(baseclasses STATIC ${BASECLASSES_SOURCES})

target_include_directories(baseclasses PUBLIC
	"${SOURCE_ROOT}/external/baseclasses/"
)

target_compile_definitions(baseclasses 
	PRIVATE "UNICODE" "_UNICODE"
)