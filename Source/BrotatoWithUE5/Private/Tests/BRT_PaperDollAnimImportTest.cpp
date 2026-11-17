// Fill out your copyright notice in the Description page of Project Settings.
//
// 导入工具（编辑器内通过自动化测试驱动，由 unreal-mcp AutomationTestToolset 触发）：
//  1. Brotato.ImportDemo.AnimAssets —— 批量导入 7 个 Godot .tres 变换动画（player move/idle/death、unit idle、
//     turret shoot、enemy shoot/charge-prep）为 UBRT_PaperDollAnimAsset 资产并落盘
//  2. Brotato.ImportDemo.SceneAnimAssets —— 从 entity.tscn（内嵌 RESET/death/idle 三个 Animation sub_resource）
//     按名称拆分导入 idle / death 两套动画，验证多 sub_resource 场景解析与 RESET 自动跳过
//  3. Brotato.ImportDemo.BodySprite —— 从 potato.png 纹理生成/更新 potato_Sprite 精灵资产（PPU/材质对齐腿部精灵）
//  4. Brotato.ImportDemo.BatchImportAll —— 递归扫描 Godot 工程，把能找到的动画（独立 .tres + .tscn 内嵌
//     sub_resource）全部导入为 UBRT_PaperDollAnimAsset，资产目录镜像 Godot 目录结构
// 四个测试均可重复执行（存在则覆盖导入，幂等）。

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#include "Animation/BRT_GodotAnimBatchImporter.h"
#include "Animation/BRT_GodotTresAnimParser.h"
#include "Animation/BRT_PaperDollAnimData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture2D.h"
#include "Misc/PackageName.h"
#include "PaperSprite.h"
#include "SpriteEditorOnlyTypes.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace BRT_PaperDollAnimImportTest
{
	// Godot 工程根目录（BatchImportAll 从其下递归扫描所有动画）
	const TCHAR* GodotRootDir = TEXT("E:/UE/UEProject/BrotatoProject-main");
	const TCHAR* MoveTresPath = TEXT("E:/UE/UEProject/BrotatoProject-main/entities/units/player/player_move.tres");
	const TCHAR* IdleTresPath = TEXT("E:/UE/UEProject/BrotatoProject-main/entities/units/player/player_idle.tres");
	const TCHAR* DeathTresPath = TEXT("E:/UE/UEProject/BrotatoProject-main/entities/units/player/player_death.tres");
	const TCHAR* UnitIdleTresPath = TEXT("E:/UE/UEProject/BrotatoProject-main/entities/units/unit/unit_idle.tres");
	const TCHAR* TurretShootTresPath = TEXT("E:/UE/UEProject/BrotatoProject-main/entities/structures/turret/turret_shoot_animation.tres");
	const TCHAR* EnemyShootTresPath = TEXT("E:/UE/UEProject/BrotatoProject-main/entities/units/enemies/attack_behaviors/enemy_shoot_animation.tres");
	const TCHAR* EnemyChargePrepTresPath = TEXT("E:/UE/UEProject/BrotatoProject-main/entities/units/enemies/attack_behaviors/enemy_charge_prep_animation.tres");
	// entity.tscn：全体单位共用的基础动画，内嵌 RESET(id=3，运行时不播放) / death(id=4) / idle(id=5) 三个 Animation sub_resource
	const TCHAR* EntityScnPath = TEXT("E:/UE/UEProject/BrotatoProject-main/entities/entity.tscn");

	const TCHAR* MoveAssetPackage = TEXT("/Game/BrotatoProject/entities/units/player/Animations/PA_PlayerMove");
	const TCHAR* IdleAssetPackage = TEXT("/Game/BrotatoProject/entities/units/player/Animations/PA_PlayerIdle");
	const TCHAR* DeathAssetPackage = TEXT("/Game/BrotatoProject/entities/units/player/Animations/PA_PlayerDeath");
	const TCHAR* UnitIdleAssetPackage = TEXT("/Game/BrotatoProject/entities/units/unit/Animations/PA_UnitIdle");
	const TCHAR* TurretShootAssetPackage = TEXT("/Game/BrotatoProject/entities/structures/turret/Animations/PA_TurretShoot");
	const TCHAR* EnemyShootAssetPackage = TEXT("/Game/BrotatoProject/entities/units/enemies/attack_behaviors/PA_EnemyShoot");
	const TCHAR* EnemyChargePrepAssetPackage = TEXT("/Game/BrotatoProject/entities/units/enemies/attack_behaviors/PA_EnemyChargePrep");
	const TCHAR* EntityIdleAssetPackage = TEXT("/Game/BrotatoProject/entities/Animations/PA_EntityIdle");
	const TCHAR* EntityDeathAssetPackage = TEXT("/Game/BrotatoProject/entities/Animations/PA_EntityDeath");
	const TCHAR* BodySpritePackage = TEXT("/Game/BrotatoProject/entities/units/player/potato_Sprite");
	const TCHAR* PotatoTexturePath = TEXT("/Game/BrotatoProject/entities/units/player/potato.potato");
	const TCHAR* LegsSprite0Path = TEXT("/Game/BrotatoProject/entities/units/player/legs_Sprite_0.legs_Sprite_0");

	bool SaveAssetToDisk(UObject* Asset)
	{
		UPackage* Package = Asset->GetOutermost();
		const FString FileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		return UPackage::SavePackage(Package, Asset, *FileName, SaveArgs);
	}

	UBRT_PaperDollAnimAsset* LoadOrCreateAnimAsset(const FString& PackagePath, bool& bOutCreated)
	{
		bOutCreated = false;
		const FString AssetName = FPackageName::GetShortName(PackagePath);
		if (UBRT_PaperDollAnimAsset* Existing = LoadObject<UBRT_PaperDollAnimAsset>(nullptr, *FString::Printf(TEXT("%s.%s"), *PackagePath, *AssetName)))
		{
			return Existing;
		}
		UPackage* Package = CreatePackage(*PackagePath);
		UBRT_PaperDollAnimAsset* NewAsset = NewObject<UBRT_PaperDollAnimAsset>(Package, *AssetName, RF_Public | RF_Standalone);
		bOutCreated = true;
		return NewAsset;
	}
}

BEGIN_DEFINE_SPEC(FBRT_PaperDollAnimImportSpec, "Brotato.ImportDemo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FBRT_PaperDollAnimImportSpec)

void FBRT_PaperDollAnimImportSpec::Define()
{
	using namespace BRT_PaperDollAnimImportTest;

	Describe("AnimAssets", [this]()
	{
		It("should import 7 Godot .tres transform animations into DataAssets", [this]()
		{
			// PPU 与已导入腿部精灵对齐（实测 1）
			float PPU = 1.f;
			if (UPaperSprite* LegsSprite = LoadObject<UPaperSprite>(nullptr, LegsSprite0Path))
			{
				PPU = LegsSprite->GetPixelsPerUnrealUnit();
			}

			struct FImportJob
			{
				const TCHAR* PackagePath;
				const TCHAR* TresPath;
				const TCHAR* ExpectedName;
				int32 ExpectedTracks;
				int32 ExpectedEvents;
			};
			const FImportJob Jobs[] = {
				{ MoveAssetPackage, MoveTresPath, TEXT("move"), 14, 2 },
				{ IdleAssetPackage, IdleTresPath, TEXT("idle"), 14, 0 },
				// Sprite:scale(2通道) + Sprite:rotation_degrees(1通道)；Sprite:modulate 非 Transform，正确跳过
				{ DeathAssetPackage, DeathTresPath, TEXT("death"), 3, 2 },
				{ UnitIdleAssetPackage, UnitIdleTresPath, TEXT("idle"), 4, 0 },
				{ TurretShootAssetPackage, TurretShootTresPath, TEXT("shoot"), 4, 1 },
				{ EnemyShootAssetPackage, EnemyShootTresPath, TEXT("shoot"), 4, 1 },
				{ EnemyChargePrepAssetPackage, EnemyChargePrepTresPath, TEXT("shoot"), 4, 2 },
			};

			for (const FImportJob& Job : Jobs)
			{
				if (!FPaths::FileExists(Job.TresPath))
				{
					AddInfo(FString::Printf(TEXT("Godot .tres 源文件不存在，跳过：%s"), Job.TresPath));
					continue;
				}

				bool bCreated = false;
				UBRT_PaperDollAnimAsset* Asset = LoadOrCreateAnimAsset(Job.PackagePath, bCreated);
				if (!TestTrue(TEXT("asset valid"), Asset != nullptr))
				{
					continue;
				}

				Asset->TresImportPath = Job.TresPath;
				Asset->ImportPixelsPerUnrealUnit = PPU;
				Asset->ImportRotationSign = -1.f;

				if (!TestTrue(FString::Printf(TEXT("import %s"), Job.ExpectedName), Asset->ImportFromTresFile(Job.TresPath)))
				{
					continue;
				}

				TestTrue(FString::Printf(TEXT("%s name"), Job.ExpectedName), Asset->AnimName == FName(Job.ExpectedName));
				TestTrue(FString::Printf(TEXT("%s tracks==%d"), Job.ExpectedName, Job.ExpectedTracks), Asset->Tracks.Num() == Job.ExpectedTracks);
				TestTrue(FString::Printf(TEXT("%s events==%d"), Job.ExpectedName, Job.ExpectedEvents), Asset->Events.Num() == Job.ExpectedEvents);

				if (!TestTrue(FString::Printf(TEXT("save %s"), Job.ExpectedName), SaveAssetToDisk(Asset)))
				{
					continue;
				}
				AddInfo(FString::Printf(TEXT("%s -> %s (%s, %d tracks, %d events, PPU=%.3f)"),
					Job.TresPath, Job.PackagePath, bCreated ? TEXT("created") : TEXT("updated"), Asset->Tracks.Num(), Asset->Events.Num(), PPU));
			}
		});
	});

	Describe("SceneAnimAssets", [this]()
	{
		It("should split entity.tscn (RESET/death/idle) into 2 DataAssets, skipping RESET", [this]()
		{
			if (!FPaths::FileExists(EntityScnPath))
			{
				AddInfo(FString::Printf(TEXT("Godot .tscn 源文件不存在，跳过：%s"), EntityScnPath));
				return;
			}

			// PPU 与已导入腿部精灵对齐（与 AnimAssets 测试保持一致）
			float PPU = 1.f;
			if (UPaperSprite* LegsSprite = LoadObject<UPaperSprite>(nullptr, LegsSprite0Path))
			{
				PPU = LegsSprite->GetPixelsPerUnrealUnit();
			}

			FBRT_GodotAnimImportParams Params;
			Params.PixelsPerUnrealUnit = PPU;
			Params.RotationSign = -1.f;

			FBRT_GodotSceneAnimParseResult SceneResult;
			if (!TestTrue(TEXT("parse entity.tscn"), FBRT_GodotTresAnimParser::ParseScnFile(EntityScnPath, Params, SceneResult)))
			{
				return;
			}
			// RESET（id=3）运行时不播放，解析器需自动跳过，只剩 death / idle 两套
			TestTrue(TEXT("entity.tscn yields 2 animations (RESET skipped)"), SceneResult.Animations.Num() == 2);

			struct FImportJob
			{
				const TCHAR* PackagePath;
				const TCHAR* WantedName;
				int32 ExpectedTracks;
				int32 ExpectedEvents;
			};
			const FImportJob Jobs[] = {
				// Root(Animation 容器)：position:x/y + scale:x/y 各 1 条 bezier 轨道
				{ EntityIdleAssetPackage, TEXT("idle"), 4, 0 },
				// Sprite 子节点：scale(2通道) + rotation_degrees(1通道)；Sprite:modulate 非 Transform，正确跳过
				{ EntityDeathAssetPackage, TEXT("death"), 3, 2 },
			};

			for (const FImportJob& Job : Jobs)
			{
				bool bCreated = false;
				UBRT_PaperDollAnimAsset* Asset = LoadOrCreateAnimAsset(Job.PackagePath, bCreated);
				if (!TestTrue(TEXT("asset valid"), Asset != nullptr))
				{
					continue;
				}

				Asset->TresImportPath = EntityScnPath;
				Asset->ImportPixelsPerUnrealUnit = PPU;
				Asset->ImportRotationSign = -1.f;

				if (!TestTrue(FString::Printf(TEXT("import %s"), Job.WantedName), Asset->ImportFromScnFile(EntityScnPath, FName(Job.WantedName))))
				{
					continue;
				}

				TestTrue(FString::Printf(TEXT("%s name"), Job.WantedName), Asset->AnimName == FName(Job.WantedName));
				TestTrue(FString::Printf(TEXT("%s tracks==%d"), Job.WantedName, Job.ExpectedTracks), Asset->Tracks.Num() == Job.ExpectedTracks);
				TestTrue(FString::Printf(TEXT("%s events==%d"), Job.WantedName, Job.ExpectedEvents), Asset->Events.Num() == Job.ExpectedEvents);

				if (!TestTrue(FString::Printf(TEXT("save %s"), Job.WantedName), SaveAssetToDisk(Asset)))
				{
					continue;
				}
				AddInfo(FString::Printf(TEXT("entity.tscn[%s] -> %s (%s, %d tracks, %d events)"),
					Job.WantedName, Job.PackagePath, bCreated ? TEXT("created") : TEXT("updated"), Asset->Tracks.Num(), Asset->Events.Num()));
			}
		});
	});

	Describe("BodySprite", [this]()
	{
		It("should create potato_Sprite from the potato texture", [this]()
		{
			UPaperSprite* Sprite = LoadObject<UPaperSprite>(nullptr, TEXT("/Game/BrotatoProject/entities/units/player/potato_Sprite.potato_Sprite"));
			const bool bCreated = (Sprite == nullptr);
			if (Sprite == nullptr)
			{
				UPackage* Package = CreatePackage(BodySpritePackage);
				Sprite = NewObject<UPaperSprite>(Package, TEXT("potato_Sprite"), RF_Public | RF_Standalone);
			}
			if (!TestTrue(TEXT("sprite valid"), Sprite != nullptr))
			{
				return;
			}

			UTexture2D* PotatoTexture = LoadObject<UTexture2D>(nullptr, PotatoTexturePath);
			if (!TestTrue(TEXT("potato texture exists"), PotatoTexture != nullptr))
			{
				return;
			}

			// PPU / 材质与既有腿部精灵一致
			float PPU = 1.f;
			UMaterialInterface* Material = nullptr;
			if (UPaperSprite* LegsSprite = LoadObject<UPaperSprite>(nullptr, LegsSprite0Path))
			{
				PPU = LegsSprite->GetPixelsPerUnrealUnit();
				Material = LegsSprite->GetDefaultMaterial();
			}

			FSpriteAssetInitParameters Init;
			Init.SetTextureAndFill(PotatoTexture);
			Init.SetPixelsPerUnrealUnit(PPU);
			Init.DefaultMaterialOverride = Material;
			Sprite->InitializeSprite(Init, /*bRebuildData*/ true);
			// 与腿部精灵同为 Center_Center：对齐 Godot 居中 Sprite
			Sprite->SetPivotMode(ESpritePivotMode::Center_Center, FVector2D::ZeroVector, /*bRebuildData*/ true);

			if (!TestTrue(TEXT("save potato_Sprite"), SaveAssetToDisk(Sprite)))
			{
				return;
			}
			AddInfo(FString::Printf(TEXT("potato_Sprite %s (PPU=%.3f, %dx%d px)"),
				bCreated ? TEXT("created") : TEXT("updated"), PPU,
				PotatoTexture->GetSizeX(), PotatoTexture->GetSizeY()));
		});
	});

	Describe("BatchImportAll", [this]()
	{
		It("should import every Godot animation, mirroring the Godot directory structure", [this]()
		{
			// PPU 与已导入腿部精灵对齐（与 AnimAssets 测试保持一致）
			float PPU = 1.f;
			if (UPaperSprite* LegsSprite = LoadObject<UPaperSprite>(nullptr, LegsSprite0Path))
			{
				PPU = LegsSprite->GetPixelsPerUnrealUnit();
			}

			FBRT_GodotAnimImportOptions Options;
			Options.GodotRoot = GodotRootDir;
			Options.ContentRoot = TEXT("/Game/BrotatoProject");
			Options.PixelsPerUnrealUnit = PPU;
			Options.RotationSign = -1.f;
			Options.bSaveAssets = true;
			Options.bOverwriteExisting = true;
			Options.bKeepEmptyAnimations = true;

			const FBRT_GodotAnimImportReport Report = FBRT_GodotAnimBatchImporter::ImportAll(Options);
			AddInfo(Report.ToSummaryString());

			// 新写入磁盘的资产包通知资产注册表，内容浏览器无需手动重新扫描即可见
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
			AssetRegistryModule.Get().ScanPathsSynchronous({ Options.ContentRoot }, /*bForceRescan*/ false);

			for (const FBRT_GodotAnimImportEntry& Entry : Report.Entries)
			{
				AddInfo(FString::Printf(TEXT("%s [%s] -> %s (%s, %d tracks, %d events)"),
					*Entry.SourceFile, *Entry.AnimName, *Entry.PackagePath,
					Entry.bCreated ? TEXT("created") : TEXT("updated"), Entry.NumTracks, Entry.NumEvents));
			}
			for (const FString& Skipped : Report.SkippedFiles)
			{
				AddInfo(FString::Printf(TEXT("跳过：%s"), *Skipped));
			}

			if (!TestTrue(TEXT("至少导入一个动画"), Report.NumImported > 0))
			{
				return;
			}
			TestEqual(TEXT("无导入失败项"), Report.NumFailed, 0);

			// 回读校验：资产可从包路径重新加载，且动画名与 Godot 动画名一致
			for (const FBRT_GodotAnimImportEntry& Entry : Report.Entries)
			{
				const FString AssetName = FPackageName::GetShortName(Entry.PackagePath);
				UBRT_PaperDollAnimAsset* Loaded = LoadObject<UBRT_PaperDollAnimAsset>(nullptr,
					*FString::Printf(TEXT("%s.%s"), *Entry.PackagePath, *AssetName));
				if (!TestNotNull(*FString::Printf(TEXT("资产可加载 %s"), *Entry.PackagePath), Loaded))
				{
					continue;
				}
				TestTrue(*FString::Printf(TEXT("动画名匹配 %s"), *Entry.PackagePath), Loaded->AnimName == FName(*Entry.AnimName));
				TestTrue(*FString::Printf(TEXT("动画时长有效 %s"), *Entry.PackagePath), Loaded->Length > 0.f);
			}
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
