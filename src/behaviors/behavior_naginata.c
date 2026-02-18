/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_naginata

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/behavior.h>
#include <zmk/hid.h>
#include <dt-bindings/zmk/modifiers.h>

#include <zmk_naginata/nglist.h>
#include <zmk_naginata/nglistarray.h>
#include <zmk_naginata/naginata_func.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);
extern int64_t timestamp;

#define NONE 0

// 薙刀式

// 31キーを32bitの各ビットに割り当てる
#define B_Q (1UL << 0)
#define B_W (1UL << 1)
#define B_E (1UL << 2)
#define B_R (1UL << 3)
#define B_T (1UL << 4)

#define B_Y (1UL << 5)
#define B_U (1UL << 6)
#define B_I (1UL << 7)
#define B_O (1UL << 8)
#define B_P (1UL << 9)

#define B_A (1UL << 10)
#define B_S (1UL << 11)
#define B_D (1UL << 12)
#define B_F (1UL << 13)
#define B_G (1UL << 14)

#define B_H (1UL << 15)
#define B_J (1UL << 16)
#define B_K (1UL << 17)
#define B_L (1UL << 18)
#define B_SEMI (1UL << 19)

#define B_Z (1UL << 20)
#define B_X (1UL << 21)
#define B_C (1UL << 22)
#define B_V (1UL << 23)
#define B_B (1UL << 24)

#define B_N (1UL << 25)
#define B_M (1UL << 26)
#define B_COMMA (1UL << 27)
#define B_DOT (1UL << 28)
#define B_SLASH (1UL << 29)

#define B_SPACE (1UL << 30)

static NGListArray nginput;
static int64_t nginput_updated_at[LIST_SIZE];
static uint32_t pressed_keys = 0UL; // 押しているキーのビットをたてる
static uint8_t pressed_b_space_count = 0;
static int8_t n_pressed_keys = 0;   // 押しているキーの数
static bool ime_preedit_pending = false;
static bool kuten_confirm_extra_backspace_pending = false;
static uint64_t bypass_keys = 0ULL;
static bool alpha_backspace_bypass_latched = false;
static bool forced_bypass_from_ng_off_lock = false;
static uint64_t consumed_jk_combo_release_keys = 0ULL;
static uint32_t late_shift_window_ms = 80;
static uint8_t kuten_confirm_mode = 1U;

struct behavior_naginata_config {
    uint32_t late_shift_window_ms;
    uint32_t kuten_confirm_enter;
};

#define KANA_BACKSPACE_HISTORY_SIZE 10
typedef struct {
    uint8_t backspace_count;
    uint8_t delete_count;
} kana_delete_action_t;

typedef struct {
    uint8_t backspace_count;
    uint8_t delete_count;
    bool from_history;
} naginata_backspace_action_t;

static kana_delete_action_t kana_delete_history[KANA_BACKSPACE_HISTORY_SIZE];
static uint8_t kana_delete_history_size = 0;
static bool kana_backspace_armed = false;
static bool kana_backspace_needs_space_undo = false;
static uint8_t pending_func_backspace_count = 0;
static uint8_t pending_func_delete_count = 0;

#define NG_WINDOWS 0
#define NG_MACOS 1
#define NG_LINUX 2
#define NG_IOS 3
#define KUTEN_CONFIRM_DISABLED 0U
#define KUTEN_CONFIRM_ENTER 1U
#define KUTEN_CONFIRM_SPACE 2U
#define MAX_PENDING_BYPASS_JK_KEYS 8

struct pending_bypass_jk_key {
    uint32_t keycode;
    bool released;
};

static struct pending_bypass_jk_key pending_bypass_jk_keys[MAX_PENDING_BYPASS_JK_KEYS];
static uint8_t pending_bypass_jk_keys_len = 0;

// EEPROMに保存する設定
typedef union {
    uint8_t os : 2;  // 2 bits can store values 0-3 (NG_WINDOWS, NG_MACOS, NG_LINUX, NG_IOS)
    bool tategaki : true; // true: 縦書き, false: 横書き
} user_config_t;

static uint64_t bypass_bit(uint32_t keycode) {
    switch (keycode) {
    case A ... Z:
        return 1ULL << (keycode - A);
    case SPACE:
        return 1ULL << 26;
    case ENTER:
        return 1ULL << 27;
    case TAB:
        return 1ULL << 28;
    case RIGHT:
        return 1ULL << 29;
    case LEFT:
        return 1ULL << 30;
    case DOWN:
        return 1ULL << 31;
    case UP:
        return 1ULL << 32;
    case BACKSPACE:
        return 1ULL << 33;
    case DOT:
        return 1ULL << 34;
    case COMMA:
        return 1ULL << 35;
    case SLASH:
        return 1ULL << 36;
    case SEMI:
        return 1ULL << 37;
    default:
        return 0ULL;
    }
}

extern user_config_t naginata_config;

static const uint32_t ng_key[] = {
    [A - A] = B_A,     [B - A] = B_B,         [C - A] = B_C,         [D - A] = B_D,
    [E - A] = B_E,     [F - A] = B_F,         [G - A] = B_G,         [H - A] = B_H,
    [I - A] = B_I,     [J - A] = B_J,         [K - A] = B_K,         [L - A] = B_L,
    [M - A] = B_M,     [N - A] = B_N,         [O - A] = B_O,         [P - A] = B_P,
    [Q - A] = B_Q,     [R - A] = B_R,         [S - A] = B_S,         [T - A] = B_T,
    [U - A] = B_U,     [V - A] = B_V,         [W - A] = B_W,         [X - A] = B_X,
    [Y - A] = B_Y,     [Z - A] = B_Z,         [SEMI - A] = B_SEMI,   [COMMA - A] = B_COMMA,
    [DOT - A] = B_DOT, [SLASH - A] = B_SLASH, [SPACE - A] = B_SPACE, [ENTER - A] = B_SPACE,
};

static uint32_t ng_keycode_to_bit(uint32_t keycode) {
    switch (keycode) {
    case A ... Z:
    case SEMI:
    case COMMA:
    case DOT:
    case SLASH:
    case SPACE:
    case ENTER:
        return ng_key[keycode - A];
    case BACKSPACE:
        return B_SPACE;
    default:
        return 0UL;
    }
}

static void pressed_keys_add(uint32_t keycode) {
    uint32_t bit = ng_keycode_to_bit(keycode);
    if (bit == 0UL) {
        return;
    }
    if (bit == B_SPACE) {
        if (pressed_b_space_count < 0xFF) {
            pressed_b_space_count++;
        }
        pressed_keys |= B_SPACE;
        return;
    }
    pressed_keys |= bit;
}

static void pressed_keys_remove(uint32_t keycode) {
    uint32_t bit = ng_keycode_to_bit(keycode);
    if (bit == 0UL) {
        return;
    }
    if (bit == B_SPACE) {
        if (pressed_b_space_count > 0) {
            pressed_b_space_count--;
        }
        if (pressed_b_space_count == 0) {
            pressed_keys &= ~B_SPACE;
        }
        return;
    }
    pressed_keys &= ~bit;
}

static void reset_pressed_keys_state(void) {
    pressed_keys = 0UL;
    pressed_b_space_count = 0;
}

// カナ変換テーブル
typedef struct {
    uint32_t shift;
    uint32_t douji;
    uint32_t kana[6];
    void (*func)(void);
} naginata_kanamap;

static naginata_kanamap ngdickana[] = {
    // 清音
    {.shift = NONE    , .douji = B_J            , .kana = {A, NONE, NONE, NONE, NONE, NONE}, .func = nofunc }, // あ
    {.shift = NONE    , .douji = B_K            , .kana = {I, NONE, NONE, NONE, NONE, NONE}, .func = nofunc }, // い
    {.shift = NONE    , .douji = B_L            , .kana = {U, NONE, NONE, NONE, NONE, NONE}, .func = nofunc }, // う
    {.shift = B_SPACE , .douji = B_O            , .kana = {E, NONE, NONE, NONE, NONE, NONE}, .func = nofunc }, // え
    {.shift = NONE    , .douji = B_U            , .kana = {O, NONE, NONE, NONE, NONE, NONE   }, .func = nofunc}, // お
    {.shift = NONE    , .douji = B_F            , .kana = {K, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // か
    {.shift = NONE    , .douji = B_W            , .kana = {K, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // き
    {.shift = NONE    , .douji = B_H            , .kana = {K, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // く
    {.shift = NONE    , .douji = B_S            , .kana = {K, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // け
    {.shift = NONE    , .douji = B_V            , .kana = {K, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // こ
    {.shift = B_SPACE , .douji = B_U            , .kana = {S, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // さ
    {.shift = NONE    , .douji = B_R            , .kana = {S, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // し
    {.shift = NONE    , .douji = B_O            , .kana = {S, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // す
    {.shift = B_SPACE , .douji = B_SLASH        , .kana = {S, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // す
    {.shift = B_SPACE , .douji = B_C            , .kana = {S, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // せ
    {.shift = NONE    , .douji = B_B            , .kana = {S, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // そ
    {.shift = NONE    , .douji = B_N            , .kana = {T, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // た
    {.shift = B_SPACE , .douji = B_G            , .kana = {T, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ち
    {.shift = B_SPACE , .douji = B_L            , .kana = {T, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // つ
    {.shift = NONE    , .douji = B_E            , .kana = {T, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // て
    {.shift = NONE    , .douji = B_D            , .kana = {T, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // と
    {.shift = NONE    , .douji = B_M            , .kana = {N, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // な
    {.shift = B_SPACE , .douji = B_D            , .kana = {N, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // に
    {.shift = B_SPACE , .douji = B_W            , .kana = {N, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぬ
    {.shift = B_SPACE , .douji = B_R           , .kana = {N, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ね
    {.shift = B_SPACE , .douji = B_J            , .kana = {N, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // の
    {.shift = NONE    , .douji = B_A            , .kana = {H, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ろ
    {.shift = NONE    , .douji = B_X            , .kana = {H, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ひ
    {.shift = B_SPACE , .douji = B_X            , .kana = {H, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ひ
    {.shift = B_SPACE , .douji = B_SEMI         , .kana = {H, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ふ
    {.shift = NONE    , .douji = B_P            , .kana = {H, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // へ
    {.shift = NONE    , .douji = B_Z            , .kana = {H, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ほ
    {.shift = B_SPACE , .douji = B_Z            , .kana = {H, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ほ
    {.shift = B_SPACE , .douji = B_F            , .kana = {M, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ま
    {.shift = B_SPACE , .douji = B_B            , .kana = {M, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // み
    {.shift = B_SPACE , .douji = B_COMMA        , .kana = {M, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // む
    {.shift = B_SPACE , .douji = B_S            , .kana = {M, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // め
    {.shift = B_SPACE , .douji = B_K            , .kana = {M, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // も
    {.shift = B_SPACE , .douji = B_H            , .kana = {Y, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // や
    {.shift = B_SPACE , .douji = B_P            , .kana = {Y, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゆ
    {.shift = B_SPACE , .douji = B_I            , .kana = {Y, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // よ
    {.shift = NONE    , .douji = B_DOT          , .kana = {R, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ら
    {.shift = B_SPACE , .douji = B_E            , .kana = {R, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // り
    {.shift = NONE    , .douji = B_I            , .kana = {R, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // る
    {.shift = NONE    , .douji = B_SLASH        , .kana = {R, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // れ
    {.shift = NONE    , .douji = B_C            , .kana = {R, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ろ
    {.shift = B_SPACE , .douji = B_DOT          , .kana = {W, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // わ
    {.shift = B_SPACE , .douji = B_A            , .kana = {W, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // を
    {.shift = NONE    , .douji = B_COMMA        , .kana = {N, N, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ん
    {.shift = NONE    , .douji = B_SEMI         , .kana = {MINUS, NONE, NONE, NONE, NONE, NONE}, .func = nofunc }, // ー

    // 濁音
    {.shift = NONE    , .douji = B_J|B_F        , .kana = {G, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // が
    {.shift = NONE    , .douji = B_J|B_W        , .kana = {G, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぎ
    {.shift = 0UL     , .douji = B_F|B_H        , .kana = {G, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぐ
    {.shift = NONE    , .douji = B_J|B_S        , .kana = {G, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // げ
    {.shift = NONE    , .douji = B_J|B_V        , .kana = {G, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ご
    {.shift = 0UL     , .douji = B_F|B_U        , .kana = {Z, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ざ
    {.shift = NONE    , .douji = B_J|B_R        , .kana = {Z, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // じ
    {.shift = 0UL     , .douji = B_F|B_O        , .kana = {Z, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ず
    {.shift = NONE    , .douji = B_J|B_C        , .kana = {Z, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぜ
    {.shift = NONE    , .douji = B_J|B_B        , .kana = {Z, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぞ
    {.shift = 0UL     , .douji = B_F|B_N        , .kana = {D, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // だ
    {.shift = NONE    , .douji = B_J|B_G        , .kana = {D, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぢ
    {.shift = 0UL     , .douji = B_F|B_L        , .kana = {D, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // づ
    {.shift = NONE    , .douji = B_J|B_E        , .kana = {D, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // で
    {.shift = NONE    , .douji = B_J|B_D        , .kana = {D, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ど
    {.shift = NONE    , .douji = B_J|B_A        , .kana = {B, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ば
    {.shift = NONE    , .douji = B_J|B_X        , .kana = {B, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // び
    {.shift = 0UL     , .douji = B_F|B_SEMI     , .kana = {B, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぶ
    {.shift = 0UL     , .douji = B_F|B_P        , .kana = {B, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // べ
    {.shift = NONE    , .douji = B_J|B_Z        , .kana = {B, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぼ
    {.shift = 0UL     , .douji = B_F|B_L|B_SEMI , .kana = {V, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゔ

    // 半濁音
    {.shift = NONE    , .douji = B_M|B_A        , .kana = {P, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぱ
    {.shift = NONE    , .douji = B_M|B_X        , .kana = {P, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぴ
    {.shift = NONE    , .douji = B_V|B_SEMI     , .kana = {P, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぷ
    {.shift = NONE    , .douji = B_V|B_P        , .kana = {P, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぺ
    {.shift = NONE    , .douji = B_M|B_Z        , .kana = {P, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぽ

    // 小書き
    {.shift = NONE    , .douji = B_Q|B_H        , .kana = {X, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // ゃ
    {.shift = NONE    , .douji = B_Q|B_P        , .kana = {X, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // ゅ
    {.shift = NONE    , .douji = B_Q|B_I        , .kana = {X, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // ょ
    {.shift = NONE    , .douji = B_Q|B_J        , .kana = {X, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぁ
    {.shift = NONE    , .douji = B_Q|B_K        , .kana = {X, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぃ
    {.shift = NONE    , .douji = B_Q|B_L        , .kana = {X, U, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぅ
    {.shift = NONE    , .douji = B_Q|B_O        , .kana = {X, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぇ
    {.shift = NONE    , .douji = B_Q|B_U        , .kana = {X, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ぉ
    {.shift = NONE    , .douji = B_Q|B_DOT      , .kana = {X, W, A, NONE, NONE, NONE      }, .func = nofunc }, // ゎ
    {.shift = NONE    , .douji = B_G            , .kana = {X, T, U, NONE, NONE, NONE      }, .func = nofunc }, // っ
    {.shift = NONE    , .douji = B_Q|B_S        , .kana = {X, K, E, NONE, NONE, NONE      }, .func = nofunc }, // ヶ
    {.shift = NONE    , .douji = B_Q|B_F        , .kana = {X, K, A, NONE, NONE, NONE      }, .func = nofunc }, // ヵ

    // 清音拗音 濁音拗音 半濁拗音
    {.shift = NONE    , .douji = B_R|B_H        , .kana = {S, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // しゃ
    {.shift = NONE    , .douji = B_R|B_P        , .kana = {S, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // しゅ
    {.shift = NONE    , .douji = B_R|B_I        , .kana = {S, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // しょ
    {.shift = NONE    , .douji = B_J|B_R|B_H    , .kana = {Z, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // じゃ
    {.shift = NONE    , .douji = B_J|B_R|B_P    , .kana = {Z, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // じゅ
    {.shift = NONE    , .douji = B_J|B_R|B_I    , .kana = {Z, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // じょ
    {.shift = NONE    , .douji = B_W|B_H        , .kana = {K, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // きゃ
    {.shift = NONE    , .douji = B_W|B_P        , .kana = {K, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // きゅ
    {.shift = NONE    , .douji = B_W|B_I        , .kana = {K, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // きょ
    {.shift = NONE    , .douji = B_J|B_W|B_H    , .kana = {G, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // ぎゃ
    {.shift = NONE    , .douji = B_J|B_W|B_P    , .kana = {G, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // ぎゅ
    {.shift = NONE    , .douji = B_J|B_W|B_I    , .kana = {G, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // ぎょ
    {.shift = NONE    , .douji = B_G|B_H        , .kana = {T, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // ちゃ
    {.shift = NONE    , .douji = B_G|B_P        , .kana = {T, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // ちゅ
    {.shift = NONE    , .douji = B_G|B_I        , .kana = {T, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // ちょ
    {.shift = NONE    , .douji = B_J|B_G|B_H    , .kana = {D, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // ぢゃ
    {.shift = NONE    , .douji = B_J|B_G|B_P    , .kana = {D, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // ぢゅ
    {.shift = NONE    , .douji = B_J|B_G|B_I    , .kana = {D, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // ぢょ
    {.shift = NONE    , .douji = B_D|B_H        , .kana = {N, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // にゃ
    {.shift = NONE    , .douji = B_D|B_P        , .kana = {N, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // にゅ
    {.shift = NONE    , .douji = B_D|B_I        , .kana = {N, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // にょ
    {.shift = NONE    , .douji = B_X|B_H        , .kana = {H, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // ひゃ
    {.shift = NONE    , .douji = B_X|B_P        , .kana = {H, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // ひゅ
    {.shift = NONE    , .douji = B_X|B_I        , .kana = {H, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // ひょ
    {.shift = NONE    , .douji = B_J|B_X|B_H    , .kana = {B, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // びゃ
    {.shift = NONE    , .douji = B_J|B_X|B_P    , .kana = {B, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // びゅ
    {.shift = NONE    , .douji = B_J|B_X|B_I    , .kana = {B, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // びょ
    {.shift = NONE    , .douji = B_M|B_X|B_H    , .kana = {P, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // ぴゃ
    {.shift = NONE    , .douji = B_M|B_X|B_P    , .kana = {P, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // ぴゅ
    {.shift = NONE    , .douji = B_M|B_X|B_I    , .kana = {P, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // ぴょ
    {.shift = NONE    , .douji = B_B|B_H        , .kana = {M, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // みゃ
    {.shift = NONE    , .douji = B_B|B_P        , .kana = {M, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // みゅ
    {.shift = NONE    , .douji = B_B|B_I        , .kana = {M, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // みょ
    {.shift = NONE    , .douji = B_E|B_H        , .kana = {R, Y, A, NONE, NONE, NONE      }, .func = nofunc }, // りゃ
    {.shift = NONE    , .douji = B_E|B_P        , .kana = {R, Y, O, NONE, NONE, NONE      }, .func = nofunc }, // りゅ
    {.shift = NONE    , .douji = B_E|B_I        , .kana = {R, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // りょ

    // 清音外来音 濁音外来音
    {.shift = NONE    , .douji = B_M|B_E|B_K    , .kana = {T, H, I, NONE, NONE, NONE      }, .func = nofunc }, // てぃ
    {.shift = NONE    , .douji = B_M|B_E|B_I    , .kana = {T, E, X, Y, U, NONE            }, .func = nofunc }, // てゅ
    {.shift = NONE    , .douji = B_J|B_E|B_K    , .kana = {D, H, I, NONE, NONE, NONE      }, .func = nofunc }, // でぃ
    {.shift = NONE    , .douji = B_J|B_E|B_I    , .kana = {D, H, U, NONE, NONE, NONE      }, .func = nofunc }, // でゅ
    {.shift = NONE    , .douji = B_M|B_D|B_L    , .kana = {T, O, X, U, NONE, NONE         }, .func = nofunc }, // とぅ
    {.shift = NONE    , .douji = B_J|B_D|B_L    , .kana = {D, O, X, U, NONE, NONE         }, .func = nofunc }, // どぅ
    {.shift = NONE    , .douji = B_M|B_R|B_O    , .kana = {S, Y, E, NONE, NONE, NONE      }, .func = nofunc }, // しぇ
    {.shift = NONE    , .douji = B_M|B_G|B_O    , .kana = {T, Y, E, NONE, NONE, NONE      }, .func = nofunc }, // ちぇ
    {.shift = NONE    , .douji = B_J|B_R|B_O    , .kana = {Z, Y, E, NONE, NONE, NONE      }, .func = nofunc }, // じぇ
    {.shift = NONE    , .douji = B_J|B_G|B_O    , .kana = {D, Y, E, NONE, NONE, NONE      }, .func = nofunc }, // ぢぇ
    {.shift = NONE    , .douji = B_V|B_SEMI|B_J , .kana = {F, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ふぁ
    {.shift = NONE    , .douji = B_V|B_SEMI|B_K , .kana = {F, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ふぃ
    {.shift = NONE    , .douji = B_V|B_SEMI|B_O , .kana = {F, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ふぇ
    {.shift = NONE    , .douji = B_V|B_SEMI|B_U , .kana = {F, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ふぉ
    {.shift = NONE    , .douji = B_V|B_SEMI|B_I , .kana = {F, Y, U, NONE, NONE, NONE      }, .func = nofunc }, // ふゅ
    {.shift = NONE    , .douji = B_V|B_K|B_O    , .kana = {I, X, E, NONE, NONE, NONE      }, .func = nofunc }, // いぇ
    {.shift = NONE    , .douji = B_V|B_L|B_K    , .kana = {W, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // うぃ
    {.shift = NONE    , .douji = B_V|B_L|B_O    , .kana = {W, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // うぇ
    {.shift = NONE    , .douji = B_V|B_L|B_U    , .kana = {U, X, O, NONE, NONE, NONE      }, .func = nofunc }, // うぉ
    {.shift = NONE    , .douji = B_F|B_L|B_J    , .kana = {V, A, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゔぁ
    {.shift = NONE    , .douji = B_F|B_L|B_K    , .kana = {V, I, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゔぃ
    {.shift = NONE    , .douji = B_F|B_L|B_O    , .kana = {V, E, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゔぇ
    {.shift = NONE    , .douji = B_F|B_L|B_U    , .kana = {V, O, NONE, NONE, NONE, NONE   }, .func = nofunc }, // ゔぉ
    {.shift = NONE    , .douji = B_F|B_L|B_P    , .kana = {V, U, X, Y, U, NONE            }, .func = nofunc }, // ゔゅ
    {.shift = NONE    , .douji = B_V|B_H|B_J    , .kana = {K, U, X, A, NONE, NONE         }, .func = nofunc }, // くぁ
    {.shift = NONE    , .douji = B_V|B_H|B_K    , .kana = {K, U, X, I, NONE, NONE         }, .func = nofunc }, // くぃ
    {.shift = NONE    , .douji = B_V|B_H|B_O    , .kana = {K, U, X, E, NONE, NONE         }, .func = nofunc }, // くぇ
    {.shift = NONE    , .douji = B_V|B_H|B_U    , .kana = {K, U, X, O, NONE, NONE         }, .func = nofunc }, // くぉ
    {.shift = NONE    , .douji = B_V|B_H|B_DOT  , .kana = {K, U, X, W, A, NONE            }, .func = nofunc }, // くゎ
    {.shift = NONE    , .douji = B_F|B_H|B_J    , .kana = {G, U, X, A, NONE, NONE         }, .func = nofunc }, // ぐぁ
    {.shift = NONE    , .douji = B_F|B_H|B_K    , .kana = {G, U, X, I, NONE, NONE         }, .func = nofunc }, // ぐぃ
    {.shift = NONE    , .douji = B_F|B_H|B_O    , .kana = {G, U, X, E, NONE, NONE         }, .func = nofunc }, // ぐぇ
    {.shift = NONE    , .douji = B_F|B_H|B_U    , .kana = {G, U, X, O, NONE, NONE         }, .func = nofunc }, // ぐぉ
    {.shift = NONE    , .douji = B_F|B_H|B_DOT  , .kana = {G, U, X, W, A, NONE            }, .func = nofunc }, // ぐゎ
    {.shift = NONE    , .douji = B_V|B_L|B_J    , .kana = {T, S, A, NONE, NONE, NONE      }, .func = nofunc }, // つぁ
    
    // 追加
    {.shift = NONE    , .douji = B_SPACE        , .kana = {SPACE, NONE, NONE, NONE, NONE, NONE  }, .func = nofunc},
    {.shift = B_SPACE , .douji = B_V            , .kana = {COMMA, ENTER, NONE, NONE, NONE, NONE }, .func = nofunc},
    {.shift = NONE    , .douji = B_Q            , .kana = {R, E, NONE, NONE, NONE, NONE   }, .func = nofunc},
    {.shift = B_SPACE , .douji = B_M            , .kana = {DOT, ENTER, NONE, NONE, NONE, NONE   }, .func = nofunc},

    {.shift = NONE    , .douji = B_V|B_M        , .kana = {NONE, NONE, NONE, NONE, NONE, NONE   }, .func = nofunc}, // disabled: vm confirm
    // {.shift = B_SPACE, .douji = B_V|B_M, .kana = {ENTER, NONE, NONE, NONE, NONE, NONE}, .func = nofunc}, // enter+シフト(連続シフト)

    {.shift = NONE    , .douji = B_T            , .kana = {NONE, NONE, NONE, NONE, NONE, NONE   }, .func = nofunc}, //
    {.shift = NONE    , .douji = B_Y            , .kana = {NONE, NONE, NONE, NONE, NONE, NONE   }, .func = nofunc}, //
    {.shift = B_SPACE , .douji = B_T            , .kana = {NONE, NONE, NONE, NONE, NONE, NONE   }, .func = ng_ST}, //
    // {.shift = NONE, .douji = B_F | B_G, .kana = {NONE, NONE, NONE, NONE, NONE, NONE}, .func = naginata_off}, // 　かなオフ

    // 編集モード
    {.shift = B_J|B_K    , .douji = B_Q     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKQ    }, // ^{End}
    {.shift = B_J|B_K    , .douji = B_W     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKW    }, // ／{改行}
    {.shift = B_J|B_K    , .douji = B_E     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKE    }, // /*ディ*/
    {.shift = B_J|B_K    , .douji = B_R     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKR    }, // ^s
    {.shift = B_J|B_K    , .douji = B_T     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKT    }, // ・
    {.shift = B_J|B_K    , .douji = B_A     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKA    }, // ……{改行}
    {.shift = B_J|B_K    , .douji = B_S     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKS    }, // 『{改行}
    {.shift = B_J|B_K    , .douji = B_D     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKD    }, // ？{改行}
    {.shift = B_J|B_K    , .douji = B_F     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKF    }, // 「{改行}
    {.shift = B_J|B_K    , .douji = B_G     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKG    }, // ({改行}
    {.shift = B_J|B_K    , .douji = B_Z     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKZ    }, // ――{改行}
    {.shift = B_J|B_K    , .douji = B_X     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKX    }, // 』{改行}
    {.shift = B_J|B_K    , .douji = B_C     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKC    }, // ！{改行}
    {.shift = B_J|B_K    , .douji = B_V     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKV    }, // 」{改行}
    {.shift = B_J|B_K    , .douji = B_B     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_JKB    }, // ){改行}
    {.shift = B_D|B_F    , .douji = B_Y     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFY    }, // {Home}
    {.shift = B_D|B_F    , .douji = B_U     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFU    }, // +{End}{BS}
    {.shift = B_D|B_F    , .douji = B_I     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFI    }, // {vk1Csc079}
    {.shift = B_D|B_F    , .douji = B_O     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFO    }, // {Del}
    {.shift = B_D|B_F    , .douji = B_P     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFP    }, // +{Esc 2}
    {.shift = B_D|B_F    , .douji = B_H     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFH    }, // {Enter}{End}
    {.shift = B_D|B_F    , .douji = B_J     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFJ    }, // {↑}
    {.shift = B_D|B_F    , .douji = B_K     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFK    }, // +{↑}
    {.shift = B_D|B_F    , .douji = B_L     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFL    }, // +{↑ 7}
    {.shift = B_D|B_F    , .douji = B_SEMI  , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFSCLN }, // ^i
    {.shift = B_D|B_F    , .douji = B_N     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFN    }, // {End}
    {.shift = B_D|B_F    , .douji = B_M     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFM    }, // {↓}
    {.shift = B_D|B_F    , .douji = B_COMMA , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFCOMM }, // +{↓}
    {.shift = B_D|B_F    , .douji = B_DOT   , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFDOT  }, // +{↓ 7}
    {.shift = B_D|B_F    , .douji = B_SLASH , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_DFSLSH }, // ^u
    {.shift = B_M|B_COMMA, .douji = B_Q     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCQ    }, // ｜{改行}
    {.shift = B_M|B_COMMA, .douji = B_W     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCW    }, // 　　　×　　　×　　　×{改行 2}
    {.shift = B_M|B_COMMA, .douji = B_E     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCE    }, // {Home}{→}{End}{Del 2}{←}
    {.shift = B_M|B_COMMA, .douji = B_R     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCR    }, // {Home}{改行}{Space 1}{←}
    {.shift = B_M|B_COMMA, .douji = B_T     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCT    }, // 〇{改行}
    {.shift = B_M|B_COMMA, .douji = B_A     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCA    }, // 《{改行}
    {.shift = B_M|B_COMMA, .douji = B_S     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCS    }, // 【{改行}
    {.shift = B_M|B_COMMA, .douji = B_D     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCD    }, // {Home}{→}{End}{Del 4}{←}
    {.shift = B_M|B_COMMA, .douji = B_F     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCF    }, // {Home}{改行}{Space 3}{←}
    {.shift = B_M|B_COMMA, .douji = B_G     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCG    }, // {Space 3}
    {.shift = B_M|B_COMMA, .douji = B_Z     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCZ    }, // 》{改行}
    {.shift = B_M|B_COMMA, .douji = B_X     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCX    }, // 】{改行}
    {.shift = B_M|B_COMMA, .douji = B_C     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCC    }, // 」{改行}{改行}
    {.shift = B_M|B_COMMA, .douji = B_V     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCV    }, // 」{改行}{改行}「{改行}
    {.shift = B_M|B_COMMA, .douji = B_B     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_MCB    }, // 」{改行}{改行}{Space}
    {.shift = B_C|B_V    , .douji = B_Y     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVY    }, // +{Home}
    {.shift = B_C|B_V    , .douji = B_U     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVU    }, // ^x
    {.shift = B_C|B_V    , .douji = B_I     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVI    }, // {vk1Csc079}
    {.shift = B_C|B_V    , .douji = B_O     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVO    }, // ^v
    {.shift = B_C|B_V    , .douji = B_P     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVP    }, // ^z
    {.shift = B_C|B_V    , .douji = B_H     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVH    }, // ^c
    {.shift = B_C|B_V    , .douji = B_J     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVJ    }, // {←}
    {.shift = B_C|B_V    , .douji = B_K     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVK    }, // {→}
    {.shift = B_C|B_V    , .douji = B_L     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVL    }, // {改行}{Space}+{Home}^x{BS}
    {.shift = B_C|B_V    , .douji = B_SEMI  , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVSCLN }, // ^y
    {.shift = B_C|B_V    , .douji = B_N     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVN    }, // +{End}
    {.shift = B_C|B_V    , .douji = B_M     , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVM    }, // +{←}
    {.shift = B_C|B_V    , .douji = B_COMMA , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVCOMM }, // +{→}
    {.shift = B_C|B_V    , .douji = B_DOT   , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVDOT  }, // +{← 7}
    {.shift = B_C|B_V    , .douji = B_SLASH , .kana = {NONE, NONE, NONE, NONE, NONE, NONE} , .func = ngh_CVSLSH }, // +{→ 7}

};

// Helper function for counting matches/candidates
static int count_kana_entries(NGList *keys, bool exact_match) {
  if (keys->size == 0) return 0;

  int count = 0;
  uint32_t keyset0 = 0UL, keyset1 = 0UL, keyset2 = 0UL;
  uint32_t keyset_all = 0UL;
  
  // keysetを配列にしたらバイナリサイズが増えた
  switch (keys->size) {
    case 1:
      keyset0 = ng_keycode_to_bit(keys->elements[0]);
      keyset_all = keyset0;
      break;
    case 2:
      keyset0 = ng_keycode_to_bit(keys->elements[0]);
      keyset1 = ng_keycode_to_bit(keys->elements[1]);
      keyset_all = keyset0 | keyset1;
      break;
    default:
      keyset0 = ng_keycode_to_bit(keys->elements[0]);
      keyset1 = ng_keycode_to_bit(keys->elements[1]);
      keyset2 = ng_keycode_to_bit(keys->elements[2]);
      keyset_all = keyset0 | keyset1 | keyset2;
      break;
  }

  for (int i = 0; i < sizeof ngdickana / sizeof ngdickana[i]; i++) {
    bool matches = false;

    switch (keys->size) {
      case 1:
        if (exact_match) {
          matches = (ngdickana[i].shift == keyset0) || 
                   (ngdickana[i].shift == 0UL && ngdickana[i].douji == keyset0);
        } else {
          matches = ((ngdickana[i].shift & keyset0) == keyset0) ||
                   (ngdickana[i].shift == 0UL && (ngdickana[i].douji & keyset0) == keyset0);
        }
        break;
      case 2:
        if (exact_match) {
          matches = ((ngdickana[i].shift | ngdickana[i].douji) == keyset_all);
        } else {
          matches = (((ngdickana[i].shift | ngdickana[i].douji) & keyset_all) == keyset_all);
          // しぇ、ちぇ、など2キーで確定してはいけない
          if (matches && (ngdickana[i].shift | ngdickana[i].douji) != keyset_all) {
            count = 2;
          }
        }
        break;
      default:
        if (exact_match) {
          matches = ((ngdickana[i].shift | ngdickana[i].douji) == keyset_all);
        } else {
          matches = (((ngdickana[i].shift | ngdickana[i].douji) & keyset_all) == keyset_all);
        }
        break;
    }

    if (matches) {
      count++;
      if (count > 1) break;
    }
  }

  return count;
}

int number_of_matches(NGList *keys) {  
  int result = count_kana_entries(keys, true);
  return result;
}

int number_of_candidates(NGList *keys) {
  int result = count_kana_entries(keys, false);
  return result;
}

static bool nglist_contains_key(const NGList *keys, uint32_t keycode) {
    for (int i = 0; i < keys->size; i++) {
        if (keys->elements[i] == keycode) {
            return true;
        }
    }
    return false;
}

static bool is_shift_keycode(uint32_t keycode) { return keycode == SPACE || keycode == ENTER; }

static bool nglist_contains_shift_key(const NGList *keys) {
    return nglist_contains_key(keys, SPACE) || nglist_contains_key(keys, ENTER);
}

static bool nglist_contains_dual_shift_keys(const NGList *keys) {
    return nglist_contains_key(keys, SPACE) && nglist_contains_key(keys, ENTER);
}

static void clear_kana_output_history(void) {
    kana_delete_history_size = 0;
    kana_backspace_armed = false;
    kana_backspace_needs_space_undo = false;
    ime_preedit_pending = false;
    kuten_confirm_extra_backspace_pending = false;
}

static void disarm_kana_backspace_history(void) {
    kana_backspace_armed = false;
    kana_backspace_needs_space_undo = false;
}

static void preserve_history_for_space_undo(void) {
    if (kana_delete_history_size == 0) {
        return;
    }
    kana_backspace_armed = false;
    kana_backspace_needs_space_undo = true;
}

static void push_kana_delete_action(uint8_t backspace_count, uint8_t delete_count) {
    if (backspace_count == 0 && delete_count == 0) {
        return;
    }
    if (kana_delete_history_size >= KANA_BACKSPACE_HISTORY_SIZE) {
        for (int i = 1; i < KANA_BACKSPACE_HISTORY_SIZE; i++) {
            kana_delete_history[i - 1] = kana_delete_history[i];
        }
        kana_delete_history_size = KANA_BACKSPACE_HISTORY_SIZE - 1;
    }

    kana_delete_history[kana_delete_history_size++] =
        (kana_delete_action_t){.backspace_count = backspace_count, .delete_count = delete_count};
    kana_backspace_armed = true;
    kana_backspace_needs_space_undo = false;
}

static void push_kana_output_len(uint8_t len) {
    if (len == 0) {
        return;
    }
    if (len > 5) {
        len = 5;
    }
    push_kana_delete_action(len, 0);
}

static kana_delete_action_t pop_kana_delete_action(void) {
    if (kana_delete_history_size == 0) {
        kana_backspace_armed = false;
        return (kana_delete_action_t){0};
    }

    kana_delete_action_t action = kana_delete_history[--kana_delete_history_size];
    kana_backspace_armed = (kana_delete_history_size > 0);
    return action;
}

void ng_set_func_backspace_action(uint8_t backspace_count, uint8_t delete_count) {
    pending_func_backspace_count = backspace_count;
    pending_func_delete_count = delete_count;
}

void ng_set_forced_bypass(uint8_t active) { forced_bypass_from_ng_off_lock = (active != 0); }

void ng_arm_bypass_latch(void) { alpha_backspace_bypass_latched = true; }

static void clear_bypass_mode_state(void) {
    alpha_backspace_bypass_latched = false;
    forced_bypass_from_ng_off_lock = false;
    pending_bypass_jk_keys_len = 0;
    consumed_jk_combo_release_keys = 0ULL;
}

static bool is_alpha_keycode(uint32_t keycode) { return keycode >= A && keycode <= Z; }

static bool is_navigation_or_tab_keycode(uint32_t keycode) {
    return keycode == TAB || keycode == LEFT || keycode == RIGHT || keycode == UP ||
           keycode == DOWN;
}

static bool is_symbol_keycode(uint32_t keycode) {
    return keycode == DOT || keycode == COMMA || keycode == SLASH || keycode == SEMI;
}

static bool is_latched_bypass_keycode(uint32_t keycode) {
    return is_alpha_keycode(keycode) || keycode == BACKSPACE || keycode == SPACE ||
           keycode == ENTER || is_navigation_or_tab_keycode(keycode) ||
           is_symbol_keycode(keycode);
}

static bool is_jk_function_combo_keycode(uint32_t keycode) {
    uint32_t douji_bit = ng_keycode_to_bit(keycode);
    if (douji_bit == 0UL) {
        return false;
    }
    for (int i = 0; i < sizeof ngdickana / sizeof ngdickana[0]; i++) {
        if (ngdickana[i].func == nofunc) {
            continue;
        }
        if (ngdickana[i].shift == (B_J | B_K) && ngdickana[i].douji == douji_bit) {
            return true;
        }
    }
    return false;
}

static bool is_bypass_jk_combo_candidate_keycode(uint32_t keycode) {
    return keycode == J || keycode == K || keycode == SPACE || keycode == ENTER ||
           is_jk_function_combo_keycode(keycode);
}

static int pending_bypass_jk_index(uint32_t keycode) {
    for (int i = 0; i < pending_bypass_jk_keys_len; i++) {
        if (pending_bypass_jk_keys[i].keycode == keycode) {
            return i;
        }
    }
    return -1;
}

static bool remove_pending_bypass_jk_index(int idx) {
    if (idx < 0 || idx >= pending_bypass_jk_keys_len) {
        return false;
    }
    for (int i = idx + 1; i < pending_bypass_jk_keys_len; i++) {
        pending_bypass_jk_keys[i - 1] = pending_bypass_jk_keys[i];
    }
    pending_bypass_jk_keys_len--;
    return true;
}

static bool remove_pending_bypass_jk_keycode(uint32_t keycode) {
    int idx = pending_bypass_jk_index(keycode);
    if (idx < 0) {
        return false;
    }
    return remove_pending_bypass_jk_index(idx);
}

static bool append_pending_bypass_jk_key(uint32_t keycode) {
    int idx = pending_bypass_jk_index(keycode);
    if (idx >= 0) {
        pending_bypass_jk_keys[idx].released = false;
        return true;
    }
    if (pending_bypass_jk_keys_len >= MAX_PENDING_BYPASS_JK_KEYS) {
        return false;
    }
    pending_bypass_jk_keys[pending_bypass_jk_keys_len++] =
        (struct pending_bypass_jk_key){.keycode = keycode, .released = false};
    return true;
}

static void tap_keycode(uint32_t keycode) {
    raise_zmk_keycode_state_changed_from_encoded(keycode, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(keycode, false, timestamp);
}

static void flush_released_pending_bypass_jk_keys(void) {
    while (pending_bypass_jk_keys_len > 0 && pending_bypass_jk_keys[0].released) {
        uint32_t keycode = pending_bypass_jk_keys[0].keycode;
        // 薙刀レイヤーのENTERキーは右スペースとして扱うため、
        // バイパス単押しのフォールバックもSPACEを送る。
        if (keycode == ENTER) {
            keycode = SPACE;
        }
        tap_keycode(keycode);
        remove_pending_bypass_jk_index(0);
    }
}

static bool should_reset_bypass_on_keycode_event(const struct zmk_keycode_state_changed *ev) {
    if (ev == NULL || !ev->state || ev->usage_page != HID_USAGE_KEY) {
        return false;
    }
    uint32_t esc_id = ZMK_HID_USAGE_ID(ESC);
    uint32_t lang1_id = ZMK_HID_USAGE_ID(LANG1);
    uint32_t lang2_id = ZMK_HID_USAGE_ID(LANG2);
    return ev->keycode == esc_id || ev->keycode == lang1_id || ev->keycode == lang2_id;
}

static bool should_clear_kuten_confirm_pending_on_keycode_event(
    const struct zmk_keycode_state_changed *ev) {
    if (!kuten_confirm_extra_backspace_pending) {
        return false;
    }
    if (ev == NULL || !ev->state || ev->usage_page != HID_USAGE_KEY) {
        return false;
    }

    uint32_t backspace_id = ZMK_HID_USAGE_ID(BACKSPACE);
    return ev->keycode != backspace_id;
}

static bool is_vowel_keycode(uint32_t keycode) {
    return keycode == A || keycode == I || keycode == U || keycode == E || keycode == O;
}

static uint8_t estimate_kana_output_len(const uint32_t kana[6]) {
    uint32_t seq[6];
    int n = 0;
    for (int i = 0; i < 6; i++) {
        if (kana[i] == NONE) {
            break;
        }
        seq[n++] = kana[i];
    }

    if (n <= 0) {
        return 0;
    }
    if (n == 1) {
        return 1;
    }

    for (int i = 0; i < n; i++) {
        if (!is_alpha_keycode(seq[i])) {
            return 1;
        }
    }

    if (seq[0] == X) {
        return 1;
    }
    if (n == 2 && seq[0] == N && seq[1] == N) {
        return 1;
    }
    if (n >= 3 && seq[1] == Y && is_vowel_keycode(seq[2])) {
        return 2;
    }
    if (n >= 3 && seq[1] == H && is_vowel_keycode(seq[2])) {
        return 2;
    }
    for (int i = 1; i < n; i++) {
        if (seq[i] == X) {
            return 2;
        }
    }
    if (n == 2 && (seq[0] == F || seq[0] == V) && is_vowel_keycode(seq[1])) {
        return 2;
    }
    if (n == 2 && seq[0] == W && (seq[1] == I || seq[1] == E)) {
        return 2;
    }

    return 1;
}

static bool has_kuten_confirm_sequence(const uint32_t kana[6]) {
    for (int i = 1; i < 6; i++) {
        if (kana[i] == NONE) {
            break;
        }
        if (kana[i] == ENTER && (kana[i - 1] == COMMA || kana[i - 1] == DOT)) {
            return true;
        }
    }
    return false;
}

static void clear_nginput_timestamps(void) {
    for (int i = 0; i < LIST_SIZE; i++) {
        nginput_updated_at[i] = 0;
    }
}

static bool nginput_add(NGList *list) {
    bool ok = addToListArray(&nginput, list);
    if (ok && nginput.size > 0) {
        nginput_updated_at[nginput.size - 1] = timestamp;
    }
    return ok;
}

static bool nginput_remove_at(int idx) {
    if (idx < 0 || idx >= nginput.size) {
        return false;
    }
    if (!removeFromListArrayAt(&nginput, idx)) {
        return false;
    }
    for (int i = idx; i < LIST_SIZE - 1; i++) {
        nginput_updated_at[i] = nginput_updated_at[i + 1];
    }
    nginput_updated_at[LIST_SIZE - 1] = 0;
    return true;
}

static bool within_late_shift_window(int idx) {
    if (idx < 0 || idx >= nginput.size) {
        return false;
    }

    int64_t age = timestamp - nginput_updated_at[idx];
    return age >= 0 && age <= late_shift_window_ms;
}

static bool has_shift_match(const NGList *keys) {
    if (keys->size == 0 || keys->size >= 3) {
        return false;
    }
    if (nglist_contains_shift_key(keys)) {
        return false;
    }

    NGList shifted;
    initializeList(&shifted);
    addToList(&shifted, SPACE);
    for (int i = 0; i < keys->size; i++) {
        addToList(&shifted, keys->elements[i]);
    }

    return number_of_matches(&shifted) > 0;
}

static bool kana_output_contains_alpha(const uint32_t kana[6]) {
    for (int i = 0; i < 6; i++) {
        if (kana[i] == NONE) {
            break;
        }
        if (is_alpha_keycode(kana[i])) {
            return true;
        }
    }
    return false;
}

static bool should_preconfirm_before_function(uint32_t shift, uint32_t douji) {
    if (shift != (B_J | B_K)) {
        return false;
    }
    switch (douji) {
    case B_F: // 「
    case B_V: // 」
    case B_S: // 『
    case B_X: // 』
        return false;
    default:
        return true;
    }
}

static void add_shift_key_to_input(uint32_t keycode) {
    bool shift_key = is_shift_keycode(keycode);
    bool combined = false;

    if (nginput.size > 0) {
        NGList last;
        copyList(&(nginput.elements[nginput.size - 1]), &last);
        bool last_has_shift = nglist_contains_shift_key(&last);
        bool allow_combine = !last_has_shift;
        if (!allow_combine && shift_key && last.size == 1 && last.elements[0] != keycode) {
            allow_combine = true;
        }

        if (last.size > 0 && last.size < 3 && allow_combine &&
            within_late_shift_window(nginput.size - 1)) {
            NGList shifted;
            initializeList(&shifted);
            addToList(&shifted, keycode);
            for (int i = 0; i < last.size; i++) {
                addToList(&shifted, last.elements[i]);
            }
            if (number_of_matches(&shifted) > 0 || nglist_contains_dual_shift_keys(&shifted)) {
                nginput_remove_at(nginput.size - 1);
                nginput_add(&shifted);
                combined = true;
            }
        }
    }

    if (!combined) {
        NGList a;
        initializeList(&a);
        addToList(&a, keycode);
        nginput_add(&a);
    }
}

static naginata_backspace_action_t resolve_naginata_backspace_action(void) {
    naginata_backspace_action_t action = {
        .backspace_count = 1,
        .delete_count = 0,
        .from_history = false,
    };

    if (kana_backspace_needs_space_undo && kana_delete_history_size > 0) {
        // 変換解除に必要な1回分と、削除アクションを1回のBSに統合する
        kana_backspace_needs_space_undo = false;
        action.from_history = true;
        kana_delete_action_t history_action = pop_kana_delete_action();
        if (history_action.backspace_count > 0 || history_action.delete_count > 0) {
            uint16_t merged = (uint16_t)history_action.backspace_count + 1U;
            action.backspace_count = (merged > 0xFFU) ? 0xFFU : (uint8_t)merged;
            action.delete_count = history_action.delete_count;
        } else {
            action.backspace_count = 1;
            action.delete_count = 0;
        }
    } else if (kana_backspace_armed && kana_delete_history_size > 0) {
        action.from_history = true;
        kana_delete_action_t history_action = pop_kana_delete_action();
        if (history_action.backspace_count > 0 || history_action.delete_count > 0) {
            action.backspace_count = history_action.backspace_count;
            action.delete_count = history_action.delete_count;
        }
    } else {
        clear_kana_output_history();
    }

    if (kuten_confirm_extra_backspace_pending) {
        uint16_t merged = (uint16_t)action.backspace_count + 1U;
        action.backspace_count = (merged > 0xFFU) ? 0xFFU : (uint8_t)merged;
        kuten_confirm_extra_backspace_pending = false;
    }

    return action;
}

static void emit_naginata_backspace_action(const naginata_backspace_action_t *action) {
    for (int i = 0; i < action->backspace_count; i++) {
        LOG_DBG(" NAGINATA type keycode 0x%02X", BACKSPACE);
        raise_zmk_keycode_state_changed_from_encoded(BACKSPACE, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(BACKSPACE, false, timestamp);
    }
    for (int i = 0; i < action->delete_count; i++) {
        LOG_DBG(" NAGINATA type keycode 0x%02X", DELETE);
        raise_zmk_keycode_state_changed_from_encoded(DELETE, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(DELETE, false, timestamp);
    }
}

static void press_backspace_for_repeat(void) {
    uint64_t bit = bypass_bit(BACKSPACE);
    if (bit) {
        bypass_keys |= bit;
    }
    LOG_DBG(" NAGINATA type keycode 0x%02X", BACKSPACE);
    raise_zmk_keycode_state_changed_from_encoded(BACKSPACE, true, timestamp);
}

// キー入力を文字に変換して出力する
void ng_type(NGList *keys) {
    LOG_DBG(">NAGINATA NG_TYPE");

    if (keys->size == 0)
        return;

    bool backspace_only = keys->size == 1 && keys->elements[0] == BACKSPACE;
    if (!backspace_only) {
        // 句読点+確定の追加BSは「直後の単独BS」のみ有効。
        // 他の薙刀入力(同時押し含む)を処理する時点で必ず無効化する。
        kuten_confirm_extra_backspace_pending = false;
    }

    if (keys->size == 2 && nglist_contains_dual_shift_keys(keys)) {
        LOG_DBG(" NAGINATA type keycode 0x%02X", ENTER);
        raise_zmk_keycode_state_changed_from_encoded(ENTER, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(ENTER, false, timestamp);
        ng_post_enter_maybe_move_right();
        clear_kana_output_history();
        return;
    }

    if (keys->size == 1 && keys->elements[0] == ENTER) {
        LOG_DBG(" NAGINATA type keycode 0x%02X", SPACE);
        raise_zmk_keycode_state_changed_from_encoded(SPACE, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(SPACE, false, timestamp);
        clear_kana_output_history();
        return;
    }
    if (keys->size == 1 && keys->elements[0] == BACKSPACE) {
        naginata_backspace_action_t action = resolve_naginata_backspace_action();
        emit_naginata_backspace_action(&action);
        ime_preedit_pending = false;
        kuten_confirm_extra_backspace_pending = false;
        return;
    }

    uint32_t keyset = 0UL;
    for (int i = 0; i < keys->size; i++) {
        keyset |= ng_keycode_to_bit(keys->elements[i]);
    }

    for (int i = 0; i < sizeof ngdickana / sizeof ngdickana[0]; i++) {
        if ((ngdickana[i].shift | ngdickana[i].douji) == keyset) {
            if (ngdickana[i].kana[0] != NONE) {
                uint8_t out_len = estimate_kana_output_len(ngdickana[i].kana);
                bool kuten_confirm_sequence = kuten_confirm_mode == KUTEN_CONFIRM_SPACE &&
                                              has_kuten_confirm_sequence(ngdickana[i].kana);
                for (int k = 0; k < 6; k++) {
                    if (ngdickana[i].kana[k] == NONE)
                        break;
                    if (ngdickana[i].kana[k] == ENTER && k > 0 &&
                        (ngdickana[i].kana[k - 1] == COMMA || ngdickana[i].kana[k - 1] == DOT)) {
                        if (kuten_confirm_mode == KUTEN_CONFIRM_DISABLED) {
                            continue;
                        }
                        if (kuten_confirm_mode == KUTEN_CONFIRM_SPACE) {
                            LOG_DBG(" NAGINATA type keycode 0x%02X", SPACE);
                            raise_zmk_keycode_state_changed_from_encoded(SPACE, true, timestamp);
                            raise_zmk_keycode_state_changed_from_encoded(SPACE, false, timestamp);
                            continue;
                        }
                    }
                    LOG_DBG(" NAGINATA type keycode 0x%02X", ngdickana[i].kana[k]);
                    raise_zmk_keycode_state_changed_from_encoded(ngdickana[i].kana[k], true,
                                                                 timestamp);
                    raise_zmk_keycode_state_changed_from_encoded(ngdickana[i].kana[k], false,
                                                                 timestamp);
                }
                push_kana_output_len(out_len);
                ime_preedit_pending = kana_output_contains_alpha(ngdickana[i].kana);
                // 句読点+変換の2回消しは「次の1回のBSのみ」有効にする。
                kuten_confirm_extra_backspace_pending = kuten_confirm_sequence;
            } else {
                pending_func_backspace_count = 0;
                pending_func_delete_count = 0;
                if (ime_preedit_pending &&
                    should_preconfirm_before_function(ngdickana[i].shift, ngdickana[i].douji)) {
                    // 「」系以外の編集系機能を実行する前に、先行する未確定入力を確定。
                    LOG_DBG(" NAGINATA pre-confirm pending input");
                    raise_zmk_keycode_state_changed_from_encoded(ENTER, true, timestamp);
                    raise_zmk_keycode_state_changed_from_encoded(ENTER, false, timestamp);
                    ime_preedit_pending = false;
                }
                ngdickana[i].func();
                if (pending_func_backspace_count > 0 || pending_func_delete_count > 0) {
                    clear_kana_output_history();
                    push_kana_delete_action(pending_func_backspace_count,
                                            pending_func_delete_count);
                    pending_func_backspace_count = 0;
                    pending_func_delete_count = 0;
                } else {
                    clear_kana_output_history();
                }
                ime_preedit_pending = false;
                kuten_confirm_extra_backspace_pending = false;
            }
            LOG_DBG("<NAGINATA NG_TYPE");
            return;
        }
    }

    // JIみたいにJIを含む同時押しはたくさんあるが、JIのみの同時押しがないとき
    // 最後の１キーを別に分けて変換する
    NGList a, b;
    initializeList(&a);
    initializeList(&b);
    for (int i = 0; i < keys->size - 1; i++) {
        addToList(&a, keys->elements[i]);
    }
    addToList(&b, keys->elements[keys->size - 1]);
    ng_type(&a);
    ng_type(&b);

    LOG_DBG("<NAGINATA NG_TYPE");
}

static bool emit_jk_function_combo(uint32_t function_keycode) {
    NGList combo;
    initializeList(&combo);
    addToList(&combo, J);
    addToList(&combo, K);
    addToList(&combo, function_keycode);
    if (number_of_matches(&combo) != 1) {
        return false;
    }

    ng_type(&combo);
    consumed_jk_combo_release_keys |= bypass_bit(J) | bypass_bit(K) | bypass_bit(function_keycode);
    remove_pending_bypass_jk_keycode(function_keycode);
    remove_pending_bypass_jk_keycode(K);
    remove_pending_bypass_jk_keycode(J);
    return true;
}

static bool emit_space_enter_combo(bool clear_latched_bypass) {
    tap_keycode(ENTER);
    ng_post_enter_maybe_move_right();
    if (clear_latched_bypass) {
        alpha_backspace_bypass_latched = false;
    }
    consumed_jk_combo_release_keys |= bypass_bit(SPACE) | bypass_bit(ENTER);
    remove_pending_bypass_jk_keycode(ENTER);
    remove_pending_bypass_jk_keycode(SPACE);
    return true;
}

static bool try_handle_bypass_jk_combo_press(uint32_t keycode, bool bypass_active,
                                             bool clear_latched_bypass_on_enter) {
    if (!bypass_active || !is_bypass_jk_combo_candidate_keycode(keycode)) {
        return false;
    }

    // 修飾キーによるバイパス中は、右スペース(ENTER)を通常のEnterとして扱う。
    // SPACE+ENTER同時押し判定は、ラッチ由来バイパス時のみ有効にする。
    if ((keycode == SPACE || keycode == ENTER) && !clear_latched_bypass_on_enter) {
        return false;
    }

    if (!append_pending_bypass_jk_key(keycode)) {
        return false;
    }

    bool has_space = pending_bypass_jk_index(SPACE) >= 0;
    bool has_enter = pending_bypass_jk_index(ENTER) >= 0;
    if (has_space && has_enter) {
        return emit_space_enter_combo(clear_latched_bypass_on_enter);
    }

    bool has_j = pending_bypass_jk_index(J) >= 0;
    bool has_k = pending_bypass_jk_index(K) >= 0;
    if (!has_j || !has_k) {
        return true;
    }

    for (int i = 0; i < pending_bypass_jk_keys_len; i++) {
        uint32_t candidate = pending_bypass_jk_keys[i].keycode;
        if (candidate == J || candidate == K) {
            continue;
        }
        if (is_jk_function_combo_keycode(candidate)) {
            if (emit_jk_function_combo(candidate)) {
                return true;
            }
        }
    }

    return true;
}

// 薙刀式の入力処理
bool naginata_press(struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event event) {
    LOG_DBG(">NAGINATA PRESS");

    uint32_t keycode = binding->param1;

    switch (keycode) {
    case A ... Z:
    case SPACE:
    case BACKSPACE:
    case ENTER:
    case DOT:
    case COMMA:
    case SLASH:
    case SEMI:
    case TAB:
    case LEFT:
    case RIGHT:
    case UP:
    case DOWN:
        if (keycode != BACKSPACE) {
            kuten_confirm_extra_backspace_pending = false;
        }
        zmk_mod_flags_t explicit_mods = zmk_hid_get_explicit_mods();
        zmk_mod_flags_t active_mods = zmk_hid_get_keyboard_report()->body.modifiers;
        bool shift_mods_active = (active_mods & (MOD_LSFT | MOD_RSFT)) != 0;
        if (forced_bypass_from_ng_off_lock && !shift_mods_active) {
            // Fail-safe: ng_off_lock release取りこぼし時の強制バイパス残留を防ぐ
            forced_bypass_from_ng_off_lock = false;
        }
        bool mods_bypass_active =
            explicit_mods || shift_mods_active || forced_bypass_from_ng_off_lock;
        bool is_latched_bypass_key = is_latched_bypass_keycode(keycode);
        bool is_passthrough_only_key = is_navigation_or_tab_keycode(keycode);

        if (alpha_backspace_bypass_latched && !is_latched_bypass_key) {
            alpha_backspace_bypass_latched = false;
            pending_bypass_jk_keys_len = 0;
            consumed_jk_combo_release_keys = 0ULL;
        }

        bool bypass_active_for_key =
            mods_bypass_active || (alpha_backspace_bypass_latched && is_latched_bypass_key);
        bool clear_latched_bypass_on_enter =
            alpha_backspace_bypass_latched && !mods_bypass_active;
        if (try_handle_bypass_jk_combo_press(keycode, bypass_active_for_key,
                                             clear_latched_bypass_on_enter)) {
            return true;
        }

        if (mods_bypass_active || (alpha_backspace_bypass_latched && is_latched_bypass_key)) {
            uint64_t bit = bypass_bit(keycode);
            if (bit) {
                bypass_keys |= bit;
            }
            if (shift_mods_active &&
                (is_alpha_keycode(keycode) || is_symbol_keycode(keycode))) {
                alpha_backspace_bypass_latched = true;
            }
            clear_kana_output_history();
            raise_zmk_keycode_state_changed_from_encoded(keycode, true, timestamp);
            return true;
        }
        if (keycode == BACKSPACE) {
            naginata_backspace_action_t action = resolve_naginata_backspace_action();
            if (action.from_history) {
                // 履歴削除の最後の1回は押下維持に置き換えて、削除数を増やさず長押しリピートへ移行
                naginata_backspace_action_t pre_action = action;
                if (pre_action.backspace_count > 0) {
                    pre_action.backspace_count--;
                }
                emit_naginata_backspace_action(&pre_action);
            }
            press_backspace_for_repeat();
            return true;
        }
        if (is_passthrough_only_key) {
            uint64_t bit = bypass_bit(keycode);
            if (bit) {
                bypass_keys |= bit;
            }
            clear_kana_output_history();
            raise_zmk_keycode_state_changed_from_encoded(keycode, true, timestamp);
            return true;
        }
        if (keycode == SPACE || keycode == ENTER) {
            if (kana_backspace_armed || kana_backspace_needs_space_undo) {
                preserve_history_for_space_undo();
            } else {
                disarm_kana_backspace_history();
            }
        } else if (keycode != BACKSPACE) {
            disarm_kana_backspace_history();
        }
        n_pressed_keys++;
        pressed_keys_add(keycode); // キーの重ね合わせ

        if (keycode == SPACE || keycode == ENTER) {
            add_shift_key_to_input(keycode);
        } else {
            NGList a;
            NGList b;
            if (nginput.size > 0) {
                copyList(&(nginput.elements[nginput.size - 1]), &a);
                copyList(&a, &b);
                addToList(&b, keycode);
            }

            // 前のキーとの同時押しの可能性があるなら前に足す
            // 同じキー連打を除外
            if (nginput.size > 0 && a.elements[a.size - 1] != keycode &&
                number_of_candidates(&b) > 0) {
                nginput_remove_at(nginput.size - 1);
                nginput_add(&b);
                // 前のキーと同時押しはない
            } else {
                // 連続シフトではない
                NGList e;
                initializeList(&e);
                addToList(&e, keycode);
                nginput_add(&e);
            }
        }

        // 連続シフト
        static uint32_t rs[10][2] = {{D, F},     {C, V}, {J, K}, {M, COMMA}, {SPACE, 0},
                                     {ENTER, 0}, {F, 0}, {V, 0}, {J, 0},     {M, 0}};

        uint32_t keyset = 0UL;
        for (int i = 0; i < nginput.elements[0].size; i++) {
            keyset |= ng_keycode_to_bit(nginput.elements[0].elements[i]);
        }
        for (int i = 0; i < 10; i++) {
            NGList rskc;
            initializeList(&rskc);
            addToList(&rskc, rs[i][0]);
            if (rs[i][1] > 0) {
                addToList(&rskc, rs[i][1]);
            }

            int c = includeList(&rskc, keycode);
            uint32_t brs = 0UL;
            for (int j = 0; j < rskc.size; j++) {
                brs |= ng_keycode_to_bit(rskc.elements[j]);
            }

            NGList l = nginput.elements[nginput.size - 1];
            for (int j = 0; j < l.size; j++) {
                addToList(&rskc, l.elements[j]);
            }

            if (c < 0 && ((brs & pressed_keys) == brs) && (keyset & brs) != brs && number_of_matches(&rskc) > 0) {
                nginput.elements[nginput.size - 1] = rskc;
                nginput_updated_at[nginput.size - 1] = timestamp;
                break;
            }
        }

        bool defer_for_space_shift = false;
        if ((pressed_keys & B_SPACE) == 0 && nginput.size > 0) {
            defer_for_space_shift =
                has_shift_match(&(nginput.elements[0])) && within_late_shift_window(0);
        }

        if (!defer_for_space_shift &&
            (nginput.size > 1 || number_of_candidates(&(nginput.elements[0])) == 1)) {
            ng_type(&(nginput.elements[0]));
            nginput_remove_at(0);
        }
        break;
    }

    LOG_DBG("<NAGINATA PRESS");

    return true;
}

bool naginata_release(struct zmk_behavior_binding *binding,
                      struct zmk_behavior_binding_event event) {
    LOG_DBG(">NAGINATA RELEASE");

    uint32_t keycode = binding->param1;

    switch (keycode) {
    case A ... Z:
    case SPACE:
    case BACKSPACE:
    case ENTER:
    case DOT:
    case COMMA:
    case SLASH:
    case SEMI:
    case TAB:
    case LEFT:
    case RIGHT:
    case UP:
    case DOWN:
        {
            uint64_t bit = bypass_bit(keycode);
            if (bit && (consumed_jk_combo_release_keys & bit)) {
                consumed_jk_combo_release_keys &= ~bit;
                return true;
            }
        }
        if (is_bypass_jk_combo_candidate_keycode(keycode)) {
            int idx = pending_bypass_jk_index(keycode);
            if (idx >= 0) {
                pending_bypass_jk_keys[idx].released = true;
                flush_released_pending_bypass_jk_keys();
                return true;
            }
        }
        {
            uint64_t bit = bypass_bit(keycode);
            if (bit && (bypass_keys & bit)) {
                bypass_keys &= ~bit;
                raise_zmk_keycode_state_changed_from_encoded(keycode, false, timestamp);
                return true;
            }
        }
        if (is_navigation_or_tab_keycode(keycode)) {
            return true;
        }
        if (n_pressed_keys > 0)
            n_pressed_keys--;
        if (n_pressed_keys == 0) {
            reset_pressed_keys_state();
        } else {
            pressed_keys_remove(keycode); // キーの重ね合わせ
        }

        if (pressed_keys == 0UL) {
            while (nginput.size > 0) {
                ng_type(&(nginput.elements[0]));
                nginput_remove_at(0);
            }
        } else {
            if (nginput.size > 0 && number_of_candidates(&(nginput.elements[0])) == 1) {
                ng_type(&(nginput.elements[0]));
                nginput_remove_at(0);
            }
        }
        break;
    }

    LOG_DBG("<NAGINATA RELEASE");

    return true;
}

// 薙刀式

static int behavior_naginata_init(const struct device *dev) {
    LOG_DBG("NAGINATA INIT");
    const struct behavior_naginata_config *cfg = dev->config;

    initializeListArray(&nginput);
    clear_nginput_timestamps();
    clear_kana_output_history();
    reset_pressed_keys_state();
    n_pressed_keys = 0;
    bypass_keys = 0ULL;
    clear_bypass_mode_state();
    naginata_config.os =  NG_WINDOWS;
    late_shift_window_ms = cfg->late_shift_window_ms;
    switch (cfg->kuten_confirm_enter) {
    case KUTEN_CONFIRM_DISABLED:
    case KUTEN_CONFIRM_ENTER:
    case KUTEN_CONFIRM_SPACE:
        kuten_confirm_mode = (uint8_t)cfg->kuten_confirm_enter;
        break;
    default:
        kuten_confirm_mode = KUTEN_CONFIRM_ENTER;
        break;
    }

    return 0;
};

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    LOG_DBG("position %d keycode 0x%02X", event.position, binding->param1);

    // F15が押されたらnaginata_config.os=NG_WINDOWS
    switch (binding->param1) {
        case F15:
            naginata_config.os = NG_WINDOWS;
            return ZMK_BEHAVIOR_OPAQUE;
        case F16:
            naginata_config.os = NG_MACOS;
            return ZMK_BEHAVIOR_OPAQUE;
        case F17:
            naginata_config.os = NG_LINUX;
            return ZMK_BEHAVIOR_OPAQUE;
        case F18:
            naginata_config.tategaki = true;
            return ZMK_BEHAVIOR_OPAQUE;
        case F19:
            naginata_config.tategaki = false;
            return ZMK_BEHAVIOR_OPAQUE;
    }

    timestamp = event.timestamp;
    naginata_press(binding, event);

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    LOG_DBG("position %d keycode 0x%02X", event.position, binding->param1);

    timestamp = event.timestamp;
    naginata_release(binding, event);

    return ZMK_BEHAVIOR_OPAQUE;
}

static int naginata_bypass_reset_listener(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (should_clear_kuten_confirm_pending_on_keycode_event(ev)) {
        kuten_confirm_extra_backspace_pending = false;
    }
    if (should_reset_bypass_on_keycode_event(ev)) {
        clear_bypass_mode_state();
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(behavior_naginata_bypass_reset, naginata_bypass_reset_listener);
ZMK_SUBSCRIPTION(behavior_naginata_bypass_reset, zmk_keycode_state_changed);

static const struct behavior_driver_api behavior_naginata_driver_api = {
    .binding_pressed = on_keymap_binding_pressed, .binding_released = on_keymap_binding_released};

#define KP_INST(n)                                                                                 \
    static const struct behavior_naginata_config behavior_naginata_config_##n = {                  \
        .late_shift_window_ms = DT_INST_PROP(n, late_shift_window_ms),                             \
        .kuten_confirm_enter = DT_INST_PROP(n, kuten_confirm_enter),                               \
    };                                                                                              \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_naginata_init, NULL, NULL,                                 \
                            &behavior_naginata_config_##n, POST_KERNEL,                            \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_naginata_driver_api);

DT_INST_FOREACH_STATUS_OKAY(KP_INST)
