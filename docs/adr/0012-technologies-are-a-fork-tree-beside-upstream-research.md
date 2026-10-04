# Technologies are a fork tree beside upstream research

The plan put technologies into upstream's research lists as `Research::EntryType::technology`, progressed by labs
instead of funding. Upstream treats every non-ride research item as a scenery group (`ResearchRemoveNullItems`,
`ResearchMarkItemAsResearched`, news, the research and inventions-list windows, `ScResearch`), so a third entry type
needs touch points in about ten files. And one queue mixing funded and lab-driven items stalls ride research whenever
labs are idle. Instead, technologies form a separate tree in fork state (`ResearchState` in the pools chunk):
researched technologies, units done and the technology labs work on, chosen in the Factory research window through
`FactorySetParkOptionAction`. Labs consume packs and progress the current technology alongside funded research.

The tree is still one unlock tree for the whole park. A technology unlocks factory prototypes (locked while any
loaded technology lists them and none of those is researched) and ride entries and scenery groups. Two hooks in
`Research.cpp` keep upstream consistent: `withholdGatedResearch` removes technology-gated rides and scenery groups
from upstream's lists, and `applyTechnologyUnlocks` invents those of researched technologies whenever upstream rebuilds
its invented tables. Both do nothing when no technology is loaded, so Vanilla Mode and parks without technologies keep
upstream behaviour. The lock index is derived from loaded objects only and rebuilt after any factory prototype loads
or unloads. The "ignore research status" cheat unlocks everything.
