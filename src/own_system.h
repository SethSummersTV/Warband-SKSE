#pragma once

namespace OwnSystem
{
	inline RE::BSScript::IVirtualMachine* Machine()
	{
		return RE::BSScript::Internal::VirtualMachine::GetSingleton();
	}

	inline bool Has(const std::string& a_script, const std::string& a_fn)
	{
		auto* vm = Machine();
		if (!vm || a_script.empty() || a_fn.empty()) {
			return false;
		}
		RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> type;
		if (!vm->GetScriptObjectTypeNoLoad(RE::BSFixedString(a_script), type) || !type) {
			return false;
		}
		const RE::BSFixedString want(a_fn);
		int depth = 0;
		for (const auto* t = type.get(); t && depth < 16; t = t->GetParent(), ++depth) {
			const auto n = t->GetNumMemberFuncs();
			const auto* it = t->GetMemberFuncIter();
			if (!it || n > 4096) {
				return false;
			}
			for (std::uint32_t k = 0; k < n; ++k) {
				const auto& f = it[k].func;
				if (f && f->GetName() == want) {
					return f->GetParamCount() == 0 && !f->GetIsNative();
				}
			}
		}
		return false;
	}

	inline bool PointsAt(RE::TESQuest* a_quest, const std::string& a_script, RE::Actor* a_actor)
	{
		auto* vm = Machine();
		if (!vm || !a_quest || !a_actor || a_script.empty()) {
			return false;
		}
		auto* policy = vm->GetObjectHandlePolicy();
		if (!policy) {
			return false;
		}
		const auto handle = policy->GetHandleForObject(a_quest->GetFormType(), a_quest);
		if (handle == policy->EmptyHandle()) {
			return false;
		}
		RE::BSTSmartPointer<RE::BSScript::Object> obj;
		if (!vm->FindBoundObject(handle, a_script.c_str(), obj) || !obj) {
			return false;
		}
		const auto* player = RE::PlayerCharacter::GetSingleton();
		bool hit = false;
		int depth = 0;
		for (const auto* t = obj->GetTypeInfo(); t && depth < 16; t = t->GetParent(), ++depth) {
			const auto n = t->GetNumProperties();
			const auto* props = t->GetPropertyIter();
			if (n == 0) {
				continue;
			}
			if (!props || n > 4096) {
				return false;
			}
			for (std::uint32_t k = 0; k < n; ++k) {
				const auto* v = obj->GetProperty(props[k].name);
				if (!v || !v->IsObject()) {
					continue;
				}
#pragma push_macro("GetObject")
#undef GetObject
				const auto target = v->GetObject();
#pragma pop_macro("GetObject")
				if (!target) {
					continue;
				}
				const RE::Actor* who = nullptr;
				if (const auto* alias = static_cast<const RE::BGSRefAlias*>(target->Resolve(RE::BGSRefAlias::VMTYPEID))) {
					if (alias->owningQuest != a_quest) {
						continue;
					}
					who = alias->GetActorReference();
				} else if (const auto* ref = static_cast<const RE::TESObjectREFR*>(target->Resolve(static_cast<RE::VMTypeID>(RE::FormType::ActorCharacter)))) {
					who = ref->As<RE::Actor>();
				}
				if (!who || who == player) {
					continue;
				}
				if (who != a_actor) {
					logger::info("own system: {} on quest {:08X} also points at {:08X}, not only at {:08X}: not used", a_script, a_quest->GetFormID(), who->GetFormID(), a_actor->GetFormID());
					return false;
				}
				hit = true;
			}
		}
		if (!hit) {
			logger::info("own system: {} on quest {:08X} does not point at {:08X}: not used", a_script, a_quest->GetFormID(), a_actor->GetFormID());
		}
		return hit;
	}

	inline bool Call(RE::TESQuest* a_quest, const std::string& a_script, const std::string& a_fn)
	{
		auto* vm = Machine();
		if (!vm || !a_quest || !Has(a_script, a_fn)) {
			return false;
		}
		auto* policy = vm->GetObjectHandlePolicy();
		if (!policy) {
			return false;
		}
		const auto handle = policy->GetHandleForObject(a_quest->GetFormType(), a_quest);
		if (handle == policy->EmptyHandle()) {
			return false;
		}
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> none;
		auto* args = RE::MakeFunctionArguments();
		const bool queued = vm->DispatchMethodCall(handle, RE::BSFixedString(a_script), RE::BSFixedString(a_fn), args, none);
		logger::info("own system: {}.{}() on quest {:08X}: {}", a_script, a_fn, a_quest->GetFormID(), queued ? "queued" : "refused by the script machine");
		return queued;
	}
}
