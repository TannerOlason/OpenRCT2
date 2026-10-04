/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Loads the shipped content pack through the object repository.

#include <gtest/gtest.h>
#include <memory>
#include <openrct2/Context.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/core/EnumUtils.hpp>
#include <openrct2/drawing/Drawing.Sprite.h>
#include <openrct2/factory/FactoryPrototypeObject.h>
#include <openrct2/object/ObjectList.h>
#include <openrct2/object/ObjectManager.h>
#include <openrct2/object/ObjectRepository.h>
#include <openrct2/object/ObjectTypes.h>

using namespace OpenRCT2;
using namespace OpenRCT2::Factory;

class FactoryPrototypeTests : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        gOpenRCT2Headless = true;
        // Images are only decoded when graphics are enabled; the image pool itself works headless on Linux
        // but a Windows headless context will not initialise with graphics on, so images are checked only
        // where they load.
#ifndef _WIN32
        gOpenRCT2NoGraphics = false;
#endif
        _context = CreateContext();
        ASSERT_TRUE(_context->Initialise());
    }

    static void TearDownTestCase()
    {
        _context.reset();
        gOpenRCT2NoGraphics = true;
    }

    static FactoryPrototypeObject* Load(const char* identifier)
    {
        auto& objectManager = _context->GetObjectManager();
        auto* object = objectManager.LoadObject(identifier);
        return dynamic_cast<FactoryPrototypeObject*>(object);
    }

    static std::shared_ptr<IContext> _context;
};

std::shared_ptr<IContext> FactoryPrototypeTests::_context;

TEST_F(FactoryPrototypeTests, ObjectTypeIsRegisteredAndTransient)
{
    EXPECT_TRUE(ObjectTypeIsTransient(ObjectType::factoryPrototype));
    EXPECT_EQ(getObjectEntryGroupCount(ObjectType::factoryPrototype), 8192u);
    EXPECT_EQ(parsePrototypeKind("belt"), PrototypeKind::belt);
    EXPECT_EQ(parsePrototypeKind("nonsense"), PrototypeKind::count);
    EXPECT_EQ(prototypeKindName(PrototypeKind::container), "container");
    EXPECT_EQ(subtypeForKind(PrototypeKind::item), FactoryElementSubtype::count);
    EXPECT_EQ(subtypeForKind(PrototypeKind::inserter), FactoryElementSubtype::inserter);
}

TEST_F(FactoryPrototypeTests, ContentPackIsIndexed)
{
    auto& repository = _context->GetObjectRepository();
    EXPECT_NE(repository.FindObject("factory-tour.factory_prototype.iron_plate"), nullptr);
    EXPECT_NE(repository.FindObject("factory-tour.factory_prototype.belt_basic"), nullptr);
    EXPECT_NE(repository.FindObject("factory-tour.factory_prototype.inserter_basic"), nullptr);
    EXPECT_NE(repository.FindObject("factory-tour.factory_prototype.chest_wooden"), nullptr);
}

TEST_F(FactoryPrototypeTests, BeltLoadsWithImagesAndProperties)
{
    auto* belt = Load("factory-tour.factory_prototype.belt_basic");
    ASSERT_NE(belt, nullptr);
    EXPECT_EQ(belt->getKind(), PrototypeKind::belt);
    EXPECT_TRUE(belt->isPlaceable());
    EXPECT_EQ(belt->getSubtype(), FactoryElementSubtype::belt);
    EXPECT_EQ(belt->getBelt().speed, 12);
    EXPECT_EQ(belt->getBelt().frames, 8);
    EXPECT_EQ(belt->getClearance(), 2);
    if (!gOpenRCT2NoGraphics)
    {
        EXPECT_EQ(belt->getNumLoadedImages(), 3u * 4u * 8u);
        EXPECT_TRUE(belt->hasImages());
    }
    if (!belt->hasImages())
        return;

    auto base = belt->getBeltImage(BeltShape::straight, 0, 0);
    EXPECT_NE(base, kImageIndexUndefined);
    EXPECT_EQ(belt->getBeltImage(BeltShape::straight, 1, 0), base + 8);
    EXPECT_EQ(belt->getBeltImage(BeltShape::turnLeft, 0, 3), base + 32 + 3);
    EXPECT_EQ(belt->getBeltImage(BeltShape::turnRight, 3, 7), base + 64 + 24 + 7);
    EXPECT_EQ(belt->getBeltImage(BeltShape::straight, 0, 8), base); // frames wrap
    EXPECT_EQ(belt->GetName(), "Basic transport belt");

    // The imported sprite keeps its size and the anchor from object.json.
    const auto* g1 = GfxGetG1Element(base);
    ASSERT_NE(g1, nullptr);
    EXPECT_EQ(g1->width, 64);
    EXPECT_EQ(g1->height, 34);
    EXPECT_EQ(g1->xOffset, -32);
    EXPECT_EQ(g1->yOffset, -2);
    EXPECT_NE(g1->offset, nullptr);

    auto& objectManager = _context->GetObjectManager();
    auto index = objectManager.GetLoadedObjectEntryIndex(belt);
    EXPECT_NE(index, kObjectEntryIndexNull);
    EXPECT_EQ(objectManager.GetLoadedObject<FactoryPrototypeObject>(index), belt);
}

TEST_F(FactoryPrototypeTests, OtherKindsLoad)
{
    auto* item = Load("factory-tour.factory_prototype.iron_plate");
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->getKind(), PrototypeKind::item);
    EXPECT_FALSE(item->isPlaceable());
    EXPECT_EQ(item->getItem().stackSize, 100);
    if (item->hasImages())
    {
        EXPECT_NE(item->getItemIconImage(), kImageIndexUndefined);
        EXPECT_EQ(item->getItemBeltImage(), item->getItemIconImage() + 1);
    }

    auto* inserter = Load("factory-tour.factory_prototype.inserter_basic");
    ASSERT_NE(inserter, nullptr);
    EXPECT_EQ(inserter->getInserter().frames, 8);
    EXPECT_EQ(inserter->getInserter().swingTicks, 24);
    if (inserter->hasImages())
    {
        EXPECT_EQ(inserter->getInserterImage(2, 5), inserter->getInserterImage(0, 0) + 2 * 8 + 5);
        EXPECT_EQ(inserter->getInserterImage(0, 99), inserter->getInserterImage(0, 7)); // clamps
    }

    auto* chest = Load("factory-tour.factory_prototype.chest_wooden");
    ASSERT_NE(chest, nullptr);
    EXPECT_EQ(chest->getContainer().slots, 16);
    EXPECT_EQ(chest->getContainerImage(3), chest->getContainerImage(0));
    EXPECT_EQ(chest->getPrice(), 30);
}
