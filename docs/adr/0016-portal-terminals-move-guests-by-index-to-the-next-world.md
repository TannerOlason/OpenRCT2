# Portal terminals move guests by index to the next world

The plan linked portal rides explicitly (`PortalLink{worldA, rideA, worldB, rideB}` set by a `PortalLinkAction`).
The fork instead pairs terminals implicitly. A guest who finishes a ride on the k-th Portal Terminal of world w,
counting in ascending ride id order, steps out of terminal k mod n of world w + 1, which wraps to world 0. Linking
then needs no window, no action and no cleanup when a ride is demolished, and every peer computes the same pairing.
Explicit links can be added later if scenarios need non-linear networks.

The ride is a track ride cloned from the Factory Tour and placed in the free `RIDE_TYPE_22` slot, so it reuses the
tour tram's sprite layout and needs no building art. The transfer copies only world-neutral fields: happiness,
energy, the needs, nausea, money and name. Everything that refers to ids in the old world is dropped. The departure
(removal from the old world) and the arrival (`Guest::generate` on the footpath beside the matching terminal's exit)
go through a company-level queue, processed in each world's factory tick, so the order is fixed for every peer.
