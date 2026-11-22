
#ifndef BASIC_DRAW_DEVICE_H
#define BASIC_DRAW_DEVICE_H

#include "DrawDevice.h"
#include "GPUComposite.h"
#include <d3d9.h>
#include <map>
#include <vector>

//---------------------------------------------------------------------------
//! @brief		「Basic」デバイス(もっとも基本的な描画を行うのみのデバイス)
//---------------------------------------------------------------------------
class tTVPBasicDrawDevice : public tTVPDrawDevice
{
	typedef tTVPDrawDevice inherited;

	HWND TargetWindow;
	bool IsMainWindow;
	bool DrawUpdateRectangle;
	bool BackBufferDirty;

	IDirect3D9*				Direct3D;
	IDirect3DDevice9*		Direct3DDevice;
	IDirect3DTexture9*		Texture;
	D3DPRESENT_PARAMETERS	D3dPP;
	D3DDISPLAYMODE			DispMode;

	UINT	CurrentMonitor;
	void*	TextureBuffer; //!< テクスチャのサーフェースへのメモリポインタ
	long	TexturePitch; //!< テクスチャのピッチ

	tjs_uint TextureWidth; //!< テクスチャの横幅
	tjs_uint TextureHeight; //!< テクスチャの縦幅

	bool ShouldShow; //!< show で実際に画面に画像を転送すべきか

	tjs_uint VsyncInterval;

	//---- GPU 合成 (-gcomp)
	struct tLayerTex
	{
		IDirect3DTexture9 * Tex;
		void * BmpId;		//!< 転送済みビットマップ同一性
		tjs_int W, H;
	};
	std::map<void *, tLayerTex> LayerTexCache;	//!< レイヤ -> テクスチャ
	std::vector<TVPGPUQuad> LastQuads;			//!< 最後に描いた quad (静止フレーム再描画用)
	bool LastFrameGPU;							//!< 直前フレームを GPU で描いたか
	iTVPLayerManager * RenderManager;			//!< 現在の合成のマネージャ (グループ参照用)

	//! レイヤ座標系 (1920x1080 等) で 1:1 合成するオフスクリーン面。
	//! これを一度だけクライアントへ縮小転送する (CPU 合成と同じ経路)
	IDirect3DTexture9 * GPUTargetTex;
	IDirect3DSurface9 * GPUTargetSurf;
	tjs_int GPUTargetW, GPUTargetH;
	//! 半透明グループを一時合成する面 (CPU の temp bitmap 相当)
	IDirect3DTexture9 * GPUScratchTex;
	IDirect3DSurface9 * GPUScratchSurf;

	void ReleaseGPUTarget();
	bool EnsureGPUTarget(tjs_int w, tjs_int h);

	void ReleaseLayerTexCache();
	void UpdateLayerTexture(const TVPGPUQuad & q);
	bool DrawGPUQuadRange(std::vector<TVPGPUQuad> & quads, int from, int to,
		IDirect3DSurface9 * target, float scalex, float scaley, int group, bool clearTarget);
	//! グループを DFS 順に合成する。group==-1 の quad は直接 target へ、
	//! 子グループは一時サーフェスへ合成してから opacity を 1 回掛けて target へ重ねる。
	bool DrawGPUQuadsComposite(std::vector<TVPGPUQuad> & quads, int from, int to,
		IDirect3DSurface9 * target, float scalex, float scaley,
		const std::vector<TVPGPUGroup> & groups, bool clearTarget);
	bool RenderGPUQuads(std::vector<TVPGPUQuad> & quads, bool updateTextures = true);
	bool GPUFrameFailed;	//!< このフレームの GPU 合成が失敗 (CPU へフォールバック)

public:
	tTVPBasicDrawDevice(); //!< コンストラクタ

private:
	~tTVPBasicDrawDevice(); //!< デストラクタ

	void InvalidateAll();

	// Steam オーバーレイ対応: Texture の内容をバックバッファへ再転送する。
	// (Show() で「ゲームが再描画していないフレーム」を強制 Present する直前に使う)
	void RedrawBackBufferFromTexture();

	UINT GetMonitorNumber( HWND window );

	bool IsTargetWindowActive() const;

	bool GetDirect3D9Device();
	HRESULT InitializeDirect3DState();
	HRESULT DecideD3DPresentParameters();

	bool CreateD3DDevice();
	void DestroyD3DDevice();

	bool CreateTexture();
	void DestroyTexture();

	void TryRecreateWhenDeviceLost();
	void ErrorToLog( HRESULT hr );
	void CheckMonitorMoved();

public:
	void SetToRecreateDrawer() { DestroyD3DDevice(); }

	//! 指定レイヤのキャッシュ済みテクスチャを破棄 (レイヤ破棄時 / デバイス再生成時)
	void PurgeLayerTexture(void * layer);
	//! GPU 合成を実行できる状態か
	bool IsGPUReady() const;

#ifdef __HAS_STEAM_API__
	// SteamAPI_Init 後に既存デバイスを作り直す (overlay の D3D フックは
	// init 以後に生成されたデバイスしか wrap しないため)。
	static void RecreateForSteamOverlayHook();
	// オーバーレイの開閉に合わせてビデオ子ウィンドウを隠す/戻す。
	static void SteamSetMovieWindowsVisible( bool visible );
#endif

public:
	void EnsureDevice();

//---- LayerManager の管理関連
	virtual void TJS_INTF_METHOD AddLayerManager(iTVPLayerManager * manager);

//---- 描画位置・サイズ関連
	virtual void TJS_INTF_METHOD SetTargetWindow(HWND wnd, bool is_main);
	virtual void TJS_INTF_METHOD SetDestRectangle(const tTVPRect & rect);
	virtual void TJS_INTF_METHOD NotifyLayerResize(iTVPLayerManager * manager);

//---- 再描画関連
	virtual void TJS_INTF_METHOD Show();
	virtual bool TJS_INTF_METHOD WaitForVBlank( tjs_int* in_vblank, tjs_int* delayed );

//---- LayerManager からの画像受け渡し関連
	virtual void TJS_INTF_METHOD StartBitmapCompletion(iTVPLayerManager * manager);
	virtual void TJS_INTF_METHOD NotifyBitmapCompleted(iTVPLayerManager * manager,
		tjs_int x, tjs_int y, const void * bits, const BITMAPINFO * bitmapinfo,
		const tTVPRect &cliprect, tTVPLayerType type, tjs_int opacity);
	virtual void TJS_INTF_METHOD EndBitmapCompletion(iTVPLayerManager * manager);

//---- デバッグ支援
	virtual void TJS_INTF_METHOD SetShowUpdateRect(bool b);

//---- フルスクリーン
	virtual bool TJS_INTF_METHOD SwitchToFullScreen( HWND window, tjs_uint w, tjs_uint h, tjs_uint bpp, tjs_uint color, bool changeresolution );
	virtual void TJS_INTF_METHOD RevertFromFullScreen( HWND window, tjs_uint w, tjs_uint h, tjs_uint bpp, tjs_uint color );

};
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// tTJSNI_BasicDrawDevice
//---------------------------------------------------------------------------
class tTJSNI_BasicDrawDevice :
	public tTJSNativeInstance
{
	typedef tTJSNativeInstance inherited;

	tTVPBasicDrawDevice * Device;

public:
	tTJSNI_BasicDrawDevice();
	~tTJSNI_BasicDrawDevice();
	tjs_error TJS_INTF_METHOD
		Construct(tjs_int numparams, tTJSVariant **param,
			iTJSDispatch2 *tjs_obj);
	void TJS_INTF_METHOD Invalidate();

public:
	tTVPBasicDrawDevice * GetDevice() const { return Device; }

};
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------
// tTJSNC_BasicDrawDevice
//---------------------------------------------------------------------------
class tTJSNC_BasicDrawDevice : public tTJSNativeClass
{
public:
	tTJSNC_BasicDrawDevice();

	static tjs_uint32 ClassID;

private:
	iTJSNativeInstance *CreateNativeInstance();
};
//---------------------------------------------------------------------------


#endif // BASIC_DRAW_DEVICE_H