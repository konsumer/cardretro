#include "raylib.h"
#include "raymath.h"

#ifdef PLATFORM_WEB
#include <emscripten/emscripten.h>
#endif

#define CARD_COUNT   2000
#define WINDOW_HALF  8     // textures to keep loaded on each side of the focused card

#define REPEAT_DELAY 0.4f
#define REPEAT_RATE  0.08f

// Replace this with your actual per-card filename logic
static const char *CardFilename(int i) {
    // return TextFormat("poster%d.png", i + 1);
    (void)i;
    return "poster.png";  // demo: every card uses the same image
}

// ---------------------------------------------------------------------------
// All mutable state lives here so it's in the data segment, not the stack.
// This is required for emscripten_set_main_loop (no ASYNCIFY needed).
// ---------------------------------------------------------------------------
typedef struct {
    Camera3D  camera;
    Model     cardModel;
    Texture2D placeholder;
    Texture2D textures[CARD_COUNT];
    bool      textureLoaded[CARD_COUNT];
    int       targetIndex;
    int       loadedCenter;
    float     smoothIndex;
    float     rightTimer;
    float     leftTimer;
} GameState;

static GameState g;  // zero-initialised at program start

// ---------------------------------------------------------------------------

static void UpdateWindow(int center) {
    int lo = center - WINDOW_HALF;
    int hi = center + WINDOW_HALF;

    for (int i = 0; i < CARD_COUNT; i++) {
        if (i >= lo && i <= hi) {
            if (!g.textureLoaded[i]) {
                g.textures[i] = LoadTexture(CardFilename(i));
                if (g.textures[i].id == 0) g.textures[i] = g.placeholder;
                g.textureLoaded[i] = true;
            }
        } else {
            if (g.textureLoaded[i]) {
                if (g.textures[i].id != g.placeholder.id) UnloadTexture(g.textures[i]);
                g.textures[i] = (Texture2D){0};
                g.textureLoaded[i] = false;
            }
        }
    }
}

static void GameLoop(void) {
    float dt = GetFrameTime();

    bool rightDown    = IsKeyDown(KEY_RIGHT)    || IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
    bool leftDown     = IsKeyDown(KEY_LEFT)     || IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
    bool rightPressed = IsKeyPressed(KEY_RIGHT) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
    bool leftPressed  = IsKeyPressed(KEY_LEFT)  || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT);

    if (rightDown) {
        if (rightPressed) {
            if (g.targetIndex < CARD_COUNT - 1) g.targetIndex++;
            g.rightTimer = -REPEAT_DELAY;
        } else {
            g.rightTimer += dt;
            if (g.rightTimer >= REPEAT_RATE) {
                if (g.targetIndex < CARD_COUNT - 1) g.targetIndex++;
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

    if (g.targetIndex != g.loadedCenter) {
        UpdateWindow(g.targetIndex);
        g.loadedCenter = g.targetIndex;
    }

    g.smoothIndex = Lerp(g.smoothIndex, (float)g.targetIndex, 0.1f);

    BeginDrawing();
        ClearBackground(BLACK);

        BeginMode3D(g.camera);
            for (int i = 0; i < CARD_COUNT; i++) {
                float offset    = (float)i - g.smoothIndex;
                float absOffset = fabsf(offset);

                float scale = 1.2f - (absOffset * 0.15f);
                if (scale < 0.8f) scale = 0.8f;

                float zPop = (1.0f - absOffset) * 1.5f;
                if (zPop < 0) zPop = 0;

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

        if (g.targetIndex == 0) {
            DrawText("Navigate with D-Pad / Arrows", 10, 10, 20, LIGHTGRAY);
        }
    EndDrawing();
}

int main(void) {
    InitWindow(1280, 720, "CardRetro");

    g.camera.position   = (Vector3){ 0.0f, 0.0f, 10.0f };
    g.camera.target     = (Vector3){ 0.0f, 0.0f, 0.0f };
    g.camera.up         = (Vector3){ 0.0f, 1.0f, 0.0f };
    g.camera.fovy       = 45.0f;
    g.camera.projection = CAMERA_PERSPECTIVE;

    Image placeholderImg    = GenImageColor(1, 1, DARKGRAY);
    g.placeholder           = LoadTextureFromImage(placeholderImg);
    UnloadImage(placeholderImg);

    Mesh mesh       = GenMeshPlane(2.5f, 3.5f, 1, 1);
    g.cardModel     = LoadModelFromMesh(mesh);
    g.loadedCenter  = -1;  // force UpdateWindow on the first frame

    SetTargetFPS(60);

#ifdef PLATFORM_WEB
    emscripten_set_main_loop(GameLoop, 0, 1);
#else
    while (!WindowShouldClose()) GameLoop();

    for (int i = 0; i < CARD_COUNT; i++) {
        if (g.textureLoaded[i] && g.textures[i].id != g.placeholder.id)
            UnloadTexture(g.textures[i]);
    }
    UnloadTexture(g.placeholder);
    UnloadModel(g.cardModel);
    CloseWindow();
#endif

    return 0;
}
