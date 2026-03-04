#include "son_tmp_excute_mission.h"
#include "../../hardware/mcu/uart.h"
#include "../../hardware/mcu/timer.h"
#include "../../core/logging/son_tmp_piclog.h"
#include "son_tmp_mode_mission.h"
#include "son_tmp_mode_flash.h"
#include "../../../lib/communication/communication.h" // transmit_ack() 等を使うために追加

// ============================================================================
// グローバル変数の実体定義
// ============================================================================
unsigned int8 status = 0;
int1 is_use_smf_req_in_mission = 0;

// ============================================================================
// 初期化・ディスパッチ処理
// ============================================================================

void cigs_system_init(void)
{
    fprintf(PC, "TMP System Initialize Start\r\n");

    disable_interrupts(GLOBAL);

    setup_timer();
    setup_uart_to_boss();

    status = 0;
    is_use_smf_req_in_mission = 0;

    piclog_make(0x00, 0x00); // PICLOG_STARTUP

    fprintf(PC, "TMP System Initialize Complete\r\n");
}

static void process_boss_command(uint8_t cmd)
{
    fprintf(PC, "Received Command: 0x%02X\r\n", cmd);

    switch (cmd)
    {
        case CMD_MISSION_START:
        {
            status = EXECUTING_MISSION;
            execute_mission_sequence();
            break;
        }
        case CMD_SMF_PREPARE:
        {
            status = COPYING;
            prepare_smf_transfer();
            break;
        }
        case REQ_SMF_COPY:
        {
            execute_smf_transfer();
            break;
        }
        case CMD_SMF_PERMIT:
        {
            permit_smf_transfer();
            break;
        }
        case REQ_POWER_OFF:
        {
            status = 0;
            break;
        }
        default:
        {
            fprintf(PC, "Unknown Command: 0x%02X\r\n", cmd);
            break;
        }
    }
}

int1 execute_command(Command* cmd)
{
    // 共通ライブラリの解釈による仮のID (00=通常コマンド, 01=Ping疎通確認)
    uint8_t lib_frame_id = cmd->frame_id;

    // どちらにせよ、まずはBOSSにACK(受領確認)を返す
    transmit_ack();

    // もし Ping (AA C1 C1) だった場合は、コマンド処理はせずに終わる
    if (lib_frame_id == 0x01)
    {
        fprintf(PC, "Received Ping (Heartbeat)\r\n");
        return TRUE;
    }

    // 通常のコマンドパケット (AA C0 ...) の場合
    // 本当のコマンドID(10など)は、ペイロードの先頭(content[0])に入っている！
    uint8_t real_cmd = cmd->content[0];

    // コマンド受信ログを記録
    piclog_make(0x10, real_cmd);

    // 本当のコマンドIDを使って分岐処理へ
    process_boss_command(real_cmd);

    return TRUE;
}