# Skill System

> 무기 장착·행동 충돌·스킬 실행을 관리하는 서버 권위적 전투 컴포넌트

![Unreal Engine 5](https://img.shields.io/badge/Unreal%20Engine-5-black?logo=unrealengine)
![C++](https://img.shields.io/badge/C++-blue?logo=cplusplus)

---

## UCombatComponent

캐릭터에 부착되는 `UActorComponent`로, 전투와 관련된 모든 처리를 담당합니다.

| 역할 | 내용 |
|---|---|
| **스킬 관리** | 무기 장착 시 Q·E·R·Attack 슬롯에 스킬 할당, 무기 교체 시 교체 |
| **행동 시스템** | 입력을 ActionQueue에 적재하고, 우선순위 판정 후 스킬 실행·중단 |
| **쿨타임·상태** | 스킬별 쿨타임 타이머 관리, 행동 상태 플래그(`bIsAttacking` 등) 유지 |
| **데미지 적용** | 히트 감지 후 대상 타입에 따라 피해 계산 및 적용 |
| **투사체 발사** | 크로스헤어 기준 발사 위치·방향 계산 |

모든 판정은 서버에서만 수행되며, 클라이언트는 Server RPC로 요청만 보냅니다.

---

## 무기 장착에 따른 스킬 교체

캐릭터가 무기를 장착하면 `SetSkillBase()`와 `SpawnCombatHelper()`가 호출됩니다.

```cpp
// 무기 타입에 맞는 Helper 생성
UCombatHelperBase* CombatHelper = SpawnCombatHelper(MainWeaponType, SubWeaponType);

// Q·E·R·Attack 슬롯에 스킬 할당
SetSkillBase(SkillData, SkillQAction, CombatHelper);
```

`SkillBases`는 `ENewActionType` 크기의 배열로, 슬롯 인덱스로 스킬에 O(1) 접근합니다.  
무기 교체 시 이전 스킬 오브젝트는 `MarkAsGarbage()`로 폐기하고 새 오브젝트를 `NewObject`로 생성해 교체합니다.

스킬 계층 구조(SkillBase → SkillQBase → 무기별 구체 클래스)와 CombatHelper 분리 설계는 [SkillBase/README.md](SkillBase/README.md)에서 상세 설명합니다.

---

## 입력에 따른 스킬 실행 과정
클라이언트 입력은 즉시 실행되지 않고 서버의 TQueue에 적재된 뒤 순서대로 처리됩니다.  
같은 프레임에 여러 입력이 몰려도 처리 순서를 보장하고, 액션 처리 중 새 입력이 판정 로직에 끼어드는 재진입 문제를 방지하기 위해서입니다.
<img width="1440" height="1240" alt="image" src="https://github.com/user-attachments/assets/9365682f-8a9b-4bdb-84de-d5a7fed25df3" />


---

## 스킬 판단 — ActionLevel / CancelLevel 이중 우선순위

행동 충돌은 단순한 우선순위 하나로 해결되지 않습니다. 막기(Block)는 스킬과 동등하게 시작할 수 있어야 하지만 막는 도중엔 웬만한 행동으로 끊기면 안 됩니다. 스턴은 어떤 행동이든 즉시 중단시켜야 합니다.

이 시스템은 **실행 우선순위(ActionLevel)** 와 **취소 우선순위(CancelLevel)** 를 분리해 각 행동의 시작 조건과 중단 조건을 독립적으로 설정합니다. 숫자가 낮을수록 우선순위가 높습니다.

```cpp
struct FNewAction
{
    ENewActionType ActionType;
    int32 ActionLevel;   // 실행 우선순위: 현재 행동의 CancelLevel 이하여야 실행 가능
    int32 CancelLevel;   // 취소 우선순위: 다음 행동의 ActionLevel이 이 이하여야 취소됨
    float LifeTime;      // 양수면 해당 시간 후 자동 End. -1이면 무제한
    FName ActionName;
    bool  bIsCombo;      // 콤보 입력 시 우선순위 판정 우회
};
```

| Action | ActionLevel | CancelLevel | 비고 |
|---|---|---|---|
| Dead | 0 | 0 | 어떤 행동도 불가 |
| Stun / KnockBack | 1 | 1 | 거의 모든 행동을 취소 가능 |
| Dash / Skill | 3 | 2 | 시작은 쉽고, 끊기도 쉬움 |
| Attack | 4 | 3 | |
| **Block** | **3** | **5** | 시작은 쉽지만, 막는 중엔 잘 안 끊김 |
| Default | 100 | 100 | 항상 취소 가능 |

Block은 ActionLevel=3이라 스킬·대시와 동등하게 시작할 수 있지만, CancelLevel=5라 막기 도중엔 ActionLevel 5 이하인 행동만 끊을 수 있습니다.
스턴(ActionLevel=1)은 막기를 중단시키지만, 다른 스킬(ActionLevel=3)은 막기를 끊지 못합니다.

판정은 한 줄로 완성됩니다.

```cpp
bool CanPlayNewAction(const int32 ActionLevel) const
{
    return ActionLevel <= CurNewAction.CancelLevel;
}
```

---

## 스킬 중단

스킬이 중단되는 경로는 세 가지입니다.

**① 우선순위가 높은 행동 진입 — `CancelByActionSystem()`**

`NewTryPlayAction_Internal`에서 새 행동의 우선순위 판정이 통과하면, 현재 진행 중인 스킬의 `CancelByActionSystem()`을 호출합니다. 스킬이 내부적으로 `Cancel()`을 호출해 몽타주 중지·상태 초기화를 처리합니다.

```cpp
// 이전 행동 취소 후 새 행동 실행
SkillBases[CurNewAction.ActionType]->CancelByActionSystem();
GetWorld()->GetTimerManager().ClearTimer(ActionHandle);
CurNewAction = Action;
SkillBase->Execute();
```

**② LifeTime 만료 — 자동 `End()`**

`FNewAction::LifeTime`이 양수이면 실행 시 타이머를 설정하고, 만료 시 `End()`를 자동 호출합니다.

```cpp
if (Action.LifeTime > 0)
{
    GetWorld()->GetTimerManager().SetTimer(ActionHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        SkillBases[CurNewAction.ActionType]->End();
    }), CurNewAction.LifeTime, false);
}
```

**③ 몽타주 종료 — `End()` / `Cancel()`**

애니메이션 노티파이가 `CombatComponent::End()` 또는 `Cancel()`을 호출하면, `LastPlayedMontage`를 기준으로 해당 스킬을 찾아 처리합니다.

```cpp
void UCombatComponent::End(const UAnimMontage* EndMontage)
{
    for (USkillBase* SkillBase : SkillBases)
    {
        if (IsValid(SkillBase) && SkillBase->LastPlayedMontage == EndMontage)
            SkillBase->End();
    }
}
```

---

## File Structure

```
CombatComponent/
├── CombatComponent.h     FNewAction 정의, 스킬·쿨타임·상태 관리 선언
├── CombatComponent.cpp   NewTryPlayAction_Internal, 액션 큐, 스킬 교체, 데미지 적용
└── SkillBase/            스킬 계층 구조 (SkillBase/README.md 참조)
```
