// Real SDL virtual devices + real map, movement, weapons and animation.
// No mocks; enabled only with -DCOOP_SMOKE_TESTS=ON.
#include <stdio.h>
#include <stdlib.h>
#include "myglobals.h"
#include "objecttypes.h"
#include "font.h"
#include "externs.h"
#include "coop.h"
#include "coopcontrols.h"
#include "myguy2.h"
#include "triggers.h"
#include "cinema.h"
#include "infobar.h"
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
bool gRunCoopSetupSmoke;
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
// Test-only bridge: keyboard F5/F6 operate the two virtual Start buttons;
// F7 presses virtual P1 shoot and F8 moves its raw X axis. UI capture still
// reads real SDL joystick state through the production path.
void CoopSetupSmokeInput(void)
{
 const bool* keys=SDL_GetKeyboardState(NULL);
 for (int p=0;p<2;p++) SDL_SetJoystickVirtualButton(sticks[p],SDL_GAMEPAD_BUTTON_START,keys[p==0 ? SDL_SCANCODE_F5 : SDL_SCANCODE_F6]);
 SDL_SetJoystickVirtualButton(sticks[0],SDL_GAMEPAD_BUTTON_WEST,keys[SDL_SCANCODE_F7]);
 SDL_SetJoystickVirtualAxis(sticks[0],SDL_GAMEPAD_AXIS_LEFTX,keys[SDL_SCANCODE_F8] ? 32767 : 0);
 SDL_UpdateJoysticks();
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
// Keep the survivor inside the viewport, but place a dead partner beyond the
// old tether radius. Test both player orders without requiring a long map walk.
static void CheckDeadPartnerTether(int deadPlayer)
{
 int survivor = deadPlayer ^ 1;
 PlayerState saved = gCoopPlayers[deadPlayer];
 ObjNode* corpse = saved.gMyNodePtr;
 MikeFixed oldX = corpse->X, oldY = corpse->Y;
 long oldAnim = corpse->SubType;
 long scrollX = gScrollX, scrollY = gScrollY;
 ObjNode* live = gCoopPlayers[survivor].gMyNodePtr;
 CoopBeginObject(corpse);
 gMyHealth = -1;
 gCoopPlayers[deadPlayer].doneDead = true;
 corpse->SubType = MY_ANIMBASE_DIE;
 corpse->X.Int = gMyX = live->X.Int - PF_WINDOW_WIDTH;
 corpse->Y.Int = gMyY = live->Y.Int - PF_WINDOW_HEIGHT;
 CoopEndObject();
 gScrollX = live->X.Int - PF_WINDOW_WIDTH / 2;
 gScrollY = live->Y.Int - PF_WINDOW_HEIGHT / 2;
 gThisNodePtr = live;
 CoopBeginObject(live);
 GetObjectInfo();
 gX.L += 0x10000;
 gY.L += 0x10000;
 CoopLimitMove();
 Check(gX.Int == live->X.Int + 1 && gY.Int == live->Y.Int + 1,
  deadPlayer == 0 ? "P2 can move beyond dead P1's tether" : "P1 can move beyond dead P2's tether");
 CoopEndObject();
 long centerX, centerY;
 CoopCameraCenter(&centerX, &centerY);
 Check(centerX == live->X.Int && centerY == live->Y.Int, "camera follows the sole survivor");
 CoopBeginObject(corpse);
 corpse->X = oldX; corpse->Y = oldY; corpse->SubType = oldAnim;
 gMyHealth = saved.gMyHealth;
 gMyX = saved.gMyX; gMyY = saved.gMyY;
 gCoopPlayers[deadPlayer].doneDead = saved.doneDead;
 CoopEndObject();
 gScrollX = scrollX; gScrollY = scrollY;
}

static void CheckPermanentDeath(int p)
{
 int oldLives=gNumLives;
 gNumLives=1; // display has zero shared spares
 CoopBeginObject(gCoopPlayers[p].gMyNodePtr);
 gMyHealth=0; gMyBlinkieTimer=gShieldTimer=0;
 IGotHurt(); CoopEndObject(); Tick(160);
 Check(gCoopPlayers[p].doneDead && !gGlobFlag_MeDoneDead,"permanent death leaves the partner playable with zero spares");
 CheckDeadPartnerTether(p);
 int survivor=p^1;
 long x=gCoopPlayers[survivor].gMyX;
 Axis(survivor,SDL_GAMEPAD_AXIS_LEFTX,32767); Tick(3); ResetInput();
 Check(gCoopPlayers[survivor].gMyX>x,"actual sole survivor moves after permanent death");
 int freeze=gEnemyFreezeTimer; Tick(2);
 Check(gEnemyFreezeTimer==freeze-2,"shared freeze timer advances with either sole survivor");
 gNumLives=2; Tick(1); gNumLives=oldLives;
 Check(!gCoopPlayers[p].doneDead && gCoopPlayers[p].revivedTicks>0,"later shared spare revives the fallen player");
}
static void CheckCoopBindings(void)
{
 CoopProfile old[2]; SDL_memcpy(old,gCoopProfiles,sizeof(old));
 KeyBinding legacy[NUM_CONTROL_NEEDS]; SDL_memcpy(legacy,gGamePrefs.keys,sizeof(legacy));
 Check(CoopControllersNeedSetup(),"startup requires readiness before the first enemy step");
 CoopTestReady(0); Check(CoopControllersNeedSetup(),"one ready player cannot start the team");
 CoopTestReady(1); Check(!CoopControllersNeedSetup(),"both players unlock the readiness gate");
 gCoopProfiles[1].binding[COOP_SHOOT][0]=(CoopBinding){COOP_RAW_BUTTON,SDL_GAMEPAD_BUTTON_SOUTH,0,0};
 gCoopProfiles[1].binding[COOP_SHOOT][1]=(CoopBinding){0};
 gCoopProfiles[1].binding[COOP_AIM_UP][0]=(CoopBinding){COOP_RAW_BUTTON,SDL_GAMEPAD_BUTTON_NORTH,0,0};
 gCoopProfiles[1].binding[COOP_AIM_RIGHT][0]=(CoopBinding){COOP_RAW_BUTTON,SDL_GAMEPAD_BUTTON_EAST,0,0};
 SDL_SetJoystickVirtualButton(sticks[1],SDL_GAMEPAD_BUTTON_SOUTH,true);
 SDL_SetJoystickVirtualButton(sticks[1],SDL_GAMEPAD_BUTTON_NORTH,true);
 SDL_SetJoystickVirtualButton(sticks[1],SDL_GAMEPAD_BUTTON_EAST,true);
 SDL_UpdateJoysticks(); UpdateInput();
 Check(gPlayerInputs[1].pressed[kNeed_Attack] && !gPlayerInputs[0].held[kNeed_Attack],"captured P2 shoot binding is independent of P1");
 Check(gPlayerInputs[1].aim==AIM_UP_RIGHT && gPlayerInputs[0].aim==AIM_NONE,"button aiming combines diagonals without a right stick");
 ClearInput(); UpdateInput();
 Check(!gPlayerInputs[1].held[kNeed_Attack] && gPlayerInputs[1].aim==AIM_NONE,"held buttons and aim are suppressed after pause");
 for (int b=0;b<SDL_GAMEPAD_BUTTON_COUNT;b++) SDL_SetJoystickVirtualButton(sticks[1],b,false);
 SDL_UpdateJoysticks(); UpdateInput();
 SDL_SetJoystickVirtualButton(sticks[1],SDL_GAMEPAD_BUTTON_SOUTH,true);
 SDL_UpdateJoysticks(); UpdateInput();
 Check(gPlayerInputs[1].pressed[kNeed_Attack],"release rearms captured inputs");
 Check(SDL_memcmp(legacy,gGamePrefs.keys,sizeof(legacy))==0,"co-op remapping leaves legacy single-player bindings unchanged");
 Check(CoopSaveBindings(),"co-op profiles save to isolated test preferences");
 CoopProfile persisted=gCoopProfiles[1]; CoopResetBindings(1); CoopTestReloadBindings();
 Check(SDL_memcmp(&persisted,&gCoopProfiles[1],sizeof(persisted))==0,"co-op binding file round trip preserves both profiles");
 SDL_memcpy(gCoopProfiles,old,sizeof(old));
 for (int b=0;b<SDL_GAMEPAD_BUTTON_COUNT;b++) SDL_SetJoystickVirtualButton(sticks[1],b,false);
 SDL_UpdateJoysticks(); UpdateInput();
}
static void CheckUnmappedDevice(void)
{
 SDL_JoystickID survivor=CoopControllerID(0);
 SDL_VirtualJoystickDesc desc; SDL_zero(desc); SDL_INIT_INTERFACE(&desc);
 desc.type=SDL_JOYSTICK_TYPE_UNKNOWN; desc.naxes=2; desc.nbuttons=9; desc.nhats=1; desc.name="Unmapped co-op calibration fixture";
 SDL_JoystickID rawID=SDL_AttachVirtualJoystick(&desc); SDL_Joystick* raw=SDL_OpenJoystick(rawID);
 Check(raw && !SDL_IsGamepad(rawID),"unmapped raw joystick is available without a database mapping");
 CoopAssignController(1,rawID); SDL_UpdateJoysticks(); UpdateInput();
 Check(!GetPlayerGamepad(1) && CoopControllerID(0)==survivor && CoopControllersNeedSetup(),"unmapped device assignment preserves survivor and requires calibration");
 CoopResetBindings(1);
 for (int a=0;a<4;a++) gCoopProfiles[1].binding[a][0]=(CoopBinding){COOP_RAW_AXIS,a<2 ? 1 : 0,a%2 ? 1 : -1,0};
 int directions[]={SDL_HAT_UP,SDL_HAT_DOWN,SDL_HAT_LEFT,SDL_HAT_RIGHT};
 for (int a=0;a<4;a++) gCoopProfiles[1].binding[a][1]=(CoopBinding){COOP_RAW_HAT,0,directions[a],0};
 for (int a=0;a<4;a++) gCoopProfiles[1].binding[COOP_AIM_UP+a][0]=(CoopBinding){COOP_RAW_BUTTON,5+a,0,0};
 for (int a=0;a<5;a++) gCoopProfiles[1].binding[COOP_SHOOT+a][0]=(CoopBinding){COOP_RAW_BUTTON,a,0,0};
 SDL_SetJoystickVirtualAxis(raw,0,0); SDL_UpdateJoysticks(); UpdateInput();
 SDL_SetJoystickVirtualAxis(raw,0,32767); SDL_SetJoystickVirtualButton(raw,5,true); SDL_SetJoystickVirtualButton(raw,8,true);
 SDL_SetJoystickVirtualButton(raw,0,true); SDL_SetJoystickVirtualButton(raw,3,true); SDL_SetJoystickVirtualButton(raw,4,true);
 SDL_UpdateJoysticks(); UpdateInput();
 Check(gPlayerInputs[1].held[kNeed_Right] && gPlayerInputs[1].aim==AIM_UP_RIGHT,"unmapped raw axis movement and N64-style button aiming");
 Check(gPlayerInputs[1].pressed[kNeed_Attack] && gPlayerInputs[1].pressed[kNeed_Radar] && gPlayerInputs[1].pressed[kNeed_UIPause],"unmapped shooting radar and pause use captured raw buttons");
 SDL_SetJoystickVirtualAxis(raw,0,0); SDL_SetJoystickVirtualHat(raw,0,SDL_HAT_DOWN); SDL_UpdateJoysticks(); UpdateInput();
 Check(gPlayerInputs[1].held[kNeed_Down] && !gPlayerInputs[1].held[kNeed_Right],"captured hat movement is independent of axis movement");
 CoopTestReady(0); CoopTestReady(1); Check(!CoopControllersNeedSetup(),"calibrated raw device passes the ready gate");
 SDL_CloseJoystick(raw); SDL_DetachVirtualJoystick(rawID); SDL_UpdateJoysticks(); UpdateInput();
 Check(CoopControllersNeedSetup() && CoopControllerID(0)==survivor && !gPlayerInputs[1].held[kNeed_Attack] && gPlayerInputs[1].aim==AIM_NONE,"raw disconnect clears stale input and pauses without moving the survivor");
 Check(CoopControllerID(1)==testIDs[1],"another connected device can fill the vacant co-op slot");
 CoopResetBindings(1); SDL_UpdateJoysticks(); UpdateInput();
}

static void CheckCampaignMechanics(void)
{
 bool teleported=false, platformTested=false;
 for (int scene=0;scene<5;scene++) for (int area=0;area<3;area++)
 {
  gIsInGame=false; gSceneNum=scene; gAreaNum=area; InitArea(); gIsInGame=true;
  Check(gNumBunnies>0 && gCoopPlayers[0].gMyNodePtr->CoopOwner==1 && gCoopPlayers[1].gMyNodePtr->CoopOwner==2,"campaign area loads two owned avatars and shared objectives");
  gEnemyFreezeTimer=30000;
  if (scene==SCENE_FAIRY || scene==SCENE_BARGAIN)
  {
   ObjNode* real[2]={gCoopPlayers[0].gMyNodePtr,gCoopPlayers[1].gMyNodePtr};
   for (int p=0;p<2;p++)
   {
    CoopBeginObject(real[p]);
    if (scene==SCENE_FAIRY) TurnMeIntoFrog();
    else
    {
     ObjNode* ship=MakeNewShape(GroupNum_SpaceShip,ObjType_SpaceShip,0,gMyX,gMyY,100,NULL,PLAYFIELD_RELATIVE);
     Check(ship!=NULL,"ship fixture uses real campaign artwork"); TurnMeIntoShip(ship);
    }
    CoopEndObject();
    Check(gCoopPlayers[p].gMyNodePtr!=real[p] && gCoopPlayers[p].gMyNodePtr->CoopOwner==p+1,"transformed avatar retains its owner");
   }
   Tick(3);
   for (int p=0;p<2;p++)
    Check(gCoopPlayers[p].gRealMePtr==real[p] && !real[p]->MoveFlag,"simultaneous transformations retain independent hidden avatars");
  }
  if (!teleported)
  {
   ObjectEntryType* source=NULL;
   for (int i=0;i<gNumItems;i++)
    if ((gMasterItemList[i].type & ITEM_NUM)==PF_OBJ_NUM_TELEPORT && gMasterItemList[i].parm[0]==0) { source=&gMasterItemList[i]; break; }
   if (source)
   {
    Check(AddTeleport(source),"create real campaign teleporter trigger"); ObjNode* trigger=gMostRecentShape;
    // Teleporters are non-sprite nodes; find the item-backed trigger directly.
    for (ObjNode* n=FirstNodePtr;n;n=n->NextNode) if (n->ItemIndex==source && (n->CType&CTYPE_TRIGGER)) { trigger=n; break; }
    ObjNode* frog=gCoopPlayers[1].gMyNodePtr;
    gThisNodePtr=frog; CoopBeginObject(frog); GetObjectInfo();
    HandleTrigger(trigger,SIDE_BITS_TOP); CalcObjectBox(); UpdateObject();
    long dx=gX.Int,dy=gY.Int; CoopEndObject(); CoopEndFrame();
    for (int p=0;p<2;p++)
    {
     PlayerState* state=&gCoopPlayers[p];
     Check(state->gMyNodePtr->X.Int==dx && state->gMyNodePtr->Y.Int==dy && state->gMyDX==0 && !state->gTeleportingFlag,"P2 teleporter moves team and clears momentum");
     if (state->gRealMePtr) Check(state->gRealMePtr->X.Int==dx && state->gRealMePtr->Y.Int==dy,"teleport synchronizes hidden transformed avatar");
     Check(state->gMyNodePtr->LeftSide==dx+state->gMyNodePtr->LeftOff,"teleported collision box is immediately current");
    }
    teleported=true;
   }
  }
  if (scene==SCENE_FAIRY || scene==SCENE_BARGAIN)
  {
   // Completion of a warp while transformed must update hidden Pete too.
   CoopBeginObject(gCoopPlayers[1].gMyNodePtr);
   gMyNodePtr->X.Int+=32; gMyNodePtr->Y.Int+=32; gTeleportingFlag=true;
   CoopEndObject(); CoopEndFrame();
   for (int p=0;p<2;p++)
    Check(gCoopPlayers[p].gRealMePtr->X.Int==gCoopPlayers[p].gMyNodePtr->X.Int && gCoopPlayers[p].gRealMePtr->Y.Int==gCoopPlayers[p].gMyNodePtr->Y.Int,"transformed warp completion synchronizes hidden avatars");
  }
  if (scene==SCENE_FAIRY || scene==SCENE_BARGAIN)
   for (int p=0;p<2;p++)
   {
    CoopBeginObject(gCoopPlayers[p].gMyNodePtr); DisposeFrog(); DisposeSpaceShip(); CoopEndObject();
    Check(!gCoopPlayers[p].gFrogFlag && !gCoopPlayers[p].gSpaceShipFlag && gCoopPlayers[p].gMyNodePtr->CoopOwner==p+1 && gCoopPlayers[p].gMyNodePtr->MoveFlag,"transformation disposal restores the correct Pete");
   }
  if (scene==SCENE_CANDY && !platformTested)
  {
   // Actual moving-platform collision/carry routine, controlled location/delta.
   ObjNode* platform=MakeNewShape(GroupNum_CandyMPlatform,ObjType_CandyMPlatform,0,gMyX,gMyY,1,NULL,PLAYFIELD_RELATIVE);
   Check(platform!=NULL,"real moving-platform sprite fixture");
   platform->CType=CTYPE_MPLATFORM; platform->TopOff=platform->LeftOff=-40; platform->BottomOff=platform->RightOff=40;
   platform->DX=0x10000; platform->DY=0x20000; CalcObjectBox2(platform);
   for (int p=0;p<2;p++)
   {
    gThisNodePtr=gCoopPlayers[p].gMyNodePtr; CoopBeginObject(gThisNodePtr); GetObjectInfo(); gSumDX=gSumDY=0;
    CheckIfMeOnMPlatform();
    Check(gMyNodePtr->MPlatform==platform && gSumDX==0x10000 && gSumDY==0x20000,"same moving platform carries either Pete independently");
    CoopEndObject();
   }
   DeleteObject(platform); platformTested=true;
  }
  Tick(2);
 }
 Check(teleported && platformTested,"campaign-specific teleport and platform checks executed");
 gIsInGame=false;
}

void CoopRunSmokeTests(void)
{
 Check(GetPlayerGamepad(0) && GetPlayerGamepad(1), "two independently assigned gamepads");
 Check(SDL_GetGamepadID(GetPlayerGamepad(0)) == testIDs[0]
  && SDL_GetGamepadID(GetPlayerGamepad(1)) == testIDs[1], "stable controller slots");
 CoopResetBindings(0); CoopResetBindings(1);
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
 if (gRunCoopSetupSmoke)
 {
  long frames=gFrames; short health=gMyHealth;
  CoopSetupScreen(true);
  Check(gFrames==frames && gMyHealth==health,"setup modal freezes simulation and enemy damage");
  Check(!CoopControllersNeedSetup(),"setup modal resumes only after both virtual players are ready");
  fprintf(stderr,"COOP SETUP SMOKE PASSED\n"); gIsInGame=false; return;
 }
 CheckCoopBindings();
 CheckDeadPartnerTether(0);
 CheckDeadPartnerTether(1);
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
 CheckPermanentDeath(0); CheckPermanentDeath(1);
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
 Check(gFinishedArea && gCoopPlayers[0].gMyNodePtr->YOffset.Int < -340 && gCoopPlayers[1].gMyNodePtr->YOffset.Int < -340, "both real liftoff animations complete level");
 gIsInGame=false; SetScreenOffsetFor640x480();
 gNumCoins=201; gGamePrefs.music=false;
 short max0=gMyMaxHealth,max1=gCoopPlayers[1].gMyMaxHealth; long bonusScore=gScore;
 ShowBonusScreen();
 Check(gScore>bonusScore && gNumCoins==0 && gMyMaxHealth==max0+1 && gCoopPlayers[1].gMyMaxHealth==max1+1,"real bonus screen awards team points and both max-health upgrades without save prompt");
 gIsInGame = false;
 gMyHealth = gCoopPlayers[0].gMyHealth = -1;
 gCoopPlayers[1].gMyHealth = -1;
 gAreaNum = 1;
 InitArea();
 Check(gCoopPlayers[0].gMyNodePtr->CoopOwner == 1 && gCoopPlayers[1].gMyNodePtr->CoopOwner == 2
  && gCoopPlayers[0].gMyHealth > 0 && gCoopPlayers[1].gMyHealth > 0
  && gNumBunnies > 0 && !gFinishedArea, "next level creates healthy players and fresh objectives");
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
 CheckCampaignMechanics();
 CheckUnmappedDevice();
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
 gPlayerMode=TWO_PLAYER; InitGame(); InitArea();
 peteCount=0;
 for (ObjNode* n=FirstNodePtr;n;n=n->NextNode) if (n->CType==CTYPE_MYGUY) peteCount++;
 Check(peteCount==1 && !gCoopReady,"original alternating-player mode initializes one active Pete");
 for (int p = 0; p < 2; p++) { SDL_CloseJoystick(sticks[p]); SDL_DetachVirtualJoystick(testIDs[p]); }
 fprintf(stderr, "COOP SMOKE PASSED (SDL virtual controllers; human play unverified)\n");
}
