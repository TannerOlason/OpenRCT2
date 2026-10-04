# Store fork fields on upstream structs as side tables

Fork data that logically belongs to an upstream object (guest `factoryFlags`, ride `health` and
`stockMode`, scenario `constructionMode` and `shopStockMode`, objective extras) is stored in Side Tables
keyed by `EntityId` or `RideId` and persisted in the `parkExt` Fork Chunk, not as new members on `Guest`,
`Ride`, `Scenario::Options` or `Objective`. Upstream chunk layouts and struct sizes stay untouched, which
keeps merges mechanical and Vanilla Mode saves identical. Lookups are a hashed map hit, acceptable because
these fields are read at guest-decision and ride-tick granularity rather than per belt item.
