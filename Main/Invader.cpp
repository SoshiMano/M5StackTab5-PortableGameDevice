#include "Invader.h"

extern void updateGameLEDs(); // Main.inoのLED更新関数を使うための宣言を追加

/* ---------------------------------------------------------
   ステージ・進行管理用の変数（インベーダー専用）
   --------------------------------------------------------- */
int currentStage = 1;            // 現在のステージ数
int activeEnemyRows = 0;         // 出現する敵の行数
int activeEnemyCols = 0;         // 出現する敵の列数
int enemiesRemaining = 0;        // 生き残っている敵の数

/* ---------------------------------------------------------
   入力（タッチ）管理用の変数
   --------------------------------------------------------- */
unsigned long touchStartTime = 0; // タッチを開始した時間（長押し判定用）
bool isMovingMode = false;        // 移動モードかどうかのフラグ

/* ---------------------------------------------------------
   自機（プレイヤー）の設定
   --------------------------------------------------------- */
int playerX = 360;               // 自機の現在のX座標
int oldPlayerX = 360;            // 差分描画用の過去のX座標
const int playerY = 1100;        // 自機のY座標（固定）
const int playerW = 50;          // 自機の幅
const int playerH = 50;          // 自機の高さ
const int playerSpeed = 15;      // 自機の移動速度
const int DEFENSE_LINE_Y = 1000; // 画面下部の防衛線のY座標

M5Canvas playerSprite(&M5.Display); // 自機描画用のスプライト

/* ---------------------------------------------------------
   自機の弾（攻撃）の設定
   --------------------------------------------------------- */
int bulletX, bulletY;            // 自機の弾の座標
bool bulletActive = false;       // 弾が画面内に存在するかどうかのフラグ
const int bulletW = 4;           // 弾の幅
const int bulletH = 15;          // 弾の高さ
const int bulletSpeed = 25;      // 弾の移動速度

/* ---------------------------------------------------------
   敵（インベーダー）の設定
   --------------------------------------------------------- */
const int ENEMY_ROWS = 5;        // 敵の最大行数
const int ENEMY_COLS = 11;       // 敵の最大列数
const int ENEMY_W = 40;          // 敵の幅
const int ENEMY_H = 40;          // 敵の高さ

/* 敵1体ごとの状態を管理する構造体 */
struct Enemy {
    int x, y;       // 敵の座標
    bool alive;     // 生存フラグ
};
Enemy enemies[5][11]; // 敵の2次元配列

int enemyDir = 1;                // 敵の移動方向（1=右, -1=左）
int enemySpeed = 15;             // 敵の移動速度
unsigned long lastEnemyMove = 0; // 最後に敵が移動した時間（移動間隔制御用）
int enemyInterval = 500;         // 敵が移動する間隔（ミリ秒）

M5Canvas enemySprite(&M5.Display); // 敵描画用のスプライト

/* ---------------------------------------------------------
   敵の弾の設定
   --------------------------------------------------------- */
const int MAX_ENEMY_BULLETS = 3; // 画面内に同時に存在できる敵の弾の最大数
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
/* 画面下部に残機とスコアを描画する関数 */
void drawGameUI() {
    int uiY = BAR_HEIGHT + 10;
    // 残機の描画
    M5.Display.fillRect(10, uiY, 150, playerH, TFT_BLACK);
    playerSprite.pushSprite(10, uiY);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setTextDatum(middle_left); M5.Display.setTextSize(3);
    char livesText[16]; sprintf(livesText, " x %d", lives);
    M5.Display.drawString(livesText, 10 + playerW + 5, uiY + playerH / 2);
    
    // スコアの描画
    int scoreX = M5.Display.width() - 10;
    M5.Display.fillRect(M5.Display.width() - 300, uiY, 300, 40, TFT_BLACK);
    M5.Display.setTextDatum(middle_right);
    char scoreText[32]; sprintf(scoreText, "SCORE:%05d", score);
    M5.Display.drawString(scoreText, scoreX, uiY + 20);
}

/* プレイヤーが被弾した際の処理 */
void playerMiss() {
    lives--; 
    triggerLEDEffect(3); // 被弾時に赤くチカチカ(EFFECT_PLAYER_HIT)
    
    /* 1. 全ての弾の描画を画面から消去する */
    if (bulletActive) {
        M5.Display.fillRect(bulletX - bulletW / 2, bulletY, bulletW, bulletH, TFT_BLACK);
    }
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if (enemyBullets[i].active) {
            M5.Display.fillRect(enemyBullets[i].x - enemyBulletW / 2, enemyBullets[i].y, enemyBulletW, enemyBulletH, TFT_BLACK);
        }
    }

    /* 2. 判定（フラグ）を全部オフにする */
    bulletActive = false; 
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        enemyBullets[i].active = false;
    }

    // 残機が0未満ならゲームオーバー処理へ
    if (lives < 0) { 
        isGameOver = true; 
        playHitSound(); 
    } 
    else {
        // 残機がある場合はリスポーン処理
        playHitSound(); 
        drawGameUI();
        M5.Display.fillRect(oldPlayerX - playerW / 2, playerY, playerW, playerH, TFT_BLACK);
        playerX = M5.Display.width() / 2; 
        oldPlayerX = playerX;
        playerSprite.pushSprite(playerX - playerW / 2, playerY);
        
        // LEDを更新しながら1秒待機する処理
        unsigned long waitStart = millis();
        while (millis() - waitStart < 1000) {
            updateGameLEDs();
            delay(10);
        }
    }
}

/* ---------------------------------------------------------
   初期化とメインループ
   --------------------------------------------------------- */
/* インベーダーゲーム開始時の初期化処理 */
void initInvader() {
    M5.Speaker.stop(0); 
    drawLoadingScreen("Now Loading...");
    isGameOver = false; bulletActive = false;
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) { enemyBullets[i].active = false; }
    
    M5.Display.fillScreen(TFT_BLACK); 
    drawTopBar(" <- BACK");
    M5.Display.drawFastHLine(0, DEFENSE_LINE_Y, M5.Display.width(), TFT_WHITE);
    
    // 自機スプライトの作成（アスキーアート風のドット定義）
    playerSprite.setColorDepth(16); playerSprite.createSprite(playerW, playerH);
    playerSprite.fillScreen(TFT_BLACK);
    const char* playerDot[10] = { "....WW....", "....WW....", "...WWWW...", "..WWWWWW..", ".WWWWWWWW.", "WWWWWWWWWW", ".WWWWWWWW.", "...WWWW...", "..WW..WW..", ".WW....WW." };
    for (int y = 0; y < 10; y++) { for (int x = 0; x < 10; x++) { if (playerDot[y][x] == 'W') playerSprite.fillRect(x * 5, y * 5, 5, 5, TFT_WHITE); } }
    drawGameUI();
    playerX = M5.Display.width() / 2; oldPlayerX = playerX;
    playerSprite.pushSprite(playerX - playerW / 2, playerY);
    
    // 敵スプライトの作成
    const char* enemyDot[10] = { "....RR....", "...RRRR...", "..RRRRRR..", ".RR.RR.RR.", "RRRRRRRRRR", "R.RRRRRR.R", "R.R....R.R", "...RRRR...", "..R....R..", ".R......R." };
    enemySprite.setColorDepth(16); enemySprite.createSprite(ENEMY_W, ENEMY_H);
    enemySprite.fillScreen(TFT_BLACK);
    for (int y = 0; y < 10; y++) { for (int x = 0; x < 10; x++) { if (enemyDot[y][x] == 'R') enemySprite.fillRect(x * 4, y * 4, 4, 4, TFT_RED); } }
    
    // ステージに応じた敵の配置計算
    activeEnemyRows = currentStage; if (activeEnemyRows > ENEMY_ROWS) activeEnemyRows = ENEMY_ROWS;
    activeEnemyCols = 4 + currentStage; if (activeEnemyCols > ENEMY_COLS) activeEnemyCols = ENEMY_COLS;
    enemiesRemaining = activeEnemyRows * activeEnemyCols;
    
    // 敵の移動速度の上昇処理
    enemyInterval = 600 - (currentStage * 50); if (enemyInterval < 100) enemyInterval = 100; 
    enemyDir = 1; lastEnemyMove = millis();
    
    // 敵の初期配置（中央寄せ）
    int startX = (M5.Display.width() - (activeEnemyCols * ENEMY_W + (activeEnemyCols - 1) * 15)) / 2;
    for (int r = 0; r < ENEMY_ROWS; r++) { 
        for (int c = 0; c < ENEMY_COLS; c++) { 
            if (r < activeEnemyRows && c < activeEnemyCols) { 
                enemies[r][c].x = startX + c * (ENEMY_W + 15); 
                enemies[r][c].y = 150 + r * (ENEMY_H + 15); 
                enemies[r][c].alive = true; 
                enemySprite.pushSprite(enemies[r][c].x, enemies[r][c].y); 
            } else { 
                enemies[r][c].alive = false; 
            } 
        } 
    }
}

/* インベーダーゲームのメイン処理（毎フレーム呼ばれる） */
void INVADER() {
    int muteX = M5.Display.width() - 120; int shotX = muteX - 130;
    
    // ゲームオーバー時の処理（画面タップで再開またはメニューへ）
    if (isGameOver) {
        if (M5.Touch.getCount() > 0) {
            auto t = M5.Touch.getDetail();
            if (t.wasPressed()) {
                int tx = t.x; int ty = t.y;
                if (ty <= BAR_HEIGHT) { 
                    if (tx >= muteX) toggleMute(); 
                    else if (tx >= shotX && tx < muteX) takeScreenshot(); 
                    else { playDecisionSound(); currentState = STATE_MENU; drawMenu(); } 
                }
                else { playDecisionSound(); currentStage = 1; score = 0; lives = 2; initInvader(); }
            }
        }
        return; 
    }
    
    // 敵を全滅させた場合のステージクリア処理
    if (enemiesRemaining <= 0) { currentStage++; playerSprite.deleteSprite(); enemySprite.deleteSprite(); initInvader(); return; }
    
    // 敵の弾の移動と当たり判定処理
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if (enemyBullets[i].active) {
            M5.Display.fillRect(enemyBullets[i].x - enemyBulletW / 2, enemyBullets[i].y, enemyBulletW, enemyBulletH, TFT_BLACK);
            enemyBullets[i].y += enemyBulletSpeed;
            if (enemyBullets[i].y > M5.Display.height()) { 
                enemyBullets[i].active = false; 
            }
            else {
                // 自機との当たり判定
                if (enemyBullets[i].x >= playerX - playerW / 2 && enemyBullets[i].x <= playerX + playerW / 2 && enemyBullets[i].y + enemyBulletH >= playerY && enemyBullets[i].y <= playerY + playerH) {
                    playerMiss(); 
                    if (isGameOver) { 
                        // ゲームオーバー画面の描画
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
    
    // 自機の弾の移動と敵への当たり判定
    if (bulletActive) {
        int oldBulletY = bulletY; 
        M5.Display.fillRect(bulletX - bulletW / 2, bulletY, bulletW, bulletH, TFT_BLACK); 
        bulletY -= bulletSpeed;
        
        // 弾がUI領域を通過した際のUI再描画
        if (bulletX < 180 && oldBulletY < BAR_HEIGHT + 10 + playerH + bulletH) { drawGameUI(); }
        if (bulletX > M5.Display.width() - 320 && oldBulletY < BAR_HEIGHT + 10 + 40 + bulletH) { drawGameUI(); }
        
        if (bulletY < BAR_HEIGHT) { bulletActive = false; }
        else {
            bool hasHit = false;
            // 敵全員との当たり判定チェック
            for (int r = 0; r < ENEMY_ROWS; r++) { 
                for (int c = 0; c < ENEMY_COLS; c++) { 
                    if (enemies[r][c].alive) { 
                        if (bulletX >= enemies[r][c].x && bulletX <= enemies[r][c].x + ENEMY_W && bulletY >= enemies[r][c].y && bulletY <= enemies[r][c].y + ENEMY_H) { 
                            enemies[r][c].alive = false; enemiesRemaining--; bulletActive = false; hasHit = true; 
                            score += 100; drawGameUI(); playHitSound(); 
                            triggerLEDEffect(2); // 敵撃墜時に緑くチカチカ(EFFECT_ENEMY_HIT)
                            M5.Display.fillRect(enemies[r][c].x, enemies[r][c].y, ENEMY_W, ENEMY_H, TFT_BLACK); 
                            break; 
                        } 
                    } 
                } 
                if (hasHit) break; 
            }
            if (bulletActive) M5.Display.fillRect(bulletX - bulletW / 2, bulletY, bulletW, bulletH, TFT_WHITE);
        }
    }
    
    // 敵集団の移動処理
    if (millis() - lastEnemyMove > enemyInterval) {
        lastEnemyMove = millis(); playEnemyMoveSound(); 
        
        // ランダムで敵が弾を発射する処理
        for (int c = 0; c < activeEnemyCols; c++) { 
            for (int r = activeEnemyRows - 1; r >= 0; r--) { 
                if (enemies[r][c].alive) { 
                    if (random(0, 100) < 3) { 
                        for (int i = 0; i < MAX_ENEMY_BULLETS; i++) { 
                            if (!enemyBullets[i].active) { 
                                enemyBullets[i].active = true; 
                                enemyBullets[i].x = enemies[r][c].x + ENEMY_W / 2; 
                                enemyBullets[i].y = enemies[r][c].y + ENEMY_H; 
                                playEnemyShootSound(); 
                                break; 
                            } 
                        } 
                    } 
                    break; 
                } 
            } 
        }
        
        // 画面端に到達したかの判定
        bool hitEdge = false;
        for (int r = 0; r < ENEMY_ROWS; r++) { 
            for (int c = 0; c < ENEMY_COLS; c++) { 
                if (enemies[r][c].alive) { 
                    int nextX = enemies[r][c].x + (enemyDir * enemySpeed); 
                    if (nextX < 10 || nextX + ENEMY_W > M5.Display.width() - 10) hitEdge = true; 
                } 
            } 
        }
        
        // 敵の古い描画を消去
        for (int r = 0; r < ENEMY_ROWS; r++) { 
            for (int c = 0; c < ENEMY_COLS; c++) { 
                if (enemies[r][c].alive) M5.Display.fillRect(enemies[r][c].x, enemies[r][c].y, ENEMY_W, ENEMY_H, TFT_BLACK); 
            } 
        }
        
        // 端に到達していたら進行方向を反転し、一段下がる
        if (hitEdge) { 
            enemyDir *= -1; 
            for (int r = 0; r < ENEMY_ROWS; r++) { 
                for (int c = 0; c < ENEMY_COLS; c++) { 
                    if (enemies[r][c].alive) enemies[r][c].y += 30; 
                } 
            } 
        }
        else { 
            // 端でなければ横に移動
            for (int r = 0; r < ENEMY_ROWS; r++) { 
                for (int c = 0; c < ENEMY_COLS; c++) { 
                    if (enemies[r][c].alive) enemies[r][c].x += (enemyDir * enemySpeed); 
                } 
            } 
        }
        
        // 新しい座標に敵を描画
        for (int r = 0; r < ENEMY_ROWS; r++) { 
            for (int c = 0; c < ENEMY_COLS; c++) { 
                if (enemies[r][c].alive) enemySprite.pushSprite(enemies[r][c].x, enemies[r][c].y); 
            } 
        }
        M5.Display.drawFastHLine(0, DEFENSE_LINE_Y, M5.Display.width(), TFT_WHITE);
    }
    
    // タッチによる自機の操作処理
    if (M5.Touch.getCount() > 0) {
        auto t = M5.Touch.getDetail(); int tx = t.x; int ty = t.y;
        if (ty > BAR_HEIGHT) {
            // タッチ開始時
            if (t.wasPressed()) { touchStartTime = millis(); isMovingMode = false; }
            // 長押し中（スライドで移動）
            if (t.isPressed() && millis() - touchStartTime >= 200) { 
                isMovingMode = true; 
                if (tx < playerX - 10) playerX -= playerSpeed; 
                else if (tx > playerX + 10) playerX += playerSpeed; 
                if (playerX < playerW / 2) playerX = playerW / 2; 
                if (playerX > M5.Display.width() - playerW / 2) playerX = M5.Display.width() - playerW / 2; 
            }
            // 短いタップ（リリース時に弾を発射）
            if (t.wasReleased() && millis() - touchStartTime < 200 && !bulletActive) { 
                bulletActive = true; bulletX = playerX; bulletY = playerY - 10; playShootSound(); 
                triggerLEDEffect(1); // 弾発射時に白く一瞬光る(EFFECT_SHOOT)
            }
        }
        // トップバーの操作判定
        if (t.wasPressed() && ty <= BAR_HEIGHT) { 
            if (tx >= muteX) toggleMute(); 
            else if (tx >= shotX && tx < muteX) takeScreenshot(); 
            else { playDecisionSound(); currentState = STATE_MENU; drawMenu(); } 
        }
    }
    
    // 自機が移動した際のスプライト再描画
    if (playerX != oldPlayerX) { 
        M5.Display.fillRect(oldPlayerX - playerW / 2, playerY, playerW, playerH, TFT_BLACK); 
        playerSprite.pushSprite(playerX - playerW / 2, playerY); 
        oldPlayerX = playerX; 
    }
}