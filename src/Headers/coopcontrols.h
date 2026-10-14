#pragma once
#include "input.h"

enum { COOP_MOVE_UP, COOP_MOVE_DOWN, COOP_MOVE_LEFT, COOP_MOVE_RIGHT,
 COOP_AIM_UP, COOP_AIM_DOWN, COOP_AIM_LEFT, COOP_AIM_RIGHT,
 COOP_SHOOT, COOP_PREV, COOP_NEXT, COOP_RADAR, COOP_PAUSE, COOP_ACTIONS };
// Defaults use SDL's verified gamepad mapping. Captures use raw joystick inputs.
enum { COOP_UNBOUND, COOP_PAD_BUTTON, COOP_PAD_AXIS, COOP_RAW_BUTTON, COOP_RAW_AXIS, COOP_RAW_HAT };
typedef struct CoopBinding { int type, id, direction, rest; } CoopBinding;
typedef struct CoopProfile { char guid[33]; CoopBinding binding[COOP_ACTIONS][2]; } CoopProfile;
extern CoopProfile gCoopProfiles[2];
void CoopSyncControllers(void);
SDL_JoystickID CoopControllerID(int player);
void CoopAssignController(int player, SDL_JoystickID id);
bool CoopReadNeed(int player, int need);
short CoopReadAim(int player);
void CoopResetBindings(int player);
void CoopRequireReady(void);
bool CoopControllersNeedSetup(void);
bool CoopControllerGate(void);
void CoopSetupScreen(bool required);
bool CoopSaveBindings(void);
void CoopDrawText(int x, int y, const char* text, Byte color);
void CoopDrawBox(int x, int y, int w, int h, Byte color);
void CoopPaletteColors(Byte* light, Byte* dark);
#ifdef COOP_SMOKE_TESTS
void CoopTestReady(int player);
#endif
