// Copyright © Earth Heroes 2025. Defend The Dungeon™ is a trademark of Earth Heroes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Character/CombatComponent/CombatComponent.h"
#include "Data/SkillData.h"
#include "SkillBase.generated.h"

class UCombatHelperBase;
class ADDCharacter;
class UCombatComponent;

USTRUCT()
struct FSoftObjectPathRow
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FSoftObjectPath> Row;
};

USTRUCT(BlueprintType)
struct FMontageRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<UAnimMontage*> Montages;
};


UCLASS(Blueprintable)
class DEFENDTHEDUNGEON_API USkillBase : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	USkillBase();
	virtual void Initialize(UCombatComponent *CombatComp, const FSkillData InSkillData, UCombatHelperBase *InCombatHelper, ENewActionType InActionType);

	//==========================================
	//FTickableGameObject Interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;


	//==========================================
	// UObject Replication
	
	virtual bool IsSupportedForNetworking() const override { return true; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//=========================================
	//Execute
	virtual bool CanExecute(FNewAction &Action);

	UFUNCTION()
	virtual void Execute();
	
	UFUNCTION()
	virtual void DetectedHit();

	UFUNCTION()
	virtual bool Cancel();

	UFUNCTION()
	void CancelByActionSystem();

	UFUNCTION()
	virtual bool End();

	UFUNCTION()
	virtual void Confirm();

	UFUNCTION()
	virtual void ConfirmSkillActivate();

	UFUNCTION()
	virtual void CrosshairUpdate();

	UFUNCTION()
	virtual void CrosshairUpdateEnd();

	//=========================================
	//RPC Request/Response Function

	//PlayMontage
	virtual void RequestPlayMontage(UAnimMontage *MontageToPlay, float PlayRate);

	UPROPERTY()
	UAnimMontage *LastPlayedMontage;

	virtual void PlayMontage_Action();

	void SetMontageIndex(int32 InMontageIndex) {MontageIndex = InMontageIndex;}

protected:

	bool bExecuted = false;
	
	//=========================================
	//Raw Pointer
	UPROPERTY()
	UCombatComponent *CombatComponent;

	UPROPERTY(Replicated)
	ADDCharacter *DDCharacter;

	//=========================================
	//Montages
	UPROPERTY()
	TArray<FMontageRow> Montages;
	int32 MontageIndex = 0;

	bool bMontageLoadEnded = false;

	//=========================================
	//Skill Info
	FSkillData SkillData;

	ENewActionType ActionType;
	
	//=========================================
	//Tick Function
	void StartTick();
	void StopTick();

	//=========================================
	//Combat properties
	TArray<int32> ComboMax;
	int32 ComboCount;

	FTimerHandle ComboHandle;

	UPROPERTY(Replicated)
	bool bConfirmSkillActivated = false;
	
public:
	UFUNCTION()
	virtual void ResetCombo();

	bool IsConfirmSkillLActivated() const {return bConfirmSkillActivated;}

protected:
	//========================================
	//Combat Helper
	UPROPERTY(ReplicatedUsing = OnRep_CombatHelper)
	UCombatHelperBase *CombatHelper;

	UFUNCTION()
	virtual void OnRep_CombatHelper();

public:
	UCombatHelperBase *GetCombatHelper() const {return CombatHelper;}
	
private:
	bool bIsTicking;
};
