/****************************************************************************/
/*! @file
@brief NV12 -> BGRA (下から上 DIB) 変換

	エンジンのレイヤ Bitmap は BGRA・下から上。MF の H.264 デコーダの原生出力は
	NV12 なので、トポロジにカラー変換 MFT を挿させずこちらで変換する
	(マイクロソフトのソフトウェア Video Processor は 1440p で 11ms/フレーム掛かる)。

	AVX2 / SSE2 / スカラの 3 バージョンは係数と丸めを共用しているので、
	出力はビット単位で一致する。
-----------------------------------------------------------------------------
*****************************************************************************/
#ifndef __MF_VIDEO_COLOR_H__
#define __MF_VIDEO_COLOR_H__

#include <windows.h>

#include <mfapi.h>

// MFVideoFormat_NV12 (mfuuid.lib をリンクしないためここで定義する)
extern const GUID kMFVideoFormat_NV12;

//! CPU 判定。XSAVE など OS 側の可否判定を含めてエンジン共通の TVPGetCPUType() に任せる
bool TVPMFColorHasSSE2();
bool TVPMFColorHasAVX2();

//! 以下の変換関数はすべて dst が書き込み先 BGRA の「最終行」を指す前提。
//! dstPitch は正値で、y 行目は dst - y * dstPitch へ書く。
//! bt709 は解像度から推定した色マトリクス (無印 HD=709 / SD=601)

//! スカラ版 (SSE2 を持たない CPU の保険。常に利用可)
void TVPConvertNV12ToBGRAFlip_Scalar( const BYTE *srcY, const BYTE *srcUV, long yStride,
	BYTE *dst, long dstPitch, long width, long height, bool srcBottomUp, bool bt709 );

void TVPConvertNV12ToBGRAFlip_SSE2( const BYTE *srcY, const BYTE *srcUV, long yStride,
	BYTE *dst, long dstPitch, long width, long height, bool srcBottomUp, bool bt709 );

void TVPConvertNV12ToBGRAFlip_AVX2( const BYTE *srcY, const BYTE *srcUV, long yStride,
	BYTE *dst, long dstPitch, long width, long height, bool srcBottomUp, bool bt709 );

void TVPConvertNV12ToBGRAFlip( const BYTE *srcY, const BYTE *srcUV, long yStride,
	BYTE *dst, long dstPitch, long width, long height, bool srcBottomUp, bool bt709 );

#endif	// __MF_VIDEO_COLOR_H__
