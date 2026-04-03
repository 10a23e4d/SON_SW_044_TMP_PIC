#ifndef son_tmp_FLASH_H
#define son_tmp_FLASH_H

#include <stdint.h>
#include <string.h>
#include "../../system/son_tmp_config.h"

// 共通ライブラリのヘッダ (MissionIDなどの定数用)
// #include "../../../lib/communication/typedef_content.h"
// #include "../../../lib/communication/value_status.h"

// ============================================================================
// メモリマップ定義 (MT25QL128ABA: 16MB)
// ============================================================================
#define MISF_START 0x00000000
#define MISF_END   0x00FFFFFF

#define SECTOR_64K_BYTE  0x10000

#define MISF_TMP_DATA_TABLE_SIZE 64

#define MISF_TMP_DATA_TABLE_START 0x00000000
#define MISF_TMP_DATA_TABLE_END   0x0000FFFF
#define MISF_TMP_PICLOG_START     0x00010000
#define MISF_TMP_PICLOG_END       0x001FFFFF
#define MISF_TMP_STR_DATA_START   0x00200000
#define MISF_TMP_STR_DATA_END     0x00FFFFFF

// ============================================================================
// データ構造体定義
// ============================================================================

// フラッシュカウンタ構造体 (11 Bytes)
typedef struct
{
    uint8_t   mission_id;
    uint32_t  used_counter;
    uint32_t  uncopied_counter;
    uint8_t   reserve_counter1;
    uint8_t   reserve_counter2;
} Flash_t;

// テーブル保存用の64Byteパケット構造体
typedef union
{
    struct
    {
        union
        {
            struct
            {
                Flash_t piclog;
                Flash_t environment;
                Flash_t iv_header;
                Flash_t str_data;
            } logdata;
            uint8_t reserve[63];
        } payload;
        uint8_t crc; // 1バイトのCRC
    } packet;
    uint8_t bytes[64];
} FlashData_t;

typedef struct
{
    uint32_t start_address;
    uint32_t end_address;
} MisfStatusStruct;

typedef struct
{
    uint32_t start_address;
    uint32_t size;
} MisfWriteStruct;

// ============================================================================
// グローバル変数 (宣言のみ、実体は.cファイルに記述)
// ============================================================================
extern Flash_t piclog_data;
extern Flash_t environment_data;
extern Flash_t iv_header;
extern Flash_t str_data;

// ============================================================================
// 外部公開関数プロトタイプ
// ============================================================================
void misf_init(void);
FlashData_t make_flash_data_table(void);
void write_misf_address_area(void);
MisfStatusStruct get_misf_status_struct(uint8_t mission_id);
MisfWriteStruct get_misf_write_struct(uint8_t mission_id);
void print_flash_status(void);

#endif // son_tmp_FLASH_H