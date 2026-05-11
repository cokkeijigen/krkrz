cmake_minimum_required(VERSION 3.20.0)
project(KRKRZ C CXX ASM_NASM)

set(TARGET_NAME "tvpwin")
option(TARGET_ARCH_SUFFIX "Enable architecture suffix (32/64) for targets" TRUE)

if(TARGET_ARCH_SUFFIX)
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(TARGET_NAME "${TARGET_NAME}32")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(TARGET_NAME "${TARGET_NAME}64")
    endif()
endif()

set(IS_RELEASE "$<CONFIG:Release>")
set(IS_C_CPP   "$<OR:$<COMPILE_LANGUAGE:C>,$<COMPILE_LANGUAGE:CXX>>")

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")

	set(MSVC_ONLY_FLAGS
		"/wd4838"
		"/wd4311"
		"/wd4312"
		"/wd4477"
		"/wd4297"
		"/source-charset:utf-8"
	)
	
	foreach(flag ${MSVC_ONLY_FLAGS})
		add_compile_options("$<${IS_C_CPP}:${flag}>")
	endforeach()
	
	set(MSVC_RELEASE_ONLY_FLAGS "/GF" "/Oy" "/GL" "/GS-")
	foreach(flag ${MSVC_RELEASE_ONLY_FLAGS})
		add_compile_options(
			"$<$<AND:${IS_RELEASE},${IS_C_CPP}>:${flag}>"
		)
	endforeach()

	enable_language(ASM_MASM)

elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")

	set(CLANG_ONLY_FLAGS
		"-fms-extensions"
		"-fms-compatibility"
		"-Wno-microsoft-template"
		"-Wno-deprecated-register"
		"-Waddress-of-temporary"
		"-Wno-invalid-token-paste"
		"-Wno-c++11-narrowing"
		"-Wno-narrowing"
		"-fno-operator-names"
		"-fno-sanitize=all"
		"-fno-stack-protector"
		"-Wno-ignored-attributes"
		"-Wno-pointer-to-int-cast"
		"-Wno-int-to-void-pointer-cast"
		"-Wno-int-to-pointer-cast"
		"-Wno-undefined-bool-conversion"
		"-fno-delete-null-pointer-checks"
		"-Wno-microsoft-extra-qualification"
		"-Wno-shift-negative-value"
		"-Wno-microsoft-enum-forward-reference"
		"-Wno-delete-abstract-non-virtual-dtor"
		"-Wno-implicit-exception-spec-mismatch"
		"-Wno-non-pod-varargs"
		"-Wno-unused-value"
		"-Wno-mismatched-new-delete"
		"-Wno-microsoft-cast"
		"-Wno-deprecated-declarations"
		"-Wno-nonportable-include-path"
		"-Wno-tautological-constant-out-of-range-compare"
		"-Wno-inconsistent-missing-override"
		"-Wno-error=deprecated-declarations"
		"-Wno-microsoft-exception-spec"
		"-Wno-explicit-specialization-storage-class"
		"-Wno-error=non-pod-varargs"
		"-Wno-pointer-bool-conversion"
		"-Wno-parentheses"
		"-Wno-null-dereference"
		"-Wno-deprecated-non-prototype"
		"-Wno-writable-strings"
		"-Wno-extra-tokens"
		"-Wno-switch"
		"-Wno-exceptions"
		"-Wno-missing-exception-spec"
		"-msse4.1" 
		"-mavx" 
		"-mavx2"
	)
	foreach(flag ${CLANG_ONLY_FLAGS})
		add_compile_options("$<${IS_C_CPP}:${flag}>")
	endforeach()
	add_link_options("-Wl,/SAFESEH:NO")
endif()

set(SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}")
include("${SOURCE_ROOT}/cmake/external.cmake")

set(C_CXX_SOURCES

	"${SOURCE_ROOT}/base/BinaryStream.cpp"
	"${SOURCE_ROOT}/base/CharacterSet.cpp"
	"${SOURCE_ROOT}/base/EventIntf.cpp"
	"${SOURCE_ROOT}/base/PluginIntf.cpp"
	"${SOURCE_ROOT}/base/ScriptMgnIntf.cpp"
	"${SOURCE_ROOT}/base/StorageIntf.cpp"
	"${SOURCE_ROOT}/base/SysInitIntf.cpp"
	"${SOURCE_ROOT}/base/SystemIntf.cpp"
	"${SOURCE_ROOT}/base/TextStream.cpp"
	"${SOURCE_ROOT}/base/UtilStreams.cpp"
	"${SOURCE_ROOT}/base/XP3Archive.cpp"

	"${SOURCE_ROOT}/base/win32/EventImpl.cpp"
	"${SOURCE_ROOT}/base/win32/FileSelector.cpp"
	"${SOURCE_ROOT}/base/win32/FuncStubs.cpp"
	"${SOURCE_ROOT}/base/win32/NativeEventQueue.cpp"
	"${SOURCE_ROOT}/base/win32/PluginImpl.cpp"
	"${SOURCE_ROOT}/base/win32/ScriptMgnImpl.cpp"
	"${SOURCE_ROOT}/base/win32/StorageImpl.cpp"
	"${SOURCE_ROOT}/base/win32/SusieArchive.cpp"
	"${SOURCE_ROOT}/base/win32/SysInitImpl.cpp"
	"${SOURCE_ROOT}/base/win32/SystemImpl.cpp"

	"${SOURCE_ROOT}/environ/win32/Application.cpp"
	"${SOURCE_ROOT}/environ/win32/CompatibleNativeFuncs.cpp"
	"${SOURCE_ROOT}/environ/win32/ConfigFormUnit.cpp"
	"${SOURCE_ROOT}/environ/win32/DetectCPU.cpp"
	"${SOURCE_ROOT}/environ/win32/EmergencyExit.cpp"
	"${SOURCE_ROOT}/environ/win32/MouseCursor.cpp"
	"${SOURCE_ROOT}/environ/win32/SystemControl.cpp"
	"${SOURCE_ROOT}/environ/win32/TouchPoint.cpp"
	"${SOURCE_ROOT}/environ/win32/TVPWindow.cpp"
	"${SOURCE_ROOT}/environ/win32/VersionFormUnit.cpp"
	"${SOURCE_ROOT}/environ/win32/WindowFormUnit.cpp"
	"${SOURCE_ROOT}/environ/win32/WindowsUtil.cpp"

	"${SOURCE_ROOT}/extension/Extension.cpp"

	"${SOURCE_ROOT}/movie/win32/asyncio.cpp"
	"${SOURCE_ROOT}/movie/win32/asyncrdr.cpp"
	"${SOURCE_ROOT}/movie/win32/BufferRenderer.cpp"
	"${SOURCE_ROOT}/movie/win32/CDemuxOutputPin.cpp"
	"${SOURCE_ROOT}/movie/win32/CDemuxSource.cpp"
	"${SOURCE_ROOT}/movie/win32/CMediaSeekingProxy.cpp"
	"${SOURCE_ROOT}/movie/win32/CVMRCustomAllocatorPresenter9.cpp"
	"${SOURCE_ROOT}/movie/win32/CWMAllocator.cpp"
	"${SOURCE_ROOT}/movie/win32/CWMBuffer.cpp"
	"${SOURCE_ROOT}/movie/win32/CWMReader.cpp"
	"${SOURCE_ROOT}/movie/win32/DShowException.cpp"
	"${SOURCE_ROOT}/movie/win32/dslayerd.cpp"
	"${SOURCE_ROOT}/movie/win32/dsmixer.cpp"
	"${SOURCE_ROOT}/movie/win32/dsmovie.cpp"
	"${SOURCE_ROOT}/movie/win32/dsoverlay.cpp"
	"${SOURCE_ROOT}/movie/win32/krlmovie.cpp"
	"${SOURCE_ROOT}/movie/win32/krmmovie.cpp"
	"${SOURCE_ROOT}/movie/win32/krmovie.cpp"
	"${SOURCE_ROOT}/movie/win32/MFPlayer.cpp"
	"${SOURCE_ROOT}/movie/win32/PlayWindow.cpp"
	"${SOURCE_ROOT}/movie/win32/TVPVideoOverlay.cpp"
	"${SOURCE_ROOT}/movie/win32/IBufferRenderer_i.c"
	"${SOURCE_ROOT}/movie/win32/IRendererBufferAccess_i.c"
	"${SOURCE_ROOT}/movie/win32/IRendererBufferVideo_i.c"

	"${SOURCE_ROOT}/msg/MsgIntf.cpp"
	"${SOURCE_ROOT}/msg/win32/MsgImpl.cpp"
	"${SOURCE_ROOT}/msg/win32/MsgLoad.cpp"
	"${SOURCE_ROOT}/msg/win32/ReadOptionDesc.cpp"

	"${SOURCE_ROOT}/sound/MathAlgorithms.cpp"
	"${SOURCE_ROOT}/sound/MathAlgorithms_SSE.cpp"
	"${SOURCE_ROOT}/sound/PhaseVocoderDSP.cpp"
	"${SOURCE_ROOT}/sound/PhaseVocoderFilter.cpp"
	"${SOURCE_ROOT}/sound/RealFFT.cpp"
	"${SOURCE_ROOT}/sound/RealFFT_SSE.cpp"
	"${SOURCE_ROOT}/sound/SoundBufferBaseIntf.cpp"
	"${SOURCE_ROOT}/sound/WaveFormatConverter.cpp"
	"${SOURCE_ROOT}/sound/WaveFormatConverter_SSE.cpp"
	"${SOURCE_ROOT}/sound/WaveIntf.cpp"
	"${SOURCE_ROOT}/sound/WaveLoopManager.cpp"
	"${SOURCE_ROOT}/sound/WaveSegmentQueue.cpp"
	"${SOURCE_ROOT}/sound/xmmlib.cpp"

	"${SOURCE_ROOT}/sound/win32/SoundBufferBaseImpl.cpp"
	"${SOURCE_ROOT}/sound/win32/WaveImpl.cpp"
	"${SOURCE_ROOT}/sound/win32/tvpsnd.c"

	"${SOURCE_ROOT}/tjs2/tjs.cpp"
	"${SOURCE_ROOT}/tjs2/tjs.tab.cpp"
	"${SOURCE_ROOT}/tjs2/tjsArray.cpp"
	"${SOURCE_ROOT}/tjs2/tjsBinarySerializer.cpp"
	"${SOURCE_ROOT}/tjs2/tjsByteCodeLoader.cpp"
	"${SOURCE_ROOT}/tjs2/tjsCompileControl.cpp"
	"${SOURCE_ROOT}/tjs2/tjsConfig.cpp"
	"${SOURCE_ROOT}/tjs2/tjsConstArrayData.cpp"
	"${SOURCE_ROOT}/tjs2/tjsDate.cpp"
	"${SOURCE_ROOT}/tjs2/tjsdate.tab.cpp"
	"${SOURCE_ROOT}/tjs2/tjsDateParser.cpp"
	"${SOURCE_ROOT}/tjs2/tjsDebug.cpp"
	"${SOURCE_ROOT}/tjs2/tjsDictionary.cpp"
	"${SOURCE_ROOT}/tjs2/tjsDisassemble.cpp"
	"${SOURCE_ROOT}/tjs2/tjsError.cpp"
	"${SOURCE_ROOT}/tjs2/tjsException.cpp"
	"${SOURCE_ROOT}/tjs2/tjsGlobalStringMap.cpp"
	"${SOURCE_ROOT}/tjs2/tjsInterCodeExec.cpp"
	"${SOURCE_ROOT}/tjs2/tjsInterCodeGen.cpp"
	"${SOURCE_ROOT}/tjs2/tjsInterface.cpp"
	"${SOURCE_ROOT}/tjs2/tjsLex.cpp"
	"${SOURCE_ROOT}/tjs2/tjsMath.cpp"
	"${SOURCE_ROOT}/tjs2/tjsMessage.cpp"
	"${SOURCE_ROOT}/tjs2/tjsMT19937ar-cok.cpp"
	"${SOURCE_ROOT}/tjs2/tjsNamespace.cpp"
	"${SOURCE_ROOT}/tjs2/tjsNative.cpp"
	"${SOURCE_ROOT}/tjs2/tjsObject.cpp"
	"${SOURCE_ROOT}/tjs2/tjsObjectExtendable.cpp"
	"${SOURCE_ROOT}/tjs2/tjsOctPack.cpp"
	"${SOURCE_ROOT}/tjs2/tjspp.tab.cpp"
	"${SOURCE_ROOT}/tjs2/tjsRandomGenerator.cpp"
	"${SOURCE_ROOT}/tjs2/tjsRegExp.cpp"
	"${SOURCE_ROOT}/tjs2/tjsScriptBlock.cpp"
	"${SOURCE_ROOT}/tjs2/tjsScriptCache.cpp"
	"${SOURCE_ROOT}/tjs2/tjsString.cpp"
	"${SOURCE_ROOT}/tjs2/tjsUtils.cpp"
	"${SOURCE_ROOT}/tjs2/tjsVariant.cpp"
	"${SOURCE_ROOT}/tjs2/tjsVariantString.cpp"

	"${SOURCE_ROOT}/utils/win32/ClipboardImpl.cpp"
	"${SOURCE_ROOT}/utils/win32/ThreadImpl.cpp"
	"${SOURCE_ROOT}/utils/win32/TVPTimer.cpp"

	"${SOURCE_ROOT}/utils/ClipboardIntf.cpp"
	"${SOURCE_ROOT}/utils/cp932_uni.cpp"
	"${SOURCE_ROOT}/utils/DebugIntf.cpp"
	"${SOURCE_ROOT}/utils/MiscUtility.cpp"
	"${SOURCE_ROOT}/utils/Random.cpp"
	"${SOURCE_ROOT}/utils/ThreadIntf.cpp"
	"${SOURCE_ROOT}/utils/TickCount.cpp"
	"${SOURCE_ROOT}/utils/TimerImpl.cpp"
	"${SOURCE_ROOT}/utils/TimerIntf.cpp"
	"${SOURCE_ROOT}/utils/uni_cp932.cpp"
	"${SOURCE_ROOT}/utils/VelocityTracker.cpp"
	"${SOURCE_ROOT}/utils/md5.c"

	"${SOURCE_ROOT}/visual/gl/adjust_color_sse2.cpp"
	"${SOURCE_ROOT}/visual/gl/blend_function.cpp"
	"${SOURCE_ROOT}/visual/gl/blend_function_avx2.cpp"
	"${SOURCE_ROOT}/visual/gl/blend_function_sse2.cpp"
	"${SOURCE_ROOT}/visual/gl/boxblur_sse2.cpp"
	"${SOURCE_ROOT}/visual/gl/colorfill_sse2.cpp"
	"${SOURCE_ROOT}/visual/gl/colormap_sse2.cpp"
	"${SOURCE_ROOT}/visual/gl/pixelformat_sse2.cpp"
	"${SOURCE_ROOT}/visual/gl/ResampleImage.cpp"
	"${SOURCE_ROOT}/visual/gl/ResampleImageAVX2.cpp"
	"${SOURCE_ROOT}/visual/gl/ResampleImageSSE2.cpp"
	"${SOURCE_ROOT}/visual/gl/tlg_sse2.cpp"
	"${SOURCE_ROOT}/visual/gl/univtrans_sse2.cpp"
	"${SOURCE_ROOT}/visual/gl/WeightFunctor.cpp"
	"${SOURCE_ROOT}/visual/gl/x86simdutil.cpp"
	"${SOURCE_ROOT}/visual/gl/x86simdutilAVX2.cpp"

	"${SOURCE_ROOT}/visual/IA32/detect_cpu.cpp"
	"${SOURCE_ROOT}/visual/IA32/tvpgl_ia32_intf.c"

	"${SOURCE_ROOT}/visual/win32/BasicDrawDevice.cpp"
	"${SOURCE_ROOT}/visual/win32/BitmapBitsAlloc.cpp"
	"${SOURCE_ROOT}/visual/win32/BitmapInfomation.cpp"
	"${SOURCE_ROOT}/visual/win32/DInputMgn.cpp"
	"${SOURCE_ROOT}/visual/win32/DrawDevice.cpp"
	"${SOURCE_ROOT}/visual/win32/GDIFontRasterizer.cpp"
	"${SOURCE_ROOT}/visual/win32/GraphicsLoaderImpl.cpp"
	"${SOURCE_ROOT}/visual/win32/LayerImpl.cpp"
	"${SOURCE_ROOT}/visual/win32/NativeFreeTypeFace.cpp"
	"${SOURCE_ROOT}/visual/win32/TVPScreen.cpp"
	"${SOURCE_ROOT}/visual/win32/TVPSysFont.cpp"
	"${SOURCE_ROOT}/visual/win32/VideoOvlImpl.cpp"
	"${SOURCE_ROOT}/visual/win32/VSyncTimingThread.cpp"
	"${SOURCE_ROOT}/visual/win32/WindowImpl.cpp"

	"${SOURCE_ROOT}/visual/BitmapIntf.cpp"
	"${SOURCE_ROOT}/visual/BitmapLayerTreeOwner.cpp"
	"${SOURCE_ROOT}/visual/CharacterData.cpp"
	"${SOURCE_ROOT}/visual/ComplexRect.cpp"
	"${SOURCE_ROOT}/visual/FontSystem.cpp"
	"${SOURCE_ROOT}/visual/FreeType.cpp"
	"${SOURCE_ROOT}/visual/FreeTypeFontRasterizer.cpp"
	"${SOURCE_ROOT}/visual/GraphicsLoaderIntf.cpp"
	"${SOURCE_ROOT}/visual/GraphicsLoadThread.cpp"
	"${SOURCE_ROOT}/visual/ImageFunction.cpp"
	"${SOURCE_ROOT}/visual/LayerBitmapImpl.cpp"
	"${SOURCE_ROOT}/visual/LayerBitmapIntf.cpp"
	"${SOURCE_ROOT}/visual/LayerIntf.cpp"
	"${SOURCE_ROOT}/visual/LayerManager.cpp"
	"${SOURCE_ROOT}/visual/LayerTreeOwnerImpl.cpp"
	"${SOURCE_ROOT}/visual/LoadJPEG.cpp"
	"${SOURCE_ROOT}/visual/LoadJXR.cpp"
	"${SOURCE_ROOT}/visual/LoadPNG.cpp"
	"${SOURCE_ROOT}/visual/LoadTLG.cpp"
	"${SOURCE_ROOT}/visual/PrerenderedFont.cpp"
	"${SOURCE_ROOT}/visual/RectItf.cpp"
	"${SOURCE_ROOT}/visual/SaveTLG5.cpp"
	"${SOURCE_ROOT}/visual/SaveTLG6.cpp"
	"${SOURCE_ROOT}/visual/TransIntf.cpp"
	"${SOURCE_ROOT}/visual/VideoOvlIntf.cpp"
	"${SOURCE_ROOT}/visual/WindowIntf.cpp"
	"${SOURCE_ROOT}/visual/tvpgl.c"
)

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
	set(ASM_SOURCES
		"${SOURCE_ROOT}/visual/IA32/tlg6_chroma.nas"
		"${SOURCE_ROOT}/visual/IA32/tlg6_golomb.nas"	
	)
	set_source_files_properties(${ASM_SOURCES} 
		PROPERTIES LANGUAGE ASM_NASM
	)
endif()

add_executable(KRKRZ WIN32 
	"${SOURCE_ROOT}/vcproj/tvpwin32.rc"
	${C_CXX_SOURCES} ${ASM_SOURCES}
)

target_include_directories(KRKRZ PRIVATE
    
	"${SOURCE_ROOT}/vcproj"
	
	"${SOURCE_ROOT}/base/"
	"${SOURCE_ROOT}/base/win32"

	"${SOURCE_ROOT}/environ/win32"

	"${SOURCE_ROOT}/extension"

	"${SOURCE_ROOT}/movie/win32"

	"${SOURCE_ROOT}/sound/"
	"${SOURCE_ROOT}/sound/win32"

	"${SOURCE_ROOT}/msg/"
	"${SOURCE_ROOT}/msg/win32"

	"${SOURCE_ROOT}/tjs2"

	"${SOURCE_ROOT}/utils/win32"
	"${SOURCE_ROOT}/utils"

	"${SOURCE_ROOT}/visual/gl"
	"${SOURCE_ROOT}/visual/IA32"
	"${SOURCE_ROOT}/visual/win32"
	"${SOURCE_ROOT}/visual"

	"${SOURCE_ROOT}/external"
	"${SOURCE_ROOT}/external/onig/src"
	"${SOURCE_ROOT}/external/jxrlib/image/sys"
    "${SOURCE_ROOT}/external/jxrlib/jxrgluelib"
)

target_compile_definitions(KRKRZ PRIVATE
	
    "__WIN32__"
	"__CODEGUARD__"
	"_UNICODE" "UNICODE" 
	"_CRT_SECURE_NO_WARNING"

	"NO_STRICT"
	"TJS_TEXT_OUT_CRLF"
	"TJS_JP_LOCALIZED"
	"TJS_DEBUG_DUMP_STRING"
	"TVP_LOG_TO_COMMANDLINE_CONSOLE"
	"TVP_REPORT_HW_EXCEPTION"
	"TVP_ENABLE_EXECUTE_AT_EXCEPTION"
	#"TVP_DISABLE_SELECT_XP3_OR_FOLDER"
)

target_link_libraries(KRKRZ PRIVATE 
	
	zlib
	onig
    libpng
    turbojpeg
    freetype
    baseclasses

	"mpr.lib"
	"winmm.lib"
	"imm32.lib"
	"version.lib"
)

set_target_properties(KRKRZ
	PROPERTIES OUTPUT_NAME ${TARGET_NAME}
)