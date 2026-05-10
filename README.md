# mod-bot-token-exchanger

Server-side module for Playerbots that resolves staged TBC armor/tier tokens into the correct reward item and can exchange them when explicit safety gates are enabled.

## Scope

- TBC staged mappings only
- Playerbots only
- No WotLK support yet
- No architecture changes required for normal use

## Main-Service Config

Use these defaults for live service:

- `BotTokenExchanger.Enable = 1`
- `BotTokenExchanger.Debug = 0`
- `BotTokenExchanger.OnlyPlayerbots = 1`
- `BotTokenExchanger.DiscoveryWriteDb = 0`
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

## Discovery

Use `.tokenex discover tbc` to regenerate staged mappings from runtime DBC data and known TBC vendors.

Keep `BotTokenExchanger.DiscoveryWriteDb = 0` unless you are intentionally refreshing the mapping table.

## Status

Use `.tokenex status` to verify:

- active safety gates
- loaded mapping count
- queue size

## Loot Pass

`BotTokenExchanger.PlayerbotLootPassEnable` controls whether Playerbots can pass staged token rolls when the resolved reward is already owned or equipped.

You can disable that behavior independently by setting:

- `BotTokenExchanger.PlayerbotLootPassEnable = 0`

## Notes

- Auto exchange, resolver, and loot-pass behavior are all gated and can be disabled independently.
- Detailed historical testing notes live in `notes/`.
