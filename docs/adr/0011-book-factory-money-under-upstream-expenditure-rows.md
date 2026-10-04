# Book factory money under upstream expenditure rows

The plan appended `factoryConstruction, factoryRunningCosts, goodsSales, rawMaterialPurchase` to
`ExpenditureType`. The park file writes the expenditure table as `months x ExpenditureType::count` money values and
reads back `min(count, stored)` columns per row without skipping extras, so a longer enum would change every save
(Vanilla Mode included) and misalign the table for any upstream build that opens a fork save. The fork therefore books
its money under the closest upstream row: Market income is Shop sales, factory building stays Landscaping, and future
raw material purchases will be Shop stock. Fork-side totals that need their own line (Market income so far:
`Market::goodsSold`) live in fork state and are shown in fork windows. If upstream ever makes the table
forward-compatible, separate rows can return without a save migration.
