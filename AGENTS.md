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
