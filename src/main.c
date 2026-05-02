#include "raylib.h"
#include "raymath.h"
#define RAYLIB_LIBRETRO_IMPLEMENTATION
#include "raylib-libretro.h"
#include <string.h>
#include <stdio.h>

#define MAX_SYSTEMS     16
#define MAX_ROMS        512
#define WINDOW_HALF     8

#define REPEAT_DELAY    0.4f
#define REPEAT_RATE     0.08f

#if defined(_WIN32)
#define CORE_EXT ".dll"
#elif defined(__APPLE__)
#define CORE_EXT ".dylib"
#else
#define CORE_EXT ".so"
#endif

typedef enum { LEVEL_SYSTEM, LEVEL_ROM, LEVEL_PLAYING } AppLevel;

typedef struct {
    char label[128];
    char romPath[256];
    char posterPath[256];
} RomEntry;

typedef struct {
    char     dirName[64];
    char     displayName[64];
    int      count;
    RomEntry roms[MAX_ROMS];
} System;

typedef struct {
    Camera3D  camera;
    Model     cardModel;
    Texture2D placeholder;

    AppLevel  level;
    int       systemCount;
    System    systems[MAX_SYSTEMS];
    int       selectedSystem;

    int       cardCount;
    int       targetIndex;
    float     smoothIndex;
    float     rightTimer;
    float     leftTimer;

    Texture2D textures[MAX_ROMS];
    bool      textureLoaded[MAX_ROMS];
    int       loadedCenter;

    int       playingSysIdx;
    int       playingRomIdx;
} GameState;

static GameState g;

// ---------------------------------------------------------------------------

static const char *SystemDisplayName(const char *dir) {
    static const struct { const char *key, *val; } map[] = {
        {"gb",          "Game Boy"},
        {"gbc",         "Game Boy Color"},
        {"gba",         "Game Boy Advance"},
        {"nes",         "NES"},
        {"snes",        "Super NES"},
        {"n64",         "Nintendo 64"},
        {"nds",         "Nintendo DS"},
        {"3ds",         "Nintendo 3DS"},
        {"gamecube",    "GameCube"},
        {"gc",          "GameCube"},
        {"wii",         "Wii"},
        {"virtualboy",  "Virtual Boy"},
        {"vb",          "Virtual Boy"},
        {"genesis",     "Sega Genesis"},
        {"megadrive",   "Sega Mega Drive"},
        {"md",          "Sega Mega Drive"},
        {"sms",         "Master System"},
        {"mastersystem","Master System"},
        {"gg",          "Game Gear"},
        {"gamegear",    "Game Gear"},
        {"saturn",      "Saturn"},
        {"32x",         "Sega 32X"},
        {"segacd",      "Sega CD"},
        {"scd",         "Sega CD"},
        {"dreamcast",   "Dreamcast"},
        {"dc",          "Dreamcast"},
        {"psx",         "PlayStation"},
        {"ps1",         "PlayStation"},
        {"ps2",         "PlayStation 2"},
        {"psp",         "PSP"},
        {"atari2600",   "Atari 2600"},
        {"2600",        "Atari 2600"},
        {"atari7800",   "Atari 7800"},
        {"7800",        "Atari 7800"},
        {"lynx",        "Atari Lynx"},
        {"jaguar",      "Atari Jaguar"},
        {"pce",         "PC Engine"},
        {"tg16",        "TurboGrafx-16"},
        {"wonderswan",  "WonderSwan"},
        {"wsc",         "WonderSwan Color"},
        {"c64",         "Commodore 64"},
        {NULL, NULL}
    };
    for (int i = 0; map[i].key; i++)
        if (strcmp(dir, map[i].key) == 0) return map[i].val;
    return dir;
}

static const char *CoreForSystem(const char *dir) {
    static const struct { const char *key, *val; } map[] = {
        {"gb",          "gambatte_libretro"},
        {"gbc",         "gambatte_libretro"},
        {"gba",         "mgba_libretro"},
        {"nes",         "fceumm_libretro"},
        {"famicom",     "fceumm_libretro"},
        {"fds",         "fceumm_libretro"},
        {"snes",        "snes9x_libretro"},
        {"superfamicom","snes9x_libretro"},
        {"n64",         "mupen64plus_next_libretro"},
        {"nds",         "desmume_libretro"},
        {"genesis",     "genesis_plus_gx_libretro"},
        {"megadrive",   "genesis_plus_gx_libretro"},
        {"md",          "genesis_plus_gx_libretro"},
        {"sms",         "genesis_plus_gx_libretro"},
        {"mastersystem","genesis_plus_gx_libretro"},
        {"gg",          "genesis_plus_gx_libretro"},
        {"gamegear",    "genesis_plus_gx_libretro"},
        {"saturn",      "yabasanshiro_libretro"},
        {"32x",         "picodrive_libretro"},
        {"segacd",      "genesis_plus_gx_libretro"},
        {"scd",         "genesis_plus_gx_libretro"},
        {"dreamcast",   "flycast_libretro"},
        {"dc",          "flycast_libretro"},
        {"psx",         "pcsx_rearmed_libretro"},
        {"ps1",         "pcsx_rearmed_libretro"},
        {"psp",         "ppsspp_libretro"},
        {"atari2600",   "stella_libretro"},
        {"2600",        "stella_libretro"},
        {"atari7800",   "prosystem_libretro"},
        {"7800",        "prosystem_libretro"},
        {"lynx",        "handy_libretro"},
        {"jaguar",      "virtualjaguar_libretro"},
        {"pce",         "mednafen_pce_libretro"},
        {"tg16",        "mednafen_pce_libretro"},
        {"ngp",         "mednafen_ngp_libretro"},
        {"ngpc",        "mednafen_ngp_libretro"},
        {"wonderswan",  "mednafen_wswan_libretro"},
        {"wsc",         "mednafen_wswan_libretro"},
        {"c64",         "vice_x64_libretro"},
        {NULL, NULL}
    };
    for (int i = 0; map[i].key; i++)
        if (strcmp(dir, map[i].key) == 0) return map[i].val;
    return NULL;
}

// ROM file extensions — anything that's not one of these (or .png) is skipped
static bool IsRomExtension(const char *path) {
    static const char *exts[] = {
        ".nes",".fds",".unf",".unif",
        ".smc",".sfc",".fig",".swc",
        ".gb",".gbc",".gba",
        ".z64",".n64",".v64",
        ".nds",".3ds",
        ".iso",".bin",".cue",".chd",".pbp",
        ".md",".gen",".smd",
        ".sms",".gg",
        ".pce",
        ".a26",".a52",".a78",
        ".lnx",
        ".ngp",".ngc",
        ".ws",".wsc",
        ".col",".int",".vec",
        ".d64",".t64",".prg",
        ".adf",
        ".zip",".7z",
        NULL
    };
    for (int i = 0; exts[i]; i++)
        if (IsFileExtension(path, exts[i])) return true;
    return false;
}

static void ScanSystems(const char *romsDir) {
    FilePathList dirs = LoadDirectoryFiles(romsDir);
    for (int i = 0; i < (int)dirs.count && g.systemCount < MAX_SYSTEMS; i++) {
        if (!DirectoryExists(dirs.paths[i])) continue;

        const char *dirName = GetFileName(dirs.paths[i]);
        System     *sys     = &g.systems[g.systemCount];

        strncpy(sys->dirName, dirName, sizeof(sys->dirName) - 1);
        strncpy(sys->displayName, SystemDisplayName(dirName), sizeof(sys->displayName) - 1);

        FilePathList files = LoadDirectoryFiles(dirs.paths[i]);
        for (int j = 0; j < (int)files.count && sys->count < MAX_ROMS; j++) {
            if (!IsRomExtension(files.paths[j])) continue;

            RomEntry *rom  = &sys->roms[sys->count];
            const char *stem = GetFileNameWithoutExt(files.paths[j]);

            strncpy(rom->romPath, files.paths[j], sizeof(rom->romPath) - 1);
            strncpy(rom->label,   stem,            sizeof(rom->label)   - 1);
            snprintf(rom->posterPath, sizeof(rom->posterPath),
                "%s/%s.png", dirs.paths[i], stem);
            sys->count++;
        }
        UnloadDirectoryFiles(files);

        if (sys->count > 0) g.systemCount++;
    }
    UnloadDirectoryFiles(dirs);
}

// ---------------------------------------------------------------------------

static void UnloadAllTextures(void) {
    for (int i = 0; i < MAX_ROMS; i++) {
        if (g.textureLoaded[i]) {
            if (g.textures[i].id != g.placeholder.id)
                UnloadTexture(g.textures[i]);
            g.textures[i]      = (Texture2D){0};
            g.textureLoaded[i] = false;
        }
    }
}

static const char *ActiveCardPath(int i) {
    if (g.level == LEVEL_SYSTEM)
        return g.systems[i].roms[0].posterPath;
    return g.systems[g.selectedSystem].roms[i].posterPath;
}

static const char *ActiveCardLabel(int i) {
    if (g.level == LEVEL_SYSTEM)
        return g.systems[i].displayName;
    return g.systems[g.selectedSystem].roms[i].label;
}

static void UpdateWindow(int center) {
    int lo = center - WINDOW_HALF;
    int hi = center + WINDOW_HALF;
    for (int i = 0; i < g.cardCount; i++) {
        if (i >= lo && i <= hi) {
            if (!g.textureLoaded[i]) {
                g.textures[i] = LoadTexture(ActiveCardPath(i));
                if (g.textures[i].id == 0) g.textures[i] = g.placeholder;
                g.textureLoaded[i] = true;
            }
        } else {
            if (g.textureLoaded[i]) {
                if (g.textures[i].id != g.placeholder.id)
                    UnloadTexture(g.textures[i]);
                g.textures[i]      = (Texture2D){0};
                g.textureLoaded[i] = false;
            }
        }
    }
}

static void EnterSystemLevel(void) {
    UnloadAllTextures();
    g.level        = LEVEL_SYSTEM;
    g.cardCount    = g.systemCount;
    g.targetIndex  = 0;
    g.smoothIndex  = 0.0f;
    g.loadedCenter = -1;
    g.rightTimer = g.leftTimer = 0.0f;
}

static void EnterRomLevel(int sysIdx) {
    UnloadAllTextures();
    g.level          = LEVEL_ROM;
    g.selectedSystem = sysIdx;
    g.cardCount      = g.systems[sysIdx].count;
    g.targetIndex    = 0;
    g.smoothIndex    = 0.0f;
    g.loadedCenter   = -1;
    g.rightTimer = g.leftTimer = 0.0f;
}

static void LaunchRom(int sysIdx, int romIdx) {
    System   *sys = &g.systems[sysIdx];
    RomEntry *rom = &sys->roms[romIdx];

    const char *coreName = CoreForSystem(sys->dirName);
    if (!coreName) {
        TraceLog(LOG_WARNING, "No core mapped for system: %s", sys->dirName);
        return;
    }

    char corePath[512];
    snprintf(corePath, sizeof(corePath), "cores/%s" CORE_EXT, coreName);

    TraceLog(LOG_INFO, "Core: %s", corePath);
    TraceLog(LOG_INFO, "ROM:  %s", rom->romPath);

    if (!InitLibretro(corePath)) {
        TraceLog(LOG_ERROR, "Failed to load core: %s", corePath);
        return;
    }
    if (!LoadLibretroGame(rom->romPath)) {
        TraceLog(LOG_ERROR, "Failed to load game: %s", rom->romPath);
        CloseLibretro();
        return;
    }

    g.playingSysIdx = sysIdx;
    g.playingRomIdx = romIdx;
    g.level         = LEVEL_PLAYING;
}

// ---------------------------------------------------------------------------

static void GameLoop(void) {
    // ---- PLAYING state ----
    if (g.level == LEVEL_PLAYING) {
        if (IsKeyPressed(KEY_F1)
         || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_LEFT)) {
            UnloadLibretroGame();
            CloseLibretro();
            EnterRomLevel(g.playingSysIdx);
            g.targetIndex  = g.playingRomIdx;
            g.smoothIndex  = (float)g.playingRomIdx;
            UpdateWindow(g.playingRomIdx);
            g.loadedCenter = g.playingRomIdx;
            return;
        }
        if (LibretroShouldClose()) {
            UnloadLibretroGame();
            CloseLibretro();
            EnterRomLevel(g.playingSysIdx);
            return;
        }
        UpdateLibretro();
        BeginDrawing();
            ClearBackground(BLACK);
            DrawLibretroPro((Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()}, WHITE);
            DrawLibretroMessage();
        EndDrawing();
        return;
    }

    // ---- CARDFLOW state ----
    float dt = GetFrameTime();

    bool rightDown    = IsKeyDown(KEY_RIGHT)    || IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
    bool leftDown     = IsKeyDown(KEY_LEFT)     || IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
    bool rightPressed = IsKeyPressed(KEY_RIGHT) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
    bool leftPressed  = IsKeyPressed(KEY_LEFT)  || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
    bool select       = IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER)
                     || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
    bool back         = IsKeyPressed(KEY_X)
                     || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);

    if (IsKeyPressed(KEY_F11)) ToggleFullscreen();

    if (rightDown) {
        if (rightPressed) {
            if (g.targetIndex < g.cardCount - 1) g.targetIndex++;
            g.rightTimer = -REPEAT_DELAY;
        } else {
            g.rightTimer += dt;
            if (g.rightTimer >= REPEAT_RATE) {
                if (g.targetIndex < g.cardCount - 1) g.targetIndex++;
                g.rightTimer -= REPEAT_RATE;
            }
        }
    } else {
        g.rightTimer = 0.0f;
    }

    if (leftDown) {
        if (leftPressed) {
            if (g.targetIndex > 0) g.targetIndex--;
            g.leftTimer = -REPEAT_DELAY;
        } else {
            g.leftTimer += dt;
            if (g.leftTimer >= REPEAT_RATE) {
                if (g.targetIndex > 0) g.targetIndex--;
                g.leftTimer -= REPEAT_RATE;
            }
        }
    } else {
        g.leftTimer = 0.0f;
    }

    if (select) {
        if (g.level == LEVEL_SYSTEM) {
            EnterRomLevel(g.targetIndex);
        } else {
            LaunchRom(g.selectedSystem, g.targetIndex);
        }
    }

    if (back && g.level == LEVEL_ROM) {
        int prev = g.selectedSystem;
        EnterSystemLevel();
        g.targetIndex  = prev;
        g.smoothIndex  = (float)prev;
        UpdateWindow(prev);
        g.loadedCenter = prev;
    }

    if (g.targetIndex != g.loadedCenter) {
        UpdateWindow(g.targetIndex);
        g.loadedCenter = g.targetIndex;
    }

    g.smoothIndex = Lerp(g.smoothIndex, (float)g.targetIndex, 0.1f);

    BeginDrawing();
        ClearBackground(BLACK);

        BeginMode3D(g.camera);
            for (int i = 0; i < g.cardCount; i++) {
                float offset    = (float)i - g.smoothIndex;
                float absOffset = fabsf(offset);

                float scale = 1.2f - (absOffset * 0.15f);
                if (scale < 0.8f) scale = 0.8f;

                float zPop = (1.0f - absOffset) * 1.5f;
                if (zPop < 0.0f) zPop = 0.0f;

                Vector3 pos = { offset * 3.2f, 0.0f, (-absOffset * 1.5f) + zPop };

                g.cardModel.materials[0].maps[MATERIAL_MAP_ALBEDO].texture =
                    g.textureLoaded[i] ? g.textures[i] : g.placeholder;

                float  sideTilt = offset * -20.0f;
                Matrix transform = MatrixIdentity();
                transform = MatrixMultiply(transform, MatrixScale(scale, 1.0f, scale));
                transform = MatrixMultiply(transform, MatrixRotateX(DEG2RAD * 90));
                transform = MatrixMultiply(transform, MatrixRotateY(DEG2RAD * sideTilt));
                transform = MatrixMultiply(transform, MatrixTranslate(pos.x, pos.y, pos.z));

                g.cardModel.transform = transform;
                Color tint = (i == g.targetIndex) ? WHITE : GRAY;
                DrawModel(g.cardModel, (Vector3){0,0,0}, 1.0f, tint);
            }
        EndMode3D();

        const char *label = ActiveCardLabel(g.targetIndex);
        int fontSize  = 24;
        int textWidth = MeasureText(label, fontSize);
        DrawText(label, (GetScreenWidth() - textWidth) / 2, GetScreenHeight() - 40, fontSize, WHITE);

        if (g.level == LEVEL_ROM) {
            const char *sysName = g.systems[g.selectedSystem].displayName;
            DrawText(sysName, 10, 10, 20, LIGHTGRAY);
            DrawText("X: back", GetScreenWidth() - 90, 10, 20, DARKGRAY);
        } else {
            DrawText("Select a system", 10, 10, 20, LIGHTGRAY);
        }

    EndDrawing();
}

// ---------------------------------------------------------------------------

int main(void) {
    InitWindow(1280, 720, "CardRetro");
    InitAudioDevice();
    SetTargetFPS(60);

    g.camera.position   = (Vector3){ 0.0f, 0.0f, 10.0f };
    g.camera.target     = (Vector3){ 0.0f, 0.0f, 0.0f };
    g.camera.up         = (Vector3){ 0.0f, 1.0f, 0.0f };
    g.camera.fovy       = 45.0f;
    g.camera.projection = CAMERA_PERSPECTIVE;

    Image placeholderImg = GenImageColor(1, 1, DARKGRAY);
    g.placeholder        = LoadTextureFromImage(placeholderImg);
    UnloadImage(placeholderImg);

    Mesh mesh   = GenMeshPlane(2.5f, 3.5f, 1, 1);
    g.cardModel = LoadModelFromMesh(mesh);

    ScanSystems("roms");
    if (g.systemCount == 0) {
        TraceLog(LOG_ERROR, "No systems found in roms/");
        CloseAudioDevice();
        CloseWindow();
        return 1;
    }

    EnterSystemLevel();

    while (!WindowShouldClose()) GameLoop();

    if (g.level == LEVEL_PLAYING) {
        UnloadLibretroGame();
        CloseLibretro();
    }
    UnloadAllTextures();
    UnloadTexture(g.placeholder);
    UnloadModel(g.cardModel);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
