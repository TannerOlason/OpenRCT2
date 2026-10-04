# The Factory Tour ride takes the free 1D ride type slot

The tour ride is a real ride type, `RIDE_TYPE_FACTORY_TOUR`, in the unused `RIDE_TYPE_1D` slot rather than a new id
past `RIDE_TYPE_COUNT` or a scripted variant of the car ride. A slot inside the existing range keeps every per-type
table (RTD array, track style, ride lists) its current size, saves keep the one-byte ride type, and vehicles bind to
it by name (`"type": "factory_tour"`) like any other ride object. Its rating bonus is a new modifier appended to
`RatingsModifierType`; the per-ride scan totals live in fork state, not in upstream's `RideRating::UpdateState`, so
upstream's rating chunk keeps its layout. If upstream ever claims 1D, the fork moves to another free slot and remaps
the byte on load from fork saves.
