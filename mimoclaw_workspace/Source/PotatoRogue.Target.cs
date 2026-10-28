// Copyright (c) PotatoRogue. MIT Licensed.

using UnrealBuildTool;
using System.Collections.Generic;

public class PotatoRogueTarget : TargetRules
{
	public PotatoRogueTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V4;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_4;
		ExtraModuleNames.Add("PotatoRogue");
	}
}
