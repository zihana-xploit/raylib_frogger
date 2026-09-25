/*
    ZISIN FROGGER - bigger edition (v3 - visual & audio polish pass)
    ------------------------------------------------------------------
    Changes in this pass, all requested by the player:
      1. Lives shown as heart icons instead of a plain number.
      2. River restyled to look more like moving water + a distinct
         "drowning" sound when the frog falls in.
      3. Overall colour palette reworked to look more natural.
      4. Log height increased so the frog's whole body sits on the log
         (it used to hang half off the bottom edge).
      5. Road lanes repositioned so cars sit fully on the asphalt,
         between the lane-divider lines, instead of overlapping the
         grass or the dashed lines.
      6. The 5 goal "lily pads" now look like actual lily pads with a
         little flower, and turn into a planted flag once claimed.
      7. Grass bands (median strip + bottom strip) now have a mowed-
         lawn stripe pattern; the goal area uses a completely
         different checkerboard "finish line" look.
      8. New/updated sound effects: crash (car hit), drown (river),
         goal (single lily pad reached), level-up (all 5 pads filled).

    run command (Windows / MSYS2 MINGW64):
    gcc main.c -o main.exe -Iraylib/raylib-6.0_win64_mingw-w64/include -Lraylib/raylib-6.0_win64_mingw-w64/lib -lraylib -lopengl32 -lgdi32 -lwinmm

    Assets expected next to the executable (same folder as main.c):
    assets/images/frog.png
    assets/images/car.png
    assets/images/log.png
    assets/sounds/jump.wav
    assets/sounds/crash.wav
    assets/sounds/splash.wav
    assets/sounds/goal.wav
    assets/sounds/levelup.wav
    assets/sounds/gameover.wav
*/

#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ================================================================
   SECTION: CONFIG / CONSTANTS
   Everything about screen size, object counts and the vertical
   "layout bands" of the level lives here, so the whole game can be
   tuned from one place.
   ================================================================ */

#define SCREEN_WIDTH 1000
#define SCREEN_HEIGHT 600

#define FROG_SIZE 30

#define CAR_COUNT 9
#define LOG_COUNT 10

#define GOAL_SLOT_COUNT 5

#define FROG_NAME "Zisin"

#define HIGH_SCORE_FILE "highscore.txt"

/* how much faster obstacles get, per level */
#define DIFFICULTY_STEP 0.15f

/* Car types for variety */

/* ---- vertical layout bands - these all add up to SCREEN_HEIGHT ----
   (0-120)   goal / finish line
   (120-320) river
   (320-390) median SAND strip (safe)
   (390-560) road
   (560-600) bottom grass strip (safe, frog starts here)          */
#define GOAL_TOP        0
#define GOAL_HEIGHT     120

#define RIVER_TOP       120
#define RIVER_HEIGHT    200

#define MEDIAN_TOP      320
#define MEDIAN_HEIGHT   70

#define ROAD_TOP        390
#define ROAD_HEIGHT     170

#define BOTTOM_TOP      560
#define BOTTOM_HEIGHT   40

/* The frog moves in fixed 40px hops. Log rows are spaced on that same
   40px grid (see reset_level) so every row the frog can land on
   always has a log underneath it - no "invisible gap" to fall through. */
#define LOG_ROW_STEP    40

/* Log height is taller than the frog (30px) on purpose, so the whole
   frog sprite sits on top of the log instead of hanging off the edge. */
#define LOG_HEIGHT      34

/* Lane-divider dashed lines sit BETWEEN the 3 car lanes, not on top
   of them - see draw_road(). */
#define LANE_DIVIDER_1_Y 447
#define LANE_DIVIDER_2_Y 507


typedef enum
{
    STATE_MENU,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_LEVEL_COMPLETE,
    STATE_GAME_OVER
} GameState;

typedef enum
{
    CAR_TYPE_SEDAN = 0,
    CAR_TYPE_TRUCK = 1,
    CAR_TYPE_SPORTS = 2,
    CAR_TYPE_VAN = 3,
    CAR_TYPE_COUNT = 4
} CarType;

typedef struct
{
    Rectangle rect;
    float speed;
    CarType type;
} Object;

Texture2D carTexture;
Texture2D carSportsTexture;
Texture2D carTruckTexture;
bool carTextureValid = false;
bool carSportsTextureValid = false;
bool carTruckTextureValid = false;


/* ================================================================
   SECTION: SAVE FILE HELPERS
   Tiny text file next to the .exe that remembers the best score
   between runs.
   ================================================================ */

int load_high_score(void)
{
    int value = 0;
    FILE *f = fopen(HIGH_SCORE_FILE, "r");

    if (f != NULL)
    {
        if (fscanf(f, "%d", &value) != 1)
        {
            value = 0;
        }
        fclose(f);
    }

    return value;
}

void save_high_score(int score)
{
    FILE *f = fopen(HIGH_SCORE_FILE, "w");

    if (f != NULL)
    {
        fprintf(f, "%d", score);
        fclose(f);
    }
}


/* ================================================================
   SECTION: LIVES (HEART ICONS)
   Draws a single heart using two circles for the top lobes and a
   triangle for the bottom point - a classic, cheap way to draw a
   heart shape without needing an image file.
   ================================================================ */

void draw_heart(int cx, int cy, int size, Color color)
{
    int r = size / 4;

    DrawCircle(cx - r, cy - r, r, color);
    DrawCircle(cx + r, cy - r, r, color);

    DrawTriangle(
        (Vector2){cx - size / 2.0f, cy - r / 2.0f},
        (Vector2){cx, cy + size / 2.0f},
        (Vector2){cx + size / 2.0f, cy - r / 2.0f},
        color
    );
}

/* Draws `maxLives` hearts, filling `lives` of them in red and the
   rest in a dim grey (representing lives already lost). */
void draw_lives(int lives, int maxLives, int x, int y)
{
    int spacing = 26;

    for (int i = 0; i < maxLives; i++)
    {
        Color heartColor = (i < lives) ? (Color){220, 40, 60, 255} : (Color){90, 90, 90, 255};
        draw_heart(x + i * spacing, y, 20, heartColor);
    }
}


/* ================================================================
   SECTION: FROG
   ================================================================ */

/* beng babaji ager position e ashbe - send the frog back to its
   starting spot (used at the start and after losing a life) */
void reset_frog(Vector2 *frog)
{
    frog->x = SCREEN_WIDTH / 2 - FROG_SIZE / 2;
    frog->y = SCREEN_HEIGHT - 40;
}

/* beng aka - drawn from a sprite instead of plain shapes */
void draw_frog(Vector2 frog, Texture2D frogTex)
{
    Rectangle src = {0, 0, (float)frogTex.width, (float)frogTex.height};
    Rectangle dest = {frog.x, frog.y, FROG_SIZE, FROG_SIZE};
    Vector2 origin = {0, 0};

    DrawTexturePro(frogTex, src, dest, origin, 0.0f, WHITE);

    /* Frog er naam, floating just above its head */
    DrawText(
        FROG_NAME,
        frog.x - 5,
        frog.y - 22,
        16,
        BLACK
    );
}


/* ================================================================
   SECTION: OBSTACLES (CARS & LOGS)
   ================================================================ */

/* Move object horizontally and wrap it back around the screen */
void move_object(Object *obj)
{
    obj->rect.x += obj->speed;

    if (obj->speed > 0 && obj->rect.x > SCREEN_WIDTH)
    {
        obj->rect.x = -obj->rect.width;
    }

    if (obj->speed < 0 && obj->rect.x + obj->rect.width < 0)
    {
        obj->rect.x = SCREEN_WIDTH;
    }
}

/* Draw car - different car types with varied sizes and shapes, using textures */
void draw_car(Object car, Color tint)
{
    Texture2D tex = carTexture;
    bool texValid = false;
    Rectangle src = {0, 0, 0, 0};
    Rectangle dest = car.rect;
    bool flipHorizontally = false;

    switch (car.type)
    {
        case CAR_TYPE_SEDAN:
            if (carTextureValid) { tex = carTexture; texValid = true; }
            dest.width = 80;
            dest.height = 35;
            flipHorizontally = (car.speed < 0);
            break;
        case CAR_TYPE_TRUCK:
            if (carTruckTextureValid) { tex = carTruckTexture; texValid = true; }
            dest.width = 100;
            dest.height = 40;
            dest.y -= 3;
            /* Truck sprite faces LEFT by default, so flip when moving RIGHT */
            flipHorizontally = (car.speed > 0);
            break;
        case CAR_TYPE_SPORTS:
            if (carSportsTextureValid) { tex = carSportsTexture; texValid = true; }
            dest.width = 70;
            dest.height = 28;
            dest.y += 4;
            /* Sports car sprite faces LEFT by default, so flip when moving RIGHT */
            flipHorizontally = (car.speed > 0);
            break;
        case CAR_TYPE_VAN:
            /* No dedicated van texture; reuse truck texture */
            if (carTruckTextureValid) { tex = carTruckTexture; texValid = true; }
            dest.width = 85;
            dest.height = 42;
            dest.y -= 5;
            /* Van uses truck texture (faces LEFT), so flip when moving RIGHT */
            flipHorizontally = (car.speed > 0);
            break;
        default:
            dest.width = 80;
            dest.height = 35;
            flipHorizontally = (car.speed < 0);
            break;
    }

    Vector2 origin = {0, 0};

    if (texValid)
    {
        src = (Rectangle){0, 0, (float)tex.width, (float)tex.height};
        if (flipHorizontally)
        {
            src.width = -(float)tex.width;
        }
        DrawTexturePro(tex, src, dest, origin, 0.0f, tint);
    }
    else
    {
        TraceLog(LOG_WARNING, "draw_car: FALLBACK for type=%d, carTextureValid=%d, carSportsTextureValid=%d, carTruckTextureValid=%d", car.type, carTextureValid, carSportsTextureValid, carTruckTextureValid);
        DrawRectangleRounded(dest, 0.2f, 8, tint);
    }
}

/* Get collision rectangle for car (matches visual representation) */
Rectangle get_car_collision_rect(Object car)
{
    Rectangle rect = car.rect;

    switch (car.type)
    {
        case 0: /* Sedan */
            rect.width = 80;
            rect.height = 35;
            break;
        case 1: /* Truck */
            rect.width = 100;
            rect.height = 40;
            rect.y -= 3;
            break;
        case 2: /* Sports car */
            rect.width = 70;
            rect.height = 28;
            rect.y += 4;
            break;
        case 3: /* Van */
            rect.width = 85;
            rect.height = 42;
            rect.y -= 5;
            break;
        default:
            rect.width = 80;
            rect.height = 35;
            break;
    }

    return rect;
}

/* Draw log - same stretching trick as the car */
void draw_log(Object log, Texture2D logTex)
{
    Rectangle src = {0, 0, (float)logTex.width, (float)logTex.height};
    Vector2 origin = {0, 0};

    DrawTexturePro(logTex, src, log.rect, origin, 0.0f, (Color){255, 220, 170, 255});
}


/* ================================================================
   SECTION: LEVEL SETUP
   Base layouts for cars/logs - the level number scales their speed
   so the game gets harder without needing a whole new layout per
   level.
   ================================================================ */

void reset_level(Object cars[], Object logs[], bool goalFilled[], int level)
{
    float mult = 1.0f + (level - 1) * DIFFICULTY_STEP;

    /* Cars now sit fully inside the road band (390-560) and clear of
       the two lane-divider lines - see the constants above. Three
       lanes, three cars per lane for denser traffic. 
       Rect dimensions here are BASE values; actual collision/draw size
       depends on car type (see get_car_collision_rect / draw_car). */
    Object baseCars[CAR_COUNT] =
    {
        {{100,  400, 80, 35},  2, 0},   /* Sedan - lane 1 */
        {{400,  400, 70, 28},  2, 2},   /* Sports - lane 1 */
        {{700,  400, 80, 35},  2, 0},   /* Sedan - lane 1 */

        {{250,  460, 100, 40}, -2, 1},  /* Truck - lane 2 */
        {{600,  460, 85, 42}, -2, 3},   /* Van - lane 2 */
        {{80,   460, 100, 40}, -2, 1},  /* Truck - lane 2 */

        {{50,   520, 70, 28},  3, 2},   /* Sports - lane 3 */
        {{450,  520, 80, 35},  3, 0},   /* Sedan - lane 3 */
        {{750,  520, 70, 28},  3, 2}    /* Sports - lane 3 */
    };

    /* Logs: 5 rows, spaced exactly LOG_ROW_STEP (40px) apart so they
       line up with every row the frog can actually land on. */
    Object baseLogs[LOG_COUNT] =
    {
        {{40,  RIVER_TOP + 0 * LOG_ROW_STEP, 150, LOG_HEIGHT},  2, 0},
        {{450, RIVER_TOP + 0 * LOG_ROW_STEP, 150, LOG_HEIGHT},  2, 0},

        {{120, RIVER_TOP + 1 * LOG_ROW_STEP, 150, LOG_HEIGHT}, -2, 0},
        {{520, RIVER_TOP + 1 * LOG_ROW_STEP, 150, LOG_HEIGHT}, -2, 0},

        {{60,  RIVER_TOP + 2 * LOG_ROW_STEP, 150, LOG_HEIGHT},  3, 0},
        {{470, RIVER_TOP + 2 * LOG_ROW_STEP, 150, LOG_HEIGHT},  3, 0},

        {{180, RIVER_TOP + 3 * LOG_ROW_STEP, 150, LOG_HEIGHT}, -2, 0},
        {{600, RIVER_TOP + 3 * LOG_ROW_STEP, 150, LOG_HEIGHT}, -2, 0},

        {{20,  RIVER_TOP + 4 * LOG_ROW_STEP, 150, LOG_HEIGHT},  2, 0},
        {{430, RIVER_TOP + 4 * LOG_ROW_STEP, 150, LOG_HEIGHT},  2, 0}
    };

    for (int i = 0; i < CAR_COUNT; i++)
    {
        cars[i] = baseCars[i];
        cars[i].speed *= mult;
    }

    for (int i = 0; i < LOG_COUNT; i++)
    {
        logs[i] = baseLogs[i];
        logs[i].speed *= mult;
    }

    for (int i = 0; i < GOAL_SLOT_COUNT; i++)
    {
        goalFilled[i] = false;
    }
}


/* ================================================================
   SECTION: SCENERY DRAWING
   All the background bands (goal / river / grass / road) live here,
   restyled to look more like an actual place instead of flat
   single-colour rectangles.
   ================================================================ */

/* Goal area: classic Frogger-style 5 home bays with dangerous barriers between.
   Each bay is a safe "home" slot. Barriers between bays have animated hazards. */
void draw_goal_area(bool goalFilled[], Texture2D frogTex, int level, int score)
{
    /* Top decorative area */
    DrawRectangle(0, GOAL_TOP, SCREEN_WIDTH, 25, (Color){30, 100, 40, 255});

    /* LEVEL text above 3rd goal spot (index 2) */
    float bayWidth = (float)SCREEN_WIDTH / GOAL_SLOT_COUNT;
    int levelX = (int)(bayWidth * 2.5f);  /* center of 3rd bay */
    const char *levelLabel = TextFormat("LEVEL: %d", level);
    int levelW = MeasureText(levelLabel, 22);
    DrawText(levelLabel, levelX - levelW / 2, 2, 22, (Color){255, 230, 120, 255});

    /* SCORE text above 2nd goal spot (index 1) */
    int scoreX = (int)(bayWidth * 1.5f);  /* center of 2nd bay */
    const char *scoreLabel = TextFormat("SCORE: %d", score);
    int scoreW = MeasureText(scoreLabel, 22);
    DrawText(scoreLabel, scoreX - scoreW / 2, 2, 22, (Color){255, 230, 120, 255});

    /* Bay dimensions */
    int bayCount = GOAL_SLOT_COUNT;
    int bayTop = 25;
    int bayHeight = GOAL_HEIGHT - 25;  /* 95px */

    for (int i = 0; i < bayCount; i++)
    {
        int bx = (int)(bayWidth * i);
        int bw = (int)bayWidth;

        /* Bay background - claimed bays show light green, empty show water decoration */
        Color bayBg = goalFilled[i] ? (Color){100, 200, 48, 255} : (Color){40, 150, 50, 255};
        DrawRectangle(bx + 2, bayTop + 2, bw - 4, bayHeight - 4, bayBg);

        /* Bay border */
        Color borderColor = goalFilled[i] ? (Color){180, 150, 50, 255} : (Color){20, 90, 30, 255};
        DrawRectangleLinesEx((Rectangle){bx + 2, bayTop + 2, bw - 4, bayHeight - 4}, 3, borderColor);

        if (goalFilled[i])
        {
            /* Claimed bay - small frog icon instead of flag */
            int iconSize = 28;
            int fx = bx + bw/2;
            int fy = bayTop + bayHeight/2 - 8;

            Rectangle src = {0, 0, (float)frogTex.width, (float)frogTex.height};
            Rectangle dest = {fx - iconSize/2, fy - iconSize/2, (float)iconSize, (float)iconSize};
            Vector2 origin = {0, 0};

            DrawTexturePro(frogTex, src, dest, origin, 0.0f, WHITE);

            DrawText("SAFE", bx + bw/2 - MeasureText("SAFE", 14)/2, bayTop + bayHeight - 22, 14, GOLD);
        }
        else
        {
            /* Empty bay - "GOAL" label + subtle lily decoration */
            DrawText("GOAL", bx + bw/2 - MeasureText("GOAL", 14)/2, bayTop + bayHeight - 22, 14, (Color){200, 220, 100, 255});

            int decoX = bx + bw/2;
            int decoY = bayTop + bayHeight/2;
            DrawCircle(decoX, decoY, 18, (Color){40, 140, 50, 100});
            DrawCircle(decoX, decoY, 12, (Color){60, 170, 70, 100});
        }
    }

    /* Barriers BETWEEN bays - with animated hazards */
    float time = (float)GetTime();

    for (int i = 0; i < bayCount - 1; i++)
    {
        int barrierX = (int)(bayWidth * (i + 1));
        int barrierWidth = 28;  /* Width of the barrier zone */
        int left = barrierX - barrierWidth/2;
        int right = barrierX + barrierWidth/2;

        /* Barrier base - dark stone/wall */
        DrawRectangle(left, bayTop, barrierWidth, bayHeight, (Color){40, 35, 35, 255});
        DrawRectangleLines(left, bayTop, barrierWidth, bayHeight, (Color){80, 75, 75, 255});

        /* Animated hazard: Alligator heads popping up */
        Color gatorColor = (Color){50, 140, 50, 255};
        Color gatorDark = (Color){35, 110, 35, 255};
        Color eyeColor = (Color){255, 255, 100, 255};

        for (int g = 0; g < 3; g++)
        {
            float gatorY = bayTop + 15 + g * 28;
            /* Bobbing animation */
            float bob = sinf(time * 1.5f + i * 2.0f + g * 1.8f) * 6.0f;
            int gy = (int)(gatorY + bob);

            /* Gator head body */
            DrawCircle(left + barrierWidth/2, gy, 12, gatorColor);
            DrawCircle(left + barrierWidth/2 - 3, gy - 2, 5, eyeColor);
            DrawCircle(left + barrierWidth/2 + 3, gy - 2, 5, eyeColor);
            DrawCircle(left + barrierWidth/2 - 3, gy - 2, 2, BLACK);
            DrawCircle(left + barrierWidth/2 + 3, gy - 2, 2, BLACK);

            /* Snout */
            DrawTriangle(
                (Vector2){left + barrierWidth/2 - 8, gy + 5},
                (Vector2){left + barrierWidth/2, gy + 14},
                (Vector2){left + barrierWidth/2 + 8, gy + 5},
                gatorDark
            );
        }

        /* Warning stripes on barrier edges */
        for (int s = 0; s < bayHeight; s += 16)
        {
            Color stripe = (s % 32 == 0) ? (Color){255, 220, 50, 255} : (Color){140, 30, 30, 255};
            DrawRectangle(left, bayTop + s, barrierWidth, 8, stripe);
        }
    }
}

/* River: solid water colour plus a few slow-moving wave lines
   (animated with GetTime()) so the water doesn't look totally flat. */
void draw_river(float time)
{
    DrawRectangle(0, RIVER_TOP, SCREEN_WIDTH, RIVER_HEIGHT, (Color){35, 130, 220, 255});

    Color waveColor = (Color){140, 195, 240, 130};

    for (int i = 0; i < 6; i++)
    {
        float waveY = RIVER_TOP + 16 + i * 32;
        float offset = sinf(time * 1.6f + i * 1.3f) * 12.0f;

        DrawLineEx(
            (Vector2){0 + offset, waveY},
            (Vector2){SCREEN_WIDTH + offset, waveY},
            2.0f,
            waveColor
        );
    }
}

/* A mowed-lawn stripe pattern, used for both grass bands so they read
   as an actual field rather than a flat green block. */
void draw_grass_band(int y, int height)
{
    int stripeWidth = 40;

    for (int x = 0; x < SCREEN_WIDTH; x += stripeWidth)
    {
        bool light = (x / stripeWidth) % 2 == 0;
        Color c = light ? (Color){110, 200, 100, 255} : (Color){85, 175, 85, 255};
        DrawRectangle(x, y, stripeWidth, height, c);
    }
}

/* Road: dark asphalt plus dashed lane-divider lines placed BETWEEN
   the car lanes (not on top of them, like the original layout did). */
void draw_road(void)
{
    DrawRectangle(0, ROAD_TOP, SCREEN_WIDTH, ROAD_HEIGHT, (Color){75, 75, 85, 255});

    for (int x = 0; x < SCREEN_WIDTH; x += 80)
    {
        DrawRectangle(x, LANE_DIVIDER_1_Y, 40, 5, (Color){235, 220, 60, 255});
        DrawRectangle(x, LANE_DIVIDER_2_Y, 40, 5, (Color){235, 220, 60, 255});
    }
}

/* Sandy field between river and road - deeper golden tan with subtle grain pattern */
void draw_sand_band(int y, int height)
{
    Color baseSand = (Color){210, 180, 130, 255};
    Color darkSand = (Color){180, 155, 110, 255};
    Color lightSand = (Color){230, 200, 155, 255};

    DrawRectangle(0, y, SCREEN_WIDTH, height, baseSand);

    /* Add subtle sand grain texture with random dots */
    for (int i = 0; i < 200; i++)
    {
        int sx = (i * 37) % SCREEN_WIDTH;
        int sy = y + (i * 73) % height;
        Color grain = (i % 3 == 0) ? darkSand : lightSand;
        DrawRectangle(sx, sy, 2, 2, grain);
    }

    /* Add a few larger "pebbles" for variety */
    for (int i = 0; i < 30; i++)
    {
        int px = (i * 197) % SCREEN_WIDTH;
        int py = y + (i * 151) % height;
        DrawCircle(px, py, 2, darkSand);
    }
}


/* ================================================================
   SECTION: SCREENS / OVERLAYS (menu, pause, level complete, game over, HUD)
   ================================================================ */

void draw_menu(int highScore, Texture2D startBgTex)
{
    /* Draw the detailed background image (contains title, frog-lives box, flowers, butterflies, lily pads, logs, turtles) */
    DrawTexturePro(
        startBgTex,
        (Rectangle){0, 0, (float)startBgTex.width, (float)startBgTex.height},
        (Rectangle){0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()},
        (Vector2){0, 0},
        0.0f,
        WHITE
    );

    /* HIGH SCORE display - moved to bottom strip in plain stone/sand border area */
    {
        const char *hiLabel = "HIGH SCORE:";
        const char *hiValueStr = TextFormat("%d", highScore);
        int fontSize = 20;
        int labelW = MeasureText(hiLabel, fontSize);
        int valueW = MeasureText(hiValueStr, fontSize);
        int textW = labelW + 10 + valueW;
        int padding = 10;
        int bgW = textW + padding * 2;
        int bgH = fontSize + padding * 2;
        int bgX = SCREEN_WIDTH - bgW - 20;
        int bgY = SCREEN_HEIGHT - bgH - 20;

        /* Semi-transparent dark rounded rectangle backing for legibility */
        DrawRectangleRounded((Rectangle){bgX, bgY, bgW, bgH}, 6, 8, Fade(BLACK, 0.5f));

        /* Text on top in yellow */
        DrawText(hiLabel, bgX + padding, bgY + padding, fontSize, YELLOW);
        DrawText(hiValueStr, bgX + padding + labelW + 10, bgY + padding, fontSize, YELLOW);
    }

    /* "PRESS ENTER TO START" - positioned in open water area between title (upper-third) and turtles row */
    {
        const char *startText = "PRESS ENTER TO START";
        int startSize = 32;
        int startW = MeasureText(startText, startSize);
        int startPadding = 12;
        int startBgW = startW + startPadding * 2;
        int startBgH = startSize + startPadding * 2;
        int startBgX = (SCREEN_WIDTH - startBgW) / 2;
        int startBgY = 200;  /* below title area, above center frog-on-log illustration */

        /* Pulsing alpha */
        float pulseAlpha = 0.4f + 0.6f * (sinf(GetTime() * 4.2f) * 0.5f + 0.5f);

        /* Semi-transparent dark rounded backing */
        DrawRectangleRounded((Rectangle){startBgX, startBgY, startBgW, startBgH}, 6, 8, Fade(BLACK, 0.5f));

        /* Text */
        DrawText(startText, startBgX + startPadding, startBgY + startPadding, startSize, Fade(YELLOW, pulseAlpha));
    }

    /* CONTROLS text - at very bottom in stone/sand border strip */
    {
        const char *controls = "ARROW KEYS or WASD = Move    P = Pause";
        int ctrlSize = 16;
        int ctrlW = MeasureText(controls, ctrlSize);
        int ctrlPadding = 10;
        int ctrlBgW = ctrlW + ctrlPadding * 2;
        int ctrlBgH = ctrlSize + ctrlPadding * 2;
        int ctrlBgX = (SCREEN_WIDTH - ctrlBgW) / 2;
        int ctrlBgY = SCREEN_HEIGHT - ctrlBgH - 10;

        /* Semi-transparent dark rounded backing */
        DrawRectangleRounded((Rectangle){ctrlBgX, ctrlBgY, ctrlBgW, ctrlBgH}, 6, 8, Fade(BLACK, 0.5f));

        /* Text */
        DrawText(controls, ctrlBgX + ctrlPadding, ctrlBgY + ctrlPadding, ctrlSize, Fade(YELLOW, 0.9f));
    }
}

void draw_pause_overlay(void)
{
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.6f));
    DrawText("PAUSED", SCREEN_WIDTH / 2 - MeasureText("PAUSED", 50) / 2, SCREEN_HEIGHT / 2 - 50, 50, YELLOW);
    DrawText("Press P to resume", SCREEN_WIDTH / 2 - MeasureText("Press P to resume", 22) / 2, SCREEN_HEIGHT / 2 + 15, 22, (Color){255, 230, 120, 255});
}

void draw_level_complete_overlay(int level)
{
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.55f));

    const char *msg = TextFormat("LEVEL %d COMPLETE!", level);
    DrawText(msg, SCREEN_WIDTH / 2 - MeasureText(msg, 45) / 2, SCREEN_HEIGHT / 2 - 40, 45, GOLD);

    const char *next = TextFormat("Get ready for level %d...", level + 1);
    DrawText(next, SCREEN_WIDTH / 2 - MeasureText(next, 20) / 2, SCREEN_HEIGHT / 2 + 30, 20, (Color){255, 230, 120, 255});
}

void draw_game_over_overlay(int score, int highScore, bool isNewHighScore)
{
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.7f));

    DrawText("khela parena", 260, 210, 50, RED);

    DrawText(TextFormat("Final Score: %d", score), 315, 280, 25, (Color){255, 230, 120, 255});
    DrawText(TextFormat("High Score: %d", highScore), 315, 315, 25, (Color){255, 220, 60, 255});

    if (isNewHighScore)
    {
        DrawText("NEW HIGH SCORE!", SCREEN_WIDTH / 2 - MeasureText("NEW HIGH SCORE!", 22) / 2, 350, 22, (Color){255, 200, 0, 255});
    }

    DrawText("Press R to Restart", 290, 400, 25, (Color){255, 230, 120, 255});
}

/* Score/level/timer text plus the heart-based lives display */
void draw_hud(int lives, float timeRemaining)
{
    /* HUD bar spans y=0 to y=25 (top decorative area in draw_goal_area).
       Vertical center = 12.5, so use y=12 for heart centers and y=2 for text baseline. */
    int fontSize = 22;
    int textY = 2;  /* vertically centered in 25px bar */
    int heartCenterY = 12;
    int leftMargin = 20;
    Color hudColor = (Color){255, 230, 120, 255};

    /* LEFT GROUP: LIVES only */
    const char *livesPrefix = "LIVES:";
    int prefixW = MeasureText(livesPrefix, fontSize);
    DrawText(livesPrefix, leftMargin, textY, fontSize, hudColor);

    int heartSpacing = 26;
    int heartSize = 20;
    int heartsStartX = leftMargin + prefixW + 14;
    for (int i = 0; i < lives; i++)
    {
        draw_heart(heartsStartX + i * heartSpacing, heartCenterY, heartSize, (Color){220, 40, 60, 255});
    }

    /* TOP-RIGHT: mini time bar (30s max) */
    float maxTime = 30.0f;
    float pct = timeRemaining / maxTime;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;

    int barWidth = 120;
    int barHeight = 10;
    int barX = SCREEN_WIDTH - 20 - barWidth;
    int barY = (25 - barHeight) / 2;  /* vertically centered in 25px bar */

    /* Bar background */
    DrawRectangle(barX, barY, barWidth, barHeight, (Color){60, 60, 60, 255});
    /* Bar fill */
    Color barColor = (pct <= 0.17f) ? RED : (Color){255, 230, 120, 255};
    DrawRectangle(barX, barY, (int)(barWidth * pct), barHeight, barColor);
    /* Bar border */
    DrawRectangleLines(barX, barY, barWidth, barHeight, (Color){180, 180, 180, 255});
}


/* ================================================================
   SECTION: MAIN - window setup, asset loading, and the game loop
   ================================================================ */

int main(void)
{
    InitWindow(
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        "ZISIN FROGGER"
    );

    InitAudioDevice();
    SetTargetFPS(60);

    /* ------------- load assets ------------- */

    TraceLog(LOG_INFO, "Working directory: %s", GetWorkingDirectory());

    Texture2D frogTex = LoadTexture("assets/images/frog.png");
    carTexture = LoadTexture("assets/images/car.png");
    carSportsTexture = LoadTexture("assets/images/car_sports.png");
    carTruckTexture = LoadTexture("assets/images/car_truck.png");
    Texture2D logTex  = LoadTexture("assets/images/log.png");
    Texture2D startBgTex = LoadTexture("assets/start_bg.png");

    carTextureValid = (carTexture.id != 0);
    carSportsTextureValid = (carSportsTexture.id != 0);
    carTruckTextureValid = (carTruckTexture.id != 0);

    if (frogTex.id == 0 || logTex.id == 0)
    {
        TraceLog(LOG_ERROR, "Failed to load texture assets");
        CloseWindow();
        return 1;
    }

    if (!carTextureValid) TraceLog(LOG_WARNING, "Failed to load assets/images/car.png, using fallback");
    if (!carSportsTextureValid) TraceLog(LOG_WARNING, "Failed to load assets/images/car_sports.png, using fallback");
    if (!carTruckTextureValid) TraceLog(LOG_WARNING, "Failed to load assets/images/car_truck.png, using fallback");

    Sound sndJump     = LoadSound("assets/sounds/jump.wav");
    Sound sndCrash    = LoadSound("assets/sounds/crash.wav");
    Sound sndSplash   = LoadSound("assets/sounds/splash.wav");   /* drowning sound */
    Sound sndGoal     = LoadSound("assets/sounds/goal.wav");     /* single lily pad reached */
    Sound sndLevelUp  = LoadSound("assets/sounds/levelup.wav");  /* all 5 pads filled */
    Sound sndGameOver = LoadSound("assets/sounds/gameover.wav");

    if (sndJump.frameCount == 0 || sndCrash.frameCount == 0 || sndSplash.frameCount == 0 ||
        sndGoal.frameCount == 0 || sndLevelUp.frameCount == 0 || sndGameOver.frameCount == 0)
    {
        TraceLog(LOG_WARNING, "Some sound assets failed to load (continuing without sound)");
    }

    /* nicer, more natural-looking per-lane car colours */
    Color laneColors[CAR_COUNT] =
    {
        (Color){230, 60, 60, 255},
        (Color){255, 170, 50, 255},
        (Color){150, 100, 220, 255},
        (Color){200, 50, 90, 255},
        (Color){240, 120, 180, 255},
        (Color){230, 60, 60, 255},
        (Color){100, 200, 100, 255},
        (Color){255, 100, 255, 255},
        (Color){100, 200, 255, 255}
    };

    /* ------------- game state ------------- */

    GameState state = STATE_MENU;

    int highScore = load_high_score();
    bool newHighScoreThisRun = false;

    Vector2 frog;
    reset_frog(&frog);

    Object cars[CAR_COUNT];
    Object logs[LOG_COUNT];
    bool goalFilled[GOAL_SLOT_COUNT];

    int score = 0;
    int lives = 3;
    int level = 1;
    float timeRemaining = 30.0f;  /* 30-second countdown timer */

    reset_level(cars, logs, goalFilled, level);

    float levelCompleteTimer = 0.0f;
    bool gameOverSoundPlayed = false;

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        float time = (float)GetTime();

        /* ------------- input / state transitions ------------- */

        if (state == STATE_MENU)
        {
            if (IsKeyPressed(KEY_ENTER))
            {
                score = 0;
                lives = 3;
                level = 1;
                timeRemaining = 30.0f;
                newHighScoreThisRun = false;
                gameOverSoundPlayed = false;
                reset_frog(&frog);
                reset_level(cars, logs, goalFilled, level);
                state = STATE_PLAYING;
            }
        }
        else if (state == STATE_PLAYING)
        {
            if (IsKeyPressed(KEY_P))
            {
                state = STATE_PAUSED;
            }

            timeRemaining -= dt;
            if (timeRemaining <= 0.0f)
            {
                lives--;
                timeRemaining = 30.0f;
                if (sndGameOver.frameCount > 0) PlaySound(sndGameOver);
                reset_frog(&frog);
            }

            bool moved = false;

            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
            {
                frog.y -= 40;
                moved = true;
            }

            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
            {
                frog.y += 40;
                moved = true;
            }

            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A))
            {
                frog.x -= 40;
                moved = true;
            }

            if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
            {
                frog.x += 40;
                moved = true;
            }

            if (moved)
            {
                if (sndJump.frameCount > 0) PlaySound(sndJump);
            }

            /* Keep frog inside screen */

            if (frog.x < 0)
            {
                frog.x = 0;
            }
            else if (frog.x > SCREEN_WIDTH - FROG_SIZE)
            {
                frog.x = SCREEN_WIDTH - FROG_SIZE;
            }

            if (frog.y < 0)
            {
                frog.y = 0;
            }
            else if (frog.y > SCREEN_HEIGHT - FROG_SIZE)
            {
                frog.y = SCREEN_HEIGHT - FROG_SIZE;
            }

            /* gari move kora */
            for (int i = 0; i < CAR_COUNT; i++)
            {
                move_object(&cars[i]);
            }

            /* kath move korte */
            for (int i = 0; i < LOG_COUNT; i++)
            {
                move_object(&logs[i]);
            }

            Rectangle frogRect = {frog.x, frog.y, FROG_SIZE, FROG_SIZE};

            /* garite dhakka - hit by a car */
            for (int i = 0; i < CAR_COUNT; i++)
            {
                Rectangle carRect = get_car_collision_rect(cars[i]);
                if (CheckCollisionRecs(frogRect, carRect))
                {
                    lives--;
                    timeRemaining = 30.0f;
                    if (sndCrash.frameCount > 0) PlaySound(sndCrash);
                    reset_frog(&frog);
                    break;
                }
            }

            /* nodite dube jaowa check - in the river band? */
            if (frog.y >= RIVER_TOP && frog.y < RIVER_TOP + RIVER_HEIGHT)
            {
                int logerupor = 0;

                for (int i = 0; i < LOG_COUNT; i++)
                {
                    if (CheckCollisionRecs(frogRect, logs[i].rect))
                    {
                        logerupor = 1;
                        frog.x += logs[i].speed;
                        break;
                    }
                }

                if (!logerupor)
                {
                    lives--;
                    timeRemaining = 30.0f;
                    if (sndSplash.frameCount > 0) PlaySound(sndSplash); /* drowning sound */
                    reset_frog(&frog);
                }
            }

            /* reach goal - check if frog is in one of the 5 home bays */
            if (frog.y < GOAL_HEIGHT)
            {
                float bayWidth = (float)SCREEN_WIDTH / GOAL_SLOT_COUNT;
                int bayTop = 25;
                int bayHeight = GOAL_HEIGHT - 25;  /* 95px */
                int barrierWidth = 28;

                float frogCenterX = frog.x + FROG_SIZE / 2.0f;
                float frogCenterY = frog.y + FROG_SIZE / 2.0f;

                bool inBay = false;
                int bayIndex = -1;
                bool inBarrier = false;

                /* Check each bay */
                for (int i = 0; i < GOAL_SLOT_COUNT; i++)
                {
                    int bx = (int)(bayWidth * i);
                    int bw = (int)bayWidth;
                    int bayLeft = bx + 2;      /* Bay inner left (accounting for border) */
                    int bayRight = bx + bw - 2; /* Bay inner right */

                    if (frogCenterX >= bayLeft && frogCenterX <= bayRight &&
                        frogCenterY >= bayTop && frogCenterY <= bayTop + bayHeight)
                    {
                        inBay = true;
                        bayIndex = i;
                        break;
                    }
                }

                /* Check barriers between bays */
                if (!inBay)
                {
                    for (int i = 0; i < GOAL_SLOT_COUNT - 1; i++)
                    {
                        int barrierX = (int)(bayWidth * (i + 1));
                        int left = barrierX - barrierWidth/2;
                        int right = barrierX + barrierWidth/2;

                        if (frogCenterX >= left && frogCenterX <= right &&
                            frogCenterY >= bayTop && frogCenterY <= bayTop + bayHeight)
                        {
                            inBarrier = true;
                            break;
                        }
                    }
                }

                if (inBay)
                {
                    if (goalFilled[bayIndex])
                    {
                        /* Bay already claimed - bounce back to safe sand */
                        frog.y = MEDIAN_TOP + MEDIAN_HEIGHT;
                    }
                    else
                    {
                        /* Claim this bay */
                        goalFilled[bayIndex] = true;
                        score += 10;
                        timeRemaining = 30.0f;
                        if (sndGoal.frameCount > 0) PlaySound(sndGoal);
                        reset_frog(&frog);

                        bool allFilled = true;
                        for (int i = 0; i < GOAL_SLOT_COUNT; i++)
                        {
                            if (!goalFilled[i])
                            {
                                allFilled = false;
                                break;
                            }
                        }

                        if (allFilled)
                        {
                            if (sndLevelUp.frameCount > 0) PlaySound(sndLevelUp);
                            levelCompleteTimer = 2.0f;
                            state = STATE_LEVEL_COMPLETE;
                        }
                    }
                }
                else if (inBarrier || frogCenterY < bayTop)
                {
                    /* Hit barrier or upper decorative area - lose life */
                    lives--;
                    timeRemaining = 30.0f;
                    if (sndSplash.frameCount > 0) PlaySound(sndSplash);
                    reset_frog(&frog);
                }
                /* If in goal area but not in bay or barrier (shouldn't happen), treat as barrier */
                else
                {
                    lives--;
                    timeRemaining = 30.0f;
                    if (sndSplash.frameCount > 0) PlaySound(sndSplash);
                    reset_frog(&frog);
                }
            }

            if (lives <= 0)
            {
                state = STATE_GAME_OVER;
            }
        }
        else if (state == STATE_PAUSED)
        {
            if (IsKeyPressed(KEY_P))
            {
                state = STATE_PLAYING;
            }
        }
        else if (state == STATE_LEVEL_COMPLETE)
        {
            levelCompleteTimer -= dt;

            if (levelCompleteTimer <= 0.0f)
            {
                level++;
                timeRemaining = 30.0f;
                reset_frog(&frog);
                reset_level(cars, logs, goalFilled, level);
                state = STATE_PLAYING;
            }
        }
        else if (state == STATE_GAME_OVER)
        {
            if (!gameOverSoundPlayed)
            {
                if (sndGameOver.frameCount > 0) PlaySound(sndGameOver);
                gameOverSoundPlayed = true;

                if (score > highScore)
                {
                    highScore = score;
                    save_high_score(highScore);
                    newHighScoreThisRun = true;
                }
            }

            if (IsKeyPressed(KEY_R))
            {
                state = STATE_MENU;
            }
        }

        /* ------------- drawing ------------- */

        BeginDrawing();

        if (state == STATE_MENU)
        {
            draw_menu(highScore, startBgTex);
        }
        else
        {
            ClearBackground(RAYWHITE);

            /* scenery, back to front */
            draw_goal_area(goalFilled, frogTex, level, score);
            draw_river(time);
            draw_grass_band(MEDIAN_TOP, MEDIAN_HEIGHT);  /* Grass strip between river and road */
            draw_road();
            draw_grass_band(BOTTOM_TOP, BOTTOM_HEIGHT);  /* Grass strip below road */

            /* lily pad slots are now drawn inside draw_goal_area() */

            /* logs (under the frog, in the river band) */
            for (int i = 0; i < LOG_COUNT; i++)
            {
                draw_log(logs[i], logTex);
            }

            /* cars (in the road band) */
            for (int i = 0; i < CAR_COUNT; i++)
            {
                draw_car(cars[i], laneColors[i]);
            }

            /* frog on top of everything */
            draw_frog(frog, frogTex);

            /* HUD */
            draw_hud(lives, timeRemaining);

            if (state == STATE_PAUSED)
            {
                draw_pause_overlay();
            }
            else if (state == STATE_LEVEL_COMPLETE)
            {
                draw_level_complete_overlay(level);
            }
            else if (state == STATE_GAME_OVER)
            {
                draw_game_over_overlay(score, highScore, newHighScoreThisRun);
            }
        }

        EndDrawing();
    }

    UnloadTexture(frogTex);
    if (carTextureValid) UnloadTexture(carTexture);
    if (carSportsTextureValid) UnloadTexture(carSportsTexture);
    if (carTruckTextureValid) UnloadTexture(carTruckTexture);
    UnloadTexture(logTex);
    UnloadTexture(startBgTex);

    UnloadSound(sndJump);
    UnloadSound(sndCrash);
    UnloadSound(sndSplash);
    UnloadSound(sndGoal);
    UnloadSound(sndLevelUp);
    UnloadSound(sndGameOver);

    CloseAudioDevice();
    CloseWindow();

    return 0;
}
