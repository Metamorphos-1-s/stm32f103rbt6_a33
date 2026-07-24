#ifndef RS485_TX_CONTROLLER_H
#define RS485_TX_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

#define RS485_DE_SETUP_US 10U
#define RS485_DE_HOLD_US 10U

typedef enum
{
    RS485_TX_IDLE = 0,
    RS485_TX_ASSERT_DE,
    RS485_TX_WAIT_DE_SETUP,
    RS485_TX_DMA_ACTIVE,
    RS485_TX_WAIT_UART_TC,
    RS485_TX_WAIT_DE_HOLD,
    RS485_TX_RELEASE_DE,
    RS485_TX_ERROR
} Rs485TxState;

void Rs485TxController_Init(void);
bool Rs485TxController_Start(const uint8_t *data, uint16_t length);
void Rs485TxController_Process(void);
void Rs485TxController_Abort(void);
bool Rs485TxController_IsBusy(void);
bool Rs485TxController_TakeCompleted(void);
Rs485TxState Rs485TxController_GetState(void);

#endif
