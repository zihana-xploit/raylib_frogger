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

#define FROG_NAME "Zisin"  /* default name, shown until the player sets their own */

/* The player's chosen name - shown floating above the frog and saved
   with leaderboard entries. Editable from the main menu ("NAME" button).
   Starts as FROG_NAME and can be changed any time before playing. */
char playerName[32] = FROG_NAME;

#define HIGH_SCORE_FILE "highscore.txt"

/* how much faster obstacles get, per level - BASE values for MEDIUM
   difficulty; actual values used at runtime are the variables
   below, which apply_difficulty() adjusts per difficulty mode. */
#define BASE_DIFFICULTY_STEP 0.12f  /* vehicle speed increase per level */

/* Level scaling constants (MEDIUM baseline) */
#define BASE_VEHICLE_SPACING_SCALE 0.08f  /* spacing reduction per level */
#define BASE_VEHICLE_MIN_SPACING 0.6f     /* min spacing multiplier (never below 60%) */
#define BASE_LOG_SPACING_SCALE 0.1f       /* log gap increase per level */
#define BASE_LOG_MAX_GAP 2.5f             /* max log gap multiplier */
#define BASE_TIMER_SCALE 0.1f             /* timer reduction per level */
#define BASE_TIMER_MIN_MULT 0.1f          /* min timer multiplier (never below 50%) */
#define BASE_TIMER_DURATION 30.0f         /* seconds on the goal-row timer at level 1 */
#define BASE_STARTING_LIVES 5


/* ================================================================
   SECTION: DIFFICULTY
   Easy / Medium / Hard - adjusts how fast the level-to-level scaling
   ramps up, the starting timer duration, and starting lives. Chosen
   from the main menu (LEFT/RIGHT arrows) before pressing ENTER to
   start; takes effect on the next new game.
   ================================================================ */

typedef enum Difficulty
{
    DIFF_EASY,
    DIFF_MEDIUM,
    DIFF_HARD
} Difficulty;

Difficulty currentDifficulty = DIFF_MEDIUM;

/* Runtime scaling values - set by apply_difficulty(), read everywhere
   the old #defines used to be read directly. */
float g_difficultyStep;
float g_vehicleSpacingScale;
float g_vehicleMinSpacing;
float g_logSpacingScale;
float g_logMaxGap;
float g_timerScale;
float g_timerMinMult;
float g_baseTimerDuration;
int   g_startingLives;

void apply_difficulty(Difficulty d)
{
    switch (d)
    {
        case DIFF_EASY:
            g_difficultyStep      = BASE_DIFFICULTY_STEP * 0.6f;   /* obstacles speed up slower */
            g_vehicleSpacingScale = BASE_VEHICLE_SPACING_SCALE * 0.6f;
            g_vehicleMinSpacing   = 0.75f;                          /* never gets as crowded */
            g_logSpacingScale     = BASE_LOG_SPACING_SCALE * 0.6f;
            g_logMaxGap           = 2.0f;
            g_timerScale          = BASE_TIMER_SCALE * 0.6f;
            g_timerMinMult        = 0.1f;
            g_baseTimerDuration   = 35.0f;                          /* more time per level */
            g_startingLives       = 4;                              /* one extra life */
            break;

        case DIFF_HARD:
            g_difficultyStep      = BASE_DIFFICULTY_STEP * 1.5f;   /* obstacles speed up faster */
            g_vehicleSpacingScale = BASE_VEHICLE_SPACING_SCALE * 1.4f;
            g_vehicleMinSpacing   = 0.45f;                          /* allowed to get much denser */
            g_logSpacingScale     = BASE_LOG_SPACING_SCALE * 1.4f;
            g_logMaxGap           = 3.2f;
            g_timerScale          = BASE_TIMER_SCALE * 1.4f;
            g_timerMinMult        = 0.1f;
            g_baseTimerDuration   = 25.0f;                          /* less time per level */
            g_startingLives       = 2;                              /* one fewer life */
            break;

        case DIFF_MEDIUM:
        default:
            g_difficultyStep      = BASE_DIFFICULTY_STEP;
            g_vehicleSpacingScale = BASE_VEHICLE_SPACING_SCALE;
            g_vehicleMinSpacing   = BASE_VEHICLE_MIN_SPACING;
            g_logSpacingScale     = BASE_LOG_SPACING_SCALE;
            g_logMaxGap           = BASE_LOG_MAX_GAP;
            g_timerScale          = BASE_TIMER_SCALE;
            g_timerMinMult        = BASE_TIMER_MIN_MULT;
            g_baseTimerDuration   = BASE_TIMER_DURATION;
            g_startingLives       = BASE_STARTING_LIVES;
            break;
    }
}

const char *difficulty_name(Difficulty d)
{
    switch (d)
    {
        case DIFF_EASY:   return "EASY";
        case DIFF_HARD:   return "HARD";
        case DIFF_MEDIUM:
        default:          return "MEDIUM";
    }
}

/* Computes the goal-row timer's remaining-time multiplier for a given
   level, using whichever difficulty's scaling is currently active.
   Replaces the timerMult calculation that used to be copy-pasted at
   every spot the timer gets reset. */
float compute_timer_mult(int level)
{
    float mult = 1.0f - (level - 1) * g_timerScale;
    if (mult < g_timerMinMult) mult = g_timerMinMult;
    return mult;
}

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
    STATE_LEADERBOARD,
    STATE_HOW_TO_PLAY,
    STATE_CREDITS,
    STATE_NAME_ENTRY,
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

typedef struct
{
    Rectangle rect;
    float speed;
    float phaseOffset;  /* for movement sync */
} SharkFin;

#define SHARK_FIN_COUNT 36  /* 6 per gap × 4 gaps + 6 left edge + 6 right edge = 36 total */
#define SHARK_FIN_WIDTH 16
#define SHARK_FIN_HEIGHT 14

Texture2D carTexture;
Texture2D carSportsTexture;
Texture2D carTruckTexture;
Texture2D turtleTexture;
bool carTextureValid = false;
bool carSportsTextureValid = false;
bool carTruckTextureValid = false;
bool turtleTextureValid = false;


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
   SECTION: LEADERBOARD (TOP SCORERS)
   A small text file storing the best runs across all sessions,
   name + score per line, kept sorted highest-first and capped at
   LEADERBOARD_MAX entries.
   ================================================================ */

#define LEADERBOARD_FILE "leaderboard.txt"
#define LEADERBOARD_MAX 10

typedef struct LeaderboardEntry
{
    char name[32];
    int score;
} LeaderboardEntry;

/* Reads the leaderboard file into entries[], returns how many were read. */
int load_leaderboard(LeaderboardEntry entries[LEADERBOARD_MAX])
{
    int count = 0;
    FILE *f = fopen(LEADERBOARD_FILE, "r");

    if (f != NULL)
    {
        while (count < LEADERBOARD_MAX &&
               fscanf(f, "%31s %d", entries[count].name, &entries[count].score) == 2)
        {
            count++;
        }
        fclose(f);
    }

    return count;
}

void save_leaderboard(LeaderboardEntry entries[LEADERBOARD_MAX], int count)
{
    FILE *f = fopen(LEADERBOARD_FILE, "w");

    if (f != NULL)
    {
        for (int i = 0; i < count; i++)
        {
            fprintf(f, "%s %d\n", entries[i].name, entries[i].score);
        }
        fclose(f);
    }
}

/* Inserts a new score into the leaderboard under the player's current
   chosen name (playerName - set from the main menu's NAME button),
   keeps it sorted highest-to-lowest, and trims it back down to
   LEADERBOARD_MAX entries. Returns the 1-based rank the new score
   landed at, or -1 if it didn't make the top LEADERBOARD_MAX. */
int add_leaderboard_entry(int score)
{
    LeaderboardEntry entries[LEADERBOARD_MAX];
    int count = load_leaderboard(entries);

    /* find insertion point (descending order) */
    int insertPos = count;
    for (int i = 0; i < count; i++)
    {
        if (score > entries[i].score)
        {
            insertPos = i;
            break;
        }
    }

    if (insertPos >= LEADERBOARD_MAX)
    {
        return -1; /* didn't make the cut */
    }

    int newCount = (count < LEADERBOARD_MAX) ? count + 1 : LEADERBOARD_MAX;

    /* shift everything at/after insertPos down by one, dropping the last if full */
    for (int i = newCount - 1; i > insertPos; i--)
    {
        entries[i] = entries[i - 1];
    }

    TextCopy(entries[insertPos].name, playerName);
    entries[insertPos].score = score;

    save_leaderboard(entries, newCount);
    return insertPos + 1;
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

    /* Player's chosen name, floating just above its head, centered */
    int nameW = MeasureText(playerName, 16);
    DrawText(
        playerName,
        (int)(frog.x + FROG_SIZE / 2 - nameW / 2),
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

/* Draw a floating turtle-group "platform" in place of a log - a cluster
   of 3 turtles sitting side by side, close enough together that they act
   as one connected safe platform (collision still uses the same
   rectangle as a log would, this only changes what's drawn on top).
   Uses turtleTexture (assets/images/turtle.png) if it loaded correctly;
   otherwise falls back to the hand-drawn shapes below so the game never
   breaks just because an image is missing. */
void draw_turtle_group(Rectangle rect, float speed)
{
    if (turtleTextureValid)
    {
        int turtleCount = 3;
        float baseW = rect.width / turtleCount;   /* spacing between turtle centers stays anchored to the log-sized rect */
        bool facingRight = (speed >= 0);

        /* turtle.png has empty transparent padding around the actual shell -
           crop to just the drawn content so turtles look bigger/closer
           together instead of leaving invisible gaps between them. */
        float contentX = 22, contentY = 10, contentW = 86, contentH = 71;

        Rectangle src = {contentX, contentY, contentW, contentH};
        if (!facingRight)
        {
            /* mirror within the same content box when moving left */
            src.width = -contentW;
        }
        Vector2 origin = {0, 0};

        /* draw each turtle bigger than the raw sprite but with a bit of
           breathing room between them, so it reads as a connected group
           without the shells crushing into each other. */
        float drawW = baseW * 1.25f;
        float drawH = rect.height * 1.1f;
        float destY = rect.y + rect.height / 2.0f - drawH / 2.0f;

        for (int t = 0; t < turtleCount; t++)
        {
            float centerX = rect.x + baseW * t + baseW / 2.0f;
            Rectangle dest = {
                centerX - drawW / 2.0f,
                destY,
                drawW,
                drawH
            };
            DrawTexturePro(turtleTexture, src, dest, origin, 0.0f, WHITE);
        }
        return;
    }

    /* ---- fallback: hand-drawn turtle shapes (used if turtle.png fails to load) ---- */
    Color shellColor  = (Color){190, 60, 45, 255};   /* red/orange-brown shell */
    Color shellDark   = (Color){140, 40, 30, 255};   /* darker shell segments  */
    Color headColor   = (Color){70, 150, 70, 255};   /* green head/neck        */
    Color flipperColor= (Color){60, 130, 60, 255};   /* green flippers         */
    Color eyeColor    = BLACK;

    int turtleCount = 3;
    float turtleW = rect.width / turtleCount;
    bool facingRight = (speed >= 0);

    for (int t = 0; t < turtleCount; t++)
    {
        float cx = rect.x + turtleW * t + turtleW / 2.0f;
        float cy = rect.y + rect.height / 2.0f;
        float shellRx = turtleW / 2.0f - 2.0f;
        float shellRy = rect.height / 2.0f - 2.0f;

        /* shell */
        DrawEllipse((int)cx, (int)cy, shellRx, shellRy, shellColor);

        /* simple shell segment lines (3 short darker lines across the shell) */
        for (int s = -1; s <= 1; s++)
        {
            float lx = cx + s * (shellRx * 0.4f);
            DrawLine((int)lx, (int)(cy - shellRy * 0.6f), (int)lx, (int)(cy + shellRy * 0.6f), shellDark);
        }
        DrawEllipseLines((int)cx, (int)cy, shellRx, shellRy, shellDark);

        /* head - poking out on the side the turtle is "facing" */
        float headOffsetX = facingRight ? shellRx * 0.9f : -shellRx * 0.9f;
        float headCx = cx + headOffsetX;
        DrawCircle((int)headCx, (int)cy, shellRy * 0.45f, headColor);
        DrawCircle((int)(headCx + (facingRight ? 2 : -2)), (int)(cy - shellRy * 0.15f), 2, eyeColor);

        /* small flippers poking out top and bottom */
        DrawEllipse((int)(cx - shellRx * 0.3f), (int)(cy - shellRy * 0.85f), shellRx * 0.3f, shellRy * 0.35f, flipperColor);
        DrawEllipse((int)(cx - shellRx * 0.3f), (int)(cy + shellRy * 0.85f), shellRx * 0.3f, shellRy * 0.35f, flipperColor);
    }
}


/* ================================================================
   SECTION: LEVEL SETUP
   Base layouts for cars/logs - the level number scales their speed
   so the game gets harder without needing a whole new layout per
   level.
   ================================================================ */

void reset_level(Object cars[], Object logs[], SharkFin sharkFins[], bool goalFilled[], int level)
{
    float mult = 1.0f + (level - 1) * g_difficultyStep;

    /* Level-scaled spacing factors (clamped) */
    float vehicleSpacingMult = 1.0f - (level - 1) * g_vehicleSpacingScale;
    if (vehicleSpacingMult < g_vehicleMinSpacing) vehicleSpacingMult = g_vehicleMinSpacing;

    float logGapMult = 1.0f + (level - 1) * g_logSpacingScale;
    if (logGapMult > g_logMaxGap) logGapMult = g_logMaxGap;

    /* Cars: base positions scaled by vehicleSpacingMult to reduce gaps between cars in same lane */
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

    /* Logs: base positions with gaps scaled by logGapMult (fewer/sparser logs) */
    Object baseLogs[LOG_COUNT] =
    {
        {{40,  RIVER_TOP + 0 * LOG_ROW_STEP, 150, LOG_HEIGHT},  2, 0},
        {{(int)(450 * logGapMult), RIVER_TOP + 0 * LOG_ROW_STEP, 150, LOG_HEIGHT},  2, 0},

        {{120, RIVER_TOP + 1 * LOG_ROW_STEP, 150, LOG_HEIGHT}, -2, 0},
        {{(int)(520 * logGapMult), RIVER_TOP + 1 * LOG_ROW_STEP, 150, LOG_HEIGHT}, -2, 0},

        {{60,  RIVER_TOP + 2 * LOG_ROW_STEP, 150, LOG_HEIGHT},  3, 0},
        {{(int)(470 * logGapMult), RIVER_TOP + 2 * LOG_ROW_STEP, 150, LOG_HEIGHT},  3, 0},

        {{180, RIVER_TOP + 3 * LOG_ROW_STEP, 150, LOG_HEIGHT}, -2, 0},
        {{(int)(600 * logGapMult), RIVER_TOP + 3 * LOG_ROW_STEP, 150, LOG_HEIGHT}, -2, 0},

        {{20,  RIVER_TOP + 4 * LOG_ROW_STEP, 150, LOG_HEIGHT},  2, 0},
        {{(int)(430 * logGapMult), RIVER_TOP + 4 * LOG_ROW_STEP, 150, LOG_HEIGHT},  2, 0}
    };

    for (int i = 0; i < CAR_COUNT; i++)
    {
        cars[i] = baseCars[i];
        /* Scale x-positions of cars in same lane to reduce spacing */
        if (i % 3 == 1) cars[i].rect.x = (int)(baseCars[i].rect.x * vehicleSpacingMult);
        else if (i % 3 == 2) cars[i].rect.x = (int)(baseCars[i].rect.x * vehicleSpacingMult);
        cars[i].speed *= mult;
    }

    for (int i = 0; i < LOG_COUNT; i++)
    {
        logs[i] = baseLogs[i];
        logs[i].speed *= mult;
    }

    /* Shark fins in goal-row water gaps: 3 fins per gap (12 total), centered in each of 4 gaps */
    float bayWidth = (float)SCREEN_WIDTH / GOAL_SLOT_COUNT;
    int bayTop = 25;
    int bayHeight = GOAL_HEIGHT - 25;  /* 95px */
    int centerY = bayTop + bayHeight / 2;
    const int FINS_PER_GAP = 6;
    const int GAP_COUNT = 4;

    for (int gap = 0; gap < GAP_COUNT; gap++)
    {
        int barrierX = (int)(bayWidth * (gap + 1));
        int centerX = barrierX;

        for (int f = 0; f < FINS_PER_GAP; f++)
        {
            int finIndex = gap * FINS_PER_GAP + f;

            /* Spread 6 fins vertically within the gap, with slight horizontal stagger */
            float vOffset = (f - 2.5f) * 14.0f;  /* -35, -21, -7, +7, +21, +35 pixels */
            float hOffset = (f % 2 == 0 ? -1 : 1) * 8.0f;  /* alternate left/right */

            sharkFins[finIndex].rect = (Rectangle){
                centerX - SHARK_FIN_WIDTH / 2 + (int)hOffset,
                centerY - SHARK_FIN_HEIGHT / 2 + (int)vOffset,
                SHARK_FIN_WIDTH,
                SHARK_FIN_HEIGHT
            };
            /* Static obstacles - no movement */
            sharkFins[finIndex].speed = 0.0f;
            sharkFins[finIndex].phaseOffset = finIndex * 0.5f;
        }
    }

    /* 6 fins on left edge (left of leftmost goal slot) */
    const int EDGE_FINS = 6;
    int leftSlotLeft = 100 - 84/2;  /* 58 - left edge of first goal slot */
    int leftEdgeX = leftSlotLeft - 24;  /* 24px left of slot edge */
    for (int f = 0; f < EDGE_FINS; f++)
    {
        int finIndex = GAP_COUNT * FINS_PER_GAP + f;
        float vOffset = (f - 2.5f) * 14.0f;  /* -35, -21, -7, +7, +21, +35 */
        float hOffset = (f % 2 == 0 ? -1 : 1) * 8.0f;  /* alternate left/right, same as gap fins */

        sharkFins[finIndex].rect = (Rectangle){
            leftEdgeX - SHARK_FIN_WIDTH / 2 + (int)hOffset,
            centerY - SHARK_FIN_HEIGHT / 2 + (int)vOffset,
            SHARK_FIN_WIDTH,
            SHARK_FIN_HEIGHT
        };
        sharkFins[finIndex].speed = 0.0f;
        sharkFins[finIndex].phaseOffset = finIndex * 0.5f;
    }

    /* 6 fins on right edge (right of rightmost goal slot) */
    int rightSlotRight = 900 + 84/2;  /* 942 - right edge of last goal slot */
    int rightEdgeX = rightSlotRight + 24;  /* 24px right of slot edge */
    for (int f = 0; f < EDGE_FINS; f++)
    {
        int finIndex = GAP_COUNT * FINS_PER_GAP + EDGE_FINS + f;
        float vOffset = (f - 2.5f) * 14.0f;  /* -35, -21, -7, +7, +21, +35 */
        float hOffset = (f % 2 == 0 ? -1 : 1) * 8.0f;  /* alternate left/right, same as gap fins */

        sharkFins[finIndex].rect = (Rectangle){
            rightEdgeX - SHARK_FIN_WIDTH / 2 + (int)hOffset,
            centerY - SHARK_FIN_HEIGHT / 2 + (int)vOffset,
            SHARK_FIN_WIDTH,
            SHARK_FIN_HEIGHT
        };
        sharkFins[finIndex].speed = 0.0f;
        sharkFins[finIndex].phaseOffset = finIndex * 0.5f;
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

/* Goal area: 5 home bays floating on water with gaps between them */
void draw_goal_area(bool goalFilled[], SharkFin sharkFins[], Texture2D frogTex, int level, int score)
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

    /* Water colors (matching river section) */
    Color waterColor = (Color){35, 130, 220, 255};
    Color waveColor = (Color){140, 195, 240, 130};

    /* Bay dimensions */
    int bayCount = GOAL_SLOT_COUNT;
    int bayTop = 25;
    int bayHeight = GOAL_HEIGHT - 25;  /* 95px */

    float time = (float)GetTime();

    /* First, draw water everywhere in the goal area below the top bar */
    DrawRectangle(0, bayTop, SCREEN_WIDTH, bayHeight, waterColor);

    /* Animated wave lines across the entire goal area (including gaps and under bays) */
    for (int i = 0; i < 5; i++)
    {
        float waveY = bayTop + 16 + i * 22;
        float offset = sinf(time * 1.6f + i * 1.3f) * 10.0f;
        DrawLineEx(
            (Vector2){0 + offset, waveY},
            (Vector2){SCREEN_WIDTH + offset, waveY},
            2.0f,
            waveColor
        );
    }

    /* Goal slots (floating platforms on water) - smaller and square */
    float roundness = 0.2f;  /* moderate corner radius */
    int shadowOffset = 3;
    int slotSize = 84;  /* square size in pixels */

    for (int i = 0; i < bayCount; i++)
    {
        int bx = (int)(bayWidth * i);
        int bw = (int)bayWidth;

        /* Slot centered in bay */
        int slotLeft = bx + bw/2 - slotSize/2;
        int slotTop = bayTop + bayHeight/2 - slotSize/2;
        int slotWidth = slotSize;
        int slotHeight = slotSize;

        /* Drop shadow beneath slot (draw first, so it's behind the slot) */
        DrawRectangleRounded(
            (Rectangle){slotLeft + shadowOffset, slotTop + shadowOffset, (float)slotWidth, (float)slotHeight},
            roundness, 8,
            Fade((Color){0, 0, 0, 255}, 0.25f)
        );

        /* Thin dark-blue water ring/outline beneath slot to sell the "floating" look */
        DrawRectangleRoundedLines(
            (Rectangle){slotLeft - 2, slotTop - 2, (float)slotWidth + 4, (float)slotHeight + 4},
            roundness, 8,
            (Color){20, 80, 160, 255}
        );

        /* Bay background - claimed bays show light green, empty show water decoration */
        Color bayBg = goalFilled[i] ? (Color){100, 200, 48, 255} : (Color){40, 150, 50, 255};
        DrawRectangleRounded(
            (Rectangle){slotLeft, slotTop, (float)slotWidth, (float)slotHeight},
            roundness, 8,
            bayBg
        );

        /* Bay border (rounded) */
        Color borderColor = goalFilled[i] ? (Color){180, 150, 50, 255} : (Color){20, 90, 30, 255};
        DrawRectangleRoundedLines(
            (Rectangle){slotLeft, slotTop, (float)slotWidth, (float)slotHeight},
            roundness, 8,
            borderColor
        );

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

    /* Draw shark fins in water gaps (on top of water, under/over slots) */
    Color finColor = (Color){60, 70, 85, 255};      /* dark blue-gray */
    Color finEdgeColor = (Color){40, 50, 65, 255};  /* darker edge */
    Color wakeColor = (Color){180, 200, 220, 180};  /* subtle white wake */

    for (int i = 0; i < SHARK_FIN_COUNT; i++)
    {
        Rectangle r = sharkFins[i].rect;
        bool movingRight = sharkFins[i].speed > 0;

        /* Fin triangle: pointed top, wider base at water surface */
        int finTipX = (int)(r.x + r.width / 2);
        int finTipY = (int)(r.y);
        int finBaseY = (int)(r.y + r.height);
        int halfBase = (int)(r.width / 2);

        /* Slight curve on leading edge: leading point slightly forward */
        int leadOffset = movingRight ? 3 : -3;

        /* Main fin triangle */
        DrawTriangle(
            (Vector2){finTipX + leadOffset, finTipY},
            (Vector2){finTipX - halfBase, finBaseY},
            (Vector2){finTipX + halfBase, finBaseY},
            finColor
        );

        /* Darker edge line along leading edge for depth */
        DrawLineEx(
            (Vector2){finTipX + leadOffset, finTipY},
            (Vector2){finTipX - halfBase, finBaseY},
            2.0f,
            finEdgeColor
        );

        /* Trailing wake: thin curved line behind fin (scaled to smaller fin) */
        int wakeLen = 8;
        int wakeStartX = finTipX + (movingRight ? -halfBase : halfBase);
        int wakeStartY = finBaseY - 2;
        for (int w = 0; w < 2; w++)
        {
            int wx = wakeStartX + (movingRight ? -w * 3 : w * 3);
            int wy = wakeStartY + w * 2;
            DrawLineEx(
                (Vector2){wx, wy},
                (Vector2){wx + (movingRight ? -2 : 2), wy + 1},
                1.0f,
                Fade(wakeColor, 0.5f - w * 0.2f)
            );
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

/* Clickable button rectangles for the main menu, laid out once here so
   both the input-handling code (to detect clicks) and draw_menu() (to
   render them) always agree on exactly where each button is. */
typedef struct MenuButtons
{
    Rectangle start;
    Rectangle leaderboard;
    Rectangle howToPlay;
    Rectangle credits;
    Rectangle difficulty;
    Rectangle music;
} MenuButtons;

MenuButtons compute_menu_buttons(void)
{
    MenuButtons b;

    int centerX = SCREEN_WIDTH / 2;
    int y = 195;

    int startW = 320, startH = 54;
    b.start = (Rectangle){centerX - startW / 2, y, startW, startH};
    y += startH + 14;

    int smallW = 260, smallH = 42;
    b.leaderboard = (Rectangle){centerX - smallW / 2, y, smallW, smallH};
    y += smallH + 10;

    b.howToPlay = (Rectangle){centerX - smallW / 2, y, smallW, smallH};
    y += smallH + 10;

    b.credits = (Rectangle){centerX - smallW / 2, y, smallW, smallH};
    y += smallH + 10;

    b.difficulty = (Rectangle){centerX - smallW / 2, y, smallW, smallH};
    y += smallH + 10;

    b.music = (Rectangle){centerX - smallW / 2, y, smallW, smallH};

    return b;
}

/* Draws one menu button: rounded background (lighter when hovered),
   centered label text. Purely visual - click detection uses the same
   rectangle separately in the input-handling code via
   compute_menu_buttons(), so the two always stay in sync. */
void draw_menu_button(Rectangle rect, const char *label, Color baseColor, Color textColor, int fontSize)
{
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, rect);

    Color fill = hovered ? Fade(baseColor, 0.9f) : Fade(baseColor, 0.65f);
    DrawRectangleRounded(rect, 0.25f, 8, fill);
    DrawRectangleRoundedLines(rect, 0.25f, 8, hovered ? WHITE : Fade(WHITE, 0.4f));

    int textW = MeasureText(label, fontSize);
    int textX = (int)(rect.x + (rect.width - textW) / 2);
    int textY = (int)(rect.y + (rect.height - fontSize) / 2);
    DrawText(label, textX, textY, fontSize, textColor);
}

/* Shared "Back" button used by the Leaderboard / How-to-Play / Credits
   overlay screens - same rectangle used for both drawing and click
   detection, same pattern as the main menu buttons above. */
Rectangle compute_back_button(void)
{
    int w = 220, h = 40;
    return (Rectangle){SCREEN_WIDTH / 2 - w / 2, SCREEN_HEIGHT - h - 20, w, h};
}

void draw_back_button(void)
{
    draw_menu_button(compute_back_button(), "BACK  (ENTER / ESC)", (Color){40, 70, 130, 255}, RAYWHITE, 18);
}

/* Name-entry screen's Start/Cancel buttons, laid out side by side. */
Rectangle compute_name_confirm_button(void)
{
    int w = 200, h = 44;
    return (Rectangle){SCREEN_WIDTH / 2 - w - 10, 330, w, h};
}

Rectangle compute_name_cancel_button(void)
{
    int w = 200, h = 44;
    return (Rectangle){SCREEN_WIDTH / 2 + 10, 330, w, h};
}

/* Name-entry screen - lets the player type a custom name before playing.
   Typed characters build up in nameEditBuffer (owned by main()); this
   function only draws the current state of that buffer plus a blinking
   cursor and Start/Cancel buttons. Reached by pressing/clicking Start
   Game on the main menu; ENTER or clicking Start Game here commits
   nameEditBuffer to playerName AND begins the actual game. ESC or
   clicking Back to Menu discards the edit and returns to the menu. */
void draw_name_entry(const char *nameEditBuffer)
{
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){15, 25, 45, 255});

    const char *title = "ENTER YOUR NAME";
    int titleSize = 34;
    DrawText(title, SCREEN_WIDTH / 2 - MeasureText(title, titleSize) / 2, 120, titleSize, (Color){140, 220, 255, 255});

    /* Text entry box */
    int boxW = 360, boxH = 54;
    int boxX = SCREEN_WIDTH / 2 - boxW / 2;
    int boxY = 210;
    DrawRectangleRounded((Rectangle){boxX, boxY, boxW, boxH}, 0.2f, 8, (Color){30, 30, 40, 255});
    DrawRectangleRoundedLines((Rectangle){boxX, boxY, boxW, boxH}, 0.2f, 8, (Color){140, 220, 255, 255});

    int textSize = 28;
    int textW = MeasureText(nameEditBuffer, textSize);
    int textX = boxX + boxW / 2 - textW / 2;
    int textY = boxY + boxH / 2 - textSize / 2;
    DrawText(nameEditBuffer, textX, textY, textSize, RAYWHITE);

    /* blinking cursor right after the typed text */
    if (fmodf((float)GetTime(), 1.0f) < 0.5f)
    {
        DrawRectangle(textX + textW + 3, textY, 3, textSize, RAYWHITE);
    }

    const char *hint = "Letters, numbers and spaces - up to 14 characters";
    DrawText(hint, SCREEN_WIDTH / 2 - MeasureText(hint, 15) / 2, boxY + boxH + 14, 15, (Color){170, 170, 170, 255});

    draw_menu_button(compute_name_confirm_button(), "START GAME  (ENTER)", (Color){40, 160, 60, 255}, YELLOW, 17);
    draw_menu_button(compute_name_cancel_button(), "BACK TO MENU  (ESC)", (Color){140, 40, 40, 255}, RAYWHITE, 17);
}

void draw_menu(Texture2D startBgTex, bool musicMuted)
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

    /* MAIN MENU BUTTONS - clickable (mouse) and still keyboard-shortcut-able;
       compute_menu_buttons() is the single source of truth for where each
       button sits, shared with the click-detection code in main(). */
    {
        MenuButtons btn = compute_menu_buttons();

        /* Pulsing highlight for the primary Start button so it still draws the eye */
        float pulse = 0.75f + 0.25f * (sinf(GetTime() * 4.2f) * 0.5f + 0.5f);
        draw_menu_button(btn.start, "START GAME  (ENTER)", Fade((Color){40, 160, 60, 255}, pulse), YELLOW, 26);

        draw_menu_button(btn.leaderboard, "LEADERBOARD  (L)", (Color){40, 70, 130, 255}, RAYWHITE, 19);
        draw_menu_button(btn.howToPlay, "HOW TO PLAY  (H)", (Color){40, 70, 130, 255}, RAYWHITE, 19);
        draw_menu_button(btn.credits, "CREDITS  (C)", (Color){40, 70, 130, 255}, RAYWHITE, 19);

        const char *diffText = TextFormat("DIFFICULTY: %s  (< / >)", difficulty_name(currentDifficulty));
        Color diffColor = (currentDifficulty == DIFF_EASY) ? (Color){120, 220, 120, 255}
                         : (currentDifficulty == DIFF_HARD) ? (Color){230, 90, 90, 255}
                         : (Color){255, 210, 90, 255};
        draw_menu_button(btn.difficulty, diffText, (Color){90, 60, 120, 255}, diffColor, 18);

        const char *musicText = musicMuted ? "MUSIC: OFF  (M)" : "MUSIC: ON  (M)";
        Color musicColor = musicMuted ? (Color){170, 170, 170, 255} : (Color){140, 220, 255, 255};
        draw_menu_button(btn.music, musicText, (Color){90, 60, 120, 255}, musicColor, 18);
    }
}

/* Top-scorers screen - lists the best runs saved to leaderboard.txt,
   highest score first. Reached from the main menu by pressing L. */
void draw_leaderboard(void)
{
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){10, 40, 20, 255});

    const char *title = "TOP SCORERS";
    int titleSize = 40;
    DrawText(title, SCREEN_WIDTH / 2 - MeasureText(title, titleSize) / 2, 40, titleSize, GOLD);

    LeaderboardEntry entries[LEADERBOARD_MAX];
    int count = load_leaderboard(entries);

    int rowY = 120;
    int rowHeight = 34;

    if (count == 0)
    {
        const char *empty = "No scores yet - go set one!";
        DrawText(empty, SCREEN_WIDTH / 2 - MeasureText(empty, 22) / 2, rowY, 22, LIGHTGRAY);
    }
    else
    {
        for (int i = 0; i < count; i++)
        {
            Color rowColor = (i == 0) ? GOLD : (i == 1) ? (Color){200, 200, 210, 255} : (i == 2) ? (Color){205, 140, 80, 255} : RAYWHITE;

            const char *rankStr = TextFormat("%2d.", i + 1);
            const char *nameStr = entries[i].name;
            const char *scoreStr = TextFormat("%d", entries[i].score);

            int fontSize = 24;
            int leftColX = SCREEN_WIDTH / 2 - 160;
            int nameColX = SCREEN_WIDTH / 2 - 100;
            int scoreColX = SCREEN_WIDTH / 2 + 100;

            DrawText(rankStr, leftColX, rowY + i * rowHeight, fontSize, rowColor);
            DrawText(nameStr, nameColX, rowY + i * rowHeight, fontSize, rowColor);
            DrawText(scoreStr, scoreColX - MeasureText(scoreStr, fontSize), rowY + i * rowHeight, fontSize, rowColor);
        }
    }

    draw_back_button();
}

/* How-to-play screen - explains controls and rules. Reached from the
   main menu by pressing H. */
void draw_how_to_play(void)
{
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){10, 30, 50, 255});

    const char *title = "HOW TO PLAY";
    int titleSize = 38;
    DrawText(title, SCREEN_WIDTH / 2 - MeasureText(title, titleSize) / 2, 30, titleSize, (Color){120, 200, 255, 255});

    int fontSize = 20;
    int lineHeight = 30;
    int y = 100;
    int x = 60;

    const char *lines[] = {
        "GOAL: ",
        "Guide your frog safely into all 5 empty goal slots.",
        "at the top of the screen to clear the level.",
        "",
        "CONTROLS:",
        "  Arrow Keys or WASD  -  Hop up / down / left / right",
        "  P                   -  Pause the game",
        "",
        "TIMER: ",
        "Each attempt has a countdown shown top-right and as",
        "a bar in the goal row - reach a goal before it runs out!",
        "",
        "LIVES & SCORING: ",
        "You start with a set number of lives",
        "(fewer on Hard, more on Easy). Reaching a goal scores points.",
        "running out of lives ends the game."
    };
    int lineCount = sizeof(lines) / sizeof(lines[0]);

    for (int i = 0; i < lineCount; i++)
    {
        Color lineColor = RAYWHITE;
        if (TextIsEqual(lines[i], "CONTROLS:") || TextIsEqual(lines[i], "OBSTACLES:") ||
            strncmp(lines[i], "GOAL:", 5) == 0 ||
            strncmp(lines[i], "TIMER:", 6) == 0 ||
            strncmp(lines[i], "LIVES & SCORING:", 16) == 0)
        {
            lineColor = (Color){255, 210, 90, 255};
        }
        DrawText(lines[i], x, y + i * lineHeight, fontSize, lineColor);
    }

    draw_back_button();
}

/* Credits screen - attribution for external resources used in the
   project. Reached from the main menu by pressing C. */
void draw_credits(void)
{
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){20, 20, 30, 255});

    const char *title = "CREDITS";
    int titleSize = 38;
    DrawText(title, SCREEN_WIDTH / 2 - MeasureText(title, titleSize) / 2, 30, titleSize, (Color){220, 190, 255, 255});

    int fontSize = 19;
    int lineHeight = 28;
    int y = 110;
    int x = 60;

    /* NOTE: fill in the actual source/author/license for each asset you
       used below - this list only has placeholders for whatever you
       downloaded (background art, car/turtle sprites, sound effects). */
    const char *lines[] = {
        "GAME DESIGN & PROGRAMMING",
        " Md. Zihan Ahammed & MAshfia Bint Matin",
        "",
        "SPRITES",
        "  Vehicle sprites (car, sports car, truck) - generated programmatically with Python",
        "",
        "SOUND EFFECTS",
        "  Jump, crash, splash, goal, level up, game over - freesound.org / Kenney.nl",
        "  Background Music - Three Initials Left (from album Infinite Credits)",
        "",
        "BUILT WITH",
        "  raylib (https://www.raylib.com)",
    };
    int lineCount = sizeof(lines) / sizeof(lines[0]);

    for (int i = 0; i < lineCount; i++)
    {
        Color lineColor = RAYWHITE;
        if (TextIsEqual(lines[i], "GAME DESIGN & PROGRAMMING") ||
            TextIsEqual(lines[i], "BACKGROUND ART") ||
            TextIsEqual(lines[i], "SPRITES") ||
            TextIsEqual(lines[i], "SOUND EFFECTS") ||
            TextIsEqual(lines[i], "BUILT WITH"))
        {
            lineColor = (Color){255, 210, 90, 255};
        }
        DrawText(lines[i], x, y + i * lineHeight, fontSize, lineColor);
    }

    draw_back_button();
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

    const char *next = "Press ENTER to start next LEVEL";
    DrawText(next, SCREEN_WIDTH / 2 - MeasureText(next, 22) / 2, SCREEN_HEIGHT / 2 + 30, 22, (Color){255, 230, 120, 255});
}

void draw_game_over_overlay(int score)
{
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.7f));

    const char *gameOverText = "khela parena";
    DrawText(gameOverText, GetScreenWidth() / 2 - MeasureText(gameOverText, 50) / 2, 210, 50, RED);

    const char *finalScoreText = TextFormat("Final Score: %d", score);
    DrawText(finalScoreText, GetScreenWidth() / 2 - MeasureText(finalScoreText, 25) / 2, 280, 25, (Color){255, 230, 120, 255});

    const char *restartText = "Press R to Restart";
    DrawText(restartText, GetScreenWidth() / 2 - MeasureText(restartText, 25) / 2, 400, 25, (Color){255, 230, 120, 255});
}

/* Score/level/timer text plus the heart-based lives display */
void draw_hud(int lives, float timeRemaining, bool musicMuted)
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

    /* TOP-RIGHT: mini time bar (30s max), with a small music mute icon just left of it */
    float maxTime = g_baseTimerDuration;
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

    /* Music mute indicator - a small note icon, dimmed/crossed when muted */
    int noteX = barX - 26;
    int noteY = 12;
    Color noteColor = musicMuted ? (Color){120, 120, 120, 255} : (Color){255, 230, 120, 255};
    DrawCircle(noteX, noteY + 5, 4, noteColor);
    DrawRectangle(noteX + 3, noteY - 6, 2, 11, noteColor);
    DrawRectangle(noteX + 3, noteY - 6, 7, 3, noteColor);
    if (musicMuted)
    {
        DrawLine(noteX - 6, noteY - 8, noteX + 10, noteY + 10, RED);
    }
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

    apply_difficulty(currentDifficulty);  /* set up MEDIUM defaults before anything reads g_* values */

    /* ------------- load assets ------------- */

    TraceLog(LOG_INFO, "Working directory: %s", GetWorkingDirectory());

    Texture2D frogTex = LoadTexture("assets/images/frog.png");
    carTexture = LoadTexture("assets/images/car.png");
    carSportsTexture = LoadTexture("assets/images/car_sports.png");
    carTruckTexture = LoadTexture("assets/images/car_truck.png");
    Texture2D logTex  = LoadTexture("assets/images/log.png");
    Texture2D startBgTex = LoadTexture("assets/start_bg.png");
    turtleTexture = LoadTexture("assets/images/turtle.png");

    carTextureValid = (carTexture.id != 0);
    carSportsTextureValid = (carSportsTexture.id != 0);
    carTruckTextureValid = (carTruckTexture.id != 0);
    turtleTextureValid = (turtleTexture.id != 0);

    if (frogTex.id == 0 || logTex.id == 0)
    {
        TraceLog(LOG_ERROR, "Failed to load texture assets");
        CloseWindow();
        return 1;
    }

    if (!carTextureValid) TraceLog(LOG_WARNING, "Failed to load assets/images/car.png, using fallback");
    if (!carSportsTextureValid) TraceLog(LOG_WARNING, "Failed to load assets/images/car_sports.png, using fallback");
    if (!carTruckTextureValid) TraceLog(LOG_WARNING, "Failed to load assets/images/car_truck.png, using fallback");
    if (!turtleTextureValid) TraceLog(LOG_WARNING, "Failed to load assets/images/turtle.png, using drawn fallback");

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

    /* Background music - loaded fully into memory (not streamed from disk)
       so a slow/stalled disk read (antivirus scan, cloud-sync placeholder
       file, etc.) can never freeze the running game the way a Music
       stream's per-frame disk reads could. Looped manually by restarting
       it whenever it finishes. */
    Sound bgMusic = LoadSound("assets/sounds/bgm.WAV");
    bool bgMusicValid = (bgMusic.frameCount > 0);
    bool musicMuted = false;

    if (bgMusicValid)
    {
        SetSoundVolume(bgMusic, 0.5f);
        PlaySound(bgMusic);
    }
    else
    {
        TraceLog(LOG_WARNING, "Failed to load assets/sounds/bgm.WAV, continuing without music");
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

    char nameEditBuffer[32];
    TextCopy(nameEditBuffer, playerName);

    Vector2 frog;
    reset_frog(&frog);

    Object cars[CAR_COUNT];
    Object logs[LOG_COUNT];
    SharkFin sharkFins[SHARK_FIN_COUNT];
    bool goalFilled[GOAL_SLOT_COUNT];

    int score = 0;
    int lives = g_startingLives;
    int level = 1;
    float timeRemaining = g_baseTimerDuration * compute_timer_mult(level); /* 30-second countdown timer */

    reset_level(cars, logs, sharkFins, goalFilled, level);

    float levelCompleteTimer = 0.0f;
    bool gameOverSoundPlayed = false;

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        float time = (float)GetTime();

        /* ------------- music (plays/loops regardless of screen) ------------- */

        if (bgMusicValid && !musicMuted && !IsSoundPlaying(bgMusic))
        {
            /* fully-loaded Sound has no per-frame disk I/O, so this is a
               cheap check - just replay it the instant it finishes, giving
               a manual loop with no streaming stalls possible. */
            PlaySound(bgMusic);
        }

        if (IsKeyPressed(KEY_M))
        {
            musicMuted = !musicMuted;
            if (bgMusicValid)
            {
                if (musicMuted)
                {
                    StopSound(bgMusic);
                }
                else
                {
                    PlaySound(bgMusic);
                }
            }
        }

        /* ------------- input / state transitions ------------- */

        if (state == STATE_MENU)
        {
            Vector2 mousePos = GetMousePosition();
            bool mouseClicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
            MenuButtons menuBtn = compute_menu_buttons();

            bool clickStart       = mouseClicked && CheckCollisionPointRec(mousePos, menuBtn.start);
            bool clickLeaderboard = mouseClicked && CheckCollisionPointRec(mousePos, menuBtn.leaderboard);
            bool clickHowToPlay   = mouseClicked && CheckCollisionPointRec(mousePos, menuBtn.howToPlay);
            bool clickCredits     = mouseClicked && CheckCollisionPointRec(mousePos, menuBtn.credits);
            bool clickDifficulty  = mouseClicked && CheckCollisionPointRec(mousePos, menuBtn.difficulty);
            bool clickMusic       = mouseClicked && CheckCollisionPointRec(mousePos, menuBtn.music);

            if (IsKeyPressed(KEY_ENTER) || clickStart)
            {
                /* Start Game no longer jumps straight into play - it first
                   asks for the player's name, pre-filled with whatever
                   name is currently set. The actual game-start logic now
                   lives in the STATE_NAME_ENTRY confirm handler below. */
                TextCopy(nameEditBuffer, playerName);
                state = STATE_NAME_ENTRY;
            }
            else if (IsKeyPressed(KEY_L) || clickLeaderboard)
            {
                state = STATE_LEADERBOARD;
            }
            else if (IsKeyPressed(KEY_H) || clickHowToPlay)
            {
                state = STATE_HOW_TO_PLAY;
            }
            else if (IsKeyPressed(KEY_C) || clickCredits)
            {
                state = STATE_CREDITS;
            }
            else if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A))
            {
                if (currentDifficulty == DIFF_EASY) currentDifficulty = DIFF_HARD;
                else if (currentDifficulty == DIFF_MEDIUM) currentDifficulty = DIFF_EASY;
                else currentDifficulty = DIFF_MEDIUM;
                apply_difficulty(currentDifficulty);
            }
            else if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D) || clickDifficulty)
            {
                /* clicking the difficulty button cycles forward, same as RIGHT/D */
                if (currentDifficulty == DIFF_EASY) currentDifficulty = DIFF_MEDIUM;
                else if (currentDifficulty == DIFF_MEDIUM) currentDifficulty = DIFF_HARD;
                else currentDifficulty = DIFF_EASY;
                apply_difficulty(currentDifficulty);
            }
            else if (clickMusic)
            {
                /* same toggle as pressing M */
                musicMuted = !musicMuted;
                if (bgMusicValid)
                {
                    if (musicMuted) StopSound(bgMusic);
                    else PlaySound(bgMusic);
                }
            }
        }
        else if (state == STATE_NAME_ENTRY)
        {
            /* Capture typed characters - letters, numbers, spaces only,
               capped at 14 visible characters so it fits neatly above the
               frog and in the leaderboard columns. */
            int ch = GetCharPressed();
            while (ch > 0)
            {
                bool allowed = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                               (ch >= '0' && ch <= '9') || (ch == ' ');
                int len = TextLength(nameEditBuffer);
                if (allowed && len < 14)
                {
                    nameEditBuffer[len] = (char)ch;
                    nameEditBuffer[len + 1] = '\0';
                }
                ch = GetCharPressed();
            }

            if (IsKeyPressed(KEY_BACKSPACE))
            {
                int len = TextLength(nameEditBuffer);
                if (len > 0)
                {
                    nameEditBuffer[len - 1] = '\0';
                }
            }

            bool mouseClicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
            Vector2 mousePos = GetMousePosition();
            bool clickConfirm = mouseClicked && CheckCollisionPointRec(mousePos, compute_name_confirm_button());
            bool clickCancel  = mouseClicked && CheckCollisionPointRec(mousePos, compute_name_cancel_button());

            if (IsKeyPressed(KEY_ENTER) || clickConfirm)
            {
                /* trim leading/trailing spaces; keep the old name if the
                   result is empty so the player can never end up with a
                   blank name */
                int start = 0;
                while (nameEditBuffer[start] == ' ') start++;
                int end = TextLength(nameEditBuffer) - 1;
                while (end >= start && nameEditBuffer[end] == ' ') end--;

                if (end >= start)
                {
                    int trimmedLen = end - start + 1;
                    for (int i = 0; i < trimmedLen; i++)
                    {
                        playerName[i] = nameEditBuffer[start + i];
                    }
                    playerName[trimmedLen] = '\0';
                }

                /* name is set - now actually start the game */
                score = 0;
                lives = g_startingLives;
                level = 1;
                timeRemaining = g_baseTimerDuration * compute_timer_mult(level);
                gameOverSoundPlayed = false;
                reset_frog(&frog);
                reset_level(cars, logs, sharkFins, goalFilled, level);
                state = STATE_PLAYING;
            }
            else if (IsKeyPressed(KEY_ESCAPE) || clickCancel)
            {
                /* discard edits, keep whatever playerName already was, and
                   go back to the menu (not into the game) */
                state = STATE_MENU;
            }
        }
        else if (state == STATE_LEADERBOARD)
        {
            bool clickBack = IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
                              CheckCollisionPointRec(GetMousePosition(), compute_back_button());
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE) || clickBack)
            {
                state = STATE_MENU;
            }
        }
        else if (state == STATE_HOW_TO_PLAY)
        {
            bool clickBack = IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
                              CheckCollisionPointRec(GetMousePosition(), compute_back_button());
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE) || clickBack)
            {
                state = STATE_MENU;
            }
        }
        else if (state == STATE_CREDITS)
        {
            bool clickBack = IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
                              CheckCollisionPointRec(GetMousePosition(), compute_back_button());
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE) || clickBack)
            {
                state = STATE_MENU;
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
                timeRemaining = g_baseTimerDuration * compute_timer_mult(level);
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
                    timeRemaining = g_baseTimerDuration * compute_timer_mult(level);
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
                    timeRemaining = g_baseTimerDuration * compute_timer_mult(level);
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

                /* Check shark fins in water gaps (between bays) */
                bool onSharkFin = false;
                if (!inBay)
                {
                    for (int i = 0; i < SHARK_FIN_COUNT; i++)
                    {
                        if (CheckCollisionRecs(frogRect, sharkFins[i].rect))
                        {
                            onSharkFin = true;
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
                        timeRemaining = g_baseTimerDuration * compute_timer_mult(level);
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
                else if (onSharkFin)
                {
                    /* Shark fin - always dangerous */
                    lives--;
                    timeRemaining = g_baseTimerDuration * compute_timer_mult(level);
                    if (sndSplash.frameCount > 0) PlaySound(sndSplash);
                    reset_frog(&frog);
                }
                else if (inBarrier || frogCenterY < bayTop)
                {
                    /* Hit barrier or upper decorative area - lose life */
                    lives--;
                    timeRemaining = g_baseTimerDuration * compute_timer_mult(level);
                    if (sndSplash.frameCount > 0) PlaySound(sndSplash);
                    reset_frog(&frog);
                }
                /* If in goal area but not in bay, alligator, or barrier (shouldn't happen), treat as barrier */
                else
                {
                    lives--;
                    timeRemaining = g_baseTimerDuration * compute_timer_mult(level);
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
            if (IsKeyPressed(KEY_ENTER))
            {
                level++;
                timeRemaining = g_baseTimerDuration * compute_timer_mult(level);
                reset_frog(&frog);
                reset_level(cars, logs, sharkFins, goalFilled, level);
                state = STATE_PLAYING;
            }
        }
        else if (state == STATE_GAME_OVER)
        {
            if (!gameOverSoundPlayed)
            {
                if (sndGameOver.frameCount > 0) PlaySound(sndGameOver);
                gameOverSoundPlayed = true;

                add_leaderboard_entry(score);
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
            draw_menu(startBgTex, musicMuted);
        }
        else if (state == STATE_LEADERBOARD)
        {
            draw_leaderboard();
        }
        else if (state == STATE_HOW_TO_PLAY)
        {
            draw_how_to_play();
        }
        else if (state == STATE_CREDITS)
        {
            draw_credits();
        }
        else if (state == STATE_NAME_ENTRY)
        {
            draw_name_entry(nameEditBuffer);
        }
        else
        {
            ClearBackground(RAYWHITE);

            /* scenery, back to front */
            draw_goal_area(goalFilled, sharkFins, frogTex, level, score);
            draw_river(time);
            draw_grass_band(MEDIAN_TOP, MEDIAN_HEIGHT);  /* Grass strip between river and road */
            draw_road();
            draw_grass_band(BOTTOM_TOP, BOTTOM_HEIGHT);  /* Grass strip below road */

            /* lily pad slots are now drawn inside draw_goal_area() */

            /* logs (under the frog, in the river band) - indices 2,3,6,7
               (river rows 1 and 3) are drawn as turtle groups instead of
               logs, giving 3 log rows + 2 turtle rows, alternating. The
               underlying rect/speed/collision behaviour is identical to
               a log either way - only the visual changes. */
            for (int i = 0; i < LOG_COUNT; i++)
            {
                bool isTurtleRow = (i == 2 || i == 3 || i == 6 || i == 7);
                if (isTurtleRow)
                {
                    draw_turtle_group(logs[i].rect, logs[i].speed);
                }
                else
                {
                    draw_log(logs[i], logTex);
                }
            }

            /* cars (in the road band) */
            for (int i = 0; i < CAR_COUNT; i++)
            {
                draw_car(cars[i], laneColors[i]);
            }

            /* frog on top of everything */
            draw_frog(frog, frogTex);

            /* HUD */
            draw_hud(lives, timeRemaining, musicMuted);

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
                draw_game_over_overlay(score);
            }
        }

        EndDrawing();
    }

    UnloadTexture(frogTex);
    if (carTextureValid) UnloadTexture(carTexture);
    if (carSportsTextureValid) UnloadTexture(carSportsTexture);
    if (carTruckTextureValid) UnloadTexture(carTruckTexture);
    if (turtleTextureValid) UnloadTexture(turtleTexture);
    UnloadTexture(logTex);
    UnloadTexture(startBgTex);

    UnloadSound(sndJump);
    UnloadSound(sndCrash);
    UnloadSound(sndSplash);
    UnloadSound(sndGoal);
    UnloadSound(sndLevelUp);
    UnloadSound(sndGameOver);

    if (bgMusicValid) UnloadSound(bgMusic);

    CloseAudioDevice();
    CloseWindow();

    return 0;
}