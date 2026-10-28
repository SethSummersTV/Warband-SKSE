#pragma once
#include "ControlMap.h"
#include "Player.h"

namespace Game::Canary
{
	[[nodiscard]] inline bool Readable(const void* a_at, std::size_t a_size) noexcept
	{
		auto at = reinterpret_cast<std::uintptr_t>(a_at);
		if (at < 0x10000 || a_size == 0 || at + a_size < at) {
			return false;
		}
		const auto end = at + a_size;
		while (at < end) {
			MEMORY_BASIC_INFORMATION info{};
			if (VirtualQuery(reinterpret_cast<const void*>(at), &info, sizeof(info)) == 0) {
				return false;
			}
			const DWORD kind = info.Protect & 0xFF;
			const bool  read = kind == PAGE_READONLY || kind == PAGE_READWRITE || kind == PAGE_WRITECOPY || kind == PAGE_EXECUTE_READ ||
			                  kind == PAGE_EXECUTE_READWRITE || kind == PAGE_EXECUTE_WRITECOPY;
			if (info.State != MEM_COMMIT || !read || (info.Protect & PAGE_GUARD) != 0) {
				return false;
			}
			at = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
		}
		return true;
	}

	struct ExeImage
	{
		std::uintptr_t base = 0;
		std::uintptr_t size = 0;

		[[nodiscard]] bool Has(std::uintptr_t a_at) const noexcept { return a_at >= base && a_at < base + size; }
	};

	[[nodiscard]] inline const ExeImage& Exe() noexcept
	{
		static const ExeImage image = []() {
			ExeImage    out;
			const auto  base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
			const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
			if (base && Readable(dos, sizeof(IMAGE_DOS_HEADER)) && dos->e_magic == IMAGE_DOS_SIGNATURE) {
				const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
				if (Readable(nt, sizeof(IMAGE_NT_HEADERS64)) && nt->Signature == IMAGE_NT_SIGNATURE) {
					out.base = base;
					out.size = nt->OptionalHeader.SizeOfImage;
				}
			}
			return out;
		}();
		return image;
	}

	struct Type
	{
		enum Where : std::uint8_t
		{
			kNoObject,
			kOtherModule,
			kGame,
		};

		Where            where = kNoObject;
		std::string_view name;
		std::uint32_t    offset = 0;
	};

	[[nodiscard]] inline Type TypeOf(const void* a_part) noexcept
	{
		Type        type;
		const auto& exe = Exe();
		if (exe.size == 0 || !Readable(a_part, 8)) {
			return type;
		}
		const auto table = *static_cast<const std::uintptr_t*>(a_part);
		if (!exe.Has(table)) {
			HMODULE other = nullptr;
			if (table >= 0x10000 && Readable(reinterpret_cast<const void*>(table), 8) &&
				GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
					reinterpret_cast<LPCWSTR>(table), &other) &&
				other) {
				type.where = Type::kOtherModule;
			}
			return type;
		}
		if (!exe.Has(table - 8) || !Readable(reinterpret_cast<const void*>(table - 8), 8)) {
			return type;
		}
		const auto locator = *reinterpret_cast<const std::uintptr_t*>(table - 8);
		if (!exe.Has(locator) || !Readable(reinterpret_cast<const void*>(locator), 0x18)) {
			return type;
		}
		const auto* fields = reinterpret_cast<const std::uint32_t*>(locator);
		if (fields[0] != 1 || exe.base + fields[5] != locator) {
			return type;
		}
		const auto descriptor = exe.base + fields[3];
		if (!exe.Has(descriptor) || !exe.Has(descriptor + 0x12) || !Readable(reinterpret_cast<const void*>(descriptor), 0x12)) {
			return type;
		}
		const char* name = reinterpret_cast<const char*>(descriptor + 0x10);
		if (name[0] != '.') {
			return type;
		}
		std::size_t       length = 1;
		const std::size_t most = (std::min<std::size_t>)(4096, exe.base + exe.size - (descriptor + 0x10));
		while (length < most && ((length % 256) != 0 || Readable(name + length, 1)) && name[length] != '\0') {
			++length;
		}
		type.where = Type::kGame;
		type.name = std::string_view(name, length);
		type.offset = fields[1];
		return type;
	}

	[[nodiscard]] inline bool Named(std::string_view a_decorated, std::string_view a_class) noexcept
	{
		return a_decorated.size() == a_class.size() + 6 && a_decorated.substr(0, 3) == ".?A" && (a_decorated[3] == 'V' || a_decorated[3] == 'U') &&
		       a_decorated.substr(4, a_class.size()) == a_class && a_decorated.substr(4 + a_class.size()) == "@@";
	}

	[[nodiscard]] inline bool Is(const void* a_object, std::string_view a_class) noexcept
	{
		const auto type = TypeOf(a_object);
		return type.where == Type::kGame && type.offset == 0 && Named(type.name, a_class);
	}

	[[nodiscard]] inline bool IsGameObject(const void* a_object) noexcept
	{
		const auto type = TypeOf(a_object);
		return type.where == Type::kGame && type.offset == 0;
	}

	[[nodiscard]] inline bool IsSomeObject(const void* a_object) noexcept
	{
		const auto type = TypeOf(a_object);
		return type.where == Type::kOtherModule || (type.where == Type::kGame && type.offset == 0);
	}

	inline std::atomic<bool> ran{ false };
	inline std::atomic<bool> actors{ true };
	inline std::atomic<bool> camera{ true };
	inline std::atomic<bool> hud{ true };
	inline std::atomic<bool> fightStyles{ true };
	inline std::atomic<bool> followerLines{ true };
	inline bool              canNotify = true;

	struct Group
	{
		std::atomic<bool>& on;
		const char*        notice;
		const char*        loses;
		bool               settled = false;
		int                softFailures = 0;
	};

	inline Group actorsGroup{ actors, "Warband: the party screen and the band HUD are off on this game version. See WarbandUI.log.",
		"the party screen (the list menus take over), the band HUD (the iWant HUD takes over), the inspect card and fight styles" };
	inline Group cameraGroup{ camera, "Warband: aiming the camp with the crosshair is off on this game version. See WarbandUI.log.",
		"aiming the camp with the crosshair (the camp goes in front of you) and the direction pip on HUD cards" };
	inline Group hudGroup{ hud, "Warband: the band HUD is off on this game version. See WarbandUI.log.", "the band HUD (the iWant HUD takes over)" };
	inline Group fightStylesGroup{ fightStyles, "Warband: fight styles are off on this game version. See WarbandUI.log.",
		"fight styles (members keep their own way of fighting)" };
	inline Group followerLinesGroup{ followerLines, "Warband: hiding the game's own follower lines is off on this game version. See WarbandUI.log.",
		"hiding the game's own follower lines on band members (the MCM switch \"Count as followers for other mods\" shows them again)" };

	inline void SwitchOff(Group& a_group, const char* a_check)
	{
		a_group.settled = true;
		if (!a_group.on.exchange(false)) {
			return;
		}
		logger::error("layout check failed on Skyrim {}: it should hold that {}. Off for this session: {}.", Runtime().version.Text(), a_check, a_group.loses);
		if (canNotify) {
			RE::DebugNotification(a_group.notice);
		}
	}

	namespace detail
	{
		struct Checks
		{
			const char* structural = nullptr;
			const char* stateDependent = nullptr;

			[[nodiscard]] bool Failed() const noexcept { return structural || stateDependent; }

			bool Hard(bool a_ok, const char* a_what) noexcept
			{
				if (!Failed() && !a_ok) {
					structural = a_what;
				}
				return !Failed();
			}

			bool Soft(bool a_ok, const char* a_what) noexcept
			{
				if (!Failed() && !a_ok) {
					stateDependent = a_what;
				}
				return !Failed();
			}
		};

		inline Checks CheckActors()
		{
			Checks need;
			auto*  player = RE::PlayerCharacter::GetSingleton();
			if (!need.Hard(player && Readable(player, 0x500) && Is(player, "PlayerCharacter"), "the player pointer points at the player object")) {
				return need;
			}
			if (!need.Hard(player->GetFormID() == 0x14, "the player's form id reads 00000014")) {
				return need;
			}
			const auto isPart = [&](const void* a_part) {
				const auto type = TypeOf(a_part);
				return type.where == Type::kGame && Named(type.name, "PlayerCharacter") &&
				       type.offset == static_cast<std::uint32_t>(static_cast<const char*>(a_part) - reinterpret_cast<const char*>(player));
			};
			if (!need.Hard(isPart(player->AsMagicTarget()), "an actor's magic-target part starts where the library has it") ||
				!need.Hard(isPart(player->AsActorValueOwner()), "an actor's actor-value part starts where the library has it") ||
				!need.Hard(isPart(player->AsActorState()), "an actor's state part starts where the library has it")) {
				return need;
			}
			auto* base = player->GetBaseObject();
			if (!need.Hard(base && Is(base, "TESNPC"), "the player's base object is an NPC") ||
				!need.Hard(base->GetFormID() == 0x7, "the player's base NPC reads form id 00000007")) {
				return need;
			}
			auto* npc = static_cast<RE::TESNPC*>(base);
			if (!need.Hard(npc->race && Is(npc->race, "TESRace"), "the player NPC's race is a race")) {
				return need;
			}

			if (!need.Soft(!npc->npcClass || Is(npc->npcClass, "TESClass"), "an NPC's class is a class or empty") ||
				!need.Soft(!npc->combatStyle || Is(npc->combatStyle, "TESCombatStyle"), "an NPC's combat style is a combat style or empty") ||
				!need.Soft(!npc->faceNPC || Is(npc->faceNPC, "TESNPC"), "an NPC's face NPC is an NPC or empty")) {
				return need;
			}
			const auto& data = player->GetActorRuntimeData();
			if (!need.Soft(!data.race || Is(data.race, "TESRace"), "an actor's own race is a race or empty")) {
				return need;
			}
			for (const auto hand : { RE::Actor::SlotTypes::kLeftHand, RE::Actor::SlotTypes::kRightHand }) {
				const auto* caster = data.magicCasters[hand];
				const auto  type = TypeOf(caster);
				const bool  ok = !caster || type.where == Type::kOtherModule ||
				                 (type.where == Type::kGame && type.offset == 0 && type.name.find("MagicCaster") != std::string_view::npos);
				if (!need.Soft(ok, "an actor's hand caster is a magic caster or empty")) {
					return need;
				}
			}
			if (!need.Soft(!data.combatController || Readable(data.combatController, 0x48), "an actor's combat controller is readable memory or empty")) {
				return need;
			}
			if (const auto* process = data.currentProcess) {
				if (!need.Soft(Readable(process, 0x100), "an actor's process is readable memory")) {
					return need;
				}
				for (const auto* held : process->equippedObjects) {
					if (!need.Soft(!held || IsSomeObject(held), "what an actor holds in a hand is an object or nothing")) {
						return need;
					}
				}
			}
			if (!need.Soft(static_cast<std::uint32_t>(player->AsActorState()->GetLifeState()) <= 8, "the player's life state is one of the game's nine")) {
				return need;
			}
			if (const auto* perks = PlayerPerks()) {
				if (!need.Soft(Readable(perks, 0x18) && perks->size() <= perks->capacity() && perks->capacity() < 0x10000 &&
								   (perks->empty() || Readable(perks->data(), perks->size() * sizeof(void*))),
						"the player's perk list is a list")) {
					return need;
				}
				std::uint32_t looked = 0;
				for (const auto* perk : *perks) {
					if (looked++ >= 8) {
						break;
					}
					if (!need.Soft(!perk || Is(perk, "BGSPerk"), "the player's perks are perks")) {
						return need;
					}
				}
			}
			if (auto* effects = player->AsMagicTarget()->GetActiveEffectList(); effects) {
				if (!need.Soft(Readable(effects, 0x10), "the player's list of active effects is readable memory")) {
					return need;
				}
				RE::ActiveEffect* first = nullptr;
				for (auto* effect : *effects) {
					first = effect;
					break;
				}
				if (first) {
					if (!need.Soft(IsSomeObject(first) && Readable(first, 0x90), "an active effect is an object") ||
						!need.Soft(!first->spell || IsGameObject(first->spell), "an active effect's spell is a game object or empty") ||
						!need.Soft(!first->effect || Readable(first->effect, 0x20), "an active effect's effect entry is readable memory or empty")) {
						return need;
					}
					if (const auto* setting = first->effect ? first->effect->baseEffect : nullptr; setting) {
						if (!need.Soft(Is(setting, "EffectSetting"), "an effect entry's magic effect is a magic effect") ||
							!need.Soft(static_cast<std::uint32_t>(setting->data.archetype) < 64, "a magic effect's archetype is one of the game's") ||
							!need.Soft(static_cast<std::int32_t>(setting->data.resistVariable) >= -1 && static_cast<std::int32_t>(setting->data.resistVariable) < 256,
								"a magic effect's resistance is an actor value or none")) {
							return need;
						}
					}
				}
			}
			return need;
		}

		inline Checks CheckCamera()
		{
			Checks need;
			auto*  playerCamera = RE::PlayerCamera::GetSingleton();
			if (!need.Hard(playerCamera && Readable(playerCamera, 0x40) && Is(playerCamera, "PlayerCamera"), "the camera pointer points at the player camera")) {
				return need;
			}
			const auto* root = playerCamera->cameraRoot.get();
			if (!need.Soft(!root || (Readable(root, 0x110) && IsSomeObject(root)), "the camera's node is a scene node or empty")) {
				return need;
			}
			if (root) {
				const auto& world = root->world;
				bool        rotation = std::isfinite(world.translate.x) && std::isfinite(world.translate.y) && std::isfinite(world.translate.z);
				for (int row = 0; row < 3 && rotation; ++row) {
					const float x = world.rotate.entry[row][0], y = world.rotate.entry[row][1], z = world.rotate.entry[row][2];
					const float length = x * x + y * y + z * z;
					rotation = std::isfinite(length) && length > 0.01f && length < 100.0f;
				}
				need.Soft(rotation, "the camera node's world transform is a place and a rotation");
			}
			return need;
		}

		inline Checks CheckHud()
		{
			Checks need;
			auto*  ui = RE::UI::GetSingleton();
			const auto* controls = RE::ControlMap::GetSingleton();
			if (!need.Hard(ui && Readable(ui, 0x1C8), "the UI object is readable memory") ||
				!need.Hard(controls && Readable(controls, 0x130), "the control map is readable memory")) {
				return need;
			}
			if (!need.Soft(*reinterpret_cast<const std::uint8_t*>(&ui->menuSystemVisible) <= 1, "the UI's menus-shown flag is 0 or 1") ||
				!need.Soft(ui->numPausesGame <= 64, "the UI's count of pausing menus is small")) {
				return need;
			}
			const int typing = TextEntryCount();
			need.Soft(typing >= 0 && typing <= 32, "the count of text fields being typed in is small");
			return need;
		}

		inline void Judge(Group& a_group, const char* a_name, Checks (*a_check)())
		{
			if (a_group.settled) {
				return;
			}
			const Checks found = a_check();
			if (found.structural) {
				SwitchOff(a_group, found.structural);
			} else if (found.stateDependent) {
				if (++a_group.softFailures >= 2) {
					SwitchOff(a_group, found.stateDependent);
				} else {
					logger::warn("layout check ({}): this time it did not hold that {}. Nothing is switched off; it is asked again at the next "
								 "loaded or new game",
						a_name, found.stateDependent);
				}
			} else {
				a_group.softFailures = 0;
				a_group.settled = true;
			}
		}
	}

	inline void Run()
	{
		const bool wasOpen = !(actorsGroup.settled && cameraGroup.settled && hudGroup.settled);
		detail::Judge(actorsGroup, "actors", detail::CheckActors);
		detail::Judge(cameraGroup, "camera", detail::CheckCamera);
		detail::Judge(hudGroup, "HUD", detail::CheckHud);
		ran.store(true);
		if (wasOpen && actorsGroup.settled && cameraGroup.settled && hudGroup.settled && actors.load() && camera.load() && hud.load()) {
			logger::info("layout checks passed on Skyrim {}: actors, camera, HUD", Runtime().version.Text());
		}
	}

	inline void EnsureRan()
	{
		if (!ran.load()) {
			Run();
		}
	}

	enum class Slot
	{
		kUse,
		kWait,
		kOff,
	};

	inline bool styleSlotProven = false;

	[[nodiscard]] inline Slot StyleSlot(const RE::CombatController* a_controller)
	{
		if (!fightStyles.load() || !actors.load()) {
			return Slot::kOff;
		}
		if (!Readable(a_controller, 0x48)) {
			SwitchOff(fightStylesGroup, "a combat controller is readable memory");
			return Slot::kOff;
		}
		const auto* style = a_controller->combatStyle;
		if (!style) {
			return styleSlotProven ? Slot::kUse : Slot::kWait;
		}
		if (!Is(style, "TESCombatStyle")) {
			SwitchOff(fightStylesGroup, "a combat controller's style is a combat style or empty");
			return Slot::kOff;
		}
		styleSlotProven = true;
		return Slot::kUse;
	}

	[[nodiscard]] inline const char* ConditionsProblem(const RE::TESTopicInfo* a_info) noexcept
	{
		if (!Readable(a_info, 0x48) || !Is(a_info, "TESTopicInfo")) {
			return "a dialogue line found by its form type is a dialogue line";
		}
		if (a_info->parentTopic && !Is(a_info->parentTopic, "TESTopic")) {
			return "a dialogue line's topic is a topic or empty";
		}
		if (const auto* head = a_info->objConditions.head; head && !Readable(head, sizeof(RE::TESConditionItem))) {
			return "a dialogue line's first condition is readable memory or empty";
		}
		return nullptr;
	}

	[[nodiscard]] inline bool SaneCondition(const RE::TESConditionItem* a_item) noexcept
	{
		if (!Readable(a_item, sizeof(RE::TESConditionItem))) {
			return false;
		}
		const auto function = *reinterpret_cast<const std::uint16_t*>(&a_item->data.functionData.function);
		const auto comparison = static_cast<std::uint8_t>(*reinterpret_cast<const std::uint8_t*>(&a_item->data.flags) >> 5);
		const auto runOn = *reinterpret_cast<const std::uint8_t*>(&a_item->data.object);
		return function < 1024 && comparison <= 5 && runOn <= 7 && (!a_item->next || Readable(a_item->next, sizeof(RE::TESConditionItem)));
	}

	inline void FailFollowerLines(const char* a_check)
	{
		SwitchOff(followerLinesGroup, a_check);
	}
}
