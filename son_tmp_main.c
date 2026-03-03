#include "cigs/system/son_tmp_main.h"  // ヘッダーファイルから自動的にインクルードされるため不要

void main()
{
    delay_ms(100); // wait for power stable
    fprintf(PC,"\r\n\r\n\r\n============================================================\r\n");
    fprintf(PC,"This is MOMIJI CIGS PIC BBM for MIS7_BBM4.\r\n");
    fprintf(PC,"Last updated on %s %s, by Inoue.\r\n\r\n", __DATE__, __TIME__);

    io_init();
    setup_uart_to_boss();
    setup_timer();
    misf_init();

    piclog_make(0x00, 0x00); // PICLOG_STARTUP

    int1 is_finished = FALSE;
    fprintf(PC,"____CIGS PIC Start Operation_____\r\n\r\n");

    fprintf(PC,"waiting for BOSS PIC command");

    //Start loop
    while(!is_finished)
    {
        // handle from boss commands
        if(boss_receive_buffer_size > 0)
        {
            volatile Command recieve_cmd = make_receive_command(boss_receive_buffer, boss_receive_buffer_size);

            // ここでバッファがクリア(サイズ0)になる
            clear_receive_signal(boss_receive_buffer, &boss_receive_buffer_size);

            if(recieve_cmd.is_exist)
            {
                // 変更: 古い関数ではなく、新しいディスパッチャにCommand構造体を渡す
                execute_mission_command((Command*)&recieve_cmd);

                fprintf(PC,"\r\nwaiting for BOSS PIC command");
            }
        }

        // check `is break while loop`
        if(is_finished == TRUE)
            break;

        delay_ms(400);
        fprintf(PC, ".");
    }

    fprintf(PC, "\r\n\r\n======\r\n\r\nFinished process.\r\nWait for BOSS PIC turn off me");

    while (TRUE)
    {
        fprintf(PC, ".");
        delay_ms(1000);
    }

    fprintf(PC, "End main\r\n");
}