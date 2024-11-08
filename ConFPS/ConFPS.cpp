#include <iostream>
#include <chrono>
#include <Windows.h>
#include <vector>
#include <utility>
#include <algorithm>
#include <cmath>

using namespace std;

constexpr int nScreenWidth = 120;  ///< Width of the console screen in characters.
constexpr int nScreenHeight = 40;  ///< Height of the console screen in characters.
constexpr int nMapHeight = 16;     ///< Height of the map in grid units.
constexpr int nMapWidth = 16;      ///< Width of the map in grid units.
constexpr float fFOV = 3.14159f / 4.0f; ///< Field of view in radians.
constexpr float fDepth = 16.0f;    ///< Maximum depth for ray casting.
constexpr float fSpeed = 5.0f;     ///< Player movement speed.
constexpr float fTurnSpeed = 0.8f; ///< Player turning speed.
constexpr float fBoundaryTolerance = 0.01f; ///< Tolerance for wall boundary detection.

float fPlayerX = 8.0f; ///< Player's initial X position.
float fPlayerY = 8.0f; ///< Player's initial Y position.
float fPlayerA = 0.0f; ///< Player's initial angle (orientation).

/// The map layout as a string, where '#' represents a wall and '.' represents empty space.
wstring map =
L"################"
L"#..............#"
L"####...##..##..#"
L"#..............#"
L"#..............#"
L"#.........#....#"
L"#..............#"
L"#....###....#..#"
L"#..............#"
L"####...........#"
L"#..............#"
L"#..............#"
L"#.......########"
L"#..............#"
L"#..............#"
L"################";

/// Moves the player in the specified direction, checking for wall collisions.
///
/// @param x Reference to the player's X position.
/// @param y Reference to the player's Y position.
/// @param angle The direction angle for movement.
/// @param elapsedTime The time elapsed since the last frame.
/// @param forward If true, moves forward; if false, moves backward.
void MovePlayer(float& x, float& y, float angle, float elapsedTime, bool forward) {
    float moveStep = forward ? fSpeed : -fSpeed;
    x += sinf(angle) * moveStep * elapsedTime;
    y += cosf(angle) * moveStep * elapsedTime;

    // Collision detection: Undo movement if a wall is hit
    if (map[(int)y * nMapWidth + (int)x] == '#') {
        x -= sinf(angle) * moveStep * elapsedTime;
        y -= cosf(angle) * moveStep * elapsedTime;
    }
}

/// Checks if the player is near a boundary between wall segments, to handle edge shading.
///
/// @param p A vector of pairs representing distances and dot products with wall edges.
/// @return True if the player is near a boundary, false otherwise.
bool CheckWallBoundary(const vector<pair<float, float>>& p) {
    for (int i = 0; i < 2; i++) {
        if (acos(p.at(i).second) < fBoundaryTolerance) {
            return true;
        }
    }
    return false;
}

/// Renders the 3D scene by casting rays and calculating wall heights and floor shading.
///
/// @param screen The screen buffer for output.
/// @param fElapsedTime The time elapsed since the last frame.
void RenderScene(wchar_t* screen, float fElapsedTime) {
    for (int x = 0; x < nScreenWidth; x++) {
        // Calculate ray angle for each screen column
        float fRayAngle = (fPlayerA - fFOV / 2.0f) + ((float)x / (float)nScreenWidth) * fFOV;

        float fDistanceToWall = 0;
        bool bHitWall = false;
        bool bBoundary = false;

        float fEyeX = sinf(fRayAngle); // Unit vector for ray in player space
        float fEyeY = cosf(fRayAngle);

        // Cast the ray outwards until it hits a wall or reaches maximum depth
        while (!bHitWall && fDistanceToWall < fDepth) {
            fDistanceToWall += 0.1f;
            int nTestX = (int)(fPlayerX + fEyeX * fDistanceToWall);
            int nTestY = (int)(fPlayerY + fEyeY * fDistanceToWall);

            // Check if ray is out of map bounds
            if (nTestX < 0 || nTestX >= nMapWidth || nTestY < 0 || nTestY >= nMapHeight) {
                bHitWall = true; // Treat as wall hit if out of bounds
                fDistanceToWall = fDepth;
            }
            else if (map[nTestY * nMapWidth + nTestX] == '#') {
                // Wall hit; calculate boundaries
                bHitWall = true;

                vector<pair<float, float>> p; // Store distances and angles for boundary check
                for (int tx = 0; tx < 2; tx++) {
                    for (int ty = 0; ty < 2; ty++) {
                        float vy = (float)nTestY + ty - fPlayerY;
                        float vx = (float)nTestX + tx - fPlayerX;
                        float d = sqrt(vx * vx + vy * vy);
                        float dot = (fEyeX * vx / d) + (fEyeY * vy / d);
                        p.push_back(make_pair(d, dot));
                    }
                }
                sort(p.begin(), p.end(), [](const pair<float, float>& left, const pair<float, float>& right) {
                    return left.first < right.first;
                    });

                bBoundary = CheckWallBoundary(p); // Determine if the wall edge is a boundary
            }
        }

        // Calculate ceiling and floor positions for this column
        int nCeiling = (nScreenHeight / 2.0f) - nScreenHeight / fDistanceToWall;
        int nFloor = nScreenHeight - nCeiling;

        // Wall shading based on distance
        short nShade = ' ';
        if (fDistanceToWall <= fDepth / 4.0f) nShade = 0x2588;      // Very close
        else if (fDistanceToWall < fDepth / 3.0f) nShade = 0x2593;  // Close
        else if (fDistanceToWall < fDepth / 2.0f) nShade = 0x2592;  // Medium distance
        else if (fDistanceToWall < fDepth) nShade = 0x2591;         // Far
        if (bBoundary) nShade = ' ';                                // Light shading at boundary

        // Fill screen buffer with ceiling, wall, and floor characters
        for (int y = 0; y < nScreenHeight; y++) {
            if (y < nCeiling)
                screen[y * nScreenWidth + x] = ' ';  // Ceiling
            else if (y >= nCeiling && y < nFloor)
                screen[y * nScreenWidth + x] = nShade;  // Wall
            else {
                // Floor shading based on distance
                float b = 1.0f - ((float)y - nScreenHeight / 2.0f) / (nScreenHeight / 2.0f);
                short nFloorShade = ' ';
                if (b < 0.25) nFloorShade = '#';
                else if (b < 0.5) nFloorShade = 'x';
                else if (b < 0.75) nFloorShade = '.';
                else if (b < 0.9) nFloorShade = '~';
                screen[y * nScreenWidth + x] = nFloorShade;
            }
        }
    }
}

/// Main function initializing screen, handling player input, and rendering the scene.
///
/// @return Program exit status.
int main() {
    wchar_t* screen = new wchar_t[nScreenWidth * nScreenHeight];
    HANDLE hConsole = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    SetConsoleActiveScreenBuffer(hConsole);
    DWORD dwBytesWritten = 0;

    auto tp1 = chrono::system_clock::now();

    while (1) {
        auto tp2 = chrono::system_clock::now();
        chrono::duration<float> elapsedTime = tp2 - tp1;
        tp1 = tp2;
        float fElapsedTime = elapsedTime.count();

        // Player movement controls
        if (GetAsyncKeyState(VK_LEFT) & 0x8000) fPlayerA -= fTurnSpeed * fElapsedTime;
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000) fPlayerA += fTurnSpeed * fElapsedTime;
        if (GetAsyncKeyState((unsigned short)'W') & 0x8000) MovePlayer(fPlayerX, fPlayerY, fPlayerA, fElapsedTime, true);
        if (GetAsyncKeyState((unsigned short)'S') & 0x8000) MovePlayer(fPlayerX, fPlayerY, fPlayerA, fElapsedTime, false);
        if (GetAsyncKeyState((unsigned short)'A') & 0x8000) MovePlayer(fPlayerX, fPlayerY, fPlayerA - 3.14159f / 2, fElapsedTime, true);
        if (GetAsyncKeyState((unsigned short)'D') & 0x8000) MovePlayer(fPlayerX, fPlayerY, fPlayerA + 3.14159f / 2, fElapsedTime, true);

        RenderScene(screen, fElapsedTime);

        swprintf_s(screen, 40, L"X=%3.2f, Y=%3.2f, A=%3.2f FPS=%3.2f ", fPlayerX, fPlayerY, fPlayerA, 1.0f / fElapsedTime);

        // Determine player facing direction for the minimap
        wchar_t playerDirection;
        float angle = fmod(fPlayerA, 2 * 3.14159f);

        if (angle >= -3.14159f / 4 && angle < 3.14159f / 4) {
            playerDirection = 'v';  // Facing down
        }
        else if (angle >= 3.14159f / 4 && angle < 3 * 3.14159f / 4) {
            playerDirection = '>';  // Facing left
        }
        else if (angle >= 3 * 3.14159f / 4 || angle < -3 * 3.14159f / 4) {
            playerDirection = '^';  // Facing up
        }
        else {
            playerDirection = '<';  // Facing right
        }

        // Render the minimap with player direction
        for (int nx = 0; nx < nMapWidth; nx++)
            for (int ny = 0; ny < nMapHeight; ny++)
                screen[(ny + 1) * nScreenWidth + nx] = map[ny * nMapWidth + nx];

        // Place player direction character on the minimap
        screen[((int)fPlayerY + 1) * nScreenWidth + (int)fPlayerX] = playerDirection;

        screen[nScreenWidth * nScreenHeight - 1] = '\0';
        WriteConsoleOutputCharacter(hConsole, screen, nScreenWidth * nScreenHeight, { 0, 0 }, &dwBytesWritten);
    }

    delete[] screen;
    return 0;
}
