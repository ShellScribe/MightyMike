/****************************/
/*   	  INPUT.C	   	    */
/* By Brian Greenstone      */
/* (c)1997-99 Pangea Software  */
/* (c)2022 Iliyas Jorio     */
/****************************/


/***************/
/* EXTERNALS   */
/***************/

#include <Pomme.h>
#include <SDL3/SDL.h>
#include <math.h>

#include "misc.h"
#include "input.h"
#include "window.h"
#include "structures.h"
#include "externs.h"
#include "coop.h"

/**********************/
/*     PROTOTYPES     */
/**********************/

SDL_Gamepad*		gSDLGamepad = NULL;
SDL_Haptic*			gSDLHaptic = NULL;

Byte				gRawKeyboardState[SDL_SCANCODE_COUNT];
char				gTextInput[64];

Byte				gNeedStates[NUM_CONTROL_NEEDS];

PlayerInput gPlayerInputs[2];
static SDL_Gamepad* gSecondGamepad;
static Byte gSecondNeedStates[NUM_CONTROL_NEEDS];
static int gInputPlayer = -1;
static int32_t ReadStickMagnitude(SDL_Gamepad* pad);
static short ReadStickAim(SDL_Gamepad* pad);
void SetInputPlayer(int player) { gInputPlayer = player; }
SDL_Gamepad* GetPlayerGamepad(int player) { return player == 0 ? gSDLGamepad : gSecondGamepad; }

static bool ReadGamepadNeed(SDL_Gamepad* pad, int need)
{
 if (!pad) return false;
 const KeyBinding* kb = &gGamePrefs.keys[need];
 int deadZone = need >= NUM_REMAPPABLE_NEEDS ? 21626 : 10813;
 bool down = false;
 for (int j = 0; j < KEYBINDING_MAX_GAMEPAD_BUTTONS; j++)
 {
  int id = kb->gamepad[j].id;
  switch (kb->gamepad[j].type)
  {
   case kButton: down |= SDL_GetGamepadButton(pad, id); break;
   case kAxisPlus: down |= SDL_GetGamepadAxis(pad, id) > deadZone; break;
   case kAxisMinus: down |= SDL_GetGamepadAxis(pad, id) < -deadZone; break;
  }
 }
 return down;
}

static void ParseAltEnter(void);
static void OnJoystickRemoved(SDL_JoystickID which);

/****************************/
/*    CONSTANTS             */
/****************************/

enum
{
	KEYSTATE_ACTIVE_BIT		= 0b001,
	KEYSTATE_CHANGE_BIT		= 0b010,
	KEYSTATE_IGNORE_BIT		= 0b100,

	KEYSTATE_OFF			= 0b000,
	KEYSTATE_DOWN			= KEYSTATE_ACTIVE_BIT | KEYSTATE_CHANGE_BIT,
	KEYSTATE_HELD			= KEYSTATE_ACTIVE_BIT,
	KEYSTATE_UP				= KEYSTATE_OFF | KEYSTATE_CHANGE_BIT,
	KEYSTATE_IGNOREHELD		= KEYSTATE_OFF | KEYSTATE_IGNORE_BIT,
};

const int16_t kJoystickDeadZone		= (33 * 32767 / 100);
const int16_t kJoystickDeadZone_UI	= (66 * 32767 / 100);


/**********************/
/*     VARIABLES      */
/**********************/


/**********************/
/* STATIC FUNCTIONS   */
/**********************/

static inline void UpdateKeyState(Byte* state, bool downNow)
{
	switch (*state)	// look at prev state
	{
		case KEYSTATE_HELD:
		case KEYSTATE_DOWN:
			*state = downNow ? KEYSTATE_HELD : KEYSTATE_UP;
			break;

		case KEYSTATE_OFF:
		case KEYSTATE_UP:
		default:
			*state = downNow ? KEYSTATE_DOWN : KEYSTATE_OFF;
			break;

		case KEYSTATE_IGNOREHELD:
			*state = downNow ? KEYSTATE_IGNOREHELD : KEYSTATE_OFF;
			break;
	}
}

/************************* INIT INPUT *********************************/

void InitInput(void)
{
}


void UpdateInput(void)
{

	gTextInput[0] = '\0';


	/**********************/
	/* DO SDL MAINTENANCE */
	/**********************/

	int mouseWheelDelta = 0;

	SDL_PumpEvents();
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
		case SDL_EVENT_QUIT:
			CleanQuit();
			return;

		case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			CleanQuit();
			return;

		case SDL_EVENT_WINDOW_RESIZED:
			OnChangeIntegerScaling();
			break;

		case SDL_EVENT_TEXT_INPUT:
			SDL_snprintf(gTextInput, sizeof(gTextInput), "%s", event.text.text);
			break;

		case SDL_EVENT_GAMEPAD_ADDED:
			TryOpenGamepad(false);
			break;

		case SDL_EVENT_GAMEPAD_REMOVED:
			OnJoystickRemoved(event.gdevice.which);
			break;

		case SDL_EVENT_MOUSE_WHEEL:
			mouseWheelDelta += event.wheel.y;
			mouseWheelDelta += event.wheel.x;
			break;
		}
	}

	// --------------------------------------------
	// Update raw keyboard state

	int numkeys = 0;
	const bool* keystate = SDL_GetKeyboardState(&numkeys);
	uint32_t mouseButtons = SDL_GetMouseState(NULL, NULL);

	{
		int minNumKeys = numkeys < SDL_SCANCODE_COUNT ? numkeys : SDL_SCANCODE_COUNT;

		for (int i = 0; i < minNumKeys; i++)
		{
			UpdateKeyState(&gRawKeyboardState[i], keystate[i]);
		}

		// fill out the rest
		for (int i = minNumKeys; i < SDL_SCANCODE_COUNT; i++)
		{
			UpdateKeyState(&gRawKeyboardState[i], false);
		}
	}

	// --------------------------------------------
	// Parse system key chords

	ParseAltEnter();

	if ((!gIsInGame || gIsGamePaused) && IsCmdQPressed())
	{
		CleanQuit();
	}

	// --------------------------------------------
	// Update needs

	for (int i = 0; i < NUM_CONTROL_NEEDS; i++)
	{
		const KeyBinding* kb = &gGamePrefs.keys[i];

		bool downNow = false;

		for (int j = 0; j < KEYBINDING_MAX_KEYS; j++)
			if (kb->key[j] && kb->key[j] < numkeys)
				downNow |= KEYSTATE_ACTIVE_BIT & gRawKeyboardState[kb->key[j]];

		switch (kb->mouse.type)
		{
			case kButton:
				downNow |= KEYSTATE_ACTIVE_BIT & (mouseButtons & SDL_BUTTON_MASK(kb->mouse.id));
				break;

			case kAxisPlus:
				downNow |= mouseWheelDelta > 0;
				break;

			case kAxisMinus:
				downNow |= mouseWheelDelta < 0;
				break;

			default:
				break;
		}

		if (gSDLGamepad)
		{
			int16_t deadZone = i >= NUM_REMAPPABLE_NEEDS
								? kJoystickDeadZone_UI
								: kJoystickDeadZone;

			for (int j = 0; j < KEYBINDING_MAX_GAMEPAD_BUTTONS; j++)
			{
				switch (kb->gamepad[j].type)
				{
					case kButton:
						downNow |= KEYSTATE_ACTIVE_BIT & SDL_GetGamepadButton(gSDLGamepad, kb->gamepad[j].id);
						break;

					case kAxisPlus:
						downNow |= SDL_GetGamepadAxis(gSDLGamepad, kb->gamepad[j].id) > deadZone;
						break;

					case kAxisMinus:
						downNow |= SDL_GetGamepadAxis(gSDLGamepad, kb->gamepad[j].id) < -deadZone;
						break;

					default:
						break;
				}
			}
		}

		UpdateKeyState(&gNeedStates[i], downNow);
		UpdateKeyState(&gSecondNeedStates[i], ReadGamepadNeed(gSecondGamepad, i));
		gPlayerInputs[0].held[i] = (gNeedStates[i] & KEYSTATE_ACTIVE_BIT) != 0;
		gPlayerInputs[0].pressed[i] = gNeedStates[i] == KEYSTATE_DOWN;
		gPlayerInputs[1].held[i] = (gSecondNeedStates[i] & KEYSTATE_ACTIVE_BIT) != 0;
		gPlayerInputs[1].pressed[i] = gSecondNeedStates[i] == KEYSTATE_DOWN;
	}
	for (int p = 0; p < 2; p++)
	{
		gPlayerInputs[p].moveMagnitude = ReadStickMagnitude(GetPlayerGamepad(p));
		gPlayerInputs[p].aim = ReadStickAim(GetPlayerGamepad(p));
	}
}

void ClearInput(void)
{
	SDL_memset(gRawKeyboardState, KEYSTATE_HELD, sizeof(gRawKeyboardState));
	SDL_memset(gNeedStates, KEYSTATE_HELD, sizeof(gNeedStates));
	SDL_memset(gSecondNeedStates, KEYSTATE_HELD, sizeof(gSecondNeedStates));
	SDL_memset(gPlayerInputs, 0, sizeof(gPlayerInputs));
//	ClearMouseState();
//	EatMouseEvents();
}

static void ParseAltEnter(void)
{
	if ((GetSDLKeyState(SDL_SCANCODE_LALT) || GetSDLKeyState(SDL_SCANCODE_RALT))
		&& GetNewSDLKeyState(SDL_SCANCODE_RETURN))
	{
		gRawKeyboardState[SDL_SCANCODE_LALT] = KEYSTATE_IGNOREHELD;
		gRawKeyboardState[SDL_SCANCODE_RALT] = KEYSTATE_IGNOREHELD;
		gRawKeyboardState[SDL_SCANCODE_RETURN] = KEYSTATE_IGNOREHELD;

#if 0
		gGamePrefs.displayMode++;
		gGamePrefs.displayMode %= kDisplayMode_COUNT;
#else
		static int previousFullscreenSetting = kDisplayMode_FullscreenStretched;

		if (gGamePrefs.displayMode == kDisplayMode_Windowed)
		{
			gGamePrefs.displayMode = previousFullscreenSetting;
		}
		else
		{
			previousFullscreenSetting = gGamePrefs.displayMode;
			gGamePrefs.displayMode = kDisplayMode_Windowed;
		}
#endif

		SetFullscreenMode(false);
	}
}

/************************ GET SKIP KEY STATE ***************************/

bool UserWantsOut(void)
{
	return GetNewNeedState(kNeed_UIConfirm) || GetNewNeedState(kNeed_UIBack);
}

bool UserWantsOutContinuous(void)
{
	return GetNeedState(kNeed_UIConfirm) || GetNeedState(kNeed_UIBack);
}

#pragma mark -


bool GetNewSDLKeyState(SDL_Scancode sdlScanCode)
{
	return gRawKeyboardState[sdlScanCode] == KEYSTATE_DOWN;
}

bool GetSDLKeyState(SDL_Scancode sdlScanCode)
{
	return gRawKeyboardState[sdlScanCode] == KEYSTATE_DOWN || gRawKeyboardState[sdlScanCode] == KEYSTATE_HELD;
}

bool GetNeedState(int needID)
{
	GAME_ASSERT(needID < NUM_CONTROL_NEEDS);
	if (gInputPlayer >= 0) return gPlayerInputs[gInputPlayer].held[needID];
	if (gCoopEnabled && needID >= NUM_REMAPPABLE_NEEDS)
		return (gNeedStates[needID] | gSecondNeedStates[needID]) & KEYSTATE_ACTIVE_BIT;
	return 0 != (gNeedStates[needID] & KEYSTATE_ACTIVE_BIT);
}

bool GetNewNeedState(int needID)
{
	GAME_ASSERT(needID < NUM_CONTROL_NEEDS);
	if (gInputPlayer >= 0) return gPlayerInputs[gInputPlayer].pressed[needID];
	if (gCoopEnabled && (needID >= NUM_REMAPPABLE_NEEDS || needID == kNeed_Radar))
		return gNeedStates[needID] == KEYSTATE_DOWN || gSecondNeedStates[needID] == KEYSTATE_DOWN;
	return gNeedStates[needID] == KEYSTATE_DOWN;
}

bool IsCmdQPressed(void)
{
#if __APPLE__
	return (GetSDLKeyState(SDL_SCANCODE_LGUI) || GetSDLKeyState(SDL_SCANCODE_RGUI))
		&& GetNewSDLKeyState(SDL_GetScancodeFromKey(SDLK_Q, NULL));
#else
	// on non-mac systems, alt-f4 is handled by the system
	return false;
#endif
}


#pragma mark -

/****************************** SDL JOYSTICK FUNCTIONS ********************************/

SDL_Gamepad* TryOpenGamepad(bool showMessage)
{
 (void) showMessage;
 int count = 0;
 SDL_JoystickID* ids = SDL_GetGamepads(&count);
 for (int i = 0; i < count; i++)
 {
  if ((gSDLGamepad && SDL_GetGamepadID(gSDLGamepad) == ids[i])
   || (gSecondGamepad && SDL_GetGamepadID(gSecondGamepad) == ids[i])) continue;
  SDL_Gamepad** slot = !gSDLGamepad ? &gSDLGamepad : gCoopEnabled && !gSecondGamepad ? &gSecondGamepad : NULL;
  if (!slot) break;
  *slot = SDL_OpenGamepad(ids[i]);
  if (*slot) SDL_Log("P%d controller: %s", slot == &gSDLGamepad ? 1 : 2, SDL_GetGamepadName(*slot));
 }
 SDL_free(ids);
 return gSDLGamepad;
}

static void OnJoystickRemoved(SDL_JoystickID which)
{
 for (int p = 0; p < 2; p++)
 {
  SDL_Gamepad** slot = p == 0 ? &gSDLGamepad : &gSecondGamepad;
  if (!*slot || SDL_GetGamepadID(*slot) != which) continue;
  SDL_CloseGamepad(*slot);
  *slot = NULL;
  SDL_memset(&gPlayerInputs[p], 0, sizeof(PlayerInput));
  if (p == 0 && gSDLHaptic) { SDL_CloseHaptic(gSDLHaptic); gSDLHaptic = NULL; }
 }
 if (!gCoopEnabled && !gSDLGamepad && gSecondGamepad)
 { gSDLGamepad = gSecondGamepad; gSecondGamepad = NULL; }
 TryOpenGamepad(false); // Existing co-op slots never migrate to the other player.
}

int32_t GetLeftStickMagnitude_Fix32(void)
{
 return gInputPlayer >= 0 ? gPlayerInputs[gInputPlayer].moveMagnitude : ReadStickMagnitude(gSDLGamepad);
}

short GetRightStick8WayAim(void)
{
 return gInputPlayer >= 0 ? gPlayerInputs[gInputPlayer].aim : ReadStickAim(gSDLGamepad);
}

static int32_t ReadStickMagnitude(SDL_Gamepad* pad)
{
	if (!pad)
	{
		return 0;
	}

	int dxRaw = (int) SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
	int dyRaw = (int) SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY);

	int64_t magnitudeSquared = (int64_t)dxRaw * dxRaw + (int64_t)dyRaw * dyRaw;

	if (magnitudeSquared < kJoystickDeadZone * kJoystickDeadZone)
	{
		return 0;
	}

	return (int32_t)(0x10000 * sqrtf((float)magnitudeSquared) / 32767.0f);
}

static short ReadStickAim(SDL_Gamepad* pad)
{
	if (!pad)
	{
		return AIM_NONE;
	}

	int dxRaw = (int) SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX);
	int dyRaw = (int) SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY);

	bool right	= dxRaw > kJoystickDeadZone;
	bool left	= dxRaw < -kJoystickDeadZone;
	bool down	= dyRaw > kJoystickDeadZone;
	bool up		= dyRaw < -kJoystickDeadZone;

	if (down)
	{
		if (right)		return AIM_DOWN_RIGHT;
		else if (left)	return AIM_DOWN_LEFT;
		else			return AIM_DOWN;
	}
	else if (up)
	{
		if (right)		return AIM_UP_RIGHT;
		else if (left)	return AIM_UP_LEFT;
		else			return AIM_UP;
	}
	else
	{
		if (right)		return AIM_RIGHT;
		else if (left)	return AIM_LEFT;
		else			return AIM_NONE;
	}
}
