#ifndef son_tmp_MODE_FLASH_H
#define son_tmp_MODE_FLASH_H

#include <stdint.h>
#include "../../system/son_tmp_config.h"
#include "../../../lib/communication/typedef_content.h"

// ============================================================================
// 外部公開関数プロトタイプ
// ============================================================================

// SMF転送準備 (CMD_SMF_PREPARE 受信時に実行)
void prepare_smf_transfer(void);

// SMFデータ転送実行 (REQ_SMF_COPY 受信時に実行)
void execute_smf_transfer(void);

// SMF転送完了許可 (CMD_SMF_PERMIT 受信時に実行)
void permit_smf_transfer(void);

// Mis Flash読み出し (CMD_MIS_FLASH_READ 受信時に実行)
void execute_flash_dump(void);

// PICF関連コマンド実行関数
void execute_picf_read_address(void);
void execute_picf_reset_address(void);
void execute_picf_read(uint32_t start_address, uint16_t packet_num);
void execute_picf_erase_4k(uint32_t start_address, uint8_t sector_num);
void execute_picf_erase_1sector(uint32_t start_address, uint8_t sector_num);
void execute_picf_erase_32k(uint32_t start_address, uint8_t sector_num);
void execute_picf_write_demo(uint32_t start_address, uint16_t packet_num);
void execute_picf_erase_all(void);
void execute_picf_write_4k(uint32_t start_address);
void execute_picf_erase_and_reset(void);
void execute_picf_read_area(uint8_t area, uint8_t start_packet, uint8_t request_packet);

// SMF関連コマンド実行関数
void execute_smf_direct_copy(uint32_t address, uint16_t packet_num, bool is_force);
void execute_smf_read(uint32_t address, uint16_t packet_num, bool is_force);
void execute_smf_erase(uint32_t address, uint8_t sector_num, bool is_force);
void execute_smf_erase_force(void);

#endif // son_tmp_MODE_FLASH_H
