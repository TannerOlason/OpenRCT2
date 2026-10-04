/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "FactoryBench.h"

#include "../Context.h"
#include "../GameState.h"
#include "../OpenRCT2.h"
#include "../command_line/CommandLine.hpp"
#include "../core/Console.hpp"
#include "../object/ObjectManager.h"
#include "../world/Map.h"
#include "../world/tile_element/FactoryElement.h"
#include "../world/tile_element/SurfaceElement.h"
#include "FactoryPrototypeObject.h"
#include "FactoryState.h"
#include "FactoryTopology.h"
#include "SyncChecksum.h"

#include <algorithm>
#include <chrono>
#include <memory>

namespace OpenRCT2::Factory
{
    namespace
    {
        struct BenchPrototypes
        {
            ObjectEntryIndex belt = kObjectEntryIndexNull;
            ObjectEntryIndex inserter = kObjectEntryIndexNull;
            ObjectEntryIndex chest = kObjectEntryIndexNull;
            ObjectEntryIndex drill = kObjectEntryIndexNull;
            ObjectEntryIndex furnace = kObjectEntryIndexNull;
            ObjectEntryIndex assembler = kObjectEntryIndexNull;
            ObjectEntryIndex generator = kObjectEntryIndexNull;
            ObjectEntryIndex pole = kObjectEntryIndexNull;
            ObjectEntryIndex pipe = kObjectEntryIndexNull;
            ObjectEntryIndex orePatch = kObjectEntryIndexNull;
            ObjectEntryIndex coal = kObjectEntryIndexNull;
            ObjectEntryIndex plate = kObjectEntryIndexNull;
            ObjectEntryIndex gearRecipe = kObjectEntryIndexNull;
        };

        ObjectEntryIndex load(const char* name, std::string& error)
        {
            auto& objectManager = GetContext()->GetObjectManager();
            const std::string identifier = std::string("factory-tour.factory_prototype.") + name;
            auto* object = objectManager.LoadObject(identifier);
            if (object == nullptr)
            {
                if (error.empty())
                    error = "missing object " + identifier + " (run scan-objects?)";
                return kObjectEntryIndexNull;
            }
            return objectManager.GetLoadedObjectEntryIndex(object);
        }

        FactoryElement* placeAt(GameState_t& gameState, int32_t tx, int32_t ty, Direction dir, ObjectEntryIndex entry)
        {
            auto* surface = MapGetSurfaceElementAt(TileCoordsXY{ tx, ty });
            if (surface == nullptr)
                return nullptr;
            return placeElement(
                gameState, CoordsXYZ{ tx * kCoordsXYStep, ty * kCoordsXYStep, surface->getBaseZ() }, dir, entry, false);
        }
    } // namespace

    void benchMapSize(int32_t cells, int32_t& width, int32_t& height)
    {
        cells = std::clamp(cells, 1, kBenchMaxCells);
        const int32_t columns = std::min(cells, kBenchMaxColumns);
        const int32_t rows = (cells + columns - 1) / columns;
        width = columns * kBenchCellWidth + 4;
        height = rows * kBenchCellHeight + 4;
    }

    bool buildBenchFactory(GameState_t& gameState, int32_t cells, BenchCounts& counts, std::string& error)
    {
        if (cells < 1 || cells > kBenchMaxCells)
        {
            error = "cells must be between 1 and " + std::to_string(kBenchMaxCells);
            return false;
        }
        BenchPrototypes p;
        p.belt = load("belt_basic", error);
        p.inserter = load("inserter_basic", error);
        p.chest = load("chest_wooden", error);
        p.drill = load("burner_drill", error);
        p.furnace = load("stone_furnace", error);
        p.assembler = load("assembling_machine", error);
        p.generator = load("burner_generator", error);
        p.pole = load("small_pole", error);
        p.pipe = load("pipe_basic", error);
        p.orePatch = load("iron_ore_patch", error);
        p.coal = load("coal", error);
        p.plate = load("iron_plate", error);
        p.gearRecipe = load("iron_gear_recipe", error);
        load("iron_ore", error);
        load("iron_plate_smelting", error);
        load("iron_gear", error);
        if (!error.empty())
            return false;

        int32_t width = 0;
        int32_t height = 0;
        benchMapSize(cells, width, height);
        if (gameState.mapSize.x < width || gameState.mapSize.y < height)
        {
            error = "map too small for the bench";
            return false;
        }
        auto& state = gameState.factory;
        state.ore.resize(gameState.mapSize);

        const Direction east = 2;
        const Direction west = 0;
        const int32_t columns = std::min(cells, kBenchMaxColumns);
        for (int32_t cell = 0; cell < cells; cell++)
        {
            const int32_t cx = 2 + (cell % columns) * kBenchCellWidth;
            const int32_t cy = 2 + (cell / columns) * kBenchCellHeight;
            for (int32_t dx = 0; dx < 3; dx++)
                for (int32_t dy = 0; dy < 3; dy++)
                    state.ore.set({ cx + dx, cy + dy }, { p.orePatch, 0, 100000 });

            // Mining row: drill -> belts -> inserter -> furnace -> inserter -> chest.
            auto* drill = placeAt(gameState, cx + 1, cy + 1, east, p.drill);
            for (int32_t x = cx + 2; x <= cx + 13; x++)
                placeAt(gameState, x, cy + 1, east, p.belt);
            placeAt(gameState, cx + 14, cy + 1, east, p.inserter);
            auto* furnace = placeAt(gameState, cx + 15, cy + 1, east, p.furnace);
            placeAt(gameState, cx + 16, cy + 1, east, p.inserter);
            placeAt(gameState, cx + 17, cy + 1, east, p.chest);

            // Crafting row: chest -> inserter -> assembler -> inserter -> chest, a generator and a pole covering both.
            auto* plates = placeAt(gameState, cx + 1, cy + 3, east, p.chest);
            placeAt(gameState, cx + 2, cy + 3, east, p.inserter);
            auto* assembler = placeAt(gameState, cx + 3, cy + 3, east, p.assembler);
            placeAt(gameState, cx + 4, cy + 3, east, p.inserter);
            placeAt(gameState, cx + 5, cy + 3, west, p.chest);
            auto* generator = placeAt(gameState, cx + 6, cy + 3, east, p.generator);
            placeAt(gameState, cx + 4, cy + 2, east, p.pole);
            for (int32_t x = cx + 6; x <= cx + 11; x++)
                placeAt(gameState, x, cy + 2, east, p.pipe);

            if (drill == nullptr || furnace == nullptr || plates == nullptr || assembler == nullptr || generator == nullptr)
            {
                error = "could not place cell " + std::to_string(cell);
                return false;
            }
            // Records may move as pools grow, so look them up by id only now.
            for (auto* element : { drill, furnace, generator })
            {
                auto* machine = state.machines.get(element->getRecordId());
                if (machine != nullptr)
                    machine->fuel = { p.coal, 50 };
            }
            if (auto* machine = state.machines.get(assembler->getRecordId()))
                machine->recipe = p.gearRecipe;
            if (auto* chest = state.containers.get(plates->getRecordId()))
                for (auto& slot : chest->slots)
                    slot = { p.plate, 100 };
        }

        counts.cells = cells;
        counts.mapWidth = width;
        counts.mapHeight = height;
        state.beltSegments.forEach([&](RecordId, BeltSegmentRecord& segment) {
            counts.segments++;
            counts.belts += segment.tiles.size();
        });
        counts.inserters = state.inserters.aliveCount();
        counts.machines = state.machines.aliveCount();
        counts.containers = state.containers.aliveCount();
        counts.poles = state.poles.aliveCount();
        counts.pipes = state.pipes.aliveCount();
        return true;
    }
} // namespace OpenRCT2::Factory

namespace OpenRCT2
{
    using namespace OpenRCT2::CommandLine;

    // clang-format off
    static constexpr CommandLineOptionDefinition kNoOptions[]
    {
        kOptionTableEnd
    };

    static ExitCode HandleFactoryBench(CommandLineArgEnumerator* argEnumerator);

    const CommandLineCommand CommandLine::kFactoryBenchCommands[]{
        DefineCommand("", "[ticks] [cells] [budget ms]", kNoOptions, HandleFactoryBench),
        kCommandTableEnd
    };
    // clang-format on

    static double toMilliseconds(uint64_t nanoseconds)
    {
        return static_cast<double>(nanoseconds) / 1e6;
    }

    /**
     * Builds the bench factory on a fresh map, runs the factory update for `ticks` ticks and prints per-phase
     * timings and the sync checksum. Fails when the average tick exceeds the budget (the CI gate).
     */
    static ExitCode HandleFactoryBench(CommandLineArgEnumerator* argEnumerator)
    {
        int32_t ticks = 400;
        int32_t cells = 2000;
        int32_t budgetMs = 8;
        argEnumerator->TryPopInteger(&ticks);
        argEnumerator->TryPopInteger(&cells);
        argEnumerator->TryPopInteger(&budgetMs);
        ticks = std::max(1, ticks);

        gOpenRCT2Headless = true;
        gOpenRCT2NoGraphics = true;
        std::unique_ptr<IContext> context(CreateContext());
        if (!context->Initialise())
        {
            Console::Error::WriteLine("Context initialization failed.");
            return ExitCode::fail;
        }

        int32_t width = 0;
        int32_t height = 0;
        Factory::benchMapSize(cells, width, height);
        auto& gameState = getGameState();
        gameStateInitAll(gameState, { std::max(width, height), std::max(width, height) });

        using Clock = std::chrono::steady_clock;
        const auto setupStart = Clock::now();
        Factory::BenchCounts counts;
        std::string error;
        if (!Factory::buildBenchFactory(gameState, cells, counts, error))
        {
            Console::Error::WriteLine("factory-bench: %s", error.c_str());
            return ExitCode::fail;
        }
        const auto setupNs = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - setupStart).count();
        Console::WriteLine(
            "factory-bench: %d cells on a %dx%d map: %zu belts in %zu segments, %zu inserters, %zu machines, "
            "%zu chests, %zu poles, %zu pipes",
            counts.cells, counts.mapWidth, counts.mapHeight, counts.belts, counts.segments, counts.inserters, counts.machines,
            counts.containers, counts.poles, counts.pipes);
        Console::WriteLine("setup: %.1f ms", toMilliseconds(static_cast<uint64_t>(setupNs)));

        Factory::UpdatePhaseTimes phases;
        uint64_t total = 0;
        uint64_t worst = 0;
        for (int32_t i = 0; i < ticks; i++)
        {
            const auto start = Clock::now();
            Factory::update(gameState, &phases);
            const auto elapsed = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count());
            total += elapsed;
            worst = std::max(worst, elapsed);
            gameState.currentTicks++;
        }
        const double average = toMilliseconds(total) / ticks;
        const auto perTick = [&](uint64_t ns) { return static_cast<double>(ns) / 1e3 / ticks; };
        Console::WriteLine("%d ticks: average %.3f ms/tick, worst %.3f ms", ticks, average, toMilliseconds(worst));
        Console::WriteLine(
            "phases (us/tick): belts %.1f, splitters %.1f, inserters %.1f, power %.1f, fluids %.1f, machines %.1f",
            perTick(phases.belts), perTick(phases.splitters), perTick(phases.inserters), perTick(phases.power),
            perTick(phases.fluids), perTick(phases.machines));
        Console::WriteLine("checksum: %s", Factory::computeSyncChecksum(gameState).toString().c_str());
        if (average > budgetMs)
        {
            Console::Error::WriteLine("factory-bench: average %.3f ms/tick is over the %d ms budget", average, budgetMs);
            return ExitCode::fail;
        }
        return ExitCode::ok;
    }
} // namespace OpenRCT2
