// Copyright © Earth Heroes 2025. Defend The Dungeon™ is a trademark of Earth Heroes. All Rights Reserved.


#include "Character/Skill/SkillBase.h"

#include "Character/DDCharacter.h"
#include "Character/CombatComponent/CombatComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Net/UnrealNetwork.h"

//=======================================
//Initialize

USkillBase::USkillBase()
{
	CombatComponent = nullptr;
	DDCharacter = nullptr;
	bIsTicking = false;
	SkillData = FSkillData();
	ComboCount = 0;
	CombatHelper = nullptr;
	ActionType = ENewActionType::ActionMax;
}

void USkillBase::Initialize(UCombatComponent* CombatComp, const FSkillData InSkillData, UCombatHelperBase* InCombatHelper, ENewActionType InActionType)
{
	CombatComponent = CombatComp;
	SkillData = InSkillData;
	DDCharacter = CombatComp->GetCharacter();
	CombatHelper = InCombatHelper;
	ActionType = InActionType;

	const int32 RowCount = InSkillData.AnimMontages.Num();
	TArray<FSoftObjectPathRow> PathsToLoad;
	PathsToLoad.Reserve(RowCount);
	for (int i = 0; i < RowCount; i++)
	{
		const FMontageSoftRow& row = InSkillData.AnimMontages[i];
		const int32 MontageCount = row.AnimMontages.Num();

		FSoftObjectPathRow TempRow;
		TempRow.Row.Reserve(MontageCount);
		for (const TSoftObjectPtr<UAnimMontage>& SoftPtr : row.AnimMontages)
		{
			if (!SoftPtr.IsNull())
			{
				TempRow.Row.Add(SoftPtr.ToSoftObjectPath());
			}
		}
		PathsToLoad.Add(TempRow);
	}

	if (PathsToLoad.IsEmpty())
	{
		// 로드할 것이 없으면 즉시 완료 처리
		Montages.Empty();
		return;
	}
	
	FStreamableDelegate DoneDelegate = FStreamableDelegate::CreateWeakLambda(this, [this, InSkillData, RowCount]() {
			
		Montages.Empty();
		Montages.Reserve(RowCount);
		ComboMax.SetNum(RowCount);

		for (int i = 0; i < RowCount; i++)
		{
			const FMontageSoftRow& row = InSkillData.AnimMontages[i];

			FMontageRow TempRow;
			for (const TSoftObjectPtr<UAnimMontage>& SoftPtr : row.AnimMontages)
			{
				// 비동기 로딩이 끝났으므로 .Get()을 호출하면 바로 로드된 포인터를 얻을 수 있습니다.
				if (UAnimMontage* LoadedMontage = SoftPtr.Get())
				{
					TempRow.Montages.Add(LoadedMontage);
				}
			}
			Montages.Add(TempRow);
			ComboMax[i] = TempRow.Montages.Num()-1;		
		}

		MY_LOG(LogTemp, Log, TEXT("All montages ready : %s"), *InSkillData.SkillName.ToString());
		bMontageLoadEnded = true;
	});

	// 3. 에셋 매니저를 통해 비동기 로딩을 요청합니다.
	for (FSoftObjectPathRow Row : PathsToLoad)
	{
		UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(Row.Row, DoneDelegate);
	}
}

void USkillBase::Tick(float DeltaTime)
{
}

bool USkillBase::IsTickable() const
{
	return bIsTicking && IsValid(CombatComponent);
}

TStatId USkillBase::GetStatId() const
{
	return TStatId();
}

void USkillBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	UObject::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USkillBase, DDCharacter);
	DOREPLIFETIME(USkillBase, CombatHelper);
	DOREPLIFETIME(USkillBase, bConfirmSkillActivated);
}

bool USkillBase::CanExecute(FNewAction &Action)
{
	if (!bMontageLoadEnded)
	{
		MY_LOG(LogTemp, Error, TEXT("No Montage or Montage Load Not Ended"));
		return false;
	}

	return true;
}

void USkillBase::Execute()
{
	bExecuted = true;
}

void USkillBase::RequestPlayMontage(UAnimMontage* MontageToPlay, float PlayRate)
{
	LastPlayedMontage = MontageToPlay;
	CombatComponent->Multicast_PlayMontage(MontageToPlay, PlayRate, ActionType);
}

void USkillBase::PlayMontage_Action()
{
}

void USkillBase::DetectedHit()
{
}

bool USkillBase::Cancel()
{
	if (!bExecuted)
	{
		MY_LOG(LogTemp, Log, TEXT("Cancel Called but bExecuted = false, return false, SkillName = %s"), *SkillData.SkillName.ToString());
		return false;
	}

	MY_LOG(LogTemp, Log, TEXT("Cancel Called, SkillName = %s"), *GetNameSafe(LastPlayedMontage), *SkillData.SkillName.ToString());
	bConfirmSkillActivated = false;
	bExecuted = false;
	LastPlayedMontage = nullptr;
	return true;
}

void USkillBase::CancelByActionSystem()
{
	if (LastPlayedMontage)
	{
		//MY_LOG(LogTemp, Log, TEXT("Stop Montage %s, SkillName = %s"), *GetNameSafe(LastPlayedMontage), *SkillData.SkillName.ToString());
		CombatComponent->StopMontage(0.f, LastPlayedMontage);
	}
	
	Cancel();
}

bool USkillBase::End()
{
	if (!bExecuted)
	{
		//MY_LOG(LogTemp, Log, TEXT("End Called but bExecuted = false, return false, SkillName = %s"), *SkillData.SkillName.ToString());
		return false;
	}

	//MY_LOG(LogTemp, Log, TEXT("End Called, SkillName = %s"), *SkillData.SkillName.ToString());
	//FDebug::DumpStackTraceToLog(ELogVerbosity::Type::Warning);
	bExecuted = false;
	LastPlayedMontage = nullptr;
	CombatComponent->SetDefaultNewAction();
	return true;
}

void USkillBase::Confirm()
{
	bConfirmSkillActivated = false;
}

void USkillBase::ConfirmSkillActivate()
{
	bConfirmSkillActivated = true;
}

void USkillBase::CrosshairUpdate()
{
}

void USkillBase::CrosshairUpdateEnd()
{
}

void USkillBase::StartTick()
{
	bIsTicking = true;
}

void USkillBase::StopTick()
{
	bIsTicking = false;
}

void USkillBase::ResetCombo()
{
	ComboCount = 0;
}

void USkillBase::OnRep_CombatHelper()
{
}

