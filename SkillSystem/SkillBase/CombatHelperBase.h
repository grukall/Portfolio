// Copyright © Earth Heroes 2025. Defend The Dungeon™ is a trademark of Earth Heroes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CombatHelperBase.generated.h"

class ADDCharacter;
class UCombatComponent;
/**
 * 
 */
UCLASS(Blueprintable)
class DEFENDTHEDUNGEON_API UCombatHelperBase : public UObject
{
	GENERATED_BODY()

public:

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Initialize(UCombatComponent *CombatComp);

	void SetAttackMontageIndex(int32 MontageIndex) const;
	virtual bool IsSupportedForNetworking() const override { return true; }

protected:
	UPROPERTY(Replicated)
	UCombatComponent *CombatComponent;
	UPROPERTY(Replicated)
	ADDCharacter *DDCharacter;
};

//발사체 발사 기능이 필요한 CombatHelper의 경우, Interface 선언
UINTERFACE(MinimalAPI)
class UShootProjectileInterface : public UInterface
{
	GENERATED_BODY()
};

class IShootProjectileInterface
{
	GENERATED_BODY()

public:
	UFUNCTION()
	DEFENDTHEDUNGEON_API virtual void ShootProjectile();
};