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

    // ==========================================
    // 0. 測定開始時に電源・VREF・LEDをONにする
    // ==========================================
    output_high(PIN_VREF_EN); // RE0 ON (リファレンス電圧供給)
    output_high(PIN_LDO_EN);  // RE1 ON (ADC用LDO起動)
    output_high(PIN_LED2);    // RA1 ON
    output_high(PIN_LED1);    // RA5 ON

    // ========================================================
    // 1. バッファとパケット番号の初期化 (ひずみ用 & 温度用)
    // ========================================================
    // --- ひずみ用 (パケット 1 〜 18) ---
    uint16_t strain_packet_num = 1;
    uint8_t strain_data_idx = 0;
    uint8_t strain_buffer[PACKET_SIZE];
    memset(strain_buffer, 0, PACKET_SIZE);

    // --- 温度用 (パケット 19 〜 32) ---
    uint16_t temp_packet_num = 19;
    uint8_t temp_data_idx = 0;
    uint8_t temp_buffer[PACKET_SIZE];
    memset(temp_buffer, 0, PACKET_SIZE);


    // ========================================================
    // 2. メインサンプリングループ (固定 555回)
    // ========================================================
    for (uint16_t step = 0; step < 555; step++)
    {
        // ----------------------------------------------------
        // [A] ひずみのADC読み取りとバッファ格納
        // ----------------------------------------------------
        // TODO: ここでADC(LTC2452等)からひずみデータを読み取る
        uint16_t strain_val = 0xFFFF; // ダミーデータ

        // バッファに格納 (ビッグエンディアンかリトルエンディアンかはシステム仕様に合わせる)
        strain_buffer[strain_data_idx++] = (strain_val >> 8) & 0xFF;
        strain_buffer[strain_data_idx++] = strain_val & 0xFF;

        // バッファが満杯になったらFlashへ書き込み、次のパケットへ
        if (strain_data_idx >= PACKET_SIZE) // ヘッダ等がある場合はペイロードサイズで判定
        {
            // TODO: Flashへ strain_buffer を書き込む (パケット番号: strain_packet_num)
            // write_to_flash(strain_packet_num, strain_buffer);

            strain_packet_num++;
            strain_data_idx = 0;
            memset(strain_buffer, 0, PACKET_SIZE);
        }

        // ----------------------------------------------------
        // [B] 温度のADC(またはセンサ)読み取りとバッファ格納
        // ----------------------------------------------------
        // TODO: ここで温度データを読み取る
        uint16_t temp_val = 0xEEEE; // ダミーデータ

        temp_buffer[temp_data_idx++] = (temp_val >> 8) & 0xFF;
        temp_buffer[temp_data_idx++] = temp_val & 0xFF;

        // バッファが満杯になったらFlashへ書き込み、次のパケットへ
        if (temp_data_idx >= PACKET_SIZE)
        {
            // TODO: Flashへ temp_buffer を書き込む (パケット番号: temp_packet_num)
            // write_to_flash(temp_packet_num, temp_buffer);

            temp_packet_num++;
            temp_data_idx = 0;
            memset(temp_buffer, 0, PACKET_SIZE);
        }

        // ----------------------------------------------------
        // [C] サンプリングレートに応じたウェイト
        // ----------------------------------------------------
        switch (samplingRate)
        {
            case SAMP_RATE_10MS:   delay_ms(10);   break;
            case SAMP_RATE_50MS:   delay_ms(50);   break;
            case SAMP_RATE_100MS:  delay_ms(100);  break;
            case SAMP_RATE_500MS:  delay_ms(500);  break;
            case SAMP_RATE_1000MS: delay_ms(1000); break;
            case SAMP_RATE_5000MS: delay_ms(5000); break;
            default:               delay_ms(10);   break; // フェールセーフ
        }
    }

    // ========================================================
    // 3. ループ終了後、端数（満杯にならなかった分）をFlashへ保存
    // ========================================================
    if (strain_data_idx > 0 && strain_packet_num <= 18)
    {
        // write_to_flash(strain_packet_num, strain_buffer);
    }

    if (temp_data_idx > 0 && temp_packet_num <= 32)
    {
        // write_to_flash(temp_packet_num, temp_buffer);
    }

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