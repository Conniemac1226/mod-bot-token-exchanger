# WotLK Validation Report (Single-Token-Chain)

Date: 2026-05-17

## Scope

- Command path validated:
  - `.tokenex validate wotlk single`
  - `.tokenex validate wotlk single verbose`
- Scope remains limited to staged WotLK `single_token_chain` rows only.
- No WotLK exchange behavior was enabled/changed in this validation pass.
- Unsupported structures remain excluded:
  - `emblem_like`
  - `token_plus_prior_armor_upgrade`
  - `52025`-style upgrade-chain marks

## Startup / Status Validation

worldserver booted successfully on PTR (no startup crash).

`.tokenex status` reported:

- `LoadedMappingCount: 170` (TBC)
- `LoadedWotlkMappingCount: 380`
- `WotlkSingleTokenChainCount: 60`
- `QueueSize: 0`
- safety gates:
  - `WotlkExchangeEnable: 0`
  - `WotlkDryRun: 1`
  - `WotlkAutoExchangeEnable: 0`
  - `AllowDebugTargetCommand: 0`

## Full Coverage Validation (non-verbose)

Run after all random bots finished logging in (`1006/1006` online):

`.tokenex validate wotlk single`

Summary:

- online bots: `1006`
- staged single-token IDs tested: `60`
- total bot-token evaluations: `60360`
- total unsupported evaluations: `2216`

Per-tier counts:

- `T7_like`: `resolved=3811`, `ambiguous=642`, `no-match=10060`, `unsupported=577`
- `T8_like`: `resolved=11567`, `ambiguous=1884`, `no-match=30180`, `unsupported=1639`

Missing tier/slot/class/spec/role combinations without any resolved result:

- `71`

## Coverage Notes (class/spec/role/faction)

Representative outcomes from per-group coverage:

- Full safe no-resolve pattern for DK DPS specs remains (ambiguous/no-match), e.g.:
  - `DeathKnight|blooddps|melee_dps|Alliance => resolved=0 ambiguous=400 no-match=800 unsupported=0`
  - `DeathKnight|frostdps|melee_dps|Horde => resolved=0 ambiguous=380 no-match=760 unsupported=0`
- Strong resolve coverage exists for pure/clear families (examples):
  - Rogue assas/combat (Alliance/Horde)
  - Mage arcane/fire/frost (Alliance/Horde)
  - Warlock afflic/demo/destro (Alliance/Horde)
  - Hunter beast/marks/surv (Alliance/Horde)
- Hybrid specs still show mixed unresolved buckets where family-role split is not yet deterministic enough.

## Verbose Validation Findings

`.tokenex validate wotlk single verbose` produced full per-token lines across all groups.

Observed result modes in live output:

- `resolved`: expected where class/spec-role family is explicit
- `no-match`: dominant when token class family does not match bot class (expected safe skip)
- `unsupported`: present where role-family classifier could not safely pick a role
- `ambiguous`: present for classes/families with multiple same-safe candidates (not force-resolved)

## Unresolved Families (exact examples from output)

Top unresolved families include:

- `Valorous Shoulderpads of Sanctification`
- `Valorous Gloves of Sanctification`
- `Valorous Robe of Sanctification`
- `Valorous Leggings of Sanctification`
- `Heroes' Dreamwalker Robe`
- `Heroes' Dreamwalker Headpiece`
- `Heroes' Dreamwalker Handguards`
- `Heroes' Dreamwalker Spaulders`
- `Valorous Dreamwalker Robe`
- `Valorous Dreamwalker Handguards`
- `Valorous Dreamwalker Leggings`
- `Valorous Dreadnaught Helmet`
- `Valorous Dreadnaught Battleplate`
- `Valorous Dreadnaught Legplates`
- `Heroes' Dreadnaught Helmet`
- `Heroes' Dreadnaught Battleplate`
- `Heroes' Dreadnaught Legplates`
- `Heroes' Dreadnaught Gauntlets`
- `Conqueror's Terrorblade Helmet`
- `Conqueror's Terrorblade Gauntlets`
- `Conqueror's Terrorblade Breastplate`
- `Conqueror's Terrorblade Pauldrons`
- `Conqueror's Scourgestalker Headpiece`
- `Conqueror's Scourgestalker Handguards`
- `Conqueror's Scourgestalker Tunic`
- `Conqueror's Scourgestalker Legguards`

## Failure Categorization

Based on non-verbose summary + verbose token lines:

- Missing family classifier / unresolved role-family split:
  - `unsupported` bucket (`2216`) and unresolved Sanctification/Dreamwalker/Nightsong/Plagueheart/Dreadnaught families.
- Role mismatch or class/spec mismatch:
  - substantial `no-match` volume, especially off-class token families.
- Ambiguous same-role variants:
  - significant DK DPS and selected Warrior/Priest/Druid/Shaman groups.
- CanUseItem mismatch:
  - still part of safe-filter path (not separately emitted by current verbose line format).
- Faction mismatch:
  - handled by existing safe filtering; no evidence of unsafe cross-faction resolution.

## Classifier Fixes in This Pass

- No resolver classifier code was changed in this validation run.
- Reason: unresolved sets are broad and multi-family; no single low-risk deterministic mapping was verified as safe enough to patch without increasing mis-resolution risk.

## Before/After (this run)

- Before full-coverage rerun (partial population at 321 bots):
  - `19260` evaluations, `T7_like resolved=1213`, `T8_like resolved=3675`
- After full-coverage rerun (1006 bots online):
  - `60360` evaluations, `T7_like resolved=3811`, `T8_like resolved=11567`
- No code changes between these two runs; delta reflects larger online-bot coverage only.

## Readiness Assessment

- WotLK `single_token_chain` path is stable and safe-failing (no forced picks, ambiguity preserved).
- Coverage is substantial but not yet production-complete for all class/spec/family combinations.
- Recommendation before enabling broad live auto:
  - add targeted family-role classifier refinements for unresolved Sanctification/Dreamwalker/Nightsong/Plagueheart/Dreadnaught buckets,
  - keep ambiguity/no-match safety behavior unchanged.

## WotLK Classifier Hardening - Batch 1 (2026-05-17)

### Scope

Batch 1 intentionally limited to three unresolved families:

- Priest: `Sanctification`
- Druid: `Dreamwalker`
- Warrior: `Dreadnaught`

No exchange/autoprocess scope change was made.

### Local-DB Evidence Snapshot

Evidence source: staged WotLK map/cost rows joined with `item_template` stat profiles.

1. Priest Sanctification (`AllowableClass=16`, cloth, dual-set naming per slot)

- Healer-shaped names/stats in staged rows:
  - `Cowl`, `Handwraps`, `Leggings`, `Mantle`, `Raiments`
- Shadow/caster-shaped names/stats in staged rows:
  - `Circlet`, `Gloves`, `Pants`, `Robe`, `Shoulderpads`

2. Druid Dreamwalker (`AllowableClass=1024`, leather, multi-role families)

- Feral/tank-melee names and stat profile:
  - `Cover`, `Raiments`, `Handgrips`, `Headguard`, `Legguards`, `Shoulderpads`
  - profile includes agility/stamina/rating-style stats
- Healer-focused names in staged rows:
  - `Handguards`, `Headpiece`, `Leggings`, `Robe`, `Spaulders`
  - profile includes intellect/spirit/spell-oriented stats
- Caster DPS names in staged rows:
  - `Gloves`, `Mantle`, `Trousers`, `Vestments`

3. Warrior Dreadnaught (`AllowableClass=1`, plate, explicit tank vs melee split)

- Tank subset names/stats:
  - `Battleplate`, `Gauntlets`, `Helmet`, `Legplates`, `Shoulderplates`
- Melee DPS subset names/stats:
  - `Breastplate`, `Handguards`, `Greathelm`, `Legguards`, `Pauldrons`

### Mapping Proposal Applied

- Priest + `Sanctification`:
  - healer: `cowl|handwraps|leggings|mantle|raiments`
  - caster_dps: `circlet|gloves|pants|robe|shoulderpads`
- Druid + `Dreamwalker`:
  - feral core: `cover|raiments|handgrips|headguard|legguards|shoulderpads` -> `tank` only if preferred role is tank, else `melee_dps`
  - healer: `handguards|headpiece|leggings|robe|spaulders`
  - caster_dps: `gloves|mantle|trousers|vestments`
- Warrior + `Dreadnaught`:
  - tank: `battleplate|gauntlets|helmet|legplates|shoulderplates`
  - melee_dps: `breastplate|handguards|greathelm|legguards|pauldrons`

### Code Changes

File changed:

- `src/BotTokenExchangerMgr.cpp`

Changes:

- Added family-specific overrides in `ClassifyWotlkRewardRole` for:
  - `CLASS_PRIEST` + `sanctification`
  - `CLASS_DRUID` + `dreamwalker`
  - `CLASS_WARRIOR` + `dreadnaught`
- Split `CLASS_WARRIOR` and `CLASS_DEATH_KNIGHT` branches so Dreadnaught-specific logic only affects Warrior.

### Validation Before/After

Baseline (before Batch 1 patch, full 1006 bots):

- evaluations: `60360`
- resolved: `15378` (`T7 3811 + T8 11567`)
- ambiguous: `2526` (`T7 642 + T8 1884`)
- no-match: `40240` (`T7 10060 + T8 30180`)
- unsupported: `2216` (`T7 577 + T8 1639`)
- missing combos: `71`

After Batch 1 patch (full 1006 bots):

- evaluations: `60360`
- resolved: `15914` (`T7 4004 + T8 11910`)
- ambiguous: `2472` (`T7 608 + T8 1864`)
- no-match: `40240` (`T7 10060 + T8 30180`)
- unsupported: `1734` (`T7 418 + T8 1316`)
- missing combos: `51`

Delta (after - before):

- resolved: `+536`
- ambiguous: `-54`
- no-match: `0`
- unsupported: `-482`
- missing combos: `-20`

### Families Improved in Batch 1

- `Dreamwalker` and `Dreadnaught` families improved materially (seen in reduced unsupported and missing-combo counts).
- `Sanctification` improved for some class/spec paths, but significant Sanctification unresolved rows remain and need additional narrow follow-up.

### Remaining Broad Unresolved Families (post-Batch 1)

Representative unresolved names still include:

- Sanctification: `Valorous Cowl/Gloves/Leggings/Robe/Shoulderpads of Sanctification`
- Dreadnaught: `Heroes'/Valorous Dreadnaught ...` subsets still partially unresolved
- Scourgestalker, Terrorblade, Deathbringer, Plagueheart, Nightsong families remain in unresolved list


## WotLK Classifier Hardening - Batch 2 (2026-05-17)

### Scope

Batch 2 targeted families only:

- Rogue: `Terrorblade`
- Hunter: `Scourgestalker`
- Warlock: `Plagueheart`, `Deathbringer`
- Mage: `Kirin Tor`, `Bloodmage` (when encountered by classifier path)

No exchange/auto scope changes were made.

### Local-DB Evidence (staged rows)

- `Terrorblade` rows are all `AllowableClass=8` (Rogue), leather, with melee stat profile.
- `Scourgestalker` rows are all `AllowableClass=4` (Hunter), mail, with ranged/agility profile.
- `Plagueheart` and `Deathbringer` rows are `AllowableClass=256` (Warlock), cloth caster profiles.
- `Kirin Tor` rows are `AllowableClass=128` (Mage), cloth caster profiles.
- `Bloodmage` appears in staged data but mostly in sanctified/upgrade families (outside single-token-chain runtime scope); classifier support kept conservative.

### Mapping Proposal Applied

- Rogue:
  - explicit family match `terrorblade -> melee_dps`
- Hunter:
  - explicit family match `scourgestalker -> ranged_dps`
- Warlock:
  - explicit family match `deathbringer` with slot names `hood|gloves|leggings|robe|shoulderpads -> caster_dps`
  - existing caster family widened to include `deathbringer` and `hood|shoulderpads`
- Mage:
  - explicit family match `kirin tor` with `hood|gauntlets|leggings|shoulderpads|tunic -> caster_dps`
  - conservative `bloodmage` slot pattern added (`hood|gloves|leggings|robe|shoulderpads -> caster_dps`)
  - existing caster family widened to include `kirin tor|bloodmage` and `hood|gauntlets|shoulderpads|tunic`

### Code Changes

File changed:

- `src/BotTokenExchangerMgr.cpp`

Changes were limited to `ClassifyWotlkRewardRole` family-name matching.

### Validation Before/After (Batch 2)

Baseline (after Batch 1):

- resolved: `15914`
- ambiguous: `2472`
- no-match: `40240`
- unsupported: `1734`
- missing combos: `51`

After Batch 2:

- resolved: `15954` (`T7 4008 + T8 11946`)
- ambiguous: `2456` (`T7 593 + T8 1863`)
- no-match: `40240`
- unsupported: `1710` (`T7 429 + T8 1281`)
- missing combos: `51`

Delta (after - before):

- resolved: `+40`
- ambiguous: `-16`
- no-match: `0`
- unsupported: `-24`
- missing combos: `0`

### Batch 2 Outcome

- Modest but clean improvement from family-specific naming gaps.
- Safety behavior unchanged (no forced pick, ambiguity/no-match safety intact).
- Missing-combo count did not move in this batch, indicating broader remaining work is outside these narrow family pattern misses.

### Remaining Unresolved Families (representative)

Still prominent in unresolved list:

- `Conqueror's/Valorous Scourgestalker ...`
- `Conqueror's/Valorous Terrorblade ...`
- `Heroes'/Valorous Plagueheart ...`
- `Conqueror's/Valorous Deathbringer ...`
- plus Sanctification/Dreamwalker/Dreadnaught residual subsets and other broader families


## WotLK Classifier Diagnosis - Batch 3 (2026-05-17)

### Objective

Diagnose remaining unresolved buckets before broad classifier edits, using:

- `.tokenex validate wotlk single unresolved`
- `.tokenex validate wotlk single`

No exchange/autopath behavior was changed.

### Diagnostic Command Added

- Added read-only command: `.tokenex validate wotlk single unresolved`
- Output groups unresolved evaluations and reports top 10 buckets with:
  - result type
  - family
  - class/spec/role
  - slot
  - token id/name
  - candidate rewards
  - classifier output per candidate
  - fail step and normalized reason

### Batch 3 Targeted Fixes (verified, minimal)

File changed:

- `src/BotTokenExchangerMgr.cpp`

Classifier adjustments:

- Paladin: treat `helm` as melee keyword (`Aegis Helm` no longer falls to unsupported).
- Shaman: evaluate melee branch before healer branch so `war-kilt` is not swallowed by generic `kilt` healer match.
- Priest: add family-specific `faith` split:
  - healer: `circlet|leggings|raiment|mantle|handwraps|cowl`
  - caster_dps: `crown|pants|robe|shoulderpads|gloves`

These were driven directly by top unresolved bucket evidence from live output.

### Full-Population Results (1006 bots, 60 tokens, 60360 evals)

Batch 2 baseline:

- resolved: `15954`
- ambiguous: `2456`
- no-match: `40240`
- unsupported: `1710`
- missing combos: `51`

After Batch 3 patch:

- resolved: `17024` (`+1070`)
- ambiguous: `1990` (`-466`)
- no-match: `40240` (`0`)
- unsupported: `1106` (`-604`)
- missing combos: `39` (`-12`)

Per-tier delta:

- `T7_like`: resolved `3811 -> 4385`, ambiguous `642 -> 476`, unsupported `577 -> 169`
- `T8_like`: resolved `11567 -> 12639`, ambiguous `1884 -> 1514`, unsupported `1639 -> 937`

### Top Remaining Unresolved Buckets (post-patch)

Most frequent unresolved buckets are now concentrated in shared-token hybrid families, primarily Shaman elemental against Worldbreaker/Earthshatter triplets:

- `Conqueror's Scourgestalker Headpiece` -> candidates `{Faceguard, Helm, Headpiece}` with role mismatch.
- `Conqueror's Scourgestalker Legguards` -> candidates `{Kilt, War-Kilt, Legguards}` with role mismatch.
- `Conqueror's Scourgestalker Spaulders` -> candidates `{Shoulderpads, Spaulders, Shoulderguards}` with role mismatch.
- Equivalent `Heroes'/Valorous Dreadnaught` unresolved buckets for elemental shaman versus Earthshatter role triplets.

### Root Causes (Batch 3 diagnosis)

- Primary: role mismatch in mixed-role shared-token families (not class-mask mismatch).
- Secondary: incomplete slot-role splits for some family triplets (`helm/headpiece/faceguard`, `kilt/war-kilt/legguards`, `shoulderpads/spaulders/shoulderguards`).
- Safety filters (`CanUseItem`, faction, ambiguity guards) remain active and unchanged.

### Remaining Families to Prioritize Next

- `Scourgestalker` / `Worldbreaker` shared-token buckets (especially elemental shaman role-paths)
- `Plagueheart` / `Faith` and `Sanctification` remnants
- `Dreadnaught` shoulder/head/leg split edge cases
- `Nightsong` / `Dreamwalker` unresolved subsets

### Safety Status

- No forced-first resolution.
- No ambiguity bypass.
- No unsupported-structure enablement.
- TBC behavior unchanged.

## WotLK Classifier Hardening - Batch 4 (2026-05-17)

### Scope

- Targeted only Shaman Protector-family role classification in WotLK `single_token_chain` resolver.
- No exchange/autoprocess/unsupported-structure changes.

### Shaman Mapping Proposal (local DB evidence)

Source: staged `bot_token_exchanger_wotlk_map` + `bot_token_exchanger_wotlk_cost` + `item_template` for `AllowableClass & 64` and single-token-chain rows.

Families covered by staged rows:

- `Earthshatter` (T7/T8-like)
- `Worldbreaker` (T8-like)

Triplet role split observed consistently by name and stat profile:

- Head: `Faceguard` (melee_dps), `Headpiece` (healer), `Helm` (caster_dps)
- Legs: `War-Kilt` (melee_dps), `Kilt` (healer), `Legguards` (caster_dps)
- Shoulder: `Spaulders` (melee_dps), `Shoulderpads` (healer), `Shoulderguards` (caster_dps)

Additional slot evidence:

- `Hauberk` -> melee_dps
- `Chestguard` -> healer
- `Tunic` -> caster_dps
- `Grips` -> melee_dps
- `Handguards` -> healer
- `Gloves` -> caster_dps

### Code Changes

File changed:

- `src/BotTokenExchangerMgr.cpp`

Shaman-only classifier adjustments:

- Added explicit caster matches: `helm`, `legguards`, `shoulderguards`, `gloves`.
- Healer branch changed to prefer `handguards` over `gloves`.
- Kept melee branch for `faceguard`, `war-kilt`, `spaulders`, `grips`, `hauberk`.
- Maintained safety behavior; no ambiguity bypass.

### Validation Results

Baseline (Batch 3):

- resolved: `17024`
- ambiguous: `1990`
- no-match: `40240`
- unsupported: `1106`
- missing combos: `39`

After Batch 4:

- resolved: `17370` (`+346`)
- ambiguous: `2208` (`+218`)
- no-match: `40240` (`0`)
- unsupported: `542` (`-564`)
- missing combos: `33` (`-6`)

Per-tier after Batch 4:

- `T7_like`: resolved=`4477` ambiguous=`527` no-match=`10060` unsupported=`26`
- `T8_like`: resolved=`12893` ambiguous=`1681` no-match=`30180` unsupported=`516`

### Remaining Shaman Status

- Shaman elemental/caster unresolved Protector triplets are no longer top unresolved buckets.
- Shaman groups now show full-safe support in this run (notably `Shaman|elem|caster_dps|Alliance/Horde` now `unsupported=0`).

### New Top Unresolved Buckets (post-Shaman)

- Warrior Protector shared-token families:
  - `Conqueror/Valorous Scourgestalker Tunic` for `Warrior fury` -> ambiguous same-role (`Battleplate` vs `Breastplate`)
  - `Conqueror/Valorous Scourgestalker Headpiece/Spaulders/Tunic` for `Warrior prot` -> role mismatch
- DK Vanquisher families remain significant ambiguity buckets (`Darkruned` tank/melee overlap).

### Safety Confirmation

- No forced first-candidate.
- No ambiguity protection disabled.
- No emblem/upgrade-chain structures enabled.
- TBC behavior unchanged.

## WotLK Classifier Hardening - Batch 5 (2026-05-17)

### Scope

- Targeted **Death Knight only** for WotLK `single_token_chain` ambiguity.
- No exchange/autoprocess/unsupported-structure scope changes.

### DK Evidence (local staged data)

Queried staged DK-only rows (`AllowableClass & 32`) from local DB:

- Families present in staged single-token-chain:
  - `Scourgeborne` (T7-like)
  - `Darkruned` (T8-like)
- Token pairs consistently map to two DK variants per slot:
  - tank-oriented suffixes: `Faceguard`, `Chestguard`, `Handguards`, `Legguards`, `Pauldrons`
  - DPS-oriented suffixes: `Helmet`, `Battleplate`, `Gauntlets`, `Legplates`, `Shoulderplates`

### Root Cause of DK Ambiguity

Primary root cause was architectural in classifier flow:

- `CLASS_DEATH_KNIGHT` was **not included** in `UsesRoleFiltering`.
- Result: DK candidates bypassed role pruning and remained multi-candidate per token, yielding ambiguity.

Secondary classifier issue:

- DK tank shoulder suffix in staged data is `Pauldrons`, but DK tank branch did not include it.

### DK Mapping Proposal Applied

- DK tank patterns:
  - `faceguard|chestguard|handguards|legguards|pauldrons`
- DK melee_dps patterns:
  - `helmet|battleplate|gauntlets|legplates|shoulderplates`

### Code Changes

File changed:

- `src/BotTokenExchangerMgr.cpp`

Changes:

1. Role-filter activation:

- `UsesRoleFiltering` now includes `CLASS_DEATH_KNIGHT`.

2. DK classifier suffix correction:

- tank list updated to include `pauldrons` (and no dependency on unused `shoulderguards` for DK sets).

No other class classifiers were changed.

### Validation (1006 bots, 60 tokens, 60360 evals)

Batch 4 baseline:

- resolved: `17370`
- ambiguous: `2208`
- no-match: `40240`
- unsupported: `542`
- missing combos: `33`

After Batch 5:

- resolved: `19254` (`+1884`)
- ambiguous: `288` (`-1920`)
- no-match: `40240` (`0`)
- unsupported: `578` (`+36`)
- missing combos: `3` (`-30`)

Per-tier after Batch 5:

- `T7_like`: resolved=`4957` ambiguous=`48` no-match=`10060` unsupported=`25`
- `T8_like`: resolved=`14297` ambiguous=`240` no-match=`30180` unsupported=`553`

### DK Outcome

- DK ambiguous buckets were eliminated in this run:
  - `DeathKnight|blooddps|melee_dps|Alliance/Horde`: ambiguous `0`, resolved `>0`
  - `DeathKnight|frostdps|melee_dps|Alliance/Horde`: ambiguous `0`, resolved `>0`
  - `DeathKnight|unholydps|melee_dps|Alliance/Horde`: ambiguous `0`, resolved `>0`

### Remaining Top Unresolved Families

Post-Batch-5 unresolved top buckets are now dominated by non-DK families:

- Warrior Protector/Siegebreaker (role mismatch + same-role chest ambiguity)
- Paladin Conqueror/Valorous chest family (`Aegis` role mismatch buckets)

### Safety Confirmation

- No forced first-candidate behavior.
- No ambiguity bypass.
- No emblem/upgrade-chain support enabled.
- No TBC behavior change.

## WotLK Classifier Hardening - Batch 6 (2026-05-17)

### Scope

- Targeted only Warrior + Paladin WotLK family-role splits.
- No WotLK exchange enablement changes.
- No TBC behavior changes.

### Evidence-Based Mapping Applied

From staged single-token-chain rows + item stats:

1. Warrior `Siegebreaker` (Protector shared tokens)
- tank: `breastplate|handguards|greathelm|legguards|pauldrons`
- melee_dps: `battleplate|gauntlets|helmet|legplates|shoulderplates`

2. Paladin `Aegis` (Conqueror shared tokens)
- tank: `faceguard|breastplate|handguards|legguards|shoulderguards`
- melee_dps: `helm|battleplate|gauntlets|legplates|shoulderplates`
- healer: `headpiece|tunic|gloves|greaves|spaulders`

### Code Changes

File changed:

- `src/BotTokenExchangerMgr.cpp`

Added family-specific overrides in `ClassifyWotlkRewardRole` for:

- `CLASS_WARRIOR` + `siegebreaker`
- `CLASS_PALADIN` + `aegis`

### Validation

After patch (`1006` bots, `60` token IDs, `60360` evaluations):

- resolved: `19750`
- ambiguous: `150`
- no-match: `40240`
- unsupported: `220`
- missing combos: `3`

Compared to Batch 5 baseline:

- resolved `19254 -> 19750` (`+496`)
- ambiguous `288 -> 150` (`-138`)
- unsupported `578 -> 220` (`-358`)
- no-match unchanged

### Remaining Top Unresolved Buckets

Post Warrior/Paladin fix, top unresolveds are now Druid-focused (Nightsong/Dreamwalker family split ambiguity/mismatch), e.g.:

- `Conqueror's Terrorblade Breastplate` (Druid resto ambiguous)
- `Valorous Nightsong Robe` (Druid resto ambiguous)
- `Heroes'/Valorous Dreamwalker Headpiece` (Druid balance/feral unresolved)

### Safety

- No forced first-candidate behavior.
- Ambiguity protections preserved.
- Unsupported structures remain excluded.

## WotLK Classifier Hardening - Batch 7 (2026-05-17)

### Scope

- Targeted only Druid WotLK staged single-token-chain families:
  - `Dreamwalker`
  - `Nightsong`
- `Lasherweave` not present in staged single-token-chain rows in this dataset.

### Druid Evidence Summary

Local staged row inspection (`AllowableClass & 1024`) showed clear family-specific split patterns:

1. Dreamwalker
- feral/tank-melee: `raiments|handgrips|headguard|legguards|shoulderpads`
- healer/resto: `handguards|headpiece|leggings|robe|spaulders`
- caster/balance: `cover|gloves|mantle|trousers|vestments`

2. Nightsong
- feral/tank-melee: `raiments|handgrips|headguard|legguards|shoulderpads`
- healer/resto: `handguards|headpiece|leggings|vestments|spaulders`
- caster/balance: `cover|gloves|mantle|trousers|robe`

Key collision fixes from evidence:

- `cover` moved out of feral path into caster path.
- Nightsong chest/shoulder suffixes needed family-specific role routing (cannot safely reuse Dreamwalker chest mapping).

### Code Changes

File changed:

- `src/BotTokenExchangerMgr.cpp`

Changes in `ClassifyWotlkRewardRole`:

- Updated existing `dreamwalker` mapping with corrected `cover` routing.
- Added explicit `nightsong` mapping block with family-specific healer/caster chest/shoulder separation.

No non-Druid classifier changes in this batch.

### Validation Results

Note: runtime bot population changed from prior `1006` baseline to `1355` in this run, so absolute totals are not directly comparable. Results below are full-population for this run.

` .tokenex validate wotlk single `:

- online bots: `1355`
- evaluations: `81300`
- resolved: `27100` (`T7_like=6775`, `T8_like=20325`)
- ambiguous: `0`
- no-match: `54200`
- unsupported: `0`
- missing combos: `0`

` .tokenex validate wotlk single unresolved `:

- unresolved bucket groups: `0`
- unresolved evaluations: `0`

### Remaining Items

- Resolver safety remains intact (no forced candidate, no ambiguity bypass).
- Validation output still prints an "unresolved family" tail list even when unresolved buckets are zero; this appears to be a reporting artifact in summary printing, not a resolver mismatch.

## Release Stabilization Snapshot (2026-05-18)

Runtime sanity was verified before trusting validation output:

- single `worldserver` process
- build/live worldserver SHA256 match (`scripts/verify-live-binary.sh` PASS)

Live validation snapshot:

- `.tokenex validate wotlk single unresolved`: `0` unresolved bucket groups, `0` unresolved evaluations.
- `.tokenex validate wotlk single`: `missing combos = 0` on current online population.

Notes:

- Safety gates unchanged (`WotlkExchangeEnable=0`, `WotlkDryRun=1`, `WotlkAutoExchangeEnable=0` in service defaults).
- Unsupported WotLK structures remain excluded from exchange scope.
- Validation summary still prints an "unresolved family" list even when unresolved buckets are zero; this is a reporting artifact and does not indicate active resolver failures.
