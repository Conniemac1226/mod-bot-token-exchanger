# mod-bot-token-exchanger

Server-side module for Playerbots that resolves staged TBC armor/tier tokens into the correct reward item and can exchange them when explicit safety gates are enabled.

## Scope

- TBC staged mappings only
- Playerbots only
- WotLK support is limited to staged `single_token_chain` scope
- WotLK `emblem_like` and upgrade-chain (`token_plus_prior_armor_upgrade`, including `52025`-style marks) are intentionally excluded
- WotLK auto exchange remains disabled by default
- No WotLK loot-pass integration yet
- No architecture changes required for normal use

## Main-Service Config

Use these defaults for live service:

- `BotTokenExchanger.Enable = 1`
- `BotTokenExchanger.Debug = 0`
- `BotTokenExchanger.OnlyPlayerbots = 1`
- `BotTokenExchanger.DiscoveryWriteDb = 0`
- `BotTokenExchanger.AutoPopulateMappings = 1` (first-startup staging bootstrap)
- `BotTokenExchanger.ResolveOnly = 1`
- `BotTokenExchanger.DryRun = 0`
- `BotTokenExchanger.ExchangeEnable = 1`
- `BotTokenExchanger.AutoExchangeEnable = 1`
- `BotTokenExchanger.PlayerbotLootPassEnable = 1`
- `BotTokenExchanger.AllowDebugTargetCommand = 0`
- `BotTokenExchanger.ExchangeDelayMs = 1000`
- `BotTokenExchanger.AutoExchangeDelayMs = 1500`
- `BotTokenExchanger.AutoExchangeOnLoot = 1`
- `BotTokenExchanger.AutoExchangeOnLogin = 0`
- `BotTokenExchanger.AutoExchangeMaxPerBotPerPass = 1`
- `BotTokenExchanger.AnnounceToBotOwner = 0`

## Emergency Disable

If anything looks unsafe, set:

- `BotTokenExchanger.DryRun = 1`
- `BotTokenExchanger.ExchangeEnable = 0`
- `BotTokenExchanger.AutoExchangeEnable = 0`
- `BotTokenExchanger.PlayerbotLootPassEnable = 0`
- `BotTokenExchanger.AllowDebugTargetCommand = 0`

Restart `worldserver` if the process is already running with unsafe config loaded.

## WotLK Safety Gates

Use these safe defaults unless actively running controlled WotLK tests:

- `BotTokenExchanger.WotlkExchangeEnable = 0`
- `BotTokenExchanger.WotlkDryRun = 1`
- `BotTokenExchanger.WotlkAutoExchangeEnable = 0`
- `BotTokenExchanger.AllowDebugTargetCommand = 0`
- `BotTokenExchanger.WotlkTelemetryEnable = 1` (throttled unresolved telemetry for beta)

Current WotLK exchange safety behavior (single_token_chain only):

- requires exactly one staged resolver candidate after class/faction/role filtering
- duplicate ownership prevention: skips if resolved reward is already equipped or in bags
- transaction ordering and rollback are handled through shared exchange helper
- excluded structures are not loaded into WotLK resolver scope and cannot be exchanged by WotLK commands

## Discovery

Use `.tokenex discover tbc` to regenerate staged mappings from runtime DBC data and known TBC vendors.

Keep `BotTokenExchanger.DiscoveryWriteDb = 0` unless you are intentionally refreshing the mapping table.
On first startup with empty module tables, `BotTokenExchanger.AutoPopulateMappings = 1` auto-stages:

- TBC staged map (`bot_token_exchanger_token_map`)
- WotLK staged tables (`bot_token_exchanger_wotlk_map`, `bot_token_exchanger_wotlk_cost`)

This runs once when counts are empty and does not overwrite existing staged/custom rows.
Manual rediscovery remains explicit (`DiscoveryWriteDb=1` + discovery command).

### First Run Mapping Population

On a fresh install, the SQL schema is created by the normal AzerothCore DB update flow.

If the staged mapping table is empty and you want to populate it:

1. Set `BotTokenExchanger.DiscoveryWriteDb = 1`
2. Run `.tokenex discover tbc`
3. Set `BotTokenExchanger.DiscoveryWriteDb = 0` afterward

If the table exists but is empty, the module starts normally and discovery is required before any exchange can occur.

## Status

Use `.tokenex status` to verify:

- active safety gates
- loaded mapping count
- queue size

## Binary / Runtime Sanity (Required Before Validation)

Validation output is not trustworthy unless the running `worldserver` matches the current build and only one process is active.

Use:

```bash
cd /home/cbur/azerothcore-wotlk
scripts/verify-live-binary.sh
```

The check fails if:

- more than one `worldserver` process is running
- build and installed `worldserver` SHA256 hashes differ

For clean validation runs:

```bash
cd /home/cbur/azerothcore-wotlk
pkill -f '/home/cbur/azeroth-server/bin/worldserver' || true
cmake --build build --target worldserver -j"$(nproc)"
cmake --install build
scripts/verify-live-binary.sh
/home/cbur/azeroth-server/bin/worldserver
```

Run `.tokenex validate ...` only after the sanity check passes.

## Loot Pass

`BotTokenExchanger.PlayerbotLootPassEnable` controls whether Playerbots can pass staged token rolls when the resolved reward is already owned or equipped.

You can disable that behavior independently by setting:

- `BotTokenExchanger.PlayerbotLootPassEnable = 0`

## Notes

- Auto exchange, resolver, and loot-pass behavior are all gated and can be disabled independently.
- Detailed historical testing notes live in `notes/`.
