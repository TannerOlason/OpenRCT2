/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "WorldManager.h"

#include "../Context.h"
#include "../Diagnostic.h"
#include "../Game.h"
#include "../GameState.h"
#include "../OpenRCT2.h"
#include "../ParkImporter.h"
#include "../actions/GameAction.hpp"
#include "../core/Compression.h"
#include "../core/MemoryStream.h"
#include "../core/OrcaStream.hpp"
#include "../drawing/Drawing.Screen.h"
#include "../entity/EntityTweener.h"
#include "../entity/PatrolArea.h"
#include "../interface/Viewport.h"
#include "../interface/Window.h"
#include "../interface/WindowBase.h"
#include "../management/NewsItem.h"
#include "../park/ParkFile.h"
#include "../peep/RideUseSystem.h"
#include "../ride/Ride.h"
#include "../ui/WindowManager.h"
#include "../world/Banner.h"
#include "../world/Map.h"
#include "../world/MapAnimation.h"
#include "../world/MapOwnership.h"
#include "../world/Park.h"
#include "../world/Weather.h"
#include "../world/tile_element/SurfaceElement.h"
#include "FactorySerialisation.h"

#include <algorithm>
#include <any>
#include <cstring>
#include <memory>
#include <vector>

namespace OpenRCT2::Factory::Worlds
{
    namespace
    {
        // Per-world state that lives outside GameState_t, held here while the world is inactive.
        struct Stash
        {
            std::any map;       // tile index and tile elements in use (MapSwapWorldCaches)
            std::any animation; // map animation sets (MapAnimations::SwapWorldCaches)
            RideUse::RideHistory rideHistory;
            RideUse::RideTypeHistory rideTypeHistory;
            uint32_t landOwnershipSales{};
            uint32_t landConstructionSales{};
        };

        // Slot i holds world i's state, except the active world's slot, which is empty (its state is in GameState.cpp).
        std::vector<std::unique_ptr<GameState_t>> gStates(1);
        std::vector<Stash> gStashes(1);
        WorldId gActive = kPrimaryWorld;
        WorldId gViewed = kPrimaryWorld;
        bool gTicking = false;
        WorldId gTickingWorld = kPrimaryWorld;
        bool gSavingNested = false;
        bool gImportingNested = false;
        std::vector<std::vector<uint8_t>> gPendingWorlds;
        constexpr uint16_t kWorldsChunkVersion = 1;

        void swapCaches(Stash& stash)
        {
            MapSwapWorldCaches(stash.map);
            MapAnimations::SwapWorldCaches(stash.animation);
            std::swap(RideUse::GetHistory(), stash.rideHistory);
            std::swap(RideUse::GetTypeHistory(), stash.rideTypeHistory);
            std::swap(gLandRemainingOwnershipSales, stash.landOwnershipSales);
            std::swap(gLandRemainingConstructionSales, stash.landConstructionSales);
        }

        // Company state: one bank account, research effort, date and set of goals across every world.
        void moveCompany(GameState_t& from, GameState_t& to)
        {
            auto& a = from.park;
            auto& b = to.park;
            b.flags = a.flags;
            b.cash = a.cash;
            std::copy(std::begin(a.cashHistory), std::end(a.cashHistory), std::begin(b.cashHistory));
            b.weeklyProfitAverageDivisor = a.weeklyProfitAverageDivisor;
            b.weeklyProfitAverageDividend = a.weeklyProfitAverageDividend;
            std::copy(std::begin(a.weeklyProfitHistory), std::end(a.weeklyProfitHistory), std::begin(b.weeklyProfitHistory));
            b.historicalProfit = a.historicalProfit;
            b.currentProfit = a.currentProfit;
            std::memcpy(b.expenditureTable, a.expenditureTable, sizeof(a.expenditureTable));
            b.currentExpenditure = a.currentExpenditure;
            b.companyValue = a.companyValue;
            b.bankLoan = a.bankLoan;
            b.maxBankLoan = a.maxBankLoan;
            b.bankLoanInterestRate = a.bankLoanInterestRate;

            to.scenarioOptions = from.scenarioOptions;
            to.date = from.date;
            to.nextGuestNumber = from.nextGuestNumber;
            to.scenarioCompletedCompanyValue = from.scenarioCompletedCompanyValue;
            to.scenarioCompanyValueRecord = from.scenarioCompanyValueRecord;
            to.scenarioCompletedBy = from.scenarioCompletedBy;
            to.scenarioFileName = from.scenarioFileName;
            to.pluginStorage = from.pluginStorage;
            to.cheats = from.cheats;

            to.researchFundingLevel = from.researchFundingLevel;
            to.researchPriorities = from.researchPriorities;
            to.researchProgress = from.researchProgress;
            to.researchProgressStage = from.researchProgressStage;
            to.researchExpectedMonth = from.researchExpectedMonth;
            to.researchExpectedDay = from.researchExpectedDay;
            to.researchLastItem = from.researchLastItem;
            to.researchNextItem = from.researchNextItem;
            to.researchItemsUninvented = from.researchItemsUninvented;
            to.researchItemsInvented = from.researchItemsInvented;
            to.researchUncompletedCategories = from.researchUncompletedCategories;

            auto& fa = from.factory;
            auto& fb = to.factory;
            fb.research = fa.research;
            fb.transfers = fa.transfers;
            fb.portals = fa.portals;
            fb.market = fa.market;
            fb.production = fa.production;
            fb.parkExt.constructionMode = fa.parkExt.constructionMode;
            fb.parkExt.shopStockMode = fa.parkExt.shopStockMode;
            fb.parkExt.guestsToured = fa.parkExt.guestsToured;
        }

        void ownAllLand(GameState_t& gameState)
        {
            for (int32_t y = 1; y < gameState.mapSize.y - 1; y++)
                for (int32_t x = 1; x < gameState.mapSize.x - 1; x++)
                    if (auto* surface = MapGetSurfaceElementAt(TileCoordsXY{ x, y }))
                        surface->setOwnership(OwnershipFlags{ OwnershipFlag::landOwned });
        }
    } // namespace

    size_t count()
    {
        return gStates.size();
    }

    WorldId active()
    {
        return gActive;
    }

    WorldId viewed()
    {
        return gViewed;
    }

    GameState_t& state(WorldId id)
    {
        if (id == gActive || id >= gStates.size() || gStates[id] == nullptr)
            return getGameState();
        return *gStates[id];
    }

    void activate(WorldId id)
    {
        if (id == gActive || id >= gStates.size())
            return;
        const WorldId previous = gActive;
        swapCaches(gStashes[previous]);
        swapCaches(gStashes[id]);
        swapGameState(gStates[id]); // the active slot now holds the previous world's state
        std::swap(gStates[previous], gStates[id]);
        moveCompany(*gStates[previous], getGameState());
        gActive = id;
        UpdateConsolidatedPatrolAreas();
    }

    Scope::Scope(WorldId id)
        : _previous(gActive)
    {
        activate(id);
    }

    Scope::~Scope()
    {
        activate(_previous);
    }

    WorldId create(const TileCoordsXY& mapSize)
    {
        if (gStates.size() >= kMaxWorlds)
            return kMaxWorlds;
        const auto id = static_cast<WorldId>(gStates.size());
        const WorldId previous = gActive;
        gStates.push_back(std::make_unique<GameState_t>());
        gStashes.emplace_back();

        // Swap the blank world in without moving company state, initialise it, then hand the company over.
        swapCaches(gStashes[previous]);
        swapCaches(gStashes[id]);
        swapGameState(gStates[id]);
        std::swap(gStates[previous], gStates[id]);
        gActive = id;
        auto& gameState = getGameState();
        MapInit(mapSize, Drawing::Colour::black);
        Park::Initialise(gameState.park, gameState);
        BannerInit(gameState);
        RideInitAll();
        gameState.entities.resetAllEntities();
        gameState.factory.reset();
        UpdateConsolidatedPatrolAreas();
        Weather::reset();
        News::InitQueue(gameState);
        ownAllLand(gameState);
        moveCompany(*gStates[previous], gameState);
        gameState.currentTicks = gStates[previous]->currentTicks;
        gameState.savedView = Translate3DTo2DWithZ(0, CoordsXYZ{ mapSize.x * 16, mapSize.y * 16, 14 * kCoordsZStep });
        gameState.savedViewZoom = ZoomLevel{ 0 };
        gameState.savedViewRotation = 0;

        activate(previous);
        return id;
    }

    void adoptActiveAsPrimary()
    {
        if (gImportingNested || (gStates.size() == 1 && gActive == kPrimaryWorld))
            return;
        // The active world's caches are the live globals; every stash and inactive state is dropped.
        gStates.clear();
        gStates.resize(1);
        gStashes.clear();
        gStashes.resize(1);
        gActive = kPrimaryWorld;
        gViewed = kPrimaryWorld;
    }

    bool tickAll()
    {
        // Single worlds go through the loop too, so a world created during world 0's pass (by an action replayed at
        // the start of the tick) still ticks this tick, exactly as when it existed before the tick began.
        if (gTicking)
            return false;
        gTicking = true;
        const WorldId shown = gViewed;
        for (WorldId id = 0; id < gStates.size(); id++)
        {
            activate(id);
            gTickingWorld = id;
            const auto ticksBefore = getGameState().currentTicks;
            gameStateUpdateLogic();
            // A client that must not run past the server stops in world 0's pass: no world ticks then.
            if (id == kPrimaryWorld && getGameState().currentTicks == ticksBefore)
                break;
        }
        gTickingWorld = kPrimaryWorld;
        activate(shown);
        gTicking = false;
        return true;
    }

    bool isPrimaryPass()
    {
        return !gTicking || gTickingWorld == kPrimaryWorld;
    }

    bool isViewedActive()
    {
        return gActive == gViewed;
    }

    void setViewed(WorldId id)
    {
        if (id >= gStates.size() || id == gViewed)
            return;
        ViewportSetSavedView();
        auto* windowMgr = Ui::GetWindowManager();
        if (windowMgr != nullptr)
        {
            // Windows that point at things in the old world close; lists, finances and the worlds window stay.
            windowMgr->CloseConstructionWindows();
            for (auto cls : { WindowClass::ride, WindowClass::rideConstruction, WindowClass::peep, WindowClass::banner,
                              WindowClass::tileInspector, WindowClass::patrolArea, WindowClass::trackDesignPlace,
                              WindowClass::demolishRidePrompt, WindowClass::firePrompt, WindowClass::viewport, WindowClass::map,
                              WindowClass::factoryBuild, WindowClass::factoryInfo, WindowClass::factoryPower,
                              WindowClass::factoryBlueprint })
                windowMgr->CloseByClass(cls);
        }
        if (auto* mainWindow = WindowGetMain())
            WindowUnfollowSprite(*mainWindow);
        EntityTweener::get().reset();
        activate(id);
        gViewed = id;
        auto& gameState = getGameState();
        gameState.entities.resetEntitySpatialIndices();
        ResetAllSpriteQuadrantPlacements();
        if (windowMgr != nullptr)
        {
            windowMgr->SetMainView(gameState.savedView, gameState.savedViewZoom, gameState.savedViewRotation);
            Drawing::GfxInvalidateScreen();
        }
    }

    WorldId saveTarget()
    {
        return gSavingNested ? gActive : kPrimaryWorld;
    }

    void writeWorldsChunk(OrcaStream& os)
    {
        if (gSavingNested || gStates.size() <= 1)
            return;
        std::vector<std::vector<uint8_t>> parks;
        for (WorldId id = 1; id < gStates.size(); id++)
        {
            Scope inWorld(id);
            gSavingNested = true;
            MemoryStream ms;
            ParkFileExporter exporter;
            exporter.Export(getGameState(), ms, Compression::kNoCompressionLevel);
            gSavingNested = false;
            auto* data = static_cast<const uint8_t*>(ms.GetData());
            parks.emplace_back(data, data + ms.GetLength());
        }
        os.readWriteChunk(ChunkType::worlds, [&](OrcaStream::ChunkStream& cs) {
            uint16_t version = kWorldsChunkVersion;
            cs.write(version);
            auto worldCount = static_cast<uint32_t>(parks.size());
            cs.write(worldCount);
            for (const auto& park : parks)
            {
                auto length = static_cast<uint32_t>(park.size());
                cs.write(length);
                cs.write(park.data(), park.size());
            }
        });
    }

    void readWorldsChunk(OrcaStream& os)
    {
        if (gImportingNested)
            return;
        gPendingWorlds.clear();
        os.readWriteChunk(ChunkType::worlds, [&](OrcaStream::ChunkStream& cs) {
            auto version = cs.read<uint16_t>();
            if (version > kWorldsChunkVersion)
            {
                LOG_ERROR("Worlds chunk version %u is newer than supported %u", version, kWorldsChunkVersion);
                return;
            }
            const auto worldCount = cs.read<uint32_t>();
            for (uint32_t i = 0; i < worldCount && i + 1 < kMaxWorlds; i++)
            {
                const auto length = cs.read<uint32_t>();
                std::vector<uint8_t> park(length);
                cs.read(park.data(), length);
                gPendingWorlds.push_back(std::move(park));
            }
        });
    }

    void finishImport()
    {
        if (gImportingNested || gPendingWorlds.empty())
            return;
        auto pending = std::move(gPendingWorlds);
        gPendingWorlds.clear();
        auto& context = *GetContext();
        for (auto& park : pending)
        {
            const auto id = create({ 32, 32 });
            if (id >= kMaxWorlds)
                break;
            Scope inWorld(id);
            gImportingNested = true;
            try
            {
                MemoryStream ms(park.data(), park.size());
                auto importer = ParkImporter::CreateParkFile(context.GetObjectRepository());
                importer->LoadFromStream(&ms, false, true);
                MapAnimations::ClearAll();
                importer->Import(getGameState());
                MapAnimations::MarkAllTiles();
                getGameState().entities.resetEntitySpatialIndices();
                UpdateConsolidatedPatrolAreas();
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Unable to load world %u: %s", id, e.what());
            }
            gImportingNested = false;
            // World 0 holds the company: overwrite this world's saved copy before handing back.
            moveCompany(*gStates[kPrimaryWorld], getGameState());
        }
    }

    WorldId actionWorld(const GameActions::GameAction& action)
    {
        return static_cast<WorldId>((action.GetFlags().holder >> 16) & 0xFF);
    }

    void stampActionWorld(const GameActions::GameAction& action)
    {
        const auto flags0 = action.GetFlags();
        if (gActive == kPrimaryWorld || actionWorld(action) != kPrimaryWorld
            || flags0.hasAny(GameActions::CommandFlag::networked, GameActions::CommandFlag::replay))
            return;
        auto& mutableAction = const_cast<GameActions::GameAction&>(action);
        auto flags = mutableAction.GetFlags();
        flags.holder |= static_cast<uint32_t>(gActive) << 16;
        mutableAction.SetFlags(flags);
    }
} // namespace OpenRCT2::Factory::Worlds
