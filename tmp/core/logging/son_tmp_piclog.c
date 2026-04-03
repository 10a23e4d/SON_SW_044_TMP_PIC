#include "son_tmp_piclog.h"

// 必要な外部モジュールのインクルード
#include "../../hardware/mcu/timer.h"
#include "../storage/son_tmp_flash.h"
#include "../../../lib/device/mt25q.h"

// ============================================================================
// グローバル変数
// ============================================================================
// 64Byte境界合わせのための4Byteパディング用データ
uint8_t PICLOG_BLANK_DATA[PICLOG_BLANK_SIZE] = {0x00, 0x00, 0x00, 0x00};

// ============================================================================
// ログ作成とフラッシュ保存処理
// ============================================================================
void piclog_make(uint8_t function, uint8_t parameter)
{
    uint8_t piclog_bytes[PICLOG_PACKET_SIZE];
    memset(piclog_bytes, 0, PICLOG_PACKET_SIZE);

    // データの設定 (エンディアンのズレを防ぐため、手動で1バイトずつ詰める)
    uint32_t current_time = get_current_sec();
    piclog_bytes[0] = (uint8_t)((current_time >> 24) & 0xFF);
    piclog_bytes[1] = (uint8_t)((current_time >> 16) & 0xFF);
    piclog_bytes[2] = (uint8_t)((current_time >> 8) & 0xFF);
    piclog_bytes[3] = (uint8_t)(current_time & 0xFF);

    piclog_bytes[4] = function;
    piclog_bytes[5] = parameter;

    // UARTへログを出力
    fprintf(PC, "\t[PICLOG] : ");
    for (uint8_t i = 0; i < PICLOG_PACKET_SIZE; i++)
    {
        fprintf(PC, "%02X ", piclog_bytes[i]);
    }
    fprintf(PC, "\r\n");

    // 1. Flashへ書き込み (6 Byte)
    uint32_t write_address = MISF_TMP_PICLOG_START + piclog_data.used_counter;
    write_data_bytes(mis_fm, write_address, piclog_bytes, PICLOG_PACKET_SIZE);

    // カウンタの更新
    piclog_data.used_counter += PICLOG_PACKET_SIZE;
    piclog_data.uncopied_counter += PICLOG_PACKET_SIZE;
    piclog_data.reserve_counter1 += PICLOG_PACKET_SIZE;

    // 2. ページ境界(64Byte)の管理
    // 次に6Byteを書き込むと64Byteを超える場合(現在60Byte書き込み済みの場合)は、残りの4Byteをゼロで埋める
    if (piclog_data.reserve_counter1 + PICLOG_PACKET_SIZE > PACKET_SIZE)
    {
        write_address = MISF_TMP_PICLOG_START + piclog_data.used_counter;
        write_data_bytes(mis_fm, write_address, PICLOG_BLANK_DATA, PICLOG_BLANK_SIZE);

        // パディング分のカウンタ更新
        piclog_data.used_counter += PICLOG_BLANK_SIZE;
        piclog_data.uncopied_counter += PICLOG_BLANK_SIZE;
        piclog_data.reserve_counter1 = 0; // 次の64Byteブロックのためにリセット
    }

    // フラッシュのアドレス管理領域を更新
    write_misf_address_area();
}