# CityBuildingTimeSubSystem

> **UE5 WorldSubsystem** — 도시 건설 시뮬레이션을 위한 커스텀 타이머 매니저

![Unreal Engine 5](https://img.shields.io/badge/Unreal%20Engine-5-black?logo=unrealengine)
![C++](https://img.shields.io/badge/C++-blue?logo=cplusplus)

---

## Overview

도시 건설 시뮬레이션은 플레이어가 게임 속 시간 흐름 속도를 직접 제어(일시정지/1배속/4배속/32배속)해야 합니다.  
이 시스템은 `FTimerManager`와 동일한 인터페이스를 유지하면서, 독립적인 시뮬레이션 시간 축과 게임 내 달력 시스템을 함께 제공하는 UWorldSubsystem입니다.

<img width="689" height="466" alt="image" src="https://github.com/user-attachments/assets/103cb884-5acb-49de-9c9c-29f481cd3d89" />

### UE5 내장 Time Dilation을 사용하지 않은 이유

UE5에는 시간 흐름을 제어하는 두 가지 방법이 있지만, 둘 다 city-building 시뮬레이션에 적합하지 않습니다.

**① Global Time Dilation — `FTimerManager` 타이머에는 반영되지만, Dilation = 0 시 렌더링과 플레이어 입력까지 동결됩니다**  
Global TimeDilation은 World DeltaTime 자체를 스케일하므로 `FTimerManager` 타이머가 배속에 반응합니다.  
그러나 일시정지(Dilation = 0)를 하면 엔진 전체가 멈춰 카메라 조작, UI 입력, 렌더링 업데이트가 모두 불가능해집니다.  
City builder의 일시정지는 "시뮬레이션만 멈추고, 플레이어는 계속 지도를 둘러볼 수 있어야 한다"는 요구사항을 충족할 수 없습니다.

이 시스템은 `GameSpeed = 0`일 때 Tick 내부에서만 early return합니다. 엔진은 계속 동작하고 시뮬레이션 시간 축만 정지됩니다.

```cpp
void UCityBuildingTimeSubSystem::Tick(float DeltaTime)
{
    if (GameSpeed <= 0.0f) return;  // 시뮬레이션만 정지. 엔진/UI는 계속 동작.
    InternalTime += (double)DeltaTime * (double)GameSpeed;
    ...
}
```

**② Custom Time Dilation — 액터별 배속 적용은 가능하지만, 진행 중인 `FTimerManager` 타이머에는 반영되지 않습니다**  
`AActor::CustomTimeDilation`은 해당 액터의 Tick 속도를 개별 조정할 수 있습니다.  
그러나 `GetWorldTimerManager()`에 등록된 타이머는 World DeltaTime 기준으로 동작하므로, 특정 액터의 CustomTimeDilation 값을 변경해도 이미 등록된 타이머의 만료 시각에는 영향을 주지 않습니다.  
액터 자신의 배속과 그 액터가 등록한 타이머의 배속이 분리되어 일관성을 보장할 수 없습니다.

---

## Architecture
이 시스템은 `FTimerManager`의 내부 구조(자료구조 선택, Re-entrancy 처리, Frame-drop 보정)를
기본 골격으로 삼아 구현했습니다. 그 위에 독립적인 GameSpeed 시간 축과 달력 시스템을 추가했습니다.

<img width="1440" height="512" alt="image" src="https://github.com/user-attachments/assets/cf2d497f-de04-418b-a2af-c4d9ea305296" />

| 구조 | 역할 | 선택 근거 |
|---|---|---|
| `TArray` (Min-Heap) | 활성 타이머 우선순위 큐 | O(log N) 삽입, O(1) 만료 검사. 매 Tick마다 "가장 빨리 만료되는 타이머 하나"만 꺼내면 되므로 힙이 최적 |
| `TSparseArray` | 타이머 데이터 저장소 | 삽입/삭제 시 기존 원소의 메모리 주소가 유지됨. 힙이 들고 있는 Index Handle이 무효화되지 않음 |
| `TMap<const void*, TSet<Handle>>` | 오브젝트-타이머 역인덱스 | Actor 소멸 시 `ClearAllTimersForObject` 한 번으로 해당 오브젝트의 모든 타이머를 O(K) 정리 |

---

### Timer Lifecycle (상태 전이도)

```
        SetTimer()
            │
            ▼
    ┌───────────────┐     Tick 전에 등록됐으면
    │    Pending    │ ─────────────────────────────────┐
    └───────────────┘                                  │
            │ Tick 이후 PendingSet → Heap 이동          │
            ▼                                          ▼
    ┌───────────────┐   PauseTimer()   ┌───────────────┐
    │    Active     │ ───────────────► │    Paused     │
    └───────────────┘                  └───────────────┘
            │ 만료                           │ UnPauseTimer()
            ▼                               │
    ┌───────────────┐                        │
    │   Executing   │ ◄──────────────────────┘
    └───────────────┘
         │        │
    Loop │        │ ClearTimer() during callback
         │        ▼
         │  ┌─────────────────────┐
         │  │ ActivePendingRemoval │ → 다음 Tick에 안전 삭제
         │  └─────────────────────┘
         │
         ▼ (non-loop or loop ends)
       삭제
```

---


#### Handle 설계: Index + Serial Number
```
FGameTimeTimerHandle
├── Index        : TSparseArray 슬롯 위치
└── SerialNumber : 슬롯 재사용 시 stale handle 감지용 단조 증가 값
```

삭제 후 같은 슬롯에 새 타이머가 들어와도, SerialNumber가 달라서 이전 핸들로 잘못 접근하는 것을 방지합니다.

---
#### 1. Tick 도중 새 타이머 등록 → `PendingTimerSet`
콜백 실행 중 `SetTimer`를 호출하면 힙에 즉시 삽입할 수 없습니다 (HeapPop 진행 중이므로 힙 불변식 위반).  
등록 요청을 `PendingTimerSet`에 모아두고, Tick 루프 종료 후 일괄 힙에 삽입합니다.
```
HasBeenTickedThisFrame() == true  →  PendingTimerSet에 적재
HasBeenTickedThisFrame() == false →  ActiveTimerHeap에 즉시 삽입
```

#### 2. 콜백 실행 중 자기 자신을 Clear → `ActivePendingRemoval`
```cpp
// Tick 루프 내부에서의 흐름
TopData.Status = ECityTimerStatus::Executing;
TopData.TimerDelegate.Execute();              // ← 여기서 ClearTimer 호출 가능
// ClearTimer는 Status를 ActivePendingRemoval로만 변경하고 즉시 삭제하지 않음
// 콜백 종료 후 Status를 확인해서 정리
if (TopData.Status == ECityTimerStatus::ActivePendingRemoval) { ... }

```
#### 3. 바인딩된 UObject가 Tick 직전에 소멸 → `IsBound()` 체크

힙에서 꺼낸 직후, 콜백을 실행하기 전에 `IsBound()`로 바인딩 유효성을 확인합니다.  
UObject가 PendingKill 상태면 `IsBound()`가 false를 반환하므로, 무효 콜백 실행 없이 안전하게 폐기합니다.

#### 4. Frame-drop Catch-up (루핑 타이머 보정)
프레임이 크게 드랍되면 루핑 타이머의 만료 시각이 한 번의 Tick 안에 여러 번 지나쳐버릴 수 있습니다.  
`InternalTime - ExpireTime`을 `Rate`로 나눠 실제로 호출됐어야 할 횟수(`CallCount`)를 계산하고 일괄 실행합니다.
```cpp
int32 CallCount = FMath::TruncToInt((InternalTime - TopData.ExpireTime) / Rate) + 1;
for (int32 i = 0; i < CallCount; ++i) { Delegate.Execute(); }
```

## Features

### 타이머 API — 3가지 바인딩 방식

```cpp
// 1. C++ 멤버 함수
SubSystem->SetTimer(Handle, this, &AMyActor::OnHarvestComplete, 5.0f, false);

// 2. Lambda / TFunction
SubSystem->SetTimer([this]() { SpawnWorker(); }, 10.0f, true);

// 3. Blueprint Dynamic Delegate (Blueprint에서 직접 바인딩 가능)
SubSystem->SetTimer(MyDelegate, 3.0f, false);
```

`FCityTimeUnifiedDelegate`가 세 방식을 단일 인터페이스로 통합합니다.  
C++ 경로는 `TFunction`으로, Blueprint 경로는 `FOnTimeSystemTimerExpired`(Dynamic Delegate)로 처리합니다.

### Game Speed 제어

```cpp
SubSystem->SetGameSpeed(0.f);  // 일시정지
SubSystem->SetGameSpeed(1.f);  // 1배속
SubSystem->SetGameSpeed(3.f);  // 3배속

// 변경 시 브로드캐스트
SubSystem->GameSpeedChanged.AddDynamic(this, &UMyWidget::OnSpeedChanged);
```

`InternalTime`은 `DeltaTime * GameSpeed`로만 누적되므로, 엔진 TimeDilation과 완전히 독립적입니다.

### 달력 기반 API

```cpp
// 현재 인게임 날짜 조회
FCityCalendarDate Today = SubSystem->GetCurrentDate();
// → Year:3, Month:2, Day:15, Hour:08, Minute:30

// 특정 날짜에 이벤트 등록
FCityCalendarDate TaxDay(3, 3, 1, 0, 0);  // 3년 3월 1일 자정
SubSystem->SetTimerAtDate(Handle, this, &ACityManager::CollectTax, TaxDay);

// 매 인게임 분마다 UI 업데이트
SubSystem->OnMinuteChanged.AddDynamic(this, &UHUDWidget::RefreshClock);
```

`TimeRatio`가 현실 1초를 게임 내 몇 초로 환산할지를 결정합니다 (기본값 20.0f).

### Save / Load

```cpp
// 세이브 파일에서 날짜 복원
FCityCalendarDate SavedDate = LoadFromSaveFile();
SubSystem->InitializeFromSavedDate(SavedDate);
// → InternalTime 재계산 + OnMinuteChanged 즉시 브로드캐스트
```

### 오브젝트 단위 타이머 일괄 정리

```cpp
// Actor 소멸 시 해당 오브젝트에 바인딩된 모든 타이머 제거
SubSystem->ClearAllTimersForObject(this);
```

`ObjectToTimerHandles` 역인덱스를 통해 O(K)로 처리합니다 (K = 해당 오브젝트의 타이머 수).

---

## File Structure

```
CityBuildingTimeSubSystem/
├── CityBuildingTimerTypes.h       타입 정의 (Handle, TimerData, CalendarDate, Delegate, HeapOrder)
├── CityBuildingTimeSubSystem.h    서브시스템 선언 및 공개 API
└── CityBuildingTimeSubSystem.cpp  Tick 루프, 타이머 등록/삭제/일시정지 구현
```

---

## Performance Characteristics

| 연산 | 시간 복잡도 |
|---|---|
| 타이머 등록 (`SetTimer`) | O(log N) |
| 타이머 만료 처리 (매 Tick) | O(log N) |
| 타이머 데이터 조회 (`FindTimerData`) | O(1) |
| 오브젝트 단위 전체 삭제 | O(K log N) |
| 힙 초기화 메모리 예약 | 1024 슬롯 pre-reserve |
