#ifndef son_tmp_MODE_FLASH_H
#define son_tmp_MODE_FLASH_H

#include <stdint.h>
#include "../../system/son_tmp_config.h"

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

#endif // son_tmp_MODE_FLASH_H
