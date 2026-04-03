#include "son_tmp_smf.h"
#include "son_tmp_flash.h"
#include "../../../lib/device/mt25q.h"

// ============================================================================
// グローバル変数 (転送状態の保持)
// ============================================================================
static uint32_t current_transfer_address = 0;
static uint32_t current_transfer_size = 0;

// ============================================================================
// SMF（CPLD経由）転送の実処理
// ============================================================================

void cigs_smf_prepare(void)
{
    // 例として、IVデータ（温度データ含む）の未コピー分を転送対象とする
    if (str_data.uncopied_counter > 0)
    {
        // コピー開始アドレスの計算 (使用済み量から未コピー量を引いた場所が先頭)
        current_transfer_address = MISF_TMP_STR_DATA_START + str_data.used_counter - str_data.uncopied_counter;

        // 最大15パケット分（64Byte × 15 = 960Byte）を1回の転送サイズとする
        uint32_t max_transfer_bytes = PACKET_SIZE * SMF_TRANSFER_PACKETS;

        if (str_data.uncopied_counter >= max_transfer_bytes)
        {
            current_transfer_size = max_transfer_bytes;
        }
        else
        {
            // 残りが15パケット未満なら、ある分だけ転送する
            current_transfer_size = str_data.uncopied_counter;
        }

        fprintf(PC, "\t[SMF] Prepare: Addr=0x%08lX, Size=%lu bytes\r\n", current_transfer_address, current_transfer_size);
    }
    else
    {
        current_transfer_size = 0;
        fprintf(PC, "\t[SMF] Prepare: No data to copy.\r\n");
    }
}

void cigs_smf_copy(void)
{
    if (current_transfer_size == 0) return;

    uint8_t buffer[PACKET_SIZE];
    uint32_t remaining = current_transfer_size;
    uint32_t addr = current_transfer_address;

    fprintf(PC, "\t[SMF] Copying data to CPLD...\r\n");

    // CPLDのチップセレクトをLowにしてSPI通信開始
    output_low(SMF_CS);

    // パケット単位で読み出してCPLDへ転送する
    while (remaining > 0)
    {
        uint8_t size = (remaining >= PACKET_SIZE) ? PACKET_SIZE : remaining;

        // 1. MIS_FM(ミッションフラッシュ)からデータを読み出し
        read_data_bytes(mis_fm, addr, (int8*)buffer, size);

        // 2. SPI2(SMF_STREAM)経由でCPLDへ送信
        for (uint8_t i = 0; i < size; i++)
        {
            spi_xfer(SMF_STREAM, buffer[i]);
        }

        addr += size;
        remaining -= size;

        // パケット転送間のウェイト（CPLD側の処理落ちを防ぐため適宜調整）
        delay_us(100);
    }

    // CPLDのチップセレクトをHighにしてSPI通信終了
    output_high(SMF_CS);

    fprintf(PC, "\t[SMF] Copy Complete.\r\n");
}

void cigs_smf_permit(void)
{
    if (current_transfer_size > 0)
    {
        // 転送が成功したので、未コピーカウンタを減算する
        str_data.uncopied_counter -= current_transfer_size;
        current_transfer_size = 0;

        // 更新されたカウンタ情報をFlashのアドレス管理領域に保存する
        write_misf_address_area();

        fprintf(PC, "\t[SMF] Permit: Counters updated and saved.\r\n");
    }
}