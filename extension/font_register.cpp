
#include <font_register.hpp>
#include <windows.h>
#include <algorithm>
#include <unordered_map>
#include <optional>
#include <string_view>
#include <vector>
#include <iostream>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace winfont
{
	struct entry
	{
		fontid_t   fontid;
		std::wstring path;
		std::wstring name;
		HANDLE     handle;
	};
	
	static std::vector<entry> FONTS{};
	static std::unordered_map<std::wstring, uint64_t> FONT_ALIAS{};

	static auto strhash(std::wstring_view str) noexcept -> std::optional<uint64_t>
	{
		if (str.empty()) 
		{
			return std::nullopt;
		}

		constexpr wchar_t whitespace[]{ L" \t\n\r\v\f" };
		auto  start{ str.find_first_not_of(whitespace) };
		if (start == std::wstring_view::npos)
		{
			return std::nullopt;
		}

		auto end{ str.find_last_not_of(whitespace) };
		str = str.substr(start, end - start + 1);
		if (str.empty())
		{
			return std::nullopt;
		}

		constexpr uint64_t  fnv1a_prime{ 0x00000100000001B3 };
		constexpr uint64_t fnv1a_offset{ 0xCBF29CE484222325 };
		
		uint64_t hash{ fnv1a_offset };
		for (const wchar_t chr : str)
		{
			hash ^= static_cast<uint64_t>(chr);
			hash *= fnv1a_prime;
		}
		return hash;
	}

	static inline auto read_u16_be(const uint8_t* p) noexcept -> uint16_t
	{
		const auto result 
		{
			(static_cast<uint16_t>(p[0]) << 8) | static_cast<uint16_t>(p[1])
		};
		return static_cast<uint16_t>(result);
	}

	static inline auto read_u32_be(const uint8_t* p) noexcept -> uint32_t
	{
		return uint32_t
		{
			(static_cast<uint32_t>(p[0]) << 24) | 
			(static_cast<uint32_t>(p[1]) << 16) |
			(static_cast<uint32_t>(p[2]) <<  8) |
			 static_cast<uint32_t>(p[3])
		};
	}

	static auto score_name_quality(const std::wstring& str) noexcept -> int
	{
		if (str.empty())
		{
			return -1000; 
		}

		int ascii_alnum{}, ascii_print{}, cjk_count{}, other_uni{}, bad{};
		for (wchar_t ch : str)
		{
			if ((ch >= L'A' && ch <= L'Z') || (ch >= L'a' && ch <= L'z') || (ch >= L'0' && ch <= L'9')) 
			{
				++ascii_alnum;
			}
			else if (ch >= 0x0020 && ch <= 0x007E) 
			{
				++ascii_print;
			}
			else if 
			(
				(ch >= 0x3040 && ch <= 0x30FF) || (ch >= 0x3400 && ch <= 0x9FFF) ||
				(ch >= 0xAC00 && ch <= 0xD7AF) || (ch >= 0xF900 && ch <= 0xFAFF)
			){
				++cjk_count;
			}
			else if 
			(
				(ch >= 0x00A0 && ch <= 0x1FFF) || (ch >= 0x2000 && ch <= 0x2FFF) || (ch >= 0x3000 && ch <= 0x303F) ||
				(ch >= 0x3100 && ch <= 0x31FF) || (ch >= 0xFB00 && ch <= 0xFFEF)
			)
			{
				++other_uni;
			}
			else 
			{
				++bad;
			}
		}

		if (bad > 0)
		{
			return -500 * bad;
		}

		const int total{ ascii_alnum + ascii_print + cjk_count + other_uni };
		if (total == 0) 
		{ 
			return -1000;
		}

		int score{};
		const double ascii_ratio{ (ascii_alnum + ascii_print) / static_cast<double>(total) };
		const double   cjk_ratio{ cjk_count / static_cast<double>(total) };
		if (ascii_ratio >= 0.85) 
		{
			score += 500 + ascii_alnum * 3;
		}
		else if (cjk_ratio >= 0.25) 
		{
			score += 400 + cjk_count * 4;
		}
		else
		{
			score -= 200;
			score += ascii_alnum;
		}

		if (ascii_alnum + cjk_count < 2) 
		{
			score -= 300;
		}
		return score;
	}

	static auto parse_name_table(std::span<uint8_t> name_table_data) noexcept -> std::wstring
	{
		if (name_table_data.size() < 6)
		{
			return {};
		}

		const uint8_t*   name_base{ name_table_data.data() };
		const uint32_t name_length{ static_cast<uint32_t>(name_table_data.size()) };

		const uint16_t  record_count{ read_u16_be(name_base + 2) };
		const uint16_t string_offset{ read_u16_be(name_base + 4) };
		if (name_length < 6u + static_cast<size_t>(record_count) * 12u) 
		{
			return {};
		}

		constexpr int NAME_ID_FULL_FONT_NAME     =  4;
		constexpr int NAME_ID_POSTSCRIPT_NAME    =  6;
		constexpr int NAME_ID_TYPOGRAPHIC_FAMILY = 16;
		constexpr int NAME_ID_FONT_FAMILY        =  1;

		struct candidate_t
		{
			std::wstring name;
			int score, name_priority, order;
		};

		std::vector<candidate_t> candidates{};
		candidates.reserve(16);

		constexpr int NAME_ID_WEIGHT[4]{ 100000, 80000, 60000, 10000 };
		auto name_id_to_priority
		{ 
			[](int nid) noexcept -> int 
			{
				switch (nid)
				{
				case NAME_ID_FULL_FONT_NAME:     return 0;
				case NAME_ID_TYPOGRAPHIC_FAMILY: return 1;
				case NAME_ID_FONT_FAMILY:        return 2;
				case NAME_ID_POSTSCRIPT_NAME:    return 3;
				default: return -1;
				}
			} 
		};
		
		enum 
		{
			STBTT_MAC_LANG_ENGLISH            = 0,
			STBTT_MAC_EID_ROMAN               = 0,
			STBTT_PLATFORM_ID_UNICODE         = 0,
			STBTT_MS_EID_UNICODE_BMP          = 1,
			STBTT_PLATFORM_ID_MAC             = 1,
			STBTT_PLATFORM_ID_MICROSOFT       = 3,
			STBTT_UNICODE_EID_UNICODE_2_0_BMP = 3,
			STBTT_MS_EID_UNICODE_FULL         = 10,
			STBTT_MS_LANG_ENGLISH             = 0x0409,
			STBTT_MS_LANG_CHINESE             = 0x0804,
			STBTT_MS_LANG_JAPANESE            = 0x0411,
			STBTT_MS_LANG_KOREAN              = 0x0412,
		};

		int order{ 0 };
		std::wstring first_nonempty{};
		for (uint16_t i{ 0 }; i < record_count; ++i)
		{
			const uint8_t* rec{ name_base + 6 + i * 12 };
			const int pid{ static_cast<int>(winfont::read_u16_be(rec)) };
			const int eid{ static_cast<int>(winfont::read_u16_be(rec + 2)) };
			const int lid{ static_cast<int>(winfont::read_u16_be(rec + 4)) };
			const int nid{ static_cast<int>(winfont::read_u16_be(rec + 6)) };
			const uint16_t len{ winfont::read_u16_be(rec +  8) };
			const uint16_t off{ winfont::read_u16_be(rec + 10) };

			const int priority{ name_id_to_priority(nid) };
			if (priority < 0) 
			{
				continue;
			}

			bool match{ false };
			if (pid == STBTT_PLATFORM_ID_MICROSOFT)
			{
				if (eid == STBTT_MS_EID_UNICODE_BMP || eid == STBTT_MS_EID_UNICODE_FULL)
				{
					if (
						lid == STBTT_MS_LANG_ENGLISH  || lid == STBTT_MS_LANG_CHINESE ||
						lid == STBTT_MS_LANG_JAPANESE || lid == STBTT_MS_LANG_KOREAN)
					{
						match = true;
					}
				}
			}
			else if (pid == STBTT_PLATFORM_ID_UNICODE)
			{
				if (eid == STBTT_UNICODE_EID_UNICODE_2_0_BMP && lid == 0)
				{
					match = true;
				}
			}
			else if (pid == STBTT_PLATFORM_ID_MAC)
			{
				if (eid == STBTT_MAC_EID_ROMAN && lid == STBTT_MAC_LANG_ENGLISH)
				{
					match = true;
				}
			}
			
			if (!match) 
			{
				continue;
			}

			const size_t str_start{ static_cast<size_t>(string_offset) + off };
			if (str_start + len > name_length) 
			{ 
				continue; 
			}

			const char* str{ reinterpret_cast<const char*>(name_base + str_start) };

			std::wstring decoded{};
			if (pid == STBTT_PLATFORM_ID_MICROSOFT || pid == STBTT_PLATFORM_ID_UNICODE)
			{
				const int char_count{ static_cast<int>(len / 2) };
				decoded.reserve(char_count);

				for (int j{ 0 }; j < char_count; ++j)
				{
					const uint8_t*  q{ reinterpret_cast<const uint8_t*>(str) + j * 2 };
					const auto ch
					{ 
						(static_cast<uint16_t>(q[0]) << 8) | static_cast<uint16_t>(q[1])
					};
					if (ch == 0) 
					{
						break;
					}
					decoded.push_back(static_cast<wchar_t>(ch));
				}
			}
			else
			{
				decoded.reserve(len);
				for (uint16_t j{ 0 }; j < len; ++j)
				{
					if (str[j] == 0) 
					{
						break; 
					}
					decoded.push_back(static_cast<wchar_t>(static_cast<uint8_t>(str[j])));
				}
			}

			if (decoded.empty()) 
			{
				continue; 
			}

			bool ok{ true };
			for (wchar_t ch : decoded)
			{
				if (ch < 0x0020 && ch != 0x09 && ch != 0x0A && ch != 0x0D) 
				{ 
					ok = false; 
					break; 
				}
				if (ch == 0x007F) 
				{ 
					ok = false; 
					break; 
				}
				if (ch >= 0xD800 && ch <= 0xDFFF)
				{ 
					ok = false; 
					break; 
				}
			}
			if (!ok) 
			{
				continue;
			}

			if (first_nonempty.empty()) 
			{ 
				first_nonempty = decoded;
			}

			const int    name_weight{ NAME_ID_WEIGHT[priority]    };
			const int        quality{ score_name_quality(decoded) };
			const int platform_bonus{ (pid == STBTT_PLATFORM_ID_MICROSOFT) ? 50 : 0 };
			
			candidates.push_back
			(
				candidate_t
				{
					.name          = std::move(decoded),
					.score         = name_weight + quality + platform_bonus,
					.name_priority = priority,
					.order         = order++

				}
			);
		}

		if (candidates.empty()) 
		{
			return first_nonempty;
		}

		std::stable_sort
		(
			candidates.begin(), candidates.end(),
			[](const candidate_t& a, const candidate_t& b)
			{
				if (a.score != b.score) 
				{
					return a.score > b.score;
				}
				if (a.name_priority != b.name_priority)
				{ 
					return a.name_priority < b.name_priority;
				}
				return a.order < b.order;
			}
		);
		return std::move(candidates.front().name);
	}

	static auto get_font_face_name(std::span<uint8_t> data) -> std::wstring
	{
		if (data.size() < 12) 
		{ 
			return {}; 
		}
		const uint8_t*   base{ data.data() };
		const uint32_t flavor{ read_u32_be(base) };

		if (flavor != 0x00010000u && flavor != 0x4F54544Fu) 
		{
			return {};
		}

		const uint16_t num_tables{ winfont::read_u16_be(base + 4) };
		if (data.size() < 12u + static_cast<size_t>(num_tables) * 16u) 
		{
			return {};
		}

		uint32_t name_offset{ 0 };
		uint32_t name_length{ 0 };
		for (uint16_t i{ 0 }; i < num_tables; ++i)
		{
			const uint8_t* rec{ base + 12 + i * 16 };
			if (winfont::read_u32_be(rec) == 0x6E616D65u) // 'name'
			{
				name_offset = winfont::read_u32_be(rec +  8);
				name_length = winfont::read_u32_be(rec + 12);
				break;
			}
		}
		if (name_length == 0 || name_offset + name_length > data.size()) 
		{
			return {};
		}
		return winfont::parse_name_table(data.subspan(name_offset, name_length));
	}
	
	static auto get_font_face_name(const std::wstring& font_path) -> std::wstring
	{
		const HANDLE hFile = ::CreateFileW
		(
			font_path.c_str(),
			GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
			nullptr
		);

		if (hFile == INVALID_HANDLE_VALUE) 
		{
			return {};
		}

		uint8_t header[12]{};
		{
			DWORD     remaining{ 12 };
			uint8_t *header_dst{ header };
			while (remaining > 0)
			{
				DWORD got{ 0 };
				if (!::ReadFile(hFile, header_dst, remaining, &got, nullptr))
				{
					::CloseHandle(hFile);
					return {};
				}
				if (got == 0) 
				{ 
					::CloseHandle(hFile);
					return {};
				}
				header_dst += got;
				remaining  -= got;
			}
		}

		const uint32_t flavor{ winfont::read_u32_be(header) };
		if (flavor != 0x00010000u && flavor != 0x4F54544Fu)
		{
			::CloseHandle(hFile);
			return {};
		}

		const uint16_t num_tables{ winfont::read_u16_be(header + 4) };
		const DWORD     dir_bytes{ static_cast<DWORD>(num_tables) * 16u };

		std::vector<uint8_t> dir(static_cast<size_t>(dir_bytes));
		{
			uint8_t* dir_dst{ dir.data() };
			DWORD  remaining{ dir_bytes};
			while (remaining > 0)
			{
				DWORD got{ 0 };
				if (!::ReadFile(hFile, dir_dst, remaining, &got, nullptr))
				{
					::CloseHandle(hFile);
					return {};
				}

				if (got == 0) 
				{ 
					::CloseHandle(hFile); 
					return {};
				}

				dir_dst   += got;
				remaining -= got;
			}
		}

		uint32_t name_offset{ 0 };
		uint32_t name_length{ 0 };
		for (uint16_t i{ 0 }; i < num_tables; ++i)
		{
			const uint8_t* rec{ dir.data() + i * 16 };
			if (winfont::read_u32_be(rec) == 0x6E616D65u) // 'name'
			{
				name_offset = winfont::read_u32_be(rec + 8);
				name_length = winfont::read_u32_be(rec + 12);
				break;
			}
		}
		if (name_length == 0)
		{
			::CloseHandle(hFile);
			return {};
		}

		LARGE_INTEGER li { .QuadPart = static_cast<int64_t>(name_offset) };
		if (!::SetFilePointerEx(hFile, li, nullptr, FILE_BEGIN))
		{
			::CloseHandle(hFile);
			return {};
		}

		std::vector<uint8_t> name_table(static_cast<size_t>(name_length));
		{
			uint8_t* name_dst{ name_table.data() };
			DWORD   remaining{ static_cast<DWORD>(name_length) };
			while (remaining > 0)
			{
				DWORD got{ 0 };
				if (!::ReadFile(hFile, name_dst, remaining, &got, nullptr))
				{
					::CloseHandle(hFile);
					return {};
				}
				if (got == 0) 
				{ 
					::CloseHandle(hFile);
					return {}; 
				}
				name_dst  += got;
				remaining -= got;
			}
			::CloseHandle(hFile);
		}

		return winfont::parse_name_table(std::span<uint8_t>{ name_table });
	}


	auto register_private_font(std::span<uint8_t> data, std::wstring_view alias) noexcept -> fontid_t
	{
		if (data.empty())
		{
			return 0;
		}

		std::wstring face_name{ winfont::get_font_face_name(data) };
		if (face_name.empty()) 
		{
			return 0;
		}

		const std::optional<uint64_t> name_hash{ winfont::strhash(face_name) };
		if (!name_hash.has_value())
		{
			return 0;
		}

		DWORD font_count{ 0 };
		const HANDLE handle = ::AddFontMemResourceEx
		(
			data.data(),
			static_cast<DWORD>(data.size()),
			nullptr,
			&font_count
		);

		if (handle == nullptr)
		{
			return 0;
		}

		const fontid_t font_id{ name_hash.value() + reinterpret_cast<uint64_t>(handle)};

		winfont::entry _entry
		{
			.fontid = font_id,
			.name   = std::move(face_name),
			.handle = handle,
		};

		winfont::FONTS.push_back(std::move(_entry));
		if (!alias.empty())
		{
			winfont::FONT_ALIAS[std::wstring(alias)] = winfont::FONTS.size() - 1;
		}

		return font_id;
	}

	auto register_private_font(std::wstring_view file, bool as_memory, std::wstring_view alias) noexcept -> fontid_t
	{
		const auto file_hash{ winfont::strhash(file) };
		if (!file_hash.has_value())
		{
			return 0;
		}
		
		std::wstring font_path{ file };
		std::wstring face_name{ winfont::get_font_face_name(font_path) };

		if (face_name.empty())
		{
			return 0;
		}

		const std::optional<uint64_t> name_hash{ winfont::strhash(face_name) };
		if (!name_hash.has_value())
		{
			return 0;
		}

		const fontid_t font_id{ file_hash.value() + name_hash.value() };
		for (auto it = winfont::FONTS.begin(); it != winfont::FONTS.end();) 
		{
			if (it->fontid == font_id) 
			{
				uint64_t index = std::distance(winfont::FONTS.begin(), it);
				winfont::FONT_ALIAS[std::wstring(alias)] = index;
				return font_id;
			}
			it++;
		}

		if (as_memory)
		{
			const HANDLE hFile = ::CreateFileW
			(
				font_path.c_str(),
				GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				nullptr,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
				nullptr
			);

			if (hFile == INVALID_HANDLE_VALUE)
			{
				return 0;
			}

			LARGE_INTEGER li{};
			if (!::GetFileSizeEx(hFile, &li) || li.QuadPart <= 0)
			{
				::CloseHandle(hFile);
				return 0;
			}

			std::vector<uint8_t> file_data(static_cast<size_t>(li.QuadPart)); 
			{
				uint8_t* file_data_dst{ file_data.data() };
				DWORD        remaining{ static_cast<DWORD>(file_data.size()) };
				while (remaining > 0)
				{
					DWORD got{ 0 };
					if (!::ReadFile(hFile, file_data_dst, remaining, &got, nullptr))
					{
						::CloseHandle(hFile);
						return 0;
					}
					if (got == 0)
					{
						::CloseHandle(hFile);
						return 0;
					}
					file_data_dst += got;
					remaining     -= got;
				}
				::CloseHandle(hFile);
			}

			DWORD font_count{ 0 };
			const HANDLE handle = ::AddFontMemResourceEx
			(
				file_data.data(),
				static_cast<DWORD>(file_data.size()),
				nullptr,
				&font_count
			);

			if (handle == nullptr)
			{
				return 0;
			}

			winfont::entry _entry
			{
				.fontid = font_id,
				.path   = std::move(font_path),
				.name   = std::move(face_name),
				.handle = handle,
			};

			winfont::FONTS.push_back(std::move(_entry));
			if (!alias.empty())
			{
				winfont::FONT_ALIAS[std::wstring(alias)] = winfont::FONTS.size() - 1;
			}

			return font_id;
		}
		else
		{
			const int result{ ::AddFontResourceExW(font_path.c_str(), FR_PRIVATE, nullptr) };
			if (result <= 0) 
			{
				return 0;
			}

			winfont::entry _entry
			{
				.fontid = font_id,
				.path   = std::move(font_path),
				.name   = std::move(face_name),
				.handle = nullptr
			};

			winfont::FONTS.push_back(std::move(_entry));
			if (!alias.empty())
			{
				winfont::FONT_ALIAS[std::wstring(alias)] = winfont::FONTS.size() - 1;
			}
			
			return font_id;
		}
	}

	auto unregister_private_font(const fontid_t fontid) noexcept -> void
	{
		if (fontid == 0) 
		{
			return;
		}

		uint64_t removed_index{ static_cast<uint64_t>(-1) };
		if (!winfont::FONTS.empty())
		{
			for (size_t i = 0; i < winfont::FONTS.size(); ++i)
			{
				if (winfont::FONTS[i].fontid == fontid)
				{
					if (winfont::FONTS[i].handle != nullptr)
					{
						::RemoveFontMemResourceEx(winfont::FONTS[i].handle);
					}
					else
					{
						::RemoveFontResourceExW(winfont::FONTS[i].path.c_str(), FR_PRIVATE, NULL);
					}

					removed_index = i;
					winfont::FONTS.erase(winfont::FONTS.begin() + i);
					break;
				}
			}
		}

		if (removed_index != static_cast<uint64_t>(-1) && !winfont::FONT_ALIAS.empty())
		{
			for (auto it = winfont::FONT_ALIAS.begin(); it != winfont::FONT_ALIAS.end(); )
			{
				if (it->second == removed_index)
				{
					it = winfont::FONT_ALIAS.erase(it);
				}
				else if (it->second > removed_index)
				{
					it->second -= 1;
					++it;
				}
				else
				{
					++it;
				}
			}
		}
	}

	auto unregister_private_font() noexcept -> void 
	{
		if (!winfont::FONT_ALIAS.empty()) 
		{
			winfont::FONT_ALIAS = {};
		}

		if (!winfont::FONTS.empty())
		{
			for (const winfont::entry& e : winfont::FONTS)
			{
				if (e.handle != nullptr)
				{
					::RemoveFontMemResourceEx(e.handle);
					continue;
				}
				::RemoveFontResourceExW(e.path.c_str(), FR_PRIVATE, NULL);
			}
			winfont::FONTS = {};
		}
	}

	auto get_private_font_name(std::wstring_view alias) noexcept -> std::wstring
	{
		if (!winfont::FONT_ALIAS.empty())
		{
			for (const auto& [_alias, _index] : winfont::FONT_ALIAS) 
			{
				if (_alias != alias) 
				{
					continue;
				}
				if (_index < winfont::FONTS.size()) 
				{
					return winfont::FONTS[_index].name;
				}
				return {};
			}
		}
		return {};
	}

	auto get_private_font_name(const fontid_t fontid) noexcept -> std::wstring 
	{
		if (!winfont::FONTS.empty()) 
		{
			for (const winfont::entry& e : winfont::FONTS) 
			{
				if (e.fontid == fontid) 
				{
					return e.name;
				}
			}
		}
		return {};
	}

	auto get_private_font_path(const fontid_t fontid) noexcept -> std::wstring 
	{
		if (!winfont::FONTS.empty())
		{
			for (const winfont::entry& e : winfont::FONTS)
			{
				if (e.fontid == fontid)
				{
					return e.path;
				}
			}
		}
		return {};
	}

	auto get_private_font_path(std::wstring_view alias) noexcept -> std::wstring 
	{
		if (!winfont::FONT_ALIAS.empty())
		{
			for (const auto& [_alias, _index] : winfont::FONT_ALIAS)
			{
				if (_alias != alias)
				{
					continue;
				}
				if (_index < winfont::FONTS.size())
				{
					return winfont::FONTS[_index].path;
				}
				return {};
			}
		}
		return {};
	}

	auto get_private_fontid(std::wstring_view alias) noexcept -> fontid_t 
	{
		if (!winfont::FONT_ALIAS.empty())
		{
			for (const auto& [_alias, _index] : winfont::FONT_ALIAS)
			{
				if (_alias != alias)
				{
					continue;
				}
				if (_index < winfont::FONTS.size())
				{
					return winfont::FONTS[_index].fontid;
				}
				return {};
			}
		}
		return {};
	}
}