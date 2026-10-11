/****************************************************************************/
/*! @file
@brief part of KRLMOVIE.DLL

-----------------------------------------------------------------------------
	Copyright (C) 2004 T.Imoto <http://www.kaede-software.com>
-----------------------------------------------------------------------------
@author		T.Imoto
@date		2004/09/22
@note
			2004/09/22	T.Imoto		
*****************************************************************************/

#include "tjsCommHead.h"
#include "SysInitIntf.h"
#include "PluginImpl.h"
#include "dslayerd.h"
#include "krmovie.h"
#include "TVPVideoOverlay.h"

#include <wchar.h>

#include "asyncio.h"
#include "asyncrdr.h"

#include "OptionInfo.h"


//----------------------------------------------------------------------------
//! @brief dsmovie.cpp の ParseVideoType / IsWindowsMediaFile と同じ判定。
//! false になった形式は DirectShow のデコーダが無いので Media Foundation へ回す。
//----------------------------------------------------------------------------
static bool TVPIsDSLayerMovieType( const wchar_t *type )
{
	if( _wcsicmp(type, L".mpg")  == 0 ) return true;
	if( _wcsicmp(type, L".mpeg") == 0 ) return true;
	if( _wcsicmp(type, L".mpv")  == 0 ) return true;
	if( _wcsicmp(type, L".m1v")  == 0 ) return true;
	if( _wcsicmp(type, L".dat")  == 0 ) return true;
	if( _wcsicmp(type, L".avi")  == 0 ) return true;
	if( _wcsicmp(type, L".mov")  == 0 ) return true;
	if( _wcsicmp(type, L".asf")  == 0 ) return true;
	if( _wcsicmp(type, L".wma")  == 0 ) return true;
	if( _wcsicmp(type, L".wmv")  == 0 ) return true;
#ifdef ENABLE_THEORA
	if( _wcsicmp(type, L".ogv")  == 0 ) return true;
#endif
	// ユーザーが登録した DirectShow フィルタハンドラがあればそれを優先する
	return TVPGetDSFilterHandler( ttstr(type) ) != NULL;
}

//----------------------------------------------------------------------------
//! @brief DirectShow でデコードできない形式 (mp4/webm など) は Media Foundation で
//! デコードし、フレームをエンジンの Bitmap へ書き出す。どちらも映像は「レイヤ」
//! としてメインの描画デバイス経由で描かれるため Steam overlay を上に合成できる。
//----------------------------------------------------------------------------
void __stdcall GetVideoLayerObject(
	HWND callbackwin, IStream *stream, const wchar_t * streamname,
	const wchar_t *type, unsigned __int64 size, iTVPVideoOverlay **out)
{

	if( !TVPIsDSLayerMovieType( type ) ) {
		GetMFVideoLayerObject( callbackwin, stream, streamname, type, size, out );
		return;
	}

	*out = new tTVPDSLayerVideo;

	if( *out )
		static_cast<tTVPDSLayerVideo*>(*out)->BuildGraph( callbackwin, stream, streamname, type, size );
}
