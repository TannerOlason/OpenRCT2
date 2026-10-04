# Freight railway in the free 1F slot, with cargo in a side table

The plan cloned the Miniature Railway as `freightRailway` with a `CarEntry` that carries inventory slots, plus a
vehicle hook on station arrival in `ride/Vehicle.cpp`. A new `CarEntry` field changes ride objects and vehicle saves,
and a hook in the vehicle state machine is a deep touch point. Instead:

- `RIDE_TYPE_FREIGHT_RAILWAY` takes the free `RIDE_TYPE_1F` slot, as the tour ride took 1D (ADR 0010). Its descriptor
  is flat miniature-railway track without an entrance or exit. One touch point in `RideCheckForEntranceExit` lets it
  test and open without them, and the new-ride list shows it with the transport rides.
- Cargo is a fork side table (`FreightState::cargo`, one entry per car entity id: item and count, up to 200 of one
  kind) in the pools chunk. Entries are pruned when their car leaves the freight railway.
- Loading happens in the factory tick, not in vehicle code. Every train whose head car is standing still trades
  through each of its cars on station track. A freight loader container beside that car moves one item a tick
  into it, and a freight unloader takes one a tick out. Upstream's `trainAtStation` is not used because it only
  tracks passenger boarding.

Trains run on upstream's departure logic, so players set minimum and maximum waiting times to give loaders time.
Vanilla parks have no freight rides, so nothing runs for them.
