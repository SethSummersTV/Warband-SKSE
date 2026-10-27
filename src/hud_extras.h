#pragma once
#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

namespace BandHUD
{
	struct Extras
	{
		std::string name;
		bool hold = false;
		bool guest = false;
		std::string face;
	};

	inline Extras ParseExtras(const std::string& a_text)
	{
		Extras out;
		const auto p1 = a_text.find('|');
		const auto p2 = p1 == std::string::npos ? p1 : a_text.find('|', p1 + 1);
		out.name = a_text.substr(0, p1);
		out.hold = p1 != std::string::npos && a_text.size() > p1 + 1 && a_text[p1 + 1] == '1';
		out.guest = p1 != std::string::npos && a_text.size() > p1 + 2 && (p2 == std::string::npos || p2 > p1 + 2) && a_text[p1 + 2] == 'g';
		out.face = p2 == std::string::npos ? "" : a_text.substr(p2 + 1);
		if (out.guest) {
			static constexpr std::string_view ending = " (guest)";
			if (out.name.size() > ending.size()) {
				const auto at = out.name.size() - ending.size();
				bool same = true;
				for (std::size_t k = 0; k < ending.size(); ++k) {
					const char ch = out.name[at + k];
					same = same && (ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : ch) == ending[k];
				}
				if (same) {
					out.name.resize(at);
				}
			}
		}
		return out;
	}

	struct Grid
	{
		int across = 1;
		int rows = 0;
	};

	inline Grid FitGrid(int a_cards, int a_columns, float a_screenH, float a_cardH, float a_pad)
	{
		Grid g;
		const int n = (std::max)(a_cards, 0);
		g.across = (std::max)(1, (std::min)(a_columns, n));
		g.rows = (n + g.across - 1) / g.across;
		const int fit = a_cardH > 0.0f ? (std::max)(1, static_cast<int>((a_screenH - a_pad) / a_cardH)) : g.rows;
		if (g.rows > fit) {
			g.across = (n + fit - 1) / fit;
			g.rows = (n + g.across - 1) / g.across;
		}
		return g;
	}
}
