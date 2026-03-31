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
    fprintf(PC, "Start Synchronized Measurement (Mode:%u, HW_Ch:%u)\r\n", mode, hw_channel);

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

        switch_channel(hw_channel);
        delay_us(10);
        uint16_t strain1 = read_adc_ltc2452();
        switch_channel(hw_channel + 1); // 次のチャンネルに切り替え
        delay_us(10);
        uint16_t strain2 = read_adc_ltc2452();

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
        // [C] 満杯判定とFlash書き込み
        // パケット0は9個、パケット1以降は10個で満杯
        // ----------------------------------------------------
        uint8_t max_blocks = (packet_num == 0) ? 9 : 10;

        if (block_count >= max_blocks)
        {
            // CRC-24 を計算して末尾の3バイト (Byte 61, 62, 63) に格納
            // ※ calc_crc24() は lib/tool/calc_tools に別途実装が必要です
            uint32_t crc24 = calc_crc24(packet_buffer, 61);
            packet_buffer[61] = (crc24 >> 16) & 0xFF;
            packet_buffer[62] = (crc24 >> 8) & 0xFF;
            packet_buffer[63] = crc24 & 0xFF;

            // Flashへ書き込み
            uint32_t flash_addr = MISF_CIGS_IV_DATA_START + iv_data.used_counter;
            write_data_bytes(mis_fm, flash_addr, (int8*)packet_buffer, PACKET_SIZE);

            iv_data.used_counter += PACKET_SIZE;
            iv_data.uncopied_counter += PACKET_SIZE;

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
    // 3. ループ終了後、端数（満杯にならなかった分）をFlashへ保存
    // ========================================================
    if (block_count > 0)
    {
        uint32_t crc24 = calc_crc24(packet_buffer, 61);
        packet_buffer[61] = (crc24 >> 16) & 0xFF;
        packet_buffer[62] = (crc24 >> 8) & 0xFF;
        packet_buffer[63] = crc24 & 0xFF;

        uint32_t flash_addr = MISF_CIGS_IV_DATA_START + iv_data.used_counter;
        write_data_bytes(mis_fm, flash_addr, (int8*)packet_buffer, PACKET_SIZE);

        iv_data.used_counter += PACKET_SIZE;
        iv_data.uncopied_counter += PACKET_SIZE;
    }

    // Flashのアドレス管理領域を一度だけセーブ
    write_misf_address_area();

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

/*
void execute_measurement(uint8_t mode, uint8_t channel, uint8_t samplingRate)
{
    PacketBuffer packet;
    uint8_t currentPacketNum = 1; // 要件に合わせてパケット1からスタート
    uint8_t dataIdxInPkt = 0;
    bool isFirstPacket = true;

    fprintf(PC, "Start Measurement (Mode:%u, Ch:%u)\r\n", mode, channel);

    // 1. 周辺回路の電源投入と安定待ち
    output_high(PIN_LDO_EN);
    output_high(PIN_VREF_EN);
    output_high(PIN_LED1); // 計測中インジケータON
    delay_ms(10);

    // 2. アナログスイッチの切り替え
    switch_channel(channel);

    // 3. IVデータ計測ループ (パケット1 〜 18まで固定)
    memset(packet.raw, 0, PACKET_SIZE);

    // 最初のパケット(FirstPacket)のヘッダー情報を設定
    packet.first.header.timestamp = get_current_sec();
    packet.first.header.mode = mode;
    packet.first.header.channel = channel;
    packet.first.header.samplingRate = samplingRate;
    packet.first.header.dataCount = 554; // IVデータの総数(固定): 27 + (17 * 31)
    packet.first.header.zeroFill = 0;

    packet.first.packetNum = currentPacketNum;

    while (currentPacketNum <= 18)
    {
        uint16_t adc_val = read_adc_ltc2452();

        // --- 1パケット目の処理 (データ27個) ---
        if (isFirstPacket)
        {
            packet.first.adcData[dataIdxInPkt++] = adc_val;

            // パケットが満杯(27個)になった場合
            if (dataIdxInPkt == 27)
            {
                packet.first.crc8 = calc_crc8(packet.raw, PACKET_SIZE - 1);

                // Flashへ書き込み
                uint32_t write_address = MISF_CIGS_IV_DATA_START + iv_data.used_counter;
                write_data_bytes(mis_fm, write_address, packet.raw, PACKET_SIZE);

                iv_data.used_counter += PACKET_SIZE;
                iv_data.uncopied_counter += PACKET_SIZE;

                isFirstPacket = false;
                currentPacketNum++;

                memset(packet.raw, 0, PACKET_SIZE);
                if (currentPacketNum <= 18)
                {
                    packet.sub.packetNum = currentPacketNum;
                }
                dataIdxInPkt = 0;
            }
        }
        // --- 2〜18パケット目の処理 (データ31個) ---
        else
        {
            packet.sub.adcData[dataIdxInPkt++] = adc_val;

            // パケットが満杯(31個)になった場合
            if (dataIdxInPkt == 31)
            {
                packet.sub.crc8 = calc_crc8(packet.raw, PACKET_SIZE - 1);

                // Flashへ書き込み
                uint32_t write_address = MISF_CIGS_IV_DATA_START + iv_data.used_counter;
                write_data_bytes(mis_fm, write_address, packet.raw, PACKET_SIZE);

                iv_data.used_counter += PACKET_SIZE;
                iv_data.uncopied_counter += PACKET_SIZE;

                currentPacketNum++;

                memset(packet.raw, 0, PACKET_SIZE);
                if (currentPacketNum <= 18)
                {
                    packet.sub.packetNum = currentPacketNum;
                }
                dataIdxInPkt = 0;
            }
        }

        // Sampling Rate に応じたウェイト処理
        switch (samplingRate)
        {
            case SAMP_RATE_10MS:   delay_ms(10);   break;
            case SAMP_RATE_50MS:   delay_ms(50);   break;
            case SAMP_RATE_100MS:  delay_ms(100);  break;
            case SAMP_RATE_500MS:  delay_ms(500);  break;
            case SAMP_RATE_1000MS: delay_ms(1000); break;
            case SAMP_RATE_5000MS: delay_ms(5000); break;
            default:
                delay_ms(10); // 指定外のIDが来た場合は安全のため最速(10ms)で回す
                break;
        }

    }

    // 4. IV計測が完了したら、引き続いて温度計測シーケンスを呼び出す
    // パケット19から14パケット分 (パケット19〜32) を敷き詰める
    execute_temp_measurement(19, 14);

    // 5. 周辺回路の電源OFF
    output_low(PIN_LDO_EN);
    output_low(PIN_VREF_EN);
    output_low(PIN_LED1);

    fprintf(PC, "End TMP Measurement Sequence\r\n");

    // フラッシュのアドレス管理領域をまとめて更新
    write_misf_address_area();
}
*/