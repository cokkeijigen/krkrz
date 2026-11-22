#include <tjsCommHead.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <tjsNative.h>
#include <tjsArray.h>
#include <tjsDictionary.h>
#include <Extension.h>
#include <EventIntf.h>
#include <steam_overlay_state.h>
#include <CharacterSet.h>
#include <MsgIntf.h>

#ifdef __HAS_STEAM_API__
#include <steam_api.h>
#endif

#ifdef __HAS_STEAM_API__
bool TVPSteamOverlayActive   {};
bool TVPSteamOverlayAvailable{};
bool TVPSteamOverlayNeedsHook{};
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

namespace steam_helper_tjs
{

	#ifdef __HAS_STEAM_API__
	class steam_helper : public tTVPContinuousEventCallbackIntf
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

		steam_helper () noexcept;
		~steam_helper() noexcept;

		auto init_steam_early() noexcept -> bool;
		auto init()  noexcept -> bool;
		auto destroy() noexcept -> void;

		auto set_achievement  (const char* id) const noexcept -> bool;
		auto clear_achievement(const char* id) const noexcept -> bool;
		auto clear_achievements() const noexcept -> int;
		auto query_achievement(const char* id, bool always) const noexcept -> int;
		auto current_app_id() const noexcept -> uint32;

		auto achievement_count(bool achieved_only, bool always) const noexcept -> int;
		auto get_achievements (bool always) const noexcept -> const std::vector<achievement>&;
		auto get_achievement(const char* id, bool always) const noexcept -> const achievement*;

		auto trigger_screenshot() const noexcept -> bool;

		auto on_game_overlay_activated(GameOverlayActivated_t* param) -> void;
		auto on_user_stats_received   (UserStatsReceived_t* param, bool io_failure) -> void;

		virtual auto TJS_INTF_METHOD OnContinuousCallback(tjs_uint64 tick) -> void override;

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

		CCallbackManual<steam_helper, GameOverlayActivated_t> m_overlay_cb{};
		CCallResult<steam_helper, UserStatsReceived_t>        m_stats_cb{};

		auto refresh_achievement_cache() const noexcept -> void;
		auto request_user_stats()        noexcept -> void;
		auto sync_achievement(const char* id) const noexcept -> int;
	};
	#endif // __HAS_STEAM_API__

	#ifdef __HAS_STEAM_API__
	steam_helper::steam_helper() noexcept
	{
	}

	steam_helper::~steam_helper() noexcept
	{
		this->destroy();
	}

	auto TJS_INTF_METHOD steam_helper::OnContinuousCallback(tjs_uint64) -> void
	{
		if (this->m_steam_ready)
		{
			::SteamAPI_RunCallbacks();
		}
	}

	auto steam_helper::on_game_overlay_activated(GameOverlayActivated_t* param) -> void
	{
		const bool active
		{
			param != nullptr && param->m_bActive != 0
		};

		TVPSteamOverlayActive = active;
		TVPSteamOverlaySetMovieWindowsVisible(!active);
	}

	auto steam_helper::on_user_stats_received(UserStatsReceived_t* param, bool io_failure) -> void
	{
		if (io_failure || param == nullptr || param->m_eResult != ::k_EResultOK)
		{
			return;
		}
		if (::SteamUser() == nullptr || param->m_steamIDUser != ::SteamUser()->GetSteamID())
		{
			return;
		}

		this->refresh_achievement_cache();
	}

	auto steam_helper::init_steam_early() noexcept -> bool
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

	auto steam_helper::init() noexcept -> bool
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

			this->m_overlay_cb.Register(this, &steam_helper::on_game_overlay_activated);

			this->m_hooked = true;
		}
		return this->m_steam_ready;
	}

	auto steam_helper::destroy() noexcept -> void
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

	auto steam_helper::request_user_stats() noexcept -> void
	{
		ISteamUserStats* const stats{ ::SteamUserStats() };
		if (stats == nullptr || ::SteamUser() == nullptr)
		{
			return;
		}

		const SteamAPICall_t call{ stats->RequestUserStats(::SteamUser()->GetSteamID()) };
		if (call != ::k_uAPICallInvalid)
		{
			this->m_stats_cb.Set(call, this, &steam_helper::on_user_stats_received);
		}
	}

	auto steam_helper::refresh_achievement_cache() const noexcept -> void
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

	auto steam_helper::sync_achievement(const char* id) const noexcept -> int
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

	auto steam_helper::set_achievement(const char* id) const noexcept -> bool
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

	auto steam_helper::clear_achievement(const char* id) const noexcept -> bool
	{
		ISteamUserStats* const stats{ ::SteamUserStats() };
		if (!this->m_steam_ready || !this->m_stats_ready || id == nullptr || stats == nullptr)
		{
			return false;
		}
		if (!stats->ClearAchievement(id))
		{
			return false;
		}

		this->sync_achievement(id);
		return stats->StoreStats();
	}

	auto steam_helper::clear_achievements() const noexcept -> int
	{
		ISteamUserStats* const stats{ ::SteamUserStats() };
		if (!this->m_steam_ready || !this->m_stats_ready || stats == nullptr)
		{
			return 0;
		}

		const uint32 count{ stats->GetNumAchievements() };
		int cleared{};
		for (uint32 i{}; i < count; ++i)
		{
			const char* const name{ stats->GetAchievementName(i) };
			if (name == nullptr)
			{
				continue;
			}
			if (stats->ClearAchievement(name))
			{
				++cleared;
			}
		}

		stats->StoreStats();
		this->refresh_achievement_cache();
		return cleared;
	}

	auto steam_helper::current_app_id() const noexcept -> uint32
	{
		return static_cast<uint32>(this->m_appid);
	}

	auto steam_helper::query_achievement(const char* id, bool always) const noexcept -> int
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

	auto steam_helper::get_achievement(const char* id, bool always) const noexcept -> const steam_helper::achievement*
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

	auto steam_helper::achievement_count(bool achieved_only, bool always) const noexcept -> int
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

	auto steam_helper::get_achievements(bool always) const noexcept -> const std::vector<steam_helper::achievement>&
	{
		if (always && this->m_steam_ready && this->m_stats_ready)
		{
			this->refresh_achievement_cache();
		}
		return this->m_achievements;
	}

	auto steam_helper::trigger_screenshot() const noexcept -> bool
	{
		ISteamScreenshots* const shots{ ::SteamScreenshots() };
		if (!this->m_steam_ready || shots == nullptr)
		{
			return false;
		}
		shots->TriggerScreenshot();
		return true;
	}

	static steam_helper helper{};

	// caller releases the returned object
	static auto create_achievement_dict(const steam_helper::achievement * state) noexcept -> iTJSDispatch2*
	{
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

		return dict;
	}

	static auto to_steam_achievement_id(const tTJSVariant& value, std::string& out) noexcept -> bool
	{
		if (value.Type() != tvtString)
		{
			return false;
		}

		const std::wstring_view name{ value.GetString() };
		if (name.empty())
		{
			return false;
		}

		std::string id{};
		try
		{
			id.reserve(name.size());

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
					return false;
				}
				id.push_back(static_cast<char>(chr));
			}
		}
		catch (...)
		{
			return false;
		}

		out = std::move(id);
		return true;
	}

	#endif // __HAS_STEAM_API__

	static auto TJS_INTF_METHOD init(tTJSVariant* result, tjs_int, tTJSVariant**, iTJSDispatch2*) noexcept -> tjs_error
	{
		#ifdef __HAS_STEAM_API__
		if (result != nullptr)
		{
			*result = helper.init();
		}
		#else
		if (result != nullptr)
		{
			*result = false;
		}
		#endif
		return TJS_S_OK;
	}

	static auto TJS_INTF_METHOD set_achievement(tTJSVariant* result, tjs_int numparams, tTJSVariant** param, iTJSDispatch2*) noexcept -> tjs_error
	{
		tjs_int _result{ 0 };
		#ifdef __HAS_STEAM_API__
		if (numparams < 1 || param == nullptr || param[0] == nullptr)
		{
			return TJS_E_BADPARAMCOUNT;
		}

		if (param[0]->Type() != tvtString)
		{
			return TJS_E_INVALIDPARAM;
		}

		std::string id{};
		if (to_steam_achievement_id(*param[0], id))
		{
			_result = helper.set_achievement(id.c_str()) ? 1 : 0;
		}
		#endif

		if (result != nullptr)
		{
			*result = _result;
		}

		return TJS_S_OK;
	}

	static auto TJS_INTF_METHOD clear_achievement(tTJSVariant* result, tjs_int numparams, tTJSVariant** param, iTJSDispatch2*) noexcept -> tjs_error
	{
		tjs_int _result{ 0 };
		#ifdef __HAS_STEAM_API__
		if (numparams < 1 || param == nullptr || param[0] == nullptr)
		{
			return TJS_E_BADPARAMCOUNT;
		}

		if (param[0]->Type() != tvtString)
		{
			return TJS_E_INVALIDPARAM;
		}

		std::string id{};
		if (to_steam_achievement_id(*param[0], id))
		{
			_result = helper.clear_achievement(id.c_str()) ? 1 : 0;
		}
		#endif

		if (result != nullptr)
		{
			*result = _result;
		}

		return TJS_S_OK;
	}

	static auto TJS_INTF_METHOD clear_achievements(tTJSVariant* result, tjs_int, tTJSVariant**, iTJSDispatch2*) noexcept -> tjs_error
	{
		tjs_int _result{ 0 };
		#ifdef __HAS_STEAM_API__
		_result = helper.clear_achievements();
		#endif

		if (result != nullptr)
		{
			*result = _result;
		}

		return TJS_S_OK;
	}

	static auto TJS_INTF_METHOD current_app_id(tTJSVariant* result, tjs_int, tTJSVariant**, iTJSDispatch2*) noexcept -> tjs_error
	{
		tjs_int64 _result{ 0 };
		#ifdef __HAS_STEAM_API__
		_result = static_cast<tjs_int64>(helper.current_app_id());
		#endif

		if (result != nullptr)
		{
			*result = _result;
		}

		return TJS_S_OK;
	}

	static auto TJS_INTF_METHOD restart_app_if_necessary(tTJSVariant* result, tjs_int numparams, tTJSVariant** param, iTJSDispatch2*) noexcept -> tjs_error
	{
		tjs_int _result{ 0 };
		#ifdef __HAS_STEAM_API__
		if (numparams < 1 || param == nullptr || param[0] == nullptr)
		{
			return TJS_E_BADPARAMCOUNT;
		}

		const tjs_int64 appid{ static_cast<tjs_int64>(*param[0]) };
		if (appid <= 0)
		{
			return TJS_E_INVALIDPARAM;
		}

		_result = ::SteamAPI_RestartAppIfNecessary(static_cast<uint32>(appid)) ? 1 : 0;
		#endif

		if (result != nullptr)
		{
			*result = _result;
		}

		return TJS_S_OK;
	}

	static auto TJS_INTF_METHOD query_achievement_state(tTJSVariant* result, tjs_int numparams, tTJSVariant** param, iTJSDispatch2*) noexcept -> tjs_error
	{
		if (result == nullptr)
		{
			return TJS_S_OK;
		}

		int32_t _result{ -1 };

		#ifdef __HAS_STEAM_API__
		if (numparams < 1 || param == nullptr || param[0] == nullptr)
		{
			return TJS_E_BADPARAMCOUNT;
		}

		if (param[0]->Type() != tvtString)
		{
			return TJS_E_INVALIDPARAM;
		}

		std::string id{};
		if (to_steam_achievement_id(*param[0], id))
		{
			const bool always
			{
				numparams >= 2 && param[1] != nullptr && static_cast<tjs_int>(*param[1]) != 0
			};
			_result = helper.query_achievement(id.c_str(), always);
		}
		#endif

		*result = _result;

		return TJS_S_OK;
	}

	static auto TJS_INTF_METHOD get_achievement_info(tTJSVariant* result, tjs_int numparams, tTJSVariant** param, iTJSDispatch2*) noexcept -> tjs_error
	{
		if (result == nullptr)
		{
			return TJS_S_OK;
		}

		*result = tTJSVariant();

		#ifdef __HAS_STEAM_API__

		if (numparams < 1 || param == nullptr || param[0] == nullptr)
		{
			return TJS_S_OK;
		}

		std::string id{};
		if (!to_steam_achievement_id(*param[0], id))
		{
			return TJS_S_OK;
		}

		const bool always
		{
			numparams >= 2 && param[1] != nullptr && static_cast<tjs_int>(*param[1]) != 0
		};

		const steam_helper::achievement* const state{ helper.get_achievement(id.c_str(), always) };
		if (state == nullptr)
		{
			return TJS_S_OK;
		}

		iTJSDispatch2* const dict{ create_achievement_dict(state) };
		if (dict == nullptr)
		{
			return TJS_S_OK;
		}

		*result = tTJSVariant(dict, dict);
		dict->Release();
		#endif

		return TJS_S_OK;
	}

	static auto TJS_INTF_METHOD count_achievements(tTJSVariant* result, tjs_int numparams, tTJSVariant** param, iTJSDispatch2*) noexcept -> tjs_error
	{
		if (result == nullptr)
		{
			return TJS_S_OK;
		}

		int32_t _result{ -1 };
		#ifdef __HAS_STEAM_API__

		const bool _achieved_only
		{
			numparams >= 1 && param != nullptr && param[0] != nullptr && static_cast<tjs_int>(*param[0]) != 0
		};

		const bool _always
		{
			numparams >= 2 && param[1] != nullptr && static_cast<tjs_int>(*param[1]) != 0
		};

		_result = helper.achievement_count(_achieved_only, _always);
		#endif

		*result = _result;

		return TJS_S_OK;
	}

	static auto TJS_INTF_METHOD get_achievements(tTJSVariant* result, tjs_int numparams, tTJSVariant** param, iTJSDispatch2*) noexcept -> tjs_error
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
			numparams >= 1 && param != nullptr && param[0] != nullptr && static_cast<tjs_int>(*param[0]) != 0
		};
		tjs_uint index{};

		for (const steam_helper::achievement& state : helper.get_achievements(always))
		{
			iTJSDispatch2* const dic{ create_achievement_dict(&state) };
			if (dic == nullptr)
			{
				continue;
			}

			try
			{
				tTJSVariant value(dic, dic);
				array->PropSetByNum(TJS_MEMBERENSURE, index, &value, array);
				++index;
			}
			catch (...)
			{
			}

			dic->Release();
		}

		#endif

		*result = tTJSVariant(array, array);
		array->Release();

		return TJS_S_OK;
	}

	static auto TJS_INTF_METHOD trigger_screenshot(tTJSVariant* result, tjs_int, tTJSVariant**, iTJSDispatch2*) noexcept -> tjs_error
	{
		bool _result{};
		#ifdef __HAS_STEAM_API__
		_result = helper.trigger_screenshot();
		#endif

		if (result != nullptr)
		{
			*result = _result;
		}

		return TJS_S_OK;
	}
}

//---------------------------------------------------------------------------
// tTJSNC_SteamHelper : TJS SteamHelper class
//---------------------------------------------------------------------------
class tTJSNC_SteamHelper : public tTJSNativeClass
{
	typedef tTJSNativeClass inherited;

public:
	tTJSNC_SteamHelper();
	static tjs_uint32 ClassID;

protected:
	tTJSNativeInstance *CreateNativeInstance();
};

tjs_uint32 tTJSNC_SteamHelper::ClassID = -1;

tTJSNC_SteamHelper::tTJSNC_SteamHelper() : inherited(TJS_W("SteamHelper"))
{
	TJS_BEGIN_NATIVE_MEMBERS(/*TJS class name*/SteamHelper)
	TJS_DECL_EMPTY_FINALIZE_METHOD
//----------------------------------------------------------------------
	TJS_BEGIN_NATIVE_METHOD_DECL(/*func. name*/init)
	{
		return steam_helper_tjs::init(result, numparams, param, objthis);
	}
	TJS_END_NATIVE_STATIC_METHOD_DECL(/*func. name*/init)
//----------------------------------------------------------------------
	TJS_BEGIN_NATIVE_METHOD_DECL(/*func. name*/setAchievement)
	{
		return steam_helper_tjs::set_achievement(result, numparams, param, objthis);
	}
	TJS_END_NATIVE_STATIC_METHOD_DECL(/*func. name*/setAchievement)
//----------------------------------------------------------------------
	TJS_BEGIN_NATIVE_METHOD_DECL(/*func. name*/clearAchievement)
	{
		return steam_helper_tjs::clear_achievement(result, numparams, param, objthis);
	}
	TJS_END_NATIVE_STATIC_METHOD_DECL(/*func. name*/clearAchievement)
//----------------------------------------------------------------------
	TJS_BEGIN_NATIVE_METHOD_DECL(/*func. name*/clearAchievements)
	{
		return steam_helper_tjs::clear_achievements(result, numparams, param, objthis);
	}
	TJS_END_NATIVE_STATIC_METHOD_DECL(/*func. name*/clearAchievements)
//----------------------------------------------------------------------
	TJS_BEGIN_NATIVE_METHOD_DECL(/*func. name*/currentAppId)
	{
		return steam_helper_tjs::current_app_id(result, numparams, param, objthis);
	}
	TJS_END_NATIVE_STATIC_METHOD_DECL(/*func. name*/currentAppId)
//----------------------------------------------------------------------
	TJS_BEGIN_NATIVE_METHOD_DECL(/*func. name*/restartAppIfNecessary)
	{
		return steam_helper_tjs::restart_app_if_necessary(result, numparams, param, objthis);
	}
	TJS_END_NATIVE_STATIC_METHOD_DECL(/*func. name*/restartAppIfNecessary)
//----------------------------------------------------------------------
	TJS_BEGIN_NATIVE_METHOD_DECL(/*func. name*/queryAchievementState)
	{
		return steam_helper_tjs::query_achievement_state(result, numparams, param, objthis);
	}
	TJS_END_NATIVE_STATIC_METHOD_DECL(/*func. name*/queryAchievementState)
//----------------------------------------------------------------------
	TJS_BEGIN_NATIVE_METHOD_DECL(/*func. name*/getAchievementInfo)
	{
		return steam_helper_tjs::get_achievement_info(result, numparams, param, objthis);
	}
	TJS_END_NATIVE_STATIC_METHOD_DECL(/*func. name*/getAchievementInfo)
//----------------------------------------------------------------------
	TJS_BEGIN_NATIVE_METHOD_DECL(/*func. name*/getAchievements)
	{
		return steam_helper_tjs::get_achievements(result, numparams, param, objthis);
	}
	TJS_END_NATIVE_STATIC_METHOD_DECL(/*func. name*/getAchievements)
//----------------------------------------------------------------------
	TJS_BEGIN_NATIVE_METHOD_DECL(/*func. name*/countAchievements)
	{
		return steam_helper_tjs::count_achievements(result, numparams, param, objthis);
	}
	TJS_END_NATIVE_STATIC_METHOD_DECL(/*func. name*/countAchievements)
//----------------------------------------------------------------------
	TJS_BEGIN_NATIVE_METHOD_DECL(/*func. name*/triggerScreenshot)
	{
		return steam_helper_tjs::trigger_screenshot(result, numparams, param, objthis);
	}
	TJS_END_NATIVE_STATIC_METHOD_DECL(/*func. name*/triggerScreenshot)
	TJS_END_NATIVE_MEMBERS
}
//---------------------------------------------------------------------------
tTJSNativeInstance * tTJSNC_SteamHelper::CreateNativeInstance()
{
	TVPThrowExceptionMessage(TVPCannotCreateInstance);

	return nullptr;
}
//---------------------------------------------------------------------------
static tTJSNativeClass * TVPCreateNativeClass_SteamHelper()
{
	return new tTJSNC_SteamHelper();
}
//---------------------------------------------------------------------------
static iTJSDispatch2 * TVPInstallSteamHelperClass(iTJSDispatch2 *)
{
	return TVPCreateNativeClass_SteamHelper();
}
//---------------------------------------------------------------------------
static tTVPAtInstallClass TVPInstallClassSteamHelper
	(TJS_W("SteamHelper"), TVPInstallSteamHelperClass, TJS_W(""));