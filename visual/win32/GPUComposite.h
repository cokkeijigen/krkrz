//---------------------------------------------------------------------------
// GPU 合成 (-gcomp / TVP_GPU_COMPOSITE)
//
// レイヤツリーを quad リストへ展開し、BasicDrawDevice が直接 GPU で描く。
// 従来の CPU 合成 (レイヤ毎の BLT) を置き換え、合成コストを削減する。
// ツリー内に非対応機能 (トランジション/キャッシュ/フィルタ等) があれば
// そのフレームは丸ごと CPU 合成へフォールバックする。
//---------------------------------------------------------------------------
#ifndef GPUComposite_H
#define GPUComposite_H

#include "tjsCommHead.h"
#include <vector>

class tTJSNI_BaseLayer;
class iTVPLayerManager;

//---------------------------------------------------------------------------
// 描画リスト 1 要素
//---------------------------------------------------------------------------
struct TVPGPUQuad
{
	tTJSNI_BaseLayer * Layer;	//!< テクスチャ元レイヤ (NULL = 単色塗りつぶし)
	float DestX, DestY;			//!< プライマリ座標系での画像 (0,0) 位置
	tjs_int SrcX, SrcY;			//!< 画像内の表示開始オフセット
	tjs_int SrcW, SrcH;			//!< 表示サイズ
	tjs_int BlendType;			//!< tTVPLayerType
	tjs_int Opacity;			//!< 累積不透明度 (0-255)
	bool IsFill;				//!< 単色塗りつぶし
	tjs_uint32 FillColor;
	void * Bmp;					//!< 元ビットマップ (所有権なし)
	void * BmpId;				//!< ビットマップ同一性 (差し替え検出用)
	bool BmpDirty;				//!< ImageModified のスナップショット
	bool Subpixel;				//!< 位置を連続化した層 (合成グリッドに量子化しない)
	tjs_int ImgW, ImgH;			//!< 元画像サイズ (UV 計算用)
	int Group;					//!< 所属する合成グループ (-1 = ルート)
};

//! CPU の半透明コンテナ層と同じ意味の合成グループ。
struct TVPGPUGroup
{
	tjs_int Opacity;			//!< グループ境界の不透明度 (0-255)
	tjs_int BlendType;			//!< 合成モード (tTVPLayerType)
	int QuadStart;				//!< このグループの quad 開始 index
	int QuadEnd;				//!< このグループの quad 終了 index (排他的)
};

//---------------------------------------------------------------------------
// グローバル
//---------------------------------------------------------------------------
extern int TVPGPUCompositeMode;	//!< 0=無効 1=有効

//! 亜像素平滑の on/off (TVP_GPU_SMOOTH=0 で無効)
bool TVPGPUSmoothEnabled();

void TVPParseGPUCompositeOption();	//!< -gcomp / 環境変数を解釈 (一度だけ)

//! 合成フレーム番号 (亜像素平滑の基準時計)。CompleteForWindow で進める
extern tjs_uint TVPGPUCompositeFrameNo;
tjs_uint TVPGPUCompositeFrame();

//! GPU 合成で扱える合成モードか (CPU 側 blend_functor_c.h に対応があるもの)
bool TVPGPUBlendSupported(tjs_int type);

//! テクスチャを保持できるデバイスが 1 つ以上生きているか
bool TVPGPUCompositeDeviceOK();

//---------------------------------------------------------------------------
// マネージャ毎の quad リスト
//---------------------------------------------------------------------------
std::vector<TVPGPUQuad> & TVPGPUCompositeGetQuads(iTVPLayerManager * manager);
bool TVPGPUCompositeHasQuads(iTVPLayerManager * manager);
void TVPGPUCompositeClearQuads(iTVPLayerManager * manager);

//---------------------------------------------------------------------------
// 合成グループ (半透明コンテナ層のサブツリーを一時サーフェスへ合成する)
//---------------------------------------------------------------------------
void TVPGPUCompositeClearGroups(iTVPLayerManager * manager);
int  TVPGPUCompositeBeginGroup(iTVPLayerManager * manager, tjs_int opacity, tjs_int blendType);
void TVPGPUCompositeEndGroup(iTVPLayerManager * manager);
int  TVPGPUCompositeCurrentGroup(iTVPLayerManager * manager);
const std::vector<TVPGPUGroup> & TVPGPUCompositeGetGroups(iTVPLayerManager * manager);

//---------------------------------------------------------------------------
// レイヤ破棄通知 -> デバイス側テクスチャキャッシュの破棄
//---------------------------------------------------------------------------
typedef void (*tTVPGPULayerPurgeFn)(void * layer);
void TVPGPUCompositeSetLayerPurge(tTVPGPULayerPurgeFn fn);
void TVPGPUCompositeOnLayerDestroyed(void * layer);

#endif // GPUComposite_H
