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
    fprintf(PC, "CIGS System Initialize Start\r\n");

    disable_interrupts(GLOBAL);

    setup_timer();
    setup_uart_to_boss();

    status = 0;
    is_use_smf_req_in_mission = 0;

    piclog_make(0x00, 0x00); // PICLOG_STARTUP

    fprintf(PC, "CIGS System Initialize Complete\r\n");
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

// 変更: 解析済みのコマンドを引数として受け取る
void execute_mission_command(Command* cmd)
{
    uint8_t frame_id = cmd->frame_id;

    // 1. コマンドを受理したことをBOSS(シミュレータ)に知らせる
    transmit_ack();

    // 2. コマンド受信ログを記録 (0x10 は仮のコマンド受信イベントID)
    piclog_make(0x10, frame_id);

    // 3. コマンドに応じた処理(計測開始など)へ分岐
    process_boss_command(frame_id);
}