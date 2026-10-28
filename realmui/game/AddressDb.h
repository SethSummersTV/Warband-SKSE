#pragma once
#include "Runtime.h"

namespace Game
{
	class AddressDb
	{
	public:
		struct Row
		{
			std::uint64_t id;
			std::uint64_t offset;
		};

		[[nodiscard]] static std::wstring FileFor(const RuntimeInfo& a_rt)
		{
			const auto& v = a_rt.version;
			return a_rt.exeFolder + L"Data\\SKSE\\Plugins\\" + (a_rt.line == Line::kSE ? L"version-" : L"versionlib-") + std::to_wstring(v.major) + L"-" +
			       std::to_wstring(v.minor) + L"-" + std::to_wstring(v.build) + L"-" + std::to_wstring(v.sub) + L".bin";
		}

		bool Load(const std::wstring& a_path, const Version& a_game, std::string& a_why) noexcept
		{
			try {
				rows.clear();
				std::ifstream f(std::filesystem::path(a_path), std::ios::binary);
				if (!f) {
					a_why = "the file is not there";
					return false;
				}
				const std::vector<char> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
				Reader                  in{ data };
				format = in.Get<std::int32_t>();
				std::int32_t v[4] = { in.Get<std::int32_t>(), in.Get<std::int32_t>(), in.Get<std::int32_t>(), in.Get<std::int32_t>() };
				if (in.failed) {
					a_why = "the file is too short";
					return false;
				}
				if (v[0] != a_game.major || v[1] != a_game.minor || v[2] != a_game.build || v[3] != a_game.sub) {
					a_why = "the file is for another game version";
					return false;
				}
				if (format == 1 || format == 2) {
					const auto nameLength = in.Get<std::int32_t>();
					in.Skip(nameLength);
					const auto pointerSize = static_cast<std::uint64_t>(in.Get<std::int32_t>());
					const auto count = in.Get<std::int32_t>();
					if (in.failed || pointerSize == 0 || count <= 0 || static_cast<std::size_t>(count) > data.size()) {
						a_why = "the file's header is not as expected";
						return false;
					}
					rows.reserve(static_cast<std::size_t>(count));
					std::uint64_t prevID = 0, prevOffset = 0;
					for (std::int32_t i = 0; i < count && !in.failed; ++i) {
						const auto          type = in.Get<std::uint8_t>();
						const std::uint8_t  lo = type & 0xF, hi = type >> 4;
						std::uint64_t       id = 0, offset = 0;
						switch (lo) {
						case 0: id = in.Get<std::uint64_t>(); break;
						case 1: id = prevID + 1; break;
						case 2: id = prevID + in.Get<std::uint8_t>(); break;
						case 3: id = prevID - in.Get<std::uint8_t>(); break;
						case 4: id = prevID + in.Get<std::uint16_t>(); break;
						case 5: id = prevID - in.Get<std::uint16_t>(); break;
						case 6: id = in.Get<std::uint16_t>(); break;
						case 7: id = in.Get<std::uint32_t>(); break;
						default: in.failed = true; break;
						}
						const std::uint64_t from = (hi & 8) != 0 ? prevOffset / pointerSize : prevOffset;
						switch (hi & 7) {
						case 0: offset = in.Get<std::uint64_t>(); break;
						case 1: offset = from + 1; break;
						case 2: offset = from + in.Get<std::uint8_t>(); break;
						case 3: offset = from - in.Get<std::uint8_t>(); break;
						case 4: offset = from + in.Get<std::uint16_t>(); break;
						case 5: offset = from - in.Get<std::uint16_t>(); break;
						case 6: offset = in.Get<std::uint16_t>(); break;
						case 7: offset = in.Get<std::uint32_t>(); break;
						default: break;
						}
						if ((hi & 8) != 0) {
							offset *= pointerSize;
						}
						rows.push_back({ id, offset });
						prevID = id;
						prevOffset = offset;
					}
				} else if (format == 5) {
					if (data.size() < 0x60) {
						a_why = "the file is too short";
						return false;
					}
					in.at = 0x5C;
					const auto count = in.Get<std::int32_t>();
					if (count <= 0 || data.size() != 0x60 + 4 * static_cast<std::size_t>(count)) {
						a_why = "the file's size does not fit its header (format 5)";
						return false;
					}
					rows.reserve(static_cast<std::size_t>(count));
					for (std::int32_t id = 0; id < count; ++id) {
						if (const auto offset = in.Get<std::uint32_t>(); offset != 0) {
							rows.push_back({ static_cast<std::uint64_t>(id), offset });
						}
					}
				} else {
					a_why = "Address Library format " + std::to_string(format) + " is not one this plugin reads (1, 2 and 5)";
					return false;
				}
				if (in.failed || rows.empty()) {
					rows.clear();
					a_why = "the file ends early or holds nothing";
					return false;
				}
				std::sort(rows.begin(), rows.end(), [](const Row& a_l, const Row& a_r) { return a_l.id < a_r.id; });
				return true;
			} catch (...) {
				rows.clear();
				a_why = "the file could not be read";
				return false;
			}
		}

		[[nodiscard]] const Row* Find(std::uint64_t a_id) const noexcept
		{
			const auto it = std::lower_bound(rows.begin(), rows.end(), a_id, [](const Row& a_row, std::uint64_t a_want) { return a_row.id < a_want; });
			return it != rows.end() && it->id == a_id ? &*it : nullptr;
		}

		[[nodiscard]] bool Has(std::uint64_t a_id) const noexcept { return Find(a_id) != nullptr; }

		void Free() noexcept
		{
			rows.clear();
			rows.shrink_to_fit();
		}

		std::vector<Row> rows;
		std::int32_t     format = 0;

	private:
		struct Reader
		{
			const std::vector<char>& data;
			std::size_t              at = 0;
			bool                     failed = false;

			template <class T>
			T Get() noexcept
			{
				T value{};
				if (failed || at + sizeof(T) > data.size()) {
					failed = true;
					return value;
				}
				std::memcpy(&value, data.data() + at, sizeof(T));
				at += sizeof(T);
				return value;
			}

			void Skip(std::int64_t a_count) noexcept
			{
				if (a_count < 0 || at + static_cast<std::size_t>(a_count) > data.size()) {
					failed = true;
				} else {
					at += static_cast<std::size_t>(a_count);
				}
			}
		};
	};
}
