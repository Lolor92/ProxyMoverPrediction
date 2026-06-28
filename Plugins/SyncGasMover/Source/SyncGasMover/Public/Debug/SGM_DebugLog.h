#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSyncGasMoverBlend, Log, All);

#ifndef SGM_ENABLE_VERBOSE_MONTAGE_LOGS
#define SGM_ENABLE_VERBOSE_MONTAGE_LOGS 0
#endif

#if SGM_ENABLE_VERBOSE_MONTAGE_LOGS
#define SGM_VERBOSE_MONTAGE_LOG(Format, ...) UE_LOG(LogTemp, Warning, Format, ##__VA_ARGS__)
#else
#define SGM_VERBOSE_MONTAGE_LOG(Format, ...) do { } while (false)
#endif

#if !UE_BUILD_SHIPPING
#define SGM_BLEND_BOOL_LOG(Format, ...) UE_LOG(LogSyncGasMoverBlend, Warning, Format, ##__VA_ARGS__)
#else
#define SGM_BLEND_BOOL_LOG(Format, ...) do { } while (false)
#endif
