// Copyright © Earth Heroes 2025. Defend The Dungeon™ is a trademark of Earth Heroes. All Rights Reserved.


#include "Character/Skill/CombatHelperBase.h"

#include "AttackBase.h"
#include "Character/CombatComponent/CombatComponent.h"
#include "Net/UnrealNetwork.h"

void UCombatHelperBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	UObject::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCombatHelperBase, DDCharacter);
	DOREPLIFETIME(UCombatHelperBase, CombatComponent);
}

void UCombatHelperBase::Initialize(UCombatComponent* CombatComp)
{
	CombatComponent = CombatComp;
	DDCharacter = CombatComp->GetCharacter();
}


void UCombatHelperBase::SetAttackMontageIndex(int32 MontageIndex) const
{
	if (CombatComponent)
	{
		if (USkillBase* SkillBase = CombatComponent->GetSkillBase(AttackAction))
		{
			if (UAttackBase *AttackBase = Cast<UAttackBase>(SkillBase))
			{
				AttackBase->SetMontageIndex(MontageIndex);
			}
		}
	}
}
