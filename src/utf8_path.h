#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace Utf8Path
{
	inline std::wstring Wide(std::string_view a_utf8) noexcept
	{
		try {
			if (a_utf8.empty()) {
				return {};
			}
			const int n = ::MultiByteToWideChar(CP_UTF8, 0, a_utf8.data(), static_cast<int>(a_utf8.size()), nullptr, 0);
			if (n <= 0) {
				return {};
			}
			std::wstring out(static_cast<std::size_t>(n), L'\0');
			::MultiByteToWideChar(CP_UTF8, 0, a_utf8.data(), static_cast<int>(a_utf8.size()), out.data(), n);
			return out;
		} catch (...) {
			return {};
		}
	}

	inline std::filesystem::path Of(std::string_view a_utf8) noexcept
	{
		try {
			return std::filesystem::path(Wide(a_utf8));
		} catch (...) {
			return {};
		}
	}

	inline bool Exists(std::string_view a_utf8) noexcept
	{
		try {
			const auto p = Of(a_utf8);
			if (p.empty()) {
				return false;
			}
			std::error_code ec;
			return std::filesystem::exists(p, ec);
		} catch (...) {
			return false;
		}
	}
}
