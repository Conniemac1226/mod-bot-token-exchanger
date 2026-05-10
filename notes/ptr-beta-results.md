# PTR Beta Results

## Scope

PTR beta hybrid resolver validation for TBC staged mappings only.

## Verified Bot Roles

- `Catboy`: Horde shaman, `spec resto`, `role healer`
- `Farts`: Horde druid, `spec balance`, `role caster_dps`
- `Ag`: Horde warrior, `spec prot`, `role tank`

## Results

- `Catboy`
  - `29754 Chestguard of the Fallen Champion` resolved to `29033 Cyclone Chestguard`
  - `30245 Leggings of the Vanquished Champion` resolved to `30172 Cataclysm Leggings`
  - `29763 Pauldrons of the Fallen Champion` resolved to `29037 Cyclone Shoulderguards`
- `Farts`
  - `29764 Pauldrons of the Fallen Defender` resolved to `29095 Pauldrons of Malorne`
  - `29758 Gloves of the Fallen Defender` resolved to `29092 Gloves of Malorne`
  - `29767 Leggings of the Fallen Defender` remained safely unresolved
- `Ag`
  - `29753 Chestguard of the Fallen Defender` resolved to `29012 Warbringer Chestguard`

## Fix Notes

- Warrior now uses the same role-filter path as the hybrid classes.
- Shaman `Shoulderpads` no longer win against resto `Shoulderguards`.
- Druid `Mantle of Malorne` is no longer forced into caster by a broad suffix hint.

## Follow-Up

- After the Druid caster-leg classification fix, `29767` now resolves for `Farts` to `29094 Britches of Malorne`.
- `29764` and `29758` continue to resolve correctly for the same bot.
