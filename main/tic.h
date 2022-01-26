#ifndef __TIC_H__
#define __TIC_H__

#define TIC_BUFFER_SIZE (2048)
#define TIC_READ_BUFFER_SIZE (TIC_BUFFER_SIZE / 2)
#define TIC_CHECKSUM_THRESHOLD (10)
#define TIC_UART_NUM UART_NUM_2

void tic_uart_init();

#endif
