// Copyright © Earth Heroes 2025. Defend The Dungeon™ is a trademark of Earth Heroes. All Rights Reserved.


#include "Character/Skill/SkillQBase.h"
#include "Character/CombatComponent/CombatComponent.h"


bool USkillQBase::CanExecute(FNewAction &Action)
{
	if (!USkillBase::CanExecute(Action)) return false;
	
	//콤보 진행 중이면 허가
	if (ComboCount > 0  || bConfirmSkillActivated)
	{
		Action.bIsCombo = true;
		return true;
	}

	if (!CombatComponent->bQReady) return false;
	return true;
}

void USkillQBase::Execute()
{
	Super::Execute();
	
	CombatComponent->bQReady = false;
	CombatComponent->bSkillQ = true;
}

bool USkillQBase::Cancel()
{
	if (!Super::Cancel()) return false;
	CombatComponent->bSkillQ = false;
	CombatComponent->StartQCoolTime();

	return true;
}

bool USkillQBase::End()
{
	if (!Super::End()) return false;
	CombatComponent->bSkillQ = false;
	CombatComponent->StartQCoolTime();

	return true;
}