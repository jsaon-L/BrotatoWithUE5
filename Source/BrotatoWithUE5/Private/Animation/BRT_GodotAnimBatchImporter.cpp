// Fill out your copyright notice in the Description page of Project Settings.

#include "Animation/BRT_GodotAnimBatchImporter.h"

#if WITH_EDITOR

#include "Animation/BRT_GodotTresAnimParser.h"
#include "Animation/BRT_PaperDollAnimData.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace BRT_GodotAnimBatchImporterPrivate
{
	/** 递归收集指定扩展名的文件（绝对路径，已排序以保证导入顺序稳定） */
	void CollectFilesByExtension(const FString& RootDir, const TCHAR* Extension, TArray<FString>& OutFiles)
	{
		IFileManager::Get().IterateDirectoryRecursively(*RootDir,
			[Extension, &OutFiles](const TCHAR* Filename, bool bIsDirectory)
			{
				if (!bIsDirectory && FPaths::GetExtension(Filename).Equals(Extension, ESearchCase::IgnoreCase))
				{
					OutFiles.Add(FPaths::ConvertRelativePathToFull(Filename));
				}
				return true;
			});
		OutFiles.Sort();
	}

	/** 取 Godot 资源头行（首行非空且非注释），形如 [gd_resource type="Animation" ...] */
	FString GetGodotResourceHeader(const FString& Text)
	{
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines);
		for (const FString& RawLine : Lines)
		{
			const FString Line = RawLine.TrimStartAndEnd();
			if (Line.IsEmpty() || Line.StartsWith(TEXT(";")))
			{
				continue;
			}
			return Line;
		}
		return FString();
	}

	/** 源文件相对 Godot 根目录的目录（正斜杠，无首尾斜杠；根目录下文件返回空串） */
	FString MakeGodotRelativeDir(const FString& SourceFile, const FString& GodotRoot)
	{
		FString RelativeDir = FPaths::GetPath(SourceFile);
		// MakePathRelativeTo 要求基准路径以 '/' 结尾
		FPaths::MakePathRelativeTo(RelativeDir, *(GodotRoot / TEXT("")));
		RelativeDir.ReplaceInline(TEXT("\\"), TEXT("/"));
		while (RelativeDir.EndsWith(TEXT("/")))
		{
			RelativeDir.LeftChopInline(1);
		}
		return RelativeDir == TEXT(".") ? FString() : RelativeDir;
	}

	/** 去掉末尾的 "animation" / "anim" 词（turret_shoot_animation -> turret_shoot） */
	FString StripAnimationSuffix(const FString& InName)
	{
		TArray<FString> Words;
		InName.ParseIntoArray(Words, TEXT("_"), /*InCullEmpty*/ true);
		while (Words.Num() > 1 &&
			(Words.Last().Equals(TEXT("animation"), ESearchCase::IgnoreCase) ||
			 Words.Last().Equals(TEXT("anim"), ESearchCase::IgnoreCase)))
		{
			Words.Pop();
		}

		FString Result;
		for (int32 WordIdx = 0; WordIdx < Words.Num(); ++WordIdx)
		{
			if (WordIdx > 0)
			{
				Result.AppendChar(TEXT('_'));
			}
			Result += Words[WordIdx];
		}
		return Result;
	}

	UBRT_PaperDollAnimAsset* LoadOrCreateAnimAsset(const FString& PackagePath, bool& bOutCreated)
	{
		bOutCreated = false;
		const FString AssetName = FPackageName::GetShortName(PackagePath);
		const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *PackagePath, *AssetName);
		if (UBRT_PaperDollAnimAsset* Existing = LoadObject<UBRT_PaperDollAnimAsset>(nullptr, *ObjectPath))
		{
			return Existing;
		}

		UPackage* Package = CreatePackage(*PackagePath);
		UBRT_PaperDollAnimAsset* NewAsset = NewObject<UBRT_PaperDollAnimAsset>(Package, *AssetName, RF_Public | RF_Standalone);
		bOutCreated = true;
		return NewAsset;
	}

	bool SaveAssetToDisk(UObject* Asset)
	{
		UPackage* Package = Asset->GetOutermost();
		const FString FileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		return UPackage::SavePackage(Package, Asset, *FileName, SaveArgs);
	}

	/** 资产在磁盘上是否已存在（不依赖资产注册表） */
	bool DoesAssetFileExist(const FString& PackagePath)
	{
		const FString FileName = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
		return FPaths::FileExists(FileName);
	}
}

FString FBRT_GodotAnimImportReport::ToSummaryString() const
{
	return FString::Printf(TEXT("导入动画资产 %d 个（新建 %d，更新 %d），失败 %d，跳过源文件 %d"),
		NumImported, NumCreated, NumImported - NumCreated, NumFailed, SkippedFiles.Num());
}

FString FBRT_GodotAnimBatchImporter::ToPascalCase(const FString& InName)
{
	FString Result;
	bool bUpperNext = true;
	for (const TCHAR Char : InName)
	{
		if (Char == TEXT('_') || Char == TEXT('-') || Char == TEXT(' ') || Char == TEXT('.'))
		{
			bUpperNext = true;
			continue;
		}
		Result.AppendChar(bUpperNext ? FChar::ToUpper(Char) : Char);
		bUpperNext = false;
	}
	return Result;
}

FString FBRT_GodotAnimBatchImporter::MakeTresAssetName(const FString& InFileName)
{
	using namespace BRT_GodotAnimBatchImporterPrivate;

	return FString::Printf(TEXT("PA_%s"), *ToPascalCase(StripAnimationSuffix(InFileName)));
}

FString FBRT_GodotAnimBatchImporter::MakeSceneAnimAssetName(const FString& InSceneFileName, const FString& InAnimName)
{
	using namespace BRT_GodotAnimBatchImporterPrivate;

	return FString::Printf(TEXT("PA_%s%s"), *ToPascalCase(StripAnimationSuffix(InSceneFileName)), *ToPascalCase(InAnimName));
}

FString FBRT_GodotAnimBatchImporter::MakeAssetPackagePath(const FString& ContentRoot, const FString& GodotRelativeDir, const FString& AssetName)
{
	using namespace BRT_GodotAnimBatchImporterPrivate;

	const FString DirPart = GodotRelativeDir.IsEmpty() ? FString() : (GodotRelativeDir + TEXT("/"));
	const FString Preferred = FString::Printf(TEXT("%s/%sAnimations/%s"), *ContentRoot, *DirPart, *AssetName);
	const FString Legacy = FString::Printf(TEXT("%s/%s%s"), *ContentRoot, *DirPart, *AssetName);

	// 早期导入把部分动画直接放在源目录下（如 attack_behaviors/PA_EnemyShoot），沿用旧位置避免重复资产
	if (!DoesAssetFileExist(Preferred) && DoesAssetFileExist(Legacy))
	{
		return Legacy;
	}
	return Preferred;
}

FBRT_GodotAnimImportReport FBRT_GodotAnimBatchImporter::ImportAll(const FBRT_GodotAnimImportOptions& Options)
{
	using namespace BRT_GodotAnimBatchImporterPrivate;

	FBRT_GodotAnimImportReport Report;
	if (!FPaths::DirectoryExists(Options.GodotRoot))
	{
		Report.SkippedFiles.Add(FString::Printf(TEXT("%s：Godot 工程根目录不存在"), *Options.GodotRoot));
		return Report;
	}

	FBRT_GodotAnimImportParams Params;
	Params.PixelsPerUnrealUnit = Options.PixelsPerUnrealUnit;
	Params.RotationSign = Options.RotationSign;
	Params.bKeepEmptyAnimations = Options.bKeepEmptyAnimations;

	// 单个动画 -> 资产
	auto ImportSingleAnimation = [&Options, &Report](const FString& SourceFile, const FBRT_GodotAnimParseResult& Parsed,
		const FString& AssetName, const TArray<FString>& ParseWarnings)
	{
		FBRT_GodotAnimImportEntry Entry;
		Entry.SourceFile = SourceFile;
		Entry.AnimName = Parsed.AnimName.ToString();
		Entry.PackagePath = MakeAssetPackagePath(Options.ContentRoot, MakeGodotRelativeDir(SourceFile, Options.GodotRoot), AssetName);
		Entry.Warnings = ParseWarnings;

		if (!Options.bOverwriteExisting)
		{
			const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *Entry.PackagePath, *FPackageName::GetShortName(Entry.PackagePath));
			if (LoadObject<UBRT_PaperDollAnimAsset>(nullptr, *ObjectPath) != nullptr)
			{
				++Report.NumImported;
				Report.Entries.Add(MoveTemp(Entry));
				return;
			}
		}

		bool bCreated = false;
		UBRT_PaperDollAnimAsset* Asset = LoadOrCreateAnimAsset(Entry.PackagePath, bCreated);
		if (Asset == nullptr)
		{
			Entry.Warnings.Add(TEXT("资产创建失败"));
			++Report.NumFailed;
			Report.Entries.Add(MoveTemp(Entry));
			return;
		}

		Asset->TresImportPath = SourceFile;
		Asset->ImportPixelsPerUnrealUnit = Options.PixelsPerUnrealUnit;
		Asset->ImportRotationSign = Options.RotationSign;
		Asset->ApplyParseResult(Parsed);

		if (Options.bSaveAssets && !SaveAssetToDisk(Asset))
		{
			Entry.Warnings.Add(TEXT("资产保存失败"));
			++Report.NumFailed;
			Report.Entries.Add(MoveTemp(Entry));
			return;
		}

		Entry.bCreated = bCreated;
		Entry.bImported = true;
		Entry.NumTracks = Asset->Tracks.Num();
		Entry.NumEvents = Asset->Events.Num();
		++Report.NumImported;
		if (bCreated)
		{
			++Report.NumCreated;
		}
		Report.Entries.Add(MoveTemp(Entry));
	};

	TArray<FString> SourceFiles;
	CollectFilesByExtension(Options.GodotRoot, TEXT("tres"), SourceFiles);
	CollectFilesByExtension(Options.GodotRoot, TEXT("tscn"), SourceFiles);
	SourceFiles.Sort();

	for (const FString& SourceFile : SourceFiles)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *SourceFile))
		{
			Report.SkippedFiles.Add(FString::Printf(TEXT("%s：读取失败"), *SourceFile));
			continue;
		}

		const bool bIsScene = FPaths::GetExtension(SourceFile).Equals(TEXT("tscn"), ESearchCase::IgnoreCase);
		if (bIsScene)
		{
			// 场景：内嵌 sub_resource 动画可能有多套（RESET 由解析器跳过）
			if (!Text.Contains(TEXT("sub_resource type=\"Animation\"")))
			{
				continue;
			}

			FBRT_GodotSceneAnimParseResult SceneResult;
			if (!FBRT_GodotTresAnimParser::ParseScnFile(SourceFile, Params, SceneResult))
			{
				Report.SkippedFiles.Add(FString::Printf(TEXT("%s：未解析出可用动画（%s）"),
					*SourceFile, *FString::Join(SceneResult.Warnings, TEXT("; "))));
				continue;
			}

			const FString SceneBaseName = FPaths::GetBaseFilename(SourceFile);
			for (const FBRT_GodotAnimParseResult& Anim : SceneResult.Animations)
			{
				ImportSingleAnimation(SourceFile, Anim, MakeSceneAnimAssetName(SceneBaseName, Anim.AnimName.ToString()), SceneResult.Warnings);
			}
			continue;
		}

		// 单资源：只处理 [gd_resource type="Animation"]，其它 .tres（物品/波次数据等）忽略
		const FString Header = GetGodotResourceHeader(Text);
		if (!Header.StartsWith(TEXT("[gd_resource")) || !Header.Contains(TEXT("type=\"Animation\"")))
		{
			continue;
		}

		FBRT_GodotAnimParseResult Result;
		if (!FBRT_GodotTresAnimParser::ParseTresFile(SourceFile, Params, Result))
		{
			Report.SkippedFiles.Add(FString::Printf(TEXT("%s：未解析出变换轨道（%s）"),
				*SourceFile, *FString::Join(Result.Warnings, TEXT("; "))));
			continue;
		}

		ImportSingleAnimation(SourceFile, Result, MakeTresAssetName(FPaths::GetBaseFilename(SourceFile)), Result.Warnings);
	}

	return Report;
}

#endif // WITH_EDITOR
