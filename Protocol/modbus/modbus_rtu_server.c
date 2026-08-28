#include "modbus_rtu_server.h"

#include "bsp_time.h"
#include "modbus_crc16.h"
#include "modbus_register_model.h"

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

static bool AppendCrc(uint8_t *response, uint16_t capacity, uint16_t *length)
{
    uint16_t crc;
    if ((*length > capacity) || ((uint16_t)(capacity - *length) < 2U))
        return false;
    crc = ModbusCrc16_Calculate(response, *length);
    response[(*length)++] = (uint8_t)crc;
    response[(*length)++] = (uint8_t)(crc >> 8U);
    return true;
}

static bool BuildException(ModbusRtuServer *server, uint8_t address,
                           uint8_t function, uint8_t exception,
                           uint8_t *response, uint16_t capacity,
                           uint16_t *length)
{
    if (capacity < 5U) return false;
    response[0] = address;
    response[1] = (uint8_t)(function | 0x80U);
    response[2] = exception;
    *length = 3U;
    server->statistics.last_exception = exception;
    ++server->statistics.exception_response_count;
    return AppendCrc(response, capacity, length);
}

static bool HandleRead(ModbusRtuServer *server, const uint8_t *request,
                       uint16_t request_length, uint8_t *response,
                       uint16_t capacity, uint16_t *length)
{
    uint16_t start;
    uint16_t quantity;
    uint16_t index;
    uint8_t exception;
    ModbusRegisterResult result;
    if (request_length != 8U)
    {
        ++server->statistics.length_error_count;
        return BuildException(server, request[0], request[1],
            MODBUS_EXCEPTION_ILLEGAL_VALUE, response, capacity, length);
    }
    start = ReadBe16(&request[2]);
    quantity = ReadBe16(&request[4]);
    if ((quantity == 0U) || (quantity > 125U) ||
        ((uint32_t)quantity * 2U + 5U > capacity))
        return BuildException(server, request[0], request[1],
            MODBUS_EXCEPTION_ILLEGAL_VALUE, response, capacity, length);
    result = ModbusRegisterModel_ReadHolding(start, quantity,
                                             server->registers);
    exception = MapException(result);
    if (exception != 0U)
        return BuildException(server, request[0], request[1], exception,
                              response, capacity, length);
    response[0] = request[0];
    response[1] = request[1];
    response[2] = (uint8_t)(quantity * 2U);
    *length = 3U;
    for (index = 0U; index < quantity; ++index)
    {
        WriteBe16(&response[*length], server->registers[index]);
        *length = (uint16_t)(*length + 2U);
    }
    return AppendCrc(response, capacity, length);
}

static bool HandleWriteSingle(ModbusRtuServer *server,
                              const uint8_t *request,
                              uint16_t request_length, uint8_t *response,
                              uint16_t capacity, uint16_t *length)
{
    uint8_t exception;
    ModbusRegisterResult result;
    if (request_length != 8U)
    {
        ++server->statistics.length_error_count;
        return BuildException(server, request[0], request[1],
            MODBUS_EXCEPTION_ILLEGAL_VALUE, response, capacity, length);
    }
    result = ModbusRegisterModel_WriteSingle(ReadBe16(&request[2]),
                                             ReadBe16(&request[4]),
                                             server->source);
    exception = MapException(result);
    if (exception != 0U)
        return BuildException(server, request[0], request[1], exception,
                              response, capacity, length);
    if (capacity < 8U) return false;
    (void)memcpy(response, request, 6U);
    *length = 6U;
    return AppendCrc(response, capacity, length);
}

static bool HandleWriteMultiple(ModbusRtuServer *server,
                                const uint8_t *request,
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
        ++server->statistics.length_error_count;
        return BuildException(server, request[0], request[1],
            MODBUS_EXCEPTION_ILLEGAL_VALUE, response, capacity, length);
    }
    start = ReadBe16(&request[2]);
    quantity = ReadBe16(&request[4]);
    expected = (uint16_t)(9U + (uint32_t)quantity * 2U);
    if ((quantity == 0U) || (quantity > 123U) ||
        (request[6] != (uint8_t)(quantity * 2U)) ||
        (request_length != expected))
        return BuildException(server, request[0], request[1],
            MODBUS_EXCEPTION_ILLEGAL_VALUE, response, capacity, length);
    for (index = 0U; index < quantity; ++index)
        server->registers[index] = ReadBe16(&request[7U + index * 2U]);
    result = ModbusRegisterModel_WriteMultiple(start, quantity,
        server->registers, server->source);
    exception = MapException(result);
    if (exception != 0U)
        return BuildException(server, request[0], request[1], exception,
                              response, capacity, length);
    if (capacity < 8U) return false;
    (void)memcpy(response, request, 6U);
    *length = 6U;
    return AppendCrc(response, capacity, length);
}

bool ModbusRtuServer_HandleAdu(ModbusRtuServer *server,
                              const uint8_t *request, uint16_t request_length,
                              uint8_t *response, uint16_t response_capacity,
                              uint16_t *response_length, bool *respond)
{
    uint16_t received_crc;
    uint16_t calculated_crc;
    bool built;
    if ((server == NULL) || (request == NULL) || (response == NULL) ||
        (response_length == NULL) || (respond == NULL) ||
        (request_length > MODBUS_RTU_ADU_MAX_SIZE)) return false;
    *respond = false;
    *response_length = 0U;
    if (request_length < 4U)
    {
        ++server->statistics.length_error_count;
        return true;
    }
    received_crc = (uint16_t)(request[request_length - 2U] |
        ((uint16_t)request[request_length - 1U] << 8U));
    calculated_crc = ModbusCrc16_Calculate(request,
                                           (uint16_t)(request_length - 2U));
    if (received_crc != calculated_crc)
    {
        ++server->statistics.crc_error_count;
        return true;
    }
    ++server->statistics.valid_frame_count;
    server->statistics.last_request_address = request[0];
    server->statistics.last_function = request[1];
    server->statistics.last_request_length = request_length;
    if (request[0] == 0U)
    {
        ++server->statistics.broadcast_count;
        return true;
    }
    if (request[0] != server->config.modbus_address)
    {
        ++server->statistics.ignored_address_count;
        return true;
    }
    ++server->statistics.addressed_frame_count;
    if (request[1] == MODBUS_FC_READ_HOLDING)
    {
        ++server->statistics.function03_count;
        built = HandleRead(server, request, request_length, response,
                           response_capacity, response_length);
    }
    else if (request[1] == MODBUS_FC_WRITE_SINGLE)
    {
        ++server->statistics.function06_count;
        built = HandleWriteSingle(server, request, request_length, response,
                                  response_capacity, response_length);
    }
    else if (request[1] == MODBUS_FC_WRITE_MULTIPLE)
    {
        ++server->statistics.function16_count;
        built = HandleWriteMultiple(server, request, request_length, response,
                                    response_capacity, response_length);
    }
    else
    {
        ++server->statistics.illegal_function_count;
        built = BuildException(server, request[0], request[1],
            MODBUS_EXCEPTION_ILLEGAL_FUNCTION, response, response_capacity,
            response_length);
    }
    if (!built) return false;
    *respond = true;
    server->statistics.last_response_length = *response_length;
    return true;
}

bool ModbusRtuServer_Init(ModbusRtuServer *server,
                          const CommunicationConfig *config,
                          ModbusRtuFramer *framer,
                          const ModbusRtuTransport *transport,
                          CommandSource source)
{
    if ((server == NULL) || (config == NULL) || (framer == NULL) ||
        !ModbusRtuTransport_IsValid(transport) ||
        (config->modbus_address < 1U) || (config->modbus_address > 247U) ||
        (config->response_delay_ms > 1000U)) return false;
    (void)memset(server, 0, sizeof(*server));
    server->config = *config;
    server->state = MODBUS_SERVER_IDLE;
    server->source = source;
    server->framer = framer;
    server->transport = transport;
    return true;
}

void ModbusRtuServer_Process(ModbusRtuServer *server)
{
    bool respond;
    bool timeout;
    if (server == NULL) return;
    switch (server->state)
    {
        case MODBUS_SERVER_IDLE:
            if (!server->suspended && ModbusRtuFramer_TryGetFrame(
                server->framer, server->request, sizeof(server->request),
                &server->request_length))
                server->state = MODBUS_SERVER_FRAME_PENDING;
            break;
        case MODBUS_SERVER_FRAME_PENDING:
            server->state = MODBUS_SERVER_PROCESSING;
            break;
        case MODBUS_SERVER_PROCESSING:
            if (!ModbusRtuServer_HandleAdu(server, server->request,
                server->request_length, server->response,
                sizeof(server->response), &server->response_length, &respond))
                server->state = MODBUS_SERVER_ERROR;
            else if (!respond) server->state = MODBUS_SERVER_IDLE;
            else
            {
                server->delay_start_ms = BSP_TimeNowMs();
                server->state = MODBUS_SERVER_RESPONSE_DELAY;
            }
            break;
        case MODBUS_SERVER_RESPONSE_DELAY:
            if ((uint32_t)(BSP_TimeNowMs() - server->delay_start_ms) >=
                server->config.response_delay_ms)
                server->state = MODBUS_SERVER_TX_PENDING;
            break;
        case MODBUS_SERVER_TX_PENDING:
            if (server->transport->start_tx(server->transport->context,
                server->response, server->response_length))
                server->state = MODBUS_SERVER_TX_ACTIVE;
            else
            {
                ++server->statistics.tx_error_count;
                ++server->statistics.tx_start_error_count;
                server->state = MODBUS_SERVER_ERROR;
            }
            break;
        case MODBUS_SERVER_TX_ACTIVE:
            server->state = MODBUS_SERVER_WAIT_TX_COMPLETE;
            break;
        case MODBUS_SERVER_WAIT_TX_COMPLETE:
            if (server->transport->take_tx_completed(
                server->transport->context))
            {
                ++server->statistics.tx_response_count;
                server->state = MODBUS_SERVER_IDLE;
            }
            else if (server->transport->take_tx_error(
                server->transport->context, &timeout))
            {
                ++server->statistics.tx_error_count;
                if (timeout) ++server->statistics.tx_timeout_error_count;
                else ++server->statistics.tx_start_error_count;
                server->transport->abort_tx(server->transport->context);
                server->state = MODBUS_SERVER_ERROR;
            }
            break;
        case MODBUS_SERVER_ERROR:
            server->transport->abort_tx(server->transport->context);
            server->state = MODBUS_SERVER_IDLE;
            break;
        default:
            server->state = MODBUS_SERVER_ERROR;
            break;
    }
}

bool ModbusRtuServer_IsBusy(const ModbusRtuServer *server)
{
    return (server != NULL) && (server->state != MODBUS_SERVER_IDLE);
}

void ModbusRtuServer_Suspend(ModbusRtuServer *server)
{
    if (server == NULL) return;
    server->suspended = true;
    server->transport->abort_tx(server->transport->context);
}

bool ModbusRtuServer_Resume(ModbusRtuServer *server,
                           const CommunicationConfig *config)
{
    if ((server == NULL) || (config == NULL)) return false;
    server->config = *config;
    server->suspended = false;
    server->state = MODBUS_SERVER_IDLE;
    return true;
}

ModbusRtuServerState ModbusRtuServer_GetState(const ModbusRtuServer *server)
{
    return (server != NULL) ? server->state : MODBUS_SERVER_ERROR;
}

const ModbusRtuServerStatistics *ModbusRtuServer_GetStatistics(
    const ModbusRtuServer *server)
{
    return (server != NULL) ? &server->statistics : NULL;
}
