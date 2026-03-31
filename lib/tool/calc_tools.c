unsigned int8 calc_crc8(unsigned int8 *frame, unsigned int8 payload_size)
{
   if (payload_size <= 0) return 0;

   unsigned int8 crc = *frame++;
   while (--payload_size > 0)
      crc ^= *frame++;

   return crc;
}

int1 is_crc_valid(unsigned int8 *frame, int8 payload_size)
{
   unsigned int8 received_crc = frame[payload_size - 1]; // CRC is the last byte
   unsigned int8 calculated_crc = calc_crc8(frame, payload_size - 1);
   return (received_crc == calculated_crc);
}

void int32_to_be_array(int32 value, int8 *buf, int8 index)
{
    buf[index + 0] = (int8)(value >> 24);   // MSB
    buf[index + 1] = (int8)(value >> 16);
    buf[index + 2] = (int8)(value >>  8);
    buf[index + 3] = (int8)(value      );   // LSB
}

int32 lsb_array_to_int32(int8 *buf, int8 index)
{
    return ((int32)buf[index + 0] <<  0) |
           ((int32)buf[index + 1] <<  8) |
           ((int32)buf[index + 2] << 16) |
           ((int32)buf[index + 3] << 24);
}


int32 msb_array_to_int32(int8 *buf, int8 index)
{
    return ((int32)buf[index + 0] << 24) |
           ((int32)buf[index + 1] << 16) |
           ((int32)buf[index + 2] <<  8) |
           ((int32)buf[index + 3]);
}

unsigned int32 int32_lsb_to_msb(unsigned int32 value)
{
    return ((value & 0x000000FF) << 24) |
           ((value & 0x0000FF00) << 8)  |
           ((value & 0x00FF0000) >> 8)  |
           ((value & 0xFF000000) >> 24);
}

unsigned int32 int32_msb_to_lsb(unsigned int32 value)
{
    return ((value & 0xFF000000) >> 24) |
           ((value & 0x00FF0000) >> 8)  |
           ((value & 0x0000FF00) << 8)  |
           ((value & 0x000000FF) << 24);
}
// End of file

uint32_t calc_crc24(uint8_t *data, uint16_t length)
{
    uint32_t crc = 0xFFFFFF; // 初期値
    uint32_t poly = 0x864CFB; // 多項式

    for (uint16_t i = 0; i < length; i++)
    {
        crc ^= ((uint32_t)data[i] << 16);
        for (uint8_t j = 0; j < 8; j++)
        {
            crc <<= 1;
            if (crc & 0x1000000) // 24bit目(あふれたビット)が1ならXOR
            {
                crc ^= poly;
            }
        }
    }
    return crc & 0xFFFFFF; // 下位24bitのみを返す
}