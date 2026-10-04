/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. The freight railway: flat miniature-railway track whose trains carry items between
// loader and unloader containers beside their stations instead of guests (ADR 0014). Occupies the free RIDE_TYPE_1F
// slot as RIDE_TYPE_FREIGHT_RAILWAY.

#pragma once

#include "../SpriteIds.h"
#include "../drawing/LightFX.h"
#include "../ride/RideData.h"
#include "../ride/RideStringIds.h"
#include "../ride/ShopItem.h"
#include "FactoryStringIds.h"

// clang-format off
namespace OpenRCT2
{
constexpr RideTypeDescriptor kFreightRailwayRTD =
{
    .Category = RideCategory::transport,
    .StartTrackPiece = TrackElemType::endStation,
    .TrackPaintFunctions = TrackDrawerDescriptor({
        .trackStyle = TrackStyle::miniatureRailway,
        .supportType = WoodenSupportType::truss,
        // Flat only: the freight wagons have flat sprites.
        .enabledTrackGroups = {TrackGroup::straight, TrackGroup::stationEnd, TrackGroup::sBend, TrackGroup::curveSmall, TrackGroup::curve},
        .extraTrackGroups = {},
    }),
    .InvertedTrackPaintFunctions = {},
    .flags = RtdFlags(RtdFlag::hasTrackColourSupports, RtdFlag::canSynchroniseWithAdjacentStations,
                     RtdFlag::hasDataLogging, RtdFlag::hasLoadOptions, RtdFlag::hasVehicleColours, RtdFlag::hasTrack,
                     RtdFlag::supportsMultipleColourSchemes, RtdFlag::allowMoreVehiclesThanStationFits,
                     RtdFlag::allowMultipleCircuits, RtdFlag::showInTrackDesigner, RtdFlag::interestingToLookAt),
    .rideModes = { RideMode::continuousCircuit },
    .DefaultMode = RideMode::continuousCircuit,
    .OperatingSettings = { 5, 18 },
    .Naming = { STR_FT_RIDE_NAME_FREIGHT_RAILWAY, STR_FT_RIDE_DESCRIPTION_FREIGHT_RAILWAY },
    .NameConvention = { RideComponentType::train, RideComponentType::track, RideComponentType::station },
    .availableBreakdowns = { Breakdown::safetyCutOut, Breakdown::vehicleMalfunction },
    .Heights = { 7, 32, 5, 9, },
    .MaxMass = 39,
    .LiftData = { Audio::SoundId::null, 5, 5 },
    .RatingsMultipliers = { 70, 10, 10 },
    .UpkeepCosts = { 70, 20, 0, 8, 3, 5 },
    .BuildCosts = { 12.50_GBP, 2.50_GBP, 30, },
    .DefaultPrices = { 0, 0 },
    .DefaultMusic = kMusicObjectSummer,
    .PhotoItem = ShopItem::photo,
    .BonusValue = 50,
    .ColourPresets = TRACK_COLOUR_PRESETS(
        { Drawing::Colour::saturatedBrown, Drawing::Colour::saturatedBrown, Drawing::Colour::grey },
        { Drawing::Colour::lightPurple, Drawing::Colour::lightPurple, Drawing::Colour::white },
        { Drawing::Colour::bordeauxRed, Drawing::Colour::bordeauxRed, Drawing::Colour::oliveGreen },
        { Drawing::Colour::grey, Drawing::Colour::grey, Drawing::Colour::black },
        { Drawing::Colour::black, Drawing::Colour::black, Drawing::Colour::saturatedBrown },
        { Drawing::Colour::brightYellow, Drawing::Colour::brightYellow, Drawing::Colour::brightRed },
        { Drawing::Colour::lightWater, Drawing::Colour::lightWater, Drawing::Colour::grey },
        { Drawing::Colour::icyBlue, Drawing::Colour::icyBlue, Drawing::Colour::white },
        { Drawing::Colour::white, Drawing::Colour::white, Drawing::Colour::oliveGreen },
    ),
    .ColourPreview = { SPR_RIDE_DESIGN_PREVIEW_MINIATURE_RAILWAY_TRACK, SPR_RIDE_DESIGN_PREVIEW_MINIATURE_RAILWAY_SUPPORTS },
    .ColourKey = RideColourKey::ride,
    .Name = "freight_railway",
    .RatingsData =
    {
        RatingsCalculationType::normal,
        { RideRating::make(1, 50), RideRating::make(0, 40), RideRating::make(0, 10) },
        12,
        kDynamicRideShelterRating,
        false,
        {
            { RatingsModifierType::bonusLength,           6000,             764, 0, 0 },
            { RatingsModifierType::bonusSynchronisation,  0,                RideRating::make(0, 15), RideRating::make(0, 00), 0 },
            { RatingsModifierType::bonusTrainLength,      0,                187245, 0, 0 },
            { RatingsModifierType::bonusMaxSpeed,         0,                44281, 88562, 35424 },
            { RatingsModifierType::bonusAverageSpeed,     0,                291271, 436906, 0 },
            { RatingsModifierType::bonusDuration,         150,              26214, 0, 0 },
            { RatingsModifierType::bonusTurns,            0,                14860, 0, 11437 },
            { RatingsModifierType::bonusSheltered,        0,                12850, 6553, 4681 },
            { RatingsModifierType::bonusProximity,        0,                11183, 0, 0 },
            { RatingsModifierType::bonusScenery,          0,                8366, 0, 0 },
            { RatingsModifierType::requirementLength,     0xC80000,         8, 2, 2 },
        },
    },
    .UpdateRotating = UpdateRotatingDefault,
    .LightFXAddLightsMagicVehicle = Drawing::LightFx::AddLightsMagicVehicle_MiniatureRailway,
};
} // namespace OpenRCT2
// clang-format on
