#include <iostream>
#include <cwctype>
#include <extension_tjs.hpp>

#ifdef __HAS_STEAM_API__
#include <steam_api.h>
#endif

namespace achievement_tjs 
{

	#ifdef __HAS_STEAM_API__
	class achievement_helper : public tTVPContinuousEventCallbackIntf
	{

		int64_t  m_appid{};
		bool    m_inited{};

	public:

		achievement_helper() noexcept;
		~achievement_helper() noexcept;

		auto    init() noexcept -> bool;
		auto destroy() noexcept -> void;

		auto set_achievement(const char* id) const noexcept -> bool;

		virtual void TJS_INTF_METHOD OnContinuousCallback(tjs_uint64 tick);

	};

	achievement_helper::~achievement_helper() noexcept { this->destroy(); }
	achievement_helper:: achievement_helper() noexcept {}

	auto TJS_INTF_METHOD achievement_helper::OnContinuousCallback(tjs_uint64 tick) -> void
	{
		::SteamAPI_RunCallbacks();
	}
	
	auto achievement_helper::init() noexcept -> bool 
	{
		if (this->m_inited)
		{
			return true;
		}

		this->m_inited = false;
		if (::SteamAPI_Init())
		{
			this->m_appid = ::SteamUtils()->GetAppID();
			if (::SteamUserStats() != nullptr && ::SteamUser() != nullptr)
			{
				const CSteamID steamid{ ::SteamUser()->GetSteamID() };
				this->m_inited = ::SteamUserStats()->RequestUserStats(steamid);
			}
		}
		if (this->m_inited)
		{
			ISteamUtils* const utils{ ::SteamUtils() };
			if (utils != nullptr) 
			{
				utils->SetOverlayNotificationPosition(::k_EPositionTopLeft);
			}
			::TVPAddContinuousEventHook(this);
		}
		return this->m_inited;
	}

	auto achievement_helper::destroy() noexcept -> void
	{
		this->m_inited = false;
		::TVPRemoveContinuousEventHook(this);
		::SteamAPI_Shutdown();
	}

	auto achievement_helper::set_achievement(const char* id) const noexcept -> bool
	{
		if (this->m_inited)
		{
			::SteamUserStats()->SetAchievement(id);
			return ::SteamUserStats()->StoreStats();
		}
		return false;
	}

	static std::unique_ptr<achievement_helper> helper{};

	static auto init_achievement_system() noexcept -> bool 
	{
		if (helper.get() == nullptr) 
		{
			helper = std::make_unique<achievement_helper>();
		}
		return helper->init();
	}

	static auto set_achievement(const char* id) noexcept -> bool 
	{
		if (helper.get() == nullptr) 
		{
			return false;
		}
		return helper->set_achievement(id);
	}
	#endif

	auto TJS_INTF_METHOD init_achievement_system(tTJSVariant* result, tjs_int, tTJSVariant**, iTJSDispatch2*) noexcept -> tjs_error
	{
		#ifdef __HAS_STEAM_API__
			*result = achievement_tjs::init_achievement_system();
		#else
			*result = false;
		#endif
		return TJS_S_OK;
	}

	auto TJS_INTF_METHOD set_achievement(tTJSVariant* result, tjs_int numparams, tTJSVariant** param, iTJSDispatch2*) noexcept -> tjs_error 
	{
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
						*result = false;
						return TJS_S_OK;
					}
					_name.push_back(static_cast<char>(chr));
				}
				*result = achievement_tjs::set_achievement(_name.c_str());
			}
			else 
			{
				*result = false;
			}
		#else
			*result = false;
		#endif
		return TJS_S_OK;
	}
}