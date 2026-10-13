//
// input.h
//

#pragma once
#include <SDL3/SDL.h>
#include "structures.h"

// Sampled once per simulation tick. Gameplay consumes this, not SDL devices.
typedef struct PlayerInput
{
 bool held[NUM_CONTROL_NEEDS];
 bool pressed[NUM_CONTROL_NEEDS];
 int32_t moveMagnitude;
 short aim;
} PlayerInput;
extern PlayerInput gPlayerInputs[2];
void SetInputPlayer(int player); // -1 restores menu/legacy input
SDL_Gamepad* GetPlayerGamepad(int player);

void InitInput(void);
void UpdateInput(void);
void ClearInput(void);

bool GetNewSDLKeyState(SDL_Scancode sdlScanCode);
bool GetSDLKeyState(SDL_Scancode sdlScanCode);
bool UserWantsOut(void);
bool UserWantsOutContinuous(void);
bool IsCmdQPressed(void);
bool GetNewNeedState(int needID);
bool GetNeedState(int needID);
SDL_Gamepad* TryOpenGamepad(bool showMessage);
int32_t GetLeftStickMagnitude_Fix32(void);
short GetRightStick8WayAim(void);
