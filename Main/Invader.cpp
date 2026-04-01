#include "Invader.h"

/* ---------------------------------------------------------
   ステージ・進行管理用の変数（インベーダー用）
   --------------------------------------------------------- */
int currentStage = 1;            
int activeEnemyRows = 0;         
int activeEnemyCols = 0;         
int enemiesRemaining = 0;        

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
Enemy enemies[5][11]; 

int enemyDir = 1;                
int enemySpeed = 15;             
unsigned long lastEnemyMove = 0; 
int enemyInterval = 500;         

M5Canvas enemySprite(&M5.Display);

/* ---------------------------------------------------------
   敵の弾の設定
   --------------------------------------------------------- */
const int MAX_ENEMY_BULLETS = 3; 
struct EnemyBullet {
    int x, y;
    bool active;
};
EnemyBullet enemyBullets[3];
const int enemyBulletW = 4;
const int enemyBulletH = 15;
const int enemyBulletSpeed = 15; 

/* ---------------------------------------------------------
   UI描画・ミス処理
   --------------------------------------------------------- */
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
    lives--; 
    /* 弾を全部消去する処理を追加したよ♡ */
    bulletActive = false; 
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        enemyBullets[i].active = false;
    }

    if (lives < 0) { isGameOver = true; playHitSound(); } 
    else {
        playHitSound(); drawGameUI();
        M5.Display.fillRect(oldPlayerX - playerW / 2, playerY, playerW, playerH, TFT_BLACK);
        playerX = M5.Display.width() / 2; oldPlayerX = playerX;
        playerSprite.pushSprite(playerX - playerW / 2, playerY);
        delay(1000); 
    }
}

/* ---------------------------------------------------------
   初期化とメインループ
   --------------------------------------------------------- */
void initInvader() {
    M5.Speaker.stop(0); 
    drawLoadingScreen("Now Loading...");
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
                    playerMiss(); 
                    if (isGameOver) { 
                        M5.Speaker.stop(0); 
                        playerSprite.deleteSprite(); enemySprite.deleteSprite(); 
                        M5.Display.fillRect(0, BAR_HEIGHT + 1, M5.Display.width(), M5.Display.height() - BAR_HEIGHT - 1, TFT_BLACK); 
                        M5.Display.setTextColor(TFT_RED, TFT_BLACK); M5.Display.setTextDatum(middle_center); M5.Display.setTextSize(5); 
                        M5.Display.drawString("GAME OVER", M5.Display.width() / 2, M5.Display.height() / 2 - 60); 
                        M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK); M5.Display.setTextSize(3); char resultText[32]; sprintf(resultText, "SCORE: %05d", score); 
                        M5.Display.drawString(resultText, M5.Display.width() / 2, M5.Display.height() / 2 + 10); 
                        M5.Display.setTextColor(TFT_WHITE, TFT_BLACK); M5.Display.setTextSize(2); 
                        M5.Display.drawString("Tap to RESTART", M5.Display.width() / 2, M5.Display.height() / 2 + 80); 
                        playGameOverSound(); 
                        return; 
                    }
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