# mod-bot-token-exchanger

## Architecture

- DB-staged resolver model; no hardcoded reward mappings.
- Runtime source for vendor costs is `npc_vendor.ExtendedCost` + `sItemExtendedCostStore`.
- Separate resolver caches:
  - TBC cache from `bot_token_exchanger_token_map`
  - WotLK cache from `bot_token_exchanger_wotlk_map` + `bot_token_exchanger_wotlk_cost` (`single_token_chain` only)
- Separate auto queues:
  - TBC auto queue (`AutoExchangeEnable`)
  - WotLK auto queue (`WotlkAutoExchangeEnable`)
- Loot-pass callback is Playerbot-only and no-op when disabled.

## Hard Safety Rules

- Never process real players (`OnlyPlayerbots=1` for live usage).
- Keep `AllowDebugTargetCommand=0` except short debugging windows.
- Keep `DiscoveryWriteDb=0` unless intentionally staging mappings.
- No SQL in hot loot-roll paths.
- Keep duplicate-ownership prevention enabled before exchange.
- Keep rollback/store-failure handling intact in shared transaction helper.

## Supported Scope

- TBC:
  - discovery, resolver, real exchange, auto exchange, loot-pass duplicate suppression.
- WotLK:
  - staged discovery model and resolver.
  - exchange + auto exchange only for staged `single_token_chain`.

## Unsupported Scope

- WotLK `emblem_like` costs.
- WotLK `token_plus_prior_armor_upgrade` (including `52025`-style upgrade marks).
- WotLK loot-pass integration.

## Build / Install

```bash
cd /home/cbur/azerothcore-wotlk
cmake --build build --target worldserver -j"$(nproc)"
cmake --install build
```

## Known Risks

- Full-bag/store-failure path should continue periodic live verification under load.
- Hybrid class role/spec inference can still yield safe no-match outcomes in edge gear/spec states.
- WotLK auto should remain limited rollout until additional live observation confirms stability.
- Ambiguous faction variants still need resolver logging and safe skip behavior.
- Hybrid class classification can require spec/role detection and occasional family-name hints.
- The loot-pass callback must stay no-op when caches are unavailable or resolution is ambiguous.
- Queue state must stay bounded and cleared on logout to avoid stale processing.

## Completed Phases

- TBC discovery from runtime DBC and vendor data.
- Staged DB mapping table and resolver cache.
- Dry-run and real exchange flows.
- Auto exchange queue.
- Playerbot loot-pass callback with duplicate ownership suppression.
- PTR beta hybrid-resolution fixes for Warrior, Shaman, and Druid TBC cases.

## Active Blockers

- None for the current TBC release path.
- WotLK support remains intentionally out of scope.

## Main Server Follow-Up (2026-05-15)

- Main-server logs showed repeated safe skips caused by unresolved ambiguity for Priest Incarnate (`Light-*` vs `Soul-*`) and Druid Malorne shoulders (`Shoulderguards` vs `Pauldrons`).
- Verified fix added resolver-side role-family disambiguation for those TBC name families and enabled role filtering for Priests.
- Additional verified Shaman fix: `29763` ambiguity on bot `Ah` came from shoulder-family role fallback (`Shoulderpads` drifting with preferred role); resolver now classifies `Shoulderpads -> caster_dps` and `Shoulderplates -> melee_dps` explicitly, while preserving `Shoulderguards -> healer`.
- Live main-server validation completed: `Ah` logged a unique `29763 -> 29043 Cyclone Shoulderplates` auto exchange with `1 resolved, 0 skipped`.
- Additional Shaman helm-family fix: corrected Cyclone/Cataclysm helm-role mapping (`Helm -> melee_dps`, `Faceguard/Headpiece -> caster_dps`, `Headdress/Headguard -> healer`) to prevent unresolved helm-token outcomes under hybrid role filtering.
- Additional Druid T5 fix: corrected Nordrassil helm hint mapping so `Headguard` resolves via healer-family path (not caster-family) during Druid role filtering.
- Mapping table/load path remained healthy (`170` staged rows loaded); no discovery/table rewrite was required.
- Detailed evidence is documented in `notes/main-server-beta-results.md`.

## Loot Pass / Duplicate Ownership Investigation

- Hook point: `modules/mod-playerbots/src/Ai/Base/Actions/LootRollAction.cpp` before `group->CountRollVote(...)`.
- External pre-roll pass is possible only through the optional Playerbots callback; default behavior is unchanged.
- Post-award duplicate detection remains available as a fallback.
- Detailed notes: `notes/playerbot-loot-pass-hook.md`.
