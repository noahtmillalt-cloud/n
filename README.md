# QuestHUDTracker

An SKSE plugin that keeps your active quest and its objectives on screen at all times, Witcher 3 style, using the vanilla-style quest list SWF (`QuestItemList.swf`) so it looks like the list you get when facing a quest marker on the compass.

**Status: untested first draft.** It was written without a Windows build environment or the game. Expect a few compile fixes and some tuning in game. See "Things I could not verify" at the end.

Target game: Skyrim SE **1.5.97** (also built to work on AE through CommonLibSSE-NG).

---

## Step by step

### 1. Install the game-side requirements (1.5.97)

1. **SKSE64 2.0.20** (the build for 1.5.97).
2. **Address Library for SKSE Plugins**, the **SE** version (it contains the 1.5.97 `.bin`). The AE/1.6 builds will not work.

### 2. Build the plugin

**Easiest: let GitHub build it.** Create a new GitHub repository, upload this whole folder (including the hidden `.github` folder), then open the **Actions** tab, run **Build QuestHUDTracker**, and download the `QuestHUDTracker` artifact when it finishes. It already has the right folder structure for step 3, so you can skip the rest of this step. If the build fails, open the failed step and send me the error text. The workflow fills in the vcpkg baselines itself.

**Or build locally.** You need Visual Studio 2022 (Desktop development with C++), CMake 3.24 or newer, and vcpkg.

1. Clone vcpkg and run its bootstrap, then set the environment variable `VCPKG_ROOT` to that folder.
2. Open `vcpkg-configuration.json` and replace both `REPLACE_WITH_...` values with current commit hashes:
   - the first is the latest commit of `https://github.com/microsoft/vcpkg`
   - the second is the latest commit of `https://gitlab.com/colorglass/vcpkg-colorglass`
3. In a terminal in this folder:
   ```
   cmake --preset release
   cmake --build --preset release
   ```
4. The result is `QuestHUDTracker.dll` under `build/Release`.

### 3. Install the mod files

Put these files in a mod folder (or directly in `Data` if you do not use a mod manager), keeping the folder structure:

```
Data/SKSE/Plugins/QuestHUDTracker.dll          (from step 2)
Data/SKSE/Plugins/QuestHUDTracker.ini          (from this project's data folder)
Data/Interface/QHT_QuestItemList.swf           (from this project's data folder)
```

`QHT_QuestItemList.swf` is your uploaded `QuestItemList.swf` with one change, described below. It has a different name on purpose so it never overwrites another mod's copy.

### 4. Check it in game

1. Start the game through SKSE, load a save, and make sure a quest is selected as active in the journal.
2. The quest title and objectives should appear at the left of the screen and stay there.
3. Look at the log: `Documents/My Games/Skyrim Special Edition/SKSE/QuestHUDTracker.log`.
   - `Loading QHT_QuestItemList.swf` then `Quest list SWF ready` means the SWF loaded.
   - `QuestItemList SWF did not load` means the file is missing or in the wrong folder.

### 5. Adjust the position

Edit `QuestHUDTracker.ini` and restart the game. `SwfX`, `SwfY` and `SwfMaxHeight` are ratios of the screen (0 to 1). Set `Mode = text` to use the plain text fallback if the SWF mode gives you trouble.

---

## What was changed in the SWF

In `Update()` the original list only becomes visible while the compass's direction rectangle is visible (that is the "facing the marker" behavior). The patch makes that first check never skip, so the list is visible whenever the compass is. The second check (the compass itself being visible) is kept on purpose, so the list still hides when the HUD or compass hides.

`tools/patch_swf.py` reproduces the patch from the original file. To do the same by hand: open the original in JPEXS Free Flash Decompiler, go to the `Update` function in the first script, delete the condition on `HUDMovieBaseInstance.CompassShoutMeterHolder.Compass.DirectionRect._alpha`, and save as `QHT_QuestItemList.swf`.

## How the plugin drives the SWF

1. Creates an empty clip inside the HUD and calls `loadMovie` on it.
2. When the SWF has defined its functions, sets `SCALE = 100`, calls `QuestItemList(x, y, maxHeight)`, and optionally `AddToHudElements()`.
3. Every `RefreshMs` it reads the active quest. When the quest or its objectives change it calls `RemoveAllQuests()`, then `AddQuest(type, title, false, objectives, 0)` and `ShowQuest()`. It calls `Update()` each tick so the SWF lays itself out.

Quest category (Main, Mages Guild, Thieves Guild, Companions, Daedric, Civil War, Dawnguard, Dragonborn, side quests, misc) is mapped to the SWF's frame labels so the right header style is used.

## Things I could not verify

1. **Active quest detection.** `quest->IsActive()` is expected to match the quest selected in the journal. If the wrong quest shows, this is the line to fix.
2. **Quest category accessor.** `active->GetType()` may be named differently in your CommonLibSSE-NG version (for example `data.questType`).
3. **Loading by relative path.** `loadMovie("QHT_QuestItemList.swf")` is expected to resolve inside `Data/Interface`. If the log says it did not load, try `SwfPath = Interface/QHT_QuestItemList.swf`.
4. **`SCALE` and `_root`.** The SWF reads a global `SCALE` and `_root.HUDMovieBaseInstance` from the original HUD. If entries look the wrong size, change the `SCALE` value in `Tracker.cpp`. If nothing shows at all, set `RegisterHudElement = 0` to rule out the HUD's own visibility handling.
5. **Alias tokens.** Objectives like "Find <Alias=Target>" are passed raw for now.
6. **Re-fade on change.** When objectives change, the whole entry fades out and in again, since the SWF only supports replacing an entry.
7. **Compile details.** Some CommonLibSSE-NG signatures (`QUEST_OBJECTIVE_STATE`, `GFxValue` argument handling, `CreateArray`) differ slightly between versions.
