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
// Steam オーバーレイ機構が利用可能か (SteamAPI_Init に成功したら true)。
//
//   true の間は DrawDevice::Show() が静止画でも毎フレーム Present する。
//   overlay は「閉じる過程」でも Present を必要とするため、 表示中フラグだけ
//   では足りない。 Steam が使えない環境では false のままになり、 従来どおり
//   静止画では Present をスキップする (省電力動作を維持)。
extern bool TVPSteamOverlayAvailable;

// true なら main window 表示時にデバイスを作り直して overlay をフックさせる
// (Show() が消費する。実装: BasicDrawDevice.cpp / achievement_tjs.cpp)。
extern bool TVPSteamOverlayNeedsHook;

// SteamAPI_Init 成功後に呼ぶ。既存の D3D デバイスを作り直し、overlay の
// フック対象にする (実装: BasicDrawDevice.cpp)。
extern void TVPSteamOverlayHookDevices();

// オーバーレイの開閉に合わせてビデオ子ウィンドウ (krmovie) を隠す/戻す。
// 子ウィンドウはクライアント領域より常に手前に合成されるため、隠さないと
// オーバーレイがビデオの裏に隠れる (実装: BasicDrawDevice.cpp)。
extern void TVPSteamOverlaySetMovieWindowsVisible( bool visible );

#endif // __HAS_STEAM_API__
#endif // _steam_overlay_state_h_
