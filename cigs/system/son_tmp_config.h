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
// 2. システムシーケンス コマンド定義
// ==========================================
#define CMD_MISSION_START       0x10
#define RES_MISSION_DONE        0x11
#define CMD_FLASH_DUMP          0x12
#define CMD_SMF_PREPARE         0x20
#define REQ_SMF_COPY            0x21
#define CMD_SMF_PERMIT          0x22
#define REQ_POWER_OFF           0x30

#define SAMP_RATE_10MS          0x01  // 0.01秒
#define SAMP_RATE_50MS          0x02  // 0.05秒
#define SAMP_RATE_100MS         0x03  // 0.10秒
#define SAMP_RATE_500MS         0x04  // 0.50秒
#define SAMP_RATE_1000MS        0x05  // 1.00秒
#define SAMP_RATE_5000MS        0x06  // 5.00秒

// ==========================================
// 3. データフォーマット・バッファ定義
// ==========================================
#define PACKET_SIZE             64
#define FLASH_PAGE_SIZE         256
#define PACKETS_PER_PAGE        4
#define SMF_TRANSFER_PACKETS    15

// ヘッダー構造体 (8Byte)
typedef struct
{
    unsigned int32 timestamp;      // 4 Byte
    unsigned int8  mode : 3;       // 3 bit
    unsigned int8  channel : 1;    // 1 bit
    unsigned int8  samplingRate : 4; // 4 bit
    unsigned int16 dataCount;      // 12bitではなく16bit(2Byte)で確保しエラーを回避
    unsigned int8  zeroFill;       // 1 Byte (合計8Byteにするため)
} PacketHeader;

typedef struct
{
    PacketHeader header;           // 8 Byte
    unsigned int8 packetNum;       // 1 Byte
    unsigned int16 adcData[27];    // 54 Byte
    unsigned int8 crc8;            // 1 Byte
} FirstPacket;

typedef struct
{
    unsigned int8 packetNum;       // 1 Byte
    unsigned int16 adcData[31];    // 62 Byte
    unsigned int8 crc8;            // 1 Byte
} SubsequentPacket;

typedef union
{
    unsigned int8 raw[PACKET_SIZE];
    FirstPacket first;
    SubsequentPacket sub;
} PacketBuffer;

#endif // son_tmp_CONFIG_H