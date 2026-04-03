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

int1 execute_command(Command* cmd)
{
    if (!cmd->is_exist) return false;

    uint8_t cmd_id = cmd->frame_id;
    fprintf(PC, "Received Command: 0x%02X\r\n", cmd_id);

    switch (cmd_id)
    {
        // ----------------------------------------------------
        // 計測コマンド
        // ----------------------------------------------------
        case CMD_STR:
        {
            fprintf(PC, "[CMD] STR (0xA0)\r\n");
            status = EXECUTING_MISSION;
            // 第3引数に mode = 0x01 を渡す
            execute_mission_sequence((uint8_t)cmd->content[1], (uint8_t)cmd->content[2], 0x01);
            break;
        }
        case CMD_STR_DEBUG:
        {
            fprintf(PC, "[CMD] STR_DEBUG (0xA1)\r\n");
            status = EXECUTING_MISSION;
            // 第3引数に mode = 0x02 を渡す
            execute_mission_sequence((uint8_t)cmd->content[1], (uint8_t)cmd->content[2], 0x02);
            break;
        }
        case CMD_STR_PRINT:
        {
            fprintf(PC, "[CMD] STR_PRINT (0xA2)\r\n");
            status = EXECUTING_MISSION;
            // 第3引数に mode = 0x03 を渡す
            execute_mission_sequence((uint8_t)cmd->content[1], (uint8_t)cmd->content[2], 0x03);
            break;
        }

        // ----------------------------------------------------
        // PICF (Mission Flash) コマンド
        // ----------------------------------------------------
        case CMD_PICF_READ:
        {
            fprintf(PC, "[CMD] PICF_READ (0x86)\r\n");

            uint32_t address = ((uint32_t)cmd->content[1] << 24) |
                               ((uint32_t)cmd->content[2] << 16) |
                               ((uint32_t)cmd->content[3] << 8)  |
                                (uint32_t)cmd->content[4];

            uint16_t packet_num = ((uint16_t)cmd->content[6] << 8) | (uint16_t)cmd->content[7];

            execute_picf_read(address, packet_num);
            break;
        }
        case CMD_PICF_ERASE_ALL:
        {
            fprintf(PC, "[CMD] PICF_ERASE_ALL (0x80)\r\n");
            execute_picf_erase_all();
            break;
        }
        case CMD_PICF_ERASE_1SECTOR:
        {
            fprintf(PC, "[CMD] PICF_ERASE_1SECTOR (0x81)\r\n");

            uint32_t address = ((uint32_t)cmd->content[1] << 24) |
                               ((uint32_t)cmd->content[2] << 16) |
                               ((uint32_t)cmd->content[3] << 8)  |
                                (uint32_t)cmd->content[4];

            uint8_t sector_num = cmd->content[7]; // Arg7

            execute_picf_erase_1sector(address, sector_num);
            break;
        }
        case CMD_PICF_ERASE_4K_SUBSECTOR:
        {
            fprintf(PC, "[CMD] PICF_ERASE_4K_SUBSECTOR (0x82)\r\n");

            uint32_t address = ((uint32_t)cmd->content[1] << 24) |
                               ((uint32_t)cmd->content[2] << 16) |
                               ((uint32_t)cmd->content[3] << 8)  |
                                (uint32_t)cmd->content[4];

            uint8_t sector_num = cmd->content[7];

            execute_picf_erase_4k(address, sector_num);
            break;
        }
        case CMD_PICF_ERASE_32K_SUBSECTOR:
        {
            fprintf(PC, "[CMD] PICF_ERASE_32K_SUBSECTOR (0x83)\r\n");

            uint32_t address = ((uint32_t)cmd->content[1] << 24) |
                               ((uint32_t)cmd->content[2] << 16) |
                               ((uint32_t)cmd->content[3] << 8)  |
                                (uint32_t)cmd->content[4];

            uint8_t sector_num = cmd->content[7];

            execute_picf_erase_32k(address, sector_num);
            break;
        }
        case CMD_PICF_WRITE_DEMO:
        {
            fprintf(PC, "[CMD] PICF_WRITE_DEMO (0x84)\r\n");

            uint32_t address = ((uint32_t)cmd->content[1] << 24) |
                               ((uint32_t)cmd->content[2] << 16) |
                               ((uint32_t)cmd->content[3] << 8)  |
                                (uint32_t)cmd->content[4];

            uint16_t packet_num = ((uint16_t)cmd->content[6] << 8) | (uint16_t)cmd->content[7];

            execute_picf_write_demo(address, packet_num);
            break;
        }
        case CMD_PICF_WRITE_4K_SUBSECTOR:
        {
            fprintf(PC, "[CMD] PICF_WRITE_4K_SUBSECTOR (0x85)\r\n");

            uint32_t address = ((uint32_t)cmd->content[1] << 24) |
                               ((uint32_t)cmd->content[2] << 16) |
                               ((uint32_t)cmd->content[3] << 8)  |
                                (uint32_t)cmd->content[4];

            execute_picf_write_4k(address);
            break;
        }
        case CMD_PICF_READ_ADDRESS:
        {
            fprintf(PC, "[CMD] PICF_READ_ADDRESS (0x87)\r\n");
            execute_picf_read_address();
            break;
        }
        case CMD_PICF_ERASE_AND_RESET:
        {
            fprintf(PC, "[CMD] PICF_ERASE_AND_RESET (0x88)\r\n");
            execute_picf_erase_and_reset();
            break;
        }
        case CMD_PICF_READ_AREA:
        {
            fprintf(PC, "[CMD] PICF_READ_AREA (0x89)\r\n");

            uint8_t area = cmd->content[1];
            uint8_t start_packet = cmd->content[2];
            uint8_t request_packet = cmd->content[3];

            execute_picf_read_area(area, start_packet, request_packet);
            break;
        }
        case CMD_PICF_RESET_ADDRESS:
        {
            fprintf(PC, "[CMD] PICF_RESET_ADDRESS (0x8F)\r\n");
            execute_picf_reset_address();
            break;
        }

        // ----------------------------------------------------
        // SMF (CPLD Flash) コマンド (0x90 - 0x95)
        // ----------------------------------------------------
        case CMD_SMF_COPY:
        case CMD_SMF_COPY_FORCE:
        {
            // FORCE版かどうかを判定
            bool is_force = (cmd_id == CMD_SMF_COPY_FORCE);
            fprintf(PC, "[CMD] SMF_COPY%s (0x%02X)\r\n", is_force ? "_FORCE" : "", cmd_id);

            uint32_t address = ((uint32_t)cmd->content[1] << 24) |
                               ((uint32_t)cmd->content[2] << 16) |
                               ((uint32_t)cmd->content[3] << 8)  |
                                (uint32_t)cmd->content[4];

            uint16_t packet_num = ((uint16_t)cmd->content[6] << 8) | (uint16_t)cmd->content[7];

            execute_smf_direct_copy(address, packet_num, is_force);
            break;
        }
        case CMD_SMF_READ:
        case CMD_SMF_READ_FORCE:
        {
            bool is_force = (cmd_id == CMD_SMF_READ_FORCE);
            fprintf(PC, "[CMD] SMF_READ%s (0x%02X)\r\n", is_force ? "_FORCE" : "", cmd_id);

            uint32_t address = ((uint32_t)cmd->content[1] << 24) |
                               ((uint32_t)cmd->content[2] << 16) |
                               ((uint32_t)cmd->content[3] << 8)  |
                                (uint32_t)cmd->content[4];

            uint16_t packet_num = ((uint16_t)cmd->content[6] << 8) | (uint16_t)cmd->content[7];

            execute_smf_read(address, packet_num, is_force);
            break;
        }
        case CMD_SMF_ERASE:
        {
            bool is_force = (cmd_id == CMD_SMF_ERASE_FORCE);
            fprintf(PC, "[CMD] SMF_ERASE%s (0x%02X)\r\n", is_force ? "_FORCE" : "", cmd_id);

            uint32_t address = ((uint32_t)cmd->content[1] << 24) |
                               ((uint32_t)cmd->content[2] << 16) |
                               ((uint32_t)cmd->content[3] << 8)  |
                                (uint32_t)cmd->content[4];

            uint8_t sector_num = cmd->content[7]; // Arg7

            execute_smf_erase(address, sector_num, is_force);
            break;
        }
        case CMD_SMF_ERASE_FORCE:
        {
            fprintf(PC, "[CMD] SMF_ERASE_FORCE (0x%02X)\r\n", cmd_id);
            execute_smf_erase_force();
            break;
        }

        // ----------------------------------------------------
        // その他・システム制御コマンド
        // ----------------------------------------------------
        case CMD_RETURN_TIME:
        {
            fprintf(PC, "[CMD] RETURN_TIME (0xB0)\r\n");

            // システムの現在時刻（秒）を取得
            uint32_t current_time = get_current_sec();

            fprintf(PC, "Current Time: %lu sec\r\n", current_time);

            // ※将来的には、ここでBOSSに対して時刻データを送信する関数を呼び出します
            // send_time_to_boss(current_time);

            break;
            break;
        }
        default:
        {
            fprintf(PC, "Command received but not implemented yet: 0x%02X\r\n", cmd_id);
            break;
        }
    }

    return true;
}