#pragma once
#include "game/Canary.h"

namespace OwnFollow
{
	inline constexpr RE::FormID kCurrentFollowerFaction = 0x05C84E;
	inline constexpr RE::FormID kPotentialFollowerFaction = 0x05C84D;
	inline constexpr RE::FormID kPlayerFollowerCount = 0x0BCC98;

	inline RE::TESFaction* currentFollower = nullptr;
	inline RE::TESFaction* potentialFollower = nullptr;
	inline RE::TESGlobal*  followerCount = nullptr;
	inline const RE::TESFile* warbandFile = nullptr;
	inline RE::FormID      lastSpeaker = 0;
	inline RE::FormID      lastLine = 0;
	inline std::uint64_t   lastSent = 0;
	inline int             seenBad = 0;

	inline bool Holds(RE::CONDITION_ITEM_DATA::OpCode a_op, float a_value, float a_to)
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
		default:
			return false;
		}
	}

	inline bool AsksFaction(const RE::CONDITION_ITEM_DATA& a_d, const RE::TESFaction* a_faction, bool a_in)
	{
		using Fn = RE::FUNCTION_DATA::FunctionID;
		const auto fn = a_d.functionData.function.get();
		if (!a_faction || a_d.functionData.params[0] != a_faction) {
			return false;
		}
		const float v = a_d.comparisonValue.f;
		const auto  op = a_d.flags.opCode;
		float       inside = 0.0f, outside = 0.0f;
		if (fn == Fn::kGetInFaction) {
			inside = 1.0f;
			outside = 0.0f;
		} else if (fn == Fn::kGetFactionRank) {
			inside = 0.0f;
			outside = -1.0f;
		} else {
			return false;
		}
		return a_in ? Holds(op, inside, v) && !Holds(op, outside, v) : Holds(op, outside, v) && !Holds(op, inside, v);
	}

	inline bool IsRecruitLine(const RE::TESTopicInfo* a_info)
	{
		using Fn = RE::FUNCTION_DATA::FunctionID;
		if (Game::Canary::ConditionsProblem(a_info) != nullptr) {
			return false;
		}
		const auto* topic = a_info->parentTopic;
		if (!topic || topic->data.type.get() != RE::DIALOGUE_TYPE::kPlayerDialogue) {
			return false;
		}
		if (warbandFile && a_info->GetFile(0) == warbandFile) {
			return false;
		}
		bool notFollowing = false, mayFollow = false, prevOr = false;
		int  n = 0;
		for (const auto* c = a_info->objConditions.head; c; c = c->next) {
			if (++n > 256 || !Game::Canary::SaneCondition(c)) {
				return false;
			}
			const auto& d = c->data;
			const bool inOr = prevOr || d.flags.isOR;
			prevOr = d.flags.isOR;
			if (inOr || d.flags.global) {
				continue;
			}
			const auto fn = d.functionData.function.get();
			if (fn == Fn::kGetGlobalValue) {
				if (followerCount && d.functionData.params[0] == followerCount && Holds(d.flags.opCode, 0.0f, d.comparisonValue.f)) {
					mayFollow = true;
				}
				continue;
			}
			if (d.object.get() != RE::CONDITIONITEMOBJECT::kSelf) {
				continue;
			}
			if (fn == Fn::kGetPlayerTeammate) {
				if (Holds(d.flags.opCode, 0.0f, d.comparisonValue.f) && !Holds(d.flags.opCode, 1.0f, d.comparisonValue.f)) {
					notFollowing = true;
				}
			} else if (AsksFaction(d, currentFollower, false)) {
				notFollowing = true;
			} else if (AsksFaction(d, potentialFollower, true)) {
				mayFollow = true;
			}
		}
		return notFollowing && mayFollow;
	}

	inline void Check(RE::FormID a_speaker, RE::FormID a_line)
	{
		auto* info = RE::TESForm::LookupByID<RE::TESTopicInfo>(a_line);
		auto* speaker = RE::TESForm::LookupByID<RE::Actor>(a_speaker);
		if (!info || !speaker) {
			if (seenBad < 3) {
				++seenBad;
				logger::info("own follow line: an event in the dialogue menu named {:08X} / {:08X}, which are not a line and a person: ignored",
					a_line, a_speaker);
			}
			return;
		}
		if (!IsRecruitLine(info)) {
			return;
		}
		const auto now = GetTickCount64();
		if (a_speaker == lastSpeaker && a_line == lastLine && now - lastSent < 20000) {
			return;
		}
		lastSpeaker = a_speaker;
		lastLine = a_line;
		lastSent = now;
		const auto* file = info->GetFile(0);
		logger::info("own follow line: {:08X} ({}) said recruit line {:08X} of {}: sent to Papyrus", a_speaker, speaker->GetDisplayFullName(),
			a_line, file ? file->GetFilename() : "?");
		SKSE::ModCallbackEvent ev{ "Warband_OwnFollowLine", RE::BSFixedString(std::to_string(static_cast<std::int32_t>(a_line)).c_str()), 0.0f, speaker };
		SKSE::GetModCallbackEventSource()->SendEvent(&ev);
	}

	class Sink final : public RE::BSTEventSink<RE::TESTopicInfoEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(const RE::TESTopicInfoEvent* a_event, RE::BSTEventSource<RE::TESTopicInfoEvent>*) override
		{
			auto* ui = RE::UI::GetSingleton();
			if (!a_event || !ui || !ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) {
				return RE::BSEventNotifyControl::kContinue;
			}
			const auto* raw = reinterpret_cast<const std::byte*>(a_event);
			if (!Game::Canary::Readable(raw, 0x18)) {
				return RE::BSEventNotifyControl::kContinue;
			}
			const auto* who = *reinterpret_cast<const RE::TESObjectREFR* const*>(raw);
			const auto  line = *reinterpret_cast<const RE::FormID*>(raw + 0x10);
			if (!who || !Game::Canary::Is(who, "Character")) {
				return RE::BSEventNotifyControl::kContinue;
			}
			const auto speaker = who->GetFormID();
			if (auto* tasks = SKSE::GetTaskInterface()) {
				tasks->AddTask([speaker, line]() { Check(speaker, line); });
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};
	inline Sink sink;

	inline void Start()
	{
		auto* dh = RE::TESDataHandler::GetSingleton();
		auto* events = RE::ScriptEventSourceHolder::GetSingleton();
		if (!dh || !events) {
			logger::info("own follow line: off (no data handler or event holder)");
			return;
		}
		currentFollower = dh->LookupForm<RE::TESFaction>(kCurrentFollowerFaction, "Skyrim.esm");
		potentialFollower = dh->LookupForm<RE::TESFaction>(kPotentialFollowerFaction, "Skyrim.esm");
		followerCount = dh->LookupForm<RE::TESGlobal>(kPlayerFollowerCount, "Skyrim.esm");
		warbandFile = dh->LookupModByName("Warband.esp");
		if (!currentFollower || (!potentialFollower && !followerCount)) {
			logger::info("own follow line: off (the game's follower faction or follower count is missing)");
			return;
		}
		events->AddEventSink<RE::TESTopicInfoEvent>(&sink);
		logger::info("own follow line: listening for a follower's own recruit line that goes nowhere");
	}
}
