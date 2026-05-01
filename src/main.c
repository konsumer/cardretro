#include "raylib.h"
#include "raymath.h"

#define CARD_COUNT 30

#define REPEAT_DELAY 0.4f   // seconds before repeat kicks in
#define REPEAT_RATE  0.08f  // seconds between repeats while held

int main(void) {
    InitWindow(1280, 720, "CardRetro");

    Camera3D camera = { 0 };
    camera.position = (Vector3){ 0.0f, 0.0f, 10.0f };
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    // 1. Array of textures for different cards
    Texture2D posters[CARD_COUNT];

    // For this example, we'll load the same image, but you can load poster1.png, poster2.png, etc.
    for (int i = 0; i < CARD_COUNT; i++) posters[i] = LoadTexture("poster.png");

    Mesh mesh = GenMeshPlane(2.5f, 3.5f, 1, 1);
    Model cardModel = LoadModelFromMesh(mesh);

    int targetIndex = 0;
    float smoothIndex = 0.0f;
    float rightTimer = 0.0f;
    float leftTimer  = 0.0f;

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
                rightTimer = -REPEAT_DELAY;         // wait before first repeat
            } else {
                rightTimer += dt;
                if (rightTimer >= REPEAT_RATE) {
                    if (targetIndex < CARD_COUNT - 1) targetIndex++;
                    rightTimer -= REPEAT_RATE;      // keep remainder for steady cadence
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

        smoothIndex = Lerp(smoothIndex, (float)targetIndex, 0.1f);

        BeginDrawing();
            ClearBackground(BLACK);

            BeginMode3D(camera);
                for (int i = 0; i < CARD_COUNT; i++) {
                    float offset = (float)i - smoothIndex;
                    float absOffset = fabsf(offset);

                    // 2. THE POP EFFECT:
                    // Scale: Center (offset 0) is 1.2x, edges are 1.0x
                    float scale = 1.2f - (absOffset * 0.15f);
                    if (scale < 0.8f) scale = 0.8f; // Don't let it get too tiny

                    // Position: Center card moves slightly closer to camera (Z + 1.0)
                    float zPop = (1.0f - absOffset) * 1.5f;
                    if (zPop < 0) zPop = 0;

                    Vector3 pos = { offset * 3.2f, 0.0f, (-absOffset * 1.5f) + zPop };

                    // 3. APPLY INDIVIDUAL TEXTURE
                    cardModel.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = posters[i];

                    // Rotation & Matrix
                    float sideTilt = offset * -20.0f;
                    Matrix transform = MatrixIdentity();
                    transform = MatrixMultiply(transform, MatrixScale(scale, 1.0f, scale)); // Apply Scale
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

    for (int i = 0; i < CARD_COUNT; i++) UnloadTexture(posters[i]);
    UnloadModel(cardModel);
    CloseWindow();

    return 0;
}
