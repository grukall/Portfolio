// Copyright © Earth Heroes 2025. Defend The Dungeon™ is a trademark of Earth Heroes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/Skill/SkillQBase.h"
#include "SB_SkillQ_SingleSword.generated.h"

class USingleSwordHelper;
/**
 * 
 */
UCLASS()
class DEFENDTHEDUNGEON_API USB_SkillQ_SingleSword : public USkillQBase
{
	GENERATED_BODY()

public:
	virtual bool CanExecute(FNewAction& Action) override;
	virtual void Execute() override;
	virtual bool Cancel() override;
	virtual bool End() override;
	
	virtual void DetectedHit() override;
	virtual void ResetCombo() override;

private:
	bool bToggle = false;
};
