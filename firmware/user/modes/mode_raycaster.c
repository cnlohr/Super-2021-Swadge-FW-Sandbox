/*==============================================================================
 * Includes
 *============================================================================*/

#include <math.h>
#include <stdlib.h>
#include <osapi.h>
#include <user_interface.h>
#include <mem.h>
#include "user_main.h"
#include "printControl.h"
#include "nvm_interface.h"
#include "mode_raycaster.h"

#include "oled.h"
#include "assets.h"
#include "font.h"
#include "cndraw.h"

#include "buttons.h"

#include "menu2d.h"

/*==============================================================================
 * Defines
 *============================================================================*/

#define MAP_WIDTH  48
#define MAP_HEIGHT 48

#define TEX_WIDTH  48
#define TEX_HEIGHT 48

#define NUM_SPRITES 57

// For the player
#define PLAYER_SHOT_COOLDOWN  300000
#define LED_ON_TIME           500000

// For enemy textures
#define ENEMY_SHOT_COOLDOWN  3000000
#define SHOOTING_ANIM_TIME    500000
#define GOT_SHOT_ANIM_TIME    500000
#define LONG_WALK_ANIM_TIME  3000000
#define WALK_ANIM_TIME       1000000
#define STEP_ANIM_TIME        250000

#define NUM_WALK_FRAMES            2
#define NUM_SHOT_FRAMES            2
#define NUM_HURT_FRAMES            2

#define ENEMY_HEALTH               2

// Helper macro to return the absolute value of an integer
#define ABS(X) (((X) < 0) ? -(X) : (X))

/*==============================================================================
 * Enums
 *============================================================================*/

typedef enum
{
    E_IDLE,
    E_PICK_DIR_PLAYER,
    E_PICK_DIR_RAND,
    E_WALKING,
    E_SHOOTING,
    E_GOT_SHOT,
    E_DEAD
} enemyState_t;

typedef enum
{
    RC_MENU,
    RC_GAME,
    RC_GAME_OVER,
    RC_SCORES
} raycasterMode_t;

// World map tiles. Use defines, not an enum, so worldMap[][] can be a uint8_t[][]
#define WMT_W1 0 ///< Wall 1
#define WMT_W2 1 ///< Wall 2
#define WMT_W3 2 ///< Wall 2
#define WMT_C  3 ///< Column
#define WMT_E  4 ///< Empty
#define WMT_S  5 ///< Spawn point

/*==============================================================================
 * Structs
 *============================================================================*/

typedef struct
{
    uint8_t mapX;
    uint8_t mapY;
    uint8_t side;
    int32_t drawStart;
    int32_t drawEnd;
    float perpWallDist;
    float rayDirX;
    float rayDirY;
} rayResult_t;

typedef struct
{
    // Sprite location and direction
    float posX;
    float posY;
    float dirX;
    float dirY;

    // Sprite texture
    color* texture;
    int32_t texTimer;
    int8_t texFrame;
    bool mirror;

    // Sprite logic
    enemyState_t state;
    int32_t stateTimer;
    int32_t shotCooldown;
    bool isBackwards;
    int32_t health;
} raySprite_t;

typedef struct
{
    raycasterMode_t mode;
    menu_t* menu;

    uint8_t rButtonState;
    float posX;
    float posY;
    float dirX;
    float dirY;
    float planeX;
    float planeY;
    int32_t shotCooldown;
    bool checkShot;
    int32_t initialHealth;
    int32_t health;
    uint32_t tRoundStartedUs;
    uint32_t tRoundElapsed;
    raycasterDifficulty_t difficulty;

    // The enemies
    raySprite_t sprites[NUM_SPRITES];
    uint8_t liveSprites;
    uint8_t kills;

    // Storage for textures
    color stoneTex [TEX_WIDTH * TEX_HEIGHT];
    color stripeTex[TEX_WIDTH * TEX_HEIGHT];
    color brickTex [TEX_WIDTH * TEX_HEIGHT];
    color sinTex   [TEX_WIDTH * TEX_HEIGHT];

    color walk    [NUM_WALK_FRAMES][TEX_WIDTH * TEX_HEIGHT];
    color shooting[NUM_SHOT_FRAMES][TEX_WIDTH * TEX_HEIGHT];
    color hurt    [NUM_HURT_FRAMES][TEX_WIDTH * TEX_HEIGHT];
    color dead                     [TEX_WIDTH * TEX_HEIGHT];

    // Storage for HUD images
    pngHandle heart;
    pngHandle mnote;
    pngSequenceHandle gtr;

    // For LEDs
    timer_t ledTimer;
    uint32_t closestDist;
    float closestAngle;
    int32_t gotShotTimer;
    int32_t shotSomethingTimer;
    int32_t killedSpriteTimer;
    int32_t healthWarningTimer;
    bool healthWarningInc;
} raycaster_t;

/*==============================================================================
 * Prototypes
 *============================================================================*/

void ICACHE_FLASH_ATTR raycasterEnterMode(void);
void ICACHE_FLASH_ATTR raycasterExitMode(void);
void ICACHE_FLASH_ATTR raycasterButtonCallback(uint8_t state, int32_t button, int32_t down);
void ICACHE_FLASH_ATTR raycasterMenuButtonCallback(const char* selected);
bool ICACHE_FLASH_ATTR raycasterRenderTask(void);
void ICACHE_FLASH_ATTR raycasterGameRenderer(uint32_t tElapsedUs);
void ICACHE_FLASH_ATTR raycasterLedTimer(void* arg __attribute__((unused)));
void ICACHE_FLASH_ATTR raycasterDrawScores(void);
void ICACHE_FLASH_ATTR raycasterEndRound(void);
void ICACHE_FLASH_ATTR raycasterDrawRoundOver(uint32_t tElapsedUs);

void ICACHE_FLASH_ATTR moveEnemies(uint32_t tElapsed);
void ICACHE_FLASH_ATTR handleRayInput(uint32_t tElapsed);

void ICACHE_FLASH_ATTR castRays(rayResult_t* rayResult);
void ICACHE_FLASH_ATTR drawTextures(rayResult_t* rayResult);
void ICACHE_FLASH_ATTR drawOutlines(rayResult_t* rayResult);
void ICACHE_FLASH_ATTR drawSprites(rayResult_t* rayResult);
void ICACHE_FLASH_ATTR drawHUD(void);

void ICACHE_FLASH_ATTR raycasterInitGame(raycasterDifficulty_t difficulty);
void ICACHE_FLASH_ATTR sortSprites(int32_t* order, float* dist, int32_t amount);
float ICACHE_FLASH_ATTR Q_rsqrt( float number );
bool ICACHE_FLASH_ATTR checkLineToPlayer(raySprite_t* sprite, float pX, float pY);
void ICACHE_FLASH_ATTR setSpriteState(raySprite_t* sprite, enemyState_t state);

/*==============================================================================
 * Variables
 *============================================================================*/

swadgeMode raycasterMode =
{
    .modeName = "raycaster",
    .fnEnterMode = raycasterEnterMode,
    .fnExitMode = raycasterExitMode,
    .fnRenderTask = raycasterRenderTask,
    .fnButtonCallback = raycasterButtonCallback,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .menuImg = "ray-menu.gif"
};

raycaster_t* rc;

static const uint8_t worldMap[MAP_WIDTH][MAP_HEIGHT] =
{
    {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, },
    {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 0, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, },
    {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 4, 5, 4, 4, 0, 0, 0, 4, 4, 5, 4, 4, 0, 4, 2, 4, 4, 5, 4, 5, 4, 3, 3, 3, 4, 4, 4, 4, 4, 4, 2, },
    {4, 4, 4, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 4, 0, 0, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 0, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 4, 2, },
    {4, 4, 4, 1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 1, 4, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4, 3, 4, 2, },
    {1, 1, 1, 1, 4, 3, 5, 3, 4, 3, 4, 3, 4, 1, 1, 0, 0, 0, 0, 4, 4, 5, 5, 5, 4, 4, 0, 0, 0, 0, 4, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 4, 2, },
    {2, 4, 4, 4, 4, 4, 5, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 4, 2, 4, 4, 4, 4, 4, 4, 3, 3, 3, 4, 5, 4, 5, 4, 4, 2, },
    {2, 4, 1, 1, 4, 3, 5, 3, 4, 3, 4, 3, 4, 1, 4, 0, 0, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 0, 0, 4, 2, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, },
    {2, 4, 2, 1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 1, 4, 0, 4, 4, 5, 4, 4, 0, 0, 0, 4, 4, 5, 4, 4, 0, 4, 2, 4, 3, 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, },
    {2, 4, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 4, 4, 4, 4, 0, 4, 2, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, },
    {2, 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 4, 5, 4, 5, 4, 3, 3, 3, 4, 4, 4, 4, 4, 4, 2, },
    {2, 4, 4, 4, 4, 4, 4, 2, 4, 4, 4, 4, 4, 4, 2, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 4, 2, },
    {2, 4, 5, 4, 4, 4, 4, 2, 4, 4, 4, 4, 5, 4, 2, 0, 4, 5, 5, 4, 4, 4, 4, 4, 4, 5, 4, 4, 5, 4, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4, 3, 4, 2, },
    {2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 0, 4, 5, 5, 4, 4, 3, 3, 3, 4, 4, 3, 3, 4, 4, 0, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 4, 2, },
    {2, 4, 4, 4, 3, 4, 4, 5, 4, 4, 3, 4, 4, 4, 2, 0, 4, 4, 4, 4, 4, 3, 4, 4, 4, 5, 4, 4, 5, 4, 0, 2, 4, 4, 4, 4, 4, 4, 3, 3, 3, 4, 5, 4, 4, 4, 4, 2, },
    {2, 4, 4, 4, 4, 3, 4, 4, 4, 3, 4, 4, 4, 4, 2, 0, 4, 4, 3, 4, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 0, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, },
    {2, 4, 4, 4, 4, 4, 3, 4, 3, 4, 4, 4, 4, 4, 2, 0, 4, 4, 3, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 2, 4, 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, },
    {2, 2, 2, 4, 5, 4, 4, 3, 4, 4, 5, 4, 2, 2, 2, 0, 4, 4, 4, 4, 4, 4, 4, 4, 0, 1, 1, 1, 1, 1, 1, 1, 4, 4, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 4, },
    {2, 4, 4, 4, 4, 4, 3, 4, 3, 4, 4, 4, 4, 4, 2, 0, 4, 4, 4, 4, 5, 5, 4, 4, 0, 1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 1, 4, },
    {2, 4, 4, 4, 4, 3, 4, 4, 4, 3, 4, 4, 4, 4, 2, 0, 4, 4, 3, 4, 5, 5, 4, 4, 0, 1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 1, 4, },
    {2, 4, 4, 4, 3, 4, 4, 5, 4, 4, 3, 4, 4, 4, 2, 0, 4, 4, 3, 4, 4, 4, 4, 4, 0, 1, 4, 4, 3, 3, 3, 3, 3, 3, 3, 4, 4, 3, 3, 3, 3, 3, 3, 3, 4, 4, 1, 4, },
    {2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 0, 4, 4, 3, 3, 3, 4, 4, 4, 0, 1, 4, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 4, 4, 1, 4, },
    {2, 4, 5, 4, 4, 4, 4, 2, 4, 4, 4, 4, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 1, 4, 4, 3, 4, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 4, 3, 4, 4, 1, 4, },
    {2, 4, 4, 4, 4, 4, 4, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 1, 4, 4, 3, 4, 4, 3, 3, 3, 3, 4, 4, 3, 3, 3, 3, 4, 4, 3, 4, 4, 1, 4, },
    {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 4, 4, 3, 4, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 3, 4, 4, 3, 4, 4, 1, 4, },
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 4, 4, 4, 4, 4, 4, 4, 5, 4, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 1, 4, },
    {1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 4, 5, 4, 4, 4, 4, 4, 4, 4, 1, 4, },
    {1, 4, 5, 4, 4, 4, 4, 4, 4, 5, 4, 4, 4, 2, 4, 4, 4, 4, 5, 4, 4, 4, 4, 4, 2, 1, 4, 4, 3, 4, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 3, 4, 4, 3, 4, 4, 1, 4, },
    {1, 4, 4, 3, 3, 4, 4, 3, 3, 4, 4, 1, 4, 2, 4, 4, 3, 4, 4, 4, 3, 4, 4, 4, 2, 1, 4, 4, 3, 4, 4, 3, 3, 3, 3, 4, 4, 3, 3, 3, 3, 4, 4, 3, 4, 4, 1, 4, },
    {1, 4, 4, 3, 3, 4, 4, 3, 3, 4, 4, 1, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 4, 4, 2, 1, 4, 4, 3, 4, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 4, 3, 4, 4, 1, 4, },
    {1, 4, 4, 4, 4, 5, 4, 4, 4, 4, 4, 1, 4, 4, 4, 4, 4, 4, 4, 4, 3, 4, 4, 4, 2, 1, 4, 4, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 4, 4, 1, 4, },
    {1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 1, 2, 2, 4, 4, 4, 3, 4, 4, 4, 4, 4, 4, 2, 1, 4, 4, 3, 3, 3, 3, 3, 3, 3, 4, 4, 3, 3, 3, 3, 3, 3, 3, 4, 4, 1, 4, },
    {1, 4, 4, 3, 3, 4, 4, 3, 3, 4, 4, 1, 4, 2, 4, 4, 3, 3, 3, 4, 4, 5, 4, 4, 2, 1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 1, 4, },
    {1, 4, 4, 3, 3, 4, 4, 3, 3, 4, 4, 1, 4, 2, 4, 4, 4, 3, 4, 4, 4, 4, 4, 4, 2, 1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 1, 4, },
    {1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 1, 4, 2, 4, 4, 4, 4, 4, 4, 3, 4, 4, 4, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 4, 4, 1, 4, },
    {1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 1, 4, 2, 4, 4, 4, 5, 4, 3, 3, 3, 4, 4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 1, 4, 4, 1, 4, },
    {1, 1, 1, 1, 4, 4, 1, 1, 1, 1, 1, 1, 4, 2, 4, 4, 4, 4, 4, 4, 3, 4, 4, 4, 2, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 4, 4, 4, 1, 4, },
    {0, 4, 4, 4, 4, 4, 1, 4, 4, 4, 4, 4, 4, 2, 4, 4, 4, 3, 4, 4, 4, 4, 4, 4, 2, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 4, 4, 4, 4, 4, 4, 4, 4, 1, 1, 4, },
    {0, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 2, 4, 4, 3, 3, 3, 4, 4, 4, 4, 4, 2, 0, 4, 4, 3, 3, 3, 3, 3, 3, 3, 4, 4, 3, 4, 4, 4, 4, 4, 4, 0, 4, 4, 4, },
    {0, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 0, 4, 2, 4, 4, 4, 3, 4, 4, 4, 3, 4, 4, 2, 0, 4, 4, 3, 4, 4, 4, 4, 4, 3, 4, 4, 4, 5, 4, 0, 0, 0, 0, 0, 4, 4, 4, },
    {0, 4, 5, 4, 4, 4, 4, 4, 4, 0, 4, 0, 4, 2, 4, 4, 4, 4, 4, 5, 4, 4, 4, 4, 2, 0, 4, 4, 3, 4, 4, 4, 4, 4, 3, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 4, },
    {0, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 0, 4, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 0, 4, 4, 3, 4, 5, 3, 4, 5, 3, 4, 5, 3, 4, 4, 0, 4, 4, 4, 4, 4, 4, 4, },
    {0, 4, 5, 4, 3, 3, 4, 4, 4, 0, 4, 0, 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4, 4, 2, 0, 4, 4, 4, 4, 4, 3, 4, 4, 4, 4, 4, 3, 4, 4, 0, 4, 4, 4, 4, 4, 4, 4, },
    {0, 4, 4, 4, 3, 3, 4, 4, 4, 0, 4, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 4, 4, 2, 0, 4, 4, 4, 4, 4, 3, 4, 4, 4, 4, 4, 3, 4, 4, 0, 4, 4, 4, 4, 4, 4, 4, },
    {0, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 4, 4, 2, 0, 4, 4, 3, 4, 4, 3, 3, 3, 3, 3, 3, 3, 4, 4, 0, 4, 4, 4, 4, 4, 4, 4, },
    {0, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 4, },
    {0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 4, 4, 4, 4, 4, 4, 4, },
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 4, 4, 4, 4, 4, },
};

static const char rc_title[]  = "SHREDDER";
static const char rc_easy[]   = "EASY";
static const char rc_med[]    = "MEDIUM";
static const char rc_hard[]   = "HARD";
static const char rc_scores[] = "SCORES";
static const char rc_quit[]   = "QUIT";

/*==============================================================================
 * Functions
 *============================================================================*/

/**
 * Initialize the raycaster mode and allocate all assets
 */
void ICACHE_FLASH_ATTR raycasterEnterMode(void)
{
    RAY_PRINTF("malloc %d\n", sizeof(raycaster_t));
    RAY_PRINTF("system_get_free_heap_size %d\n", system_get_free_heap_size());

    // Allocate and zero out everything
    rc = os_malloc(sizeof(raycaster_t));
    ets_memset(rc, 0, sizeof(raycaster_t));

    // Initialize and start at the menu
    rc->mode = RC_MENU;
    rc->menu = initMenu(rc_title, raycasterMenuButtonCallback);
    addRowToMenu(rc->menu);
    addItemToRow(rc->menu, rc_easy);
    addItemToRow(rc->menu, rc_med);
    addItemToRow(rc->menu, rc_hard);
    addRowToMenu(rc->menu);
    addItemToRow(rc->menu, rc_scores);
    addRowToMenu(rc->menu);
    addItemToRow(rc->menu, rc_quit);

    // Turn off debounce
    enableDebounce(false);

    // Load the enemy texture to RAM
    pngHandle tmpPngHandle;
    allocPngAsset("h8_wlk1.png", &tmpPngHandle);
    drawPngToBuffer(&tmpPngHandle, rc->walk[0]);
    freePngAsset(&tmpPngHandle);

    allocPngAsset("h8_wlk2.png", &tmpPngHandle);
    drawPngToBuffer(&tmpPngHandle, rc->walk[1]);
    freePngAsset(&tmpPngHandle);

    allocPngAsset("h8_atk1.png", &tmpPngHandle);
    drawPngToBuffer(&tmpPngHandle, rc->shooting[0]);
    freePngAsset(&tmpPngHandle);

    allocPngAsset("h8_atk2.png", &tmpPngHandle);
    drawPngToBuffer(&tmpPngHandle, rc->shooting[1]);
    freePngAsset(&tmpPngHandle);

    allocPngAsset("h8_hrt1.png", &tmpPngHandle);
    drawPngToBuffer(&tmpPngHandle, rc->hurt[0]);
    freePngAsset(&tmpPngHandle);

    allocPngAsset("h8_hrt2.png", &tmpPngHandle);
    drawPngToBuffer(&tmpPngHandle, rc->hurt[1]);
    freePngAsset(&tmpPngHandle);

    allocPngAsset("h8_ded.png", &tmpPngHandle);
    drawPngToBuffer(&tmpPngHandle, rc->dead);
    freePngAsset(&tmpPngHandle);

    // Load the wall textures to RAM
    allocPngAsset("txstone.png", &tmpPngHandle);
    drawPngToBuffer(&tmpPngHandle, rc->stoneTex);
    freePngAsset(&tmpPngHandle);

    allocPngAsset("txstripe.png", &tmpPngHandle);
    drawPngToBuffer(&tmpPngHandle, rc->stripeTex);
    freePngAsset(&tmpPngHandle);

    allocPngAsset("txbrick.png", &tmpPngHandle);
    drawPngToBuffer(&tmpPngHandle, rc->brickTex);
    freePngAsset(&tmpPngHandle);

    allocPngAsset("txsinw.png", &tmpPngHandle);
    drawPngToBuffer(&tmpPngHandle, rc->sinTex);
    freePngAsset(&tmpPngHandle);

    // Load the HUD assets
    allocPngAsset("heart.png", &(rc->heart));
    allocPngAsset("mnote.png", &(rc->mnote));
    allocPngSequence(&(rc->gtr), 5,
                     "gtr1.png",
                     "gtr2.png",
                     "gtr3.png",
                     "gtr4.png",
                     "gtr5.png");

    // Set up the LED timer
    rc->closestDist = 0xFFFFFFFF;
    rc->closestAngle = 0;
    timerSetFn(&(rc->ledTimer), raycasterLedTimer, NULL);
    timerArm(&(rc->ledTimer), 10, true);

    RAY_PRINTF("system_get_free_heap_size %d\n", system_get_free_heap_size());
}

/**
 * Free all resources allocated in raycasterEnterMode
 */
void ICACHE_FLASH_ATTR raycasterExitMode(void)
{
    // Free menu
    deinitMenu(rc->menu);

    // Free HUD assets (textures dont need freeing)
    freePngAsset(&(rc->heart));
    freePngAsset(&(rc->mnote));
    freePngSequence(&(rc->gtr));

    // stop the LED timer
    timerDisarm(&(rc->ledTimer));
    timerFlush();

    // Free everything else
    os_free(rc);
    rc = NULL;
}

/**
 * Initialize the game state and start it
 *
 * @param difficulty This round's difficulty
 */
void ICACHE_FLASH_ATTR raycasterInitGame(raycasterDifficulty_t difficulty)
{
    // Set the mode and difficulty
    rc->mode = RC_GAME;
    rc->difficulty = difficulty;

    // Clear the button state
    rc->rButtonState = 0;

    // x and y start position
    rc->posY = 10.5;
    rc->posX = 40;
    // initial direction vector
    rc->dirX = 1;
    rc->dirY = 0;
    // the 2d raycaster version of camera plane
    rc->planeX = 0;
    rc->planeY = -0.66;

    // Make sure all sprites are intially out of bounds
    for(uint8_t i = 0; i < NUM_SPRITES; i++)
    {
        rc->sprites[i].posX = -1;
        rc->sprites[i].posY = -1;
    }

    // Set the number of enemies based on the difficulty
    uint16_t diffMod = 0;
    if(RC_EASY == difficulty)
    {
        diffMod = 2;
    }
    else if(RC_MED == difficulty)
    {
        diffMod = 3;
    }
    else if(RC_HARD == difficulty)
    {
        diffMod = 1;
    }

    // Start spawning sprites
    rc->liveSprites = 0;
    rc->kills = 0;
    uint16_t spawnIdx = 0;
#ifndef TEST_GAME_OVER
    // Look over the whole map
    for(uint8_t x = 0; x < MAP_WIDTH; x++)
    {
        for(uint8_t y = 0; y < MAP_HEIGHT; y++)
        {
            // If this is a spawn point
            if(worldMap[x][y] == WMT_S)
            {
                // And a sprite should be spawned
                if(rc->liveSprites < NUM_SPRITES && (((spawnIdx % diffMod) > 0) || (RC_HARD == difficulty)))
                {
                    // Spawn it here
                    rc->sprites[rc->liveSprites].posX = x;
                    rc->sprites[rc->liveSprites].posY = y;
                    rc->sprites[rc->liveSprites].dirX = 0;
                    rc->sprites[rc->liveSprites].dirX = 0;
                    rc->sprites[rc->liveSprites].shotCooldown = 0;
                    rc->sprites[rc->liveSprites].isBackwards = false;
                    rc->sprites[rc->liveSprites].health = ENEMY_HEALTH;
                    setSpriteState(&(rc->sprites[rc->liveSprites]), E_IDLE);
                    rc->liveSprites++;
                }
                spawnIdx++;
            }
        }
    }
#else
    // For testing, just spawn one sprite
    rc->sprites[rc->liveSprites].posX = 45;
    rc->sprites[rc->liveSprites].posY = 2;
    rc->sprites[rc->liveSprites].dirX = 0;
    rc->sprites[rc->liveSprites].dirX = 0;
    rc->sprites[rc->liveSprites].shotCooldown = 0;
    rc->sprites[rc->liveSprites].isBackwards = false;
    rc->sprites[rc->liveSprites].health = ENEMY_HEALTH;
    setSpriteState(&(rc->sprites[rc->liveSprites]), E_IDLE);
    rc->liveSprites++;
#endif

    // Set health based on the number of enemies and difficulty
    if(RC_EASY == difficulty)
    {
        rc->initialHealth = rc->liveSprites * 2;
    }
    else if(RC_MED == difficulty)
    {
        rc->initialHealth = (rc->liveSprites * 3) / 2;
    }
    else if(RC_HARD == difficulty)
    {
        rc->initialHealth = rc->liveSprites;
    }
    rc->health = rc->initialHealth;

    // Clear player shot variables
    rc->shotCooldown = 0;
    rc->checkShot = false;
    rc->gotShotTimer = 0;
    rc->shotSomethingTimer = 0;

    rc->killedSpriteTimer = 0;
    rc->healthWarningTimer = 0;
    rc->healthWarningInc = true;

    // Reset the closest distance to not shine LEDs
    rc->closestDist = 0xFFFFFFFF;
    rc->closestAngle = 0;

    // Start the round clock
    rc->tRoundElapsed = 0;
    rc->tRoundStartedUs = system_get_time();
}

/**
 * The button callback. Handle the buttons differently per-mode
 *
 * @param state  A bitmask with all the current button states
 * @param button The button that caused this interrupt
 * @param down   true if the button was pushed, false if it was released
 */
void ICACHE_FLASH_ATTR raycasterButtonCallback(uint8_t state, int32_t button, int32_t down)
{
    switch(rc->mode)
    {
        default:
        case RC_MENU:
        {
            if(down)
            {
                // Just pass the button along to the menu button handler
                menuButton(rc->menu, button);
            }
            break;
        }
        case RC_GAME:
        {
            // Save the button state for the game to process later
            rc->rButtonState = state;
            break;
        }
        case RC_GAME_OVER:
        {
            // If any button is pressed on game over, just return to the menu
            if(down)
            {
                rc->mode = RC_MENU;
            }
            break;
        }
        case RC_SCORES:
        {
            // If the button is pressed on the score screen
            if(down)
            {
                switch (button)
                {
                    case 2:
                    {
                        // Left, cycle left
                        rc->difficulty = (rc->difficulty + 1) % RC_NUM_DIFFICULTIES;
                        break;
                    }
                    case 0:
                    {
                        // Right, cycle right
                        if(0 == rc->difficulty)
                        {
                            rc->difficulty = RC_NUM_DIFFICULTIES - 1;
                        }
                        else
                        {
                            rc->difficulty--;
                        }
                        break;
                    }
                    case 4:
                    {
                        // Action, return to the menu
                        rc->mode = RC_MENU;
                        break;
                    }
                    default:
                    {
                        // Do nothing
                        break;
                    }
                }
            }
            break;
        }
    }
}

/**
 * Button callback from the top level menu when an item is selected
 *
 * @param selected The string that was selected
 */
void ICACHE_FLASH_ATTR raycasterMenuButtonCallback(const char* selected)
{
    if(rc_quit == selected)
    {
        // Quit to the Swadge menu
        switchToSwadgeMode(0);
    }
    else if(rc_easy == selected)
    {
        // Start the game
        raycasterInitGame(RC_EASY);
    }
    else if(rc_med == selected)
    {
        // Start the game
        raycasterInitGame(RC_MED);
    }
    else if(rc_hard == selected)
    {
        // Start the game
        raycasterInitGame(RC_HARD);
    }
    else if (rc_scores == selected)
    {
        // Show some scores
        rc->mode = RC_SCORES;
        rc->difficulty = RC_EASY;
    }
}

/**
 * This is called whenever there is a screen to render
 * Draw the menu, game, game over, or high scores
 *
 * @return true if the screen should be drawn, false otherwise
 */
bool ICACHE_FLASH_ATTR raycasterRenderTask(void)
{
    static uint32_t tLastUs = 0; // time of current frame
    if(tLastUs == 0)
    {
        // Initialize the last call time
        tLastUs = system_get_time();
    }
    else
    {
        // Figure out how much time elapsed between the prior call and now
        uint32_t tNowUs = system_get_time();
        uint32_t tElapsedUs = tNowUs - tLastUs;
        tLastUs = tNowUs;

        // Draw something based on the mode
        switch(rc->mode)
        {
            default:
            case RC_MENU:
            {
                drawMenu(rc->menu);
                break;
            }
            case RC_GAME:
            {
                raycasterGameRenderer(tElapsedUs);
                break;
            }
            case RC_GAME_OVER:
            {
                raycasterDrawRoundOver(tElapsedUs);
                break;
            }
            case RC_SCORES:
            {
                raycasterDrawScores();
                break;
            }
        }
        return true;
    }
    return false;
}

/**
 * This function renders the scene and handles input. It is called whenever
 * the system is ready to render a new frame
 *
 * @param tElapsedUs The time since the last call
 */
void ICACHE_FLASH_ATTR raycasterGameRenderer(uint32_t tElapsedUs)
{
    // First handle button input
    handleRayInput(tElapsedUs);

    // Then move enemies around
    moveEnemies(tElapsedUs);

    // Tick down the timer to flash LEDs when shot or shooting
    if(rc->gotShotTimer > 0)
    {
        rc->gotShotTimer -= tElapsedUs;
    }
    if(rc->shotSomethingTimer > 0)
    {
        rc->shotSomethingTimer -= tElapsedUs;
    }
    if(rc->killedSpriteTimer > 0)
    {
        rc->killedSpriteTimer -= tElapsedUs;
    }

    // Cast all the rays for the scene and save the result
    rayResult_t rayResult[OLED_WIDTH] = {{0}};
    castRays(rayResult);

    // Clear the display, then draw all the layers
    clearDisplay();
    drawTextures(rayResult);
    drawOutlines(rayResult);
    drawSprites(rayResult);
    drawHUD();
}

/**
 * Cast all the rays into the scene, iterating across the X axis, and save the
 * results in the rayResult argument
 *
 * @param rayResult A pointer to an array of rayResult_t where this scene's
 *                  information is stored
 */
void ICACHE_FLASH_ATTR castRays(rayResult_t* rayResult)
{
    for(int32_t x = 0; x < OLED_WIDTH; x++)
    {
        // calculate ray position and direction
        // x-coordinate in camera space
        float cameraX = 2 * x / (float)OLED_WIDTH - 1;
        float rayDirX = rc->dirX + rc->planeX * cameraX;
        float rayDirY = rc->dirY + rc->planeY * cameraX;

        // which box of the map we're in
        int32_t mapX = (int32_t)(rc->posX);
        int32_t mapY = (int32_t)(rc->posY);

        // length of ray from current position to next x or y-side
        float sideDistX;
        float sideDistY;

        // length of ray from one x or y-side to next x or y-side
        float deltaDistX = (1 / rayDirX);
        if(deltaDistX < 0)
        {
            deltaDistX = -deltaDistX;
        }

        float deltaDistY = (1 / rayDirY);
        if(deltaDistY < 0)
        {
            deltaDistY = -deltaDistY;
        }

        // what direction to step in x or y-direction (either +1 or -1)
        int32_t stepX;
        int32_t stepY;

        int32_t hit = 0; // was there a wall hit?
        int32_t side; // was a NS or a EW wall hit?
        // calculate step and initial sideDist
        if(rayDirX < 0)
        {
            stepX = -1;
            sideDistX = (rc->posX - mapX) * deltaDistX;
        }
        else
        {
            stepX = 1;
            sideDistX = (mapX + 1.0 - rc->posX) * deltaDistX;
        }

        if(rayDirY < 0)
        {
            stepY = -1;
            sideDistY = (rc->posY - mapY) * deltaDistY;
        }
        else
        {
            stepY = 1;
            sideDistY = (mapY + 1.0 - rc->posY) * deltaDistY;
        }

        // perform DDA
        while (hit == 0)
        {
            // jump to next map square, OR in x-direction, OR in y-direction
            if(sideDistX < sideDistY)
            {
                sideDistX += deltaDistX;
                mapX += stepX;
                side = 0;
            }
            else
            {
                sideDistY += deltaDistY;
                mapY += stepY;
                side = 1;
            }

            // Check if ray has hit a wall
            if(worldMap[mapX][mapY] <= WMT_C)
            {
                hit = 1;
            }
        }

        // Calculate distance projected on camera direction
        // (Euclidean distance will give fisheye effect!)
        float perpWallDist;
        if(side == 0)
        {
            perpWallDist = (mapX - rc->posX + (1 - stepX) / 2) / rayDirX;
        }
        else
        {
            perpWallDist = (mapY - rc->posY + (1 - stepY) / 2) / rayDirY;
        }

        // Calculate height of line to draw on screen
        int32_t lineHeight;
        if(0 != perpWallDist)
        {
            lineHeight = (int32_t)(OLED_HEIGHT / perpWallDist);
        }
        else
        {
            lineHeight = 0;
        }


        // calculate lowest and highest pixel to fill in current stripe
        int32_t drawStart = -lineHeight / 2 + OLED_HEIGHT / 2;
        int32_t drawEnd = lineHeight / 2 + OLED_HEIGHT / 2;

        // Because of how drawStart and drawEnd are calculated, lineHeight needs to be recomputed
        lineHeight = drawEnd - drawStart;

        // Save a bunch of data to render the scene later
        rayResult[x].mapX = mapX;
        rayResult[x].mapY = mapY;
        rayResult[x].side = side;
        rayResult[x].drawEnd = drawEnd;
        rayResult[x].drawStart = drawStart;
        rayResult[x].perpWallDist = perpWallDist;
        rayResult[x].rayDirX = rayDirX;
        rayResult[x].rayDirY = rayDirY;
    }
}

/**
 * With the data in rayResult, render all the wall textures to the scene
 *
 * @param rayResult The information for all the rays cast
 */
void ICACHE_FLASH_ATTR drawTextures(rayResult_t* rayResult)
{
    for(int32_t x = 0; x < OLED_WIDTH; x++)
    {
        // For convenience
        uint8_t mapX      = rayResult[x].mapX;
        uint8_t mapY      = rayResult[x].mapY;
        uint8_t side      = rayResult[x].side;
        int16_t drawStart = rayResult[x].drawStart;
        int16_t drawEnd   = rayResult[x].drawEnd;

        // Only draw textures for walls and columns, not empty space or spawn points
        if(worldMap[mapX][mapY] <= WMT_C)
        {
            // Make sure not to waste any draws out-of-bounds
            if(drawStart < 0)
            {
                drawStart = 0;
            }
            else if(drawStart > OLED_HEIGHT)
            {
                drawStart = OLED_HEIGHT;
            }

            if(drawEnd < 0)
            {
                drawEnd = 0;
            }
            else if(drawEnd > OLED_HEIGHT)
            {
                drawEnd = OLED_HEIGHT;
            }

            // Pick a texture
            color* wallTex = NULL;
            switch(worldMap[mapX][mapY])
            {
                case WMT_W1:
                {
                    wallTex = rc->sinTex;
                    break;
                }
                case WMT_W2:
                {
                    wallTex = rc->brickTex;
                    break;
                }
                case WMT_W3:
                {
                    wallTex = rc->stripeTex;
                    break;
                }
                case WMT_C:
                {
                    wallTex = rc->stoneTex;
                    break;
                }
                default:
                case WMT_E:
                case WMT_S:
                {
                    // Empty spots don't have textures
                    break;
                }
            }

            // If there's no texture to draw, continue looping
            if(NULL == wallTex)
            {
                continue;
            }

            // calculate value of wallX, where exactly the wall was hit
            float wallX;
            if(side == 0)
            {
                wallX = rc->posY + rayResult[x].perpWallDist * rayResult[x].rayDirY;
            }
            else
            {
                wallX = rc->posX + rayResult[x].perpWallDist * rayResult[x].rayDirX;
            }
            wallX -= (int32_t)(wallX);

            // X coordinate on the texture. Make sure it's in bounds
            int32_t texX = (int32_t)(wallX * TEX_WIDTH);
            if(texX >= TEX_WIDTH)
            {
                texX = TEX_WIDTH - 1;
            }

            // Draw this texture's vertical stripe
            // Calculate how much to increase the texture coordinate per screen pixel
            int32_t lineHeight = rayResult[x].drawEnd - rayResult[x].drawStart;
            float step = TEX_HEIGHT / (float)lineHeight;
            // Starting texture coordinate
            float texPos = (drawStart - OLED_HEIGHT / 2 + lineHeight / 2) * step;
            for(int32_t y = drawStart; y < drawEnd; y++)
            {
                // Y coordinate on the texture. Round it, make sure it's in bounds
                int32_t texY = (int32_t)(texPos);
                if(texY >= TEX_HEIGHT)
                {
                    texY = TEX_HEIGHT - 1;
                }

                // Increment the texture position by the step size
                texPos += step;

                // Draw the pixel specified by the texture
                drawPixelUnsafeC(x, y, wallTex[(texX * TEX_HEIGHT) + texY ]);
            }
        }
    }
}

/**
 * Draw the outlines of all the walls and corners based on the cast ray info
 *
 * @param rayResult The information for all the rays cast
 */
void ICACHE_FLASH_ATTR drawOutlines(rayResult_t* rayResult)
{
    // Set starting points
    int16_t tLineStartX = 0;
    int16_t tLineStartY = rayResult[0].drawStart;
    int16_t bLineStartX = 0;
    int16_t bLineStartY = rayResult[0].drawEnd;

    // Iterate over all but the first and last strips of pixels
    for(int32_t x = 1; x < OLED_WIDTH - 1; x++)
    {
        // See if this strip is part of the previous or next strip's wall
        bool sameWallAsPrev = (rayResult[x].side == rayResult[x - 1].side) &&
                              (ABS(rayResult[x].mapX - rayResult[x - 1].mapX) + ABS(rayResult[x].mapY - rayResult[x - 1].mapY) < 2);
        bool sameWallAsNext = (rayResult[x].side == rayResult[x + 1].side) &&
                              (ABS(rayResult[x].mapX - rayResult[x + 1].mapX) + ABS(rayResult[x].mapY - rayResult[x + 1].mapY) < 2);

        // If this is a boundary
        if(!(sameWallAsNext && sameWallAsPrev))
        {
            // Always draw a vertical stripe
            speedyWhiteLine(x, rayResult[x].drawStart, x, rayResult[x].drawEnd, false);

            // If we didn't draw a vertical stripe in the previous x index
            // Or if we are drawing in the first two pixels
            if(tLineStartX != x - 1 || (0 == tLineStartX && 1 == x))
            {
                // Connect the top and bottom wall lines too
                speedyWhiteLine(tLineStartX, tLineStartY, x, rayResult[x].drawStart, false);
                speedyWhiteLine(bLineStartX, bLineStartY, x, rayResult[x].drawEnd, false);
            }

            // Save points for the next lines
            tLineStartX = x;
            tLineStartY = rayResult[x].drawStart;
            bLineStartX = x;
            bLineStartY = rayResult[x].drawEnd;
        }
    }

    // Draw the final top and bottom wall lines
    speedyWhiteLine(tLineStartX, tLineStartY, OLED_WIDTH - 1, rayResult[OLED_WIDTH - 1].drawStart, false);
    speedyWhiteLine(bLineStartX, bLineStartY, OLED_WIDTH - 1, rayResult[OLED_WIDTH - 1].drawEnd, false);
}

/**
 * Draw all the sprites
 *
 * @param rayResult The information for all the rays cast
 */
void ICACHE_FLASH_ATTR drawSprites(rayResult_t* rayResult)
{
    // Local memory for figuring out the draw order
    int32_t spriteOrder[NUM_SPRITES];
    float spriteDistance[NUM_SPRITES];

    // Track if any sprite was shot
    int16_t spriteIdxShot = -1;

    // sort sprites from far to close
    for(uint32_t i = 0; i < NUM_SPRITES; i++)
    {
        spriteOrder[i] = i;
        // sqrt not taken, unneeded
        spriteDistance[i] = ((rc->posX - rc->sprites[i].posX) * (rc->posX - rc->sprites[i].posX) +
                             (rc->posY - rc->sprites[i].posY) * (rc->posY - rc->sprites[i].posY));
    }
    sortSprites(spriteOrder, spriteDistance, NUM_SPRITES);

    // after sorting the sprites, do the projection and draw them
    for(uint32_t i = 0; i < NUM_SPRITES; i++)
    {
        // Skip over the sprite if posX is negative
        if(rc->sprites[spriteOrder[i]].posX < 0)
        {
            continue;
        }
        // translate sprite position to relative to camera
        float spriteX = rc->sprites[spriteOrder[i]].posX - rc->posX;
        float spriteY = rc->sprites[spriteOrder[i]].posY - rc->posY;

        // transform sprite with the inverse camera matrix
        // [ planeX dirX ] -1                                  [ dirY     -dirX ]
        // [             ]    =  1/(planeX*dirY-dirX*planeY) * [                ]
        // [ planeY dirY ]                                     [ -planeY planeX ]

        // required for correct matrix multiplication
        float invDet = 1.0 / (rc->planeX * rc->dirY - rc->dirX * rc->planeY);

        float transformX = invDet * (rc->dirY * spriteX - rc->dirX * spriteY);
        // this is actually the depth inside the screen, that what Z is in 3D
        float transformY = invDet * (-rc->planeY * spriteX + rc->planeX * spriteY);

        // If this is negative, the texture isn't going to be drawn, so just stop here
        if(transformY < 0)
        {
            continue;
        }

        int32_t spriteScreenX = (int32_t)((OLED_WIDTH / 2) * (1 + transformX / transformY));

        // calculate height of the sprite on screen
        // using 'transformY' instead of the real distance prevents fisheye
        int32_t spriteHeight = (int32_t)(OLED_HEIGHT / (transformY));
        if(spriteHeight < 0)
        {
            spriteHeight = -spriteHeight;
        }

        // calculate lowest and highest pixel to fill in current stripe
        int32_t drawStartY = -spriteHeight / 2 + OLED_HEIGHT / 2;
        if(drawStartY < 0)
        {
            drawStartY = 0;
        }

        int32_t drawEndY = spriteHeight / 2 + OLED_HEIGHT / 2;
        if(drawEndY > OLED_HEIGHT)
        {
            drawEndY = OLED_HEIGHT;
        }

        // calculate width of the sprite
        int32_t spriteWidth = ( (int32_t) (OLED_HEIGHT / (transformY)));
        if(spriteWidth < 0)
        {
            spriteWidth = -spriteWidth;
        }
        int32_t drawStartX = -spriteWidth / 2 + spriteScreenX;
        if(drawStartX < 0)
        {
            drawStartX = 0;
        }
        int32_t drawEndX = spriteWidth / 2 + spriteScreenX;
        if(drawEndX > OLED_WIDTH)
        {
            drawEndX = OLED_WIDTH;
        }

        // loop through every vertical stripe of the sprite on screen
        for(int32_t stripe = drawStartX; stripe < drawEndX; stripe++)
        {
            int32_t texX = (int32_t)(256 * (stripe - (-spriteWidth / 2 + spriteScreenX)) * TEX_WIDTH / spriteWidth) / 256;
            // the conditions in the if are:
            // 1) it's in front of camera plane so you don't see things behind you
            // 2) it's on the screen (left)
            // 3) it's on the screen (right)
            // 4) ZBuffer, with perpendicular distance
            if(transformY < rayResult[stripe].perpWallDist)
            {
                // for every pixel of the current stripe
                for(int32_t y = drawStartY; y < drawEndY; y++)
                {
                    // 256 and 128 factors to avoid floats
                    int32_t d = (y) * 256 - OLED_HEIGHT * 128 + spriteHeight * 128;
                    int32_t texY = ((d * TEX_HEIGHT) / spriteHeight) / 256;
                    // get current color from the texture
                    uint16_t texIdx = 0;

                    // If the sprite is mirrored, get the mirrored idx
                    if(rc->sprites[spriteOrder[i]].mirror)
                    {
                        texIdx = ((TEX_WIDTH - texX - 1) * TEX_HEIGHT) + texY;
                    }
                    else
                    {
                        texIdx = (texX * TEX_HEIGHT) + texY;
                    }

                    // draw the pixel for the texture
                    drawPixelUnsafeC(stripe, y, rc->sprites[spriteOrder[i]].texture[texIdx]);

                    // If we should check a shot, and a sprite is centered
                    if(true == rc->checkShot && (stripe == 63 || stripe == 64) &&
                            rc->sprites[spriteOrder[i]].health > 0)
                    {
                        // Mark that sprite as shot
                        spriteIdxShot = spriteOrder[i];
                    }
                }
            }
        }
    }

    // If you shot something
    if(spriteIdxShot >= 0)
    {
        // And it's fewer than six units away
        float distSqr = ((rc->sprites[spriteIdxShot].posX - rc->posX) * (rc->sprites[spriteIdxShot].posX - rc->posX)) +
                        ((rc->sprites[spriteIdxShot].posY - rc->posY) * (rc->sprites[spriteIdxShot].posY - rc->posY));
        if(distSqr < 36.0f)
        {
            // And it's not already getting shot or dead
            if(rc->sprites[spriteIdxShot].state != E_GOT_SHOT &&
                    rc->sprites[spriteIdxShot].state != E_DEAD)
            {
                // decrement health by one
                rc->sprites[spriteIdxShot].health--;
                // Animate getting shot
                setSpriteState(&(rc->sprites[spriteIdxShot]), E_GOT_SHOT);
                // Flash LEDs that we shot something
                rc->shotSomethingTimer = LED_ON_TIME;
            }
        }
    }

    // Shot was checked, so clear the boolean
    rc->checkShot = false;
}

/**
 * Bubble sort algorithm to which sorts both order and dist by the values in dist
 *
 * @param order  Sprite indices to be sorted by dist
 * @param dist   The distances from the camera to the sprites
 * @param amount The number of values to sort
 */
void ICACHE_FLASH_ATTR sortSprites(int32_t* order, float* dist, int32_t amount)
{
    // Iterate over everything
    for (int32_t i = 0; i < amount - 1; i++)
    {
        // Last i elements are already in place
        for (int32_t j = 0; j < amount - i - 1; j++)
        {
            // If the smaller distance is first
            if (dist[j] < dist[j + 1])
            {
                // Swap the dist[] elements
                float tmp = dist[j];
                dist[j] = dist[j + 1];
                dist[j + 1] = tmp;

                // And also swap the order[] elements
                int32_t tmp2 = order[j];
                order[j] = order[j + 1];
                order[j + 1] = tmp2;
            }
        }
    }
}

/**
 * Update the camera position based on the current button state
 * Also handle firing the gun
 *
 * @param tElapsedUs The time elapsed since the last call
 */
void ICACHE_FLASH_ATTR handleRayInput(uint32_t tElapsedUs)
{
    float frameTime = (tElapsedUs) / 1000000.0;

    // speed modifiers
    float moveSpeed = frameTime * 5.0; // the constant value is in squares/second
    float rotSpeed = frameTime * 3.0; // the constant value is in radians/second

    // move forward if no wall in front of you
    if(rc->rButtonState & 0x08)
    {
        if(worldMap[(int32_t)(rc->posX + rc->dirX * moveSpeed)][(int32_t)(rc->posY)] > WMT_C)
        {
            rc->posX += rc->dirX * moveSpeed;
        }
        if(worldMap[(int32_t)(rc->posX)][(int32_t)(rc->posY + rc->dirY * moveSpeed)] > WMT_C)
        {
            rc->posY += rc->dirY * moveSpeed;
        }
    }

    // move backwards if no wall behind you
    if(rc->rButtonState & 0x02)
    {
        if(worldMap[(int32_t)(rc->posX - rc->dirX * moveSpeed)][(int32_t)(rc->posY)] > WMT_C)
        {
            rc->posX -= rc->dirX * moveSpeed;
        }
        if(worldMap[(int32_t)(rc->posX)][(int32_t)(rc->posY - rc->dirY * moveSpeed)] > WMT_C)
        {
            rc->posY -= rc->dirY * moveSpeed;
        }
    }

    // rotate to the right
    if(rc->rButtonState & 0x04)
    {
        // both camera direction and camera plane must be rotated
        float oldDirX = rc->dirX;
        rc->dirX = rc->dirX * cos(-rotSpeed) - rc->dirY * sin(-rotSpeed);
        rc->dirY = oldDirX * sin(-rotSpeed) + rc->dirY * cos(-rotSpeed);
        float oldPlaneX = rc->planeX;
        rc->planeX = rc->planeX * cos(-rotSpeed) - rc->planeY * sin(-rotSpeed);
        rc->planeY = oldPlaneX * sin(-rotSpeed) + rc->planeY * cos(-rotSpeed);
    }

    // rotate to the left
    if(rc->rButtonState & 0x01)
    {
        // both camera direction and camera plane must be rotated
        float oldDirX = rc->dirX;
        rc->dirX = rc->dirX * cos(rotSpeed) - rc->dirY * sin(rotSpeed);
        rc->dirY = oldDirX * sin(rotSpeed) + rc->dirY * cos(rotSpeed);
        float oldPlaneX = rc->planeX;
        rc->planeX = rc->planeX * cos(rotSpeed) - rc->planeY * sin(rotSpeed);
        rc->planeY = oldPlaneX * sin(rotSpeed) + rc->planeY * cos(rotSpeed);
    }

    // Decrement the shot cooldown
    if(rc->shotCooldown > 0)
    {
        rc->shotCooldown -= tElapsedUs;
    }
    else
    {
        rc->shotCooldown = 0;
    }

    // Static bool keeps track of trigger state to require button releases to shoot
    static bool triggerPulled = false;

    // If the trigger hasn't been pulled, and the button was pressed, and cooldown isn't active
    if(!triggerPulled && (rc->rButtonState & 0x10) && 0 == rc->shotCooldown)
    {
        // Set the cooldown, tell drawSprites() to check the shot, and pull the trigger
        rc->shotCooldown = PLAYER_SHOT_COOLDOWN;
        rc->checkShot = true;
        triggerPulled = true;
    }
    else if(triggerPulled && (rc->rButtonState & 0x10) == 0)
    {
        // If the button was released, release the trigger
        triggerPulled = false;
    }
}

/**
 * Fast inverse square root. Works like magic. Ignore the warning
 *
 * See: https://en.wikipedia.org/wiki/Fast_inverse_square_root
 *
 * @param number The number to find 1/sqrt(x) for
 * @return 1/sqrt(x), or close enough
 */
float ICACHE_FLASH_ATTR Q_rsqrt( float number )
{
    const float x2 = number * 0.5F;
    const float threehalfs = 1.5F;

    union
    {
        float f;
        unsigned long i;
    } conv  = { .f = number };
    conv.i  = 0x5f3759df - ( conv.i >> 1 );
    conv.f  *= ( threehalfs - ( x2 * conv.f * conv.f ) );
    return conv.f;
}

/**
 * Move all sprites around and govern enemy behavior
 *
 * @param tElapsed The time elapsed since this was last called
 */
void ICACHE_FLASH_ATTR moveEnemies(uint32_t tElapsedUs)
{
    float frameTimeSec = (tElapsedUs) / 1000000.0;

    // Keep track of the closest live sprite
    rc->closestDist = 0xFFFFFFFF;
    rc->closestAngle = 0;

    // Figure out the movement speed for this frame
    float moveSpeed;
    switch(rc->difficulty)
    {
        default:
        case RC_NUM_DIFFICULTIES:
        case RC_EASY:
        {
            moveSpeed = frameTimeSec * 0.7f; // the constant value is in squares/second
            break;
        }
        case RC_MED:
        {
            moveSpeed = frameTimeSec * 1.0f;
            break;
        }
        case RC_HARD:
        {
            moveSpeed = frameTimeSec * 1.3f;
            break;
        }
    }

    // For each sprite
    int16_t closestIdx = -1;
    for(uint8_t i = 0; i < NUM_SPRITES; i++)
    {
        // Skip over sprites with negative position, these weren't spawned
        if(rc->sprites[i].posX < 0)
        {
            continue;
        }

        // Always run down the sprite's shot cooldown, regardless of state
        if(rc->sprites[i].shotCooldown > 0)
        {
            rc->sprites[i].shotCooldown -= tElapsedUs;
        }

        // Find the distance between this sprite and the player, generally useful
        float toPlayerX = rc->posX - rc->sprites[i].posX;
        float toPlayerY = rc->posY - rc->sprites[i].posY;
        float magSqr = (toPlayerX * toPlayerX) + (toPlayerY * toPlayerY);

        // Keep track of the closest live sprite
        if(rc->sprites[i].health > 0 && (uint32_t)magSqr < rc->closestDist)
        {
            rc->closestDist = (uint32_t)magSqr;
            closestIdx = i;
        }

        switch (rc->sprites[i].state)
        {
            default:
            case E_IDLE:
            {
                // If the sprite iss less than 8 units away (avoid the sqrt!)
                if(magSqr < 64)
                {
                    // Wake it up and have it hunt the player
                    setSpriteState(&(rc->sprites[i]), E_PICK_DIR_PLAYER);
                }
                break;
            }
            case E_PICK_DIR_RAND:
            {
                // Walking randomly is good if the sprite can't make a valid move
                // Get a random unit vector in the first quadrant
                rc->sprites[i].dirX = (os_random() % 90) / 90.0f;
                rc->sprites[i].dirY = 1 / Q_rsqrt(1 - (rc->sprites[i].dirX * rc->sprites[i].dirX));

                // Randomize the unit vector quadrant
                switch(os_random() % 4)
                {
                    default:
                    case 0:
                    {
                        break;
                    }
                    case 1:
                    {
                        rc->sprites[i].dirX = -rc->sprites[i].dirX;
                        break;
                    }
                    case 2:
                    {
                        rc->sprites[i].dirY = -rc->sprites[i].dirY;
                        break;
                    }
                    case 3:
                    {
                        rc->sprites[i].dirX = -rc->sprites[i].dirX;
                        rc->sprites[i].dirY = -rc->sprites[i].dirY;
                        break;
                    }
                }

                // Have the sprite move foward
                rc->sprites[i].isBackwards = false;

                // And let the sprite walk for a bit
                setSpriteState(&(rc->sprites[i]), E_WALKING);
                // Walk for a little extra in the random direction
                rc->sprites[i].stateTimer = LONG_WALK_ANIM_TIME;
                break;
            }
            case E_PICK_DIR_PLAYER:
            {
                // Either pick a directon to walk towards the player, or take a shot

                // Randomly take a shot
                if(rc->sprites[i].shotCooldown <= 0 &&           // If the sprite is not in cooldown
                        (uint32_t)magSqr < 64 &&                 // If the player is no more than 8 units away
                        ((os_random() % 64) > (uint32_t)magSqr)) // And distance-weighted RNG says so
                {
                    // Take the shot!
                    setSpriteState(&(rc->sprites[i]), E_SHOOTING);
                }
                else // Pick a direction to walk in
                {
                    // Normalize the vector
                    float invSqr = Q_rsqrt(magSqr);
                    toPlayerX *= invSqr;
                    toPlayerY *= invSqr;

                    // Rotate the vector, maybe
                    switch(os_random() % 3)
                    {
                        default:
                        case 0:
                        {
                            // Straight ahead!
                            break;
                        }
                        case 1:
                        {
                            // Rotate 45 degrees
                            toPlayerX = toPlayerX * 0.70710678f - toPlayerY * 0.70710678f;
                            toPlayerY = toPlayerY * 0.70710678f + toPlayerX * 0.70710678f;
                            break;
                        }
                        case 2:
                        {
                            // Rotate -45 degrees
                            toPlayerX = toPlayerX * 0.70710678f + toPlayerY * 0.70710678f;
                            toPlayerY = toPlayerY * 0.70710678f - toPlayerX * 0.70710678f;
                            break;
                        }
                    }

                    // Set the sprite's direction
                    rc->sprites[i].dirX = toPlayerX;
                    rc->sprites[i].dirY = toPlayerY;
                    rc->sprites[i].isBackwards = false;

                    // And let the sprite walk for a bit
                    setSpriteState(&(rc->sprites[i]), E_WALKING);
                }
                break;
            }
            case E_WALKING:
            {
                // This is just when the sprite is walking in a straight line

                // See if we're done walking
                rc->sprites[i].stateTimer -= tElapsedUs;
                if (rc->sprites[i].stateTimer <= 0)
                {
                    // When done walking, pick a new direction
                    setSpriteState(&(rc->sprites[i]), E_PICK_DIR_PLAYER);
                    break;
                }

                // Mirror sprites for a walking animation
                rc->sprites[i].texTimer -= tElapsedUs;
                while(rc->sprites[i].texTimer < 0)
                {
                    // Animate a 'step'
                    rc->sprites[i].texTimer += STEP_ANIM_TIME;
                    // Pick the next texture
                    rc->sprites[i].texFrame = (rc->sprites[i].texFrame + 1) % NUM_WALK_FRAMES;
                    rc->sprites[i].texture = rc->walk[rc->sprites[i].texFrame];
                    // Mirror the texture if we're back to 0
                    if(0 == rc->sprites[i].texFrame)
                    {
                        rc->sprites[i].mirror = !rc->sprites[i].mirror;
                    }
                }

                // Find the new position
                // If the sprite is really close and not walking backwards
                if(magSqr < 0.5f && false == rc->sprites[i].isBackwards)
                {
                    // walk backwards instead
                    rc->sprites[i].dirX = -rc->sprites[i].dirX;
                    rc->sprites[i].dirY = -rc->sprites[i].dirY;
                    rc->sprites[i].isBackwards = true;
                }
                // If the sprite is far enough away and walking backwards
                else if(magSqr > 1.5f && true == rc->sprites[i].isBackwards)
                {
                    // Walk forwards again
                    rc->sprites[i].dirX = -rc->sprites[i].dirX;
                    rc->sprites[i].dirY = -rc->sprites[i].dirY;
                    rc->sprites[i].isBackwards = false;
                }

                // Find the new position
                float newPosX = rc->sprites[i].posX + (rc->sprites[i].dirX * moveSpeed);
                float newPosY = rc->sprites[i].posY + (rc->sprites[i].dirY * moveSpeed);

                // Integer-ify it
                int newPosXi = newPosX;
                int newPosYi = newPosY;

                // Make sure the new position is in bounds, assume invalid
                bool moveIsValid = false;
                if(     (0 <= newPosXi && newPosXi < MAP_WIDTH) &&
                        (0 <= newPosYi && newPosYi < MAP_HEIGHT) &&
                        // And that it's not occupied by a wall or column
                        (worldMap[newPosXi][newPosYi] > WMT_C))
                {
                    // In bounds, so it's valid so far
                    moveIsValid = true;

                    // Make sure the new square is unoccupied
                    for(uint8_t oth = 0; oth < NUM_SPRITES; oth++)
                    {
                        // If some other sprite is in the new square
                        if((oth != i) &&
                                (int)rc->sprites[oth].posX == newPosXi &&
                                (int)rc->sprites[oth].posY == newPosYi )
                        {
                            // Occupied, so this move is invalid
                            moveIsValid = false;
                            break;
                        }
                    }
                }

                // If the move is valid, move there
                if(moveIsValid)
                {
                    rc->sprites[i].posX = newPosX;
                    rc->sprites[i].posY = newPosY;
                }
                else
                {
                    // Else pick a new direction, totally random
                    setSpriteState(&(rc->sprites[i]), E_PICK_DIR_RAND);
                }
                break;
            }
            case E_SHOOTING:
            {
                // Animate a shot
                rc->sprites[i].texTimer -= tElapsedUs;
                while(rc->sprites[i].texTimer < 0)
                {
                    rc->sprites[i].texTimer += SHOOTING_ANIM_TIME;

                    // Pick the next texture
                    rc->sprites[i].texFrame = (rc->sprites[i].texFrame + 1) % NUM_SHOT_FRAMES;
                    rc->sprites[i].texture = rc->shooting[rc->sprites[i].texFrame];

                    // Reset if we're back to zero
                    if(0 == rc->sprites[i].texFrame)
                    {
                        // After the shot, go to E_PICK_DIR_PLAYER
                        setSpriteState(&(rc->sprites[i]), E_PICK_DIR_PLAYER);
                    }
                    // If we're halfway through the animation
                    else if (NUM_SHOT_FRAMES / 2 == rc->sprites[i].texFrame)
                    {
                        // Actually take the shot
                        rc->sprites[i].shotCooldown = ENEMY_SHOT_COOLDOWN;

                        // Check if the sprite can still see the player
                        if(checkLineToPlayer(&rc->sprites[i], rc->posX, rc->posY))
                        {
                            // If it can, the player got shot
                            rc->health--;
                            rc->gotShotTimer = LED_ON_TIME;

                            // If the player is out of health
                            if(rc->health == 0)
                            {
                                // End the round
                                raycasterEndRound();
                            }
                        }
                    }
                }
                break;
            }
            case E_GOT_SHOT:
            {
                // Animate getting shot
                rc->sprites[i].texTimer -= tElapsedUs;
                while(rc->sprites[i].texTimer < 0)
                {
                    rc->sprites[i].texTimer += GOT_SHOT_ANIM_TIME;

                    // Pick the next texture
                    rc->sprites[i].texFrame = (rc->sprites[i].texFrame + 1) % NUM_HURT_FRAMES;
                    rc->sprites[i].texture = rc->hurt[rc->sprites[i].texFrame];

                    // Reset if we're back to zero
                    if(0 == rc->sprites[i].texFrame)
                    {
                        // If the enemy has any health
                        if(rc->sprites[i].health > 0)
                        {
                            // Go back to E_PICK_DIR_PLAYER
                            setSpriteState(&(rc->sprites[i]), E_PICK_DIR_PLAYER);
                        }
                        else
                        {
                            // If there is no health, go to E_DEAD
                            setSpriteState(&(rc->sprites[i]), E_DEAD);
                        }
                    }
                }
                break;
            }
            case E_DEAD:
            {
                // Do nothing, ya dead
                break;
            }
        }
    }

    // If there is a nearby sprite
    if(closestIdx >= 0)
    {
        // Find the angle between the 'straight ahead' vector and the nearest sprite
        // Use rc->dirX, rc->dirY as vector straight ahead
        // This is the vector to the closest sprite
        float xClosest = (rc->sprites[closestIdx].posX - rc->posX);
        float yClosest = (rc->sprites[closestIdx].posY - rc->posY);

        // Find the angle between the two vectors
        rc->closestAngle = acosf(
                               ((rc->dirX * xClosest) + (rc->dirY * yClosest)) *        // Dot Product
                               Q_rsqrt((xClosest * xClosest) + (yClosest * yClosest))); // (1 / distance to sprite)
        // The magnitude of the 'straight ahead' vector is always 1, so it's left out of the denominator

        // Check if the sprite is to the left or right of us
        if(((rc->dirX) * (yClosest) - (rc->dirY) * (xClosest)) > 0)
        {
            // If it's to the left, rotate the angle to the pi->2*pi range
            rc->closestAngle = (2 * M_PI) - rc->closestAngle;
        }
    }
}

/**
 * Use DDA to draw a line between the player and a sprite, and check if there
 * are any walls between the two. This line drawing algorithm is the same one
 * used to cast rays from the player to walls
 *
 * @param sprite The sprite to draw a line from
 * @param pX     The player's X position
 * @param pY     The player's Y position
 * @return true  if there is a clear line between the player and sprite,
 *         false if there is an obstruction
 */
bool ICACHE_FLASH_ATTR checkLineToPlayer(raySprite_t* sprite, float pX, float pY)
{
    // If the sprite and the player are in the same cell
    if(((int32_t)sprite->posX == (int32_t)pX) &&
            ((int32_t)sprite->posY == (int32_t)pY))
    {
        // We can definitely draw a line between the two
        return true;
    }

    // calculate ray position and direction
    // x-coordinate in camera space
    float rayDirX = sprite->posX - pX;
    float rayDirY = sprite->posY - pY;

    // which box of the map we're in
    int32_t mapX = (int32_t)(pX);
    int32_t mapY = (int32_t)(pY);

    // length of ray from current position to next x or y-side
    float sideDistX;
    float sideDistY;

    // length of ray from one x or y-side to next x or y-side
    float deltaDistX = (1 / rayDirX);
    if(deltaDistX < 0)
    {
        deltaDistX = -deltaDistX;
    }

    float deltaDistY = (1 / rayDirY);
    if(deltaDistY < 0)
    {
        deltaDistY = -deltaDistY;
    }

    // what direction to step in x or y-direction (either +1 or -1)
    int32_t stepX;
    int32_t stepY;

    // calculate step and initial sideDist
    if(rayDirX < 0)
    {
        stepX = -1;
        sideDistX = (pX - mapX) * deltaDistX;
    }
    else
    {
        stepX = 1;
        sideDistX = (mapX + 1.0 - pX) * deltaDistX;
    }

    if(rayDirY < 0)
    {
        stepY = -1;
        sideDistY = (pY - mapY) * deltaDistY;
    }
    else
    {
        stepY = 1;
        sideDistY = (mapY + 1.0 - pY) * deltaDistY;
    }

    // perform DDA until a wall is hit or the ray reaches the sprite
    while (true)
    {
        // jump to next map square, OR in x-direction, OR in y-direction
        if(sideDistX < sideDistY)
        {
            sideDistX += deltaDistX;
            mapX += stepX;
        }
        else
        {
            sideDistY += deltaDistY;
            mapY += stepY;
        }

        // Check if ray has hit a wall
        if(worldMap[mapX][mapY] <= WMT_C)
        {
            // There is a wall between the player and the sprite
            return false;
        }
        else if(mapX == (int32_t)sprite->posX && mapY == (int32_t)sprite->posY)
        {
            // Ray reaches from the player to the sprite unobstructed
            return true;
        }
    }
    // Shouldn't reach here
    return false;
}

/**
 * Set the sprite state and associated timers and textures
 *
 * @param sprite The sprite to set state for
 * @param state  The state to set
 */
void ICACHE_FLASH_ATTR setSpriteState(raySprite_t* sprite, enemyState_t state)
{
    // See if the sprite had been in a walking state
    // When transitioning between 'walking' states, don't reset texture indices
    bool wasWalking;
    switch(sprite->state)
    {
        default:
        case E_IDLE:
        case E_PICK_DIR_PLAYER:
        case E_PICK_DIR_RAND:
        case E_WALKING:
        {
            wasWalking = true;
            break;
        }
        case E_SHOOTING:
        case E_GOT_SHOT:
        case E_DEAD:
        {
            wasWalking = false;
        }
    }

    // Set the state, reset the texture frame
    sprite->state = state;
    sprite->texFrame = 0;

    // Set timers and textures
    switch(state)
    {
        default:
        case E_IDLE:
        case E_PICK_DIR_PLAYER:
        case E_PICK_DIR_RAND:
        {
            // These state are transient, so the timer is 0
            sprite->stateTimer = 0;
            // If the sprite wasn't walking before, or isn't initialized
            if(!wasWalking || NULL == sprite->texture)
            {
                // Set up the walking texture
                sprite->texture = rc->walk[0];
                sprite->mirror = false;
                sprite->texTimer = 0;
            }
            break;
        }
        case E_WALKING:
        {
            // Walk for WALK_ANIM_TIME
            sprite->stateTimer = WALK_ANIM_TIME;
            // If the sprite wasn't walking before, or isn't initialized
            if(!wasWalking || NULL == sprite->texture)
            {
                // Set up the walking texture
                sprite->texture = rc->walk[0];
                sprite->mirror = false;
                sprite->texTimer = STEP_ANIM_TIME;
            }
            break;
        }
        case E_SHOOTING:
        {
            // Set up the shooting texture
            sprite->stateTimer = SHOOTING_ANIM_TIME;
            sprite->texture = rc->shooting[0];
            sprite->texTimer = SHOOTING_ANIM_TIME;
            break;
        }
        case E_GOT_SHOT:
        {
            // Set up the got shot texture
            sprite->stateTimer = GOT_SHOT_ANIM_TIME;
            sprite->texture = rc->hurt[0];
            sprite->texTimer = GOT_SHOT_ANIM_TIME;
            break;
        }
        case E_DEAD:
        {
            // If the sprite is dead, decrement live sprite and increment kills
            rc->liveSprites--;
            rc->killedSpriteTimer = LED_ON_TIME;
            rc->kills++;
            // If there are no sprites left
            if(0 == rc->liveSprites)
            {
                // End the round
                raycasterEndRound();
            }

            // ALso set up the corpse texture
            sprite->stateTimer = 0;
            sprite->texture = rc->dead;
            sprite->texTimer = 0;
        }
    }
}

/**
 * Draw the HUD, including the weapon and health
 */
void ICACHE_FLASH_ATTR drawHUD(void)
{
    // Figure out width for note display
    char notes[8] = {0};
    ets_snprintf(notes, sizeof(notes) - 1, "%d", rc->liveSprites);
    int16_t noteWidth = textWidth(notes, IBM_VGA_8);

    // Clear area behind note display
    fillDisplayArea(0, OLED_HEIGHT - FONT_HEIGHT_IBMVGA8 - 1,
                    noteWidth + rc->mnote.width + 1, OLED_HEIGHT,
                    BLACK);

    // Draw note display
    if(rc->killedSpriteTimer <= 0)
    {
        // Blink the note when an enemy is defeated
        drawPng(&(rc->mnote),
                0,
                OLED_HEIGHT - rc->mnote.height,
                false, false, 0);
    }
    plotText(rc->mnote.width + 2,
             OLED_HEIGHT - FONT_HEIGHT_IBMVGA8,
             notes, IBM_VGA_8, WHITE);

    // Figure out width for health display
    char health[8] = {0};
    ets_snprintf(health, sizeof(health) - 1, "%d", rc->health);
    int16_t healthWidth = textWidth(health, IBM_VGA_8);
    int16_t healthDrawX = OLED_WIDTH - rc->heart.width - 1 - healthWidth;

    // Clear area behind health display
    fillDisplayArea(healthDrawX - 1, OLED_HEIGHT - FONT_HEIGHT_IBMVGA8 - 1,
                    OLED_WIDTH, OLED_HEIGHT,
                    BLACK);

    // Draw health display
    if(rc->gotShotTimer <= 0)
    {
        // Blink the heart when getting shot, same as the red LED
        drawPng(&(rc->heart),
                OLED_WIDTH - rc->heart.width,
                OLED_HEIGHT - rc->heart.height,
                false, false, 0);
    }
    plotText(healthDrawX,
             OLED_HEIGHT - FONT_HEIGHT_IBMVGA8,
             health, IBM_VGA_8, WHITE);

    // Draw Guitar
    if(0 == rc->shotCooldown)
    {
        // This is just the 0th frame, not shooting
        drawPngSequence(&(rc->gtr),
                        (OLED_WIDTH - rc->gtr.handles->width) / 2,
                        (OLED_HEIGHT - rc->gtr.handles->height),
                        false, false, 0, 0);
    }
    else
    {
        // Get the frame index from the shotCooldown count
        int8_t idx = (rc->gtr.count * (PLAYER_SHOT_COOLDOWN - rc->shotCooldown)) / PLAYER_SHOT_COOLDOWN;
        // Make sure this is in bounds
        if(idx >= rc->gtr.count)
        {
            idx = rc->gtr.count - 1;
        }
        // Draw the guitar frame
        drawPngSequence(&(rc->gtr),
                        (OLED_WIDTH - rc->gtr.handles->width) / 2,
                        (OLED_HEIGHT - rc->gtr.handles->height),
                        false, false, 0, idx);
    }
}

/**
 * Called when the round ends
 */
void ICACHE_FLASH_ATTR raycasterEndRound(void)
{
    // See how long this round took
    rc->tRoundElapsed = system_get_time() - rc->tRoundStartedUs;

    // Save the score
    addRaycasterScore(rc->difficulty, rc->kills, rc->tRoundElapsed);

    // Show game over screen, disable radar
    rc->mode = RC_GAME_OVER;
    rc->closestDist = 0xFFFFFFFF;
    rc->closestAngle = 0;
}

/**
 * Display the time elapsed and number of kills
 *
 * @param tElapsedUs Time elapsed since the last call
 */
void ICACHE_FLASH_ATTR raycasterDrawRoundOver(uint32_t tElapsedUs)
{
    // Break down the elapsed time in microseconds to human readable numbers
    uint32_t dSec = (rc->tRoundElapsed / 100000) % 10;
    uint32_t sec  = (rc->tRoundElapsed / 1000000) % 60;
    uint32_t min  = (rc->tRoundElapsed / (1000000 * 60));

    // Create the time and kills string
    char timestr[64] = {0};
    ets_snprintf(timestr, sizeof(timestr), "Time: %02d:%02d.%d", min, sec, dSec);
    char killstr[64] = {0};
    ets_snprintf(killstr, sizeof(killstr), "Kills: %d", rc->kills);

    clearDisplay();

    // Plot a title depending on if all enemies were killed or not
    uint16_t width;
    if(rc->liveSprites > 0)
    {
        width = textWidth("YOU LOSE", RADIOSTARS);
        plotText((OLED_WIDTH - width) / 2, 0, "YOU LOSE", RADIOSTARS, WHITE);
    }
    else
    {
        width = textWidth("YOU WIN!", RADIOSTARS);
        plotText((OLED_WIDTH - width) / 2, 0, "YOU WIN!", RADIOSTARS, WHITE);
    }

    // Plot the round stats
    width = textWidth(killstr, IBM_VGA_8);
    plotText((OLED_WIDTH - width) / 2, (FONT_HEIGHT_RADIOSTARS + 5), killstr, IBM_VGA_8, WHITE);
    width = textWidth(timestr, IBM_VGA_8);
    plotText((OLED_WIDTH - width) / 2, (FONT_HEIGHT_RADIOSTARS + 5) + (FONT_HEIGHT_IBMVGA8 + 3), timestr, IBM_VGA_8, WHITE);

    // Draw the HUD, with animation!
    if(0 == rc->shotCooldown)
    {
        rc->shotCooldown = PLAYER_SHOT_COOLDOWN;
    }
    else if(rc->shotCooldown > 0)
    {
        rc->shotCooldown -= tElapsedUs;
    }
    else
    {
        rc->shotCooldown = 0;
    }
    drawHUD();
}

/**
 * @brief Drive LEDs based on game state
 *
 * @param arg unused
 */
void ICACHE_FLASH_ATTR raycasterLedTimer(void* arg __attribute__((unused)))
{
    led_t leds[NUM_LIN_LEDS] = {{0}};

    // If we were shot, flash red
    if(rc->gotShotTimer > 0)
    {
        for(uint8_t i = 0; i < NUM_LIN_LEDS; i++)
        {
            leds[i].r = (rc->gotShotTimer * 0x40) / LED_ON_TIME;
        }
    }
    // Otherwise if we shot somethhing, flash green
    else if(rc->shotSomethingTimer > 0)
    {
        for(uint8_t i = 0; i < NUM_LIN_LEDS; i++)
        {
            leds[i].g = (rc->shotSomethingTimer * 0x40) / LED_ON_TIME;
        }
    }
    // Otherwise use the LEDs like a radar
    else if(rc->closestDist < 64) // This is the squared dist, so check for radius 8
    {
        // Associate each LED with a radian angle around the circle
        float ledAngles[NUM_LIN_LEDS] =
        {
            3.665191f,
            4.712389f,
            5.759587f,
            0.523599f,
            1.570796f,
            2.617994f,
        };

        // For each LED
        for(uint8_t i = 0; i < NUM_LIN_LEDS; i++)
        {
            // Find the distance between this LED's angle and the angle to the nearest enemy
            float dist = ABS(rc->closestAngle - ledAngles[i]);
            if(dist > M_PI)
            {
                dist = (2 * M_PI) - dist;
            }

            // If the distance is small enough
            if(dist < 1)
            {
                // Light this LED proportional to the player distance to the sprite
                // and the LED distance to the angle
                leds[i].b = (4 * (64 - rc->closestDist)) * (1 - dist);
            }
        }
    }

    // If we're at a quarter health or less
    if(rc->mode == RC_GAME && rc->health <= rc->initialHealth / 4)
    {
        // Increment or decrement the red warning LED
        if(true == rc->healthWarningInc)
        {
            rc->healthWarningTimer++;
            if(rc->healthWarningTimer == 0x80)
            {
                rc->healthWarningInc = false;
            }
        }
        else
        {
            rc->healthWarningTimer--;
            if(rc->healthWarningTimer == 0)
            {
                rc->healthWarningInc = true;
            }
        }

        // Pulse two LEDs red as a warning, reduce G and B
        leds[0].r = rc->healthWarningTimer;
        leds[0].g /= 4;
        leds[0].b /= 4;
        leds[NUM_LIN_LEDS - 1].r = rc->healthWarningTimer;
        leds[NUM_LIN_LEDS - 1].g /= 4;
        leds[NUM_LIN_LEDS - 1].b /= 4;
    }

    // Push out the LEDs
    setLeds(leds, sizeof(leds));
}

/**
 * Display the high scores
 */
void ICACHE_FLASH_ATTR raycasterDrawScores(void)
{
    clearDisplay();

    // Plot title
    switch(rc->difficulty)
    {
        default:
        case RC_NUM_DIFFICULTIES:
        case RC_EASY:
        {
            uint16_t width = textWidth(rc_easy, RADIOSTARS);
            plotText((OLED_WIDTH - width) / 2, 0, rc_easy, RADIOSTARS, WHITE);
            break;
        }
        case RC_MED:
        {
            uint16_t width = textWidth(rc_med, RADIOSTARS);
            plotText((OLED_WIDTH - width) / 2, 0, rc_med, RADIOSTARS, WHITE);
            break;
        }
        case RC_HARD:
        {
            uint16_t width = textWidth(rc_hard, RADIOSTARS);
            plotText((OLED_WIDTH - width) / 2, 0, rc_hard, RADIOSTARS, WHITE);
            break;
        }
    }

    // Display scores
    raycasterScores_t* s = getRaycasterScores();
    for(uint8_t i = 0; i < RC_NUM_SCORES; i++)
    {
        // If this is a valid score
        if(s->scores[rc->difficulty][i].kills > 0)
        {
            // Make the time human readable
            uint32_t dSec = (s->scores[rc->difficulty][i].tElapsedUs / 100000) % 10;
            uint32_t sec  = (s->scores[rc->difficulty][i].tElapsedUs / 1000000) % 60;
            uint32_t min  = (s->scores[rc->difficulty][i].tElapsedUs / (1000000 * 60));

            // Print the string to an array
            char scoreStr[64] = {0};
            ets_snprintf(scoreStr, sizeof(scoreStr), "%2d in %02d:%02d.%d",
                         s->scores[rc->difficulty][i].kills,
                         min, sec, dSec);

            // Plot the string
            plotText(0, FONT_HEIGHT_RADIOSTARS + 3 + (i * (FONT_HEIGHT_IBMVGA8 + 3)),
                     scoreStr, IBM_VGA_8, WHITE);
        }
    }
}
