#include <libdragon.h>
#include <math.h>

static sprite_t *background_sprite;
static sprite_t *brew_sprite;
static sprite_t *ball_sprite;
static sprite_t *net_sprite;
static rspq_block_t* background_block;

static xm64player_t sfx_music;
static wav64_t sfx_hit;
static wav64_t sfx_halt;
static wav64_t sfx_win;

enum {
    FONT_PACIFICO = 1,
};

typedef struct {
    float x;
    float y;
} vector2d_t;

typedef struct {
    vector2d_t pos;
    vector2d_t dir;
    vector2d_t normalized;
    float length;
} collision_t;

typedef struct {
    float x;
    float y;
    float dx;
    float dy;
    float scale_factor;
} object_t;

#define NUM_BLOBS 2
#define INITIAL_COUNTDOWN 3
#define MAX_POINTS 21

#define FRAMERATE 60
#define AIR_FRICTION_FACTOR 0.99f
#define GROUND_FRICTION_FACTOR 0.9f
#define GRAVITY_FACTOR 9.81f
#define SPEED_EPSILON 1e-1
#define POSITION_EPSILON 10

static object_t blobs[NUM_BLOBS];
static object_t ball;
static object_t net;

static int32_t obj_min_x;
static int32_t obj_max_x;
static int32_t obj_min_y;
static int32_t obj_max_y;
static int32_t cur_tick = 0;

// DEBUG collisions
static collision_t collisions[NUM_BLOBS];

static int scorePlayer1 = 0;
static int scorePlayer2 = 0;
static int lastPlayer = -1;
static int hitCount = 0;
static int countdown = 0;
uint64_t startTime = 0;

// Mixer channel allocation
#define CHANNEL_SFX1    0
#define CHANNEL_SFX2    2
#define CHANNEL_SFX3    4
#define CHANNEL_MUSIC   6


void init_player(uint32_t i) {
    uint32_t display_width = display_get_width();
    object_t* obj = &blobs[i];
    obj->x = i == 0 ? 40 : display_width - brew_sprite->width - 40;
    obj->y = obj_max_y - brew_sprite->height;
    obj->dx = 0;
    obj->dy = 0;
    obj->scale_factor = 1.0f;
}

bool rectRect(float r1x, float r1y, float r1w, float r1h, float r2x, float r2y, float r2w, float r2h) {
  return (r1x + r1w >= r2x &&    // r1 right edge past r2 left
          r1x <= r2x + r2w &&    // r1 left edge past r2 right
          r1y + r1h >= r2y &&    // r1 top edge past r2 bottom
          r1y <= r2y + r2h);     // r1 bottom edge past r2 top
}

collision_t circleRect(float cx, float cy, float radius, float rx, float ry, float rw, float rh) {
  float nearestX = cx;
  float nearestY = cy;

  // which edge is closest?
  if (cx < rx)         nearestX = rx;      // test left edge
  else if (cx > rx+rw) nearestX = rx+rw;   // right edge
  if (cy < ry)         nearestY = ry;      // top edge
  else if (cy > ry+rh) nearestY = ry+rh;   // bottom edge

  // get distance from closest edges
  float distX = cx - nearestX;
  float distY = cy - nearestY;
  float distance = sqrt( (distX*distX) + (distY*distY) );

  // if the distance is less than the radius, collision!
  vector2d_t pos = {nearestX, nearestY};
  vector2d_t dir = {distX, distY};
  vector2d_t normal = {0, 0};
  if (distance > 0 && distance <= radius) {
    normal.x = distX/distance;
    normal.y = distY/distance;
  }
  collision_t retval = {pos, dir, normal, distance};
  return retval;
}

void applyScreenLimits(float x, float y, float w, float h, float dx, float dy, object_t* obj) {
    float next_x = x + dx;
    float next_y = y + dy;

    if (next_x + w >= obj_max_x) {
        next_x = obj_max_x - (next_x + w - obj_max_x) - w;
        obj->dx = -1.0 * dx;
    }
    if (next_x < obj_min_x) {
        next_x = obj_min_x + (obj_min_x - next_x);
        obj->dx = -1.0 * dx;
    }
    if (next_y + h >= obj_max_y) {
        next_y = obj_max_y - (next_y + h - obj_max_y) - h + 1;
        obj->dy = -1.0 * dy / 2;
    }
    if (next_y < obj_min_y) {
        next_y = obj_min_y + (obj_min_x - next_y);
        obj->dy = -1.0 * dy;
    }
    
    obj->x = next_x;
    obj->y = next_y;
}

void applyScreenLimitsRect(object_t* obj, sprite_t* sprite) {
    applyScreenLimits(obj->x, obj->y, sprite->width, sprite->height, obj->dx, obj->dy, obj);
}

void applyScreenLimitsCircle(object_t* obj, sprite_t* sprite) {
    applyScreenLimits(obj->x - sprite->width/2, obj->y - sprite->height/2, sprite->width, sprite->height, obj->dx, obj->dy, obj);
    obj->x += sprite->width/2;
    obj->y += sprite->height/2;
}

void applyFriction(object_t* obj) {
    if (obj->dx != 0) {
        if (fabs(obj->dx) < SPEED_EPSILON) {
            obj->dx = 0;
        } else {
            float factor = (obj->y < obj_max_y) ? AIR_FRICTION_FACTOR : GROUND_FRICTION_FACTOR;
            float next_dx = fabs(obj->dx) * factor;
            if (obj->dx < 0) {
                obj->dx = -1.0f * next_dx;
            } else {
                obj->dx = next_dx;
            }
        }
    }
}

void applyGravity(object_t* obj) {
    if (obj->dy > 0 && obj->dy < SPEED_EPSILON && (obj_max_y - fabs(obj->y)) < POSITION_EPSILON) {
        obj->dy = 0;
        obj->y = obj_max_y;
    } else if (obj->y < obj_max_y - ball_sprite->height) {
        float next_dy = obj->dy + (GRAVITY_FACTOR / FRAMERATE);
        obj->dy = next_dy;
    }
}

int get_winner() {
    return (scorePlayer1 >= MAX_POINTS && (scorePlayer1 - scorePlayer2) > 1)
        ? 1
        : (scorePlayer2 >= MAX_POINTS && (scorePlayer2 - scorePlayer1) > 1)
            ? 2
            : 0;
}

bool in_play() {
    return countdown == 0 && !get_winner();
}

void update(int ovfl)
{
    if (!in_play()) {
        uint64_t now = timer_ticks() / (TICKS_PER_SECOND / 1000);
        uint64_t elapsed = now < startTime ? (now + (91625 - startTime)) : (now - startTime);
        countdown = INITIAL_COUNTDOWN - (elapsed / 1000);
        // New game
        if (countdown == 0 && !in_play()) {
            scorePlayer1 = 0;
            scorePlayer2 = 0;
            countdown = INITIAL_COUNTDOWN;
            startTime = timer_ticks() / (TICKS_PER_SECOND / 1000);
        }
        return;
    }

    // Ball
    // Ball hits ground ???
    if (ball.y + ball.dy + ball_sprite->height/2 >= obj_max_y) {
        // Sound FX
        wav64_play(&sfx_halt, CHANNEL_SFX2);
        uint32_t display_width = display_get_width();
        // score + no more hits
        if (ball.x > net.x) {
            scorePlayer1++;
            ball.x = display_width / 4.0f;
        } else {
            scorePlayer2++;
            ball.x = 3.0 * (display_width / 4.0f);
        }
        ball.y = obj_min_y + ball_sprite->height/2;
        ball.dx = 0;
        ball.dy = 0;
        hitCount = 0;
        lastPlayer = -1;
        // relocate players
        for (uint32_t i = 0; i < NUM_BLOBS; i++) {
            init_player(i);
        }
        // Handle next point (with a little pause)
        countdown = INITIAL_COUNTDOWN;  // Don't allow moves during countdown
        startTime = get_ticks_ms();
        // Handle end of game
        int winner = get_winner();
        if (winner) {
            // play sfx
            wav64_play(&sfx_win, CHANNEL_SFX3);
        }
    }
    applyScreenLimitsCircle(&ball, ball_sprite);
    // air friction + gravity
    applyFriction(&ball);
    applyGravity(&ball);

    // Handle collision with net
    collision_t netCollision = circleRect(ball.x, ball.y, ball_sprite->width/2, net.x, net.y, net_sprite->width, net_sprite->height);
    vector2d_t netCollisionNormal = netCollision.normalized;
    if (netCollisionNormal.x != 0 || netCollisionNormal.y != 0) {
        // Stop / bounce
            // Use (normalized) vector from player center to ball center instead of nearest collision poiint ???
            float distX = ball.x - (net.x + net_sprite->width/2);
            float distY = ball.y - (net.y + net_sprite->height/2);
            float distance = sqrt( (distX*distX) + (distY*distY) );
            vector2d_t netBallNormal = {
                distX/distance,
                distY/distance
            };
            netCollisionNormal = netBallNormal;

            // if hitting on the side, reverse ball dx
            // If hitting on top, reverse ball dy
            float next_ball_dx = (netCollision.pos.x == net.x || netCollision.pos.x == (net.x + net_sprite->width)) ? -1.0f * ball.dx : ball.dx;
            float next_ball_dy = (netCollision.pos.y == net.y) ? -1.0f *ball.dy : ball.dy;
            ball.dx = next_ball_dx;
            ball.dy = next_ball_dy;

            // Resolve collisions --> move ball
            float next_ball_x = ball.x;
            float next_ball_y = ball.y;
            // dependns on nearest X/Y if to the right, add x, if to the left, sub x, if to the top, add y if to the bottom, sub y
            if (netCollision.pos.x == net.x) {
                // Ball is on the left
                next_ball_x -= ball_sprite->width/2 - fabs(netCollision.dir.x);
            } else if (netCollision.pos.x == (net.x + net_sprite->width)) {
                // Ball is on the right
                next_ball_x += ball_sprite->width/2 - fabs(netCollision.dir.x);
            } else if (netCollision.pos.y == net.y) {
                // Ball is on the top
                next_ball_y -= ball_sprite->height/2 - fabs(netCollision.dir.y);
            } else if (netCollision.pos.y == (net.y + net_sprite->height)) {
                // Ball is on the bottom
                next_ball_y += ball_sprite->height/2 - fabs(netCollision.dir.y);
            }

            ball.x = next_ball_x;
            ball.y = next_ball_y;
    }

    for (uint32_t i = 0; i < NUM_BLOBS; i++)
    {
        object_t *obj = &blobs[i];
        applyScreenLimitsRect(obj, brew_sprite); // Handle with collisions to be resolved all at once
        // Apply gravity / friction
        applyFriction(obj);
        applyGravity(obj);

        // Handle collisions
            // Player / Net (bounce / block)
            // Player / Ball (up to 3 hits per turn)
            // Screen borders / Ball (bounce, loose some momentum)
            // Ground / Ball (end point)
            // Player / Bonus ??? (higher bounce, faster speed, move net down/up, ...)

        // Player/net collision
        if (rectRect(obj->x, obj->y, brew_sprite->width, brew_sprite->height, net.x, net.y, net_sprite->width, net_sprite->height)) {
            // Reposition player to the left/right of net
            if (obj->x < net.x) {
                obj->x = net.x - brew_sprite->width;
            } else {
                obj->x = net.x + net_sprite->width;
            }
        }
        
        // Ball collision
        collision_t collision = circleRect(ball.x, ball.y, ball_sprite->width/2, obj->x, obj->y, brew_sprite->width, brew_sprite->height);
        vector2d_t collisionNormal = collision.normalized;
        if ((collisionNormal.x != 0 || collisionNormal.y != 0) && !(lastPlayer == i && hitCount > 2)) {
            // Use (normalized) vector from player center to ball center instead of nearest collision poiint ???
            float distX = ball.x - (obj->x + brew_sprite->width/2);
            float distY = ball.y - (obj->y + brew_sprite->height/2);
            float distance = sqrt( (distX*distX) + (distY*distY) );
            vector2d_t playerBallNormal = {
                distX/distance,
                distY/distance
            };
            collisionNormal = playerBallNormal;

            // should bounce even if obj is not moving !!!
            // should depend on the ball position relative to the player ??
            float next_ball_dx = obj->dx - ball.dx;
            float next_ball_dy = obj->dy - ball.dy;

            // bounce with ball velocity
            ball.dx = next_ball_dx;
            ball.dy = next_ball_dy;

            // Resolve collisions --> move ball
            float next_ball_x = ball.x;
            float next_ball_y = ball.y;
            // dependns on nearest X/Y if to the right, add x, if to the left, sub x, if to the top, add y if to the bottom, sub y
            if (collision.pos.x == obj->x) {
                // Ball is on the left
                next_ball_x -= ball_sprite->width/2 - fabs(collision.dir.x);
            } else if (collision.pos.x == (obj->x + brew_sprite->width)) {
                // Ball is on the right
                next_ball_x += ball_sprite->width/2 - fabs(collision.dir.x);
            } else if (collision.pos.y == obj->y) {
                // Ball is on the top
                next_ball_y -= ball_sprite->height/2 - fabs(collision.dir.y);
            } else if (collision.pos.y == (obj->y + brew_sprite->height)) {
                // Ball is on the bottom
                next_ball_y += ball_sprite->height/2 - fabs(collision.dir.y);
            }
            ball.x = next_ball_x;
            ball.y = next_ball_y;

            // Max 3 hits per player
            if (lastPlayer != i) {
                lastPlayer = i;
                hitCount = 0;
            }
            hitCount++;

            // Sound FX
			wav64_play(&sfx_hit, CHANNEL_SFX1);
        }
        collisions[i] = collision;
    }

    cur_tick++;
}

void render(int cur_frame)
{
    surface_t *disp = display_get();
    rdpq_attach_clear(disp, NULL);

    // Render a background with copy-mode and under an rspq block for optimization purposes
    if(!background_block){
        rspq_block_begin();

            rdpq_set_mode_copy(false);
            rdpq_sprite_blit(background_sprite, 0, 0, &(rdpq_blitparms_t){
                .scale_x = 1, .scale_y = 1,
            });

            // Setup a mode for the rest of the sprites
            rdpq_set_mode_standard();
            rdpq_mode_filter(FILTER_BILINEAR);
            rdpq_mode_alphacompare(1);
            rdpq_mode_antialias(false);
            rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);

        background_block = rspq_block_end();
    } rspq_block_run(background_block);

    // Players
    for (uint32_t i = 0; i < NUM_BLOBS; i++)
    {
        rdpq_sprite_blit(brew_sprite, blobs[i].x, (int32_t) blobs[i].y, &(rdpq_blitparms_t){
            .scale_x = blobs[i].scale_factor, .scale_y = blobs[i].scale_factor,
        });
    }

    // Ball
    rdpq_sprite_blit(ball_sprite, (ball.x - ball_sprite->width/2), (int32_t) (ball.y - ball_sprite->height/2), &(rdpq_blitparms_t){
        .scale_x = ball.scale_factor, .scale_y = ball.scale_factor,
    });


    // Draw net
    rdpq_sprite_blit(net_sprite, net.x, net.y, &(rdpq_blitparms_t){
        .scale_x = net.scale_factor, .scale_y = net.scale_factor,
    });

    // Draw text
    rdpq_text_printf(&(rdpq_textparms_t){
            .align = ALIGN_CENTER,
            .valign = VALIGN_TOP,
            .width = 200,
            .height = 200,
            .wrap = WRAP_WORD,
    }, FONT_PACIFICO, 210, 20, "^00Score:\n %d | %d", scorePlayer1, scorePlayer2);

    int winner = get_winner();
    if (winner) {
        rdpq_text_printf(&(rdpq_textparms_t){
            .align = ALIGN_CENTER,
            .width = 400,
        }, FONT_PACIFICO, 120, 180, "^01Player %d WINS!", winner);
    } else {
        if (countdown > 0) {
            rdpq_text_printf(&(rdpq_textparms_t){
            .align = ALIGN_CENTER,
            .width = 400,
            }, FONT_PACIFICO, 120, 180, "^01%d", countdown);
        }
    }

    // Force backbuffer flip
    rdpq_detach_show();
}

#include <float.h>
#include "n64sys.h"

int main()
{
    debug_init_isviewer();
    debug_init_usblog();

    display_init(RESOLUTION_640x480, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_DEDITHER);

    joypad_init();
    timer_init();

    uint32_t display_width = display_get_width();
    uint32_t display_height = display_get_height();
    
    dfs_init(DFS_DEFAULT_LOCATION);

    rdpq_init();

	audio_init(44100, 4);
	mixer_init(20);

	wav64_open(&sfx_hit, "rom:/hit.wav64");
	wav64_open(&sfx_halt, "rom:/halt.wav64");
	wav64_open(&sfx_win, "rom:/win.wav64");

    xm64player_open(&sfx_music, "rom:/autang.xm64");
    xm64player_set_loop(&sfx_music, true);
    xm64player_set_vol(&sfx_music, 0.55);
    xm64player_play(&sfx_music, CHANNEL_MUSIC);

    background_sprite = sprite_load("rom:/background.sprite");

    brew_sprite = sprite_load("rom:/n64brew.sprite");

    rdpq_font_t *fnt1 = rdpq_font_load("rom:/Pacifico.font64");
    rdpq_font_style(fnt1, 0, &(rdpq_fontstyle_t){
        .color = RGBA32(0xFD, 0xFE, 0x99, 0xFF),
    });
    rdpq_font_style(fnt1, 1, &(rdpq_fontstyle_t){
        .color = RGBA32(0xFD, 0x9E, 0x99, 0xFF),
    });
    rdpq_text_register_font(FONT_PACIFICO, fnt1);

    obj_min_x = 5;
    obj_max_x = display_width - 5;
    obj_min_y = 5;
    obj_max_y = display_height - 16;

    for (uint32_t i = 0; i < NUM_BLOBS; i++)
    {
        init_player(i);
    }


    ball_sprite = sprite_load("rom:/ball.sprite");
    ball.x = display_width / 4.0f;
    ball.y = obj_min_y + ball_sprite->height/2;
    ball.dx = 0;
    ball.dy = 0;
    ball.scale_factor = 1.0f;

    net_sprite = sprite_load("rom:/net.sprite");
    net.x = display_width/2.0f - (net_sprite->width/2.0f);
    net.y = display_height - net_sprite->height;
    net.dx = 0;
    net.dy = 0;
    net.scale_factor = 1.0f;

    countdown = INITIAL_COUNTDOWN;
    startTime = get_ticks_ms();

    int cur_frame = 0;
    while (1)
    {
        update(0);
        render(cur_frame);
        joypad_poll();

        if (in_play()) {
            for (uint32_t i = 0; i < NUM_BLOBS; i++)
            {
                if (joypad_is_connected((joypad_port_t) i)) {
                    joypad_buttons_t buttons = joypad_get_buttons((joypad_port_t) i);
                    object_t *obj = &blobs[i];

                    if ((buttons.d_up || buttons.a || buttons.b) && (obj_max_y - fabs(obj->y) - brew_sprite->height) < POSITION_EPSILON) {
                        obj->dy = -6;
                    }

                    if (buttons.d_left) {
                        obj->dx = -6;
                    }

                    if (buttons.d_right) {
                        obj->dx = 6;
                    }
                }
            }
        }

        // Check whether one audio buffer is ready, otherwise wait for next
        // frame to perform mixing.
        mixer_try_play();

        cur_frame++;
    }
}
