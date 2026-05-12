#pragma once

#include "NpcReplication.h"

#ifndef XRMP_WITH_OPENXRAY
#define XRMP_WITH_OPENXRAY 0
#endif

#if XRMP_WITH_OPENXRAY
class CAI_Stalker;

namespace xrmp::npc::openxray
{
using ExtractTransformFn = bool (*)(const CAI_Stalker&, rep::Vec3&, rep::Vec3&, rep::Vec3&);
using ApplyTransformFn = bool (*)(CAI_Stalker&, const rep::Vec3&, const rep::Vec3&, const rep::Vec3&);
using ExtractAnimationFn = bool (*)(const CAI_Stalker&, NpcAnimationState&);
using ApplyAnimationFn = bool (*)(CAI_Stalker&, const NpcAnimationState&);
using ExtractBehaviorFn = bool (*)(const CAI_Stalker&, NpcBehaviorState&, bool& alive);
using ApplyBehaviorFn = bool (*)(CAI_Stalker&, const NpcBehaviorState&, bool alive);

struct BindingHooks
{
    ExtractTransformFn extractTransform = nullptr;
    ApplyTransformFn applyTransform = nullptr;
    ExtractAnimationFn extractAnimation = nullptr;
    ApplyAnimationFn applyAnimation = nullptr;
    ExtractBehaviorFn extractBehavior = nullptr;
    ApplyBehaviorFn applyBehavior = nullptr;
};

// Extracts one generic replicated NPC state from CAI_Stalker through engine-specific hooks.
bool syncFromStalker(NpcRepComponent& component, const CAI_Stalker& stalker, const BindingHooks& hooks,
    std::string* error = nullptr);

// Applies one generic NPC state back to CAI_Stalker through engine-specific hooks.
bool applyToStalker(const NpcRepComponent& component, CAI_Stalker& stalker, const BindingHooks& hooks,
    std::string* error = nullptr);
} // namespace xrmp::npc::openxray
#endif
