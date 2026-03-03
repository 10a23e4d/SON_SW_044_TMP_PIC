#ifndef SON_TMP_EXCUTE_MISSION_H
#define SON_TMP_EXCUTE_MISSION_H

#include <stdint.h>
#include <stdbool.h>

#include "../../system/son_tmp_config.h"
#include "../../../lib/communication/value_status.h"
#include "../../../lib/communication/typedef_content.h" // Command構造体を使うために追加

// ============================================================================
// 関数プロトタイプ
// ============================================================================
void cigs_system_init(void);

// バッファを直接読むのではなく、解析済みのCommandを引数で受け取るように変更
void execute_mission_command(Command* cmd);

#endif // SON_TMP_EXCUTE_MISSION_H