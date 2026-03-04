#INT_RDA3
volatile unsigned int8 boss_receive_buffer[RECEIVE_BUFFER_MAX] = {0x00};
volatile int8 boss_receive_buffer_size = 0;

/*/
static void RDA_isr(void)
{
    // ハードウェアがデータを受信した瞬間に自動で呼ばれる
    if (boss_receive_buffer_size < RECEIVE_BUFFER_MAX)
    {
        boss_receive_buffer[boss_receive_buffer_size] = fgetc(BOSS);
        boss_receive_buffer_size++;
    }
    else
    {
        fgetc(BOSS); // バッファ満杯時は捨てる
    }
}
*/

void setup_uart_to_boss()
{
    fprintf(PC, "UART Initialize\r\n");
    //enable_interrupts(INT_RDA3);
    //enable_interrupts(GLOBAL);
    fprintf(PC, "\tComplete\r\n");
}



void clear_receive_signal(unsigned int8 receive_signal[], int8 *receive_signal_size)
{
    memset(receive_signal, 0x00, *receive_signal_size);
    *receive_signal_size = 0;
}
// End of file