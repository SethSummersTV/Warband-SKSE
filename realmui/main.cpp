#include "PrismaUI_API.h"
#include "game/AddressDb.h"
#include "game/AddressIds.h"
#include "game/LibraryBridge.h"
#include "game/Log.h"

namespace
{
	PRISMA_UI_API::IVPrismaUI1* prisma = nullptr;
	PrismaView view = 0;
	bool domReady = false;
	std::atomic<bool> screenOpen{ false };

	void SendToPapyrus(std::string a_action)
	{
		SKSE::GetTaskInterface()->AddTask([action = std::move(a_action)]() {
			SKSE::ModCallbackEvent ev{ "Realm_UIAction", RE::BSFixedString(action.c_str()), 0.0f, nullptr };
			SKSE::GetModCallbackEventSource()->SendEvent(&ev);
		});
	}

	void HideView()
	{
		screenOpen.store(false);
		if (!prisma || !view) {
			return;
		}
		if (prisma->HasFocus(view)) {
			prisma->Unfocus(view);
		}
		prisma->Hide(view);
	}

	void OnAction(const char* a_arg)
	{
		std::string arg = a_arg ? a_arg : "";
		if (arg.rfind("jserror|", 0) == 0) {
			logger::error("estates screen: {}", arg.substr(8));
			return;
		}
		SendToPapyrus(std::move(arg));
	}

	void OnClose(const char*)
	{
		SKSE::GetTaskInterface()->AddTask([]() { HideView(); });
		SendToPapyrus("close");
	}

	std::filesystem::path DataFolder()
	{
		wchar_t exe[MAX_PATH * 4] = {};
		const DWORD n = GetModuleFileNameW(nullptr, exe, static_cast<DWORD>(std::size(exe)));
		if (n == 0 || n >= std::size(exe)) {
			return {};
		}
		return std::filesystem::path(exe).parent_path() / L"Data";
	}

	bool PlainPluginName(const std::string& a_name)
	{
		if (a_name.size() < 5 || a_name.size() > 200) {
			return false;
		}
		for (const char c : a_name) {
			if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|' || static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) > 0x7E) {
				return false;
			}
		}
		std::string ext = a_name.substr(a_name.size() - 4);
		for (char& c : ext) {
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
		return ext == ".esp" || ext == ".esm" || ext == ".esl";
	}

	void OnPlugins(const char* a_arg)
	{
		std::string names = a_arg ? a_arg : "";
		std::string answer;
		const auto data = DataFolder();
		std::size_t at = 0;
		while (at <= names.size()) {
			std::size_t end = names.find('|', at);
			if (end == std::string::npos) {
				end = names.size();
			}
			const std::string one = names.substr(at, end - at);
			bool have = false;
			if (!data.empty() && PlainPluginName(one)) {
				try {
					std::error_code ec;
					have = std::filesystem::is_regular_file(data / std::wstring(one.begin(), one.end()), ec) && !ec;
				} catch (...) {
					have = false;
				}
			}
			if (at) {
				answer += '|';
			}
			answer += have ? '1' : '0';
			at = end + 1;
		}
		SKSE::GetTaskInterface()->AddTask([answer = std::move(answer)]() {
			if (prisma && view && prisma->IsValid(view)) {
				prisma->InteropCall(view, "realmInstalled", answer.c_str());
			}
		});
	}

	void ToPage(const char* a_key)
	{
		SKSE::GetTaskInterface()->AddTask([k = std::string(a_key)]() {
			if (prisma && view && prisma->IsValid(view) && screenOpen.load()) {
				prisma->InteropCall(view, "realmPad", k.c_str());
			}
		});
	}

	bool GameInFront()
	{
		const HWND front = GetForegroundWindow();
		if (!front) {
			return false;
		}
		DWORD pid = 0;
		GetWindowThreadProcessId(front, &pid);
		return pid == GetCurrentProcessId();
	}

	void StartKeyWatch()
	{
		std::thread([]() {
			struct PadKey { WORD mask; const char* name; };
			static constexpr PadKey pad[] = {
				{ XINPUT_GAMEPAD_DPAD_UP, "up" }, { XINPUT_GAMEPAD_DPAD_DOWN, "down" }, { XINPUT_GAMEPAD_DPAD_LEFT, "left" },
				{ XINPUT_GAMEPAD_DPAD_RIGHT, "right" }, { XINPUT_GAMEPAD_LEFT_SHOULDER, "lb" }, { XINPUT_GAMEPAD_RIGHT_SHOULDER, "rb" },
				{ XINPUT_GAMEPAD_A, "a" }, { XINPUT_GAMEPAD_B, "b" }, { XINPUT_GAMEPAD_X, "x" }
			};
			bool keyWasDown = true;
			WORD padWas[XUSER_MAX_COUNT] = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };
			for (;;) {
				std::this_thread::sleep_for(std::chrono::milliseconds(30));
				if (!screenOpen.load() || !GameInFront()) {
					keyWasDown = true;
					for (auto& w : padWas) {
						w = 0xFFFF;
					}
					continue;
				}
				const bool down = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0 || (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;
				if (down && !keyWasDown) {
					ToPage("b");
				}
				keyWasDown = down;
				for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i) {
					XINPUT_STATE state{};
					if (XInputGetState(i, &state) != ERROR_SUCCESS) {
						padWas[i] = 0xFFFF;
						continue;
					}
					const WORD now = state.Gamepad.wButtons;
					for (const auto& k : pad) {
						if ((now & k.mask) && !(padWas[i] & k.mask)) {
							ToPage(k.name);
						}
					}
					padWas[i] = now;
				}
			}
		}).detach();
	}

	void CreateRealmView()
	{
		prisma = static_cast<PRISMA_UI_API::IVPrismaUI1*>(PRISMA_UI_API::RequestPluginAPI(PRISMA_UI_API::InterfaceVersion::V1));
		if (!prisma) {
			logger::warn("Prisma UI not found: the estate mods keep their list menus");
			return;
		}
		view = prisma->CreateView("Realm/index.html", [](PrismaView) { domReady = true; });
		prisma->RegisterJSListener(view, "realmAction", OnAction);
		prisma->RegisterJSListener(view, "realmClose", OnClose);
		prisma->RegisterJSListener(view, "realmPlugins", OnPlugins);
		prisma->Hide(view);
		logger::info("estates screen view created");
	}

	bool IsReady(RE::StaticFunctionTag*)
	{
		return prisma && view && prisma->IsValid(view) && domReady;
	}

	bool IsOpen(RE::StaticFunctionTag*)
	{
		return screenOpen.load();
	}

	void Open(RE::StaticFunctionTag*, std::string a_data)
	{
		SKSE::GetTaskInterface()->AddTask([data = std::move(a_data)]() {
			if (!prisma || !view || !prisma->IsValid(view)) {
				return;
			}
			screenOpen.store(true);
			prisma->Show(view);
			prisma->InteropCall(view, "realmOpen", data.c_str());
			prisma->Focus(view, true);
		});
	}

	void Update(RE::StaticFunctionTag*, std::string a_data)
	{
		SKSE::GetTaskInterface()->AddTask([data = std::move(a_data)]() {
			if (prisma && view && prisma->IsValid(view)) {
				prisma->InteropCall(view, "realmUpdate", data.c_str());
			}
		});
	}

	void Close(RE::StaticFunctionTag*)
	{
		SKSE::GetTaskInterface()->AddTask([]() { HideView(); });
	}

	bool RegisterFuncs(RE::BSScript::IVirtualMachine* a_vm)
	{
		a_vm->RegisterFunction("IsReady", "RealmUI", IsReady);
		a_vm->RegisterFunction("IsOpen", "RealmUI", IsOpen);
		a_vm->RegisterFunction("Open", "RealmUI", Open);
		a_vm->RegisterFunction("Update", "RealmUI", Update);
		a_vm->RegisterFunction("Close", "RealmUI", Close);
		return true;
	}

	void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			CreateRealmView();
			StartKeyWatch();
			break;
		case SKSE::MessagingInterface::kPreLoadGame:
			SKSE::GetTaskInterface()->AddTask([]() { HideView(); });
			break;
		case SKSE::MessagingInterface::kNewGame:
			SKSE::GetTaskInterface()->AddTask([]() { HideView(); });
			[[fallthrough]];
		case SKSE::MessagingInterface::kPostLoadGame:
			if (a_msg->type == SKSE::MessagingInterface::kNewGame || a_msg->data != nullptr) {
				SKSE::GetTaskInterface()->AddTask([]() {
					SKSE::ModCallbackEvent ev{ "Realm_HostCall", RE::BSFixedString(""), 0.0f, nullptr };
					SKSE::GetModCallbackEventSource()->SendEvent(&ev);
					logger::info("save loaded: estates told to renew");
				});
			}
			break;
		default:
			break;
		}
	}
}

namespace
{
	constexpr const char* kWithout = "The estate mods keep their list menus";

	bool Preflight(const Game::RuntimeInfo& a_rt, const Game::AddressDb& a_db)
	{
		int missing = 0;
		for (const auto& id : Game::kAddressIds) {
			const auto want = a_rt.UsesAeIds() ? id.ae : id.se;
			if (want != 0 && a_db.Has(want)) {
				continue;
			}
			++missing;
			logger::error("Address Library id {} ({}) is not in this game's database", want, id.what);
		}
		if (missing == 0) {
			logger::info("Address Library format {}: {} ids, and all {} that RealmUI uses are among them", a_db.format, a_db.rows.size(),
				std::size(Game::kAddressIds));
		}
		return missing == 0;
	}

	bool Start(const SKSE::LoadInterface* a_skse)
	{
		const auto& rt = Game::Runtime();
		if (!rt.read) {
			logger::error("RealmUI 2.0.0: the game's version could not be read from its exe. The screen stays off. {}", kWithout);
			return false;
		}
		logger::info("RealmUI 2.0.0 on Skyrim {} ({})", rt.version.Text(), rt.LineName());
		if (rt.line == Game::Line::kUnknown) {
			logger::error("this game version is not one RealmUI runs on (1.5.97, 1.6.317 and later 1.6, 1.7). The screen stays off. {}", kWithout);
			return false;
		}
		if (rt.NewerThanChecked()) {
			logger::warn("this build of the game is newer than the newest one this dll was checked against ({}). It runs: RealmUI reads "
						 "no game structure of its own, only the script machine through the checked Address Library numbers",
				rt.Ae17() ? "1.7.104" : "1.6.1179");
		}
		Game::Version byName;
		if (GetEnvironmentVariableW(L"SKSE_RUNTIME", nullptr, 0) > 1) {
			logger::error("the environment variable SKSE_RUNTIME is set, which this build's game library cannot work with. The screen "
						  "stays off. {}",
				kWithout);
			return false;
		}
		if (GetModuleHandleW(L"SkyrimSE.exe") != GetModuleHandleW(nullptr) || GetModuleHandleW(L"SkyrimVR.exe") != nullptr ||
			Game::detail::ReadExeVersion(L"SkyrimSE.exe", byName) != 1 || byName != rt.version) {
			logger::error("the game's exe is not running under the name SkyrimSE.exe, or Windows cannot read its version by that name. "
						  "The screen stays off. {}",
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
				logger::error("Address Library for SKSE Plugins: Data\\SKSE\\Plugins\\{}: {}. The screen stays off (install or update "
							  "Address Library for SKSE Plugins). {}",
					fileName, why, kWithout);
				return false;
			}
		}
		logger::info("Address Library: {}", fileName);
		if (!Preflight(rt, db)) {
			logger::error("the screen stays off on this game version: the ids above are missing. {}", kWithout);
			return false;
		}
		if (!Game::Bridge::Start(rt, db, why)) {
			logger::error("{}. The screen stays off. {}", why, kWithout);
			return false;
		}
		db.Free();

		SKSE::Init(a_skse);
		const auto* messaging = SKSE::GetMessagingInterface();
		const auto* papyrus = SKSE::GetPapyrusInterface();
		if (!messaging || !papyrus || !SKSE::GetTaskInterface()) {
			logger::error("SKSE did not hand over its messaging, Papyrus or task interface. The screen stays off. {}", kWithout);
			return false;
		}
		messaging->RegisterListener("SKSE", MessageHandler);
		papyrus->Register(RegisterFuncs);
		logger::info("started: the Your Estates screen is on");
		return true;
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	if (const int where = Game::OpenLog(L"RealmUI.log"); where == 2) {
		logger::warn("the log could not be opened in SKSE's log folder (Documents\\My Games\\...\\SKSE): this is the fallback, "
					 "RealmUI.log in the Windows temporary folder");
	}
	try {
		Start(a_skse);
	} catch (const std::exception& e) {
		logger::error("RealmUI stays off: {}", e.what());
	} catch (...) {
		logger::error("RealmUI stays off: an unknown error at start-up");
	}
	return true;
}
