// Copyright (c) PotatoRogue. MIT Licensed.

#include "PotatoRogue.h"

DEFINE_LOG_CATEGORY(LogPotatoRogue);

void FPotatoRogueModule::StartupModule()
{
	UE_LOG(LogPotatoRogue, Log, TEXT("PotatoRogue module started"));
}

void FPotatoRogueModule::ShutdownModule()
{
	UE_LOG(LogPotatoRogue, Log, TEXT("PotatoRogue module shut down"));
}

IMPLEMENT_PRIMARY_GAME_MODULE(FPotatoRogueModule, PotatoRogue, "PotatoRogue");
