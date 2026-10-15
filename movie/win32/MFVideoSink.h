/****************************************************************************/
/*! @file
@brief Media Foundation のデコード結果をエンジンのメモリバッファに書き出す Media Sink

	吉里吉里の vomLayer (レイヤ描画) 専用。オーバーレイ再生 (EVR/VMR9) は映像を
	WS_CHILD 子ウィンドウに描くため Steam overlay がその下に隠れてしまうが、
	レイヤ描画なら映像がメインの描画デバイス経由で描かれるので overlay を
	映像の上に合成できる。

	入力は NV12 を要求する。H.264 デコーダの native 出力が NV12 なので、
	RGB32 を要求するとトポロジローダが Video Processor MFT (色変換) を挿入し、
	1440p で 1 フレーム 11ms かかって 60fps に届かなくなる。
	NV12 のまま受け取って自前の SIMD 変換 (0.67ms) で BGRA に落とす。
	デコーダが NV12 を出せない場合は RGB32 も受け付ける。

-----------------------------------------------------------------------------
*****************************************************************************/

#ifndef __MF_VIDEO_SINK_H__
#define __MF_VIDEO_SINK_H__

#include <Mfidl.h>
#include <mfapi.h>
#include <streams.h>
#include <atlbase.h>
#include <atlcom.h>
#include <deque>

class tTVPMFPlayer;

//----------------------------------------------------------------------------
//! @brief トポロジの出力ノードには IMFStreamSink として直接ぶら下げるので、
//! IMFActivate (IMFAttributes) は実装していない。
//----------------------------------------------------------------------------
class tTVPMFVideoSink : public CUnknown,
						public IMFMediaSink,
						public IMFStreamSink,
						public IMFMediaTypeHandler,
						public IMFClockStateSink
{
private:
	tTVPMFPlayer	*m_Owner;

	CCritSec		 m_Lock;

	std::deque<IMFMediaEvent*>	m_Events;
	IMFAsyncCallback			*m_pEventCallback;
	IUnknown					*m_pEventState;

	CComPtr<IMFMediaType>			m_CurrentType;
	CComPtr<IMFPresentationClock>	m_Clock;

	long	m_VideoWidth;
	long	m_VideoHeight;
	long	m_Stride;			//!< 負値なら下から上の DIB
	bool	m_StrideGuessed;	//!< DEFAULT_STRIDE が無く幅から推定した (ならサンプル長で補正する)
	bool	m_UseBT709;			//!< YUV→RGB のマトリクス。無印 HD は 709、SD は 601
	GUID	m_InputSubtype;		//!< ProcessSample に届くデータの形式 (NV12 / RGB32)

	bool	m_Shutdown;
	bool	m_SamplePumpStarted;

	// 実時間再生用のアンカー
	bool		m_AnchorValid;
	LONGLONG	m_AnchorSampleTime;
	DWORD		m_AnchorTick;

	HRESULT CreateVideoType( UINT32 width, UINT32 height, IMFMediaType **ppType );
	void WaitForPresentation( LONGLONG hnsSampleTime );
	//! ロック外から呼ぶこと
	void InvokeEventCallback();
	void RequestSample();

public:
	tTVPMFVideoSink( tTVPMFPlayer *owner, UINT32 width, UINT32 height );
	virtual ~tTVPMFVideoSink();

	// Methods of IUnknown
	DECLARE_IUNKNOWN
	STDMETHODIMP NonDelegatingQueryInterface( REFIID riid, void **ppv );

	// Methods of IMFMediaSink
	STDMETHODIMP GetCharacteristics( DWORD *pdwCharacteristics );
	STDMETHODIMP AddStreamSink( DWORD dwStreamSinkIdentifier, IMFMediaType *pMediaType, IMFStreamSink **ppStreamSink );
	STDMETHODIMP RemoveStreamSink( DWORD dwStreamSinkIdentifier );
	STDMETHODIMP GetStreamSinkCount( DWORD *pcStreamSinkCount );
	STDMETHODIMP GetStreamSinkByIndex( DWORD dwIndex, IMFStreamSink **ppStreamSink );
	STDMETHODIMP GetStreamSinkById( DWORD dwStreamSinkIdentifier, IMFStreamSink **ppStreamSink );
	STDMETHODIMP SetPresentationClock( IMFPresentationClock *pPresentationClock );
	STDMETHODIMP GetPresentationClock( IMFPresentationClock **ppPresentationClock );
	STDMETHODIMP Shutdown();

	// Methods of IMFStreamSink (IMFMediaEventGenerator を含む)
	STDMETHODIMP GetMediaSink( IMFMediaSink **ppMediaSink );
	STDMETHODIMP GetIdentifier( DWORD *pdwIdentifier );
	STDMETHODIMP GetMediaTypeHandler( IMFMediaTypeHandler **ppHandler );
	STDMETHODIMP ProcessSample( IMFSample *pSample );
	STDMETHODIMP PlaceMarker( MFSTREAMSINK_MARKER_TYPE eMarkerType, const PROPVARIANT *pvarMarkerValue, const PROPVARIANT *pvarContextValue );
	STDMETHODIMP Flush();

	STDMETHODIMP GetEvent( DWORD dwFlags, IMFMediaEvent **ppEvent );
	STDMETHODIMP BeginGetEvent( IMFAsyncCallback *pCallback, IUnknown *punkState );
	STDMETHODIMP EndGetEvent( IMFAsyncResult *pResult, IMFMediaEvent **ppEvent );
	STDMETHODIMP QueueEvent( MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT *pvValue );

	// Methods of IMFMediaTypeHandler
	STDMETHODIMP IsMediaTypeSupported( IMFMediaType *pMediaTypeIn, IMFMediaType **ppMediaTypeOut );
	STDMETHODIMP GetMediaTypeCount( DWORD *pdwTypeCount );
	STDMETHODIMP GetMediaTypeByIndex( DWORD dwIndex, IMFMediaType **ppType );
	STDMETHODIMP SetCurrentMediaType( IMFMediaType *pMediaType );
	STDMETHODIMP GetCurrentMediaType( IMFMediaType **ppMediaType );
	STDMETHODIMP GetMajorType( GUID *pguidMajorType );

	// Methods of IMFClockStateSink
	STDMETHODIMP OnClockStart( MFTIME hnsSystemTime, LONGLONG llClockStartOffset );
	STDMETHODIMP OnClockStop( MFTIME hnsSystemTime );
	STDMETHODIMP OnClockPause( MFTIME hnsSystemTime );
	STDMETHODIMP OnClockRestart( MFTIME hnsSystemTime );
	STDMETHODIMP OnClockSetRate( MFTIME hnsSystemTime, float flRate );

	void GetFrameSize( long *width, long *height );
};

#endif	// __MF_VIDEO_SINK_H__
