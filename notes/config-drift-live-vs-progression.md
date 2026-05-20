# Live vs Progression Module Config Drift

Generated: 2026-05-20 05:57:27 UTC

## High-Level Summary

- Live module config files: 30
- Progression module config files: 33
- Common filenames: 30
- Live-only files: 0
- Progression-only files: 3
- Common files with active key/value drift: 6

## File Presence Differences

### Present On Live Only
- None

### Present On Progression Only
- `individualProgression.conf`
- `individualProgression.conf.bak_2026-05-19_tbcrollback`
- `individualProgression.conf.dist`

## Active Key Drift By File (comments/blank lines ignored)

| File | Diff line count |
|---|---:|
| `mod_ahbot.conf` | 15 |
| `mod_ahbot.conf.dist` | 15 |
| `playerbots.conf` | 6 |
| `bot_token_exchanger.conf` | 5 |
| `bot_token_exchanger.conf.dist` | 5 |
| `mod_player_bot_level_brackets.conf` | 2 |

## Detailed Diff: `mod_player_bot_level_brackets.conf`

| Key | Live | Progression | Assessment |
|---|---|---|---|
| `BotLevelBrackets.Enabled` | `1` | `0` | high impact behavior |

## Detailed Diff: `playerbots.conf`

| Key | Live | Progression | Assessment |
|---|---|---|---|
| `AiPlayerbot.CommandServerPort` | `8888` | `8889` | high impact behavior |
| `AiPlayerbot.DowngradeMaxLevelBot` | `0` | `1` | high impact behavior |
| `AiPlayerbot.RandomBotMaxLevel` | `80` | `70` | high impact behavior |

## Detailed Diff: `bot_token_exchanger.conf`

| Key | Live | Progression | Assessment |
|---|---|---|---|
| `BotTokenExchanger.AutoPopulateMappings` | `<missing>` | `1` | high impact behavior |
| `BotTokenExchanger.RunValidationOnStartup` | `<missing>` | `0` | review |
| `BotTokenExchanger.StartupValidationDelayMs` | `<missing>` | `60000` | review |
| `BotTokenExchanger.StartupValidationMode` | `<missing>` | `"none"` | review |
| `BotTokenExchanger.WotlkTelemetryEnable` | `<missing>` | `1` | high impact behavior |

## Detailed Diff: `mod_ahbot.conf`

| Key | Live | Progression | Assessment |
|---|---|---|---|
| `AuctionHouseBot.ListingStack.MaxStackSize.Armor` | `<missing>` | `0` | review |
| `AuctionHouseBot.ListingStack.MaxStackSize.Consumable` | `<missing>` | `0` | review |
| `AuctionHouseBot.ListingStack.MaxStackSize.Container` | `<missing>` | `0` | review |
| `AuctionHouseBot.ListingStack.MaxStackSize.Gem` | `<missing>` | `0` | review |
| `AuctionHouseBot.ListingStack.MaxStackSize.Generic` | `<missing>` | `0` | review |
| `AuctionHouseBot.ListingStack.MaxStackSize.Glyph` | `<missing>` | `0` | review |
| `AuctionHouseBot.ListingStack.MaxStackSize.Key` | `<missing>` | `0` | review |
| `AuctionHouseBot.ListingStack.MaxStackSize.Misc` | `<missing>` | `0` | review |
| `AuctionHouseBot.ListingStack.MaxStackSize.Projectile` | `<missing>` | `0` | review |
| `AuctionHouseBot.ListingStack.MaxStackSize.Quest` | `<missing>` | `0` | review |
| `AuctionHouseBot.ListingStack.MaxStackSize.Quiver` | `<missing>` | `0` | review |
| `AuctionHouseBot.ListingStack.MaxStackSize.Reagent` | `<missing>` | `0` | review |
| `AuctionHouseBot.ListingStack.MaxStackSize.Recipe` | `<missing>` | `0` | review |
| `AuctionHouseBot.ListingStack.MaxStackSize.TradeGood` | `<missing>` | `0` | review |
| `AuctionHouseBot.ListingStack.MaxStackSize.Weapon` | `<missing>` | `0` | review |

## Full Active-Setting Comparison: `mod_player_bot_level_brackets.conf`

| Key | Live | Progression | Match | Classification |
|---|---|---|---|---|
| `BotLevelBrackets.Alliance.Range1.Lower` | `1` | `1` | yes | same |
| `BotLevelBrackets.Alliance.Range1.Pct` | `12` | `12` | yes | same |
| `BotLevelBrackets.Alliance.Range1.Upper` | `9` | `9` | yes | same |
| `BotLevelBrackets.Alliance.Range2.Lower` | `10` | `10` | yes | same |
| `BotLevelBrackets.Alliance.Range2.Pct` | `11` | `11` | yes | same |
| `BotLevelBrackets.Alliance.Range2.Upper` | `19` | `19` | yes | same |
| `BotLevelBrackets.Alliance.Range3.Lower` | `20` | `20` | yes | same |
| `BotLevelBrackets.Alliance.Range3.Pct` | `11` | `11` | yes | same |
| `BotLevelBrackets.Alliance.Range3.Upper` | `29` | `29` | yes | same |
| `BotLevelBrackets.Alliance.Range4.Lower` | `30` | `30` | yes | same |
| `BotLevelBrackets.Alliance.Range4.Pct` | `11` | `11` | yes | same |
| `BotLevelBrackets.Alliance.Range4.Upper` | `39` | `39` | yes | same |
| `BotLevelBrackets.Alliance.Range5.Lower` | `40` | `40` | yes | same |
| `BotLevelBrackets.Alliance.Range5.Pct` | `11` | `11` | yes | same |
| `BotLevelBrackets.Alliance.Range5.Upper` | `49` | `49` | yes | same |
| `BotLevelBrackets.Alliance.Range6.Lower` | `50` | `50` | yes | same |
| `BotLevelBrackets.Alliance.Range6.Pct` | `11` | `11` | yes | same |
| `BotLevelBrackets.Alliance.Range6.Upper` | `59` | `59` | yes | same |
| `BotLevelBrackets.Alliance.Range7.Lower` | `60` | `60` | yes | same |
| `BotLevelBrackets.Alliance.Range7.Pct` | `11` | `11` | yes | same |
| `BotLevelBrackets.Alliance.Range7.Upper` | `69` | `69` | yes | same |
| `BotLevelBrackets.Alliance.Range8.Lower` | `70` | `70` | yes | same |
| `BotLevelBrackets.Alliance.Range8.Pct` | `11` | `11` | yes | same |
| `BotLevelBrackets.Alliance.Range8.Upper` | `79` | `79` | yes | same |
| `BotLevelBrackets.Alliance.Range9.Lower` | `80` | `80` | yes | same |
| `BotLevelBrackets.Alliance.Range9.Pct` | `11` | `11` | yes | same |
| `BotLevelBrackets.Alliance.Range9.Upper` | `80` | `80` | yes | same |
| `BotLevelBrackets.CheckFlaggedFrequency` | `30` | `30` | yes | same |
| `BotLevelBrackets.CheckFrequency` | `900` | `900` | yes | same |
| `BotLevelBrackets.Dynamic.RealPlayerWeight` | `1.0` | `1.0` | yes | same |
| `BotLevelBrackets.Dynamic.SyncFactions` | `1` | `1` | yes | same |
| `BotLevelBrackets.Dynamic.UseDynamicDistribution` | `1` | `1` | yes | same |
| `BotLevelBrackets.Enabled` | `1` | `0` | no | risky / should review |
| `BotLevelBrackets.ExcludeNames` | `<missing>` | `<missing>` | yes | same |
| `BotLevelBrackets.FlaggedProcessLimit` | `5` | `5` | yes | same |
| `BotLevelBrackets.FullDebugMode` | `0` | `0` | yes | same |
| `BotLevelBrackets.GuildTrackerUpdateFrequency` | `1800` | `1800` | yes | same |
| `BotLevelBrackets.Horde.Range1.Lower` | `1` | `1` | yes | same |
| `BotLevelBrackets.Horde.Range1.Pct` | `12` | `12` | yes | same |
| `BotLevelBrackets.Horde.Range1.Upper` | `9` | `9` | yes | same |
| `BotLevelBrackets.Horde.Range2.Lower` | `10` | `10` | yes | same |
| `BotLevelBrackets.Horde.Range2.Pct` | `11` | `11` | yes | same |
| `BotLevelBrackets.Horde.Range2.Upper` | `19` | `19` | yes | same |
| `BotLevelBrackets.Horde.Range3.Lower` | `20` | `20` | yes | same |
| `BotLevelBrackets.Horde.Range3.Pct` | `11` | `11` | yes | same |
| `BotLevelBrackets.Horde.Range3.Upper` | `29` | `29` | yes | same |
| `BotLevelBrackets.Horde.Range4.Lower` | `30` | `30` | yes | same |
| `BotLevelBrackets.Horde.Range4.Pct` | `11` | `11` | yes | same |
| `BotLevelBrackets.Horde.Range4.Upper` | `39` | `39` | yes | same |
| `BotLevelBrackets.Horde.Range5.Lower` | `40` | `40` | yes | same |
| `BotLevelBrackets.Horde.Range5.Pct` | `11` | `11` | yes | same |
| `BotLevelBrackets.Horde.Range5.Upper` | `49` | `49` | yes | same |
| `BotLevelBrackets.Horde.Range6.Lower` | `50` | `50` | yes | same |
| `BotLevelBrackets.Horde.Range6.Pct` | `11` | `11` | yes | same |
| `BotLevelBrackets.Horde.Range6.Upper` | `59` | `59` | yes | same |
| `BotLevelBrackets.Horde.Range7.Lower` | `60` | `60` | yes | same |
| `BotLevelBrackets.Horde.Range7.Pct` | `11` | `11` | yes | same |
| `BotLevelBrackets.Horde.Range7.Upper` | `69` | `69` | yes | same |
| `BotLevelBrackets.Horde.Range8.Lower` | `70` | `70` | yes | same |
| `BotLevelBrackets.Horde.Range8.Pct` | `11` | `11` | yes | same |
| `BotLevelBrackets.Horde.Range8.Upper` | `79` | `79` | yes | same |
| `BotLevelBrackets.Horde.Range9.Lower` | `80` | `80` | yes | same |
| `BotLevelBrackets.Horde.Range9.Pct` | `11` | `11` | yes | same |
| `BotLevelBrackets.Horde.Range9.Upper` | `80` | `80` | yes | same |
| `BotLevelBrackets.IgnoreArenaTeamBots` | `1` | `1` | yes | same |
| `BotLevelBrackets.IgnoreFriendListed` | `1` | `1` | yes | same |
| `BotLevelBrackets.IgnoreGuildBotsWithRealPlayers` | `1` | `1` | yes | same |
| `BotLevelBrackets.LiteDebugMode` | `0` | `0` | yes | same |
| `BotLevelBrackets.NumRanges` | `9` | `9` | yes | same |

## High-Risk Differences

- `playerbots.conf`: `AiPlayerbot.RandomBotMaxLevel`, `AiPlayerbot.DowngradeMaxLevelBot`, and command server port drift change bot behavior and control channel.
- `mod_player_bot_level_brackets.conf`: `BotLevelBrackets.Enabled` drift can disable spread enforcement entirely on Progression.
- `bot_token_exchanger.conf`: toggles like `AutoPopulateMappings`/telemetry/startup-validation keys change runtime diagnostics and map behavior.
- `mod_ahbot.conf`: added listing stack max-size keys alter listing behavior and economy shape.

## Recommendations (No Changes Applied)

- Treat `individualProgression.conf*` as intentional Progression-only divergence.
- Decide whether `BotLevelBrackets.Enabled = 0` on Progression is intentional for TBC cap enforcement or accidental drift; this is the highest bot-level-spread risk.
- Keep `AiPlayerbot.RandomBotMaxLevel = 70` on Progression if TBC launch cap is intended; confirm `DowngradeMaxLevelBot=1` is desired for already-overcap bots.
- Keep port differences (`8888` vs `8889`) as intentional isolation.

## Applied Safe Drift Fix (Progression)

- Date: 2026-05-20 UTC
- File: `/home/cbur/azeroth-progression-server/etc/modules/mod_player_bot_level_brackets.conf`
- Change: `BotLevelBrackets.Enabled` updated from `0` to `1` to match live bracket enforcement behavior while keeping Progression-specific level caps/settings unchanged.
- No other module config files were modified.
