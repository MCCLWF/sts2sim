# Shared relics batch 12 report

## Scope and source

Ported Membership Card from v0.111.0 and connected `AbstractModel.ModifyMerchantPrice` through merchant `EntryRef` handles. The hook runs over run listeners only while the current room is a shop. It multiplies the original Decimal price before converting back to an integer, so the adjusted value is used for display, affordability, and gold spent.

Membership Card applies 50% of the original price to its owner. The Courier's existing 20% discount is also implemented through the new hook. Its `ShouldRefillMerchantEntry` callback and the restocking effects remain unported.

The shared relic pool has 118 entries. C++ model headers now exist for 117; `UnsettlingLamp` is the only shared relic without a model header. This branch has direct behavior hooks for 48 shared relics.

## Verification

| Check | Result |
|---|---|
| `cmake --build build --target test_relics -j4` | Passed |
| `./build/test_relics` | Passed; verifies Membership Card's discounted shop price and purchase debit, plus The Courier's 20% discount |
| `./scripts/check.sh` build and unit tests | Passed; all 8 CTest cases |
| Whole-run smoke | Passed; `death=93 unported=7`, `death=99 unported=1`, and `boss=4 death=96`; no crashes or hangs |
| L2 setup/maps | 200/200 identical |
| L3 Ironclad A0/A10 | 40/40 each; 784/784 and 827/827 states matched |
| L4 Ironclad A0/A10 | 40/40 each; 1958/1958 and 2131/2131 decisions matched; 0 unported and 0 diverged |

Oracle compilation reported one pre-existing nullable warning in `tools/Sts2Oracle/OracleRun.cs`; build succeeded with zero errors.

## Limits

- `TheCourier.ShouldRefillMerchantEntry` and the associated entry restocking behavior are still unported. Purchasing while The Courier is owned remains outside this batch's verified behavior.
- Unsettling Lamp remains unported.
- Cross-character card generation during Ironclad runs remains explicitly unimplemented.
