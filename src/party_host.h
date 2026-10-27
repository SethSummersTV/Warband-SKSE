#pragma once
#include "MagelightUI_API.h"
#include "PrismaUI_API.h"

namespace PartyHost
{
	using Listener = void (*)(const char*);

	struct NamedListener
	{
		const char* name;
		Listener    fn;
	};

	enum class Kind
	{
		kNone,
		kMagelight,
		kPrisma,
	};

	inline Kind              kind = Kind::kNone;
	inline std::atomic<bool> domReady{ false };
	inline void (*onHostClosed)() = nullptr;

	inline PRISMA_UI_API::IVPrismaUI1* prisma = nullptr;
	inline PrismaView                  prismaView = 0;

	inline const MAGELIGHT_API::MagelightApi4* magelight = nullptr;
	inline MAGELIGHT_API::ModId                mod = 0;
	inline MAGELIGHT_API::ViewId               magelightView = 0;
	inline bool                                wantFocus = false;

	inline constexpr std::uint32_t kMinMagelight = MAGELIGHT_API::PackVersion(1, 1, 0);

	class Worker
	{
	public:
		void Post(Listener a_fn, std::string a_arg)
		{
			{
				std::lock_guard g(lock);
				jobs.emplace_back(a_fn, std::move(a_arg));
				if (!started) {
					started = true;
					std::thread([this]() { Run(); }).detach();
				}
			}
			ready.notify_one();
		}

	private:
		void Run()
		{
			for (;;) {
				std::pair<Listener, std::string> job;
				{
					std::unique_lock g(lock);
					ready.wait(g, [this]() { return !jobs.empty(); });
					job = std::move(jobs.front());
					jobs.pop_front();
				}
				job.first(job.second.c_str());
			}
		}

		std::mutex                                   lock;
		std::condition_variable                      ready;
		std::deque<std::pair<Listener, std::string>> jobs;
		bool                                         started = false;
	};

	inline Worker worker;

	inline void OnMagelightListener(MAGELIGHT_API::ViewId, const char* a_arg, void* a_user)
	{
		worker.Post(static_cast<const NamedListener*>(a_user)->fn, a_arg ? a_arg : "");
	}

	inline void OnMagelightEvent(const MAGELIGHT_API::EventData* a_event, void*)
	{
		using MAGELIGHT_API::Event;
		if (!a_event) {
			return;
		}
		switch (a_event->type) {
		case Event::ViewDomReady:
		case Event::ViewReloaded:
			if (a_event->view == magelightView) {
				domReady = true;
			}
			break;
		case Event::ViewLoadFailed:
			logger::error("party screen: Magelight UI could not load the page: {}", a_event->detail ? a_event->detail : "");
			break;
		case Event::UIModeRefused:
		case Event::UIModeExited:
			if (wantFocus) {
				wantFocus = false;
				if (a_event->type == Event::UIModeRefused) {
					logger::warn("party screen: Magelight UI refused input focus: {}", a_event->detail ? a_event->detail : "");
				}
				if (onHostClosed) {
					onHostClosed();
				}
			}
			break;
		case Event::RenderDead:
			logger::error("party screen: Magelight UI stopped drawing for this game session");
			break;
		default:
			break;
		}
	}

	inline std::string PagePath()
	{
		wchar_t     exe[MAX_PATH * 4] = {};
		const DWORD n = GetModuleFileNameW(nullptr, exe, static_cast<DWORD>(std::size(exe)));
		if (n == 0 || n >= std::size(exe)) {
			return {};
		}
		const auto page = std::filesystem::path(std::wstring(exe, n)).parent_path() / L"Data" / L"PrismaUI" / L"views" / L"Warband" / L"index.html";
		try {
			return page.string();
		} catch (...) {
			return {};
		}
	}

	inline bool StartMagelight(std::span<const NamedListener> a_listeners)
	{
		magelight = MAGELIGHT_API::RequestApi4();
		if (!magelight) {
			return false;
		}
		if (magelight->hostVersionNumber < kMinMagelight || magelight->QueryCapability("uiModeCancel") != 1) {
			logger::info("Magelight UI {} is older than 1.1.0: the party screen uses Prisma UI", magelight->hostVersion ? magelight->hostVersion : "");
			magelight = nullptr;
			return false;
		}
		MAGELIGHT_API::ModDesc desc{};
		desc.size = sizeof(desc);
		desc.modId = "Warband";
		desc.displayName = "Warband";
		desc.modVersion = MAGELIGHT_API::PackVersion(2, 0, 0);
		desc.minHostVersion = kMinMagelight;
		desc.callbackThread = MAGELIGHT_API::CallbackThread::GameThread;
		desc.onEvent = OnMagelightEvent;
		if (const auto result = magelight->RegisterMod(&desc, &mod); result != MAGELIGHT_API::Result::Ok) {
			logger::warn("Magelight UI did not register Warband (result {}): the party screen uses Prisma UI", static_cast<int>(result));
			magelight = nullptr;
			mod = 0;
			return false;
		}
		const auto page = PagePath();
		MAGELIGHT_API::ViewDesc view{};
		view.size = sizeof(view);
		view.name = "party";
		view.htmlPath = page.c_str();
		view.anchor = MAGELIGHT_API::Anchor::TopLeft;
		view.fullscreen = true;
		view.clickThrough = false;
		view.startVisible = false;
		view.layer = MAGELIGHT_API::Layer::Panel;
		const auto result = page.empty() ? MAGELIGHT_API::Result::FileNotFound : magelight->CreateViewEx(mod, &view, &magelightView);
		if (result != MAGELIGHT_API::Result::Ok || !magelightView) {
			const char* why = magelight->GetLastErrorMessage(mod);
			logger::warn("Magelight UI did not create the party screen (result {}: {}): the party screen uses Prisma UI", static_cast<int>(result),
				why ? why : "");
			magelight->UnregisterMod(mod);
			magelight = nullptr;
			mod = 0;
			magelightView = 0;
			return false;
		}
		for (const auto& listener : a_listeners) {
			magelight->RegisterJSListenerEx(magelightView, listener.name, OnMagelightListener, const_cast<NamedListener*>(&listener));
		}
		magelight->SetEscapeCapture(magelightView, true);
		kind = Kind::kMagelight;
		logger::info("party screen host: Magelight UI {} ({})", magelight->hostVersion ? magelight->hostVersion : "", page);
		return true;
	}

	inline bool StartPrisma(std::span<const NamedListener> a_listeners)
	{
		prisma = static_cast<PRISMA_UI_API::IVPrismaUI1*>(PRISMA_UI_API::RequestPluginAPI(PRISMA_UI_API::InterfaceVersion::V1));
		if (!prisma) {
			return false;
		}
		prismaView = prisma->CreateView("Warband/index.html", [](PrismaView) { domReady = true; });
		for (const auto& listener : a_listeners) {
			prisma->RegisterJSListener(prismaView, listener.name, listener.fn);
		}
		prisma->Hide(prismaView);
		kind = Kind::kPrisma;
		logger::info("party screen host: Prisma UI");
		return true;
	}

	inline void Start(std::span<const NamedListener> a_listeners, void (*a_onHostClosed)())
	{
		onHostClosed = a_onHostClosed;
		if (StartPrisma(a_listeners) || StartMagelight(a_listeners)) {
			return;
		}
		logger::warn("neither Magelight UI nor Prisma UI found: Warband falls back to its list menus");
	}

	[[nodiscard]] inline bool Exists()
	{
		return kind != Kind::kNone;
	}

	[[nodiscard]] inline bool IsValid()
	{
		switch (kind) {
		case Kind::kMagelight:
			return magelight->IsViewValid(magelightView);
		case Kind::kPrisma:
			return prisma->IsValid(prismaView);
		default:
			return false;
		}
	}

	[[nodiscard]] inline bool ReadyFast()
	{
		switch (kind) {
		case Kind::kMagelight:
			return magelightView != 0;
		case Kind::kPrisma:
			return prismaView != 0 && domReady.load();
		default:
			return false;
		}
	}

	[[nodiscard]] inline bool Ready()
	{
		switch (kind) {
		case Kind::kMagelight:
			return IsValid();
		case Kind::kPrisma:
			return IsValid() && domReady.load();
		default:
			return false;
		}
	}

	inline void Call(const char* a_function, const char* a_argument)
	{
		switch (kind) {
		case Kind::kMagelight:
			magelight->InteropCall(magelightView, a_function, a_argument);
			break;
		case Kind::kPrisma:
			prisma->InteropCall(prismaView, a_function, a_argument);
			break;
		default:
			break;
		}
	}

	inline void Show()
	{
		switch (kind) {
		case Kind::kMagelight:
			magelight->ShowView(magelightView, true);
			break;
		case Kind::kPrisma:
			prisma->Show(prismaView);
			break;
		default:
			break;
		}
	}

	inline void Hide()
	{
		switch (kind) {
		case Kind::kMagelight:
			magelight->ShowView(magelightView, false);
			break;
		case Kind::kPrisma:
			prisma->Hide(prismaView);
			break;
		default:
			break;
		}
	}

	inline void Focus()
	{
		switch (kind) {
		case Kind::kMagelight:
			{
				wantFocus = true;
				const auto result = magelight->RequestUIMode(magelightView, MAGELIGHT_API::kUIModeFlagPause | MAGELIGHT_API::kUIModeFlagReclaim);
				if (result != MAGELIGHT_API::Result::Ok) {
					const char* why = magelight->GetLastErrorMessage(mod);
					logger::warn("party screen: Magelight UI did not give it input focus (result {}: {})", static_cast<int>(result), why ? why : "");
					wantFocus = false;
					if (onHostClosed) {
						onHostClosed();
					}
				}
			}
			break;
		case Kind::kPrisma:
			prisma->Focus(prismaView, true);
			break;
		default:
			break;
		}
	}

	inline void Unfocus()
	{
		switch (kind) {
		case Kind::kMagelight:
			if (wantFocus) {
				wantFocus = false;
				magelight->CancelUIModeRequest(mod);
				if (magelight->GetUIModeOwner() == mod) {
					magelight->ReleaseUIMode(mod);
				}
			}
			break;
		case Kind::kPrisma:
			if (prisma->HasFocus(prismaView)) {
				prisma->Unfocus(prismaView);
			}
			break;
		default:
			break;
		}
	}
}
