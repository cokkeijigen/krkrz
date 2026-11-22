#include "tjsCommHead.h"

#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl.h>

#include <tjsNative.h>
#include <MsgIntf.h>
#include <StorageIntf.h>
#include <PluginImpl.h>
#include <extension_tjs.hpp>

#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Uuid.lib")

namespace storages_tjs
{
	static auto CALLBACK legacy_callback(HWND hwnd, UINT msg, LPARAM, LPARAM lpdata) noexcept -> int
	{
		if (msg != BFFM_INITIALIZED)
		{
			return {};
		}

		const auto initial{ reinterpret_cast<const ttstr*>(lpdata) };
		if (initial != nullptr && !initial->IsEmpty())
		{
			IShellFolder* desktop{};
			const bool   has_desktop
			{
				SUCCEEDED(::SHGetDesktopFolder(&desktop)) &&
				desktop != nullptr
			};
			if (has_desktop)
			{
				LPITEMIDLIST pidl{};
				const HRESULT hr_parse
				{
					desktop->ParseDisplayName
					(
						nullptr, nullptr,
						const_cast<LPWSTR>(initial->c_str()),
						nullptr, &pidl, nullptr
					)
				};
				const bool has_pidl{ SUCCEEDED(hr_parse) && pidl != nullptr };
				if (has_pidl)
				{
					::SendMessageW(hwnd, BFFM_SETSELECTIONW, FALSE, reinterpret_cast<LPARAM>(pidl));
					::CoTaskMemFree(pidl);
				}
				desktop->Release();
			}
		}

		::SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		return {};
	}

	auto TJS_INTF_METHOD select_folder(tTJSVariant* result, tjs_int numparams, tTJSVariant** param, iTJSDispatch2* objthis) noexcept -> tjs_error
	{
		if (numparams < 1 || param[0]->Type() != tvtObject)
		{
			return TJS_E_BADPARAMCOUNT;
		}

		iTJSDispatch2* const params{ param[0]->AsObjectNoAddRef() };
		if (params == nullptr)
		{
			return TJS_E_BADPARAMCOUNT;
		}

		ttstr       initial {};
		ttstr       title   {};
		ttstr       root_dir{};
		tTJSVariant val     {};

		{
			const bool has_name
			{
				TJS_SUCCEEDED(params->PropGet(0, TJS_W("name"), nullptr, &val, params)) &&
				!val.NormalCompare(ttstr{ TJS_W("") })
			};
			if (has_name)
			{
				initial = ttstr{ val };
				try
				{
					ttstr local{ TVPNormalizeStorageName(initial) };
					TVPGetLocalName(local);
					initial = local;
				}
				catch (...)
				{
				}
			}
		}

		if (TJS_SUCCEEDED(params->PropGet(0, TJS_W("title"), nullptr, &val, params)))
		{
			title = ttstr{ val };
		}

		if (TJS_SUCCEEDED(params->PropGet(0, TJS_W("rootDir"), nullptr, &val, params)))
		{
			root_dir = ttstr{ val };
			try
			{
				ttstr local{ TVPNormalizeStorageName(root_dir) };
				TVPGetLocalName(local);
				root_dir = local;
			}
			catch (...)
			{
			}
		}

		HWND owner{};
		{
			const bool has_window
			{
				TJS_SUCCEEDED(params->PropGet(0, TJS_W("window"), nullptr, &val, params)) &&
				val.Type() != tvtVoid
			};
			if (has_window)
			{
				iTJSDispatch2* const win{ val.AsObjectNoAddRef() };
				if (win != nullptr)
				{
					tTJSVariant hwnd_val{};
					if (TJS_SUCCEEDED(win->PropGet(0, TJS_W("HWND"), nullptr, &hwnd_val, win)))
					{
						const auto hwnd{ reinterpret_cast<HWND>(static_cast<tjs_intptr_t>(hwnd_val.AsInteger())) };
						if (hwnd != nullptr && ::IsWindow(hwnd) != FALSE)
						{
							owner = hwnd;
						}
					}
				}
			}
		}
		if (owner == nullptr)
		{
			const HWND hwnd{ TVPGetApplicationWindowHandle() };
			const bool is_valid_owner
			{
				hwnd != nullptr &&
				hwnd != INVALID_HANDLE_VALUE &&
				::IsWindow(hwnd) != FALSE
			};
			if (is_valid_owner)
			{
				owner = hwnd;
			}
		}

		const HRESULT hr_com{ ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED) };
		const bool    is_sta{ hr_com != RPC_E_CHANGED_MODE };

		IFileOpenDialog* dlg{};
		const bool need_modern{ is_sta && root_dir.IsEmpty() };
		if (need_modern)
		{
			const HRESULT hr_create
			{
				::CoCreateInstance
				(
					__uuidof(FileOpenDialog), nullptr, CLSCTX_INPROC_SERVER,
					__uuidof(IFileOpenDialog), reinterpret_cast<void**>(&dlg)
				)
			};
			if (FAILED(hr_create))
			{
				dlg = nullptr;
			}
		}

		bool  ok    {};
		ttstr picked{};

		if (dlg != nullptr)
		{
			DWORD options{};
			if (SUCCEEDED(dlg->GetOptions(&options)))
			{
				dlg->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
			}

			if (!title.IsEmpty())
			{
				dlg->SetTitle(title.c_str());
			}

			if (!initial.IsEmpty())
			{
				IShellItem* folder{};
				const HRESULT hr_item
				{
					::SHCreateItemFromParsingName
					(
						initial.c_str(), nullptr,
						__uuidof(IShellItem), reinterpret_cast<void**>(&folder)
					)
				};
				const bool has_folder{ SUCCEEDED(hr_item) && folder != nullptr };
				if (has_folder)
				{
					dlg->SetFolder(folder);
				}
				if (folder != nullptr)
				{
					folder->Release();
				}
			}

			if (SUCCEEDED(dlg->Show(owner)))
			{
				IShellItem* item{};
				const bool  has_result
				{
					SUCCEEDED(dlg->GetResult(&item)) &&
					item != nullptr
				};
				if (has_result)
				{
					LPWSTR path{};
					const bool has_path
					{
						SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) &&
						path != nullptr
					};
					if (has_path)
					{
						picked = path;
						::CoTaskMemFree(path);
						ok = true;
					}
					item->Release();
				}
			}
			dlg->Release();
		}
		else
		{
			BROWSEINFOW bi{};
			bi.hwndOwner = owner;
			bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

			if (!title.IsEmpty())
			{
				bi.lpszTitle = title.c_str();
			}

			if (!root_dir.IsEmpty())
			{
				IShellFolder* desktop{};
				const bool    has_desktop
				{
					SUCCEEDED(::SHGetDesktopFolder(&desktop)) &&
					desktop != nullptr
				};
				if (has_desktop)
				{
					LPITEMIDLIST root{};
					const HRESULT hr_parse
					{
						desktop->ParseDisplayName
						(
							nullptr, nullptr,
							const_cast<LPWSTR>(root_dir.c_str()),
							nullptr, &root, nullptr
						)
					};
					if (SUCCEEDED(hr_parse))
					{
						bi.pidlRoot = root;
					}
					desktop->Release();
				}
			}

			bi.lpfn   = legacy_callback;
			bi.lParam = reinterpret_cast<LPARAM>(&initial);

			LPITEMIDLIST pidl{ ::SHBrowseForFolderW(&bi) };
			if (pidl != nullptr)
			{
				IShellFolder* desktop{};
				const bool    has_desktop
				{
					SUCCEEDED(::SHGetDesktopFolder(&desktop)) &&
					desktop != nullptr
				};
				if (has_desktop)
				{
					STRRET strret{};
					const HRESULT hr_name
					{
						desktop->GetDisplayNameOf(pidl, SHGDN_FORPARSING, &strret)
					};
					if (SUCCEEDED(hr_name))
					{
						LPWSTR path{};
						const HRESULT hr_path{ ::StrRetToStrW(&strret, pidl, &path) };
						const bool    has_path{ SUCCEEDED(hr_path) && path != nullptr };
						if (has_path)
						{
							picked = path;
							ok     = true;
						}
						if (path != nullptr)
						{
							::CoTaskMemFree(path);
						}
					}
					desktop->Release();
				}
				::CoTaskMemFree(pidl);
			}
			if (bi.pidlRoot != nullptr)
			{
				::CoTaskMemFree(const_cast<LPITEMIDLIST>(bi.pidlRoot));
			}
		}

		if (hr_com == S_OK)
		{
			::CoUninitialize();
		}

		if (!ok)
		{
			if (result != nullptr)
			{
				*result = 0;
			}
			return TJS_S_OK;
		}

		tTJSVariant out{ TVPNormalizeStorageName(picked) };
		params->PropSet(TJS_MEMBERENSURE, TJS_W("name"), nullptr, &out, params);

		if (result != nullptr)
		{
			*result = TJS_S_TRUE;
		}
		return TJS_S_OK;
	}

	auto TJS_INTF_METHOD create_folders(tTJSVariant* result, tjs_int numparams, tTJSVariant** param, iTJSDispatch2* objthis) noexcept -> tjs_error
	{
		if (numparams < 1)
		{
			return TJS_E_BADPARAMCOUNT;
		}

		const ttstr name{ *param[0] };
		bool        ok  {};
		try
		{
			ttstr local{ TVPNormalizeStorageName(name) };
			TVPGetLocalName(local);
			ok = ::TVPCreateFolders(local);
		}
		catch (...)
		{
			ok = false;
		}

		if (result != nullptr)
		{
			*result = static_cast<tjs_int>(ok);
		}
		return TJS_S_OK;
	}
}
