#ifndef STAGE4B_STORAGE_DIAGNOSTICS_H
#define STAGE4B_STORAGE_DIAGNOSTICS_H

#include "config_store.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    ConfigLoadResult load_result;
    bool slot_a_valid;
    bool slot_b_valid;
    uint32_t slot_a_sequence;
    uint32_t slot_b_sequence;
    uint8_t active_slot;
    ConfigStoreState store_state;
    ConfigOperationType operation;
    uint32_t current_revision;
    uint32_t saved_revision;
    bool dirty;
    uint32_t save_request_count;
    uint32_t save_success_count;
    uint32_t save_no_change_count;
    uint32_t save_failure_count;
    uint32_t page_erase_count;
    uint32_t halfword_program_count;
    uint32_t recovery_count;
    uint32_t crc_error_count;
    uint32_t last_error;
} Stage4BStorageDiagnosticSnapshot;

void Stage4BStorageDiagnostics_Update(void);
const Stage4BStorageDiagnosticSnapshot *
Stage4BStorageDiagnostics_GetSnapshot(void);

#endif /* STAGE4B_STORAGE_DIAGNOSTICS_H */
