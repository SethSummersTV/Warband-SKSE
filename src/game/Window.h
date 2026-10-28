#pragma once
namespace Game
{
	[[nodiscard]] inline HWND Window() noexcept
	{
		static std::atomic<HWND> known{ nullptr };
		HWND                     hwnd = known.load();
		if (hwnd && IsWindow(hwnd)) {
			return hwnd;
		}
		struct Best
		{
			HWND hwnd = nullptr;
			LONG area = 0;
		} best;
		EnumWindows(
			[](HWND a_hwnd, LPARAM a_best) -> BOOL {
				DWORD pid = 0;
				GetWindowThreadProcessId(a_hwnd, &pid);
				RECT rc{};
				if (pid == GetCurrentProcessId() && IsWindowVisible(a_hwnd) && GetClientRect(a_hwnd, &rc)) {
					auto* b = reinterpret_cast<Best*>(a_best);
					if (const LONG area = rc.right * rc.bottom; area > b->area) {
						b->area = area;
						b->hwnd = a_hwnd;
					}
				}
				return TRUE;
			},
			reinterpret_cast<LPARAM>(&best));
		known.store(best.hwnd);
		return best.hwnd;
	}

	[[nodiscard]] inline bool InFront() noexcept
	{
		const HWND hwnd = Window();
		return hwnd && hwnd == GetForegroundWindow();
	}

	[[nodiscard]] inline bool KeyDown(int a_virtualKey) noexcept
	{
		return (GetAsyncKeyState(a_virtualKey) & 0x8000) != 0;
	}

	struct PadRoute
	{
		using GetState_t = DWORD(WINAPI*)(DWORD, void*);

		GetState_t* slot = nullptr;
		GetState_t  direct = nullptr;
		std::string how = "no XInput found, the gamepad does not reach the party screen";
	};

	[[nodiscard]] inline PadRoute FindPadRoute() noexcept
	{
		PadRoute route;
		try {
			const auto  base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
			const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
			const auto* nt = base && dos->e_magic == IMAGE_DOS_SIGNATURE ? reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew) : nullptr;
			const auto  dir = nt && nt->Signature == IMAGE_NT_SIGNATURE ? nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT] : IMAGE_DATA_DIRECTORY{};
			if (dir.VirtualAddress != 0 && dir.Size >= sizeof(IMAGE_IMPORT_DESCRIPTOR)) {
				const auto* entry = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress);
				const auto* last = entry + dir.Size / sizeof(IMAGE_IMPORT_DESCRIPTOR);
				for (; entry < last && entry->Name != 0 && !route.slot; ++entry) {
					const char* dllName = reinterpret_cast<const char*>(base + entry->Name);
					if (_strnicmp(dllName, "XINPUT", 6) != 0 || entry->OriginalFirstThunk == 0 || entry->FirstThunk == 0) {
						continue;
					}
					const HMODULE dll = GetModuleHandleA(dllName);
					const auto    getState = dll ? GetProcAddress(dll, "XInputGetState") : nullptr;
					if (!getState) {
						continue;
					}
					const auto* names = reinterpret_cast<const IMAGE_THUNK_DATA64*>(base + entry->OriginalFirstThunk);
					auto*       slots = reinterpret_cast<PadRoute::GetState_t*>(base + entry->FirstThunk);
					for (std::size_t i = 0; names[i].u1.AddressOfData != 0; ++i) {
						const bool byOrdinal = IMAGE_SNAP_BY_ORDINAL64(names[i].u1.Ordinal);
						const auto ordinal = static_cast<WORD>(IMAGE_ORDINAL64(names[i].u1.Ordinal));
						const char* name = byOrdinal ? MAKEINTRESOURCEA(ordinal) :
						                               reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + names[i].u1.AddressOfData)->Name;
						const bool  extended = byOrdinal ? ordinal == 100 : std::strcmp(name, "XInputGetStateEx") == 0;
						if (extended || GetProcAddress(dll, name) == getState) {
							route.slot = &slots[i];
							route.how = std::string("read through the game's own import of ") + dllName + (extended ? " (extended state, ordinal 100)" : " XInputGetState") +
							            (reinterpret_cast<void*>(slots[i]) == reinterpret_cast<void*>(getState) || extended ? "" : ", which another program has redirected");
							if (!extended) {
								break;
							}
						}
					}
				}
			}
			if (!route.slot) {
				for (const char* name : { "XINPUT1_3.dll", "XINPUT1_4.dll", "XINPUT9_1_0.dll" }) {
					if (const HMODULE dll = LoadLibraryA(name)) {
						if (const auto fn = reinterpret_cast<PadRoute::GetState_t>(GetProcAddress(dll, "XInputGetState"))) {
							route.direct = fn;
							route.how = std::string("read from ") + name + " directly (the game exe has no XInput import of its own)";
							break;
						}
					}
				}
			}
		} catch (...) {}
		return route;
	}

	[[nodiscard]] inline const PadRoute& Pad() noexcept
	{
		static const PadRoute route = FindPadRoute();
		return route;
	}

	[[nodiscard]] inline bool PadState(DWORD a_index, XINPUT_STATE& a_state) noexcept
	{
		const auto& route = Pad();
		const auto  fn = route.slot ? *route.slot : route.direct;
		if (!fn) {
			return false;
		}
		struct
		{
			XINPUT_STATE state;
			DWORD        more[4];
		} buffer{};
		if (fn(a_index, &buffer) != ERROR_SUCCESS) {
			return false;
		}
		a_state = buffer.state;
		return true;
	}

	[[nodiscard]] inline int VirtualKeyOf(std::uint32_t a_scanCode) noexcept
	{
		if (a_scanCode == 0 || a_scanCode > 0xFF) {
			return 0;
		}
		if (a_scanCode == 0xC5) {
			return VK_PAUSE;
		}
		if (a_scanCode == 0x45) {
			return VK_NUMLOCK;
		}
		const UINT scan = a_scanCode < 0x80 ? a_scanCode : (0xE000u | (a_scanCode & 0x7Fu));
		return static_cast<int>(MapVirtualKeyW(scan, MAPVK_VSC_TO_VK_EX));
	}

	[[nodiscard]] inline int NumLockKeyOf(std::uint32_t a_scanCode) noexcept
	{
		switch (a_scanCode) {
		case 0x47: return VK_NUMPAD7;
		case 0x48: return VK_NUMPAD8;
		case 0x49: return VK_NUMPAD9;
		case 0x4B: return VK_NUMPAD4;
		case 0x4C: return VK_NUMPAD5;
		case 0x4D: return VK_NUMPAD6;
		case 0x4F: return VK_NUMPAD1;
		case 0x50: return VK_NUMPAD2;
		case 0x51: return VK_NUMPAD3;
		case 0x52: return VK_NUMPAD0;
		case 0x53: return VK_DECIMAL;
		default: return 0;
		}
	}

	[[nodiscard]] inline bool ScanKeyState(std::uint32_t a_scanCode, bool& a_tapped) noexcept
	{
		bool down = false;
		a_tapped = false;
		for (const int vk : { VirtualKeyOf(a_scanCode), NumLockKeyOf(a_scanCode) }) {
			if (vk != 0) {
				const SHORT state = GetAsyncKeyState(vk);
				down = down || (state & 0x8000) != 0;
				a_tapped = a_tapped || (state & 0x0001) != 0;
			}
		}
		return down;
	}
}
