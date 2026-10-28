#include "PrismaUI_API.h"
#include "utf8_path.h"
#include "game/AddressIds.h"
#include "game/Canary.h"
#include "game/LibraryBridge.h"
#include "game/Log.h"
#include "party_data.h"
#include "band_hud.h"
#include "combat_style.h"
#include "follower_lines.h"
#include "own_voices.h"
#include "own_system.h"
#include "own_follow.h"
#include "portrait_pick.h"

namespace
{
	PRISMA_UI_API::IVPrismaUI1* prisma = nullptr;
	PrismaView view = 0;
	bool domReady = false;

	void SendToPapyrus(std::string a_action)
	{
		SKSE::GetTaskInterface()->AddTask([action = std::move(a_action)]() {
			SKSE::ModCallbackEvent ev{ "Warband_UIAction", RE::BSFixedString(action.c_str()), 0.0f, nullptr };
			SKSE::GetModCallbackEventSource()->SendEvent(&ev);
		});
	}

	void HideView()
	{
		if (!prisma || !view) {
			return;
		}
		if (prisma->HasFocus(view)) {
			prisma->Unfocus(view);
		}
		if (prisma->IsValid(view)) {
			prisma->InteropCall(view, "wbBlank", "");
		}
		prisma->Hide(view);
		BandHUD::SetScreenOpen(false);
	}

	void OnAction(const char* a_arg)
	{
		std::string arg = a_arg ? a_arg : "";
		if (arg.rfind("jserror|", 0) == 0) {
			logger::error("party screen: {}", arg.substr(8));
			return;
		}
		SendToPapyrus(std::move(arg));
	}

	std::string Base64(const std::string& a_in)
	{
		static constexpr char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		std::string out;
		out.reserve(((a_in.size() + 2) / 3) * 4);
		std::size_t i = 0;
		for (; i + 2 < a_in.size(); i += 3) {
			const auto n = (static_cast<std::uint8_t>(a_in[i]) << 16) | (static_cast<std::uint8_t>(a_in[i + 1]) << 8) | static_cast<std::uint8_t>(a_in[i + 2]);
			out += tbl[(n >> 18) & 63];
			out += tbl[(n >> 12) & 63];
			out += tbl[(n >> 6) & 63];
			out += tbl[n & 63];
		}
		if (i < a_in.size()) {
			auto n = static_cast<std::uint8_t>(a_in[i]) << 16;
			if (i + 1 < a_in.size()) {
				n |= static_cast<std::uint8_t>(a_in[i + 1]) << 8;
			}
			out += tbl[(n >> 18) & 63];
			out += tbl[(n >> 12) & 63];
			out += i + 1 < a_in.size() ? tbl[(n >> 6) & 63] : '=';
			out += '=';
		}
		return out;
	}

	void OnPortrait(const char* a_name)
	{
		std::string name = a_name ? a_name : "";
		std::string answer = name + "|";
		const bool safe = !name.empty() && name.size() < 200 && name.find("..") == std::string::npos &&
		                  name.find_first_of("/\\:") == std::string::npos;
		std::string path = name == "@player" ? "Data/Interface/PartySheet/Backgrounds/class-player.jpg" :
		                                       "Data/Interface/PartySheet/Portraits/" + name;
		bool custom = false;
		if (name.rfind("custom:", 0) == 0) {
			const std::string file = name.substr(7);
			custom = PortraitPick::SafeKey(file);
			path = std::string(PortraitPick::kFolder) + file;
			if (custom && !Utf8Path::Exists(path) && file.size() > 4) {
				path = std::string(PortraitPick::kFolder) + file.substr(0, file.size() - 4) + ".png";
			}
		}
		if (safe || custom || name == "@player") {
			std::ifstream f(Utf8Path::Of(path), std::ios::binary);
			if (f) {
				std::string bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
				if (!bytes.empty() && bytes.size() < 8 * 1024 * 1024) {
					const bool png = bytes.size() > 4 && bytes[1] == 'P' && bytes[2] == 'N' && bytes[3] == 'G';
					answer += std::string(png ? "data:image/png;base64," : "data:image/jpeg;base64,") + Base64(bytes);
				}
			}
		}
		SKSE::GetTaskInterface()->AddTask([answer = std::move(answer)]() {
			if (prisma && view) {
				prisma->InteropCall(view, "wbPortraitData", answer.c_str());
			}
		});
	}

	constexpr int kWheelWaiting = -99;
	inline std::atomic<bool> wheelOpen{ false };
	inline std::atomic<int> wheelPick{ -1 };
	inline std::atomic<int> wheelStick{ -1 };

	class MenuSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
		{
			if (a_event) {
				BandHUD::MenuChanged(a_event->menuName, a_event->opening);
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};
	MenuSink menuSink;

	void ToPage(std::string a_name, bool a_wheelOnly = false)
	{
		SKSE::GetTaskInterface()->AddTask([name = std::move(a_name), a_wheelOnly]() {
			if (prisma && view && prisma->IsValid(view) && BandHUD::screenOpen.load() && (!a_wheelOnly || wheelOpen.load())) {
				prisma->InteropCall(view, "wbPad", name.c_str());
			}
		});
	}

	void StartInputWatch()
	{
		std::thread([]() {
			struct PadKey
			{
				WORD        mask;
				const char* name;
			};
			static constexpr PadKey pad[] = {
				{ XINPUT_GAMEPAD_DPAD_UP, "up" }, { XINPUT_GAMEPAD_DPAD_DOWN, "down" }, { XINPUT_GAMEPAD_DPAD_LEFT, "left" },
				{ XINPUT_GAMEPAD_DPAD_RIGHT, "right" }, { XINPUT_GAMEPAD_LEFT_SHOULDER, "lb" }, { XINPUT_GAMEPAD_RIGHT_SHOULDER, "rb" },
				{ XINPUT_GAMEPAD_A, "a" }, { XINPUT_GAMEPAD_B, "b" }, { XINPUT_GAMEPAD_X, "x" }
			};
			bool wasBack = true, wasPrev = true, wasNext = true;
			WORD padWas[XUSER_MAX_COUNT] = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };
			logger::info("gamepad: {}", Game::Pad().how);
			for (;;) {
				std::this_thread::sleep_for(std::chrono::milliseconds(30));
				const bool front = Game::InFront();
				BandHUD::SampleMoveKey(front);
				if (!BandHUD::screenOpen.load() || !front) {
					wasBack = wasPrev = wasNext = true;
					for (auto& was : padWas) {
						was = 0xFFFF;
					}
					continue;
				}
				const bool back = Game::KeyDown(VK_ESCAPE) || Game::KeyDown(VK_TAB);
				if (back && !wasBack) {
					ToPage("b");
				}
				wasBack = back;
				const bool prev = Game::KeyDown(VK_LEFT) || Game::KeyDown('A');
				const bool next = Game::KeyDown(VK_RIGHT) || Game::KeyDown('D');
				if (next && !wasNext) {
					ToPage("gnext");
				} else if (prev && !wasPrev) {
					ToPage("gprev");
				}
				wasPrev = prev;
				wasNext = next;
				for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i) {
					XINPUT_STATE state{};
					if (!Game::PadState(i, state)) {
						padWas[i] = 0xFFFF;
						continue;
					}
					const WORD now = state.Gamepad.wButtons;
					for (const auto& key : pad) {
						if ((now & key.mask) && !(padWas[i] & key.mask)) {
							ToPage(key.name);
						}
					}
					padWas[i] = now;
					if (!wheelOpen.load()) {
						continue;
					}
					float x = state.Gamepad.sThumbLX / 32767.0f, y = state.Gamepad.sThumbLY / 32767.0f;
					if (x * x + y * y <= 0.42f) {
						x = state.Gamepad.sThumbRX / 32767.0f;
						y = state.Gamepad.sThumbRY / 32767.0f;
					}
					if (x * x + y * y > 0.42f) {
						static const float angles[8] = { 324.0f, 288.0f, 252.0f, 216.0f, 36.0f, 72.0f, 108.0f, 144.0f };
						float deg = std::atan2(x, y) * 57.29578f;
						if (deg < 0.0f) {
							deg += 360.0f;
						}
						int best = 0;
						float bd = 999.0f;
						for (int s = 0; s < 8; ++s) {
							float d = std::fabs(deg - angles[s]);
							d = d > 180.0f ? 360.0f - d : d;
							if (d < bd) {
								bd = d;
								best = s;
							}
						}
						if (wheelStick.exchange(best) != best) {
							ToPage("s" + std::to_string(best), true);
						}
					}
				}
			}
		}).detach();
	}

	void PictureChanged(const std::string& a_key)
	{
		SKSE::GetTaskInterface()->AddTask([key = a_key]() {
			BandHUD::portraitCache.clear();
			for (const char* ext : { ".jpg", ".png" }) {
				std::string tex = std::string(PortraitPick::kFolder) + key + ext;
				std::replace(tex.begin(), tex.end(), '/', '\\');
				SKSEMenuFramework::DisposeTexture(tex);
			}
			if (prisma && view && prisma->IsValid(view)) {
				prisma->InteropCall(view, "wbPictureChanged", key.c_str());
			}
		});
	}

	void OnPickPicture(const char* a_key)
	{
		PortraitPick::Pick(a_key ? a_key : "", [](bool a_saved, std::string a_k) {
			logger::info("picture for {}: {}", a_k, a_saved ? "saved" : "nothing chosen, or not a picture Windows can open");
			if (a_saved) {
				PictureChanged(a_k);
			}
		});
	}

	void OnClearPicture(const char* a_key)
	{
		const std::string key = a_key ? a_key : "";
		if (PortraitPick::Clear(key)) {
			PictureChanged(key);
		}
	}

	void OnWheelPick(const char* a_index)
	{
		int pick = -1;
		try {
			pick = a_index ? std::stoi(a_index) : -1;
		} catch (...) {
			pick = -1;
		}
		if (pick < -1 || pick > 7) {
			pick = -1;
		}
		wheelOpen.store(false);
		wheelPick.store(pick);
		SKSE::GetTaskInterface()->AddTask([]() { HideView(); });
	}

	void OnClose(const char*)
	{
		SKSE::GetTaskInterface()->AddTask([]() { HideView(); });
		SendToPapyrus("close");
	}

	void CreateWarbandView()
	{
		prisma = static_cast<PRISMA_UI_API::IVPrismaUI1*>(PRISMA_UI_API::RequestPluginAPI(PRISMA_UI_API::InterfaceVersion::V1));
		if (!prisma) {
			logger::warn("Prisma UI not found: Warband falls back to its list menus");
			return;
		}
		view = prisma->CreateView("Warband/index.html", [](PrismaView) { domReady = true; });
		prisma->RegisterJSListener(view, "wbAction", OnAction);
		prisma->RegisterJSListener(view, "wbClose", OnClose);
		prisma->RegisterJSListener(view, "wbPortrait", OnPortrait);
		prisma->RegisterJSListener(view, "wbWheelPick", OnWheelPick);
		prisma->RegisterJSListener(view, "wbPickPicture", OnPickPicture);
		prisma->RegisterJSListener(view, "wbClearPicture", OnClearPicture);
		prisma->Hide(view);
		logger::info("Warband party screen view created");
	}

	bool IsReady(RE::StaticFunctionTag*)
	{
		return prisma && view && prisma->IsValid(view) && domReady && Game::Canary::actors.load();
	}

	void Open(RE::StaticFunctionTag*, std::string a_json)
	{
		SKSE::GetTaskInterface()->AddTask([json = std::move(a_json)]() {
			if (!prisma || !view) {
				return;
			}
			BandHUD::SetScreenOpen(true);
			prisma->Show(view);
			prisma->InteropCall(view, "wbOpen", json.c_str());
			prisma->Focus(view, true);
		});
	}

	bool OpenWheel(RE::StaticFunctionTag*, std::string a_title, std::vector<std::string> a_labels)
	{
		if (!prisma || !view || !prisma->IsValid(view) || !domReady || wheelOpen.load()) {
			return false;
		}
		std::string json = "{\"title\":" + PartyData::Str(a_title) + ",\"items\":[";
		for (std::size_t i = 0; i < 8; ++i) {
			std::string label = i < a_labels.size() ? a_labels[i] : "";
			std::string full = label;
			if (const auto bar = label.find('|'); bar != std::string::npos) {
				full = label.substr(bar + 1);
				label = label.substr(0, bar);
			}
			json += std::string(i ? "," : "") + "{\"s\":" + PartyData::Str(label) + ",\"f\":" + PartyData::Str(full) + "}";
		}
		json += "]}";
		wheelPick.store(kWheelWaiting);
		wheelStick.store(-1);
		wheelOpen.store(true);
		SKSE::GetTaskInterface()->AddTask([json = std::move(json)]() {
			if (!prisma || !view) {
				wheelOpen.store(false);
				wheelPick.store(-1);
				return;
			}
			BandHUD::SetScreenOpen(true);
			prisma->Show(view);
			prisma->InteropCall(view, "wbWheel", json.c_str());
			prisma->Focus(view, true);
		});
		return true;
	}

	std::int32_t WheelPick(RE::StaticFunctionTag*)
	{
		return wheelPick.load();
	}

	void CloseWheel(RE::StaticFunctionTag*)
	{
		if (wheelOpen.exchange(false)) {
			wheelPick.store(-1);
			SKSE::GetTaskInterface()->AddTask([]() {
				if (prisma && view) {
					prisma->InteropCall(view, "wbWheelHide", "");
				}
				HideView();
			});
		}
	}

	void Update(RE::StaticFunctionTag*, std::string a_json)
	{
		SKSE::GetTaskInterface()->AddTask([json = std::move(a_json)]() {
			if (prisma && view) {
				prisma->InteropCall(view, "wbUpdate", json.c_str());
			}
		});
	}

	void Close(RE::StaticFunctionTag*)
	{
		SKSE::GetTaskInterface()->AddTask([]() { HideView(); });
	}

	std::vector<float> AimRay(RE::StaticFunctionTag*)
	{
		if (!Game::Canary::ran.load()) {
			SKSE::GetTaskInterface()->AddTask([]() { Game::Canary::EnsureRan(); });
			return {};
		}
		if (!Game::Canary::camera.load()) {
			return {};
		}
		auto* camera = RE::PlayerCamera::GetSingleton();
		if (!camera || !camera->cameraRoot) {
			return {};
		}
		const auto& world = camera->cameraRoot->world;
		const auto& r = world.rotate;
		return { world.translate.x, world.translate.y, world.translate.z, r.entry[0][1], r.entry[1][1], r.entry[2][1] };
	}

	void OpenParty(RE::StaticFunctionTag*, std::vector<RE::Actor*> a_actors, std::vector<std::string> a_extras,
		std::string a_band, std::int32_t a_sel, bool a_refresh)
	{
		std::vector<RE::ActorHandle> handles;
		for (auto* a : a_actors) {
			handles.push_back(a ? a->GetHandle() : RE::ActorHandle());
		}
		SKSE::GetTaskInterface()->AddTask([handles = std::move(handles), extras = std::move(a_extras), band = std::move(a_band), a_sel, a_refresh]() {
			Game::Canary::EnsureRan();
			if (!prisma || !view || !Game::Canary::actors.load()) {
				return;
			}
			std::vector<RE::Actor*> actors;
			std::vector<RE::NiPointer<RE::Actor>> keep;
			for (const auto& h : handles) {
				auto ptr = h.get();
				actors.push_back(ptr.get());
				keep.push_back(ptr);
			}
			const auto json = PartyData::Party(actors, extras, band, a_sel);
			if (a_refresh) {
				prisma->InteropCall(view, "wbUpdate", json.c_str());
				return;
			}
			BandHUD::SetScreenOpen(true);
			prisma->Show(view);
			prisma->InteropCall(view, "wbOpen", json.c_str());
			prisma->Focus(view, true);
		});
	}

	bool HudReady(RE::StaticFunctionTag*)
	{
		return BandHUD::Ready();
	}

	void HudSet(RE::StaticFunctionTag*, std::vector<RE::Actor*> a_actors, std::vector<std::string> a_extras, std::string a_settings)
	{
		if (BandHUD::LayoutsOk()) {
			BandHUD::Set(a_actors, a_extras, a_settings);
		}
	}

	void ShowInspect(RE::StaticFunctionTag*, RE::Actor* a_actor, std::string a_verdict, bool a_canJoin)
	{
		if (BandHUD::LayoutsOk()) {
			BandHUD::ShowInspect(a_actor, a_verdict, a_canJoin);
		}
	}

	void ShowLoadingFn(RE::StaticFunctionTag*) {}

	void SetFightStyle(RE::StaticFunctionTag*, RE::Actor* a_actor, RE::TESCombatStyle* a_style)
	{
		if (Game::Canary::actors.load() && Game::Canary::fightStyles.load()) {
			FightStyle::Set(a_actor, a_style);
		}
	}

	void FollowerLinesFn(RE::StaticFunctionTag*, RE::TESFaction* a_band, bool a_hide)
	{
		if (Game::Canary::followerLines.load()) {
			FollowerLines::Set(a_band, a_hide);
		}
	}

	bool HasOwnFn(RE::StaticFunctionTag*, std::string a_script, std::string a_fn)
	{
		return OwnSystem::Has(a_script, a_fn);
	}

	bool OwnFn(RE::StaticFunctionTag*, RE::TESQuest* a_quest, std::string a_script, std::string a_fn)
	{
		return OwnSystem::Call(a_quest, a_script, a_fn);
	}

	bool OwnPointsAt(RE::StaticFunctionTag*, RE::TESQuest* a_quest, std::string a_script, RE::Actor* a_actor)
	{
		return OwnSystem::PointsAt(a_quest, a_script, a_actor);
	}

	bool RegisterFuncs(RE::BSScript::IVirtualMachine* a_vm)
	{
		Game::Bridge::SetScriptMachine(a_vm);
		a_vm->RegisterFunction("SetFightStyle", "WarbandUI", SetFightStyle);
		a_vm->RegisterFunction("ShowLoading", "WarbandUI", ShowLoadingFn);
		a_vm->RegisterFunction("FollowerLines", "WarbandUI", FollowerLinesFn);
		a_vm->RegisterFunction("ShowInspect", "WarbandUI", ShowInspect);
		a_vm->RegisterFunction("HudReady", "WarbandUI", HudReady);
		a_vm->RegisterFunction("HudSet", "WarbandUI", HudSet);
		a_vm->RegisterFunction("OpenParty", "WarbandUI", OpenParty);
		a_vm->RegisterFunction("AimRay", "WarbandUI", AimRay);
		a_vm->RegisterFunction("IsReady", "WarbandUI", IsReady);
		a_vm->RegisterFunction("Open", "WarbandUI", Open);
		a_vm->RegisterFunction("Update", "WarbandUI", Update);
		a_vm->RegisterFunction("Close", "WarbandUI", Close);
		a_vm->RegisterFunction("OpenWheel", "WarbandUI", OpenWheel);
		a_vm->RegisterFunction("WheelPick", "WarbandUI", WheelPick);
		a_vm->RegisterFunction("CloseWheel", "WarbandUI", CloseWheel);
		a_vm->RegisterFunction("HasOwnFn", "WarbandUI", HasOwnFn);
		a_vm->RegisterFunction("OwnFn", "WarbandUI", OwnFn);
		a_vm->RegisterFunction("OwnPointsAt", "WarbandUI", OwnPointsAt);
		OwnSystem::vm = a_vm;
		return true;
	}

	bool voiceCheck = false;

	void LogVoicePaths()
	{
		static bool done = false;
		if (done || !voiceCheck || !Game::Canary::ran.load() || !Game::Canary::actors.load()) {
			return;
		}
		done = true;
		auto* dh = RE::TESDataHandler::GetSingleton();
		if (!dh) {
			return;
		}
		if (const auto* file = dh->LookupModByName("Warband.esp")) {
			logger::info("Warband.esp: light {} compileIndex {:X} smallFileCompileIndex {:X}", file->IsLight(),
				file->GetCompileIndex(), file->GetSmallFileCompileIndex());
		}
		RE::Actor* speaker = nullptr;
		if (auto* procs = RE::ProcessLists::GetSingleton()) {
			for (auto& h : procs->highActorHandles) {
				auto ptr = h.get();
				auto* a = ptr.get();
				auto* npc = a ? a->GetActorBase() : nullptr;
				if (npc && npc->GetFile(0) && std::string_view(npc->GetFile(0)->GetFilename()) == "Warband.esp" &&
					npc->GetLocalFormID() == 0x92A) {
					speaker = a;
					break;
				}
			}
		}
		if (!speaker) {
			logger::info("voice check: Gorbag is not loaded near the player");
			return;
		}
		auto* npc = speaker->GetActorBase();
		const char* vt = npc && npc->voiceType ? npc->voiceType->GetFormEditorID() : "";
		logger::info("voice check: speaker voice type '{}'", vt ? vt : "");
		int ok = 0, stock = 0, other = 0;
		std::vector<RE::TESTopicInfo*> infos;
		for (auto* t : dh->GetFormArray<RE::TESTopic>()) {
			if (!t || !t->topicInfos) {
				continue;
			}
			for (std::uint32_t i = 0; i < t->numTopicInfos; ++i) {
				if (auto* in = t->topicInfos[i]) {
					infos.push_back(in);
				}
			}
		}
		for (auto* info : infos) {
			if (!info || !info->GetFile(0) || std::string_view(info->GetFile(0)->GetFilename()) != "Warband.esp") {
				continue;
			}
			auto* topic = info->parentTopic;
			auto* quest = topic ? topic->ownerQuest : nullptr;
			if (!quest || !topic) {
				continue;
			}
			const std::string qe = quest->GetFormEditorID(), te = topic->GetFormEditorID();
			std::unique_ptr<RE::DialogueItem> item(new RE::DialogueItem(quest, topic, info, speaker));
			int n = 0;
			for (auto* resp : item->responses) {
				if (!resp) {
					continue;
				}
				++n;
				const std::string engine = resp->voice.c_str() ? resp->voice.c_str() : "";
				char ours[260];
				const bool cut = qe.size() + te.size() > 25;
				std::snprintf(ours, sizeof(ours), "Sound\\Voice\\Warband.esp\\%s\\%s_%s_%08X_%d.fuz", vt ? vt : "",
					(cut ? qe.substr(0, 10) : qe).c_str(), (cut ? te.substr(0, 15) : te).c_str(), info->GetLocalFormID(), n);
				RE::BSResourceNiBinaryStream probe(ours);
				const bool seen = probe.good();
				const bool isStock = engine.find("Fuz Ro Doh") != std::string::npos;
				const bool isOurs = engine.find("Warband.esp") != std::string::npos;
				(isStock ? stock : isOurs ? ok : other)++;
				if (!isOurs) {
					logger::info("voice check: {:06X} {}/{} #{} engine '{}' | our file '{}' {}", info->GetLocalFormID(), qe, te, n,
						engine, ours, seen ? "IS visible" : "NOT visible");
				}
			}
		}
		logger::info("voice check: {} responses use Warband's voice, {} fall back to Fuz Ro D'oh stock, {} other", ok, stock, other);
	}

	void ForgetPortraits()
	{
		SKSE::GetTaskInterface()->AddTask([]() { BandHUD::portraitCache.clear(); });
	}

	void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			CreateWarbandView();
			BandHUD::Start();
			if (auto* ui = RE::UI::GetSingleton(); ui && BandHUD::Ready()) {
				ui->AddEventSink<RE::MenuOpenCloseEvent>(&menuSink);
			}
			FightStyle::Start();
			StartInputWatch();
			OwnVoices::Start(OwnVoices::canProve);
			if (auto* ui = RE::UI::GetSingleton()) {
				ui->AddEventSink<RE::MenuOpenCloseEvent>(&OwnVoices::sink);
			}
			OwnFollow::Start();
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
			if (a_msg->data != nullptr) {
				SKSE::GetTaskInterface()->AddTask([]() { Game::Canary::Run(); });
			}
			SKSE::GetTaskInterface()->AddTask([]() { LogVoicePaths(); });
			ForgetPortraits();
			break;
		case SKSE::MessagingInterface::kPreLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			if (a_msg->type == SKSE::MessagingInterface::kNewGame) {
				SKSE::GetTaskInterface()->AddTask([]() { Game::Canary::Run(); });
			}
			FightStyle::Clear();
			HideView();
			if (a_msg->type == SKSE::MessagingInterface::kNewGame) {
				ForgetPortraits();
			}
			break;
		default:
			break;
		}
	}

	bool voiceIds = true;

	bool Preflight(const Game::RuntimeInfo& a_rt, const Game::AddressDb& a_db)
	{
		int missing = 0;
		for (const auto& id : Game::kAddressIds) {
			const auto want = a_rt.UsesAeIds() ? id.ae : id.se;
			if (want != 0 && a_db.Has(want)) {
				continue;
			}
			if (id.optional == Game::AddressId::kVoiceCheck) {
				voiceIds = false;
				logger::warn("Address Library id {} ({}) is not in this game's database: the voice-file check is left out", want, id.what);
			} else if (id.optional == Game::AddressId::kNotice) {
				Game::Canary::canNotify = false;
				logger::warn("Address Library id {} ({}) is not in this game's database: no on-screen notice, the log only", want, id.what);
			} else {
				++missing;
				logger::error("Address Library id {} ({}) is not in this game's database", want, id.what);
			}
		}
		if (missing == 0) {
			logger::info("Address Library format {}: {} ids, and all {} that WarbandUI uses are among them", a_db.format, a_db.rows.size(),
				std::size(Game::kAddressIds));
		}
		return missing == 0;
	}

	constexpr const char* kWithout = "Warband runs without it: list menus for the party screen, the UIExtensions wheel, the iWant HUD, "
	                                 "the camp in front of the player, no inspect card, members keep their own fighting style";

	bool Start(const SKSE::LoadInterface* a_skse)
	{
		const auto& rt = Game::Runtime();
		if (!rt.read) {
			logger::error("WarbandUI 2.0.0: the game's version could not be read from its exe. WarbandUI does not start. {}", kWithout);
			return false;
		}
		logger::info("WarbandUI 2.0.0 on Skyrim {} ({})", rt.version.Text(), rt.LineName());
		if (rt.line == Game::Line::kUnknown) {
			logger::error("this game version is not one WarbandUI runs on (1.5.97, 1.6.317 and later 1.6, 1.7). WarbandUI does not start. {}", kWithout);
			return false;
		}
		if (rt.NewerThanChecked()) {
			logger::warn("this build is newer than the newest one WarbandUI's layouts were checked on ({}). It runs; the layout checks "
						 "at the first loaded game decide what stays on",
				rt.Ae17() ? "1.7.104" : "1.6.1179");
		}
		Game::Version byName;
		if (GetEnvironmentVariableW(L"SKSE_RUNTIME", nullptr, 0) > 1) {
			logger::error("the environment variable SKSE_RUNTIME is set, which this build's game library cannot work with. "
						  "WarbandUI does not start. {}",
				kWithout);
			return false;
		}
		if (GetModuleHandleW(L"SkyrimSE.exe") != GetModuleHandleW(nullptr) || GetModuleHandleW(L"SkyrimVR.exe") != nullptr ||
			Game::detail::ReadExeVersion(L"SkyrimSE.exe", byName) != 1 || byName != rt.version) {
			logger::error("the game's exe is not running under the name SkyrimSE.exe, or Windows cannot read its version by that name. "
						  "WarbandUI does not start. {}",
				kWithout);
			return false;
		}
		Game::AddressDb db;
		std::string     why;
		auto            file = Game::AddressDb::FileFor(rt);
		const auto      slash = file.find_last_of(L'\\');
		const auto      bare = file.substr(slash == std::wstring::npos ? 0 : slash + 1);
		const auto      fileName = SKSE::stl::utf16_to_utf8(bare).value_or("the Address Library file");
		if (!db.Load(file, rt.version, why)) {
			std::string whyRelative;
			if (!db.Load(L"Data\\SKSE\\Plugins\\" + bare, rt.version, whyRelative)) {
				logger::error("Address Library for SKSE Plugins: Data\\SKSE\\Plugins\\{}: {}. WarbandUI does not start (install or update "
							  "Address Library for SKSE Plugins). {}",
					fileName, why, kWithout);
				return false;
			}
		}
		logger::info("Address Library: {}", fileName);
		if (!Preflight(rt, db)) {
			logger::error("WarbandUI does not start on this game version: the ids above are missing. {}", kWithout);
			return false;
		}
		if (!Game::Bridge::Start(rt, db, why)) {
			logger::error("{}. WarbandUI does not start. {}", why, kWithout);
			return false;
		}
		db.Free();

		SKSE::Init(a_skse);
		const auto* messaging = SKSE::GetMessagingInterface();
		const auto* papyrus = SKSE::GetPapyrusInterface();
		if (!messaging || !papyrus || !SKSE::GetTaskInterface()) {
			logger::error("SKSE did not hand over its messaging, Papyrus or task interface. WarbandUI does not start. {}", kWithout);
			return false;
		}
		messaging->RegisterListener("SKSE", MessageHandler);
		messaging->RegisterListener(nullptr, PartySheet::OnMessage);
		papyrus->Register(RegisterFuncs);
		const auto ini = rt.exeFolder + L"Data\\SKSE\\Plugins\\WarbandUI.ini";
		OwnVoices::canProve = voiceIds;
		voiceCheck = voiceIds && GetPrivateProfileIntW(L"Debug", L"VoicePaths", 0, ini.c_str()) == 1;
		logger::info("started: every feature is on (text entry count at {:#x}, player perks {}{})", Game::TextEntryCountOffset(),
			rt.Ae17() ? "at 0x4d8 (1.7)" : "from the library", voiceCheck ? ", voice-file check on by WarbandUI.ini" : "");
		return true;
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	if (const int where = Game::OpenLog(L"WarbandUI.log"); where == 2) {
		logger::warn("the log could not be opened in SKSE's log folder (Documents\\My Games\\...\\SKSE): this is the fallback, "
					 "WarbandUI.log in the Windows temporary folder");
	}
	try {
		return Start(a_skse);
	} catch (const std::exception& e) {
		logger::error("WarbandUI does not start: {}", e.what());
	} catch (...) {
		logger::error("WarbandUI does not start: an unknown error at start-up");
	}
	return false;
}
