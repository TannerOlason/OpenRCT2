<!-- Research note produced 2026-10-03 against upstream commit 5d86c6b. Line numbers drift as upstream moves; verify before editing. -->

# Integration brief: adding `ObjectType::factoryPrototype` ("factory_prototype", cap 8192, transient)

Repo: /home/user/Documents/Projects/factory-tour (branch `factory-tour/main`, HEAD c789149677). Fork rules from CLAUDE.md that apply here:
- Put `// FACTORY-TOUR:` on the line above every edit to an upstream file.
- Append to enums only, never insert in the middle.
- Add new files to `src/openrct2/libopenrct2.vcxproj`. ClimateObject is listed at line 358 `<ClInclude Include="object\ClimateObject.h" />` and line 995 `<ClCompile Include="object\ClimateObject.cpp" />`. CMake picks files up through GLOB_RECURSE.
- Do not bump `kParkFileCurrentVersion`.
- Relevant docs: CONTEXT.md reserved id table (line 188: `ObjectType | factoryPrototype (one type, properties.kind), cap 8192`), docs/adr/0005-one-prototype-object-type-with-a-kind-field.md, and wiki/SPEC.md lines 37, 102, 149 and 153 ("Fork content under `data/factory/objects/**` via an extra `ObjectRepository` root").

---
## Problems found that you must handle (read first)

1. **Index bug for slots ≥ 2047.** `ObjectGetTypeEntryIndex` (src/openrct2/object/ObjectList.cpp:212-232) walks `kObjectEntryGroupCounts`, subtracting each group count in turn:
```
    uint8_t objectType = EnumValue(ObjectType::ride);
    for (size_t groupCount : kObjectEntryGroupCounts)
    { if (index >= groupCount) { index -= groupCount; objectType++; } else break; }
```
   Its only caller is `ObjectManager::GetLoadedObjectEntryIndex(const Object*)` (src/openrct2/object/ObjectManager.cpp:137-146), which passes an index that is *already per-type*: `size_t index = GetLoadedObjectIndex(object); if (index != SIZE_MAX) { ObjectGetTypeEntryIndex(index, nullptr, &result); }`. Today the largest cap is 2047, so the bug never shows. A factoryPrototype in slot 2047..8191 would get a wrong index (for example 2047 becomes 0, 3000 becomes 953).
   - Fix: a FACTORY-TOUR touch point at ObjectManager.cpp:143 that uses `result = static_cast<ObjectEntryIndex>(index);` for factoryPrototype, or for all types.
   - Every `GetLoadedObjectEntryIndex(obj)` / `ObjectManagerGetLoadedObjectEntryIndex` path depends on this, including scripting `ScObjectManager::load`.

2. **Crash in the editor's in-use scan, introduced by the previous commit.** In src/openrct2/scenes/editor/EditorController.cpp:355-366 the `default:` label sits on top of the surface case:
```
            switch (iter.element->getType())
            {
                default:
                case TileElementType::surface:
                {
                    auto surfaceEl = iter.element->asSurface();
                    auto surfaceIndex = surfaceEl->getSurfaceObjectIndex();
```
   A `TileElementType::factory` (9) element falls into this case. `asSurface()` then returns nullptr (TileElementBase.h:100-105, `as<>` returns nullptr when the type doesn't match), and the next line dereferences it. This runs from `SetupInUseSelectionFlags`, i.e. object selection or "remove unused objects" in the editor.
   - Fix: add `case TileElementType::factory: Editor::SetSelectedObject(ObjectType::factoryPrototype, iter.element->asFactory()->getEntryIndex(), ObjectSelectionFlag::inUse); break;`
   - FactoryElement stores the entry as `ObjectEntryIndex entry; // 11..12` (FactoryElement.h:59). Its accessors are `getEntryIndex()`/`setEntryIndex()` (lines 78-79) and `asFactory()` is at TileElementBase.h:125-126.

3. **Object string id pool.** Object strings come from `kBaseObjectStringID = 0x2000; kMaxObjectCachedStrings = 0x5000 - kBaseObjectStringID;` (src/openrct2/localisation/LocalisationService.cpp:23-24), which is about 12288 ids.
   - With 8192 prototypes, do **not** call `LocalisationService::AllocateObjectString` per prototype. Use `Object::GetName()` / `GetString()`, which return `std::string`.
   - Off-by-one: the constructor loop `for (StringId stringId = kBaseObjectStringID + kMaxObjectCachedStrings; stringId >= kBaseObjectStringID; stringId--)` (line 29) also pushes 0x5000. That is the first id of the fork's reserved range 0x5000–0x9FFF. It is popped last, so in practice it rarely matters, but either start fork ids at 0x5001 or fix the loop.

4. **Only fixed string keys are read.** `StringTable::ParseStringId` (src/openrct2/object/StringTable.cpp:85-100) accepts only `name`, `description`, `park_name`, `details`, `capacity` and `vehicleName`; every other key under `"strings"` is silently dropped (line 112 `if (stringId != ObjectStringID::unknown)`). If prototypes need more text fields, extend `ObjectStringID` (StringTable.h:23-33) and the parser (a touch point), or read them yourself from `root["properties"]`.

5. **Object index cache must be bumped.** `kVersion` in src/openrct2/object/ObjectRepository.cpp:73 is `static constexpr uint16_t kVersion = 31;`. Bump it to 32.
   - Why: `FileIndex::ReadIndexFile` (src/openrct2/core/FileIndex.hpp:229-233) reuses objects.idx when file count, size, mtime and path checksums are unchanged. An index built by a binary that rejected `"factory_prototype"` simply leaves those files out, and they would stay missing.
   - Bumping is also required if you add a per-type field to `ObjectRepositoryItem` and its `Serialise` switch.

6. **Do not use `data/object/` in the repo.** CMakeLists.txt:459-468 (and 466 `SKIP_IF_EXISTS ${CMAKE_SOURCE_DIR}/data/object/`) skips the objects.zip download whenever repo `data/object/` exists. Use `data/factory/objects/` plus an extra scan root (see §8).

7. **Packing into saves.** `GetPackableObjects` (ObjectManager.cpp:281-294) packs every loaded object for which `IsObjectCustom` is true. A missing `"sourceGame"` defaults to custom (ObjectFactory.cpp:511-515), and `IsObjectCustom` (ObjectRepository.cpp:611-627) returns false only for the official/rct sources. So prototypes without `"sourceGame": "official"` (or another non-custom value) are zipped into every network map and save (ParkFile.cpp:911-931; only `.parkobj` and `.dat` are packable). Set `"sourceGame": "official"` on shipped content.

8. **Static asserts and array sizes that break if you add the enum value without updating them:**
   - ObjectTypes.cpp:42 `static_assert(kAllObjectTypes.size() == EnumValue(ObjectType::count));`
   - ObjectTypes.cpp:59 `static_assert(kNumTransientObjectTypes + kNumIntransientObjectTypes == static_cast<size_t>(ObjectType::count));`
   - ObjectTypes.cpp:45 `std::array<const ObjectType, kNumTransientObjectTypes>`: the size comes from the constant in ObjectTypes.h:51, so bump it to 20.
   - ObjectList.cpp:24/41: `std::array<int32_t, EnumValue(ObjectType::count)> kObjectEntryGroupCounts` plus a static_assert. With one entry too few, the array value-initialises the last entry to 0 and the assert still passes, so the new type would silently get a cap of 0. Add the entry explicitly.
   - InteractiveConsole.cpp:1144 `static_assert(_objectTypeNames.size() == EnumValue(ObjectType::count));`
   - ParkInfoCommands.cpp:95-99: `typeToName` stops at "FootpathRailings" (index 16). Only add factoryPrototype to the loop if you also pad the array up to index 21, otherwise it reads out of bounds.

9. **`-Werror=switch` is not a risk.** Every `switch` over ObjectType has a `default:`: ObjectFactory.cpp:417, ObjectRepository.cpp:148, ObjectLoadError.cpp:359, ScObjectManager.cpp:340, EditorObjectSelection.cpp:1554, EditorController.cpp:595, Research.cpp:554, SceneryGroupObject.cpp:91, S4Importer.cpp:600/2609, TrackDesign.cpp, Scenery.cpp. Only the factory switch actually needs a new case.

---
## 1. src/openrct2/object/ObjectTypes.h / .cpp

ObjectTypes.h:23-52:
```
    enum class ObjectType : uint8_t
    {
        ride, ... (lines 25-43)
        peepAnimations,   // 44
        climate,          // 45
                          // ADD: factoryPrototype, (= 21)
        count,            // 47
        none = 255
    };
    static constexpr size_t kNumTransientObjectTypes = 19;   // line 51 -> 20
    static constexpr size_t kNumIntransientObjectTypes = 2;  // line 52 unchanged
```
- Line 22 has the comment `// First 0xF of RCTObjectEntry->flags`. `RCTObjectEntry::GetType()` (Object.h:76-79) masks with `& 0x0F`, so a value of 21 cannot be stored in a DAT entry.
- That is harmless for JSON objects: `ObjectEntryDescriptor::GetType()` returns `Type` when `Generation == json` (Object.cpp:75-77).
- If a prototype JSON carries an `originalId`, ObjectFactory.cpp:569 logs an error and ignores the override.

ObjectTypes.cpp:
- line 38-39 `ObjectType::peepAnimations,` / `ObjectType::climate,` inside `constexpr std::array kAllObjectTypes = {` (18-40). Append `ObjectType::factoryPrototype,`.
- lines 45-51:
```
    static constexpr std::array<const ObjectType, kNumTransientObjectTypes> kTransientObjectTypes = {
        ...
        ObjectType::peepNames,    ObjectType::peepAnimations, ObjectType::climate,
    };
```
  Append factoryPrototype here.
- lines 54-57 `kIntransientObjectTypes = { ObjectType::scenarioMeta, ObjectType::audio, };` stays unchanged.
- Separately, Object.h:342-345 has `constexpr bool IsIntransientObjectType(ObjectType type) { return type == ObjectType::audio; }`. ObjectManager uses this one for unloading. No change needed.

## 2. ObjectLimits.h and ObjectList.cpp

src/openrct2/object/ObjectLimits.h:37-38:
```
    constexpr uint16_t kMaxPeepAnimationsObjects = 255;
    constexpr uint16_t kMaxClimateObjects = 1;
```
Add `constexpr uint16_t kMaxFactoryPrototypeObjects = 8192;`. It fits in uint16, and `ObjectEntryIndex = uint16_t` with null = 0xFFFF (ObjectTypes.h:19-20).

src/openrct2/object/ObjectList.cpp:24-54:
```
    static constexpr std::array<int32_t, EnumValue(ObjectType::count)> kObjectEntryGroupCounts = {
        kMaxRideObjects, ... 
        kMaxAudioObjects,          kMaxPeepNamesObjects,       kMaxPeepAnimationsObjects,
        kMaxClimateObjects,                                    // line 39 -> add kMaxFactoryPrototypeObjects,
    };
    static_assert(std::size(kObjectEntryGroupCounts) == EnumValue(ObjectType::count));

    size_t getObjectEntryGroupCount(ObjectType objectType) { return kObjectEntryGroupCounts[EnumValue(objectType)]; }
    size_t getObjectTypeLimit(ObjectType type) { auto index = EnumValue(type); if (index >= EnumValue(ObjectType::count)) return 0; return ...; }
```
- Both lookups index this array directly; they are declared in ObjectList.h:57-58.
- `ObjectList` stores `std::vector<std::vector<ObjectEntryDescriptor>> _subLists`, and `GetList(type)` grows lazily (lines 123-131). No per-type code.
- See problem 1 for `ObjectGetTypeEntryIndex` (lines 212-232).

These consumers loop `0..getObjectEntryGroupCount` (8192 iterations is fine): ObjectManager.cpp:151-162 (`GetLoadedObjects`), 546-574 (`GetRequiredObjects`), 412 (`FindSpareSlot`), 88 (`GetLoadedObject` bounds check); Context.cpp:954 `HasObjectsThatUseFallbackImages`; EditorController.cpp:335-347 `SetupInUseSelectionFlags`; EditorInventionsList.cpp:88-95; ScObjectManager.cpp:262 `getAllObjects`; InteractiveConsole.cpp:1148.

## 3. ObjectFactory / ObjectRepository / ObjectManager / ObjectEntryManager / object class pattern

**src/openrct2/object/ObjectFactory.cpp**
- Includes are alphabetical at lines 28-50 (`#include "ClimateObject.h"` is line 30, `#include "PeepAnimationsObject.h"` is line 40). Add `#include "FactoryPrototypeObject.h"`, or the fork path `"../factory/FactoryPrototypeObject.h"` if the class lives in `src/openrct2/factory/`.
- `CreateObject(ObjectType type)` switch, lines 349-421:
```
            case ObjectType::peepAnimations:
                result = std::make_unique<PeepAnimationsObject>();
                break;
            case ObjectType::climate:
                result = std::make_unique<ClimateObject>();
                break;
            default:
                throw std::runtime_error("Invalid object type");
```
  Add a case before `default` (line 417).
- `static const EnumMap<ObjectType> kObjectTypeMap` (JSON `objectType` names), lines 423-445:
```
        { "peep_animations", ObjectType::peepAnimations },
        { "climate", ObjectType::climate },
    };
```
  Add `{ "factory_prototype", ObjectType::factoryPrototype },`. EnumMap (core/EnumMap.hpp) sorts entries itself and has no ordering or contiguity requirement.
- `CreateObjectFromJson`, lines 529-613:
  - looks up `jRoot["objectType"]` (541); unknown types return nullptr, which is how older binaries skip these files;
  - then calls `CreateObject` (585), `SetIdentifier`/`SetDescriptor`/`MarkAsJsonObject`, then `result->ReadJson(&readContext, jRoot)` (592);
  - throws if `readContext.WasError()`;
  - reads `authors`, `isCompatibilityObject`, `sourceGame`.
- The only DAT-specific paths are `CreateObjectFromLegacyFile` and `CreateObjectFromLegacyData` (278-347). Neither needs changes.

**src/openrct2/object/ObjectRepository.cpp**
- line 72-74:
```
        static constexpr uint32_t kMagicNumber = 0x5844494F; // OIDX
        static constexpr uint16_t kVersion = 31;
        static constexpr auto kPattern = "*.dat;*.pob;*.json;*.parkobj";
```
- Per-type index data, `Serialise` (lines 118-152):
```
            switch (item.Type)
            {
                case ObjectType::ride: ds << item.RideInfo.RideFlags; ds << item.RideInfo.RideType; break;
                case ObjectType::sceneryGroup: { ds << item.SceneryGroupInfo.Entries; break; }
                case ObjectType::footpathSurface: ds << item.FootpathSurfaceInfo.Flags; break;
                case ObjectType::peepAnimations: ds << item.PeepAnimationsInfo.PeepType; break;
                default: break;
            }
```
  The matching struct members are in ObjectRepository.h:43-59, e.g. `struct { uint8_t PeepType{}; } PeepAnimationsInfo;`.
  - These are filled by the object's `SetRepositoryItem(ObjectRepositoryItem*) const override`, which is called at ObjectRepository.cpp:113. PeepAnimationsObject.h:60 overrides it.
  - If the editor needs `kind` without loading the object, add `struct { uint8_t Kind{}; } FactoryPrototypeInfo;` plus a Serialise case, and bump kVersion.

**src/openrct2/object/ObjectManager.cpp**
- line 61: `std::array<std::vector<Object*>, EnumValue(ObjectType::count)> _loadedObjects;` sizes itself from `count`.
- There is no per-type load order. Every loop uses `getAllObjectTypes()` (151, 338, 470, 546, 706).
- The only type-specific code is ride/sceneryGroup handling (396, 510-516, 758). Problem 1 (line 143) is the one thing to change.
- Load sequence: `GetOrLoadObject` (728-747) calls `_objectRepository.LoadObject(ori)` → `object->Load()` → `RegisterLoadedObject`. Unloading calls `object->Unload()` (445).
- Typed access goes through `template<typename TClass> TClass* GetLoadedObject(size_t index) { return static_cast<TClass*>(GetLoadedObject(TClass::kObjectType, index)); }` (ObjectManager.h:37-41), so define `static constexpr ObjectType kObjectType = ObjectType::factoryPrototype;` in your class.

**src/openrct2/object/ObjectEntryManager.{h,cpp}**
- Generic: `GetObjectEntry(type, idx)` returns `object->GetLegacyData()`, and the `GetObjectEntry<T>` template uses `T::kObjectType`. No per-type table.
- Only relevant if you override `GetLegacyData()` to expose a POD entry struct, as WaterObject.h:23-25 does.

**Class pattern.** Full src/openrct2/object/ClimateObject.h:
```
#pragma once

#include "../world/Weather.h"
#include "Object.h"

namespace OpenRCT2
{

    using YearlyDistribution = std::array<uint8_t, EnumValue(Weather::Type::count)>;

    class ClimateObject final : public Object
    {
    private:
        Weather::Climate _climate;
        std::string _scriptName;

    public:
        static constexpr ObjectType kObjectType = ObjectType::climate;

        void ReadJson(IReadObjectContext* context, json_t& root) override;
        void Load() override;
        void Unload() override;

        void DrawPreview(Drawing::RenderTarget& rt, int32_t width, int32_t height) const override;

        const Weather::TemperatureThresholds& getItemThresholds() const;
        const Weather::Pattern& getPatternForMonth(uint8_t month) const;
        std::string getScriptName() const;
        YearlyDistribution getYearlyDistribution() const;
    };
} // namespace OpenRCT2
```

ClimateObject.cpp, no images (lines 48-54, 79-93):
```
    void ClimateObject::Load() {}
    void ClimateObject::Unload() {}
    void ClimateObject::ReadJson(IReadObjectContext* context, json_t& root)
    {
        Guard::Assert(root.is_object(), "ClimateObject::ReadJson expects parameter root to be an object");
        PopulateTablesFromJson(context, root);
        ...
        _scriptName = Json::GetString(root["scriptName"], std::string(GetIdentifier()));
        Guard::Assert(root["properties"].is_object(), "ClimateObject::ReadJson expects properties key to be an object");
        _climate.itemThresholds.cold = Json::GetNumber(root["properties"]["coldItemTempThreshold"], 11);
```

PeepAnimationsObject.cpp shows the image pattern (lines 32-38, 62-73):
```
    void PeepAnimationsObject::Load()
    {
        auto numImages = GetImageTable().GetCount();
        if (numImages == 0) return;
        _imageOffsetId = LoadImages();     // base ImageIndex; image i = _imageOffsetId + i
        ...
    }
    void PeepAnimationsObject::Unload() { UnloadImages(); }
    void PeepAnimationsObject::ReadJson(IReadObjectContext* context, json_t& root)
    {
        Guard::Assert(root.is_object(), ...);
        PopulateTablesFromJson(context, root);
        Guard::Assert(root["properties"].is_object(), ...);
        ReadProperties(root["properties"]);
```
Its header (PeepAnimationsObject.h:22-61) has the same shape: `final : public Object`, `static constexpr ObjectType kObjectType = ObjectType::peepAnimations;`, ReadJson/Load/Unload, `DrawPreview`, and `void SetRepositoryItem(ObjectRepositoryItem* item) const override;`.

Base-class helpers (src/openrct2/object/Object.h / Object.cpp):
- `void PopulateTablesFromJson(IReadObjectContext* context, json_t& root);` (protected, Object.h:198). Implementation at Object.cpp:155-159: `_stringTable.ReadJson(root); _usesFallbackImages = _imageTable.ReadJson(context, root);`.
- `StringTable::ReadJson` (StringTable.cpp:102-126) reads `root["strings"]` as `{ "<key>": { "<locale>": "text" } }`. See problem 4 for the allowed keys.
- Getters: `std::string GetString(ObjectStringID)` (protected), `virtual std::string GetName() const` (returns `GetString(ObjectStringID::name)`, Object.cpp:203-206), and `GetString(int32_t language, ObjectStringID)`.
- `ImageTable::ReadJson` (ImageTable.cpp:527+) reads `root["images"]` only when `context->ShouldLoadImages()`. Entries can be strings (`"$G1[..]"`, `"$CSG.."`, file paths) or objects (`{"path": "...", "x":..,"y":..}` / `{"gx": ...}`). Paths resolve through IFileDataRetriever: the zip for `.parkobj`, otherwise the directory of object.json (ObjectFactory.cpp:480).
- `uint32_t LoadImages()` / `void UnloadImages()` (Object.cpp:213-229) allocate from the dynamic image pool (`GfxObjectAllocateImages`). Also available: `GetBaseImageId()`, `GetImageTable()`, `GetNumImages()`.
- The repository index builds objects with `loadImages=false` (ObjectRepository.cpp:90), so `ReadJson` must cope with an empty image table.

## 4. park/ParkFile.cpp objects chunk

Write side, lines 521-557. Generic over `getTransientObjectTypes()`:
```
                os.readWriteChunk(ParkFileChunkType::objects, [](OrcaStream::ChunkStream& cs) {
                    auto& objManager = GetContext()->GetObjectManager();
                    auto objectList = objManager.GetLoadedObjects();
                    cs.write(static_cast<uint16_t>(getTransientObjectTypes().size()));
                    for (auto objectType : getTransientObjectTypes())
                    {
                        const auto& list = objectList.GetList(objectType);
                        cs.write(static_cast<uint16_t>(objectType));
                        cs.write(static_cast<uint32_t>(list.size()));
                        for (const auto& entry : list) { ... kDescriptorJson: identifier + VersionString ... }
```

Read side, lines 353-487. The type id comes from the stream and every descriptor is stored:
```
                    auto numSubLists = cs.read<uint16_t>();
                    for (size_t i = 0; i < numSubLists; i++)
                    {
                        auto objectType = static_cast<ObjectType>(cs.read<uint16_t>());
                        auto subListSize = static_cast<ObjectEntryIndex>(cs.read<uint32_t>());
                        ...
                            case kDescriptorJson: { ObjectEntryDescriptor desc; desc.Type = objectType; ... requiredObjects.SetObject(j, desc);
```
Conclusions:
- Adding the type to `kTransientObjectTypes` is enough. No `kParkFileCurrentVersion` (ParkFile.h:22, `= 63`) change is needed or allowed.
- Old fork saves simply lack the sublist.
- An upstream build reading a fork save stores the type-21 sublist in `ObjectList`, but `GetRequiredObjects` iterates only its own `getAllObjectTypes()`, so it silently ignores those objects.
- A missing prototype object on load raises `ObjectLoadException`, which is reported through the ObjectLoadError window.
- Version-gated `AppendRequiredObjects` blocks (496-517) exist only for migrations; nothing is needed for the new type.
- Packed objects: see problem 7 (lines 849-942).
- If the network map format changes, CLAUDE.md says to bump `kStreamVersion` (src/openrct2/network/NetworkBase.cpp:50, `constexpr uint8_t kStreamVersion = 2;`) and `kReplayVersion` (ReplayManager.cpp:109, `= 11`).

## 5. Scripting

- src/openrct2/scripting/bindings/object/ScInstalledObject.cpp:18-40, `static const EnumMap<ObjectType> kObjectTypeMap` used by `objectTypeToString`/`objectTypeFromString`:
```
        { "peep_names", ObjectType::peepNames },
        { "peep_animations", ObjectType::peepAnimations },
        { "climate", ObjectType::climate },
    };
```
  Add `{ "factory_prototype", ObjectType::factoryPrototype },`. ScObjectManager.cpp uses this map (lines 188, 236, 257) and so does ScResearch.cpp:251.
- ScObjectManager.cpp:304-345 `CreateScObject` switch has `default: return ScObject::New(ctx, type, index);`. A new type works without change; add a case only for a dedicated `ScFactoryPrototypeObject`.
- distribution/scripting/openrct2.d.ts:634-655:
```
    type ObjectType =
        "ride" |
        ...
        "peep_animations" |
        "climate";
```
  Change the last line to `"climate" |` and add `"factory_prototype";`. Upstream also has a mismatch here: d.ts says `"scenario_text"` (line 645) while C++ uses `"scenario_meta"`. Leave it.
- Bump `kPluginApiVersion` (src/openrct2/scripting/ScriptEngine.h:46, `static constexpr int32_t kPluginApiVersion = 124;`) and keep that exact format.

## 6. src/openrct2-ui

**EditorObjectSelection.cpp**
- Sub-tab and page tables, lines 124-193:
```
    struct ObjectSubTab { StringId tooltip; ObjectType subObjectType; uint16_t flagFilter; uint32_t baseImage; uint8_t animationLength; uint8_t animationDivisor; };
    struct ObjectPageDesc { StringId Caption; ObjectType mainObjectType; uint32_t Image; std::span<ObjectSubTab> subTabs; };
    ...
    static ObjectSubTab kTerrainObjectSubTabs[] = {
        ...
        { STR_OBJECT_SELECTION_CLIMATE,           ObjectType::climate,          FILTER_NONE, SPR_WEATHER_SUN_CLOUD,   1, 1 },
    };
    static ObjectSubTab kPeepObjectSubTabs[] = {
        { STR_OBJECT_SELECTION_PEEP_ANIMATIONS,   ObjectType::peepAnimations,   FILTER_NONE, SPR_G2_PEEP_ANIMATIONS, 1, 1 },
        { STR_OBJECT_SELECTION_PEEP_NAMES,        ObjectType::peepNames,        FILTER_NONE, SPR_TAB_GUESTS_0,       1, 1 },
    };
    static constexpr ObjectPageDesc ObjectSelectionPages[] = {
        { STR_OBJECT_SELECTION_RIDE_VEHICLES_ATTRACTIONS, ObjectType::ride, SPR_TAB_RIDE_16, kRideObjectSubTabs },
        ...
        { STR_OBJECT_SELECTION_MUSIC, ObjectType::music, SPR_TAB_MUSIC_0, {} },
        { STR_OBJECT_SELECTION_GUESTS_AND_STAFF, ObjectType::peepNames, SPR_TAB_GUESTS_0, kPeepObjectSubTabs },   // line 192
    };
```
- To add a page, append `{ STR_OBJECT_SELECTION_FACTORY_PROTOTYPES, ObjectType::factoryPrototype, <sprite>, {} },`, or give it sub-tabs (at most 7, `WIDX_SUB_TAB_0..6`, line 955 `for (int8_t i = 0; i <= 6; i++)`). Sub-tabs could filter by `kind` through a custom flag.
- Main tab widgets are cloned dynamically per page (`initWidgets`, lines 1130-1142) and laid out 31 px apart (916-926), so there is no fixed widget limit.
- `GoToTab(ObjectType)` (1116-1126) matches `mainObjectType`. The list shows repository items with `item->Type == GetSelectedObjectType()` (1187).
- Track designer and manager hide pages 1 and up (930-936).
- Type-specific UI: 1362 (ride), 1369 (`if (GetSelectedObjectType() == ObjectType::peepAnimations)`), 1282-1311, and `ObjectGetDescription` switch 1547-1556 with a default.
- The selection counter shows `numSelected/getObjectEntryGroupCount` (1066-1067).

**ObjectLoadError.cpp:313-362**, `GetStringFromObjectType`:
```
            case ObjectType::peepAnimations:
                return STR_OBJECT_SELECTION_PEEP_ANIMATIONS;
            case ObjectType::climate:
                return STR_OBJECT_SELECTION_CLIMATE;
            // Intransient objects, should never pop up here
            case ObjectType::scenarioMeta: ...
            case ObjectType::audio:
            default:
                return STR_UNKNOWN_OBJECT_TYPE;
```
Add a case, otherwise missing prototypes display as "unknown object type".

Other UI enumerations:
- EditorInventionsList.cpp:88 loops `getTransientObjectTypes()`; automatic.
- Footpath.cpp and Scenery.cpp use only specific types.
- Grep of `ObjectType::climate|peepAnimations|count` over src/ found nothing else in openrct2-ui.

## 7. Engine-side tables and switches

**src/openrct2/interface/InteractiveConsole.cpp:1121-1144** (static_assert):
```
constexpr auto _objectTypeNames = std::to_array<StringId>({
    STR_OBJECT_SELECTION_RIDE_VEHICLES_ATTRACTIONS,
    ...
    STR_OBJECT_SELECTION_PEEP_ANIMATIONS,
    STR_OBJECT_SELECTION_CLIMATE,
});
static_assert(_objectTypeNames.size() == EnumValue(ObjectType::count));
```
Append the new string id.

**src/openrct2/scenes/editor/EditorController.cpp**
- Arrays sized by `count`, automatic: lines 60, 62, 64 (`_numSelectedObjectsForType`, `_numAvailableObjectsForType`, `_editorSelectedObjectFlags`). EditorController.h:46 has the extern.
- `CheckObjectSelection` (96-163): `kBasicCheckPairs` (99-104) and the pairs at 129-135 hold "at least one X must be selected" rules. Add `{ ObjectType::factoryPrototype, STR_... }` only if prototypes become mandatory. Scenario-wide auto-selection would go in `kDefaultScenarioObjects` in `selectScenarioEditorObjects`.
- `SetupInUseSelectionFlags` tile switch at 355: see problem 2 (required fix).
- `RemoveUnusedObjects` (917-971) skip list:
```
                    // Avoid deleting climate objects, as they're not bound to entities.
                    if (objectType == ObjectType::climate)
                        continue;
```
  Lines 950-952. Consider also skipping `factoryPrototype`, because items and recipes are referenced from pools rather than tiles and the in-use scan cannot see them.
- `ObjectSelectionSelectObject` (739-857) enforces `maxObjects = getObjectEntryGroupCount(objectType)` and is otherwise generic.
- `SetSelectedObject` asserts `static_cast<size_t>(objectType) < getObjectEntryGroupCount(ObjectType::paths)` (line 235, i.e. < 255). 21 passes.

**src/openrct2/command_line/ParkInfoCommands.cpp:95-123** has a hardcoded `typeToName` array (ending `"TerrainEdge", "Station", "Music", "FootpathSurface", "FootpathRailings",`) and an explicit type loop ending at `ObjectType::footpathRailings`. It is optional; see problem 8.

**RCT1/RCT2 importers** (S6Importer.cpp:1859 loops `ride..water`; 1981/1984 and S4Importer.cpp:1575/1578 use `AppendRequiredObjects`). They have no tables over all types and need no changes.

**Others with ObjectType switches, all with `default:`**: management/Research.cpp:538, world/Scenery.cpp:400/443/546/565, ride/TrackDesign.cpp:497/781/980/1072, object/SceneryGroupObject.cpp:79. No tests reference `ObjectType::count` or `climate`.

## 8. How objects are found on disk

- src/openrct2/object/ObjectRepository.cpp:77-85:
```
        explicit ObjectFileIndex(IObjectRepository& objectRepository, const IPlatformEnvironment& env)
            : FileIndex(
                  "object index", kMagicNumber, kVersion, env.GetFilePath(PathId::cacheObjects), std::string(kPattern),
                  std::vector<std::string>{
                      env.GetDirectoryPath(DirBase::openrct2, DirId::objects),
                      env.GetDirectoryPath(DirBase::user, DirId::objects),
                  })
```
- `IsTrackReadOnly` (155-158) references `SearchPaths[0]` and `SearchPaths[1]`, so append the new root rather than inserting it.
- `FileIndex::Scan` (core/FileIndex.hpp:141-171) does a **recursive** `Path::scanDirectory(pattern, true)` on each search path, skipping empty paths. Each file goes through `ObjectFactory::CreateObjectFromFile`, which branches on extension:
  - `.json`: `CreateObjectFromJsonFile`. Images resolve relative to the json's directory, so a loose `folder/object.json` with `images/*.png` works. Any other `*.json` in the tree is attempted and rejected harmlessly (no valid `objectType`).
  - `.parkobj`: a zip that must contain `object.json` at its root (ObjectFactory.cpp:451-455).
  - `.dat` / `.pob`: legacy.
- Directory ids (PlatformEnvironment.h:31-54; names in PlatformEnvironment.cpp:41-63): `DirId::objects` maps to `u8"object"`, under `DirBase::openrct2` (= `Platform::GetInstallPath()` or `--openrct2-data-path`, PlatformEnvironment.cpp:260/277) and `DirBase::user`.
- In this tree, `build/data -> factory-tour-build/install/usr/local/share/openrct2`. It is populated by the install step, `install(DIRECTORY "data/" DESTINATION "${CMAKE_INSTALL_DATADIR}/openrct2")` (CMakeLists.txt:505). Repo `data/factory/objects/` therefore lands at `<openrct2 base>/factory/objects/`.

Recommended extra root, a single touch point in the initializer above:
```
                      env.GetDirectoryPath(DirBase::openrct2, DirId::objects),
                      env.GetDirectoryPath(DirBase::user, DirId::objects),
                      // FACTORY-TOUR: fork content pack
                      Path::Combine(env.GetDirectoryPath(DirBase::openrct2), u8"factory", u8"objects"),
```
`Path` is already available through `#include "../core/Path.hpp"` (line 24). Alternatives:
- Add a new `DirId` value. That would also need entries in both `kDirectoryNamesRCT2` and `kDirectoryNamesOpenRCT2`, and it is more invasive.
- Drop folders into the user dir `~/.config/OpenRCT2/object/`, which is fine for local testing.

Do not commit `data/object/` (problem 6). Remember to bump kVersion or run `build/openrct2-cli scan-objects` (problem 5).

## 9. String ids

- Engine ids live in src/openrct2/localisation/StringIds.h, e.g. line 1743 `STR_OBJECT_SELECTION_PEEP_ANIMATIONS = 6718,`, line 1754 `STR_OBJECT_SELECTION_CLIMATE = 6743,`, line 1504 `STR_UNKNOWN_OBJECT_TYPE = 6126,`. The highest id there is 7063.
- UI ids live in src/openrct2-ui/UiStringIds.h, e.g. 552 `STR_OBJECT_SELECTION_GUESTS_AND_STAFF = 6793,`, 554 `STR_OBJECT_SELECTION_SCENERY_AND_THEMES = 6794,`. The highest there is **7081**, which is also the last line of en-GB.txt (`STR_7081    :Silver`).
- Use StringIds.h for the type-name id, because InteractiveConsole/ObjectLoadError in libopenrct2 need it as well as the UI.
- en-GB format (data/language/en-GB.txt, for example line 3736 `STR_6743    :Climate`, line 3711 `STR_6718    :Peep Animations`): `STR_NNNN`, padding to column 13, then `:` and the text. Comments start with `#`. Other languages fall back to en-GB automatically.
- Parser limit: LanguagePack.cpp:257 is `if (sscanf(identifier, "STR_%4d", &stringId) != 1)`, i.e. 4 digits, so ids ≤ 9999.
- The fork reservation (CONTEXT.md, "String ids | 0x5000–0x9FFF after the 5-digit LanguagePack parser patch") therefore needs that parser touch point first (`STR_%5d`). If you skip it, use the next upstream id 7082, accepting merge-collision risk.
- `StringId` is `uint16_t` with `kStringIdNone = 0xFFFF` (StringIdType.h:14-16). Ids 0x2000–0x4FFF are the object-string pool, and 0x5000 itself has the off-by-one from problem 3.
- Editor tab icon options: an existing sprite (e.g. `SPR_G2_*`, SpriteIds.h:1161 `SPR_G2_PEEP_ANIMATIONS`, or `SPR_WEATHER_SUN_CLOUD = 23191` at line 910), or a planned g3 sprite.

---
## Checklist of edits (file:line → change)

1. ObjectTypes.h:45-46: add `factoryPrototype,` after `climate,`; line 51 changes `19` to `20`.
2. ObjectTypes.cpp:39 (kAllObjectTypes) and :50 (kTransientObjectTypes): append the new type.
3. ObjectLimits.h:38: add `kMaxFactoryPrototypeObjects = 8192`. ObjectList.cpp:39: append it to `kObjectEntryGroupCounts`.
4. ObjectManager.cpp:143: fix the entry-index mapping for indices ≥ 2047.
5. ObjectFactory.cpp:30-50 (include), :414-416 (CreateObject case), :444 (`"factory_prototype"` map entry).
6. ObjectRepository.cpp:73: kVersion 31→32. Lines 80-83: add the `factory/objects` root. Optionally add a Serialise case at 145 plus ObjectRepository.h:56-59 FactoryPrototypeInfo.
7. New `FactoryPrototypeObject.{h,cpp}` following the ClimateObject/PeepAnimationsObject pattern, added to libopenrct2.vcxproj.
8. InteractiveConsole.cpp:1142: append the string id (static_assert).
9. ObjectLoadError.cpp:353-354: add a case.
10. EditorObjectSelection.cpp:192: add a page or sub-tab entry.
11. EditorController.cpp:357: add the factory element case (crash fix). Line 951: optional skip in RemoveUnusedObjects.
12. ScInstalledObject.cpp:39 and openrct2.d.ts:655: add the name. Bump kPluginApiVersion (ScriptEngine.h:46).
13. StringIds.h, data/language/en-GB.txt: new `STR_OBJECT_SELECTION_FACTORY_PROTOTYPES`. LanguagePack.cpp:257: parser patch if using 0x5000+.
14. Optional: ParkInfoCommands.cpp:95-123.
15. distribution/changelog.txt, the wiki, and an ADR if any of these choices deviate from ADR 0005.
