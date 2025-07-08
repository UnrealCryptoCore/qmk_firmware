#ifndef VIM_H
#define VIM_H
#include <stdbool.h>
#include <stdint.h>

#define VIM_MASK_SHIFT 0b01000000
#define VIM_MASK_CTRL 0b10000000
#define COMB2(a, b) ((a << 8) | b)

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
void        reset_vim_buf(Vim *vim);
void        tap_vim_code(Vim *vim, uint16_t keycode, int n);
bool        process_vim_key(Vim *vim, uint16_t keycode);
void        set_vim_mode(Vim *vim, vim_mode mode);
void        sendn_code(uint16_t keycode, int n);
uint8_t     get_keystroke(uint16_t keycode);

// from user
void    send_code(uint16_t keycode);
uint8_t get_modifiers(void);
#endif
