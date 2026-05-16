// Copyright © Earth Heroes 2025. Defend The Dungeon™ is a trademark of Earth Heroes. All Rights Reserved.


#include "Character/Skill/SingleSword/SB_SkillQ_SingleSword.h"
#include "SingleSwordHelper.h"
#include "Ability/Effect/DamageEffect.h"
#include "Character/DDCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Utility/FunctionLibrary/DTDLibrary.h"

bool USB_SkillQ_SingleSword::CanExecute(FNewAction& Action)
{
	if (ComboCount > 0)
	{
		if (CombatComponent->bSkillQ)
		{
			return false;
		}
		
		return true;
	}
	
	return Super::CanExecute(Action);
}

void USB_SkillQ_SingleSword::Execute()
{
	Super::Execute();
	
	RequestPlayMontage(Montages[MontageIndex].Montages[ComboCount], 1.f);
	
	GetWorld()->GetTimerManager().SetTimer(ComboHandle, this, &USkillBase::ResetCombo, 2.0, false);
	ComboCount++;
	if (ComboCount > 1)
	{
		//MY_LOG(LogTemp, Warning, TEXT("ComboCount = %d, set flying"), ComboCount);
		//두 번째 스킬 사용 성공 시, 콤보 핸들러는 초기화
		DDCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		CombatComponent->bQReady = false;
		GetWorld()->GetTimerManager().ClearTimer(ComboHandle);
		ComboCount = 0;
		CombatComponent->StartQCoolTime();
	}
}

bool USB_SkillQ_SingleSword::Cancel()
{
	if (!Super::Cancel()) return false;
	GetWorld()->GetTimerManager().ClearTimer(ComboHandle);
	DDCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	//MY_LOG(LogTemp, Warning, TEXT("Cancel Set Walking"));
	ComboCount = 0;

	return true;
}

bool USB_SkillQ_SingleSword::End()
{
	if (!bExecuted) return false;
	bExecuted = false;
	
	CombatComponent->SetDefaultNewAction();
	CombatComponent->bSkillQ = false;
	DDCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	//MY_LOG(LogTemp, Warning, TEXT("End Set Walking"));

	return true;
}

void USB_SkillQ_SingleSword::DetectedHit()
{
	Super::DetectedHit();

	USingleSwordHelper *SingleSwordHelper = Cast<USingleSwordHelper>(CombatHelper);
	if (!SingleSwordHelper) return;

	//전방을 빠르게 두 번 베기 -> 기본 공격과 동일한 Trace
	if (ComboCount == 1)
	{
		FVector RightVector = DDCharacter->GetActorRightVector();
		FVector StartLocation = DDCharacter->GetActorLocation() + DDCharacter->GetActorForwardVector() * 200 + RightVector * 200;
		FVector EndLocation = DDCharacter->GetActorLocation() + DDCharacter->GetActorForwardVector() * 200 - RightVector * 200;
		float SphereRadius = 105.0f;

		TArray<FHitResult> HitResults;
		bool bHit = UDTDLibrary::SphereTrace(DDCharacter, StartLocation, EndLocation, SphereRadius, HitResults);

		if (bHit)
		{
			for (FHitResult &HitResult : HitResults)
			{
				AActor *HitActor = HitResult.GetActor();
				CombatComponent->ApplyCombatDamage(HitActor,  CombatComponent->GetQAdRatio(),  CombatComponent->GetQApRatio(), true,
				EDamageType::AdDamage, EAttackType::SkillAttack, "Skill_Q_SingleSword", EHitEffectState::Default, HitResult.ImpactPoint);
			}
		}

		if (SingleSwordHelper->bEnhanced)
		{
			bToggle = !bToggle;
			SingleSwordHelper->ShootProjectile_WithRoll(bToggle ? -20.f : 20.f);
		}
	}
	else
	{
		// 아래로 내려 찍기
		FVector StartLocation = DDCharacter->GetActorLocation() + DDCharacter->GetActorForwardVector() * 200;
		FVector EndLocation = StartLocation;
		float SphereRadius =  SingleSwordHelper->bEnhanced ? 500.f : 350.0f;

		TArray<FHitResult> HitResults;
		bool bHit = UDTDLibrary::SphereTrace(DDCharacter, StartLocation, EndLocation, SphereRadius, HitResults);

		if (bHit)
		{
			for (FHitResult &HitResult : HitResults)
			{
				AActor *HitActor = HitResult.GetActor();
				CombatComponent->ApplyCombatDamage(HitActor,  CombatComponent->GetQAdRatio() * 1.5, CombatComponent->GetQApRatio(), true,
				EDamageType::AdDamage, EAttackType::SkillAttack, "Skill_Q_SingleSword", EHitEffectState::Default, HitResult.ImpactPoint);

				if (SingleSwordHelper->bEnhanced)
				{
					if (AMonsterBase *MonsterBase = Cast<AMonsterBase>(HitActor))
					{
						MonsterBase->SetStun(1.5f);
					}
				}
			}
		}
	}
}

void USB_SkillQ_SingleSword::ResetCombo()
{
	Super::ResetCombo();
	
	CombatComponent->bQReady = false;
	GetWorld()->GetTimerManager().ClearTimer(ComboHandle);
	ComboCount = 0;
	CombatComponent->StartQCoolTime();
}
