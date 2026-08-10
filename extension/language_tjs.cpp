#include <iostream>
#include <cwctype>
#include <windows.h>
#include <extension_tjs.hpp>

namespace language_tjs
{

	auto TJS_INTF_METHOD get_language(tTJSVariant* result, tjs_int, tTJSVariant**, iTJSDispatch2*) noexcept -> tjs_error 
	{
		const LANGID   lang_id{ ::GetUserDefaultUILanguage() };
		const int primary_lang{ PRIMARYLANGID(lang_id)       };
		const int     sub_lang{ SUBLANGID(lang_id)           };

		switch (primary_lang)
		{
		case LANG_CHINESE: 
		{
			if (sub_lang == SUBLANG_CHINESE_SIMPLIFIED || sub_lang == SUBLANG_CHINESE_SINGAPORE)
			{
				*result = TJS_W("schinese");
			}
			else 
			{
				*result = TJS_W("tchinese");
			}
			break;
		}
		case LANG_ENGLISH: 
		{
			*result = TJS_W("english");
			break;
		}
		case LANG_JAPANESE: 
		{
			*result = TJS_W("japanese");
			break;
		}
		default: 
		{
			wchar_t buffer[LOCALE_NAME_MAX_LENGTH]{};
			if (::GetLocaleInfoW(MAKELCID(lang_id, SORT_DEFAULT), LOCALE_SENGLISHLANGUAGENAME, buffer, LOCALE_NAME_MAX_LENGTH) > 0)
			{
				int i{};
				while (buffer[i] != L'\0')
				{
					buffer[i] = std::towlower(buffer[i]);
					if (++i >= LOCALE_NAME_MAX_LENGTH) 
					{
						break;
					}
				}
				*result = ttstr{ buffer, i - 1 };
			}
			else
			{
				*result = TJS_W("unknown");
			}
			break;
		}
		}
		return TJS_S_OK;
	}
}