// Copyright © Earth Heroes 2024. Defend The Dungeon™ is a trademark of Earth Heroes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Ability/Effect/DamageEffect.h"
#include "Component/Effect/HitEffectComponent.h"
#include "Components/ActorComponent.h"
#include "DefendTheDungeon/ETC/Enum/Enum.h"
#include "CombatComponent.generated.h"


class UCombatHelperBase;
class USkillBase;
struct FSkillData;


class AIngamePlayerController;
enum class EHitEffectState : uint8;
class AGravityProjectile;
enum class EAttackType : uint8;
enum class EDamageType : uint8;
class ADecalActor;
class ADarkMagicOrbSkill;
class UProjectileShooterComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class AMonsterBase;
class ADDCharacter;


UENUM(Blueprintable)
enum ENewActionType
{
	//액션 클래스을 통해 실행합니다.
	NormalAction,

	//기본 상태
	DefaultAction,
	DeadAction,
	AttackAction,
	SkillQAction,
	SkillEAction,
	SkillRAction,
	BlockAction,
	DashAction,
	StunAction,
	KnockBackAction,
	BigKnockBackAction,
	Max,
};

UENUM()
enum ECombatHelperType
{
	SingleSword,
	DarkMagicWand,
	Spear,
	TwoHandSword,
	MagicWand,
	DoubleSword,
	BowAndArrow,
	CombatHelperMax
};

USTRUCT(Blueprintable)
struct FNewAction
{
	GENERATED_BODY()
	
	//액션 타입
	UPROPERTY()
	TEnumAsByte<ENewActionType> ActionType;

	//실행 우선순위
	UPROPERTY()
	int32 ActionLevel;

	//취소 우선순위
	UPROPERTY()
	int32 CancelLevel;

	//액션 lifetime 입니다. LifeTime이 지나면 액션은 End 함수를 호출합니다. -1이면 무제한 지속됩니다.
	UPROPERTY()
	float LifeTime;
	
	//액션 이름, ActionName이 달라도 다른 속성이 동일한 경우, 같은 Action으로 취급합니다.
	UPROPERTY()
	FName ActionName;

	UPROPERTY()
	bool bIsCombo = false;

	UPROPERTY()
	bool bIsContinuous = false;

	// UPROPERTY()
	// TSubclassOf<USkillBase> SkillClass = USkillBase::StaticClass();

	FNewAction()
	: ActionType(DefaultAction) // 기본값으로 Normal 설정 가정
	, ActionLevel(100)   // 기본 우선순위(낮음) 예시
	, CancelLevel(100)   // 기본 취소 우선순위(낮음) 예시
	, LifeTime(-1)
	, ActionName(NAME_None)
	{
	}

	FNewAction(ENewActionType ActionType, int32 ActionLevel, int32 CancelLevel, float LifeTime,  FName ActionName)
	: ActionType(ActionType)
	, ActionLevel(ActionLevel)
	, CancelLevel(CancelLevel)
	, LifeTime(LifeTime)
	, ActionName(ActionName)
	{
	}
	
	FNewAction(ENewActionType ActionType, int32 ActionLevel, int32 CancelLevel, float LifeTime,  FName ActionName, bool bContinuous)
	: ActionType(ActionType)
	, ActionLevel(ActionLevel)
	, CancelLevel(CancelLevel)
	, LifeTime(LifeTime)
	, ActionName(ActionName)
	, bIsContinuous(bContinuous)
	{
	}

	bool operator==(const FNewAction& Action) const
	{
		if (   ActionLevel == Action.ActionLevel
			&& CancelLevel == Action.CancelLevel
			&& ActionType == Action.ActionType
			&& LifeTime == Action.LifeTime)
			return true;

		return false;
	}
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class DEFENDTHEDUNGEON_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

protected:
	
	UCombatComponent();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	UPROPERTY()
	ADDCharacter *DDCharacter;
	UPROPERTY()
	UBaseStatComponent *StatComponent;

	UPROPERTY(Blueprintable, EditAnywhere)
	TArray<TSubclassOf<UCombatHelperBase>> CombatHelperBaseClasses;

	UPROPERTY()
	TArray<UCombatHelperBase*> CombatHelperBases;

	UPROPERTY(EditAnywhere, Category="Montage")
	TArray<UAnimMontage*> KnockBackMontage;

	UPROPERTY(EditAnywhere, Category="Montage")
	UAnimMontage *BigKnockBackMontage;

	//Particles
	UPROPERTY(EditAnywhere, Category="Effect")
	UParticleSystem *BlockSuccessEffect2;
	UPROPERTY(EditAnywhere, Category="Effect")
	UParticleSystem *ShockParticle;

	
public:
	
	//초기 세팅
	void SetCharacter(ADDCharacter *AddCharacter) { DDCharacter = AddCharacter;}
	ADDCharacter* GetCharacter() const { return DDCharacter; }
	
	/**
	 * 현재 캐릭터가 아이템을 장착할 수 있는 상태인지 검사합니다.
	 * @return 장착 가능하면 true, 불가능하면 false 반환.
	 */
	bool CanEquipItem() const;

	//델리게이트
	UFUNCTION()
	void OnDamaged(AActor* InInstigator, float Damage, EAttackType DamageAttackType);

/*********************************************************/
/*
 스킬 관리
 SkillBases = {SkillQ, SkillE, SKillR, Attack}
 */

	void SetSkillBase_Init();
	void SetSkillBase(const FSkillData *SkillData, ESkillCommand SkillCommand, UCombatHelperBase *CombatHelperBase);
	void SetSkillBase(const FSkillData *SkillData, ENewActionType ActionType, UCombatHelperBase *CombatHelperBase);

	USkillBase *GetSkillBase(const ENewActionType ActionType);
	USkillBase *GetCurSkillBase();
	
	UCombatHelperBase *SpawnCombatHelper(EWeaponType MainWeaponType, EWeaponType SubWeaponType);

	UPROPERTY(EditAnywhere, Category=CombatActor)
	TArray<TSubclassOf<AActor>> CombatActors;
	
	AActor* SpawnActorByClass(const int32 &Index, FVector const Location = FVector::ZeroVector , FRotator const Rotation = FRotator::ZeroRotator) const;

private:

	UPROPERTY(Replicated)
	TArray<USkillBase*> SkillBases;

	UPROPERTY(Replicated)
	USkillBase* CurSkillBase;

	UPROPERTY()
	UCombatHelperBase *CombatHelper;

protected:

/*********************************************************/
/*
 행동(Action) 가상 함수 목록
 캐릭터가 할 수 있는 행동들은 우선 순위가 정해져 있다.
 
 하고자 할 행동의 우선 순위가 현재 하고 있는 행동보다 낮으면, 실행을 거절한다.
 만약 반대인 경우, 현재 행동을 중단하고 입력받은 행동을 실행한다.

우선순위 변수는 CancelLevel, ActionLevel이 있는데, 낮을 수록 높은 우선순위를 가진다.
취소 우선수위를 따로 다루는 이유는, 대부분의 Action이 실행 우선순위와 취소 우선순위가 다르기 때문이다.

예를 들면, 우리 게임의 경우 캐릭터의 스킬 시전 중, 다른 스킬을 사용할 수 없다.
하지만 Stun은 실행중인 스킬 액션을 취소하고, 스턴에 걸릴 수 있다. 

 가장 낮은 우선순위 행동은 idle, move등의 Default 상태 이며, 이 행동은 중단될 경우가 많다.
 가장 높은 우선순위 행동은 dead로, 다른 어떠한 행동도 할 수 없다.

 현재까지 대표적인 Action의 우선순위이다.
			ActionLev	CancelLev
	Dead		0			0
	Stun		1			1
	KnockBack	1			1
	BigKK		1			1
	Dash		3			2
	Attack		4			3
	Skill		3			2
	Block		3			5
	jump		3	        *
	Default	  Max(100)    Max(100)

	* : 디른 어떤 액션도 할 수 있지만 취소 되진 않는다.
 */
public:
	FNewAction CurNewAction;

	UPROPERTY(Replicated)
	int32 CurActionType;
	
private:
	

	//타이머
	FTimerHandle ActionHandle;
	bool bIsProcessingAction = false;

	//대기 큐
	TQueue<FNewAction> ActionQueue;
	
	void AddAction(const FNewAction &Action);
	bool NewTryPlayAction_Internal(FNewAction &Action);
		
public:
	UFUNCTION()
	void SetDefaultNewAction();
	
	//Action Max
	static constexpr int32 ActionMax = 100;

	/**
	 * 플레이어가 특정 액션을 지금 할 수 있는지 검사합니다.
	 * 
	 * @param ActionLevel 캐릭터가 시도하려는 액션의 우선순위입니다.
	 * 숫자가 낮을수록 우선순위가 높으며, 0이 최고 우선순위, ActionMax가 최하위 우선순위입니다.
	 * 현재 실행 중인 액션의 CancelLevel과 비교하여 허용 여부를 판정합니다.
	 * 
	 * @return 현재 상태에서 주어진 ActionLevel 액션을 수행할 수 있으면 true, 그렇지 않으면 false 반환.
	*/
	
	bool CanPlayNewAction(const int32 ActionLevel) const;

	/**
	 * 클라이언트에서 액션 실행을 요청할 때 호출하는 서버 RPC 함수입니다.
	 * 
	 * @param Action 실행을 요청하는 액션 구조체 정보입니다.
	 * 
	 * 서버 전용, 신뢰 가능한 (Reliable) 호출이어야 하며, 네트워크 환경에서 클라이언트가 액션 수행 요청을 할 때 사용합니다.
	 */

	UFUNCTION(Server, Reliable)
	void Server_NewTryAction(FNewAction Action);
	
	/**
	 * 현재 캐릭터가 수행 중인 액션 정보를 가져옵니다.
	 * 
	 * @return 현재 액션 구조체.
	 */
	FNewAction &GetCurNewAction() {return CurNewAction;}
	
/*
 1. 행동 시작 함수
 플레이어 입력 시 호출되는 함수
 -  애니메이션 유효 검사
 - 시전 시 처리해야 할 행동
 */
	UFUNCTION(BlueprintCallable)
	virtual bool Attack();
	bool bAttackStarted = false;
	
	void Dash();
	UFUNCTION(Server, Reliable)
	virtual void Server_Dash(ENoWeaponDash InDashSide);
	
	UFUNCTION(BlueprintCallable)
	virtual bool SkillQ();

	UFUNCTION(BlueprintCallable)
	virtual bool SkillE();

	UFUNCTION(BlueprintCallable)
	virtual bool SkillR();
	
	UFUNCTION(BlueprintCallable, Server, Reliable)
	void Skill_Confirm();
	
	void Stun(float Duration);
	void Block();
	void ShockCharacter(float Duration);
	void BigKnockBackCharacter();
	void KnockBackCharacter();


public:
/*
 2. 행동 중간 함수
 행동 중, 중간에 해야할 일을 정의하는 함수, 애님 몽타주 중, AnimNotify, AnimNotifyState로 호출된다.
 AnimNotify 목록(NS_HitDetection, NS_ShootProjectile, NS_ClientTickUpdate, AN_ESkill)
 - 주로 적 쿼리를 위한 Trace 후 공격을 구현
 - 발사체 발사
 - 애니메이션 특정 구간 동안 UI 처리
 */
	UFUNCTION()
	virtual void DetectedHit();
	
	UFUNCTION()
	virtual void ShootProjectile();
	
	UFUNCTION()
	virtual void Client_TickUpdate();
	UFUNCTION(BlueprintCallable)
	virtual void Client_TickUpdateEnd();

	
/*
 3. 행동 끝 함수
 행동이 온전히 끝나면 호출되는 함수, 애님 몽타주 중, AnimNotify, AnimNotifyState로 호출된다.
 온전히 끝나지 않으면 중단 함수 호출
 */
	UFUNCTION()
	void End(const UAnimMontage *EndMontage);

/*
 4. 중단 함수
 행동 검사 함수에 의해 호출, 행동 중 중단 시 호출된다.
 - 행동 중단 시, 각 행동에 대한 처리
 */
	UFUNCTION()
	void Cancel(const UAnimMontage *CanceledMontage);
	
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable)
	virtual void StopAllMontages();

	UFUNCTION(BlueprintCallable, NetMulticast, Reliable)
	void StopMontage(float BlendOut, UAnimMontage *Montage);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayMontage(UAnimMontage *MontageToPlay, float PlayRate, int32 ActionType);

	

	//===============================
	//ETC 함수
	
	UFUNCTION(NetMulticast, Reliable)
	void MC_BlockSuccess();
	
	//UI에 필요한 함수
	FORCEINLINE float GetSpread() const { return ProjectileSpread; }
	
	//카메라를 움직일 수 있는지
	UFUNCTION(Client, Reliable)
	void CL_SetbCanLook(bool bNewCanLook);

	UFUNCTION(NetMulticast, Reliable)
	void MC_SetWalkSpeed(float walkspeed);
	void CrosshairHitReaction();
	
	/**
	 * 대상 액터에 공격 피해를 적용하는 함수입니다.
	 * 
	 * @param TargetActor 피해를 입힐 대상 액터.
	 * @param AdScale 물리 공격력 배율.
	 * @param ApScale 마법 공격력 배율.
	 * @param bHasKnockback 넉백 효과 적용 여부.
	 * @param DamageType 피해 유형 (물리, 마법 등).
	 * @param AttackType 공격 유형 (일반 공격, 스킬 등).
	 * @param SkillName 공격/스킬 이름.
	 * @param HitEffectState 피격 이펙트 상태.
	 * @param HitLocation 피격 위치 정보 (ZeroVector면 액터 위치 기준).
	 * 
	 * 일반 공격일 경우 캐릭터의 OnNormalAttackHit 델리게이트를 브로드캐스트합니다.
	 * 대상이 몬스터라면 피격 위치를 기준으로 피격 이펙트를 재생하며, 넉백 여부와 공격 타입, 스킬명도 전달합니다.
	 * ADamageableActor 유형 대상이면 별도 Damaged 함수를 호출하고 피해 적용 과정을 종료합니다.
	 * 대상이 지정된 타입이 아니면 피해 적용을 하지 않고 리턴합니다.
	 */
	void ApplyCombatDamage(AActor* TargetActor, float AdScale = 1.f, float ApScale = 1.f, bool bHasKnockback = false, EDamageType DamageType = EDamageType::AdDamage, EAttackType AttackType = EAttackType::NormalAttack, FName SkillName = TEXT("None"), EHitEffectState HitEffectState = EHitEffectState::None, FVector HitLocation = FVector::ZeroVector);

	/**
	 * 화면 크로스헤어에 맞춰 발사할 프로젝타일의 위치와 회전 값을 계산합니다.
	 * 
	 * @param Location 프로젝타일이 생성될 위치. 캐릭터의 "SkillActorSpawn" 소켓 위치를 기본으로 사용합니다.
	 * @param Rotation 프로젝타일의 발사 회전 값. 크로스헤어가 가리키는 방향 또는 맞춤 위치를 기반으로 설정됩니다.
	 * @param bHaveGravity 프로젝타일에 중력 영향 여부. 중력 영향을 받는 경우 발사 각도를 약간 조정합니다.
	 * 
	 * @return 발사 경로 상에 충돌체가 감지되면 true, 아니면 false 반환.
	 */
	bool FindTransformToShootProjectile(FVector &Location, FRotator &Rotation, const bool bHaveGravity = false) const;

public:
	void ResetValue();
	void EndStealth();
	void SetStealth();

	UFUNCTION(NetMulticast, Reliable)
	void MC_SetStealth(bool bStealth);

	//공격 시 초기화해야 하는 변수들을 초기화 하는 함수
	void ResetBeforeAttack();

/*******************************************************************/
/*
 상태(State)
 상태는 행동의 결과
 행동 검사 시 사용
 */
private:
	//모든 캐릭터 공통 : 대쉬 애니메이션 실행 중 true.
	UPROPERTY(BlueprintReadOnly, meta=(AllowPrivateAccess = true))
	bool bIsDashing = false;
	
	//모든 캐릭터 공통 : 공격 애니메이션 or 공격 관련 코드 실행 중 true.
	UPROPERTY(BlueprintReadOnly, meta=(AllowPrivateAccess = true))
	bool bIsAttacking = false;
	
	//모든 캐릭터 공통 : SkillQ 수행 중 true.
	UPROPERTY(BlueprintReadOnly, meta=(AllowPrivateAccess = true))
	bool bSkillQ = false;

	//모든 캐릭터 공통 : SKillE 수행 중 true.
	UPROPERTY(BlueprintReadOnly, meta=(AllowPrivateAccess = true), Replicated)
	bool bSkillE = false;
	
	//모든 캐릭터 공통 : SKillR 수행 중 true.
	UPROPERTY(BlueprintReadOnly, meta=(AllowPrivateAccess = true))
	bool bSkillR = false;
	
	UPROPERTY(BlueprintReadOnly, meta=(AllowPrivateAccess = true))
	bool bIsBlockValid = false;

	//모든 캐릭터 공통 : 스킬 준비 완료시 true.
	UPROPERTY(BlueprintReadOnly, meta=(AllowPrivateAccess = true), Replicated)
	bool bQReady = true;

	UPROPERTY(BlueprintReadOnly, meta=(AllowPrivateAccess = true), Replicated)
	bool bEReady = true;

	UPROPERTY(BlueprintReadOnly, meta=(AllowPrivateAccess = true), Replicated)
	bool bRReady = true;

	UPROPERTY(BlueprintReadOnly, meta=(AllowPrivateAccess = true), Replicated)
	bool bBlockReady = true;
	
	UPROPERTY(BlueprintReadOnly, meta=(AllowPrivateAccess = true), Replicated)
	bool bDashReady = true;

	//상태 불 변수
	UPROPERTY(BlueprintReadOnly, meta=(AllowPrivateAccess = true), Replicated)
	bool bStealthed = false;
	
	UPROPERTY(BlueprintReadOnly, meta=(AllowPrivateAccess = true), Replicated)
	bool bAimingToGiveShield = false;

	UPROPERTY(BlueprintReadOnly, meta=(AllowPrivateAccess = true))
	bool bDarkMagicOrbSkill = false;

	UPROPERTY(BlueprintReadOnly, meta=(AllowPrivateAccess = true))
	bool bIsBlocking = false;

	UPROPERTY(BlueprintReadOnly, meta=(AllowPrivateAccess = true))
	bool bShocked = false;

	UPROPERTY(BlueprintReadOnly, meta=(AllowPrivateAccess = true))
	bool bStunned = false;

	UPROPERTY(BlueprintReadOnly, meta=(AllowPrivateAccess = true))
	bool bKnockbacked = false;

	UPROPERTY(BlueprintReadOnly, meta=(AllowPrivateAccess = true))
	bool bBigKnockbacked = false;
	
public:
    bool IsDashing() const { return bIsDashing; }
    bool IsAttacking() const { return bIsAttacking; }
    bool IsSkillQActive() const { return bSkillQ; }
    bool IsSkillEActive() const { return bSkillE; }
    bool IsSkillRActive() const { return bSkillR; }
    bool IsQReady() const { return bQReady; }
    bool IsEReady() const { return bEReady; }
    bool IsRReady() const { return bRReady; }
    bool IsBlockReady() const { return bBlockReady; }
    bool IsDashReady() const { return bDashReady; }
    bool IsStealthed() const { return bStealthed; }
    bool IsAimingToGiveShield() const { return bAimingToGiveShield; }
    bool IsDarkMagicOrbSkillActive() const { return bDarkMagicOrbSkill; }
    bool IsBlocking() const { return bIsBlocking; }
    bool IsShocked() const { return bShocked; }
    bool IsStunned() const { return bStunned; }
    bool IsKnockbacked() const { return bKnockbacked; }
    bool IsBigKnockbacked() const { return bBigKnockbacked; }

public:
	bool CanBeSeen() const {return !bStealthed;}


protected:
/*******************************************************************/
/*
 쿨타임 (CoolTime)
 행동 가능 함수 내부에서 쿨타임 검사를 먼저 실시한다.
 */
	
	UPROPERTY(Replicated)
	float QCoolTime = 5.f;

	UPROPERTY(Replicated)
	float ECoolTime = 5.f;

	UPROPERTY(Replicated)
	float RCoolTime = 5.f;

	UPROPERTY(Replicated)
	float BlockCoolTime = 1.5f;

	UPROPERTY(Replicated)
	float DashCoolTime = 5.f;

public:
	void StartQCoolTime();
	void StartECoolTime();
	void StartRCoolTime();
	void StartDashCoolTime();

protected:
	UFUNCTION(Client, Reliable)
	void Client_StartQCoolTime();
	UFUNCTION(Client, Reliable)
	void Client_StartECoolTime(float CoolTime);
	UFUNCTION(Client, Reliable)
	void Client_StartRCoolTime();
	UFUNCTION(Client, Reliable)
	void Client_StartBlockCoolTime();
	UFUNCTION(Client, Reliable)
	void Client_StartDashCoolTime();
	
public:
	void StartBlockCoolTime();

	void SetQCoolTime(const float InCoolTime) {QCoolTime = InCoolTime;}
	void SetECoolTime(const float InCoolTime) {ECoolTime = InCoolTime;}
	void SetRCoolTime(const float InCoolTime) {RCoolTime = InCoolTime;}
	void SetDashCoolTime(const float InCoolTime) {DashCoolTime = InCoolTime;}
	void SetBlockCoolTime(const float InCoolTime) {BlockCoolTime = InCoolTime;}

protected:
	void QReady();
	void EReady();
	void RReady();
	void BlockReady();
	void DashReady();

/*******************************************************************/
/*
 액션 스텟 (Action Stat)
 스킬, 공격에 붙어있는 고유 스텟, StatComponent에서 나온 스텟 결과물과 곱해진다.
 ex) 대검 Q는 초당 50%의 데미지를 준다 => QAdRatio = 0.5f,
	 초당 데미지 ->  QAdRatio * StatComp->GetFinalDamage(AD)
 */
	
	//캐릭터 공격 속도(임시), 서버에서 설정해야 함
	UPROPERTY(Replicated)
	float AttackSpeed = 1.f;

	//캐릭터 스킬 계수(임시)
	UPROPERTY()
	float QAdRatio = 1.f;

	UPROPERTY()
	float QApRatio = 1.f;

	UPROPERTY()
	float EAdRatio = 1.f;

	UPROPERTY()
	float EApRatio = 1.f;

	UPROPERTY()
	float RAdRatio = 1.f;
	
	UPROPERTY()
	float RApRatio = 1.f;

public:
	void SetQAdRatio(const float InRatio) {QAdRatio = InRatio;}
	void SetEAdRatio(const float InRatio) {EAdRatio = InRatio;}
	void SetRAdRatio(const float InRatio) {RAdRatio = InRatio;}
	void SetQApRatio(const float InRatio) {QApRatio = InRatio;}
	void SetEApRatio(const float InRatio) {EApRatio = InRatio;}
	void SetRApRatio(const float InRatio) {RApRatio = InRatio;}
	
	float GetQAdRatio() const { return QAdRatio; }
	float GetQApRatio() const { return QApRatio; }
	float GetEAdRatio() const { return EAdRatio; }
	float GetEApRatio() const { return EApRatio; }
	float GetRAdRatio() const { return RAdRatio; }
	float GetRApRatio() const { return RApRatio; }
	float GetAttackSpeed() const  {return AttackSpeed;}
	void SetAttackSpeed(float InAttackSpeed) {AttackSpeed = InAttackSpeed;}
/*
 임시 객체, 정보들
 CombatComponent 내에서 구현을 위해 잠시 저장된 객체나 정보들
 상황에 따라 유효하지 않을 수 있으며, 접근 시 유효성 검사를 꼭 해야한다.
 */
	//Enum
	UPROPERTY()
	ENoWeaponDash DashSide;
	
	UPROPERTY()
	UProjectileShooterComponent *ProjectileShooterComp;

	UPROPERTY()
	UParticleSystemComponent *StunComp;
	UPROPERTY()
	UParticleSystemComponent *ShockComp;

	//TimerHandler
	FTimerHandle ShockTimerHandle;
	FTimerHandle StealthHandle;
	FTimerHandle DarkMagicOrbHandle;
	
	FTimerHandle AttackComboHandle;
	FTimerHandle SkillComboHandle;

	FTimerHandle QCoolTimeHandle;
	FTimerHandle ECoolTimeHandle;
	FTimerHandle RCoolTimeHandle;
	FTimerHandle BlockCoolTimeHandle;
	FTimerHandle DashCoolTimeHandle;

	//Materials
	UPROPERTY()
	TArray<UMaterialInterface*> VisibleMaterials;
	UPROPERTY(EditAnywhere)
	TObjectPtr<UMaterialInterface> StealthMaterial;

	
	//Info
	FVector DecalLocation;
	
	//Crosshair, 값이 커지면 벌어지고, 값이 작아지면 줄어든다.
	float ProjectileSpread = 0.f;
	
	UFUNCTION(NetMulticast, Reliable)
	void MC_ShockParticle(bool bInShocked);
	
	UFUNCTION(Client, Reliable)
	void CL_TickUpdate(float IncreaseAmount);

	UFUNCTION(Client, Reliable)
	void CL_TickUpdateEnd();
};