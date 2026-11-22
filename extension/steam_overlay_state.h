#pragma once
#ifndef _steam_overlay_state_h_
#define _steam_overlay_state_h_

#ifdef __HAS_STEAM_API__

extern bool TVPSteamOverlayActive;

// true once SteamAPI_Init succeeded
extern bool TVPSteamOverlayAvailable;

// recreate the device on next show so the overlay hooks it
extern bool TVPSteamOverlayNeedsHook;

// call after SteamAPI_Init succeeded
extern void TVPSteamOverlayHookDevices();

// hide or restore the video child windows while the overlay is open
extern void TVPSteamOverlaySetMovieWindowsVisible(bool visible);

#endif // __HAS_STEAM_API__
#endif // _steam_overlay_state_h_
