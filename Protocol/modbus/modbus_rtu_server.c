#include "modbus_rtu_server.h"

#include "bsp_time.h"
#include "modbus_crc16.h"
#include "modbus_register_model.h"
#include "modbus_rtu_framer.h"
#include "rs485_tx_controller.h"
#include "uart2_dma_transport.h"

#include <stddef.h>
#include <string.h>

#define MODBUS_FC_READ_HOLDING 0x03U
#define MODBUS_FC_WRITE_SINGLE 0x06U
#define MODBUS_FC_WRITE_MULTIPLE 0x10U
#define MODBUS_EXCEPTION_ILLEGAL_FUNCTION 0x01U
#define MODBUS_EXCEPTION_ILLEGAL_ADDRESS 0x02U
#define MODBUS_EXCEPTION_ILLEGAL_VALUE 0x03U
#define MODBUS_EXCEPTION_DEVICE_FAILURE 0x04U
#define MODBUS_EXCEPTION_DEVICE_BUSY 0x06U

static CommunicationConfig s_config;
static ModbusRtuServerState s_state;
static ModbusRtuServerStatistics s_statistics;
static uint8_t s_request[MODBUS_RTU_ADU_MAX_SIZE];
static uint8_t s_response[MODBUS_TX_BUFFER_SIZE];
static uint16_t s_registers[125U];
static uint16_t s_request_length;
static uint16_t s_response_length;
static uint32_t s_delay_start_ms;
static bool s_suspended;

static uint16_t ReadBe16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8U) | data[1]);
}

static void WriteBe16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value >> 8U);
    data[1] = (uint8_t)value;
}

static uint8_t MapException(ModbusRegisterResult result)
{
    switch (result)
    {
        case MODBUS_REGISTER_ILLEGAL_ADDRESS:
        case MODBUS_REGISTER_READ_ONLY: return MODBUS_EXCEPTION_ILLEGAL_ADDRESS;
        case MODBUS_REGISTER_ILLEGAL_VALUE: return MODBUS_EXCEPTION_ILLEGAL_VALUE;
        case MODBUS_REGISTER_BUSY: return MODBUS_EXCEPTION_DEVICE_BUSY;
        case MODBUS_REGISTER_DEVICE_FAILURE: return MODBUS_EXCEPTION_DEVICE_FAILURE;
        case MODBUS_REGISTER_OK:
        default: return 0U;
    }
}

static bool AppendCrc(uint8_t *response, uint16_t capacity,
                      uint16_t *length)
{
    uint16_t crc;
    if ((*length > capacity) || ((uint16_t)(capacity - *length) < 2U)) return false;
    crc = ModbusCrc16_Calculate(response, *length);
    response[(*length)++] = (uint8_t)crc;
    response[(*length)++] = (uint8_t)(crc >> 8U);
    return true;
}

static bool BuildException(uint8_t address, uint8_t function,
                           uint8_t exception, uint8_t *response,
                           uint16_t capacity, uint16_t *length)
{
    if (capacity < 5U) return false;
    response[0] = address;
    response[1] = (uint8_t)(function | 0x80U);
    response[2] = exception;
    *length = 3U;
    s_statistics.last_exception = exception;
    ++s_statistics.exception_response_count;
    return AppendCrc(response, capacity, length);
}

static bool HandleRead(const uint8_t *request, uint16_t request_length,
                       uint8_t *response, uint16_t capacity,
                       uint16_t *length)
{
    uint16_t start;
    uint16_t quantity;
    uint16_t index;
    uint8_t exception;
    ModbusRegisterResult result;
    if (request_length != 8U)
    {
        ++s_statistics.length_error_count;
        return BuildException(request[0], request[1],
            MODBUS_EXCEPTION_ILLEGAL_VALUE, response, capacity, length);
    }
    start = ReadBe16(&request[2]);
    quantity = ReadBe16(&request[4]);
    if ((quantity == 0U) || (quantity > 125U) ||
        ((uint32_t)quantity * 2U + 5U > capacity))
        return BuildException(request[0], request[1],
            MODBUS_EXCEPTION_ILLEGAL_VALUE, response, capacity, length);
    result = ModbusRegisterModel_ReadHolding(start, quantity, s_registers);
    exception = MapException(result);
    if (exception != 0U)
        return BuildException(request[0], request[1], exception,
                              response, capacity, length);
    response[0] = request[0];
    response[1] = request[1];
    response[2] = (uint8_t)(quantity * 2U);
    *length = 3U;
    for (index = 0U; index < quantity; ++index)
    {
        WriteBe16(&response[*length], s_registers[index]);
        *length = (uint16_t)(*length + 2U);
    }
    return AppendCrc(response, capacity, length);
}

static bool HandleWriteSingle(const uint8_t *request, uint16_t request_length,
                              uint8_t *response, uint16_t capacity,
                              uint16_t *length)
{
    uint8_t exception;
    ModbusRegisterResult result;
    if (request_length != 8U)
    {
        ++s_statistics.length_error_count;
        return BuildException(request[0], request[1],
            MODBUS_EXCEPTION_ILLEGAL_VALUE, response, capacity, length);
    }
    result = ModbusRegisterModel_WriteSingle(ReadBe16(&request[2]),
                                             ReadBe16(&request[4]));
    exception = MapException(result);
    if (exception != 0U)
        return BuildException(request[0], request[1], exception,
                              response, capacity, length);
    if (capacity < 8U) return false;
    (void)memcpy(response, request, 6U);
    *length = 6U;
    return AppendCrc(response, capacity, length);
}

static bool HandleWriteMultiple(const uint8_t *request,
                                uint16_t request_length, uint8_t *response,
                                uint16_t capacity, uint16_t *length)
{
    uint16_t start;
    uint16_t quantity;
    uint16_t expected;
    uint16_t index;
    uint8_t exception;
    ModbusRegisterResult result;
    if (request_length < 9U)
    {
        ++s_statistics.length_error_count;
        return BuildException(request[0], request[1],
            MODBUS_EXCEPTION_ILLEGAL_VALUE, response, capacity, length);
    }
    start = ReadBe16(&request[2]);
    quantity = ReadBe16(&request[4]);
    expected = (uint16_t)(9U + (uint32_t)quantity * 2U);
    if ((quantity == 0U) || (quantity > 123U) ||
        (request[6] != (uint8_t)(quantity * 2U)) ||
        (request_length != expected))
        return BuildException(request[0], request[1],
            MODBUS_EXCEPTION_ILLEGAL_VALUE, response, capacity, length);
    for (index = 0U; index < quantity; ++index)
        s_registers[index] = ReadBe16(&request[7U + index * 2U]);
    result = ModbusRegisterModel_WriteMultiple(start, quantity, s_registers);
    exception = MapException(result);
    if (exception != 0U)
        return BuildException(request[0], request[1], exception,
                              response, capacity, length);
    if (capacity < 8U) return false;
    (void)memcpy(response, request, 6U);
    *length = 6U;
    return AppendCrc(response, capacity, length);
}

bool ModbusRtuServer_HandleAdu(const uint8_t *request, uint16_t request_length,
                              uint8_t *response, uint16_t response_capacity,
                              uint16_t *response_length, bool *respond)
{
    uint16_t received_crc;
    uint16_t calculated_crc;
    bool built;
    if ((request == NULL) || (response == NULL) || (response_length == NULL) ||
        (respond == NULL) || (request_length > MODBUS_RTU_ADU_MAX_SIZE))
        return false;
    *respond = false;
    *response_length = 0U;
    if (request_length < 4U)
    {
        ++s_statistics.length_error_count;
        return true;
    }
    received_crc = (uint16_t)(request[request_length - 2U] |
        ((uint16_t)request[request_length - 1U] << 8U));
    calculated_crc = ModbusCrc16_Calculate(request,
                                           (uint16_t)(request_length - 2U));
    if (received_crc != calculated_crc)
    {
        ++s_statistics.crc_error_count;
        return true;
    }
    ++s_statistics.valid_frame_count;
    s_statistics.last_request_address = request[0];
    s_statistics.last_function = request[1];
    s_statistics.last_request_length = request_length;
    if (request[0] == 0U)
    {
        ++s_statistics.broadcast_count;
        return true;
    }
    if (request[0] != s_config.modbus_address)
    {
        ++s_statistics.ignored_address_count;
        return true;
    }
    ++s_statistics.addressed_frame_count;
    if (request[1] == MODBUS_FC_READ_HOLDING)
    {
        ++s_statistics.function03_count;
        built = HandleRead(request, request_length, response,
                           response_capacity, response_length);
    }
    else if (request[1] == MODBUS_FC_WRITE_SINGLE)
    {
        ++s_statistics.function06_count;
        built = HandleWriteSingle(request, request_length, response,
                                  response_capacity, response_length);
    }
    else if (request[1] == MODBUS_FC_WRITE_MULTIPLE)
    {
        ++s_statistics.function16_count;
        built = HandleWriteMultiple(request, request_length, response,
                                    response_capacity, response_length);
    }
    else
    {
        ++s_statistics.illegal_function_count;
        built = BuildException(request[0], request[1],
            MODBUS_EXCEPTION_ILLEGAL_FUNCTION, response, response_capacity,
            response_length);
    }
    if (!built) return false;
    *respond = true;
    s_statistics.last_response_length = *response_length;
    return true;
}

bool ModbusRtuServer_Init(const CommunicationConfig *config)
{
    if ((config == NULL) || (config->modbus_address < 1U) ||
        (config->modbus_address > 247U) || (config->response_delay_ms > 1000U))
        return false;
    s_config = *config;
    (void)memset(&s_statistics, 0, sizeof(s_statistics));
    s_state = MODBUS_SERVER_IDLE;
    s_suspended = false;
    Rs485TxController_Init();
    return true;
}

void ModbusRtuServer_Process(void)
{
    bool respond;
    switch (s_state)
    {
        case MODBUS_SERVER_IDLE:
            if (!s_suspended && ModbusRtuFramer_TryGetFrame(s_request,
                sizeof(s_request), &s_request_length))
                s_state = MODBUS_SERVER_FRAME_PENDING;
            break;
        case MODBUS_SERVER_FRAME_PENDING:
            s_state = MODBUS_SERVER_PROCESSING;
            break;
        case MODBUS_SERVER_PROCESSING:
            if (!ModbusRtuServer_HandleAdu(s_request, s_request_length,
                s_response, sizeof(s_response), &s_response_length, &respond))
                s_state = MODBUS_SERVER_ERROR;
            else if (!respond) s_state = MODBUS_SERVER_IDLE;
            else
            {
                s_delay_start_ms = BSP_TimeNowMs();
                s_state = MODBUS_SERVER_RESPONSE_DELAY;
            }
            break;
        case MODBUS_SERVER_RESPONSE_DELAY:
            if ((uint32_t)(BSP_TimeNowMs() - s_delay_start_ms) >=
                s_config.response_delay_ms) s_state = MODBUS_SERVER_TX_PENDING;
            break;
        case MODBUS_SERVER_TX_PENDING:
            if (Rs485TxController_Start(s_response, s_response_length))
                s_state = MODBUS_SERVER_TX_ACTIVE;
            else
            {
                ++s_statistics.tx_error_count;
                s_state = MODBUS_SERVER_ERROR;
            }
            break;
        case MODBUS_SERVER_TX_ACTIVE:
            Rs485TxController_Process();
            s_state = MODBUS_SERVER_WAIT_TX_COMPLETE;
            break;
        case MODBUS_SERVER_WAIT_TX_COMPLETE:
            Rs485TxController_Process();
            if (Rs485TxController_TakeCompleted())
            {
                ++s_statistics.tx_response_count;
                s_state = MODBUS_SERVER_IDLE;
            }
            else if (Rs485TxController_GetState() == RS485_TX_ERROR)
            {
                ++s_statistics.tx_error_count;
                Rs485TxController_Abort();
                s_state = MODBUS_SERVER_ERROR;
            }
            break;
        case MODBUS_SERVER_ERROR:
            Rs485TxController_Abort();
            s_state = MODBUS_SERVER_IDLE;
            break;
        default:
            s_state = MODBUS_SERVER_ERROR;
            break;
    }
}

bool ModbusRtuServer_IsBusy(void) { return s_state != MODBUS_SERVER_IDLE; }
void ModbusRtuServer_Suspend(void)
{
    s_suspended = true;
    Rs485TxController_Abort();
}
bool ModbusRtuServer_Resume(const CommunicationConfig *config)
{
    if (config == NULL) return false;
    s_config = *config;
    s_suspended = false;
    s_state = MODBUS_SERVER_IDLE;
    return true;
}
ModbusRtuServerState ModbusRtuServer_GetState(void) { return s_state; }
const ModbusRtuServerStatistics *ModbusRtuServer_GetStatistics(void)
{
    return &s_statistics;
}
