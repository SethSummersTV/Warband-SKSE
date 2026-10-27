#pragma once

namespace PartyData
{
	inline constexpr RE::FormID kGold = 0x0000000F;

	inline std::int32_t GoldOf(RE::Actor* a_actor)
	{
		auto* gold = a_actor ? RE::TESForm::LookupByID<RE::TESBoundObject>(kGold) : nullptr;
		if (!gold) {
			return 0;
		}
		const auto inventory = a_actor->GetInventory([](RE::TESBoundObject& a_object) { return a_object.GetFormID() == kGold; });
		const auto it = inventory.find(gold);
		return it != inventory.end() ? it->second.first : 0;
	}

	inline std::string Esc(std::string_view a_in)
	{
		std::string out;
		out.reserve(a_in.size() + 8);
		for (const char c : a_in) {
			switch (c) {
			case '"':
				out += "\\\"";
				break;
			case '\\':
				out += "\\\\";
				break;
			case '\n':
				out += "\\n";
				break;
			case '\r':
			case '\t':
				out += ' ';
				break;
			default:
				if (static_cast<unsigned char>(c) >= 0x20) {
					out += c;
				}
			}
		}
		return out;
	}

	inline std::string Str(std::string_view a_in) { return "\"" + Esc(a_in) + "\""; }
	inline std::string Str(const char* a_in) { return Str(std::string_view(a_in ? a_in : "")); }
	inline std::string Str(const std::string& a_in) { return Str(std::string_view(a_in)); }

	inline int AV(RE::Actor* a_actor, RE::ActorValue a_av)
	{
		return static_cast<int>(std::lround(a_actor->AsActorValueOwner()->GetActorValue(a_av)));
	}

	inline std::string Pair(RE::Actor* a_actor, RE::ActorValue a_av)
	{
		auto* owner = a_actor->AsActorValueOwner();
		const float cur = owner->GetActorValue(a_av);
		const float max = owner->GetPermanentActorValue(a_av) +
		                  a_actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, a_av);
		return "[" + std::to_string((std::max)(0, static_cast<int>(std::lround(cur)))) + "," +
		       std::to_string((std::max)(1, static_cast<int>(std::lround(max)))) + "]";
	}

	inline std::string Item(const RE::TESForm* a_form, int a_value)
	{
		if (!a_form) {
			return "null";
		}
		return "{\"n\":" + Str(a_form->GetName()) + ",\"v\":" + std::to_string(a_value) + "}";
	}

	inline std::string Hand(RE::Actor* a_actor, bool a_left)
	{
		auto* form = a_actor->GetEquippedObject(a_left);
		if (!form) {
			return "null";
		}
		if (auto* weap = form->As<RE::TESObjectWEAP>(); weap && a_left && (weap->IsTwoHandedSword() || weap->IsTwoHandedAxe() ||
		                                                                      weap->IsBow() || weap->IsCrossbow())) {
			return "null";
		}
		if (auto* weap = form->As<RE::TESObjectWEAP>()) {
			return Item(weap, weap->GetAttackDamage());
		}
		if (auto* armo = form->As<RE::TESObjectARMO>()) {
			return Item(armo, static_cast<int>(armo->armorRating / 100));
		}
		return Item(form, 0);
	}

	inline std::string Worn(RE::Actor* a_actor, RE::BGSBipedObjectForm::BipedObjectSlot a_slot)
	{
		auto* armo = a_actor->GetWornArmor(a_slot);
		return armo ? Item(armo, static_cast<int>(armo->armorRating / 100)) : "null";
	}

	inline std::string Hex(std::uint32_t a_v, int a_width)
	{
		char buf[16];
		if (a_width) {
			std::snprintf(buf, sizeof(buf), "%0*X", a_width, a_v);
		} else {
			std::snprintf(buf, sizeof(buf), "%X", a_v);
		}
		return buf;
	}

	inline int MaxResist()
	{
		if (auto* gmst = RE::GameSettingCollection::GetSingleton()) {
			if (auto* setting = gmst->GetSetting("fPlayerMaxResistance")) {
				return static_cast<int>(std::lround(setting->GetFloat()));
			}
		}
		return 85;
	}

	inline std::string Pics(RE::Actor* a_actor)
	{
		auto* npc = a_actor->GetActorBase();
		if (npc && npc->IsDynamicForm() && npc->faceNPC) {
			npc = npc->faceNPC;
		}
		if (!npc) {
			return "[]";
		}
		std::string list = "[";
		if (auto* file = npc->GetFile(0)) {
			list += Str(std::string(file->GetFilename()) + "-0x" + Hex(npc->GetLocalFormID(), 0) + ".jpg") + ",";
		}
		list += Str(Hex(npc->GetFormID(), 8) + ".jpg");
		if (const char* name = npc->GetName(); name && *name) {
			list += "," + Str(std::string(name) + ".jpg");
		}
		return list + "]";
	}

	inline std::string Effects(RE::Actor* a_actor)
	{
		auto* target = a_actor->AsMagicTarget();
		auto* list = target ? target->GetActiveEffectList() : nullptr;
		std::string out = "[";
		std::vector<std::string> seen;
		int n = 0;
		if (list) {
			for (auto* ae : *list) {
				if (!ae || n >= 30 || ae->flags.any(RE::ActiveEffect::Flag::kInactive, RE::ActiveEffect::Flag::kDispelled)) {
					continue;
				}
				auto* mgef = ae->GetBaseObject();
				if (!mgef || mgef->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kHideInUI)) {
					continue;
				}
				const auto type = ae->spell ? ae->spell->GetSpellType() : RE::MagicSystem::SpellType::kSpell;
				const bool disease = type == RE::MagicSystem::SpellType::kDisease;
				if (type == RE::MagicSystem::SpellType::kAbility && !disease) {
					continue;
				}
				std::string name = disease && ae->spell ? ae->spell->GetName() : mgef->GetName();
				if (name.empty() || std::find(seen.begin(), seen.end(), name) != seen.end()) {
					continue;
				}
				seen.push_back(name);
				const int left = ae->duration > 0.0f ? static_cast<int>(std::lround((std::max)(0.0f, ae->duration - ae->elapsedSeconds))) : -1;
				out += (n++ ? "," : "") + std::string("{\"n\":") + Str(name) + ",\"m\":" + std::to_string(static_cast<int>(std::lround(ae->magnitude))) +
				       ",\"t\":" + std::to_string(left) + ",\"bad\":" + ((mgef->IsHostile() || mgef->IsDetrimental()) ? "true" : "false") +
				       ",\"dis\":" + (disease ? "true" : "false") + "}";
			}
		}
		return out + "]";
	}

	inline std::string Perks(RE::Actor* a_actor)
	{
		std::vector<std::string> names;
		const auto add = [&](RE::BGSPerk* a_perk) {
			if (!a_perk || !a_perk->data.playable || a_perk->data.hidden || names.size() >= 80) {
				return;
			}
			std::string n = a_perk->GetName();
			if (!n.empty() && std::find(names.begin(), names.end(), n) == names.end()) {
				names.push_back(n);
			}
		};
		if (a_actor->IsPlayerRef()) {
			if (auto* player = RE::PlayerCharacter::GetSingleton()) {
				for (auto* perk : player->GetPlayerRuntimeData().perks) {
					add(perk);
				}
			}
		} else if (auto* npc = a_actor->GetActorBase()) {
			for (std::uint32_t i = 0; i < npc->perkCount; ++i) {
				add(npc->perks[i].perk);
			}
		}
		std::string out = "[";
		for (std::size_t i = 0; i < names.size(); ++i) {
			out += (i ? "," : "") + Str(names[i]);
		}
		return out + "]";
	}

	inline const char* Standing(RE::Actor* a_actor)
	{
		static constexpr const char* words[9] = { "Lover", "Ally", "Confidant", "Friend", "Acquaintance", "Rival", "Foe", "Enemy", "Archnemesis" };
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* mine = player ? player->GetActorBase() : nullptr;
		auto* npc = a_actor->GetActorBase();
		if (!mine || !npc || npc == mine) {
			return "";
		}
		const auto find = [mine](RE::TESNPC* a_npc) -> RE::BGSRelationship* {
			if (!a_npc || a_npc == mine) {
				return nullptr;
			}
			auto* rel = RE::BGSRelationship::GetRelationship(a_npc, mine);
			return rel ? rel : RE::BGSRelationship::GetRelationship(mine, a_npc);
		};
		auto* rel = find(npc);
		if (!rel && npc->IsDynamicForm() && npc->faceNPC) {
			rel = find(npc->faceNPC);
		}
		if (!rel) {
			return words[4];
		}
		const auto level = static_cast<std::size_t>(rel->level.underlying());
		return level < 9 ? words[level] : "";
	}

	inline std::string Member(RE::Actor* a_actor, const std::string& a_extra)
	{
		using AVs = RE::ActorValue;
		using Slot = RE::BGSBipedObjectForm::BipedObjectSlot;
		static constexpr AVs skills[18] = {
			AVs::kOneHanded, AVs::kSneak, AVs::kConjuration, AVs::kTwoHanded, AVs::kArchery, AVs::kIllusion,
			AVs::kBlock, AVs::kSpeech, AVs::kAlteration, AVs::kHeavyArmor, AVs::kAlchemy, AVs::kDestruction,
			AVs::kLightArmor, AVs::kLockpicking, AVs::kRestoration, AVs::kSmithing, AVs::kPickpocket, AVs::kEnchanting
		};
		std::string j = "{" + a_extra;
		const auto* race = a_actor->GetRace();
		j += ",\"race\":" + Str(race ? race->GetName() : "");
		j += ",\"lv\":" + std::to_string(a_actor->GetLevel());
		j += ",\"pics\":" + Pics(a_actor);
		j += ",\"hp\":" + Pair(a_actor, AVs::kHealth) + ",\"sp\":" + Pair(a_actor, AVs::kStamina) +
		     ",\"mp\":" + Pair(a_actor, AVs::kMagicka);
		j += ",\"skills\":[";
		for (int i = 0; i < 18; ++i) {
			j += (i ? "," : "") + std::to_string(AV(a_actor, skills[i]));
		}
		const int cap = MaxResist();
		const auto res = [&](AVs a_av, int a_max) { return std::to_string((std::min)(AV(a_actor, a_av), a_max)); };
		j += "],\"def\":{\"armor\":" + std::to_string(AV(a_actor, AVs::kDamageResist)) +
		     ",\"magic\":" + res(AVs::kResistMagic, cap) +
		     ",\"fire\":" + res(AVs::kResistFire, cap) +
		     ",\"frost\":" + res(AVs::kResistFrost, cap) +
		     ",\"shock\":" + res(AVs::kResistShock, cap) +
		     ",\"poison\":" + res(AVs::kPoisonResist, 100) +
		     ",\"disease\":" + res(AVs::kResistDisease, 100) + "}";
		int dmg = AV(a_actor, AVs::kUnarmedDamage);
		if (auto* form = a_actor->GetEquippedObject(false)) {
			if (auto* weap = form->As<RE::TESObjectWEAP>()) {
				dmg = weap->GetAttackDamage();
			}
		}
		char speed[16];
		std::snprintf(speed, sizeof(speed), "%.2f", a_actor->AsActorValueOwner()->GetActorValue(AVs::kWeaponSpeedMult));
		j += ",\"cmb\":{\"dmg\":" + std::to_string(dmg) + ",\"move\":" + std::to_string(AV(a_actor, AVs::kSpeedMult)) +
		     ",\"speed\":" + speed + "}";
		j += ",\"gear\":{\"right\":" + Hand(a_actor, false) + ",\"left\":" + Hand(a_actor, true) +
		     ",\"head\":" + Worn(a_actor, Slot::kHead) + ",\"body\":" + Worn(a_actor, Slot::kBody) +
		     ",\"hands\":" + Worn(a_actor, Slot::kHands) + ",\"feet\":" + Worn(a_actor, Slot::kFeet) +
		     ",\"neck\":" + Worn(a_actor, Slot::kAmulet) + ",\"ring\":" + Worn(a_actor, Slot::kRing) + "}";
		j += ",\"effects\":" + Effects(a_actor) + ",\"perks\":" + Perks(a_actor);
		const bool isPlayer = a_actor->IsPlayerRef();
		const bool isPet = a_extra.find("\"pet\":") != std::string::npos;
		if (isPlayer) {
			j += ",\"gold\":" + std::to_string(GoldOf(a_actor));
		}
		if (isPlayer || !isPet) {
			j += ",\"carry\":" + Str(std::to_string(AV(a_actor, AVs::kInventoryWeight)) + " / " +
			                         std::to_string(AV(a_actor, AVs::kCarryWeight)));
		}
		if (!isPlayer && !isPet) {
			if (const char* rel = Standing(a_actor); *rel) {
				j += ",\"rel\":" + Str(rel);
			}
			j += std::string(",\"ess\":") + (a_actor->IsEssential() ? "true" : "false") +
			     ",\"prot\":" + (a_actor->IsProtected() ? "true" : "false");
		}
		return j + "}";
	}

	inline std::string FixWords(std::string a_json)
	{
		static constexpr std::pair<std::string_view, std::string_view> words[] = {
			{ "True", "true" }, { "False", "false" }, { "None", "null" }, { "NULL", "null" }, { "Null", "null" }, { "TRUE", "true" }, { "FALSE", "false" }
		};
		for (const auto& [bad, good] : words) {
			for (const char* lead : { ":", ": ", ",", "[" }) {
				const std::string from = std::string(lead) + std::string(bad);
				const std::string to = std::string(lead) + std::string(good);
				std::size_t p = 0;
				while ((p = a_json.find(from, p)) != std::string::npos) {
					const std::size_t end = p + from.size();
					const char next = end < a_json.size() ? a_json[end] : ',';
					if (next == ',' || next == '}' || next == ']' || next == ' ') {
						a_json.replace(p, from.size(), to);
						p += to.size();
					} else {
						p = end;
					}
				}
			}
		}
		return a_json;
	}

	inline std::string Party(const std::vector<RE::Actor*>& a_actors, const std::vector<std::string>& a_extras,
		const std::string& a_band, int a_sel)
	{
		std::string members;
		const auto n = (std::min)(a_actors.size(), a_extras.size());
		int sel = a_sel;
		for (std::size_t i = 0; i < n; ++i) {
			if (!a_actors[i] || a_extras[i].empty()) {
				if (static_cast<int>(i) < a_sel) {
					--sel;
				}
				continue;
			}
			members += (members.empty() ? "" : ",") + Member(a_actors[i], a_extras[i]);
		}
		return FixWords("{\"band\":" + (a_band.empty() ? std::string("{}") : a_band) +
		       ",\"tempers\":[\"Bold\",\"Steady\",\"Careful\",\"Peaceful healer\",\"Own nature\"]" +
		       ",\"sel\":" + std::to_string((std::max)(0, sel)) + ",\"members\":[" + members + "]}");
	}
}
