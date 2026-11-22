
#define NOMINMAX
#include "tjsCommHead.h"
#include "DrawDevice.h"
#include "BasicDrawDevice.h"
#include "LayerIntf.h"
#include "MsgIntf.h"
#include "SysInitIntf.h"
#include "WindowIntf.h"
#include "DebugIntf.h"
#include "ThreadIntf.h"
#include "ComplexRect.h"
#include "EventImpl.h"
#include "WindowImpl.h"
#include "PresentDiag.h"
#include "LayerBitmapIntf.h"
#include <math.h>
#include "GPUComposite.h"

// Steam オーバーレイ (Shift+Tab) の状態 (extension/steam_helper_tjs.cpp が更新)。
#include "steam_overlay_state.h"

#include <d3d9.h>
#include <mmsystem.h>
#include <algorithm>
#include <vector>

#ifdef __HAS_STEAM_API__
// Steam overlay は SteamAPI_Init 以降に生成された D3D デバイスしかフックできない。
// init 成功後に呼ばれ、既存デバイスを作り直してフック対象にする。
static std::vector<tTVPBasicDrawDevice*>& TVPGetBasicDrawDeviceInstances() {
	static std::vector<tTVPBasicDrawDevice*> instances;
	return instances;
}

void TVPSteamOverlayHookDevices() {
	tTVPBasicDrawDevice::RecreateForSteamOverlayHook();
}

// オーバーレイ表示中はビデオ子ウィンドウ (krmovie) を隠す。
// 子ウィンドウは常にクライアント領域より手前に合成されるため、そのままだと
// オーバーレイがビデオの裏に回って見えなくなる。閉じたら戻す。
static BOOL CALLBACK TVPEnumVisibleChildProc( HWND hwnd, LPARAM lp ) {
	std::vector<HWND>& v = *reinterpret_cast<std::vector<HWND>*>( lp );
	// 復元対象は「表示中だったもの」だけ (元々隠れていたものは戻さない)
	if( ::IsWindowVisible( hwnd ) ) v.push_back( hwnd );
	return TRUE;
}

void tTVPBasicDrawDevice::SteamSetMovieWindowsVisible( bool visible ) {
	static std::vector<HWND> hidden;	// 自分で隠した子ウィンドウ (復元用)
	if( visible ) {
		for( size_t i = 0; i < hidden.size(); i++ )
			if( ::IsWindow( hidden[i] ) ) ::ShowWindow( hidden[i], SW_SHOW );
		hidden.clear();
		return;
	}
	hidden.clear();
	for( size_t i = 0; i < TVPGetBasicDrawDeviceInstances().size(); i++ ) {
		tTVPBasicDrawDevice* dev = TVPGetBasicDrawDeviceInstances()[i];
		if( !dev->IsMainWindow || !dev->TargetWindow || !IsWindow( dev->TargetWindow ) ) continue;
		EnumChildWindows( dev->TargetWindow, (WNDENUMPROC)TVPEnumVisibleChildProc, reinterpret_cast<LPARAM>( &hidden ) );
	}
	for( size_t i = 0; i < hidden.size(); i++ ) ::ShowWindow( hidden[i], SW_HIDE );
}

void TVPSteamOverlaySetMovieWindowsVisible( bool visible ) {
	tTVPBasicDrawDevice::SteamSetMovieWindowsVisible( visible );
}
#endif

//---------------------------------------------------------------------------
// オプション
//---------------------------------------------------------------------------
static tjs_int TVPBasicDrawDeviceOptionsGeneration = 0;
bool TVPZoomInterpolation = true;
//---------------------------------------------------------------------------
static void TVPInitBasicDrawDeviceOptions()
{
	if(TVPBasicDrawDeviceOptionsGeneration == TVPGetCommandLineArgumentGeneration()) return;
	TVPBasicDrawDeviceOptionsGeneration = TVPGetCommandLineArgumentGeneration();

	tTJSVariant val;
	TVPZoomInterpolation = true;
	if(TVPGetCommandLine(TJS_W("-smoothzoom"), &val))
	{
		ttstr str(val);
		if(str == TJS_W("no"))
			TVPZoomInterpolation = false;
		else
			TVPZoomInterpolation = true;
	}
}
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// GPU 合成: テクスチャキャッシュを持つデバイスの登録簿
static std::vector<tTVPBasicDrawDevice*> TVPGPUDeviceRegistry;

bool TVPGPUCompositeDeviceOK()
{
	for(size_t i = 0; i < TVPGPUDeviceRegistry.size(); i++)
		if(TVPGPUDeviceRegistry[i]->IsGPUReady()) return true;
	return false;
}
//---------------------------------------------------------------------------
static void TVPGPUPurgeLayerCaches(void * layer)
{
	for(size_t i = 0; i < TVPGPUDeviceRegistry.size(); i++)
		TVPGPUDeviceRegistry[i]->PurgeLayerTexture(layer);
}
//---------------------------------------------------------------------------
// 直近に GPU 合成を完了した時刻 (ms)。Show の補完合成の間引きに使う
static double g_gpu_last_comp_time = 0.0;
static void TVPGPUMarkCompositeTime() { g_gpu_last_comp_time = PresentDiag::NowMs(); }
static double TVPGPULastCompositeTime() { return g_gpu_last_comp_time; }
//---------------------------------------------------------------------------
tTVPBasicDrawDevice::tTVPBasicDrawDevice()
{
	TVPInitBasicDrawDeviceOptions(); // read and initialize options
	TargetWindow = NULL;
	DrawUpdateRectangle = false;
	BackBufferDirty = true;

	Direct3D = NULL;
	Direct3DDevice = NULL;
	Texture = NULL;
	ShouldShow = false;
	TextureBuffer = NULL;
	TextureWidth = TextureHeight = 0;
	VsyncInterval = 16;
	LastFrameGPU = false;
	GPUFrameFailed = false;
	GPUTargetTex = NULL;
	GPUTargetSurf = NULL;
	GPUTargetW = GPUTargetH = 0;
	GPUScratchTex = NULL;
	GPUScratchSurf = NULL;
	RenderManager = NULL;
	ZeroMemory( &D3dPP, sizeof(D3dPP) );
	ZeroMemory( &DispMode, sizeof(DispMode) );
	TVPGPUDeviceRegistry.push_back(this);
	TVPGPUCompositeSetLayerPurge(&TVPGPUPurgeLayerCaches);
#ifdef __HAS_STEAM_API__
	TVPGetBasicDrawDeviceInstances().push_back(this);
#endif
}
//---------------------------------------------------------------------------
tTVPBasicDrawDevice::~tTVPBasicDrawDevice()
{
	for(size_t i = 0; i < TVPGPUDeviceRegistry.size(); i++)
	{
		if(TVPGPUDeviceRegistry[i] == this)
		{
			TVPGPUDeviceRegistry.erase(TVPGPUDeviceRegistry.begin() + i);
			break;
		}
	}
#ifdef __HAS_STEAM_API__
	auto& instances = TVPGetBasicDrawDeviceInstances();
	for( size_t i = 0; i < instances.size(); i++ ) {
		if( instances[i] == this ) {
			instances.erase( instances.begin() + i );
			break;
		}
	}
#endif
	DestroyD3DDevice();
}
//---------------------------------------------------------------------------
void tTVPBasicDrawDevice::DestroyD3DDevice() {
	ReleaseGPUTarget();
	ReleaseLayerTexCache();
	DestroyTexture();
	if(Direct3DDevice) Direct3DDevice->Release(), Direct3DDevice = NULL;
	if(Direct3D) Direct3D = NULL;
}
//---------------------------------------------------------------------------
void tTVPBasicDrawDevice::DestroyTexture() {
	if(TextureBuffer && Texture) Texture->UnlockRect(0), TextureBuffer = NULL;
	if(Texture) Texture->Release(), Texture = NULL;
}
//---------------------------------------------------------------------------
void tTVPBasicDrawDevice::InvalidateAll()
{
	// レイヤ演算結果をすべてリクエストする
	// サーフェースが lost した際に内容を再構築する目的で用いる
	RequestInvalidation(tTVPRect(0, 0, DestRect.get_width(), DestRect.get_height()));
}
//---------------------------------------------------------------------------
void tTVPBasicDrawDevice::CheckMonitorMoved() {
	UINT iCurrentMonitor = GetMonitorNumber( TargetWindow );
	if( CurrentMonitor != iCurrentMonitor ) {
		// モニタ移動が発生しているので、デバイスを再生成する
		CreateD3DDevice();
	}
}
//---------------------------------------------------------------------------
bool tTVPBasicDrawDevice::IsTargetWindowActive() const {
	if( TargetWindow == NULL ) return false;
	return ::GetForegroundWindow() == TargetWindow;
}
//---------------------------------------------------------------------------
bool tTVPBasicDrawDevice::GetDirect3D9Device() {
	DestroyD3DDevice();

	TVPEnsureDirect3DObject();

	if( NULL == ( Direct3D = TVPGetDirect3DObjectNoAddRef() ) )
		TVPThrowExceptionMessage( TVPFaildToCreateDirect3D );

	HRESULT hr;
	if( FAILED( hr = DecideD3DPresentParameters() ) ) {
		if( IsTargetWindowActive() ) {
			ErrorToLog( hr );
			TVPThrowExceptionMessage( TVPFaildToDecideBackbufferFormat );
		}
		return false;
	}

	UINT iCurrentMonitor = GetMonitorNumber( TargetWindow );
	DWORD	BehaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED;
	if( D3D_OK != ( hr = Direct3D->CreateDevice( iCurrentMonitor, D3DDEVTYPE_HAL, TargetWindow, BehaviorFlags, &D3dPP, &Direct3DDevice ) ) ) {
		if( IsTargetWindowActive() ) {
			ErrorToLog( hr );
			TVPThrowExceptionMessage( TVPFaildToCreateDirect3DDevice );
		}
		return false;
	}
	CurrentMonitor = iCurrentMonitor;
	BackBufferDirty = true;

	/*
	D3DVIEWPORT9 vp;
	vp.X  = DestLeft;
	vp.Y  = DestTop;
	vp.Width = DestWidth != 0 ? DestWidth : D3dPP.BackBufferWidth;
	vp.Height = DestHeight != 0 ? DestHeight : D3dPP.BackBufferHeight;
	*/
	
	D3DVIEWPORT9 vp;
	vp.X  = 0;
	vp.Y  = 0;
	vp.Width = D3dPP.BackBufferWidth;
	vp.Height = D3dPP.BackBufferHeight;
	vp.MinZ  = 0.0f;
	vp.MaxZ  = 1.0f;
	if( FAILED(hr = Direct3DDevice->SetViewport(&vp)) ) {
		if( IsTargetWindowActive() ) {
			ErrorToLog( hr );
			TVPThrowExceptionMessage( TVPFaildToSetViewport );
		}
		return false;
	}

	if( FAILED( hr = InitializeDirect3DState() ) ) {
		if( IsTargetWindowActive() ) {
			ErrorToLog( hr );
 			TVPThrowExceptionMessage( TVPFaildToSetRenderState );
		}
		return false;
	}

	int refreshrate = DispMode.RefreshRate;
	if( refreshrate == 0 ) {
		HDC hdc;
		hdc = ::GetDC(TargetWindow);
		refreshrate = GetDeviceCaps( hdc, VREFRESH );
		::ReleaseDC( TargetWindow, hdc );
	}
	VsyncInterval = 1000 / refreshrate;
	return true;
}
//---------------------------------------------------------------------------
HRESULT tTVPBasicDrawDevice::InitializeDirect3DState() {
	HRESULT	hr;
	D3DCAPS9	d3dcaps;
	if( FAILED( hr = Direct3DDevice->GetDeviceCaps( &d3dcaps ) ) )
		return hr;

	if( d3dcaps.TextureFilterCaps & D3DPTFILTERCAPS_MAGFLINEAR ) {
		if( FAILED( hr = Direct3DDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, TVPZoomInterpolation?D3DTEXF_LINEAR:D3DTEXF_POINT ) ) )
			return hr;
	} else {
		if( FAILED( hr = Direct3DDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT ) ) )
			return hr;
	}

	if( d3dcaps.TextureFilterCaps & D3DPTFILTERCAPS_MINFLINEAR ) {
		if( FAILED( hr = Direct3DDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, TVPZoomInterpolation?D3DTEXF_LINEAR:D3DTEXF_POINT ) ) )
			return hr;
	} else {
		if( FAILED( hr = Direct3DDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT ) ) )
		return hr;
	}

	if( FAILED( hr = Direct3DDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU,  D3DTADDRESS_CLAMP ) ) )
		return hr;
	if( FAILED( hr = Direct3DDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV,  D3DTADDRESS_CLAMP ) ) )
		return hr;
	if( FAILED( hr = Direct3DDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE ) ) )
		return hr;
	if( FAILED( hr = Direct3DDevice->SetRenderState( D3DRS_LIGHTING, FALSE ) ) )
		return hr;
	if( FAILED( hr = Direct3DDevice->SetRenderState( D3DRS_ZENABLE, FALSE ) ) )
		return hr;
	if( FAILED( hr = Direct3DDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE ) ) )
		return hr;
	if( FAILED( hr = Direct3DDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 ) ) )
		return hr;
	if( FAILED( hr = Direct3DDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE ) ) )
		return hr;
	if( FAILED( hr = Direct3DDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE ) ) )
		return hr;

	return S_OK;
}
//---------------------------------------------------------------------------
UINT tTVPBasicDrawDevice::GetMonitorNumber( HWND window )
{
	if( Direct3D == NULL || window == NULL ) return D3DADAPTER_DEFAULT;
	HMONITOR windowMonitor = ::MonitorFromWindow( window, MONITOR_DEFAULTTOPRIMARY );
	UINT iCurrentMonitor = 0;
	UINT numOfMonitor = Direct3D->GetAdapterCount();
	for( ; iCurrentMonitor < numOfMonitor; ++iCurrentMonitor ) 	{
		if( Direct3D->GetAdapterMonitor(iCurrentMonitor) == windowMonitor )
			break;
	}
	if( iCurrentMonitor == numOfMonitor )
		iCurrentMonitor = D3DADAPTER_DEFAULT;
	return iCurrentMonitor;
}
//---------------------------------------------------------------------------
HRESULT tTVPBasicDrawDevice::DecideD3DPresentParameters() {
	HRESULT			hr;
	UINT iCurrentMonitor = GetMonitorNumber(TargetWindow);
	if( FAILED( hr = Direct3D->GetAdapterDisplayMode( iCurrentMonitor, &DispMode ) ) )
		return hr;

	ZeroMemory( &D3dPP, sizeof(D3dPP) );
	D3dPP.Windowed = TRUE;
	D3dPP.SwapEffect = D3DSWAPEFFECT_COPY;
	D3dPP.BackBufferCount = 1;
	D3dPP.BackBufferFormat = D3DFMT_UNKNOWN;

	// バックバッファはクライアント領域と同じサイズにする
	// (大きいと Steam オーバーレイの座標系がずれるため。 DestRect を下限、
	//  不正値はディスプレイモードにフォールバック)
	tjs_int cw = 0, ch = 0;
	RECT client = { 0, 0, 0, 0 };
	if( TargetWindow != NULL && ::GetClientRect( TargetWindow, &client ) ) {
		cw = client.right - client.left;
		ch = client.bottom - client.top;
	}
	// クライアント領域より DestRect が大きい場合はそれを下限とする
	if( cw < (tjs_int)DestRect.get_width()  ) cw = (tjs_int)DestRect.get_width();
	if( ch < (tjs_int)DestRect.get_height() ) ch = (tjs_int)DestRect.get_height();
	// それでも不正な場合はディスプレイモードにフォールバック
	if( cw <= 0 ) cw = (tjs_int)DispMode.Width;
	if( ch <= 0 ) ch = (tjs_int)DispMode.Height;

	D3dPP.BackBufferWidth  = (UINT)cw;
	D3dPP.BackBufferHeight = (UINT)ch;
	D3dPP.hDeviceWindow = TargetWindow;
	D3dPP.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

	return S_OK;
}
//---------------------------------------------------------------------------
bool tTVPBasicDrawDevice::CreateD3DDevice()
{
	// Direct3D デバイス、テクスチャなどを作成する
	DestroyD3DDevice();
	if( TargetWindow ) {
		tjs_int w, h;
		GetSrcSize( w, h );
		if( w > 0 && h > 0 ) {
			// get Direct3D9 interface
			if( GetDirect3D9Device() ) {
				return CreateTexture();
			}
		}
	}
	return false;
}
//---------------------------------------------------------------------------
bool tTVPBasicDrawDevice::CreateTexture() {
	DestroyTexture();
	tjs_int w, h;
	GetSrcSize( w, h );
	if(TargetWindow && w > 0 && h > 0) {
		HRESULT hr = S_OK;

		D3DCAPS9 d3dcaps;
		Direct3DDevice->GetDeviceCaps( &d3dcaps );

		TextureWidth = w;
		TextureHeight = h;
		if( d3dcaps.TextureCaps & D3DPTEXTURECAPS_SQUAREONLY) {
			// only square textures are supported
			TextureWidth = std::max(TextureHeight, TextureWidth);
			TextureHeight = TextureWidth;
		}

		DWORD dwWidth = 64;
		DWORD dwHeight = 64;
		if( d3dcaps.TextureCaps & D3DPTEXTURECAPS_POW2 ) {
			// 2の累乗のみ許可するかどうか判定
			while( dwWidth < TextureWidth ) dwWidth = dwWidth << 1;
			while( dwHeight < TextureHeight ) dwHeight = dwHeight << 1;
			TextureWidth = dwWidth;
			TextureHeight = dwHeight;

			if( dwWidth > d3dcaps.MaxTextureWidth || dwHeight > d3dcaps.MaxTextureHeight ) {
				TVPAddLog( (const tjs_char*)TVPWarningImageSizeTooLargeMayBeCannotCreateTexture );
			}
			TVPAddLog( (const tjs_char*)TVPUsePowerOfTwoSurface );
		} else {
			dwWidth = TextureWidth;
			dwHeight = TextureHeight;
		}

		if( D3D_OK != ( hr = Direct3DDevice->CreateTexture( dwWidth, dwHeight, 1, D3DUSAGE_DYNAMIC, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &Texture, NULL) ) ) {
			if( IsTargetWindowActive() ) {
				ErrorToLog( hr );
				TVPThrowExceptionMessage(TVPCannotAllocateD3DOffScreenSurface,TJSInt32ToHex(hr, 8));
			}
			return false;
		}
	}
	return true;
}
//---------------------------------------------------------------------------
#ifdef __HAS_STEAM_API__
// main window 表示後にデバイスを作り直して overlay をフックさせる。
// Show() は vsync スレッドから呼ばれるため、実際の再構築はメインスレッド
// (連続イベント) で行う。
class tTVPSteamOverlayHookInvoker : public tTVPContinuousEventCallbackIntf
{
public:
	void TJS_INTF_METHOD OnContinuousCallback(tjs_uint64 tick) override {
		TVPRemoveContinuousEventHook(this);
		tTVPBasicDrawDevice::RecreateForSteamOverlayHook();
	}
} static TVPSteamOverlayHookInvoker;

void tTVPBasicDrawDevice::RecreateForSteamOverlayHook()
{
	// 対象は main window のデバイスのみ (他は overlay を介しない)。
	// 窓が既に壊れている場合は触らない。
	for( size_t i = 0; i < TVPGetBasicDrawDeviceInstances().size(); i++ ) {
		tTVPBasicDrawDevice* dev = TVPGetBasicDrawDeviceInstances()[i];
		if( !dev->IsMainWindow || !dev->TargetWindow || !IsWindow( dev->TargetWindow ) ) continue;
		dev->DestroyD3DDevice();
		TVPReleaseDirect3DObjectForSteamOverlay();
		// キャッシュ済み IDirect3D9 (init 前生成 = 未フック) を捨てたうえで
		// 再生成させる。失敗しても Show() の自己修復で再試行される。
		dev->EnsureDevice();
	}
}
#endif
//---------------------------------------------------------------------------
void tTVPBasicDrawDevice::EnsureDevice()
{
	TVPInitBasicDrawDeviceOptions();
	if( TargetWindow ) {
		try {
			bool recreate = false;
			if( Direct3D == NULL || Direct3DDevice == NULL ) {
				if( GetDirect3D9Device() == false ) {
					return;
				}
				recreate = true;
			}
			if( Texture == NULL ) {
				if( CreateTexture() == false ) {
					return;
				}
				recreate = true;
			}
			if( recreate ) {
				InvalidateAll();
			}
		} catch(const eTJS & e) {
			TVPAddImportantLog( TVPFormatMessage(TVPBasicDrawDeviceFailedToCreateDirect3DDevice,e.GetMessage()) );
			DestroyD3DDevice();
		} catch(...) {
			TVPAddImportantLog( (const tjs_char*)TVPBasicDrawDeviceFailedToCreateDirect3DDeviceUnknownReason );
			DestroyD3DDevice();
		}
	}
}
//---------------------------------------------------------------------------
void tTVPBasicDrawDevice::TryRecreateWhenDeviceLost()
{
	bool success = false;
	if( Direct3DDevice ) {
		DestroyTexture();
		HRESULT hr = Direct3DDevice->TestCooperativeLevel();
		if( hr == D3DERR_DEVICENOTRESET ) {
			hr = Direct3DDevice->Reset(&D3dPP);
		}
		if( hr == D3DERR_DEVICELOST ) {
			// 復帰できない
			return;
		}
		if( FAILED(hr) ) {
			success = CreateD3DDevice();
		} else {
			if( D3D_OK == InitializeDirect3DState() ) {
				success = true;
			}
		}
	} else {
		success = CreateD3DDevice();
	}
	if( success ) {
		InvalidateAll();	// 画像の再描画(Layer Update)を要求する
	}
}
//---------------------------------------------------------------------------
void tTVPBasicDrawDevice::ErrorToLog( HRESULT hr ) {
	switch( hr ) {
	case D3DERR_DEVICELOST:
		TVPAddLog( (const tjs_char*)TVPD3dErrDeviceLost );
		break;
	case D3DERR_DRIVERINTERNALERROR:
		TVPAddLog( (const tjs_char*)TVPD3dErrDriverIinternalError );
		break;
	case D3DERR_INVALIDCALL:
		TVPAddLog( (const tjs_char*)TVPD3dErrInvalidCall );
		break;
	case D3DERR_OUTOFVIDEOMEMORY:
		TVPAddLog( (const tjs_char*)TVPD3dErrOutOfVideoMemory );
		break;
	case E_OUTOFMEMORY:
		TVPAddLog( (const tjs_char*)TVPD3dErrOutOfMemory );
		break;
	case D3DERR_WRONGTEXTUREFORMAT:
		TVPAddLog( (const tjs_char*)TVPD3dErrWrongTextureFormat );
		break;
	case D3DERR_UNSUPPORTEDCOLOROPERATION:
		TVPAddLog( (const tjs_char*)TVPD3dErrUnsuportedColorOperation );
		break;
	case D3DERR_UNSUPPORTEDCOLORARG:
		TVPAddLog( (const tjs_char*)TVPD3dErrUnsuportedColorArg );
		break;
	case D3DERR_UNSUPPORTEDALPHAOPERATION:
		TVPAddLog( (const tjs_char*)TVPD3dErrUnsuportedAalphtOperation );
		break;
	case D3DERR_UNSUPPORTEDALPHAARG:
		TVPAddLog( (const tjs_char*)TVPD3dErrUnsuportedAlphaArg );
		break;
	case D3DERR_TOOMANYOPERATIONS:
		TVPAddLog( (const tjs_char*)TVPD3dErrTooManyOperations );
		break;
	case D3DERR_CONFLICTINGTEXTUREFILTER:
		TVPAddLog( (const tjs_char*)TVPD3dErrConflictioningTextureFilter );
		break;
	case D3DERR_UNSUPPORTEDFACTORVALUE:
		TVPAddLog( (const tjs_char*)TVPD3dErrUnsuportedFactorValue );
		break;
	case D3DERR_CONFLICTINGRENDERSTATE:
		TVPAddLog( (const tjs_char*)TVPD3dErrConflictioningRenderState );
		break;
	case D3DERR_UNSUPPORTEDTEXTUREFILTER:
		TVPAddLog( (const tjs_char*)TVPD3dErrUnsupportedTextureFilter );
		break;
	case D3DERR_CONFLICTINGTEXTUREPALETTE:
		TVPAddLog( (const tjs_char*)TVPD3dErrConflictioningTexturePalette );
		break;
	case D3DERR_NOTFOUND:
		TVPAddLog( (const tjs_char*)TVPD3dErrNotFound );
		break;
	case D3DERR_MOREDATA:
		TVPAddLog( (const tjs_char*)TVPD3dErrMoreData );
		break;
	case D3DERR_DEVICENOTRESET:
		TVPAddLog( (const tjs_char*)TVPD3dErrDeviceNotReset );
		break;
	case D3DERR_NOTAVAILABLE:
		TVPAddLog( (const tjs_char*)TVPD3dErrNotAvailable );
		break;
	case D3DERR_INVALIDDEVICE:
		TVPAddLog( (const tjs_char*)TVPD3dErrInvalidDevice );
		break;
	case D3DERR_DRIVERINVALIDCALL:
		TVPAddLog( (const tjs_char*)TVPD3dErrDriverInvalidCall );
		break;
	case D3DERR_WASSTILLDRAWING:
		TVPAddLog( (const tjs_char*)TVPD3dErrWasStillDrawing );
		break;
	case D3DERR_DEVICEHUNG:
		TVPAddLog( (const tjs_char*)TVPD3dErrDeviceHung );
		break;
	case D3DERR_UNSUPPORTEDOVERLAY:
		TVPAddLog( (const tjs_char*)TVPD3dErrUnsupportedOverlay );
		break;
	case D3DERR_UNSUPPORTEDOVERLAYFORMAT:
		TVPAddLog( (const tjs_char*)TVPD3dErrUnsupportedOverlayFormat );
		break;
	case D3DERR_CANNOTPROTECTCONTENT:
		TVPAddLog( (const tjs_char*)TVPD3dErrCannotProtectContent );
		break;
	case D3DERR_UNSUPPORTEDCRYPTO:
		TVPAddLog( (const tjs_char*)TVPD3dErrUnsupportedCrypto );
		break;
	case D3DERR_PRESENT_STATISTICS_DISJOINT:
		TVPAddLog( (const tjs_char*)TVPD3dErrPresentStatisticsDisJoint );
		break;
	case D3DERR_DEVICEREMOVED:
		TVPAddLog( (const tjs_char*)TVPD3dErrDeviceRemoved );
		break;
	case D3D_OK:
		break;
	case D3DOK_NOAUTOGEN:
		TVPAddLog( (const tjs_char*)TVPD3dOkNoAutoGen );
		break;
	case E_FAIL:
		TVPAddLog( (const tjs_char*)TVPD3dErrFail );
		break;
	case E_INVALIDARG:
		TVPAddLog( (const tjs_char*)TVPD3dErrInvalidArg );
		break;
	default:
		TVPAddLog( (const tjs_char*)TVPD3dUnknownError );
		break;
	}
}
//---------------------------------------------------------------------------
void TJS_INTF_METHOD tTVPBasicDrawDevice::AddLayerManager(iTVPLayerManager * manager)
{
	if(inherited::Managers.size() > 0)
	{
		// "Basic" デバイスでは２つ以上のLayer Managerを登録できない
		TVPThrowExceptionMessage(TVPBasicDrawDeviceDoesNotSupporteLayerManagerMoreThanOne);
	}
	inherited::AddLayerManager(manager);

	manager->SetDesiredLayerType(ltOpaque); // ltOpaque な出力を受け取りたい
}
//---------------------------------------------------------------------------
void TJS_INTF_METHOD tTVPBasicDrawDevice::SetTargetWindow(HWND wnd, bool is_main)
{
	TVPInitBasicDrawDeviceOptions();
	DestroyD3DDevice();
	TargetWindow = wnd;
	IsMainWindow = is_main;
}
//---------------------------------------------------------------------------
void TJS_INTF_METHOD tTVPBasicDrawDevice::SetDestRectangle(const tTVPRect & rect)
{
	BackBufferDirty = true;
	// 位置だけの変更の場合かどうかをチェックする
	if(rect.get_width() == DestRect.get_width() && rect.get_height() == DestRect.get_height()) {
		// 位置だけの変更だ
		inherited::SetDestRectangle(rect);
	} else {
		// サイズも違う
		// バックバッファがクライアント領域と合わない場合は作り直す
		// (縮小時に残ると Steam オーバーレイがずれる)
		if( rect.get_width() != (tjs_int)D3dPP.BackBufferWidth || rect.get_height() != (tjs_int)D3dPP.BackBufferHeight ) {
			DestroyD3DDevice();
		}
		bool success = true;
		inherited::SetDestRectangle(rect);

		try {
			EnsureDevice();
		} catch(const eTJS & e) {
			TVPAddImportantLog( TVPFormatMessage(TVPBasicDrawDeviceFailedToCreateDirect3DDevices,e.GetMessage() ) );
			success = false;
		} catch(...) {
			TVPAddImportantLog( (const tjs_char*)TVPBasicDrawDeviceFailedToCreateDirect3DDevicesUnknownReason );
			success = false;
		}
		if( success == false ) {
			DestroyD3DDevice();
		}
	}
}
//---------------------------------------------------------------------------
void TJS_INTF_METHOD tTVPBasicDrawDevice::NotifyLayerResize(iTVPLayerManager * manager)
{
	inherited::NotifyLayerResize(manager);

	BackBufferDirty = true;

	// テクスチャを捨てて作り直す。
	CreateTexture();
}
//---------------------------------------------------------------------------
void TJS_INTF_METHOD tTVPBasicDrawDevice::Show()
{
	if(!TargetWindow) return;

	// GPU 合成は位置を実時間基準で進めるので、毎フレーム 1 回は合成する。
	// ただし無条件に呼ぶとエンジン側の合成と重なり「4ms 後に 2 回目 →
	// 12ms 空白」というムラが出て、動きがガタつく。直近の合成から
	// 12ms 以上経っているときだけ補う
	if(TVPGPUCompositeMode != 0 && !ShouldShow &&
		PresentDiag::NowMs() - TVPGPULastCompositeTime() >= 12.0)
		Update();
#ifdef __HAS_STEAM_API__
	// Steam overlay のフックは main window が実際に表示されてから行う
	// (init 直後に起動オプション画面へ出さないため)。実処理はメインスレッドへ。
	if( TVPSteamOverlayNeedsHook && IsMainWindow &&
		TargetWindow && IsWindowVisible( TargetWindow ) ) {
		TVPSteamOverlayNeedsHook = false;
		TVPAddContinuousEventHook( &TVPSteamOverlayHookInvoker );
		TVPProcessContinuousHandlerEventFlag = true;
	}
#endif
	if(!Texture) return;
	if(!Direct3DDevice) return;

	// Steam オーバーレイは Present hook で合成するため、静止画でも
	// 毎フレーム Present する必要がある (閉じる過程でも必要)。
	#ifdef __HAS_STEAM_API__
	const bool steamOverlayPresent = TVPSteamOverlayAvailable || TVPSteamOverlayActive;
	#else
	const bool steamOverlayPresent = false;	// 非 Steam ビルド (従来動作)
	#endif
	if(!ShouldShow && !steamOverlayPresent) {
		return;
	}

	// このフレームでゲームが合成したかどうか (ShouldShow=true なら合成済み)
	const bool gameRenderedThisFrame = ShouldShow;
	ShouldShow = false;

	// 静止画の強制 Present 前に Texture を再転送し、黒画面を防ぐ
	if( steamOverlayPresent && !gameRenderedThisFrame ) {
		RedrawBackBufferFromTexture();
	}

	HRESULT hr = D3D_OK;
	RECT client;
	if( ::GetClientRect( TargetWindow, &client ) ) {
		// 自己修復: バックバッファが「あるべきサイズ」(DecideD3DPresentParameters
		// と同じ max(クライアント, DestRect) 式。 生のクライアント領域と比べると
		// 永遠に一致せず毎フレーム再構築→描画凍結を起こす) と違えば作り直す
		LONG expectW = client.right - client.left;
		LONG expectH = client.bottom - client.top;
		if( expectW < (LONG)DestRect.get_width()  ) expectW = (LONG)DestRect.get_width();
		if( expectH < (LONG)DestRect.get_height() ) expectH = (LONG)DestRect.get_height();
		if( expectW > 0 && expectH > 0 &&
			( (LONG)D3dPP.BackBufferWidth  != expectW ||
			  (LONG)D3dPP.BackBufferHeight != expectH ) ) {
			DestroyD3DDevice();
			EnsureDevice();		// 新しいサイズで再作成 (+InvalidateAll)
			ShouldShow = false;
			return;				// 次フレームから通常どおり Present される
		}

		RECT drect;
		drect.left   = 0;
		drect.top    = 0;
		drect.right  = client.right - client.left;
		drect.bottom = client.bottom - client.top;

		RECT srect = drect;

		// デバッグ: Present 直前のバックバッファを書き出す (CPU/GPU 共通の比較用)
		{
			static int bbdump = -1;
			static int bbcount = 0;
			static double bb_t0 = -1;
			static char bbpath[560] = {0};
			if(bbdump < 0)
			{
				bbdump = ::GetEnvironmentVariableA("TVP_PRESENT_BBDUMP", bbpath, 540) > 0 ? 1 : 0;
				bb_t0 = PresentDiag::NowMs();
			}
			// 起動から 32 秒後 (= ED 再生中) に連続 40 フレーム書き出す
			if(bbdump && bbcount < 8 &&
				PresentDiag::NowMs() - bb_t0 > 30000.0 + (double)bbcount * 2000.0)
			{
				bbcount++;
				char full[600];
				sprintf_s(full, "%s_%02d_%.0f.raw", bbpath, bbcount, PresentDiag::NowMs());
				IDirect3DSurface9 * bb = NULL;
				if(SUCCEEDED(Direct3DDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)))
				{
					D3DSURFACE_DESC desc;
					bb->GetDesc(&desc);
					IDirect3DSurface9 * sys = NULL;
					if(SUCCEEDED(Direct3DDevice->CreateOffscreenPlainSurface(
						desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &sys, NULL)))
					{
						if(SUCCEEDED(Direct3DDevice->GetRenderTargetData(bb, sys)))
						{
							D3DLOCKED_RECT lr;
							if(SUCCEEDED(sys->LockRect(&lr, NULL, D3DLOCK_READONLY)))
							{
								FILE * fp = fopen(full, "wb");
								if(fp)
								{
									for(UINT y = 0; y < desc.Height; y++)
										fwrite((tjs_uint8*)lr.pBits + (size_t)lr.Pitch * y, 1, (size_t)desc.Width * 4, fp);
									fclose(fp);
								}
								sys->UnlockRect();
							}
						}
						sys->Release();
					}
					bb->Release();
				}
			}
		}

		// デバッグ: 連続フレームを 1/3 縮小で書き出す (ちらつきのフレーム単位追跡用)
		// TVP_BBDUMP_SEQ="start_ms,count" (start は最初の Present からの経過)
		{
			static int sq_start = -2, sq_left = 0, sq_idx = 0;
			static char sq_path[560] = {0};
			static double sq_t0 = -1.0;
			if(sq_start == -2)
			{
				char s[64] = {0};
				sq_start = -1;
				if(::GetEnvironmentVariableA("TVP_BBDUMP_SEQ", s, 60) > 0)
				{
					int a = 0, b = 0;
					if(sscanf_s(s, "%d,%d", &a, &b) == 2) { sq_start = a; sq_left = b; }
				}
				if(::GetEnvironmentVariableA("TVP_PRESENT_BBDUMP", sq_path, 540) <= 0) sq_path[0] = 0;
				sq_t0 = PresentDiag::NowMs();
			}
			if(sq_start >= 0 && sq_left > 0 && sq_path[0] &&
				PresentDiag::NowMs() - sq_t0 >= (double)sq_start)
			{
				sq_left--;
				IDirect3DSurface9 * bb = NULL;
				if(SUCCEEDED(Direct3DDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)))
				{
					D3DSURFACE_DESC desc;
					bb->GetDesc(&desc);
					IDirect3DSurface9 * sys = NULL;
					if(SUCCEEDED(Direct3DDevice->CreateOffscreenPlainSurface(
						desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &sys, NULL)))
					{
						if(SUCCEEDED(Direct3DDevice->GetRenderTargetData(bb, sys)))
						{
							D3DLOCKED_RECT lr;
							if(SUCCEEDED(sys->LockRect(&lr, NULL, D3DLOCK_READONLY)))
							{
								char full[600];
								sprintf_s(full, "%s_s%04d_%.0f.raw", sq_path, sq_idx++, PresentDiag::NowMs());
								FILE * fp = fopen(full, "wb");
								if(fp)
								{
									// 領域指定があればそこだけ等倍で書き出す
									// (文字の亜像素の跳び/シャープネス揺れを測る用)
									static int rx0 = -1, ry0 = 0, rw = 0, rh = 0;
									if(rx0 == -1)
									{
										char r[64] = {0};
										rx0 = 0;
										if(::GetEnvironmentVariableA("TVP_BBDUMP_REGION", r, 60) > 0)
										{
											if(sscanf_s(r, "%d,%d,%d,%d", &rx0, &ry0, &rw, &rh) != 4)
												rx0 = 0, ry0 = 0, rw = 0, rh = 0;
										}
									}
									const bool region = (rw > 0 && rh > 0);
									const int x0 = region ? rx0 : 0;
									const int y0 = region ? ry0 : 0;
									const int x1 = region ? rx0 + rw : (int)desc.Width;
									const int y1 = region ? ry0 + rh : (int)desc.Height;
									const int step = region ? 1 : 3;
									for(int y = y0; y < y1 && y < (int)desc.Height; y += step)
									{
										const tjs_uint8 * src = (tjs_uint8*)lr.pBits + (size_t)lr.Pitch * y;
										for(int x = x0; x < x1 && x < (int)desc.Width; x += step)
											fwrite(src + (size_t)x * 4, 1, 4, fp);
									}
									fclose(fp);
								}
								sys->UnlockRect();
							}
						}
						sys->Release();
					}
					bb->Release();
				}
			}
		}

		{
			double t0 = PresentDiag::NowMs();
			hr = Direct3DDevice->Present( &srect, &drect, TargetWindow, NULL );
			PresentDiag::Emit("SHOW", t0, "ms=%.3f rendered=%d hr=%08x",
				PresentDiag::NowMs() - t0, gameRenderedThisFrame ? 1 : 0, hr);
		}
		// 亜像素平滑の基準時計は「実際に画面へ出した回数」で進める。
		// 合成回数 (1 フレームに 2 回あり得る) を基準にすると進み量がムラになる
		TVPGPUCompositeFrameNo++;
		// DISCARD では Present 後にバックバッファの内容が無効になるので次回描画前に消す
		BackBufferDirty = true;
	} else {
		ShouldShow = true;
	}

	if(hr == D3DERR_DEVICELOST) {
		if( IsTargetWindowActive() ) ErrorToLog( hr );
		TryRecreateWhenDeviceLost();
	} else if(hr != D3D_OK) {
		ErrorToLog( hr );
		TVPAddImportantLog( TVPFormatMessage(TVPBasicDrawDeviceInfDirect3DDevicePresentFailed,TJSInt32ToHex(hr, 8)) );
	}
}
//---------------------------------------------------------------------------
bool TJS_INTF_METHOD tTVPBasicDrawDevice::WaitForVBlank( tjs_int* in_vblank, tjs_int* delayed )
{
	if( Direct3DDevice == NULL ) return false;

	bool inVsync = false;
	D3DRASTER_STATUS rs;
	if( D3D_OK == Direct3DDevice->GetRasterStatus(0,&rs) ) {
		inVsync = rs.InVBlank == TRUE;
	}

	// VSync 待ちを行う
	// 予算を約半フレームに広げ、タイマが少し早く起きても立ち上がりを捕まえられるようにする
	bool isdelayed = false;
	if(!inVsync) {
		// vblank から抜けるまで待つ
		DWORD timeout_target_tick = ::timeGetTime() + 8;
		rs.InVBlank = FALSE;
		HRESULT hr = D3D_OK;
		do {
			hr = Direct3DDevice->GetRasterStatus(0,&rs);
		} while( D3D_OK == hr && rs.InVBlank == TRUE && (long)(::timeGetTime() - timeout_target_tick) <= 0);

		// vblank に入るまで待つ
		rs.InVBlank = TRUE;
		do {
			hr = Direct3DDevice->GetRasterStatus(0,&rs);
		} while( D3D_OK == hr && rs.InVBlank == FALSE && (long)(::timeGetTime() - timeout_target_tick) <= 0);

		if((int)(::timeGetTime() - timeout_target_tick) > 0) {
			// フレームスキップが発生したと考えてよい
			isdelayed  = true;
		}
		inVsync = rs.InVBlank == TRUE;
	}
	*delayed = isdelayed ? 1 : 0;
	*in_vblank = inVsync ? 1 : 0;
	return true;
}
//---------------------------------------------------------------------------
void TJS_INTF_METHOD tTVPBasicDrawDevice::StartBitmapCompletion(iTVPLayerManager * manager)
{
	EnsureDevice();

	if( Texture && TargetWindow ) {
		if(TextureBuffer) {
			TVPAddImportantLog( (const tjs_char*)TVPBasicDrawDeviceTextureHasAlreadyBeenLocked );
			Texture->UnlockRect(0), TextureBuffer = NULL;
		}

		D3DLOCKED_RECT rt;
		HRESULT hr = Texture->LockRect( 0, &rt, NULL, D3DLOCK_NO_DIRTY_UPDATE );

		if(hr == D3DERR_INVALIDCALL && IsTargetWindowActive() ) {
			TVPThrowExceptionMessage( TVPInternalErrorResult, TJSInt32ToHex(hr, 8));
		}

		if(hr != D3D_OK) {
			ErrorToLog( hr );
			TextureBuffer = NULL;
			TryRecreateWhenDeviceLost();
		} else /*if(hr == DD_OK) */ {
			TextureBuffer = rt.pBits;
			TexturePitch = rt.Pitch;
		}
	}
}
//---------------------------------------------------------------------------
void TJS_INTF_METHOD tTVPBasicDrawDevice::NotifyBitmapCompleted(iTVPLayerManager * manager,
	tjs_int x, tjs_int y, const void * bits, const BITMAPINFO * bitmapinfo,
	const tTVPRect &cliprect, tTVPLayerType type, tjs_int opacity)
{
	// bits, bitmapinfo で表されるビットマップの cliprect の領域を、x, y に描画
	// する。
	// opacity と type は無視するしかないので無視する
	tjs_int w, h;
	GetSrcSize( w, h );
	if( TextureBuffer && TargetWindow &&
		!(x < 0 || y < 0 ||
			x + cliprect.get_width() > w ||
			y + cliprect.get_height() > h) &&
		!(cliprect.left < 0 || cliprect.top < 0 ||
			cliprect.right > bitmapinfo->bmiHeader.biWidth ||
			cliprect.bottom > bitmapinfo->bmiHeader.biHeight))
	{
		// 範囲外の転送は(一部だけ転送するのではなくて)無視してよい
		ShouldShow = true;

		// bitmapinfo で表された cliprect の領域を x,y にコピーする
		long src_y       = cliprect.top;
		long src_y_limit = cliprect.bottom;
		long src_x       = cliprect.left;
		long width_bytes   = cliprect.get_width() * 4; // 32bit
		long dest_y      = y;
		long dest_x      = x;
		const tjs_uint8 * src_p = (const tjs_uint8 *)bits;
		long src_pitch;

		if(bitmapinfo->bmiHeader.biHeight < 0)
		{
			// bottom-down
			src_pitch = bitmapinfo->bmiHeader.biWidth * 4;
		}
		else
		{
			// bottom-up
			src_pitch = -bitmapinfo->bmiHeader.biWidth * 4;
			src_p += bitmapinfo->bmiHeader.biWidth * 4 * (bitmapinfo->bmiHeader.biHeight - 1);
		}

		for(; src_y < src_y_limit; src_y ++, dest_y ++)
		{
			const void *srcp = src_p + src_pitch * src_y + src_x * 4;
			void *destp = (tjs_uint8*)TextureBuffer + TexturePitch * dest_y + dest_x * 4;
			memcpy(destp, srcp, width_bytes);
		}
	}
}
//---------------------------------------------------------------------------
// Steam オーバーレイ対策: 強制 Present の直前に最新 Texture を
// バックバッファへ再転送する (静止画中にバックバッファが空になるのを防ぐ)。
void tTVPBasicDrawDevice::RedrawBackBufferFromTexture()
{
	if( !Direct3DDevice ) return;

	// GPU 合成中は CPU テクスチャが「更新領域のみ有効な部分画像」なので
	// 画面全体へ転送すると古い内容 (前の画面の残骸) が出る。
	// 静止フレームの再描画は必ず最後の quad リストで行う
	if( TVPGPUCompositeMode != 0 && !LastQuads.empty() )
	{
		if( RenderGPUQuads( LastQuads, false ) ) return;
	}

	if( !Texture ) return;
	if( TextureBuffer ) return;          // CPU 書き込み中は触らない
	if( TextureWidth == 0 || TextureHeight == 0 ) return;

	struct tVertices { float x, y, z, rhw; float tu, tv; };

	float dl = (float)( DestRect.left   < ClipRect.left   ? ClipRect.left   : DestRect.left );
	float dt = (float)( DestRect.top    < ClipRect.top    ? ClipRect.top    : DestRect.top );
	float dr = (float)( DestRect.right  > ClipRect.right  ? ClipRect.right  : DestRect.right );
	float db = (float)( DestRect.bottom > ClipRect.bottom ? ClipRect.bottom : DestRect.bottom );
	float dw = (float)DestRect.get_width();
	float dh = (float)DestRect.get_height();
	if( dw <= 0 || dh <= 0 ) return;

	float cl = dl - (float)DestRect.left;
	float ct = dt - (float)DestRect.top;
	float cr = (float)DestRect.right  - dr;
	float cb = (float)DestRect.bottom - db;

	tjs_int w, h;
	GetSrcSize( w, h );

	float sl = (float)(cl * w / dw ) / (float)TextureWidth;
	float st = (float)(ct * h / dh ) / (float)TextureHeight;
	float sr = (float)(w - (cr * w / dw) ) / (float)TextureWidth;
	float sb = (float)(h - (cb * h / dh) ) / (float)TextureHeight;

	tVertices vertices[] =
	{
		{dl - 0.5f, dt - 0.5f, 1.0f, 1.0f, sl, st },
		{dr - 0.5f, dt - 0.5f, 1.0f, 1.0f, sr, st },
		{dl - 0.5f, db - 0.5f, 1.0f, 1.0f, sl, sb },
		{dr - 0.5f, db - 0.5f, 1.0f, 1.0f, sr, sb }
	};

	D3DVIEWPORT9 vp;
	vp.X = 0;
	vp.Y = 0;
	vp.Width  = D3dPP.BackBufferWidth;
	vp.Height = D3dPP.BackBufferHeight;
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;
	Direct3DDevice->SetViewport(&vp);

	if( SUCCEEDED( Direct3DDevice->BeginScene() ) ) {
		if( BackBufferDirty ) {
			Direct3DDevice->Clear( 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0,0,0), 1.0, 0 );
			BackBufferDirty = false;
		}
		Direct3DDevice->SetTexture(0, Texture);
		Direct3DDevice->SetFVF( D3DFVF_XYZRHW|D3DFVF_TEX1 );
		// GPU 合成フレームが残したステージ状態 (COLORARG1=DIFFUSE 等) を
		// 引き継ぐとテクスチャではなく頂点色 (白) で塗ってしまう
		Direct3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		Direct3DDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
		Direct3DDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		Direct3DDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		Direct3DDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		Direct3DDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(tVertices) );
		Direct3DDevice->SetTexture(0, NULL);
		Direct3DDevice->EndScene();
	}
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// GPU 合成 (-gcomp)
//---------------------------------------------------------------------------
// 多角形のブレンド状態。CPU 側 blend_functor_c.h の式に厳密対応させる。
// ltAdditive / ltSubtractive / ltMultiplicative はテクスチャ alpha を
// 一切使わず opacity のみ。ltAlpha だけが per-pixel alpha を使う。
// ここでステージ設定を上書きしても、呼び出し側が quad ごとに
// COLOROP/ALPHAOP/TFACTOR を設定し直すので戻す必要はない。
static void SetGPUBlendStates(IDirect3DDevice9* dev, tjs_int type, tjs_int opacity)
{
	switch((tTVPLayerType)type)
	{
	case ltOpaque:
		if(opacity >= 255)
		{
			dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		}
		else
		{
			dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
			dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		}
		break;
	case ltAlpha:
	case ltBinder:
		dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		break;
	case ltAdditive:
		// result = dest + src.rgb * src.a * opa
		// ソース alpha (画像の透過) を掛けないと、画像全体の RGB が
		// 加算されて画面が白飛び・縞状の残像になる
		dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
		dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
		break;
	case ltSubtractive:
		// result = dest - src.rgb * src.a * opa
		dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
		dev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_REVSUBTRACT);
		dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
		break;
	case ltMultiplicative:
		// result = dest * lerp(1, src.rgb, opa)
		//        = dest * (src.rgb * opa) + dest * (1 - opa)
		// ステージ色を src.rgb*opa (ALPHAREPLICATE|TFACTOR) にして、
		// BLENDFACTOR.rgb = opa により INVBLENDFACTOR = (1 - opa) を作る。
		// BLENDFACTOR は TEXTUREFACTOR とは別のステート (既定は白 = 1.0) なので
		// 必ず両方設定する。片方だけだと係数 0 になり画面全体が opa 倍に暗くなる
		dev->SetRenderState(D3DRS_TEXTUREFACTOR,
			D3DCOLOR_RGBA((tjs_uint8)opacity, (tjs_uint8)opacity,
				(tjs_uint8)opacity, (tjs_uint8)opacity));
		dev->SetRenderState(D3DRS_BLENDFACTOR,
			D3DCOLOR_RGBA((tjs_uint8)opacity, (tjs_uint8)opacity,
				(tjs_uint8)opacity, (tjs_uint8)opacity));
		dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		dev->SetTextureStageState(0, D3DTSS_COLORARG2,
			D3DTA_ALPHAREPLICATE | D3DTA_TFACTOR);
		dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
		dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR);
		dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVBLENDFACTOR);
		break;
	default:
		break;
	}
}
//---------------------------------------------------------------------------
bool tTVPBasicDrawDevice::IsGPUReady() const
{
	return Direct3DDevice != NULL && Texture != NULL && TargetWindow != NULL;
}
//---------------------------------------------------------------------------
void tTVPBasicDrawDevice::ReleaseLayerTexCache()
{
	for(std::map<void *, tLayerTex>::iterator it = LayerTexCache.begin();
		it != LayerTexCache.end(); ++it)
		if(it->second.Tex) it->second.Tex->Release();
	LayerTexCache.clear();
	LastQuads.clear();
	LastFrameGPU = false;
}
//---------------------------------------------------------------------------
void tTVPBasicDrawDevice::PurgeLayerTexture(void * layer)
{
	std::map<void *, tLayerTex>::iterator it = LayerTexCache.find(layer);
	if(it != LayerTexCache.end())
	{
		if(it->second.Tex) it->second.Tex->Release();
		LayerTexCache.erase(it);
	}
	// 静止フレーム再描画用リストからも外す (破棄済みポインタの参照防止)
	for(size_t i = 0; i < LastQuads.size(); i++)
		if((void*)LastQuads[i].Layer == layer) LastQuads[i].Layer = NULL;
}
//---------------------------------------------------------------------------
void tTVPBasicDrawDevice::UpdateLayerTexture(const TVPGPUQuad & q)
{
	if(!Direct3DDevice || !q.Layer || !q.Bmp) return;

	std::map<void *, tLayerTex>::iterator it = LayerTexCache.find(q.Layer);
	bool create = it == LayerTexCache.end() ||
		it->second.BmpId != q.BmpId ||
		it->second.W != q.ImgW || it->second.H != q.ImgH;

	if(create)
	{
		if(it != LayerTexCache.end())
		{
			if(it->second.Tex) it->second.Tex->Release();
			LayerTexCache.erase(it);
		}
		if(q.ImgW <= 0 || q.ImgH <= 0) return;
		IDirect3DTexture9 * tex = NULL;
		HRESULT hr = Direct3DDevice->CreateTexture(q.ImgW, q.ImgH, 1, 0,
			D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, NULL);
		if(FAILED(hr))
		{
			ErrorToLog(hr);
			return;
		}
		tLayerTex e;
		e.Tex = tex; e.BmpId = q.BmpId; e.W = q.ImgW; e.H = q.ImgH;
		it = LayerTexCache.insert(std::make_pair((void*)q.Layer, e)).first;
	}
	else if(!q.BmpDirty)
	{
		// デバッグ: 強制再転送でキャッシュ汚染かを切り分ける
		static int force_upload = -1;
		if(force_upload < 0)
		{
			char f[16];
			force_upload = ::GetEnvironmentVariableA("TVP_GPU_FORCEUPLOAD", f, 8) > 0 ? 1 : 0;
		}
		if(!force_upload) return;	// 内容に変更なし
	}

	// 全内容転送 (kirikiri のビットマップは top-down: scanline 0 = 先頭行)
	IDirect3DTexture9 * tex = it->second.Tex;
	D3DLOCKED_RECT lr;
	if(FAILED(tex->LockRect(0, &lr, NULL, D3DLOCK_NO_DIRTY_UPDATE))) return;
	tTVPBaseBitmap * bmp = (tTVPBaseBitmap*)q.Bmp;
	const tjs_uint8 * src = (const tjs_uint8*)bmp->GetScanLine(0);
	tjs_int pitch = bmp->GetPitchBytes();
	tjs_int w = q.ImgW < it->second.W ? q.ImgW : it->second.W;
	tjs_int h = q.ImgH < it->second.H ? q.ImgH : it->second.H;
	for(tjs_int y = 0; y < h; y++)
		memcpy((tjs_uint8*)lr.pBits + (size_t)lr.Pitch * y,
			src + (size_t)pitch * y, (size_t)w * 4);
	tex->UnlockRect(0);
	tex->AddDirtyRect(NULL);
	q.Layer->SetImageModified(false);
}
//---------------------------------------------------------------------------
bool tTVPBasicDrawDevice::EnsureGPUTarget(tjs_int w, tjs_int h)
{
	if(GPUTargetTex && GPUTargetW == w && GPUTargetH == h) return true;
	ReleaseGPUTarget();
	IDirect3DTexture9 * tex = NULL;
	HRESULT hr = Direct3DDevice->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET,
		D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex, NULL);
	if(FAILED(hr))
	{
		ErrorToLog(hr);
		return false;
	}
	IDirect3DSurface9 * surf = NULL;
	if(FAILED(hr = tex->GetSurfaceLevel(0, &surf)))
	{
		ErrorToLog(hr);
		tex->Release();
		return false;
	}
	GPUTargetTex = tex;
	GPUTargetSurf = surf;
	GPUTargetW = w;
	GPUTargetH = h;

	if(GPUScratchTex) GPUScratchTex->Release(), GPUScratchTex = NULL;
	if(GPUScratchSurf) GPUScratchSurf->Release(), GPUScratchSurf = NULL;
	if(SUCCEEDED(Direct3DDevice->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET,
		D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &GPUScratchTex, NULL)))
	{
		GPUScratchTex->GetSurfaceLevel(0, &GPUScratchSurf);
	}
	return true;
}
//---------------------------------------------------------------------------
void tTVPBasicDrawDevice::ReleaseGPUTarget()
{
	if(GPUTargetSurf) GPUTargetSurf->Release(), GPUTargetSurf = NULL;
	if(GPUTargetTex) GPUTargetTex->Release(), GPUTargetTex = NULL;
	GPUTargetW = GPUTargetH = 0;
	if(GPUScratchSurf) GPUScratchSurf->Release(), GPUScratchSurf = NULL;
	if(GPUScratchTex) GPUScratchTex->Release(), GPUScratchTex = NULL;
}
//---------------------------------------------------------------------------
bool tTVPBasicDrawDevice::DrawGPUQuadRange(std::vector<TVPGPUQuad> & quads, int from, int to,
	IDirect3DSurface9 * target, float scalex, float scaley, int group, bool clearTarget)
{
	D3DVIEWPORT9 vp; vp.X=0; vp.Y=0; vp.Width=GPUTargetW; vp.Height=GPUTargetH; vp.MinZ=0.0f; vp.MaxZ=1.0f;
	Direct3DDevice->SetRenderTarget(0, target);
	Direct3DDevice->SetViewport(&vp);
	if(FAILED(Direct3DDevice->BeginScene())) return false;
	struct tVT { float x, y, z, rhw; float u, v; };
	struct tVC { float x, y, z, rhw; tjs_uint32 color; };
	if(clearTarget) Direct3DDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0,0,0,0), 1.0, 0);
	Direct3DDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	Direct3DDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	Direct3DDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	Direct3DDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	Direct3DDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
	HRESULT hr = D3D_OK;
for(int i = from; i < to; i++)
		{
			const TVPGPUQuad & q = quads[i];
			if(group == -2 && q.Group != -1) continue;
			if(group >= 0 && q.Group != group) continue;
			if(!q.IsFill && !q.Layer) continue;
			// デバッグ: 描画する quad の範囲を制限 (白帯の犯人を二分探索)
			// TVP_GPU_DRAWRANGE="a,b" で [a,b) のみ描く
			{
				static int rng_a = -2, rng_b = -2;
				if(rng_a == -2)
				{
					char sr[32] = {0};
					rng_a = -1; rng_b = -1;
					if(::GetEnvironmentVariableA("TVP_GPU_DRAWRANGE", sr, 24) > 0)
					{
						if(sscanf_s(sr, "%d,%d", &rng_a, &rng_b) != 2) rng_a = -1, rng_b = -1;
					}
				}
				if(rng_a >= 0 && ((int)i < rng_a || (rng_b >= 0 && (int)i >= rng_b))) continue;
			}
			// デバッグ: 指定インデックスのテクスチャを生画像で書き出す
			{
				static int texdump_idx = -2;
				if(texdump_idx == -2)
				{
					char ti[16] = {0};
					texdump_idx = ::GetEnvironmentVariableA("TVP_GPU_TEXDUMPIDX", ti, 8) > 0 ?
						atoi(ti) : -1;
				}
				static int texdump_done = 0;
				if(texdump_idx >= 0 && (int)i == texdump_idx && !texdump_done && !q.IsFill && q.Layer)
				{
					std::map<void *, tLayerTex>::iterator it = LayerTexCache.find(q.Layer);
					if(it != LayerTexCache.end() && it->second.Tex)
					{
						D3DLOCKED_RECT lr;
						if(SUCCEEDED(it->second.Tex->LockRect(0, &lr, NULL, D3DLOCK_READONLY)))
						{
							char out[560];
							sprintf_s(out, "%s\\gputex_%d.raw", ::getenv("TEMP") ? ::getenv("TEMP") : ".", (int)i);
							FILE * fp = fopen(out, "wb");
							if(fp)
							{
								fprintf(fp, "W=%d H=%d\n", q.ImgW, q.ImgH);
								for(tjs_int y = 0; y < q.ImgH; y++)
									fwrite((tjs_uint8*)lr.pBits + (size_t)lr.Pitch*y,
										1, (size_t)q.ImgW * 4, fp);
								fclose(fp);
								texdump_done = 1;
							}
							it->second.Tex->UnlockRect(0);
						}
					}
				}
			}
			// デバッグ: 各 quad をインデックス由来の単色で塗る (TVP_GPU_WIRE)
			// 画面のどの領域が何番の quad で描かれているかを一目で判別する
			{
				static int wire = -1;
				if(wire < 0)
				{
					char wbuf[8] = {0};
					wire = ::GetEnvironmentVariableA("TVP_GPU_WIRE", wbuf, 8) > 0 ? 1 : 0;
				}
				if(wire)
				{
					float wx = q.DestX * scalex, wy = q.DestY * scaley;
					float ww = (float)q.SrcW * scalex, wh = (float)q.SrcH * scaley;
					if(ww > 0 && wh > 0)
					{
						tjs_uint32 wc = D3DCOLOR_RGBA((int)((i * 37) & 0xff), (int)((i * 97) & 0xff), (int)((i * 61) & 0xff), 255);
						tVC wv[4] = {
							{wx - 0.5f, wy - 0.5f, 1.0f, 1.0f, wc},
							{wx + ww - 0.5f, wy - 0.5f, 1.0f, 1.0f, wc},
							{wx - 0.5f, wy + wh - 0.5f, 1.0f, 1.0f, wc},
							{wx + ww - 0.5f, wy + wh - 0.5f, 1.0f, 1.0f, wc}
						};
						Direct3DDevice->SetTexture(0, NULL);
						Direct3DDevice->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
						Direct3DDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
						Direct3DDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
						Direct3DDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
						Direct3DDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
						Direct3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
						Direct3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, wv, sizeof(tVC));
						Direct3DDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
						continue;
					}
				}
			}
			float cx = q.DestX * scalex;
			float cy = q.DestY * scaley;
			float cw = (float)q.SrcW * scalex;
			float ch = (float)q.SrcH * scaley;
			if(cw <= 0 || ch <= 0) continue;
			// 位置を連続化していない層は合成バッファの画素グリッドに載せる。
			// 半解像度合成では層座標の 1px が 0.5px になり、そのまま描くと
			// 文字が毎フレーム半画素位相で再標本されてちらつく
			if(!q.Subpixel)
			{
				const float rx0 = floorf(cx + 0.5f), ry0 = floorf(cy + 0.5f);
				const float rx1 = floorf(cx + cw + 0.5f), ry1 = floorf(cy + ch + 0.5f);
				cx = rx0; cy = ry0; cw = rx1 - rx0; ch = ry1 - ry0;
			}

			// 頂点は legacy パスと同じ -0.5 半ピクセル規約に合わせる
			Direct3DDevice->SetRenderState(D3DRS_TEXTUREFACTOR,
				D3DCOLOR_RGBA(255, 255, 255, (tjs_uint8)q.Opacity));
			Direct3DDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
			Direct3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
			Direct3DDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
			Direct3DDevice->SetTextureStageState(0, D3DTSS_COLORARG1,
				q.IsFill ? D3DTA_DIFFUSE : D3DTA_TEXTURE);
			Direct3DDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
			Direct3DDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1,
				q.IsFill ? D3DTA_DIFFUSE : D3DTA_TEXTURE);
			Direct3DDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);

			if(q.IsFill)
			{
				// 頂点色の alpha はそのまま使う。CPU 側も画像なし層を
				// NeutralColor で塗るだけで、既定値は alpha=0 (何も塗らない)。
				// ここで 255 に固定すると画面全体が黒く覆われる
				tjs_uint32 col = q.FillColor;
				tVC v[4] = {
					{cx - 0.5f, cy - 0.5f, 1.0f, 1.0f, col},
					{cx+cw - 0.5f, cy - 0.5f, 1.0f, 1.0f, col},
					{cx - 0.5f, cy+ch - 0.5f, 1.0f, 1.0f, col},
					{cx+cw - 0.5f, cy+ch - 0.5f, 1.0f, 1.0f, col}
				};
				Direct3DDevice->SetTexture(0, NULL);
				Direct3DDevice->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
				// 半透明の塗りは ltOpaque のままでは alpha を見ないので
				// ltAlpha として混ぜる
				SetGPUBlendStates(Direct3DDevice,
					(col >> 24) == 0xff ? q.BlendType : ltAlpha, q.Opacity);
				hr = Direct3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(tVC));
			}
			else
			{
				std::map<void *, tLayerTex>::iterator it = LayerTexCache.find(q.Layer);
				if(it == LayerTexCache.end() || !it->second.Tex)
				{
					// テクスチャ欠落 = このフレームは描ききれない → CPU へ戻す
					static int fbd = 0;
					if(fbd < 12)
					{
						fbd++;
						PresentDiag::Emit("GTEX", PresentDiag::NowMs(), "i=%d %dx%d l=%p",
							(int)i, q.ImgW, q.ImgH, q.Layer);
					}
					GPUFrameFailed = true;
					break;
				}
				IDirect3DTexture9 * tex = it->second.Tex;

				float u0 = (float)q.SrcX / (float)q.ImgW;
				float v0 = (float)q.SrcY / (float)q.ImgH;
				float u1 = (float)(q.SrcX + q.SrcW) / (float)q.ImgW;
				float v1 = (float)(q.SrcY + q.SrcH) / (float)q.ImgH;
				tVT v[4] = {
					{cx - 0.5f, cy - 0.5f, 1.0f, 1.0f, u0, v0},
					{cx+cw - 0.5f, cy - 0.5f, 1.0f, 1.0f, u1, v0},
					{cx - 0.5f, cy+ch - 0.5f, 1.0f, 1.0f, u0, v1},
					{cx+cw - 0.5f, cy+ch - 0.5f, 1.0f, 1.0f, u1, v1}
				};
				Direct3DDevice->SetTexture(0, tex);
				Direct3DDevice->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1);
				SetGPUBlendStates(Direct3DDevice, q.BlendType, q.Opacity);
				hr = Direct3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(tVT));
			}
			if(FAILED(hr)) break;
		}
	Direct3DDevice->SetTexture(0, NULL);
	Direct3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	Direct3DDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
	Direct3DDevice->EndScene();
	if(GPUFrameFailed) return false;
	if(FAILED(hr)) { ErrorToLog(hr); return false; }
	return true;
}
//---------------------------------------------------------------------------
bool tTVPBasicDrawDevice::DrawGPUQuadsComposite(std::vector<TVPGPUQuad> & quads, int from, int to,
	IDirect3DSurface9 * target, float scalex, float scaley,
	const std::vector<TVPGPUGroup> & groups, bool clearTarget)
{
	D3DVIEWPORT9 vp; vp.X=0; vp.Y=0; vp.Width=GPUTargetW; vp.Height=GPUTargetH; vp.MinZ=0.0f; vp.MaxZ=1.0f;
	Direct3DDevice->SetRenderTarget(0, target);
	Direct3DDevice->SetViewport(&vp);
	if(FAILED(Direct3DDevice->BeginScene())) return false;
	if(clearTarget) Direct3DDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0,0,0,0), 1.0, 0);

	int i = from;
	while(i < to)
	{
		const int gid = quads[i].Group;
		if(gid == -1 || gid >= (int)groups.size())
		{
			// ルート quad をこの位置で直接描く
			if(!DrawGPUQuadRange(quads, i, i+1, target, scalex, scaley, -2, false))
			{ Direct3DDevice->EndScene(); return false; }
			++i;
			continue;
		}
		// サブグループ: DFS 順のこの位置で纏めて合成してから重ねる
		const TVPGPUGroup & g = groups[gid];
		const int gend = g.QuadEnd > i ? g.QuadEnd : i+1;
		if(!DrawGPUQuadRange(quads, i, gend, GPUScratchSurf, scalex, scaley, gid, true))
		{ Direct3DDevice->EndScene(); return false; }
		// target へグループ結果を重ねる (opacity を 1 回だけ適用)
		Direct3DDevice->SetRenderTarget(0, target); Direct3DDevice->SetViewport(&vp);
		{
			struct tVG { float x,y,z,rhw; float u,v; };
			Direct3DDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
			Direct3DDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
			Direct3DDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
			Direct3DDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
			Direct3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
			Direct3DDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
			Direct3DDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
			Direct3DDevice->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_RGBA(255,255,255,(tjs_uint8)g.Opacity));
			Direct3DDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
			Direct3DDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			Direct3DDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
			Direct3DDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
			Direct3DDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
			const float dl=-0.5f, dt=-0.5f, dr=(float)GPUTargetW-0.5f, db=(float)GPUTargetH-0.5f;
			tVG bv[4]={{dl,dt,1,1,0,0},{dr,dt,1,1,1,0},{dl,db,1,1,0,1},{dr,db,1,1,1,1}};
			Direct3DDevice->SetTexture(0, GPUScratchTex);
			Direct3DDevice->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1);
			Direct3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, bv, sizeof(tVG));
			Direct3DDevice->SetTexture(0, NULL);
			Direct3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
			Direct3DDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
			Direct3DDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		}
		i = gend;
	}
	Direct3DDevice->EndScene();
	return true;
}
//---------------------------------------------------------------------------
bool tTVPBasicDrawDevice::RenderGPUQuads(std::vector<TVPGPUQuad> & quads, bool updateTextures)
{
	if(!Direct3DDevice)
	{
		PresentDiag::Emit("GF1", PresentDiag::NowMs(), "dev null");
		return false;
	}
	tjs_int srcw, srch;
	GetSrcSize(srcw, srch);
	if(srcw <= 0 || srch <= 0)
	{
		PresentDiag::Emit("GF2", PresentDiag::NowMs(), "src %dx%d dest %dx%d",
			(int)srcw, (int)srch, (int)DestRect.get_width(), (int)DestRect.get_height());
		return false;
	}
	GPUFrameFailed = false;
	tjs_int ssW = srcw, ssH = srch;
	float scaleTmp = 1.0f;
	if(!EnsureGPUTarget(ssW, ssH))
	{
		ssW = srcw; ssH = srch; scaleTmp = 1.0f;
		if(!EnsureGPUTarget(ssW, ssH))
		{
			PresentDiag::Emit("GERR", PresentDiag::NowMs(), "target %dx%d failed", (int)ssW, (int)ssH);
			return false;
		}
	}
	float scalex = scaleTmp;
	float scaley = scaleTmp;
	static int gpu_frame_no = 0;
	gpu_frame_no++;

	IDirect3DSurface9 * origRT = NULL;
	HRESULT ghr = Direct3DDevice->GetRenderTarget(0, &origRT);
	if(FAILED(ghr))
	{
		PresentDiag::Emit("GF3", PresentDiag::NowMs(), "GetRT hr=%08x", (unsigned)ghr);
		return false;
	}
	if(updateTextures)
		for(size_t i = 0; i < quads.size(); i++)
			if(quads[i].Layer && quads[i].Bmp) UpdateLayerTexture(quads[i]);
	if(updateTextures)
	{
		LastQuads = quads;
		LastFrameGPU = true;
	}
// デバッグ: quad リスト + 実際に使うテクスチャ内容を独立ファイルへ
	// (TVP_PRESENT_DIAG 設定時のみ。.qd = quad, .tx = テクスチャサンプル)
	{
		static int qdump = 0;
		static double q_t0 = -1.0;
		if(q_t0 < 0) q_t0 = PresentDiag::NowMs();
		// 起動から 26 秒後 (= ED 再生中) だけ記録する。上限に達して
		// 肝心の ED が記録されないのを避ける
		if(qdump < 6000 && PresentDiag::NowMs() - q_t0 > 26000.0)
		{
			// 未設定時にバッファが未初期化だと、スタックの残骸をファイル名に
			// してカレントディレクトリ (ゲームフォルダ) へ書き散らす
			char pp[560] = {0};
			if(::GetEnvironmentVariableA("TVP_PRESENT_DIAG", pp, 500) > 0 && pp[0])
			{
				qdump++;
				char p2[560] = {0};
				strncpy_s(p2, pp, sizeof(p2) - 1);
				strncat_s(p2, ".qd", sizeof(p2) - strlen(p2) - 1);
				FILE * fp = fopen(p2, "a");
				if(fp)
				{
					fprintf(fp, "QF %.3f f=%d n=%d df=%d\n", PresentDiag::NowMs() / 1000.0, gpu_frame_no,
						(int)quads.size(), (int)TVPGPUCompositeFrame());
					for(size_t i = 0; i < quads.size(); i++)
					{
						const TVPGPUQuad & q = quads[i];
						tjs_uint32 c0 = 0, cm = 0, c1 = 0;
						if(!q.IsFill && q.Layer)
						{
							std::map<void *, tLayerTex>::iterator it = LayerTexCache.find(q.Layer);
							if(it != LayerTexCache.end() && it->second.Tex)
							{
								D3DLOCKED_RECT lr;
								if(SUCCEEDED(it->second.Tex->LockRect(0, &lr, NULL, D3DLOCK_READONLY)))
								{
									const tjs_uint8 * tp = (const tjs_uint8*)lr.pBits;
									tjs_int sx = q.SrcX + q.SrcW/2;
									tjs_int sy = q.SrcY + q.SrcH/2;
									if(sx < q.ImgW && sy < q.ImgH)
										cm = *(const tjs_uint32*)(tp + (size_t)lr.Pitch*sy + (size_t)sx*4);
									c0 = *(const tjs_uint32*)tp;
									tjs_int ex = q.ImgW - 1, ey = q.ImgH - 1;
									c1 = *(const tjs_uint32*)(tp + (size_t)lr.Pitch*ey + (size_t)ex*4);
									it->second.Tex->UnlockRect(0);
								}
							}
						}
						fprintf(fp, "QD i=%d l=%p d=(%.2f,%.2f) s=(%d,%d) sr=(%d,%d) b=%d o=%d f=%d c=%08x img=(%d,%d) tl=%08x mid=%08x br=%08x",
							(int)i, (void*)q.Layer, q.DestX, q.DestY,
							q.SrcW, q.SrcH, q.SrcX, q.SrcY, q.BlendType, q.Opacity,
							q.IsFill ? 1 : 0, q.FillColor, q.ImgW, q.ImgH, c0, cm, c1);
						// 平滑化前の生の目標位置 (階段の段幅を測るため)
						if(q.Layer)
							fprintf(fp, " rt=(%d,%d)", (int)q.Layer->GetRect().left,
								(int)q.Layer->GetRect().top);
						// 画面の固定点 (レイヤ座標) を覆う quad なら、その点のテクセル色
						{
							static const float pts[3][2] = {
								{1481.0f, 925.0f}, {1200.0f, 900.0f}, {1700.0f, 800.0f}
							};
							tjs_uint32 pv[3] = {0,0,0};
							for(int pi = 0; pi < 3; pi++)
							{
								float px = pts[pi][0], py = pts[pi][1];
								if(!q.IsFill && q.Layer && px >= q.DestX && px < q.DestX + q.SrcW &&
									py >= q.DestY && py < q.DestY + q.SrcH)
								{
									std::map<void *, tLayerTex>::iterator it2 = LayerTexCache.find(q.Layer);
									if(it2 != LayerTexCache.end() && it2->second.Tex)
									{
										D3DLOCKED_RECT lr2;
										if(SUCCEEDED(it2->second.Tex->LockRect(0, &lr2, NULL, D3DLOCK_READONLY)))
										{
											tjs_int tx = q.SrcX + (tjs_int)(px - q.DestX);
											tjs_int ty = q.SrcY + (tjs_int)(py - q.DestY);
											if(tx >= 0 && ty >= 0 && tx < q.ImgW && ty < q.ImgH)
												pv[pi] = *(const tjs_uint32*)((const tjs_uint8*)lr2.pBits +
													(size_t)lr2.Pitch * ty + (size_t)tx * 4);
											it2->second.Tex->UnlockRect(0);
										}
									}
								}
							}
							fprintf(fp, " pv=%08x,%08x,%08x", pv[0], pv[1], pv[2]);
						}
						fprintf(fp, "\n");
					}
					fclose(fp);
				}
			}
		}
	}

	// GPUTarget を透明黒でクリア
	{
		D3DVIEWPORT9 vp; vp.X=0; vp.Y=0; vp.Width=ssW; vp.Height=ssH; vp.MinZ=0.0f; vp.MaxZ=1.0f;
		Direct3DDevice->SetRenderTarget(0, GPUTargetSurf);
		Direct3DDevice->SetViewport(&vp);
		Direct3DDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0,0,0,0), 1.0, 0);
		BackBufferDirty = false;
	}
	// グループを DFS 順に合成 (z-order を保つ)
	const std::vector<TVPGPUGroup> * pgroups = NULL;
	static const std::vector<TVPGPUGroup> emptyGroups;
	if(RenderManager) pgroups = &TVPGPUCompositeGetGroups(RenderManager);
	if(!DrawGPUQuadsComposite(quads, 0, (int)quads.size(), GPUTargetSurf, scalex, scaley,
		pgroups ? *pgroups : emptyGroups, false))
	{
		Direct3DDevice->SetRenderTarget(0, origRT); origRT->Release();
		return false;
	}

	// デバッグマーカー
	{
		static int marker_on = -1;
		if(marker_on < 0) { char m[16]={0}; marker_on = ::GetEnvironmentVariableA("TVP_PRESENT_DIAG", m, 8) > 0 ? 1 : 0; }
		if(marker_on)
		{
			D3DVIEWPORT9 vp; vp.X=0; vp.Y=0; vp.Width=ssW; vp.Height=ssH; vp.MinZ=0.0f; vp.MaxZ=1.0f;
			Direct3DDevice->SetRenderTarget(0, GPUTargetSurf); Direct3DDevice->SetViewport(&vp);
			if(SUCCEEDED(Direct3DDevice->BeginScene()))
			{
				struct tVM { float x,y,z,rhw; tjs_uint32 color; };
				tjs_uint8 gg=(tjs_uint8)(gpu_frame_no&0xff), bl=(tjs_uint8)(quads.size()&0xff);
				tjs_uint32 mc=D3DCOLOR_RGBA(255,gg,bl,255);
				tVM mv[4]={{8,8,1,1,mc},{24,8,1,1,mc},{8,24,1,1,mc},{24,24,1,1,mc}};
				Direct3DDevice->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
				Direct3DDevice->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
				Direct3DDevice->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
				Direct3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,mv,sizeof(tVM));
				Direct3DDevice->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
				Direct3DDevice->EndScene();
			}
		}
	}

	// 合成結果を DestRect へ 1 回だけ転送
	HRESULT hr = D3D_OK;
	{
		Direct3DDevice->SetRenderTarget(0, origRT);
		D3DVIEWPORT9 vp; vp.X=0; vp.Y=0; vp.Width=D3dPP.BackBufferWidth; vp.Height=D3dPP.BackBufferHeight;
		vp.MinZ=0.0f; vp.MaxZ=1.0f;
		Direct3DDevice->SetViewport(&vp);
		Direct3DDevice->Clear(0,NULL,D3DCLEAR_TARGET,D3DCOLOR_XRGB(0,0,0),1.0,0);
		if(!GPUFrameFailed && SUCCEEDED(Direct3DDevice->BeginScene()))
		{
			struct tVB { float x,y,z,rhw; float u,v; };
			Direct3DDevice->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);
			Direct3DDevice->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
			Direct3DDevice->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);
			Direct3DDevice->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
			Direct3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
			Direct3DDevice->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
			Direct3DDevice->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
			Direct3DDevice->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);
			Direct3DDevice->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
			const float dl=(float)DestRect.left-0.5f, dt=(float)DestRect.top-0.5f;
			const float dr=(float)DestRect.right-0.5f, db=(float)DestRect.bottom-0.5f;
			tVB bv[4]={{dl,dt,1,1,0,0},{dr,dt,1,1,1,0},{dl,db,1,1,0,1},{dr,db,1,1,1,1}};
			Direct3DDevice->SetTexture(0, GPUTargetTex);
			Direct3DDevice->SetFVF(D3DFVF_XYZRHW|D3DFVF_TEX1);
			hr = Direct3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, bv, sizeof(tVB));
			Direct3DDevice->SetTexture(0, NULL);
			Direct3DDevice->EndScene();
		}
	}

	if(origRT) origRT->Release();
	if(GPUFrameFailed) return false;
	if(hr == D3DERR_DEVICELOST) TryRecreateWhenDeviceLost();
	else if(FAILED(hr)) ErrorToLog(hr);
	return true;
}
//---------------------------------------------------------------------------
void TJS_INTF_METHOD tTVPBasicDrawDevice::EndBitmapCompletion(iTVPLayerManager * manager)
{
	if(!TargetWindow) return;
	if(!Direct3DDevice) return;

	PresentDiag::Emit("EP", PresentDiag::NowMs(), "has=%d mode=%d n=%d",
		TVPGPUCompositeHasQuads(manager) ? 1 : 0,
		TVPGPUCompositeMode,
		(int)TVPGPUCompositeGetQuads(manager).size());
	// GPU 合成フレーム: quad リストを直接描画する
	if(TVPGPUCompositeHasQuads(manager))
	{
		// StartBitmapCompletion でロックしたテクスチャは必ず解放する
		if(TextureBuffer && Texture)
		{
			Texture->UnlockRect(0);
			TextureBuffer = NULL;
		}
		// 現在のマネージャを記録 (グループ一覧の参照用)
		RenderManager = manager;
		bool ok = RenderGPUQuads(TVPGPUCompositeGetQuads(manager));
		PresentDiag::Emit("EP2", PresentDiag::NowMs(), "ok=%d", ok ? 1 : 0);
		TVPGPUCompositeClearQuads(manager);
		TVPGPUMarkCompositeTime();
		if(ok)
		{
			ShouldShow = true;
			return;
		}
		// 失敗したフレームは下の CPU 合成結果を転送する (黒画面を出さない)
	}

	if(!Texture) return;

	// CPU 合成フレームであることを記録する。更新しないと Show() が毎回
	// 「12ms 経過」と判断して余分に合成し、古い quad リストで新しい画面を
	// 上書きしてしまう (トランジションの残像の原因)
	TVPGPUMarkCompositeTime();
	LastQuads.clear();

	if(TextureBuffer)
	{
		Texture->UnlockRect(0);
		TextureBuffer = NULL;
	}


	//- build vertex list
	struct tVertices
	{
		float x, y, z, rhw;
		float tu, tv;
	};

	// 転送先をクリッピング矩形に基づきクリッピング
	float dl = (float)( DestRect.left < ClipRect.left ? ClipRect.left : DestRect.left );
	float dt = (float)( DestRect.top < ClipRect.top ? ClipRect.top : DestRect.top );
	float dr = (float)( DestRect.right > ClipRect.right ? ClipRect.right : DestRect.right );
	float db = (float)( DestRect.bottom > ClipRect.bottom ? ClipRect.bottom : DestRect.bottom );
	float dw = (float)DestRect.get_width();
	float dh = (float)DestRect.get_height();

	// はみ出している幅を求める
	float cl = dl - (float)DestRect.left;
	float ct = dt - (float)DestRect.top;
	float cr = (float)DestRect.right - dr;
	float cb = (float)DestRect.bottom - db;

	// はみ出している幅を考慮して、転送元画像をクリッピング
	tjs_int w, h;
	GetSrcSize( w, h );
	float sl = (float)(cl * w / dw ) / (float)TextureWidth;
	float st = (float)(ct * h / dh ) / (float)TextureHeight;
	float sr = (float)(w - (cr * w / dw) ) / (float)TextureWidth;
	float sb = (float)(h - (cb * h / dh) ) / (float)TextureHeight;

	tVertices vertices[] =
	{
		{dl - 0.5f, dt - 0.5f, 1.0f, 1.0f, sl, st },
		{dr - 0.5f, dt - 0.5f, 1.0f, 1.0f, sr, st },
		{dl - 0.5f, db - 0.5f, 1.0f, 1.0f, sl, sb },
		{dr - 0.5f, db - 0.5f, 1.0f, 1.0f, sr, sb }
	};

	HRESULT hr;

// フルスクリーン化後、1回は全体消去、それ以降はウィンドウの範囲内のみにした方が効率的。
	D3DVIEWPORT9 vp;
	vp.X  = 0;
	vp.Y  = 0;
	vp.Width = D3dPP.BackBufferWidth;
	vp.Height = D3dPP.BackBufferHeight;
	vp.MinZ  = 0.0f;
	vp.MaxZ  = 1.0f;
	Direct3DDevice->SetViewport(&vp);

	if( SUCCEEDED(Direct3DDevice->BeginScene()) ) {
		struct CAutoEndSceneCall {
			IDirect3DDevice9*	m_Device;
			CAutoEndSceneCall( IDirect3DDevice9* device ) : m_Device(device) {}
			~CAutoEndSceneCall() { m_Device->EndScene(); }
		} autoEnd(Direct3DDevice);

		if( BackBufferDirty ) {
			Direct3DDevice->Clear( 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0,0,0), 1.0, 0 );
			BackBufferDirty = false;
		}

		//- draw as triangles
		if( FAILED(hr = Direct3DDevice->SetTexture(0, Texture)) )
			goto got_error;

		// GPU 合成フレームのステージ状態を引き継がない (白画面化防止)
		Direct3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		Direct3DDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
		Direct3DDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		Direct3DDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		Direct3DDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

		if( FAILED( hr = Direct3DDevice->SetFVF( D3DFVF_XYZRHW|D3DFVF_TEX1 ) ) )
			goto got_error;

		if( FAILED( hr = Direct3DDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(tVertices) ) ) )
			goto got_error;

		if( FAILED( hr = Direct3DDevice->SetTexture( 0, NULL) ) )
			goto got_error;

		// デバッグ: CPU 合成フレームであることを示す赤マーカー (録画解析用)
		{
			static int marker_on2 = -1;
			if(marker_on2 < 0)
			{
				char m2[16] = {0};
				marker_on2 = ::GetEnvironmentVariableA("TVP_PRESENT_DIAG", m2, 8) > 0 ? 1 : 0;
			}
			if(marker_on2)
			{
				struct tMC { float x, y, z, rhw; tjs_uint32 color; };
				tMC mv[4] = {
					{32.0f, 8.0f, 1.0f, 1.0f, D3DCOLOR_RGBA(255, 0, 0, 255)},
					{48.0f, 8.0f, 1.0f, 1.0f, D3DCOLOR_RGBA(255, 0, 0, 255)},
					{32.0f, 24.0f, 1.0f, 1.0f, D3DCOLOR_RGBA(255, 0, 0, 255)},
					{48.0f, 24.0f, 1.0f, 1.0f, D3DCOLOR_RGBA(255, 0, 0, 255)}
				};
				Direct3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
				Direct3DDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
				Direct3DDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
				Direct3DDevice->SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
				Direct3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, mv, sizeof(tMC));
				Direct3DDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			}
		}
	}

got_error:
	if( hr == D3DERR_DEVICELOST ) {
		TryRecreateWhenDeviceLost();
	} else if(hr == D3DERR_DEVICENOTRESET ) {
		hr = Direct3DDevice->Reset(&D3dPP);
		if( hr == D3DERR_DEVICELOST ) {
			TVPAddLog( (const tjs_char*)TVPD3dErrDeviceLost );
			TryRecreateWhenDeviceLost();
		} else if( hr == D3DERR_DRIVERINTERNALERROR ) {
			TVPAddLog( (const tjs_char*)TVPD3dErrDriverIinternalError );
			TryRecreateWhenDeviceLost();
		} else if( hr == D3DERR_INVALIDCALL ) {
			TVPAddLog( (const tjs_char*)TVPD3dErrInvalidCall );
			TryRecreateWhenDeviceLost();
		} else if( hr  == D3DERR_OUTOFVIDEOMEMORY ) {
			TVPAddLog( (const tjs_char*)TVPD3dErrOutOfVideoMemory );
			TryRecreateWhenDeviceLost();
		} else if( hr == E_OUTOFMEMORY  ) {
			TVPAddLog( (const tjs_char*)TVPD3dErrOutOfMemory );
			TryRecreateWhenDeviceLost();
		} else if( hr == D3D_OK ) {
			if( FAILED( hr = InitializeDirect3DState() ) ) {
				if( IsTargetWindowActive() ) {
					ErrorToLog( hr );
					TVPThrowExceptionMessage( TVPFaildToSetRenderState );
				} else {
					DestroyD3DDevice();
				}
			}
		}
	} else if(hr != D3D_OK) {
		ErrorToLog( hr );
		TVPAddImportantLog( TVPFormatMessage(TVPBasicDrawDeviceInfPolygonDrawingFailed,TJSInt32ToHex(hr, 8)) );
	}
}
//---------------------------------------------------------------------------
void TJS_INTF_METHOD tTVPBasicDrawDevice::SetShowUpdateRect(bool b)
{
	DrawUpdateRectangle = b;
}
//---------------------------------------------------------------------------
bool TJS_INTF_METHOD tTVPBasicDrawDevice::SwitchToFullScreen( HWND window, tjs_uint w, tjs_uint h, tjs_uint bpp, tjs_uint color, bool changeresolution )
{
	// フルスクリーン化の処理はなにも行わない、互換性のためにウィンドウを全画面化するのみで処理する
	// Direct3D9 でフルスクリーン化するとフォーカスを失うとデバイスをロストするので、そのたびにリセットor作り直しが必要になる。
	// モーダルウィンドウを使用するシステムでは、これは困るので常にウィンドウモードで行う。
	// モーダルウィンドウを使用しないシステムにするのなら、フルスクリーンを使用するDrawDeviceを作ると良い。
	BackBufferDirty = true;
	ShouldShow = true;
	CheckMonitorMoved();
	return true;
}
//---------------------------------------------------------------------------
void TJS_INTF_METHOD tTVPBasicDrawDevice::RevertFromFullScreen( HWND window, tjs_uint w, tjs_uint h, tjs_uint bpp, tjs_uint color )
{
	BackBufferDirty = true;
	ShouldShow = true;
	CheckMonitorMoved();
}
//---------------------------------------------------------------------------







//---------------------------------------------------------------------------
// tTJSNI_BasicDrawDevice : BasicDrawDevice TJS native class
//---------------------------------------------------------------------------
tjs_uint32 tTJSNC_BasicDrawDevice::ClassID = (tjs_uint32)-1;
tTJSNC_BasicDrawDevice::tTJSNC_BasicDrawDevice() :
	tTJSNativeClass(TJS_W("BasicDrawDevice"))
{
	// register native methods/properties

	TJS_BEGIN_NATIVE_MEMBERS(BasicDrawDevice)
	TJS_DECL_EMPTY_FINALIZE_METHOD
//----------------------------------------------------------------------
// constructor/methods
//----------------------------------------------------------------------
TJS_BEGIN_NATIVE_CONSTRUCTOR_DECL(/*var.name*/_this, /*var.type*/tTJSNI_BasicDrawDevice,
	/*TJS class name*/BasicDrawDevice)
{
	return TJS_S_OK;
}
TJS_END_NATIVE_CONSTRUCTOR_DECL(/*TJS class name*/BasicDrawDevice)
//----------------------------------------------------------------------
TJS_BEGIN_NATIVE_METHOD_DECL(/*func. name*/recreate)
{
	TJS_GET_NATIVE_INSTANCE(/*var. name*/_this, /*var. type*/tTJSNI_BasicDrawDevice);
	_this->GetDevice()->SetToRecreateDrawer();
	_this->GetDevice()->EnsureDevice();
	return TJS_S_OK;
}
TJS_END_NATIVE_METHOD_DECL(/*func. name*/recreate)
//----------------------------------------------------------------------


//---------------------------------------------------------------------------
//----------------------------------------------------------------------
// properties
//----------------------------------------------------------------------
TJS_BEGIN_NATIVE_PROP_DECL(interface)
{
	TJS_BEGIN_NATIVE_PROP_GETTER
	{
		TJS_GET_NATIVE_INSTANCE(/*var. name*/_this, /*var. type*/tTJSNI_BasicDrawDevice);
		*result = reinterpret_cast<tjs_int64>(_this->GetDevice());
		return TJS_S_OK;
	}
	TJS_END_NATIVE_PROP_GETTER

	TJS_DENY_NATIVE_PROP_SETTER
}
TJS_END_NATIVE_PROP_DECL(interface)
//----------------------------------------------------------------------
	TJS_END_NATIVE_MEMBERS
}
//---------------------------------------------------------------------------
iTJSNativeInstance *tTJSNC_BasicDrawDevice::CreateNativeInstance()
{
	return new tTJSNI_BasicDrawDevice();
}
//---------------------------------------------------------------------------




//---------------------------------------------------------------------------
tTJSNI_BasicDrawDevice::tTJSNI_BasicDrawDevice()
{
	Device = new tTVPBasicDrawDevice();
}
//---------------------------------------------------------------------------
tTJSNI_BasicDrawDevice::~tTJSNI_BasicDrawDevice()
{
	if(Device) Device->Destruct(), Device = NULL;
}
//---------------------------------------------------------------------------
tjs_error TJS_INTF_METHOD
	tTJSNI_BasicDrawDevice::Construct(tjs_int numparams, tTJSVariant **param,
		iTJSDispatch2 *tjs_obj)
{
	return TJS_S_OK;
}
//---------------------------------------------------------------------------
void TJS_INTF_METHOD tTJSNI_BasicDrawDevice::Invalidate()
{
	if(Device) Device->Destruct(), Device = NULL;
}
//---------------------------------------------------------------------------

