# ZISIN FROGGER

A Frogger-style arcade game built with [raylib](https://www.raylib.com) in C, made for the CSE102 Raylib Project.

---

## Features

- Classic Frogger gameplay: guide the frog across a road and river to 5 goal slots
- Multiple vehicle types (car, sports car, truck) with distinct speeds and lanes
- Logs and turtle groups to ride across the river
- Animated hazard obstacles between goal slots
- Level progression with increasing difficulty (faster/denser traffic, shrinking timer)
- Three selectable difficulty modes: Easy / Medium / Hard
- Main menu with mouse-clickable buttons (and keyboard shortcuts)
- Player name entry before each game, shown above the frog and saved to the leaderboard
- Persistent high score and a top-10 leaderboard
- Dedicated How to Play and Credits screens
- Sound effects for jumping, crashing, drowning, reaching a goal, leveling up, and game over
- Looping background music with an in-game mute toggle



## Dependencies

- **raylib** (tested with raylib 5.x)
- A **C compiler** with raylib properly linked:
  - Windows: MinGW-w64 (GCC), or MSVC with raylib set up
  - Standard raylib link libraries on Windows: `raylib`, `opengl32`, `gdi32`, `winmm`
- (Optional) **VS Code** with the C/C++ extension, if using the provided `.vscode` build/debug tasks

No other third-party libraries are required. everything used (textures, sounds, text, shapes) is handled directly through raylib's built-in modules.

---

## How to Compile and Run


### Option A: Using VS Code (recommended, if `.vscode/tasks.json` is present)
1. Open the project folder in VS Code.
2. Press **Ctrl+Shift+B** to build (runs the configured build task).
3. Press **F5** to run/debug, or run the produced executable directly.

### Option B: Manual compilation (Windows, MinGW-w64/w64devkit)
From the project's root folder (where `main.c` and `assets/` live), run:


```

gcc main.c -o zisin_frogger.exe -O2 -Wall -std=c99 -I include -L lib -lraylib -lopengl32 -lgdi32 -lwinmm

```

## Controls

- **Arrow Keys / WASD** — move the frog
- **P** — pause
- **Mouse** — click any main menu button (Start, Leaderboard, How to Play, Credits, Difficulty, Music)
- **L** — open Leaderboard (from menu)
- **H** — open How to Play (from menu)
- **C** — open Credits (from menu)
- **Left / Right (or A / D)** — cycle difficulty (from menu)
- **M** — mute/unmute music
- **Enter** — confirm / start
- **Esc** — go back / cancel

A full in-game explanation is also available from the **How to Play** screen in the main menu.

---

## Credits

- **Game design & programming:** Zihan Ahammed & Mashfia Bin Matin
- **Engine:** [raylib](https://www.raylib.com)
- Sprites: Vehicle sprites (car, sports car, truck) — generated programmatically with Python. Turtle sprite — original artwork made for this project.
- Sound Effects: Jump, crash, splash, goal, level up, game over — freesound.org / Kenney.nl
- Background Music: "Three Initials Left" (from the album Infinite Credits)

