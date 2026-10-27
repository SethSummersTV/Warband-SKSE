#pragma once

namespace FollowerLines
{
	struct Added
	{
		RE::FormID            info;
		RE::TESConditionItem* item;
		bool                  linked;
		bool                  hireling;
	};
	inline std::vector<Added> added;
	inline RE::FormID         bandID = 0;

	inline constexpr RE::FormID kFollowerQuest = 0x000750BA;
	inline constexpr RE::FormID kHirelingQuest = 0x000BCC94;

	inline void Unlink(bool a_onlyFollower = false)
	{
		for (auto& a : added) {
			if (!a.linked || (a_onlyFollower && a.hireling)) {
				continue;
			}
			auto* info = RE::TESForm::LookupByID<RE::TESTopicInfo>(a.info);
			if (!info) {
				a.linked = false;
				continue;
			}
			for (auto** link = &info->objConditions.head; *link; link = &(*link)->next) {
				if (*link == a.item) {
					*link = a.item->next;
					break;
				}
			}
			a.linked = false;
		}
	}

	inline void Link(RE::TESFaction* a_band, bool a_follower)
	{
		for (auto& a : added) {
			if (a.linked || (!a.hireling && !a_follower)) {
				continue;
			}
			auto* info = RE::TESForm::LookupByID<RE::TESTopicInfo>(a.info);
			if (!info) {
				continue;
			}
			a.item->data.functionData.params[0] = a_band;
			a.item->next = info->objConditions.head;
			info->objConditions.head = a.item;
			a.linked = true;
		}
	}

	inline void Apply(RE::FormID a_bandID, bool a_hide)
	{
		auto* band = a_bandID ? RE::TESForm::LookupByID<RE::TESFaction>(a_bandID) : nullptr;
		if (!band) {
			if (std::any_of(added.begin(), added.end(), [](const Added& a) { return a.linked; })) {
				Unlink();
				logger::info("follower lines: the game's own follower and hireling lines are as they were again");
			}
			return;
		}
		if (bandID != a_bandID) {
			Unlink();
			bandID = a_bandID;
		}
		if (added.empty()) {
			const auto* quest = RE::TESForm::LookupByID<RE::TESQuest>(kFollowerQuest);
			const auto* hireQuest = RE::TESForm::LookupByID<RE::TESQuest>(kHirelingQuest);
			if (!quest && !hireQuest) {
				return;
			}
			std::vector<std::pair<RE::FormID, bool>> infos;
			{
				const auto& [map, lock] = RE::TESForm::GetAllForms();
				const RE::BSReadLockGuard guard{ lock };
				if (map) {
					for (const auto& [id, form] : *map) {
						if (!form || !form->Is(RE::FormType::Info)) {
							continue;
						}
						const auto* info = static_cast<const RE::TESTopicInfo*>(form);
						const auto* topic = info->parentTopic;
						const bool  follower = quest && topic && topic->ownerQuest == quest && topic->data.subtype.get() == RE::DIALOGUE_DATA::Subtype::kCustom;
						const bool  hireling = hireQuest && topic && topic->ownerQuest == hireQuest;
						if (follower || hireling) {
							infos.emplace_back(id, hireling);
						}
					}
				}
			}
			if (infos.empty()) {
				return;
			}
			added.reserve(infos.size());
			for (const auto& [id, hireling] : infos) {
				auto* item = new RE::TESConditionItem();
				item->data.functionData.function = RE::FUNCTION_DATA::FunctionID::kGetInFaction;
				item->data.functionData.params[0] = band;
				item->data.comparisonValue.f = 0.0f;
				item->data.flags.opCode = RE::CONDITION_ITEM_DATA::OpCode::kEqualTo;
				item->data.object = RE::CONDITIONITEMOBJECT::kSelf;
				added.push_back({ id, item, false, hireling });
			}
			const auto hire = std::count_if(added.begin(), added.end(), [](const Added& a) { return a.hireling; });
			logger::info("follower lines: {} lines of the game's follower quest and {} of its hireling quest found", added.size() - hire, hire);
		}
		const auto linkedOf = [](bool a_hireling) {
			return std::count_if(added.begin(), added.end(), [a_hireling](const Added& a) { return a.linked && a.hireling == a_hireling; });
		};
		const auto beforeF = linkedOf(false), beforeH = linkedOf(true);
		if (!a_hide) {
			Unlink(true);
		}
		Link(band, a_hide);
		const auto afterF = linkedOf(false), afterH = linkedOf(true);
		if (afterF != beforeF) {
			logger::info("follower lines: follower quest lines {} on band members ({} lines)", afterF ? "hidden" : "as they were", afterF ? afterF : beforeF);
		}
		if (afterH != beforeH) {
			logger::info("follower lines: hireling quest lines hidden on band members ({} lines)", afterH);
		}
	}

	inline void Set(RE::TESFaction* a_band, bool a_hide)
	{
		const RE::FormID id = a_band ? a_band->GetFormID() : 0;
		if (auto* tasks = SKSE::GetTaskInterface()) {
			tasks->AddTask([id, a_hide]() { Apply(id, a_hide); });
		}
	}
}
