#include "son_tmp_strain.h"
#include "son_tmp_temp.h" // 温度計測モジュールをインクルード

// 必要な外部モジュールのインクルード
#include "../../hardware/mcu/timer.h"
#include "../storage/son_tmp_flash.h"
#include "../../../lib/tool/calc_tools.h"
#include "../../../lib/device/mt25q.h"

// ============================================================================
// ハードウェア・ペリフェラル制御
// ============================================================================

void io_init()
{
    fprintf(PC, "IO Initialize\r\n");

    // SPI Chip Select の初期化 (Highで非選択)
    output_high(PIN_CS_ADC);
    output_high(MIS_FM_CS);
    output_high(SMF_CS);

    // 電源・制御ピンの初期化 (LowでOFF)
    output_low(PIN_LDO_EN);
    output_low(PIN_VREF_EN);

    // アナログスイッチの初期化
    output_low(PIN_SW_DIO0);
    output_low(PIN_SW_DIO1);

    // テレメトリLEDの初期化
    output_low(PIN_LED1);
    output_low(PIN_LED2);

    // 内蔵ADC(温度センサ用)の初期化もここで呼び出す
    temp_io_init();

    delay_ms(1);
    fprintf(PC, "\tComplete\r\n");
}

void switch_channel(uint8_t ch)
{
    // MAX4734EUB+ は 2bit (DIO1, DIO0) で 4ch を切り替える
    if (ch & 0x01)
    {
        output_high(PIN_SW_DIO0);
    }
    else
    {
        output_low(PIN_SW_DIO0);
    }

    if (ch & 0x02)
    {
        output_high(PIN_SW_DIO1);
    }
    else
    {
        output_low(PIN_SW_DIO1);
    }

    // スイッチ切り替え後の安定待ち
    delay_us(10);
}

uint16_t read_adc_ltc2452()
{
    uint16_t adc_val = 0;

    output_low(PIN_CS_ADC);

    // LTC2452は16bitデータを出力 (MSBファースト)
    adc_val = spi_xfer(MIS_FM_STREAM, 0x00);                  // 上位8bit
    adc_val = (adc_val << 8) | spi_xfer(MIS_FM_STREAM, 0x00); // 下位8bit

    output_high(PIN_CS_ADC);

    return adc_val;
}

// ============================================================================
// 計測シーケンスとFlash保存処理
// ============================================================================

void execute_measurement(uint8_t mode, uint8_t hw_channel, uint8_t samplingRate)
{
    fprintf(PC, "Start Synchronized Measurement (Mode:0x%02X, HW_Ch:%u)\r\n", mode, hw_channel);

    temp_io_init(); // 内蔵ADCの初期化 (温度計測用)
    delay_ms(10);

    // ==========================================
    // 0. 測定開始時に電源・VREF・LEDをONにする
    // ==========================================
    output_high(PIN_VREF_EN); // RE0 ON
    output_high(PIN_LDO_EN);  // RE1 ON
    output_high(PIN_LED2);    // RA1 ON
    output_high(PIN_LED1);    // RA5 ON

    // ========================================================
    // 1. バッファとパケット管理変数の初期化
    // ========================================================
    uint16_t data_count = 555; // 測定回数 (Data Count 12bit用)
    uint16_t packet_num = 0;   // パケット番号
    uint8_t packet_buffer[PACKET_SIZE];
    uint8_t packet_idx = 0;    // バッファの書き込み位置
    uint8_t block_count = 0;   // 現在のパケットに詰めた6Byte塊の数
    uint8_t data_num = 0;      // 温度データ用のデータ番号 (0〜15)

    memset(packet_buffer, 0, PACKET_SIZE);

    // パケット0のヘッダー作成 (7 Byte)
    uint32_t ts = get_current_sec();
    packet_buffer[0] = (ts >> 24) & 0xFF;
    packet_buffer[1] = (ts >> 16) & 0xFF;
    packet_buffer[2] = (ts >> 8) & 0xFF;
    packet_buffer[3] = ts & 0xFF;
    // Mis Mode(3bit) | SW Ch(1bit) | Sampling Rate(4bit)
    packet_buffer[4] = ((mode & 0x07) << 5) | ((hw_channel & 0x01) << 4) | (samplingRate & 0x0F);
    // Data Count(12bit) の上位8bit
    packet_buffer[5] = (data_count >> 4) & 0xFF;
    // Data Countの下位4bit | 0Fill(4bit)
    packet_buffer[6] = ((data_count & 0x0F) << 4) | 0x00;

    packet_idx = 7; // データ塊の書き込み開始位置

    // ========================================================
    // 2. メインサンプリングループ
    // ========================================================
    for (uint16_t step = 0; step < data_count; step++)
    {
        // ----------------------------------------------------
        // [A] データの読み取り
        // ----------------------------------------------------
        uint16_t temp_val = read_adc_internal();

        uint16_t strain1 = read_adc_ltc2452();

        // アナログスイッチを対象チャンネルに切り替える
        switch_channel(hw_channel);
        delay_us(10); // セトリングタイム

        uint16_t strain2 = read_adc_ltc2452();

        // ★分岐: STR_PRINT (0x03) の場合は、生データを直接シリアル出力する
        if (mode == 0x03)
        {
            fprintf(PC, "[PRINT] Step:%lu, Temp:0x%03X, STR1:0x%04X, STR2:0x%04X\r\n",
                    step, temp_val, strain1, strain2);
        }

        // ----------------------------------------------------
        // [B] 6Byteデータ塊のパッキング
        // ----------------------------------------------------
        // 温度データ (データ番号:4bit | 温度データ:12bit)
        packet_buffer[packet_idx++] = ((data_num & 0x0F) << 4) | ((temp_val >> 8) & 0x0F);
        packet_buffer[packet_idx++] = temp_val & 0xFF;

        // ひずみデータ1 (2Byte)
        packet_buffer[packet_idx++] = (strain1 >> 8) & 0xFF;
        packet_buffer[packet_idx++] = strain1 & 0xFF;

        // ひずみデータ2 (2Byte)
        packet_buffer[packet_idx++] = (strain2 >> 8) & 0xFF;
        packet_buffer[packet_idx++] = strain2 & 0xFF;

        data_num = (data_num + 1) & 0x0F; // 0〜15でループ
        block_count++;

        // ----------------------------------------------------
        // [C] 満杯判定と保存/出力処理
        // パケット0は9個、パケット1以降は10個で満杯
        // ----------------------------------------------------
        uint8_t max_blocks = (packet_num == 0) ? 9 : 10;

        if (block_count >= max_blocks)
        {
            // CRC-24 を計算して末尾の3バイトに格納
            uint32_t crc24 = calc_crc24(packet_buffer, 61);
            packet_buffer[61] = (crc24 >> 16) & 0xFF;
            packet_buffer[62] = (crc24 >> 8) & 0xFF;
            packet_buffer[63] = crc24 & 0xFF;

            // ★分岐: modeに応じた処理
            if (mode == 0x01)
            {
                // STR (通常ミッション): Flashへ書き込み
                uint32_t flash_addr = MISF_TMP_STR_DATA_START + str_data.used_counter;
                write_data_bytes(mis_fm, flash_addr, (int8*)packet_buffer, PACKET_SIZE);

                str_data.used_counter += PACKET_SIZE;
                str_data.uncopied_counter += PACKET_SIZE;
            }
            else if (mode == 0x02)
            {
                // STR_DEBUG: バッファの内容をシリアルにダンプ出力 (Flashには書かない)
                fprintf(PC, "[DEBUG] Packet %lu: ", packet_num);
                for (uint8_t i = 0; i < PACKET_SIZE; i++)
                {
                    fprintf(PC, "%02X ", packet_buffer[i]);
                }
                fprintf(PC, "\r\n");
            }
            // mode == 0x03 (STR_PRINT) の場合は、上で毎ステップ出力しているのでここでは何もしない

            // 次のパケットの準備
            packet_num++;
            memset(packet_buffer, 0, PACKET_SIZE);
            packet_buffer[0] = packet_num & 0xFF; // 先頭はパケット番号
            packet_idx = 1;
            block_count = 0;
        }

        // ----------------------------------------------------
        // [D] サンプリングレートに応じたウェイト
        // ----------------------------------------------------
        switch (samplingRate)
        {
            case SAMP_RATE_10MS:   delay_ms(10);   break;
            case SAMP_RATE_50MS:   delay_ms(50);   break;
            case SAMP_RATE_100MS:  delay_ms(100);  break;
            case SAMP_RATE_500MS:  delay_ms(500);  break;
            case SAMP_RATE_1000MS: delay_ms(1000); break;
            case SAMP_RATE_5000MS: delay_ms(5000); break;
            default:               delay_ms(10);   break;
        }
    }

    // ========================================================
    // 3. ループ終了後、端数（満杯にならなかった分）の処理
    // ========================================================
    if (block_count > 0)
    {
        uint32_t crc24 = calc_crc24(packet_buffer, 61);
        packet_buffer[61] = (crc24 >> 16) & 0xFF;
        packet_buffer[62] = (crc24 >> 8) & 0xFF;
        packet_buffer[63] = crc24 & 0xFF;

        // ★分岐: 端数分の処理も mode に応じて分ける
        if (mode == 0x01)
        {
            uint32_t flash_addr = MISF_TMP_str_data_START + str_data.used_counter;
            write_data_bytes(mis_fm, flash_addr, (int8*)packet_buffer, PACKET_SIZE);

            str_data.used_counter += PACKET_SIZE;
            str_data.uncopied_counter += PACKET_SIZE;
        }
        else if (mode == 0x02)
        {
            fprintf(PC, "[DEBUG] Packet %lu (Fraction): ", packet_num);
            for (uint8_t i = 0; i < PACKET_SIZE; i++)
            {
                fprintf(PC, "%02X ", packet_buffer[i]);
            }
            fprintf(PC, "\r\n");
        }
    }

    // Flashのアドレス管理領域を一度だけセーブ (通常ミッション時のみ)
    if (mode == 0x01)
    {
        write_misf_address_area();
    }

    // ==========================================
    // 4. 測定終了後にアナログ回路と内蔵ADCをOFFに戻す
    // ==========================================
    setup_adc_ports(NO_ANALOGS);
    setup_adc(ADC_OFF);

    output_low(PIN_VREF_EN);
    output_low(PIN_LDO_EN);
    output_low(PIN_LED2);
    output_low(PIN_LED1);

    fprintf(PC, "End Synchronized Measurement Sequence\r\n");
}