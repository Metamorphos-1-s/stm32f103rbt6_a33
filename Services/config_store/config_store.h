#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include "device_config.h"
#include "flash_backend.h"
#include "runtime_state.h"

#include <stdbool.h>
#include <stdint.h>

#define CONFIG_STORE_HALFWORDS_PER_PROCESS 16U
#define CONFIG_STORE_SLOT_NONE 0U
#define CONFIG_STORE_SLOT_A    1U
#define CONFIG_STORE_SLOT_B    2U

typedef enum
{
    CONFIG_LOAD_OK = 0,
    CONFIG_LOAD_NOT_FOUND,
    CONFIG_LOAD_RECOVERED_SLOT_A,
    CONFIG_LOAD_RECOVERED_SLOT_B,
    CONFIG_LOAD_BOTH_VALID,
    CONFIG_LOAD_CORRUPT,
    CONFIG_LOAD_UNSUPPORTED_SCHEMA,
    CONFIG_LOAD_VALIDATION_FAILED,
    CONFIG_LOAD_IO_ERROR
} ConfigLoadResult;

typedef enum
{
    CONFIG_OPERATION_NONE = 0,
    CONFIG_OPERATION_SAVE,
    CONFIG_OPERATION_FACTORY_RESET
} ConfigOperationType;

typedef enum
{
    CONFIG_STORE_STATE_IDLE = 0,
    CONFIG_STORE_STATE_PREPARE,
    CONFIG_STORE_STATE_ERASE_PAGE_0,
    CONFIG_STORE_STATE_ERASE_PAGE_1,
    CONFIG_STORE_STATE_PROGRAM_BODY,
    CONFIG_STORE_STATE_VERIFY_BODY,
    CONFIG_STORE_STATE_PROGRAM_COMMIT,
    CONFIG_STORE_STATE_VERIFY_FINAL,
    CONFIG_STORE_STATE_COMPLETE,
    CONFIG_STORE_STATE_ERROR
} ConfigStoreState;

typedef enum
{
    CONFIG_STORE_OPERATION_NONE = 0,
    CONFIG_STORE_OPERATION_IN_PROGRESS,
    CONFIG_STORE_OPERATION_SUCCESS,
    CONFIG_STORE_OPERATION_NO_CHANGE,
    CONFIG_STORE_OPERATION_INVALID,
    CONFIG_STORE_OPERATION_IO_ERROR,
    CONFIG_STORE_OPERATION_VERIFY_ERROR
} ConfigStoreOperationResult;

typedef struct
{
    bool slot_a_valid;
    bool slot_b_valid;
    uint32_t slot_a_sequence;
    uint32_t slot_b_sequence;
    uint8_t active_slot;
    uint32_t active_sequence;
    uint32_t active_flags;
    bool sequence_conflict;
} ConfigLoadInfo;

typedef struct
{
    uint32_t save_request_count;
    uint32_t save_success_count;
    uint32_t save_no_change_count;
    uint32_t save_failure_count;
    uint32_t page_erase_count;
    uint32_t halfword_program_count;
    uint32_t recovery_count;
    uint32_t crc_error_count;
} ConfigStoreStatistics;

void ConfigStore_Init(const FlashBackendOps *backend);
ConfigLoadResult ConfigStore_Load(DeviceConfig *config, RuntimeState *runtime,
                                  ConfigLoadInfo *info);
bool ConfigStore_RequestSave(const DeviceConfig *config,
                             const RuntimeState *runtime,
                             uint32_t source_revision);
bool ConfigStore_RequestFactoryReset(const DeviceConfig *defaults,
                                     const RuntimeState *default_runtime,
                                     uint32_t source_revision);
void ConfigStore_Process(void);
bool ConfigStore_IsBusy(void);
ConfigStoreState ConfigStore_GetState(void);
ConfigOperationType ConfigStore_GetOperation(void);
ConfigStoreOperationResult ConfigStore_GetLastOperationResult(void);
uint32_t ConfigStore_GetOperationRevision(void);
uint32_t ConfigStore_GetActiveSequence(void);
uint8_t ConfigStore_GetActiveSlot(void);
uint32_t ConfigStore_GetLastError(void);
const ConfigStoreStatistics *ConfigStore_GetStatistics(void);
void ConfigStore_AcknowledgeResult(void);
bool ConfigStore_IsSequenceNewer(uint32_t candidate, uint32_t reference);

#endif /* CONFIG_STORE_H */
