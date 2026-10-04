/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "Combat.h"

#include "../GameState.h"
#include "../ride/Ride.h"
#include "../ride/RideManager.hpp"
#include "../world/Map.h"
#include "FactoryPrototypeObject.h"
#include "FactoryState.h"
#include "FactoryTopology.h"
#include "scripting/ScFactory.h"

#include <algorithm>

namespace OpenRCT2::Factory
{
    namespace
    {
        // A machine's footprint in world units: centre and half extent.
        struct Footprint
        {
            int32_t cx{};
            int32_t cy{};
            int32_t half{};
        };

        Footprint footprintOf(const MachineRecord& machine)
        {
            const int32_t size = footprintSize(getPrototype(machine.entry));
            return { machine.x * kCoordsXYStep + size * 16, machine.y * kCoordsXYStep + size * 16, size * 16 };
        }

        // Chebyshev distance from a point to a footprint's edge (0 inside).
        int32_t distanceTo(const Footprint& fp, int32_t x, int32_t y)
        {
            const int32_t dx = std::max(0, std::abs(x - fp.cx) - fp.half);
            const int32_t dy = std::max(0, std::abs(y - fp.cy) - fp.half);
            return std::max(dx, dy);
        }

        bool isDestructible(const MachineRecord& machine)
        {
            auto* proto = getPrototype(machine.entry);
            return proto != nullptr && proto->getMachine().health > 0 && !machine.isDestroyed();
        }

        // Nearest destructible machine, lowest id on ties.
        RecordId nearestMachine(const State& state, int32_t x, int32_t y)
        {
            RecordId best = kNullRecord;
            int32_t bestDistance = INT32_MAX;
            state.machines.forEach([&](RecordId id, const MachineRecord& machine) {
                if (!isDestructible(machine))
                    return;
                const int32_t distance = distanceTo(footprintOf(machine), x, y);
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best = id;
                }
            });
            return best;
        }

        constexpr int32_t kReach = 12; // world units from a footprint's edge a threat hits from

        DamageResult damageMachine(GameState_t& gameState, RecordId id, uint16_t amount)
        {
            auto* machine = gameState.factory.machines.get(id);
            if (machine == nullptr || !isDestructible(*machine))
                return {};
            machine->health = static_cast<uint16_t>(machine->health > amount ? machine->health - amount : 0);
            DamageResult result{ true, machine->health, false };
            if (machine->health == 0)
            {
                machine->craftCost = 0;
                machine->progress = 0;
                setMachineStatus(*machine, MachineStatus::destroyed);
                if (machine->powerNetwork != kNullRecord)
                    gameState.factory.powerDirty = true;
                result.destroyed = true;
            }
            return result;
        }

        DamageResult damageRide(GameState_t& gameState, uint32_t id, uint16_t amount)
        {
            auto* ride = GetRide(RideId::FromUnderlying(static_cast<uint16_t>(id)));
            if (ride == nullptr)
                return {};
            auto& ext = gameState.factory.parkExt;
            const uint32_t damage = ext.rideDamage(ride->id.ToUnderlying()) + amount;
            DamageResult result{ true, 0, false };
            if (damage >= kRideHealth)
            {
                ext.setRideDamage(ride->id.ToUnderlying(), 0);
                RidePrepareBreakdown(*ride, Breakdown::safetyCutOut);
                result.destroyed = true;
                result.health = 0;
            }
            else
            {
                ext.setRideDamage(ride->id.ToUnderlying(), static_cast<uint16_t>(damage));
                result.health = static_cast<uint16_t>(kRideHealth - damage);
            }
            return result;
        }

        DamageResult damageThreat(GameState_t& gameState, RecordId id, uint16_t amount)
        {
            auto* threat = gameState.factory.threats.get(id);
            if (threat == nullptr)
                return {};
            threat->health = static_cast<uint16_t>(threat->health > amount ? threat->health - amount : 0);
            DamageResult result{ true, threat->health, false };
            if (threat->health == 0)
            {
                despawnThreat(gameState, id);
                result.destroyed = true;
            }
            return result;
        }

        uint8_t facing(int32_t dx, int32_t dy)
        {
            // CoordsDirectionDelta: 0 = -x, 1 = +y, 2 = +x, 3 = -y.
            if (std::abs(dx) >= std::abs(dy))
                return dx < 0 ? 0 : 2;
            return dy > 0 ? 1 : 3;
        }

        void updateThreat(GameState_t& gameState, RecordId id)
        {
            auto& state = gameState.factory;
            auto* threat = state.threats.get(id);
            auto* proto = threat != nullptr ? getPrototype(threat->entry) : nullptr;
            if (proto == nullptr || proto->getKind() != PrototypeKind::threat)
                return;
            const auto& props = proto->getThreat();
            if (threat->cooldown > 0)
                threat->cooldown--;

            auto* target = threat->targetKind == static_cast<uint8_t>(ThreatTargetKind::machine)
                ? state.machines.get(threat->target)
                : nullptr;
            if (target == nullptr || !isDestructible(*target))
            {
                threat->target = nearestMachine(state, threat->x, threat->y);
                threat->targetKind = static_cast<uint8_t>(
                    threat->target != kNullRecord ? ThreatTargetKind::machine : ThreatTargetKind::none);
                target = state.machines.get(threat->target);
                if (target == nullptr)
                    return;
            }

            const auto fp = footprintOf(*target);
            if (distanceTo(fp, threat->x, threat->y) <= kReach)
            {
                threat->direction = facing(fp.cx - threat->x, fp.cy - threat->y);
                if (threat->cooldown == 0)
                {
                    threat->cooldown = props.attackTicks;
                    const auto targetId = threat->target;
                    applyDamage(gameState, DamageTarget::machine, targetId, props.damage, 0);
                }
                return;
            }

            // Walk straight at the footprint's centre, sub-unit steps carried in Q8.
            const uint32_t step = static_cast<uint32_t>(threat->subStep) + props.speedQ8;
            const int32_t units = static_cast<int32_t>(step >> 8);
            threat->subStep = static_cast<uint16_t>(step & 0xFF);
            threat->animation++;
            if (units == 0)
                return;
            const int32_t vx = fp.cx - threat->x;
            const int32_t vy = fp.cy - threat->y;
            const int32_t length = std::max(std::abs(vx), std::abs(vy));
            const int32_t move = std::min(units, length);
            threat->x += vx * move / length;
            threat->y += vy * move / length;
            threat->direction = facing(vx, vy);
            const CoordsXY pos{ threat->x, threat->y };
            if (MapIsLocationValid(pos))
                threat->z = TileElementHeight(pos);
        }
    } // namespace

    DamageResult applyDamage(GameState_t& gameState, DamageTarget target, uint32_t id, uint16_t amount, uint8_t damageType)
    {
        DamageResult result;
        switch (target)
        {
            case DamageTarget::machine:
                result = damageMachine(gameState, id, amount);
                break;
            case DamageTarget::ride:
                result = damageRide(gameState, id, amount);
                break;
            case DamageTarget::threat:
                result = damageThreat(gameState, id, amount);
                break;
            default:
                break;
        }
        if (result.hit)
            invokeDamageHook(target, id, amount, damageType, result.health, result.destroyed);
        return result;
    }

    std::optional<RecordId> spawnThreat(GameState_t& gameState, ObjectEntryIndex entry, const CoordsXYZ& pos)
    {
        auto* proto = getPrototype(entry);
        if (proto == nullptr || proto->getKind() != PrototypeKind::threat || !MapIsLocationValid(pos))
            return std::nullopt;
        auto& state = gameState.factory;
        RecordId id;
        auto& threat = state.threats.allocateRecord(id);
        threat.x = pos.x;
        threat.y = pos.y;
        threat.z = TileElementHeight(pos);
        threat.entry = entry;
        threat.health = proto->getThreat().health;
        threat.cooldown = proto->getThreat().attackTicks;
        state.topologyVersion++; // a park with only threats still saves its factory state
        invokeThreatHook(true, id, threat.x, threat.y);
        return id;
    }

    void despawnThreat(GameState_t& gameState, RecordId id)
    {
        auto& state = gameState.factory;
        auto* threat = state.threats.get(id);
        if (threat == nullptr)
            return;
        const int32_t x = threat->x;
        const int32_t y = threat->y;
        state.threats.release(id);
        invokeThreatHook(false, id, x, y);
    }

    void updateThreats(GameState_t& gameState)
    {
        auto& threats = gameState.factory.threats;
        if (threats.aliveCount() == 0)
            return;
        // Collect ids first: a hit can despawn records (through script-free paths only, but stay safe).
        std::vector<RecordId> ids;
        threats.forEach([&](RecordId id, const ThreatRecord&) { ids.push_back(id); });
        for (const auto id : ids)
            updateThreat(gameState, id);
    }

    bool turretAcceptsAmmo(const MachineRecord& machine, const FactoryPrototypeObject& proto, ObjectEntryIndex item)
    {
        const auto& props = proto.getMachine();
        if (machine.isDestroyed() || props.turretRange == 0 || item == kObjectEntryIndexNull
            || props.ammoItem.resolve() != item)
            return false;
        uint32_t count = 0;
        for (const auto& slot : machine.inputs)
            if (slot.item == item)
                count += slot.count;
        return count < 10 && !machine.inputs.empty()
            && std::any_of(
                   machine.inputs.begin(), machine.inputs.end(),
                   [&](const ItemStack& slot) { return slot.isEmpty() || slot.item == item; });
    }

    void updateTurret(GameState_t& gameState, MachineRecord& machine, const FactoryPrototypeObject& proto)
    {
        auto& state = gameState.factory;
        const auto& props = proto.getMachine();
        if (machine.progress > 0)
            machine.progress--; // cooldown
        // Rounds left in the loaded ammo item live in fuelEnergy (turrets burn no fuel).
        const int32_t size = std::max<int32_t>(1, props.size);
        const int32_t cx = machine.x * kCoordsXYStep + size * 16;
        const int32_t cy = machine.y * kCoordsXYStep + size * 16;
        const int32_t range = props.turretRange * kCoordsXYStep + size * 16;
        RecordId best = kNullRecord;
        int32_t bestDistance = INT32_MAX;
        state.threats.forEach([&](RecordId id, const ThreatRecord& threat) {
            const int32_t distance = std::max(std::abs(threat.x - cx), std::abs(threat.y - cy));
            if (distance <= range && distance < bestDistance)
            {
                bestDistance = distance;
                best = id;
            }
        });
        if (best == kNullRecord)
        {
            setMachineStatus(machine, MachineStatus::idle);
            return;
        }
        if (machine.fuelEnergy == 0)
        {
            const auto ammo = props.ammoItem.resolve();
            bool loaded = false;
            for (auto& slot : machine.inputs)
            {
                if (ammo != kObjectEntryIndexNull && slot.item == ammo && slot.count > 0)
                {
                    if (--slot.count == 0)
                        slot.item = kObjectEntryIndexNull;
                    machine.fuelEnergy = props.shotsPerAmmo;
                    loaded = true;
                    break;
                }
            }
            if (!loaded)
            {
                setMachineStatus(machine, MachineStatus::noAmmo);
                return;
            }
        }
        setMachineStatus(machine, MachineStatus::working);
        if (machine.progress > 0)
            return;
        machine.fuelEnergy--;
        machine.progress = props.turretCooldownTicks;
        invokeTurretFireHook(machine, best);
        applyDamage(gameState, DamageTarget::threat, best, props.turretDamage, 1);
    }

    std::pair<uint16_t, uint16_t> machineHealth(const MachineRecord& machine)
    {
        auto* proto = getPrototype(machine.entry);
        const uint16_t maxHealth = proto != nullptr ? proto->getMachine().health : 0;
        return { maxHealth > 0 ? machine.health : 0, maxHealth };
    }
} // namespace OpenRCT2::Factory
