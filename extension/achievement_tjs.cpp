#include "tjsCommHead.h"

#include <iostream>
#include <cstdint>
#include <cwctype>
#include <extension_tjs.hpp>
#include <tjsArray.h>
#include <tjsDictionary.h>
#include <steam_overlay_state.h>
#include "CharacterSet.h"

#ifdef __HAS_STEAM_API__
#include <steam_api.h>
#endif

#ifdef __HAS_STEAM_API__
bool TVPSteamOverlayActive    = false;
bool TVPSteamOverlayAvailable = false;
bool TVPSteamOverlayNeedsHook = false;
#endif

#ifdef __HAS_STEAM_API__

extern bool TVPPeriodicIdleWake;
extern void (*TVPSteamIdlePump)();

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
	public:

		struct achievement
		{
			std::string  name;
			std::wstring display_name;
			std::wstring description;
			bool         hidden{};
			bool         achieved{};
			uint32_t     unlock_time{};
		};

		achievement_helper () noexcept;
		~achievement_helper() noexcept;

		auto init_steam_early() noexcept -> bool;
		auto init()  noexcept -> bool;
		auto destroy() noexcept -> void;

		auto set_achievement  (const char* id) const noexcept -> bool;
		auto query_achievement(const char* id, bool always) const noexcept -> int;

		auto achievement_count(bool achieved_only, bool always) const noexcept -> int;
		auto get_achievements (bool always) const noexcept -> const std::vector<achievement>&;
		auto get_achievement(const char* id, bool always) const noexcept -> const achievement*;

		auto on_game_overlay_activated(GameOverlayActivated_t* param) -> void;
		auto on_user_stats_received   (UserStatsReceived_t* param, bool io_failure) -> void;

		virtual void TJS_INTF_METHOD OnContinuousCallback(tjs_uint64 tick);

		inline auto get_appid() noexcept -> ::AppId_t
		{
			return this->m_appid;
		}

	private:

		::AppId_t m_appid{};
		bool      m_steam_ready{};
		bool      m_steam_rejected{};
		bool      m_stats_ready{};
		bool      m_hooked{};

		mutable std::vector<achievement> m_achievements{};

		CCallbackManual<achievement_helper, GameOverlayActivated_t> m_overlay_cb{};
		CCallResult<achievement_helper, UserStatsReceived_t>        m_stats_cb{};

		auto refresh_achievement_cache() const noexcept -> void;
		auto request_user_stats()        noexcept -> void;
		auto sync_achievement(const char* id) const noexcept -> int;
	};

	achievement_helper::achievement_helper() noexcept
	{
	}

	achievement_helper::~achievement_helper() noexcept
	{
		this->destroy();
	}

	auto TJS_INTF_METHOD achievement_helper::OnContinuousCallback(tjs_uint64) -> void
	{
		if (this->m_steam_ready)
		{
			::SteamAPI_RunCallbacks();
		}
	}

	auto achievement_helper::on_game_overlay_activated(GameOverlayActivated_t* param) -> void
	{
		const bool active
		{
			param != nullptr && param->m_bActive != 0
		};
		TVPSteamOverlayActive = active;
		TVPSteamOverlaySetMovieWindowsVisible(!active);
	}

	auto achievement_helper::on_user_stats_received(UserStatsReceived_t* param, bool io_failure) -> void
	{
		if (io_failure || param == nullptr || param->m_eResult != ::k_EResultOK)
		{
			return;
		}
		//if (::CGameID(param->m_nGameID).AppID() != this->m_appid)
		//{
		//	return;
		//}
		if (::SteamUser() == nullptr || param->m_steamIDUser != ::SteamUser()->GetSteamID())
		{
			return;
		}

		this->refresh_achievement_cache();
	}

	auto achievement_helper::init_steam_early() noexcept -> bool
	{
		if (this->m_steam_ready)
		{
			return true;
		}
		if (this->m_steam_rejected)
		{
			return false;
		}

		this->m_stats_ready = false;
		if (!::SteamAPI_Init())
		{
			return false;
		}

		ISteamUtils* const utils{ ::SteamUtils() };
		if (utils == nullptr)
		{
			this->m_steam_rejected = true;
			::SteamAPI_Shutdown();
			return false;
		}

		this->m_appid       = utils->GetAppID();
		this->m_steam_ready = true;

		TVPSteamOverlayAvailable = true;
		TVPSteamOverlayNeedsHook = true;
		utils->SetOverlayNotificationPosition(::k_EPositionTopLeft);
		return true;
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
				this->m_stats_ready = true;
				this->refresh_achievement_cache();
				this->request_user_stats();
			}

			::TVPAddContinuousEventHook(this);
			TVPPeriodicIdleWake = true;
			TVPSteamIdlePump    = &TVPSteamIdlePumpImpl;

			this->m_overlay_cb.Register(this, &achievement_helper::on_game_overlay_activated);

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

			if (this->m_stats_cb.IsActive())
			{
				this->m_stats_cb.Cancel();
			}

			TVPSteamOverlayActive    = false;
			TVPSteamOverlayNeedsHook = false;
			TVPPeriodicIdleWake      = false;
			TVPSteamIdlePump         = nullptr;
			this->m_hooked           = false;
		}
		if (this->m_steam_ready)
		{
			this->m_steam_ready = false;
			this->m_stats_ready = false;
			TVPSteamOverlayAvailable = false;
			::SteamAPI_Shutdown();
		}
		this->m_achievements.clear();
	}

	auto achievement_helper::request_user_stats() noexcept -> void
	{
		ISteamUserStats* const stats{ ::SteamUserStats() };
		if (stats == nullptr || ::SteamUser() == nullptr)
		{
			return;
		}

		const SteamAPICall_t call{ stats->RequestUserStats(::SteamUser()->GetSteamID()) };
		if (call != ::k_uAPICallInvalid)
		{
			this->m_stats_cb.Set(call, this, &achievement_helper::on_user_stats_received);
		}
	}

	auto achievement_helper::refresh_achievement_cache() const noexcept -> void
	{
		ISteamUserStats* const stats{ ::SteamUserStats() };
		if (stats == nullptr)
		{
			return;
		}

		std::vector<achievement> cache{};
		try
		{
			const uint32 count{ stats->GetNumAchievements() };
			cache.reserve(count);

			for (uint32 i{}; i < count; ++i)
			{
				const char* const name{ stats->GetAchievementName(i) };
				if (name == nullptr)
				{
					continue;
				}

				achievement state{};
				state.name = name;

				const char* const display{ stats->GetAchievementDisplayAttribute(name, "name") };
				if (display != nullptr)
				{
					const tjs_int len{ ::TVPUtf8ToWideCharString(display, nullptr) };
					if (len > 0)
					{
						state.display_name.resize(static_cast<size_t>(len));
						::TVPUtf8ToWideCharString(display, state.display_name.data());
					}
				}

				const char* const description{ stats->GetAchievementDisplayAttribute(name, "desc") };
				if (description != nullptr)
				{
					const tjs_int len{ ::TVPUtf8ToWideCharString(description, nullptr) };
					if (len > 0)
					{
						state.description.resize(static_cast<size_t>(len));
						::TVPUtf8ToWideCharString(description, state.description.data());
					}
				}

				const char* const hidden{ stats->GetAchievementDisplayAttribute(name, "hidden") };
				state.hidden = hidden != nullptr && *hidden == '1';

				stats->GetAchievementAndUnlockTime(name, &state.achieved, &state.unlock_time);
				cache.push_back(std::move(state));
			}
		}
		catch (...)
		{
			return;
		}

		this->m_achievements = std::move(cache);
	}

	auto achievement_helper::sync_achievement(const char* id) const noexcept -> int
	{
		ISteamUserStats* const stats{ ::SteamUserStats() };
		if (stats == nullptr || id == nullptr)
		{
			return -1;
		}

		bool     achieved{};
		uint32_t unlock_time{};
		if (!stats->GetAchievementAndUnlockTime(id, &achieved, &unlock_time))
		{
			return -1;
		}

		for (achievement& state : this->m_achievements)
		{
			if (state.name == id)
			{
				state.achieved    = achieved;
				state.unlock_time = unlock_time;
				return achieved ? 1 : 0;
			}
		}

		this->refresh_achievement_cache();
		return achieved ? 1 : 0;
	}

	auto achievement_helper::set_achievement(const char* id) const noexcept -> bool
	{
		ISteamUserStats* const stats{ ::SteamUserStats() };
		if (!this->m_steam_ready || !this->m_stats_ready || id == nullptr || stats == nullptr)
		{
			return false;
		}
		if (!stats->SetAchievement(id))
		{
			return false;
		}

		this->sync_achievement(id);
		return stats->StoreStats();
	}

	auto achievement_helper::query_achievement(const char* id, bool always) const noexcept -> int
	{
		if (!this->m_steam_ready || !this->m_stats_ready || id == nullptr)
		{
			return -1;
		}

		if (always)
		{
			return this->sync_achievement(id);
		}

		for (const achievement& state : this->m_achievements)
		{
			if (state.name == id)
			{
				return state.achieved ? 1 : 0;
			}
		}
		return -1;
	}

	auto achievement_helper::get_achievement(const char* id, bool always) const noexcept -> const achievement_helper::achievement*
	{
		if (!this->m_steam_ready || !this->m_stats_ready || id == nullptr)
		{
			return nullptr;
		}
		if (always)
		{
			this->sync_achievement(id);
		}

		for (const achievement& state : this->m_achievements)
		{
			if (state.name == id)
			{
				return &state;
			}
		}
		return nullptr;
	}

	auto achievement_helper::achievement_count(bool achieved_only, bool always) const noexcept -> int
	{
		if (!this->m_steam_ready || !this->m_stats_ready)
		{
			return -1;
		}

		if (always) 
		{
			this->refresh_achievement_cache();
		}

		if (achieved_only) 
		{
			int result{};
			for (achievement& state : this->m_achievements)
			{
				if (state.achieved) 
				{
					++result;
				}
			}
			return result;
		}
		return static_cast<int>(this->m_achievements.size());
	}

	auto achievement_helper::get_achievements(bool always) const noexcept -> const std::vector<achievement_helper::achievement>&
	{
		if (always && this->m_steam_ready && this->m_stats_ready)
		{
			this->refresh_achievement_cache();
		}
		return this->m_achievements;
	}

	static achievement_helper helper{};

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

	auto TJS_INTF_METHOD uninit_achievement_system(tTJSVariant* result, tjs_int, tTJSVariant**, iTJSDispatch2*) noexcept -> tjs_error
	{
		#ifdef __HAS_STEAM_API__
		achievement_tjs::helper.destroy();
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
				const bool is_valid
				{
					(chr >= L'A' && chr <= L'Z') ||
					(chr >= L'a' && chr <= L'z') ||
					(chr >= L'0' && chr <= L'9') ||
					chr == L'_'
				};
				if (!is_valid)
				{
					if (result != nullptr)
					{
						*result = false;
					}
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

	auto TJS_INTF_METHOD query_achievement(tTJSVariant* result, tjs_int numparams, tTJSVariant** param, iTJSDispatch2*) noexcept -> tjs_error
	{
		if (result == nullptr)
		{
			return TJS_S_OK;
		}

		int32_t _result{ -1 };
		
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
				const bool is_valid
				{
					(chr >= L'A' && chr <= L'Z') ||
					(chr >= L'a' && chr <= L'z') ||
					(chr >= L'0' && chr <= L'9') ||
					chr == L'_'
				};
				if (!is_valid)
				{
					*result = -1;
					return TJS_S_OK;
				}
				_name.push_back(static_cast<char>(chr));
			}
			bool _always{};
			if(numparams >= 2 && param[1] != nullptr && static_cast<tjs_int>(*param[1]))
			{
				_always = true;
			}
			_result = achievement_tjs::helper.query_achievement(_name.c_str(), _always);
		}
		#endif

		*result = _result;

		return TJS_S_OK;
	}

	auto TJS_INTF_METHOD get_achievement(tTJSVariant* result, tjs_int numparams, tTJSVariant** param, iTJSDispatch2*) noexcept -> tjs_error 
	{
		if (result == nullptr)
		{
			return TJS_S_OK;
		}

		*result = tTJSVariant();

		#ifdef __HAS_STEAM_API__

		if (numparams < 1 || param == nullptr || param[0] == nullptr || param[0]->Type() != tvtString)
		{
			return TJS_S_OK;
		}

		const std::wstring_view id{ param[0]->GetString() };
		if (id.empty())
		{
			return TJS_S_OK;
		}

		std::string name{};
		try
		{
			name.reserve(id.size());

			for (wchar_t chr : id)
			{
				const bool is_valid
				{
					(chr >= L'A' && chr <= L'Z') ||
					(chr >= L'a' && chr <= L'z') ||
					(chr >= L'0' && chr <= L'9') ||
					chr == L'_'
				};
				if (!is_valid)
				{
					return TJS_S_OK;
				}
				name.push_back(static_cast<char>(chr));
			}
		}
		catch (...)
		{
			return TJS_S_OK;
		}

		const bool always
		{
			numparams >= 2 && param[1] != nullptr && static_cast<tjs_int>(*param[1]) != 0
		};

		const achievement_helper::achievement* const state
		{
			achievement_tjs::helper.get_achievement(name.c_str(), always)
		};
		if (state == nullptr)
		{
			return TJS_S_OK;
		}

		iTJSDispatch2* dict{ nullptr };
		try
		{
			dict = ::TJSCreateDictionaryObject();
			if (dict != nullptr)
			{
				const std::wstring wide(state->name.begin(), state->name.end());

				tTJSVariant value(wide.c_str());
				dict->PropSet(TJS_MEMBERENSURE, TJS_W("name"), nullptr, &value, dict);

				value = tTJSVariant(state->display_name.c_str());
				dict->PropSet(TJS_MEMBERENSURE, TJS_W("display_name"), nullptr, &value, dict);

				value = tTJSVariant(state->description.c_str());
				dict->PropSet(TJS_MEMBERENSURE, TJS_W("description"), nullptr, &value, dict);

				value = tTJSVariant(state->hidden ? 1 : 0);
				dict->PropSet(TJS_MEMBERENSURE, TJS_W("hidden"), nullptr, &value, dict);

				value = tTJSVariant(static_cast<tjs_int64>(state->unlock_time));
				dict->PropSet(TJS_MEMBERENSURE, TJS_W("unlock_time"), nullptr, &value, dict);

				value = tTJSVariant(state->achieved ? 1 : 0);
				dict->PropSet(TJS_MEMBERENSURE, TJS_W("achieved"), nullptr, &value, dict);
			}
		}
		catch (...)
		{
			if (dict != nullptr)
			{
				dict->Release();
			}
			dict = nullptr;
		}

		if (dict == nullptr)
		{
			return TJS_S_OK;
		}

		*result = tTJSVariant(dict, dict);
		dict->Release();
		#endif

		return TJS_S_OK;
	}

	auto TJS_INTF_METHOD achievement_count(tTJSVariant* result, tjs_int numparams, tTJSVariant** param, iTJSDispatch2*) noexcept -> tjs_error
	{
		if (result == nullptr)
		{
			return TJS_S_OK;
		}

		int32_t _result{ -1 };
		#ifdef __HAS_STEAM_API__

		bool _achieved_only{ false };
		if (numparams >= 1 && param[0] != nullptr && static_cast<tjs_int>(*param[0]))
		{
			_achieved_only = true;
		}

		bool _always{ false };
		if (numparams >= 2 && param[1]!= nullptr && static_cast<tjs_int>(*param[1]))
		{
			_always = true;
		}
		_result = achievement_tjs::helper.achievement_count(_achieved_only, _always);
		#endif

		*result = _result;

		return TJS_S_OK;
	}

	auto TJS_INTF_METHOD get_achievements(tTJSVariant* result, tjs_int numparams, tTJSVariant** param, iTJSDispatch2*) noexcept -> tjs_error
	{
		if (result == nullptr)
		{
			return TJS_S_OK;
		}

		iTJSDispatch2* array{ nullptr };
		try
		{
			array = ::TJSCreateArrayObject();
		}
		catch (...)
		{
			array = nullptr;
		}

		if (array == nullptr)
		{
			*result = tTJSVariant();
			return TJS_S_OK;
		}

		#ifdef __HAS_STEAM_API__

		const bool always
		{
			numparams >= 1 && param != nullptr && 
			param[0] != nullptr && static_cast<tjs_int>(*param[0]) != 0
		};
		tjs_uint index{};

		for (const achievement_helper::achievement& state : achievement_tjs::helper.get_achievements(always))
		{
			iTJSDispatch2* dic{ nullptr };
			try
			{
				dic = ::TJSCreateDictionaryObject();
				if (dic == nullptr)
				{
					continue;
				}

				const std::wstring name{ state.name.begin(), state.name.end() };

				tTJSVariant value(name.c_str());
				dic->PropSet(TJS_MEMBERENSURE, TJS_W("name"), nullptr, &value, dic);

				value = tTJSVariant(state.display_name.c_str());
				dic->PropSet(TJS_MEMBERENSURE, TJS_W("display_name"), nullptr, &value, dic);

				value = tTJSVariant(state.description.c_str());
				dic->PropSet(TJS_MEMBERENSURE, TJS_W("description"), nullptr, &value, dic);

				value = tTJSVariant(state.hidden ? 1 : 0);
				dic->PropSet(TJS_MEMBERENSURE, TJS_W("hidden"), nullptr, &value, dic);

				value = tTJSVariant(static_cast<tjs_int64>(state.unlock_time));
				dic->PropSet(TJS_MEMBERENSURE, TJS_W("unlock_time"), nullptr, &value, dic);

				value = tTJSVariant(state.achieved ? 1 : 0);
				dic->PropSet(TJS_MEMBERENSURE, TJS_W("achieved"), nullptr, &value, dic);

				value = tTJSVariant(dic, dic);
				array->PropSetByNum(TJS_MEMBERENSURE, index, &value, array);
				++index;
			}
			catch (...)
			{
			}

			if (dic != nullptr)
			{
				dic->Release();
			}
		}

		#endif

		*result = tTJSVariant(array, array);
		array->Release();

		return TJS_S_OK;
	}
}
