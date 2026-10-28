#pragma once
#include "AddressDb.h"

namespace Game::Bridge
{
	namespace detail
	{
		template <class Tag>
		struct Key
		{
			friend auto Reach(Key);
		};

		template <class Tag, auto Member>
		struct Open
		{
			friend auto Reach(Key<Tag>) { return Member; }
		};

		struct ModuleRuntime;
		struct DbInstance;
		struct DbLoaded;
		struct DbTable;

		template struct Open<ModuleRuntime, &REL::Module::_runtime>;
		template struct Open<DbInstance, &REL::IDDatabase::_instance>;
		template struct Open<DbLoaded, &REL::IDDatabase::_initialized>;
		template struct Open<DbTable, &REL::IDDatabase::_id2offset>;

		inline constexpr std::uint64_t kSkyrimVMse = 514315, kSkyrimVMae = 400475;
		inline constexpr std::size_t kMachineSlot = 0x200;

		inline void* standIn[kMachineSlot / sizeof(void*) + 1] = {};
		inline void* standInPointer = standIn;
	}

	inline bool Start(const RuntimeInfo& a_rt, const AddressDb& a_db, std::string& a_why)
	{
		using namespace detail;
		auto& module = REL::Module::get();
		if (a_rt.Ae17()) {
			module.*Reach(Key<ModuleRuntime>{}) = REL::Module::Runtime::AE;
		}
		const auto seen = module.version();
		if (seen[0] != a_rt.version.major || seen[1] != a_rt.version.minor || seen[2] != a_rt.version.build || seen[3] != a_rt.version.sub ||
			REL::Module::IsAE() != a_rt.UsesAeIds() || module.base() != reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr))) {
			a_why = "the game library and WarbandUI do not read the same game version from the exe";
			return false;
		}
		const auto vmId = a_rt.UsesAeIds() ? kSkyrimVMae : kSkyrimVMse;
		if (!a_db.Has(vmId)) {
			a_why = "the Address Library has no id " + std::to_string(vmId) + " (the script machine)";
			return false;
		}

		auto& library = *Reach(Key<DbInstance>{});
		auto& table = library.*Reach(Key<DbTable>{});
		using LibraryRow = typename std::remove_cvref_t<decltype(table)>::element_type;
		static std::vector<LibraryRow> rows;
		rows.clear();
		rows.reserve(a_db.rows.size());
		for (const auto& row : a_db.rows) {
			const auto offset = row.id == vmId ? reinterpret_cast<std::uintptr_t>(&standInPointer) - module.base() : row.offset;
			rows.push_back(LibraryRow{ row.id, offset });
		}
		table = std::span<LibraryRow>(rows);
		Reach(Key<DbLoaded>{})->store(true);
		return true;
	}

	inline void SetScriptMachine(RE::BSScript::IVirtualMachine* a_vm) noexcept
	{
		detail::standIn[detail::kMachineSlot / sizeof(void*)] = a_vm;
	}
}
