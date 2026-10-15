#pragma once
#ifndef _steam_overlay_state_h_
#define _steam_overlay_state_h_

//---------------------------------------------------------------------------
// Steam オーバーレイの状態。 DrawDevice::Show() が「静止画でも毎フレーム
// Present するか」の判定に使う (overlay は Present hook で合成するため)。
//   更新: extension/achievement_tjs.cpp。 include を持たない。
//   非 Steam ビルドではこのヘッダは空になる。
//---------------------------------------------------------------------------
#ifdef __HAS_STEAM_API__

extern bool TVPSteamOverlayActive;

// SteamAPI_Init に成功したら true。「閉じる過程」でも Present が要るため、
// 表示中フラグとは別にこれでも毎フレーム Present させる。
extern bool TVPSteamOverlayAvailable;

// true なら main window 表示時にデバイスを作り直して overlay をフックさせる
// (Show() が消費する。実装: BasicDrawDevice.cpp / achievement_tjs.cpp)。
extern bool TVPSteamOverlayNeedsHook;

// SteamAPI_Init 成功後に呼ぶ。既存の D3D デバイスを作り直し、overlay の
// フック対象にする (実装: BasicDrawDevice.cpp)。
extern void TVPSteamOverlayHookDevices();

// オーバーレイの開閉に合わせてビデオ子ウィンドウ (krmovie) を隠す/戻す。
// 子ウィンドウは常に手前なので、隠さないと overlay がビデオの裏に回る。
extern void TVPSteamOverlaySetMovieWindowsVisible( bool visible );

#endif // __HAS_STEAM_API__
#endif // _steam_overlay_state_h_
