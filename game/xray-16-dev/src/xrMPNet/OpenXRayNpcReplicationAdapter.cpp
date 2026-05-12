#include "OpenXRayNpcReplicationAdapter.h"

#if XRMP_WITH_OPENXRAY
#include "../xrGame/ai/stalker/ai_stalker.h"

namespace xrmp::npc::openxray
{
bool syncFromStalker(NpcRepComponent& component, const CAI_Stalker& stalker, const BindingHooks& hooks,
    std::string* error)
{
    if (!hooks.extractTransform || !hooks.extractAnimation || !hooks.extractBehavior)
    {
        if (error)
            *error = "missing CAI_Stalker extraction hooks";
        return false;
    }

    rep::Vec3 position{};
    rep::Vec3 rotation{};
    rep::Vec3 velocity{};
    NpcAnimationState animation{};
    NpcBehaviorState behavior{};
    bool alive = true;

    if (!hooks.extractTransform(stalker, position, rotation, velocity) || !hooks.extractAnimation(stalker, animation) ||
        !hooks.extractBehavior(stalker, behavior, alive))
    {
        if (error)
            *error = "CAI_Stalker extraction hook failed";
        return false;
    }

    component.setPosition(position);
    component.setRotation(rotation);
    component.setVelocity(velocity);
    component.setAnimation(animation);
    component.setBehavior(behavior);
    component.setAlive(alive);
    return true;
}

bool applyToStalker(const NpcRepComponent& component, CAI_Stalker& stalker, const BindingHooks& hooks,
    std::string* error)
{
    if (!hooks.applyTransform || !hooks.applyAnimation || !hooks.applyBehavior)
    {
        if (error)
            *error = "missing CAI_Stalker apply hooks";
        return false;
    }

    const NpcRepState& state = component.state();
    if (!hooks.applyTransform(stalker, state.position, state.rotation, state.velocity) ||
        !hooks.applyAnimation(stalker, state.animation) || !hooks.applyBehavior(stalker, state.behavior, state.alive))
    {
        if (error)
            *error = "CAI_Stalker apply hook failed";
        return false;
    }

    return true;
}
} // namespace xrmp::npc::openxray
#endif
