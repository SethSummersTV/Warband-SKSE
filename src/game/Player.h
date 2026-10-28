#pragma once
#include "Runtime.h"

namespace Game
{
	[[nodiscard]] inline const RE::BSTArray<RE::BGSPerk*>* PlayerPerks()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return nullptr;
		}
		if (Runtime().Ae17()) {
			return reinterpret_cast<const RE::BSTArray<RE::BGSPerk*>*>(reinterpret_cast<const char*>(player) + 0x4D8);
		}
		return &player->GetPlayerRuntimeData().perks;
	}

	inline constexpr RE::FormID kGold = 0x0000000F;

	[[nodiscard]] inline std::int32_t GoldOf(RE::Actor* a_actor)
	{
		auto* gold = a_actor ? RE::TESForm::LookupByID<RE::TESBoundObject>(kGold) : nullptr;
		if (!gold) {
			return 0;
		}
		const auto inventory = a_actor->GetInventory([](RE::TESBoundObject& a_object) { return a_object.GetFormID() == kGold; });
		const auto it = inventory.find(gold);
		return it != inventory.end() ? it->second.first : 0;
	}
}
