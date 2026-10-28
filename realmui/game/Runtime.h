#pragma once
namespace Game
{
	struct Version
	{
		std::uint16_t major = 0, minor = 0, build = 0, sub = 0;

		[[nodiscard]] constexpr auto operator<=>(const Version&) const noexcept = default;

		[[nodiscard]] std::string Text() const
		{
			return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(build) + "." + std::to_string(sub);
		}
	};

	enum class Line
	{
		kUnknown,
		kSE,
		kAE,
		kAE1130,
		kAE17,
	};

	inline constexpr std::uint16_t kNewestChecked16 = 1179;
	inline constexpr std::uint16_t kNewestChecked17 = 104;

	struct RuntimeInfo
	{
		Version      version;
		bool         read = false;
		Line         line = Line::kUnknown;
		std::wstring exeFolder;

		[[nodiscard]] bool Ae629() const noexcept { return version >= Version{ 1, 6, 629, 0 }; }
		[[nodiscard]] bool Ae1130() const noexcept { return line == Line::kAE1130 || line == Line::kAE17; }
		[[nodiscard]] bool Ae17() const noexcept { return line == Line::kAE17; }
		[[nodiscard]] bool UsesAeIds() const noexcept { return line == Line::kAE || line == Line::kAE1130 || line == Line::kAE17; }
		[[nodiscard]] bool NewerThanChecked() const noexcept
		{
			return (line == Line::kAE1130 && version.build > kNewestChecked16) || (line == Line::kAE17 && version.build > kNewestChecked17);
		}

		[[nodiscard]] const char* LineName() const noexcept
		{
			switch (line) {
			case Line::kSE: return "Special Edition 1.5.97";
			case Line::kAE: return "Anniversary Edition before 1.6.1130";
			case Line::kAE1130: return "Anniversary Edition 1.6.1130 or later";
			case Line::kAE17: return "1.7";
			default: return "unknown";
			}
		}
	};

	namespace detail
	{
		inline int ReadExeVersion(const std::wstring& a_exe, Version& a_out) noexcept
		{
			try {
				DWORD       dummy = 0;
				const DWORD size = GetFileVersionInfoSizeW(a_exe.c_str(), &dummy);
				if (size == 0) {
					return 0;
				}
				std::vector<char> buf(size);
				if (!GetFileVersionInfoW(a_exe.c_str(), 0, size, buf.data())) {
					return 0;
				}
				void* text = nullptr;
				UINT  len = 0;
				if (VerQueryValueW(buf.data(), L"\\StringFileInfo\\040904B0\\ProductVersion", &text, &len) && text && len > 0) {
					const std::wstring s(static_cast<const wchar_t*>(text), len);
					unsigned           part[4] = { 0, 0, 0, 0 };
					if (swscanf_s(s.c_str(), L"%u.%u.%u.%u", &part[0], &part[1], &part[2], &part[3]) >= 3) {
						a_out = { static_cast<std::uint16_t>(part[0]), static_cast<std::uint16_t>(part[1]),
							static_cast<std::uint16_t>(part[2]), static_cast<std::uint16_t>(part[3]) };
						return 1;
					}
				}
				VS_FIXEDFILEINFO* fixed = nullptr;
				if (VerQueryValueW(buf.data(), L"\\", reinterpret_cast<void**>(&fixed), &len) && fixed && len >= sizeof(VS_FIXEDFILEINFO)) {
					a_out = { HIWORD(fixed->dwProductVersionMS), LOWORD(fixed->dwProductVersionMS), HIWORD(fixed->dwProductVersionLS),
						LOWORD(fixed->dwProductVersionLS) };
					return 2;
				}
			} catch (...) {}
			return 0;
		}

		inline RuntimeInfo Detect() noexcept
		{
			RuntimeInfo info;
			try {
				wchar_t     path[MAX_PATH * 4] = {};
				const DWORD n = GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
				if (n == 0 || n >= std::size(path)) {
					return info;
				}
				const std::wstring exe(path, n);
				const auto         slash = exe.find_last_of(L"\\/");
				info.exeFolder = slash == std::wstring::npos ? std::wstring() : exe.substr(0, slash + 1);
				info.read = ReadExeVersion(exe, info.version) != 0;
				if (!info.read) {
					return info;
				}
				const auto& v = info.version;
				if (v.major == 1 && v.minor == 5 && v.build == 97) {
					info.line = Line::kSE;
				} else if (v.major == 1 && v.minor == 6 && v.build >= 317) {
					info.line = v.build >= 1130 ? Line::kAE1130 : Line::kAE;
				} else if (v.major == 1 && v.minor == 7) {
					info.line = Line::kAE17;
				}
			} catch (...) {}
			return info;
		}
	}

	[[nodiscard]] inline const RuntimeInfo& Runtime() noexcept
	{
		static const RuntimeInfo info = detail::Detect();
		return info;
	}
}
