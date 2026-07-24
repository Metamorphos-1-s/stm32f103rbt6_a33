#include "default_config.h"
#include "communication_manager.h"
#include "modbus_crc16.h"
#include "modbus_register_model.h"
#include "modbus_rtu_server.h"
#include "modbus_rtu_timing.h"
#include "modbus_rtu_framer.h"
#include "rs485_tx_controller.h"
#include "stage5a_model_adapters.h"
#include "stage5b_transport_adapters.h"

#include <stdio.h>
#include <string.h>

static unsigned failures;
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); } } while (0)

static uint16_t AddCrc(uint8_t *frame, uint16_t length)
{
    uint16_t crc = ModbusCrc16_Calculate(frame, length);
    frame[length++] = (uint8_t)crc;
    frame[length++] = (uint8_t)(crc >> 8U);
    return length;
}

static uint8_t ExceptionCode(const uint8_t *response, uint16_t length)
{
    return (length == 5U) ? response[2] : 0U;
}

static void TestCrcAndTiming(void)
{
    static const uint8_t text[] = "123456789";
    static const uint8_t request[] = {1U,3U,0U,0U,0U,10U};
    uint8_t damaged[sizeof(request)];
    ModbusRtuTiming timing;
    CHECK(ModbusCrc16_Calculate(text, 9U) == 0x4B37U);
    CHECK(ModbusCrc16_Calculate(request, sizeof(request)) == 0xCDC5U);
    (void)memcpy(damaged, request, sizeof(request));
    damaged[2] ^= 1U;
    CHECK(ModbusCrc16_Calculate(damaged, sizeof(damaged)) != 0xCDC5U);
    CHECK(ModbusCrc16_Update(0xFFFFU, NULL, 0U) == 0xFFFFU);
    CHECK(ModbusCrc16_Update(0xFFFFU, NULL, 1U) == 0U);
    CHECK(ModbusRtuTiming_Calculate(9600U, COMM_PARITY_NONE,
                                    COMM_STOP_BITS_1, &timing));
    CHECK(timing.character_time_us == 1042U);
    CHECK(timing.t1_5_us == 1563U);
    CHECK(timing.t3_5_us == 3646U);
    CHECK(ModbusRtuTiming_Calculate(9600U, COMM_PARITY_EVEN,
                                    COMM_STOP_BITS_2, &timing));
    CHECK(timing.character_time_us == 1250U);
    CHECK(timing.t1_5_us == 1875U && timing.t3_5_us == 4375U);
    CHECK(ModbusRtuTiming_Calculate(115200U, COMM_PARITY_ODD,
                                    COMM_STOP_BITS_1, &timing));
    CHECK(timing.t1_5_us == 750U && timing.t3_5_us == 1750U);
    CHECK(!ModbusRtuTiming_Calculate(4800U, COMM_PARITY_NONE,
                                     COMM_STOP_BITS_1, &timing));
}

static void TestCommunicationManagerStart(void)
{
    DeviceConfig config;
    DefaultConfig_Load(&config);
    Stage5B_TransportReset();
    CHECK(CommunicationManager_Init(&config.communication));
    CHECK(CommunicationManager_GetState()==COMM_STATE_RUNNING);
    CHECK(CommunicationManager_GetActiveConfig()->baud_rate==115200U);
}

static void TestServer(void)
{
    DeviceConfig config;
    uint8_t request[256];
    uint8_t response[256];
    uint16_t request_length;
    uint16_t response_length;
    bool respond;
    DefaultConfig_Load(&config);
    Stage5A_ModelAdaptersInit();
    ModbusRegisterModel_Init();
    CHECK(ModbusRtuServer_Init(&config.communication));

    request[0]=1U; request[1]=3U; request[2]=0U; request[3]=0U;
    request[4]=0U; request[5]=2U; request_length=AddCrc(request,6U);
    CHECK(ModbusRtuServer_HandleAdu(request,request_length,response,
        sizeof(response),&response_length,&respond));
    CHECK(respond && response_length==9U && response[1]==3U && response[2]==4U);
    CHECK(ModbusCrc16_Calculate(response,response_length)==0U);

    request[0]=1U; request[1]=6U; request[2]=1U; request[3]=0U;
    request[4]=0U; request[5]=1U; request_length=AddCrc(request,6U);
    CHECK(ModbusRtuServer_HandleAdu(request,request_length,response,
        sizeof(response),&response_length,&respond));
    CHECK(respond && ExceptionCode(response,response_length)==2U);

    request[0]=1U; request[1]=0x7FU; request[2]=0U; request[3]=0U;
    request_length=AddCrc(request,4U);
    CHECK(ModbusRtuServer_HandleAdu(request,request_length,response,
        sizeof(response),&response_length,&respond));
    CHECK(respond && ExceptionCode(response,response_length)==1U);

    request[0]=2U; request[1]=3U; request[2]=0U; request[3]=0U;
    request[4]=0U; request[5]=1U; request_length=AddCrc(request,6U);
    CHECK(ModbusRtuServer_HandleAdu(request,request_length,response,
        sizeof(response),&response_length,&respond) && !respond);
    request[0]=0U; request_length=AddCrc(request,6U);
    CHECK(ModbusRtuServer_HandleAdu(request,request_length,response,
        sizeof(response),&response_length,&respond) && !respond);
    request[0]=1U; request[request_length-1U]^=1U;
    CHECK(ModbusRtuServer_HandleAdu(request,request_length,response,
        sizeof(response),&response_length,&respond) && !respond);

    request[0]=1U; request[1]=16U; request[2]=1U; request[3]=0x40U;
    request[4]=0U; request[5]=2U; request[6]=4U;
    request[7]=0x12U; request[8]=0x34U; request[9]=0x56U; request[10]=0x78U;
    request_length=AddCrc(request,11U);
    CHECK(ModbusRtuServer_HandleAdu(request,request_length,response,
        sizeof(response),&response_length,&respond));
    CHECK(respond && response_length==8U && response[1]==16U);
    request[6]=3U; request_length=AddCrc(request,11U);
    CHECK(ModbusRtuServer_HandleAdu(request,request_length,response,
        sizeof(response),&response_length,&respond));
    CHECK(respond && ExceptionCode(response,response_length)==3U);

    request[0]=1U; request[1]=3U; request[2]=0U; request[3]=0U;
    request[4]=0U; request[5]=0U; request_length=AddCrc(request,6U);
    CHECK(ModbusRtuServer_HandleAdu(request,request_length,response,
        sizeof(response),&response_length,&respond));
    CHECK(ExceptionCode(response,response_length)==3U);
    request[4]=0U; request[5]=126U; request_length=AddCrc(request,6U);
    CHECK(ModbusRtuServer_HandleAdu(request,request_length,response,
        sizeof(response),&response_length,&respond));
    CHECK(ExceptionCode(response,response_length)==3U);
}

static void TestFramerAndRs485(void)
{
    ModbusRtuTiming timing;
    uint8_t frame[8];
    uint8_t output[256];
    uint16_t length;
    unsigned index;
    static const uint8_t tx[] = {1U, 3U, 0U, 0U};

    Stage5B_TransportReset();
    CHECK(ModbusRtuTiming_Calculate(115200U, COMM_PARITY_NONE,
                                    COMM_STOP_BITS_1, &timing));
    CHECK(ModbusRtuFramer_Init(&timing));
    ModbusRtuFramer_OnTimerEvent();
    CHECK(ModbusRtuFramer_GetState() == MODBUS_FRAMER_WAITING);
    frame[0]=1U; frame[1]=3U; frame[2]=0U; frame[3]=0U;
    for(index=0U; index<4U; ++index) ModbusRtuFramer_OnByte(frame[index]);
    ModbusRtuFramer_OnIdleEvent(0U,0U);
    CHECK(ModbusRtuFramer_GetState() == MODBUS_FRAMER_WAIT_T1_5);
    ModbusRtuFramer_OnTimerEvent();
    CHECK(ModbusRtuFramer_GetState() == MODBUS_FRAMER_WAIT_T3_5);
    ModbusRtuFramer_OnTimerEvent();
    CHECK(ModbusRtuFramer_TryGetFrame(output,sizeof(output),&length));
    CHECK(length==4U && memcmp(output,frame,4U)==0);

    for(index=0U; index<4U; ++index) ModbusRtuFramer_OnByte(frame[index]);
    ModbusRtuFramer_OnIdleEvent(0U,0U);
    ModbusRtuFramer_OnTimerEvent();
    ModbusRtuFramer_OnByte(0x55U);
    CHECK(ModbusRtuFramer_GetState()==MODBUS_FRAMER_DISCARD_UNTIL_SILENCE);
    ModbusRtuFramer_Reset();
    for(index=0U; index<257U; ++index) ModbusRtuFramer_OnByte((uint8_t)index);
    CHECK(ModbusRtuFramer_GetState()==MODBUS_FRAMER_DISCARD_UNTIL_SILENCE);

    Stage5B_TransportReset();
    Rs485TxController_Init();
    CHECK(!Stage5B_IsDeAsserted());
    CHECK(Rs485TxController_Start(tx,sizeof(tx)));
    Rs485TxController_Process();
    CHECK(Stage5B_IsDeAsserted() &&
          Rs485TxController_GetState()==RS485_TX_WAIT_DE_SETUP);
    Stage5B_SetNowUs(10U);
    Rs485TxController_Process();
    CHECK(Rs485TxController_GetState()==RS485_TX_DMA_ACTIVE);
    Rs485TxController_Process();
    CHECK(Stage5B_IsDeAsserted());
    Stage5B_SetTxComplete(true);
    Rs485TxController_Process();
    CHECK(Rs485TxController_GetState()==RS485_TX_WAIT_UART_TC &&
          Stage5B_IsDeAsserted());
    Rs485TxController_Process();
    CHECK(Rs485TxController_GetState()==RS485_TX_WAIT_DE_HOLD);
    Stage5B_SetNowUs(20U);
    Rs485TxController_Process();
    Rs485TxController_Process();
    CHECK(!Stage5B_IsDeAsserted() && Rs485TxController_TakeCompleted());
}

static void TestDeterministicBadFrames(void)
{
    uint32_t state = 0x13579BDFU;
    uint8_t frame[256];
    uint8_t response[256];
    uint16_t length;
    uint16_t response_length;
    unsigned iteration;
    bool respond;
    for (iteration=0U; iteration<512U; ++iteration)
    {
        uint16_t index;
        state = state * 1664525U + 1013904223U;
        length = (uint16_t)(state % 257U);
        for (index=0U; index<length; ++index)
        {
            state = state * 1664525U + 1013904223U;
            frame[index]=(uint8_t)(state >> 24U);
        }
        if (length >= 4U)
        {
            frame[0]=1U;
            frame[length-2U]=0U;
            frame[length-1U]=0U;
        }
        CHECK(ModbusRtuServer_HandleAdu(frame,length,response,
            sizeof(response),&response_length,&respond));
        CHECK(response_length <= sizeof(response));
    }
}

int main(void)
{
    TestCrcAndTiming();
    TestCommunicationManagerStart();
    TestServer();
    TestFramerAndRs485();
    TestDeterministicBadFrames();
    CHECK(checks >= 128U);
    if(failures==0U) printf("Stage 5B host tests passed (%u checks).\n",checks);
    return failures==0U?0:1;
}
