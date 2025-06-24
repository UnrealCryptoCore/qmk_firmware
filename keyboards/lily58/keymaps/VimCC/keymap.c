#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "action.h"
#include "action_util.h"
#include "keycodes.h"
#include "modifiers.h"
#include "quantum.h"
#include "quantum_keycodes.h"
#include QMK_KEYBOARD_H

#define VIM_MASK_SHIFT 0b01000000
#define VIM_MASK_CTRL 0b10000000
#define COMB2(a, b) ((a << 8) | b)

char  vim_state_buffer[21] = {0};
char *vim_mode_str[]       = {"disabled", "normal", "insert", "insert rep1", "insert rep", "find", "visual", "cmd"};

typedef enum vim_mode {
    VIM_DISABLED,
    VIM_NORMAL,
    VIM_INSERT,
    VIM_INSERT_REP1,
    VIM_INSERT_REP,
    VIM_FIND,
    VIM_VISUAL,
    VIM_CMD,
} vim_mode;

typedef enum VimInputState {
    VS_IDLE,
    VS_NUMBER,
    VS_G,
} VimInputState;

typedef enum Operator {
    OP_NONE,
    OP_DELETE,
    OP_YANK,
    OP_CHANGE,
    OP_UPPER,
    OP_LOWER,
    OP_TOGGLE,
} Operator;

typedef struct VimMotion {
    enum Operator op;
    char          count[6];
    uint8_t       motion;
} VimMotion;

typedef struct Vim {
    vim_mode      mode;
    uint32_t      buf; // 4 key strokes
    uint32_t      num1;
    uint32_t      num2;
    Operator      op;
    VimInputState state;
} Vim;

void toggle_vim(Vim *vim) {
    if (vim->mode == VIM_DISABLED) {
        vim->mode = VIM_NORMAL;
    } else {
        vim->mode = VIM_DISABLED;
    }
}

const char *read_vim_state(Vim *vim) {
    char *mode = vim_mode_str[vim->mode];
    if (vim->mode == VIM_DISABLED) {
        snprintf(vim_state_buffer, sizeof(vim_state_buffer), "VIM: %s ", mode);
    } else {
        snprintf(vim_state_buffer, sizeof(vim_state_buffer), "VIM: %s (%d,%lu,%lu,%d)", mode, vim->op, vim->num1, vim->num2, vim->state);
    }
    return vim_state_buffer;
}

uint8_t get_keystroke(uint16_t keycode) {
    uint8_t mods       = get_mods() | get_oneshot_mods();
    bool    shift_held = mods & MOD_MASK_SHIFT;
    bool    ctrl_held  = mods & MOD_MASK_CTRL;
    uint8_t res        = (uint8_t)keycode;
    if (shift_held) {
        res |= VIM_MASK_SHIFT;
    }
    if (ctrl_held) {
        res |= VIM_MASK_CTRL;
    }
    return res;
}

void tapn_code(uint16_t keycode, int n) {
    for (int i = 0; i < n; i++) {
        tap_code16(keycode);
    }
}

void reset_vim_buf(Vim *vim) {
    vim->op    = OP_NONE;
    vim->state = VS_IDLE;
    vim->num1  = 0;
    vim->num2  = 0;
    vim->buf   = 0;
}

void tap_vim_code(Vim *vim, uint16_t keycode, int n) {
    switch (vim->op) {
        case OP_YANK:
            tapn_code(S(keycode), n);
            tap_code16(KC_COPY);
            /* fallthrough */
        case OP_NONE:
            if (vim->mode == VIM_VISUAL) {
                tapn_code(S(keycode), n);
            } else {
                tapn_code(keycode, n);
            }
            break;
        case OP_DELETE:
            tapn_code(S(keycode), n);
            tap_code16(KC_CUT);
            break;
        default:
            break;
    }
}

bool process_vim_key(Vim *vim, uint16_t keycode) {
    if (vim->mode == VIM_DISABLED) {
        return true;
    }

    if (keycode >= 64) {
        return true;
    }

    uint8_t key  = get_keystroke(keycode);
    uint8_t mods = (get_mods() | get_oneshot_mods()) & (MOD_MASK_SHIFT | MOD_MASK_CTRL);
    unregister_mods(mods);
    vim->buf = (vim->buf << 8) | key;

    bool    ret    = true;
    bool    cmd    = true;
    uint8_t resBuf = 0;
    bool    number = false;
    switch (vim->mode) {
        case VIM_VISUAL:
        /* fallthrough */
        case VIM_NORMAL:
            ret = false;
            switch (vim->buf) {
                case KC_C | VIM_MASK_CTRL:
                    /* fallthrough */
                case KC_ESC:
                    if (vim->mode == VIM_VISUAL) {
                        vim->mode = VIM_NORMAL;
                    } else {
                        ret = true;
                    }
                    break;
                case KC_A | VIM_MASK_SHIFT:
                    tap_code16(KC_END);
                    vim->mode = VIM_INSERT;
                    break;
                case KC_A:
                    tap_code16(KC_RIGHT);
                    vim->mode = VIM_INSERT;
                    break;
                case KC_I | VIM_MASK_SHIFT:
                    tap_code16(KC_HOME);
                    /* fallthrough */
                case KC_I:
                    vim->mode = VIM_INSERT;
                    break;
                case KC_V:
                    vim->mode = VIM_VISUAL;
                    break;
                case KC_U | VIM_MASK_CTRL:
                    tap_code16(KC_PAGE_UP);
                    break;
                case KC_D | VIM_MASK_CTRL:
                    tap_code16(KC_PAGE_DOWN);
                    break;
                case KC_B | VIM_MASK_CTRL:
                    tap_code16(KC_PAGE_UP);
                    break;
                case KC_F | VIM_MASK_CTRL:
                    tap_code16(KC_PAGE_DOWN);
                    break;
                case KC_P:
                    tap_code16(KC_PASTE);
                    break;
                case KC_U:
                    tap_code16(C(KC_Z));
                    break;
                case KC_R | VIM_MASK_CTRL:
                    tap_code16(C(KC_Y));
                    break;
                case KC_R:
                    vim->mode = VIM_INSERT_REP1;
                    break;
                case KC_R | VIM_MASK_SHIFT:
                    vim->mode = VIM_INSERT_REP;
                    break;
                case KC_N:
                    tap_code16(KC_ENTER);
                    break;
                case KC_ENTER | VIM_MASK_SHIFT:
                    tap_code16(S(KC_ENTER));
                    break;
                default:
                    cmd = false;
                    break;
            }
            if (cmd) {
                reset_vim_buf(vim);
                break;
            }
            switch (vim->buf) {
                case KC_Y: // yank
                    if (vim->mode == VIM_VISUAL) {
                        tap_code16(KC_COPY);
                        vim->mode = VIM_NORMAL;
                    } else {
                        switch (vim->op) {
                            case OP_NONE:
                                vim->op = OP_YANK;
                                resBuf  = 1;
                                break;
                            case OP_YANK:
                                // yank line
                                break;
                            default:
                                break;
                        }
                    }
                    break;
                case KC_D: // delete
                    if (vim->mode == VIM_VISUAL) {
                        tap_code16(KC_CUT);
                        vim->mode = VIM_NORMAL;
                    } else {
                        switch (vim->op) {
                            case OP_NONE:
                                vim->op = OP_DELETE;
                                resBuf  = 1;
                                break;
                            case OP_DELETE:
                                // delete line
                                break;
                            default:
                                break;
                        }
                    }
                    break;
                case KC_C: // delete insert
                    if (vim->mode == VIM_VISUAL) {
                        tap_code16(KC_CUT);
                        vim->mode = VIM_INSERT;
                    } else {
                        switch (vim->op) {
                            case OP_NONE:
                                vim->op = OP_CHANGE;
                                resBuf  = 1;
                                break;
                            case OP_CHANGE:
                                // delete line
                                vim->mode = VIM_INSERT;
                                break;
                            default:
                                break;
                        }
                    }
                    break;
                case KC_0:
                    if (vim->state == VS_IDLE) {
                        tap_vim_code(vim, KC_HOME, 1);
                    } else {
                        vim->num2 = vim->num2 * 10 + 0;
                        number    = true;
                        resBuf    = 1;
                    }
                    break;
                case KC_1 ... KC_9:
                    if (vim->op != OP_NONE && vim->state != VS_NUMBER) {
                        vim->num1 = vim->num2;
                    }
                    vim->num2 = vim->num2 * 10 + vim->buf - KC_1 + 1;
                    number    = true;
                    resBuf    = 1;
                    break;
                case KC_6 | VIM_MASK_SHIFT:
                    tap_vim_code(vim, KC_HOME, 1);
                    break;
                case KC_4 | VIM_MASK_SHIFT:
                    tap_vim_code(vim, KC_END, 1);
                    break;
                case KC_G | VIM_MASK_SHIFT:
                    tap_vim_code(vim, C(KC_END), 1);
                    break;
                case KC_J:
                    vim->num1 = vim->num1 == 0 ? 1 : vim->num1;
                    vim->num2 = vim->num2 == 0 ? 1 : vim->num2;
                    tap_vim_code(vim, KC_DOWN, vim->num1 * vim->num2);
                    break;
                case KC_K:
                    vim->num1 = vim->num1 == 0 ? 1 : vim->num1;
                    vim->num2 = vim->num2 == 0 ? 1 : vim->num2;
                    tap_vim_code(vim, KC_UP, vim->num1 * vim->num2);
                    break;
                case KC_H:
                    vim->num1 = vim->num1 == 0 ? 1 : vim->num1;
                    vim->num2 = vim->num2 == 0 ? 1 : vim->num2;
                    tap_vim_code(vim, KC_LEFT, vim->num1 * vim->num2);
                    break;
                case KC_L:
                    vim->num1 = vim->num1 == 0 ? 1 : vim->num1;
                    vim->num2 = vim->num2 == 0 ? 1 : vim->num2;
                    tap_vim_code(vim, KC_RIGHT, vim->num1 * vim->num2);
                    break;
                case KC_W:
                case KC_W | VIM_MASK_SHIFT:
                    vim->num1 = vim->num1 == 0 ? 1 : vim->num1;
                    vim->num2 = vim->num2 == 0 ? 1 : vim->num2;
                    tap_vim_code(vim, C(KC_RIGHT), vim->num1 * vim->num2);
                    break;
                case KC_E:
                case KC_E | VIM_MASK_SHIFT:
                    vim->num1 = vim->num1 == 0 ? 1 : vim->num1;
                    vim->num2 = vim->num2 == 0 ? 1 : vim->num2;
                    tap_vim_code(vim, C(KC_RIGHT), vim->num1 * vim->num2);
                    tap_vim_code(vim, KC_LEFT, vim->num1 * vim->num2);
                    break;
                case KC_B:
                case KC_B | VIM_MASK_SHIFT:
                    vim->num1 = vim->num1 == 0 ? 1 : vim->num1;
                    vim->num2 = vim->num2 == 0 ? 1 : vim->num2;
                    tap_vim_code(vim, C(KC_LEFT), vim->num1 * vim->num2);
                    break;
                case KC_G:
                    // vim->state = VS_G;
                    resBuf = 2; // waiting for next key
                    break;
                case COMB2(KC_G, KC_G):
                    tap_code16(C(KC_HOME));
                    tap_vim_code(vim, KC_DOWN, vim->num2);
                    break;
                case KC_X:
                    vim->num2 = vim->num2 == 0 ? 1 : vim->num2;
                    tapn_code(KC_DEL, vim->num2);
                    break;
                case KC_X | VIM_MASK_SHIFT:
                    vim->num2 = vim->num2 == 0 ? 1 : vim->num2;
                    tapn_code(KC_BACKSPACE, vim->num2);
                    break;
                case KC_SLSH:
                    tap_code16(C(KC_F));
                    vim->mode = VIM_FIND;
                    break;
                default:
                    break;
            }
            if (resBuf == 0) {
                reset_vim_buf(vim);
            } else if (resBuf == 1) {
                vim->buf = 0;
            }
            if (number) {
                vim->state = VS_NUMBER;
            } else {
                vim->state = VS_IDLE;
            }
            break;
        case VIM_INSERT:
        case VIM_INSERT_REP:
        case VIM_INSERT_REP1:
            switch (key) {
                case KC_ESC:
                    /* fallthrough */
                case KC_C | VIM_MASK_CTRL:
                    vim->mode = VIM_NORMAL;
                    ret       = false;
                    break;
                case KC_H | VIM_MASK_CTRL:
                    tap_code16(KC_BACKSPACE);
                    ret = false;
                    break;
                default:
                    ret = true;
                    break;
            }
            if (vim->mode == VIM_INSERT_REP1) {
                tap_code16(KC_DELETE);
                tap_code16(KC_LEFT);
                vim->mode = VIM_NORMAL;
            } else if (vim->mode == VIM_INSERT_REP) {
                tap_code16(KC_DELETE);
            }
            break;
        case VIM_FIND:
            switch (key) {
                case KC_ESC:
                    /* fallthrough */
                case KC_C | VIM_MASK_CTRL:
                    tap_code16(KC_ESC);
                    vim->mode = VIM_NORMAL;
                    ret       = false;
                    break;
                case KC_ENTER:
                    tap_code16(KC_ENTER);
                    ret = false;
                    break;
                default:
                    ret = true;
                    break;
            }
            break;
        case VIM_CMD:
            if (keycode == KC_ESC) {
                vim->mode = VIM_NORMAL;
                ret       = false;
                break;
            }
            break;
        default:
            break;
    }
    register_mods(mods);
    return ret;
}

static Vim *vim = &(Vim){
    .mode  = VIM_DISABLED,
    .buf   = 0,
    .num1  = 0,
    .num2  = 0,
    .op    = OP_NONE,
    .state = VS_IDLE,
};

enum layer_number {
    _QWERTY = 0,
    _LOWER,
    _RAISE,
    _ADJUST,
};

enum custom_keycodes {
    KC_VIM = SAFE_RANGE,
};

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

    /* QWERTY
     * ,-----------------------------------------.                    ,-----------------------------------------.
     * | ESC  |   1  |   2  |   3  |   4  |   5  |                    |   6  |   7  |   8  |   9  |   0  |  =   |
     * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
     * | Tab  |   Q  |   W  |   E  |   R  |   T  |                    |   Y  |   U  |   I  |   O  |   P  |  -   |
     * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
     * |LCTRL |   A  |   S  |   D  |   F  |   G  |-------.    ,-------|   H  |   J  |   K  |   L  |   ;  |  '   |
     * |------+------+------+------+------+------| Play  |    |       |------+------+------+------+------+------|
     * |LShift|   Z  |   X  |   C  |   V  |   B  |-------|    |-------|   N  |   M  |   ,  |   .  |   /  |RShift|
     * `-----------------------------------------/       /     \      \-----------------------------------------'
     *                   | LGUI | LALT |LOWER | /Space  /       \Enter \  |RAISE |BackSP|AltGr |
     *                   |      |      |      |/       /         \      \ |      |      |      |
     *                   `----------------------------'           '------''--------------------'
     */

    [_QWERTY] = LAYOUT(KC_ESC, KC_1, KC_2, KC_3, KC_4, KC_5, KC_6, KC_7, KC_8, KC_9, KC_0, KC_EQL, KC_TAB, KC_Q, KC_W, KC_E, KC_R, KC_T, KC_Y, KC_U, KC_I, KC_O, KC_P, KC_MINS, KC_LCTL, KC_A, KC_S, KC_D, KC_F, KC_G, KC_H, KC_J, KC_K, KC_L, KC_SCLN, KC_QUOT, KC_LSFT, KC_Z, KC_X, KC_C, KC_V, KC_B, KC_PAUS, XXXXXXX, KC_N, KC_M, KC_COMM, KC_DOT, KC_SLSH, KC_RSFT, KC_LGUI, KC_LALT, MO(_LOWER), KC_SPC, KC_ENT, MO(_RAISE), KC_BSPC, KC_ALGR),

    /* LOWER
     * ,-----------------------------------------.                    ,-----------------------------------------.
     * |      |      |      |      |      |      |                    |   F7  |  F8 |  F9  | F10  | F11  | F12  |
     * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
     * |      |  1   |   2  |   3  |   4  |  5   |                    |      |   +  |   =  |   -  |      |      |
     * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
     * |   `  |   !  |   @  |   #  |   $  |   %  |-------.    ,-------| Left | Down  | Up | Right |      |      |
     * |------+------+------+------+------+------|       |    |       |------+------+------+------+------+------|
     * |      |      |      |      |  VIM |      |-------|    |-------|      |      |      |      |      |      |
     * `-----------------------------------------/       /     \      \-----------------------------------------'
     *                   | LAlt | LGUI |LOWER | /Space  /       \Enter \  |RAISE |BackSP| RGUI |
     *                   |      |      |      |/       /         \      \ |      |      |      |
     *                   `----------------------------'           '------''--------------------'
     */
    [_LOWER] = LAYOUT(_______, _______, _______, _______, _______, _______, KC_F7, KC_F8, KC_F9, KC_F10, KC_F11, KC_F12, _______, KC_1, KC_2, KC_3, KC_4, KC_5, _______, KC_PLUS, KC_EQL, KC_MINS, _______, _______, KC_GRV, KC_EXLM, KC_AT, KC_HASH, KC_DLR, KC_PERC, KC_LEFT, KC_DOWN, KC_UP, KC_RGHT, XXXXXXX, XXXXXXX, _______, _______, _______, _______, KC_VIM, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______),

    /* RAISE
     * ,-----------------------------------------.                    ,-----------------------------------------.
     * |  F1  |  F2  |  F3  |  F4  |  F5  |  F6  |                    |      |      |      |      |      |      |
     * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
     * |      |      |      |  {   |   }  |      |                    |   6  |   7  |   8  |   9  |   0  |      |
     * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
     * |      |   |  |  \   |  (   |   )  |      |-------.    ,-------|   ^  |   &  |   *  |   (  |   )  |   ~  |
     * |------+------+------+------+------+------|       |    |       |------+------+------+------+------+------|
     * |      |      |      |  [   |   ]  |      |-------|    |-------|      |      |      |      |      |      |
     * `-----------------------------------------/       /     \      \-----------------------------------------'
     *                   | LAlt | LGUI |LOWER | /Space  /       \Enter \  |RAISE |BackSP| RGUI |
     *                   |      |      |      |/       /         \      \ |      |      |      |
     *                   `----------------------------'           '------''--------------------'
     */
    [_RAISE] = LAYOUT(KC_F1, KC_F2, KC_F3, KC_F4, KC_F5, KC_F6, _______, _______, _______, _______, _______, _______, _______, _______, _______, KC_LCBR, KC_RCBR, _______, KC_6, KC_7, KC_8, KC_9, KC_0, _______, _______, KC_PIPE, KC_BSLS, KC_LPRN, KC_RPRN, _______, KC_CIRC, KC_AMPR, KC_ASTR, KC_LPRN, KC_RPRN, KC_TILD, _______, _______, _______, KC_LBRC, KC_RBRC, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______),
    /* ADJUST
     * ,-----------------------------------------.                    ,-----------------------------------------.
     * |      |      |MCPLAY|      |      |      |                    |      |      |      |      |      |      |
     * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
     * |      |MCREC |      |BRI+  | VOL+ |      |                    |      |      |      |      |      |      |
     * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
     * |      |      |      |NEXT  |PAUSE |PREV  |-------.    ,-------|      |      |RGB ON| HUE+ | SAT+ | VAL+ |
     * |------+------+------+------+------+------|       |    |       |------+------+------+------+------+------|
     * |      |      |      |BRI-  | VOL- |      |-------|    |-------|      |      | MODE | HUE- | SAT- | VAL- |
     * `-----------------------------------------/       /     \      \-----------------------------------------'
     *                   | LAlt | LGUI |LOWER | /Space  /       \Enter \  |RAISE |BackSP| RGUI |
     *                   |      |      |      |/       /         \      \ |      |      |      |
     *                   `----------------------------'           '------''--------------------'
     */
    [_ADJUST] = LAYOUT(XXXXXXX, XXXXXXX, QK_DYNAMIC_MACRO_PLAY_1, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, QK_DYNAMIC_MACRO_RECORD_START_1, XXXXXXX, KC_BRIU, KC_VOLU, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, KC_MEDIA_PREV_TRACK, KC_MEDIA_PLAY_PAUSE, KC_MEDIA_NEXT_TRACK, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, _______, _______, _______, _______, _______, _______, _______, _______)};

layer_state_t layer_state_set_user(layer_state_t state) {
    return update_tri_layer_state(state, _LOWER, _RAISE, _ADJUST);
}

// SSD1306 OLED update loop, make sure to enable OLED_ENABLE=yes in rules.mk
#ifdef OLED_ENABLE

oled_rotation_t oled_init_user(oled_rotation_t rotation) {
    if (!is_keyboard_master()) return OLED_ROTATION_180; // flips the display 180 degrees if offhand
    return rotation;
}

// When you add source files to SRC in rules.mk, you can use functions.
const char *read_layer_state(void);
const char *read_logo(void);
void        set_keylog(uint16_t keycode, keyrecord_t *record);
const char *read_keylog(void);
const char *read_keylogs(void);

// const char *read_mode_icon(bool swap);
// const char *read_host_led_state(void);
// void set_timelog(void);
// const char *read_timelog(void);

bool oled_task_user(void) {
    if (is_keyboard_master()) {
        // If you want to change the display of OLED, you need to change here
        oled_write_ln(read_layer_state(), false);
        oled_write_ln(read_keylog(), false);
        oled_write_ln(read_keylogs(), false);
        oled_write_ln(read_vim_state(vim), false);
        // oled_write_ln(read_mode_icon(keymap_config.swap_lalt_lgui), false);
        // oled_write_ln(read_host_led_state(), false);
        // oled_write_ln(read_timelog(), false);
    } else {
        oled_write(read_logo(), false);
    }
    return false;
}
#endif // OLED_ENABLE

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
#ifdef OLED_ENABLE
        set_keylog(keycode, record);
#endif
        // set_timelog();
        if (keycode == KC_VIM) {
            toggle_vim(vim);
        }
        return process_vim_key(vim, keycode);
    }
    return true;
}
void keyboard_post_init_user(void) {
    rgblight_enable();                              // Make sure it's on
    rgblight_mode(RGBLIGHT_MODE_RAINBOW_SWIRL + 1); // Set to some visible effect
}
