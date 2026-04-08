#ifndef MAIN_H
#define MAIN_H

#include <M5Unified.h>

/* ==========================================================
   ゲームの画面状態管理
   どの画面（メニュー・インベーダー・スパイク）にいるかを表す。
   Main.ino で currentState として管理し、各ゲームからも参照する。
   ========================================================== */
enum SystemState {
    STATE_MENU,     // メインメニュー
    STATE_INVADER,  // インベーダーゲーム
    STATE_SPIKE     // スパイクゲーム
};

/* ==========================================================
   LEDエフェクトの種別
   triggerLEDEffect() に渡す値の意味を明確にするための列挙型。

   ========================================================== */
enum LEDEffect {
    EFFECT_NONE       = 0,  // エフェクトなし
    EFFECT_SHOOT      = 1,  // 弾発射時（白く一瞬光る）
    EFFECT_ENEMY_HIT  = 2,  // 敵撃墜時（緑にチカチカ）
    EFFECT_PLAYER_HIT = 3   // 自機被弾時（赤にチカチカ）
};

/* ==========================================================
   Main.ino で定義されている共通変数（各ゲームから extern で参照）
   ========================================================== */
extern SystemState currentState; // 現在の画面状態
extern int score;                // プレイヤーのスコア
extern int lives;                // 残機数
extern bool isGameOver;          // ゲームオーバーフラグ
extern bool isMuted;             // ミュートフラグ
extern const int BAR_HEIGHT;     // 上部バーの高さ（レイアウト定数）

/* SPIKEが使う音声バッファ */
extern uint8_t* wav_spike_bgm;
extern size_t   size_spike_bgm;

/* ==========================================================
   Main.ino で定義されている共通関数（各ゲームから extern で参照）
   ========================================================== */

/* UI */
extern void drawTopBar(const char* leftText);
extern void drawMenu();
extern void drawLoadingScreen(const char* message);

/* 音声操作 */
extern void toggleMute();
extern void takeScreenshot();

/* SE（効果音） */
extern void playShootSound();
extern void playHitSound();
extern void playEnemyMoveSound();
extern void playEnemyShootSound();
extern void playSpikeHitSound();
extern void playGameOverSound();
extern void playDecisionSound();
extern void playTapSound();

/* LED制御 */
extern void triggerLEDEffect(int effect);  // LEDEffect の値を渡す
extern void updateGameLEDs();
extern void showHeightLED(float current, float max_val);

/* ==========================================================
   Invader.cpp で定義され Main.ino でも参照する変数
   ========================================================== */
extern int currentStage; // 現在のステージ数

#endif // MAIN_H
