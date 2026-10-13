#pragma once
#include "structures.h"

// These are the legacy player globals scoped by the object dispatcher.
// World objectives, keys, score, coins, freeze timer and spare lives stay shared.
#define COOP_PLAYER_FIELDS(X) \
 X(ObjNode*, gMyNodePtr) \
 X(ObjNode*, gRealMePtr) \
 X(ObjNode*, gShieldNodePtr) \
 X(long, gMyX) \
 X(long, gMyY) \
 X(long, gMySumDX) \
 X(long, gMySumDY) \
 X(long, gMyDX) \
 X(long, gMyDY) \
 X(short, gMyDirection) \
 X(Byte, gMyMode) \
 X(long, gMyNormalMaxSpeed) \
 X(long, gMyMaxSpeedX) \
 X(long, gMyMaxSpeedY) \
 X(long, gMyAcceleration) \
 X(Boolean, gMeOnIceFlag) \
 X(Boolean, gMeOnWaterFlag) \
 X(long, gMyWindDX) \
 X(long, gMyWindDY) \
 X(short, gMyGunDrawnTimer) \
 X(short, gLastNonDeathX) \
 X(short, gLastNonDeathY) \
 X(short, gMyBlinkieTimer) \
 X(short, gSpeedyTimer) \
 X(short, gMyHealth) \
 X(short, gMyMaxHealth) \
 X(short, gShieldTimer) \
 X(Boolean, gFrogFlag) \
 X(Boolean, gSpaceShipFlag) \
 X(Boolean, gTeleportingFlag) \
 X(long, gFrogTimer) \
 X(long, gSpaceShipTimer) \
 X(short, gShipSoundChannelNum) \
 X(Byte, gCurrentWeaponType) \
 X(Byte, gNumWeaponsIHave) \
 X(Byte, gCurrentWeaponIndex) \
 X(long, gLastRocketTime) \
 X(long, gLastPixieTime) \
 X(long, gOozieTick) \
 X(unsigned long, gLastTripleShotFrame)

typedef struct PlayerState
{
#define FIELD(type, name) type name;
 COOP_PLAYER_FIELDS(FIELD)
#undef FIELD
 WeaponType weapons[MAX_WEAPONS];
 bool doneDead;
} PlayerState;
extern bool gCoopEnabled;
extern bool gCoopReady;
extern int gCoopActivePlayer;
extern PlayerState gCoopPlayers[2];
void CoopInitGame(void);
void CoopInitArea(void);
void CoopBeginObject(ObjNode* node);
void CoopEndObject(void);
void CoopEndFrame(void);
void CoopLimitMove(void);
void CoopTagObject(ObjNode* node);
void CoopDrawOverlay(void);
bool CoopSetDeathFlag(ObjNode* node);
void CoopCameraCenter(long* x, long* y);
