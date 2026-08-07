

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
set(BACKUP_WXDEBUG   "${ORIGINAL_WXDEBUG}.bak")

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	list(APPEND BASECLASSES_SOURCES ${ORIGINAL_WXDEBUG})
else()
	if(EXISTS ${ORIGINAL_WXDEBUG})
		if(NOT EXISTS ${BACKUP_WXDEBUG})
			file(COPY_FILE ${ORIGINAL_WXDEBUG} ${BACKUP_WXDEBUG})
		endif()

		file(READ ${ORIGINAL_WXDEBUG} FILE_CONTENTS)
		string(
			REPLACE "CDisp::CDisp(pp)" "(void)CDisp(pp)" 
			MODIFIED_CONTENTS "${FILE_CONTENTS}"
		)
		file(WRITE ${ORIGINAL_WXDEBUG} "${MODIFIED_CONTENTS}")
		list(APPEND BASECLASSES_SOURCES ${ORIGINAL_WXDEBUG})
	endif()
endif()

set(ORIGINAL_TRANSIP_H "${SOURCE_ROOT}/external/baseclasses/transip.h")
set(BACKUP_TRANSIP_H   "${ORIGINAL_TRANSIP_H}.bak")

if(EXISTS ${ORIGINAL_TRANSIP_H})
	if(NOT EXISTS ${BACKUP_TRANSIP_H})
		file(COPY_FILE ${ORIGINAL_TRANSIP_H} ${BACKUP_TRANSIP_H})
	endif()

	file(READ ${ORIGINAL_TRANSIP_H} FILE_CONTENTS)
	string(
		REPLACE 
			"__out_opt IMediaSample * CTransInPlaceFilter::Copy(IMediaSample *pSource);" 
		    "__out_opt IMediaSample * Copy(IMediaSample *pSource);" 
		MODIFIED_CONTENTS "${FILE_CONTENTS}"
	)
	file(WRITE ${ORIGINAL_TRANSIP_H} "${MODIFIED_CONTENTS}")
endif()

set(ORIGINAL_VIDEOCTL_H "${SOURCE_ROOT}/external/baseclasses/videoctl.h")
set(BACKUP_VIDEOCTL_H   "${ORIGINAL_VIDEOCTL_H}.bak")

if(EXISTS ${ORIGINAL_VIDEOCTL_H})
	if(NOT EXISTS ${BACKUP_VIDEOCTL_H})
		file(COPY_FILE ${ORIGINAL_VIDEOCTL_H} ${BACKUP_VIDEOCTL_H})
	endif()

	file(READ ${ORIGINAL_VIDEOCTL_H} FILE_CONTENTS)
	string(
		REPLACE 
			"virtual CAggDirectDraw::~CAggDirectDraw() { };" 
			"virtual ~CAggDirectDraw() { };" 
		MODIFIED_CONTENTS "${FILE_CONTENTS}"
	)
	file(WRITE ${ORIGINAL_VIDEOCTL_H} "${MODIFIED_CONTENTS}")
endif()


add_library(baseclasses STATIC ${BASECLASSES_SOURCES})
set_target_properties(baseclasses PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
target_include_directories(baseclasses PUBLIC
	"${SOURCE_ROOT}/external/baseclasses/"
)

target_compile_definitions(baseclasses 
	PRIVATE "UNICODE" "_UNICODE"
)