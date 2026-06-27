#include "Game/PMP_GameMode.h"

#include "Player/PMP_PlayerState.h"

APMP_GameMode::APMP_GameMode()
{
	// Keep DefaultPawnClass editable in a Blueprint child, but make PlayerState GAS-ready by default.
	PlayerStateClass = APMP_PlayerState::StaticClass();
}
