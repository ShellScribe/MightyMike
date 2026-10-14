// Co-op-only device assignment, captured bindings and frozen setup screen.
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "myglobals.h"
#include "externs.h"
#include "coop.h"
#include "coopcontrols.h"
#include "font.h"
#include "shape.h"
#include "objecttypes.h"
#include "window.h"
#include "sound2.h"
#include "misc.h"
#include "main.h"

CoopProfile gCoopProfiles[2];
static SDL_Joystick* controllers[2];
static bool ready[2];
static bool loaded;
static CoopProfile savedProfiles[2];
static const char* captions[COOP_ACTIONS] = {
 "move up", "move down", "move left", "move right",
 "aim up", "aim down", "aim left", "aim right",
 "shoot", "previous weapon", "next weapon", "bunny radar", "pause" };

static bool BindingPath(char* path, size_t size)
{
#ifdef COOP_SMOKE_TESTS
 if (gRunCoopSmokeTests) { SDL_snprintf(path,size,"/private/tmp/mightymike-coop-test-bindings.txt"); return true; }
#endif
 char* folder = SDL_GetPrefPath("Pangea Software", "MightyMike");
 if (!folder) return false;
 SDL_snprintf(path, size, "%scoop-controls-v1.txt", folder);
 SDL_free(folder);
 return true;
}
static bool ValidBinding(CoopBinding b)
{
 if (b.type < COOP_UNBOUND || b.type > COOP_RAW_HAT || b.id < 0 || b.id > 255) return false;
 if (b.type == COOP_PAD_BUTTON) return b.id < SDL_GAMEPAD_BUTTON_COUNT;
 if (b.type == COOP_PAD_AXIS) return b.id < SDL_GAMEPAD_AXIS_COUNT && (b.direction == -1 || b.direction == 1);
 if (b.type == COOP_RAW_AXIS) return (b.direction == -1 || b.direction == 1) && b.rest >= -32768 && b.rest <= 32767;
 if (b.type == COOP_RAW_HAT) return b.direction == SDL_HAT_UP || b.direction == SDL_HAT_DOWN || b.direction == SDL_HAT_LEFT || b.direction == SDL_HAT_RIGHT;
 return true;
}
static void LoadBindings(void)
{
 if (loaded) return;
 loaded = true;
 char path[1024];
 if (!BindingPath(path, sizeof(path))) return;
 FILE* file = fopen(path, "r");
 if (!file) return;
 CoopProfile profiles[2] = {0};
 bool valid = true;
 for (int p = 0; p < 2 && valid; p++)
 {
  valid = fscanf(file, "%32s", profiles[p].guid) == 1 && SDL_strlen(profiles[p].guid) == 32;
  for (int a = 0; a < COOP_ACTIONS && valid; a++)
   for (int j = 0; j < 2 && valid; j++)
   {
    CoopBinding* b = &profiles[p].binding[a][j];
    valid = fscanf(file, "%d %d %d %d", &b->type, &b->id, &b->direction, &b->rest) == 4 && ValidBinding(*b);
   }
 }
 fclose(file);
 if (valid) SDL_memcpy(savedProfiles, profiles, sizeof(profiles));
}
bool CoopSaveBindings(void)
{
 char path[1024], temp[1040];
 if (!BindingPath(path, sizeof(path))) return false;
 SDL_snprintf(temp, sizeof(temp), "%s.tmp", path);
 FILE* file = fopen(temp, "w");
 if (!file) return false;
 bool ok = true;
 for (int p = 0; p < 2; p++)
 {
  const CoopProfile* profile = &gCoopProfiles[p];
  ok &= fprintf(file, "%s\n", profile->guid[0] ? profile->guid : "00000000000000000000000000000000") > 0;
  for (int a = 0; a < COOP_ACTIONS; a++)
   for (int j = 0; j < 2; j++)
   {
    CoopBinding b = profile->binding[a][j];
    ok &= fprintf(file, "%d %d %d %d\n", b.type, b.id, b.direction, b.rest) > 0;
   }
 }
 if (fclose(file) != 0) ok = false;
 if (ok) ok = rename(temp, path) == 0;
 if (ok) SDL_memcpy(savedProfiles, gCoopProfiles, sizeof(savedProfiles));
 return ok;
}
SDL_JoystickID CoopControllerID(int p) { return controllers[p] ? SDL_GetJoystickID(controllers[p]) : 0; }
void CoopRequireReady(void) { ready[0] = ready[1] = false; }
void CoopResetBindings(int p)
{
 SDL_memset(gCoopProfiles[p].binding, 0, sizeof(gCoopProfiles[p].binding));
 // An unmapped device has no inferred layout. The capture wizard is required.
 if (!controllers[p] || !SDL_IsGamepad(CoopControllerID(p))) { ready[p] = false; return; }
 const int needs[] = {kNeed_Up,kNeed_Down,kNeed_Left,kNeed_Right};
 for (int a = 0; a < 4; a++)
  for (int j = 0; j < 2; j++)
  {
   int type = kDefaultKeyBindings[needs[a]].gamepad[j].type;
   int id = kDefaultKeyBindings[needs[a]].gamepad[j].id;
   gCoopProfiles[p].binding[a][j] = (CoopBinding){type == kButton ? COOP_PAD_BUTTON : COOP_PAD_AXIS, id, type == kAxisMinus ? -1 : 1, 0};
  }
 const int aimAxes[] = {SDL_GAMEPAD_AXIS_RIGHTY,SDL_GAMEPAD_AXIS_RIGHTY,SDL_GAMEPAD_AXIS_RIGHTX,SDL_GAMEPAD_AXIS_RIGHTX};
 for (int a = 0; a < 4; a++)
  gCoopProfiles[p].binding[COOP_AIM_UP+a][0] = (CoopBinding){COOP_PAD_AXIS, aimAxes[a], a % 2 ? 1 : -1, 0};
 const int actionNeeds[] = {kNeed_Attack,kNeed_PrevWeapon,kNeed_NextWeapon,kNeed_Radar,kNeed_UIPause};
 for (int a = 0; a < 5; a++)
  for (int j = 0; j < 2; j++)
  {
   int type = kDefaultKeyBindings[actionNeeds[a]].gamepad[j].type;
   int id = kDefaultKeyBindings[actionNeeds[a]].gamepad[j].id;
   gCoopProfiles[p].binding[COOP_SHOOT+a][j] = (CoopBinding){type == kUnbound ? COOP_UNBOUND : type == kButton ? COOP_PAD_BUTTON : COOP_PAD_AXIS, id, type == kAxisMinus ? -1 : 1, 0};
  }
 ready[p] = false;
}
void CoopAssignController(int p, SDL_JoystickID id)
{
 if (id == CoopControllerID(p)) return;
 if (id && id == CoopControllerID(p ^ 1)) return; // no duplicate ownership
 if (controllers[p]) SDL_CloseJoystick(controllers[p]);
 controllers[p] = id ? SDL_OpenJoystick(id) : NULL;
 ready[p] = false;
 ClearInput();
 if (!controllers[p]) return;
 char guid[33];
 SDL_GUIDToString(SDL_GetJoystickGUID(controllers[p]), guid, sizeof(guid));
 if (SDL_strcmp(guid, gCoopProfiles[p].guid) == 0) return;
 SDL_strlcpy(gCoopProfiles[p].guid, guid, sizeof(gCoopProfiles[p].guid));
 CoopResetBindings(p);
 if (SDL_strcmp(guid, savedProfiles[p].guid) == 0) gCoopProfiles[p] = savedProfiles[p];
}
void CoopSyncControllers(void)
{
 LoadBindings();
 for (int p = 0; p < 2; p++)
  if (controllers[p] && !SDL_JoystickConnected(controllers[p])) CoopAssignController(p, 0);
 int count;
 SDL_JoystickID* ids = SDL_GetJoysticks(&count);
 for (int i = 0; i < count; i++)
 {
  if (ids[i] == CoopControllerID(0) || ids[i] == CoopControllerID(1)) continue;
  int p = !controllers[0] ? 0 : !controllers[1] ? 1 : -1;
  if (p < 0) break;
  CoopAssignController(p, ids[i]);
 }
 SDL_free(ids);
}
static bool BindingDown(int p, CoopBinding b)
{
 SDL_Joystick* joy = controllers[p];
 SDL_Gamepad* pad = GetPlayerGamepad(p);
 if (!joy) return false;
 switch (b.type)
 {
  case COOP_PAD_BUTTON: return pad && SDL_GetGamepadButton(pad, b.id);
  case COOP_PAD_AXIS: return pad && b.direction * (int)SDL_GetGamepadAxis(pad, b.id) > 10813;
  case COOP_RAW_BUTTON: return b.id < SDL_GetNumJoystickButtons(joy) && SDL_GetJoystickButton(joy, b.id);
  case COOP_RAW_AXIS: return b.id < SDL_GetNumJoystickAxes(joy) && b.direction * ((int)SDL_GetJoystickAxis(joy, b.id) - b.rest) > 10813;
  case COOP_RAW_HAT: return b.id < SDL_GetNumJoystickHats(joy) && (SDL_GetJoystickHat(joy, b.id) & b.direction);
  default: return false;
 }
}
static bool ActionDown(int p, int a)
{
 return BindingDown(p, gCoopProfiles[p].binding[a][0]) || BindingDown(p, gCoopProfiles[p].binding[a][1]);
}
bool CoopReadNeed(int p, int need)
{
 int a = -1;
 switch (need)
 {
  case kNeed_Up: case kNeed_UIUp: a = COOP_MOVE_UP; break;
  case kNeed_Down: case kNeed_UIDown: a = COOP_MOVE_DOWN; break;
  case kNeed_Left: case kNeed_UILeft: a = COOP_MOVE_LEFT; break;
  case kNeed_Right: case kNeed_UIRight: a = COOP_MOVE_RIGHT; break;
  case kNeed_Attack: case kNeed_UIConfirm: a = COOP_SHOOT; break;
  case kNeed_PrevWeapon: case kNeed_UIPrev: a = COOP_PREV; break;
  case kNeed_NextWeapon: case kNeed_UINext: a = COOP_NEXT; break;
  case kNeed_Radar: a = COOP_RADAR; break;
  case kNeed_UIPause: case kNeed_UIBack: a = COOP_PAUSE; break;
 }
 return a >= 0 && ActionDown(p, a);
}
short CoopReadAim(int p)
{
 int x = ActionDown(p, COOP_AIM_RIGHT) - ActionDown(p, COOP_AIM_LEFT);
 int y = ActionDown(p, COOP_AIM_DOWN) - ActionDown(p, COOP_AIM_UP);
 if (y > 0) return x > 0 ? AIM_DOWN_RIGHT : x < 0 ? AIM_DOWN_LEFT : AIM_DOWN;
 if (y < 0) return x > 0 ? AIM_UP_RIGHT : x < 0 ? AIM_UP_LEFT : AIM_UP;
 return x > 0 ? AIM_RIGHT : x < 0 ? AIM_LEFT : AIM_NONE;
}
static bool ProfileUsable(int p)
{
 if (!controllers[p]) return false;
 for (int a = 0; a < COOP_ACTIONS; a++)
 {
  if (a >= COOP_AIM_UP && a <= COOP_AIM_RIGHT) continue; // movement direction aims if no separate aim
  if (!gCoopProfiles[p].binding[a][0].type && !gCoopProfiles[p].binding[a][1].type) return false;
 }
 return true;
}
bool CoopControllersNeedSetup(void) { return !ready[0] || !ready[1] || !ProfileUsable(0) || !ProfileUsable(1); }
#ifdef COOP_SMOKE_TESTS
void CoopTestReady(int p) { ready[p] = ProfileUsable(p); }
void CoopTestReloadBindings(void) { loaded=false; LoadBindings(); SDL_memcpy(gCoopProfiles,savedProfiles,sizeof(savedProfiles)); }
#endif

void CoopDrawBox(int x, int y, int w, int h, Byte color)
{
 for (int j = y; j < y+h; j++)
  for (int i = x; i < x+w; i++)
   if (i >= 0 && i < VISIBLE_WIDTH && j >= 0 && j < VISIBLE_HEIGHT) gScreenLookUpTable[j][i] = color;
}
void CoopPaletteColors(Byte* light, Byte* dark)
{
 unsigned long brightest = 0, darkest = ~0UL;
 *light = *dark = 0;
 for (int i = 0; i < 255; i++)
 {
  RGBColor c = gGamePalette.baseColors[i];
  unsigned long l = (unsigned long)c.red + c.green + c.blue;
  if (l > brightest) { brightest = l; *light = i; }
  if (l < darkest) { darkest = l; *dark = i; }
 }
}
// Existing font, scaled down and recolored for the current level palette.
void CoopDrawText(int x, int y, const char* text, Byte color)
{
 for (; *text && x < VISIBLE_WIDTH-12; text++, x += 10)
 {
  unsigned char c = tolower((unsigned char)*text);
  if (c == ' ') continue;
  if (c == '-') { CoopDrawBox(x+1,y+6,7,1,color); continue; }
  int frame = ASCIIToBigFont(c);
  if (!frame) continue;
  const uint8_t *pixels, *mask;
  const FrameHeader* fh = GetFrameHeader(GroupNum_BigFont, ObjType_BigFont, frame-1, &pixels, &mask);
  for (int j = 0; j < 12; j++)
   for (int i = 0; i < 9; i++)
   {
    int offset = (j * fh->height / 12) * fh->width + i * fh->width / 9;
    if (mask[offset] != 255 && pixels[offset] != 179 && (pixels[offset] < 221 || pixels[offset] > 224)) CoopDrawBox(x+i, y+j, 1, 1, color);
   }
 }
}
static void BindingName(CoopBinding b, char* text, size_t size)
{
 switch (b.type)
 {
  case COOP_PAD_BUTTON: SDL_snprintf(text,size,"pad button %d",b.id); break;
  case COOP_PAD_AXIS: SDL_snprintf(text,size,"pad axis %d %s",b.id,b.direction > 0 ? "plus" : "minus"); break;
  case COOP_RAW_BUTTON: SDL_snprintf(text,size,"raw button %d",b.id); break;
  case COOP_RAW_AXIS: SDL_snprintf(text,size,"raw axis %d %s rest %d",b.id,b.direction > 0 ? "plus" : "minus",b.rest); break;
  case COOP_RAW_HAT: SDL_snprintf(text,size,"raw hat %d direction %d",b.id,b.direction); break;
  default: SDL_strlcpy(text,"unbound",size); break;
 }
}
// Neutral baseline allows asymmetric axes/triggers. Capture only an intentional
// excursion, then show the actual input and require confirmation.
static bool Capture(int p, int a, Byte light, Byte dark)
{
 SDL_JoystickID id = CoopControllerID(p);
 if (!id) return false;
 int rest[256], buttons[256], hats[256];
 bool calibrated = false, captured = false;
 CoopBinding binding = {0};
 Byte* backdrop = malloc((size_t)VISIBLE_WIDTH * VISIBLE_HEIGHT);
 if (!backdrop) return false;
 for (int y = 0; y < VISIBLE_HEIGHT; y++) SDL_memcpy(backdrop+y*VISIBLE_WIDTH,gScreenLookUpTable[y],VISIBLE_WIDTH);
 ClearInput();
 for (;;)
 {
  UpdateInput();
  if (CoopControllerID(p) != id) break;
  SDL_Joystick* joy = controllers[p];
  int na = SDL_min(SDL_GetNumJoystickAxes(joy),256), nb = SDL_min(SDL_GetNumJoystickButtons(joy),256), nh = SDL_min(SDL_GetNumJoystickHats(joy),256);
  for (int y = 0; y < VISIBLE_HEIGHT; y++) SDL_memcpy(gScreenLookUpTable[y],backdrop+y*VISIBLE_WIDTH,VISIBLE_WIDTH);
  int x = (VISIBLE_WIDTH-600)/2, y = 80;
  CoopDrawBox(x,y,600,230,dark);
  char line[80]; SDL_snprintf(line,sizeof(line),"P%d capture: %s",p+1,captions[a]);
  CoopDrawText(x+12,y+14,line,light);
  CoopDrawText(x+12,y+44,"escape cancels. space leaves this unbound.",light);
  if (!calibrated)
  {
   CoopDrawText(x+12,y+80,"release buttons. center sticks. then enter.",light);
   if (GetNewSDLKeyState(SDL_SCANCODE_RETURN))
   {
    for (int i=0;i<na;i++) rest[i]=SDL_GetJoystickAxis(joy,i);
    for (int i=0;i<nb;i++) buttons[i]=SDL_GetJoystickButton(joy,i);
    for (int i=0;i<nh;i++) hats[i]=SDL_GetJoystickHat(joy,i);
    calibrated=true;
   }
  }
  else if (!captured)
  {
   CoopDrawText(x+12,y+80,"press one button or move one axis fully.",light);
   for (int i=0;i<nb && !captured;i++)
   {
    bool down=SDL_GetJoystickButton(joy,i);
    if (down && !buttons[i]) { binding=(CoopBinding){COOP_RAW_BUTTON,i,0,0}; captured=true; }
    buttons[i]=down;
   }
   for (int i=0;i<nh && !captured;i++)
   {
    int hat=SDL_GetJoystickHat(joy,i);
    int dirs[]={SDL_HAT_UP,SDL_HAT_DOWN,SDL_HAT_LEFT,SDL_HAT_RIGHT};
    for (int d=0;d<4 && !captured;d++)
     if ((hat & dirs[d]) && !(hats[i] & dirs[d])) { binding=(CoopBinding){COOP_RAW_HAT,i,dirs[d],0}; captured=true; }
    hats[i]=hat;
   }
   for (int i=0;i<na && !captured;i++)
   {
    int delta=(int)SDL_GetJoystickAxis(joy,i)-rest[i];
    if (abs(delta)>22000) { binding=(CoopBinding){COOP_RAW_AXIS,i,delta>0 ? 1 : -1,rest[i]}; captured=true; }
   }
  }
  else
  {
   BindingName(binding,line,sizeof(line));
   CoopDrawText(x+12,y+80,line,light);
   CoopDrawText(x+12,y+110,"release it. enter accepts. backspace retries.",light);
   if (GetNewSDLKeyState(SDL_SCANCODE_BACKSPACE)) calibrated=captured=false;
   if (GetNewSDLKeyState(SDL_SCANCODE_RETURN) && !BindingDown(p,binding))
   {
    gCoopProfiles[p].binding[a][0]=binding;
    gCoopProfiles[p].binding[a][1]=(CoopBinding){0};
    free(backdrop); ClearInput(); return true;
   }
  }
  PresentIndexedFramebuffer(); SDL_Delay(16);
  if (GetNewSDLKeyState(SDL_SCANCODE_ESCAPE)) break;
  if (GetNewSDLKeyState(SDL_SCANCODE_SPACE))
  {
   SDL_memset(gCoopProfiles[p].binding[a],0,sizeof(gCoopProfiles[p].binding[a]));
   free(backdrop); ClearInput(); return true;
  }
 }
 free(backdrop); ClearInput(); return false;
}
static void CycleDevice(int p, int direction)
{
 int count; SDL_JoystickID* ids=SDL_GetJoysticks(&count);
 int current=-1;
 for (int i=0;i<count;i++) if (ids[i]==CoopControllerID(p)) current=i;
 for (int step=1;step<=count;step++)
 {
  int index=(current+direction*step+count*2)%count;
  if (ids[index] == CoopControllerID(p^1))
  {
   SDL_JoystickID mine=CoopControllerID(p), other=CoopControllerID(p^1);
   CoopAssignController(p,0); CoopAssignController(p^1,0);
   CoopAssignController(p,other); CoopAssignController(p^1,mine);
   break;
  }
  CoopAssignController(p,ids[index]); break;
 }
 SDL_free(ids); TryOpenGamepad(false); CoopRequireReady(); ClearInput();
}
void CoopSetupScreen(bool required)
{
 CoopProfile previous[2]; SDL_memcpy(previous,gCoopProfiles,sizeof(previous));
 SDL_JoystickID previousIDs[2]={CoopControllerID(0),CoopControllerID(1)};
 bool previousReady[2]={ready[0],ready[1]};
 Byte light,dark; CoopPaletteColors(&light,&dark);
 Byte* backdrop=malloc((size_t)VISIBLE_WIDTH*VISIBLE_HEIGHT);
 if (!backdrop) { gAbortGameFlag=true; return; }
 for (int y=0;y<VISIBLE_HEIGHT;y++) SDL_memcpy(backdrop+y*VISIBLE_WIDTH,gScreenLookUpTable[y],VISIBLE_WIDTH);
 bool wasPaused=gIsGamePaused;
 gIsGamePaused=true; PauseAllChannels(true); ClearInput();
 int row=0, action=-1, player=0;
 bool done=false;
 const char* message="both players press their pause button when ready";
 while (!done)
 {
  UpdateInput();
  bool up=GetNewNeedState(kNeed_UIUp), down=GetNewNeedState(kNeed_UIDown);
  bool confirm=GetNewNeedState(kNeed_UIConfirm);
  if (up) row=(row+10)%11;
  if (down) row=(row+1)%11;
  for (int p=0;p<2;p++)
   if (gPlayerInputs[p].pressed[kNeed_UIPause] && ProfileUsable(p)) ready[p]=true;
  if (GetNewNeedState(kNeed_UILeft) || GetNewNeedState(kNeed_UIRight))
  {
   if (row<2) CycleDevice(row,GetNewNeedState(kNeed_UILeft) ? -1 : 1);
   else if (row==8 || row==9) { player=row-8; action=((action<0 ? 0 : action)+(GetNewNeedState(kNeed_UILeft) ? -1 : 1)+COOP_ACTIONS)%COOP_ACTIONS; }
  }
  if (confirm)
  {
   if (row<2) CycleDevice(row,1);
   else if (row<4)
   {
    int p=row-2; CoopProfile backup=gCoopProfiles[p]; SDL_JoystickID id=CoopControllerID(p);
    ready[p]=false;
    for (int a=0;a<COOP_ACTIONS;a++)
     if (!Capture(p,a,light,dark)) { gCoopProfiles[p]=backup; break; }
    if (CoopControllerID(p)!=id) SDL_memset(gCoopProfiles[p].binding,0,sizeof(gCoopProfiles[p].binding));
   }
   else if (row<6) { CoopResetBindings(row-4); CoopRequireReady(); }
   else if (row==6)
   {
    if (CoopControllersNeedSetup()) message="connect. configure. both press pause. then resume.";
    else if (!CoopSaveBindings()) message="could not save bindings. retry or cancel.";
    else done=true;
   }
   else if (row==7)
   {
    // Restore only still-connected identities. Never silently move the survivor.
    for (int p=0;p<2;p++) CoopAssignController(p,0);
    for (int p=0;p<2;p++)
    {
     if (previousIDs[p] && SDL_GetJoystickNameForID(previousIDs[p])) CoopAssignController(p,previousIDs[p]);
     gCoopProfiles[p]=previous[p]; ready[p]=previousReady[p] && CoopControllerID(p)==previousIDs[p];
    }
    TryOpenGamepad(false);
    if (!required && !CoopControllersNeedSetup()) done=true;
    else message="changes cancelled. both players must be ready.";
   }
   else if (row==8 || row==9)
   {
    player=row-8; if (action<0) action=0;
    Capture(player,action,light,dark); CoopRequireReady();
   }
   else { gAbortGameFlag=true; done=true; }
   ClearInput();
  }
  for (int y=0;y<VISIBLE_HEIGHT;y++) SDL_memcpy(gScreenLookUpTable[y],backdrop+y*VISIBLE_WIDTH,VISIBLE_WIDTH);
  int x=(VISIBLE_WIDTH-620)/2,y=40;
  CoopDrawBox(x,y,620,380,dark);
  CoopDrawText(x+12,y+12,"COUCH CO OP SETUP",light);
  CoopDrawBox(x+2,y+40+row*24,3,12,light);
  for (int p=0;p<2;p++)
  {
   char line[80]; const char* name=controllers[p] ? SDL_GetJoystickName(controllers[p]) : "disconnected";
   SDL_snprintf(line,sizeof(line),"%sP%d %.38s %s",row==p ? "> " : "  ",p+1,name ? name : "unknown",ready[p] ? "ready" : "waiting");
   CoopDrawText(x+10,y+40+p*24,line,light);
  }
  const char* labels[]={"configure P1 all controls","configure P2 all controls","reset P1 defaults","reset P2 defaults","save and resume","cancel changes"};
  for (int i=0;i<6;i++)
  {
   char line[80]; SDL_snprintf(line,sizeof(line),"%s%s",row==i+2 ? "> " : "  ",labels[i]);
   CoopDrawText(x+10,y+88+i*24,line,light);
  }
  for (int p=0;p<2;p++)
  {
   char line[100], binding[48];
   BindingName(gCoopProfiles[p].binding[action<0 ? 0 : action][0],binding,sizeof(binding));
   SDL_snprintf(line,sizeof(line),"  P%d %s: %s",p+1,captions[action<0 ? 0 : action],binding);
   CoopDrawText(x+10,y+232+p*24,line,light);
  }
  CoopDrawText(x+10,y+280,row==10 ? "> quit campaign" : "  quit campaign",light);
  CoopDrawText(x+10,y+310,"arrows select or edit action. enter confirms.",light);
  CoopDrawText(x+10,y+340,message,light);
  PresentIndexedFramebuffer(); SDL_Delay(16);
 }
 for (int y=0;y<VISIBLE_HEIGHT;y++) SDL_memcpy(gScreenLookUpTable[y],backdrop+y*VISIBLE_WIDTH,VISIBLE_WIDTH);
 free(backdrop); gIsGamePaused=wasPaused; PauseAllChannels(wasPaused); ClearInput();
}
bool CoopControllerGate(void)
{
 if (!gCoopEnabled) return true;
 if (CoopControllersNeedSetup() || GetNewSDLKeyState(SDL_SCANCODE_F1)) CoopSetupScreen(CoopControllersNeedSetup());
 return !gAbortGameFlag;
}
