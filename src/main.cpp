#include "party_host.h"
#include "utf8_path.h"
#include "log_file.h"
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
	void SendToPapyrus(std::string a_action)
	{
		SKSE::GetTaskInterface()->AddTask([action = std::move(a_action)]() {
			SKSE::ModCallbackEvent ev{ "Warband_UIAction", RE::BSFixedString(action.c_str()), 0.0f, nullptr };
			SKSE::GetModCallbackEventSource()->SendEvent(&ev);
		});
	}

	void HideView()
	{
		if (!PartyHost::Exists()) {
			return;
		}
		PartyHost::Unfocus();
		if (PartyHost::IsValid()) {
			PartyHost::Call("wbBlank", "");
		}
		PartyHost::Hide();
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
			if (PartyHost::Exists()) {
				PartyHost::Call("wbPortraitData", answer.c_str());
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
			if (PartyHost::Exists() && PartyHost::IsValid() && BandHUD::screenOpen.load() && (!a_wheelOnly || wheelOpen.load())) {
				PartyHost::Call("wbPad", name.c_str());
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
			logger::info("gamepad: {}", Win::Pad().how);
			for (;;) {
				std::this_thread::sleep_for(std::chrono::milliseconds(30));
				const bool front = Win::InFront();
				BandHUD::SampleMoveKey(front);
				if (!BandHUD::screenOpen.load() || !front) {
					wasBack = wasPrev = wasNext = true;
					for (auto& was : padWas) {
						was = 0xFFFF;
					}
					continue;
				}
				const bool back = Win::KeyDown(VK_ESCAPE) || Win::KeyDown(VK_TAB);
				if (back && !wasBack) {
					ToPage("b");
				}
				wasBack = back;
				const bool prev = Win::KeyDown(VK_LEFT) || Win::KeyDown('A');
				const bool next = Win::KeyDown(VK_RIGHT) || Win::KeyDown('D');
				if (next && !wasNext) {
					ToPage("gnext");
				} else if (prev && !wasPrev) {
					ToPage("gprev");
				}
				wasPrev = prev;
				wasNext = next;
				for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i) {
					XINPUT_STATE state{};
					if (!Win::PadState(i, state)) {
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
			if (PartyHost::Exists() && PartyHost::IsValid()) {
				PartyHost::Call("wbPictureChanged", key.c_str());
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

	void OnHostClosed()
	{
		if (wheelOpen.exchange(false)) {
			wheelPick.store(-1);
			HideView();
		} else if (BandHUD::screenOpen.load()) {
			HideView();
			SendToPapyrus("close");
		}
	}

	const PartyHost::NamedListener kListeners[] = {
		{ "wbAction", OnAction },
		{ "wbClose", OnClose },
		{ "wbPortrait", OnPortrait },
		{ "wbWheelPick", OnWheelPick },
		{ "wbPickPicture", OnPickPicture },
		{ "wbClearPicture", OnClearPicture },
	};

	void CreateWarbandView()
	{
		PartyHost::Start(kListeners, OnHostClosed);
	}

	bool IsReady(RE::StaticFunctionTag*)
	{
		return PartyHost::ReadyFast();
	}

	void ShowLoading(RE::StaticFunctionTag*)
	{
		SKSE::GetTaskInterface()->AddTask([]() {
			if (PartyHost::Exists() && !BandHUD::screenOpen.load()) {
				PartyHost::Show();
				PartyHost::Call("wbLoading", "");
			}
		});
	}

	void Open(RE::StaticFunctionTag*, std::string a_json)
	{
		SKSE::GetTaskInterface()->AddTask([json = std::move(a_json)]() {
			if (!PartyHost::Exists()) {
				return;
			}
			BandHUD::SetScreenOpen(true);
			PartyHost::Show();
			PartyHost::Call("wbOpen", json.c_str());
			PartyHost::Focus();
		});
	}

	bool OpenWheel(RE::StaticFunctionTag*, std::string a_title, std::vector<std::string> a_labels)
	{
		if (!PartyHost::ReadyFast() || wheelOpen.load()) {
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
			if (!PartyHost::Exists()) {
				wheelOpen.store(false);
				wheelPick.store(-1);
				return;
			}
			BandHUD::SetScreenOpen(true);
			PartyHost::Show();
			PartyHost::Call("wbWheel", json.c_str());
			PartyHost::Focus();
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
				if (PartyHost::Exists()) {
					PartyHost::Call("wbWheelHide", "");
				}
				HideView();
			});
		}
	}

	void Update(RE::StaticFunctionTag*, std::string a_json)
	{
		SKSE::GetTaskInterface()->AddTask([json = std::move(a_json)]() {
			if (PartyHost::Exists()) {
				PartyHost::Call("wbUpdate", json.c_str());
			}
		});
	}

	void Close(RE::StaticFunctionTag*)
	{
		SKSE::GetTaskInterface()->AddTask([]() { HideView(); });
	}

	std::vector<float> AimRay(RE::StaticFunctionTag*)
	{
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
			if (!PartyHost::Exists()) {
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
				PartyHost::Call("wbUpdate", json.c_str());
				return;
			}
			BandHUD::SetScreenOpen(true);
			PartyHost::Show();
			PartyHost::Call("wbOpen", json.c_str());
			PartyHost::Focus();
		});
	}

	bool HudReady(RE::StaticFunctionTag*)
	{
		return BandHUD::Ready();
	}

	void HudSet(RE::StaticFunctionTag*, std::vector<RE::Actor*> a_actors, std::vector<std::string> a_extras, std::string a_settings)
	{
		BandHUD::Set(a_actors, a_extras, a_settings);
	}

	void ShowInspect(RE::StaticFunctionTag*, RE::Actor* a_actor, std::string a_verdict, bool a_canJoin)
	{
		BandHUD::ShowInspect(a_actor, a_verdict, a_canJoin);
	}

	void SetFightStyle(RE::StaticFunctionTag*, RE::Actor* a_actor, RE::TESCombatStyle* a_style)
	{
		FightStyle::Set(a_actor, a_style);
	}

	void FollowerLinesFn(RE::StaticFunctionTag*, RE::TESFaction* a_band, bool a_hide)
	{
		FollowerLines::Set(a_band, a_hide);
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
		a_vm->RegisterFunction("SetFightStyle", "WarbandUI", SetFightStyle);
		a_vm->RegisterFunction("FollowerLines", "WarbandUI", FollowerLinesFn);
		a_vm->RegisterFunction("ShowInspect", "WarbandUI", ShowInspect);
		a_vm->RegisterFunction("HudReady", "WarbandUI", HudReady);
		a_vm->RegisterFunction("HudSet", "WarbandUI", HudSet);
		a_vm->RegisterFunction("OpenParty", "WarbandUI", OpenParty);
		a_vm->RegisterFunction("AimRay", "WarbandUI", AimRay);
		a_vm->RegisterFunction("IsReady", "WarbandUI", IsReady, true);
		a_vm->RegisterFunction("Open", "WarbandUI", Open);
		a_vm->RegisterFunction("Update", "WarbandUI", Update);
		a_vm->RegisterFunction("Close", "WarbandUI", Close);
		a_vm->RegisterFunction("OpenWheel", "WarbandUI", OpenWheel, true);
		a_vm->RegisterFunction("WheelPick", "WarbandUI", WheelPick, true);
		a_vm->RegisterFunction("CloseWheel", "WarbandUI", CloseWheel, true);
		a_vm->RegisterFunction("ShowLoading", "WarbandUI", ShowLoading, true);
		a_vm->RegisterFunction("HasOwnFn", "WarbandUI", HasOwnFn);
		a_vm->RegisterFunction("OwnFn", "WarbandUI", OwnFn);
		a_vm->RegisterFunction("OwnPointsAt", "WarbandUI", OwnPointsAt);
		return true;
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
			OwnVoices::Start();
			if (auto* ui = RE::UI::GetSingleton()) {
				ui->AddEventSink<RE::MenuOpenCloseEvent>(&OwnVoices::sink);
			}
			OwnFollow::Start();
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
			ForgetPortraits();
			break;
		case SKSE::MessagingInterface::kPreLoadGame:
		case SKSE::MessagingInterface::kNewGame:
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

	bool Start(const SKSE::LoadInterface* a_skse)
	{
		SKSE::Init(a_skse, false);
		logger::info("WarbandUI {} on Skyrim {}", SKSE::PluginDeclaration::GetSingleton()->GetVersion().string("."), REL::Module::get().version().string("."));
		const auto* messaging = SKSE::GetMessagingInterface();
		const auto* papyrus = SKSE::GetPapyrusInterface();
		if (!messaging || !papyrus || !SKSE::GetTaskInterface()) {
			logger::error("SKSE did not provide its messaging, Papyrus or task interface; WarbandUI does not start");
			return false;
		}
		messaging->RegisterListener("SKSE", MessageHandler);
		messaging->RegisterListener(nullptr, PartySheet::OnMessage);
		papyrus->Register(RegisterFuncs);
		return true;
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	if (Win::OpenLog(L"WarbandUI.log") == 2) {
		logger::warn("the SKSE log folder could not be written; this log is in the Windows temporary folder");
	}
	try {
		return Start(a_skse);
	} catch (const std::exception& e) {
		logger::error("WarbandUI does not start: {}", e.what());
	} catch (...) {
		logger::error("WarbandUI does not start: unknown error");
	}
	return false;
}
