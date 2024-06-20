#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "raylib/raylib.h"
#include "raylib/raymath.h"

#define screenWidth  1920
#define screenHeight 1080
#define MAX_PARTICLES 4096

const int chunksWidth  = 15;
const int chunksHeight = 8;
int chunkAmt;

enum Kind {
    FIRST,
    SECOND,
    THIRD,
    FOURTH,
    FIFTH,
    SIXTH,
    COUNT,
};

float* data;
enum DataValues {
    INNER = COUNT*COUNT,
    OUTER = COUNT*COUNT+1,
};

Color kindColors[] = {
    {255, 0, 0, 255},
    {0, 255, 0, 255},
    {0, 0, 255, 255},
    {255, 0, 255, 255},
    {255, 255, 0, 255},
    {255, 255, 255, 255}
};

typedef struct {
    Vector2 position, velocity;
    int kind;
} Particle;

typedef struct {
    Particle** particles;
    int num;
    int flipX, flipY;
} Chunk;

Chunk* GetChunk(Chunk* chunks, int x, int y) {

    int flipX = 0, flipY = 0;
    if (x >= chunksWidth) {
        flipX = -1;
        x -= chunksWidth;
    }
    if (x < 0) {
        flipX = 1;
        x += chunksWidth;
    }
    if (y >= chunksHeight) {
        flipY = -1;
        y -= chunksHeight;
    }
    if (y < 0) {
        flipY = 1;
        y += chunksHeight;
    }
    int chunkIndex = (x * chunksHeight) + y;

    chunks[chunkIndex].flipX = flipX;
    chunks[chunkIndex].flipY = flipY;
    return chunks + chunkIndex;
}

Chunk** GetNearbyChunks(Chunk* chunks, Particle* particle) {
    int chunkX = floor( (particle->position.x / ((float)screenWidth) ) * chunksWidth  );
    int chunkY = floor( (particle->position.y / ((float)screenHeight)) * chunksHeight );

    Chunk** nearby = malloc(sizeof(Chunk*) * 9);
    nearby[0] = GetChunk(chunks, chunkX, chunkY);
    nearby[1] = GetChunk(chunks, chunkX + 1, chunkY + 1);
    nearby[2] = GetChunk(chunks, chunkX - 1, chunkY - 1);
    nearby[3] = GetChunk(chunks, chunkX - 1, chunkY + 1);
    nearby[4] = GetChunk(chunks, chunkX + 1, chunkY - 1);
    nearby[5] = GetChunk(chunks, chunkX + 1, chunkY);
    nearby[6] = GetChunk(chunks, chunkX, chunkY + 1);
    nearby[7] = GetChunk(chunks, chunkX - 1, chunkY);
    nearby[8] = GetChunk(chunks, chunkX, chunkY - 1);
    return nearby;
}

void InitParticle(Particle *ptr, Vector2 position, int kind) {
    ptr->position = position;
    ptr->kind = kind;
}

Vector2 CalculateForce(Particle* ptr1, Particle* ptr2, int flipX, int flipY) {
    if (ptr1 == ptr2) return (Vector2){0, 0};

    ptr2->position.x -= screenWidth * flipX;
    ptr2->position.y -= screenHeight * flipY;

    float  dist = Vector2Distance(ptr1->position, ptr2->position);
    Vector2 dir = Vector2Normalize(Vector2Subtract(ptr2->position, ptr1->position));
    float coef  = data[ptr1->kind + ptr2->kind * COUNT];

    float inner = data[INNER], outer = data[OUTER];

    float magnitude = 0;
    if (dist < inner) {
        magnitude = - (1.f - dist/inner) * 256;
 
    } else {
        float blend = PI * fmax(0, fmin(1, (dist - inner) / (outer - inner)));
        // printf("%f %f\n", dist, outer);
        float sine = sinf(blend);
        magnitude = sine * 1024 * coef;
    }

    Vector2 force = Vector2Multiply(dir, (Vector2){magnitude, magnitude});

    ptr2->position.x += screenWidth * flipX;
    ptr2->position.y += screenHeight * flipY;
    return force;
}

Vector2 CalculateForceSum(Particle* ptr, Chunk* chunks) {
    Chunk** nearbyChunks = GetNearbyChunks(chunks, ptr);
    Vector2 force = {0, 0};

    for (int i = 0; i < 9; i++) {
        Chunk* chunk = nearbyChunks[i];
        for (int j = 0; j < chunk->num; j++) {
            force = Vector2Add(force, CalculateForce(ptr, chunk->particles[j], chunk->flipX, chunk->flipY));
        }
    }
    free(nearbyChunks);

    return force;
}

void ProcessParticle(Particle* ptr, Chunk* chunks, float delta) {
    ptr->position = Vector2Add(
        ptr->position,
        Vector2Multiply(ptr->velocity, (Vector2){delta, delta})
    ); 

    if (ptr->position.x > screenWidth - 0.1f) ptr->position.x = 0.1f;
    if (ptr->position.x < 0.1f) ptr->position.x = screenWidth - 0.1f;
    if (ptr->position.y > screenHeight - 0.1f) ptr->position.y = 0.1f;
    if (ptr->position.y < 0.1f) ptr->position.y = screenHeight - 0.1f;

    Vector2 force = CalculateForceSum(ptr, chunks);
    force = Vector2Subtract(force, Vector2Multiply(ptr->velocity, (Vector2){8, 8}));

    ptr->velocity = Vector2Add(
        ptr->velocity,
        Vector2Multiply(force, (Vector2){delta, delta})
    );
}

void DrawParticle(Particle* ptr, int offsetX, int offsetY) {
    // DrawCircle(ptr->position.x + screenWidth * offsetX, ptr->position.y + screenHeight * offsetY, 5, kindColors[ptr->kind]);
    DrawPoly((Vector2){ptr->position.x + screenWidth * offsetX, ptr->position.y + screenHeight * offsetY},
        7, 5, 0, kindColors[ptr->kind]
    );
}

void InitChunks(Chunk* chunks) {
    for (int i = 0; i < chunkAmt; i++) {
        Chunk* chunk = chunks + i;
        chunk->particles = (Particle**)malloc(sizeof(Particle*) * 1024);
        chunk->num = 0;
        chunk->flipX = 0;
        chunk->flipY = 0;
    }
}

void ClearChunks(Chunk* chunks) {
    for (int i = 0; i < chunkAmt; i++) {
        Chunk* chunk = chunks + i;
        free(chunk->particles);

        chunk->particles = (Particle**)malloc(sizeof(Particle*) * 1024);
        chunk->num = 0;
    }
}

void BuildChunks(Chunk* chunks, Particle* particles, int numParticles) {
    for (int i = 0; i < numParticles; i++) {
        Particle* particle = particles + i;

        int chunkX = floor( (particle->position.x / ((float)screenWidth) ) * chunksWidth  );
        int chunkY = floor( (particle->position.y / ((float)screenHeight)) * chunksHeight );

        int chunkIndex = (chunkX * chunksHeight) + chunkY;
        Chunk* chunk = chunks + chunkIndex;

        // printf("%i %i %f %f", chunkX, chunkY, particle->position.x, particle->position.y);

        chunk->particles[chunk->num] = particle;
        chunk->num++;
    }
}

void RebuildChunks(Chunk* chunks, Particle* particles, int numParticles) {
    ClearChunks(chunks);
    BuildChunks(chunks, particles, numParticles);
}

bool IsFloat(char* str) {
    char* faulty;
    strtof(str, &faulty);
    
    // No characters converted
    if (str == faulty) {
        return false;
    }

    // Check for non white space
    while(*faulty != '\0') {
        if (isspace((unsigned char)*faulty)) {
            return false;
        }
        faulty++;
    }
    return true;
}
void RandomizeData(float* data) {
    for (int i = 0; i < COUNT*COUNT; i++) {
        data[i] = (rand()%100) / 50.f - 1.0f;
    }
}

void SaveData(float* data, char* filename) {
    FILE* file = fopen(filename, "w");
    char fileData[4096] = "";

    for (int i = 0; i < COUNT*COUNT + 2; i++) {
        char num[16];
        gcvt((double)data[i], 6, num);

        strcat(fileData, num);
        strcat(fileData, ";");
    }
    fputs(fileData, file);
    fclose(file);
}

void LoadData(float* data, char* filename) {
    FILE* file = fopen(filename, "r");

    char str[16], chr;
    int i = 0, memberOn = 0;

    while ((chr = fgetc(file)) != EOF) {
        if (chr == ';') {
            str[i] = '\0';
            
            if (IsFloat(str)) {
                data[memberOn] = atof(str);
                memberOn++;

            } else if (i == 0) {
                data[memberOn] = 0;
                memberOn++;
            }
            i = 0;

        } else if (chr != ' ' && chr != '\n' && chr != '\r') {
            str[i++] = chr;
        }
    }

    fclose(file);
}

char* ChooseFile() {
    system("powershell -command \"Add-Type -AssemblyName System.Windows.Forms; $ofd = New-Object System.Windows.Forms.OpenFileDialog; $ofd.ShowDialog() | Out-Null; $ofd.FileName\" > temp.txt");
    FILE* file = fopen("temp.txt", "r");
    
    char filepath[128];
    fgets(filepath, sizeof(filepath), file);

    int newlineIndex = strcspn(filepath, "\n");
    filepath[newlineIndex] = '\0';

    char* result = malloc(strlen(filepath) + 1);
    strcpy(result, filepath);
    
    fclose(file);
    remove("temp.txt");
    return result;
}

int main(int argc, char* argv[]) {
    chunkAmt = chunksHeight * chunksWidth;
    srand(time(NULL));

    data = malloc(sizeof(float) * (COUNT*COUNT + 2));
    // for (int i = 0; i < COUNT*COUNT + 2; i++) {
    //     printf("%f ", data[i]);
    // }
    LoadData(data, "zmije.csv");
    if (argc > 1)
        LoadData(data, argv[1]);
    else
        LoadData(data, "prazno.csv");

    InitWindow(screenWidth, screenHeight, "Simulacija");

    Chunk* chunks;
    chunks = (Chunk*)malloc(sizeof(Chunk) * chunkAmt);
    InitChunks(chunks);

    Particle* particles = malloc(sizeof(Particle) * MAX_PARTICLES);
    int numParticles = 1024;

    for (int i = 0; i < numParticles; i++) {
        Vector2 position = {rand() % screenWidth, rand() % screenHeight};
        InitParticle((particles + i), position, rand() % COUNT);
        particles[i].velocity = (Vector2){0, 0};
    }

    Camera2D camera;
    camera.offset = (Vector2){screenWidth*.5f, screenHeight*.5f};
    camera.rotation = 0;
    camera.target = (Vector2){screenWidth, screenHeight};
    camera.zoom = 1.f;

    Vector2 cameraVel = {0, 0};
    float scrollVel = 0;
    float cameraSpeed = 1024;

    float timeScale = 1;
    float tabOffset = -600;
    bool tabOpen = false;

    bool trail = false;

    SetTargetFPS(40);
    while (!WindowShouldClose()) {
        float delta = fminf(GetFrameTime(), 1.f/15.f) * timeScale;

        char title[256];
        sprintf(title, "Simulacija - FPS: %d", GetFPS());
        SetWindowTitle(title);

        BeginDrawing();

        if (trail)
            DrawRectangle(0, 0, screenWidth, screenHeight, (Color){0, 0, 0, 255 * GetFrameTime() * 15});
        else
            ClearBackground(BLACK);
        
        if (IsKeyPressed(KEY_V)) {
            trail = !trail;
        }
        
        BeginMode2D(camera);
        RebuildChunks(chunks, particles, numParticles);

        for (int i = 0; i < numParticles; i++) {
            Particle* particle = particles + i;
            ProcessParticle(particle, chunks, delta);
            DrawParticle(particle, 0, 0);
            DrawParticle(particle, 1, 0);
            DrawParticle(particle, 0, 1);
            DrawParticle(particle, 1, 1);
        }

        if (IsKeyPressed(KEY_ENTER)) {
            RandomizeData(data);
        }

        if (IsKeyPressed(KEY_SPACE)) {
            timeScale = 1 - timeScale;
        }

        if (IsKeyPressed(KEY_M)) {
            for (int i = 0; i < numParticles * 0.25f; i++) {
                particles[rand()%numParticles].kind = rand() % COUNT;
            }
        }

        if (IsKeyPressed(KEY_S) && IsKeyDown(KEY_LEFT_CONTROL)) {
            SaveData(data, "save.csv");
        }

        if (IsKeyPressed(KEY_L) && IsKeyDown(KEY_LEFT_CONTROL)) {
            char* path = ChooseFile();
            LoadData(data, path);
            free(path);
        }

        if (IsKeyPressed(KEY_R)) {
            LoadData(data, "prazno.csv");
            for (int i = 0; i < numParticles; i++) {
                Vector2 position = {rand() % screenWidth, rand() % screenHeight};
                InitParticle((particles + i), position, rand() % COUNT);
                particles[i].velocity = (Vector2){0, 0};
            }
        }

        Vector2 input = (Vector2){
            (float)IsKeyDown(KEY_D) - (float)IsKeyDown(KEY_A),
            (float)IsKeyDown(KEY_S) - (float)IsKeyDown(KEY_W)
        };
        float blend = 1.f - pow(.5f, GetFrameTime() * 10.f);
        cameraVel.x = Lerp(cameraVel.x, input.x * cameraSpeed, blend);
        cameraVel.y = Lerp(cameraVel.y, input.y * cameraSpeed, blend);

        camera.target = Vector2Add(
            camera.target, Vector2Multiply(cameraVel, (Vector2){GetFrameTime() / camera.zoom, GetFrameTime() / camera.zoom})
        );

        scrollVel += GetMouseWheelMove();
        scrollVel = Lerp(scrollVel, 0, blend);

        camera.zoom += scrollVel * 0.025f * camera.zoom;

        EndMode2D();

        if (timeScale == 0) {
            DrawText("Paused", 12, 8, 64, (Color){255, 255, 255, 155});
        }

        tabOffset = Lerp(tabOffset, -600 * (int)(!tabOpen), 0.2);
        if (IsKeyPressed(KEY_TAB)) tabOpen = !tabOpen;

        int rectWidth = 64, gap = 4;
        for (int i = 0; i < COUNT; i++) {
            DrawRectangle(i * (rectWidth + gap) + 112 + tabOffset, 96, rectWidth, rectWidth,
                kindColors[i]
            );
        }

        for (int i = 0; i < COUNT; i++) {
            DrawRectangle(16 + tabOffset, i * (rectWidth + gap) + 196, rectWidth, rectWidth,
                kindColors[i]
            );
        }
        Vector2 mPos = GetMousePosition();

        for (int x = 0; x < COUNT; x++) {
            for (int y = 0; y < COUNT; y++) {
                float blend = data[x + y * COUNT];
                blend = fabs(blend);

                int alpha = Lerp(126.f, 255.f, blend),
                    r = data[x + y * COUNT] < 0 ? 255 : 0,
                    g = 255 - r;
                
                float scale = fabs(data[x + y * COUNT]);
                
                Vector2 pos = {x * (rectWidth + gap) + 112 + tabOffset, y * (rectWidth + gap) + 196};
                if (mPos.x > pos.x && mPos.x < pos.x + rectWidth &&
                    mPos.y > pos.y && mPos.y < pos.y + rectWidth) {

                    int input = (int)IsMouseButtonPressed(MOUSE_BUTTON_LEFT) -
                                (int)IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
                    
                    data[x + y * COUNT] += input * 0.25f;
                    data[x + y * COUNT] = fmaxf(fminf(data[x + y * COUNT], 1), -1);
                }
                
                DrawRectangle(pos.x + rectWidth*(1 - scale)*.5f, pos.y + rectWidth*(1 - scale)*.5f, rectWidth*scale, rectWidth*scale,
                    (Color){r, g, 0, alpha}
                );
            }
        }

        EndDrawing();
    }
    // Deallocation
    ClearChunks(chunks);

    free(particles);
    free(data);
    
    // Program end
    CloseWindow();
    return 0;
}