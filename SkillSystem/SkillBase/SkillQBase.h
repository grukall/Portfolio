// Copyright © Earth Heroes 2025. Defend The Dungeon™ is a trademark of Earth Heroes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/Skill/SkillBase.h"
#include "SkillQBase.generated.h"

/**
 * 
 */
UCLASS()
class DEFENDTHEDUNGEON_API USkillQBase : public USkillBase
{
	GENERATED_BODY()

public:
	virtual bool CanExecute(FNewAction &Action) override;
	virtual void Execute() override;
	virtual bool Cancel() override;
	virtual bool End() override;
};
