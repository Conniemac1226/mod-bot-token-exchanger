# mod-bot-token-exchanger

## Architecture
- DB-staged resolver architecture; no hardcoded token->reward IDs.
- Runtime discovery uses `npc_vendor.ExtendedCost` + `sItemExtendedCostStore`.
- Separate caches/queues:
  - TBC: `bot_token_exchanger_token_map`
  - WotLK: `bot_token_exchanger_wotlk_map` + `bot_token_exchanger_wotlk_cost` (`single_token_chain` only)
- Playerbot loot-pass callback is optional and no-op when disabled.

## Hard Safety Rules
- Playerbots only in service mode: `OnlyPlayerbots=1`.
- Keep `DiscoveryWriteDb=0` unless intentionally staging mappings.
- Keep `AutoPopulateMappings=1` for first-start bootstrap; it only populates empty staging tables.
- Keep `AllowDebugTargetCommand=0` outside controlled tests.
- Keep WotLK unsupported structures disabled:
  - `emblem_like`
  - `token_plus_prior_armor_upgrade` (including `52025`-style marks)
- Keep ambiguity/no-match safety behavior (never force first candidate).
- Keep exchange rollback/store-failure handling intact.
- No SQL in hot loot-roll callback paths.
- Trust validation metrics only after:
  - exactly one `worldserver` process
  - matching build/live worldserver hashes

## Supported Scope
- TBC: discovery, resolver, exchange, auto exchange, loot-pass duplicate suppression, validation commands.
- WotLK: staged discovery + resolver + exchange/auto for `single_token_chain` only.
- Validation commands:
  - `.tokenex validate tbc`
  - `.tokenex validate tbc verbose`
  - `.tokenex validate tbc unresolved`
  - `.tokenex validate wotlk single`
  - `.tokenex validate wotlk single verbose`
  - `.tokenex validate wotlk single unresolved`

## Unsupported Scope
- WotLK emblem spending/exchange.
- WotLK upgrade-chain exchange.
- WotLK loot-pass integration.

## Build / Install / Runtime Sanity
```bash
cd /home/cbur/azerothcore-wotlk
cmake --build build --target worldserver -j"$(nproc)"
cmake --install build
scripts/verify-live-binary.sh
```

## Known Risks
- Full-bag/store-failure paths should continue periodic live verification under load.
- Hybrid role/spec detection can still produce safe no-match in edge gear/spec states.
- WotLK auto remains limited beta scope and should stay explicitly gated.
