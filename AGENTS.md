# Project Overview

* Module purpose: resolve and exchange staged TBC armor/tier tokens for Playerbots.
* Scope: server-side only, Playerbots only, WotLK support excluded for now.
* Current state: discovery, resolver, dry-run exchange, real exchange, auto exchange, and loot-pass support are implemented for validated TBC mappings only.

# Hard Rules

* Never invent item IDs, reward IDs, or AzerothCore/Playerbot APIs.
* Always inspect local headers and DB state before changing behavior.
* Prefer DB-driven mappings over hardcoded token logic.
* Do not modify core files unless a hook truly requires it.
* Never affect real players.

# Verified Hooks / APIs

* Playerbots pre-roll callback: `SetPlayerbotBeforeLootRollCallback(...)`
* Playerbots callback entry: `OnPlayerbotBeforeLootRoll(Player*, ItemTemplate const*, RollVote&)`
* PlayerScript hooks used by this module: `OnPlayerStoreNewItem`, `OnPlayerLogin`, `OnPlayerUpdate`, `OnPlayerLogout`
* Runtime DBC store: `sItemExtendedCostStore.LookupEntry(uint32)`
* Playerbot role/spec helpers: `AiFactory::GetPlayerSpecTab`, `AiFactory::GetPlayerSpecName`, `AiFactory::GetPlayerRoles`
* Bot detection: `PlayerSession::IsBot()`

# Current Architecture

* Discovery loads verified TBC mappings into `bot_token_exchanger_token_map`.
* Resolver reads staged mappings from the DB and caches them in memory.
* Auto exchange uses a delayed per-bot queue and is Playerbot-only.
* Loot-pass uses the Playerbots callback and only forces PASS for staged tokens when the resolved reward is already owned/equipped.
* No SQL runs in hot loot-roll logic.

# Production Config

* `Enable = 1`
* `Debug = 0`
* `OnlyPlayerbots = 1`
* `DiscoveryWriteDb = 0`
* `ResolveOnly = 1`
* `DryRun = 0`
* `ExchangeEnable = 1`
* `AutoExchangeEnable = 1`
* `PlayerbotLootPassEnable = 1`
* `AllowDebugTargetCommand = 0`
* `ExchangeDelayMs = 1000`
* `AutoExchangeDelayMs = 1500`
* `AutoExchangeOnLoot = 1`
* `AutoExchangeOnLogin = 0`
* `AutoExchangeMaxPerBotPerPass = 1`
* `AnnounceToBotOwner = 0`

# Rollout Checklist

* Keep `DiscoveryWriteDb = 0` unless regenerating staged mappings.
* Confirm `.tokenex status` shows the expected gate state, mapping count, and queue size.
* Start with `DryRun = 1` if testing changes to exchange logic.
* Keep `AutoExchangeEnable = 0` until automatic processing is intentionally enabled.
* Keep `PlayerbotLootPassEnable = 0` unless duplicate token rolls should be suppressed.
* Keep `AllowDebugTargetCommand = 0` unless debugging a specific bot by name.

# Known Risks

* Ambiguous faction variants still need resolver logging and safe skip behavior.
* Hybrid class classification can require spec/role detection and occasional family-name hints.
* The loot-pass callback must stay no-op when caches are unavailable or resolution is ambiguous.
* Queue state must stay bounded and cleared on logout to avoid stale processing.

# Completed Phases

* TBC discovery from runtime DBC and vendor data.
* Staged DB mapping table and resolver cache.
* Dry-run and real exchange flows.
* Auto exchange queue.
* Playerbot loot-pass callback with duplicate ownership suppression.
* PTR beta hybrid-resolution fixes for Warrior, Shaman, and Druid TBC cases.

# Active Blockers

* None for the current TBC release path.
* WotLK support remains intentionally out of scope.

# Loot Pass / Duplicate Ownership Investigation

* Hook point: `modules/mod-playerbots/src/Ai/Base/Actions/LootRollAction.cpp` before `group->CountRollVote(...)`.
* External pre-roll pass is possible only through the optional Playerbots callback; default behavior is unchanged.
* Post-award duplicate detection remains available as a fallback.
* Detailed notes: `notes/playerbot-loot-pass-hook.md`.
