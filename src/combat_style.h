#pragma once

#include "game/Canary.h"

namespace FightStyle
{
	inline std::mutex lock;
	inline std::unordered_map<RE::FormID, RE::FormID> wanted;
	inline std::unordered_map<RE::FormID, RE::FormID> applied;
	inline std::unordered_map<RE::FormID, const void*> written;
	inline std::unordered_map<RE::FormID, RE::FormID> yielded;

	inline void Set(RE::Actor* a_actor, RE::TESCombatStyle* a_style)
	{
		if (!a_actor) {
			return;
		}
		std::lock_guard g(lock);
		if (a_style) {
			wanted[a_actor->GetFormID()] = a_style->GetFormID();
		} else {
			RE::FormID ours = 0;
			if (const auto it = applied.find(a_actor->GetFormID()); it != applied.end()) {
				ours = it->second;
			}
			wanted.erase(a_actor->GetFormID());
			applied.erase(a_actor->GetFormID());
			if (!ours) {
				return;
			}
			const auto handle = a_actor->GetHandle();
			SKSE::GetTaskInterface()->AddTask([handle, ours]() {
				auto ptr = handle.get();
				auto* actor = ptr.get();
				Game::Canary::EnsureRan();
				if (!Game::Canary::actors.load()) {
					return;
				}
				auto* npc = actor ? actor->GetActorBase() : nullptr;
				if (auto* controller = actor ? actor->GetActorRuntimeData().combatController : nullptr; controller && npc) {
					if (Game::Canary::StyleSlot(controller) == Game::Canary::Slot::kUse && controller->combatStyle &&
						controller->combatStyle->GetFormID() == ours) {
						controller->combatStyle = npc->GetCombatStyle();
					}
				}
			});
		}
	}

	inline void Apply()
	{
		std::vector<std::pair<RE::FormID, RE::FormID>> list;
		{
			std::lock_guard g(lock);
			list.assign(wanted.begin(), wanted.end());
			std::erase_if(written, [](const auto& e) { return !wanted.contains(e.first); });
			std::erase_if(yielded, [](const auto& e) { return !wanted.contains(e.first); });
		}
		if (list.empty()) {
			return;
		}
		Game::Canary::EnsureRan();
		if (!Game::Canary::actors.load() || !Game::Canary::fightStyles.load()) {
			return;
		}
		for (const auto& [actorID, styleID] : list) {
			auto* actor = RE::TESForm::LookupByID<RE::Actor>(actorID);
			auto* style = RE::TESForm::LookupByID<RE::TESCombatStyle>(styleID);
			if (!actor || !style || actor->IsDead() || !actor->IsInCombat()) {
				written.erase(actorID);
				yielded.erase(actorID);
				continue;
			}
			auto* controller = actor->GetActorRuntimeData().combatController;
			if (!controller) {
				continue;
			}
			const auto slot = Game::Canary::StyleSlot(controller);
			if (slot == Game::Canary::Slot::kOff) {
				return;
			}
			if (slot == Game::Canary::Slot::kWait) {
				continue;
			}
			if (controller->combatStyle == style) {
				continue;
			}
			const auto* before = controller->combatStyle;
			auto* npc = actor->GetActorBase();
			const auto last = [&]() { std::lock_guard g(lock); const auto it = applied.find(actorID); return it != applied.end() ? it->second : 0u; }();
			const bool own = !before || (npc && before == npc->GetCombatStyle()) || before->GetFormID() == last;
			const auto seen = written.find(actorID);
			if (!own && seen != written.end() && seen->second == controller) {
				if (yielded[actorID] != before->GetFormID()) {
					yielded[actorID] = before->GetFormID();
					logger::info("fight style: {} ({:08X}) keeps style {:08X}, set by another mod during the fight", actor->GetDisplayFullName(),
						actorID, before->GetFormID());
				}
				continue;
			}
			controller->combatStyle = style;
			written[actorID] = controller;
			yielded.erase(actorID);
			std::lock_guard g(lock);
			if (applied[actorID] != styleID) {
				applied[actorID] = styleID;
				logger::info("fight style: {} ({:08X}) fights as style {:08X} (was {:08X})", actor->GetDisplayFullName(), actorID,
					styleID, before ? before->GetFormID() : 0u);
			}
		}
	}

	inline void Clear()
	{
		std::lock_guard g(lock);
		wanted.clear();
		applied.clear();
	}

	inline void Start()
	{
		std::thread([]() {
			for (;;) {
				std::this_thread::sleep_for(std::chrono::milliseconds(250));
				if (auto* tasks = SKSE::GetTaskInterface()) {
					tasks->AddTask([]() { Apply(); });
				}
			}
		}).detach();
	}
}
