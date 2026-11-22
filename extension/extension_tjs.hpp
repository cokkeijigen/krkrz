#pragma once
#ifndef _extension_tjs_
#define _extension_tjs_
#include <tjs.h>
#include <objidl.h>
#include <EventIntf.h>
#include <StorageImpl.h>
#include <DebugIntf.h>
#include <tjsNative.h>

namespace language_tjs
{
	extern auto TJS_INTF_METHOD get_language(tTJSVariant* result, tjs_int numparams, tTJSVariant** param, iTJSDispatch2* objthis) noexcept -> tjs_error;
}

namespace font_register_tjs
{
	extern auto TJS_INTF_METHOD register_private_font(tTJSVariant* result, tjs_int numparams, tTJSVariant** param, iTJSDispatch2*   objthis) noexcept -> tjs_error;
	extern auto TJS_INTF_METHOD unregister_private_font(tTJSVariant* result, tjs_int numparams, tTJSVariant** param, iTJSDispatch2* objthis) noexcept -> tjs_error;
	extern auto TJS_INTF_METHOD get_private_font_name(tTJSVariant* result, tjs_int numparams, tTJSVariant** param, iTJSDispatch2*   objthis) noexcept -> tjs_error;
}

namespace storages_tjs
{
	extern auto TJS_INTF_METHOD select_folder(tTJSVariant* result, tjs_int numparams, tTJSVariant** param, iTJSDispatch2* objthis) noexcept -> tjs_error;
	extern auto TJS_INTF_METHOD create_folders(tTJSVariant* result, tjs_int numparams, tTJSVariant** param, iTJSDispatch2* objthis) noexcept -> tjs_error;
}

#endif