#pragma once

// Include this only from a .cpp after all Unreal headers when you want to silence temporary noisy logs in that translation unit.
#ifndef SGM_SILENCE_TEMP_VERBOSE_LOGS
#define SGM_SILENCE_TEMP_VERBOSE_LOGS 1
#endif

#if SGM_SILENCE_TEMP_VERBOSE_LOGS
#undef UE_LOG
#define UE_LOG(CategoryName, Verbosity, Format, ...) do { } while (false)
#endif
