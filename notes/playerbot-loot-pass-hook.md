# Playerbot Loot Pass Hook

## Hook point

- `modules/mod-playerbots/src/Ai/Base/Actions/LootRollAction.cpp`
- Hook call is placed immediately before `group->CountRollVote(...)` in `LootRollAction::Execute`.

## Playerbots API

- `SetPlayerbotBeforeLootRollCallback(PlayerbotBeforeLootRollCallback callback)`
- `OnPlayerbotBeforeLootRoll(Player* bot, ItemTemplate const* itemTemplate, RollVote& rollVote)`
- Default callback is empty, so behavior is unchanged unless a module registers one.

## BotTokenExchanger integration

- Config gate: `BotTokenExchanger.PlayerbotLootPassEnable = 0`
- When enabled, BotTokenExchanger registers a callback during config load.
- The callback is cache-only and does not query SQL.
- The callback returns immediately for non-playerbots or non-staged token IDs.

## Resolution and ownership check

- The module uses cached staged TBC mappings already loaded in memory.
- Token resolution uses the existing in-memory resolver pipeline.
- If the resolver is ambiguous, the callback leaves the vote unchanged.
- Reward ownership check is limited to equipped slots and bags.
- Bank is intentionally skipped for now.

## Runtime behavior

- If the resolved reward is already owned, the callback forces `PASS`.
- Only a concise log line is written when PASS is forced.
- Normal loot behavior is unchanged when the gate is disabled.
- During debug mode, staged-token rolls log token id/name, original vote, resolved reward, equipment ownership, bag ownership, and final vote.

## Startup crash note

- Reproduced on 2026-05-10 with `BotTokenExchanger.PlayerbotLootPassEnable = 1`.
- SIGFPE stack:
  - `DatabaseWorkerPool<WorldDatabaseConnection>::GetFreeConnection()` at `src/server/database/Database/DatabaseWorkerPool.cpp:503`
  - `DatabaseWorkerPool<WorldDatabaseConnection>::Query()` at `src/server/database/Database/DatabaseWorkerPool.cpp:187`
  - `BotTokenExchangerMgr::LoadResolverMappings()` at `modules/mod-bot-token-exchanger/src/BotTokenExchangerMgr.cpp:1664`
  - `BotTokenExchangerMgr::UpdatePlayerbotLootPassCallback()` at `modules/mod-bot-token-exchanger/src/BotTokenExchangerMgr.cpp:187`
  - `BotTokenExchangerMgr::LoadConfig()` at `modules/mod-bot-token-exchanger/src/BotTokenExchangerMgr.cpp:176`
- Root cause: loot-pass startup path queried `WorldDatabase` before database startup, so the synchronous pool had zero connections and divided by zero.
- Fix: stop loading resolver mappings from `LoadConfig()`; preload caches in a `DatabaseScript` `OnAfterDatabasesLoaded()` hook instead.
- Validation: `PlayerbotLootPassEnable = 0` boots cleanly; `PlayerbotLootPassEnable = 1` should now boot cleanly after the preload move, pending build/install verification.

## Ownership Bug Fix

- Reproduced issue: bots with already exchanged/equipped TBC rewards still rolled `GREED` on the same token.
- Root cause: the ownership helper scanned backpack inventory slots and bags, but not the equipped slot range.
- Fix: split ownership checks into explicit equipment and bag scans, then force `PASS` when either contains the resolved reward item id.
- Callback behavior: staged tokens only, no SQL, ambiguous resolutions still no-op, non-token loot still returns immediately.
- Validation: build/install passed; startup with `PlayerbotLootPassEnable = 1` and `.tokenex status` both completed cleanly with `LoadedMappingCount=170` and `QueueSize=0`.
