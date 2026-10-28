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
		{ 12204, 12332, "LookupReferenceByHandle: the actor a handle stands for (include/RE/B/BSPointerHandle.h:220)", AddressId::kNeeded },
		{ 14108, 14298, "RE::ScriptEventSourceHolder::GetSingleton", AddressId::kNeeded },
		{ 15800, 16038, "TESObjectREFR::InitInventoryIfRequired (include/RE/Offsets.h:544)", AddressId::kNeeded },
		{ 15802, 16040, "TESObjectREFR::MakeInventoryChanges (src/RE/T/TESObjectREFR.cpp:916)", AddressId::kNeeded },
		{ 15829, 16068, "InventoryChanges::InitScripts (src/RE/I/InventoryChanges.cpp:157)", AddressId::kNeeded },
		{ 15889, 16129, "InventoryChanges::InitLeveledItems (src/RE/I/InventoryChanges.cpp:143)", AddressId::kNeeded },
		{ 15890, 16130, "InventoryChanges::InitFromContainerExtra (src/RE/I/InventoryChanges.cpp:136)", AddressId::kNeeded },
		{ 15967, 16212, "the handle of an actor (include/RE/B/BSPointerHandle.h:213)", AddressId::kNeeded },
		{ 19354, 19781, "TESObjectREFR::GetDisplayFullName (include/RE/Offsets.h:540)", AddressId::kNeeded },
		{ 23632, 24084, "BGSRelationship::GetRelationship (include/RE/B/BGSRelationship.h:65)", AddressId::kNeeded },
		{ 24537, 25066, "RE::TESQuest::CreateRefHandleByAliasID", AddressId::kNeeded },
		{ 29090, 29924, "RE::TESConditionItem::IsTrue", AddressId::kNeeded },
		{ 34413, 35220, "DialogueItem constructor (include/RE/D/DialogueItem.h:66)", AddressId::kVoiceCheck },
		{ 36344, 37334, "Actor::GetLevel (include/RE/Offsets.h:14)", AddressId::kNeeded },
		{ 37524, 38469, "Actor::GetActorValueModifier (src/RE/A/Actor.cpp:271)", AddressId::kNeeded },
		{ 52050, 52933, "DebugNotification: a line of text at the top left of the screen (include/RE/Offsets.h:581)", AddressId::kNotice },
		{ 66841, 68088, "RE::BSScrapArrayAllocator::allocate", AddressId::kNeeded },
		{ 66859, 68115, "MemoryManager::Allocate (include/RE/M/MemoryManager.h:39)", AddressId::kNeeded },
		{ 66861, 68117, "MemoryManager::Deallocate (include/RE/M/MemoryManager.h:46)", AddressId::kNeeded },
		{ 66884, 68144, "RE::ScrapHeap::Allocate", AddressId::kNeeded },
		{ 66885, 68146, "RE::ScrapHeap::Deallocate", AddressId::kNeeded },
		{ 66976, 68233, "BSReadWriteLock::LockForRead (include/RE/Offsets.h:100)", AddressId::kNeeded },
		{ 66982, 68239, "BSReadWriteLock::UnlockForRead (include/RE/Offsets.h:102)", AddressId::kNeeded },
		{ 67819, 69161, "BSFixedString constructor: a text into the game's string pool (include/RE/B/BSFixedString.h:194)", AddressId::kNeeded },
		{ 67847, 69192, "BSStringPool: letting such a text go (include/RE/B/BSStringPool.h:29)", AddressId::kNeeded },
		{ 69636, 71014, "BSResourceNiBinaryStream constructor (include/RE/Offsets.h:108)", AddressId::kVoiceCheck },
		{ 69638, 71016, "BSResourceNiBinaryStream destructor (include/RE/Offsets.h:109)", AddressId::kVoiceCheck },
		{ 69640, 71018, "BSResourceNiBinaryStream::seek (include/RE/Offsets.h:110)", AddressId::kVoiceCheck },
		{ 69643, 71021, "BSResourceNiBinaryStream::set_endian_swap (include/RE/Offsets.h:111)", AddressId::kVoiceCheck },
		{ 97462, 104246, "BSScript::Object destructor (src/RE/O/Object.cpp:117)", AddressId::kNeeded },
		{ 97463, 104247, "BSScript::Object::GetHandle (src/RE/O/Object.cpp:20)", AddressId::kNeeded },
		{ 97468, 104252, "BSScript::Object::IncRef (src/RE/O/Object.cpp:49)", AddressId::kNeeded },
		{ 97469, 104253, "BSScript::Object::DecRef (src/RE/O/Object.cpp:56)", AddressId::kNeeded },
		{ 97746, 104484, "BSScript::Stack::GetStackFrameVariable: a native's arguments (src/RE/S/Stack.cpp:31)", AddressId::kNeeded },
		{ 97923, 104651, "NativeFunctionBase::Call: how the game calls a native (include/RE/Offsets.h:130)", AddressId::kNeeded },
		{ 514141, 400269, "the TESDataHandler pointer (include/RE/Offsets.h:504)", AddressId::kVoiceCheck },
		{ 514167, 400315, "the ProcessLists pointer (src/RE/P/ProcessLists.cpp:13)", AddressId::kVoiceCheck },
		{ 514178, 400327, "the UI pointer (include/RE/Offsets.h:557)", AddressId::kNeeded },
		{ 514315, 400475, "the SkyrimVM pointer (include/RE/Offsets.h:488)", AddressId::kNeeded },
		{ 514351, 400507, "the map of all forms (include/RE/T/TESForm.h:195)", AddressId::kNeeded },
		{ 514360, 400517, "the lock of the map of all forms (include/RE/T/TESForm.h:196)", AddressId::kNeeded },
		{ 514622, 400782, "the GameSettingCollection pointer (include/RE/Offsets.h:213)", AddressId::kNeeded },
		{ 514642, 400802, "the PlayerCamera pointer (include/RE/Offsets.h:434)", AddressId::kNeeded },
		{ 514705, 400863, "the ControlMap pointer (include/RE/Offsets.h:184)", AddressId::kNeeded },
		{ 514959, 401099, "OwnVoices::Point", AddressId::kNeeded },
		{ 517014, 403521, "the PlayerCharacter pointer (include/RE/Offsets.h:451)", AddressId::kNeeded },
	};
}
