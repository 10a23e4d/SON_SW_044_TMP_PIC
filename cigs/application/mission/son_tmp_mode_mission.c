#include "son_tmp_mode_mission.h"
#include "../../core/measurement/son_tmp_strain.h" // ※環境に合わせて son_tmp_strain.h 等に変更してください
#include "../../core/logging/son_tmp_piclog.h"
#include "son_tmp_excute_mission.h"

// ============================================================================
// ミッション実行シーケンス
// ============================================================================

void execute_mission_sequence(Command* cmd)
{
    fprintf(PC, "--- Mission Sequence Start ---\r\n");

    // ミッション開始ログの記録
    piclog_make(0x11, 0x00);

    // =========================================================
    // シミュレータから送られたペイロードからパラメータを読み取る
    // content[0] : 0x10 (CMD_MISSION_START)
    // content[1] : チャンネル指定 (1〜4)
    // content[2] : サンプリングレート指定 (ID)
    // =========================================================
    uint8_t rx_channel = cmd->content[1];
    uint8_t samplingRate = cmd->content[2];
    uint8_t mode = 0x01; // 通常計測モード(固定)

    // BOSSから指定されたCH(1〜4)を、マイコン内部のスイッチ番号(0〜3)に変換
    uint8_t hw_channel = 0;
    if (rx_channel >= 1 && rx_channel <= 4)
    {
        hw_channel = rx_channel - 1;
    }
    else
    {
        fprintf(PC, "[WARN] Invalid Channel %u. Forced to CH 1 (HW:0).\r\n", rx_channel);
        hw_channel = 0;
    }

    fprintf(PC, "Executing Target -> BOSS_CH: %u (HW_CH: %u), SamplingRate: 0x%02X\r\n", rx_channel, hw_channel, samplingRate);

    // ★ forループを廃止し、指定された1つのチャンネルだけを計測(固定554回)する
    execute_measurement(mode, hw_channel, samplingRate);

    // ミッション終了ログの記録
    piclog_make(0x12, 0x00);

    fprintf(PC, "--- Mission Sequence Complete ---\r\n");
}