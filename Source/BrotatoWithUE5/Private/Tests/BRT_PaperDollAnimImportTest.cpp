// Fill out your copyright notice in the Description page of Project Settings.
//
// 导入工具（编辑器内通过自动化测试驱动，由 unreal-mcp AutomationTestToolset 触发）：
//  1. Brotato.ImportDemo.AnimAssets —— 批量导入 6 个 Godot .tres 变换动画（player move/idle、unit idle、
//     turret shoot、enemy shoot/charge-prep）为 UBRT_PaperDollAnimAsset 资产并落盘
//  2. Brotato.ImportDemo.BodySprite —— 从 potato.png 纹理生成/更新 potato_Sprite 精灵资产（PPU/材质对齐腿部精灵）
// 两个测试均可重复执行（存在则覆盖导入，幂等）。

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#include "Animation/BRT_PaperDollAnimData.h"
#include "Engine/Texture2D.h"
#include "Misc/PackageName.h"
#include "PaperSprite.h"
#include "SpriteEditorOnlyTypes.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace BRT_PaperDollAnimImportTest
{
	const TCHAR* MoveTresPath = TEXT("h:/Engine/Godot/Projects/BrotatoProject-main/entities/units/player/player_move.tres");
	const TCHAR* IdleTresPath = TEXT("h:/Engine/Godot/Projects/BrotatoProject-main/entities/units/player/player_idle.tres");
	const TCHAR* UnitIdleTresPath = TEXT("h:/Engine/Godot/Projects/BrotatoProject-main/entities/units/unit/unit_idle.tres");
	const TCHAR* TurretShootTresPath = TEXT("h:/Engine/Godot/Projects/BrotatoProject-main/entities/structures/turret/turret_shoot_animation.tres");
	const TCHAR* EnemyShootTresPath = TEXT("h:/Engine/Godot/Projects/BrotatoProject-main/entities/units/enemies/attack_behaviors/enemy_shoot_animation.tres");
	const TCHAR* EnemyChargePrepTresPath = TEXT("h:/Engine/Godot/Projects/BrotatoProject-main/entities/units/enemies/attack_behaviors/enemy_charge_prep_animation.tres");

	const TCHAR* MoveAssetPackage = TEXT("/Game/BrotatoProject/entities/units/player/Animations/PA_PlayerMove");
	const TCHAR* IdleAssetPackage = TEXT("/Game/BrotatoProject/entities/units/player/Animations/PA_PlayerIdle");
	const TCHAR* UnitIdleAssetPackage = TEXT("/Game/BrotatoProject/entities/units/unit/Animations/PA_UnitIdle");
	const TCHAR* TurretShootAssetPackage = TEXT("/Game/BrotatoProject/entities/structures/turret/Animations/PA_TurretShoot");
	const TCHAR* EnemyShootAssetPackage = TEXT("/Game/BrotatoProject/entities/units/enemies/attack_behaviors/PA_EnemyShoot");
	const TCHAR* EnemyChargePrepAssetPackage = TEXT("/Game/BrotatoProject/entities/units/enemies/attack_behaviors/PA_EnemyChargePrep");
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
		It("should import 6 Godot .tres transform animations into DataAssets", [this]()
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
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
