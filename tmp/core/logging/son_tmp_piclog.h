#ifndef SON_TMP_PICLOG_H
#define SON_TMP_PICLOG_H

#include <stdint.h>
#include <string.h>

// ============================================================================
// PICLOG 定数定義
// ============================================================================
#define PICLOG_STARTUP     0x00
#define PICLOG_PARAM_START 0x00
#define PICLOG_PARAM_END   0xFF

#define PICLOG_PACKET_SIZE 6
#define PICLOG_BLANK_SIZE  4

// ============================================================================
// 外部公開関数プロトタイプ
// ============================================================================

// ログを作成し、フラッシュメモリに書き込む
void piclog_make(uint8_t function, uint8_t parameter);

#endif // SON_TMP_PICLOG_H