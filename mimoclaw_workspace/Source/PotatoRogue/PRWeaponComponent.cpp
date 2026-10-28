// Copyright (c) PotatoRogue. MIT Licensed.

#include "PRWeaponComponent.h"
#include "PRCharacterBase.h"
#include "PRDataAssets.h"
#include "PRAbilitySystemComponent.h"
#include "PRGameplayAbility.h"
#include "PRWeaponAbilities.h"

float UPRWeaponInstance::GetStackedDamage() const
{
	if (!Definition)
	{
		return 0.f;
	}
	return Definition->Damage * (1.f + 0.35f * FMath::Max(StackCount - 1, 0));
}

UPRWeaponComponent::UPRWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

APRCharacterBase* UPRWeaponComponent::GetOwnerCharacter() const
{
	return Cast<APRCharacterBase>(GetOwner());
}

UPRWeaponInstance* UPRWeaponComponent::AddWeapon(UPRWeaponDefinition* Definition)
{
	APRCharacterBase* Owner = GetOwnerCharacter();
	if (!Definition || !Owner)
	{
		return nullptr;
	}

	UPRAbilitySystemComponent* ASC = Owner->GetPRAbilitySystemComponent();
	if (!ASC)
	{
		return nullptr;
	}

	// 同名武器：叠加
	for (UPRWeaponInstance* Existing : Weapons)
	{
		if (Existing && Existing->Definition == Definition)
		{
			Existing->StackCount++;
			return Existing;
		}
	}

	// 职业上限 6 把武器（土豆兄弟规则）
	if (Weapons.Num() >= 6)
	{
		return nullptr;
	}

	UPRWeaponInstance* Instance = NewObject<UPRWeaponInstance>(this);
	Instance->Definition = Definition;
	Weapons.Add(Instance);

	// 授予武器技能；若未配置技能类，按槽位选择默认实现
	TSubclassOf<UPRGameplayAbility> AbilityClass = Definition->AbilityClass;
	if (!*AbilityClass)
	{
		AbilityClass = (Definition->SlotType == EPRWeaponSlot::Ranged)
			? UPRGameplayAbility_RangedAttack::StaticClass()
			: UPRGameplayAbility_MeleeAttack::StaticClass();
	}

	// SourceObject = 武器实例，技能里可读取武器面板
	FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, Instance);
	Instance->AbilityHandle = ASC->GiveAbility(Spec);
	ASC->TryActivateAbility(Instance->AbilityHandle);

	return Instance;
}

bool UPRWeaponComponent::RemoveWeapon(UPRWeaponInstance* Instance)
{
	APRCharacterBase* Owner = GetOwnerCharacter();
	if (!Instance || !Owner)
	{
		return false;
	}

	if (UPRAbilitySystemComponent* ASC = Owner->GetPRAbilitySystemComponent())
	{
		if (Instance->AbilityHandle.IsValid())
		{
			ASC->ClearAbility(Instance->AbilityHandle);
		}
	}

	return Weapons.Remove(Instance) > 0;
}

float UPRWeaponComponent::GetEstimatedDPS() const
{
	float Total = 0.f;
	for (const UPRWeaponInstance* Instance : Weapons)
	{
		if (Instance && Instance->Definition)
		{
			const UPRWeaponDefinition* Def = Instance->Definition;
			Total += Instance->GetStackedDamage() * FMath::Max(Def->ProjectileCount, 1) / FMath::Max(Def->Cooldown, 0.05f);
		}
	}
	return Total;
}
