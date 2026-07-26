#pragma once

#ifndef TP_INTERNAL_COMPONENTS_GUARD
#error Include Components.h instead
#endif

/**
 * @brief Marks an actor whose equipment changed before it had a server id.
 *
 * `EquipManager` reports an equip as soon as the actor is flagged local, but the
 * `LocalComponent` that carries the server id only arrives once the server
 * answers the assignment request. Anything equipped in between used to be
 * dropped, which left the actor permanently unequipped on every other client -
 * the "naked NPC". Higher latency widens that window.
 *
 * Only the fact that something changed is recorded, not the individual changes:
 * `RequestEquipmentChanges` carries the actor's whole equipment, so replaying it
 * once after the assignment lands is enough, and stays correct however many
 * changes were missed.
 *
 * Mirrors `EarlyAnimationBufferComponent`, which solves the same ordering
 * problem for animations.
 */
struct EarlyEquipmentBufferComponent
{
};
