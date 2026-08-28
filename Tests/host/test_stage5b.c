#include "default_config.h"
#include "communication_manager.h"
#include "modbus_crc16.h"
#include "modbus_register_model.h"
#include "modbus_register_map.h"
#include "modbus_rtu_server.h"
#include "modbus_rtu_timing.h"
#include "modbus_rtu_framer.h"
#include "rs485_tx_controller.h"
#include "stage5a_model_adapters.h"
#include "stage5b_transport_adapters.h"
#include "uart2_dma_position.h"

#include <stdio.h>
#include <string.h>

static unsigned failures;
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); } } while (0)

static void TestTransportProcess(void *context) { (void)context; }
static bool TestTransportRead(void *context, uint8_t *byte)
{ (void)context; (void)byte; return false; }
static bool TestTransportIdle(void *context, uint16_t *position,
                              uint32_t *timestamp)
{ (void)context; (void)position; (void)timestamp; return false; }
static bool TestTransportError(void *context) { (void)context; return false; }
static void TestTransportDiscard(void *context) { (void)context; }
static uint16_t TestTransportPosition(void *context)
{ (void)context; return 0U; }
static bool TestTransportStartTx(void *context, const uint8_t *data,
                                 uint16_t length)
{ (void)context; return (data != NULL) && (length != 0U); }
static bool TestTransportCompleted(void *context) { (void)context; return true; }
static bool TestTransportTxError(void *context, bool *timeout)
{ (void)context; (void)timeout; return false; }
static void TestTransportAbort(void *context) { (void)context; }

static const ModbusRtuTransport s_test_transport = {
    NULL, TestTransportProcess, TestTransportRead, TestTransportIdle,
    TestTransportError, TestTransportDiscard, TestTransportPosition,
    TestTransportStartTx, TestTransportCompleted, TestTransportTxError,
    TestTransportAbort
};

typedef struct
{
    uint8_t tx[MODBUS_TX_BUFFER_SIZE];
    uint16_t tx_length;
    bool tx_started;
    bool tx_busy;
} DualMockTransport;

static void DualMockProcess(void *context) { (void)context; }
static bool DualMockRead(void *context, uint8_t *byte)
{ (void)context; (void)byte; return false; }
static bool DualMockIdle(void *context, uint16_t *position,
                         uint32_t *timestamp)
{ (void)context; (void)position; (void)timestamp; return false; }
static bool DualMockError(void *context) { (void)context; return false; }
static void DualMockDiscard(void *context) { (void)context; }
static uint16_t DualMockPosition(void *context) { (void)context; return 0U; }
static bool DualMockStartTx(void *context, const uint8_t *data,
                            uint16_t length)
{
    DualMockTransport *mock = (DualMockTransport *)context;
    if ((mock == NULL) || mock->tx_busy || (data == NULL) ||
        (length > sizeof(mock->tx))) return false;
    (void)memcpy(mock->tx, data, length);
    mock->tx_length = length;
    mock->tx_started = true;
    return true;
}
static bool DualMockCompleted(void *context)
{
    DualMockTransport *mock = (DualMockTransport *)context;
    return (mock != NULL) && mock->tx_started && !mock->tx_busy;
}
static bool DualMockTxError(void *context, bool *timeout)
{ (void)context; if (timeout != NULL) *timeout = false; return false; }
static void DualMockAbort(void *context)
{
    DualMockTransport *mock = (DualMockTransport *)context;
    if (mock != NULL) mock->tx_started = false;
}

static void InitDualTransport(ModbusRtuTransport *transport,
                              DualMockTransport *mock)
{
    transport->context = mock;
    transport->process = DualMockProcess;
    transport->try_read_byte = DualMockRead;
    transport->take_idle_event = DualMockIdle;
    transport->take_receive_error = DualMockError;
    transport->discard_pending = DualMockDiscard;
    transport->get_rx_position = DualMockPosition;
    transport->start_tx = DualMockStartTx;
    transport->take_tx_completed = DualMockCompleted;
    transport->take_tx_error = DualMockTxError;
    transport->abort_tx = DualMockAbort;
}

static void PrepareReadyFrame(ModbusRtuFramer *framer, const uint8_t *frame,
                              uint16_t length)
{
    (void)memcpy(framer->frame, frame, length);
    framer->length = length;
    framer->state = MODBUS_FRAMER_FRAME_READY;
}

static void RunServerToTx(ModbusRtuServer *server)
{
    unsigned step;
    for (step = 0U; step < 8U; ++step) ModbusRtuServer_Process(server);
}

static bool InitTestServer(ModbusRtuServer *server, ModbusRtuFramer *framer,
                           const CommunicationConfig *config,
                           CommandSource source)
{
    ModbusRtuTiming timing;
    return ModbusRtuTiming_Calculate(115200U, COMM_PARITY_NONE,
        COMM_STOP_BITS_1, &timing) &&
        ModbusRtuFramer_Init(framer, &timing, 0U) &&
        ModbusRtuServer_Init(server, config, framer, &s_test_transport, source);
}

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
    ModbusRtuFramer framer;
    ModbusRtuServer server;
    uint8_t request[256];
    uint8_t response[256];
    uint16_t request_length;
    uint16_t response_length;
    bool respond;
    DefaultConfig_Load(&config);
    Stage5A_ModelAdaptersInit();
    ModbusRegisterModel_Init();
    CHECK(InitTestServer(&server, &framer, &config.communication,
                         COMMAND_SOURCE_MODBUS));

    request[0]=1U; request[1]=3U; request[2]=0U; request[3]=0U;
    request[4]=0U; request[5]=2U; request_length=AddCrc(request,6U);
    CHECK(ModbusRtuServer_HandleAdu(&server,request,request_length,response,
        sizeof(response),&response_length,&respond));
    CHECK(respond && response_length==9U && response[1]==3U && response[2]==4U);
    CHECK(ModbusCrc16_Calculate(response,response_length)==0U);

    request[0]=1U; request[1]=3U; request[2]=2U; request[3]=3U;
    request[4]=0U; request[5]=1U; request_length=AddCrc(request,6U);
    CHECK(ModbusRtuServer_HandleAdu(&server,request,request_length,response,
        sizeof(response),&response_length,&respond));
    CHECK(respond&&response_length==7U&&response[2]==2U&&
        response[3]==0U&&response[4]==0U);
    request[2]=2U; request[3]=0U; request[4]=0U; request[5]=30U;
    request_length=AddCrc(request,6U);
    CHECK(ModbusRtuServer_HandleAdu(&server,request,request_length,response,
        sizeof(response),&response_length,&respond));
    CHECK(respond&&response_length==65U&&response[2]==60U&&
        response[9]==0U&&response[10]==0U);
    request[2]=2U; request[3]=0x1EU; request[4]=0U; request[5]=1U;
    request_length=AddCrc(request,6U);
    CHECK(ModbusRtuServer_HandleAdu(&server,request,request_length,response,
        sizeof(response),&response_length,&respond));
    CHECK(respond&&ExceptionCode(response,response_length)==2U);

    request[0]=1U; request[1]=6U; request[2]=1U; request[3]=0U;
    request[4]=0U; request[5]=1U; request_length=AddCrc(request,6U);
    CHECK(ModbusRtuServer_HandleAdu(&server,request,request_length,response,
        sizeof(response),&response_length,&respond));
    CHECK(respond && ExceptionCode(response,response_length)==2U);

    request[0]=1U; request[1]=0x7FU; request[2]=0U; request[3]=0U;
    request_length=AddCrc(request,4U);
    CHECK(ModbusRtuServer_HandleAdu(&server,request,request_length,response,
        sizeof(response),&response_length,&respond));
    CHECK(respond && ExceptionCode(response,response_length)==1U);

    request[0]=2U; request[1]=3U; request[2]=0U; request[3]=0U;
    request[4]=0U; request[5]=1U; request_length=AddCrc(request,6U);
    CHECK(ModbusRtuServer_HandleAdu(&server,request,request_length,response,
        sizeof(response),&response_length,&respond) && !respond);
    request[0]=0U; request_length=AddCrc(request,6U);
    CHECK(ModbusRtuServer_HandleAdu(&server,request,request_length,response,
        sizeof(response),&response_length,&respond) && !respond);
    request[0]=1U; request[request_length-1U]^=1U;
    CHECK(ModbusRtuServer_HandleAdu(&server,request,request_length,response,
        sizeof(response),&response_length,&respond) && !respond);

    request[0]=1U; request[1]=16U; request[2]=1U; request[3]=0x40U;
    request[4]=0U; request[5]=2U; request[6]=4U;
    request[7]=0x12U; request[8]=0x34U; request[9]=0x56U; request[10]=0x78U;
    request_length=AddCrc(request,11U);
    CHECK(ModbusRtuServer_HandleAdu(&server,request,request_length,response,
        sizeof(response),&response_length,&respond));
    CHECK(respond && response_length==8U && response[1]==16U);
    request[6]=3U; request_length=AddCrc(request,11U);
    CHECK(ModbusRtuServer_HandleAdu(&server,request,request_length,response,
        sizeof(response),&response_length,&respond));
    CHECK(respond && ExceptionCode(response,response_length)==3U);

    request[0]=1U; request[1]=3U; request[2]=0U; request[3]=0U;
    request[4]=0U; request[5]=0U; request_length=AddCrc(request,6U);
    CHECK(ModbusRtuServer_HandleAdu(&server,request,request_length,response,
        sizeof(response),&response_length,&respond));
    CHECK(ExceptionCode(response,response_length)==3U);
    request[4]=0U; request[5]=126U; request_length=AddCrc(request,6U);
    CHECK(ModbusRtuServer_HandleAdu(&server,request,request_length,response,
        sizeof(response),&response_length,&respond));
    CHECK(ExceptionCode(response,response_length)==3U);
}

static void TestFramerAndRs485(void)
{
    ModbusRtuFramer framer;
    ModbusRtuTiming timing;
    uint8_t frame[8];
    uint8_t output[256];
    uint16_t length;
    unsigned index;
    static const uint8_t tx[] = {1U, 3U, 0U, 0U};

    Stage5B_TransportReset();
    CHECK(ModbusRtuTiming_Calculate(115200U, COMM_PARITY_NONE,
                                    COMM_STOP_BITS_1, &timing));
    CHECK(ModbusRtuFramer_Init(&framer,&timing,0U));
    ModbusRtuFramer_OnTimerEvent(&framer,0U);
    CHECK(ModbusRtuFramer_GetState(&framer) == MODBUS_FRAMER_WAITING);
    frame[0]=1U; frame[1]=3U; frame[2]=0U; frame[3]=0U;
    for(index=0U; index<4U; ++index) ModbusRtuFramer_OnByte(&framer,frame[index]);
    ModbusRtuFramer_OnIdleEvent(&framer,0U,0U);
    CHECK(ModbusRtuFramer_GetState(&framer) == MODBUS_FRAMER_WAIT_T1_5);
    ModbusRtuFramer_OnTimerEvent(&framer,0U);
    CHECK(ModbusRtuFramer_GetState(&framer) == MODBUS_FRAMER_WAIT_T3_5);
    ModbusRtuFramer_OnTimerEvent(&framer,0U);
    CHECK(ModbusRtuFramer_TryGetFrame(&framer,output,sizeof(output),&length));
    CHECK(length==4U && memcmp(output,frame,4U)==0);

    for(index=0U; index<4U; ++index) ModbusRtuFramer_OnByte(&framer,frame[index]);
    Stage5B_SetNowUs(10000U);
    ModbusRtuFramer_OnIdleEvent(&framer,0U, 0U);
    CHECK(ModbusRtuFramer_TryGetFrame(&framer,output,sizeof(output),&length));
    CHECK(length==4U && memcmp(output,frame,4U)==0);

    for(index=0U; index<4U; ++index) ModbusRtuFramer_OnByte(&framer,frame[index]);
    ModbusRtuFramer_OnIdleEvent(&framer,0U,0U);
    ModbusRtuFramer_OnTimerEvent(&framer,0U);
    ModbusRtuFramer_OnByte(&framer,0x55U);
    CHECK(ModbusRtuFramer_GetState(&framer)==MODBUS_FRAMER_DISCARD_UNTIL_SILENCE);
    ModbusRtuFramer_Reset(&framer,0U);
    for(index=0U; index<257U; ++index) ModbusRtuFramer_OnByte(&framer,(uint8_t)index);
    CHECK(ModbusRtuFramer_GetState(&framer)==MODBUS_FRAMER_DISCARD_UNTIL_SILENCE);

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

    Stage5B_TransportReset();
    Rs485TxController_Init();
    CHECK(Rs485TxController_Start(tx,sizeof(tx)));
    Rs485TxController_Process();
    Stage5B_SetNowUs(10U);
    Rs485TxController_Process();
    Stage5B_SetNowUs(200001U);
    Rs485TxController_Process();
    CHECK(Rs485TxController_GetState()==RS485_TX_ERROR);
    CHECK(Rs485TxController_GetLastError()==RS485_TX_ERROR_TIMEOUT);

    Stage5B_TransportReset();
    Stage5B_SetNowUs(UINT32_MAX-5U);
    Rs485TxController_Init();
    CHECK(Rs485TxController_Start(tx,sizeof(tx)));
    Rs485TxController_Process();
    Stage5B_SetNowUs(4U);
    Rs485TxController_Process();
    CHECK(Rs485TxController_GetState()==RS485_TX_DMA_ACTIVE);
    CHECK(Rs485TxController_GetLastError()==RS485_TX_ERROR_NONE);
}

static void TestDmaPositionResolution(void)
{
    uint32_t resolved = 0U;
    bool compensated = false;
    CHECK(Uart2DmaPosition_Resolve(1000U,0U,1008U,1024U,
                                  &resolved,&compensated));
    CHECK(resolved==1008U && !compensated);
    CHECK(Uart2DmaPosition_Resolve(1020U,0U,4U,1024U,
                                  &resolved,&compensated));
    CHECK(resolved==1028U && compensated);
    CHECK(Uart2DmaPosition_Resolve(2040U,1U,8U,1024U,
                                  &resolved,&compensated));
    CHECK(resolved==2056U && compensated);
    CHECK(!Uart2DmaPosition_Resolve(4096U,0U,4U,1024U,
                                   &resolved,&compensated));
    CHECK(Uart2DmaPosition_Resolve(0xFFFFFFF0U,0x00400000U,16U,1024U,
                                  &resolved,&compensated));
    CHECK(resolved==16U && !compensated);
}

static uint64_t JoinHighWords(const uint16_t words[4])
{
    return ((uint64_t)words[0]<<48U)|((uint64_t)words[1]<<32U)|
           ((uint64_t)words[2]<<16U)|words[3];
}

static void SplitHighWords(uint64_t value,uint16_t words[4])
{
    words[0]=(uint16_t)(value>>48U);words[1]=(uint16_t)(value>>32U);
    words[2]=(uint16_t)(value>>16U);words[3]=(uint16_t)value;
}

static void TestAlarmRegisterMap(void)
{
    SystemContext *context;
    AlarmOutputDiagnostics *alarm;
    uint16_t words[32];
    uint16_t encoded[4];
    Stage5A_ModelAdaptersInit();
    context=Stage5A_ModelContext();
    alarm=Stage5A_ModelAlarmDiagnostics();
    context->config.alarm.limit_function_enable=true;
    context->config.alarm.weight_source=ALARM_WEIGHT_NET;
    context->config.alarm.lower_limit_ug=INT64_C(499000000);
    context->config.alarm.upper_limit_ug=INT64_C(501000000);
    context->config.alarm.hysteresis_ug=INT64_C(200000);
    context->config.alarm.internal_buzzer_enable=true;
    context->config.alarm.external_buzzer_enable=true;
    context->config.alarm.qualified_beep_enable=true;
    context->config_revision=0x12345678U;
    context->runtime.config_dirty=true;
    Stage5A_ModelSnapshot()->status_flags|=WEIGHT_STATUS_STABLE;
    alarm->checkweigh_state=CHECKWEIGH_HIGH;
    alarm->buzzer_mode=ALARM_BUZZER_MODE_ALARM;
    alarm->red_active=true;
    alarm->internal_buzzer_active=true;
    ModbusRegisterModel_Init();
    CHECK(ModbusRegisterModel_ReadHolding(0x000EU,1U,words)==MODBUS_REGISTER_OK);
    CHECK(words[0]==MODBUS_REGISTER_MAP_VERSION);
    CHECK(ModbusRegisterModel_ReadHolding(MODBUS_ALARM_ACTIVE_FIRST,28U,words)==MODBUS_REGISTER_OK);
    CHECK(words[0]==1U&&words[1]==ALARM_WEIGHT_NET);
    CHECK((int64_t)JoinHighWords(&words[2])==INT64_C(499000000));
    CHECK((int64_t)JoinHighWords(&words[6])==INT64_C(501000000));
    CHECK((int64_t)JoinHighWords(&words[10])==INT64_C(200000));
    CHECK(words[14]==1U&&words[15]==1U&&words[16]==1U);
    CHECK(words[17]==3U&&words[18]==1U&&words[19]==1U);
    CHECK(words[20]==0U&&words[21]==0U&&words[22]==1U);
    CHECK(words[23]==1U&&words[24]==0U);
    CHECK(words[25]==0x1234U&&words[26]==0x5678U&&words[27]==1U);
    CHECK(ModbusRegisterModel_WriteSingle(0x0240U,2U,COMMAND_SOURCE_MODBUS)==MODBUS_REGISTER_ILLEGAL_VALUE);
    CHECK(ModbusRegisterModel_WriteSingle(0x0241U,2U,COMMAND_SOURCE_MODBUS)==MODBUS_REGISTER_ILLEGAL_VALUE);
    CHECK(ModbusRegisterModel_WriteSingle(0x0242U,0U,COMMAND_SOURCE_MODBUS)==MODBUS_REGISTER_ILLEGAL_VALUE);
    SplitHighWords((uint64_t)INT64_MIN,encoded);
    CHECK(ModbusRegisterModel_WriteMultiple(0x0242U,3U,encoded,COMMAND_SOURCE_MODBUS)==MODBUS_REGISTER_ILLEGAL_VALUE);
    CHECK(ModbusRegisterModel_WriteMultiple(0x0242U,4U,encoded,COMMAND_SOURCE_MODBUS)==MODBUS_REGISTER_OK);
    CHECK(ModbusRegisterModel_ReadHolding(0x0242U,4U,words)==MODBUS_REGISTER_OK);
    CHECK((int64_t)JoinHighWords(words)==INT64_MIN);
    SplitHighWords(UINT64_MAX,encoded);
    CHECK(ModbusRegisterModel_WriteMultiple(0x024AU,4U,encoded,COMMAND_SOURCE_MODBUS)==MODBUS_REGISTER_OK);
    CHECK(ModbusRegisterModel_ReadHolding(0x024AU,4U,words)==MODBUS_REGISTER_OK);
    CHECK(JoinHighWords(words)==UINT64_MAX);
}

static void TestDeterministicBadFrames(void)
{
    DeviceConfig config;
    ModbusRtuFramer framer;
    ModbusRtuServer server;
    uint32_t state = 0x13579BDFU;
    uint8_t frame[256];
    uint8_t response[256];
    uint16_t length;
    uint16_t response_length;
    unsigned iteration;
    bool respond;
    DefaultConfig_Load(&config);
    CHECK(InitTestServer(&server, &framer, &config.communication,
                         COMMAND_SOURCE_MODBUS));
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
        CHECK(ModbusRtuServer_HandleAdu(&server,frame,length,response,
            sizeof(response),&response_length,&respond));
        CHECK(response_length <= sizeof(response));
    }
}

static void TestDualPortInstances(void)
{
    DeviceConfig config;
    ModbusRtuFramer framer2;
    ModbusRtuFramer framer3;
    ModbusRtuServer server2;
    ModbusRtuServer server3;
    ModbusRtuTiming timing;
    uint8_t request2[8] = {1U, 3U, 0U, 0U, 0U, 2U};
    uint8_t request3[8] = {1U, 3U, 0U, 2U, 0U, 1U};
    uint8_t response2[256];
    uint8_t response3[256];
    uint8_t damaged[8];
    uint16_t length2;
    uint16_t length3;
    uint16_t index;
    bool respond2;
    bool respond3;
    DefaultConfig_Load(&config);
    Stage5A_ModelAdaptersInit();
    ModbusRegisterModel_Init();
    CHECK(ModbusRtuTiming_Calculate(115200U, COMM_PARITY_NONE,
        COMM_STOP_BITS_1, &timing));
    CHECK(ModbusRtuFramer_Init(&framer2, &timing, 0U));
    CHECK(ModbusRtuFramer_Init(&framer3, &timing, 0U));
    ModbusRtuFramer_OnTimerEvent(&framer2, 0U);
    ModbusRtuFramer_OnTimerEvent(&framer3, 0U);
    CHECK(ModbusRtuServer_Init(&server2, &config.communication, &framer2,
        &s_test_transport, COMMAND_SOURCE_MODBUS));
    CHECK(ModbusRtuServer_Init(&server3, &config.communication, &framer3,
        &s_test_transport, COMMAND_SOURCE_MODBUS_USART3));
    length2 = AddCrc(request2, 6U);
    length3 = AddCrc(request3, 6U);
    for (index = 0U; index < length2; ++index)
        ModbusRtuFramer_OnByte(&framer2, request2[index]);
    for (index = 0U; index < length3; ++index)
        ModbusRtuFramer_OnByte(&framer3, request3[index]);
    ModbusRtuFramer_OnIdleEvent(&framer2, 8U, 0U);
    ModbusRtuFramer_OnIdleEvent(&framer3, 8U, 0U);
    ModbusRtuFramer_OnTimerEvent(&framer2, 8U);
    ModbusRtuFramer_OnTimerEvent(&framer3, 8U);
    ModbusRtuFramer_OnTimerEvent(&framer2, 8U);
    ModbusRtuFramer_OnTimerEvent(&framer3, 8U);
    CHECK(ModbusRtuFramer_TryGetFrame(&framer2, response2, sizeof(response2),
        &length2));
    CHECK(ModbusRtuFramer_TryGetFrame(&framer3, response3, sizeof(response3),
        &length3));
    CHECK(ModbusRtuServer_HandleAdu(&server2, response2, length2, response2,
        sizeof(response2), &length2, &respond2));
    CHECK(ModbusRtuServer_HandleAdu(&server3, response3, length3, response3,
        sizeof(response3), &length3, &respond3));
    CHECK(respond2 && respond3 && length2 == 9U && length3 == 7U);
    CHECK(response2[0] == 1U && response2[1] == 3U && response2[2] == 4U &&
        ModbusCrc16_Calculate(response2, length2) == 0U);
    CHECK(response3[0] == 1U && response3[1] == 3U && response3[2] == 2U &&
        ModbusCrc16_Calculate(response3, length3) == 0U);
    (void)memcpy(damaged, request2, sizeof(damaged));
    damaged[7] ^= 0x01U;
    CHECK(ModbusRtuServer_HandleAdu(&server2, damaged, sizeof(damaged),
        response2, sizeof(response2), &length2, &respond2) && !respond2);
    CHECK(ModbusRtuServer_GetStatistics(&server2)->crc_error_count == 1U);
    CHECK(ModbusRtuServer_GetStatistics(&server3)->crc_error_count == 0U);
    damaged[0] = 2U;
    length2 = AddCrc(damaged, 6U);
    CHECK(ModbusRtuServer_HandleAdu(&server2, damaged, length2, response2,
        sizeof(response2), &length2, &respond2) && !respond2);
    CHECK(ModbusRtuServer_GetStatistics(&server2)->ignored_address_count == 1U);
    CHECK(ModbusRegisterModel_WriteSingle(0x0040U, 42U,
        COMMAND_SOURCE_MODBUS) == MODBUS_REGISTER_OK);
    CHECK(ModbusRegisterModel_WriteSingle(0x0040U, 43U,
        COMMAND_SOURCE_MODBUS_USART3) == MODBUS_REGISTER_BUSY);
    CHECK(ModbusRegisterModel_WriteSingle(0x004BU, MODBUS_EXECUTE_VALUE,
        COMMAND_SOURCE_MODBUS) == MODBUS_REGISTER_OK);
    CHECK(ModbusRegisterModel_WriteSingle(0x0040U, 43U,
        COMMAND_SOURCE_MODBUS_USART3) == MODBUS_REGISTER_OK);
    CHECK(ModbusRegisterModel_WriteSingle(0x004BU, MODBUS_EXECUTE_VALUE,
        COMMAND_SOURCE_MODBUS_USART3) == MODBUS_REGISTER_OK);
}

static void TestDualMockTransportRouting(void)
{
    DeviceConfig config;
    ModbusRtuTiming timing;
    ModbusRtuFramer framer2;
    ModbusRtuFramer framer3;
    ModbusRtuServer server2;
    ModbusRtuServer server3;
    ModbusRtuTransport transport2;
    ModbusRtuTransport transport3;
    DualMockTransport mock2 = {0};
    DualMockTransport mock3 = {0};
    uint8_t fc03[8] = {1U, 3U, 0U, 0U, 0U, 1U};
    uint8_t fc06[8] = {1U, 6U, 0x01U, 0x7CU, 0U, 1U};
    uint8_t fc16[13] = {1U, 16U, 0x01U, 0x40U, 0U, 2U, 4U,
                        0U, 1U, 0U, 2U};
    uint16_t length;
    uint16_t response_length;
    bool respond;
    DefaultConfig_Load(&config);
    config.communication.response_delay_ms = 0U;
    Stage5A_ModelAdaptersInit();
    ModbusRegisterModel_Init();
    CHECK(ModbusRtuTiming_Calculate(115200U, COMM_PARITY_NONE,
        COMM_STOP_BITS_1, &timing));
    InitDualTransport(&transport2, &mock2);
    InitDualTransport(&transport3, &mock3);
    CHECK(ModbusRtuFramer_Init(&framer2, &timing, 0U));
    CHECK(ModbusRtuFramer_Init(&framer3, &timing, 0U));
    CHECK(ModbusRtuServer_Init(&server2, &config.communication, &framer2,
        &transport2, COMMAND_SOURCE_MODBUS));
    CHECK(ModbusRtuServer_Init(&server3, &config.communication, &framer3,
        &transport3, COMMAND_SOURCE_MODBUS_USART3));
    length = AddCrc(fc03, 6U);
    PrepareReadyFrame(&framer2, fc03, length);
    PrepareReadyFrame(&framer3, fc03, length);
    RunServerToTx(&server2);
    RunServerToTx(&server3);
    CHECK(mock2.tx_started && mock3.tx_started);
    CHECK(mock2.tx_length == 7U && mock3.tx_length == 7U);
    CHECK(ModbusCrc16_Calculate(mock2.tx, mock2.tx_length) == 0U);
    CHECK(ModbusCrc16_Calculate(mock3.tx, mock3.tx_length) == 0U);
    CHECK(mock2.tx[0] == 1U && mock2.tx[1] == 3U &&
        mock3.tx[0] == 1U && mock3.tx[1] == 3U);
    mock2.tx_started = false;
    mock3.tx_started = false;
    length = AddCrc(fc06, 6U);
    PrepareReadyFrame(&framer2, fc06, length);
    RunServerToTx(&server2);
    CHECK(mock2.tx_started && mock2.tx_length == 8U &&
        memcmp(mock2.tx, fc06, 8U) == 0 &&
        ModbusCrc16_Calculate(mock2.tx, 8U) == 0U);
    mock2.tx_started = false;
    fc06[5] = 2U;
    length = AddCrc(fc06, 6U);
    PrepareReadyFrame(&framer3, fc06, length);
    RunServerToTx(&server3);
    CHECK(mock3.tx_started && mock3.tx[0] == 1U && mock3.tx[1] == 0x86U &&
        mock3.tx[2] == 3U && ModbusCrc16_Calculate(mock3.tx, 5U) == 0U);
    mock3.tx_started = false;
    length = AddCrc(fc16, 11U);
    PrepareReadyFrame(&framer2, fc16, length);
    RunServerToTx(&server2);
    CHECK(mock2.tx_started && mock2.tx[1] == 16U && mock2.tx_length == 8U &&
        ModbusCrc16_Calculate(mock2.tx, mock2.tx_length) == 0U);
    mock2.tx_started = false;
    fc16[6] = 3U;
    length = AddCrc(fc16, 11U);
    CHECK(ModbusRtuServer_HandleAdu(&server3, fc16, length, mock3.tx,
        sizeof(mock3.tx), &response_length, &respond) && respond &&
        response_length == 5U && mock3.tx[2] == 3U);
    CHECK(ModbusCrc16_Calculate(mock3.tx, response_length) == 0U);
    mock2.tx_busy = true;
    PrepareReadyFrame(&framer2, fc03, AddCrc(fc03, 6U));
    RunServerToTx(&server2);
    CHECK(!mock2.tx_started);
    mock2.tx_busy = false;
    mock3.tx_started = false;
    PrepareReadyFrame(&framer3, fc03, AddCrc(fc03, 6U));
    RunServerToTx(&server3);
    CHECK(mock3.tx_started && mock3.tx[1] == 3U);
}

int main(void)
{
    TestCrcAndTiming();
    TestCommunicationManagerStart();
    TestServer();
    TestFramerAndRs485();
    TestDmaPositionResolution();
    TestAlarmRegisterMap();
    TestDeterministicBadFrames();
    TestDualPortInstances();
    TestDualMockTransportRouting();
    CHECK(checks >= 128U);
    if(failures==0U) printf("Stage 5B host tests passed (%u checks).\n",checks);
    return failures==0U?0:1;
}
