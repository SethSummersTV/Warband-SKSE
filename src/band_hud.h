#pragma once
#include "SKSEMenuFramework.h"
#include "window_input.h"
#include "hud_extras.h"
#include "party_sheet_api.h"
#include "utf8_path.h"

namespace BandHUD
{
	namespace IG = ImGuiMCP;

	struct Card
	{
		std::uint32_t formID = 0;
		std::string name;
		std::string portrait;
		int level = 0;
		float hp = 1.0f;
		float mp = 1.0f;
		float sp = 1.0f;
		float dist = 0.0f;
		float rel = 0.0f;
		std::uint32_t effects = 0;
		int status = 0;
		bool guest = false;
	};

	struct Member
	{
		RE::ActorHandle handle;
		std::string name;
		bool hold = false;
		bool guest = false;
		std::string face;
	};

	inline std::mutex lock;
	inline std::vector<Member> roster;
	inline std::vector<Card> cards;
	inline bool show = false;
	inline bool fold = false;
	inline int cols = 3;
	inline float scale = 0.6f;
	inline float posX = 40.0f, posY = 60.0f;
	inline float papyrusX = -1.0f, papyrusY = -1.0f, papyrusScale = -1.0f;
	inline std::atomic<std::uint32_t> moveKey{ 0 };
	inline bool showBars = true, showDist = true, showEffects = true, combatOnly = false;
	inline std::atomic<bool> anyActive{ false };
	inline bool placed = false;
	inline bool dragging = false;
	inline float grabX = 0.0f, grabY = 0.0f;
	inline std::atomic<long long> lastToggleMs{ 0 };
	inline long long NowMs()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}
	inline SKSEMenuFramework::Model::WindowInterface* moveWindow = nullptr;
	inline std::vector<std::string> faceKeys;
	inline std::unordered_map<std::uint32_t, std::string> portraitCache;

	inline std::string Norm(std::string_view a_s)
	{
		std::string out;
		for (const char c : a_s) {
			if (c >= 'A' && c <= 'Z') {
				out += static_cast<char>(c - 'A' + 'a');
			} else if (c >= 'a' && c <= 'z') {
				out += c;
			}
		}
		return out;
	}

	inline bool Exists(const std::string& a_path)
	{
		return Utf8Path::Exists(a_path);
	}

	inline bool Utf8Tail(char a_c)
	{
		return (static_cast<unsigned char>(a_c) & 0xC0) == 0x80;
	}

	inline std::size_t Utf8Count(const std::string& a_s)
	{
		std::size_t count = 0;
		for (const char c : a_s) {
			if (!Utf8Tail(c)) {
				++count;
			}
		}
		return count;
	}

	inline void Utf8Pop(std::string& a_s)
	{
		if (a_s.empty()) {
			return;
		}
		std::size_t i = a_s.size() - 1;
		for (int tail = 0; tail < 3 && i > 0 && Utf8Tail(a_s[i]); ++tail) {
			--i;
		}
		a_s.erase(i);
	}

	inline void UpperAscii(std::string& a_s)
	{
		for (auto& c : a_s) {
			if (c >= 'a' && c <= 'z') {
				c = static_cast<char>(c - 'a' + 'A');
			}
		}
	}

	inline void LoadFaceKeys()
	{
		std::ifstream f("Data/PrismaUI/views/Warband/faces/index.js");
		std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
		std::size_t p = 0;
		while ((p = text.find('"', p)) != std::string::npos) {
			const auto q = text.find('"', p + 1);
			if (q == std::string::npos) {
				break;
			}
			faceKeys.push_back(text.substr(p + 1, q - p - 1));
			p = q + 1;
		}
	}

	inline std::string Portrait(RE::Actor* a_actor, const std::string& a_name, const std::string& a_face)
	{
		auto* npc = a_actor->GetActorBase();
		if (npc && npc->IsDynamicForm() && npc->faceNPC) {
			npc = npc->faceNPC;
		}
		const std::uint32_t id = npc ? npc->GetFormID() : 0;
		if (auto it = portraitCache.find(id); it != portraitCache.end() && id) {
			return it->second;
		}
		std::vector<std::string> tries;
		const std::string views = "Data/PrismaUI/views/Warband/";
		const std::string sheet = "Data/Interface/PartySheet/Portraits/";
		std::string byPlugin, byId, byName;
		if (npc) {
			char buf[32];
			if (auto* file = npc->GetFile(0)) {
				std::snprintf(buf, sizeof(buf), "-0x%X.jpg", npc->GetLocalFormID());
				byPlugin = std::string(file->GetFilename()) + buf;
			}
			std::snprintf(buf, sizeof(buf), "%08X.jpg", npc->GetFormID());
			byId = buf;
			if (const char* n = npc->GetName(); n && *n) {
				byName = std::string(n) + ".jpg";
			}
		}
		for (const auto& s : { byPlugin, byName }) {
			if (s.size() > 4) {
				tries.push_back(views + "portraits/custom/" + s);
				tries.push_back(views + "portraits/custom/" + s.substr(0, s.size() - 4) + ".png");
			}
		}
		if (!byPlugin.empty()) {
			tries.push_back(views + "portraits/" + byPlugin);
		}
		if (!a_face.empty()) {
			tries.push_back(views + "faces/" + a_face + ".jpg");
		}
		const auto n = Norm(a_name);
		std::string best;
		for (const auto& k : faceKeys) {
			if (n == k) {
				best = k;
				break;
			}
			if (k.size() >= 4 && n.rfind(k, 0) == 0 && k.size() > best.size()) {
				best = k;
			}
		}
		if (!best.empty()) {
			tries.push_back(views + "faces/" + best + ".jpg");
		}
		for (const auto& s : { byPlugin, byId, byName }) {
			if (!s.empty()) {
				tries.push_back(sheet + s);
			}
		}
		std::string found = views + "faces/unknown.jpg";
		for (const auto& t : tries) {
			if (Exists(t)) {
				found = t;
				break;
			}
		}
		if (id) {
			portraitCache[id] = found;
		}
		return found;
	}

	inline float Frac(RE::Actor* a_actor, RE::ActorValue a_av)
	{
		auto* av = a_actor->AsActorValueOwner();
		const float max = av->GetPermanentActorValue(a_av) + a_actor->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, a_av);
		return max > 0.0f ? (std::clamp)(av->GetActorValue(a_av) / max, 0.0f, 1.0f) : 0.0f;
	}

	enum EffectBit : std::uint32_t
	{
		kFire = 1 << 0,
		kFrost = 1 << 1,
		kShock = 1 << 2,
		kPoison = 1 << 3,
		kDisease = 1 << 4,
		kParalysis = 1 << 5,
		kInvisible = 1 << 6,
		kBuff = 1 << 7,
	};

	inline std::uint32_t Effects(RE::Actor* a_actor)
	{
		std::uint32_t bits = 0;
		auto* target = a_actor->AsMagicTarget();
		auto* list = target ? target->GetActiveEffectList() : nullptr;
		if (!list) {
			return 0;
		}
		using Arch = RE::EffectArchetypes::ArchetypeID;
		for (auto* ae : *list) {
			if (!ae || ae->flags.any(RE::ActiveEffect::Flag::kInactive, RE::ActiveEffect::Flag::kDispelled)) {
				continue;
			}
			auto* mgef = ae->GetBaseObject();
			if (!mgef) {
				continue;
			}
			const auto type = ae->spell ? ae->spell->GetSpellType() : RE::MagicSystem::SpellType::kSpell;
			if (type == RE::MagicSystem::SpellType::kDisease) {
				bits |= kDisease;
				continue;
			}
			if (type == RE::MagicSystem::SpellType::kAbility || mgef->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kHideInUI)) {
				continue;
			}
			const auto arch = mgef->GetArchetype();
			if (arch == Arch::kParalysis) {
				bits |= kParalysis;
			} else if (arch == Arch::kInvisibility) {
				bits |= kInvisible;
			} else if (mgef->IsHostile() || mgef->IsDetrimental()) {
				switch (mgef->data.resistVariable) {
				case RE::ActorValue::kResistFire:
					bits |= kFire;
					break;
				case RE::ActorValue::kResistFrost:
					bits |= kFrost;
					break;
				case RE::ActorValue::kResistShock:
					bits |= kShock;
					break;
				case RE::ActorValue::kPoisonResist:
					bits |= kPoison;
					break;
				default:
					break;
				}
			} else if (arch == Arch::kValueModifier || arch == Arch::kPeakValueModifier || arch == Arch::kDualValueModifier) {
				bits |= kBuff;
			}
		}
		return bits;
	}

	inline bool OnBow(RE::Actor* a_actor)
	{
		const auto* state = a_actor->AsActorState();
		if (!state) {
			return false;
		}
		const auto attack = state->GetAttackState();
		return attack >= RE::ATTACK_STATE_ENUM::kBowDraw && attack <= RE::ATTACK_STATE_ENUM::kBowFollowThrough;
	}

	inline bool Casting(RE::Actor* a_actor)
	{
		const auto& casters = a_actor->GetActorRuntimeData().magicCasters;
		for (const auto hand : { RE::Actor::SlotTypes::kLeftHand, RE::Actor::SlotTypes::kRightHand }) {
			const auto* caster = casters[hand];
			if (!caster || !caster->currentSpell) {
				continue;
			}
			const auto state = caster->state.get();
			if (state == RE::MagicCaster::State::kCharging || state == RE::MagicCaster::State::kCasting) {
				return true;
			}
		}
		return false;
	}

	struct Inspect
	{
		RE::ActorHandle handle;
		std::string verdict;
		bool canJoin = false;
		long long until = 0;
		std::string name, sub, skills, portrait;
		int level = 0;
		float hp = 1.0f, mp = 1.0f, sp = 1.0f;
	};
	inline Inspect inspect;

	inline void ShowInspect(RE::Actor* a_actor, const std::string& a_verdict, bool a_canJoin)
	{
		std::lock_guard g(lock);
		if (!a_actor) {
			inspect.until = 0;
			return;
		}
		inspect.handle = a_actor->GetHandle();
		inspect.name.clear();
		inspect.sub.clear();
		inspect.skills.clear();
		inspect.portrait.clear();
		inspect.verdict = a_verdict;
		inspect.canJoin = a_canJoin;
		inspect.until = NowMs() + 7000;
	}

	inline constexpr std::string_view kCoverMenus[] = { RE::LoadingMenu::MENU_NAME, RE::FaderMenu::MENU_NAME, RE::MainMenu::MENU_NAME,
		RE::DialogueMenu::MENU_NAME, RE::Console::MENU_NAME, RE::RaceSexMenu::MENU_NAME, RE::MapMenu::MENU_NAME,
		RE::LockpickingMenu::MENU_NAME, RE::BookMenu::MENU_NAME, RE::GiftMenu::MENU_NAME, RE::CraftingMenu::MENU_NAME,
		RE::TrainingMenu::MENU_NAME, RE::SleepWaitMenu::MENU_NAME, RE::StatsMenu::MENU_NAME, RE::LevelUpMenu::MENU_NAME,
		RE::BarterMenu::MENU_NAME, RE::ContainerMenu::MENU_NAME, RE::InventoryMenu::MENU_NAME, RE::MagicMenu::MENU_NAME,
		RE::FavoritesMenu::MENU_NAME, RE::TweenMenu::MENU_NAME, RE::JournalMenu::MENU_NAME, RE::MessageBoxMenu::MENU_NAME,
		RE::TutorialMenu::MENU_NAME };
	inline std::atomic<bool> menusHide{ true };
	inline std::atomic<bool> consoleOpen{ false };

	inline void RefreshMenus()
	{
		bool hide = true;
		if (auto* ui = RE::UI::GetSingleton()) {
			hide = false;
			consoleOpen.store(ui->IsMenuOpen(RE::Console::MENU_NAME));
			for (const auto& name : kCoverMenus) {
				if (ui->IsMenuOpen(name)) {
					hide = true;
					break;
				}
			}
			if (!hide) {
				auto hud = ui->GetMenu<RE::HUDMenu>();
				hide = !hud || !hud->uiMovie || !hud->uiMovie->GetVisible();
			}
		}
		menusHide.store(hide);
	}

	inline void MenuChanged(const RE::BSFixedString& a_name, bool a_opening)
	{
		bool cover = false;
		for (const auto& name : kCoverMenus) {
			if (a_name == name) {
				cover = true;
				break;
			}
		}
		const bool hud = !cover && a_name == RE::HUDMenu::MENU_NAME;
		if (!cover && !hud) {
			return;
		}
		if (cover ? a_opening : !a_opening) {
			menusHide.store(true);
		}
		if (auto* tasks = SKSE::GetTaskInterface()) {
			tasks->AddTask([]() { RefreshMenus(); });
		}
	}

	inline void Snapshot()
	{
		RefreshMenus();
		std::vector<Card> next;
		auto* player = RE::PlayerCharacter::GetSingleton();
		bool active = player && player->IsInCombat();
		float yaw = 0.0f;
		if (auto* camera = RE::PlayerCamera::GetSingleton(); camera && camera->cameraRoot) {
			const auto& r = camera->cameraRoot->world.rotate;
			yaw = std::atan2(r.entry[0][1], r.entry[1][1]);
		}
		{
			std::lock_guard g(lock);
			for (const auto& m : roster) {
				auto ptr = m.handle.get();
				auto* a = ptr.get();
				if (!a || a->IsDead() || a->IsDisabled()) {
					continue;
				}
				Card c;
				c.formID = a->GetFormID();
				c.name = m.name;
				c.guest = m.guest;
				c.portrait = Portrait(a, m.name, m.face);
				c.level = a->GetLevel();
				auto* av = a->AsActorValueOwner();
				const float max = av->GetPermanentActorValue(RE::ActorValue::kHealth) +
				                  a->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kHealth);
				c.hp = max > 0.0f ? (std::clamp)(av->GetActorValue(RE::ActorValue::kHealth) / max, 0.0f, 1.0f) : 0.0f;
				c.mp = Frac(a, RE::ActorValue::kMagicka);
				c.sp = Frac(a, RE::ActorValue::kStamina);
				c.effects = Effects(a);
				if (player) {
					const auto pp = player->GetPosition(), ap = a->GetPosition();
					c.dist = pp.GetDistance(ap);
					c.rel = std::atan2(ap.x - pp.x, ap.y - pp.y) - yaw;
				}
				if (a->IsInCombat() || c.hp < 0.9f) {
					active = true;
				}
				const bool mounted = a->IsOnMount();
				if (a->AsActorState()->IsBleedingOut()) {
					c.status = 2;
				} else if (!mounted && OnBow(a)) {
					c.status = 5;
				} else if (!mounted && Casting(a)) {
					c.status = 6;
				} else if (a->IsInCombat()) {
					c.status = 3;
				} else if (mounted) {
					c.status = 4;
				} else if (m.hold) {
					c.status = 1;
				}
				next.push_back(std::move(c));
			}
		}
		anyActive.store(active);
		RE::ActorHandle ih;
		{
			std::lock_guard g(lock);
			ih = inspect.until > NowMs() ? inspect.handle : RE::ActorHandle();
		}
		if (auto iptr = ih.get(); iptr && !iptr->IsDead()) {
			auto* a = iptr.get();
			Inspect info;
			info.name = a->GetDisplayFullName() ? a->GetDisplayFullName() : "";
			info.level = a->GetLevel();
			info.hp = Frac(a, RE::ActorValue::kHealth);
			info.mp = Frac(a, RE::ActorValue::kMagicka);
			info.sp = Frac(a, RE::ActorValue::kStamina);
			auto* npc = a->GetActorBase();
			const auto* race = a->GetRace();
			std::string sub = race ? race->GetName() : "";
			if (npc && npc->npcClass && npc->npcClass->GetName() && *npc->npcClass->GetName()) {
				sub += std::string("  \xC2\xB7  ") + npc->npcClass->GetName();
			}
			info.sub = sub;
			static constexpr std::pair<RE::ActorValue, const char*> skills[] = { { RE::ActorValue::kOneHanded, "One-Handed" },
				{ RE::ActorValue::kTwoHanded, "Two-Handed" }, { RE::ActorValue::kArchery, "Archery" }, { RE::ActorValue::kBlock, "Block" },
				{ RE::ActorValue::kHeavyArmor, "Heavy Armor" }, { RE::ActorValue::kLightArmor, "Light Armor" }, { RE::ActorValue::kSneak, "Sneak" },
				{ RE::ActorValue::kDestruction, "Destruction" }, { RE::ActorValue::kRestoration, "Restoration" },
				{ RE::ActorValue::kConjuration, "Conjuration" }, { RE::ActorValue::kAlteration, "Alteration" }, { RE::ActorValue::kIllusion, "Illusion" } };
			std::vector<std::pair<int, const char*>> best;
			for (const auto& [av, label] : skills) {
				best.emplace_back(static_cast<int>(a->AsActorValueOwner()->GetActorValue(av)), label);
			}
			std::sort(best.begin(), best.end(), [](const auto& x, const auto& y) { return x.first > y.first; });
			for (int i = 0; i < 3 && i < static_cast<int>(best.size()); ++i) {
				info.skills += (i ? "   " : "") + std::string(best[i].second) + " " + std::to_string(best[i].first);
			}
			info.portrait = Portrait(a, info.name, "");
			std::lock_guard g(lock);
			inspect.name = info.name;
			inspect.sub = info.sub;
			inspect.skills = info.skills;
			inspect.portrait = info.portrait;
			inspect.level = info.level;
			inspect.hp = info.hp;
			inspect.mp = info.mp;
			inspect.sp = info.sp;
		}
		std::lock_guard g(lock);
		cards = std::move(next);
	}

	inline bool HiddenByGame()
	{
		auto* ui = RE::UI::GetSingleton();
		if (!ui || !ui->IsShowingMenus() || ui->GameIsPaused()) {
			return true;
		}
		return menusHide.load();
	}

	inline void SendMoved()
	{
		char arg[96];
		std::snprintf(arg, sizeof(arg), "done|%d|%d|%.2f", static_cast<int>(std::lround(posX)), static_cast<int>(std::lround(posY)), scale);
		std::string s = arg;
		SKSE::GetTaskInterface()->AddTask([s]() {
			SKSE::ModCallbackEvent ev{ "Warband_HudMove", RE::BSFixedString(s.c_str()), 0.0f, nullptr };
			SKSE::GetModCallbackEventSource()->SendEvent(&ev);
		});
	}

	inline bool CursorInGame(const IG::ImVec2& a_display, IG::ImVec2& a_out)
	{
		const HWND hwnd = Win::Window();
		if (!hwnd || hwnd != GetForegroundWindow()) {
			return false;
		}
		POINT pt{};
		RECT rc{};
		if (!hwnd || !GetCursorPos(&pt) || !ScreenToClient(hwnd, &pt) || !GetClientRect(hwnd, &rc) || rc.right <= 0 || rc.bottom <= 0) {
			return false;
		}
		a_out = { pt.x * a_display.x / static_cast<float>(rc.right), pt.y * a_display.y / static_cast<float>(rc.bottom) };
		return true;
	}

	inline void RenderInspect()
	{
		Inspect c;
		{
			std::lock_guard g(lock);
			if (inspect.until <= NowMs() || inspect.name.empty()) {
				return;
			}
			c = inspect;
		}
		if (HiddenByGame()) {
			return;
		}
		auto* io = IG::GetIO();
		const float W = io->DisplaySize.x, H = io->DisplaySize.y, k = H / 1080.0f;
		const float cw = 380.0f * k, ch = 132.0f * k;
		const IG::ImVec2 p0{ (W - cw) / 2.0f, H * 0.14f }, p1{ p0.x + cw, p0.y + ch };
		IG::SetNextWindowPos(p0, IG::ImGuiCond_Always);
		IG::SetNextWindowSize({ cw, ch }, IG::ImGuiCond_Always);
		IG::Begin("##WarbandInspect", nullptr, IG::ImGuiWindowFlags_NoTitleBar | IG::ImGuiWindowFlags_NoResize | IG::ImGuiWindowFlags_NoInputs |
			IG::ImGuiWindowFlags_NoBackground | IG::ImGuiWindowFlags_NoSavedSettings | IG::ImGuiWindowFlags_NoFocusOnAppearing |
			IG::ImGuiWindowFlags_NoNav | IG::ImGuiWindowFlags_NoMove);
		auto* dl = IG::GetWindowDrawList();
		auto* font = IG::GetFont();
		IG::ImDrawListManager::AddRectFilled(dl, p0, p1, IM_COL32(0, 0, 0, 200), 0.0f, 0);
		IG::ImDrawListManager::AddLine(dl, p0, { p1.x, p0.y }, IM_COL32(200, 200, 200, 150), 1.0f);
		IG::ImDrawListManager::AddLine(dl, { p0.x, p1.y }, p1, IM_COL32(200, 200, 200, 150), 1.0f);
		const float r = 40.0f * k;
		const IG::ImVec2 ctr{ p0.x + 16.0f * k + r, p0.y + ch / 2.0f };
		IG::ImDrawListManager::AddCircleFilled(dl, ctr, r + 1.0f * k, IM_COL32(0, 0, 0, 220), 48);
		std::string tp = c.portrait;
		std::replace(tp.begin(), tp.end(), '/', '\\');
		if (auto tex = SKSEMenuFramework::LoadTexture(tp)) {
			IG::ImDrawListManager::AddImageRounded(dl, tex, { ctr.x - r, ctr.y - r }, { ctr.x + r, ctr.y + r }, { 0, 0 }, { 1, 1 },
				IM_COL32(255, 255, 255, 255), r, IG::ImDrawFlags_RoundCornersAll);
		}
		IG::ImDrawListManager::AddCircle(dl, ctr, r + 0.6f * k, IM_COL32(196, 196, 196, 255), 48, 1.4f * k);
		const float tx = ctr.x + r + 16.0f * k;
		const auto text = [&](float x, float y, float size, IG::ImU32 col, const std::string& t) {
			IG::ImDrawListManager::AddText(dl, font, size, { x + 1, y + 1 }, IM_COL32(0, 0, 0, 255), t.c_str());
			IG::ImDrawListManager::AddText(dl, font, size, { x, y }, col, t.c_str());
		};
		std::string nm = c.name;
		UpperAscii(nm);
		text(tx, p0.y + 12.0f * k, 24.0f * k, IM_COL32(255, 255, 255, 255), nm);
		text(tx, p0.y + 38.0f * k, 15.0f * k, IM_COL32(170, 170, 170, 255), "LEVEL " + std::to_string(c.level) + "   " + c.sub);
		const float mw = cw - (tx - p0.x) - 16.0f * k, mh = mw * 88.0f / 880.0f;
		const IG::ImVec2 mmin{ tx, p0.y + 60.0f * k }, mmax{ tx + mw, p0.y + 60.0f * k + mh };
		const float ix0 = mmin.x + mw * 0.105f, ix1 = mmax.x - mw * 0.105f, iy0 = mmin.y + mh * 0.24f, iy1 = mmax.y - mh * 0.24f;
		const float half = (ix1 - ix0) * c.hp / 2.0f, mid = (ix0 + ix1) / 2.0f;
		if (auto fill = SKSEMenuFramework::LoadTexture("Data\\PrismaUI\\views\\Warband\\skyrim\\hud_fill.png"); fill && c.hp > 0.0f) {
			IG::ImDrawListManager::AddImage(dl, fill, { mid - half, iy0 }, { mid + half, iy1 }, { 0, 0 }, { 1, 1 }, IM_COL32(255, 255, 255, 255));
		}
		if (auto frame = SKSEMenuFramework::LoadTexture("Data\\PrismaUI\\views\\Warband\\skyrim\\hud_meter.png")) {
			IG::ImDrawListManager::AddImage(dl, frame, mmin, mmax, { 0, 0 }, { 1, 1 }, IM_COL32(255, 255, 255, 255));
		}
		const float bw = (ix1 - ix0 - 4.0f * k) / 2.0f, by0 = mmax.y + 2.0f * k, by1 = by0 + 4.0f * k;
		if (auto tex = SKSEMenuFramework::LoadTexture("Data\\PrismaUI\\views\\Warband\\skyrim\\hud_fill_magicka.png"); tex && c.mp > 0.0f) {
			IG::ImDrawListManager::AddImage(dl, tex, { ix0, by0 }, { ix0 + bw * c.mp, by1 }, { 1.0f - c.mp, 0.0f }, { 1.0f, 1.0f }, IM_COL32(255, 255, 255, 255));
		}
		if (auto tex = SKSEMenuFramework::LoadTexture("Data\\PrismaUI\\views\\Warband\\skyrim\\hud_fill_stamina.png"); tex && c.sp > 0.0f) {
			IG::ImDrawListManager::AddImage(dl, tex, { ix0 + bw + 4.0f * k, by0 }, { ix0 + bw + 4.0f * k + bw * c.sp, by1 }, { 1.0f - c.sp, 0.0f },
				{ 1.0f, 1.0f }, IM_COL32(255, 255, 255, 255));
		}
		text(tx, by1 + 6.0f * k, 14.0f * k, IM_COL32(200, 200, 200, 255), c.skills);
		text(tx, by1 + 26.0f * k, 15.0f * k, c.canJoin ? IM_COL32(236, 236, 236, 255) : IM_COL32(214, 140, 128, 255), c.verdict);
		IG::End();
	}

	inline void PollMoveKey();

	inline void __stdcall Render()
	{
		PollMoveKey();
		std::vector<Card> draw;
		bool vis, folded;
		int columns;
		float sc;
		{
			std::lock_guard g(lock);
			draw = cards;
			vis = show;
			folded = fold;
			columns = cols;
			sc = scale;
		}
		auto* mainWin = SKSEMenuFramework::GetMainWindow();
		auto* io = IG::GetIO();
		if (!(io->DisplaySize.x > 0.0f) || !(io->DisplaySize.y > 0.0f)) {
			return;
		}
		const bool psEdit = PartySheet::EditingHUD();
		const bool placing = psEdit || (moveWindow && moveWindow->IsOpen.load());
		RenderInspect();
		if (!vis || draw.empty() || ((HiddenByGame() || PartySheet::HUDHidden() || (combatOnly && !anyActive.load())) && !placing) ||
			(mainWin && mainWin->IsOpen.load())) {
			if (dragging) {
				dragging = false;
				SendMoved();
			}
			return;
		}
		const float W = io->DisplaySize.x, H = io->DisplaySize.y;
		const float k = (sc / 0.6f) * (H / 1080.0f);
		const bool cursor = placing;
		if (folded && !cursor) {
			columns = 1;
		}
		const bool compactCards = folded && !cursor;
		const float cardW = (compactCards ? 56.0f : 78.0f) * k;
		const float cardH = (compactCards ? 56.0f : (showBars ? 95.0f : 88.0f)) * k;
		const float radius = 24.0f * k;
		const int n = static_cast<int>(draw.size());
		const auto grid = FitGrid(n, columns, H, cardH, 12.0f * k);
		const int across = grid.across;
		const int rows = grid.rows;

		const IG::ImVec2 winSize{ across * cardW + 12.0f * k, rows * cardH + 12.0f * k };
		const float maxX = W > winSize.x ? (W - winSize.x) / W * 1280.0f : 0.0f;
		const float maxY = H > winSize.y ? (H - winSize.y) / H * 720.0f : 0.0f;
		const auto shown = [&]() {
			return IG::ImVec2{ (std::clamp)(posX, 0.0f, maxX) / 1280.0f * W, (std::clamp)(posY, 0.0f, maxY) / 720.0f * H };
		};
		const IG::ImVec2 winPos = shown();
		bool changed = false;
		if (cursor) {
			IG::ImVec2 mp = io->MousePos;
			bool down = io->MouseDown[0];
			float wheel = io->MouseWheel;
			if (psEdit) {
				IG::ImVec2 c;
				if (CursorInGame(io->DisplaySize, c)) {
					mp = c;
				}
				down = CursorInGame(io->DisplaySize, c) && (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
				wheel = 0.0f;
			}
			const bool inside = mp.x >= winPos.x && mp.x <= winPos.x + winSize.x && mp.y >= winPos.y && mp.y <= winPos.y + winSize.y;
			static auto lastLog = std::chrono::steady_clock::time_point{};
			if (const auto now = std::chrono::steady_clock::now(); now - lastLog > std::chrono::milliseconds(700)) {
				lastLog = now;
				logger::debug("band HUD move ({}): mouse {:.0f},{:.0f} down {} inside {} frame {:.0f},{:.0f} {:.0f}x{:.0f}",
					psEdit ? "Party Sheet HUD Settings" : "move key", mp.x, mp.y, down, inside, winPos.x, winPos.y, winSize.x, winSize.y);
			}
			if (down && !dragging && inside) {
				dragging = true;
				grabX = mp.x - winPos.x;
				grabY = mp.y - winPos.y;
				logger::info("band HUD: drag start at {:.0f},{:.0f}", mp.x, mp.y);
			}
			if (dragging && down) {
				posX = (std::clamp)((mp.x - grabX) / W * 1280.0f, 0.0f, (std::min)(maxX, 1240.0f));
				posY = (std::clamp)((mp.y - grabY) / H * 720.0f, 0.0f, (std::min)(maxY, 700.0f));
			}
			if (dragging && !down) {
				dragging = false;
				changed = true;
				logger::info("band HUD: dropped at {:.0f},{:.0f} (1280x720 units)", posX, posY);
			}
			if (inside && wheel != 0.0f) {
				std::lock_guard g(lock);
				scale = (std::clamp)(std::round((scale + (wheel > 0 ? 0.05f : -0.05f)) * 100.0f) / 100.0f, 0.3f, 1.5f);
				changed = true;
			}
		} else if (dragging) {
			dragging = false;
			changed = true;
		}
		IG::SetNextWindowPos(shown(), IG::ImGuiCond_Always);
		placed = true;
		IG::PushStyleVar(IG::ImGuiStyleVar_WindowPadding, IG::ImVec2{ 6.0f * k, 6.0f * k });
		int flags = IG::ImGuiWindowFlags_NoTitleBar | IG::ImGuiWindowFlags_NoResize | IG::ImGuiWindowFlags_NoScrollbar |
		            IG::ImGuiWindowFlags_NoCollapse | IG::ImGuiWindowFlags_AlwaysAutoResize | IG::ImGuiWindowFlags_NoSavedSettings |
		            IG::ImGuiWindowFlags_NoFocusOnAppearing | IG::ImGuiWindowFlags_NoBringToFrontOnFocus | IG::ImGuiWindowFlags_NoNav |
		            IG::ImGuiWindowFlags_NoMove | IG::ImGuiWindowFlags_NoInputs | IG::ImGuiWindowFlags_NoBackground;
		IG::Begin("##WarbandBandHUD", nullptr, flags);
		auto* dl = IG::GetWindowDrawList();
		const IG::ImVec2 origin = IG::GetCursorScreenPos();
		auto* font = IG::GetFont();
		for (int i = 0; i < n; ++i) {
			const auto& c = draw[i];
			const int gridCol = compactCards ? i / rows : i % across;
			const int gridRow = compactCards ? i % rows : i / across;
			const float x0 = origin.x + gridCol * cardW;
			const float y0 = origin.y + gridRow * cardH;
			const IG::ImVec2 ctr{ x0 + cardW / 2.0f, y0 + radius + 4.0f * k };
			const IG::ImVec2 pmin{ ctr.x - radius, ctr.y - radius }, pmax{ ctr.x + radius, ctr.y + radius };
			IG::ImDrawListManager::AddCircleFilled(dl, ctr, radius + 1.0f * k, IM_COL32(0, 0, 0, 200), 48);
			std::string texPath = c.portrait;
			std::replace(texPath.begin(), texPath.end(), '/', '\\');
			if (auto tex = SKSEMenuFramework::LoadTexture(texPath)) {
				const IG::ImU32 tint = c.status == 2 ? IM_COL32(150, 150, 150, 255) : IM_COL32(255, 255, 255, 255);
				IG::ImDrawListManager::AddImageRounded(dl, tex, pmin, pmax, { 0, 0 }, { 1, 1 }, tint, radius, IG::ImDrawFlags_RoundCornersAll);
			}
			const IG::ImU32 ring = c.status == 2 ? IM_COL32(205, 80, 66, 255) : c.status == 3 ? IM_COL32(232, 214, 170, 255) :
			                       c.status == 1 ? IM_COL32(150, 172, 205, 255) : c.status == 5 ? IM_COL32(142, 204, 122, 255) :
			                       c.status == 6 ? IM_COL32(192, 150, 236, 255) : IM_COL32(196, 196, 196, 255);
			IG::ImDrawListManager::AddCircle(dl, ctr, radius + 2.2f * k, IM_COL32(0, 0, 0, 160), 48, 2.0f * k);
			IG::ImDrawListManager::AddCircle(dl, ctr, radius + 0.6f * k, ring, 48, 1.4f * k);
			if (showDist && c.dist > 1400.0f && !compactCards) {
				const float sx = std::sin(c.rel), sy = -std::cos(c.rel), rr = radius + 3.5f * k, w = 4.0f * k;
				const IG::ImVec2 tip{ ctr.x + sx * (rr + 5.0f * k), ctr.y + sy * (rr + 5.0f * k) };
				const IG::ImVec2 b1{ ctr.x + sx * rr - sy * w, ctr.y + sy * rr + sx * w }, b2{ ctr.x + sx * rr + sy * w, ctr.y + sy * rr - sx * w };
				IG::ImDrawListManager::AddTriangleFilled(dl, tip, b1, b2, IM_COL32(240, 240, 240, 235));
				char dm[16];
				std::snprintf(dm, sizeof(dm), "%dm", static_cast<int>(c.dist / 70.0f));
				const float ds = 11.0f * k;
				const IG::ImVec2 dsz = IG::ImFontManger::CalcTextSizeA(font, ds, 1000.0f, 0.0f, dm, nullptr, nullptr);
				const IG::ImVec2 dpos{ ctr.x - radius * 0.72f - dsz.x / 2.0f, ctr.y + radius * 0.62f - dsz.y / 2.0f };
				IG::ImDrawListManager::AddText(dl, font, ds, { dpos.x + 1, dpos.y + 1 }, IM_COL32(0, 0, 0, 255), dm);
				IG::ImDrawListManager::AddText(dl, font, ds, dpos, IM_COL32(225, 225, 225, 255), dm);
			}
			if (showEffects && c.effects && !compactCards) {
				static constexpr std::pair<std::uint32_t, unsigned int> glyphs[] = { { kFire, 0xf06d }, { kFrost, 0xf2dc }, { kShock, 0xf0e7 },
					{ kPoison, 0xf714 }, { kDisease, 0xf780 }, { kParalysis, 0xf05e }, { kInvisible, 0xf070 }, { kBuff, 0xf3ed } };
				static constexpr IG::ImU32 tints[] = { IM_COL32(236, 128, 76, 255), IM_COL32(150, 200, 240, 255), IM_COL32(170, 200, 255, 255),
					IM_COL32(130, 200, 110, 255), IM_COL32(190, 170, 120, 255), IM_COL32(220, 220, 220, 255), IM_COL32(200, 200, 220, 255),
					IM_COL32(230, 220, 190, 255) };
				FontAwesome::PushSolid();
				auto* icons = IG::GetFont();
				const float is = 9.5f * k;
				float iy = ctr.y - radius;
				const float ix = ctr.x + radius + 2.5f * k;
				for (int g = 0; g < 8 && iy < ctr.y + radius; ++g) {
					if (!(c.effects & glyphs[g].first)) {
						continue;
					}
					const std::string u = FontAwesome::UnicodeToUtf8(glyphs[g].second);
					IG::ImDrawListManager::AddText(dl, icons, is, { ix + 1, iy + 1 }, IM_COL32(0, 0, 0, 230), u.c_str());
					IG::ImDrawListManager::AddText(dl, icons, is, { ix, iy }, tints[g], u.c_str());
					iy += is + 1.5f * k;
				}
				FontAwesome::Pop();
			}
			if (compactCards && c.guest) {
				const IG::ImVec2 dot{ x0 + 5.0f * k, y0 + 5.0f * k };
				IG::ImDrawListManager::AddCircleFilled(dl, dot, 4.0f * k, IM_COL32(0, 0, 0, 200), 16);
				IG::ImDrawListManager::AddCircleFilled(dl, dot, 3.0f * k, IM_COL32(214, 196, 150, 255), 16);
			}
			if (!compactCards) {
				char lv[8];
				std::snprintf(lv, sizeof(lv), "%d", c.level);
				const float fs = 12.0f * k;
				const IG::ImVec2 ts = IG::ImFontManger::CalcTextSizeA(font, fs, 1000.0f, 0.0f, lv, nullptr, nullptr);
				const IG::ImVec2 lpos{ ctr.x + radius * 0.72f - ts.x / 2.0f, ctr.y + radius * 0.62f - ts.y / 2.0f };
				IG::ImDrawListManager::AddText(dl, font, fs, { lpos.x + 1, lpos.y + 1 }, IM_COL32(0, 0, 0, 255), lv);
				IG::ImDrawListManager::AddText(dl, font, fs, lpos, IM_COL32(255, 255, 255, 255), lv);
				const float mw = cardW - 4.0f * k, mh = mw * 88.0f / 880.0f;
				const IG::ImVec2 mmin{ x0 + 2.0f * k, ctr.y + radius + 5.0f * k }, mmax{ mmin.x + mw, mmin.y + mh };
				const float ix0 = mmin.x + mw * 0.105f, ix1 = mmax.x - mw * 0.105f;
				const float iy0 = mmin.y + mh * 0.24f, iy1 = mmax.y - mh * 0.24f;
				const float hp = (std::clamp)(c.hp, 0.0f, 1.0f), half = (ix1 - ix0) * hp / 2.0f, mid = (ix0 + ix1) / 2.0f;
				if (auto fill = SKSEMenuFramework::LoadTexture("Data\\PrismaUI\\views\\Warband\\skyrim\\hud_fill.png"); fill && hp > 0.0f) {
					IG::ImDrawListManager::AddImage(dl, fill, { mid - half, iy0 }, { mid + half, iy1 }, { 0.0f, 0.0f }, { 1.0f, 1.0f },
						IM_COL32(255, 255, 255, 255));
				}
				if (auto frame = SKSEMenuFramework::LoadTexture("Data\\PrismaUI\\views\\Warband\\skyrim\\hud_meter.png")) {
					IG::ImDrawListManager::AddImage(dl, frame, mmin, mmax, { 0, 0 }, { 1, 1 }, IM_COL32(255, 255, 255, 255));
				}
				float nameTop = mmax.y + 1.0f * k;
				if (showBars) {
					const float bw = (ix1 - ix0 - 3.0f * k) / 2.0f, by0 = mmax.y + 1.5f * k, by1 = by0 + 3.5f * k;
					const IG::ImVec2 m0{ ix0, by0 }, s0{ ix0 + bw + 3.0f * k, by0 };
					IG::ImDrawListManager::AddRectFilled(dl, { m0.x - 1, by0 - 1 }, { m0.x + bw + 1, by1 + 1 }, IM_COL32(0, 0, 0, 170), 0.0f, 0);
					IG::ImDrawListManager::AddRectFilled(dl, { s0.x - 1, by0 - 1 }, { s0.x + bw + 1, by1 + 1 }, IM_COL32(0, 0, 0, 170), 0.0f, 0);
					if (auto tex = SKSEMenuFramework::LoadTexture("Data\\PrismaUI\\views\\Warband\\skyrim\\hud_fill_magicka.png"); tex && c.mp > 0.0f) {
						IG::ImDrawListManager::AddImage(dl, tex, m0, { m0.x + bw * c.mp, by1 }, { 1.0f - c.mp, 0.0f }, { 1.0f, 1.0f }, IM_COL32(255, 255, 255, 255));
					}
					if (auto tex = SKSEMenuFramework::LoadTexture("Data\\PrismaUI\\views\\Warband\\skyrim\\hud_fill_stamina.png"); tex && c.sp > 0.0f) {
						IG::ImDrawListManager::AddImage(dl, tex, s0, { s0.x + bw * c.sp, by1 }, { 1.0f - c.sp, 0.0f }, { 1.0f, 1.0f }, IM_COL32(255, 255, 255, 255));
					}
					nameTop = by1 + 1.5f * k;
				}
				const float ns = 12.0f * k;
				std::string nm = c.name;
				UpperAscii(nm);
				IG::ImVec2 nsz = IG::ImFontManger::CalcTextSizeA(font, ns, 1000.0f, 0.0f, nm.c_str(), nullptr, nullptr);
				std::size_t left = Utf8Count(nm);
				bool dots = true;
				while (nsz.x > cardW - 4.0f * k && left > 3) {
					Utf8Pop(nm);
					--left;
					nsz = IG::ImFontManger::CalcTextSizeA(font, ns, 1000.0f, 0.0f, (nm + "...").c_str(), nullptr, nullptr);
					dots = nsz.x <= cardW - 4.0f * k;
					if (dots) {
						nm += "...";
					}
				}
				if (!dots) {
					nsz = IG::ImFontManger::CalcTextSizeA(font, ns, 1000.0f, 0.0f, nm.c_str(), nullptr, nullptr);
				}
				const IG::ImVec2 npos{ x0 + (cardW - nsz.x) / 2.0f, nameTop };
				IG::ImDrawListManager::AddText(dl, font, ns, { npos.x + 1, npos.y + 1 }, IM_COL32(0, 0, 0, 255), nm.c_str());
				IG::ImDrawListManager::AddText(dl, font, ns, npos, IM_COL32(236, 236, 236, 255), nm.c_str());
				if (c.guest) {
					float gs = 7.0f * k;
					IG::ImVec2 gsz = IG::ImFontManger::CalcTextSizeA(font, gs, 1000.0f, 0.0f, "GUEST", nullptr, nullptr);
					const float room = 27.0f * k;
					if (gsz.x > room && gsz.x > 0.0f) {
						gs *= room / gsz.x;
					}
					const IG::ImVec2 gpos{ x0 + 2.0f * k, nameTop + ns + 0.3f * k };
					IG::ImDrawListManager::AddText(dl, font, gs, { gpos.x + 1, gpos.y + 1 }, IM_COL32(0, 0, 0, 255), "GUEST");
					IG::ImDrawListManager::AddText(dl, font, gs, gpos, IM_COL32(214, 196, 150, 255), "GUEST");
				}
			}
		}
		IG::Dummy({ across * cardW, rows * cardH });
		if (cursor) {
			IG::ImDrawListManager::AddRect(dl, winPos, { winPos.x + winSize.x, winPos.y + winSize.y }, IM_COL32(255, 255, 255, dragging ? 220 : 120),
				6.0f * k, IG::ImDrawFlags_RoundCornersAll, 2.0f);
		}
		IG::End();
		IG::PopStyleVar();
		if (changed) {
			SendMoved();
		}
	}

	inline void __stdcall RenderMoveWindow()
	{
		IG::SetNextWindowPos({ 0.0f, 0.0f }, IG::ImGuiCond_Always);
		IG::SetNextWindowSize({ 1.0f, 1.0f }, IG::ImGuiCond_Always);
		IG::Begin("##WarbandHudMove", nullptr,
			IG::ImGuiWindowFlags_NoTitleBar | IG::ImGuiWindowFlags_NoBackground | IG::ImGuiWindowFlags_NoSavedSettings |
				IG::ImGuiWindowFlags_NoMove | IG::ImGuiWindowFlags_NoResize | IG::ImGuiWindowFlags_NoScrollbar | IG::ImGuiWindowFlags_NoInputs);
		if (IG::IsKeyPressed(IG::ImGuiKey_X, false) || IG::IsKeyPressed(IG::ImGuiKey_Escape, false) ||
			IG::IsKeyPressed(IG::ImGuiKey_Enter, false) || IG::IsMouseClicked(1)) {
			if (NowMs() - lastToggleMs.load() < 400) {
				IG::End();
				return;
			}
			lastToggleMs.store(NowMs());
			moveWindow->IsOpen.store(false);
			logger::info("band HUD: move mode closed");
		}
		IG::End();
	}

	inline std::atomic<bool> screenOpen{ false };

	inline void SetScreenOpen(bool a_open)
	{
		screenOpen.store(a_open);
		if (a_open && moveWindow && moveWindow->IsOpen.load()) {
			moveWindow->IsOpen.store(false);
			logger::info("band HUD: move mode closed for the party screen");
		}
	}

	inline std::atomic<long long> movePressMs{ 0 };

	inline void SampleMoveKey(bool a_gameInFront)
	{
		static bool wasDown = true;
		const std::uint32_t key = moveKey;
		const bool watch = a_gameInFront && moveWindow && !screenOpen.load() && !PartySheet::api.load() && Win::VirtualKeyOf(key) != 0;
		if (!watch) {
			wasDown = true;
			return;
		}
		bool       tapped = false;
		const bool down = Win::ScanKeyState(key, tapped);
		if (!wasDown && (down || tapped)) {
			movePressMs.store(NowMs());
		}
		wasDown = down;
	}

	inline void PollMoveKey()
	{
		const auto pressed = movePressMs.exchange(0);
		if (pressed == 0 || NowMs() - pressed > 150 || !moveWindow) {
			return;
		}
		if (!moveWindow->IsOpen.load()) {
			auto* ui = RE::UI::GetSingleton();
			if (!ui || ui->GameIsPaused() || consoleOpen.load()) {
				return;
			}
			if (const auto* controls = RE::ControlMap::GetSingleton(); controls && controls->GetRuntimeData().textEntryCount > 0) {
				return;
			}
		}
		bool any;
		{
			std::lock_guard g(lock);
			any = show && !cards.empty();
		}
		if (NowMs() - lastToggleMs.load() < 400) {
			return;
		}
		if (any || moveWindow->IsOpen.load()) {
			lastToggleMs.store(NowMs());
			moveWindow->IsOpen.store(!moveWindow->IsOpen.load());
			logger::info("band HUD: move key, move mode {}", moveWindow->IsOpen.load() ? "on" : "off");
		} else {
			logger::info("band HUD: move key, but no cards are showing");
		}
	}

	inline bool Installed()
	{
		try {
			return SKSEMenuFramework::IsInstalled();
		} catch (...) {
			return false;
		}
	}

	inline bool Ready()
	{
		try {
			return SKSEMenuFramework::IsInstalled();
		} catch (...) {
			return false;
		}
	}

	inline void Start()
	{
		if (!Installed()) {
			logger::info("SKSE Menu Framework not found: the band HUD stays on iWant widgets");
			return;
		}
		LoadFaceKeys();
		SKSEMenuFramework::AddHudElement(Render);
		moveWindow = SKSEMenuFramework::AddWindow(RenderMoveWindow, true);
		std::thread([]() {
			for (;;) {
				std::this_thread::sleep_for(std::chrono::milliseconds(150));
				if (auto* tasks = SKSE::GetTaskInterface()) {
					tasks->AddTask([]() { Snapshot(); });
				}
			}
		}).detach();
		logger::info("band HUD on SKSE Menu Framework ({} faces)", faceKeys.size());
	}

	inline void Set(const std::vector<RE::Actor*>& a_actors, const std::vector<std::string>& a_extras, const std::string& a_settings)
	{
		std::vector<Member> next;
		for (std::size_t i = 0; i < a_actors.size() && i < a_extras.size(); ++i) {
			if (!a_actors[i] || a_extras[i].empty()) {
				continue;
			}
			Member m;
			m.handle = a_actors[i]->GetHandle();
			auto e = ParseExtras(a_extras[i]);
			m.name = std::move(e.name);
			m.hold = e.hold;
			m.guest = e.guest;
			m.face = std::move(e.face);
			next.push_back(std::move(m));
		}
		std::vector<std::string> f;
		std::size_t s = 0, p;
		while ((p = a_settings.find('|', s)) != std::string::npos) {
			f.push_back(a_settings.substr(s, p - s));
			s = p + 1;
		}
		f.push_back(a_settings.substr(s));
		std::lock_guard g(lock);
		roster = std::move(next);
		if (f.size() >= 7) {
			show = f[0] == "1";
			cols = (std::max)(1, std::atoi(f[1].c_str()));
			fold = f[2] == "1";
			const float x = static_cast<float>(std::atof(f[3].c_str())), y = static_cast<float>(std::atof(f[4].c_str()));
			const float sc = static_cast<float>(std::atof(f[5].c_str()));
			moveKey = static_cast<std::uint32_t>((std::max)(0, std::atoi(f[6].c_str())));
			if (f.size() >= 11) {
				combatOnly = f[7] == "1";
				showBars = f[8] == "1";
				showDist = f[9] == "1";
				showEffects = f[10] == "1";
			}
			if (x != papyrusX || y != papyrusY) {
				papyrusX = x;
				papyrusY = y;
				if (std::fabs(x - posX) > 1.0f || std::fabs(y - posY) > 1.0f || !placed) {
					posX = x;
					posY = y;
					placed = false;
				}
			}
			if (sc != papyrusScale) {
				papyrusScale = sc;
				if (sc >= 0.3f && sc <= 1.5f) {
					scale = sc;
				}
			}
		}
	}
}
