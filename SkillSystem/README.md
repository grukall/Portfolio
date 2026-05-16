# CombatComponent

> 멀티플레이어 액션 게임의 행동 충돌을 **ActionLevel / CancelLevel 이중 우선순위**로 해결하는 서버 권위적 전투 시스템

![Unreal Engine 5](https://img.shields.io/badge/Unreal%20Engine-5-black?logo=unrealengine)
![C++](https://img.shields.io/badge/C++-blue?logo=cplusplus)
---

## Overview

멀티플레이어 액션 게임에서 행동 충돌은 단순한 우선순위 하나로 해결되지 않습니다.

예를 들어 막기(Block)는 스킬과 동등하게 시작할 수 있어야 하지만, 막는 도중에는 웬만한 행동으로 끊기면 안 됩니다. 반대로 스턴(Stun)은 어떤 행동이든 즉시 중단시켜야 합니다. 이처럼 **시작 조건**과 **중단 조건**이 행동마다 다릅니다.

이 시스템은 실행 우선순위(`ActionLevel`)와 취소 우선순위(`CancelLevel`)를 분리하여 각 행동의 시작과 중단 조건을 독립적으로 설정합니다. 모든 판정은 서버에서만 수행되며, 클라이언트는 RPC로 요청만 보냅니다.

---

## Architecture

### ActionLevel / CancelLevel 이중 우선순위

`FNewAction`은 모든 행동의 단위입니다. 숫자가 낮을수록 우선순위가 높습니다.

```cpp
struct FNewAction
{
    ENewActionType ActionType;
    int32 ActionLevel;   // 실행 우선순위: 현재 행동의 CancelLevel 이하여야 실행 가능
    int32 CancelLevel;   // 취소 우선순위: 다음 행동의 ActionLevel이 이 이하여야 취소됨
    float LifeTime;      // 양수면 해당 시간 후 자동 End. -1이면 무제한
    FName ActionName;
    bool  bIsCombo;      // 콤보 입력 여부 (우선순위 판정 우회)
};
```

| Action | ActionLevel | CancelLevel | 비고 |
|---|---|---|---|
| Dead | 0 | 0 | 어떤 행동도 불가 |
| Stun / KnockBack | 1 | 1 | 거의 모든 행동 취소 가능 |
| Dash / Skill | 3 | 2 | 시작은 쉽고, 끊기도 쉬움 |
| Attack | 4 | 3 | |
| **Block** | **3** | **5** | 시작은 쉽지만, 막는 중엔 잘 안 끊김 |
| Default | 100 | 100 | 항상 취소 가능 |

Block이 설계의 핵심입니다. ActionLevel=3이라 스킬·대시와 동등하게 시작할 수 있지만, CancelLevel=5라 막기 도중에는 ActionLevel이 5 이하인 행동만 끊을 수 있습니다. 스턴(ActionLevel=1)은 막기를 중단시키지만 다른 스킬(ActionLevel=3)은 막기를 끊지 못합니다.

---

### 판정 로직 — `NewTryPlayAction_Internal`

```cpp
bool UCombatComponent::NewTryPlayAction_Internal(FNewAction& Action)
{
    // 1. 서버 권위 검사
    if (!GetOwner()->HasAuthority()) return false;

    // 2. 해당 ActionType의 SkillBase 유효성 검사
    USkillBase* SkillBase = SkillBases[Action.ActionType];
    if (!IsValid(SkillBase)) return false;

    // 3. 스킬 자체 실행 가능 여부 (쿨타임, 상태 등 스킬별 조건)
    if (!SkillBase->CanExecute(Action)) return false;

    // 4. ActionLevel 범위 클램핑 [0, ActionMax]
    Action.ActionLevel = FMath::Clamp(Action.ActionLevel, 0, ActionMax);

    // 5. 우선순위 판정: 새 행동의 ActionLevel <= 현재 행동의 CancelLevel
    if (!Action.bIsCombo && !CanPlayNewAction(Action.ActionLevel)) return false;

    // 6. 이전 행동 취소
    if (CurNewAction.ActionType != DefaultAction && !Action.bIsCombo)
    {
        SkillBases[CurNewAction.ActionType]->CancelByActionSystem();
        GetWorld()->GetTimerManager().ClearTimer(ActionHandle);
    }

    // 7. 새 행동 실행
    CurNewAction = Action;
    CurActionType = Action.ActionType;
    SkillBase->Execute();

    // 8. LifeTime 타이머 설정
    if (Action.LifeTime > 0)
        GetWorld()->GetTimerManager().SetTimer(ActionHandle, ...End()..., Action.LifeTime, false);

    return true;
}
```

핵심 판정은 한 줄입니다.

```cpp
bool CanPlayNewAction(const int32 ActionLevel) const
{
    return ActionLevel <= CurNewAction.CancelLevel;
}
```

### 액션 큐

클라이언트는 `Server_NewTryAction` RPC로 요청을 보내고, 서버는 `TQueue<FNewAction>`에 적재합니다. `TickComponent`에서 매 프레임 하나씩 꺼내 `NewTryPlayAction_Internal`을 호출합니다. 동일한 액션이 연속으로 들어오면 큐 적재 시점에 중복을 걸러냅니다.

```
[클라이언트 입력]
    │
    ▼ Server RPC
[ActionQueue (TQueue)]
    │ TickComponent에서 Dequeue
    ▼
[NewTryPlayAction_Internal]
    ├─ CanExecute() 실패 → 거부
    ├─ ActionLevel > CancelLevel → 거부
    └─ 통과 → 이전 액션 CancelByActionSystem() → Execute()
```

---

## File Structure

```
CombatComponent/
├── CombatComponent.h        FNewAction 정의, 액션 우선순위 판정, 쿨타임·상태 관리
├── CombatComponent.cpp      NewTryPlayAction_Internal, 액션 큐 처리, 데미지 적용
└── SkillBase/               스킬 계층 구조 (SkillBase/README.md 참조)
```
