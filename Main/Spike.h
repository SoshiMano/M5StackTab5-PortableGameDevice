#ifndef SPIKE_H
#define SPIKE_H

#include "Main.h" // SystemState・LEDEffect・共通変数・共通関数はすべてMain.hで定義
                  // ※旧: Invader.h をインクルードして共通変数を借用していたが、
                  //       SPIKEはINVADERに依存すべきでないため Main.h に直接切り替え。

/* スパイク専用の関数の宣言 */
void initSpike(bool isRetry = false);
void SPIKE();

#endif // SPIKE_H
