#pragma once
#ifndef _font_register_
#define _font_register_
#include <windows.h>
#include <string>
#include <span>

namespace winfont
{
	using fontid_t = uint64_t;

	extern auto register_private_font(std::wstring_view  file, bool as_memory, std::wstring_view alias = {}) noexcept -> fontid_t;
	extern auto register_private_font(std::span<uint8_t> data, std::wstring_view alias = {}) noexcept -> fontid_t;
	
	extern auto unregister_private_font() noexcept -> void;
	extern auto unregister_private_font(const fontid_t fontid) noexcept -> void;

	extern auto get_private_font_name(const fontid_t   fontid) noexcept -> std::wstring;
	extern auto get_private_font_name(std::wstring_view alias) noexcept -> std::wstring;

	extern auto get_private_font_path(const fontid_t   fontid) noexcept -> std::wstring;
	extern auto get_private_font_path(std::wstring_view alias) noexcept -> std::wstring;

	extern auto get_private_fontid(std::wstring_view alias) noexcept -> fontid_t;
}

#endif