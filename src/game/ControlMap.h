#pragma once
#include "Runtime.h"

namespace Game
{
	[[nodiscard]] inline std::ptrdiff_t TextEntryCountOffset() noexcept
	{
		return Runtime().Ae1130() ? 0x128 : 0x120;
	}

	[[nodiscard]] inline int TextEntryCount()
	{
		const auto* controls = RE::ControlMap::GetSingleton();
		if (!controls) {
			return 0;
		}
		return *reinterpret_cast<const std::int8_t*>(reinterpret_cast<const char*>(controls) + TextEntryCountOffset());
	}
}
