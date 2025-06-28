#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "oled_driver.h"
#include "tmk_core/protocol/usb_descriptor.h"
#include "action.h"
#include "action_util.h"
#include "host.h"
#include "keycodes.h"
#include "keymap_us.h"
#include "modifiers.h"
#include "quantum.h"
#include "quantum/raw_hid.h"
#include "quantum_keycodes.h"
#include QMK_KEYBOARD_H
#define VIM_IMPLEMENTATION
#include "vim.h"

enum layer_number {
    _QWERTY = 0,
    _LOWER,
    _RAISE,
    _ADJUST,
};

enum custom_keycodes {
    KC_VIM = SAFE_RANGE,
    KC_COPY_RAW,
    KC_PASTE_RAW,
};

typedef enum MSGType {
    MSG_NONE,
    MSG_OS_HELLO,
    MSG_OS_BYE,
    MSG_OS_COPY_START,
    MSG_OS_COPY_PART,
    MSG_KB_COPY,
} MSGType;

typedef struct MSGCopyData {
    uint8_t msg_type;
    char    data[31];
} MSGCopyData;


static char copy_buffer[1024] = { 0 };
static bool os_connected = false;

static Vim *vim = &(Vim){
    .mode  = VIM_DISABLED,
    .buf   = 0,
    .num1  = 0,
    .num2  = 0,
    .op    = OP_NONE,
    .state = VS_IDLE,
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
     * |      |      |      |      |      |      |                    |      |      |      |      | HOME |      |
     * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
     * |  F1  |  F2  |  F3  |  F4  |  F5  |  F6  |                    |  F7  |  F8  |  F9  | F10  | F11  | F12  |
     * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
     * |      |   !  |   @  |   #  |   $  |   %  |-------.    ,-------| Left | Down  | Up | Right | END  |      |
     * |------+------+------+------+------+------|       |    |       |------+------+------+------+------+------|
     * |      |      |      |      |  VIM |      |-------|    |-------|      |      |      |      |      |      |
     * `-----------------------------------------/       /     \      \-----------------------------------------'
     *                   | LALT | LGUI |LOWER | /Space  /       \Enter \  |RAISE |BackSP| RALT |
     *                   |      |      |      |/       /         \      \ |      |      |      |
     *                   `----------------------------'           '------''--------------------'
     */
    [_LOWER] = LAYOUT(_______, _______, _______, _______, _______, _______, _______, _______, _______, _______, KC_HOME, _______, KC_F1, KC_F2, KC_F3, KC_F4, KC_F5, KC_F6, KC_F7, KC_F8, KC_F9, KC_F10, KC_F11, KC_F12, _______, KC_EXLM, KC_AT, KC_HASH, KC_DLR, KC_PERC, KC_LEFT, KC_DOWN, KC_UP, KC_RGHT, KC_END, XXXXXXX, _______, _______, _______, _______, KC_VIM, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______),

    /* RAISE
     * ,-----------------------------------------.                    ,-----------------------------------------.
     * |      |      |      |      |      |      |                    |      |      |      |      |      |      |
     * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
     * |      |  1   |  2   |  3   |  4   |  5   |                    |   6  |   7  |   8  |   9  |   0  |      |
     * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
     * |      |   |  |  \   |  {   |   }  |  `   |-------.    ,-------|   ^  |   &  |   *  |   (  |   )  |   ~  |
     * |------+------+------+------+------+------|       |    |       |------+------+------+------+------+------|
     * |      |      |      |  [   |   ]  |      |-------|    |-------|      |      |      |      |      |      |
     * `-----------------------------------------/       /     \      \-----------------------------------------'
     *                   | LALT | LGUI |LOWER | /Space  /       \Enter \  |RAISE |BackSP| RALT |
     *                   |      |      |      |/       /         \      \ |      |      |      |
     *                   `----------------------------'           '------''--------------------'
     */
    [_RAISE] = LAYOUT(_______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, KC_P1, KC_P2, KC_P3, KC_P4, KC_P5, KC_P6, KC_P7, KC_P8, KC_P9, KC_P0, _______, _______, KC_PIPE, KC_BSLS, KC_LCBR, KC_RCBR, KC_GRAVE, KC_CIRC, KC_AMPR, KC_ASTR, KC_LPRN, KC_RPRN, KC_TILD, _______, _______, _______, KC_LBRC, KC_RBRC, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______, _______),
    /* ADJUST
     * ,-----------------------------------------.                    ,-----------------------------------------.
     * |      |      |MCPLAY|      |      |      |                    |      |      |      |      |      |      |
     * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
     * |      |MCREC |      |BRI+  | VOL+ |      |                    |CPRAW |      |      |      |PSTRAW|      |
     * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
     * |      |      |      |PREV  |PAUSE |NEXT  |-------.    ,-------|      |      |RGB ON| HUE+ | SAT+ | VAL+ |
     * |------+------+------+------+------+------|       |    |       |------+------+------+------+------+------|
     * |      |      |      |BRI-  | VOL- |      |-------|    |-------|      |      | MODE | HUE- | SAT- | VAL- |
     * `-----------------------------------------/       /     \      \-----------------------------------------'
     *                   | LALT | LGUI |LOWER | /Space  /       \Enter \  |RAISE |BackSP| RALT |
     *                   |      |      |      |/       /         \      \ |      |      |      |
     *                   `----------------------------'           '------''--------------------'
     */
    [_ADJUST] = LAYOUT(XXXXXXX, XXXXXXX, QK_DYNAMIC_MACRO_PLAY_1, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, QK_DYNAMIC_MACRO_RECORD_START_1, XXXXXXX, KC_BRIU, KC_VOLU, XXXXXXX, KC_COPY_RAW, XXXXXXX, XXXXXXX, XXXXXXX, KC_PASTE_RAW, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, KC_MEDIA_PREV_TRACK, KC_MEDIA_PLAY_PAUSE, KC_MEDIA_NEXT_TRACK, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, KC_BRID, KC_VOLD, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, XXXXXXX, _______, _______, _______, _______, _______, _______, _______, _______)};

layer_state_t layer_state_set_user(layer_state_t state) {
    return update_tri_layer_state(state, _LOWER, _RAISE, _ADJUST);
}

void process_copy_raw(void) {
    uint8_t response[RAW_EPSIZE] = {0};
    response[0] = MSG_KB_COPY;
    raw_hid_send(response, RAW_EPSIZE);
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
    char buf[24];
    if (is_keyboard_master()) {
        // If you want to change the display of OLED, you need to change here
        oled_write_ln(read_layer_state(), false);
        //oled_write_ln(read_keylog(), false);
        //oled_write_ln(read_keylogs(), false);
        oled_write_ln(read_vim_state(vim), false);
        snprintf(buf, sizeof(buf), "con: %s", os_connected ? "true" : "false");
        oled_write_ln(buf, false);
        snprintf(buf, sizeof(buf), "buf: %s", copy_buffer);
        oled_write_ln(buf, false);

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
        switch (keycode) {
            case KC_VIM:
                toggle_vim_mode(vim);
                return false;
            case KC_COPY_RAW:
                if (os_connected) {
                    process_copy_raw();
                }
                break;
            case KC_PASTE_RAW:
                SEND_STRING(copy_buffer);
                break;
            default:
                if (vim->mode != VIM_DISABLED) {
                    return process_vim_key(vim, keycode);
                }
                break;
        }
    }
    return true;
}

void keyboard_post_init_user(void) {
    rgblight_enable();
    rgblight_mode(RGBLIGHT_MODE_RAINBOW_SWIRL + 1);
}

void raw_hid_receive(uint8_t *data, uint8_t length) {
    uint8_t response[length];
    memset(response, 0, length);
    MSGCopyData *cpData;
    switch (data[0]) {
        case MSG_OS_HELLO:
            os_connected = true;
            raw_hid_send(data, length);
            break;
        case MSG_OS_BYE:
            os_connected = false;
            raw_hid_send(data, length);
            break;
        case MSG_OS_COPY_START:
            cpData = (MSGCopyData *)data;
            memset(copy_buffer, 0, sizeof(copy_buffer));
            strncpy(copy_buffer, cpData->data, sizeof(copy_buffer) - 1);
            raw_hid_send((uint8_t*)copy_buffer, length);
            break;
        case MSG_OS_COPY_PART:
            cpData = (MSGCopyData *)data;
            strncat(copy_buffer, cpData->data, sizeof(copy_buffer) - 1);
            raw_hid_send(data, length);
            break;
        default:
            response[0] = MSG_NONE;
            strcpy((char*)(response + 1), "invalid request");
            raw_hid_send(response, length);
            break;
    }
}
