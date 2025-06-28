#ifndef VIM_H
#define VIM_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "quantum.h"

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

#ifndef STICKY_VIM
#    define VIM_INSERT VIM_DISABLED
#endif

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

void        toggle_vim_mode(Vim *vim);
void        stop_vim_mode(Vim *vim);
const char *read_vim_state(Vim *vim);
uint8_t     get_keystroke(uint16_t keycode);
void        tapn_code(uint16_t keycode, int n);
void        reset_vim_buf(Vim *vim);
void        tap_vim_code(Vim *vim, uint16_t keycode, int n);
bool        process_vim_key(Vim *vim, uint16_t keycode);

// #define VIM_IMPLEMENTATION
#ifdef VIM_IMPLEMENTATION
void toggle_vim_mode(Vim *vim) {
    if (vim->mode == VIM_DISABLED) {
        vim->mode = VIM_NORMAL;
    } else {
        vim->mode = VIM_DISABLED;
    }
}

void stop_vim_mode(Vim *vim) {
    vim->mode = VIM_DISABLED;
}

const char *read_vim_state(Vim *vim) {
    char *mode = vim_mode_str[vim->mode];
    snprintf(vim_state_buffer, sizeof(vim_state_buffer), "VIM: %s ", mode);
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
#endif
#endif
