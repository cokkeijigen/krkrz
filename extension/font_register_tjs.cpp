#include <iostream>
#include <tjs.h>
#include <objidl.h>
#include <StorageImpl.h>
#include <font_register.hpp>
#include <font_register_tjs.hpp>

namespace font_register_tjs
{
	struct fonts
	{
		std::vector<ttstr> list{};

		inline auto add(const ttstr& path) noexcept -> void
		{
			this->list.push_back(path);
		}

		inline auto remove_last() noexcept -> void
		{
			if(this->list.empty())
			{
				return;
			}
			::DeleteFileW(this->list.back().c_str());
			this->list.pop_back();
		}

		inline auto remove(std::wstring_view path) noexcept -> void
		{
			if (path.empty() || this->list.empty())
			{
				return;
			}

			for (auto it = this->list.begin(); it != this->list.end(); it++)
			{
				std::wstring_view _path{ it->c_str(),  static_cast<size_t>(it->length()) };
				if (_path == path)
				{
					::DeleteFileW(it->c_str());
					this->list.erase(it);
					return;
				}
			}
		}

		inline auto remove_all()  noexcept -> void
		{
			if (this->list.empty())
			{
				return;
			}
			
			for (const ttstr& file : this->list)
			{
				::DeleteFileW(file.c_str());
			}
			this->list.clear();
		}

		~fonts() noexcept { this->remove_all(); }
	};

	static font_register_tjs::fonts temp_font_list{};

	/**
	 * @param  fontid
	 * @return void
	 */
	auto TJS_INTF_METHOD unregisterPrivateFont(tTJSVariant*, tjs_int numparams, tTJSVariant** param, iTJSDispatch2*) noexcept -> tjs_error
	{
		if (numparams >= 1)
		{
			const auto       fontid{ static_cast<winfont::fontid_t>((*param[0]).AsInteger()) };
			const std::wstring path{ winfont::get_private_font_path(fontid) };
			
			winfont::unregister_private_font(winfont::fontid_t(fontid));
			font_register_tjs::temp_font_list.remove(path);
		}
		else
		{
			winfont::unregister_private_font();
			font_register_tjs::temp_font_list.remove_all();
		}
		return TJS_S_OK;
	}

	/**
	 * @param  fontFileName
	 * @param  fontAlias
	 * @param  extract
	 * @return fontid
	 */
	auto TJS_INTF_METHOD registerPrivateFont(tTJSVariant* result, tjs_int numparams, tTJSVariant** param, iTJSDispatch2*) noexcept -> tjs_error
	{
		if (numparams < 1)
		{
			return TJS_E_BADPARAMCOUNT;
		}

		ttstr filename{ ::TVPGetPlacedPath(*param[0]) };

		if (!filename.IsEmpty())
		{
			ttstr localname{ ::TVPGetLocallyAccessibleName(filename) };
			if (!localname.IsEmpty())
			{
				std::wstring file{ localname.c_str(), static_cast<size_t>(localname.length()) }, alias{};
				if (numparams >= 2 && param[1]->Type() == tvtString)
				{
					ttstr _alias{ (*param[1]).AsString() };
					if (!_alias.IsEmpty())
					{
						alias.assign(_alias.c_str(), _alias.length());
					}
				}
				*result = static_cast<tjs_int64>(winfont::register_private_font(file, false, alias));
				return TJS_S_OK;
			}
			else
			{
				if (numparams >= 3 && static_cast<tjs_int>(*param[2]))
				{
					IStream* const in{ ::TVPCreateIStream(filename, TJS_BS_READ) };
					if (in != nullptr)
					{
						ttstr temp{ ::TVPGetTemporaryName() };
						const HANDLE hFile = ::CreateFileW
						(
							temp.c_str(),
							GENERIC_WRITE,
							FILE_SHARE_READ,
							NULL,
							CREATE_ALWAYS,
							FILE_ATTRIBUTE_NORMAL,
							NULL
						);

						if (hFile == INVALID_HANDLE_VALUE)
						{
							in->Release();
							*result = tjs_int64(0);
							return TJS_S_OK;
						}

						std::vector<uint8_t> buffer{};
						buffer.resize(1024 * 16);

						DWORD size{};
						while (in->Read(buffer.data(), buffer.size(), &size) == S_OK && size > 0)
						{
							::WriteFile(hFile, buffer.data(), size, &size, NULL);
						}

						::CloseHandle(hFile);
						in->Release();
						font_register_tjs::temp_font_list.add(temp);

						std::wstring file{ temp.c_str(), static_cast<size_t>(temp.length()) }, alias{};
						{
							ttstr _alias{ (*param[1]).AsString() };
							if (!_alias.IsEmpty())
							{
								alias.assign(_alias.c_str(), _alias.length());
							}
						}
						const winfont::fontid_t fontid{ winfont::register_private_font(file, false, alias)};
						if (static_cast<tjs_int64>(*result) == 0) 
						{
							font_register_tjs::temp_font_list.remove_last();
						}
						*result = static_cast<tjs_int64>(fontid);
					}
				}
				else
				{
					IStream* const in{ ::TVPCreateIStream(filename, TJS_BS_READ) };
					if (in != nullptr)
					{
						STATSTG stat{};
						in->Stat(&stat, STATFLAG_NONAME);

						std::vector<uint8_t> buffer{};
						buffer.resize(stat.cbSize.QuadPart);

						ULONG size{};
						if (in->Read(buffer.data(), buffer.size(), &size) == S_OK)
						{
							std::wstring alias{};
							if (numparams >= 2 && param[1]->Type() == tvtString)
							{
								ttstr _alias{ *param[1]->AsString() };
								if (!_alias.IsEmpty())
								{
									alias.assign(_alias.c_str(), _alias.length());
								}
							}
							*result = static_cast<tjs_int64>(winfont::register_private_font(buffer, alias));
							return TJS_S_OK;
						}
					}
				}
			}
		}

		*result = static_cast<tjs_int64>(0);
		return TJS_S_OK;
	}

	/**
	 * @param  fontid / alias
	 * @return String
	 */
	auto TJS_INTF_METHOD getPrivateFontName(tTJSVariant* result, tjs_int numparams, tTJSVariant** param, iTJSDispatch2*) noexcept -> tjs_error
	{
		if (numparams < 1)
		{
			return TJS_E_BADPARAMCOUNT;
		}

		switch (param[0]->Type())
		{
		case tvtString:
		{
			ttstr _alias{ (*param[0]).AsString() };
			if (_alias.IsEmpty())
			{
				*result = ttstr{};
			}
			else
			{
				std::wstring alias{ _alias.c_str(), static_cast<size_t>(_alias.length()) };
				std::wstring  name{ winfont::get_private_font_name(alias) };
				if (name.empty())
				{
					*result = ttstr{};
				}
				else
				{
					*result = ttstr{ name.c_str(), static_cast<int>(name.size()) };
				}
			}
			break;
		}
		case tvtInteger:
		{
			tTVInteger   value{ (*param[0]).AsInteger() };
			std::wstring  name{ winfont::get_private_font_name(winfont::fontid_t(value)) };
			if (name.empty())
			{
				*result = ttstr{};
			}
			else
			{
				*result = ttstr{ name.c_str(), static_cast<int>(name.size()) };
			}
			break;
		}
		default:
		{
			*result = ttstr{};
			break;
		}
		}
		return TJS_S_OK;
	}

}