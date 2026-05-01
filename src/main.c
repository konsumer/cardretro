#include "raylib.h"
#include "raymath.h"

#define CARD_COUNT   2000
#define WINDOW_HALF  8     // textures to keep loaded on each side of the focused card

#define REPEAT_DELAY 0.4f
#define REPEAT_RATE  0.08f

// Replace this with your actual per-card filename logic
static const char *CardFilename(int i) {
    (void)i;
    return "poster.png";  // demo: every card uses the same image
}

// Sync the loaded set to the window [center-WINDOW_HALF .. center+WINDOW_HALF].
// Cards outside the window are unloaded; cards inside are loaded on demand.
// Falls back to `placeholder` if the file can't be found.
static void UpdateWindow(Texture2D *textures, bool *loaded, int center, Texture2D placeholder) {
    int lo = center - WINDOW_HALF;
    int hi = center + WINDOW_HALF;

    for (int i = 0; i < CARD_COUNT; i++) {
        if (i >= lo && i <= hi) {
            if (!loaded[i]) {
                textures[i] = LoadTexture(CardFilename(i));
                if (textures[i].id == 0) textures[i] = placeholder; // file missing → placeholder
                loaded[i] = true;
            }
        } else {
            if (loaded[i]) {
                if (textures[i].id != placeholder.id) UnloadTexture(textures[i]);
                textures[i] = (Texture2D){0};
                loaded[i] = false;
            }
        }
    }
}

int main(void) {
    InitWindow(1280, 720, "CardRetro");

    Camera3D camera = { 0 };
    camera.position = (Vector3){ 0.0f, 0.0f, 10.0f };
    camera.target   = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up       = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy     = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // 1x1 dark-grey placeholder shown while a real texture is outside the window
    Image placeholderImg = GenImageColor(1, 1, DARKGRAY);
    Texture2D placeholder = LoadTextureFromImage(placeholderImg);
    UnloadImage(placeholderImg);

    Texture2D textures[CARD_COUNT] = {0};
    bool      textureLoaded[CARD_COUNT] = {0};

    Mesh  mesh      = GenMeshPlane(2.5f, 3.5f, 1, 1);
    Model cardModel = LoadModelFromMesh(mesh);

    int   targetIndex  = 0;
    int   loadedCenter = -1;   // force UpdateWindow on the first frame
    float smoothIndex  = 0.0f;
    float rightTimer   = 0.0f;
    float leftTimer    = 0.0f;

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        bool rightDown    = IsKeyDown(KEY_RIGHT)    || IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
        bool leftDown     = IsKeyDown(KEY_LEFT)     || IsGamepadButtonDown(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
        bool rightPressed = IsKeyPressed(KEY_RIGHT) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);
        bool leftPressed  = IsKeyPressed(KEY_LEFT)  || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT);

        if (rightDown) {
            if (rightPressed) {
                if (targetIndex < CARD_COUNT - 1) targetIndex++;
                rightTimer = -REPEAT_DELAY;
            } else {
                rightTimer += dt;
                if (rightTimer >= REPEAT_RATE) {
                    if (targetIndex < CARD_COUNT - 1) targetIndex++;
                    rightTimer -= REPEAT_RATE;
                }
            }
        } else {
            rightTimer = 0.0f;
        }

        if (leftDown) {
            if (leftPressed) {
                if (targetIndex > 0) targetIndex--;
                leftTimer = -REPEAT_DELAY;
            } else {
                leftTimer += dt;
                if (leftTimer >= REPEAT_RATE) {
                    if (targetIndex > 0) targetIndex--;
                    leftTimer -= REPEAT_RATE;
                }
            }
        } else {
            leftTimer = 0.0f;
        }

        // shift the window only when the focused card actually changes
        if (targetIndex != loadedCenter) {
            UpdateWindow(textures, textureLoaded, targetIndex, placeholder);
            loadedCenter = targetIndex;
        }

        smoothIndex = Lerp(smoothIndex, (float)targetIndex, 0.1f);

        BeginDrawing();
            ClearBackground(BLACK);

            BeginMode3D(camera);
                for (int i = 0; i < CARD_COUNT; i++) {
                    float offset    = (float)i - smoothIndex;
                    float absOffset = fabsf(offset);

                    float scale = 1.2f - (absOffset * 0.15f);
                    if (scale < 0.8f) scale = 0.8f;

                    float zPop = (1.0f - absOffset) * 1.5f;
                    if (zPop < 0) zPop = 0;

                    Vector3 pos = { offset * 3.2f, 0.0f, (-absOffset * 1.5f) + zPop };

                    // loaded texture or cheap placeholder — no branch in the hot path
                    cardModel.materials[0].maps[MATERIAL_MAP_ALBEDO].texture =
                        textureLoaded[i] ? textures[i] : placeholder;

                    float  sideTilt = offset * -20.0f;
                    Matrix transform = MatrixIdentity();
                    transform = MatrixMultiply(transform, MatrixScale(scale, 1.0f, scale));
                    transform = MatrixMultiply(transform, MatrixRotateX(DEG2RAD * 90));
                    transform = MatrixMultiply(transform, MatrixRotateY(DEG2RAD * sideTilt));
                    transform = MatrixMultiply(transform, MatrixTranslate(pos.x, pos.y, pos.z));

                    cardModel.transform = transform;

                    Color tint = (i == targetIndex) ? WHITE : GRAY;
                    DrawModel(cardModel, (Vector3){0,0,0}, 1.0f, tint);
                }
            EndMode3D();

            DrawText("Navigate with D-Pad / Arrows", 10, 10, 20, LIGHTGRAY);
        EndDrawing();
    }

    for (int i = 0; i < CARD_COUNT; i++) {
        if (textureLoaded[i] && textures[i].id != placeholder.id) UnloadTexture(textures[i]);
    }
    UnloadTexture(placeholder);
    UnloadModel(cardModel);
    CloseWindow();

    return 0;
}
