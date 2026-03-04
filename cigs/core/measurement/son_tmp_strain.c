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
        delay_ms(1);
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