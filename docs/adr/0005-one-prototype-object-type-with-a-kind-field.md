# One prototype object type with a kind field

Items, recipes, belts, inserters, containers, machines, poles, pipes, generators, ores and technologies are
all objects of a single new `ObjectType::factoryPrototype` distinguished by `properties.kind`. Each new
`ObjectType` costs about eleven upstream Touch Points (object list, factory, repository version, park file,
script bindings, editor selection, importers) plus editor tabs; one type pays that once. The cap is raised to
8192 because the entry index is a `uint16`. Content text lives in object `"strings"` and art in object
`"images"`, which are allocated from the one-million-slot dynamic pool, so adding content never edits
`SpriteIds.h` or `StringIds.h`.
