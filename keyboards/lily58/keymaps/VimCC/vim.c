#include "vim.h"
#include <stdio.h>
#include "keycodes.h"
#include "quantum.h"

char  vim_state_buffer[21] = {0};
char *vim_mode_str[]       = {"disabled", "normal", "insert", "insert rep1", "insert rep", "find", "visual", "cmd"};

uint8_t get_keystroke(uint16_t keycode) {
    uint8_t mods       = get_modifiers();
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

void sendn_code(uint16_t keycode, int n) {
    for (int i = 0; i < n; i++) {
        send_code(keycode);
    }
}

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
            sendn_code(S(keycode), n);
            send_code(KC_COPY);
            /* fallthrough */
        case OP_NONE:
            if (vim->mode == VIM_VISUAL) {
                sendn_code(S(keycode), n);
            } else {
                sendn_code(keycode, n);
            }
            break;
        case OP_DELETE:
            sendn_code(S(keycode), n);
            send_code(KC_CUT);
            break;
        default:
            break;
    }
}

void set_vim_mode(Vim *vim, vim_mode mode) {
    vim->mode = mode;
}

bool process_vim_key(Vim *vim, uint16_t keycode) {
    if (keycode >= 64) {
        return true;
    }

    uint8_t key  = get_keystroke(keycode);
    uint8_t mods = get_modifiers();
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
                        set_vim_mode(vim, VIM_NORMAL);
                    } else {
                        ret = true;
                    }
                    break;
                case KC_A | VIM_MASK_SHIFT:
                    send_code(KC_END);
                    set_vim_mode(vim, VIM_INSERT);
                    break;
                case KC_A:
                    send_code(KC_RIGHT);
                    set_vim_mode(vim, VIM_INSERT);
                    break;
                case KC_I | VIM_MASK_SHIFT:
                    send_code(KC_HOME);
                    /* fallthrough */
                case KC_I:
                    set_vim_mode(vim, VIM_INSERT);
                    break;
                case KC_V:
                    set_vim_mode(vim, VIM_VISUAL);
                    break;
                case KC_U | VIM_MASK_CTRL:
                    send_code(KC_PAGE_UP);
                    break;
                case KC_D | VIM_MASK_CTRL:
                    send_code(KC_PAGE_DOWN);
                    break;
                case KC_B | VIM_MASK_CTRL:
                    send_code(KC_PAGE_UP);
                    break;
                case KC_F | VIM_MASK_CTRL:
                    send_code(KC_PAGE_DOWN);
                    break;
                case KC_P:
                    send_code(KC_PASTE);
                    break;
                case KC_U:
                    send_code(C(KC_Z));
                    break;
                case KC_R | VIM_MASK_CTRL:
                    send_code(C(KC_Y));
                    break;
                case KC_R:
                    set_vim_mode(vim, VIM_INSERT_REP1);
                    break;
                case KC_R | VIM_MASK_SHIFT:
                    set_vim_mode(vim, VIM_INSERT_REP);
                    break;
                case KC_N:
                    send_code(KC_ENTER);
                    break;
                case KC_N | VIM_MASK_SHIFT:
                    send_code(S(KC_ENTER));
                    break;
                case KC_O:
                    send_code(KC_END);
                    send_code(KC_ENTER);
                    set_vim_mode(vim, VIM_INSERT);
                case KC_O | VIM_MASK_SHIFT:
                    send_code(KC_UP);
                    send_code(KC_END);
                    send_code(KC_ENTER);
                    set_vim_mode(vim, VIM_INSERT);
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
                        send_code(KC_COPY);
                        set_vim_mode(vim, VIM_NORMAL);
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
                        send_code(KC_CUT);
                        set_vim_mode(vim, VIM_NORMAL);
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
                        send_code(KC_CUT);
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
                    send_code(C(KC_HOME));
                    tap_vim_code(vim, KC_DOWN, vim->num2);
                    break;
                case KC_X:
                    vim->num2 = vim->num2 == 0 ? 1 : vim->num2;
                    sendn_code(KC_DEL, vim->num2);
                    break;
                case KC_X | VIM_MASK_SHIFT:
                    vim->num2 = vim->num2 == 0 ? 1 : vim->num2;
                    sendn_code(KC_BACKSPACE, vim->num2);
                    break;
                case KC_SLSH:
                    send_code(C(KC_F));
                    set_vim_mode(vim, VIM_FIND);
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
                    set_vim_mode(vim, VIM_NORMAL);
                    ret = false;
                    break;
                case KC_H | VIM_MASK_CTRL:
                    send_code(KC_BACKSPACE);
                    ret = false;
                    break;
                default:
                    ret = true;
                    break;
            }
            if (vim->mode == VIM_INSERT_REP1) {
                send_code(KC_DELETE);
                send_code(KC_LEFT);
                set_vim_mode(vim, VIM_NORMAL);
            } else if (vim->mode == VIM_INSERT_REP) {
                send_code(KC_DELETE);
            }
            break;
        case VIM_FIND:
            switch (key) {
                case KC_ESC:
                    /* fallthrough */
                case KC_C | VIM_MASK_CTRL:
                    send_code(KC_ESC);
                    set_vim_mode(vim, VIM_NORMAL);
                    ret = false;
                    break;
                case KC_ENTER:
                    send_code(KC_ENTER);
                    ret = false;
                    break;
                default:
                    ret = true;
                    break;
            }
            break;
        case VIM_CMD:
            if (keycode == KC_ESC) {
                set_vim_mode(vim, VIM_NORMAL);
                ret = false;
                break;
            }
            break;
        default:
            break;
    }
    register_mods(mods);
    return ret;
}
