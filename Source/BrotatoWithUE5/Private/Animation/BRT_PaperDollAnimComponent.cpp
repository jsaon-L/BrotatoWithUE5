// Fill out your copyright notice in the Description page of Project Settings.

#include "Animation/BRT_PaperDollAnimComponent.h"

#include "Animation/BRT_PaperDollAnimData.h"
#include "Components/SceneComponent.h"

UBRT_PaperDollAnimComponent::UBRT_PaperDollAnimComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UBRT_PaperDollAnimComponent::BeginPlay()
{
	Super::BeginPlay();

	// 自动注册细节面板中配置的目标
	for (const FBRT_PaperDollTargetBinding& Binding : ConfiguredTargets)
	{
		// FComponentReference 存的是组件名/路径而非对象指针，需要在本 Actor 上解析
		USceneComponent* TargetComponent = Cast<USceneComponent>(Binding.Component.GetComponent(GetOwner()));
		if (TargetComponent == nullptr)
		{
			UE_LOG(LogBRTAnim, Warning, TEXT("BeginPlay: 目标 %s 的组件引用无法解析（ComponentProperty=%s, PathToComponent=%s）"),
				*Binding.TargetName.ToString(),
				*Binding.Component.ComponentProperty.ToString(),
				*Binding.Component.PathToComponent);
			continue;
		}

		if (TargetComponent->GetOwner() != GetOwner())
		{
			UE_LOG(LogBRTAnim, Warning, TEXT("BeginPlay: 目标 %s 引用的组件 %s 不属于同一 Actor，已忽略"),
				*Binding.TargetName.ToString(), *TargetComponent->GetName());
			continue;
		}

		RegisterTarget(Binding.TargetName, TargetComponent);
	}
}

void UBRT_PaperDollAnimComponent::RegisterTarget(FName TargetName, USceneComponent* Component)
{
	if (!IsValid(Component))
	{
		UE_LOG(LogBRTAnim, Warning, TEXT("RegisterTarget: 目标 %s 组件无效"), *TargetName.ToString());
		return;
	}

	const int32* Existing = TargetNameToIndex.Find(TargetName);
	if (Existing != nullptr)
	{
		FBoundTarget& Bound = BoundTargets[*Existing];
		Bound.Component = Component;
		Bound.RestLocation = Component->GetRelativeLocation();
		Bound.RestRotation = Component->GetRelativeRotation();
		Bound.RestScale = Component->GetRelativeScale3D();
		return;
	}

	FBoundTarget Bound;
	Bound.Component = Component;
	Bound.RestLocation = Component->GetRelativeLocation();
	Bound.RestRotation = Component->GetRelativeRotation();
	Bound.RestScale = Component->GetRelativeScale3D();
	TargetNameToIndex.Add(TargetName, BoundTargets.Add(Bound));
}

void UBRT_PaperDollAnimComponent::PlayAnim(UBRT_PaperDollAnimAsset* NewAnim)
{
	if (NewAnim == CurrentAnim)
	{
		return;
	}

	CurrentAnim = NewAnim;
	TimeInAnim = 0.f;
	bFireEventsAtStart = true;  // 首次 Tick 的事件区间包含起点，确保 time=0 的事件能被触发
	RuntimeTracks.Reset();

	if (!IsValid(CurrentAnim))
	{
		return;
	}

	for (int32 TrackIdx = 0; TrackIdx < CurrentAnim->Tracks.Num(); ++TrackIdx)
	{
		const FName TargetName = CurrentAnim->Tracks[TrackIdx].TargetName;
		const int32* Found = TargetNameToIndex.Find(TargetName);
		if (Found == nullptr)
		{
			UE_LOG(LogBRTAnim, Warning, TEXT("PlayAnim(%s): 目标 %s 未注册，该轨道被忽略"),
				*CurrentAnim->AnimName.ToString(), *TargetName.ToString());
			continue;
		}

		FTrackRuntime Runtime;
		Runtime.AnimTrackIndex = TrackIdx;
		Runtime.TargetIndex = *Found;
		RuntimeTracks.Add(Runtime);
	}

	// 每次切动画都打日志，运行时刷屏并拖慢帧率，排查问题时再打开
	// UE_LOG(LogBRTAnim, Log, TEXT("PlayAnim: %s (Length=%.3f, Loop=%d, Tracks=%d)"),
	// 	*CurrentAnim->AnimName.ToString(), CurrentAnim->Length,
	// 	CurrentAnim->bLoop ? 1 : 0, RuntimeTracks.Num());
}

bool UBRT_PaperDollAnimComponent::PlayAnimByName(FName AnimName)
{
	for (const TObjectPtr<UBRT_PaperDollAnimAsset>& Asset : AnimAssets)
	{
		if (IsValid(Asset) && Asset->AnimName == AnimName)
		{
			PlayAnim(Asset);
			return true;
		}
	}

	UE_LOG(LogBRTAnim, Warning, TEXT("PlayAnimByName: 未找到名为 %s 的动画资产"), *AnimName.ToString());
	return false;
}

void UBRT_PaperDollAnimComponent::SetPaused(bool bPausedIn)
{
	bPaused = bPausedIn;
}

void UBRT_PaperDollAnimComponent::SetPlayRate(float NewPlayRate)
{
	// 事件区间按时间单调递增计算，不支持倒放；负值按 0（暂停推进）处理
	if (NewPlayRate < 0.f)
	{
		UE_LOG(LogBRTAnim, Warning, TEXT("SetPlayRate: 不支持负播放速率(%.3f)，已按 0 处理"), NewPlayRate);
		NewPlayRate = 0.f;
	}
	PlayRate = NewPlayRate;
}

void UBRT_PaperDollAnimComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsValid(CurrentAnim) || bPaused)
	{
		return;
	}

	const float Length = CurrentAnim->Length;
	const float PrevTime = TimeInAnim;
	float NewTime = PrevTime + DeltaTime * PlayRate;

	// 用折算前的时间判断是否越过循环点，而不是事后比较 TimeInAnim < PrevTime
	bool bWrapped = false;
	if (CurrentAnim->bLoop && Length > SMALL_NUMBER)
	{
		if (NewTime >= Length)
		{
			bWrapped = true;
			NewTime = FMath::Fmod(NewTime, Length);
		}
	}
	else
	{
		NewTime = FMath::Clamp(NewTime, 0.f, Length);
	}
	TimeInAnim = NewTime;

	// 组装变换：Rest 为基，被轨道驱动的通道覆盖
	const int32 NumTargets = BoundTargets.Num();
	TArray<FVector, TInlineAllocator<8>> Locations;
	TArray<FRotator, TInlineAllocator<8>> Rotations;
	TArray<FVector, TInlineAllocator<8>> Scales;
	Locations.SetNumUninitialized(NumTargets);
	Rotations.SetNumUninitialized(NumTargets);
	Scales.SetNumUninitialized(NumTargets);
	for (int32 TargetIdx = 0; TargetIdx < NumTargets; ++TargetIdx)
	{
		Locations[TargetIdx] = BoundTargets[TargetIdx].RestLocation;
		Rotations[TargetIdx] = BoundTargets[TargetIdx].RestRotation;
		Scales[TargetIdx] = BoundTargets[TargetIdx].RestScale;
	}

	for (FTrackRuntime& Runtime : RuntimeTracks)
	{
		if (!BoundTargets.IsValidIndex(Runtime.TargetIndex) || !CurrentAnim->Tracks.IsValidIndex(Runtime.AnimTrackIndex))
		{
			continue;
		}
		const FBRT_PaperDollTrack& Track = CurrentAnim->Tracks[Runtime.AnimTrackIndex];
		const float Value = Track.EvaluateAtTime(TimeInAnim, Length, CurrentAnim->bLoop, Runtime.SearchHint);

		switch (Track.Channel)
		{
		case EBRT_AnimChannel::LocationX: Locations[Runtime.TargetIndex].X = Value; break;
		case EBRT_AnimChannel::LocationY: Locations[Runtime.TargetIndex].Y = Value; break;
		case EBRT_AnimChannel::LocationZ: Locations[Runtime.TargetIndex].Z = Value; break;
		case EBRT_AnimChannel::RotationPitch: Rotations[Runtime.TargetIndex].Pitch = Value; break;
		case EBRT_AnimChannel::ScaleX: Scales[Runtime.TargetIndex].X = Value; break;
		case EBRT_AnimChannel::ScaleY: Scales[Runtime.TargetIndex].Y = Value; break;
		case EBRT_AnimChannel::ScaleZ: Scales[Runtime.TargetIndex].Z = Value; break;
		default: break;
		}
	}

	for (int32 TargetIdx = 0; TargetIdx < NumTargets; ++TargetIdx)
	{
		USceneComponent* Component = BoundTargets[TargetIdx].Component.Get();
		if (!IsValid(Component))
		{
			continue;
		}
		Component->SetRelativeLocation(Locations[TargetIdx]);
		Component->SetRelativeRotation(Rotations[TargetIdx]);
		Component->SetRelativeScale3D(Scales[TargetIdx]);
	}

	// 动画事件触发：先取走「首帧包含起点」标志，避免事件回调里再次 PlayAnim 时被误清
	const bool bIncludeStart = bFireEventsAtStart;
	bFireEventsAtStart = false;

	if (CurrentAnim->Events.Num() > 0)
	{
		if (bWrapped)
		{
			// 越过循环点：先触发 (PrevTime, Length]，再触发新一圈的 [0, TimeInAnim]（含 time=0 的事件）
			UBRT_PaperDollAnimAsset* AnimBeforeEvents = CurrentAnim;
			FireEventsInRange(PrevTime, Length, bIncludeStart);
			if (CurrentAnim == AnimBeforeEvents)
			{
				FireEventsInRange(0.f, TimeInAnim, /*bIncludeStart*/ true);
			}
		}
		else
		{
			FireEventsInRange(PrevTime, TimeInAnim, bIncludeStart);
		}
	}
}

void UBRT_PaperDollAnimComponent::FireEventsInRange(float RangeStart, float RangeEnd, bool bIncludeStart)
{
	UBRT_PaperDollAnimAsset* Anim = CurrentAnim;
	if (!IsValid(Anim))
	{
		return;
	}

	// 整表扫描：事件数量极少，换来不依赖 Events 排序（多条 method 轨道导入后顺序不保证按时间）
	for (int32 EventIdx = 0; EventIdx < Anim->Events.Num(); ++EventIdx)
	{
		const FBRT_PaperDollAnimEvent& Event = Anim->Events[EventIdx];
		const bool bAfterStart = bIncludeStart ? (Event.Time >= RangeStart) : (Event.Time > RangeStart);
		if (!bAfterStart || Event.Time > RangeEnd)
		{
			continue;
		}

		OnAnimEvent.Broadcast(Event.EventName, Event.Time, Event.Args);

		// 回调里切了动画：旧动画剩余事件不再触发
		if (CurrentAnim != Anim)
		{
			return;
		}
	}
}
