#pragma once
namespace Game
{
	struct AddressId
	{
		std::uint64_t se;
		std::uint64_t ae;
		const char*   what;
		enum : std::uint8_t { kNeeded, kVoiceCheck, kNotice };
		std::uint8_t  optional;
	};

	inline constexpr AddressId kAddressIds[] = {
		{ 11045, 11141, "the game's memory manager (include/RE/M/MemoryManager.h:32)", AddressId::kNeeded },
		{ 66859, 68115, "MemoryManager::Allocate (include/RE/M/MemoryManager.h:39)", AddressId::kNeeded },
		{ 66861, 68117, "MemoryManager::Deallocate (include/RE/M/MemoryManager.h:46)", AddressId::kNeeded },
		{ 67819, 69161, "BSFixedString constructor: a text into the game's string pool (include/RE/B/BSFixedString.h:194)", AddressId::kNeeded },
		{ 67847, 69192, "BSStringPool: letting such a text go (include/RE/B/BSStringPool.h:29)", AddressId::kNeeded },
		{ 97462, 104246, "BSScript::Object destructor (src/RE/O/Object.cpp:117)", AddressId::kNeeded },
		{ 97469, 104253, "BSScript::Object::DecRef (src/RE/O/Object.cpp:56)", AddressId::kNeeded },
		{ 97746, 104484, "BSScript::Stack::GetStackFrameVariable: a native's arguments (src/RE/S/Stack.cpp:31)", AddressId::kNeeded },
		{ 97923, 104651, "NativeFunctionBase::Call: how the game calls a native (include/RE/Offsets.h:130)", AddressId::kNeeded },
		{ 514315, 400475, "the SkyrimVM pointer (include/RE/Offsets.h:488)", AddressId::kNeeded },
	};
}
