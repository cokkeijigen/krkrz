//---------------------------------------------------------------------------
/*
	ゲーム画面（レイヤ×ズーム）をウインドウの実クライアント (innerWidth /
	innerHeight) へどう収めるかの設定。

	TTVPWindowForm::SetDrawDeviceDestRect は従来「原寸・左上」固定だったため、
	クライアントと食い違うと（最大化・リサイズ・異 DPI 移動）描画がずれた。
	収め方と配置をここで持つ。ウィンドウ側＝スクリプトから選ぶ。
*/
//---------------------------------------------------------------------------
#ifndef ViewportConfigH
#define ViewportConfigH

#include "ComplexRect.h"
#include <algorithm>
#include <cmath>

/*[*/
//---------------------------------------------------------------------------
//! @brief	収め方
//---------------------------------------------------------------------------
enum tTVPViewportFit {
	vfContain = 0, //!< アスペクト維持で収まる最大（letterbox）
	vfCover,       //!< アスペクト維持で埋める最小（はみ出しは clip）
	vfFill,        //!< アスペクト無視で全面へ引き伸ばし
	vfNone,        //!< 原寸
	vfInteger,     //!< 収まる範囲で最大の整数倍（最低 1 倍）
};
/*]*/

//---------------------------------------------------------------------------
//! @brief	配置設定。align は 0=左/上, 0.5=中央, 1=右/下
//---------------------------------------------------------------------------
struct tTVPViewportConfig {
	tTVPViewportFit fit;
	double alignX;
	double alignY;

	// 既定は従来動作（原寸・左上）。設定しなければ DestRect は変わらない。
	tTVPViewportConfig()
		: fit(vfNone)
		, alignX(0.0), alignY(0.0) {}
};

//---------------------------------------------------------------------------
//! @brief	設定から描画先矩形（ウインドウ座標）を計算する
//! @param	sw,sh	ウインドウの実クライアントサイズ
//! @param	lw,lh	ゲーム画面サイズ（レイヤ×ズーム）
//---------------------------------------------------------------------------
inline tTVPRect TVPCalcViewportDestRect(const tTVPViewportConfig &cfg,
	tjs_int sw, tjs_int sh, tjs_int lw, tjs_int lh)
{
	if (sw <= 0 || sh <= 0 || lw <= 0 || lh <= 0) {
		return tTVPRect(0, 0, sw > 0 ? sw : 1, sh > 0 ? sh : 1);
	}

	double sx, sy;
	switch (cfg.fit) {
	case vfContain:
		sx = sy = (std::min)((double)sw / lw, (double)sh / lh);
		break;
	case vfCover:
		sx = sy = (std::max)((double)sw / lw, (double)sh / lh);
		break;
	case vfFill:
		sx = (double)sw / lw;
		sy = (double)sh / lh;
		break;
	case vfInteger: {
		double s = (std::min)((double)sw / lw, (double)sh / lh);
		int n = (int)s;
		if (n < 1) n = 1;
		sx = sy = (double)n;
		break;
	}
	case vfNone:
	default:
		sx = sy = 1.0;
		break;
	}

	// 目標は実数サイズ。整数化は必ず外側へ丸める（近い側＝床、遠い側＝天井）。
	// 四捨五入だとレイヤ×ズームと実クライアントが 1px 食い違うだけで
	// その 1px が上/左に寄り、タイトルバーの下に黒線が出る。
	double fw = lw * sx, fh = lh * sy;
	double ox = (sw - fw) * cfg.alignX;
	double oy = (sh - fh) * cfg.alignY;
	tjs_int left   = (tjs_int)std::floor(ox + 1e-9);
	tjs_int top    = (tjs_int)std::floor(oy + 1e-9);
	tjs_int right  = (tjs_int)std::ceil(ox + fw - 1e-9);
	tjs_int bottom = (tjs_int)std::ceil(oy + fh - 1e-9);
	if (right <= left) right = left + 1;
	if (bottom <= top) bottom = top + 1;

	return tTVPRect(left, top, right, bottom);
}

//---------------------------------------------------------------------------
#endif
