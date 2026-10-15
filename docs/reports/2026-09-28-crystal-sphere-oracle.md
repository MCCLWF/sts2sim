# Crystal Sphere C# oracle

This report documents the headless oracle command added to `tools/Sts2Oracle`.
It drives the original `EventModel.BeginEvent`, `CrystalSphereMinigame` constructor,
`SetTool`, `CellClicked`, item reveal callbacks, `CrystalSphereItem.ToReward`, and
`Reward.Populate` methods from `sts2.dll`. It does not reimplement placement,
reveal, reward, or RNG algorithms.

`PlayMinigame` opens a Godot screen and waits for a user. The oracle therefore
uses the game's actual constructor and click methods directly, as allowed for
the headless context. It populates the game's reward objects and reports their
contents, but does not select a reward in the final UI.

## Run it

Build the oracle and run a reproducible sample from the repository root:

```sh
$HOME/.dotnet/dotnet build tools/Sts2Oracle -c Release -v quiet -nologo
$HOME/.dotnet/dotnet tools/Sts2Oracle/bin/Release/net9.0/Sts2Oracle.dll \
  crystal-sphere --seed SEED0002 --choice payment-plan \
  --clicks 'B:7,7;B:1,4;B:1,2;B:6,9;B:9,6;B:0,6'
```

The command emits one JSON object on stdout. `--seed` can be replaced by
`--seeds S1,S2`; `--choice` accepts `payment-plan` (six cell clicks) or
`uncover-future` (three); `--clicks` is a semicolon-separated sequence of
`B:x,y` or `S:x,y` actions. Coordinates are zero-based, with x increasing
left-to-right and y top-to-bottom. `--gold` defaults to 300 so the event is
eligible in Act 2, matching the C++ fixture setup; other gold amounts can be
provided explicitly. Event cost, grid placement, and reward generation use the
game's own event RNG.

The harness applies the selected option's real pre-minigame effect too:
Uncover Future calls `PlayerCmd.LoseGold` with the computed cost; Payment Plan
calls `CardPileCmd.AddCurseToDeck<Debt>`. Revealing the Curse item runs the
original `CrystalSphereCurse.RevealItem`, which adds Doubt immediately.

## `SEED0002` initial state

With Ironclad, Act index 1, and 300 gold, `CRYSTAL_SPHERE.IsAllowed` is true.
`BeginEvent` computes Uncover Future's price as **66 gold**. The event RNG
counter is 1 after the cost roll and 16 after the 15 item placements.

Items are reported in the exact source placement order. Coordinates are the
top-left cell; dimensions are width × height.

| Index | Item | Position | Size | Reward data |
|---:|---|---:|---:|---|
| 0 | Relic | (3, 1) | 4 × 4 | — |
| 1 | Common Potion | (9, 5) | 1 × 3 | — |
| 2 | Common Potion | (0, 5) | 1 × 3 | — |
| 3 | Rare Potion | (5, 5) | 2 × 2 | — |
| 4 | Common Card | (7, 7) | 2 × 2 | — |
| 5 | Uncommon Card | (1, 4) | 2 × 2 | — |
| 6 | Rare Card | (1, 2) | 2 × 2 | — |
| 7 | Curse | (6, 9) | 2 × 2 | adds Doubt when fully revealed |
| 8 | Gold | (2, 8) | 1 × 1 | 10 gold |
| 9 | Gold | (9, 4) | 1 × 1 | 10 gold |
| 10 | Gold | (9, 8) | 1 × 1 | 10 gold |
| 11 | Gold | (3, 0) | 1 × 1 | 10 gold |
| 12 | Gold | (2, 9) | 1 × 1 | 10 gold |
| 13 | Big Gold | (3, 10) | 2 × 1 | 30 gold |
| 14 | Big Gold | (8, 2) | 2 × 1 | 30 gold |

Each row below represents y=0 through y=10. `H` is hidden and `.` is already
clear. The item mask uses `0`–`9`, then `A`–`E`, for item indices; `.` is empty.
The JSON includes both masks and the per-item table.

```text
hidden:
...HHHHH...
..HHHHHHH..
.HHHHHHHHH.
HHHHHHHHHHH
HHHHHHHHHHH
HHHHHHHHHHH
HHHHHHHHHHH
HHHHHHHHHHH
.HHHHHHHHH.
..HHHHHHH..
...HHHHH...

item index:
...B.......
...0000....
.660000.EE.
.660000....
.550000..9.
255..33..1.
2....33..1.
2......441.
..8....44A.
..C...77...
...DD.77...
```

## Recorded click traces

### Payment Plan: all Big clicks

Command:

```sh
$HOME/.dotnet/dotnet tools/Sts2Oracle/bin/Release/net9.0/Sts2Oracle.dll \
  crystal-sphere --seed SEED0002 --choice payment-plan \
  --clicks 'B:7,7;B:1,4;B:1,2;B:6,9;B:9,6;B:0,6'
```

The result is `reveal_order=[4,5,6,7,1,2]`. Click-by-click, the reveals are
`[4]`, `[5]`, `[6]`, `[7]`, `[1]`, `[2]`. The option first adds `DEBT` to the
deck; the fourth click fully uncovers the Curse item and immediately adds
`DOUBT`.

After calling the original `ToReward` methods and populating each real reward,
the offers are:

| Item | Populated reward |
|---:|---|
| 4 | Common cards: `BLOOD_WALL`, `HEADBUTT`, `BODY_SLAM` |
| 5 | Uncommon cards: `HOWL_FROM_BEYOND`, `WHIRLWIND`, `INFLAME` |
| 6 | Rare cards: `PYRE`, `STOKE`, `PACTS_END` |
| 1 | Common Potion: `FIRE_POTION` |
| 2 | Common Potion: `DEXTERITY_POTION` |

RNG counters: after cost **1**, after placement **16**, after constructing all
reward objects **18**, and after `Reward.Populate` **36**. The full per-click JSON
from the run is `/tmp/crystal-sphere-SEED0002-payment.json` in the current
workspace.

### Mixed Big/Small: relic, potion, and gold

Command:

```sh
$HOME/.dotnet/dotnet tools/Sts2Oracle/bin/Release/net9.0/Sts2Oracle.dll \
  crystal-sphere --seed SEED0002 --choice payment-plan \
  --clicks 'B:5,5;B:4,2;B:5,3;B:3,4;S:6,1;B:8,2'
```

The click reveals are `[3]`, `[]`, `[]`, `[]`, `[0]`, `[14]`, giving
`reveal_order=[3,0,14]`. Populated rewards are Rare Potion `FRUIT_JUICE`, Relic
`VAJRA`, and **30 gold**. RNG counters are 1 after cost, 16 after placement,
17 after reward construction, and 19 after population. The full trace is
`/tmp/crystal-sphere-SEED0002-mixed.json`.

### Uncover Future: three clicks

Command:

```sh
$HOME/.dotnet/dotnet tools/Sts2Oracle/bin/Release/net9.0/Sts2Oracle.dll \
  crystal-sphere --seed SEED0002 --choice uncover-future \
  --clicks 'B:5,5;S:6,1;B:8,2'
```

The source cost is **66**, and the real `LoseGold` path changes 300 gold to
234. The final reveal order is `[3,14]`, giving Rare Potion `FRUIT_JUICE` and
30 gold. RNG counters are 1 after cost, 16 after placement, 17 after reward
construction, and 18 after reward population. The full trace is
`/tmp/crystal-sphere-SEED0002-future.json`.

## Verification notes

The new oracle project builds with .NET 9, and all three commands above ran
successfully against the installed game assembly. The original headless
bootstrap emits Sentry and `ReleaseInfoManager` diagnostics to stderr because
Godot extensions and the packaged `release_info.json` are unavailable in this
process; the JSON output is valid and contains the game-method results. The
final reward choice is intentionally not made, since that is the interactive
reward screen's player decision; generated options and RNG use the original
game methods.
