#include "son_tmp_mode_flash.h"
#include "son_tmp_excute_mission.h"
#include "../../core/logging/son_tmp_piclog.h"
#include "../../core/storage/son_tmp_flash.h"
#include "../../core/storage/son_tmp_smf.h"
#include "../../../lib/tool/mmj_smf_memorymap.h"
#include "../../../lib/device/mt25q.h"

extern Flash mis_fm;
extern Flash smf;


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

    int8 *p_dst;
    p_dst = (int8 *)read_buffer;
    r_len = (unsigned int32)PACKET_SIZE;

    fprintf(PC, "\r\n--- Flash Data Dump Start ---\r\n");

    for (p_num = 1; p_num <= 32; p_num++)
    {
        // 1. アドレス計算
        flash_addr = (unsigned int32)(p_num - 1);
        flash_addr *= r_len;

        // 2. Error 54対策: FLASH定数ではなく、ストリーム変数 mis_fm を渡す
        read_data_bytes(mis_fm, flash_addr, p_dst, r_len);

        // 3. Error 114対策: 32bitアドレスを確実に動く 8bit x 4つ に完全に分解する
        unsigned int8 a3 = (unsigned int8)((flash_addr >> 24) & 0xFF);
        unsigned int8 a2 = (unsigned int8)((flash_addr >> 16) & 0xFF);
        unsigned int8 a1 = (unsigned int8)((flash_addr >> 8)  & 0xFF);
        unsigned int8 a0 = (unsigned int8)(flash_addr         & 0xFF);

        // パケット番号も 8bit にキャスト (1〜32なので問題なし)
        unsigned int8 p_num_8 = (unsigned int8)p_num;

        // 全て 8bit 用のフォーマット (%u, %02X) に統一して出力
        fprintf(PC, "Packet [%u] ", p_num_8);
        fprintf(PC, "Addr: 0x%02X%02X%02X%02X | ", a3, a2, a1, a0);

        for (i = 0; i < PACKET_SIZE; i++)
        {
            fprintf(PC, "%02X ", read_buffer[i]);

            if (((i + 1) % 16 == 0) && (i < (PACKET_SIZE - 1)))
            {
                fprintf(PC, "\r\n                         | ");
            }
        }
        fprintf(PC, "\r\n------------------------------------------------------------\r\n");

        delay_ms(10);
    }

    fprintf(PC, "--- Flash Data Dump Complete ---\r\n");
}

// ----------------------------------------------------
// PICF: アドレス情報の出力 (0x87)
// ----------------------------------------------------
void execute_picf_read_address(void)
{
    fprintf(PC, "\r\n[PICF] Reading Address Info...\r\n");
    print_flash_status(); // 既存のステータス出力関数を流用
}

// ----------------------------------------------------
// PICF: アドレス情報のリセット (0x8F)
// ----------------------------------------------------
void execute_picf_reset_address(void)
{
    fprintf(PC, "\r\n[PICF] Resetting Address Info...\r\n");

    // 全てのカウンタをゼロに戻す
    piclog_data.used_counter = 0;
    piclog_data.uncopied_counter = 0;
    environment_data.used_counter = 0;
    environment_data.uncopied_counter = 0;
    iv_data.used_counter = 0;
    iv_data.uncopied_counter = 0;

    // ゼロにした状態をFlashに保存
    write_misf_address_area();

    fprintf(PC, "[PICF] Address Reset Complete.\r\n");
}

// ----------------------------------------------------
// PICF: 指定アドレスからの読み出し (0x86)
// ----------------------------------------------------
void execute_picf_read(uint32_t start_address, uint16_t packet_num)
{
    uint8_t read_buffer[PACKET_SIZE];
    uint32_t current_addr = start_address;

    fprintf(PC, "\r\n--- PICF Read Data Start ---\r\n");
    fprintf(PC, "Start Addr: 0x%08LX, Packets: %lu\r\n", start_address, (uint32_t)packet_num);

    for (uint16_t p = 0; p < packet_num; p++)
    {
        // 64Byteずつ読み出し
        read_data_bytes(mis_fm, current_addr, (int8*)read_buffer, PACKET_SIZE);

        fprintf(PC, "Packet [%lu] Addr: 0x%08LX | ", (uint32_t)(p+1), current_addr);
        for (uint8_t i = 0; i < PACKET_SIZE; i++)
        {
            fprintf(PC, "%02X ", read_buffer[i]);
        }
        fprintf(PC, "\r\n");

        current_addr += PACKET_SIZE;
        delay_ms(5); // UARTバッファ溢れ防止
    }
    fprintf(PC, "--- PICF Read Complete ---\r\n");
}

// ----------------------------------------------------
// PICF: 4KBサブセクター消去 (0x82)
// ----------------------------------------------------
void execute_picf_erase_4k(uint32_t start_address, uint8_t sector_num)
{
    fprintf(PC, "\r\n--- PICF Erase 4K Subsector ---\r\n");
    fprintf(PC, "Start Addr: 0x%08LX, Sectors: %u\r\n", start_address, sector_num);

    uint32_t current_addr = start_address;
    for (uint8_t i = 0; i < sector_num; i++)
    {
        subsector_4kByte_erase(mis_fm, current_addr);
        fprintf(PC, "Erased Addr: 0x%08LX\r\n", current_addr);
        current_addr += 4096; // 次の4KB(0x1000)へ
    }
    fprintf(PC, "--- Erase Complete ---\r\n");
}

// ----------------------------------------------------
// PICF: 1セクタ(64KB)消去 (0x81)
// ----------------------------------------------------
void execute_picf_erase_1sector(uint32_t start_address, uint8_t sector_num)
{
    fprintf(PC, "\r\n--- PICF Erase 1 Sector ---\r\n");
    fprintf(PC, "Start Addr: 0x%08LX, Sectors: %u\r\n", start_address, sector_num);

    uint32_t current_addr = start_address;
    for (uint8_t i = 0; i < sector_num; i++)
    {
        // 1セクタとして4KB消去を使用 (システムの要件に合わせて変更可)
        sector_erase(mis_fm, current_addr);
        fprintf(PC, "Erased Addr: 0x%08LX\r\n", current_addr);
        current_addr += 65536;
    }
    fprintf(PC, "--- Erase Complete ---\r\n");
}

// ----------------------------------------------------
// PICF: 32KBセクター消去 (0x83)
// ----------------------------------------------------
void execute_picf_erase_32k(uint32_t start_address, uint8_t sector_num)
{
    fprintf(PC, "\r\n--- PICF Erase 32K Sector ---\r\n");
    fprintf(PC, "Start Addr: 0x%08LX, Sectors: %u\r\n", start_address, sector_num);

    uint32_t current_addr = start_address;
    for (uint8_t i = 0; i < sector_num; i++)
    {
        // mt25q.h の 32KB消去関数 (0xD8)
        subsector_32kByte_erase(mis_fm, current_addr);
        fprintf(PC, "Erased Addr: 0x%08LX\r\n", current_addr);
        current_addr += 32768; // 32KB = 0x8000
    }
    fprintf(PC, "--- Erase Complete ---\r\n");
}

// ----------------------------------------------------
// PICF: デモ用データの書き込み (0x84)
// ----------------------------------------------------
void execute_picf_write_demo(uint32_t start_address, uint16_t packet_num)
{
    uint8_t write_buffer[PACKET_SIZE];
    uint32_t current_addr = start_address;

    fprintf(PC, "\r\n--- PICF Write Demo Data Start ---\r\n");
    fprintf(PC, "Start Addr: 0x%08LX, Packets: %lu\r\n", start_address, (uint32_t)packet_num);

    // デモ用データ (例: 0xAA で埋める)
    memset(write_buffer, 0xAA, PACKET_SIZE);

    for (uint16_t p = 0; p < packet_num; p++)
    {
        // 先頭にパケット番号を入れてわかりやすくする
        write_buffer[0] = (uint8_t)(p & 0xFF);

        write_data_bytes(mis_fm, current_addr, (int8*)write_buffer, PACKET_SIZE);

        fprintf(PC, "Wrote Demo Packet [%lu] to Addr: 0x%08LX\r\n", (uint32_t)(p+1), current_addr);
        current_addr += PACKET_SIZE;
        delay_ms(5);
    }
    fprintf(PC, "--- PICF Write Complete ---\r\n");
}
// ----------------------------------------------------
// PICF: チップ全消去 (0x80)
// ----------------------------------------------------
void execute_picf_erase_all(void)
{
    fprintf(PC, "\r\n--- PICF Erase All (Full Chip 16MB) ---\r\n");

    // MT25QL128ABA は 16MB = 256個の64KBセクタ
    uint32_t current_addr = 0x00000000;
    for (uint16_t i = 0; i < 256; i++)
    {
        sector_erase(mis_fm, current_addr);

        // 進捗状況を少しずつ出力 (16セクタごと)
        if (i % 16 == 0) {
            fprintf(PC, "Erasing... %lu / 256 Sectors\r\n", i);
        }
        current_addr += 65536; // 64KB進める
    }
    fprintf(PC, "--- Erase All Complete ---\r\n");
}

// ----------------------------------------------------
// PICF: 4KBサブセクターへの書き込み (0x85)
// ----------------------------------------------------
void execute_picf_write_4k(uint32_t start_address)
{
    uint8_t write_buffer[PACKET_SIZE];
    uint32_t current_addr = start_address;

    fprintf(PC, "\r\n--- PICF Write 4K Subsector ---\r\n");
    fprintf(PC, "Start Addr: 0x%08LX\r\n", start_address);

    // テスト用のデータ (例: 0x55) でバッファを埋める
    memset(write_buffer, 0x55, PACKET_SIZE);

    // 4KB は 64バイトのパケット × 64個
    for (uint16_t p = 0; p < 64; p++)
    {
        write_buffer[0] = (uint8_t)(p & 0xFF); // 先頭にパケット番号を仕込む
        write_data_bytes(mis_fm, current_addr, (int8*)write_buffer, PACKET_SIZE);
        current_addr += PACKET_SIZE;
        delay_us(500);
    }
    fprintf(PC, "--- Write 4K Complete ---\r\n");
}

// ----------------------------------------------------
// PICF: 全消去とアドレスリセット (0x88)
// ----------------------------------------------------
void execute_picf_erase_and_reset(void)
{
    fprintf(PC, "\r\n[PICF] Erase And Reset Sequence Start\r\n");

    execute_picf_erase_all();     // 上で作った全消去を呼ぶ
    execute_picf_reset_address(); // 以前作ったカウンタゼロリセットを呼ぶ

    fprintf(PC, "[PICF] Erase And Reset Sequence Complete\r\n");
}

// ----------------------------------------------------
// PICF: エリア指定での読み出し (0x89)
// ----------------------------------------------------
void execute_picf_read_area(uint8_t area, uint8_t start_packet, uint8_t request_packet)
{
    fprintf(PC, "\r\n--- PICF Read Area ---\r\n");
    fprintf(PC, "Area ID: 0x%02X, Start Packet: %u, Request Packets: %u\r\n", area, start_packet, request_packet);

    // mmj_cigs_flash.c の機能を使って、エリアIDから物理アドレスを取得する
    MisfStatusStruct area_info = get_misf_status_struct(area);

    // エリアが未定義の場合(0が返ってきた場合)のエラー回避
    if (area_info.start_address == 0 && area_info.end_address == 0)
    {
        fprintf(PC, "[ERROR] Invalid Area ID.\r\n");
        return;
    }

    // 取得した「エリアの先頭アドレス」から、「指定されたパケット数」分だけ後ろにずらす
    uint32_t start_address = area_info.start_address + ((uint32_t)start_packet * PACKET_SIZE);

    // 既存の「指定アドレスから読み出す」関数(0x86)をそのまま使い回す！
    execute_picf_read(start_address, (uint16_t)request_packet);
}


// ----------------------------------------------------
// SMF: ダイレクトコピー (PICF -> SMF) (0x90, 0x93)
// ----------------------------------------------------
void execute_smf_direct_copy(uint32_t address, uint16_t packet_num, bool is_force)
{
    uint8_t buffer[PACKET_SIZE];
    uint32_t current_addr = address;

    fprintf(PC, "\r\n--- SMF Direct Copy (PICF -> SMF) ---\r\n");
    fprintf(PC, "Start Addr: 0x%08LX, Packets: %lu, Force: %u\r\n", address, (uint32_t)packet_num, is_force);

    for (uint16_t p = 0; p < packet_num; p++)
    {
        // 1. PICF からデータを読み出し
        read_data_bytes(mis_fm, current_addr, (int8*)buffer, PACKET_SIZE);

        // 2. SMF(MT25QL01GBBB) へデータを書き込み
        write_data_bytes(smf, current_addr, (int8*)buffer, PACKET_SIZE);

        fprintf(PC, "Copied Packet [%lu] at Addr: 0x%08LX\r\n", (uint32_t)(p+1), current_addr);

        current_addr += PACKET_SIZE;
        delay_ms(5);
    }
    fprintf(PC, "--- SMF Copy Complete ---\r\n");
}

// ----------------------------------------------------
// SMF: 指定アドレスからの読み出し (0x91, 0x94)
// ----------------------------------------------------
void execute_smf_read(uint32_t address, uint16_t packet_num, bool is_force)
{
    uint8_t read_buffer[PACKET_SIZE];
    uint32_t current_addr = address;

    fprintf(PC, "\r\n--- SMF Read Data Start ---\r\n");
    fprintf(PC, "Start Addr: 0x%08LX, Packets: %lu, Force: %u\r\n", address, (uint32_t)packet_num, is_force);

    for (uint16_t p = 0; p < packet_num; p++)
    {
        // 第一引数が smf になっているだけで、PICFのREADと処理は同じです
        read_data_bytes(smf, current_addr, (int8*)read_buffer, PACKET_SIZE);

        fprintf(PC, "Packet [%lu] Addr: 0x%08LX | ", (uint32_t)(p+1), current_addr);
        for (uint8_t i = 0; i < PACKET_SIZE; i++)
        {
            fprintf(PC, "%02X ", read_buffer[i]);
        }
        fprintf(PC, "\r\n");

        current_addr += PACKET_SIZE;
        delay_ms(5);
    }
    fprintf(PC, "--- SMF Read Complete ---\r\n");
}

// ----------------------------------------------------
// SMF: 64KBセクター消去 (0x92, 0x95)
// ----------------------------------------------------
void execute_smf_erase(uint32_t address, uint8_t sector_num, bool is_force)
{
    fprintf(PC, "\r\n--- SMF Erase 64K Sector ---\r\n");
    fprintf(PC, "Start Addr: 0x%08LX, Sectors: %u, Force: %u\r\n", address, sector_num, is_force);

    uint32_t current_addr = address;
    for (uint8_t i = 0; i < sector_num; i++)
    {
        // 64KB消去コマンド (CMD_4BYTE_SECTOR_ERASE)
        sector_erase(smf, current_addr);
        fprintf(PC, "Erased Addr: 0x%08LX\r\n", current_addr);
        current_addr += 65536; // 64KB
    }
    fprintf(PC, "--- Erase Complete ---\r\n");
}


// ----------------------------------------------------
// SMF: 強制一括消去 (0x95)
// (引数を無視し、データテーブル〜PICLOG領域を消去)
// ----------------------------------------------------
void execute_smf_erase_force(void)
{

    fprintf(PC, "\r\n--- SMF Erase Force (Fixed Area) ---\r\n");

    /*
    // SMF_MEMORY_MAPが完成後に修正
    // uint32_t start_addr =
    // uint32_t end_addr   =

    fprintf(PC, "Target: 0x%08LX to 0x%08LX\r\n", start_addr, end_addr);

    // 指定範囲を64KBセクタ(0x10000)単位で消去していく
    for (uint32_t addr = start_addr; addr < end_addr; addr += 65536)
    {
        sector_erase(smf, addr);
        fprintf(PC, "Erased Addr: 0x%08LX\r\n", addr);
    }

    fprintf(PC, "--- Erase Force Complete ---\r\n");
    */
    fprintf(PC, "\r\n--- This Command is in progress. ---\r\n");
}
