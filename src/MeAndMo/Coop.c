// Minimal local co-op adapter. Globals are restored to P1 after every object.
#include <stdlib.h>
#include "myglobals.h"
#include "externs.h"
#include "coop.h"
#include "coopcontrols.h"
#include "myguy.h"
#include "myguy2.h"
#include "object.h"
#include "input.h"
#include "misc.h"
#include "playfield.h"
#include "weapon.h"
#include "infobar.h"
#include "window.h"
#include "objecttypes.h"
#include "shape.h"

extern ObjNode* gRealMePtr;
extern short gMyGunDrawnTimer;
extern long gFrogTimer;
extern long gSpaceShipTimer;
extern short gShipSoundChannelNum;
extern long gOozieTick;
extern unsigned long gLastTripleShotFrame;

bool gCoopEnabled;
bool gCoopReady;
int gCoopActivePlayer = -1;
PlayerState gCoopPlayers[2];
static bool gHavePlayers;

static void SavePlayer(int p)
{
 PlayerState* s = &gCoopPlayers[p];
#define SAVE(type, name) s->name = name;
 COOP_PLAYER_FIELDS(SAVE)
#undef SAVE
 SDL_memcpy(s->weapons, gMyWeapons, sizeof(s->weapons));
}
static void LoadPlayer(int p)
{
 const PlayerState* s = &gCoopPlayers[p];
#define LOAD(type, name) name = s->name;
 COOP_PLAYER_FIELDS(LOAD)
#undef LOAD
 SDL_memcpy(gMyWeapons, s->weapons, sizeof(s->weapons));
}
static bool IsDead(int p)
{
 const PlayerState* s = &gCoopPlayers[p];
 return s->doneDead || s->gMyHealth < 0
  || s->gMyNodePtr->SubType == MY_ANIMBASE_DIE
  || s->gMyNodePtr->SubType == MY_ANIMBASE_WATERDIE;
}
static void SelectPlayer(int p)
{
 gCoopActivePlayer = p;
 LoadPlayer(p);
 SetInputPlayer(p);
}
static void RestorePlayerOne(void)
{
 LoadPlayer(0);
 gCoopActivePlayer = -1;
 SetInputPlayer(-1);
}
void CoopInitGame(void)
{
 gCoopReady = gHavePlayers = false;
 if (gCoopEnabled) CoopRequireReady();
 gCoopActivePlayer = -1;
 SDL_memset(gCoopPlayers, 0, sizeof(gCoopPlayers));
 SetInputPlayer(-1);
}
void CoopTagObject(ObjNode* node)
{
 if (gCoopEnabled && gCoopActivePlayer >= 0 && node)
  node->CoopOwner = gCoopActivePlayer + 1;
}
void CoopInitArea(void)
{
 if (!gCoopEnabled) return;
 PlayerState previous = gCoopPlayers[1];
 if (gMyHealth < 0) gMyHealth = gMyMaxHealth; // fallen P1 rejoins at a new level
 gMyDX = gMyDY = gMySumDX = gMySumDY = 0;
 gMyGunDrawnTimer = 0;
 gLastNonDeathX = gMyX;
 gLastNonDeathY = gMyY;
 gRealMePtr = NULL;
 gSpaceShipFlag = false;
 gMyNodePtr->CoopOwner = 1;
 SavePlayer(0);
 gCoopPlayers[0].doneDead = false;
 gCoopPlayers[0].revivedTicks = 0;
 InitMe();
 gMyNodePtr->CoopOwner = 2;
 SavePlayer(1);
 gCoopPlayers[1].doneDead = false;
 gCoopPlayers[1].revivedTicks = 0;
 if (gHavePlayers)
 {
  gCoopPlayers[1].gMyHealth = previous.gMyHealth < 0 ? previous.gMyMaxHealth : previous.gMyHealth;
  gCoopPlayers[1].gMyMaxHealth = previous.gMyMaxHealth;
  gCoopPlayers[1].gCurrentWeaponType = previous.gCurrentWeaponType;
  gCoopPlayers[1].gCurrentWeaponIndex = previous.gCurrentWeaponIndex;
  gCoopPlayers[1].gNumWeaponsIHave = previous.gNumWeaponsIHave;
  SDL_memcpy(gCoopPlayers[1].weapons, previous.weapons, sizeof(previous.weapons));
 }
 else gCoopPlayers[1].gMyHealth = gMyMaxHealth;
 gHavePlayers = gCoopReady = true;
 CoopRequireReady();
 RestorePlayerOne();
}
void CoopBeginObject(ObjNode* node)
{
 if (!gCoopEnabled || !gCoopReady || !gIsInGame) return;
 GAME_ASSERT(gCoopActivePlayer == -1);
 SavePlayer(0); // includes P1 effects from the outer game loop
 int p = node->CoopOwner ? node->CoopOwner - 1 : 0;
 if (!node->CoopOwner)
 {
  // Let legacy AI and proximity hazards use the nearest living player.
  long d[2];
  for (int i = 0; i < 2; i++)
   d[i] = labs(node->X.Int - gCoopPlayers[i].gMyX) + labs(node->Y.Int - gCoopPlayers[i].gMyY);
  if (IsDead(0) || (!IsDead(1) && d[1] < d[0])) p = 1;
 }
 SelectPlayer(p);
}
void CoopEndObject(void)
{
 if (gCoopActivePlayer < 0) return;
 SavePlayer(gCoopActivePlayer);
 RestorePlayerOne();
}
bool CoopSetDeathFlag(ObjNode* node)
{
 if (!gCoopEnabled || !gCoopReady || !node->CoopOwner) return false;
 // Effects carry ownership too; only the current player avatar can die.
 int p = node->CoopOwner - 1;
 if (node != gMyNodePtr) return false;
 gCoopPlayers[p].doneDead = true;
 return true;
}
void CoopLimitMove(void)
{
 if (gCoopActivePlayer < 0 || gTeleportingFlag) return;
 int otherPlayer = gCoopActivePlayer ^ 1;
 const PlayerState* other = &gCoopPlayers[otherPlayer];
 // Reject only movement that increases an excessive separation. No warping
 // through walls: collision detection still resolves the accepted movement.
 long maxX = PF_WINDOW_WIDTH - 128;
 long maxY = PF_WINDOW_HEIGHT - 128;
 long oldX = gThisNodePtr->X.Int, oldY = gThisNodePtr->Y.Int;
 if (!IsDead(otherPlayer))
 {
  if (labs(gX.Int - other->gMyX) > maxX && labs(gX.Int - other->gMyX) > labs(oldX - other->gMyX))
   { gX = gThisNodePtr->X; gDX = gSumDX = 0; }
  if (labs(gY.Int - other->gMyY) > maxY && labs(gY.Int - other->gMyY) > labs(oldY - other->gMyY))
   { gY = gThisNodePtr->Y; gDY = gSumDY = 0; }
 } // A dead partner must not anchor the survivor to a cleared section of map.
 // Keep the sprite inside the visible camera while it catches up.
 if (gX.Int < gScrollX + 24 || gX.Int > gScrollX + PF_WINDOW_WIDTH - 24)
  { gX = gThisNodePtr->X; gDX = gSumDX = 0; }
 if (gY.Int < gScrollY + 40 || gY.Int > gScrollY + PF_WINDOW_HEIGHT - 24)
  { gY = gThisNodePtr->Y; gDY = gSumDY = 0; }
}
static void RevivePlayer(int p, int partner)
{
 SelectPlayer(p);
 DisposeFrog();
 DisposeSpaceShip();
 gLastNonDeathX = gCoopPlayers[partner].gLastNonDeathX;
 gLastNonDeathY = gCoopPlayers[partner].gLastNonDeathY;
 ReviveMe();
 gMyNodePtr->DX = gMyNodePtr->DY = 0;
 gMyNodePtr->OldX = gMyNodePtr->X;
 gMyNodePtr->OldY = gMyNodePtr->Y;
 gMyX = gMyNodePtr->X.Int;
 gMyY = gMyNodePtr->Y.Int;
 gMyDX = gMyDY = 0;
 gMyMode = MY_MODE_BASICSTAND;
 gCoopPlayers[p].doneDead = false;
 gCoopPlayers[p].revivedTicks = GAME_FPS * 2;
 CalcObjectBox2(gMyNodePtr);
 SavePlayer(p);
 RestorePlayerOne();
}
void CoopEndFrame(void)
{
 if (!gCoopEnabled || !gCoopReady || !gIsInGame) return;
 if (gEnemyFreezeTimer > 0) gEnemyFreezeTimer--;
 SavePlayer(0);
 for (int p = 0; p < 2; p++) if (gCoopPlayers[p].revivedTicks > 0) gCoopPlayers[p].revivedTicks--;
 // A teleport takes the team together. Rebuild camera tiles at destination
 // rather than tethering a player to a partner left on the previous island.
 for (int p = 0; p < 2; p++)
 {
  if (!gCoopPlayers[p].gTeleportingFlag) continue;
  ObjNode* from = gCoopPlayers[p].gMyNodePtr;
  long destX = from->X.Int, destY = from->Y.Int;
  for (int i = 0; i < 2; i++)
  {
   PlayerState* s = &gCoopPlayers[i];
   ObjNode* avatars[2] = {s->gMyNodePtr, s->gRealMePtr};
   for (int n = 0; n < 2; n++)
   {
    ObjNode* avatar = avatars[n];
    if (!avatar) continue;
    avatar->X = avatar->OldX = from->X;
    avatar->Y = avatar->OldY = from->Y;
    avatar->DX = avatar->DY = 0;
    avatar->MPlatform = NULL;
    CalcObjectBox2(avatar);
   }
   s->gMyX = s->gLastNonDeathX = destX;
   s->gMyY = s->gLastNonDeathY = destY;
   s->gMyDX = s->gMyDY = s->gMySumDX = s->gMySumDY = 0;
   s->gTeleportingFlag = false;
   s->gMyBlinkieTimer = GAME_FPS * 2;
  }
  RestorePlayerOne();
  InitPlayfield();
  break;
 }
 if (gNumBunnies <= 0)
 {
  for (int p = 0; p < 2; p++)
  {
   SelectPlayer(p);
   DisposeFrog();
   DisposeSpaceShip();
   StartMyLiftoff();
   SavePlayer(p);
  }
  RestorePlayerOne();
  return;
 }
 bool dead[2] = {IsDead(0), IsDead(1)};
 if (dead[0] && dead[1])
 {
  if (gCoopPlayers[0].doneDead && gCoopPlayers[1].doneDead)
  {
   if (gNumLives > 1)
   {
    gNumLives--; // one shared spare revives the team
    RevivePlayer(0, 0);
    RevivePlayer(1, 0);
   }
   else gGlobFlag_MeDoneDead = true;
  }
 }
 else
 {
  for (int p = 0; p < 2; p++)
   if (gCoopPlayers[p].doneDead && gNumLives > 1)
   { gNumLives--; RevivePlayer(p, p ^ 1); }
 }
 DoMyScreenScroll(); // once per fixed simulation step, after both players
}
void CoopCameraCenter(long* x, long* y)
{
 bool dead0 = IsDead(0), dead1 = IsDead(1);
 if (dead0 != dead1)
 {
  ObjNode* survivor = gCoopPlayers[dead0 ? 1 : 0].gMyNodePtr;
  *x = survivor->X.Int;
  *y = survivor->Y.Int;
  return;
 }
 *x = (gCoopPlayers[0].gMyNodePtr->X.Int + gCoopPlayers[1].gMyNodePtr->X.Int) / 2;
 *y = (gCoopPlayers[0].gMyNodePtr->Y.Int + gCoopPlayers[1].gMyNodePtr->Y.Int) / 2;
}

// Tiny framebuffer overlay uses the existing palette; no new assets.
static void Box(int x, int y, int w, int h, Byte color)
{
 for (int j = y; j < y + h; j++)
  for (int i = x; i < x + w; i++)
   if (i >= 0 && i < VISIBLE_WIDTH && j >= 0 && j < VISIBLE_HEIGHT)
    gScreenLookUpTable[j][i] = color;
}
static void Label(int x, int y, int p, Byte color)
{
 // 3x5 P followed by 1 or 2, rendered at twice native pixel size.
 const int glyphs[3][5] = {{7,5,7,4,4},{2,6,2,2,7},{7,1,7,4,7}};
 for (int c = 0; c < 2; c++)
  for (int j = 0; j < 5; j++)
   for (int i = 0; i < 3; i++)
    if (glyphs[c ? p + 1 : 0][j] & (4 >> i)) Box(x + c*8 + i*2, y + j*2, 2, 2, color);
}
void CoopDrawOverlay(void)
{
 if (!gCoopEnabled || !gCoopReady || !gIsInGame || gIsGamePaused) return;
 Byte white, dark;
 CoopPaletteColors(&white,&dark);
 int panelWidth = SDL_min((PF_WINDOW_WIDTH-24)/2,200);
 for (int p = 0; p < 2; p++)
 {
  const PlayerState* s = &gCoopPlayers[p];
  int x = PF_WINDOW_LEFT + 12 + p * panelWidth;
  int y = PF_WINDOW_TOP + 8;
  Box(x-3, y-3, panelWidth-4, 40, dark);
  Label(x, y, p, white);
  for (int h = 0; h < s->gMyMaxHealth; h++)
  {
   Box(x+22+h*9, y, 7, 10, white);
   if (h >= s->gMyHealth || IsDead(p)) Box(x+23+h*9, y+1, 5, 8, dark);
  }
  if (s->gCurrentWeaponType != NO_WEAPON)
  {
   // Fit the existing weapon artwork into a 24px thumbnail.
   const uint8_t* pixels;
   const FrameHeader* icon = GetFrameHeader(GroupNum_WeaponIcon,ObjType_WeaponIcon,s->gCurrentWeaponType,&pixels,NULL);
   for (int j=0;j<24;j++) for (int i=0;i<24;i++)
    Box(x+96+i,y+j,1,1,pixels[(j*icon->height/24)*icon->width+i*icon->width/24]);
   char ammo[16];
   SDL_snprintf(ammo,sizeof(ammo),"%ld",(long)s->weapons[s->gCurrentWeaponIndex].life);
   CoopDrawText(x+124,y+4,ammo,white);
  }
  else CoopDrawText(x+96,y+4,"empty",white);
  const char* status = IsDead(p) ? (gNumLives > 1 ? "reviving" : "fallen") : s->revivedTicks ? "revived" : "";
  CoopDrawText(x+22,y+22,status,white);
  ObjNode* node = s->gMyNodePtr;
  int32_t spriteX, spriteY;
  TweenObjectPosition(node, &spriteX, &spriteY);
  const FrameHeader* frame = GetFrameHeader(node->SpriteGroupNum, node->Type, node->CurrentFrame, NULL, NULL);
  int markerX = PF_WINDOW_LEFT + spriteX - gTweenedScrollX + (p ? 8 : -24);
  int markerY = PF_WINDOW_TOP + spriteY - gTweenedScrollY + frame->y - 13;
  if (node->DrawFlag && !IsDead(p) && markerY >= PF_WINDOW_TOP + 67)
  {
   Box(markerX-1, markerY-1, 17, 12, dark);
   Label(markerX, markerY, p, white);
  }
 }
 char lives[40];
 SDL_snprintf(lives,sizeof(lives),"shared spares: %d",gNumLives > 0 ? gNumLives-1 : 0);
 int x = PF_WINDOW_LEFT+12, y = PF_WINDOW_TOP+48;
 Box(x-3,y-2,190,16,dark);
 CoopDrawText(x,y,lives,white);
}
