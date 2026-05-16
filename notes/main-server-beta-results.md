# Main Server Beta Results (2026-05-15)

## Release-Stabilization Note (2026-05-16)

- TBC production path remains the live baseline (`LoadedMappingCount=170`).
- WotLK support is constrained to staged `single_token_chain` scope (`LoadedWotlkMappingCount=380`, `WotlkSingleTokenChainCount=60`).
- WotLK unsupported structures remain disabled:
  - `emblem_like`
  - `token_plus_prior_armor_upgrade` (including `52025`-style marks)
- Safe defaults for release posture:
  - `WotlkExchangeEnable=0`
  - `WotlkDryRun=1`
  - `WotlkAutoExchangeEnable=0`
  - `AllowDebugTargetCommand=0`

## Main Service Config Observed

From `/home/cbur/azeroth-server/etc/modules/bot_token_exchanger.conf`:

- `DryRun = 0`
- `ExchangeEnable = 1`
- `AutoExchangeEnable = 1`
- `PlayerbotLootPassEnable = 1`
- `DiscoveryWriteDb = 0`
- `AutoPopulateMappings`: not present in this config/module
- `AllowDebugTargetCommand = 0`

Loaded behavior in server logs:

- `BotTokenExchanger config loaded ...`
- `Loaded 170 staged token resolver mappings from bot_token_exchanger_token_map`
- Auto-exchange running in `Mode: real exchange`

## Log Source

Active main log path for this run:

- `/home/cbur/azeroth-server/logs/Server.log`

`/home/cbur/azeroth-server/bin/Server.log` had no BotTokenExchanger lines for this timeframe.

## Skipped / Ambiguous Cases Observed

### Bot `Dogshyt`

- Class/Faction from DB: `class=5 (Priest)`, `race=8 (Troll)`, faction Horde
- Token: `29761 Helm of the Fallen Defender`
- Logged candidates:
  - `29049 Light-Collar of the Incarnate`
  - `29058 Soul-Collar of the Incarnate`
- Exact reason:
  - `remains ambiguous after faction filtering`
- Classification:
  - Resolver gap (Priest role-family disambiguation missing)

### Bot `Be`

- Class/Faction from DB: `class=5 (Priest)`, `race=8 (Troll)`, faction Horde
- Token: `29764 Pauldrons of the Fallen Defender`
- Logged candidates:
  - `29054 Light-Mantle of the Incarnate`
  - `29060 Soul-Mantle of the Incarnate`
- Exact reason:
  - `remains ambiguous after faction filtering`
- Classification:
  - Resolver gap (Priest role-family disambiguation missing)

### Bot `Cf`

- Class/Faction from DB: `class=11 (Druid)`, `race=6 (Tauren)`, faction Horde
- Token: `29764 Pauldrons of the Fallen Defender`
- Logged candidates:
  - `29089 Shoulderguards of Malorne`
  - `29095 Pauldrons of Malorne`
- Exact reason:
  - `remains ambiguous after faction filtering`
- Classification:
  - Resolver gap (Malorne shoulder family naming was not decisive enough in this case)

### Bot `Ah`

- Class/Faction from DB: `class=7 (Shaman)`, `race=8 (Troll)`, faction Horde
- Token: `29763 Pauldrons of the Fallen Champion`
- Logged candidates:
  - `29031 Cyclone Shoulderpads`
  - `29043 Cyclone Shoulderplates`
- Exact reason:
  - `remains ambiguous after faction filtering`
- Classification:
  - Likely role-classifier ambiguity; no mapping-table defect found

## DB Verification

- `bot_token_exchanger_token_map` exists in `acore_world`
- Row count: `170`
- Rows for observed unresolved token IDs:
  - `29761 -> 7 rows`
  - `29763 -> 7 rows`
  - `29764 -> 7 rows`

## Verified Fix Applied

Code changes in resolver (`src/BotTokenExchangerMgr.cpp`):

- Enabled role filtering for `CLASS_PRIEST`
- Added valid-role gate for Priest (`healer` / `caster_dps`)
- Added Priest family-name disambiguation for TBC sets:
  - `Light-*` -> `healer`
  - `Soul-*` -> `caster_dps`
- Added explicit Druid Malorne shoulder-family disambiguation:
  - `Mantle of Malorne` -> `caster_dps`
  - `Pauldrons of Malorne` -> `healer`
  - `Shoulderguards of Malorne` -> `tank` (or `melee_dps` when not tank)

No token IDs were invented or hardcoded; only name-family role classification and role-filter inclusion were adjusted.

## Shaman 29763 Follow-Up (Ah) (2026-05-15)

### Evidence Collected

- Log evidence (main server):
  - `BotTokenExchanger token 29763 (Pauldrons of the Fallen Champion) remains ambiguous after faction filtering: [29031 Cyclone Shoulderpads] [29043 Cyclone Shoulderplates]`
- Bot identity:
  - `Ah`: class `7` (Shaman), race `8` (Troll, Horde), guid `6541`
- Current inventory state:
  - `Ah` still has token `29763` in bags (`slot 15`)
  - `Ah` does **not** currently own/equip `29031` or `29043`
- Equipped profile snapshot for `Ah` is melee-oriented (e.g. `Sun-forged Cleaver`, `Reflex Blades`, `Totem of the Thunderhead`), consistent with Enhancement role.

### DB Mapping and Item Evidence

For `token_item_id = 29763`:

- `29031 Cyclone Shoulderpads` (`allowable_class=64`, `inventory_type=3`, `vendor=20616`, `extcost=1212`)
- `29037 Cyclone Shoulderguards` (`allowable_class=64`, `inventory_type=3`, `vendor=20616`, `extcost=1212`)
- `29043 Cyclone Shoulderplates` (`allowable_class=64`, `inventory_type=3`, `vendor=20616`, `extcost=1212`)

Observed stat profile excerpts:

- `29031` carries intellect/spell-leaning profile
- `29043` carries attack-power/melee-leaning profile
- `29037` remains healer shoulder-family (`Shoulderguards`) and was already working

### Root Cause

Shaman shoulder special-case logic treated `shoulderpads` as role-dependent (`preferredRole` for melee/caster), which could retain both `shoulderpads` and `shoulderplates` for Enhancement bots, causing ambiguity.

### Fix Applied (Resolver-only)

In `src/BotTokenExchangerMgr.cpp` Shaman family overrides were tightened:

- `shoulderpads` => `caster_dps` (explicit)
- `shoulderplates` => `melee_dps` (explicit)
- existing `shoulderguards` => `healer` unchanged

This preserves existing working behavior for:

- `Cyclone Chestguard`
- `Cataclysm Leggings`
- `Cyclone Shoulderguards`

### Live Validation Status

- Build/install: completed successfully.
- `worldserver` restart: completed successfully.
- Initial post-restart pass: `Ah` live `.tokenex` re-check could not be executed because `Ah` was offline (`online=0`).
- Follow-up validation after `Ah` came online:
  - `BotTokenExchanger queued bot Ah guid 6541 reason login tokens 1 ...`
  - `BotTokenExchanger processing queued bot Ah guid 6541 with 1 staged token ids.`
  - `BotTokenExchanger auto bot Ah token scan found 1 staged token item IDs. Mode: real exchange.`
  - `BotTokenExchanger exchanged token 29763 (Pauldrons of the Fallen Champion) -> reward 29043 (Cyclone Shoulderplates) via vendor 20616 extcost 1212`
  - `BotTokenExchanger auto bot Ah exchange complete: 1 resolved, 0 skipped, mode real exchange.`
- No new `29763` ambiguity line was emitted for `Ah` after this fix.

## Shaman Helm Token Follow-Up (2026-05-15)

### Scope and Log Findings

- Requested issue: unresolved Shaman helm-token exchange on main server after the shoulder fix.
- Current active logs only showed successful Shaman helm exchange:
  - `BotTokenExchanger exchanged token 29760 (Helm of the Fallen Champion) -> reward 29028 (Cyclone Headdress) ...`
- No current `helm/head` Shaman ambiguity line remained in active `logs/Server.log` after this pass.
- `bin/Server.log` did not contain BotTokenExchanger evidence for this timeframe.

### DB Evidence for Shaman Helm Families

Shaman helm candidates in staged mapping table:

- `29760 Helm of the Fallen Champion` -> `29028 Cyclone Headdress`, `29035 Cyclone Faceguard`, `29040 Cyclone Helm`
- `30242 Helm of the Vanquished Champion` -> `30166 Cataclysm Headguard`, `30171 Cataclysm Headpiece`, `30190 Cataclysm Helm`

Item profile and family evidence used:

- `29040 Cyclone Helm`: melee/AP-oriented stat profile (Enhancement family)
- `29035 Cyclone Faceguard`: caster-oriented profile (Elemental family)
- `29028 Cyclone Headdress`: healer-oriented profile (Restoration family)
- `30190 Cataclysm Helm`: melee/AP-oriented profile (Enhancement family)
- `30171 Cataclysm Headpiece`: Elemental family (`Cataclysm Regalia`)
- `30166 Cataclysm Headguard`: Restoration family (`Cataclysm Raiment`)

### Root Cause

Shaman class-aware override had helm-family role mapping drift:

- `helm` was being forced to `caster_dps`
- `faceguard` was being forced to `melee_dps`
- `headguard` was being forced to `caster_dps`

This conflicts with TBC Shaman tier-family naming and can produce unresolved/incorrect helm resolution under some detected roles.

### Fix Applied (Resolver-only)

For Shaman `cyclone/cataclysm` families:

- `helm` -> `melee_dps`
- `faceguard` -> `caster_dps`
- `headguard` -> `healer`
- `headpiece` -> `caster_dps`
- `headdress` remains `healer`

No token IDs were guessed; no safety skip was removed; no architecture changes were made.

### Validation

- Build: passed.
- Install: passed.
- worldserver restart: passed (`active`).
- Post-restart logs currently show normal config load and no new Shaman helm ambiguity lines in active window.
- Shoulder fix path remains unchanged in code and no new shoulder regressions were emitted in current active log window.

## Druid Nordrassil Helm Audit Follow-Up (2026-05-15)

### Evidence

- Full staged mapping audit found T5 Druid helm token family:
  - `30243 Helm of the Vanquished Defender` ->
    - `30219 Nordrassil Headguard`
    - `30228 Nordrassil Headdress`
    - `30233 Nordrassil Headpiece`
- Current resolver class-aware Druid hints had `headguard` in caster path and not in healer path.
- External set-family reference confirms `Nordrassil Headguard` belongs to restoration set (Nordrassil Raiment).

### Root Cause

Class-aware Druid helm hint drift for Nordrassil:

- `Headguard` was treated as caster when it should be healer-family.

### Fix Applied (Minimal)

In Druid class-aware hint rules:

- Added `headguard` to healer hint list.
- Removed `headguard` from caster hint list.

No other class/family logic changed in this step.

### Validation

- Build/install: passed.
- worldserver restart: passed (`active`).
- Immediate post-restart log window showed no new BotTokenExchanger ambiguity/skip lines.
