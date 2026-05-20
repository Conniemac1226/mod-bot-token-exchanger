# Progression TBC unresolved audit (current)

Date: 2026-05-20 UTC
Scope: Progression server only (`/home/cbur/azeroth-progression-server`)

## 1) Process sanity (Progression-only)

Command:
- `pgrep -af "azeroth-progression-server/bin/worldserver"`

Result:
- `847310 /home/cbur/azeroth-progression-server/bin/worldserver -c /home/cbur/azeroth-progression-server/etc/worldserver.conf`

Conclusion:
- Exactly one Progression worldserver process is running.
- Two total worldservers on host (main + progression) is expected and not treated as duplicate progression.

## 2) Build vs install binary sanity

Installed runtime binary:
- `/home/cbur/azeroth-progression-server/bin/worldserver`

Built binaries found:
- `/home/cbur/azerothcore-progression/build-progression/src/server/apps/worldserver`
- `/home/cbur/azerothcore-progression/build/src/server/apps/worldserver`

SHA256:
- install: `34a3e3ccf175af880def1d7bece47b0d9fd4f4f868c2e5396513136c139bdd40`
- build-progression: `a407080f1d7fad00629e01684a8996a79f294a33b41b86c85646226648bf2c85`
- build: `1bd212f787c956e065ca3d1588a970b25e3c020ce676a27f8e926fee2e92b27d`

mtime/size:
- install: `2370410000 bytes`, `2026-05-20 00:20:30`
- build-progression: `2370412536 bytes`, `2026-05-20 02:25:29`
- build: `533170208 bytes`, `2026-05-16 11:58:58`

Conclusion:
- Running Progression binary path is correct (`/proc/847310/exe -> /home/cbur/azeroth-progression-server/bin/worldserver`).
- But install binary does NOT match latest build-progression binary; runtime is likely stale vs latest source/build changes.

## 3) Progression log evidence

Files checked:
- `/home/cbur/azeroth-progression-server/logs/Server.log`
- `/home/cbur/azeroth-progression-server/bin/Server.log` (missing)

Found in `logs/Server.log`:
- `BotTokenExchanger config loaded: Enable=1 ... AutoPopulateMappings=1 ... WotlkTelemetryEnable=1 ...`
- `BotTokenExchangerWorldScript::OnStartup invoked`

Not found in current log window:
- `.tokenex validate tbc` output
- unresolved bucket lines
- ambiguous/unsupported summary lines

## 4) Progression DB mapping state

`acore_world_progression.bot_token_exchanger_token_map`:
- `source_expansion='TBC'` row count: `170`
- by status/confidence:
  - `staged / runtime_dbc_verified = 170`

Distinct staged TBC token IDs:
- 30 token IDs

## 5) Live DB evidence of staged TBC tokens on bots

Using `acore_characters_progression` random bot inventory joins:
- Only one random bot currently carrying staged TBC token items:
  - bot guid `6553`, name `Bj`, class `8` (Rogue), level `70`
  - token `29762` (`Pauldrons of the Fallen Hero`), count `9`

Per-token bot holding counts:
- `29762 Pauldrons of the Fallen Hero`: `1` bot, `9` tokens total

Candidate mappings for token `29762` (TBC staged):
- present in mapping table with 3 family candidates (Hero set).
- no direct DB evidence of unresolved outcome by itself; unresolved/ambiguous is runtime role/class filtering dependent and requires command output/logged resolution events.

## 6) Discrepancy analysis (current evidence)

Verified contributors:
1. **Stale Progression runtime binary vs latest build-progression output** (hash mismatch).
2. Validation command output is not currently present in logs, so unresolved bucket accounting cannot be fully reconstructed from logs-only.
3. Mapping table itself appears populated and healthy for TBC (`170 staged/runtime_dbc_verified`).

Not currently evidenced:
- duplicate progression worldserver
- missing TBC mappings in DB

## 7) What remains blocked without console command output

Still missing exact metrics requested by command output:
- online bot count
- LoadedMappingCount
- resolved / ambiguous / unsupported counts
- missing combos
- unresolved bucket group count
- top unresolved buckets

These are emitted by `.tokenex status` and `.tokenex validate tbc*` and are not present in current logs.

## 8) RA necessity assessment

RA is not needed for DB/log sanity checks and mapping evidence (completed).

RA (or equivalent direct worldserver console command execution path) is needed to obtain authoritative current unresolved-bucket metrics if they are not logged.

