# Fluids are flagged items held as one volume per network

A fluid is an ordinary `item` prototype with `"fluid": true`, not a new prototype kind, so recipes, icons, names
and research unlocks reuse the item path; inserters, belts and chests simply refuse fluid items. Fluid lives in
Fluid Networks: connected components of pipes and machine fluid boxes, each holding one fluid as a single
`amount` against the summed capacity, with no per-pipe flow or pressure. Machines declare fluid boxes with sides
relative to their facing; a multi-side box is a pass-through node, which gives boiler chains and engine rows
without special cases. Consumers draw a proportional share from last tick's demand (the power network's
satisfaction pattern), keeping the result independent of record id order. Rebuilds redistribute existing volume
by node capacity before regrouping, so building and removing pipes conserves fluid instead of resetting it.
We accept that fluid teleports across a network instantly and that throughput limits come only from machine
rates; per-pipe flow would cost a pass per pipe per tick and break the 8 ms budget at scale.
