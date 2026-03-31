/* M5Stackの基本機能とSDカード用のライブラリを読み込む */
#include <M5Unified.h>
#include <SD.h>

/* ---------------------------------------------------------
   ゲームの状態管理
   --------------------------------------------------------- */
enum SystemState { 
    STATE_MENU,     
    STATE_INVADER,  
    STATE_SPIKE     
};
SystemState currentState = STATE_MENU;

/* ---------------------------------------------------------
   レイアウト・設定用の定数
   --------------------------------------------------------- */
const int BAR_HEIGHT = 80;       
const int BUTTON_WIDTH = 400;    
const int BUTTON_HEIGHT = 150;   
const int BUTTON_X = 160;        

/* ---------------------------------------------------------
   ステージ・進行管理用の変数
   --------------------------------------------------------- */
int currentStage = 1;            
int activeEnemyRows = 0;         
int activeEnemyCols = 0;         
int enemiesRemaining = 0;        
bool isGameOver = false;         

int score = 0;
int lives = 2; 

/* ---------------------------------------------------------
   入力（タッチ）管理用の変数
   --------------------------------------------------------- */
unsigned long touchStartTime = 0; 
bool isMovingMode = false;        

/* ---------------------------------------------------------
   自機（プレイヤー）の設定
   --------------------------------------------------------- */
int playerX = 360;               
int oldPlayerX = 360;            
const int playerY = 1100;        
const int playerW = 50;          
const int playerH = 50;          
const int playerSpeed = 15;      
const int DEFENSE_LINE_Y = 1000; 

M5Canvas playerSprite(&M5.Display);

/* ---------------------------------------------------------
   自機の弾（攻撃）の設定
   --------------------------------------------------------- */
int bulletX, bulletY;            
bool bulletActive = false;       
const int bulletW = 4;           
const int bulletH = 15;          
const int bulletSpeed = 25;      

/* ---------------------------------------------------------
   敵（インベーダー）の設定
   --------------------------------------------------------- */
const int ENEMY_ROWS = 5;        
const int ENEMY_COLS = 11;       
const int ENEMY_W = 40;          
const int ENEMY_H = 40;          

struct Enemy {
    int x, y;
    bool alive;
};
Enemy enemies[ENEMY_ROWS][ENEMY_COLS]; 

int enemyDir = 1;                
int enemySpeed = 15;             
unsigned long lastEnemyMove = 0; 
int enemyInterval = 500;         

M5Canvas enemySprite(&M5.Display);

/* ---------------------------------------------------------
   敵の弾の設定（インベーダー用）
   --------------------------------------------------------- */
const int MAX_ENEMY_BULLETS = 3; 
struct EnemyBullet {
    int x, y;
    bool active;
};
EnemyBullet enemyBullets[MAX_ENEMY_BULLETS];
const int enemyBulletW = 4;
const int enemyBulletH = 15;
const int enemyBulletSpeed = 15; 

/* ---------------------------------------------------------
   スパイク（SPIKE）ゲーム用の変数
   --------------------------------------------------------- */
float spikePlayerX = 360.0f;       
const float spikePlayerY = 200.0f; 
float spikeVelocity = 0.0f;        

/* トゲ（障害物）の管理 */
const int MAX_OBSTACLES = 5;
struct Obstacle {
    float x, y;
    float speed;
    int type; /* 0=上壁トゲ(portrait右端), 1=下壁トゲ(portrait左端) */
    bool active;
};
Obstacle obstacles[MAX_OBSTACLES];

/* トゲ用のスプライト（軽量な部分バッファ） */
M5Canvas spike0Sprite(&M5.Display);
M5Canvas spike1Sprite(&M5.Display);

/* ---------------------------------------------------------
   サウンド・効果音管理
   --------------------------------------------------------- */
uint8_t* wav_buffer = nullptr;   
size_t wav_size = 0;
uint8_t* wav_laser = nullptr;    
size_t size_laser = 0;
uint8_t* wav_blast = nullptr;    
size_t size_blast = 0;
uint8_t* wav_move = nullptr;     
size_t size_move = 0;

bool isMuted = false;            
int currentVolume = 128;         

/* ---------------------------------------------------------
   共通関数
   --------------------------------------------------------- */
void playShootSound() { if (wav_laser) M5.Speaker.playWav(wav_laser, size_laser, 1, 1); }
void playHitSound() { if (wav_blast) M5.Speaker.playWav(wav_blast, size_blast, 1, 2); }
void playEnemyMoveSound() { if (wav_move) M5.Speaker.playWav(wav_move, size_move, 1, 3); }
void playEnemyShootSound() { if (wav_laser) M5.Speaker.playWav(wav_laser, size_laser, 1, 4); }
void playTapSound() { M5.Speaker.tone(1000, 50); }
void playDecisionSound() { M5.Speaker.tone(2000, 100); }
void playGameOverSound() { M5.Speaker.tone(300, 500); } 
void playCameraSound() { M5.Speaker.tone(2500, 100); } 

void drawLoadingScreen(const char* message) {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextSize(3);
    M5.Display.drawString(message, M5.Display.width() / 2, M5.Display.height() / 2);
    delay(50);
}

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
void takeScreenshot() {
    M5.Speaker.stop();
    int w = M5.Display.width();
    int h = M5.Display.height();
    char filename[32];
    for (int i = 0; i < 1000; i++) {
        sprintf(filename, "/SHOT_%03d.bmp", i);
        if (!SD.exists(filename)) break;
    }
    File f = SD.open(filename, FILE_WRITE);
    if (!f) { playGameOverSound(); return; }
    uint32_t rowSize = (w * 3 + 3) & ~3; 
    uint32_t fileSize = 54 + (rowSize * h);
    uint8_t header[54] = { 'B','M', (uint8_t)fileSize, (uint8_t)(fileSize>>8), (uint8_t)(fileSize>>16), (uint8_t)(fileSize>>24), 0,0, 0,0, 54,0,0,0, 40,0,0,0, (uint8_t)w, (uint8_t)(w>>8), (uint8_t)(w>>16), (uint8_t)(w>>24), (uint8_t)h, (uint8_t)(h>>8), (uint8_t)(h>>16), (uint8_t)(h>>24), 1,0, 24,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 };
    f.write(header, 54);
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
void drawTopBar(const char* leftText) {
    M5.Display.fillRect(0, 0, M5.Display.width(), BAR_HEIGHT, TFT_DARKGREY);
    M5.Display.setTextColor(TFT_WHITE, TFT_DARKGREY);
    M5.Display.setTextDatum(middle_left); 
    M5.Display.setTextSize(3);
    M5.Display.drawString(leftText, 10, BAR_HEIGHT / 2);
    if (currentState == STATE_INVADER) {
        M5.Display.setTextDatum(middle_center);
        char stageText[32]; sprintf(stageText, "STAGE %d", currentStage);
        M5.Display.drawString(stageText, M5.Display.width() / 2, BAR_HEIGHT / 2);
    }
    int muteX = M5.Display.width() - 120;
    int shotX = muteX - 130;
    M5.Display.setTextDatum(middle_center); M5.Display.setTextSize(2);
    M5.Display.drawString(isMuted ? "MUTE" : "SOUND", muteX + 60, BAR_HEIGHT / 2);
    M5.Display.drawRoundRect(shotX, 10, 110, BAR_HEIGHT - 20, 5, TFT_WHITE);
    M5.Display.drawString("SS", shotX + 55, BAR_HEIGHT / 2);
    M5.Display.drawFastHLine(0, BAR_HEIGHT, M5.Display.width(), TFT_WHITE);
}

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

void drawGameUI() {
    int uiY = BAR_HEIGHT + 10;
    M5.Display.fillRect(10, uiY, 150, playerH, TFT_BLACK);
    playerSprite.pushSprite(10, uiY);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setTextDatum(middle_left); M5.Display.setTextSize(3);
    char livesText[16]; sprintf(livesText, " x %d", lives);
    M5.Display.drawString(livesText, 10 + playerW + 5, uiY + playerH / 2);
    int scoreX = M5.Display.width() - 10;
    M5.Display.fillRect(M5.Display.width() - 300, uiY, 300, 40, TFT_BLACK);
    M5.Display.setTextDatum(middle_right);
    char scoreText[32]; sprintf(scoreText, "SCORE:%05d", score);
    M5.Display.drawString(scoreText, scoreX, uiY + 20);
}

void playerMiss() {
    lives--; if (lives < 0) { isGameOver = true; playHitSound(); } 
    else {
        playHitSound(); drawGameUI();
        M5.Display.fillRect(oldPlayerX - playerW / 2, playerY, playerW, playerH, TFT_BLACK);
        playerX = M5.Display.width() / 2; oldPlayerX = playerX;
        playerSprite.pushSprite(playerX - playerW / 2, playerY);
        delay(1000); 
    }
}

/* ---------------------------------------------------------
   初期化と描画
   --------------------------------------------------------- */
void drawMenu() {
    M5.Display.fillScreen(TFT_BLACK);
    drawTopBar(" MAIN MENU");
    M5.Display.fillRoundRect(BUTTON_X, 200, BUTTON_WIDTH, BUTTON_HEIGHT, 15, TFT_BLUE);
    M5.Display.setTextColor(TFT_WHITE); M5.Display.setTextDatum(middle_center); M5.Display.setTextSize(3);
    M5.Display.drawString("INVADER", BUTTON_X + BUTTON_WIDTH / 2, 200 + BUTTON_HEIGHT / 2);
    M5.Display.fillRoundRect(BUTTON_X, 450, BUTTON_WIDTH, BUTTON_HEIGHT, 15, TFT_RED);
    M5.Display.drawString("SPIKE", BUTTON_X + BUTTON_WIDTH / 2, 450 + BUTTON_HEIGHT / 2);
}

void initInvader() {
    M5.Speaker.stop(); drawLoadingScreen("Now Loading...");
    isGameOver = false; bulletActive = false;
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) { enemyBullets[i].active = false; }
    M5.Display.fillScreen(TFT_BLACK); drawTopBar(" <- BACK");
    M5.Display.drawFastHLine(0, DEFENSE_LINE_Y, M5.Display.width(), TFT_WHITE);
    
    playerSprite.setColorDepth(16); playerSprite.createSprite(playerW, playerH);
    playerSprite.fillScreen(TFT_BLACK);
    const char* playerDot[10] = { "....WW....", "....WW....", "...WWWW...", "..WWWWWW..", ".WWWWWWWW.", "WWWWWWWWWW", ".WWWWWWWW.", "...WWWW...", "..WW..WW..", ".WW....WW." };
    for (int y = 0; y < 10; y++) { for (int x = 0; x < 10; x++) { if (playerDot[y][x] == 'W') playerSprite.fillRect(x * 5, y * 5, 5, 5, TFT_WHITE); } }
    drawGameUI();
    playerX = M5.Display.width() / 2; oldPlayerX = playerX;
    playerSprite.pushSprite(playerX - playerW / 2, playerY);
    
    const char* enemyDot[10] = { "....RR....", "...RRRR...", "..RRRRRR..", ".RR.RR.RR.", "RRRRRRRRRR", "R.RRRRRR.R", "R.R....R.R", "...RRRR...", "..R....R..", ".R......R." };
    enemySprite.setColorDepth(16); enemySprite.createSprite(ENEMY_W, ENEMY_H);
    enemySprite.fillScreen(TFT_BLACK);
    for (int y = 0; y < 10; y++) { for (int x = 0; x < 10; x++) { if (enemyDot[y][x] == 'R') enemySprite.fillRect(x * 4, y * 4, 4, 4, TFT_RED); } }
    
    activeEnemyRows = currentStage; if (activeEnemyRows > ENEMY_ROWS) activeEnemyRows = ENEMY_ROWS;
    activeEnemyCols = 4 + currentStage; if (activeEnemyCols > ENEMY_COLS) activeEnemyCols = ENEMY_COLS;
    enemiesRemaining = activeEnemyRows * activeEnemyCols;
    enemyInterval = 600 - (currentStage * 50); if (enemyInterval < 100) enemyInterval = 100; 
    enemyDir = 1; lastEnemyMove = millis();
    int startX = (M5.Display.width() - (activeEnemyCols * ENEMY_W + (activeEnemyCols - 1) * 15)) / 2;
    for (int r = 0; r < ENEMY_ROWS; r++) { for (int c = 0; c < ENEMY_COLS; c++) { if (r < activeEnemyRows && c < activeEnemyCols) { enemies[r][c].x = startX + c * (ENEMY_W + 15); enemies[r][c].y = 150 + r * (ENEMY_H + 15); enemies[r][c].alive = true; enemySprite.pushSprite(enemies[r][c].x, enemies[r][c].y); } else { enemies[r][c].alive = false; } } }
}

void initSpike() {
    M5.Speaker.stop(); 
    drawLoadingScreen("Now Loading SPIKE...");
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

    /* ---------------------------------------------------------
       SPIKEゲームの初期設定
       --------------------------------------------------------- */
    spikePlayerX = M5.Display.width() / 2;
    spikeVelocity = 0.0f;
    isGameOver = false; 
    score = 0; /* 生存時間スコアのリセット */
    
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        obstacles[i].active = false;
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
    
    playerSprite.setColorDepth(16); 
    playerSprite.createSprite(70, 50);
    playerSprite.fillScreen(TFT_BLACK);
    
    for (int y = 0; y < 10; y++) { 
        for (int x = 0; x < 10; x++) { 
            if (spikePlayerDot[y][x] == 'W') playerSprite.fillRect(x * 5 + 10, y * 5, 5, 5, TFT_WHITE); 
            else if (spikePlayerDot[y][x] == 'Y') playerSprite.fillRect(x * 5 + 10, y * 5, 5, 5, TFT_YELLOW); 
            else if (spikePlayerDot[y][x] == 'K') playerSprite.fillRect(x * 5 + 10, y * 5, 5, 5, TFT_BLACK); 
        } 
    }

    /* 【修正】トゲの長さを画面中央を少し超えるサイズに計算 */
    int spikeW = M5.Display.width() / 2 + 40; 

    if (spike0Sprite.width() != spikeW) {
        spike0Sprite.deleteSprite();
        spike0Sprite.setColorDepth(16);
        spike0Sprite.createSprite(spikeW, 95);
        spike0Sprite.fillScreen(TFT_BLACK);
        /* 右壁から突き出るトゲ（先端がX=0） */
        spike0Sprite.fillTriangle(0, 40, spikeW, 0, spikeW, 80, TFT_RED);
    }
    if (spike1Sprite.width() != spikeW) {
        spike1Sprite.deleteSprite();
        spike1Sprite.setColorDepth(16);
        spike1Sprite.createSprite(spikeW, 95);
        spike1Sprite.fillScreen(TFT_BLACK);
        /* 左壁から突き出るトゲ（先端がX=spikeW） */
        spike1Sprite.fillTriangle(spikeW, 40, 0, 0, 0, 80, TFT_RED);
    }
}

/* ---------------------------------------------------------
   メインループ
   --------------------------------------------------------- */
void MAINMENU() {
    if (wav_buffer != nullptr && !M5.Speaker.isPlaying(0)) { M5.Speaker.playWav(wav_buffer, wav_size, ~0u, 0); }
    if (M5.Touch.getCount() > 0) {
        auto t = M5.Touch.getDetail();
        if (t.wasPressed()) {
            int tx = t.x; int ty = t.y; bool isBtn = false;
            int muteX = M5.Display.width() - 120; int shotX = muteX - 130;
            if (ty <= BAR_HEIGHT) { isBtn = true; if (tx >= muteX) toggleMute(); else if (tx >= shotX && tx < muteX) takeScreenshot(); }
            else {
                if (tx >= BUTTON_X && tx <= BUTTON_X + BUTTON_WIDTH) {
                    if (ty >= 200 && ty <= 200 + BUTTON_HEIGHT) { isBtn = true; playDecisionSound(); currentStage = 1; score = 0; lives = 2; currentState = STATE_INVADER; initInvader(); }
                    else if (ty >= 450 && ty <= 450 + BUTTON_HEIGHT) { isBtn = true; playDecisionSound(); currentState = STATE_SPIKE; initSpike(); }
                }
            }
            if (!isBtn) playTapSound();
        }
    }
}

void INVADER() {
    int muteX = M5.Display.width() - 120; int shotX = muteX - 130;
    if (isGameOver) {
        if (M5.Touch.getCount() > 0) {
            auto t = M5.Touch.getDetail();
            if (t.wasPressed()) {
                int tx = t.x; int ty = t.y;
                if (ty <= BAR_HEIGHT) { if (tx >= muteX) toggleMute(); else if (tx >= shotX && tx < muteX) takeScreenshot(); else { playDecisionSound(); currentState = STATE_MENU; drawMenu(); } }
                else { playDecisionSound(); currentStage = 1; score = 0; lives = 2; initInvader(); }
            }
        }
        return; 
    }
    if (enemiesRemaining <= 0) { currentStage++; playerSprite.deleteSprite(); enemySprite.deleteSprite(); initInvader(); return; }
    
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if (enemyBullets[i].active) {
            M5.Display.fillRect(enemyBullets[i].x - enemyBulletW / 2, enemyBullets[i].y, enemyBulletW, enemyBulletH, TFT_BLACK);
            enemyBullets[i].y += enemyBulletSpeed;
            if (enemyBullets[i].y > M5.Display.height()) { enemyBullets[i].active = false; }
            else {
                if (enemyBullets[i].x >= playerX - playerW / 2 && enemyBullets[i].x <= playerX + playerW / 2 && enemyBullets[i].y + enemyBulletH >= playerY && enemyBullets[i].y <= playerY + playerH) {
                    playerMiss(); if (isGameOver) { playerSprite.deleteSprite(); enemySprite.deleteSprite(); M5.Display.fillRect(0, BAR_HEIGHT + 1, M5.Display.width(), M5.Display.height() - BAR_HEIGHT - 1, TFT_BLACK); M5.Display.setTextColor(TFT_RED, TFT_BLACK); M5.Display.setTextDatum(middle_center); M5.Display.setTextSize(5); M5.Display.drawString("GAME OVER", M5.Display.width() / 2, M5.Display.height() / 2 - 60); M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK); M5.Display.setTextSize(3); char resultText[32]; sprintf(resultText, "SCORE: %05d", score); M5.Display.drawString(resultText, M5.Display.width() / 2, M5.Display.height() / 2 + 10); M5.Display.setTextColor(TFT_WHITE, TFT_BLACK); M5.Display.setTextSize(2); M5.Display.drawString("Tap to RESTART", M5.Display.width() / 2, M5.Display.height() / 2 + 80); playGameOverSound(); return; }
                }
                if (enemyBullets[i].active) { M5.Display.fillRect(enemyBullets[i].x - enemyBulletW / 2, enemyBullets[i].y, enemyBulletW, enemyBulletH, TFT_RED); }
            }
        }
    }
    
    if (bulletActive) {
        int oldBulletY = bulletY; M5.Display.fillRect(bulletX - bulletW / 2, bulletY, bulletW, bulletH, TFT_BLACK); bulletY -= bulletSpeed;
        if (bulletX < 180 && oldBulletY < BAR_HEIGHT + 10 + playerH + bulletH) { drawGameUI(); }
        if (bulletX > M5.Display.width() - 320 && oldBulletY < BAR_HEIGHT + 10 + 40 + bulletH) { drawGameUI(); }
        if (bulletY < BAR_HEIGHT) { bulletActive = false; }
        else {
            bool hasHit = false;
            for (int r = 0; r < ENEMY_ROWS; r++) { for (int c = 0; c < ENEMY_COLS; c++) { if (enemies[r][c].alive) { if (bulletX >= enemies[r][c].x && bulletX <= enemies[r][c].x + ENEMY_W && bulletY >= enemies[r][c].y && bulletY <= enemies[r][c].y + ENEMY_H) { enemies[r][c].alive = false; enemiesRemaining--; bulletActive = false; hasHit = true; score += 100; drawGameUI(); playHitSound(); M5.Display.fillRect(enemies[r][c].x, enemies[r][c].y, ENEMY_W, ENEMY_H, TFT_BLACK); break; } } } if (hasHit) break; }
            if (bulletActive) M5.Display.fillRect(bulletX - bulletW / 2, bulletY, bulletW, bulletH, TFT_WHITE);
        }
    }
    
    if (millis() - lastEnemyMove > enemyInterval) {
        lastEnemyMove = millis(); playEnemyMoveSound(); 
        for (int c = 0; c < activeEnemyCols; c++) { for (int r = activeEnemyRows - 1; r >= 0; r--) { if (enemies[r][c].alive) { if (random(0, 100) < 3) { for (int i = 0; i < MAX_ENEMY_BULLETS; i++) { if (!enemyBullets[i].active) { enemyBullets[i].active = true; enemyBullets[i].x = enemies[r][c].x + ENEMY_W / 2; enemyBullets[i].y = enemies[r][c].y + ENEMY_H; playEnemyShootSound(); break; } } } break; } } }
        bool hitEdge = false;
        for (int r = 0; r < ENEMY_ROWS; r++) { for (int c = 0; c < ENEMY_COLS; c++) { if (enemies[r][c].alive) { int nextX = enemies[r][c].x + (enemyDir * enemySpeed); if (nextX < 10 || nextX + ENEMY_W > M5.Display.width() - 10) hitEdge = true; } } }
        for (int r = 0; r < ENEMY_ROWS; r++) { for (int c = 0; c < ENEMY_COLS; c++) { if (enemies[r][c].alive) M5.Display.fillRect(enemies[r][c].x, enemies[r][c].y, ENEMY_W, ENEMY_H, TFT_BLACK); } }
        if (hitEdge) { enemyDir *= -1; for (int r = 0; r < ENEMY_ROWS; r++) { for (int c = 0; c < ENEMY_COLS; c++) { if (enemies[r][c].alive) enemies[r][c].y += 30; } } }
        else { for (int r = 0; r < ENEMY_ROWS; r++) { for (int c = 0; c < ENEMY_COLS; c++) { if (enemies[r][c].alive) enemies[r][c].x += (enemyDir * enemySpeed); } } }
        for (int r = 0; r < ENEMY_ROWS; r++) { for (int c = 0; c < ENEMY_COLS; c++) { if (enemies[r][c].alive) enemySprite.pushSprite(enemies[r][c].x, enemies[r][c].y); } }
        M5.Display.drawFastHLine(0, DEFENSE_LINE_Y, M5.Display.width(), TFT_WHITE);
    }
    
    if (M5.Touch.getCount() > 0) {
        auto t = M5.Touch.getDetail(); int tx = t.x; int ty = t.y;
        if (ty > BAR_HEIGHT) {
            if (t.wasPressed()) { touchStartTime = millis(); isMovingMode = false; }
            if (t.isPressed() && millis() - touchStartTime >= 200) { isMovingMode = true; if (tx < playerX - 10) playerX -= playerSpeed; else if (tx > playerX + 10) playerX += playerSpeed; if (playerX < playerW / 2) playerX = playerW / 2; if (playerX > M5.Display.width() - playerW / 2) playerX = M5.Display.width() - playerW / 2; }
            if (t.wasReleased() && millis() - touchStartTime < 200 && !bulletActive) { bulletActive = true; bulletX = playerX; bulletY = playerY - 10; playShootSound(); }
        }
        if (t.wasPressed() && ty <= BAR_HEIGHT) { if (tx >= muteX) toggleMute(); else if (tx >= shotX && tx < muteX) takeScreenshot(); else { playDecisionSound(); currentState = STATE_MENU; drawMenu(); } }
    }
    if (playerX != oldPlayerX) { M5.Display.fillRect(oldPlayerX - playerW / 2, playerY, playerW, playerH, TFT_BLACK); playerSprite.pushSprite(playerX - playerW / 2, playerY); oldPlayerX = playerX; }
}

void SPIKE() {
    if (isGameOver) {
        if (M5.Touch.getCount() > 0) {
            auto t = M5.Touch.getDetail();
            if (t.wasPressed()) {
                int tx = t.x; int ty = t.y;
                int muteX = M5.Display.width() - 120; int shotX = muteX - 130;
                if (ty <= BAR_HEIGHT) { 
                    if (tx >= muteX) toggleMute(); 
                    else if (tx >= shotX && tx < muteX) takeScreenshot(); 
                    else { playDecisionSound(); currentState = STATE_MENU; drawMenu(); } 
                } else { 
                    playDecisionSound(); 
                    initSpike(); 
                }
            }
        }
        return; 
    }

    bool isUI = false;
    if (M5.Touch.getCount() > 0) {
        auto t = M5.Touch.getDetail(); int tx = t.x; int ty = t.y;
        int muteX = M5.Display.width() - 120; int shotX = muteX - 130;
        if (ty <= BAR_HEIGHT) { isUI = true; if (t.wasPressed()) { if (tx >= muteX) toggleMute(); else if (tx >= shotX && tx < muteX) takeScreenshot(); else { playDecisionSound(); currentState = STATE_MENU; drawMenu(); return; } } }
    }
    
    if (M5.Touch.getCount() > 0 && !isUI) spikeVelocity += 0.8f; else spikeVelocity -= 0.8f;
    if (spikeVelocity > 10.0f) spikeVelocity = 10.0f; if (spikeVelocity < -10.0f) spikeVelocity = -10.0f;
    spikePlayerX += spikeVelocity;
    if (spikePlayerX < playerW / 2) { spikePlayerX = playerW / 2; spikeVelocity = 0; }
    if (spikePlayerX > M5.Display.width() - playerW / 2) { spikePlayerX = M5.Display.width() - playerW / 2; spikeVelocity = 0; }

    /* 【新規】スコア（生存時間）を加算 */
    score++;
    
    /* スコアに応じて発生間隔をどんどん狭くしていく */
    int minDistance = 400 - (score / 15);
    if (minDistance < 180) minDistance = 180; /* 最低でもこのスキマは確保 */
    
    /* スコアに応じてトゲの速度を少しずつアップ */
    float currentSpikeSpeed = 10.0f + (score / 500.0f);
    if (currentSpikeSpeed > 25.0f) currentSpikeSpeed = 25.0f; /* 限界速度 */

    /* スコアに応じてトゲの発生確率もアップ */
    int spawnRate = 5 + (score / 300);
    if (spawnRate > 20) spawnRate = 20;

    bool canSpawn = true;
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (obstacles[i].active && obstacles[i].y > M5.Display.height() - minDistance) {
            canSpawn = false; 
            break;
        }
    }

    if (canSpawn && random(0, 100) < spawnRate) {
        for (int i = 0; i < MAX_OBSTACLES; i++) {
            if (!obstacles[i].active) {
                obstacles[i].active = true;
                obstacles[i].type = random(0, 2);
                obstacles[i].y = M5.Display.height() + 50;
                obstacles[i].speed = currentSpikeSpeed; /* 時間経過で上がったスピードをセット */
                break;
            }
        }
    }

    int spikeW = M5.Display.width() / 2 + 40; 
    bool hit = false;
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        if (obstacles[i].active && obstacles[i].y >= BAR_HEIGHT) {
            float ox = (obstacles[i].type == 0) ? M5.Display.width() - spikeW : spikeW;
            float oy = obstacles[i].y;
            /* 厳密な当たり判定（スレスレを狙えるように少し甘め） */
            if (spikePlayerY > oy - 35 && spikePlayerY < oy + 35) {
                if (obstacles[i].type == 0) { 
                    /* 右からのトゲ */
                    if (spikePlayerX + 15 > ox) hit = true;
                } else { 
                    /* 左からのトゲ */
                    if (spikePlayerX - 15 < ox) hit = true;
                }
            }
        }
    }

    if (hit) {
        isGameOver = true;
        playHitSound();
        M5.Display.clearClipRect(); 
        M5.Display.fillRect(0, BAR_HEIGHT + 1, M5.Display.width(), M5.Display.height() - BAR_HEIGHT - 1, TFT_BLACK); 
        M5.Display.setTextColor(TFT_RED, TFT_BLACK); 
        M5.Display.setTextDatum(middle_center); 
        M5.Display.setTextSize(5); 
        M5.Display.drawString("GAME OVER", M5.Display.width() / 2, M5.Display.height() / 2 - 40); 
        M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK); 
        M5.Display.setTextSize(3); 
        char resultText[32]; 
        sprintf(resultText, "SCORE: %05d", score); 
        M5.Display.drawString(resultText, M5.Display.width() / 2, M5.Display.height() / 2 + 10); 
        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK); 
        M5.Display.setTextSize(2); 
        M5.Display.drawString("Tap to RESTART", M5.Display.width() / 2, M5.Display.height() / 2 + 60); 
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
            } else {
                if (obstacles[i].type == 0) {
                    spike0Sprite.pushSprite(M5.Display.width() - spikeW, (int)obstacles[i].y - 40);
                } else {
                    spike1Sprite.pushSprite(0, (int)obstacles[i].y - 40);
                }
            }
        }
    }

    playerSprite.pushSprite((int)spikePlayerX - 35, (int)spikePlayerY - 25);
    
    M5.Display.clearClipRect();
    M5.Display.endWrite();
}

void setup() {
    auto cfg = M5.config(); M5.begin(cfg); M5.Display.setRotation(0); M5.Speaker.setVolume(currentVolume);
    drawLoadingScreen("Now Loading...");
    if (SD.begin()) { wav_buffer = loadWavFile("/BGM_Menu.wav", &wav_size); wav_laser = loadWavFile("/SE_INVADE_Laser.wav", &size_laser); wav_blast = loadWavFile("/SE_INVADE_Blast.wav", &size_blast); wav_move  = loadWavFile("/SE_INVADE_Move.wav",  &size_move); }
    randomSeed(millis());
    drawMenu();
}

void loop() {
    M5.update();
    switch (currentState) {
        case STATE_MENU: MAINMENU(); break;
        case STATE_INVADER: INVADER(); break;
        case STATE_SPIKE: SPIKE(); break;
    }
    delay(10);
}