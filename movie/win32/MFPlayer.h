
#ifndef __MF_PLAYER_H__
#define __MF_PLAYER_H__

#include <cmath>
#include <deque>

#include "PlayWindow.h"
#include "CDLLLoader.h"
#include "MFVideoSink.h"
#include "krmovie.h"
#include <evr.h>

// Media Foundation Player
class tTVPPlayerCallback : public IMFAsyncCallback, public CUnknown {
	class tTVPMFPlayer* owner_;
public:
	tTVPPlayerCallback( class tTVPMFPlayer* owner ) : CUnknown(L"PlayerCallback",NULL), owner_(owner) {}

	// IUnknown
	DECLARE_IUNKNOWN;
	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid,void **ppv);

	STDMETHODIMP GetParameters( DWORD *pdwFlags, DWORD *pdwQueue );
	STDMETHODIMP Invoke( IMFAsyncResult *pAsyncResult );
};

class tTVPMFPlayer : public iTVPVideoOverlay, public PlayWindow
{
protected:
	static const MFTIME ONE_SECOND = 10000000;
	static const LONG   ONE_MSEC = 1000;

	HWND		BuildWindow;
	ULONG		RefCount;
	HWND		OwnerWindow;
	bool		Visible;
	bool		Shutdown;
	RECT		Rect;
	HWND		CallbackWindow;
	bool		HasVideo;
	float		PlayRate;
	float		ZoomLevel;	// (1.0 == 100%)
	long		AudioVolumeValue;
	long		AudioBalanceValue;

	UINT32		FPSNumerator;
	UINT32		FPSDenominator;

	tTVPPlayerCallback* PlayerCallback;
	CComPtr<IMFByteStream>			ByteStream;
	CComPtr<IMFVideoDisplayControl>	VideoDisplayControl;
	CComPtr<IMFMediaSession>		MediaSession;
	CComPtr<IMFTopology>			Topology;
	CComPtr<IMFRateControl>			RateControl;
	CComPtr<IMFRateSupport>			RateSupport;
	CComPtr<IMFPresentationClock>	PresentationClock;
	CComPtr<IMFAudioStreamVolume>	AudioVolume;
	CComPtr<IMFSimpleAudioVolume>	SimpleAudioVolume;
	CComPtr<IMFMediaSource>			MediaSource;

	MFTIME				HnsDuration;

	// ---- レイヤ描画モード ----
	// 子ウィンドウを作らずデコード結果をエンジンの Bitmap へ書き出す。オーバーレイ再生
	// (EVR + WS_CHILD 子ウィンドウ) では Steam overlay が映像に隠れるため使わない。
	bool		LayerMode;

	BYTE		*LayerBitmap[2];	//!< エンジンが用意したフロント/バックバッファ
	long		LayerBufferSize;	//!< 1 バッファのバイト数
	int			LayerFrontIndex;
	long		LayerVideoWidth;
	long		LayerVideoHeight;
	//! レイヤ座標系の大きさ。0 ならクライアントの画素サイズで代用する
	long		LayerCanvasWidth;
	long		LayerCanvasHeight;
	tTVPMFVideoSink	*VideoSink;	//!< 参照カウントは自前で管理する
	CCritSec	LayerLock;
	bool		MediaSourceResolved;
	//! vsEnded の二重送信を防ぐ
	bool		EndedPosted;

	CCritSec	EventLock;
	std::deque<LONG_PTR>	EventQueue;

	std::wstring		StreamName;
	CDLLLoader MfplatDLL;

protected:
	void __stdcall ReleaseAll();

	HRESULT AddSourceNode( IMFTopology *pTopology, IMFMediaSource *pSource, IMFPresentationDescriptor *pPD, IMFStreamDescriptor *pSD, IMFTopologyNode **ppNode );
	HRESULT AddOutputNode( IMFTopology *pTopology, IMFStreamSink *pStreamSink, IMFTopologyNode **ppNode );
	HRESULT AddOutputNode( IMFTopology *pTopology, IMFActivate *pActivate, DWORD dwId, IMFTopologyNode **ppNode );

	HRESULT AddBranchToPartialTopology( IMFTopology *pTopology, IMFMediaSource *pSource, IMFPresentationDescriptor *pPD, DWORD iStream, HWND hVideoWnd );
	HRESULT CreateMediaSinkActivate( IMFStreamDescriptor *pSourceSD, HWND hVideoWindow, IMFActivate **ppActivate, IMFStreamSink **ppStreamSink );

	HRESULT CreateVideoPlayer();

	//! トポロジ作成前に映像サイズを知るために先に開く
	HRESULT ResolveMediaSource();
	HRESULT ResolveVideoInfo();
	/*
	HRESULT GetPresentationDescriptorFromTopology( IMFPresentationDescriptor **ppPD );

	template <class Q>
	HRESULT GetCollectionObject(IMFCollection *pCollection, DWORD dwIndex, Q **ppObject) {
		*ppObject = NULL;
		IUnknown *pUnk = NULL;
		HRESULT hr = pCollection->GetElement(dwIndex, &pUnk);
		if( SUCCEEDED(hr) ) {
			hr = pUnk->QueryInterface(IID_PPV_ARGS(ppObject));
			pUnk->Release();
		}
		return hr;
	}
	*/
	float CalcLeftVolume() const {
		// Attenuation (dB) = 20 * log10(Level)
		// Level = 10 ^ dB / 20
		float level = std::pow( 10.0f, AudioVolumeValue / (20.0f*100) );
		if( AudioBalanceValue > 0 ) {
			float balance = std::pow( 10.0f, (-AudioBalanceValue) / (20.0f*100) );
			level *= balance;
		}
		if( level < 0.0f ) level = 0.0f;
		if( level > 1.0f ) level = 1.0f;
		return level;
	}

	float CalcRightVolume() const {
		// Attenuation (dB) = 20 * log10(Level)
		// Level = 10 ^ dB / 20
		float level = std::pow( 10.0f, AudioVolumeValue / (20.0f*100) );
		if( AudioBalanceValue < 0 ) {
			float balance = std::pow( 10.0f, AudioBalanceValue / (20.0f*100) );
			level *= balance;
		}
		if( level < 0.0f ) level = 0.0f;
		if( level > 1.0f ) level = 1.0f;
		return level;
	}
	float CalcVolume() const {
		float level = std::pow( 10.0f, AudioVolumeValue / (20.0f*100) );
		if( level < 0.0f ) level = 0.0f;
		if( level > 1.0f ) level = 1.0f;
		return level;
	}
	HRESULT SetVolumeToMF();
public:
	IMFMediaSession* GetMediaSession() { return MediaSession; }

	// ---- tTVPMFVideoSink から呼ばれるレイヤ描画用インターフェイス ----
	//! 映像を描く先のレイヤ座標系の大きさ。open の前に呼ぶ
	void SetLayerCanvasSize( long width, long height );
	bool GetLayerBackBuffer( BYTE **buff, long *size );
	void PresentLayerFrame( LONGLONG hnsSampleTime );
	void OnLayerVideoSizeChanged( long width, long height, UINT32 fpsNumerator, UINT32 fpsDenominator );
	MFTIME GetDuration() const { return HnsDuration; }
	//! シンクが検出したストリーム終端をエンジンへ伝える
	void OnLayerEndOfStream();
	void PostEndedOnce();

	void NotifyError( HRESULT hr );
	void OnMediaItemCleared();
	void OnPause();
	void OnPlayBackEnded();
	void OnRateSet( double rate );
	void OnStop();
	void OnPlay();

	void OnTopologyStatus(UINT32 status);

	virtual void OnDestoryWindow();

	MFCLOCK_STATE GetClockState();

public:
	tTVPMFPlayer( bool layerMode = false );
	virtual ~tTVPMFPlayer();

	virtual void __stdcall BuildGraph( HWND callbackwin, IStream *stream,
		const wchar_t * streamname, const wchar_t *type, unsigned __int64 size );

	virtual void __stdcall AddRef();
	virtual void __stdcall Release();

	virtual void __stdcall SetWindow(HWND window);
	virtual void __stdcall SetMessageDrainWindow(HWND window);
	virtual void __stdcall SetRect(RECT *rect);
	virtual void __stdcall SetVisible(bool b);
	virtual void __stdcall Play();
	virtual void __stdcall Stop();
	virtual void __stdcall Pause();
	virtual void __stdcall SetPosition(unsigned __int64 tick);
	virtual void __stdcall GetPosition(unsigned __int64 *tick);
	virtual void __stdcall GetStatus(tTVPVideoStatus *status);
	virtual void __stdcall GetEvent(long *evcode, LONG_PTR *param1, LONG_PTR *param2, bool *got);

	virtual void __stdcall FreeEventParams(long evcode, LONG_PTR param1, LONG_PTR param2);

	virtual void __stdcall Rewind();
	virtual void __stdcall SetFrame( int f );
	virtual void __stdcall GetFrame( int *f );
	virtual void __stdcall GetFPS( double *f );
	virtual void __stdcall GetNumberOfFrame( int *f );
	virtual void __stdcall GetTotalTime( __int64 *t );
	
	virtual void __stdcall GetVideoSize( long *width, long *height );
	virtual void __stdcall GetFrontBuffer( BYTE **buff );
	virtual void __stdcall SetVideoBuffer( BYTE *buff1, BYTE *buff2, long size );

	virtual void __stdcall SetStopFrame( int frame );
	virtual void __stdcall GetStopFrame( int *frame );
	virtual void __stdcall SetDefaultStopFrame();

	virtual void __stdcall SetPlayRate( double rate );
	virtual void __stdcall GetPlayRate( double *rate );

	virtual void __stdcall SetAudioBalance( long balance );
	virtual void __stdcall GetAudioBalance( long *balance );
	virtual void __stdcall SetAudioVolume( long volume );
	virtual void __stdcall GetAudioVolume( long *volume );

	virtual void __stdcall GetNumberOfAudioStream( unsigned long *streamCount );
	virtual void __stdcall SelectAudioStream( unsigned long num );
	virtual void __stdcall GetEnableAudioStreamNum( long *num );
	virtual void __stdcall DisableAudioStream( void );

	virtual void __stdcall GetNumberOfVideoStream( unsigned long *streamCount );
	virtual void __stdcall SelectVideoStream( unsigned long num );
	virtual void __stdcall GetEnableVideoStreamNum( long *num );

	virtual void __stdcall SetMixingBitmap( HDC hdc, RECT *dest, float alpha );
	virtual void __stdcall ResetMixingBitmap();

	virtual void __stdcall SetMixingMovieAlpha( float a );
	virtual void __stdcall GetMixingMovieAlpha( float *a );
	virtual void __stdcall SetMixingMovieBGColor( unsigned long col );
	virtual void __stdcall GetMixingMovieBGColor( unsigned long *col );

	virtual void __stdcall PresentVideoImage();

	virtual void __stdcall GetContrastRangeMin( float *v );
	virtual void __stdcall GetContrastRangeMax( float *v );
	virtual void __stdcall GetContrastDefaultValue( float *v );
	virtual void __stdcall GetContrastStepSize( float *v );
	virtual void __stdcall GetContrast( float *v );
	virtual void __stdcall SetContrast( float v );

	virtual void __stdcall GetBrightnessRangeMin( float *v );
	virtual void __stdcall GetBrightnessRangeMax( float *v );
	virtual void __stdcall GetBrightnessDefaultValue( float *v );
	virtual void __stdcall GetBrightnessStepSize( float *v );
	virtual void __stdcall GetBrightness( float *v );
	virtual void __stdcall SetBrightness( float v );

	virtual void __stdcall GetHueRangeMin( float *v );
	virtual void __stdcall GetHueRangeMax( float *v );
	virtual void __stdcall GetHueDefaultValue( float *v );
	virtual void __stdcall GetHueStepSize( float *v );
	virtual void __stdcall GetHue( float *v );
	virtual void __stdcall SetHue( float v );

	virtual void __stdcall GetSaturationRangeMin( float *v );
	virtual void __stdcall GetSaturationRangeMax( float *v );
	virtual void __stdcall GetSaturationDefaultValue( float *v );
	virtual void __stdcall GetSaturationStepSize( float *v );
	virtual void __stdcall GetSaturation( float *v );
	virtual void __stdcall SetSaturation( float v );
};



#endif // __MF_PLAYER_H__
