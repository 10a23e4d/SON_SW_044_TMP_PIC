#include "cigs/system/son_tmp_main.h"

void main()
{
    delay_ms(100);
    fprintf(PC,"\r\n\r\n\r\n============================================================\r\n");
    fprintf(PC,"This is SHION TMP PIC.\r\n");
    fprintf(PC,"Last updated on %s %s.\r\n\r\n", __DATE__, __TIME__);

    io_init();
    setup_uart_to_boss(); // 割り込みはオフになっています
    setup_timer();
    misf_init();

    piclog_make(0x00, 0x00);

    int1 is_finished = FALSE;
    int8 last_buffer_size = 0;
    int32 loop_counter = 0;

    fprintf(PC,"____TMP PIC Start Operation_____\r\n\r\n");
    fprintf(PC,"waiting for BOSS PIC command\r\n");

    while(!is_finished)
    {
        // 約1秒ごとにピリオドを打って、フリーズしていないことを証明する
        loop_counter++;
        if (loop_counter > 20000) {
            fprintf(PC, ".");
            loop_counter = 0;
        }

        // --- 1. ハードウェアUARTのバッファから安全にデータを吸い上げる ---
        if (kbhit(BOSS))
        {
            uint8_t timeout_ms = 0;
            // データが途切れるまで全力で吸い上げる
            while(timeout_ms < 5)
            {
                if (kbhit(BOSS)) {
                    if (boss_receive_buffer_size < RECEIVE_BUFFER_MAX) {
                        boss_receive_buffer[boss_receive_buffer_size++] = fgetc(BOSS);
                    } else {
                        fgetc(BOSS); // 満杯時は捨てる
                    }
                    timeout_ms = 0;
                } else {
                    delay_ms(1);
                    timeout_ms++;
                }
            }
        }
        // ---------------------------------------------------------------

        // --- デバッグ機能：受信状況のリアルタイム表示 ---
        if (boss_receive_buffer_size != last_buffer_size)
        {
            fprintf(PC, "\r\n[DEBUG] RX(%d): ", boss_receive_buffer_size);
            for(int i = 0; i < boss_receive_buffer_size; i++)
            {
                fprintf(PC, "%02X ", boss_receive_buffer[i]);
            }
            last_buffer_size = boss_receive_buffer_size;
        }
        // ------------------------------------------------

        // 12バイト貯まったらパース処理
        if(boss_receive_buffer_size >= 12)
        {
            Command recieve_cmd = make_receive_command((uint8_t*)boss_receive_buffer, boss_receive_buffer_size);
            clear_receive_signal((uint8_t*)boss_receive_buffer, &boss_receive_buffer_size);

            if(recieve_cmd.is_exist)
            {
                fprintf(PC, "\r\n[INFO] Valid Command Frame Received! ID: %02X\r\n", recieve_cmd.frame_id);
                execute_mission_command(&recieve_cmd);

                boss_receive_buffer_size = 0;
                last_buffer_size = 0;
                fprintf(PC,"\r\nwaiting for BOSS PIC command\r\n");
            }
            else
            {
                fprintf(PC, "\r\n[WARN] Invalid Frame. Discarding buffer.\r\n");
                boss_receive_buffer_size = 0;
                last_buffer_size = 0;
            }
        }

        if(is_finished == TRUE) break;

        delay_us(50);
    }

    fprintf(PC, "\r\n\r\n======\r\n\r\nFinished process.\r\nWait for BOSS PIC turn off me\r\n");

    while (TRUE)
    {
        fprintf(PC, ".");
        delay_ms(1000);
    }

    fprintf(PC, "End main\r\n");
}

/*
#include "cigs/system/son_tmp_main.h"
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

    // Start loop
    while(!is_finished)
    {
        // BOSSからの受信バッファにデータがあるかチェック
        if(boss_receive_buffer_size > 0)
        {
            // 1. バッファからフレームを解析し、Command構造体を生成
            Command recieve_cmd = make_receive_command((uint8_t*)boss_receive_buffer, boss_receive_buffer_size);

            // 2. 解析が終わった分（または不要なゴミデータ）をバッファから消去
            clear_receive_signal((uint8_t*)boss_receive_buffer, &boss_receive_buffer_size);

            // 3. 正しいコマンドフレームが抽出できていた場合のみ処理を実行
            if(recieve_cmd.is_exist)
            {
                fprintf(PC, "\r\n[INFO] Valid Command Frame Received! ID: %02X\r\n", recieve_cmd.frame_id);

                // 新しいディスパッチャ（司令塔）へコマンドを渡す
                execute_mission_command(&recieve_cmd);

                fprintf(PC,"\r\nwaiting for BOSS PIC command");
            }
        }

        // 終了フラグのチェック
        if(is_finished == TRUE)
            break;

        delay_ms(400);
        fprintf(PC, ".");
    }

    fprintf(PC, "\r\n\r\n======\r\n\r\nFinished process.\r\nWait for BOSS PIC turn off me\r\n");

    while (TRUE)
    {
        fprintf(PC, ".");
        delay_ms(1000);
    }

    fprintf(PC, "End main\r\n");
}
    */