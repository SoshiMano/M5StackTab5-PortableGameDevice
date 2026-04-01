#include "Spike.h"
#include <SD.h>

/* ---------------------------------------------------------
   スパイク（SPIKE）ゲーム用の変数
   --------------------------------------------------------- */
float spikePlayerX = 360.0f;       
float oldSpikePlayerX = 360.0f;         
const float spikePlayerY = 200.0f; 
float spikeVelocity = 0.0f;        
bool isSpikeStarted = false; 

const int MAX_OBSTACLES = 5;
struct Obstacle {
    float x, y;
    float oldX, oldY;
    float speed;
    int type; 
    bool active;
    bool passed; 
};
Obstacle obstacles[MAX_OBSTACLES];

M5Canvas spikePlayerSprite(&M5.Display);
M5Canvas spike0Sprite(&M5.Display);
M5Canvas spike1Sprite(&M5.Display);

/* ---------------------------------------------------------
   SPIKE関係の関数
   --------------------------------------------------------- */
void initSpike(bool isRetry) {
    M5.Speaker.stop(0); 
    
    if (!isRetry) {
        M5.Display.fillScreen(TFT_BLACK);
        bool drawSuccess = false;
        File imgFile = SD.open("/Rotate.png");
        int imgW = 600; int imgH = 327;
        int imgX = (M5.Display.width() - imgW) / 2;
        int imgY = (M5.Display.height() - imgH) / 2 - 50;

        if (imgFile) {
            size_t pngSize = imgFile.size();
            uint8_t* pngBuf = (uint8_t*)ps_malloc(pngSize);
            if (pngBuf) { imgFile.read(pngBuf, pngSize); drawSuccess = M5.Display.drawPng(pngBuf, pngSize, imgX, imgY); free(pngBuf); }
            imgFile.close();
        }
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK); M5.Display.setTextDatum(middle_center);
        if (!drawSuccess) { M5.Display.setTextColor(TFT_RED); M5.Display.setTextSize(3); M5.Display.drawString("PNG Render Failed!", M5.Display.width() / 2, imgY + (imgH / 2)); }
        M5.Display.setTextColor(TFT_WHITE); M5.Display.setTextSize(4); M5.Display.drawString("Please Rotate Device", M5.Display.width() / 2, imgY + imgH + 30);
        M5.Display.setTextSize(2); M5.Display.drawString("Tap to Continue", M5.Display.width() / 2, imgY + imgH + 80);

        while (true) { M5.update(); if (M5.Touch.getCount() > 0) { auto t = M5.Touch.getDetail(); if (t.wasPressed()) { playDecisionSound(); break; } } delay(10); }
    }

    spikePlayerX = M5.Display.width() / 2; 
    oldSpikePlayerX = spikePlayerX;
    spikeVelocity = 0.0f;
    isGameOver = false; 
    isSpikeStarted = false; 
    score = 0; 
    
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        obstacles[i].active = false;
        obstacles[i].passed = false;
    }
    
    M5.Display.fillScreen(TFT_BLACK);
    drawTopBar(" <- BACK");

    const char* spikePlayerDot[10] = { 
        "....WW....", 
        "...WWWW...", 
        "..WWWWWW..", 
        "WWWWWWWWWW", 
        "WWWWWWWWWW", 
        "..WWWWWW..", 
        "..WWKKWW..", 
        "...WWWW...", 
        "....YY....", 
        "....YY...."  
    };
    
    spikePlayerSprite.setColorDepth(16); 
    spikePlayerSprite.createSprite(70, 50);
    spikePlayerSprite.fillScreen(TFT_BLACK);
    
    for (int y = 0; y < 10; y++) { 
        for (int x = 0; x < 10; x++) { 
            if (spikePlayerDot[y][x] == 'W') spikePlayerSprite.fillRect(x * 5 + 10, y * 5, 5, 5, TFT_WHITE); 
            else if (spikePlayerDot[y][x] == 'Y') spikePlayerSprite.fillRect(x * 5 + 10, y * 5, 5, 5, TFT_YELLOW); 
            else if (spikePlayerDot[y][x] == 'K') spikePlayerSprite.fillRect(x * 5 + 10, y * 5, 5, 5, TFT_BLACK); 
        } 
    }

    int spikeW = M5.Display.width() / 2 + 40; 

    if (spike0Sprite.width() != spikeW) {
        spike0Sprite.deleteSprite();
        spike0Sprite.setColorDepth(16);
        spike0Sprite.createSprite(spikeW, 95);
        spike0Sprite.fillScreen(TFT_BLACK);
        /* 右壁から突き出るトゲ */
        spike0Sprite.fillTriangle(0, 40, spikeW, 0, spikeW, 80, TFT_RED);
    }
    if (spike1Sprite.width() != spikeW) {
        spike1Sprite.deleteSprite();
        spike1Sprite.setColorDepth(16);
        spike1Sprite.createSprite(spikeW, 95);
        spike1Sprite.fillScreen(TFT_BLACK);
        /* 左壁から突き出るトゲ */
        spike1Sprite.fillTriangle(spikeW, 40, 0, 0, 0, 80, TFT_RED);
    }

    if (wav_spike_bgm != nullptr && !isMuted) {
        M5.Speaker.playWav(wav_spike_bgm, size_spike_bgm, ~0u, 0);
    }
}

void SPIKE() {
    int muteX = M5.Display.width() - 120;
    int shotX = muteX - 130;

    if (isGameOver) {
        if (M5.Touch.getCount() > 0) {
            auto t = M5.Touch.getDetail();
            if (t.wasPressed()) {
                int tx = t.x; int ty = t.y;
                if (ty <= BAR_HEIGHT) { 
                    if (tx >= muteX) toggleMute(); 
                    else if (tx >= shotX && tx < muteX) takeScreenshot();
                    else if (tx < 200) {
                        playDecisionSound(); 
                        currentState = STATE_MENU; 
                        drawMenu(); 
                    }
                } else { 
                    playDecisionSound(); 
                    initSpike(true); 
                }
            }
        }
        return; 
    }

    bool isUI = false;
    if (M5.Touch.getCount() > 0) {
        auto t = M5.Touch.getDetail(); int tx = t.x; int ty = t.y;
        if (ty <= BAR_HEIGHT) { 
            isUI = true; 
            if (t.wasPressed()) { 
                if (tx >= muteX) toggleMute(); 
                else if (tx >= shotX && tx < muteX) takeScreenshot();
                else if (tx < 200) {
                    playDecisionSound(); 
                    currentState = STATE_MENU; 
                    drawMenu(); 
                    return; 
                }
            } 
        }
    }

    if (!isSpikeStarted) {
        M5.Display.fillRect((int)oldSpikePlayerX - 35, (int)spikePlayerY - 25, 72, 52, TFT_BLACK);
        spikePlayerSprite.pushSprite((int)spikePlayerX - 35, (int)spikePlayerY - 25);
        oldSpikePlayerX = spikePlayerX;

        M5Canvas textSprite(&M5.Display);
        textSprite.setColorDepth(16);
        textSprite.createSprite(250, 40);
        textSprite.fillScreen(TFT_BLACK);
        textSprite.setTextColor(TFT_WHITE, TFT_BLACK);
        textSprite.setTextDatum(middle_center);
        textSprite.setTextSize(3);
        textSprite.drawString("TAP TO START", 125, 20);
        
        textSprite.pushRotateZoom(M5.Display.width() / 2, M5.Display.height() / 2, 90.0f, 1.0f, 1.0f);
        textSprite.deleteSprite();

        if (M5.Touch.getCount() > 0 && !isUI) {
            auto t = M5.Touch.getDetail();
            if (t.wasPressed()) {
                isSpikeStarted = true;
                spikeVelocity = -8.0f; 
                playTapSound();
                M5.Display.fillRect(M5.Display.width() / 2 - 30, M5.Display.height() / 2 - 130, 60, 260, TFT_BLACK);
            }
        }
        return; 
    }
    
    if (M5.Touch.getCount() > 0 && !isUI) spikeVelocity += 0.8f; else spikeVelocity -= 0.8f;
    if (spikeVelocity > 10.0f) spikeVelocity = 10.0f; if (spikeVelocity < -10.0f) spikeVelocity = -10.0f;
    spikePlayerX += spikeVelocity;

    int passedCount = score / 100;
    int minDistance = 400 - (passedCount * 15);
    if (minDistance < 150) minDistance = 150; 
    
    float currentSpikeSpeed = 10.0f + (passedCount * 0.5f);
    if (currentSpikeSpeed > 30.0f) currentSpikeSpeed = 30.0f; 

    int spawnRate = 3 + (passedCount / 5);
    if (spawnRate > 15) spawnRate = 15;

    bool canSpawn = true;
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (obstacles[i].active && obstacles[i].y > M5.Display.height() - minDistance) {
            canSpawn = false; 
            break;
        }
    }

    int spikeH = M5.Display.width() / 2 + 40; 

    if (canSpawn && random(0, 100) < spawnRate) {
        for (int i = 0; i < MAX_OBSTACLES; i++) {
            if (!obstacles[i].active) {
                obstacles[i].active = true;
                obstacles[i].passed = false; 
                obstacles[i].type = random(0, 2);
                obstacles[i].y = M5.Display.height() + 50; 
                obstacles[i].speed = currentSpikeSpeed; 
                obstacles[i].x = (obstacles[i].type == 0) ? M5.Display.width() - spikeH : 0;
                obstacles[i].oldX = obstacles[i].x;
                obstacles[i].oldY = obstacles[i].y;
                break;
            }
        }
    }

    bool hit = false;
    
    if (spikePlayerX < 25 || spikePlayerX > M5.Display.width() - 25) {
        hit = true;
    }

    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (obstacles[i].active && obstacles[i].y >= BAR_HEIGHT) {
            float oy = obstacles[i].y;
            
            if (!obstacles[i].passed && oy < spikePlayerY - 30) {
                obstacles[i].passed = true;
                score += 100;
                if (!isMuted) M5.Speaker.tone(1500, 50); 
            }

            if (spikePlayerY > oy - 35 && spikePlayerY < oy + 35) {
                float dx = abs(spikePlayerY - oy);
                float h = spikeH * (1.0f - (dx / 40.0f)); 
                if (h > 0) {
                    if (obstacles[i].type == 0) { 
                        if (spikePlayerX + 15 > M5.Display.width() - h) hit = true;
                    } else { 
                        if (spikePlayerX - 15 < h) hit = true;
                    }
                }
            }
        }
    }

    if (hit) {
        isGameOver = true;
        M5.Speaker.stop(0); 
        playSpikeHitSound(); 
        
        M5.Display.startWrite();
        spikePlayerSprite.pushSprite((int)spikePlayerX - 35, (int)spikePlayerY - 25);
        M5.Display.endWrite();
        
        delay(1500); 

        M5.Display.fillRect(0, BAR_HEIGHT + 1, M5.Display.width(), M5.Display.height() - BAR_HEIGHT - 1, TFT_BLACK); 
        
        M5Canvas textSprite(&M5.Display);
        textSprite.setColorDepth(16);
        textSprite.createSprite(400, 200); 
        textSprite.fillScreen(TFT_BLACK);
        textSprite.setTextDatum(middle_center);
        
        textSprite.setTextColor(TFT_RED, TFT_BLACK); 
        textSprite.setTextSize(5); 
        textSprite.drawString("GAME OVER", 200, 40); 
        
        textSprite.setTextColor(TFT_YELLOW, TFT_BLACK); 
        textSprite.setTextSize(3); 
        char resultText[32]; 
        sprintf(resultText, "SCORE: %05d", score); 
        textSprite.drawString(resultText, 200, 110); 
        
        textSprite.setTextColor(TFT_WHITE, TFT_BLACK); 
        textSprite.setTextSize(2); 
        textSprite.drawString("Tap to RESTART", 200, 170); 

        textSprite.pushRotateZoom(M5.Display.width() / 2, M5.Display.height() / 2 + 50, 90.0f, 1.0f, 1.0f);
        textSprite.deleteSprite();

        playGameOverSound(); 
        return;
    }

    M5.Display.startWrite();
    M5.Display.setClipRect(0, BAR_HEIGHT + 1, M5.Display.width(), M5.Display.height() - BAR_HEIGHT - 1);

    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (obstacles[i].active) {
            obstacles[i].y -= obstacles[i].speed;
            
            if (obstacles[i].y < BAR_HEIGHT - 100) {
                obstacles[i].active = false;
                /* 完全に画面外に出る時に、最後の残像を綺麗に消す */
                if (obstacles[i].type == 0) {
                    M5.Display.fillRect(M5.Display.width() - spikeH, (int)obstacles[i].oldY - 48, spikeH, 95, TFT_BLACK);
                } else {
                    M5.Display.fillRect(0, (int)obstacles[i].oldY - 48, spikeH, 95, TFT_BLACK);
                }
            } else {
                /* 移動した分の「はみ出た残像」だけをピンポイントで黒塗り！ */
                int dy = (int)obstacles[i].oldY - (int)obstacles[i].y;
                if (dy > 0) {
                    if (obstacles[i].type == 0) {
                        M5.Display.fillRect(M5.Display.width() - spikeH, (int)obstacles[i].y - 48 + 95, spikeH, dy, TFT_BLACK);
                        spike0Sprite.pushSprite(M5.Display.width() - spikeH, (int)obstacles[i].y - 48);
                    } else {
                        M5.Display.fillRect(0, (int)obstacles[i].y - 48 + 95, spikeH, dy, TFT_BLACK);
                        spike1Sprite.pushSprite(0, (int)obstacles[i].y - 48);
                    }
                }
                obstacles[i].oldX = obstacles[i].x;
                obstacles[i].oldY = obstacles[i].y;
            }
        }
    }

    /* 鳥の描画も差分更新にしてチラつきとおさらば！ */
    int diffX = (int)spikePlayerX - (int)oldSpikePlayerX;
    if (diffX > 0) {
        M5.Display.fillRect((int)oldSpikePlayerX - 35, (int)spikePlayerY - 25, diffX, 50, TFT_BLACK);
    } else if (diffX < 0) {
        M5.Display.fillRect((int)spikePlayerX - 35 + 70, (int)spikePlayerY - 25, -diffX, 50, TFT_BLACK);
    }
    spikePlayerSprite.pushSprite((int)spikePlayerX - 35, (int)spikePlayerY - 25);
    oldSpikePlayerX = spikePlayerX;
    
    M5Canvas scoreSprite(&M5.Display);
    scoreSprite.setColorDepth(16);
    scoreSprite.createSprite(250, 40);
    scoreSprite.fillScreen(TFT_BLACK);
    scoreSprite.setTextColor(TFT_WHITE, TFT_BLACK);
    scoreSprite.setTextDatum(middle_center);
    scoreSprite.setTextSize(3);
    char sc[32]; sprintf(sc, "SCORE: %05d", score);
    scoreSprite.drawString(sc, 125, 20);
    
    scoreSprite.pushRotateZoom(M5.Display.width() - 30, M5.Display.height() - 150, 90.0f, 1.0f, 1.0f);
    scoreSprite.deleteSprite();

    M5.Display.clearClipRect();
    M5.Display.endWrite();
}