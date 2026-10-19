// Fill out your copyright notice in the Description page of Project Settings.


#include "BrotatoGAS/BRT_AbilitySystemComponent.h"

#include "BrotatoGAS/Tags/BrotatoNativeTags.h"


// Sets default values for this component's properties
UBRT_AbilitySystemComponent::UBRT_AbilitySystemComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}

void UBRT_AbilitySystemComponent::OnGiveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	Super::OnGiveAbility(AbilitySpec);
	
	HandleAutoActivatedAbility(AbilitySpec);
}



void UBRT_AbilitySystemComponent::HandleAutoActivatedAbility(const FGameplayAbilitySpec& AbilitySpec)
{
	if (!IsValid(AbilitySpec.Ability)) return;

	// 只在服务器端触发自动激活
	if (!IsOwnerActorAuthoritative()) return;

	// 避免重复激活
	if (AbilitySpec.IsActive()) return;

	// 根据 Tag 判断（请确认 Tag 配置位置：AbilityTags 还是 AssetTags）
	if (AbilitySpec.Ability->AbilityTags.HasTagExact(BrotatoTags::BRTAbilities_ActivateOnGiven))
	{
		TryActivateAbility(AbilitySpec.Handle);
	}
}