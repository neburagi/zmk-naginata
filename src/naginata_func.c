
#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/behavior.h>
#include <zmk/behavior_queue.h>
#include <zmk_naginata/naginata_func.h>

int64_t timestamp;

#define NG_WINDOWS (uint8_t)0
#define NG_MACOS (uint8_t)1
#define NG_LINUX (uint8_t)2
#define NG_IOS (uint8_t)3
#define NG_ANDROID (uint8_t)4

typedef union
{
    uint8_t os : 3;
} user_config_t;

user_config_t naginata_config;

// 薙刀式をオン
void naginata_on(void)
{
    raise_zmk_keycode_state_changed_from_encoded(LANG1, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(LANG1, false, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(INT4, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(INT4, false, timestamp);
}

// 薙刀式をオフ
// void naginata_off(void) {
//     raise_zmk_keycode_state_changed_from_encoded(LANG2, true, timestamp);
//     raise_zmk_keycode_state_changed_from_encoded(LANG2, false, timestamp);
//     raise_zmk_keycode_state_changed_from_encoded(INT5, true, timestamp);
//     raise_zmk_keycode_state_changed_from_encoded(INT5, false, timestamp);
// }

void nofunc() {}

// Pending cursor-right moves to consume after ENTER.
// Using a counter allows nested paired symbols to be exited one layer at a time.
static uint8_t move_right_after_next_enter_count = 0;

void ng_schedule_move_right_after_next_enter(void)
{
    if (move_right_after_next_enter_count < 0xFF) {
        move_right_after_next_enter_count++;
    }
}

void ng_post_enter_maybe_move_right(void)
{
    if (move_right_after_next_enter_count == 0) {
        return;
    }
    uint8_t pending = move_right_after_next_enter_count;
    move_right_after_next_enter_count = 0;
    for (uint8_t i = 0; i < pending; i++) {
        raise_zmk_keycode_state_changed_from_encoded(RIGHT, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(RIGHT, false, timestamp);
    }
}

static void input_windows_hex_keycode(int keycode)
{
    switch (keycode) {
    case N0:
        raise_zmk_keycode_state_changed_from_encoded(KP_N0, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(KP_N0, false, timestamp);
        break;
    case N1:
        raise_zmk_keycode_state_changed_from_encoded(KP_N1, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(KP_N1, false, timestamp);
        break;
    case N2:
        raise_zmk_keycode_state_changed_from_encoded(KP_N2, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(KP_N2, false, timestamp);
        break;
    case N3:
        raise_zmk_keycode_state_changed_from_encoded(KP_N3, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(KP_N3, false, timestamp);
        break;
    case N4:
        raise_zmk_keycode_state_changed_from_encoded(KP_N4, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(KP_N4, false, timestamp);
        break;
    case N5:
        raise_zmk_keycode_state_changed_from_encoded(KP_N5, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(KP_N5, false, timestamp);
        break;
    case N6:
        raise_zmk_keycode_state_changed_from_encoded(KP_N6, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(KP_N6, false, timestamp);
        break;
    case N7:
        raise_zmk_keycode_state_changed_from_encoded(KP_N7, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(KP_N7, false, timestamp);
        break;
    case N8:
        raise_zmk_keycode_state_changed_from_encoded(KP_N8, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(KP_N8, false, timestamp);
        break;
    case N9:
        raise_zmk_keycode_state_changed_from_encoded(KP_N9, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(KP_N9, false, timestamp);
        break;
    default:
        raise_zmk_keycode_state_changed_from_encoded(keycode, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(keycode, false, timestamp);
        break;
    }
}

void switch_to_hex_input()
{
    switch (naginata_config.os)
    {
    case NG_MACOS:
        raise_zmk_keycode_state_changed_from_encoded(LANG2, true, timestamp); // 未確定文字を確定する
        raise_zmk_keycode_state_changed_from_encoded(LANG2, false, timestamp);
        k_sleep(K_MSEC(10));
        raise_zmk_keycode_state_changed_from_encoded(LC(F20), true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LC(F20), false, timestamp);
        k_sleep(K_MSEC(50));
        return;
    case NG_WINDOWS:
        return;
    case NG_LINUX:
        return;
    case NG_IOS:
    case NG_ANDROID:
    }
}

void return_to_kana_input()
{
    switch (naginata_config.os)
    {
    case NG_MACOS:
        raise_zmk_keycode_state_changed_from_encoded(LS(LANG1), true, timestamp); // 未確定文字を確定する
        raise_zmk_keycode_state_changed_from_encoded(LS(LANG1), false, timestamp);
        k_sleep(K_MSEC(10));
        raise_zmk_keycode_state_changed_from_encoded(LANG1, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LANG1, false, timestamp);
        return;
    case NG_WINDOWS:
    case NG_LINUX:
    case NG_IOS:
    case NG_ANDROID:
    }
}

void press_compose_key()
{
    switch (naginata_config.os)
    {
    case NG_MACOS:
        raise_zmk_keycode_state_changed_from_encoded(LEFT_ALT, true, timestamp);
        k_sleep(K_MSEC(50));
        return;
    case NG_WINDOWS:
        // Windows Unicode Hex Input: hold Alt, tap keypad '+', type hex, then release Alt.
        // Avoid an isolated Alt-only event, which can trigger app UI accelerators.
        raise_zmk_keycode_state_changed_from_encoded(RIGHT_ALT, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(KP_PLUS, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(KP_PLUS, false, timestamp);
        k_sleep(K_MSEC(10));
        return;
    case NG_LINUX:
        raise_zmk_keycode_state_changed_from_encoded(LC(LS(U)), true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LC(LS(U)), false, timestamp);
        k_sleep(K_MSEC(50));
        return;
    case NG_IOS:
    case NG_ANDROID:
    }
}

void release_compose_key()
{
    switch (naginata_config.os)
    {
    case NG_MACOS:
        raise_zmk_keycode_state_changed_from_encoded(LEFT_ALT, false, timestamp);
        k_sleep(K_MSEC(50));
        return;
    case NG_WINDOWS:
        raise_zmk_keycode_state_changed_from_encoded(RIGHT_ALT, false, timestamp);
        return;
    case NG_LINUX:
        raise_zmk_keycode_state_changed_from_encoded(LC(LS(U)), true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LC(LS(U)), false, timestamp);
        k_sleep(K_MSEC(50));
        return;
    case NG_IOS:
    case NG_ANDROID:
    }
}

void input_unicode_hex(int n1, int n2, int n3, int n4)
{
    switch (naginata_config.os)
    {
    case NG_MACOS:
        switch_to_hex_input();
        press_compose_key();
        raise_zmk_keycode_state_changed_from_encoded(n1, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(n1, false, timestamp);
        k_sleep(K_MSEC(10));
        raise_zmk_keycode_state_changed_from_encoded(n2, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(n2, false, timestamp);
        k_sleep(K_MSEC(10));
        raise_zmk_keycode_state_changed_from_encoded(n3, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(n3, false, timestamp);
        k_sleep(K_MSEC(10));
        raise_zmk_keycode_state_changed_from_encoded(n4, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(n4, false, timestamp);
        k_sleep(K_MSEC(10));
        release_compose_key();
        return_to_kana_input();
        return;
    case NG_WINDOWS:
        press_compose_key();
        input_windows_hex_keycode(n1);
        k_sleep(K_MSEC(10));
        input_windows_hex_keycode(n2);
        k_sleep(K_MSEC(10));
        input_windows_hex_keycode(n3);
        k_sleep(K_MSEC(10));
        input_windows_hex_keycode(n4);
        k_sleep(K_MSEC(10));
        release_compose_key();
        return_to_kana_input();
        return;
    case NG_LINUX:
        press_compose_key();
        raise_zmk_keycode_state_changed_from_encoded(n1, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(n1, false, timestamp);
        k_sleep(K_MSEC(10));
        raise_zmk_keycode_state_changed_from_encoded(n2, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(n2, false, timestamp);
        k_sleep(K_MSEC(10));
        raise_zmk_keycode_state_changed_from_encoded(n3, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(n3, false, timestamp);
        k_sleep(K_MSEC(10));
        raise_zmk_keycode_state_changed_from_encoded(n4, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(n4, false, timestamp);
        k_sleep(K_MSEC(10));
        release_compose_key();
        raise_zmk_keycode_state_changed_from_encoded(ENTER, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(ENTER, false, timestamp);
        return_to_kana_input();
        return;
    case NG_ANDROID:
        return;
    }
}

void ng_T() { ng_left(1); }

void ng_Y() { ng_right(1); }

void ng_ST()
{
    raise_zmk_keycode_state_changed_from_encoded(LSHIFT, true, timestamp);
    ng_left(1);
    raise_zmk_keycode_state_changed_from_encoded(LSHIFT, false, timestamp);
}

void ng_SY()
{
    raise_zmk_keycode_state_changed_from_encoded(LSHIFT, true, timestamp);
    ng_right(1);
    raise_zmk_keycode_state_changed_from_encoded(LSHIFT, false, timestamp);
}
void ngh_JKQ()
{ // ^{End}
    ng_eof();
}

void ngh_JKW()
{ // ／{改行}
    if (naginata_config.os == NG_WINDOWS || naginata_config.os == NG_ANDROID) {
        input_unicode_hex(N3, N0, N0, E); // 『
        input_unicode_hex(N3, N0, N0, F); // 』
        ng_schedule_move_right_after_next_enter();
        if (naginata_config.os != NG_ANDROID) {
            ng_prev_char();
        }
        ng_set_func_backspace_action(1, 1);
        return;
    }
    input_unicode_hex(F, F, N0, F);
}

void ngh_JKE()
{ // /*ディ*/
    raise_zmk_keycode_state_changed_from_encoded(D, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(D, false, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(H, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(H, false, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(I, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(I, false, timestamp);
}

void ngh_JKR()
{ // ^s
    ng_save();
}

void ngh_JKT()
{ // ・
    raise_zmk_keycode_state_changed_from_encoded(SLASH, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SLASH, false, timestamp);
}

void ngh_JKA()
{ // ……{改行}
    input_unicode_hex(N2, N0, N2, N6);
    input_unicode_hex(N2, N0, N2, N6);
}

void ngh_JKS()
{ // 『{改行}
    if (naginata_config.os == NG_WINDOWS || naginata_config.os == NG_ANDROID) {
        // 通常キーで（）を入力
        raise_zmk_keycode_state_changed_from_encoded(LS(N9), true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LS(N9), false, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LS(N0), true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LS(N0), false, timestamp);
        // IMEの未確定を確定
        raise_zmk_keycode_state_changed_from_encoded(ENTER, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(ENTER, false, timestamp);
        ng_schedule_move_right_after_next_enter();
        if (naginata_config.os != NG_ANDROID) {
            ng_prev_char();
        }
        ng_set_func_backspace_action(1, 1);
        return;
    }
    input_unicode_hex(N3, N0, N0, E);
}

void ngh_JKD()
{ // ？{改行}
    raise_zmk_keycode_state_changed_from_encoded(LS(SLASH), true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(LS(SLASH), false, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(ENTER, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(ENTER, false, timestamp);
}

void ngh_JKF()
{ // 「{改行}
    if (naginata_config.os == NG_WINDOWS || naginata_config.os == NG_ANDROID) {
        // 通常キーで「」を入力
        raise_zmk_keycode_state_changed_from_encoded(LBKT, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LBKT, false, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(RBKT, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(RBKT, false, timestamp);
        // IMEの未確定を確定
        raise_zmk_keycode_state_changed_from_encoded(ENTER, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(ENTER, false, timestamp);
        ng_schedule_move_right_after_next_enter();
        if (naginata_config.os != NG_ANDROID) {
            ng_prev_char();
        }
        ng_set_func_backspace_action(1, 1);
        return;
    }
    input_unicode_hex(N3, N0, N0, C);
}

void ngh_JKG()
{ // ({改行}
    if (naginata_config.os == NG_WINDOWS || naginata_config.os == NG_ANDROID) {
        input_unicode_hex(N3, N0, N0, A); // 《
        input_unicode_hex(N3, N0, N0, B); // 》
        ng_schedule_move_right_after_next_enter();
        if (naginata_config.os != NG_ANDROID) {
            ng_prev_char();
        }
        ng_set_func_backspace_action(1, 1);
        return;
    }
    input_unicode_hex(F, F, N0, N8);
}

void ngh_JKZ()
{ // ――{改行}
    input_unicode_hex(N2, N0, N1, N5);
    input_unicode_hex(N2, N0, N1, N5);
}

void ngh_JKX()
{ // 』{改行}
    if (naginata_config.os == NG_WINDOWS || naginata_config.os == NG_ANDROID) {
        input_unicode_hex(N3, N0, N1, N0); // 【
        input_unicode_hex(N3, N0, N1, N1); // 】
        ng_schedule_move_right_after_next_enter();
        if (naginata_config.os != NG_ANDROID) {
            ng_prev_char();
        }
        ng_set_func_backspace_action(1, 1);
        return;
    }
    input_unicode_hex(N3, N0, N0, F);
}

void ngh_JKC()
{ // ！{改行}
    raise_zmk_keycode_state_changed_from_encoded(LS(N1), true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(LS(N1), false, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(ENTER, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(ENTER, false, timestamp);
}

void ngh_JKV()
{ // 」{改行}
    input_unicode_hex(N3, N0, N0, D);
}

void ngh_JKB()
{ // ){改行}
    input_unicode_hex(F, F, N0, N9);
}

void ngh_DFY()
{ // {Home}
    ng_home();
}

void ngh_DFU()
{ // +{End}{BS}
    raise_zmk_keycode_state_changed_from_encoded(LSHIFT, true, timestamp);
    ng_end();
    raise_zmk_keycode_state_changed_from_encoded(LSHIFT, false, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(BSPC, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(BSPC, false, timestamp);
}

void ngh_DFI()
{ // {vk1Csc079}
    ng_saihenkan();
}

void ngh_DFO()
{ // {Del}
    raise_zmk_keycode_state_changed_from_encoded(DELETE, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(DELETE, false, timestamp);
}

void ngh_DFP()
{ // +{Esc 2}
    raise_zmk_keycode_state_changed_from_encoded(LSHIFT, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(ESC, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(ESC, false, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(ESC, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(ESC, false, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(LSHIFT, false, timestamp);
}

void ngh_DFH()
{ // {Enter}{End}
    raise_zmk_keycode_state_changed_from_encoded(ENTER, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(ENTER, false, timestamp);
    ng_end();
}

void ngh_DFJ()
{ // {↑}
    ng_up(1);
}

void ngh_DFK()
{ // +{↑}
    raise_zmk_keycode_state_changed_from_encoded(LSHIFT, true, timestamp);
    ng_up(1);
    raise_zmk_keycode_state_changed_from_encoded(LSHIFT, false, timestamp);
}

void ngh_DFL()
{ // +{↑ 7}
    raise_zmk_keycode_state_changed_from_encoded(LSHIFT, true, timestamp);
    ng_up(7);
    raise_zmk_keycode_state_changed_from_encoded(LSHIFT, false, timestamp);
}

void ngh_DFSCLN()
{ // ^i
    ng_katakana();
}

void ngh_DFN()
{ // {End}
    ng_end();
}

void ngh_DFM()
{ // {↓}
    ng_down(1);
}

void ngh_DFCOMM()
{ // +{↓}
    raise_zmk_keycode_state_changed_from_encoded(LSHIFT, true, timestamp);
    ng_down(1);
    raise_zmk_keycode_state_changed_from_encoded(LSHIFT, false, timestamp);
}

void ngh_DFDOT()
{ // +{↓ 7}
    raise_zmk_keycode_state_changed_from_encoded(LSHIFT, true, timestamp);
    ng_down(7);
    raise_zmk_keycode_state_changed_from_encoded(LSHIFT, false, timestamp);
}

void ngh_DFSLSH()
{ // ^u
    ng_hiragana();
}

void ngh_MCQ()
{ // ｜{改行}
    input_unicode_hex(F, F, N5, C);
}

void ngh_MCW()
{ // 　　　×　　　×　　　×{改行 2}
    raise_zmk_keycode_state_changed_from_encoded(SPACE, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, false, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, false, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, false, timestamp);
    input_unicode_hex(N0, N0, D, N7);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, false, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, false, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, false, timestamp);
    input_unicode_hex(N0, N0, D, N7);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, false, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, false, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, false, timestamp);
    input_unicode_hex(N0, N0, D, N7);
    raise_zmk_keycode_state_changed_from_encoded(ENTER, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(ENTER, false, timestamp);
}

void ngh_MCE()
{ // {Home}{→}{End}{Del 2}{←}
    ng_home();
    ng_prev_row();
    ng_end();
    raise_zmk_keycode_state_changed_from_encoded(DELETE, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(DELETE, false, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(DELETE, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(DELETE, false, timestamp);
    ng_next_row();
}

void ngh_MCR()
{ // {Home}{改行}{Space 1}{←}
    ng_home();
    raise_zmk_keycode_state_changed_from_encoded(ENTER, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(ENTER, false, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, false, timestamp);
    ng_next_row();
}

void ngh_MCT()
{ // 〇{改行}
    input_unicode_hex(N3, N0, N0, N7);
}

void ngh_MCA()
{ // 《{改行}
    input_unicode_hex(N3, N0, N0, A);
}

void ngh_MCS()
{ // 【{改行}
    input_unicode_hex(N3, N0, N1, N0);
}

void ngh_MCD()
{ // {Home}{→}{End}{Del 4}{←}
    ng_home();
    ng_prev_row();
    ng_end();
    raise_zmk_keycode_state_changed_from_encoded(DELETE, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(DELETE, false, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(DELETE, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(DELETE, false, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(DELETE, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(DELETE, false, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(DELETE, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(DELETE, false, timestamp);
    ng_next_row();
}

void ngh_MCF()
{ // {Home}{改行}{Space 3}{←}
    ng_home();
    raise_zmk_keycode_state_changed_from_encoded(ENTER, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(ENTER, false, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, false, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, false, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, false, timestamp);
    ng_next_row();
}

void ngh_MCG()
{ // {Space 3}
    raise_zmk_keycode_state_changed_from_encoded(SPACE, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, false, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, false, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, false, timestamp);
}

void ngh_MCZ()
{ // 》{改行}
    input_unicode_hex(N3, N0, N0, B);
}

void ngh_MCX()
{ // 】{改行}
    input_unicode_hex(N3, N0, N1, N1);
}

void ngh_MCC()
{ // 」{改行}{改行}
    input_unicode_hex(N3, N0, N0, D);
    raise_zmk_keycode_state_changed_from_encoded(ENTER, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(ENTER, false, timestamp);
}

void ngh_MCV()
{ // 」{改行}{改行}「{改行}
    input_unicode_hex(N3, N0, N0, D);
    raise_zmk_keycode_state_changed_from_encoded(ENTER, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(ENTER, false, timestamp);
    input_unicode_hex(N3, N0, N0, C);
}

void ngh_MCB()
{ // 」{改行}{改行}{Space}
    input_unicode_hex(N3, N0, N0, D);
    raise_zmk_keycode_state_changed_from_encoded(ENTER, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(ENTER, false, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, false, timestamp);
}

void ngh_CVY()
{ // +{Home}
    raise_zmk_keycode_state_changed_from_encoded(LSHIFT, true, timestamp);
    ng_home();
    raise_zmk_keycode_state_changed_from_encoded(LSHIFT, false, timestamp);
}

void ngh_CVU()
{ // ^x
    ng_cut();
}

void ngh_CVI()
{ // {vk1Csc079}
    ng_saihenkan();
}

void ngh_CVO()
{ // ^v
    ng_paste();
}

void ngh_CVP()
{ // ^z
    ng_undo();
}

void ngh_CVH()
{ // ^c
    ng_copy();
}

void ngh_CVJ()
{ // {←}
    ng_left(1);
}

void ngh_CVK()
{ // {→}
    ng_right(1);
}

void ngh_CVL()
{ // {改行}{Space}+{Home}^x{BS}
    raise_zmk_keycode_state_changed_from_encoded(ENTER, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(ENTER, false, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(SPACE, false, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(LSHIFT, true, timestamp);
    ng_home();
    raise_zmk_keycode_state_changed_from_encoded(LSHIFT, false, timestamp);
    ng_cut();
    raise_zmk_keycode_state_changed_from_encoded(BSPC, true, timestamp);
    raise_zmk_keycode_state_changed_from_encoded(BSPC, false, timestamp);
}

void ngh_CVSCLN()
{ // ^y
    ng_redo();
}

void ngh_CVN()
{ // +{End}
    raise_zmk_keycode_state_changed_from_encoded(LSHIFT, true, timestamp);
    ng_end();
    raise_zmk_keycode_state_changed_from_encoded(LSHIFT, false, timestamp);
}

void ngh_CVM()
{ // +{←}
    raise_zmk_keycode_state_changed_from_encoded(LSHIFT, true, timestamp);
    ng_left(1);
    raise_zmk_keycode_state_changed_from_encoded(LSHIFT, false, timestamp);
}

void ngh_CVCOMM()
{ // +{→}
    raise_zmk_keycode_state_changed_from_encoded(LSHIFT, true, timestamp);
    ng_right(1);
    raise_zmk_keycode_state_changed_from_encoded(LSHIFT, false, timestamp);
}

void ngh_CVDOT()
{ // +{← 7}
    raise_zmk_keycode_state_changed_from_encoded(LSHIFT, true, timestamp);
    ng_left(7);
    raise_zmk_keycode_state_changed_from_encoded(LSHIFT, false, timestamp);
}

void ngh_CVSLSH()
{ // +{→ 7}
    raise_zmk_keycode_state_changed_from_encoded(LSHIFT, true, timestamp);
    ng_right(7);
    raise_zmk_keycode_state_changed_from_encoded(LSHIFT, false, timestamp);
}

void ng_cut()
{
    switch (naginata_config.os)
    {
    case NG_WINDOWS:
    case NG_LINUX:
    case NG_ANDROID:
        raise_zmk_keycode_state_changed_from_encoded(LC(X), true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LC(X), false, timestamp);
        break;
    case NG_MACOS:
        raise_zmk_keycode_state_changed_from_encoded(LG(X), true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LG(X), false, timestamp);
        break;
    }
}

void ng_copy()
{
    switch (naginata_config.os)
    {
    case NG_WINDOWS:
    case NG_LINUX:
    case NG_ANDROID:
        raise_zmk_keycode_state_changed_from_encoded(LC(C), true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LC(C), false, timestamp);
        break;
    case NG_MACOS:
        raise_zmk_keycode_state_changed_from_encoded(LG(C), true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LG(C), false, timestamp);
        break;
    }
}

void ng_paste()
{
    switch (naginata_config.os)
    {
    case NG_WINDOWS:
    case NG_LINUX:
    case NG_ANDROID:
        raise_zmk_keycode_state_changed_from_encoded(LC(V), true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LC(V), false, timestamp);
        break;
    case NG_MACOS:
        raise_zmk_keycode_state_changed_from_encoded(LG(V), true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LG(V), false, timestamp);
        break;
    }
}

void ng_up(uint8_t c)
{
    for (uint8_t i = 0; i < c; i++)
    {
        raise_zmk_keycode_state_changed_from_encoded(UP, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(UP, false, timestamp);
    }
}

void ng_down(uint8_t c)
{
    for (uint8_t i = 0; i < c; i++)
    {
        raise_zmk_keycode_state_changed_from_encoded(DOWN, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(DOWN, false, timestamp);
    }
}

void ng_left(uint8_t c)
{
    for (uint8_t i = 0; i < c; i++)
    {
        raise_zmk_keycode_state_changed_from_encoded(LEFT, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LEFT, false, timestamp);
    }
}

void ng_right(uint8_t c)
{
    for (uint8_t i = 0; i < c; i++)
    {
        raise_zmk_keycode_state_changed_from_encoded(RIGHT, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(RIGHT, false, timestamp);
    }
}

void ng_next_row()
{
    switch (naginata_config.os)
    {
    case NG_WINDOWS:
    case NG_LINUX:
    case NG_ANDROID:
        ng_down(1);
        break;
    case NG_MACOS:
        raise_zmk_keycode_state_changed_from_encoded(LC(N), true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LC(N), false, timestamp);
        break;
    }
}

void ng_prev_row()
{
    switch (naginata_config.os)
    {
    case NG_WINDOWS:
    case NG_LINUX:
    case NG_ANDROID:
        ng_up(1);
        break;
    case NG_MACOS:
        raise_zmk_keycode_state_changed_from_encoded(LC(P), true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LC(P), false, timestamp);
        break;
    }
}

void ng_next_char()
{
    switch (naginata_config.os)
    {
    case NG_WINDOWS:
    case NG_LINUX:
    case NG_ANDROID:
        ng_right(1);
        break;
    case NG_MACOS:
        raise_zmk_keycode_state_changed_from_encoded(LC(F), true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LC(F), false, timestamp);
        break;
    }
}

void ng_prev_char()
{
    switch (naginata_config.os)
    {
    case NG_WINDOWS:
    case NG_LINUX:
    case NG_ANDROID:
        ng_left(1);
        break;
    case NG_MACOS:
        raise_zmk_keycode_state_changed_from_encoded(LC(B), true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LC(B), false, timestamp);
        break;
    }
}

void ng_home()
{
    switch (naginata_config.os)
    {
    case NG_WINDOWS:
    case NG_LINUX:
    case NG_ANDROID:
        raise_zmk_keycode_state_changed_from_encoded(HOME, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(HOME, false, timestamp);
        break;
    case NG_MACOS:
        raise_zmk_keycode_state_changed_from_encoded(LC(A), true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LC(A), false, timestamp);
        break;
    }
}

void ng_end()
{
    switch (naginata_config.os)
    {
    case NG_WINDOWS:
    case NG_LINUX:
    case NG_ANDROID:
        raise_zmk_keycode_state_changed_from_encoded(END, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(END, false, timestamp);
        break;
    case NG_MACOS:
        raise_zmk_keycode_state_changed_from_encoded(LC(E), true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LC(E), false, timestamp);
        break;
    }
}

void ng_katakana()
{
    switch (naginata_config.os)
    {
    case NG_WINDOWS:
    case NG_LINUX:
    case NG_ANDROID:
        raise_zmk_keycode_state_changed_from_encoded(LC(I), true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LC(I), false, timestamp);
        break;
    case NG_MACOS:
        raise_zmk_keycode_state_changed_from_encoded(LC(K), true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LC(K), false, timestamp);
        break;
    }
}

void ng_save()
{
    switch (naginata_config.os)
    {
    case NG_WINDOWS:
    case NG_LINUX:
    case NG_ANDROID:
        raise_zmk_keycode_state_changed_from_encoded(LC(S), true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LC(S), false, timestamp);
        break;
    case NG_MACOS:
        raise_zmk_keycode_state_changed_from_encoded(LG(S), true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LG(S), false, timestamp);
        break;
    }
}

void ng_hiragana()
{
    switch (naginata_config.os)
    {
    case NG_WINDOWS:
    case NG_LINUX:
    case NG_ANDROID:
        raise_zmk_keycode_state_changed_from_encoded(LC(U), true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LC(U), false, timestamp);
        break;
    case NG_MACOS:
        raise_zmk_keycode_state_changed_from_encoded(LC(J), true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LC(J), false, timestamp);
        break;
    }
}

void ng_redo()
{
    switch (naginata_config.os)
    {
    case NG_WINDOWS:
    case NG_LINUX:
    case NG_ANDROID:
        raise_zmk_keycode_state_changed_from_encoded(LC(Y), true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LC(Y), false, timestamp);
        break;
    case NG_MACOS:
        raise_zmk_keycode_state_changed_from_encoded(LS(LG((S))), true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LS(LG((S))), false, timestamp);
        break;
    }
}

void ng_undo()
{
    switch (naginata_config.os)
    {
    case NG_WINDOWS:
    case NG_LINUX:
    case NG_ANDROID:
        raise_zmk_keycode_state_changed_from_encoded(LC(Z), true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LC(Z), false, timestamp);
        break;
    case NG_MACOS:
        raise_zmk_keycode_state_changed_from_encoded(LG(Z), true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LG(Z), false, timestamp);
        break;
    }
}

void ng_saihenkan()
{
    switch (naginata_config.os)
    {
    case NG_WINDOWS:
    case NG_LINUX:
    case NG_ANDROID:
        raise_zmk_keycode_state_changed_from_encoded(INT4, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(INT4, false, timestamp);
        break;
    case NG_MACOS:
        raise_zmk_keycode_state_changed_from_encoded(LANG1, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LANG1, false, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LANG1, true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LANG1, false, timestamp);
        break;
    }
}

void ng_eof()
{
    switch (naginata_config.os)
    {
    case NG_WINDOWS:
    case NG_LINUX:
    case NG_ANDROID:
        raise_zmk_keycode_state_changed_from_encoded(LC(END), true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LC(END), false, timestamp);
        break;
    case NG_MACOS:
        raise_zmk_keycode_state_changed_from_encoded(LG(DOWN), true, timestamp);
        raise_zmk_keycode_state_changed_from_encoded(LG(DOWN), false, timestamp);
        break;
    }
}
