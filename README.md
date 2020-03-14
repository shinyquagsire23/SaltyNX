# SaltyNX
Background process for the Nintendo Switch for file/code modification

Created by: https://github.com/shinyquagsire23

This fork includes many QoL improvements

---

For additional functions you need SaltyNX-Tool (if you are using ReverseNX-Tool, it has all functions already integrated from 1.35)

https://github.com/masagrator/SaltyNX-Tool

Tests were done on FW 7.0.1-9.1.0, Atmosphere 0.9.1-0.9.4, 0.10.1-0.10.4

It should work with ReinX too.

SX OS older than 2.9 are not working (no technical support for all SX OS versions). Tested only on sysNAND 9.0.0, 2.9.2

Clean Kosmos crashes SaltyNX because of too much sysmodules. You need to delete some (f.e. emuiibo, because it crashes SaltyNX on it's own). It's recommended to use Kosmos v14.0.1 at least.

Known issues:
- Instability with some homebrews and sysmodules,
- You need to have at least Hekate 5.0.2 if you don't want issues related to Hekate (if you use KIP rememeber to add to hekate_ipl.ini under your config line
```
kip1=atmosphere/kips/*
```
- 32 bit games are unsupported,
- For EmuMMC (and maybe sysnand too): if you use freebird, then OS can crash if you try to open hbmenu while running game (don't know if this was an issue with older releases).

# How to download release:

For Atmosphere or Reinx you can use combination KIP.zip + SaltyNX or NSP.zip + SaltyNX

For SX OS (2.9 or newer, older are not working) only NSP.zip + SaltyNX

For Atmosphere <=0.9.4 and any other CFW in case of using NSP remember to rename `contents` folder to `titles`

For SX OS remember to rename `atmosphere` folder to `sxos`

For ReinX remember to rename `atmosphere` folder to `reinx` and in case of using KIP rename `kips` folder to `sysmodules`

Remember to restart Switch

---

List of not compatible titles:

| Title | Version(s) | Why? |
| ------------- | ------------- | ------------- |
| Azure Striker Gunvolt: Striker Pack | all | 32-bit game, not supported |
| Darksiders 2 | 1.0.0 | Heap related |
| Grid: Autosport | 1.4.0-1.5.0 | Heap related |
| Goat Simulator | all | 32-bit game, not supported |
| Luigi's Mansion 3 | 1.0.0-1.2.1 | Heap Related |
| Mario Kart 8 | all | 32-bit game, not supported |
| Monster Hunter Generations Ultimate | all | 32-bit game, not supported |
| New Super Mario Bros. U Deluxe | all | 32-bit game, not supported |
| Ni no Kuni: Wrath of the White Witch | all | 32-bit game, not supported |
| Planescape: Torment and Icewind Dale | all | 32-bit game, not supported |
| The Talos Principle | all | unknown |
| Tokyo Mirage Session #FE Encore | all | 32-bit game, not supported |
| YouTube | all | Unknown |

Titles other than 32-bit are added to exceptions.txt which is treated as Black list, you can find it in root of repo. SaltyNX reads it from SaltySD folder.

32-bit games are ignored by default.
