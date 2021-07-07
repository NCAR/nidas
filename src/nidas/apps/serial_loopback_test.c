 /*******************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2011, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 *******************************************************************/

//  serial_loopback_stream.c
//  Created by Josh Carnes on 2020-07-13.

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/time.h>

#define SERIAL_DEV_DEFAULT "/dev/ttyS0"
#define SEND_INTERVAL_MS_DEFAULT 100  // Interval between send, ms
#define DURATION_S_DEFAULT 1          // Test Duration, seconds
#define BAUDRATE_DEFAULT 115200       // Serial Baud Rate
#define TX_WORD_MAX_LEN 42            // Maximum TX pattern length
#define TX_WORD_DEF_LEN 26            // Default word length (see string below)
#define RX_BUFF_MAX_LEN 50            // Maximum Receive buffer length
#define SAMPLE_CNTR_LEN 100           // Sample send counter length

// rx_status enumerations
#define RX_STATUS_LINECOMPLETE     1
#define RX_STATUS_RECEIVING        0
#define RX_STATUS_ERROR_BADWORD   -1
#define RX_STATUS_ERROR_NONEWLINE -2
#define RX_STATUS_ERROR_READ      -3

int set_interface_attribs (int fd, int speed, int parity)
{
        struct termios tty;
        if (tcgetattr (fd, &tty) != 0)
        {
            printf("Error getting attributes\n");
            return -1;
        }

        cfsetospeed (&tty, speed);
        cfsetispeed (&tty, speed);

        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
        // disable IGNBRK for mismatched speed tests; otherwise receive break
        // as \000 chars
        tty.c_iflag &= ~IGNBRK;         // disable break processing
        tty.c_lflag = 0;                // no signaling chars, no echo,
                                        // no canonical processing
        tty.c_oflag = 0;                // no remapping, no delays
        tty.c_cc[VMIN]  = 0;            // read doesn't block
        tty.c_cc[VTIME] = 0;            // 0 decisecond read timeout

        tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

        tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
                                        // enable reading
        tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
        tty.c_cflag |= parity;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;

        if (tcsetattr (fd, TCSANOW, &tty) != 0)
        {
            printf("Error Setting Attributes\n");
            return -1;
        }
        return 0;
}

void print_app_usage(void)
{
    printf("\n--- Data loopback verification of half-duplex Serial Port, 8n1 ---\n");
    printf("Usage: [-h] [-v] [-T interval] [-N duration] [-b baudrate] [-p pattern] -d device\n");
    printf("-h          Help\n");
    printf("-v          Verbose Status\n");
    printf("-d (str)    Serial Device, Default=%s\n", SERIAL_DEV_DEFAULT);
    printf("-T (uint)   Word Send Interval, milliseconds, >1, <=1000, Default=%d\n", SEND_INTERVAL_MS_DEFAULT);
    printf("-N (uint)   Test Duration, seconds,  >=1, Default=%d\n", DURATION_S_DEFAULT);
    printf("-b (uint)   Baud Rate: 9600, 115200, 230400, Default=%d\n", BAUDRATE_DEFAULT);
    printf("-p (str)    ASCII Test Pattern (len<%d), newline appended\n",TX_WORD_MAX_LEN-2);
    printf("            First char of pattern must be unique\n");
    printf("            Default Pattern: #{xx},a0a0,a0a0,a0a0,a0a0,cc\n");
    printf("            where {xx} is an 8-bit send counter, hex\n");
    printf("            Overriding pattern with -p becomes a fixed pattern\n");
    printf("Notes: - Assumes port is connected to an echoing device or loopback dongle\n");
    printf("       - Some combinations of parameters may exceed the interface bandwidth\n");
    printf("       - Accessing the Serial Ports may require sudo privileges\n");
    printf("       - Specifying a custom Test Pattern may require argument string in \"\" quotes\n");
    return;
}

int main (int argc, char **argv)
{
    struct timeval now_time;
    struct timeval prev_send_time;
    struct tm * timeinfo;
    time_t now_to_print;
    char time_buffer[30];
    long int tdiff = 0;
    int send_lock = 1;
    int send_counter = 0; // %256 send sample counter
    char serial_dev[20] = SERIAL_DEV_DEFAULT;
    int interval_ms = SEND_INTERVAL_MS_DEFAULT;
    int word_rate = 1000/interval_ms;
    int duration_s = DURATION_S_DEFAULT;
    int baud_rate = BAUDRATE_DEFAULT;
    int print_status_enabled = 0;
    int pattern_override = 0;
    int end_time_s = 0;
    int temp_int = 0;
    int word_len = TX_WORD_DEF_LEN;
    float bandwidth = 0.0;

    char tx_word[TX_WORD_MAX_LEN+1] = "";
    char tx_word_prev[TX_WORD_MAX_LEN+1] = "";
    char rx_buff[RX_BUFF_MAX_LEN] = "";
    char rx_char = '\0';
    int  rx_buff_i = 0;
    ssize_t rx_len = -1;
    int  rx_status = RX_STATUS_RECEIVING;
    int tx_count = 0;
    int rx_count = 0;

    extern char *optarg; /* set by getopt() */
    extern int optind;   /* "  "     "     */
    int opt_char;        /* option character */

    // Parse inputs args
    while ((opt_char = getopt(argc, argv, "hvT:N:b:p:d:")) != -1)
    {
        switch (opt_char)
        {
            case 'v' :
                print_status_enabled = 1;
            break;

            case 'd' :
                if (strlen(optarg) < 20-1)
                {
                    strcpy(serial_dev, optarg);
                }
                else
                {
                    printf("ERROR str too long: %s\n", optarg);
                    exit(1);
                }
            break;

            case 'T' :
                interval_ms = atoi(optarg);
                if ((interval_ms > 1000) || (interval_ms < 1))
                {
                    print_app_usage();
                    exit(1);
                }
            break;

            case 'N' :
                duration_s = atoi(optarg);
            break;

            case 'b' :
                baud_rate = atoi(optarg);
                if ((baud_rate != 9600) &&
                    (baud_rate != 115200) &&
                    (baud_rate != 230400))
                {
                    print_app_usage();
                    exit(1);
                }
            break;

            case 'p' :
                if ((strlen(optarg) <= TX_WORD_MAX_LEN - 2) &
                    (strlen(optarg) > 0))
                {
                    strcpy(tx_word, optarg);
                    sprintf(tx_word, "%s\n", tx_word); // append newline
                    pattern_override = 1;     // override the default pattern
                    word_len = strlen(tx_word)+1;
		}
                else
                {
                    print_app_usage();
                    exit(1);
                }
            break;

            case 'h' :
                print_app_usage();
                exit(1);
            break;

            case '?' :
            default :
                print_app_usage();
                printf("ERROR bad input %c\n", opt_char);
                exit(1);
            break;

        }
    }

    // Open Serial port and configure
    if (print_status_enabled) printf("Opening Port %s\n", serial_dev);

    int fd0 = open(serial_dev, O_RDWR);
    if (fd0 < 0)
    {
        printf("Error opening port, %d, %s\n", fd0, strerror(errno));
        exit(1);
    }

    switch (baud_rate)
    {
        case (9600) : temp_int = B9600;
        break;

        case (115200) : temp_int = B115200;
        break;

        case (230400) : temp_int = B230400;
        break;

        default :
            printf("ERROR Baud rate");
            exit(1);
        break;
    }
    set_interface_attribs (fd0, temp_int, 0);  // set baud rate and 8n1 (no parity)

    tcflush(fd0, TCIOFLUSH);
    sleep(1);

    bandwidth = 100.0 * (10.0/8.0 * word_len * 1000.0/interval_ms) / baud_rate;

    if (print_status_enabled)
    {
        printf("Transmitting %.1f WPS at %d baud, %.1f %% BW\n", 1000.0/interval_ms, baud_rate, bandwidth);
        if (pattern_override != 0) printf("Pattern: %s", tx_word);
    }
    now_to_print = time(NULL);
    timeinfo = localtime(&now_to_print);
    strftime(time_buffer, 9, "%H:%M:%S", timeinfo);

    if (print_status_enabled) printf("Start %s, Duration %d s\n", time_buffer, duration_s);

    // Get initial times, and wait until the top of the next second
    gettimeofday(&prev_send_time, NULL);
    gettimeofday(&now_time, NULL);
    while (now_time.tv_usec > 200)
    {
        gettimeofday(&now_time, NULL);
    }
    end_time_s = now_time.tv_sec + duration_s;

    // Start sending and receiving. Allow 2s receive timeout.
    while (now_time.tv_sec < end_time_s + 2)
    {
        // Write Data to the Ports if unlocked
        if (send_lock == 0)
        {
            strcpy(tx_word_prev, tx_word); // save previous transmitted word
            if (pattern_override == 0)
            {
                sprintf(tx_word, "#%02x,a0a0,a0a0,a0a0,a0a0,cc\n", send_counter);
            }
            write(fd0, tx_word, strlen(tx_word));
            send_lock = 1;
            send_counter = (send_counter + 1) % SAMPLE_CNTR_LEN;
            tx_count++;
        }

        // unlock the send mechanism periodically per the interval
        tdiff = (now_time.tv_sec - prev_send_time.tv_sec)*1000000 + (now_time.tv_usec - prev_send_time.tv_usec);
        if ((tdiff > interval_ms*1000) && (now_time.tv_sec < end_time_s))
        {
            //printf("Unlocking...%ld %ld %ld %ld %ld\n",tdiff, now_time.tv_sec, now_time.tv_usec, prev_send_time.tv_sec,prev_send_time.tv_usec);
            send_lock = 0;
            prev_send_time = now_time;
        }

        // Read from ports, get lines, and verify
        while (1)
        {
            if (rx_status != RX_STATUS_RECEIVING) break;

            rx_len = read(fd0, &rx_char, sizeof(char));
            if (rx_len == -1) {
                rx_status = RX_STATUS_ERROR_READ;
                break;
            }
            else if (rx_len == 0) break;
            else if (rx_len > 0)
            {
                if (rx_char == tx_word[0]) // reset rx buffer if start of pattern
                    rx_buff_i = 0;

                rx_buff[rx_buff_i] = rx_char;

                if (rx_char == '\n') // end of pattern
                {
                    rx_buff[rx_buff_i+1]='\0'; // Complete string with NULL term
                    rx_status = RX_STATUS_LINECOMPLETE;

                    // Send Activity . at final sample of second, based on word rate
                    if ((rx_count % word_rate == 0) &&
                        (print_status_enabled))
                    {
                        printf(".");
                        fflush(stdout);
                    }
                }

                rx_buff_i++;
                if (rx_buff_i > RX_BUFF_MAX_LEN-1)
                {
                    rx_buff_i = 0;
                    rx_status = RX_STATUS_ERROR_NONEWLINE;
                }
            }
        }

        // Check valid received word, allow 1 sample slip
        //   If unmatched, then dump buffer
        if (rx_status == RX_STATUS_LINECOMPLETE)
        {
            // Compare word
            if (0 == strcmp(tx_word, rx_buff))
            {
                rx_status = RX_STATUS_RECEIVING;
                rx_count++;
            }
            // Compare to previous word
            else if (0 == strcmp(tx_word_prev, rx_buff))
            {
                rx_status = RX_STATUS_RECEIVING;
                rx_count++;
            }
            else
            {
                rx_status = RX_STATUS_ERROR_BADWORD;
                //for (int i=0; i < RX_BUFF_MAX_LEN; i++)
                //    printf("0x%02x,",rx_buff[i]);
                //printf("\n");
            }
            rx_buff_i = 0;
//            usleep(500); // temporarily release the process
        }

        // If error detected, report
        if (rx_status < 0)
        {
            if (print_status_enabled)
            {
                printf("\nERROR Rx Detected, %d:%s!=%s\n", rx_status, tx_word, rx_buff);
                fflush(stdout);
            }
            rx_status = RX_STATUS_RECEIVING;
        }

        // Update now time
        gettimeofday(&now_time, NULL);

        // Break if all received
        if ((now_time.tv_sec > end_time_s) && (rx_count == tx_count)) break;
    }

    // End test, report
    now_to_print = time(NULL);
    timeinfo = localtime(&now_to_print);
    strftime(time_buffer, 9, "%H:%M:%S", timeinfo);

    if (print_status_enabled) printf("\nEnd %s\n", time_buffer);

    if (tx_count != rx_count)
    {
        printf("Test FAILED, %s TX (%d) != RX (%d) words\n", serial_dev, tx_count, rx_count);
        exit(1);
    }
    else
    {
        printf("Test SUCCESS, %s RX = %d words, %d chars\n", serial_dev, rx_count, rx_count * strlen(tx_word));
    }

    close(fd0);

    return 0;
}

