#ifndef SPIKE_H
#define SPIKE_H

#include <M5Unified.h>
#include "Invader.h"

/* メインファイルで定義されている音声変数（スパイク用） */
extern uint8_t* wav_spike_bgm;
extern size_t size_spike_bgm;
extern bool isMuted;

/* メインファイルで定義されている関数の宣言（スパイクで使うもの） */
extern void playTapSound();
extern void playSpikeHitSound();

/* スパイク専用の関数 */
void initSpike(bool isRetry = false);
void SPIKE();

#endif