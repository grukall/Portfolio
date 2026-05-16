// Copyright © Earth Heroes 2024. Defend The Dungeon™ is a trademark of Earth Heroes. All Rights Reserved.


#include "CombatComponent.h"

#include "Ability/Effect/BarrierEffect.h"
#include "Ability/Effect/StealthHeistEffect.h"
#include "Camera/CameraComponent.h"
#include "Character/Skill/SkillBase.h"
#include "Character/Skill/BowAndArrow/BowAndArrowHelper.h"
#include "Character/Skill/DarkMagicWand/DarkMagicWandHelper.h"
#include "Character/Skill/DoubleSword/DoubleSwordHelper.h"
#include "Character/Skill/MagicWand/MagicWandHelper.h"
#include "Character/Skill/SingleSword/SingleSwordHelper.h"
#include "Character/Skill/Spear/SpearHelper.h"
#include "Character/Skill/TwoHandSword/TwoHandSwordHelper.h"
#include "Data/DataSubsystem.h"
#include "Data/SkillData.h"
#include "DefendTheDungeon/Ability/Effect/DamageEffect.h"
#include "DefendTheDungeon/Ability/StatComponent/CharacterStatComponent.h"
#include "DefendTheDungeon/Ability/StatComponent/MonsterStatComponent.h"
#include "DefendTheDungeon/Actor/Damageable/DamageableActor.h"
#include "DefendTheDungeon/Character/DDCharacter.h"
#include "DefendTheDungeon/Character/Monster/MonsterBase.h"
#include "DefendTheDungeon/ETC/CustomMacro.h"
#include "DefendTheDungeon/PlayerController/IngamePlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Skill/SkillActorHaveStatComp.h"
#include "Skill/SpawnSkill/DecalSkillActor.h"
#include "UI/CombatWidget/SkillWidget.h"
#include "UI/HUD/W_IngameHUD.h"
#include "Utility/Pooling/CharacterDecalSkillPool.h"


// Sets default values for this component's properties
UCombatComponent::UCombatComponent(): DDCharacter(nullptr)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);;

	static ConstructorHelpers::FObjectFinder<UParticleSystem> ParticleSystem(TEXT("/Game/Effects/FXVarietyPack/Particles/P_ky_waterBallHit.P_ky_waterBallHit"));
	if (ParticleSystem.Succeeded())
	{
		BlockSuccessEffect2 = ParticleSystem.Object;
	}
	
	constexpr int ActionCount = ENewActionType::Max;
	SkillBases.SetNum(ActionCount);
	
	CombatHelperBaseClasses.SetNum(CombatHelperMax);
	CombatHelperBases.SetNum(CombatHelperMax);
	
	static ConstructorHelpers::FClassFinder<UCombatHelperBase> SingleSwordHelpers(TEXT("/Game/Blueprints/CharacterBP/CombatHepler/CH_SingleSword"));
	if (SingleSwordHelpers.Succeeded())
	{
		CombatHelperBaseClasses[SingleSword] = (SingleSwordHelpers.Class);
	}

	static ConstructorHelpers::FClassFinder<UCombatHelperBase> DarkMagicWandHelpers(TEXT("/Game/Blueprints/CharacterBP/CombatHepler/CH_DarkMagicWand"));
	if (DarkMagicWandHelpers.Succeeded())
	{
		CombatHelperBaseClasses[DarkMagicWand] = (DarkMagicWandHelpers.Class);
	}
	
	static ConstructorHelpers::FClassFinder<UCombatHelperBase> SpearHelpers(TEXT("/Game/Blueprints/CharacterBP/CombatHepler/CH_Spear"));
	if (SpearHelpers.Succeeded())
	{
		CombatHelperBaseClasses[Spear] = (SpearHelpers.Class);
	}

	static ConstructorHelpers::FClassFinder<UCombatHelperBase> TwoHandSwordHelpers(TEXT("/Game/Blueprints/CharacterBP/CombatHepler/CH_TwoHandSword"));
	if (TwoHandSwordHelpers.Succeeded())
	{
		CombatHelperBaseClasses[TwoHandSword] = (TwoHandSwordHelpers.Class);
	}

	// static ConstructorHelpers::FClassFinder<UCombatHelperBase> MagicWandHelpers(TEXT("/Game/Blueprints/CharacterBP/CombatHepler/CH_MagicWand"));
	// if (MagicWandHelpers.Succeeded())
	// {
	// 	CombatHelperBaseClasses[MagicWand] = (MagicWandHelpers.Class);
	// }
	
	static ConstructorHelpers::FClassFinder<UCombatHelperBase> DoubleSwordHelpers(TEXT("/Game/Blueprints/CharacterBP/CombatHepler/CH_DoubleSword"));
	if (DoubleSwordHelpers.Succeeded())
	{
		CombatHelperBaseClasses[DoubleSword] = (DoubleSwordHelpers.Class);
	}

	//서브 오브젝트가 이 액터(액터 컴포넌트)를 이요해 리플리케이트 할려는 경우 true
	bReplicateUsingRegisteredSubObjectList = true;
}

bool UCombatComponent::CanEquipItem() const
{
	if (!CanPlayNewAction(5)) return false;

	if (!bQReady || !bEReady || !bRReady || bIsAttacking || !ActionQueue.IsEmpty() || bIsProcessingAction) return false;
	return true;
}

void UCombatComponent::OnDamaged(AActor* InInstigator, float Damage, EAttackType DamageAttackType)
{
	//MY_LOG(LogTemp, Error, TEXT("Damage = %f"), Damage)
	if (Damage > 0.f)
	{
		if (bStealthed) EndStealth();
	}
}

void UCombatComponent::SetSkillBase_Init()
{
	if (GetNetMode() == NM_ListenServer)
	{
		UDataSubsystem* Subsystem = DDCharacter->GetGameInstance()->GetSubsystem<UDataSubsystem>();
		if (!Subsystem) LOG_RETURN(Error, TEXT("Fail To Get DataSubSystem"));
		
		if (const FSkillData *SkillData = Subsystem->GetSkillData("Dash"))
		{
			SetSkillBase(SkillData, DashAction, nullptr);
		}
		else MY_LOG(LogTemp, Error, TEXT("Failed To get Skill Data Dash"));

		if (const FSkillData *SkillData = Subsystem->GetSkillData("Block"))
		{
			SetSkillBase(SkillData, BlockAction, nullptr);
		}
		else MY_LOG(LogTemp, Error, TEXT("Failed To get Skill Data Block"));
	
		if (const FSkillData *SkillData = Subsystem->GetSkillData("Stun"))
		{
			SetSkillBase(SkillData, StunAction, nullptr);
		}
		else MY_LOG(LogTemp, Error, TEXT("Failed To get Skill Data Stun"));

		if (const FSkillData *SkillData = Subsystem->GetSkillData("KnockBack"))
		{
			SetSkillBase(SkillData, KnockBackAction, nullptr);
		}
		else MY_LOG(LogTemp, Error, TEXT("Failed To get Skill Data KnockBack"));

		if (const FSkillData *SkillData = Subsystem->GetSkillData("BigKnockBack"))
		{
			SetSkillBase(SkillData, BigKnockBackAction, nullptr);
		}
		else MY_LOG(LogTemp, Error, TEXT("Failed To get Skill Data BigKnockBack"));

		if (const FSkillData *SkillData = Subsystem->GetSkillData("Dead"))
		{
			SetSkillBase(SkillData, DeadAction, nullptr);
		}
		else MY_LOG(LogTemp, Error, TEXT("Failed To get Skill Data Dead"));
	}
}

void UCombatComponent::SetSkillBase(const FSkillData* SkillData, ESkillCommand SkillCommand, UCombatHelperBase *CombatHelperBase)
{
	//SkillCommand를 ENewActionType으로 변경
	ENewActionType ActionType;
	if (SkillCommand == ESkillCommand::Attack)
		ActionType = ENewActionType::AttackAction;
	else if (SkillCommand == ESkillCommand::Skill_Q)
		ActionType = ENewActionType::SkillQAction;
	else if (SkillCommand == ESkillCommand::Skill_E)
		ActionType = ENewActionType::SkillEAction;
	else if (SkillCommand == ESkillCommand::Skill_R)
		ActionType = ENewActionType::SkillRAction;
	else
		LOG_RETURN(Error, TEXT("이건 말이 안되지만 SKillCommand가 4개뿐인데 말이 안되지만 혹시나 해서"));

	//이전 스킬이 있다면 제거
	if (USkillBase *OldSkillBase =  SkillBases[ActionType])
	{
		RemoveReplicatedSubObject(OldSkillBase);
		SkillBases[ActionType] = nullptr;
		OldSkillBase->MarkAsGarbage();
	}
	
	//CombatComp 스킬 설정
	if (!SkillData->SkillClass) LOG_RETURN(Error, TEXT("No Valid SkillBase Class, Check SkillDataTable"));
	
	USkillBase *SkillBase = NewObject<USkillBase>(this, SkillData->SkillClass);
	if (!SkillBase) LOG_RETURN(Error, TEXT("Failed Create SKillBase"));

	AddReplicatedSubObject(SkillBase);
	
	SkillBase->Initialize(this, *SkillData, CombatHelperBase, ActionType);
	SkillBases[ActionType] = SkillBase;
}

void UCombatComponent::SetSkillBase(const FSkillData* SkillData, ENewActionType ActionType, UCombatHelperBase *CombatHelperBase)
{
	//이전 스킬이 있다면 제거
	if (USkillBase *OldSkillBase =  SkillBases[ActionType])
	{
		RemoveReplicatedSubObject(OldSkillBase);
		SkillBases[ActionType] = nullptr;
		OldSkillBase->MarkAsGarbage();
	}

	if (!SkillData->SkillClass) LOG_RETURN(Error, TEXT("Failed Create SKillBase : No NewObject Class"));

	USkillBase *SkillBase = NewObject<USkillBase>(this, SkillData->SkillClass);
	if (!SkillBase) LOG_RETURN(Error, TEXT("Failed Create SKillBase"));
	
	AddReplicatedSubObject(SkillBase);

	FString ActionTypeString = UEnum::GetValueAsString(ActionType);
	MY_LOG(LogTemp, Log, TEXT("Set SkillBase %s, ActionType %s"), *GetNameSafe(SkillData->SkillClass), *ActionTypeString);

	SkillBase->Initialize(this, *SkillData, CombatHelperBase, ActionType);
	SkillBases[ActionType] = SkillBase;
}

USkillBase* UCombatComponent::GetSkillBase(const ENewActionType ActionType)
{
	return SkillBases[ActionType];
}

USkillBase* UCombatComponent::GetCurSkillBase()
{
	return SkillBases[CurActionType];
}

UCombatHelperBase* UCombatComponent::SpawnCombatHelper(EWeaponType MainWeaponType, EWeaponType SubWeaponType)
{
	UCombatHelperBase *CombatHelperBase = nullptr;
	switch (MainWeaponType) {
		case EWeaponType::OneHandSword:

			if (SubWeaponType == EWeaponType::OneHandSword)
			{
				if (!CombatHelperBases[DoubleSword])
				{
					CombatHelperBases[DoubleSword] = NewObject<UDoubleSwordHelper>(this, CombatHelperBaseClasses[DoubleSword]);
				}

				CombatHelperBase = CombatHelperBases[DoubleSword];
			}
			else
			{
				if (!CombatHelperBases[SingleSword])
				{
					CombatHelperBases[SingleSword] = NewObject<USingleSwordHelper>(this, CombatHelperBaseClasses[SingleSword]);
				}

				CombatHelperBase = CombatHelperBases[SingleSword];
			}
		
			break;
		case EWeaponType::TwoHandSword:
			if (!CombatHelperBases[TwoHandSword])
			{
				CombatHelperBases[TwoHandSword] = NewObject<UTwoHandSwordHelper>(this, CombatHelperBaseClasses[TwoHandSword]);
			}
		
			CombatHelperBase = CombatHelperBases[TwoHandSword];
			break;
		case EWeaponType::MagicWand:
			if (!CombatHelperBases[MagicWand])
			{
				CombatHelperBases[MagicWand] = NewObject<UMagicWandHelper>(this, CombatHelperBaseClasses[MagicWand]);
			}
		
			CombatHelperBase = CombatHelperBases[MagicWand];
			break;
		case EWeaponType::Bow:
			if (!CombatHelperBases[BowAndArrow])
			{
				CombatHelperBases[BowAndArrow] = NewObject<UBowAndArrowHelper>(this, CombatHelperBaseClasses[BowAndArrow]);
			}

			CombatHelperBase = CombatHelperBases[BowAndArrow];
			break;
		case EWeaponType::Spear:
			if (!CombatHelperBases[Spear])
			{
				CombatHelperBases[Spear] = NewObject<USpearHelper>(this, CombatHelperBaseClasses[Spear]);
			}
		
			CombatHelperBase = CombatHelperBases[Spear];
			break;
		case EWeaponType::DarkMagicWand:
			if (!CombatHelperBases[DarkMagicWand])
			{
				CombatHelperBases[DarkMagicWand] = NewObject<UDarkMagicWandHelper>(this, CombatHelperBaseClasses[DarkMagicWand]);
			}
		
			CombatHelperBase = CombatHelperBases[DarkMagicWand];
			break;
		default:
			CombatHelper = nullptr;
			return nullptr;
	}

	if (CombatHelperBase)
	{
		AddReplicatedSubObject(CombatHelperBase);
		CombatHelperBase->Initialize(this);
	}

	CombatHelper = CombatHelperBase;
	return CombatHelperBase;
}

AActor* UCombatComponent::SpawnActorByClass(const int32& Index, FVector const Location, FRotator const Rotation) const
{
	if (!CombatActors.IsValidIndex(Index))
	{
		MY_LOG(LogTemp, Error, TEXT("Invalid Index"));
		return nullptr;
	}
	
	const TSubclassOf<AActor> &ActorClass = CombatActors[Index];
	if (!IsValid(ActorClass))
	{
		MY_LOG(LogTemp, Error, TEXT("Invalid ActorClass"));
		return nullptr;
	}
	
	if (ActorClass->IsChildOf(ADecalSkillActor::StaticClass()))
	{
		if (UCharacterDecalSkillPool *CharacterDecalSkillPool = GetWorld()->GetSubsystem<UCharacterDecalSkillPool>())
		{
			const TSubclassOf<ADecalSkillActor> DecalActorClass{ActorClass};
			ADecalSkillActor* DecalActor = CharacterDecalSkillPool->GetPreparedDisabledDecalActor(DDCharacter, Location, Rotation, DecalActorClass);
			if (!DecalActor)
			{
				DecalActor->SetRangeDecal(DDCharacter->GetRootComponent());
				MY_LOG(LogTemp, Warning, TEXT("Decal pool returned nullptr for class %s"), *GetNameSafe(ActorClass));
			}

			return DecalActor; // 풀에서 가져온 데칼을 반환
		}
		else MY_LOG(LogTemp, Warning, TEXT("Actor %s is a Decal, but Decal Pool Subsystem was not found. Spawning normally."), *GetNameSafe(ActorClass));
	}

	AActor *Actor = GetWorld()->SpawnActor(ActorClass, &Location, &Rotation);
	if (!Actor)
	{
		MY_LOG(LogTemp, Error, TEXT("Fail Spawn Actor, Actor Class %s"), *GetNameSafe(ActorClass));
		return nullptr;
	}

	return Actor;
}


bool UCombatComponent::CanPlayNewAction(const int32 ActionLevel) const
{
	return ActionLevel <= CurNewAction.CancelLevel;
}


void UCombatComponent::AddAction(const FNewAction& Action)
{
	//중복된 액션 입력은 무시한다.
	if (const FNewAction *NewAction = ActionQueue.Peek())
	{
		if (*NewAction == Action)
		{
			return;
		}
	}
	
	ActionQueue.Enqueue(Action);
}

bool UCombatComponent::NewTryPlayAction_Internal(FNewAction& Action)
{
	if (!GetOwner()->HasAuthority())
	{
		MY_LOG(LogTemp, Error, TEXT("TryPlayAction Called in Client"));
		return false;
	}
	
	//대응하는 SkillBase 가져오기
	if (Action.ActionType >= ENewActionType::Max)
		MY_LOG(LogTemp, Log, TEXT("Action Type Not Valid, ActionName %s"), *Action.ActionName.ToString());
	
	
	USkillBase *SkillBase = SkillBases[Action.ActionType];
	if (!IsValid(SkillBase))
	{
		MY_LOG(LogTemp, Log, TEXT("SkillBase Not valid, ActionName %s"), *Action.ActionName.ToString());
		return false;
	}
	
	if (!SkillBase->CanExecute(Action))
	{
		MY_LOG(LogTemp, Log, TEXT("CanExecute Function false returned, ActionName %s"), *Action.ActionName.ToString());
		return false;
	}

	if (Action.ActionLevel < 0) Action.ActionLevel = 0;
	else if (Action.ActionLevel > ActionMax) Action.ActionLevel = ActionMax;

	if (Action.CancelLevel < 0) Action.CancelLevel = 0;
	else if (Action.CancelLevel > ActionMax) Action.CancelLevel = ActionMax;
	
	if (Action.bIsCombo)
	{
		MY_LOG(LogTemp, Log, TEXT("Combo!"));
	}
	else if (!CanPlayNewAction(Action.ActionLevel))
	{
		MY_LOG(LogTemp, Log, TEXT("Try Action Level %d < %d, denied, Before %s ActionName %s, ActionType %s"), Action.ActionLevel, CurNewAction.CancelLevel, *CurNewAction.ActionName.ToString() ,*Action.ActionName.ToString(), *UEnum::GetValueAsString(Action.ActionType));
		return false;
	}
	
	//이전 액션 취소 함수 호출
	if (CurNewAction.ActionType != DefaultAction && !Action.bIsCombo)
	{
		USkillBase *BeforeSkillBase = SkillBases[CurNewAction.ActionType];
		MY_LOG(LogTemp, Log, TEXT("CancelByActionSystem Called, Before Action %s will be canceled"), *CurNewAction.ActionName.ToString());

		//지금 플레이하고 있는 몽타주가 없거나, 앞으로 플레이할 액션이 몽타주가 없다면 Cancel 명시적 호출?
		BeforeSkillBase->CancelByActionSystem();

		//타이머도 초기화
		GetWorld()->GetTimerManager().ClearTimer(ActionHandle);
	}
	Action.bIsCombo = false;
	
	//액션 실행
	MY_LOG(LogTemp, Log, TEXT("Action Called, ActionName %s, ActionType %s"), *Action.ActionName.ToString(), *UEnum::GetValueAsString(Action.ActionType));
	CurNewAction = Action;

	//액션 타입과 SkillBase를 리플리케이트
	CurActionType = CurNewAction.ActionType;
	
	CurSkillBase = SkillBases[CurActionType];
	SkillBase->Execute();
	
	//타이머 설정
	if (Action.LifeTime > 0)
	{
		GetWorld()->GetTimerManager().SetTimer(ActionHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (USkillBase *BeforeSkillBase = SkillBases[CurNewAction.ActionType])
			{
				MY_LOG(LogTemp, Log, TEXT("Cancel By TimeOut, ActionType = %s"), *UEnum::GetValueAsString(CurNewAction.ActionType));
				BeforeSkillBase->End();
			} else MY_LOG(LogTemp, Error,TEXT("Cancel By TimeOut Called But Can't Find SkillBase, ActionType %d"), *UEnum::GetValueAsString(CurNewAction.ActionType));
			
		}), CurNewAction.LifeTime, false);
	}
	
	return true;
}


void UCombatComponent::SetDefaultNewAction()
{
	GetWorld()->GetTimerManager().ClearTimer(ActionHandle);
	
	FNewAction Action(DefaultAction, 0, ActionMax, -1, FName());
	MY_LOG(LogTemp, Log, TEXT("SetDefaultAction"));
	CurNewAction = Action;
}


void UCombatComponent::Server_NewTryAction_Implementation(FNewAction Action)
{
	AddAction(Action);
}


bool UCombatComponent::Attack()
{
	if (USkillBase *SkillBase = SkillBases[CurActionType])
	{
		if (SkillBase->IsConfirmSkillLActivated())
		{
			Skill_Confirm();
			return true;
		}
		//else MY_LOG(LogTemp, Log, TEXT("ConfirmSkill Is Not Activated %d"), CurActionType);
		
	}else MY_LOG(LogTemp, Warning, TEXT("No CurAction, ActionType %d"), CurActionType);
	
	Server_NewTryAction(FNewAction(AttackAction, 4, 3, -1, FName("Attack"), true));
	return true;
}

void UCombatComponent::Dash()
{
	APlayerController* PlayerController = Cast<APlayerController>(DDCharacter->GetController());
	if (!PlayerController) return;
	
	if (PlayerController->IsInputKeyDown(EKeys::A) && PlayerController->IsInputKeyDown(EKeys::W))
	{
		DashSide = ENoWeaponDash::FrontLeft;
	}
	else if (PlayerController->IsInputKeyDown(EKeys::D) && PlayerController->IsInputKeyDown(EKeys::W))
	{
		DashSide = ENoWeaponDash::FrontRight;
	}
	else if (PlayerController->IsInputKeyDown(EKeys::A) && PlayerController->IsInputKeyDown(EKeys::S))
	{
		DashSide = ENoWeaponDash::BackLeft;
	}
	else if (PlayerController->IsInputKeyDown(EKeys::D) && PlayerController->IsInputKeyDown(EKeys::S))
	{
		DashSide = ENoWeaponDash::BackRight;
	}
	else if (PlayerController->IsInputKeyDown(EKeys::A))
	{
		DashSide = ENoWeaponDash::Left;
	}
	else if (PlayerController->IsInputKeyDown(EKeys::D))
	{
		DashSide = ENoWeaponDash::Right;
	}
	else if(PlayerController->IsInputKeyDown(EKeys::W))
	{
		DashSide = ENoWeaponDash::Front;
	}
	else if (PlayerController->IsInputKeyDown(EKeys::S))
	{
		DashSide = ENoWeaponDash::Back;
	}
	else
	{
		DashSide = ENoWeaponDash::Front;
	}

	Server_Dash(DashSide);
}

void UCombatComponent::Server_Dash_Implementation(ENoWeaponDash InDashSide)
{
	DashSide = InDashSide;
	
	FNewAction Action(DashAction, 3, 2, -1, FName());
	Server_NewTryAction_Implementation(Action);

}

bool UCombatComponent::SkillQ()
{
	FNewAction QAction(SkillQAction, 3, 2, -1, FName("SkillQ"));
	Server_NewTryAction(QAction);
	
	return true;
}

//E 스킬 사용 시
bool UCombatComponent::SkillE()
{
	FNewAction EAction(ENewActionType::SkillEAction, 3, 2, -1, FName("SkillE"));
	Server_NewTryAction(EAction);
	
	return true;
}

bool UCombatComponent::SkillR()
{
	FNewAction RAction(ENewActionType::SkillRAction, 3, 2, -1, FName("SkillR"));
	Server_NewTryAction(RAction);
	
	return true;
}



void UCombatComponent::DetectedHit()
{
	if (USkillBase *SkillBase = SkillBases[CurNewAction.ActionType])
	{
		//MY_LOG(LogTemp, Log, TEXT("DetectedHit ActionType %s"), *UEnum::GetValueAsString(CurNewAction.ActionType));
		if (IsValid(SkillBase))	
			SkillBase->DetectedHit();
	}
}

void UCombatComponent::ShootProjectile()
{
	if (IsValid(CurSkillBase))
	{
		UCombatHelperBase *CombatHelper = CurSkillBase->GetCombatHelper();
		if (IsValid(CombatHelper) && CombatHelper->GetClass()->ImplementsInterface(UShootProjectileInterface::StaticClass()))
		{
			Cast<IShootProjectileInterface>(CombatHelper)->ShootProjectile();
		}
	}
}

void UCombatComponent::Client_TickUpdate()
{
	if (IsValid(CurSkillBase))
		CurSkillBase->CrosshairUpdate();
}

void UCombatComponent::Client_TickUpdateEnd()
{
	if (IsValid(CurSkillBase))
		CurSkillBase->CrosshairUpdateEnd();
}

void UCombatComponent::End(const UAnimMontage *EndMontage)
{
	for (USkillBase *SkillBase : SkillBases)
	{
		if (IsValid(SkillBase) && SkillBase->LastPlayedMontage == EndMontage)
			SkillBase->End();
	}
}

void UCombatComponent::Cancel(const UAnimMontage *CanceledMontage)
{
	for (USkillBase *SkillBase : SkillBases)
	{
		if (IsValid(SkillBase) && SkillBase->LastPlayedMontage == CanceledMontage)
			SkillBase->Cancel();
	}
}

void UCombatComponent::Skill_Confirm_Implementation()
{
	//MY_LOG(LogTemp, Log, TEXT("Skill Confirm Called, ActionType %s"), *UEnum::GetValueAsString(CurNewAction.ActionType));
	
	if (USkillBase *SkillBase = SkillBases[CurActionType])
		SkillBase->Confirm();
}

void UCombatComponent::StopMontage_Implementation(float BlendOut, UAnimMontage* Montage)
{
	if (UAnimInstance *AnimInstance = DDCharacter->GetMesh()->GetAnimInstance())
	{
		if (IsValid(Montage))
		{
			AnimInstance->Montage_Stop(BlendOut, Montage);
		}
		else MY_LOG(LogTemp, Warning, TEXT("No Valid Montage in StopMontage"));
	}
}

void UCombatComponent::StopAllMontages_Implementation()
{
	USkeletalMeshComponent* SkeletalMesh = DDCharacter->GetMesh();
	UAnimInstance* AnimInstance = SkeletalMesh->GetAnimInstance();
	if (AnimInstance)           
	{
		AnimInstance->StopAllMontages(0);
	}
	if (DDCharacter->Bow)
	{
		UAnimInstance* BowAnimInstance = DDCharacter->Bow->GetAnimInstance();
		if (BowAnimInstance)           
		{
			BowAnimInstance->StopAllMontages(0);
		}
	}
	
	if (DDCharacter->ArrowMesh)
	{
		UAnimInstance* ArrowAnimInstance = DDCharacter->ArrowMesh->GetAnimInstance();
		if (ArrowAnimInstance)           
		{
			ArrowAnimInstance->StopAllMontages(0);
		}
	}
}

void UCombatComponent::ResetValue()
{
	bIsAttacking = false;
	bSkillQ = false;
	bSkillE = false;
	bSkillR = false;
	bIsBlocking = false;
	bQReady = true;
	bEReady = true;
	bRReady = true;
	bDashReady = true;
	bBlockReady = true;
	bIsDashing = false;
	bStunned = false;
	bShocked = false;
	bIsProcessingAction = false;

	if (AIngamePlayerController *IngamePlayerController = Cast<AIngamePlayerController>(DDCharacter->GetController()))
	{
		IngamePlayerController->Client_BanSkillImage(false, true, true, true);
		IngamePlayerController->Client_SetStunState(false);
	}
	
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(QCoolTimeHandle);
		GetWorld()->GetTimerManager().ClearTimer(ECoolTimeHandle);
		GetWorld()->GetTimerManager().ClearTimer(RCoolTimeHandle);
	}
}



void UCombatComponent::EndStealth()
{
	if(!GetOwner()->HasAuthority()) return;
	
	if (bStealthed)
	{
		UBaseEffect* HeistEffect = DDCharacter->GetStatComponent()->FindEffectByName("StealthHeist");
		DDCharacter->GetStatComponent()->RemoveEffect(HeistEffect);
		
		bStealthed = false;
		GetWorld()->GetTimerManager().ClearTimer(StealthHandle);
		MC_SetStealth(false);
	}
}

void UCombatComponent::SetStealth()
{
	if(!GetOwner()->HasAuthority()) return;
	
	if (!bStealthed)
	{
		// 속도 이펙트 추가. 현재 더블스워드라면 이동속도 증가량 60%로 상향
		UStealthHeistEffect* SpeedEffect = NewObject<UStealthHeistEffect>();
		float SpeedPercent = 0.3f;
		if(DDCharacter->WeaponMode == EWeaponMode::DoubleSword)
		{
			SpeedPercent *= 2.f;
		}
		SpeedEffect->Initialize(GetOwner(), SpeedPercent);
		DDCharacter->GetStatComponent()->ApplyEffect(SpeedEffect);
		
		bStealthed = true;
		MC_SetStealth(true);

		//캐릭터가 스텔스화 됐다는 것을 이 캐릭터를 감지한 몬스터들에게 전달
		if (DDCharacter->OnCharacterStealthed.IsBound())
		{
			DDCharacter->OnCharacterStealthed.Broadcast(DDCharacter);
		}
		GetWorld()->GetTimerManager().SetTimer(StealthHandle, this, &UCombatComponent::EndStealth, 10, false);
	}
}

void UCombatComponent::MC_SetStealth_Implementation(bool bStealth)
{
	if (bStealth)
	{
		//MY_LOG(LogTemp, Error, TEXT("Stealth Activated!!"));
		VisibleMaterials = DDCharacter->GetDDCharacterMaterials();

		DDCharacter->SetDDCharacterMaterials(StealthMaterial);
	}
	else
	{
		//MY_LOG(LogTemp, Error, TEXT("Stealth DeActivated!!"));
		for (int32 i = 0; i < VisibleMaterials.Num(); i++)
		{
			DDCharacter->SetDDCharacterMaterial(i, VisibleMaterials[i]);
		}
	}
}

void UCombatComponent::Stun(float Duration)
{
	FNewAction Action(ENewActionType::StunAction, 1, 1, Duration,"Stun");
	
	if (GetNetMode() != NM_ListenServer)
	{
		Server_NewTryAction(Action);
		return;
	}
	
	Server_NewTryAction_Implementation(Action);
}

void UCombatComponent::CL_SetbCanLook_Implementation(bool bNewCanLook)
{
	if (AIngamePlayerController *IngamePlayerController = Cast<AIngamePlayerController>(DDCharacter->GetController()))
	{
		IngamePlayerController->bCanLook = bNewCanLook;
	}
}

// Called when the game starts
void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ADDCharacter *AddCharacter = Cast<ADDCharacter>(GetOwner()))
	{
		ProjectileShooterComp = AddCharacter->GetProjectileShooterComponent();
		StatComponent = AddCharacter->GetStatComponent();
		
		if (!StatComponent->OnDamagedDelegate.IsAlreadyBound(this, &UCombatComponent::OnDamaged))
			StatComponent->OnDamagedDelegate.AddDynamic(this, &UCombatComponent::OnDamaged);
		MY_LOG(LogTemp, Log, TEXT("On Damaged Dynamic binded"));
	}
}

void UCombatComponent::ResetBeforeAttack()
{
	//공격 시 은신 풀림
	if (bStealthed) EndStealth();
}

void UCombatComponent::StartQCoolTime()
{
	Client_StartQCoolTime();
	GetWorld()->GetTimerManager().SetTimer(QCoolTimeHandle, this, &UCombatComponent::QReady, QCoolTime, false);
	//MY_LOG(LogTemp, Warning, TEXT("QCoolTimeStart : %f"), QCoolTime);
}

void UCombatComponent::StartECoolTime()
{
	float NewECoolTime = ECoolTime;

	// 쌍검일 때 은신 대기시간이 줄어든다
	if(DDCharacter->WeaponMode == EWeaponMode::DoubleSword)
	{
		NewECoolTime /= 3;
	}

	Client_StartECoolTime(NewECoolTime);
	
	GetWorld()->GetTimerManager().SetTimer(ECoolTimeHandle, this, &UCombatComponent::EReady, NewECoolTime, false);
	//MY_LOG(LogTemp, Warning, TEXT("ECoolTimeStart : %f"), ECoolTime);
}

void UCombatComponent::StartRCoolTime()
{
	Client_StartRCoolTime();
	GetWorld()->GetTimerManager().SetTimer(RCoolTimeHandle, this, &UCombatComponent::RReady, RCoolTime, false);
	//MY_LOG(LogTemp, Warning, TEXT("RCoolTimeStart : %f"), RCoolTime);
}

void UCombatComponent::StartDashCoolTime()
{
	Client_StartDashCoolTime();
	GetWorld()->GetTimerManager().SetTimer(DashCoolTimeHandle, this, &UCombatComponent::DashReady, DashCoolTime, false);
}

void UCombatComponent::StartBlockCoolTime()
{
	Client_StartBlockCoolTime();
	GetWorld()->GetTimerManager().SetTimer(BlockCoolTimeHandle, this, &UCombatComponent::BlockReady, BlockCoolTime, false);
	//MY_LOG(LogTemp, Warning, TEXT("BlockCoolTimeStart : %f"), BlockCoolTime);
}

void UCombatComponent::Client_StartQCoolTime_Implementation()
{
	
	if(AIngamePlayerController* MyPC = Cast<AIngamePlayerController>(DDCharacter->GetController()))
	{
		if(MyPC->IngameHUD && MyPC->IngameHUD->WBP_Skill_Q)
		{
			MyPC->IngameHUD->WBP_Skill_Q->StartCooldown(QCoolTime);
		}
	}
}

void UCombatComponent::Client_StartECoolTime_Implementation(float CoolTime)
{
	if(AIngamePlayerController* MyPC = Cast<AIngamePlayerController>(Cast<APawn>(GetOwner())->GetController()))
	{
		if(MyPC->IngameHUD && MyPC->IngameHUD->WBP_Skill_E)
		{
			MyPC->IngameHUD->WBP_Skill_E->StartCooldown(CoolTime);
			//MY_LOG(LogTemp, Log, TEXT("ECoolTime %f"), ECoolTime);
		}
	}
}

void UCombatComponent::Client_StartRCoolTime_Implementation()
{
	if(AIngamePlayerController* MyPC = Cast<AIngamePlayerController>(Cast<APawn>(GetOwner())->GetController()))
	{
		if(MyPC->IngameHUD && MyPC->IngameHUD->WBP_Skill_R)
		{
			MyPC->IngameHUD->WBP_Skill_R->StartCooldown(RCoolTime);
		}
	}
}

void UCombatComponent::Client_StartBlockCoolTime_Implementation()
{
	//Block CoolTime Widget 설정 추가
	if(AIngamePlayerController* MyPC = Cast<AIngamePlayerController>(Cast<APawn>(GetOwner())->GetController()))
	{
		if(MyPC->IngameHUD && MyPC->IngameHUD->WBP_Skill_Block)
		{
			MyPC->IngameHUD->WBP_Skill_Block->StartCooldown(BlockCoolTime);
		}
	}
}


void UCombatComponent::Client_StartDashCoolTime_Implementation()
{
	//Dash CoolTime Widget 설정 추가
	if(AIngamePlayerController* MyPC = Cast<AIngamePlayerController>(Cast<APawn>(GetOwner())->GetController()))
	{
		if(MyPC->IngameHUD && MyPC->IngameHUD->WBP_Skill_Dash)
		{
			MyPC->IngameHUD->WBP_Skill_Dash->StartCooldown(DashCoolTime);
		}
	}
}


void UCombatComponent::QReady()
{
	bSkillQ = false;
	bQReady = true;
	//MY_LOG(LogTemp, Error, TEXT("QReady"));
}

void UCombatComponent::EReady()
{
	bSkillE = false;
	bEReady = true;
	//MY_LOG(LogTemp, Error, TEXT("EReady"));
}

void UCombatComponent::RReady()
{
	bSkillR = false;
	bRReady = true;
	//MY_LOG(LogTemp, Error, TEXT("RReady"));
}

void UCombatComponent::BlockReady()
{
	bIsBlocking = false;
	bBlockReady = true;
	//MY_LOG(LogTemp, Error, TEXT("Block Ready"));
}

void UCombatComponent::DashReady()
{
	bIsDashing = false;
	bDashReady = true;
}

void UCombatComponent::CL_TickUpdateEnd_Implementation()
{
	ProjectileSpread = 0.f;
}

void UCombatComponent::CL_TickUpdate_Implementation(float IncreaseAmount)
{
	ProjectileSpread += IncreaseAmount;
	ProjectileSpread= FMath::Clamp(ProjectileSpread, 0.0f, 1.5f);
}

void UCombatComponent::Block()
{
	FNewAction Action(BlockAction, 3, 5, -1, FName("Block"));
	Server_NewTryAction(Action);
}



//넉백은 모든 몽타주를 중지시키고 캐릭터를 넘어트린다. 넉백 애니메이션 재생하는 동안 아무 행동도 할 수 없다.
void UCombatComponent::KnockBackCharacter()
{
	FNewAction Action(KnockBackAction, 1, 1, -1, FName("KnockBack"));

	if (GetNetMode() != NM_ListenServer)
	{
		Server_NewTryAction(Action);
		return;
	}

	Server_NewTryAction_Implementation(Action);
}

void UCombatComponent::BigKnockBackCharacter()
{
	FNewAction Action(BigKnockBackAction, 1, 1, -1, FName("BigKnockBack"));

	if (GetNetMode() != NM_ListenServer)
	{
		Server_NewTryAction(Action);
		return;
	}

	Server_NewTryAction_Implementation(Action);
}

void UCombatComponent::Multicast_PlayMontage_Implementation(UAnimMontage* MontageToPlay, float PlayRate, int32 ActionType)
{
	USkeletalMeshComponent* SkeletalMesh = DDCharacter->GetMesh();
	if (UAnimInstance* AnimInstance = SkeletalMesh->GetAnimInstance())           
	{
		AnimInstance->Montage_Play(MontageToPlay, PlayRate);
	}

	//몽타주 호출 시 호출
	if (USkillBase *SkillBase = SkillBases[ActionType])
	{
		SkillBase->PlayMontage_Action();
	}
}

void UCombatComponent::ShockCharacter(float Duration)
{
	bShocked = true;
	MC_ShockParticle(true);
	if (AIngamePlayerController *IngamePlayerController = Cast<AIngamePlayerController>(DDCharacter->GetController()))
	{
		IngamePlayerController->Client_BanSkillImage(true, true, true, true);
	}

	FTimerDelegate TimerDelegate = FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		bShocked = false;
		MC_ShockParticle(false);
		if (AIngamePlayerController *IngamePlayerController = Cast<AIngamePlayerController>(DDCharacter->GetController()))
		{
			IngamePlayerController->Client_BanSkillImage(false, true, true, true);
		}
	});
	
	GetWorld()->GetTimerManager().SetTimer(ShockTimerHandle, TimerDelegate, Duration, false);
}

void UCombatComponent::MC_ShockParticle_Implementation(bool bInShocked)
{
	if (bInShocked)
	{
		if (!ShockComp)
			ShockComp = UGameplayStatics::SpawnEmitterAttached(ShockParticle, DDCharacter->GetMesh(), "root");
	}
	else
	{
		if (ShockComp)
		{
			ShockComp->DestroyComponent();
			ShockComp = nullptr;
		}
	}
}

void UCombatComponent::MC_BlockSuccess_Implementation()
{
	FVector SpawnLocation = DDCharacter->GetMesh()->GetSocketLocation("SkillActorSpawn");
	if (BlockSuccessEffect2)
		UParticleSystemComponent *ParticleSystemComponent = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BlockSuccessEffect2, SpawnLocation);
}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCombatComponent, QCoolTime);
	DOREPLIFETIME(UCombatComponent, ECoolTime);
	DOREPLIFETIME(UCombatComponent, RCoolTime);
	DOREPLIFETIME(UCombatComponent, BlockCoolTime);
	DOREPLIFETIME(UCombatComponent, DashCoolTime);
	DOREPLIFETIME(UCombatComponent, AttackSpeed);
	DOREPLIFETIME(UCombatComponent, bAimingToGiveShield);
	DOREPLIFETIME(UCombatComponent, bSkillE);
	DOREPLIFETIME(UCombatComponent, bEReady);
	DOREPLIFETIME(UCombatComponent, bQReady);
	DOREPLIFETIME(UCombatComponent, bRReady);
	DOREPLIFETIME(UCombatComponent, bBlockReady);
	DOREPLIFETIME(UCombatComponent, bDashReady);
	DOREPLIFETIME(UCombatComponent, CurActionType);
	DOREPLIFETIME(UCombatComponent, SkillBases);
	DOREPLIFETIME(UCombatComponent, CurSkillBase);
}


void UCombatComponent::MC_SetWalkSpeed_Implementation(float walkspeed)
{
	DDCharacter->GetCharacterMovement()->MaxWalkSpeed = walkspeed;
}

void UCombatComponent::CrosshairHitReaction()
{
	// Crosshair Reaction
	if(!GetOwner()) return;
	AController* Controller = Cast<ACharacter>(GetOwner())->Controller;
	AIngamePlayerController* InGamePlayerController = Cast<AIngamePlayerController>(Controller);
	if(InGamePlayerController)
	{
		InGamePlayerController->Client_CrossHairHitReact();
	}
}

void UCombatComponent::ApplyCombatDamage(AActor* TargetActor, float AdScale, float ApScale, bool bHasKnockback, EDamageType DamageType, EAttackType AttackType, FName SkillName, EHitEffectState HitEffectState, FVector HitLocation)
{
	UCharacterStatComponent* CharacterStatComponent = Cast<ADDCharacter>(GetOwner())->GetStatComponent();
	if(!CharacterStatComponent) return;

	if(AttackType == EAttackType::NormalAttack)
	{
		DDCharacter->OnNormalAttackHit.Broadcast(TargetActor);
	}
	
	UMonsterStatComponent* MonsterStatComponent = nullptr;
	if (AMonsterBase *MonsterBase = Cast<AMonsterBase>(TargetActor))
	{
		MonsterStatComponent = MonsterBase->GetStatComponent();

		//HitEffect 호출
		if (HitLocation != FVector::ZeroVector)
		{
			FVector RelativeLocation = HitLocation - TargetActor->GetActorLocation();
			//MY_LOG(LogTemp, Log, TEXT("RelativeLocation = %f, %f, %f"), RelativeLocation.X, RelativeLocation.Y, RelativeLocation.Z);
			MonsterBase->GetHitEffectComponent()->Multicast_SpawnEffect(static_cast<int>(HitEffectState), nullptr, RelativeLocation);
		}
		else
			MonsterBase->GetHitEffectComponent()->Multicast_SpawnEffect(static_cast<int>(HitEffectState), nullptr, FVector::ZeroVector);
	}
	else if(ASkillActorHaveStatComp *SkillActorHaveStatComp = Cast<ASkillActorHaveStatComp>(TargetActor))
	{
		MonsterStatComponent = SkillActorHaveStatComp->GetMonsterStatComponent();
	}
	else if (ADamageableActor *DamageableActor = Cast<ADamageableActor>(TargetActor))
	{
		DamageableActor->Damaged(DDCharacter);
		return;
	}
	else
	{
		return;
	}
	
	float CharacterAD = CharacterStatComponent->GetFinalDamage(EDamageType::AdDamage);
	float CharacterAP = CharacterStatComponent->GetFinalDamage(EDamageType::ApDamage);

	if(DamageType == EDamageType::AdDamage)
	{
		MonsterStatComponent->ApplyDamage(GetOwner(), DamageType, CharacterAD * AdScale, bHasKnockback, AttackType, SkillName);
		return;
	}
	if(DamageType == EDamageType::ApDamage)
	{
		MonsterStatComponent->ApplyDamage(GetOwner(), DamageType, CharacterAP * ApScale, bHasKnockback, AttackType, SkillName);
	}
}


//화면 크로스헤어에 맞는 projectile의 발사 Rotation과 Location 값을 넣는다. 없을 시 멀리 있는 적을 맞추는 느낌으로 조정한다.
bool UCombatComponent::FindTransformToShootProjectile(FVector& Location, FRotator& Rotation, const bool bHaveGravity) const
{
	Location = DDCharacter->GetMesh()->GetSocketLocation("SkillActorSpawn");
	
	FVector StartPos = DDCharacter->CameraComp->GetComponentLocation();
	FVector EndPos = StartPos + DDCharacter->CameraComp->GetForwardVector() * 5000.f;
	
	Rotation = (EndPos - Location).Rotation();

	if (bHaveGravity)
	{
		Rotation.Pitch += 3.f;
		return true;
	}
	
	FHitResult HitResult;
	bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, StartPos, EndPos, ECC_Visibility);
	if (bHit)
	{
		const float DistFromLocation = FVector::Dist(Location, HitResult.ImpactPoint);

		//MY_LOG(LogTemp, Log, TEXT("Dist From Hit Location  %f, Max : %f"), DistFromLocation, 1200.f);
		if (DistFromLocation > 1200.f)
		{
			EndPos = HitResult.ImpactPoint;
			Rotation = (EndPos - Location).Rotation();
		}

		// 너무 가까운 경우엔 Rotation을 적절하게 설정해준다.
		else
		{
			EndPos = StartPos + DDCharacter->CameraComp->GetForwardVector() * 2000.f;
			Rotation = (EndPos - Location).Rotation();
		}
		
		 DrawDebugSphere(GetWorld(), EndPos, 25.f, 12, FColor::Green, false, 1.0f, 0, 2.0f);
		 DrawDebugDirectionalArrow(GetWorld(), Location, EndPos, 50.f, FColor::Green, false, 1.0f, 0, 3.f);
	}
	//else MY_LOG(LogTemp, Log, TEXT("No Hit"));

	return bHit;
}


// Called every frame
void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!ActionQueue.IsEmpty() && !bIsProcessingAction)
	{
		bIsProcessingAction = true;
		if (FNewAction Action; ActionQueue.Dequeue(Action))
		{
			NewTryPlayAction_Internal(Action);
		}
		else
		{
			MY_LOG(LogTemp, Warning, TEXT("Failed Dequeue Action"));
		}
		
		bIsProcessingAction = false;
	}
}

