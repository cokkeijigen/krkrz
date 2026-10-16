
#ifndef __TVP_VIDEO_OVERLAY_H__
#define __TVP_VIDEO_OVERLAY_H__

extern void __stdcall GetVideoOverlayObject(
	HWND callbackwin, struct IStream *stream, const wchar_t * streamname,
	const wchar_t *type, unsigned __int64 size, class iTVPVideoOverlay **out);

// レイヤ描画用は映像を描く先のレイヤ座標系の大きさも受け取る (クライアントの画素サイズではない)
extern void __stdcall GetVideoLayerObject(
	HWND callbackwin, struct IStream *stream, const wchar_t * streamname,
	const wchar_t *type, unsigned __int64 size, long layerWidth, long layerHeight,
	class iTVPVideoOverlay **out);

extern void __stdcall GetMixingVideoOverlayObject(
	HWND callbackwin, struct IStream *stream, const wchar_t * streamname,
	const wchar_t *type, unsigned __int64 size, class iTVPVideoOverlay **out);

extern void __stdcall GetMFVideoOverlayObject(
	HWND callbackwin, struct IStream *stream, const wchar_t * streamname,
	const wchar_t *type, unsigned __int64 size, class iTVPVideoOverlay **out);

// レイヤ描画用 (デコード結果をエンジンの Bitmap へ書き出す)
extern void __stdcall GetMFVideoLayerObject(
	HWND callbackwin, struct IStream *stream, const wchar_t * streamname,
	const wchar_t *type, unsigned __int64 size, long layerWidth, long layerHeight,
	class iTVPVideoOverlay **out);


/*[*/
typedef void* (*tTVPCreateDSFilter)( void* formatdata );
/*]*/

TJS_EXP_FUNC_DEF( void, TVPRegisterDSVideoCodec, ( const ttstr & name, void* guid, tTVPCreateDSFilter splitter,
	tTVPCreateDSFilter video, tTVPCreateDSFilter audio, void* formatdata ) );
TJS_EXP_FUNC_DEF( void, TVPUnregisterDSVideoCodec, (const ttstr & name, void* guid, tTVPCreateDSFilter splitter,
	tTVPCreateDSFilter video, tTVPCreateDSFilter audio, void* formatdata ) );

// 一気に登録する形になっているけど、拡張子とそのフォーマットのGUID
// メジャータイプとサブタイプのGUIDとフィルタのペアを登録するようになっている方が汎用的かな
// でもそうするとDShowのようにより細かく登録できる方がいいってことになってまた大変か

struct tTVPDSFilterHandlerType
{
	ttstr Extension;
	GUID* Guid;
	tTVPCreateDSFilter SplitterHander;
	tTVPCreateDSFilter VideoHander;
	tTVPCreateDSFilter AudioHander;
	void * FormatData;

	tTVPDSFilterHandlerType(const ttstr &ext,
		void* guid,
		tTVPCreateDSFilter splitter,
		tTVPCreateDSFilter video,
		tTVPCreateDSFilter audio,
		void * data) : Extension(ext), Guid((GUID*)guid),
		SplitterHander(splitter), VideoHander(video), AudioHander(audio), FormatData(data)
	{
	}

	tTVPDSFilterHandlerType(const tTVPDSFilterHandlerType & ref)
	{
		Guid = ref.Guid;
		SplitterHander = ref.SplitterHander;
		VideoHander = ref.VideoHander;
		AudioHander = ref.AudioHander;
		Extension = ref.Extension;
		FormatData = ref.FormatData;
	}

	bool operator == (const tTVPDSFilterHandlerType & ref) const
	{
		return Guid == ref.Guid &&
				SplitterHander == ref.SplitterHander &&
				VideoHander == ref.VideoHander &&
				AudioHander == ref.AudioHander &&
				Extension == ref.Extension &&
				FormatData == ref.FormatData;
	}
};

extern tTVPDSFilterHandlerType* TVPGetDSFilterHandler( const ttstr& ext );
extern tTVPDSFilterHandlerType* TVPGetDSFilterHandler( const GUID& guid );

#endif // __TVP_VIDEO_OVERLAY_H__
