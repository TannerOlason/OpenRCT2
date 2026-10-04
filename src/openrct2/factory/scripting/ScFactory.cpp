/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "ScFactory.h"

#include "../../Context.h"
#include "../../GameState.h"
#include "../../object/ObjectManager.h"
#include "../../world/Map.h"
#include "../../world/TileElementsView.h"
#include "../../world/tile_element/FactoryElement.h"
#include "../Combat.h"
#include "../FactoryPrototypeObject.h"
#include "../FactoryState.h"
#include "../FactoryTopology.h"
#include "../Technology.h"
#include "../actions/FactoryDamageAction.h"
#include "../actions/FactorySetParkOptionAction.h"
#include "../actions/FactorySetRecipeAction.h"
#include "../actions/FactoryThreatSpawnAction.h"

#ifdef ENABLE_SCRIPTING
    #include "../../actions/GameActionRunner.h"
    #include "../../scripting/HookEngine.h"
    #include "../../scripting/ScriptEngine.h"
    #include "../../scripting/ScriptUtil.hpp"
#endif

namespace OpenRCT2::Factory
{
#ifdef ENABLE_SCRIPTING
    using namespace OpenRCT2::Scripting;

    namespace
    {
        constexpr const char* kMachineKindNames[] = {
            "drill", "furnace", "assembler", "boiler", "engine", "pump", "lab", "turret", "export_depot",
        };
        constexpr const char* kMachineStatusNames[] = {
            "idle", "working", "no_input", "output_full", "no_fuel", "no_power", "no_ore", "no_recipe", "destroyed", "no_ammo",
        };
        constexpr const char* kDamageTargetNames[] = { "machine", "ride", "threat" };

        template<size_t N>
        const char* nameAt(const char* const (&names)[N], uint8_t index)
        {
            return index < N ? names[index] : "unknown";
        }

        JSValue identifierOf(JSContext* ctx, ObjectEntryIndex entry)
        {
            auto* proto = getPrototype(entry);
            return proto != nullptr ? JSFromStdString(ctx, proto->GetIdentifier()) : JS_NULL;
        }

        ObjectEntryIndex entryOf(const std::string& identifier)
        {
            auto& objectManager = GetContext()->GetObjectManager();
            auto* object = objectManager.GetLoadedObject(ObjectEntryDescriptor(identifier));
            if (object == nullptr || object->GetObjectType() != ObjectType::factoryPrototype)
                return kObjectEntryIndexNull;
            return objectManager.GetLoadedObjectEntryIndex(object);
        }

        JSValue stackToJS(JSContext* ctx, ObjectEntryIndex item, uint32_t count)
        {
            JSValue obj = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, obj, "item", identifierOf(ctx, item));
            JS_SetPropertyStr(ctx, obj, "count", JS_NewUint32(ctx, count));
            return obj;
        }

        JSValue stacksToJS(JSContext* ctx, const std::vector<ItemStack>& stacks)
        {
            JSValue array = JS_NewArray(ctx);
            int64_t index = 0;
            for (const auto& stack : stacks)
                if (!stack.isEmpty())
                    JS_SetPropertyInt64(ctx, array, index++, stackToJS(ctx, stack.item, stack.count));
            return array;
        }

        JSValue machineToJS(JSContext* ctx, RecordId id, const MachineRecord& machine)
        {
            JSValue obj = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, obj, "id", JS_NewUint32(ctx, id));
            JS_SetPropertyStr(ctx, obj, "object", identifierOf(ctx, machine.entry));
            JS_SetPropertyStr(ctx, obj, "kind", JSFromStdString(ctx, nameAt(kMachineKindNames, machine.kind)));
            JS_SetPropertyStr(ctx, obj, "status", JSFromStdString(ctx, nameAt(kMachineStatusNames, machine.status)));
            JS_SetPropertyStr(ctx, obj, "x", JS_NewInt32(ctx, machine.x));
            JS_SetPropertyStr(ctx, obj, "y", JS_NewInt32(ctx, machine.y));
            JS_SetPropertyStr(ctx, obj, "baseHeight", JS_NewInt32(ctx, machine.z));
            JS_SetPropertyStr(ctx, obj, "direction", JS_NewUint32(ctx, machine.direction));
            JS_SetPropertyStr(ctx, obj, "recipe", identifierOf(ctx, machine.recipe));
            const uint32_t percent = machine.craftCost == 0
                ? 0
                : static_cast<uint32_t>(
                      static_cast<uint64_t>(std::min(machine.progress, machine.craftCost)) * 100 / machine.craftCost);
            JS_SetPropertyStr(ctx, obj, "progress", JS_NewUint32(ctx, percent));
            JS_SetPropertyStr(ctx, obj, "inputs", stacksToJS(ctx, machine.inputs));
            JS_SetPropertyStr(ctx, obj, "outputs", stacksToJS(ctx, machine.outputs));
            JS_SetPropertyStr(
                ctx, obj, "fuel", machine.fuel.isEmpty() ? JS_NULL : stackToJS(ctx, machine.fuel.item, machine.fuel.count));
            JS_SetPropertyStr(ctx, obj, "powered", JS_NewBool(ctx, machine.powerNetwork != kNullRecord));
            const auto [health, maxHealth] = machineHealth(machine);
            JS_SetPropertyStr(ctx, obj, "health", JS_NewUint32(ctx, health));
            JS_SetPropertyStr(ctx, obj, "maxHealth", JS_NewUint32(ctx, maxHealth));
            return obj;
        }

        // The machine whose footprint covers tile (x, y), if any.
        std::optional<RecordId> machineAt(int32_t x, int32_t y)
        {
            const CoordsXY loc{ x * kCoordsXYStep, y * kCoordsXYStep };
            if (!MapIsLocationValid(loc))
                return std::nullopt;
            for (const auto* element : TileElementsView<FactoryElement>(loc))
                if (!element->isGhost() && element->getSubtype() == FactoryElementSubtype::machine)
                    return element->getRecordId();
            return std::nullopt;
        }

        JSValue machinesGet(JSContext* ctx, JSValue)
        {
            JSValue array = JS_NewArray(ctx);
            int64_t index = 0;
            getGameState().factory.machines.forEach([&](RecordId id, const MachineRecord& machine) {
                JS_SetPropertyInt64(ctx, array, index++, machineToJS(ctx, id, machine));
            });
            return array;
        }

        JSValue getMachine(JSContext* ctx, JSValue, int argc, JSValue* argv)
        {
            JS_UNPACK_INT32(x, ctx, argv[0]);
            JS_UNPACK_INT32(y, ctx, argv[1]);
            const auto id = machineAt(x, y);
            const auto* machine = id ? getGameState().factory.machines.get(*id) : nullptr;
            return machine != nullptr ? machineToJS(ctx, *id, *machine) : JS_NULL;
        }

        JSValue setRecipe(JSContext* ctx, JSValue, int argc, JSValue* argv)
        {
            JS_UNPACK_INT32(x, ctx, argv[0]);
            JS_UNPACK_INT32(y, ctx, argv[1]);
            auto recipe = kObjectEntryIndexNull;
            if (!JS_IsNull(argv[2]) && !JS_IsUndefined(argv[2]))
            {
                JS_UNPACK_STR(identifier, ctx, argv[2]);
                recipe = entryOf(identifier);
                if (recipe == kObjectEntryIndexNull)
                    return JS_ThrowPlainError(ctx, "Unknown recipe.");
            }
            const auto id = machineAt(x, y);
            const auto* machine = id ? getGameState().factory.machines.get(*id) : nullptr;
            if (machine == nullptr)
                return JS_ThrowPlainError(ctx, "No machine on that tile.");
            const CoordsXYZ origin{ machine->x * kCoordsXYStep, machine->y * kCoordsXYStep, machine->z * kCoordsZStep };
            auto action = GameActions::FactorySetRecipeAction(origin, recipe);
            auto result = GameActions::Execute(&action, getGameState());
            return JS_NewBool(ctx, result.error == GameActions::Status::ok);
        }

        JSValue warehouseGet(JSContext* ctx, JSValue)
        {
            JSValue array = JS_NewArray(ctx);
            int64_t index = 0;
            for (const auto& stock : getGameState().factory.warehouse.stock)
                JS_SetPropertyInt64(ctx, array, index++, stackToJS(ctx, stock.item, stock.count));
            return array;
        }

        JSValue productionGet(JSContext* ctx, JSValue)
        {
            JSValue array = JS_NewArray(ctx);
            int64_t index = 0;
            for (const auto& entry : getGameState().factory.production.produced)
                JS_SetPropertyInt64(ctx, array, index++, stackToJS(ctx, entry.item, entry.count));
            return array;
        }

        JSValue marketIncomeGet(JSContext* ctx, JSValue)
        {
            return JS_NewInt64(ctx, getGameState().factory.market.goodsSold);
        }

        JSValue technologiesGet(JSContext* ctx, JSValue)
        {
            const auto& gameState = getGameState();
            const auto& research = gameState.factory.research;
            JSValue array = JS_NewArray(ctx);
            int64_t index = 0;
            for (const auto technology : loadedTechnologies())
            {
                auto* proto = getPrototype(technology);
                if (proto == nullptr)
                    continue;
                const auto& props = proto->getTechnology();
                JSValue obj = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, obj, "object", JSFromStdString(ctx, proto->GetIdentifier()));
                JS_SetPropertyStr(ctx, obj, "name", JSFromStdString(ctx, proto->GetName()));
                JS_SetPropertyStr(ctx, obj, "researched", JS_NewBool(ctx, research.isResearched(technology)));
                JS_SetPropertyStr(ctx, obj, "available", JS_NewBool(ctx, isTechnologyAvailable(gameState, technology)));
                JS_SetPropertyStr(ctx, obj, "units", JS_NewUint32(ctx, props.units));
                JS_SetPropertyStr(ctx, obj, "unitsDone", JS_NewUint32(ctx, research.unitsDone(technology)));
                JSValue prerequisites = JS_NewArray(ctx);
                for (size_t i = 0; i < props.prerequisites.size(); i++)
                    JS_SetPropertyInt64(
                        ctx, prerequisites, static_cast<int64_t>(i), JSFromStdString(ctx, props.prerequisites[i].identifier));
                JS_SetPropertyStr(ctx, obj, "prerequisites", prerequisites);
                JSValue unlocks = JS_NewArray(ctx);
                int64_t u = 0;
                for (const auto& unlock : props.unlocks)
                    JS_SetPropertyInt64(ctx, unlocks, u++, JSFromStdString(ctx, unlock.identifier));
                for (const auto& ride : props.rideEntries)
                    JS_SetPropertyInt64(ctx, unlocks, u++, JSFromStdString(ctx, ride));
                for (const auto& group : props.sceneryGroups)
                    JS_SetPropertyInt64(ctx, unlocks, u++, JSFromStdString(ctx, group));
                JS_SetPropertyStr(ctx, obj, "unlocks", unlocks);
                JS_SetPropertyInt64(ctx, array, index++, obj);
            }
            return array;
        }

        JSValue researchTargetGet(JSContext* ctx, JSValue)
        {
            return identifierOf(ctx, getGameState().factory.research.current);
        }

        JSValue researchTargetSet(JSContext* ctx, JSValue, JSValue value)
        {
            auto target = kObjectEntryIndexNull;
            if (!JS_IsNull(value) && !JS_IsUndefined(value))
            {
                JS_UNPACK_STR(identifier, ctx, value);
                target = entryOf(identifier);
                if (target == kObjectEntryIndexNull)
                    return JS_ThrowPlainError(ctx, "Unknown technology.");
            }
            auto action = GameActions::FactorySetParkOptionAction(GameActions::FactoryParkOption::researchTarget, target);
            GameActions::Execute(&action, getGameState());
            return JS_UNDEFINED;
        }

        JSValue threatsGet(JSContext* ctx, JSValue)
        {
            JSValue array = JS_NewArray(ctx);
            int64_t index = 0;
            getGameState().factory.threats.forEach([&](RecordId id, const ThreatRecord& threat) {
                JSValue obj = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, obj, "id", JS_NewUint32(ctx, id));
                JS_SetPropertyStr(ctx, obj, "object", identifierOf(ctx, threat.entry));
                JS_SetPropertyStr(ctx, obj, "x", JS_NewInt32(ctx, threat.x));
                JS_SetPropertyStr(ctx, obj, "y", JS_NewInt32(ctx, threat.y));
                JS_SetPropertyStr(ctx, obj, "z", JS_NewInt32(ctx, threat.z));
                JS_SetPropertyStr(ctx, obj, "health", JS_NewUint32(ctx, threat.health));
                JS_SetPropertyStr(
                    ctx, obj, "target",
                    threat.targetKind == static_cast<uint8_t>(ThreatTargetKind::machine) ? JS_NewUint32(ctx, threat.target)
                                                                                         : JS_NULL);
                JS_SetPropertyInt64(ctx, array, index++, obj);
            });
            return array;
        }

        JSValue spawnThreat(JSContext* ctx, JSValue, int argc, JSValue* argv)
        {
            JS_UNPACK_STR(identifier, ctx, argv[0]);
            JS_UNPACK_INT32(x, ctx, argv[1]);
            JS_UNPACK_INT32(y, ctx, argv[2]);
            const auto entry = entryOf(identifier);
            if (entry == kObjectEntryIndexNull)
                return JS_ThrowPlainError(ctx, "Unknown threat.");
            auto action = GameActions::FactoryThreatSpawnAction(entry, x, y);
            auto result = GameActions::Execute(&action, getGameState());
            return JS_NewBool(ctx, result.error == GameActions::Status::ok);
        }

        JSValue damage(JSContext* ctx, JSValue, int argc, JSValue* argv)
        {
            JS_UNPACK_STR(targetName, ctx, argv[0]);
            JS_UNPACK_UINT32(id, ctx, argv[1]);
            JS_UNPACK_UINT32(amount, ctx, argv[2]);
            uint32_t damageType = 0;
            if (argc > 3 && JS_IsNumber(argv[3]))
                JS_ToUint32(ctx, &damageType, argv[3]);
            uint8_t target = static_cast<uint8_t>(DamageTarget::count);
            for (uint8_t i = 0; i < std::size(kDamageTargetNames); i++)
                if (targetName == kDamageTargetNames[i])
                    target = i;
            if (target == static_cast<uint8_t>(DamageTarget::count))
                return JS_ThrowPlainError(ctx, "Unknown damage target.");
            auto action = GameActions::FactoryDamageAction(
                target, id, static_cast<uint16_t>(std::min<uint32_t>(amount, 0xFFFF)), static_cast<uint8_t>(damageType));
            auto result = GameActions::Execute(&action, getGameState());
            return JS_NewBool(ctx, result.error == GameActions::Status::ok);
        }

        JSValue isUnlocked(JSContext* ctx, JSValue, int argc, JSValue* argv)
        {
            JS_UNPACK_STR(identifier, ctx, argv[0]);
            const auto entry = entryOf(identifier);
            return JS_NewBool(ctx, entry != kObjectEntryIndexNull && isPrototypeUnlocked(getGameState(), entry));
        }

        class ScFactory final : public ScBase
        {
        public:
            void Register(JSContext* ctx)
            {
                static constexpr JSCFunctionListEntry funcs[] = {
                    JS_CGETSET_DEF("machines", machinesGet, nullptr),
                    JS_CFUNC_DEF("getMachine", 2, getMachine),
                    JS_CFUNC_DEF("setRecipe", 3, setRecipe),
                    JS_CGETSET_DEF("warehouse", warehouseGet, nullptr),
                    JS_CGETSET_DEF("production", productionGet, nullptr),
                    JS_CGETSET_DEF("marketIncome", marketIncomeGet, nullptr),
                    JS_CGETSET_DEF("technologies", technologiesGet, nullptr),
                    JS_CGETSET_DEF("researchTarget", researchTargetGet, researchTargetSet),
                    JS_CFUNC_DEF("isUnlocked", 1, isUnlocked),
                    JS_CGETSET_DEF("threats", threatsGet, nullptr),
                    JS_CFUNC_DEF("spawnThreat", 3, spawnThreat),
                    JS_CFUNC_DEF("damage", 4, damage),
                };
                RegisterBase(ctx, "Factory", nullptr, funcs);
            }

            JSValue New(JSContext* ctx)
            {
                return MakeWithOpaque(ctx, nullptr);
            }
        };

        ScFactory gScFactory;

        HookEngine* hookEngineWith(HookType type)
        {
            auto* context = GetContext();
            if (context == nullptr)
                return nullptr;
            auto& hookEngine = context->GetScriptEngine().GetHookEngine();
            return hookEngine.HasSubscriptions(type) ? &hookEngine : nullptr;
        }
    } // namespace

    void registerScriptClasses(JSContext* ctx)
    {
        gScFactory.Register(ctx);
    }

    void unregisterScriptClasses()
    {
        gScFactory.Unregister();
    }

    void initialiseScriptContext(JSContext* ctx)
    {
        JSValue glb = JS_GetGlobalObject(ctx);
        JS_SetPropertyStr(ctx, glb, "factory", gScFactory.New(ctx));
        JS_FreeValue(ctx, glb);
    }

    void invokeMachineStatusHook(const MachineRecord& machine, MachineStatus previous)
    {
        auto* hookEngine = hookEngineWith(HookType::factoryMachineStatus);
        if (hookEngine == nullptr)
            return;
        JSContext* ctx = GetContext()->GetScriptEngine().GetContext();
        JSValue obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, obj, "x", JS_NewInt32(ctx, machine.x));
        JS_SetPropertyStr(ctx, obj, "y", JS_NewInt32(ctx, machine.y));
        JS_SetPropertyStr(ctx, obj, "object", identifierOf(ctx, machine.entry));
        JS_SetPropertyStr(ctx, obj, "status", JSFromStdString(ctx, nameAt(kMachineStatusNames, machine.status)));
        JS_SetPropertyStr(
            ctx, obj, "previousStatus", JSFromStdString(ctx, nameAt(kMachineStatusNames, static_cast<uint8_t>(previous))));
        hookEngine->Call(HookType::factoryMachineStatus, obj, false);
    }

    void invokeResearchCompleteHook(ObjectEntryIndex technology)
    {
        auto* hookEngine = hookEngineWith(HookType::factoryResearchComplete);
        if (hookEngine == nullptr)
            return;
        JSContext* ctx = GetContext()->GetScriptEngine().GetContext();
        JSValue obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, obj, "technology", identifierOf(ctx, technology));
        hookEngine->Call(HookType::factoryResearchComplete, obj, false);
    }
    void invokeDamageHook(
        DamageTarget target, uint32_t id, uint16_t amount, uint8_t damageType, uint16_t health, bool destroyed)
    {
        auto* hookEngine = hookEngineWith(HookType::factoryDamage);
        if (hookEngine == nullptr)
            return;
        JSContext* ctx = GetContext()->GetScriptEngine().GetContext();
        JSValue obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, obj, "target", JSFromStdString(ctx, nameAt(kDamageTargetNames, static_cast<uint8_t>(target))));
        JS_SetPropertyStr(ctx, obj, "id", JS_NewUint32(ctx, id));
        JS_SetPropertyStr(ctx, obj, "amount", JS_NewUint32(ctx, amount));
        JS_SetPropertyStr(ctx, obj, "damageType", JS_NewUint32(ctx, damageType));
        JS_SetPropertyStr(ctx, obj, "health", JS_NewUint32(ctx, health));
        JS_SetPropertyStr(ctx, obj, "destroyed", JS_NewBool(ctx, destroyed));
        hookEngine->Call(HookType::factoryDamage, obj, false);
    }

    void invokeThreatHook(bool spawned, RecordId id, int32_t x, int32_t y)
    {
        const auto type = spawned ? HookType::factoryThreatSpawn : HookType::factoryThreatDespawn;
        auto* hookEngine = hookEngineWith(type);
        if (hookEngine == nullptr)
            return;
        JSContext* ctx = GetContext()->GetScriptEngine().GetContext();
        JSValue obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, obj, "id", JS_NewUint32(ctx, id));
        JS_SetPropertyStr(ctx, obj, "x", JS_NewInt32(ctx, x));
        JS_SetPropertyStr(ctx, obj, "y", JS_NewInt32(ctx, y));
        hookEngine->Call(type, obj, false);
    }

    void invokeTurretFireHook(const MachineRecord& turret, RecordId threat)
    {
        auto* hookEngine = hookEngineWith(HookType::factoryTurretFire);
        if (hookEngine == nullptr)
            return;
        JSContext* ctx = GetContext()->GetScriptEngine().GetContext();
        JSValue obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, obj, "x", JS_NewInt32(ctx, turret.x));
        JS_SetPropertyStr(ctx, obj, "y", JS_NewInt32(ctx, turret.y));
        JS_SetPropertyStr(ctx, obj, "object", identifierOf(ctx, turret.entry));
        JS_SetPropertyStr(ctx, obj, "threat", JS_NewUint32(ctx, threat));
        hookEngine->Call(HookType::factoryTurretFire, obj, false);
    }
#else
    void registerScriptClasses(JSContext*)
    {
    }
    void unregisterScriptClasses()
    {
    }
    void initialiseScriptContext(JSContext*)
    {
    }
    void invokeMachineStatusHook(const MachineRecord&, MachineStatus)
    {
    }
    void invokeResearchCompleteHook(ObjectEntryIndex)
    {
    }
    void invokeDamageHook(DamageTarget, uint32_t, uint16_t, uint8_t, uint16_t, bool)
    {
    }
    void invokeThreatHook(bool, RecordId, int32_t, int32_t)
    {
    }
    void invokeTurretFireHook(const MachineRecord&, RecordId)
    {
    }
#endif
} // namespace OpenRCT2::Factory
