#pragma once
namespace PartySheet
{
	struct IState1
	{
		virtual ~IState1() = default;
		virtual std::uint32_t GetVersion() = 0;
		virtual bool IsHUDHidden() = 0;
		virtual std::int32_t RequestHUDHidden(std::uint32_t, bool, std::uint32_t) = 0;
		virtual bool IsPanelOpen(std::uint32_t a_panel) = 0;
		virtual std::uint32_t GetOpenPanels() = 0;
	};

	enum Panel : std::uint32_t
	{
		kSheet = 0,
		kSheetV2 = 1,
		kCharacterSheet = 2,
		kInspect = 3,
		kHUDSettings = 4,
	};

	inline constexpr std::uint32_t kMessage = 0x50534831;
	inline std::atomic<IState1*> api{ nullptr };

	inline void OnMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		if (!a_msg || a_msg->type != kMessage || !a_msg->data || a_msg->dataLen != 8 || api.load()) {
			return;
		}
		auto* state = static_cast<IState1*>(a_msg->data);
		const auto version = state->GetVersion();
		if (version != 1) {
			logger::warn("Party Sheet API version {} is not one Warband knows: the band HUD keeps its own move key", version);
			return;
		}
		api.store(state);
		logger::info("Party Sheet API v1: the band HUD moves on Party Sheet's HUD Settings page");
	}

	inline bool HUDHidden()
	{
		auto* a = api.load();
		return a && a->IsHUDHidden();
	}

	inline bool EditingHUD()
	{
		auto* a = api.load();
		return a && a->IsPanelOpen(kHUDSettings);
	}
}
