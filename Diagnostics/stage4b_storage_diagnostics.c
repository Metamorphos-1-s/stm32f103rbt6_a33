#include "stage4b_storage_diagnostics.h"

#include "persistence_manager.h"
#include "system_context.h"

#include <string.h>

static Stage4BStorageDiagnosticSnapshot s_snapshot;

void Stage4BStorageDiagnostics_Update(void)
{
    const ConfigStoreStatistics *statistics = ConfigStore_GetStatistics();
    const ConfigLoadInfo *info = PersistenceManager_GetLoadInfo();
    const SystemContext *context = SystemContext_Get();

    (void)memset(&s_snapshot, 0, sizeof(s_snapshot));
    s_snapshot.load_result = PersistenceManager_GetLoadResult();
    s_snapshot.slot_a_valid = info->slot_a_valid;
    s_snapshot.slot_b_valid = info->slot_b_valid;
    s_snapshot.slot_a_sequence = info->slot_a_sequence;
    s_snapshot.slot_b_sequence = info->slot_b_sequence;
    s_snapshot.active_slot = ConfigStore_GetActiveSlot();
    s_snapshot.store_state = ConfigStore_GetState();
    s_snapshot.operation = ConfigStore_GetOperation();
    s_snapshot.current_revision = SystemContext_GetConfigRevision();
    s_snapshot.saved_revision = SystemContext_GetSavedRevision();
    s_snapshot.dirty = (context != NULL) && context->runtime.config_dirty;
    s_snapshot.save_request_count = statistics->save_request_count;
    s_snapshot.save_success_count = statistics->save_success_count;
    s_snapshot.save_no_change_count = statistics->save_no_change_count;
    s_snapshot.save_failure_count = statistics->save_failure_count;
    s_snapshot.page_erase_count = statistics->page_erase_count;
    s_snapshot.halfword_program_count = statistics->halfword_program_count;
    s_snapshot.recovery_count = statistics->recovery_count;
    s_snapshot.crc_error_count = statistics->crc_error_count;
    s_snapshot.last_error = ConfigStore_GetLastError();
}

const Stage4BStorageDiagnosticSnapshot *
Stage4BStorageDiagnostics_GetSnapshot(void)
{
    return &s_snapshot;
}
