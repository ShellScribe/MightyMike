// Minimal local co-op adapter. Globals are restored to P1 after every object.
#include <stdlib.h>
#include "myglobals.h"
#include "externs.h"
#include "coop.h"
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
 gMyDX = gMyDY = gMySumDX = gMySumDY = 0;
 gMyGunDrawnTimer = 0;
 gLastNonDeathX = gMyX;
 gLastNonDeathY = gMyY;
 gRealMePtr = NULL;
 gSpaceShipFlag = false;
 gMyNodePtr->CoopOwner = 1;
 SavePlayer(0);
 gCoopPlayers[0].doneDead = false;
 InitMe();
 gMyNodePtr->CoopOwner = 2;
 SavePlayer(1);
 gCoopPlayers[1].doneDead = false;
 if (gHavePlayers)
 {
  gCoopPlayers[1].gMyHealth = previous.gMyHealth;
  gCoopPlayers[1].gMyMaxHealth = previous.gMyMaxHealth;
  gCoopPlayers[1].gCurrentWeaponType = previous.gCurrentWeaponType;
  gCoopPlayers[1].gCurrentWeaponIndex = previous.gCurrentWeaponIndex;
  gCoopPlayers[1].gNumWeaponsIHave = previous.gNumWeaponsIHave;
  SDL_memcpy(gCoopPlayers[1].weapons, previous.weapons, sizeof(previous.weapons));
 }
 else gCoopPlayers[1].gMyHealth = gMyMaxHealth;
 gHavePlayers = gCoopReady = true;
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
 const PlayerState* other = &gCoopPlayers[gCoopActivePlayer ^ 1];
 // Reject only movement that increases an excessive separation. No warping
 // through walls: collision detection still resolves the accepted movement.
 long maxX = PF_WINDOW_WIDTH - 128;
 long maxY = PF_WINDOW_HEIGHT - 128;
 long oldX = gThisNodePtr->X.Int, oldY = gThisNodePtr->Y.Int;
 if (labs(gX.Int - other->gMyX) > maxX && labs(gX.Int - other->gMyX) > labs(oldX - other->gMyX))
  { gX = gThisNodePtr->X; gDX = gSumDX = 0; }
 if (labs(gY.Int - other->gMyY) > maxY && labs(gY.Int - other->gMyY) > labs(oldY - other->gMyY))
  { gY = gThisNodePtr->Y; gDY = gSumDY = 0; }
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
 CalcObjectBox2(gMyNodePtr);
 SavePlayer(p);
 RestorePlayerOne();
}
void CoopEndFrame(void)
{
 if (!gCoopEnabled || !gCoopReady || !gIsInGame) return;
 SavePlayer(0);
 // A teleport takes the team together. Rebuild camera tiles at destination
 // rather than tethering a player to a partner left on the previous island.
 for (int p = 0; p < 2; p++)
 {
  if (!gCoopPlayers[p].gTeleportingFlag) continue;
  int other = p ^ 1;
  ObjNode* from = gCoopPlayers[p].gMyNodePtr;
  ObjNode* to = gCoopPlayers[other].gMyNodePtr;
  to->X = to->OldX = from->X;
  to->Y = to->OldY = from->Y;
  to->DX = to->DY = 0;
  gCoopPlayers[other].gMyX = from->X.Int;
  gCoopPlayers[other].gMyY = from->Y.Int;
  for (int i = 0; i < 2; i++)
  {
   gCoopPlayers[i].gTeleportingFlag = false;
   gCoopPlayers[i].gMyBlinkieTimer = GAME_FPS * 2;
   gCoopPlayers[i].gLastNonDeathX = from->X.Int;
   gCoopPlayers[i].gLastNonDeathY = from->Y.Int;
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
 Byte white = 0, dark = 0;
 unsigned long brightest = 0, darkest = ~0UL;
 for (int i = 0; i < 255; i++)
 {
  RGBColor c = gGamePalette.baseColors[i];
  unsigned long light = (unsigned long)c.red + c.green + c.blue;
  if (light > brightest) { brightest = light; white = i; }
  if (light < darkest) { darkest = light; dark = i; }
 }
 for (int p = 0; p < 2; p++)
 {
  const PlayerState* s = &gCoopPlayers[p];
  int x = PF_WINDOW_LEFT + 12 + p * 140;
  int y = PF_WINDOW_TOP + 8;
  Box(x-3, y-3, 125, 16, dark);
  Label(x, y, p, white);
  for (int h = 0; h < s->gMyMaxHealth; h++)
  {
   Box(x+22+h*14, y, 11, 10, white);
   if (h >= s->gMyHealth || IsDead(p)) Box(x+23+h*14, y+1, 9, 8, dark);
  }
  ObjNode* node = s->gMyNodePtr;
  int markerX = PF_WINDOW_LEFT + node->drawBox.left - OFFSCREEN_WINDOW_LEFT - PF_WINDOW_LEFT;
  int markerY = node->drawBox.top - OFFSCREEN_WINDOW_TOP - 13;
  if (node->DrawFlag && !IsDead(p))
  {
   Box(markerX-1, markerY-1, 17, 12, dark);
   Label(markerX, markerY, p, white);
  }
 }
}
