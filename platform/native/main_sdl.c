/* SDL front end of the native port: the window, the input sources (touch, game controller,
 * keyboard), the on-screen controller, and the console's picture.
 *
 * It maps the console's address space, opens the disc image and a memory card from the app's
 * folder, and runs the game's own main() (renamed hmbtn_game_main). The game calls VSync() once
 * a frame, and that is where this file's frontend_vblank() runs: events, the pad, and the
 * software GPU's display area with the controller drawn over it. A message from the game
 * (SystemError, a file the disc does not have) is shown on screen.
 *
 * Without a disc, or when built without the game, it runs the demo scene instead. If the address
 * space cannot be mapped it shows an input test screen. */

#include <ctype.h>
#include <math.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#include "demo_cube.h"
#include "libpad.h"
#include "psx_buttons.h"
#include "psx_cd.h"
#include "psx_gpu.h"
#include "psx_libgpu.h"
#include "psx_mcard.h"
#include "psx_mem.h"
#include "psx_pad.h"
#include "psx_platform.h"
#include "touch_pad.h"

#define PI_F 3.14159265f

/* ---- touch fingers --------------------------------------------------------------------- */

typedef struct Finger {
    SDL_FingerID id;
    float x, y;                 /* normalised 0..1, as SDL reports them */
    int used;
} Finger;

static Finger fingers[TOUCH_MAX_POINTS];

static void finger_set(SDL_FingerID id, float x, float y) {
    int i;
    int free_slot = -1;

    for (i = 0; i < TOUCH_MAX_POINTS; i++) {
        if (fingers[i].used && fingers[i].id == id) {
            fingers[i].x = x;
            fingers[i].y = y;
            return;
        }
        if (!fingers[i].used && free_slot < 0) {
            free_slot = i;
        }
    }
    if (free_slot >= 0) {
        fingers[free_slot].id = id;
        fingers[free_slot].x = x;
        fingers[free_slot].y = y;
        fingers[free_slot].used = 1;
    }
}

static void finger_release(SDL_FingerID id) {
    int i;
    for (i = 0; i < TOUCH_MAX_POINTS; i++) {
        if (fingers[i].used && fingers[i].id == id) {
            fingers[i].used = 0;
        }
    }
}

static void fingers_clear(void) {
    memset(fingers, 0, sizeof(fingers));
}

static uint16_t touch_mask(const TouchPadLayout *layout) {
    TouchPoint points[TOUCH_MAX_POINTS];
    int i, n = 0;

    for (i = 0; i < TOUCH_MAX_POINTS; i++) {
        if (fingers[i].used) {
            points[n].x = fingers[i].x * (float)layout->width;
            points[n].y = fingers[i].y * (float)layout->height;
            n++;
        }
    }
    return touch_pad_mask(layout, points, n);
}

/* ---- game controller and keyboard ----------------------------------------------------- */

static SDL_GameController *controller;

static void open_first_controller(void) {
    int i;

    if (controller != NULL) {
        return;
    }
    for (i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            controller = SDL_GameControllerOpen(i);
            if (controller != NULL) {
                SDL_Log("controller: %s", SDL_GameControllerName(controller));
                return;
            }
        }
    }
}

static uint16_t controller_mask(void) {
    static const struct {
        SDL_GameControllerButton button;
        uint16_t bit;
    } map[] = {
        {SDL_CONTROLLER_BUTTON_A, PSX_PAD_CROSS},
        {SDL_CONTROLLER_BUTTON_B, PSX_PAD_CIRCLE},
        {SDL_CONTROLLER_BUTTON_X, PSX_PAD_SQUARE},
        {SDL_CONTROLLER_BUTTON_Y, PSX_PAD_TRIANGLE},
        {SDL_CONTROLLER_BUTTON_LEFTSHOULDER, PSX_PAD_L1},
        {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, PSX_PAD_R1},
        {SDL_CONTROLLER_BUTTON_BACK, PSX_PAD_SELECT},
        {SDL_CONTROLLER_BUTTON_START, PSX_PAD_START},
        {SDL_CONTROLLER_BUTTON_DPAD_UP, PSX_PAD_UP},
        {SDL_CONTROLLER_BUTTON_DPAD_DOWN, PSX_PAD_DOWN},
        {SDL_CONTROLLER_BUTTON_DPAD_LEFT, PSX_PAD_LEFT},
        {SDL_CONTROLLER_BUTTON_DPAD_RIGHT, PSX_PAD_RIGHT},
    };
    const int threshold = 16000;
    uint16_t mask = 0;
    unsigned i;

    if (controller == NULL) {
        return 0;
    }
    for (i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        if (SDL_GameControllerGetButton(controller, map[i].button)) {
            mask |= map[i].bit;
        }
    }
    if (SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > threshold) mask |= PSX_PAD_L2;
    if (SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > threshold) mask |= PSX_PAD_R2;
    if (SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX) < -threshold) mask |= PSX_PAD_LEFT;
    if (SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX) > threshold) mask |= PSX_PAD_RIGHT;
    if (SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY) < -threshold) mask |= PSX_PAD_UP;
    if (SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY) > threshold) mask |= PSX_PAD_DOWN;
    return mask;
}

/* Keyboard, for trying the build on a desktop. */
static uint16_t keyboard_mask(void) {
    const Uint8 *k = SDL_GetKeyboardState(NULL);
    uint16_t mask = 0;

    if (k[SDL_SCANCODE_UP]) mask |= PSX_PAD_UP;
    if (k[SDL_SCANCODE_DOWN]) mask |= PSX_PAD_DOWN;
    if (k[SDL_SCANCODE_LEFT]) mask |= PSX_PAD_LEFT;
    if (k[SDL_SCANCODE_RIGHT]) mask |= PSX_PAD_RIGHT;
    if (k[SDL_SCANCODE_Z]) mask |= PSX_PAD_CROSS;
    if (k[SDL_SCANCODE_X]) mask |= PSX_PAD_CIRCLE;
    if (k[SDL_SCANCODE_A]) mask |= PSX_PAD_SQUARE;
    if (k[SDL_SCANCODE_S]) mask |= PSX_PAD_TRIANGLE;
    if (k[SDL_SCANCODE_Q]) mask |= PSX_PAD_L1;
    if (k[SDL_SCANCODE_W]) mask |= PSX_PAD_R1;
    if (k[SDL_SCANCODE_E]) mask |= PSX_PAD_L2;
    if (k[SDL_SCANCODE_R]) mask |= PSX_PAD_R2;
    if (k[SDL_SCANCODE_RETURN]) mask |= PSX_PAD_START;
    if (k[SDL_SCANCODE_TAB]) mask |= PSX_PAD_SELECT;
    return mask;
}

/* ---- drawing --------------------------------------------------------------------------- */

/* A 3x5 pixel font: five rows of three bits, top row first. Only the characters the screen
 * needs. */
#define G(a, b, c, d, e) ((uint16_t)(((a) << 12) | ((b) << 9) | ((c) << 6) | ((d) << 3) | (e)))

static uint16_t glyph(char c) {
    switch (toupper((unsigned char)c)) {
    case '0': return G(7, 5, 5, 5, 7);
    case '1': return G(2, 6, 2, 2, 7);
    case '2': return G(7, 1, 7, 4, 7);
    case '3': return G(7, 1, 7, 1, 7);
    case '4': return G(5, 5, 7, 1, 1);
    case '5': return G(7, 4, 7, 1, 7);
    case '6': return G(7, 4, 7, 5, 7);
    case '7': return G(7, 1, 1, 2, 2);
    case '8': return G(7, 5, 7, 5, 7);
    case '9': return G(7, 5, 7, 1, 7);
    case 'A': return G(7, 5, 7, 5, 5);
    case 'B': return G(6, 5, 6, 5, 6);
    case 'C': return G(7, 4, 4, 4, 7);
    case 'D': return G(6, 5, 5, 5, 6);
    case 'E': return G(7, 4, 7, 4, 7);
    case 'F': return G(7, 4, 7, 4, 4);
    case 'H': return G(5, 5, 7, 5, 5);
    case 'L': return G(4, 4, 4, 4, 7);
    case 'M': return G(5, 7, 7, 5, 5);
    case 'N': return G(7, 5, 5, 5, 5);
    case 'O': return G(7, 5, 5, 5, 7);
    case 'I': return G(7, 2, 2, 2, 7);
    case 'K': return G(5, 5, 6, 5, 5);
    case 'G': return G(7, 4, 5, 5, 7);
    case 'P': return G(7, 5, 7, 4, 4);
    case 'R': return G(6, 5, 6, 5, 5);
    case 'S': return G(7, 4, 7, 1, 7);
    case 'T': return G(7, 2, 2, 2, 2);
    case 'J': return G(1, 1, 1, 5, 7);
    case 'Q': return G(7, 5, 5, 7, 1);
    case 'U': return G(5, 5, 5, 5, 7);
    case 'V': return G(5, 5, 5, 5, 2);
    case 'W': return G(5, 5, 7, 7, 5);
    case 'X': return G(5, 5, 2, 5, 5);
    case 'Y': return G(5, 5, 2, 2, 2);
    case 'Z': return G(7, 1, 2, 4, 7);
    case '.': return G(0, 0, 0, 0, 2);
    case ':': return G(0, 2, 0, 2, 0);
    case ';': return G(0, 2, 0, 2, 4);
    case '-': return G(0, 0, 7, 0, 0);
    case '_': return G(0, 0, 0, 0, 7);
    case '/': return G(1, 1, 2, 4, 4);
    case '\\': return G(4, 4, 2, 1, 1);
    default: return 0;
    }
}

static float text_width(const char *s, float scale) {
    size_t n = strlen(s);
    return n == 0 ? 0.0f : ((float)n * 4.0f - 1.0f) * scale;
}

static void draw_text(SDL_Renderer *r, float x, float y, float scale, const char *s) {
    for (; *s != '\0'; s++, x += 4.0f * scale) {
        uint16_t bits = glyph(*s);
        int row, col;
        for (row = 0; row < 5; row++) {
            for (col = 0; col < 3; col++) {
                if (bits & (1 << ((4 - row) * 3 + (2 - col)))) {
                    SDL_FRect cell = {x + (float)col * scale, y + (float)row * scale, scale, scale};
                    SDL_RenderFillRectF(r, &cell);
                }
            }
        }
    }
}

static void draw_text_centered(SDL_Renderer *r, float cx, float cy, float scale, const char *s) {
    draw_text(r, cx - text_width(s, scale) * 0.5f, cy - 2.5f * scale, scale, s);
}

static void fill_circle(SDL_Renderer *r, float cx, float cy, float radius) {
    int y;
    const int ir = (int)radius;

    for (y = -ir; y <= ir; y++) {
        const float dx = sqrtf(radius * radius - (float)(y * y));
        SDL_RenderDrawLineF(r, cx - dx, cy + (float)y, cx + dx, cy + (float)y);
    }
}

static void thick_line(SDL_Renderer *r, float x1, float y1, float x2, float y2, int t) {
    int i;
    for (i = -t / 2; i <= t / 2; i++) {
        SDL_RenderDrawLineF(r, x1 + (float)i, y1, x2 + (float)i, y2);
        SDL_RenderDrawLineF(r, x1, y1 + (float)i, x2, y2 + (float)i);
    }
}

static void ring(SDL_Renderer *r, float cx, float cy, float radius, int t) {
    const int segments = 40;
    int i;
    for (i = 0; i < segments; i++) {
        const float a0 = 2.0f * PI_F * (float)i / (float)segments;
        const float a1 = 2.0f * PI_F * (float)(i + 1) / (float)segments;
        thick_line(r, cx + cosf(a0) * radius, cy + sinf(a0) * radius, cx + cosf(a1) * radius,
                   cy + sinf(a1) * radius, t);
    }
}

typedef struct FaceStyle {
    Uint8 r, g, b;
} FaceStyle;

static void draw_face_glyph(SDL_Renderer *r, int index, float cx, float cy, float radius, int t) {
    const float s = radius * 0.45f;

    switch (index) {
    case 0: /* triangle */
        thick_line(r, cx, cy - s, cx - s * 1.0f, cy + s * 0.7f, t);
        thick_line(r, cx, cy - s, cx + s * 1.0f, cy + s * 0.7f, t);
        thick_line(r, cx - s, cy + s * 0.7f, cx + s, cy + s * 0.7f, t);
        break;
    case 1: /* circle */
        ring(r, cx, cy, s, t);
        break;
    case 2: /* cross */
        thick_line(r, cx - s, cy - s, cx + s, cy + s, t);
        thick_line(r, cx - s, cy + s, cx + s, cy - s, t);
        break;
    default: /* square */
        thick_line(r, cx - s, cy - s, cx + s, cy - s, t);
        thick_line(r, cx - s, cy + s, cx + s, cy + s, t);
        thick_line(r, cx - s, cy - s, cx - s, cy + s, t);
        thick_line(r, cx + s, cy - s, cx + s, cy + s, t);
        break;
    }
}

static void draw_pad(SDL_Renderer *r, const TouchPadLayout *l, uint16_t held) {
    static const FaceStyle face_style[4] = {{0x4C, 0xD9, 0x8C}, {0xF0, 0x5A, 0x5A}, {0x5A, 0x8C, 0xF0},
                                            {0xE8, 0x8C, 0xD0}};
    static const char *const shoulder_label[4] = {"L2", "R2", "L1", "R1"};
    static const char *const system_label[2] = {"SELECT", "START"};
    const float u = l->unit;
    const float arm_len = 0.22f * u;
    const float arm_thick = 0.14f * u;
    const int line_t = (int)(0.008f * u) + 1;
    int i;

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    /* D-pad: a neutral centre and four arms, each lit while its direction is held. */
    {
        const float half = arm_thick * 0.5f;
        const SDL_FRect centre = {l->dpad_cx - half, l->dpad_cy - half, arm_thick, arm_thick};
        const struct {
            uint16_t bit;
            SDL_FRect rect;
        } arms[4] = {
            {PSX_PAD_UP, {l->dpad_cx - half, l->dpad_cy - arm_len, arm_thick, arm_len - half}},
            {PSX_PAD_DOWN, {l->dpad_cx - half, l->dpad_cy + half, arm_thick, arm_len - half}},
            {PSX_PAD_LEFT, {l->dpad_cx - arm_len, l->dpad_cy - half, arm_len - half, arm_thick}},
            {PSX_PAD_RIGHT, {l->dpad_cx + half, l->dpad_cy - half, arm_len - half, arm_thick}},
        };
        SDL_SetRenderDrawColor(r, 0xE0, 0xE0, 0xE0, 0x50);
        SDL_RenderFillRectF(r, &centre);
        for (i = 0; i < 4; i++) {
            SDL_SetRenderDrawColor(r, 0xE0, 0xE0, 0xE0, (held & arms[i].bit) ? 0xD0 : 0x50);
            SDL_RenderFillRectF(r, &arms[i].rect);
        }
    }

    /* Face buttons. */
    for (i = 0; i < 4; i++) {
        const int on = (held & l->face_bit[i]) != 0;
        SDL_SetRenderDrawColor(r, face_style[i].r, face_style[i].g, face_style[i].b, on ? 0xD0 : 0x60);
        fill_circle(r, l->face[i].cx, l->face[i].cy, l->face[i].r);
        SDL_SetRenderDrawColor(r, 0xFF, 0xFF, 0xFF, 0xE0);
        draw_face_glyph(r, i, l->face[i].cx, l->face[i].cy, l->face[i].r, line_t);
    }

    /* Shoulder buttons and select/start. */
    for (i = 0; i < 4; i++) {
        const SDL_FRect rect = {l->shoulder[i].x, l->shoulder[i].y, l->shoulder[i].w, l->shoulder[i].h};
        SDL_SetRenderDrawColor(r, 0xC8, 0xC8, 0xD8, (held & l->shoulder_bit[i]) ? 0xD0 : 0x50);
        SDL_RenderFillRectF(r, &rect);
        SDL_SetRenderDrawColor(r, 0xFF, 0xFF, 0xFF, 0xF0);
        draw_text_centered(r, rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f, rect.h / 9.0f, shoulder_label[i]);
    }
    for (i = 0; i < 2; i++) {
        const SDL_FRect rect = {l->system[i].x, l->system[i].y, l->system[i].w, l->system[i].h};
        SDL_SetRenderDrawColor(r, 0xC8, 0xC8, 0xD8, (held & l->system_bit[i]) ? 0xD0 : 0x50);
        SDL_RenderFillRectF(r, &rect);
        SDL_SetRenderDrawColor(r, 0xFF, 0xFF, 0xFF, 0xF0);
        draw_text_centered(r, rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f, rect.h / 9.0f, system_label[i]);
    }
}

#ifdef HMBTN_GAME
extern int hmbtn_game_main(void);
#endif

/* ---- frame ----------------------------------------------------------------------------- */

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *screen_texture;
static int texture_w, texture_h;
static TouchPadLayout layout;
static int running = 1;
static int have_ram;
static int paced_by_vsync;
static unsigned char pad_buffers[2][34];
static uint16_t held_mask;
static int in_game;                     /* the game's main() is running */
static jmp_buf quit_jump;               /* where a quit or a fatal message leaves the game */
static char message[512];               /* shown on screen when not empty */
static char status_line[256];           /* a note under the title */
static char data_dir[512];              /* the disc image and the memory card live here */
static uint32_t frame_pixels[640 * 512];

/* Draw `text` in lines that fit `width` pixels, from (x, y). */
static void draw_wrapped(SDL_Renderer *r, float x, float y, float scale, float width, const char *text) {
    const int per_line = (int)(width / (4.0f * scale));
    const char *p = text;

    if (per_line < 8) {
        return;
    }
    while (*p != 0) {
        char line[160];
        int n = 0;
        while (n < per_line && n < (int)sizeof(line) - 1 && p[n] != 0 && p[n] != '\n') {
            n++;
        }
        memcpy(line, p, (size_t)n);
        line[n] = 0;
        draw_text(r, x, y, scale, line);
        y += 7.0f * scale;
        p += n;
        if (*p == '\n') {
            p++;
        }
    }
}

/* Draw the GPU's display rectangle, letterboxed in the window. */
static void draw_console_screen(int window_w, int window_h) {
    int w = 0, h = 0;
    SDL_Rect dst;
    float scale;

    psx_gpu_get_display(frame_pixels, 640 * 512, &w, &h);
    if (w <= 0 || h <= 0 || w * h > 640 * 512) {
        return;
    }
    if (screen_texture == NULL || texture_w != w || texture_h != h) {
        if (screen_texture != NULL) {
            SDL_DestroyTexture(screen_texture);
        }
        screen_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
        texture_w = w;
        texture_h = h;
        if (screen_texture == NULL) {
            return;
        }
    }
    SDL_UpdateTexture(screen_texture, NULL, frame_pixels, w * (int)sizeof(uint32_t));
    scale = (float)window_w / (float)w;
    if ((float)window_h / (float)h < scale) {
        scale = (float)window_h / (float)h;
    }
    dst.w = (int)((float)w * scale);
    dst.h = (int)((float)h * scale);
    dst.x = (window_w - dst.w) / 2;
    dst.y = (window_h - dst.h) / 2;
    SDL_RenderCopy(renderer, screen_texture, NULL, &dst);
}

/* One vertical blank of the front end: events, input into the pad shim, then the picture
 * and the controller on screen. The console code reaches this through VSync(). */
static void frontend_vblank(void) {
    SDL_Event e;
    uint16_t requested;
    int width = 0, height = 0;
    char text[16];

    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            running = 0;
            break;
        case SDL_FINGERDOWN:
        case SDL_FINGERMOTION:
            finger_set(e.tfinger.fingerId, e.tfinger.x, e.tfinger.y);
            break;
        case SDL_FINGERUP:
            finger_release(e.tfinger.fingerId);
            break;
        case SDL_CONTROLLERDEVICEADDED:
            open_first_controller();
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            if (controller != NULL &&
                e.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller))) {
                SDL_GameControllerClose(controller);
                controller = NULL;
                open_first_controller();
            }
            break;
        case SDL_APP_WILLENTERBACKGROUND:
            fingers_clear();
            break;
        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                fingers_clear();
            }
            break;
        default:
            break;
        }
    }
    if (!running && in_game) {
        longjmp(quit_jump, 1);              /* the game's loop never returns on its own */
    }

    SDL_GetRendererOutputSize(renderer, &width, &height);
    if (width != layout.width || height != layout.height) {
        touch_pad_layout_init(&layout, width, height);
    }

    /* The console's vertical blank: hand the buttons to the pad shim, then read them back
     * the way pad.c does (active low, d-pad in the high byte). */
    requested = touch_mask(&layout) | controller_mask() | keyboard_mask();
    psx_pad_set_buttons(0, requested);
    psx_pad_commit();
    /* The game hands the shim its own buffers; before that, read this file's. */
    held_mask = in_game ? requested : (uint16_t)~(((unsigned)pad_buffers[0][2] << 8) | pad_buffers[0][3]);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 0x12, 0x12, 0x1A, 0xFF);
    SDL_RenderClear(renderer);
    if (have_ram && message[0] == 0) {
        draw_console_screen(width, height);
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xB0);
    if (have_ram) {
        draw_text_centered(renderer, (float)width * 0.5f, 0.05f * layout.unit, layout.unit / 120.0f, "HMBTN");
    } else {
        draw_text_centered(renderer, (float)width * 0.5f, 0.07f * layout.unit, layout.unit / 70.0f, "HMBTN");
        draw_text_centered(renderer, (float)width * 0.5f, 0.30f * layout.unit, layout.unit / 100.0f, "NO RAM");
    }
    snprintf(text, sizeof(text), "PAD %04X", held_mask);
    draw_text_centered(renderer, (float)width * 0.5f, 0.12f * layout.unit, layout.unit / 100.0f, text);

    if (status_line[0] != 0 && message[0] == 0) {
        SDL_SetRenderDrawColor(renderer, 0xFF, 0xD0, 0x60, 0xE0);
        draw_wrapped(renderer, 0.05f * (float)width, 0.86f * (float)height - 7.0f * layout.unit / 140.0f, layout.unit / 140.0f,
                     0.9f * (float)width, status_line);
    }
    if (message[0] != 0) {
        SDL_SetRenderDrawColor(renderer, 0xFF, 0x90, 0x90, 0xFF);
        draw_wrapped(renderer, 0.05f * (float)width, 0.22f * (float)height, layout.unit / 70.0f, 0.9f * (float)width,
                     message);
    } else {
        draw_pad(renderer, &layout, held_mask);
    }
    SDL_RenderPresent(renderer);
    if (!paced_by_vsync) {
        SDL_Delay(16);
    }
}

/* ---- the disc and the card -------------------------------------------------------------- */

/* A message the game cannot go on after: keep it and leave the game. */
static void frontend_fatal(const char *text) {
    snprintf(message, sizeof(message), "%s", text);
    SDL_Log("fatal: %s", text);
    longjmp(quit_jump, 2);
}

/* The first disc image in the data folder: disc.bin, disc.iso or disc.img. Returns 0 if found. */
static int find_disc(char *path, size_t size) {
    static const char *const names[] = {"disc.bin", "disc.iso", "disc.img", "game.bin", "game.iso"};
    size_t i;
    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        FILE *f;
        snprintf(path, size, "%s/%s", data_dir, names[i]);
        f = fopen(path, "rb");
        if (f != NULL) {
            fclose(f);
            return 0;
        }
    }
    return -1;
}

static void choose_data_dir(int argc, char **argv) {
    const char *dir = NULL;
#ifdef __ANDROID__
    dir = SDL_AndroidGetExternalStoragePath();
#endif
    if (argc > 1) {
        dir = argv[1];
    } else if (getenv("HMBTN_DIR") != NULL) {
        dir = getenv("HMBTN_DIR");
    }
    snprintf(data_dir, sizeof(data_dir), "%s", dir != NULL ? dir : ".");
}

/* ---- main ------------------------------------------------------------------------------ */

int main(int argc, char *argv[]) {
    SDL_RendererInfo info;
    Uint32 window_flags = SDL_WINDOW_ALLOW_HIGHDPI;

    memset(&layout, 0, sizeof(layout));
    choose_data_dir(argc, argv);

    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
#ifdef __ANDROID__
    window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
#else
    window_flags |= SDL_WINDOW_RESIZABLE;
#endif
    window = SDL_CreateWindow("Harvest Moon: Back to Nature", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              1280, 720, window_flags);
    if (window == NULL) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (renderer == NULL) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    paced_by_vsync = SDL_GetRendererInfo(renderer, &info) == 0 && (info.flags & SDL_RENDERER_PRESENTVSYNC);

    /* The controller shim, exactly as the game's pad_init sets it up. */
    PadInitDirect(pad_buffers[0], pad_buffers[1]);
    PadStartCom();
    open_first_controller();

    /* The console's address space, then the demo scene. */
    have_ram = psx_mem_init() == 0;
    SDL_Log("console address space: %s; data folder: %s", have_ram ? "mapped" : "NOT mapped", data_dir);
    if (have_ram) {
        char path[640];
        const int have_disc = find_disc(path, sizeof(path)) == 0 && psx_cd_open(path) == 0;

        psx_vsync_hook = frontend_vblank;
        psx_fatal_hook = frontend_fatal;
        snprintf(path, sizeof(path), "%s/memcard1.mcr", data_dir);
        psx_mcard_attach(0, path);

#ifdef HMBTN_GAME
        if (have_disc) {
            SDL_Log("disc found; starting the game");
            in_game = 1;
            if (setjmp(quit_jump) == 0) {
                hmbtn_game_main();
            }
            in_game = 0;
            while (running) {          /* a message from the game, or its main returned */
                frontend_vblank();
            }
        } else
#endif
        {
            snprintf(status_line, sizeof(status_line), "NO DISC: PUT DISC.BIN IN %s", data_dir);
            SDL_Log("%s", status_line);
            (void)have_disc;
            demo_init();
            while (running) {
                demo_frame(held_mask);
            }
        }
        psx_mem_shutdown();
    } else {
        while (running) {
            frontend_vblank();
        }
    }

    PadStopCom();
    if (controller != NULL) {
        SDL_GameControllerClose(controller);
    }
    if (screen_texture != NULL) {
        SDL_DestroyTexture(screen_texture);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
