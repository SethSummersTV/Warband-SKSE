namespace
{
	struct VersionData
	{
		std::uint32_t dataVersion;
		std::uint32_t pluginVersion;
		char          name[256];
		char          author[256];
		char          supportEmail[252];
		std::uint32_t versionIndependenceEx;
		std::uint32_t versionIndependence;
		std::uint32_t compatibleVersions[16];
		std::uint32_t seVersionRequired;
	};
	static_assert(sizeof(VersionData) == 0x350);
	static_assert(offsetof(VersionData, versionIndependenceEx) == 0x304);

	inline constexpr std::uint32_t kPluginVersion = 2u << 24;
}

extern "C" __declspec(dllexport) constinit VersionData SKSEPlugin_Version = {
	1,
	kPluginVersion,
	"WarbandUI",
	"Seth Summers",
	"",
	(1u << 0) | (1u << 1),
	1u << 0,
	{},
	0,
};

extern "C" __declspec(dllexport) bool SKSEPlugin_Query(const SKSE::QueryInterface*, SKSE::PluginInfo* a_info)
{
	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = "WarbandUI";
	a_info->version = kPluginVersion;
	return true;
}
