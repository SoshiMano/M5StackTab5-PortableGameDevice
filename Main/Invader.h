#ifndef INVADER_H
#define INVADER_H

#include <M5Unified.h>

/* ゲームの状態管理（メインと共有するための列挙型定義） */
enum SystemState { 
    STATE_MENU,     
    STATE_INVADER,  
    STATE_SPIKE     
};

// PRの練習用にここにコメント
// 上のENUMは　別途 main.hを作成してそこで記述して　各ゲームではそのmain.hをインクルードするほうが値の意味や目的を考えると正しそう

/* メインファイル(Main.ino)で定義されている変数の宣言（externを利用して共有） */
extern SystemState currentState;
extern int score;
extern int lives;
extern bool isGameOver;
extern const int BAR_HEIGHT;

/* インベーダー側(Invader.cpp)で定義してメインで使う変数 */
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

/* LED制御関数の宣言を追加 */
extern void triggerLEDEffect(int effect);
extern void updateGameLEDs();

/* インベーダー専用の関数の宣言 */
void initInvader();
void INVADER();

#endif
