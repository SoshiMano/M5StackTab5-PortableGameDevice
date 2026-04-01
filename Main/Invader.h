#ifndef INVADER_H
#define INVADER_H

#include <M5Unified.h>

/* ゲームの状態管理（メインと共有） */
enum SystemState { 
    STATE_MENU,     
    STATE_INVADER,  
    STATE_SPIKE     
};

/* メインファイルで定義されている変数の宣言（extern） */
extern SystemState currentState;
extern int score;
extern int lives;
extern bool isGameOver;
extern const int BAR_HEIGHT;

/* インベーダー側で定義してメインで使う変数 */
extern int currentStage;

/* メインファイルで定義されている関数の宣言 */
extern void drawTopBar(const char* leftText);
extern void toggleMute();
extern void takeScreenshot();
extern void playShootSound();
extern void playHitSound();
extern void playEnemyMoveSound();
extern void playEnemyShootSound();
extern void playGameOverSound();
extern void playDecisionSound();
extern void drawMenu();
extern void drawLoadingScreen(const char* message);

/* インベーダー専用の関数 */
void initInvader();
void INVADER();

#endif