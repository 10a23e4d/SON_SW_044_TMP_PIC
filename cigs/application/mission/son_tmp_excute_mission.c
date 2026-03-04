#include "son_tmp_excute_mission.h"
#include "../../hardware/mcu/uart.h"
#include "../../hardware/mcu/timer.h"
#include "../../core/logging/son_tmp_piclog.h"
#include "son_tmp_mode_mission.h"
#include "son_tmp_mode_flash.h"
#include "../../../lib/communication/communication.h"

// ============================================================================
// グローバル変数の実体定義
// ============================================================================
unsigned int8 status = 0;
int1 is_use_smf_req_in_mission = 0;

void cigs_system_init(void)
{
    fprintf(PC, "TMP System Initialize Start\r\n");

    disable_interrupts(GLOBAL);

    setup_timer();
    setup_uart_to_boss();

    status = 0;
    is_use_smf_req_in_mission = 0;

    piclog_make(0x00, 0x00);

    fprintf(PC, "TMP System Initialize Complete\r\n");
}

// ★ 変更1: uint8_t ではなく Command構造体を丸ごと受け取る
static void process_boss_command(Command* cmd_struct)
{
    // 本当のコマンドID(10など)は content[0] に入っている
    uint8_t cmd_id = cmd_struct->content[0];

    fprintf(PC, "Received Command: 0x%02X\r\n", cmd_id);

    switch (cmd_id)
    {
        case CMD_MISSION_START:
        {
            status = EXECUTING_MISSION;
            // ★ 変更2: バケツリレーで構造体を丸ごとミッションに渡す
            execute_mission_sequence(cmd_struct);
            break;
        }
        case CMD_FLASH_DUMP: // ★追加: 0x12を受信した場合
        {
            execute_flash_dump();
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
            fprintf(PC, "Unknown Command: 0x%02X\r\n", cmd_id);
            break;
        }
    }
}

int1 execute_command(Command* cmd)
{
    uint8_t lib_frame_id = cmd->frame_id;

    transmit_ack();

    if (lib_frame_id == 0x01)
    {
        fprintf(PC, "Received Ping (Heartbeat)\r\n");
        return TRUE;
    }

    uint8_t real_cmd = cmd->content[0];

    piclog_make(0x10, real_cmd);

    // ★ 変更3: process_boss_command に構造体を丸ごと渡す
    process_boss_command(cmd);

    return TRUE;
}