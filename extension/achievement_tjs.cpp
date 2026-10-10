#include <iostream>
#include <cstdint>
#include <cwctype>
#include <extension_tjs.hpp>
#include <steam_overlay_state.h>

#ifdef __HAS_STEAM_API__
#include <steam_api.h>
#endif

#ifdef __HAS_STEAM_API__
bool TVPSteamOverlayActive    = false;
bool TVPSteamOverlayAvailable = false;
#endif

#ifdef __HAS_STEAM_API__

extern bool TVPPeriodicIdleWake;
extern void (*TVPSteamIdlePump)();

extern void (*TVPSteamEarlyInit)();

namespace
{
	void TVPSteamIdlePumpImpl()
	{
		::SteamAPI_RunCallbacks();
	}
}

#endif // __HAS_STEAM_API__

namespace achievement_tjs
{

	#ifdef __HAS_STEAM_API__
	class achievement_helper : public tTVPContinuousEventCallbackIntf
	{
		int64_t m_appid{};
		bool    m_steam_ready{};
		bool    m_stats_ready{};
		bool    m_hooked{};
		CCallbackManual<achievement_helper, GameOverlayActivated_t> m_overlay_cb;

	public:

		achievement_helper() noexcept;
		~achievement_helper() noexcept;
		auto init_steam_early() noexcept -> bool;

		auto    init() noexcept -> bool;
		auto destroy() noexcept -> void;

		auto set_achievement(const char* id) const noexcept -> bool;

		void OnGameOverlayActivated(GameOverlayActivated_t* pParam);

		virtual void TJS_INTF_METHOD OnContinuousCallback(tjs_uint64 tick);

	};

	achievement_helper::~achievement_helper() noexcept { this->destroy(); }
	achievement_helper:: achievement_helper() noexcept {}

	void achievement_helper::OnGameOverlayActivated(GameOverlayActivated_t* pParam)
	{
		const bool active = (pParam != nullptr && pParam->m_bActive != 0);
		TVPSteamOverlayActive = active;
	}

	auto TJS_INTF_METHOD achievement_helper::OnContinuousCallback(tjs_uint64) -> void
	{
		if (this->m_steam_ready)
		{
			::SteamAPI_RunCallbacks();
		}
	}
	auto achievement_helper::init_steam_early() noexcept -> bool
	{
		if (this->m_steam_ready)
		{
			return true;
		}

		this->m_stats_ready = false;
		if (::SteamAPI_Init())
		{
			this->m_steam_ready = true;
			TVPSteamOverlayAvailable = true;

			ISteamUtils* const utils{ ::SteamUtils() };
			if (utils != nullptr)
			{
				this->m_appid = utils->GetAppID();
				utils->SetOverlayNotificationPosition(::k_EPositionTopLeft);
			}
		}
		return this->m_steam_ready;
	}

	auto achievement_helper::init() noexcept -> bool
	{
		if (!this->m_steam_ready)
		{
			this->init_steam_early();
		}

		if (this->m_steam_ready && !this->m_hooked)
		{
			if (::SteamUserStats() != nullptr && ::SteamUser() != nullptr)
			{
				const CSteamID steamid{ ::SteamUser()->GetSteamID() };
				this->m_stats_ready = ::SteamUserStats()->RequestUserStats(steamid);
			}

			::TVPAddContinuousEventHook(this);
			TVPPeriodicIdleWake = true;
			TVPSteamIdlePump = &TVPSteamIdlePumpImpl;

			this->m_overlay_cb.Register(this, &achievement_helper::OnGameOverlayActivated);

			this->m_hooked = true;
		}
		return this->m_steam_ready;
	}

	auto achievement_helper::destroy() noexcept -> void
	{
		if (this->m_hooked)
		{
			::TVPRemoveContinuousEventHook(this);
			this->m_overlay_cb.Unregister();
			TVPSteamOverlayActive = false;
			TVPPeriodicIdleWake = false;
			TVPSteamIdlePump = NULL;
			this->m_hooked = false;
		}
		if (this->m_steam_ready)
		{
			this->m_steam_ready = false;
			this->m_stats_ready = false;
			TVPSteamOverlayAvailable = false;
			::SteamAPI_Shutdown();
		}
	}

	auto achievement_helper::set_achievement(const char* id) const noexcept -> bool
	{
		if (this->m_stats_ready && id != nullptr && ::SteamUserStats() != nullptr)
		{
			::SteamUserStats()->SetAchievement(id);
			return ::SteamUserStats()->StoreStats();
		}
		return false;
	}

	static achievement_helper helper{};

	namespace
	{
		void TVPSteamEarlyInitImpl()
		{
			helper.init_steam_early();
		}

		struct TVPSteamEarlyInitRegistrar
		{
			TVPSteamEarlyInitRegistrar()
			{
				::TVPSteamEarlyInit = &TVPSteamEarlyInitImpl;
			}
		} g_steam_early_init_registrar;
	}

	#endif // __HAS_STEAM_API__

	auto TJS_INTF_METHOD init_achievement_system(tTJSVariant* result, tjs_int, tTJSVariant**, iTJSDispatch2*) noexcept -> tjs_error
	{
		#ifdef __HAS_STEAM_API__
		if (result != nullptr)
		{
			*result = achievement_tjs::helper.init();
		}
		#else
		if (result != nullptr)
		{
			*result = false;
		}
		#endif
		return TJS_S_OK;
	}

	auto TJS_INTF_METHOD set_achievement(tTJSVariant* result, tjs_int numparams, tTJSVariant** param, iTJSDispatch2*) noexcept -> tjs_error 
	{
		bool _result{};
		#ifdef __HAS_STEAM_API__
			if (numparams < 1)
			{
				return TJS_E_BADPARAMCOUNT;
			}
		
			if (param[0]->Type() != tvtString)
			{
				return TJS_E_INVALIDPARAM;
			}
			const std::wstring_view name{ param[0]->GetString() };
			if (!name.empty()) 
			{
				std::string _name{};
				_name.reserve(name.size());
			
				for (wchar_t chr : name) 
				{
					if (!std::iswalpha(chr)) 
					{
						_result = false;
						return TJS_S_OK;
					}
					_name.push_back(static_cast<char>(chr));
				}
				
				_result = achievement_tjs::helper.set_achievement(_name.c_str());
			}
			else 
			{
				_result = false;
			}
		#endif
		
		if(result != nullptr) 
		{
			*result = _result;
		}

		return TJS_S_OK;
	}
}
