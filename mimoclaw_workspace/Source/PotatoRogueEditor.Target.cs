// Copyright (c) PotatoRogue. MIT Licensed.

using UnrealBuildTool;
using System.Collections.Generic;

public class PotatoRogueEditorTarget : TargetRules
{
	public PotatoRogueEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V4;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_4;
		ExtraModuleNames.Add("PotatoRogue");
	}
}
