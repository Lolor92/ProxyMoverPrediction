#include "SyncGasMover.h"
#include "Debug/SGM_DebugLog.h"

#define LOCTEXT_NAMESPACE "FSyncGasMoverModule"

DEFINE_LOG_CATEGORY(LogSyncGasMover);
DEFINE_LOG_CATEGORY(LogSyncGasMoverBlend);

void FSyncGasMoverModule::StartupModule()
{
}

void FSyncGasMoverModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSyncGasMoverModule, SyncGasMover)
