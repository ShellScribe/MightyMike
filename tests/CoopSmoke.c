// Real SDL virtual devices + real map, movement, weapons and animation.
// No mocks; enabled only with -DCOOP_SMOKE_TESTS=ON.
#include <stdio.h>
#include <stdlib.h>
#include "myglobals.h"
#include "externs.h"
#include "coop.h"
#include "input.h"
#include "main.h"
#include "myguy.h"
#include "playfield.h"
#include "object.h"
#include "weapon.h"
#include "enemy.h"
#include "misc.h"
#include "bonus.h"
#include "shape.h"
#include "objecttypes.h"
#include "window.h"
#include "io.h"

bool gRunCoopSmokeTests;
static SDL_JoystickID testIDs[2];
static SDL_Joystick* sticks[2];
static void Check(bool success, const char* label)
{
 if (!success) { fprintf(stderr, "FAIL: %s\n", label); fflush(stderr); exit(1); }
 fprintf(stderr, "PASS: %s\n", label);
}
void CoopAttachTestControllers(void)
{
 SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
 int count = 0;
 SDL_JoystickID* existing = SDL_GetGamepads(&count);
 SDL_free(existing);
 Check(count == 0, "test isolation (disconnect physical controllers for smoke tests)");
 for (int p = 0; p < 2; p++)
 {
  SDL_VirtualJoystickDesc desc;
  SDL_zero(desc);
  SDL_INIT_INTERFACE(&desc);
  desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
  desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
  desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
  desc.axis_mask = (1U << SDL_GAMEPAD_AXIS_COUNT) - 1;
  desc.button_mask = (1U << SDL_GAMEPAD_BUTTON_COUNT) - 1;
  desc.name = p == 0 ? "Coop smoke P1" : "Coop smoke P2";
  testIDs[p] = SDL_AttachVirtualJoystick(&desc);
  Check(testIDs[p] != 0, "attach virtual SDL gamepad");
  sticks[p] = SDL_OpenJoystick(testIDs[p]);
  Check(sticks[p] != NULL, "open virtual joystick");
  SDL_SetJoystickVirtualAxis(sticks[p], SDL_GAMEPAD_AXIS_LEFT_TRIGGER, -32768);
  SDL_SetJoystickVirtualAxis(sticks[p], SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, -32768);
 }
}
static void Axis(int p, int axis, int value)
{
 Check(SDL_SetJoystickVirtualAxis(sticks[p], axis, value), "set virtual axis");
}
static void Tick(int count)
{
 for (int i = 0; i < count; i++)
 {
  SDL_UpdateJoysticks();
  UpdateInput();
  gFrames++;
  MoveObjects();
 }
}
static void ResetInput(void)
{
 for (int p = 0; p < 2; p++)
  for (int a = 0; a < SDL_GAMEPAD_AXIS_COUNT; a++)
   SDL_SetJoystickVirtualAxis(sticks[p], a, a >= SDL_GAMEPAD_AXIS_LEFT_TRIGGER ? -32768 : 0);
 Tick(1);
}
static void MoveToArena(void)
{
 // Choose a clear patch of the real first map, avoiding walls and hazards.
 int startX = gMyInitX, startY = gMyInitY;
 int foundX = 0, foundY = 0;
 for (int y = startY-96; y <= startY+192 && !foundX; y += 32)
  for (int x = startX-96; x <= startX+192; x += 32)
  {
   if (x < 128 || y < 128 || x > gPlayfieldWidth-128 || y > gPlayfieldHeight-128) continue;
   bool clear = true;
   for (int dy = -64; dy <= 64; dy += 16)
    for (int dx = -64; dx <= 64; dx += 16)
     if (GetMapTileAttribs(x+dx,y+dy) & (TILE_ATTRIB_ALLSOLID|TILE_ATTRIB_DEATH|TILE_ATTRIB_HURT|TILE_ATTRIB_WATER)) clear = false;
   if (clear) { foundX = x; foundY = y; break; }
  }
 Check(foundX != 0, "clear test patch in first level");
 for (int p = 0; p < 2; p++)
 {
  PlayerState* s = &gCoopPlayers[p];
  s->gMyX = s->gLastNonDeathX = foundX;
  s->gMyY = s->gLastNonDeathY = foundY;
  s->gMyNodePtr->X.Int = foundX;
  s->gMyNodePtr->Y.Int = foundY;
  s->gMyNodePtr->OldX = s->gMyNodePtr->X;
  s->gMyNodePtr->OldY = s->gMyNodePtr->Y;
  s->gMyBlinkieTimer = GAME_FPS*10;
  CalcObjectBox2(s->gMyNodePtr);
 }
 // Restore outer P1 globals without overwriting the newly arranged state.
 gMyX = foundX; gMyY = foundY;
 gLastNonDeathX = foundX; gLastNonDeathY = foundY;
 gMyBlinkieTimer = GAME_FPS*10;
 InitPlayfield();
 gEnemyFreezeTimer = 30000; // controlled movement tests exclude active enemy AI
}
void CoopRunSmokeTests(void)
{
 Check(GetPlayerGamepad(0) && GetPlayerGamepad(1), "two independently assigned gamepads");
 Check(SDL_GetGamepadID(GetPlayerGamepad(0)) == testIDs[0]
  && SDL_GetGamepadID(GetPlayerGamepad(1)) == testIDs[1], "stable controller slots");
 gSceneNum = gStartingScene = 0;
 gAreaNum = 0;
 gPlayerMode = ONE_PLAYER;
 gLoadOldGameFlag = false;
 gDemoMode = DEMO_MODE_OFF;
 InitGame();
 InitArea();
 gIsInGame = true;
 Check(gCoopReady && gCoopPlayers[0].gMyNodePtr != gCoopPlayers[1].gMyNodePtr, "two real Pete objects");
 MoveToArena();
 ResetInput();
 long x0 = gCoopPlayers[0].gMyX, x1 = gCoopPlayers[1].gMyX;
 Axis(0, SDL_GAMEPAD_AXIS_LEFTX, 32767);
 Tick(3);
 Check(gCoopPlayers[0].gMyX > x0 && gCoopPlayers[1].gMyX == x1, "P1 moves while P2 remains still");
 ResetInput(); Tick(10);
 x0 = gCoopPlayers[0].gMyX; x1 = gCoopPlayers[1].gMyX;
 Axis(1, SDL_GAMEPAD_AXIS_LEFTX, -32767);
 Tick(3);
 Check(gCoopPlayers[1].gMyX < x1 && gCoopPlayers[0].gMyX == x0, "P2 moves while P1 remains still");
 ResetInput(); Tick(10);
 Axis(0, SDL_GAMEPAD_AXIS_RIGHTX, 32767);
 Axis(1, SDL_GAMEPAD_AXIS_RIGHTY, 32767);
 Axis(0, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 32767);
 Axis(1, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 32767);
 int ammo[2] = {gCoopPlayers[0].weapons[0].life, gCoopPlayers[1].weapons[0].life};
 Tick(1);
 Check(gCoopPlayers[0].gMyDirection == AIM_RIGHT && gCoopPlayers[1].gMyDirection == AIM_DOWN, "independent simultaneous aiming");
 Check(gCoopPlayers[0].weapons[0].life == ammo[0]-1 && gCoopPlayers[1].weapons[0].life == ammo[1]-1, "both fire with separate ammo");
 bool rightBullet = false, downBullet = false;
 for (ObjNode* n = FirstNodePtr; n; n = n->NextNode)
  if (n->CType & CTYPE_MYBULLET)
  { rightBullet |= n->DX > 0 && n->DY == 0; downBullet |= n->DY > 0 && n->DX == 0; }
 Check(rightBullet && downBullet, "real projectiles retain each shooter's direction");
 // Hit the same real enemy with both players' projectiles through the normal
 // enemy collision path. Freeze movement only; collision logic is unchanged.
 ObjNode* target = NULL;
 for (ObjNode* n = FirstNodePtr; n; n = n->NextNode)
  if (n->CType & CTYPE_ENEMYA) { target = n; break; }
 Check(target != NULL, "real enemy available for combat check");
 target->Health = 10; target->InjuryThreshold = 0;
 long targetHealth = target->Health;
 int shotsTested = 0;
 for (ObjNode* shot = FirstNodePtr; shot && shotsTested < 2; )
 {
  ObjNode* next = shot->NextNode;
  if (shot->CType & CTYPE_MYBULLET)
  {
   CalcObjectBox2(shot);
   target->X = target->OldX = shot->X;
   target->Y = target->OldY = shot->Y;
   target->DX = target->DY = 0;
   CalcObjectBox2(target);
   gThisNodePtr = target;
   CoopBeginObject(target);
   GetObjectInfo();
   DoEnemyCollisionDetect(CTYPE_MYBULLET);
   CoopEndObject();
   shotsTested++;
  }
  shot = next;
 }
 Check(shotsTested == 2 && target->Health <= targetHealth-2, "both players' projectiles damage the shared enemy");
 // Leave the controlled arena unobstructed for the remaining player tests.
 DeleteObject(target);
 gTweenFrameFactor.L = 0x10000;
 gOneMinusTweenFrameFactor.L = 0;
 SortObjectsByY(); ScrollPlayfield(); DrawObjects(); DisplayPlayfield(); CoopDrawOverlay();
 FILE* screenshot = fopen("/private/tmp/mightymike-coop-smoke.ppm", "wb");
 Check(screenshot != NULL, "open smoke screenshot");
 fprintf(screenshot, "P6\n%d %d\n255\n", VISIBLE_WIDTH, VISIBLE_HEIGHT);
 for (int y = 0; y < VISIBLE_HEIGHT; y++)
  for (int x = 0; x < VISIBLE_WIDTH; x++)
  {
   uint32_t c = gGamePalette.finalColors32[gScreenLookUpTable[y][x]];
   Byte rgb[3] = {c >> 24, c >> 16, c >> 8};
   fwrite(rgb, 1, 3, screenshot);
  }
 fclose(screenshot);
 EraseObjects();
 Check(gMyNodePtr == gCoopPlayers[0].gMyNodePtr && gMyDirection == AIM_RIGHT && gCoopActivePlayer == -1, "player context restored after object updates");
 ResetInput(); Tick(12);
 // Damage P2 using the same routine as enemy contact, scoped by dispatcher.
 short health0 = gCoopPlayers[0].gMyHealth;
 CoopBeginObject(gCoopPlayers[1].gMyNodePtr);
 gMyBlinkieTimer = 0; gShieldTimer = 0;
 short before = gMyHealth;
 IGotHurt();
 CoopEndObject();
 Check(gCoopPlayers[1].gMyHealth == before-1 && gCoopPlayers[0].gMyHealth == health0, "damage leaves partner health intact");
 CoopBeginObject(gCoopPlayers[1].gMyNodePtr);
 gMyHealth = 0; gMyBlinkieTimer = 0;
 IGotHurt();
 CoopEndObject();
 int lives = gNumLives;
 Tick(160);
 Check(gNumLives == lives-1 && gCoopPlayers[1].gMyHealth == gMyMaxHealth && !gCoopPlayers[1].doneDead, "death animation and partner revival consume one team spare");
 Check(gCoopPlayers[0].gMyHealth == health0 && !gGlobFlag_MeDoneDead, "survivor remains playable");
 // Tether under sustained opposing movement in the shared camera.
 Axis(0, SDL_GAMEPAD_AXIS_LEFTX, 32767);
 Axis(1, SDL_GAMEPAD_AXIS_LEFTX, -32767);
 Tick(80);
 Check(labs(gCoopPlayers[0].gMyX-gCoopPlayers[1].gMyX) <= PF_WINDOW_WIDTH-128, "shared-camera tether bounds separation");
 ResetInput();
 for (int p = 0; p < 2; p++)
 {
  ObjNode* n = gCoopPlayers[p].gMyNodePtr;
  Check(n->X.Int >= gScrollX+14 && n->X.Int <= gScrollX+PF_WINDOW_WIDTH-14, "player visible in shared camera");
 }
 // Exercise the last-bunny pickup through the real collision/bonus path.
 gNumBunnies = 1;
 CoopBeginObject(gCoopPlayers[1].gMyNodePtr);
 ObjNode* bunny = MakeNewShape(GroupNum_Bunny, ObjType_Bunny, 0, gMyX, gMyY, 50, NULL, PLAYFIELD_RELATIVE);
 Check(bunny != NULL, "spawn final test objective");
 for (int i = 0; i < gNumItems; i++)
  if ((gMasterItemList[i].type & (ITEM_MEMORY|ITEM_NUM)) == 3)
  { bunny->ItemIndex = &gMasterItemList[i]; break; }
 Check(bunny->ItemIndex != NULL, "objective references a real map item");
 MeHitBonusObject(bunny);
 CoopEndObject();
 Tick(1);
 Check(gNumBunnies == 0 && gCoopPlayers[0].gMyNodePtr->SubType == MY_ANIMBASE_LIFTOFF
  && gCoopPlayers[1].gMyNodePtr->SubType == MY_ANIMBASE_LIFTOFF, "either player completes shared bunny objective");
 Tick(300);
 Check(gFinishedArea, "real liftoff animation completes level");
 gIsInGame = false;
 gAreaNum = 1;
 InitArea();
 Check(gCoopPlayers[0].gMyNodePtr->CoopOwner == 1 && gCoopPlayers[1].gMyNodePtr->CoopOwner == 2
  && gNumBunnies > 0 && !gFinishedArea, "next level creates both players and fresh objectives");
 // Test game-over with no team spares, without entering high-score UI.
 gIsInGame = true;
 gNumLives = 1;
 for (int p = 0; p < 2; p++)
 {
  CoopBeginObject(gCoopPlayers[p].gMyNodePtr);
  gMyHealth = 0; gMyBlinkieTimer = gShieldTimer = 0;
  IGotHurt();
  CoopEndObject();
 }
 Tick(160);
 Check(gGlobFlag_MeDoneDead, "game ends only when both die without a spare");
 gIsInGame = false;
 SDL_CloseJoystick(sticks[0]);
 SDL_DetachVirtualJoystick(testIDs[0]);
 SDL_UpdateJoysticks(); UpdateInput();
 Check(!GetPlayerGamepad(0) && SDL_GetGamepadID(GetPlayerGamepad(1)) == testIDs[1], "disconnect P1 keeps P2 assigned to P2");
 SDL_VirtualJoystickDesc reconnect;
 SDL_zero(reconnect); SDL_INIT_INTERFACE(&reconnect);
 reconnect.type = SDL_JOYSTICK_TYPE_GAMEPAD;
 reconnect.naxes = SDL_GAMEPAD_AXIS_COUNT;
 reconnect.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
 reconnect.axis_mask = (1U << SDL_GAMEPAD_AXIS_COUNT) - 1;
 reconnect.button_mask = (1U << SDL_GAMEPAD_BUTTON_COUNT) - 1;
 reconnect.name = "Coop smoke P1 reconnected";
 testIDs[0] = SDL_AttachVirtualJoystick(&reconnect);
 sticks[0] = SDL_OpenJoystick(testIDs[0]);
 SDL_SetJoystickVirtualAxis(sticks[0], SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, -32768);
 SDL_SetJoystickVirtualAxis(sticks[0], SDL_GAMEPAD_AXIS_LEFT_TRIGGER, -32768);
 SDL_UpdateJoysticks(); UpdateInput();
 Check(GetPlayerGamepad(0) && SDL_GetGamepadID(GetPlayerGamepad(0)) == testIDs[0]
  && SDL_GetGamepadID(GetPlayerGamepad(1)) == testIDs[1], "reconnect fills vacant slot without reassigning partner");
 // Original single-player startup and movement use the untouched legacy route.
 gCoopEnabled = false;
 gSceneNum = gAreaNum = 0;
 InitGame(); InitArea();
 gIsInGame = true;
 ResetInput();
 int peteCount = 0;
 for (ObjNode* n = FirstNodePtr; n; n = n->NextNode) if (n->CType == CTYPE_MYGUY) peteCount++;
 Check(peteCount == 1 && !gCoopReady, "original single-player starts with one Pete");
 long oldX = gMyX;
 gMyBlinkieTimer = GAME_FPS*10;
 Axis(0, SDL_GAMEPAD_AXIS_LEFTX, 32767);
 Tick(3);
 Check(gMyX > oldX, "original single-player movement");
 gIsInGame = false;
 for (int p = 0; p < 2; p++) { SDL_CloseJoystick(sticks[p]); SDL_DetachVirtualJoystick(testIDs[p]); }
 fprintf(stderr, "COOP SMOKE PASSED (SDL virtual controllers; human play unverified)\n");
}
