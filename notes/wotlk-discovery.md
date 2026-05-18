# WotLK Discovery Notes (Phase W1)

Date: 2026-05-15

## Scope

- Phase W1 only: discovery/logging for potential WotLK token mappings.
- No WotLK exchange logic enabled.
- No staged WotLK mappings written to DB (`DiscoveryWriteDb=0`).
- TBC resolver/exchange behavior remains unchanged.

## Code Path Added

- New command: `.tokenex discover wotlk`
- Shared discovery pipeline now supports:
  - `TBC` (existing behavior, known-vendor filtered)
  - `WOTLK` (broad vendor scan + safety filters)
- `TryStageMapping` now accepts `source_expansion` and writes `WOTLK` only when DB writes are enabled.
- Resolver load path remains TBC-only:
  - `LoadResolverMappings()` still filters `WHERE source_expansion = 'TBC'`

## Verified Data Source Behavior

- `npc_vendor.ExtendedCost` correctly points to runtime `ItemExtendedCost` entries used by worldserver.
- On this PTR world DB, `itemextendedcost_dbc` table is effectively empty for vendor armor rows:
  - SQL check showed `missing_extcost = 9066 / 9066` for armor+ExtendedCost rows.
- Runtime DBC store is therefore required for practical discovery.

## WotLK Discovery Filters Used

- reward from `npc_vendor` must exist and be armor.
- reward inventory type must be one of supported armor slots.
- reward must have class restriction (`AllowableClass != 0`).
- skip rows with non-item costs (`honor`, `arena`, `rating`).
- require exactly one token-like item requirement with count `1`.
- WotLK token candidate must be non-equip quest/misc token and name-family match:
  - `Conqueror`, `Protector`, `Vanquisher`, `Triumph`, or `Sanctification`.
- ambiguous or unsafe rows are skipped (log-only).

## Runtime Command Execution

- `worldserver` built and installed successfully.
- `.tokenex discover wotlk` was executed from live worldserver console.
- Output was extremely large (many skip lines across broad vendor scan), dominated by safe-skip reasons.
- Representative skip reason observed repeatedly:
  - unsupported non-item requirements (honor/arena/rating)

## Current Outcome

- WotLK discovery plumbing is present and safe-gated.
- DB remains unchanged for WotLK (`source_expansion='WOTLK'` row count is `0` with write gate off).
- No WotLK exchange behavior is active.
- Additional narrowing of WotLK source vendors/token families is still required before enabling write mode.

## Phase W2 (Narrow Vendor Discovery)

Date: 2026-05-15

### Narrow Command Added

- `.tokenex discover wotlk narrow`
- Narrow mode scans only verified local-DB WotLK tier vendor entries:
  - `28992, 28995, 28997, 29523, 34252, 35496, 35497, 35498, 35500, 37688, 37696, 37991, 37992, 37993, 37997, 37998, 37999, 38054, 38181, 38182, 38283, 38284, 38316, 38840, 38841`
- Summary output now includes:
  - vendors scanned
  - reward rows inspected
  - accepted mappings
  - skipped mappings
  - grouped skip reasons
  - unresolved category focus
- Console-flood control:
  - detailed per-row logs are only sent to console when `Debug=1`
  - accepted sample output is capped (10)

### Runtime Validation Output

From live run of `.tokenex discover wotlk narrow`:

- `vendors scanned: 25`
- `reward rows inspected: 1340`
- `accepted mappings: 0`
- `skipped mappings: 1340`
- `staged: 0` (write gate still disabled)
- grouped skip reasons:
  - `not exactly one supported token requirement: 860`
  - `invalid required-item structure: 480`
- unresolved category focus:
  - `not exactly one supported token requirement`

### W2 Interpretation

- Narrow vendor discovery is now clean and auditable.
- On this PTR snapshot, runtime DBC required-item structures for these vendors do not currently satisfy the strict single-token acceptance rule.
- WotLK DB write is still not safe.

## Phase W3 (Inspect ExtendedCost Structures)

Date: 2026-05-15

### Command Added

- `.tokenex discover wotlk inspect`
- Scope:
  - same 25 verified WotLK narrow vendors
  - no DB writes
  - capped diagnostics: max 5 samples per structure
  - grouped summary counts by observed structure

### Runtime Output (inspect)

- `vendors scanned: 25`
- `reward rows inspected: 1340`
- `accepted mappings: 0`
- `skipped mappings: 1340`
- `staged: 0`
- grouped structures:
  - `invalid required-item structure: 480`
  - `two or more item requirements: 480`
  - `one required item but not supported WotLK token: 380`

### Representative Structures Observed

1. One required item, count 1, but required item is another token object not matched by current WotLK token-name rule.
   - Example:
     - reward `39531 Heroes' Dreamwalker Headpiece`
     - extcost `2487`
     - req item `40618 Helm of the Lost Vanquisher` (class 15, non-equip)

2. One required item, count > 1, emblem-style currency item.
   - Example:
     - reward `40694 Jorach's Crocolisk Skin Belt`
     - extcost `2524`
     - req item `40752 Emblem of Heroism` count `40` (class 10)

3. Two required items (upgrade structure).
   - Example:
     - reward `51155 Sanctified Bloodmage Shoulderpads`
     - extcost `2745`
     - req item A: `52025 Vanquisher's Mark of Sanctification` x1
     - req item B: `50279 Bloodmage Shoulderpads` x1

### Interpretation of WotLK Mechanics from Local Runtime Data

- Tier-like purchases are not a simple `single token item -> reward` model.
- Observed safe categories:
  - `single required item, count 1, class 15 non-equip token-like` (potentially supportable with broader token-family matching after explicit validation)
  - `multi-item upgrade costs` (token + base tier piece): structurally different from TBC.
- Observed categories that should stay skipped for current module scope:
  - emblem-only costs with high counts (`Emblem of Heroism/Valor` etc.)
  - multi-item upgrade chains until model/schema supports them explicitly.

### Schema/Model Implication

- Current map schema (`token_item_id -> reward_item_id`) is too narrow for dominant WotLK cases.
- WotLK likely needs:
  - either schema expansion to support multiple required items/counts per reward mapping, or
  - a separate WotLK upgrade-map table keyed by `reward_item_id` + required-cost vector (`req_item_1..n`, counts, optional currency fields).
- Recommendation:
  - keep existing TBC schema/path unchanged.
  - add a separate WotLK discovery+resolver model rather than weakening TBC-era single-token safety assumptions.

## Phase W4 (WotLK Staging Schema Design)

Date: 2026-05-15

### Implemented Discovery-Only Staging Model

Added new command:

- `.tokenex discover wotlk stage`

Behavior:

- scans same verified 25-vendor WotLK subset
- captures full requirement vectors from runtime DBC extended cost
- log-only unless `DiscoveryWriteDb=1`
- does **not** enable or invoke WotLK resolver/exchange paths

### Proposed/Implemented Tables

1. `bot_token_exchanger_wotlk_map`

- `reward_item_id` (PK)
- `reward_name`
- `inventory_type`
- `allowable_class`
- `vendor_entry`
- `extended_cost_id`
- `source_expansion`
- `source_tier`
- `source_raid`
- `confidence`
- `status`
- `notes`

2. `bot_token_exchanger_wotlk_cost`

- `reward_item_id`
- `required_item_id`
- `required_item_name`
- `required_count`
- `required_item_role` (`token`, `prior_armor`, `emblem_like`, `unknown`)
- PK: (`reward_item_id`, `required_item_id`)

### Conservative Required-Item Role Classification

- `token`:
  - non-equip misc token/mark style families (e.g. `Conqueror/Protector/Vanquisher`, `Lost/Wayward`, `Mark of Sanctification`, `... of Triumph`)
- `prior_armor`:
  - equippable armor piece requirement
- `emblem_like`:
  - non-equip `Emblem of ...` requirements
- `unknown`:
  - anything else

### Runtime Validation (`DiscoveryWriteDb=0`)

From `.tokenex discover wotlk stage`:

- vendors scanned: `25`
- reward rows inspected: `1340`
- rewards discovered: `905`
- cost rows discovered: `1820`
- staged reward rows: `0`
- staged cost rows: `0`

Structure summary:

- `token_plus_prior_armor_upgrade: 480`
- `single_emblem_like: 480`
- `single_token_chain: 380`

Required-item role counts:

- `token: 860`
- `prior_armor: 480`
- `emblem_like: 480`

### T7/T8/T9/T10 Presence (from local reward naming in staged discovery)

- T7-like families present (`Heroes' ...`)
- T8-like families present (`Valorous ...`, `Conqueror's ...`)
- T9-like families present (`... of Triumph`, `Triumphant ...`)
- T10-like families present (`Sanctified ...`)

### Adequacy Assessment

- The separate requirement-vector model is adequate for discovered WotLK structures.
- Current TBC `token_item_id -> reward_item_id` map is not adequate for WotLK.
- WotLK exchange should remain disabled until a WotLK-specific resolver/exchange path is designed against these staged vectors.

## Unresolved / Next Discovery Questions

- Need a verified runtime subset of WotLK raid/tier vendor entries to reduce noise.
- Need runtime-confirmed accepted candidate set from `.tokenex discover wotlk` summary output in a quieter execution window.
- After that, review candidate rows and define safe W2 DB-staging criteria.

## Phase W5 (DB Staging Write + Idempotency Validation)

Date: 2026-05-15

## Phase W9 (WotLK Dry-Run Exchange Path, Single-Token-Chain Only)

Date: 2026-05-15

### Scope Implemented

- Added WotLK exchange commands:
  - `.tokenex exchange wotlk selected`
  - `.tokenex exchange wotlk bot <botName>`
- Added WotLK config gates:
  - `BotTokenExchanger.WotlkExchangeEnable`
  - `BotTokenExchanger.WotlkDryRun`
  - `BotTokenExchanger.WotlkAutoExchangeEnable`
- WotLK path only scans token IDs present in the WotLK resolver cache (W6/W8 `single_token_chain` subset).
- No SQL is used in WotLK exchange hot path.
- No WotLK rows are loaded into the TBC resolver.

### Safety Behavior

- If `WotlkExchangeEnable=0`, command exits with explicit disabled message.
- If `WotlkDryRun=1`, command reports only `WOTLK DRY RUN` actions and does not mutate inventory.
- If `WotlkDryRun=0`, real WotLK exchange remains explicitly blocked in W9.
- Ambiguous or no-match results are skipped with reasons.
- Emblem-like and upgrade-chain requirements stay excluded because they are not in the WotLK resolver cache.

### Build/Install

- `cmake --build build --target worldserver -j"$(nproc)"`: pass
- `cmake --install build`: pass

### Runtime Validation Snapshot

Config used on PTR runtime:

- `WotlkExchangeEnable=1`
- `WotlkDryRun=1`
- `WotlkAutoExchangeEnable=0`

Status confirmed in `.tokenex status`:

- `LoadedMappingCount=170` (TBC unchanged)
- `LoadedWotlkMappingCount=380`

Command results:

- `.tokenex exchange wotlk bot Cahkul` -> `debug bot Cahkul has no staged WOTLK single-token-chain token items in bags.`
- `.tokenex exchange wotlk bot Abinaar` -> `debug bot Abinaar has no staged WOTLK single-token-chain token items in bags.`
- `.tokenex exchange wotlk bot Aevaette` -> `No online Playerbot named Aevaette was found.`
- `.tokenex exchange wotlk bot Aenstus` -> `No online Playerbot named Aenstus was found.`

Additional DB check during same window:

- Online bots had no bag entries matching staged single-token-chain token IDs.

### W9 Outcome

- WotLK dry-run exchange path is wired and safely gated.
- No inventory mutation occurred in this validation pass.
- No WotLK exchange was enabled.
- TBC resolver/exchange path remained unchanged.

### Next Validation Needed

- Re-run W9 command matrix on online bots that actually carry staged single-token-chain WotLK tokens to capture positive `WOTLK DRY RUN` reward lines.

## Phase W9B (Controlled Positive Dry-Run Validation)

Date: 2026-05-15

### Test Config

- `BotTokenExchanger.WotlkExchangeEnable = 1`
- `BotTokenExchanger.WotlkDryRun = 1`
- `BotTokenExchanger.WotlkAutoExchangeEnable = 0`
- `BotTokenExchanger.AllowDebugTargetCommand = 1` (runtime at test time; config restored to `0` after test)

### Controlled Token Injection (PTR)

Used online class-equivalent bots and injected one staged single-token-chain token at a time:

- Paladin (`Aellan`): `40610 Chestguard of the Lost Conqueror`
- Rogue (`Aihgletin`): `40618 Helm of the Lost Vanquisher`
- Shaman (`Abinaar`): `40611 Chestguard of the Lost Protector`
- Druid (`Aaren`): `40618 Helm of the Lost Vanquisher`

Excluded token check:

- Rogue (`Aihgletin`): added `52025 Vanquisher's Mark of Sanctification` alongside `40618`

### Positive Dry-Run Results

1. Paladin (simple)
- Command: `.tokenex exchange wotlk bot Aellan`
- Output: `WOTLK DRY RUN token 40610 ... -> reward 39629 (Heroes' Redemption Tunic) ... [no mutation]`
- Resolver context in logs included role filtering to healer profile and unique resolution.

2. Rogue (simple)
- Command: `.tokenex exchange wotlk bot Aihgletin`
- Output: `WOTLK DRY RUN token 40618 ... -> reward 39561 (Heroes' Bonescythe Helmet) ... [no mutation]`

3. Shaman (hybrid)
- Command: `.tokenex exchange wotlk bot Abinaar`
- Output: `WOTLK DRY RUN token 40611 ... -> reward 39588 (Heroes' Earthshatter Tunic) ... [no mutation]`
- Resolver logs show hybrid role disambiguation to caster_dps profile.

4. Druid (hybrid)
- Command: `.tokenex exchange wotlk bot Aaren`
- Output: `WOTLK DRY RUN token 40618 ... -> reward 39545 (Heroes' Dreamwalker Cover) ... [no mutation]`
- Resolver logs show hybrid role disambiguation to healer profile.

### Excluded Upgrade-Chain Token Validation

- With both `40618` and `52025` in rogue bags, WotLK exchange scan reported:
  - `WOTLK token scan found 1 staged token item IDs`
  - and processed only `40618`.
- `52025` produced no resolver entry and was ignored by W9 dry-run scope as required.

### No-Mutation Verification

- Dry-run command output explicitly reports `[no mutation]`.
- During test window, repeated runs preserved token-based resolution behavior with no reward grants.
- Post-test cleanup removed all injected tokens from test bots and saved players.
- Final DB check for injected tokens/rewards on test bots returned no rows.

### W9B Outcome

- Controlled positive dry-run validation completed for simple and hybrid class cases.
- Excluded upgrade-chain token behavior (`52025`) remains safe and out-of-scope.
- No real WotLK exchange was enabled.

## Phase W10 (First Controlled Real Exchange, Single-Token-Chain Only)

Date: 2026-05-15

### Scope/Gates Used

- `BotTokenExchanger.WotlkExchangeEnable = 1`
- `BotTokenExchanger.WotlkDryRun = 0`
- `BotTokenExchanger.WotlkAutoExchangeEnable = 0`
- `BotTokenExchanger.AllowDebugTargetCommand = 1`

Patched WotLK exchange path no longer hard-blocks real mode and now uses the same transaction helper (`TryExchangeToken`) used by TBC, while still restricted to staged WotLK `single_token_chain` resolver entries.

### Controlled Runtime Test

Test bot/class:

- `Anguss` (Rogue, class 4), GUID `7179`

Test token/reward pair:

- token `40618` (Helm of the Lost Vanquisher)
- resolved reward `39561` (Heroes' Bonescythe Helmet)

Before state (DB, owner_guid=7179):

- `39561`: `1`
- `40618`: `0`
- `52025`: `0`

Command sequence:

1. `.additem 7179 40618 1`
2. `.tokenex exchange wotlk bot Anguss`
3. `.tokenex exchange wotlk bot Anguss` (immediate second run)
4. `.additem 7179 52025 1`
5. `.tokenex exchange wotlk bot Anguss`
6. `.additem 7179 52025 -1`
7. `saveall`

Key console evidence:

- `Mode: real exchange`
- `WOTLK EXCHANGE exchanged token 40618 ... -> reward 39561 ...`
- second run: `debug bot Anguss has no staged WOTLK single-token-chain token items in bags.`
- with only `52025` present: still `has no staged WOTLK single-token-chain token items in bags.`

After state (DB, owner_guid=7179):

- `39561`: `2`
- `40618`: `0`
- `52025`: `0`

### W10 Validation Result

- Exactly one `40618` token was removed.
- Exactly one `39561` reward was added.
- Immediate second command did not exchange again.
- Excluded upgrade-chain token `52025` stayed out of W10 scope.
- No emblem-like or upgrade-chain processing occurred.
- TBC loaded mapping count remained unchanged at `170`.

### Safety Restore (Post-Test)

Config file restored to safe values:

- `BotTokenExchanger.WotlkExchangeEnable = 0`
- `BotTokenExchanger.WotlkDryRun = 1`
- `BotTokenExchanger.WotlkAutoExchangeEnable = 0`
- `BotTokenExchanger.AllowDebugTargetCommand = 0`

## Phase W11 (Single-Token-Chain Hardening Before Auto-Exchange)

Date: 2026-05-15

### Hardening Changes

1. WotLK duplicate ownership prevention

- In WotLK exchange path only, before attempting transaction:
  - skip exchange if resolved reward is already equipped or present in bags.
- Reused existing helper:
  - `HasRewardItemInEquipmentOrBags(...)`

2. Concise WotLK safety logging

- Added concise `LOG_INFO` lines for:
  - successful exchange / dry-run
  - duplicate-prevented skip
  - no-match / ambiguous / excluded-structure skip
  - store-failure (`CanStoreNewItem`/bag-space path)
  - rollback path (`reward add failed ... token restored` / hard failure)
  - repeated-command token-not-present skip

3. Status visibility

- `.tokenex status` now includes:
  - `LoadedWotlkMappingCount` (existing)
  - `WotlkExchangeEnable` (existing)
  - `WotlkDryRun` (existing)
  - `WotlkAutoExchangeEnable` (existing)
  - `WotlkSingleTokenChainCount` (new; count of staged WotLK token keys loaded in resolver cache)

### W11 Validation Matrix (PTR)

Test bot: `Aledon` (Rogue), debug-target path enabled for test.

1. Duplicate reward ownership prevention

- Injected reward `39561` and token `40618`.
- Ran: `.tokenex exchange wotlk bot Aledon`
- Result:
  - skipped with explicit reason:
    - `reward 39561 ... already owned/equipped`
  - no token consumption occurred in this duplicate-prevented case.

2. Real exchange then repeated command

- Removed duplicate reward, kept token `40618`.
- Ran exchange once:
  - `WOTLK EXCHANGE exchanged token 40618 -> reward 39561 ...`
- Ran exchange immediately again:
  - `has no staged WOTLK single-token-chain token items in bags.`
- Confirms repeat-command safety and no unintended second mutation.

3. Excluded token structure safety (`52025`)

- Added only `52025` and ran WotLK exchange.
- Result:
  - no staged single-token-chain token items detected.
  - no exchange occurred.

4. Invalid class token behavior

- Added `40610` (Conqueror chest token) to Rogue and ran exchange.
- Result:
  - class-mask skips for all candidate rewards
  - final `no safe reward resolved`
  - no exchange occurred.

5. Offline bot behavior

- Debug bot command still safely returns no-online-bot message when target is offline.

6. Full-bag/store-failure path

- Code path is present and logged through shared transaction helper (`CanStoreNewItem` and rollback handling).
- A deterministic full-bag repro was not forced in this W11 runtime pass.

### W11 Post-Test Safe Config

Restored in live module config:

- `BotTokenExchanger.WotlkExchangeEnable = 0`
- `BotTokenExchanger.WotlkDryRun = 1`
- `BotTokenExchanger.WotlkAutoExchangeEnable = 0`
- `BotTokenExchanger.AllowDebugTargetCommand = 0`

### W11 Readiness Assessment

- WotLK `single_token_chain` exchange path is hardened for controlled usage:
  - duplicate prevention in place
  - concise safety logs in place
  - repeat-command and excluded-structure safety validated
  - class mismatch no-match safety validated
- Remaining blocker before enabling WotLK auto exchange:
  - run at least one deterministic full-bag/store-failure live repro (or equivalent targeted test harness) and confirm expected rollback/store-failure log path under load.

## Phase W12 (Limited WotLK Auto-Exchange for Single-Token-Chain)

Date: 2026-05-15

### Implementation

Added an independent WotLK auto queue path (separate from TBC queue):

- WotLK auto queue gate:
  - `BotTokenExchanger.WotlkAutoExchangeEnable = 1`
- WotLK token enqueue condition:
  - token exists in loaded WotLK `single_token_chain` resolver set only.
- WotLK auto processing:
  - reuses WotLK resolver/exchange path.
  - capped to at most one resolved token per bot pass (`maxPerBotPerPass=1`).
- TBC queue/processing path unchanged.
- No SQL added in hot item-store path.

### Validation A: Dry-Run Auto

Config:

- `WotlkAutoExchangeEnable=1`
- `WotlkExchangeEnable=1`
- `WotlkDryRun=1`

Test bot: Rogue `Bredu` (GUID `6849`).

Injected:

- valid WotLK token `40618`
- non-WotLK token `29434` (control)

Persisted DB result after auto window:

- `40618 = 1`
- `29434 = 1`
- no `39561` reward added

Conclusion:

- dry-run auto caused no inventory mutation.
- non-WotLK control token did not trigger WotLK auto mutation.

### Validation B: Real Auto

Config:

- `WotlkAutoExchangeEnable=1`
- `WotlkExchangeEnable=1`
- `WotlkDryRun=0`

Test bot: Rogue `Aelenai` (GUID `7829`).

Before:

- no `40618`
- no `39561`
- no `52025`

Injected one valid token:

- `40618 x1`

Persisted DB result after auto window:

- `40618` removed
- `39561 = 1` added

Second-pass behavior:

- no second exchange (no staged token remained).

Excluded token safety in auto mode:

- injected `52025 x1`
- persisted DB after auto window:
  - `52025 = 1` (unchanged, not processed)
  - `39561` remained unchanged from prior real auto

### W12 Outcome

- Limited WotLK auto exchange works for staged `single_token_chain` scope.
- Excluded structures (`emblem_like`, upgrade chains including `52025`) remain disabled/out of scope.
- TBC auto path is kept independent by separate queue implementation.

### Post-Test Safe Config

Restored in live config file:

- `WotlkAutoExchangeEnable=0`
- `WotlkExchangeEnable=0`
- `WotlkDryRun=1`
- `AllowDebugTargetCommand=0`

### SQL Packaging

Added module SQL packaging for the WotLK staging tables through normal updater flow:

- `sql/world/base/bot_token_exchanger_wotlk_map.sql`
- `sql/world/base/bot_token_exchanger_wotlk_cost.sql`
- `sql/world/updates/2026_05_15_00_bot_token_exchanger_wotlk_tables.sql`

### Runtime Staging Execution

1. Set `BotTokenExchanger.DiscoveryWriteDb = 1` in live module config.
2. Restarted worldserver and verified module config print shows `DiscoveryWriteDb=1`.
3. Ran `.tokenex discover wotlk stage` twice.

Console summary (both runs matched):

- vendors scanned: `25`
- reward rows inspected: `1340`
- rewards discovered: `905`
- cost rows discovered (raw discovery vectors): `1820`
- staged reward rows: `1340`
- staged cost rows: `1820`

### DB Verification After Writes

`acore_world` table counts after staging:

- `bot_token_exchanger_wotlk_map`: `905`
- `bot_token_exchanger_wotlk_cost`: `1095`
- TBC baseline unchanged:
  - `bot_token_exchanger_token_map` where `source_expansion='TBC'`: `170`

Why `1095` cost rows vs `1820` discovered vectors:

- `bot_token_exchanger_wotlk_cost` uses PK (`reward_item_id`, `required_item_id`).
- Discovery reports per-vendor/per-row vectors, but staged table deduplicates repeated `(reward,required_item)` pairs across vendor variants.

### Staged Structure Summary (from persisted tables)

Per-reward structure classification over staged vectors:

- `single_token_chain`: `380`
- `single_emblem_like`: `335`
- `token_plus_prior_armor_upgrade`: `190`

Required item roles (`bot_token_exchanger_wotlk_cost`):

- `token`: `570`
- `emblem_like`: `335`
- `prior_armor`: `190`

### Representative Tier Samples (name-inferred)

- T7-like (`Heroes'`): present
  - e.g. `39491 Heroes' Frostfire Circlet` (vendor `28995`, extcost `2487`)
- T7.5-like (`Valorous`): present
  - e.g. `40416 Valorous Frostfire Circlet` (vendor `28995`, extcost `2514`)
- T8-like (`Conqueror's`): present
  - e.g. `46115 Conqueror's Darkruned Helmet` (vendor `34252`, extcost `2669`)
- T10-like (`Sanctified`): present
  - e.g. `51127 Sanctified Scourgelord Helmet` (vendor `38316`, extcost `2797`)
- `Triumphant` name-family was not present in staged map rows on this PTR snapshot.

### Idempotency

- Ran `.tokenex discover wotlk stage` a second time with `DiscoveryWriteDb=1`.
- Post-run counts remained stable:
  - map `905`
  - cost `1095`
- No duplicate growth observed.

### Safety / Gate State

- WotLK exchange remains disabled by architecture (resolver path still TBC-only).
- `.tokenex status` during W5 still reported `LoadedMappingCount: 170` (TBC mapping load unchanged).
- After staging verification, set `BotTokenExchanger.DiscoveryWriteDb` back to `0`.

## Phase W6 (WotLK Read-Only Resolver: single_token_chain only)

Date: 2026-05-15

### Scope Implemented

- Added a separate WotLK resolver cache/load path.
- Read-only command surface added:
  - `.tokenex resolve wotlk item <tokenItemId>`
  - `.tokenex resolve wotlk selected`
- No WotLK exchange logic added.
- No WotLK resolver entries are fed into TBC exchange/auto-exchange paths.

### WotLK Resolver Load Rule (strict)

Loaded only rows where:

- `bot_token_exchanger_wotlk_map.source_expansion = 'WOTLK'`
- `bot_token_exchanger_wotlk_map.status = 'staged'`
- exactly one cost row exists for the reward
- that cost row is:
  - `required_item_role = 'token'`
  - `required_count = 1`

This excludes:

- `single_emblem_like`
- `token_plus_prior_armor_upgrade`
- all multi-cost rewards

### Runtime Validation

Build/install:

- `cmake --build build --target worldserver -j"$(nproc)"` passed
- `cmake --install build` passed

Live status after restart:

- `.tokenex status` reported:
  - `LoadedMappingCount: 170` (TBC unchanged)
  - `LoadedWotlkMappingCount: 380`
  - `DiscoveryWriteDb: 0`

Interpretation:

- `380` equals staged `single_token_chain` reward count.
- Resolver exposure is limited to single-token-chain only.

### Command Validation Notes

- `.tokenex resolve wotlk item 40618` and `.tokenex resolve wotlk item 52025` both reached the new command path and correctly enforced bot selection (`Select a Playerbot first.`) in console context.
- `.tokenex resolve wotlk selected` also enforced selection safely.
- No inventory mutation occurred (resolver-only flow).
- Existing resolver path remained intact (`LoadedMappingCount: 170` still reported for TBC).

### Safety Conclusion

- W6 adds read-only WotLK resolution support for `single_token_chain` staging data only.
- WotLK exchange remains disabled.
- Emblem and upgrade structures remain excluded from resolver load and processing.

## Phase W7 (Runtime Read-Only Validation on Live Bots)

Date: 2026-05-15

### Test Token Set (from staged DB)

Representative single-token-chain token IDs used:

- `40610` Chestguard of the Lost Conqueror
- `40611` Chestguard of the Lost Protector
- `40618` Helm of the Lost Vanquisher

Excluded-structure validation token:

- `52025` Vanquisher's Mark of Sanctification (upgrade-chain structure, not single-token-chain)

### Runtime Test Helpers Added (read-only)

To allow console-driven per-bot testing without selected-player context:

- `.tokenex resolve wotlk bot <botName>`
- `.tokenex resolve wotlk botitem <botName> <tokenItemId>`

Both call the same read-only resolver path and do not exchange/remove/add items.

### Live Bot Matrix Tested

Bots/classes tested:

- `Aihthukul` (Warrior, class 1, Horde)
- `Alenlestis` (Paladin, class 2, Horde)
- `Aenstus` (Rogue, class 4, Horde)
- `Aalleen` (Priest, class 5, Alliance)
- `Admehan` (Shaman, class 7, Alliance)
- `Aaren` (Druid, class 11, Horde)

Commands run:

- `.tokenex resolve wotlk botitem Aihthukul 40618`
- `.tokenex resolve wotlk botitem Alenlestis 40610`
- `.tokenex resolve wotlk botitem Aenstus 40618`
- `.tokenex resolve wotlk botitem Aalleen 40618`
- `.tokenex resolve wotlk botitem Admehan 40611`
- `.tokenex resolve wotlk botitem Aaren 40618`
- `.tokenex resolve wotlk botitem Aaren 52025`

### Observed Output Behavior

For each tested command, output included:

- bot name
- class + class mask
- faction
- detected spec/role
- token id/name
- staged candidate count
- filtered candidate count
- explicit skip reasons (class mask mismatch / `CanUseItem()` failures)

Representative result:

- `40618` on `Aalleen`:
  - `7 staged candidates, 0 passed safe filters`
  - all candidates safely skipped with detailed reasons

Excluded structure check:

- `52025`:
  - `No WOTLK staged single-token-chain resolver entries found for token item 52025.`
  - confirms upgrade-chain rows are not exposed in W6/W7 resolver scope

### Safety/Regression Checks

- No exchange action occurred.
- No token add/remove or reward add path executed.
- Inventory check for tested token IDs on tested bots remained unchanged (no rows before, no rows after).
- TBC resolver path still functional:
  - `.tokenex resolve bot Aaren` returned expected TBC-safe message (`no staged token items in bags`) and did not error.

### W7 Outcome

- WotLK read-only resolver path is running on live Playerbots with strict single-token-chain gating.
- Current tested bots produced safe no-match outcomes (0 filtered candidates), with clear reasoned logging.
- No ambiguity safety was weakened and no exchange behavior was enabled.

## Phase W8 (WotLK Tier-Family Role Classification, Read-Only)

Date: 2026-05-15

### Scope

- Added WotLK-specific role-family classification for `single_token_chain` resolver rows only.
- Applied only to WotLK read-only resolver path:
  - `.tokenex resolve wotlk item ...`
  - `.tokenex resolve wotlk selected`
  - `.tokenex resolve wotlk bot...`
- TBC resolver/classifier path unchanged.
- No exchange behavior added.

### Families/Signals Used

Conservative family-name signals plus slot-name distinctions, only when clearly class-appropriate:

- `Heroes'`, `Valorous`, `Conqueror's`, `Sanctified`
- class families such as:
  - `Dreamwalker`, `Nightsong`, `Bonescythe`, `Frostfire`
  - `Scourgeborne`, `Dreadnaught`, `Siegebreaker`
  - `Redemption`, `Aegis`
  - `Earthshatter`, `Worldbreaker`
  - `Plagueheart`, `Terrorblade`, `Cryptstalker`, `Scourgestalker`, `Darkruned`, `Deathbringer`

Role disambiguation by slot tokens where safe (examples):

- `Faceguard/Chestguard/Legguards/Handguards/Shoulderguards` -> tank
- `Battleplate/Breastplate/Helmet/Legplates/Gauntlets/Shoulderplates` -> melee_dps
- healer/caster slot forms for class-specific families (`Cover` vs `Headpiece` vs `Headguard`, `Tunic/Chestpiece/Breastplate`, etc.)

### WotLK-Only Read-Only CanUseItem Fallback

Observed W7 issue: many correct class-family rewards were blocked by `CanUseItem()` (likely level-gated bots).

For WotLK read-only resolution only, added fallback behavior:

- if strict safe filtering yields zero candidates,
- keep class/faction-safe armor candidates even when `CanUseItem()` fails,
- then apply WotLK hybrid role filtering.

This fallback:

- is read-only,
- is WotLK-only,
- does not affect TBC filtering or any exchange path.

### Runtime Validation (live)

Representative commands run:

- `.tokenex resolve wotlk botitem Aalruna 40618` (Warrior)
- `.tokenex resolve wotlk botitem Aevaette 40610` (Paladin)
- `.tokenex resolve wotlk botitem Aenstus 40618` (Rogue)
- `.tokenex resolve wotlk botitem Aeriin 40618` (Priest)
- `.tokenex resolve wotlk botitem Abinaar 40611` (Shaman)
- `.tokenex resolve wotlk botitem Cahkul 40618` (Druid)
- `.tokenex resolve wotlk botitem Cahkul 52025` (upgrade-chain exclusion check)

Observed outcomes:

- Warrior (`Aalruna`, `40618`): safe no-match (class mismatch only).
- Paladin (`Aevaette`, `40610`): resolved uniquely to `39638 Heroes' Redemption Breastplate`.
- Rogue (`Aenstus`, `40618`): resolved uniquely to `39561 Heroes' Bonescythe Helmet`.
- Priest (`Aeriin`, `40618`): safe no-match (class mismatch only).
- Shaman (`Abinaar`, `40611`): resolved uniquely to `39588 Heroes' Earthshatter Tunic`.
- Druid (`Cahkul`, `40618`): resolved uniquely to `39545 Heroes' Dreamwalker Cover`.
- Upgrade-chain token (`52025`): still excluded with `No WOTLK staged single-token-chain resolver entries...`.

### Explicit Check Requested

- `40618` now resolves for valid Vanquisher classes when evidence supports it:
  - verified on Rogue and Druid samples above.

### Safety / Regression

- No inventory mutation occurred.
- No WotLK exchange enabled.
- No emblem-like or upgrade-chain support added.
- TBC resolver still works (`.tokenex resolve bot Cahkul` remained normal and read-only).

## Release Stabilization Snapshot (2026-05-18)

- WotLK exchange scope remains intentionally limited to staged `single_token_chain`.
- `emblem_like` and `token_plus_prior_armor_upgrade` structures remain discovery/staging-only.
- No architecture changes were introduced in stabilization.
