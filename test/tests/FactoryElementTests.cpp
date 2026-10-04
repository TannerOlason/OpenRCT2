/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include <gtest/gtest.h>
#include <openrct2/core/EnumUtils.hpp>
#include <openrct2/world/Map.h>
#include <openrct2/world/tile_element/BannerElement.h>
#include <openrct2/world/tile_element/FactoryElement.h>
#include <openrct2/world/tile_element/TileElement.h>

using namespace OpenRCT2;

TEST(FactoryElementTests, TypeValueIsReserved)
{
    // 9 is the fork's reserved slot; 8, 14 and 15 are RCT1/RCT2 importer corruption markers.
    EXPECT_EQ(EnumValue(TileElementType::factory), 9);
    EXPECT_EQ(sizeof(FactoryElement), kTileElementSize);
}

TEST(FactoryElementTests, ClearAsProducesEmptyFactoryElement)
{
    TileElement element{};
    element.clearAs(TileElementType::factory);
    element.setDirection(2);

    auto* factory = element.as<FactoryElement>();
    ASSERT_NE(factory, nullptr);
    EXPECT_EQ(element.getType(), TileElementType::factory);
    EXPECT_EQ(element.getDirection(), 2);
    EXPECT_EQ(element.as<BannerElement>(), nullptr);
    EXPECT_EQ(element.asBanner(), nullptr);
    EXPECT_EQ(element.asFactory(), factory);

    EXPECT_EQ(factory->getSubtype(), FactoryElementSubtype::belt);
    EXPECT_EQ(factory->getRecordId(), 0u);
    EXPECT_EQ(factory->getFootprintIndex(), 0);
    EXPECT_TRUE(factory->isOrigin());
    EXPECT_EQ(factory->getEntryIndex(), 0);
    EXPECT_EQ(factory->getFactoryFlags(), 0);
}

TEST(FactoryElementTests, PayloadRoundTrips)
{
    TileElement element{};
    element.clearAs(TileElementType::factory);
    auto* factory = element.as<FactoryElement>();
    ASSERT_NE(factory, nullptr);

    factory->setSubtype(FactoryElementSubtype::machine);
    factory->setRecordId(0x01020304u);
    factory->setFootprintIndex(5);
    factory->setEntryIndex(0xBEEF);
    factory->setConnectionCache(0x5A);
    factory->setFactoryFlag(FACTORY_ELEMENT_FLAG_WORKING, true);
    factory->setFactoryFlag(FACTORY_ELEMENT_FLAG_DAMAGED, true);
    factory->setFactoryFlag(FACTORY_ELEMENT_FLAG_DAMAGED, false);

    EXPECT_EQ(factory->getSubtype(), FactoryElementSubtype::machine);
    EXPECT_EQ(factory->getRecordId(), 0x01020304u);
    EXPECT_TRUE(factory->hasRecord());
    EXPECT_EQ(factory->getFootprintIndex(), 5);
    EXPECT_FALSE(factory->isOrigin());
    EXPECT_EQ(factory->getEntryIndex(), 0xBEEF);
    EXPECT_EQ(factory->getConnectionCache(), 0x5A);
    EXPECT_TRUE(factory->hasFactoryFlag(FACTORY_ELEMENT_FLAG_WORKING));
    EXPECT_FALSE(factory->hasFactoryFlag(FACTORY_ELEMENT_FLAG_DAMAGED));

    // The base bytes (type, flags, heights, owner) must be untouched by payload writes.
    EXPECT_EQ(element.getType(), TileElementType::factory);
    EXPECT_EQ(element.baseHeight, kMinimumLandHeight);
    EXPECT_EQ(element.clearanceHeight, kMinimumLandHeight);
    EXPECT_FALSE(element.isGhost());

    // Raw layout: payload byte offsets are part of the save format and must not move.
    const auto* raw = reinterpret_cast<const uint8_t*>(&element);
    EXPECT_EQ(raw[5], EnumValue(FactoryElementSubtype::machine));
    EXPECT_EQ(raw[6], 0x04);
    EXPECT_EQ(raw[7], 0x03);
    EXPECT_EQ(raw[8], 0x02);
    EXPECT_EQ(raw[9], 0x01);
    EXPECT_EQ(raw[10], 5);
    EXPECT_EQ(raw[11], 0xEF);
    EXPECT_EQ(raw[12], 0xBE);
    EXPECT_EQ(raw[13], 0x5A);
    EXPECT_EQ(raw[14], FACTORY_ELEMENT_FLAG_WORKING);
}

TEST(FactoryElementTests, GhostCarriesNullRecord)
{
    TileElement element{};
    element.clearAs(TileElementType::factory);
    auto* factory = element.as<FactoryElement>();
    ASSERT_NE(factory, nullptr);

    element.setGhost(true);
    factory->setRecordId(kFactoryRecordNull);
    EXPECT_TRUE(element.isGhost());
    EXPECT_FALSE(factory->hasRecord());
}
