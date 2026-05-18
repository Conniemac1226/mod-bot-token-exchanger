# TBC Resolver Validation Report

## Date
- 2026-05-17 (PTR)

## Commands
- `.tokenex status`
- `.tokenex validate tbc`
- `.tokenex validate tbc unresolved`

## Status Snapshot
- `LoadedMappingCount=170`
- `LoadedWotlkMappingCount=380`
- `WotlkSingleTokenChainCount=60`
- `QueueSize=0`

## Full-Matrix TBC Validation (all online Playerbots at run time)
- Online bots: `1355`
- Staged TBC token IDs tested: `30`
- Total bot-token evaluations: `40650`
- Resolved: `1975`
- Ambiguous: `305`
- No-match: `38250`
- Unsupported: `120`
- Missing tier/slot/class/spec/role combinations without any resolved result: `29`

## Unresolved Diagnostic Summary
- Unresolved bucket groups: `1962`
- Unresolved evaluations: `38675`
- Top unresolved buckets were dominated by expected off-class checks (`class mask mismatch`) for tokens such as:
  - `30237 Chestguard of the Vanquished Defender`
  - `30243 Helm of the Vanquished Defender`
  - `30240 Gloves of the Vanquished Defender`
  - `30246 Leggings of the Vanquished Defender`
  - `30249 Pauldrons of the Vanquished Defender`
  - `30241 Gloves of the Vanquished Hero`
  - `30238 Chestguard of the Vanquished Hero`
  - `30244 Helm of the Vanquished Hero`
  - `30247 Leggings of the Vanquished Hero`
  - `30250 Pauldrons of the Vanquished Hero`

## Notes
- Validation path is read-only and does not mutate inventory.
- Resolver safety behavior (ambiguous/no-match/unsupported skip) remains intact.
- No exchange behavior changes were made in this validation implementation.

## Hardening Follow-Up (2026-05-17)

### Config
- Added `BotTokenExchanger.WotlkTelemetryEnable = 1` to live config:
  - `/home/cbur/azeroth-server/etc/modules/bot_token_exchanger.conf`
- Dist config already had this key.

### Unresolved Diagnostics Improvement
- `validate tbc unresolved` now suppresses expected off-class `class mask mismatch` no-match buckets to surface actionable issues.

### Top Actionable Buckets (post-filter)
- Warrior Fury on `Destroyer` families:
  - `Destroyer Handguards`, `Destroyer Legguards`, `Destroyer Shoulderguards`
  - reason: `unsupported` / `role mismatch`
  - fail step: `BuildHybridCandidates`
  - root cause: both candidates classified `tank` (missing Warrior Destroyer split)
- Priest Holy on `Avatar/Incarnate` families:
  - `Destroyer*`/`Warbringer*` token outputs resolving to paired priest sets
  - reason: `ambiguous` / `multiple same-role candidates`
  - fail step: `post-role filter ambiguity`
  - root cause: both paired variants classified `healer`

### Mapping Proposal Applied
- Warrior `Destroyer` family role split by suffix:
  - `tank`: `chestguard`, `handguards`, `greathelm`, `legguards`, `shoulderguards`
  - `melee_dps`: `breastplate`, `gauntlets`, `battle-helm`, `greaves`, `shoulderblades`
- Priest `Avatar/Incarnate/Absolution` role split by suffix:
  - `healer`: `cowl`, `raiment`, `handguards`, `leggings`, `mantle`, `collar`
  - `caster_dps`: `hood`, `shroud`, `gloves`, `breeches`, `wings`, `tunic`

### Code Changes
- `src/BotTokenExchangerMgr.cpp`
  - unresolved report filter for off-class no-match buckets
  - Warrior Destroyer class-aware mapping
  - Priest Avatar/Incarnate/Absolution class-aware mapping

### Validation Snapshot After Patch
- Run during online bot ramp (snapshot at command time):
  - Online bots: `1232`
  - Staged TBC token IDs tested: `30`
  - Total evaluations: `36960`
  - Resolved: `1928`
  - Ambiguous: `154`
  - No-match: `34850`
  - Unsupported: `28`
  - Missing combos: `20`
- Unresolved diagnostic at same snapshot:
  - bucket groups: `52`
  - unresolved evaluations: `182`
  - top remaining buckets are mostly same-role ambiguities (Shaman/Druid/Priest paired set variants), safety-preserved.

## Batch Continuation (2026-05-17)

### Additional Evidence Queries
- Verified with local `item_template` stats:
  - `Cataclysm Gauntlets` and `Cyclone Breastplate` are melee-leaning.
  - `Cataclysm Handgrips` and `Cyclone Hauberk` are caster-leaning.

### Additional Mapping Applied
- Shaman (`cyclone`/`cataclysm`) refined:
  - `melee_dps`: `gauntlets`, `breastplate`, `helm`, `war-kilt`, `greaves`, `shoulderplates`
  - `healer`: `chestguard`, `gloves`, `headdress`, `headguard`, `kilt`, `leggings`, `shoulderguards`, `faceguard`, `legguards`, `shoulderpads`
  - `caster_dps`: `hauberk`, `handgrips`, `grips`, `chestpiece`, `handguards`, `headpiece`, `leggings`, `spaulders`
- Priest (`incarnate`/`avatar`/`absolution`) refined:
  - `Light-*` forced to `healer`
  - `Soul-*` forced to `caster_dps`

### Latest Runtime Snapshot (during login wave)
- `.tokenex validate tbc`:
  - Online bots: `180`
  - Token IDs tested: `30`
  - Evaluations: `5400`
  - Resolved: `378`
  - Ambiguous: `30`
  - No-match: `4990`
  - Unsupported: `2`
  - Missing combos: `54`
- `.tokenex validate tbc unresolved`:
  - bucket groups: `24`
  - unresolved evaluations: `25`
  - top issues:
    - Priest `Warbringer Legguards` (disc/healer) still ambiguous (`Leggings of the Incarnate` vs `Trousers of the Incarnate`).
    - Shaman elemental on champion token leg/shoulder/head slots still `role mismatch` (all candidates classed healer/melee, no caster winner).

## Targeted Batch: Shaman/Priest Follow-up (2026-05-17)

### Mapping Proposal
- Shaman `Cyclone/Cataclysm` leg split:
  - `Cyclone/Cataclysm War-Kilt` => `melee_dps`
  - `Cyclone/Cataclysm Kilt` => `healer`
  - `Cyclone/Cataclysm Legguards/Leggings` => `caster_dps`
- Druid `Malorne/Nordrassil` pair disambiguation for resto/caster overlap:
  - healer: `Chestguard`, `Crown`, `Handguards`, `Legguards`, `Life-Kilt`
  - caster_dps: `Chestpiece`, `Antlers`, `Gloves`, `Britches`, `Headpiece`
- Priest Incarnate leg pair rule kept:
  - `Trousers of the Incarnate` => `healer`
  - `Leggings of the Incarnate` => `caster_dps`

### Validation After Patch
- `.tokenex validate tbc` (stable run after full login):
  - Online bots: `1355`
  - Staged token IDs: `30`
  - Evaluations: `40650`
  - Resolved: `2186`
  - Ambiguous: `110`
  - No-match: `38330`
  - Unsupported: `24`
  - Missing combos: `20`
- `.tokenex validate tbc unresolved`:
  - bucket groups: `29`
  - unresolved evaluations: `134`
  - top unresolved remain:
    - Druid resto same-role ambiguities on `Malorne/Nordrassil` pairs
    - Shaman `Cyclone Kilt` elem/resto split still unresolved in resolver path

### Outcome
- Safety posture preserved; no ambiguity bypasses added.
- Zero-unresolved target not reached in this batch.
- Remaining buckets require resolver-path-specific disambiguation (current generic/class-aware overrides are not yet eliminating these buckets in final hybrid filtering).

## Targeted Batch: Shaman Elemental/Caster + Priest Incarnate (2026-05-18)

### Local DB Evidence Used
- Shaman Cyclone/Cataclysm head/shoulder/leg items were re-queried from `item_template` with slot+stats.
- Priest Incarnate/Absolution leg+shoulder pairs were re-queried from `item_template`.

Shaman evidence highlights:
- `Cataclysm Legplates`: AP/AGI/STR profile => melee.
- `Cataclysm Leggings`: INT/SPIRIT + spell hit/crit profile => caster.
- `Cataclysm Legguards`: INT/SPIRIT only profile => healer.
- `Cyclone Helm`: melee profile.
- `Cyclone Headdress`: healer profile.
- `Cyclone Faceguard`: caster profile.
- `Cyclone Shoulderplates`: melee profile.
- `Cyclone Shoulderpads/Shoulderguards`: caster/healer split by stats.

Priest evidence highlights:
- `Trousers of the Incarnate`: healer-leaning (higher spirit, no spell-crit stat line).
- `Leggings of the Incarnate`: caster-leaning (includes spell-crit stat line).
- `Mantle of Absolution` vs `Shoulderpads of Absolution` remain healer vs caster split.

### Mapping Proposal Applied
- Added family-specific Shaman overrides under `CLASS_SHAMAN` before broad suffix fallbacks:
  - Cyclone explicit split (`helm/shoulderplates/war-kilt` melee; `headdress/kilt` healer; `faceguard/legguards/shoulderguards/shoulderpads` caster)
  - Cataclysm explicit split (`helm/legplates/shoulderplates` melee; `legguards/shoulderguards` healer; `headguard/headpiece/leggings/shoulderpads` caster)
- Preserved and extended Priest paired logic:
  - `Light-*` => healer, `Soul-*` => caster
  - `Trousers of the Incarnate` => healer
  - `Leggings of the Incarnate` => caster
  - `Mantle of Absolution` => healer
  - `Shoulderpads of Absolution` => caster

### Validation Snapshot (during login wave)
- `.tokenex validate tbc`
  - Online bots: `1077`
  - Staged token IDs tested: `30`
  - Total evaluations: `32310`
  - Resolved: `1836`
  - Ambiguous: `18`
  - No-match: `30420`
  - Unsupported: `36`
  - Missing combos: `22`
- `.tokenex validate tbc unresolved`
  - Unresolved bucket groups: `21`
  - Unresolved evaluations: `54`

### Targeted Outcome
- Priest Incarnate/Absolution pair rules are in place and did not introduce unsafe behavior.
- Remaining top targeted Shaman blocker is explicit in unresolved output:
  - `Crystalforge Legguards` / Shaman resto: candidates include `Cataclysm Legguards`, but runtime classifier still reports `Cataclysm Legguards -> caster_dps`.
  - This conflicts with local DB stat evidence (healer-leaning), indicating a remaining resolver-path mismatch to debug before further mapping expansion.

### Stop Condition
- Zero-unresolved target not reached in this batch.
- Stopped after verified targeted patch + validation because further changes now require resolver-path debugging (not additional safe suffix guessing).

## Runtime Classifier Mismatch Debug (2026-05-18)

### Root Cause
The observed mismatch (`Cataclysm Legguards` showing `caster_dps` despite explicit healer mapping) was caused by running an out-of-date installed binary.

Evidence:
- Build artifact path is `build/src/server/apps/worldserver` (executable file, not `.../worldserver/worldserver`).
- Before fix, `build` and installed binary differed in size/hash:
  - build: `540303800`
  - installed: `540300240`
- Earlier build/install was run in parallel, which allowed install to copy an older executable while build was still finishing.
- There were also two worldserver instances alive at once, which further confused runtime verification.

### Fix Applied
1. Reinstalled after completed build and then force-synced binary explicitly:
   - copied `build/src/server/apps/worldserver` -> `/home/cbur/azeroth-server/bin/worldserver`
2. Verified binary parity:
   - size/timestamp aligned
   - sha256 matched exactly
3. Killed all running worldserver processes and started exactly one clean instance.

### Result Validation
- `.tokenex validate tbc unresolved` (first clean run snapshot):
  - online bots: `60`
  - unresolved bucket groups: `2`
  - unresolved evaluations: `4`
  - only remaining unresolveds were druid resto ambiguity buckets.
  - **No Shaman Cataclysm/Cyclone misclassification bucket remained.**

- `.tokenex validate tbc` (larger stable run):
  - online bots: `2244`
  - staged token IDs: `30`
  - evaluations: `67320`
  - resolved: `3807`
  - ambiguous: `55`
  - no-match: `63340`
  - unsupported: `118`
  - missing combos: `17`

### Conclusion
The Shaman `Cataclysm Legguards` runtime mismatch was not a classifier-branch logic fault in this patch set; it was an execution artifact from stale runtime binary + duplicate server instances.

## TBC Zero-Unresolved Hardening Batch (2026-05-18, clean hash-verified runtime)

### Sanity Preconditions
- `scripts/verify-live-binary.sh` passed before validation:
  - single `worldserver` process
  - build/live SHA256 match

### Mapping Proposal (this batch)
- **Shaman `Cyclone/Cataclysm`** (class-aware explicit split before generic fallback):
  - `Cyclone Shoulderguards` => healer
  - `Cyclone Shoulderpads` => caster_dps
  - `Cataclysm Headguard` => healer
  - `Cataclysm Headpiece` => caster_dps
- **Druid `Nordrassil/Malorne` feral set pieces**:
  - `Nordrassil Chestplate/Handgrips/Headdress/Feral-Kilt/Feral-Mantle` => `tank` when preferred role is tank, else `melee_dps`
  - `Breastplate/Gauntlets/Greaves of Malorne` => `tank` when preferred role is tank, else `melee_dps`

### Root Causes Confirmed
- Ambiguity/unsupported buckets were caused by role-collision on shared token families, not exchange path bugs.
- Final remaining buckets were all Druid feral tank cases where feral pieces were classified as melee-only.

### Before/After Metrics
- **Before batch (2244 online):**
  - resolved: `3698`
  - ambiguous: `66`
  - unsupported: `106`
  - missing combos: `15`
  - unresolved buckets: `26`
- **After first patch (2244 online):**
  - resolved: `3931`
  - ambiguous: `0`
  - unsupported: `39`
  - missing combos: `15`
  - unresolved buckets: `6`
- **After second patch (1935 online during login wave):**
  - resolved: `3360`
  - ambiguous: `0`
  - unsupported: `0`
  - missing combos: `15`
  - unresolved buckets: `0`

### Current Status
- Achieved for current online population:
  - `ambiguous = 0`
  - `unsupported = 0`
  - `unresolved bucket groups = 0`
- `missing combos = 15` remains and is currently dominated by populations/families without any resolved class/spec/role coverage in the active online set (safe no-match only).
