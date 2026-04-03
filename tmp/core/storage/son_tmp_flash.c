#include "son_tmp_flash.h"
#include "../../../lib/device/mt25q.h"
#include "../../../lib/tool/calc_tools.h"

extern void erase_subsector(Flash flash_stream, uint32_t erase_address);

// ============================================================================
// データID定数の定義 (変数名との衝突を防ぐため ID_ を付与)
// ※もし外部共通ヘッダで定義する場合はここは削除してください
// ============================================================================
#define ID_DATA_TABLE   0x00
#define ID_PICLOG_DATA  0x01
#define ID_STR_DATA     0x02

// ============================================================================
// グローバル変数の実体定義
// ============================================================================
Flash_t piclog_data = {ID_PICLOG_DATA, 0, 0, 0, 0};
Flash_t str_data    = {ID_STR_DATA, 0, 0, 0, 0};

// ----------------------------------------------------
// Flashのアドレス管理領域(Data Table)を書き込む
// ----------------------------------------------------
void write_misf_address_area()
{
    uint8_t data_table[PACKET_SIZE];
    memset(data_table, 0, PACKET_SIZE);

    memcpy(&data_table[0],               &piclog_data, sizeof(Flash_t));
    memcpy(&data_table[sizeof(Flash_t)], &str_data,    sizeof(Flash_t));

    data_table[PACKET_SIZE - 1] = calc_crc8(data_table, PACKET_SIZE - 1);

    // ★修正: subsector_4kByte_erase ではなく erase_subsector を使用する
    subsector_4kByte_erase(mis_fm, MISF_TMP_DATA_TABLE_START);
    write_data_bytes(mis_fm, MISF_TMP_DATA_TABLE_START, (int8*)data_table, PACKET_SIZE);
}

// ----------------------------------------------------
// Flash管理領域の初期化・読み込み
// ----------------------------------------------------
void misf_init()
{
    uint8_t data_table[PACKET_SIZE];

    fprintf(PC, "MISSION FLASH Initialize\r\n");

    // 起動直後に「全ての」SPIデバイスを沈黙させる
    output_high(PIN_CS_ADC);   // LTC2452 のCSをHigh
    output_high(MIS_FM_CS);    // Flash のCSをHigh
    output_high(SMF_CS);       // SMF のCSをHigh
    delay_ms(100);

    // デバイスの接続確認
    if (is_connect(mis_fm))
    {
        fprintf(PC, "\t[MIS FM] Connected\r\n");
    }
    else
    {
        fprintf(PC, "\t[MIS FM] Not Connected\r\n");
    }

    if (is_connect(smf))
    {
        fprintf(PC, "\t[SMF] Connected\r\n");
    }
    else
    {
        fprintf(PC, "\t[SMF] Not Connected\r\n");
    }

    // Flashから管理情報を読み出す
    read_data_bytes(mis_fm, MISF_TMP_DATA_TABLE_START, (int8*)data_table, PACKET_SIZE);

    // CRCチェックを行い、正常なら変数に展開する
    if (is_crc_valid(data_table, PACKET_SIZE - 1))
    {
        memcpy(&piclog_data, &data_table[0],               sizeof(Flash_t));
        memcpy(&str_data,    &data_table[sizeof(Flash_t)], sizeof(Flash_t));

        fprintf(PC, "\tData table loaded from Flash.\r\n");
    }
    else
    {
        // 異常な場合(真っ白な場合も含む)はリセット(全ゼロ)
        fprintf(PC, "\t[WARN] MISF Data Table CRC Error or Empty! Initializing counters.\r\n");

        // 変数をゼロリセット
        piclog_data.used_counter = 0;
        piclog_data.uncopied_counter = 0;
        str_data.used_counter = 0;
        str_data.uncopied_counter = 0;
        write_misf_address_area();
    }

    fprintf(PC, "\tComplete\r\n");
}

// ----------------------------------------------------
// エリアごとの物理アドレス範囲を取得
// ----------------------------------------------------
MisfStatusStruct get_misf_status_struct(uint8_t mission_id)
{
    MisfStatusStruct mis_struct = {0, 0};

    if (mission_id == ID_PICLOG_DATA)
    {
        mis_struct.start_address = MISF_TMP_PICLOG_START;
        mis_struct.end_address   = MISF_TMP_PICLOG_END;
    }
    else if (mission_id == ID_STR_DATA)
    {
        mis_struct.start_address = MISF_TMP_STR_DATA_START;
        mis_struct.end_address   = MISF_TMP_STR_DATA_END;
    }
    return mis_struct;
}

// ----------------------------------------------------
// 未転送データの書き出し位置とサイズを取得
// ----------------------------------------------------
MisfWriteStruct get_misf_write_struct(uint8_t mission_id)
{
    MisfWriteStruct mis_write_struct = {0, 0};

    if (mission_id == ID_DATA_TABLE)
    {
        mis_write_struct.start_address = MISF_TMP_DATA_TABLE_START;
        mis_write_struct.size = MISF_TMP_DATA_TABLE_SIZE;
    }
    else if (mission_id == ID_PICLOG_DATA)
    {
        mis_write_struct.start_address = MISF_TMP_PICLOG_START + piclog_data.used_counter - piclog_data.uncopied_counter;
        mis_write_struct.size = piclog_data.uncopied_counter;
    }
    else if (mission_id == ID_STR_DATA)
    {
        mis_write_struct.start_address = MISF_TMP_STR_DATA_START + str_data.used_counter - str_data.uncopied_counter;
        mis_write_struct.size = str_data.uncopied_counter;
    }
    return mis_write_struct;
}

// ----------------------------------------------------
// カウンタ状態の出力
// ----------------------------------------------------
void print_flash_status()
{
    fprintf(PC, "--- Flash Status ---\r\n");
    fprintf(PC, "PICLOG   Used: %lu, Uncopied: %lu\r\n", piclog_data.used_counter, piclog_data.uncopied_counter);
    fprintf(PC, "STR DATA Used: %lu, Uncopied: %lu\r\n", str_data.used_counter, str_data.uncopied_counter);
    fprintf(PC, "--------------------\r\n");
}