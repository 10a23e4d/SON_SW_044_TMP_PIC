#ifndef CALC_TOOLS_H
#define CALC_TOOLS_H

unsigned int8 calc_crc8(unsigned int8 *frame, unsigned int8 payload_size);
int1 is_crc_valid(unsigned int8 *frame, int8 payload_size);
void int32_to_be_array(int32 value, int8 *buf, int8 index);
int32 lsb_array_to_int32(int8 *buf, int8 index);
unsigned int32 lsb_to_msb(unsigned int32 value);
unsigned int32 msb_to_lsb(unsigned int32 value);
uint32_t calc_crc24(uint8_t *data, uint16_t length);


#endif // CALC_TOOLS_H