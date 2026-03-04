#include "son_tmp_mode_flash.h"
#include "son_tmp_excute_mission.h"
#include "../../core/logging/son_tmp_piclog.h"
#include "../../core/storage/son_tmp_smf.h"
#include "../../../lib/tool/mmj_smf_memorymap.h"


// ============================================================================
// SMF（CPLD経由）転送シーケンス
// ============================================================================

void prepare_smf_transfer(void)
{
    fprintf(PC, "--- SMF Transfer Prepare ---\r\n");

    // 準備開始のログ記録 (イベントID: 0x20, パラメータ: 0x00 と仮定)
    piclog_make(0x20, 0x00);

    // 状態を COPYING に遷移 (excute_mission.c 側でも遷移済みだが念のため)
    //status = COPYING;

    // TODO: cigs_smf.c 側の転送準備処理（転送先頭アドレスの計算やキューの準備）を呼び出す
    // cigs_smf_prepare();
    cigs_smf_prepare();
}

void execute_smf_transfer(void)
{
    fprintf(PC, "--- SMF Transfer Execute ---\r\n");

    // 転送開始のログ記録 (イベントID: 0x21, パラメータ: 0x00 と仮定)
    piclog_make(0x21, 0x00);

    cigs_smf_copy();
}

void permit_smf_transfer(void)
{
    fprintf(PC, "--- SMF Transfer Permit ---\r\n");

    // 許可のログ記録 (イベントID: 0x22, パラメータ: 0x00 と仮定)
    piclog_make(0x22, 0x00);

    cigs_smf_permit();

    // 一連の転送シーケンスが完了したため、システム状態を IDLE に戻す
    status = IDLE;

    fprintf(PC, "--- SMF Transfer Sequence Complete ---\r\n");
}

// ============================================================================
// Flash保存データ全読み出し (デバッグ用)
// ============================================================================
void execute_flash_dump(void)
{
    unsigned int8 read_buffer[PACKET_SIZE];
    unsigned int32 flash_addr;
    unsigned int16 p_num;
    unsigned int16 i;
    unsigned int32 r_len;

    // 型不一致(Warning 240)と構文エラー(Error 102)を避けるため
    // 引数用の型に合わせたポインタとサイズ変数を用意します
    int8 *p_dst;
    p_dst = (int8 *)read_buffer;
    r_len = (unsigned int32)PACKET_SIZE;

    fprintf(PC, "\r\n--- Flash Data Dump Start ---\r\n");

    for (p_num = 1; p_num <= 32; p_num++)
    {
        // 1. アドレス計算 (32bit)
        flash_addr = (unsigned int32)(p_num - 1);
        flash_addr *= r_len;

        // 2. 指摘に基づき4引数で呼び出し (ストリーム: FLASH)
        // 引数内でのキャストを避けることで Error 102 を回避します
        read_data_bytes(FLASH, flash_addr, p_dst, r_len);

        // 3. %LX を使わずに16bitずつ分割表示することで Error 114 を確実に回避
        unsigned int16 addr_h = (unsigned int16)(flash_addr >> 16);
        unsigned int16 addr_l = (unsigned int16)(flash_addr & 0xFFFF);

        fprintf(PC, "Packet [%u] ", p_num);
        fprintf(PC, "Addr: 0x%04X", addr_h);
        fprintf(PC, "%04X | ", addr_l);

        for (i = 0; i < (unsigned int16)r_len; i++)
        {
            // 既に成功実績のある %02X を使用
            fprintf(PC, "%02X ", read_buffer[i]);

            // 16バイトごとに改行
            if (((i + 1) % 16 == 0) && (i < ((unsigned int16)r_len - 1)))
            {
                fprintf(PC, "\r\n                         | ");
            }
        }
        fprintf(PC, "\r\n------------------------------------------------------------\r\n");

        // 大量のデータ送信でシリアルが飽和しないようにウェイトを置く
        delay_ms(10);
    }

    fprintf(PC, "--- Flash Data Dump Complete ---\r\n");
}