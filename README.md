# SaltyNX
Background process for the Nintendo Switch for file/code modification

Tests were done on FW 7.0.1-9.1.0, Atmosphere 0.9.1-0.9.4, 0.10.1-0.10.2
It should work with ReinX too.
SX OS older than 2.9 are not working (no technical support for all SX OS versions). Tested only on sysNAND 9.0.0, 2.9.2

Clean Kosmos crashes SaltyNX because of too much sysmodules. You need to delete some (f.e. emuiibo, because it crashes SaltyNX on it's own). It's recommended to use Kosmos v14.0.1 at least.

Known issues:
- SaltyNX is still under development, so it may cause issues on it's own:
- Instability with some homebrews and sysmodules,
- You need to have at least Hekate 5.0.2 if you don't want issues related to Hekate (if you use KIP rememeber to add to hekate_ipl.ini under your config line
```
kip1=atmosphere/kips/
```
- 32 bit games are unsupported,
- Crashes OS if you use Atmosphere cheat engine and you have in titles folder of game you want to boot cheats. For avoiding this read "How to use cheats" in SaltyNX-Tool repo's readme,
- For EmuMMC (and maybe sysnand too): if you use freebird, then OS can crash if you try to open hbmenu while running game (don't know if this was an issue with older releases).


List of not compatible games:

| Title | Version(s) | Why? |
| ------------- | ------------- | ------------- |
| Azure Striker Gunvolt: Striker Pack | all | 32-bit game, not supported |
| Darksiders 2 | 1.0.0 | Heap related |
| Grid: Autosport | 1.4.0-1.5.0 | Heap related |
| Goat Simulator | all | 32-bit game, not supported |
| Luigi's Mansion 3 | 1.0.0-1.2.1 | Heap Related |
| Mario Kart 8 | all | 32-bit game, not supported |
| Monster Hunter Generations Ultimate | all | 32-bit game, not supported |
| Ni no Kuni: Wrath of the White Witch | all | 32-bit game, not supported |
| Planescape: Torment and Icewind Dale | all | 32-bit game, not supported |
| Tokyo Mirage Session #FE Encore | all | 32-bit game, not supported |

"Heap related" games are added to excpetions.txt which is treated as Black list, 32-bit games are ignored by default
