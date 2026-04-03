#ifndef son_tmp_CONFIG_H
#define son_tmp_CONFIG_H

#define SELF_DEVICE_ID CIGS_PIC

#include <18F67J94.h>
#opt 0 // 0 = no optimization

#use delay(crystal=16MHz)
#fuses HS, NOWDT, NOBROWNOUT, NOPROTECT

#define PIC18

#ifdef PIC18
    // ==========================================
    // 1. ハードウェア構成とピンアサイン
    // ==========================================

    #define PIN_LDO_EN          PIN_E1
    #define PIN_VREF_EN         PIN_E0
    #define PIN_LED1            PIN_A5
    #define PIN_LED2            PIN_A1

    #define PIN_SW_DIO1         PIN_C1
    #define PIN_SW_DIO0         PIN_C2

    // SPI Chip Select (BOSSヘッダ名称に適合)
    #define MIS_FM_CS           PIN_B1
    #define SMF_CS              PIN_D4
    #define PIN_CS_ADC          PIN_B3

// ------------------------------------------
    // UART 通信設定
    // ------------------------------------------
    #use rs232(baud=9600, parity=N, xmit=PIN_G0, bits=8, stream=PC, FORCE_SW, ERRORS)

    // BOSS側: D1/D0ピンにハードウェアUART3を割り当てて使用する
    #pin_select TX3=PIN_D1
    #pin_select RX3=PIN_D0
    #use rs232(baud=9600, parity=N, UART3, bits=8, stream=BOSS, ERRORS)

    // ------------------------------------------
    // SPI 通信設定
    // ------------------------------------------
    #pin_select SDI2=PIN_B5     // MISO
    #pin_select SDO2=PIN_B4     // MOSI
    #pin_select SCK2OUT=PIN_B2  // CLK
    #pin_select SCK2IN=PIN_B2
    #use spi(MASTER, SPI2, BAUD=1000000, MODE=0, BITS=8, stream=MIS_FM_STREAM)

    #use spi(MASTER, DI=PIN_D6, DO=PIN_D7, CLK=PIN_D5, BAUD=1000000, MODE=0, BITS=8, stream=SMF_STREAM)


#endif

// ==========================================
// 2. システムシーケンス コマンド定義 (刷新版)
// ==========================================

// --- 応答・システム制御用 ---
#define RES_MISSION_DONE             0x11
#define REQ_POWER_OFF                0x30

// --- PICF (Mission Flash) 関連 ---
#define CMD_PICF_ERASE_ALL           0x80
#define CMD_PICF_ERASE_1SECTOR       0x81
#define CMD_PICF_ERASE_4K_SUBSECTOR  0x82
#define CMD_PICF_ERASE_32K_SUBSECTOR 0x83
#define CMD_PICF_WRITE_DEMO          0x84
#define CMD_PICF_WRITE_4K_SUBSECTOR  0x85
#define CMD_PICF_READ                0x86
#define CMD_PICF_READ_ADDRESS        0x87
#define CMD_PICF_ERASE_AND_RESET     0x88
#define CMD_PICF_READ_AREA           0x89
#define CMD_PICF_RESET_ADDRESS       0x8F

// --- SMF (CPLD Flash) 関連 ---
#define CMD_SMF_COPY                 0x90
#define CMD_SMF_READ                 0x91
#define CMD_SMF_ERASE                0x92
#define CMD_SMF_COPY_FORCE           0x93
#define CMD_SMF_READ_FORCE           0x94
#define CMD_SMF_ERASE_FORCE          0x95

// --- 計測 (Strain) 関連 ---
#define CMD_STR                      0xA0
#define CMD_STR_DEBUG                0xA1
#define CMD_STR_PRINT                0xA2

// --- その他 ---
#define CMD_RETURN_TIME              0xB0


#define SAMP_RATE_10MS               0x01  // 0.01秒
#define SAMP_RATE_50MS               0x02  // 0.05秒
#define SAMP_RATE_100MS              0x03  // 0.10秒
#define SAMP_RATE_500MS              0x04  // 0.50秒
#define SAMP_RATE_1000MS             0x05  // 1.00秒
#define SAMP_RATE_5000MS             0x06  // 5.00秒
#define SAMP_RATE_2432MS             0x07  // 2.432秒 (0.25周回連続計測)
#define SAMP_RATE_4865MS             0x08  // 4.865秒 (0.5周回連続計測)
#define SAMP_RATE_9730MS             0x09  // 9.730秒 (1周回連続計測)

// ==========================================
// 3. データフォーマット・バッファ定義
// ==========================================
#define PACKET_SIZE                  64
#define FLASH_PAGE_SIZE              256
#define PACKETS_PER_PAGE             4
#define SMF_TRANSFER_PACKETS         15

#endif // son_tmp_CONFIG_H