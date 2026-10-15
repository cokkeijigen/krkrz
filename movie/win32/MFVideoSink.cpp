/****************************************************************************/
/*! @file
@brief Media Foundation のデコード結果をエンジンのメモリバッファに書き出す Media Sink

	@see MFVideoSink.h

-----------------------------------------------------------------------------
*****************************************************************************/

#include "tjsCommHead.h"
#include "MsgIntf.h"

#include <windows.h>
#include <propvarutil.h>
#include <stdlib.h>

#include <Mfidl.h>
#include <mfapi.h>
#include <mferror.h>

#include <streams.h>
#include <atlbase.h>
#include <atlcom.h>

#include "MFVideoSink.h"
#include "MFPlayer.h"
#include "MFVideoColor.h"

//----------------------------------------------------------------------------
tTVPMFVideoSink::tTVPMFVideoSink( tTVPMFPlayer *owner, UINT32 width, UINT32 height )
 : CUnknown( L"tTVPMFVideoSink", NULL ),
   m_Owner( owner ),
   m_pEventCallback( NULL ),
   m_pEventState( NULL ),
   m_VideoWidth( (long)width ),
   m_VideoHeight( (long)height ),
   m_Stride( (long)(width * 4) ),
   m_StrideGuessed( false ),
   m_UseBT709( false ),
   m_InputSubtype( kMFVideoFormat_NV12 ),
   m_Shutdown( false ),
   m_SamplePumpStarted( false ),
   m_AnchorValid( false ),
   m_AnchorSampleTime( 0 ),
   m_AnchorTick( 0 )
{
}

tTVPMFVideoSink::~tTVPMFVideoSink()
{
	Shutdown();
}

//----------------------------------------------------------------------------
STDMETHODIMP tTVPMFVideoSink::Shutdown()
{
	CAutoLock lock( &m_Lock );
	if( m_Shutdown ) return S_OK;
	m_Shutdown = true;

	while( !m_Events.empty() ) {
		m_Events.front()->Release();
		m_Events.pop_front();
	}
	if( m_pEventCallback ) { m_pEventCallback->Release(); m_pEventCallback = NULL; }
	if( m_pEventState ) { m_pEventState->Release(); m_pEventState = NULL; }
	if( m_Clock.p ) {
		m_Clock->RemoveClockStateSink( this );
		m_Clock.Release();
	}
	return S_OK;
}

//----------------------------------------------------------------------------
void tTVPMFVideoSink::GetFrameSize( long *width, long *height )
{
	CAutoLock lock( &m_Lock );
	if( width ) *width = m_VideoWidth;
	if( height ) *height = m_VideoHeight;
}

//----------------------------------------------------------------------------
STDMETHODIMP tTVPMFVideoSink::NonDelegatingQueryInterface( REFIID riid, void **ppv )
{
	if( riid == IID_IMFMediaSink ) {
		return GetInterface( static_cast<IMFMediaSink*>(this), ppv );
	} else if( riid == IID_IMFStreamSink ) {
		return GetInterface( static_cast<IMFStreamSink*>(this), ppv );
	} else if( riid == IID_IMFMediaTypeHandler ) {
		return GetInterface( static_cast<IMFMediaTypeHandler*>(this), ppv );
	} else if( riid == IID_IMFClockStateSink ) {
		return GetInterface( static_cast<IMFClockStateSink*>(this), ppv );
	} else if( riid == IID_IMFMediaEventGenerator ) {
		return GetInterface( static_cast<IMFMediaEventGenerator*>(this), ppv );
	}
	return CUnknown::NonDelegatingQueryInterface( riid, ppv );
}

//----------------------------------------------------------------------------
// IMFMediaSink
//----------------------------------------------------------------------------
STDMETHODIMP tTVPMFVideoSink::GetCharacteristics( DWORD *pdwCharacteristics )
{
	if( pdwCharacteristics == NULL ) return E_POINTER;
	CAutoLock lock( &m_Lock );
	if( m_Shutdown ) return MF_E_SHUTDOWN;
	// ストリームは 1 本固定。MEDIASINK_CAN_PREROLL は付けない。付けるとセッションが
	// MEStreamSinkPrerolled を待つ間クロックを開始せず、OnClockStart が来ないまま停止する。
	*pdwCharacteristics = MEDIASINK_FIXED_STREAMS;
	return S_OK;
}

STDMETHODIMP tTVPMFVideoSink::AddStreamSink( DWORD, IMFMediaType *, IMFStreamSink ** )
{
	return MF_E_STREAMSINKS_FIXED;
}

STDMETHODIMP tTVPMFVideoSink::RemoveStreamSink( DWORD )
{
	return MF_E_STREAMSINKS_FIXED;
}

STDMETHODIMP tTVPMFVideoSink::GetStreamSinkCount( DWORD *pcStreamSinkCount )
{
	if( pcStreamSinkCount == NULL ) return E_POINTER;
	CAutoLock lock( &m_Lock );
	if( m_Shutdown ) return MF_E_SHUTDOWN;
	*pcStreamSinkCount = 1;
	return S_OK;
}

STDMETHODIMP tTVPMFVideoSink::GetStreamSinkByIndex( DWORD dwIndex, IMFStreamSink **ppStreamSink )
{
	if( dwIndex != 0 ) return MF_E_INVALIDSTREAMNUMBER;
	return GetStreamSinkById( 1, ppStreamSink );
}

STDMETHODIMP tTVPMFVideoSink::GetStreamSinkById( DWORD dwStreamSinkIdentifier, IMFStreamSink **ppStreamSink )
{
	if( ppStreamSink == NULL ) return E_POINTER;
	CAutoLock lock( &m_Lock );
	if( m_Shutdown ) return MF_E_SHUTDOWN;
	// MF_TOPONODE_STREAMID と一致させるため 1 を識別子にする (0 始まりは不可)
	if( dwStreamSinkIdentifier != 1 ) return MF_E_INVALIDSTREAMNUMBER;
	return GetInterface( static_cast<IMFStreamSink*>(this), (void**)ppStreamSink );
}

STDMETHODIMP tTVPMFVideoSink::SetPresentationClock( IMFPresentationClock *pPresentationClock )
{
	CAutoLock lock( &m_Lock );
	if( m_Shutdown ) return MF_E_SHUTDOWN;
	if( m_Clock.p ) {
		m_Clock->RemoveClockStateSink( this );
		m_Clock.Release();
	}
	if( pPresentationClock ) {
		HRESULT hr = pPresentationClock->AddClockStateSink( this );
		if( FAILED(hr) ) return hr;
		m_Clock = pPresentationClock;
	}
	return S_OK;
}

STDMETHODIMP tTVPMFVideoSink::GetPresentationClock( IMFPresentationClock **ppPresentationClock )
{
	if( ppPresentationClock == NULL ) return E_POINTER;
	CAutoLock lock( &m_Lock );
	if( m_Shutdown ) return MF_E_SHUTDOWN;
	if( m_Clock.p == NULL ) return MF_E_NO_CLOCK;
	return m_Clock.CopyTo( ppPresentationClock );
}

//----------------------------------------------------------------------------
// IMFStreamSink
//----------------------------------------------------------------------------
STDMETHODIMP tTVPMFVideoSink::GetMediaSink( IMFMediaSink **ppMediaSink )
{
	if( ppMediaSink == NULL ) return E_POINTER;
	CAutoLock lock( &m_Lock );
	if( m_Shutdown ) return MF_E_SHUTDOWN;
	return GetInterface( static_cast<IMFMediaSink*>(this), (void**)ppMediaSink );
}

STDMETHODIMP tTVPMFVideoSink::GetIdentifier( DWORD *pdwIdentifier )
{
	if( pdwIdentifier == NULL ) return E_POINTER;
	CAutoLock lock( &m_Lock );
	if( m_Shutdown ) return MF_E_SHUTDOWN;
	*pdwIdentifier = 1;
	return S_OK;
}

STDMETHODIMP tTVPMFVideoSink::GetMediaTypeHandler( IMFMediaTypeHandler **ppHandler )
{
	if( ppHandler == NULL ) return E_POINTER;
	CAutoLock lock( &m_Lock );
	if( m_Shutdown ) return MF_E_SHUTDOWN;
	return GetInterface( static_cast<IMFMediaTypeHandler*>(this), (void**)ppHandler );
}

//! 行を入れ替えて書き込む。エンジンの Bitmap は下から上なので dst が最終行を指す
STDMETHODIMP tTVPMFVideoSink::ProcessSample( IMFSample *pSample )
{
	if( pSample == NULL ) return E_POINTER;

	{
		CAutoLock lock( &m_Lock );
		if( m_Shutdown ) return MF_E_SHUTDOWN;
	}

	LONGLONG sampleTime = 0;
	pSample->GetSampleTime( &sampleTime );

	BYTE *dest = NULL;
	long  destSize = 0;
	if( !m_Owner->GetLayerBackBuffer( &dest, &destSize ) || dest == NULL ) {
		// SetVideoBuffer 前。要求は出し続けるが空回りしないよう少し待つ。
		::Sleep( 1 );
		RequestSample();
		return S_OK;
	}

	// デコーダは先行して吐き出すので実時間に合わせる
	WaitForPresentation( sampleTime );

	CComPtr<IMFMediaBuffer> pBuffer;
	if( FAILED( pSample->ConvertToContiguousBuffer( &pBuffer ) ) ) {
		RequestSample();
		return S_OK;	// 不連続バッファは扱わない
	}

	DWORD cbMaxLength = 0;
	DWORD cbCurrentLength = 0;
	BYTE *src = NULL;
	if( FAILED( pBuffer->Lock( &src, &cbMaxLength, &cbCurrentLength ) ) ) {
		RequestSample();
		return S_OK;
	}

	long width = 0, height = 0, stride = 0;
	GUID subtype = GUID_NULL;
	bool strideGuessed = false, bt709 = false;
	{
		CAutoLock lock( &m_Lock );
		width = m_VideoWidth;
		height = m_VideoHeight;
		stride = m_Stride;
		subtype = m_InputSubtype;
		strideGuessed = m_StrideGuessed;
		bt709 = m_UseBT709;
	}

	const size_t dstPitch = (size_t)width * 4;
	if( subtype == kMFVideoFormat_NV12 ) {
		size_t absStride = (size_t)( stride > 0 ? stride : -stride );
		if( strideGuessed && height > 0 && width > 0 ) {
			// DEFAULT_STRIDE をくれないデコーダが幅より広く揃えていると
			// stride=width のままだと横にずれて流れるのでバッファ長から逆算する
			const size_t implied = (size_t)cbCurrentLength * 2 / ( (size_t)height * 3 );
			if( implied > (size_t)width && implied <= (size_t)width + 256 &&
				implied * (size_t)height * 3 / 2 == (size_t)cbCurrentLength ) {
				absStride = implied;
			}
		}
		const size_t needed = absStride * (size_t)height * 3 / 2;
		if( src != NULL && absStride != 0 && width > 0 && height > 0 &&
			needed <= (size_t)destSize && needed <= (size_t)cbCurrentLength ) {
			TVPConvertNV12ToBGRAFlip( src, src + absStride * (size_t)height, (long)absStride,
				dest + dstPitch * (size_t)( height - 1 ), (long)dstPitch, width, height, stride < 0, bt709 );
		}
	} else {
		const size_t needed = dstPitch * (size_t)height;
		if( src != NULL && stride != 0 && width > 0 && height > 0 &&
			needed <= (size_t)destSize && needed <= (size_t)cbCurrentLength ) {
			const size_t absStride = (size_t)( stride > 0 ? stride : -stride );
			for( long y = 0; y < height; y++ ) {
				const BYTE *s = ( stride > 0 )
					? src + (size_t)y * absStride
					: src + (size_t)( height - 1 - y ) * absStride;
				BYTE *d = dest + (size_t)( height - 1 - y ) * dstPitch;
				memcpy( d, s, dstPitch );
			}
		}
	}

	pBuffer->Unlock();

	// 終端は PlaceMarker 側で通知するので、EC_UPDATE を先に投げておかないと
	// Status が ssStop になって最後のフレームが描かれないまま終わってしまう。
	m_Owner->PresentLayerFrame( sampleTime );

	RequestSample();
	return S_OK;
}

//----------------------------------------------------------------------------
//! @brief MEStreamSinkMarker を返さないとパイプラインが映像ストリームの終端を
//! 認識せず、エンジンへ vsEnded が届かない (動画が終わっても "stop" にならない)。
//----------------------------------------------------------------------------
STDMETHODIMP tTVPMFVideoSink::PlaceMarker( MFSTREAMSINK_MARKER_TYPE eMarkerType, const PROPVARIANT *pvarMarkerValue, const PROPVARIANT *pvarContextValue )
{
	{
		CAutoLock lock( &m_Lock );
		if( m_Shutdown ) return MF_E_SHUTDOWN;
	}

	if( eMarkerType != MFSTREAMSINK_MARKER_ENDOFSEGMENT ) {
		return QueueEvent( MEStreamSinkMarker, GUID_NULL, S_OK, pvarMarkerValue );
	}

	LONGLONG markerTime = 0;
	bool haveTime = false;
	if( pvarMarkerValue != NULL ) {
		if( pvarMarkerValue->vt == VT_I8 ) {
			markerTime = pvarMarkerValue->hVal.QuadPart;
			haveTime = true;
		} else if( pvarMarkerValue->vt == VT_UI8 ) {
			markerTime = (LONGLONG)pvarMarkerValue->uhVal.QuadPart;
			haveTime = true;
		}
	}
	if( haveTime ) WaitForPresentation( markerTime );

	HRESULT hr = QueueEvent( MEStreamSinkMarker, GUID_NULL, S_OK, pvarMarkerValue );

	// セグメント終端のマーカーはシーク直後にも来るので、提示時計がコンテンツ長に
	// 届いているかで「本当の終端」かどうかを判定する。
	{
		const MFTIME duration = m_Owner->GetDuration();
		MFTIME clockTime = 0;
		bool haveClock = false;
		{
			CAutoLock lock( &m_Lock );
			if( m_Clock.p != NULL && SUCCEEDED( m_Clock->GetTime( &clockTime ) ) ) haveClock = true;
		}
		// 時計が読めないときは判断材料が無いので終端とみなす
		const bool atEnd = ( duration <= 0 ) || !haveClock || ( clockTime >= duration - 150000 );
		if( atEnd ) {
			m_Owner->OnLayerEndOfStream();
		}
	}
	return hr;
}

STDMETHODIMP tTVPMFVideoSink::Flush()
{
	CAutoLock lock( &m_Lock );
	if( m_Shutdown ) return MF_E_SHUTDOWN;
	m_AnchorValid = false;
	return S_OK;
}

//----------------------------------------------------------------------------
// IMFMediaEventGenerator
//----------------------------------------------------------------------------
STDMETHODIMP tTVPMFVideoSink::GetEvent( DWORD dwFlags, IMFMediaEvent **ppEvent )
{
	if( ppEvent == NULL ) return E_POINTER;

	for( ;; ) {
		IMFMediaEvent *pEvent = NULL;
		{
			CAutoLock lock( &m_Lock );
			if( m_Shutdown ) return MF_E_SHUTDOWN;
			if( !m_Events.empty() ) {
				pEvent = m_Events.front();
				m_Events.pop_front();
			}
		}
		if( pEvent ) {
			*ppEvent = pEvent;
			return S_OK;
		}
		if( dwFlags & MF_EVENT_FLAG_NO_WAIT ) return MF_E_NO_EVENTS_AVAILABLE;
		::Sleep( 1 );
	}
}

STDMETHODIMP tTVPMFVideoSink::BeginGetEvent( IMFAsyncCallback *pCallback, IUnknown *punkState )
{
	if( pCallback == NULL ) return E_POINTER;

	bool invoke = false;
	{
		CAutoLock lock( &m_Lock );
		if( m_Shutdown ) return MF_E_SHUTDOWN;
		if( m_pEventCallback != NULL ) return MF_E_MULTIPLE_BEGIN;
		m_pEventCallback = pCallback;
		m_pEventCallback->AddRef();
		m_pEventState = punkState;
		if( punkState ) punkState->AddRef();
		invoke = !m_Events.empty();
	}
	if( invoke ) InvokeEventCallback();
	return S_OK;
}

STDMETHODIMP tTVPMFVideoSink::EndGetEvent( IMFAsyncResult *pResult, IMFMediaEvent **ppEvent )
{
	if( pResult == NULL || ppEvent == NULL ) return E_POINTER;
	CAutoLock lock( &m_Lock );
	if( m_Shutdown ) return MF_E_SHUTDOWN;

	if( m_pEventCallback ) { m_pEventCallback->Release(); m_pEventCallback = NULL; }
	if( m_pEventState ) { m_pEventState->Release(); m_pEventState = NULL; }

	if( m_Events.empty() ) return MF_E_NO_EVENTS_AVAILABLE;
	*ppEvent = m_Events.front();
	m_Events.pop_front();
	return S_OK;
}

STDMETHODIMP tTVPMFVideoSink::QueueEvent( MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT *pvValue )
{
	bool invoke = false;
	{
		CAutoLock lock( &m_Lock );
		if( m_Shutdown ) return MF_E_SHUTDOWN;

		if( met == MEStreamSinkRequestSample ) {
			// 読み手がいない可能性があるので溜め込まない
			for( size_t i = 0; i < m_Events.size(); i++ ) {
				MediaEventType t = MEUnknown;
				if( SUCCEEDED( m_Events[i]->GetType( &t ) ) && t == MEStreamSinkRequestSample ) {
					return S_OK;
				}
			}
		}

		CComPtr<IMFMediaEvent> pEvent;
		HRESULT hr = MFCreateMediaEvent( met, guidExtendedType, hrStatus, pvValue, &pEvent );
		if( FAILED(hr) ) return hr;

		// 読み手がいない場合の無限増加を防ぐ
		while( m_Events.size() >= 64 ) {
			m_Events.front()->Release();
			m_Events.pop_front();
		}
		m_Events.push_back( pEvent.Detach() );

		invoke = ( m_pEventCallback != NULL );
	}
	if( invoke ) InvokeEventCallback();
	return S_OK;
}

//----------------------------------------------------------------------------
// ロック外から呼ぶこと
//----------------------------------------------------------------------------
void tTVPMFVideoSink::InvokeEventCallback()
{
	IMFAsyncCallback *pCallback = NULL;
	IUnknown *punkState = NULL;
	{
		CAutoLock lock( &m_Lock );
		if( m_pEventCallback == NULL ) return;
		pCallback = m_pEventCallback;
		punkState = m_pEventState;
		m_pEventCallback = NULL;
		m_pEventState = NULL;
	}

	CComPtr<IMFAsyncResult> pResult;
	if( SUCCEEDED( MFCreateAsyncResult( NULL, pCallback, punkState, &pResult ) ) ) {
		pCallback->Invoke( pResult );
	}
	pCallback->Release();
	if( punkState ) punkState->Release();
}

//----------------------------------------------------------------------------
void tTVPMFVideoSink::RequestSample()
{
	QueueEvent( MEStreamSinkRequestSample, GUID_NULL, S_OK, NULL );
}

//----------------------------------------------------------------------------
// IMFMediaTypeHandler
//----------------------------------------------------------------------------
STDMETHODIMP tTVPMFVideoSink::GetMajorType( GUID *pguidMajorType )
{
	if( pguidMajorType == NULL ) return E_POINTER;
	*pguidMajorType = MFMediaType_Video;
	return S_OK;
}

STDMETHODIMP tTVPMFVideoSink::GetMediaTypeCount( DWORD *pdwTypeCount )
{
	if( pdwTypeCount == NULL ) return E_POINTER;
	*pdwTypeCount = 1;
	return S_OK;
}

//----------------------------------------------------------------------------
HRESULT tTVPMFVideoSink::CreateVideoType( UINT32 width, UINT32 height, IMFMediaType **ppType )
{
	CComPtr<IMFMediaType> pType;
	HRESULT hr = MFCreateMediaType( &pType );
	if( FAILED(hr) ) return hr;

	if( FAILED(hr = pType->SetGUID( MF_MT_MAJOR_TYPE, MFMediaType_Video ) ) ) return hr;
	// NV12 を直接要求する。RGB32 にするとトポロジローダがカラー変換 MFT を挟み、
	// 1440p で一桁 ms / フレーム掛かる (ソフトウェア Video Processor)。
	// デコーダの原生出力は NV12 なので、それをそのまま受けてこちらで変換する方が速い。
	if( FAILED(hr = pType->SetGUID( MF_MT_SUBTYPE, kMFVideoFormat_NV12 ) ) ) return hr;
	// ストライドはデコーダが決めるので DEFAULT_STRIDE は設定しない
	if( FAILED(hr = pType->SetUINT64( MF_MT_FRAME_SIZE, ( (UINT64)width << 32 ) | (UINT64)height ) ) ) return hr;
	if( FAILED(hr = pType->SetUINT64( MF_MT_PIXEL_ASPECT_RATIO, ( (UINT64)1 << 32 ) | (UINT64)1 ) ) ) return hr;
	if( FAILED(hr = pType->SetUINT32( MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive ) ) ) return hr;
	if( FAILED(hr = pType->SetUINT32( MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE ) ) ) return hr;

	*ppType = pType.Detach();
	return S_OK;
}

STDMETHODIMP tTVPMFVideoSink::GetMediaTypeByIndex( DWORD dwIndex, IMFMediaType **ppType )
{
	if( ppType == NULL ) return E_POINTER;
	if( dwIndex != 0 ) return MF_E_NO_MORE_TYPES;
	CAutoLock lock( &m_Lock );
	if( m_Shutdown ) return MF_E_SHUTDOWN;
	return CreateVideoType( (UINT32)m_VideoWidth, (UINT32)m_VideoHeight, ppType );
}

//----------------------------------------------------------------------------
STDMETHODIMP tTVPMFVideoSink::IsMediaTypeSupported( IMFMediaType *pMediaTypeIn, IMFMediaType **ppMediaTypeOut )
{
	if( pMediaTypeIn == NULL ) return E_POINTER;
	if( ppMediaTypeOut != NULL ) *ppMediaTypeOut = NULL;

	GUID major = GUID_NULL;
	if( FAILED( pMediaTypeIn->GetGUID( MF_MT_MAJOR_TYPE, &major ) ) || major != MFMediaType_Video ) {
		return MF_E_INVALIDMEDIATYPE;
	}

	CAutoLock lock( &m_Lock );
	if( m_Shutdown ) return MF_E_SHUTDOWN;

	const UINT64 wantSize = ( (UINT64)(UINT32)m_VideoWidth << 32 ) | (UINT64)(UINT32)m_VideoHeight;

	GUID subtype = GUID_NULL;
	UINT64 frameSize = 0;
	pMediaTypeIn->GetGUID( MF_MT_SUBTYPE, &subtype );
	pMediaTypeIn->GetUINT64( MF_MT_FRAME_SIZE, &frameSize );

	if( frameSize == wantSize &&
		( subtype == kMFVideoFormat_NV12 || subtype == MFVideoFormat_RGB32 ) ) {
		return S_OK;
	}

	// ここで S_OK を返すとトポロジローダがデコーダや色変換を挿入せず、
	// H264 などの圧縮データがそのまま ProcessSample に届いてしまう。
	return MF_E_INVALIDMEDIATYPE;
}

STDMETHODIMP tTVPMFVideoSink::SetCurrentMediaType( IMFMediaType *pMediaType )
{
	if( pMediaType == NULL ) return E_POINTER;

	CAutoLock lock( &m_Lock );
	if( m_Shutdown ) return MF_E_SHUTDOWN;

	GUID major = GUID_NULL;
	if( FAILED( pMediaType->GetGUID( MF_MT_MAJOR_TYPE, &major ) ) || major != MFMediaType_Video ) {
		return MF_E_INVALIDMEDIATYPE;
	}

	UINT64 frameSize = 0;
	if( SUCCEEDED( pMediaType->GetUINT64( MF_MT_FRAME_SIZE, &frameSize ) ) && frameSize != 0 ) {
		m_VideoWidth  = (long)(UINT32)( frameSize >> 32 );
		m_VideoHeight = (long)(UINT32)( frameSize & 0xffffffff );
	}

	UINT32 stride = 0;
	pMediaType->GetUINT32( MF_MT_DEFAULT_STRIDE, &stride );
	m_Stride = (long)(INT32)stride;
	m_StrideGuessed = ( m_Stride == 0 );
	if( m_StrideGuessed ) {
		// DEFAULT_STRIDE をくれないデコーダがいる
		GUID sub = GUID_NULL;
		pMediaType->GetGUID( MF_MT_SUBTYPE, &sub );
		m_Stride = ( sub == kMFVideoFormat_NV12 ) ? m_VideoWidth : m_VideoWidth * 4;
	}
	pMediaType->GetGUID( MF_MT_SUBTYPE, &m_InputSubtype );

	// マトリクスの明示が無ければ解像度で推定する (HD=709 / SD=601)。
	// MS の Video Processor も同じ推定をしていたので、これで従来の色に戻る
	m_UseBT709 = ( m_VideoWidth >= 1280 || m_VideoHeight >= 720 );

	UINT32 fpsNum = 0, fpsDen = 0;
	UINT64 frameRate = 0;
	if( SUCCEEDED( pMediaType->GetUINT64( MF_MT_FRAME_RATE, &frameRate ) ) ) {
		fpsNum = (UINT32)( frameRate >> 32 );
		fpsDen = (UINT32)( frameRate & 0xffffffff );
	}

	m_CurrentType = pMediaType;

	m_Owner->OnLayerVideoSizeChanged( m_VideoWidth, m_VideoHeight, fpsNum, fpsDen );

	// サンプル要求のポンプをここで起動しておく。OnClockStart を待つだけだと、
	// セッションがクロックを開始する前に止まったまま何も流れてこない。
	bool pump = false;
	{
		CAutoLock lock2( &m_Lock );
		if( !m_SamplePumpStarted ) { m_SamplePumpStarted = true; pump = true; }
	}
	if( pump ) RequestSample();
	return S_OK;
}

STDMETHODIMP tTVPMFVideoSink::GetCurrentMediaType( IMFMediaType **ppMediaType )
{
	if( ppMediaType == NULL ) return E_POINTER;
	CAutoLock lock( &m_Lock );
	if( m_Shutdown ) return MF_E_SHUTDOWN;
	if( m_CurrentType.p == NULL ) return MF_E_NOT_INITIALIZED;
	return m_CurrentType.CopyTo( ppMediaType );
}

//----------------------------------------------------------------------------
// IMFClockStateSink
//----------------------------------------------------------------------------
STDMETHODIMP tTVPMFVideoSink::OnClockStart( MFTIME /*hnsSystemTime*/, LONGLONG /*llClockStartOffset*/ )
{
	{
		CAutoLock lock( &m_Lock );
		if( m_Shutdown ) return MF_E_SHUTDOWN;
		m_AnchorValid = false;
	}
	QueueEvent( MEStreamSinkStarted, GUID_NULL, S_OK, NULL );
	RequestSample();
	return S_OK;
}

STDMETHODIMP tTVPMFVideoSink::OnClockStop( MFTIME /*hnsSystemTime*/ )
{
	{
		CAutoLock lock( &m_Lock );
		if( m_Shutdown ) return MF_E_SHUTDOWN;
		m_AnchorValid = false;
	}
	QueueEvent( MEStreamSinkStopped, GUID_NULL, S_OK, NULL );
	return S_OK;
}

STDMETHODIMP tTVPMFVideoSink::OnClockPause( MFTIME /*hnsSystemTime*/ )
{
	{
		CAutoLock lock( &m_Lock );
		if( m_Shutdown ) return MF_E_SHUTDOWN;
	}
	QueueEvent( MEStreamSinkPaused, GUID_NULL, S_OK, NULL );
	return S_OK;
}

STDMETHODIMP tTVPMFVideoSink::OnClockRestart( MFTIME /*hnsSystemTime*/ )
{
	{
		CAutoLock lock( &m_Lock );
		if( m_Shutdown ) return MF_E_SHUTDOWN;
		m_AnchorValid = false;
	}
	QueueEvent( MEStreamSinkStarted, GUID_NULL, S_OK, NULL );
	RequestSample();
	return S_OK;
}

STDMETHODIMP tTVPMFVideoSink::OnClockSetRate( MFTIME /*hnsSystemTime*/, float /*flRate*/ )
{
	return S_OK;
}

void tTVPMFVideoSink::WaitForPresentation( LONGLONG hnsSampleTime )
{
	MFTIME now = 0;
	bool haveClock = false;
	{
		CAutoLock lock( &m_Lock );
		if( m_Clock.p != NULL && SUCCEEDED( m_Clock->GetTime( &now ) ) ) {
			haveClock = true;
		}
	}

	if( haveClock ) {
		LONGLONG wait = (LONGLONG)hnsSampleTime - (LONGLONG)now;
		if( wait <= 0 ) return;
		if( wait > 2000000 ) wait = 2000000;	// 最大 200ms (シーク直後の暴走を防ぐ)
		::Sleep( (DWORD)( wait / 10000 ) );
		return;
	}

	// 時計が無いので実時刻で代用する
	CAutoLock lock( &m_Lock );
	const DWORD nowTick = ::GetTickCount();
	if( !m_AnchorValid ) {
		m_AnchorValid = true;
		m_AnchorSampleTime = hnsSampleTime;
		m_AnchorTick = nowTick;
		return;
	}
	const LONGLONG elapsedTick = (LONGLONG)(DWORD)( nowTick - m_AnchorTick );	// ms
	const LONGLONG dueTick = elapsedTick + ( ( hnsSampleTime - m_AnchorSampleTime ) / 10000 );
	if( dueTick > elapsedTick + 10 ) {
		::Sleep( (DWORD)( dueTick - elapsedTick ) );
	}
	if( elapsedTick - dueTick > 1000 ) {
		// 大きくずれたら基準を取り直す (一時停止やシークのあと)
		m_AnchorSampleTime = hnsSampleTime;
		m_AnchorTick = nowTick;
	}
}
