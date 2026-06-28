#include "SyncGasMover.h"

#define LOCTEXT_NAMESPACE "FSyncGasMoverModule"

DEFINE_LOG_CATEGORY(LogSyncGasMover);

void FSyncGasMoverModule::StartupModule()
{
}

void FSyncGasMoverModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSyncGasMoverModule, SyncGasMover)
