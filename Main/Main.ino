/* M5Stackの基本機能とSDカード用のライブラリを読み込む */
#include <M5Unified.h>
#include <SD.h>
#include <FastLED.h> // WS2812Bを制御するためのライブラリ
#include "Invader.h"
#include "Spike.h"

/* ---------------------------------------------------------
   ゲームの状態管理
   --------------------------------------------------------- */
/* 現在どの画面にいるかを管理するための列挙型（インベーダーのヘッダで定義済み） */
SystemState currentState = STATE_MENU;

/* ---------------------------------------------------------
   レイアウト・設定用の定数
   --------------------------------------------------------- */
const int BAR_HEIGHT = 80;       // 上部のメニューバーの高さ
const int BUTTON_WIDTH = 400;    // メニュー画面のボタンの幅
const int BUTTON_HEIGHT = 150;   // メニュー画面のボタンの高さ
const int BUTTON_X = 160;        // メニュー画面のボタンのX座標

/* ---------------------------------------------------------
   ステージ・進行管理用の変数（共通）
   --------------------------------------------------------- */
bool isGameOver = false;         // ゲームオーバー状態かどうかの判定フラグ
int score = 0;                   // プレイヤーのスコア
int lives = 2;                   // プレイヤーの残機

/* ---------------------------------------------------------
   サウンド・効果音管理
   --------------------------------------------------------- */
/* SDカードから読み込んだ音声データを格納するためのポインタとサイズ */
uint8_t* wav_buffer = nullptr;   
size_t wav_size = 0;
uint8_t* wav_laser = nullptr;    
size_t size_laser = 0;
uint8_t* wav_blast = nullptr;    
size_t size_blast = 0;
uint8_t* wav_move = nullptr;     
size_t size_move = 0;

uint8_t* wav_spike_bgm = nullptr;   
size_t size_spike_bgm = 0;
uint8_t* wav_spike_blast = nullptr; 
size_t size_spike_blast = 0;
uint8_t* wav_gameover = nullptr;    
size_t size_gameover = 0;

bool isMuted = false;            // ミュート状態かどうかのフラグ
int currentVolume = 128;         // 現在の音量

/* ---------------------------------------------------------
   LED（WS2812B）制御用の設定
   --------------------------------------------------------- */
#define NUM_LEDS 8        // 接続しているLEDの数
#define LED_PIN 0         // 制御ピン（G0）

CRGB leds[NUM_LEDS];
unsigned long lastLedToggle = 0;
int ledPattern = 0; // 点灯パターンの状態管理

// LEDの初期化関数
void initLEDs() {
    // FastLEDライブラリを使ってWS2812Bの制御設定を行う
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(15); // 明るさ（0〜255）
    FastLED.clear();
    FastLED.show();
}

// メインメニュー用のLED点滅処理関数
void updateMenuLEDs() {
    // 500ミリ秒ごとに点灯パターンを切り替える
    if (millis() - lastLedToggle > 500) {
        lastLedToggle = millis();
        ledPattern = !ledPattern;
        for (int i = 0; i < NUM_LEDS; i++) {
            // インデックスが偶数か奇数かでON/OFFを分ける（1つおき）
            if (i % 2 == ledPattern) {
                leds[i] = CRGB(0, 255, 255); // 点灯する色（ここではシアン/水色）
            } else {
                leds[i] = CRGB::Black;       // 消灯
            }
        }
        FastLED.show(); // 設定した色をLEDに反映
    }
}

// ゲーム開始時などにLEDを全て消灯する関数
void turnOffLEDs() {
    FastLED.clear();
    FastLED.show();
}

/* ---------------------------------------------------------
   ゲーム中のLEDエフェクト管理
   --------------------------------------------------------- */
enum LEDEffect {
    EFFECT_NONE = 0,
    EFFECT_SHOOT = 1,
    EFFECT_ENEMY_HIT = 2,
    EFFECT_PLAYER_HIT = 3
};

LEDEffect currentEffect = EFFECT_NONE;
unsigned long effectStartTime = 0;

// 他のファイル（Invader.cpp等）からエフェクトを発火させる関数
void triggerLEDEffect(int effect) {
    currentEffect = (LEDEffect)effect;
    effectStartTime = millis();
}

// ゲーム中に毎フレーム呼ばれて、LEDの状態を更新する関数
void updateGameLEDs() {
    if (currentEffect == EFFECT_NONE) return;
    
    unsigned long elapsed = millis() - effectStartTime;
    
    switch (currentEffect) {
        case EFFECT_SHOOT:
            // 一瞬（50ms）白く光る
            if (elapsed < 50) {
                for(int i = 0; i < NUM_LEDS; i++) leds[i] = CRGB::White;
            } else {
                FastLED.clear();
                currentEffect = EFFECT_NONE;
            }
            break;
            
        case EFFECT_ENEMY_HIT:
            // 緑にチカチカ (300ms間、50ms周期)
            if (elapsed < 300) {
                if ((elapsed / 50) % 2 == 0) {
                    for(int i = 0; i < NUM_LEDS; i++) leds[i] = CRGB::Green;
                } else {
                    FastLED.clear();
                }
            } else {
                FastLED.clear();
                currentEffect = EFFECT_NONE;
            }
            break;
            
        case EFFECT_PLAYER_HIT:
            // 赤にチカチカ (600ms間、50ms周期)
            if (elapsed < 600) {
                if ((elapsed / 50) % 2 == 0) {
                    for(int i = 0; i < NUM_LEDS; i++) leds[i] = CRGB::Red;
                } else {
                    FastLED.clear();
                }
            } else {
                FastLED.clear();
                currentEffect = EFFECT_NONE;
            }
            break;
    }
    FastLED.show();
}


/* ---------------------------------------------------------
   共通関数（音声再生）
   --------------------------------------------------------- */
/* ミュートでなければ対応するWAVファイルを再生する関数群 */
void playShootSound() { if (wav_laser && !isMuted) M5.Speaker.playWav(wav_laser, size_laser, 1, 1); }
void playHitSound() { if (wav_blast && !isMuted) M5.Speaker.playWav(wav_blast, size_blast, 1, 2); }
void playEnemyMoveSound() { if (wav_move && !isMuted) M5.Speaker.playWav(wav_move, size_move, 1, 3); }
void playEnemyShootSound() { if (wav_laser && !isMuted) M5.Speaker.playWav(wav_laser, size_laser, 1, 4); }
void playSpikeHitSound() { if (wav_spike_blast && !isMuted) M5.Speaker.playWav(wav_spike_blast, size_spike_blast, 1, 2); else playHitSound(); }
void playGameOverSound() { if (wav_gameover && !isMuted) M5.Speaker.playWav(wav_gameover, size_gameover, 1, 5); else if(!isMuted) M5.Speaker.tone(300, 500); }

/* 単純なビープ音（BEEP）を鳴らす関数群 */
void playTapSound() { if(!isMuted) M5.Speaker.tone(1000, 50); }
void playDecisionSound() { if(!isMuted) M5.Speaker.tone(2000, 100); }
void playCameraSound() { if(!isMuted) M5.Speaker.tone(2500, 100); } 

/* ---------------------------------------------------------
   ユーティリティ関数
   --------------------------------------------------------- */
/* ロード画面を描画する関数 */
void drawLoadingScreen(const char* message) {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextSize(3);
    M5.Display.drawString(message, M5.Display.width() / 2, M5.Display.height() / 2);
    delay(50);
}

/* SDカードからWAVファイルをメモリ(PSRAM)に読み込む関数 */
uint8_t* loadWavFile(const char* path, size_t* out_size) {
    File file = SD.open(path);
    if (!file) return nullptr;
    *out_size = file.size();
    uint8_t* buffer = (uint8_t*)ps_malloc(*out_size);
    if (buffer) file.read(buffer, *out_size);
    file.close();
    return buffer;
}

/* ---------------------------------------------------------
   スクリーンショット機能
   --------------------------------------------------------- */
/* 現在のディスプレイの描画内容をBMP形式でSDカードに保存する関数 */
void takeScreenshot() {
    M5.Speaker.stop(0); // 撮影時のノイズ防止のため音声を一時停止
    int w = M5.Display.width();
    int h = M5.Display.height();
    char filename[32];
    
    // 連番で空いているファイル名を検索
    for (int i = 0; i < 1000; i++) {
        sprintf(filename, "/SHOT_%03d.bmp", i);
        if (!SD.exists(filename)) break;
    }
    
    File f = SD.open(filename, FILE_WRITE);
    if (!f) { playGameOverSound(); return; }
    
    // BMPヘッダの作成と書き込み
    uint32_t rowSize = (w * 3 + 3) & ~3; 
    uint32_t fileSize = 54 + (rowSize * h);
    uint8_t header[54] = { 'B','M', (uint8_t)fileSize, (uint8_t)(fileSize>>8), (uint8_t)(fileSize>>16), (uint8_t)(fileSize>>24), 0,0, 0,0, 54,0,0,0, 40,0,0,0, (uint8_t)w, (uint8_t)(w>>8), (uint8_t)(w>>16), (uint8_t)(w>>24), (uint8_t)h, (uint8_t)(h>>8), (uint8_t)(h>>16), (uint8_t)(h>>24), 1,0, 24,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 };
    f.write(header, 54);
    
    // 画面下部から1行ずつピクセルデータを取得して書き込み（RGBをBGRに変換）
    uint8_t* buffer = (uint8_t*)ps_malloc(rowSize);
    if (buffer) {
        for (int y = h - 1; y >= 0; y--) {
            M5.Display.readRectRGB(0, y, w, 1, buffer);
            for (int x = 0; x < w; x++) {
                uint8_t r = buffer[x*3]; uint8_t g = buffer[x*3+1]; uint8_t b = buffer[x*3+2];
                buffer[x*3] = b; buffer[x*3+1] = g; buffer[x*3+2] = r;
            }
            f.write(buffer, rowSize);
        }
        free(buffer);
    }
    f.close();
    playCameraSound(); 
}

/* ---------------------------------------------------------
   UI描画機能
   --------------------------------------------------------- */
/* 画面上部のステータスバー（戻るボタンやミュートボタン）を描画する */
void drawTopBar(const char* leftText) {
    M5.Display.fillRect(0, 0, M5.Display.width(), BAR_HEIGHT, TFT_DARKGREY);
    M5.Display.setTextColor(TFT_WHITE, TFT_DARKGREY);
    M5.Display.setTextDatum(middle_left); 
    M5.Display.setTextSize(3);
    M5.Display.drawString(leftText, 10, BAR_HEIGHT / 2);
    
    // インベーダーゲーム中なら中央にステージ数を表示
    if (currentState == STATE_INVADER) {
        M5.Display.setTextDatum(middle_center);
        char stageText[32]; sprintf(stageText, "STAGE %d", currentStage);
        M5.Display.drawString(stageText, M5.Display.width() / 2, BAR_HEIGHT / 2);
    }
    
    // 右側のボタン群（ミュート、スクショ）の描画
    int muteX = M5.Display.width() - 120;
    int shotX = muteX - 130;
    M5.Display.setTextDatum(middle_center); M5.Display.setTextSize(2);
    M5.Display.drawString(isMuted ? "MUTE" : "SOUND", muteX + 60, BAR_HEIGHT / 2);
    M5.Display.drawRoundRect(shotX, 10, 110, BAR_HEIGHT - 20, 5, TFT_WHITE);
    M5.Display.drawString("SS", shotX + 55, BAR_HEIGHT / 2);
    M5.Display.drawFastHLine(0, BAR_HEIGHT, M5.Display.width(), TFT_WHITE);
}

/* ミュート状態を切り替える関数 */
void toggleMute() {
    isMuted = !isMuted;
    M5.Speaker.setVolume(isMuted ? 0 : currentVolume);
    int muteX = M5.Display.width() - 120;
    M5.Display.fillRect(muteX, 0, 120, BAR_HEIGHT, TFT_DARKGREY);
    M5.Display.setTextColor(TFT_WHITE, TFT_DARKGREY);
    M5.Display.setTextDatum(middle_center); M5.Display.setTextSize(2);
    M5.Display.drawString(isMuted ? "MUTE" : "SOUND", muteX + 60, BAR_HEIGHT / 2);
    M5.Display.drawFastHLine(0, BAR_HEIGHT, M5.Display.width(), TFT_WHITE);
}

/* ---------------------------------------------------------
   メニュー画面処理
   --------------------------------------------------------- */
/* タイトルメニューの描画 */
void drawMenu() {
    M5.Speaker.stop(0); 
    M5.Display.fillScreen(TFT_BLACK);
    drawTopBar(" MAIN MENU");
    
    M5.Display.fillRoundRect(BUTTON_X, 200, BUTTON_WIDTH, BUTTON_HEIGHT, 15, TFT_BLUE);
    M5.Display.setTextColor(TFT_WHITE); M5.Display.setTextDatum(middle_center); M5.Display.setTextSize(3);
    M5.Display.drawString("INVADER", BUTTON_X + BUTTON_WIDTH / 2, 200 + BUTTON_HEIGHT / 2);
    
    M5.Display.fillRoundRect(BUTTON_X, 450, BUTTON_WIDTH, BUTTON_HEIGHT, 15, TFT_RED);
    M5.Display.drawString("SPIKE", BUTTON_X + BUTTON_WIDTH / 2, 450 + BUTTON_HEIGHT / 2);
}

/* メニュー画面のタッチ判定と処理 */
void MAINMENU() {
    // BGMのループ再生
    if (wav_buffer != nullptr && !M5.Speaker.isPlaying(0) && !isMuted) { 
        M5.Speaker.playWav(wav_buffer, wav_size, ~0u, 0); 
    }
    
    // メニュー画面にいる間、LEDを点滅させる
    updateMenuLEDs();
    
    // タッチ入力の処理
    if (M5.Touch.getCount() > 0) {
        auto t = M5.Touch.getDetail();
        if (t.wasPressed()) {
            int tx = t.x; int ty = t.y; bool isBtn = false;
            int muteX = M5.Display.width() - 120; int shotX = muteX - 130;
            
            // トップバーのタップ判定
            if (ty <= BAR_HEIGHT) { 
                isBtn = true; 
                if (tx >= muteX) toggleMute(); 
                else if (tx >= shotX && tx < muteX) takeScreenshot(); 
            }
            // 各ゲームのボタンのタップ判定
            else {
                if (tx >= BUTTON_X && tx <= BUTTON_X + BUTTON_WIDTH) {
                    if (ty >= 200 && ty <= 200 + BUTTON_HEIGHT) { 
                        isBtn = true; playDecisionSound(); 
                        turnOffLEDs(); // ゲーム開始前にLEDを全消灯
                        currentStage = 1; score = 0; lives = 2; 
                        currentState = STATE_INVADER; initInvader(); 
                    }
                    else if (ty >= 450 && ty <= 450 + BUTTON_HEIGHT) { 
                        isBtn = true; playDecisionSound(); 
                        turnOffLEDs(); // ゲーム開始前にLEDを全消灯
                        currentState = STATE_SPIKE; initSpike(); 
                    }
                }
            }
            if (!isBtn) playTapSound();
        }
    }
}

/* ---------------------------------------------------------
   プログラムの起点
   --------------------------------------------------------- */
void setup() {
    // M5Stackの初期化と音量設定
    auto cfg = M5.config(); M5.begin(cfg); M5.Display.setRotation(0); M5.Speaker.setVolume(currentVolume);
    
    // LEDの初期化
    initLEDs();
    
    drawLoadingScreen("Now Loading...");
    
    // SDカードから音声ファイルを読み込む
    if (SD.begin()) { 
        wav_buffer = loadWavFile("/BGM_Menu.wav", &wav_size); 
        wav_laser = loadWavFile("/SE_INVADE_Laser.wav", &size_laser); 
        wav_blast = loadWavFile("/SE_INVADE_Blast.wav", &size_blast); 
        wav_move  = loadWavFile("/SE_INVADE_Move.wav",  &size_move); 
        wav_spike_bgm = loadWavFile("/BGM_SPIKE.wav", &size_spike_bgm);
        wav_spike_blast = loadWavFile("/SE_SPIKE_Blast.wav", &size_spike_blast);
        wav_gameover = loadWavFile("/SE_Gameover.wav", &size_gameover);
    }
    randomSeed(millis()); // 乱数の初期化
    drawMenu();
}

/* 常時実行されるメインループ */
void loop() {
    M5.update(); // タッチパネル等の状態を更新
    
    // 現在のステート（画面状態）に応じて各処理を呼び出す
    switch (currentState) {
        case STATE_MENU: MAINMENU(); break;
        case STATE_INVADER: INVADER(); updateGameLEDs(); break;
        case STATE_SPIKE: SPIKE(); updateGameLEDs(); break;
    }
    delay(10);
}