#ifndef SPIKE_H
#define SPIKE_H

#include <M5Unified.h>
#include "Invader.h" // 共通変数（scoreなど）を利用するためにインクルード

/* メインファイルで定義されている音声変数（スパイク専用）の参照 */
extern uint8_t* wav_spike_bgm;
extern size_t size_spike_bgm;
extern bool isMuted;

/* メインファイルで定義されている関数の宣言（スパイクで使うもの） */
extern void playTapSound();
extern void playSpikeHitSound();

/* LED制御関数の宣言を追加 */
extern void triggerLEDEffect(int effect);
extern void updateGameLEDs();
extern void showHeightLED(float current, float max_val); // 鳥の高さに合わせてLEDを光らせる関数

/* スパイク専用の関数の宣言 */
void initSpike(bool isRetry = false);
void SPIKE();

#endif