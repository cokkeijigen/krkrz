#pragma once

namespace font_register_tjs
{
	auto TJS_INTF_METHOD registerPrivateFont  (tTJSVariant* result, tjs_int numparams, tTJSVariant** param, iTJSDispatch2* objthis) noexcept -> tjs_error;
	auto TJS_INTF_METHOD unregisterPrivateFont(tTJSVariant* result, tjs_int numparams, tTJSVariant** param, iTJSDispatch2* objthis) noexcept -> tjs_error;
	auto TJS_INTF_METHOD getPrivateFontName   (tTJSVariant* result, tjs_int numparams, tTJSVariant** param, iTJSDispatch2* objthis) noexcept -> tjs_error;
}