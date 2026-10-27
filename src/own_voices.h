#pragma once

namespace OwnVoices
{
	inline constexpr std::array<std::pair<RE::FormID, RE::FormID>, 4> kLines{ {
		{ 0x000A24, 0x075084 },
		{ 0x000A27, 0x075083 },
		{ 0x000A21, 0x060020 },
		{ 0x0008DA, 0x05C80C },
	} };
	inline constexpr std::array<RE::FormID, 3> kStarters{ 0x00092A, 0x00092B, 0x00092C };
	inline constexpr RE::FormID kBandFaction = 0x000803;
	inline constexpr RE::FormID kCurrentFollowerFaction = 0x05C84E;

	inline RE::TESTopicInfo* ours[4]{};
	inline RE::TESTopic*     theirs[4]{};
	inline RE::TESNPC*       starters[3]{};
	inline RE::TESFaction*   band = nullptr;
	inline RE::TESFaction*   currentFollower = nullptr;
	inline bool              ready = false;
	inline bool              proven = false;
	inline bool              pointed = false;
	inline bool              off = false;

	inline void Start()
	{
		auto* dh = RE::TESDataHandler::GetSingleton();
		if (!dh || !dh->LookupModByName("Warband.esp") || !dh->LookupModByName("Skyrim.esm")) {
			return;
		}
		ready = true;
		for (std::size_t i = 0; i < kLines.size(); ++i) {
			ours[i] = dh->LookupForm<RE::TESTopicInfo>(kLines[i].first, "Warband.esp");
			theirs[i] = dh->LookupForm<RE::TESTopic>(kLines[i].second, "Skyrim.esm");
			ready = ready && ours[i] && theirs[i] && ours[i]->parentTopic;
		}
		for (std::size_t i = 0; i < kStarters.size(); ++i) {
			starters[i] = dh->LookupForm<RE::TESNPC>(kStarters[i], "Warband.esp");
		}
		band = dh->LookupForm<RE::TESFaction>(kBandFaction, "Warband.esp");
		currentFollower = dh->LookupForm<RE::TESFaction>(kCurrentFollowerFaction, "Skyrim.esm");
		ready = ready && band && currentFollower;
		const auto* probe = dh->LookupForm<RE::TESTopicInfo>(0x000AFE, "Warband.esp");
		const auto* want = dh->LookupForm<RE::TESTopicInfo>(0x0E26F4, "Skyrim.esm");
		const RE::FormID held = probe && probe->dataInfo ? probe->dataInfo->GetFormID() : 0;
		logger::info("own voices: {} (the plugin's borrowed line 000AFE holds {:08X}, its DNAM in the plugin is {:08X})",
			ready ? "ready" : "NOT ready: a line, topic or faction it needs is missing", held, want ? want->GetFormID() : 0);
		if (probe && want && held != 0 && held != want->GetFormID()) {
			off = true;
			logger::info("own voices: off until the game restarts: a line's response-data pointer holds something other than its DNAM");
		}
	}

	inline bool Compare(float a_value, RE::CONDITION_ITEM_DATA::OpCode a_op, float a_to)
	{
		using Op = RE::CONDITION_ITEM_DATA::OpCode;
		switch (a_op) {
		case Op::kEqualTo:
			return a_value == a_to;
		case Op::kNotEqualTo:
			return a_value != a_to;
		case Op::kGreaterThan:
			return a_value > a_to;
		case Op::kGreaterThanOrEqualTo:
			return a_value >= a_to;
		case Op::kLessThan:
			return a_value < a_to;
		case Op::kLessThanOrEqualTo:
			return a_value <= a_to;
		}
		return false;
	}

	inline bool ItemTrue(const RE::TESConditionItem* a_item, RE::ConditionCheckParams& a_params)
	{
		const auto& d = a_item->data;
		if (d.functionData.function.get() == RE::FUNCTION_DATA::FunctionID::kGetInFaction &&
			d.functionData.params[0] == currentFollower && d.object.get() == RE::CONDITIONITEMOBJECT::kSelf && !d.flags.global) {
			return Compare(1.0f, d.flags.opCode, d.comparisonValue.f);
		}
		return a_item->IsTrue(a_params);
	}

	inline bool Passes(const RE::TESTopicInfo* a_info, RE::Actor* a_who, RE::PlayerCharacter* a_player)
	{
		RE::ConditionCheckParams params(a_who, a_player);
		bool group = false, open = false;
		for (const auto* it = a_info->objConditions.head; it; it = it->next) {
			group = group || ItemTrue(it, params);
			if (it->data.flags.isOR) {
				open = true;
				continue;
			}
			if (!group) {
				return false;
			}
			group = open = false;
		}
		return !open || group;
	}

	inline RE::TESTopicInfo* Answer(RE::TESTopic* a_topic, RE::Actor* a_who, RE::PlayerCharacter* a_player)
	{
		if (!a_topic->topicInfos) {
			return nullptr;
		}
		for (std::uint32_t k = 0; k < a_topic->numTopicInfos; ++k) {
			auto* info = a_topic->topicInfos[k];
			if (!info || info->parentTopic != a_topic) {
				continue;
			}
			if (Passes(info, a_who, a_player)) {
				return info;
			}
		}
		return nullptr;
	}

	inline void Clear()
	{
		if (!pointed) {
			return;
		}
		for (auto* o : ours) {
			if (o) {
				o->dataInfo = nullptr;
			}
		}
		pointed = false;
	}

	inline std::string FileOf(const RE::TESForm* a_form)
	{
		const auto* f = a_form ? a_form->GetFile(0) : nullptr;
		return f ? std::string(f->GetFilename()) : std::string();
	}

	inline bool Prove(std::size_t a_i, RE::TESTopicInfo* a_answer, RE::Actor* a_who)
	{
		auto* topic = ours[a_i]->parentTopic;
		auto* quest = topic ? topic->ownerQuest : nullptr;
		if (!quest) {
			return false;
		}
		std::unique_ptr<RE::DialogueItem> item(new RE::DialogueItem(quest, topic, ours[a_i], a_who));
		std::string voice;
		for (auto* r : item->responses) {
			if (r && r->voice.c_str()) {
				voice = r->voice.c_str();
				break;
			}
		}
		std::string low = voice;
		std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		std::string file = FileOf(a_answer);
		std::transform(file.begin(), file.end(), file.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (low.find("warband.esp") != std::string::npos) {
			logger::info("own voices: off until the game restarts: Warband's line {:06X} pointed at {:08X} ({}) is still voiced as '{}'",
				ours[a_i]->GetLocalFormID(), a_answer->GetFormID(), FileOf(a_answer), voice);
			off = true;
			return false;
		}
		if (!file.empty() && low.find(file) != std::string::npos) {
			proven = true;
			logger::info("own voices: proven: Warband's line {:06X} pointed at {:08X} ({}) is voiced as '{}'",
				ours[a_i]->GetLocalFormID(), a_answer->GetFormID(), FileOf(a_answer), voice);
			return true;
		}
		logger::info("own voices: not proven yet: Warband's line {:06X} pointed at {:08X} ({}) is voiced as '{}'",
			ours[a_i]->GetLocalFormID(), a_answer->GetFormID(), FileOf(a_answer), voice);
		return false;
	}

	inline void Point()
	{
		Clear();
		if (!ready || off) {
			return;
		}
		auto* topics = RE::MenuTopicManager::GetSingleton();
		auto  ref = topics ? topics->speaker.get() : RE::NiPointer<RE::TESObjectREFR>();
		auto* who = ref ? ref->As<RE::Actor>() : nullptr;
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!who || !player || !who->IsInFaction(band)) {
			return;
		}
		const auto* base = who->GetActorBase();
		for (const auto* s : starters) {
			if (s && base == s) {
				return;
			}
		}
		for (std::size_t i = 0; i < kLines.size(); ++i) {
			auto* answer = Answer(theirs[i], who, player);
			if (!answer) {
				continue;
			}
			ours[i]->dataInfo = answer;
			pointed = true;
			if (!proven && !Prove(i, answer, who)) {
				if (off) {
					Clear();
					return;
				}
				ours[i]->dataInfo = nullptr;
			}
		}
	}

	class Sink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
		{
			if (a_event && a_event->menuName == RE::DialogueMenu::MENU_NAME) {
				if (a_event->opening) {
					SKSE::GetTaskInterface()->AddTask([]() { Point(); });
				} else {
					SKSE::GetTaskInterface()->AddTask([]() { Clear(); });
				}
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};
	inline Sink sink;
}
