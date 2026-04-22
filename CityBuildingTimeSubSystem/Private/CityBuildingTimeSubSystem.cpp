// Copyright © Grukall 2026.The Otium™ is a trademark of Grukall. All Rights Reserved.


#include "CityBuildingTimeSubSystem.h"


void UCityBuildingTimeSubSystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    
    SetGameSpeed(0.f);
}

void UCityBuildingTimeSubSystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    ActiveTimerHeap.Reserve(1024);
}

void UCityBuildingTimeSubSystem::Deinitialize()
{
    ActiveTimerHeap.Empty();
    PausedTimers.Empty();
    PendingTimerSet.Empty();
    Timers.Empty();
    ObjectToTimerHandles.Empty();

    Super::Deinitialize();
}

void UCityBuildingTimeSubSystem::Tick(float DeltaTime)
{
    if (GameSpeed <= 0.0f) return;

    LastTickedFrame = GFrameCounter;
    
    InternalTime += (double)DeltaTime * (double)GameSpeed;
    
    while (ActiveTimerHeap.Num() > 0)
    {
        FGameTimeTimerHandle TopHandle = ActiveTimerHeap.HeapTop();
        FTimeSystemTimerData& TopData = Timers[TopHandle.Index];

        if (InternalTime < TopData.ExpireTime) break;

        ActiveTimerHeap.HeapPop(TopHandle, FCityTimerHeapOrder(Timers), EAllowShrinking::No);

        if (TopData.Status == ECityTimerStatus::ActivePendingRemoval)
        {
            CleanupHandle(TopHandle, TopData.ObjectKey);
            Timers.RemoveAt(TopHandle.Index);
            continue;
        }
        
        // ★ [수정] 객체가 유효한지 먼저 체크합니다. ★
        // IsBound()는 바인딩된 UObject가 PendingKill 상태이거나 null이면 false를 반환합니다.
        if (!TopData.TimerDelegate.IsBound())
        {
            // 객체가 죽었으므로 타이머를 즉시 폐기합니다.
            CleanupHandle(TopHandle, TopData.ObjectKey);
            Timers.RemoveAt(TopHandle.Index);
            continue; 
        }
        
        int32 CallCount = TopData.bLoop ? FMath::TruncToInt((InternalTime - TopData.ExpireTime) / (double)TopData.Rate) + 1 : 1;
        TopData.Status = ECityTimerStatus::Executing;

        for (int32 i = 0; i < CallCount; ++i)
        {
            TopData.TimerDelegate.Execute();
            if (TopData.Status == ECityTimerStatus::ActivePendingRemoval) break;
        }

        if (TopData.bLoop && TopData.Status != ECityTimerStatus::ActivePendingRemoval)
        {
            TopData.ExpireTime += (double)CallCount * (double)TopData.Rate;
            TopData.Status = ECityTimerStatus::Active;
            ActiveTimerHeap.HeapPush(TopHandle, FCityTimerHeapOrder(Timers));
        }
        else
        {
            CleanupHandle(TopHandle, TopData.ObjectKey);
            Timers.RemoveAt(TopHandle.Index);
        }
    }
    
    if (PendingTimerSet.Num() > 0)
    {
        for (FGameTimeTimerHandle Handle : PendingTimerSet)
        {
            if (Timers.IsAllocated(Handle.Index))
            {
                FTimeSystemTimerData& Data = Timers[Handle.Index];
                Data.ExpireTime += InternalTime;
                Data.Status = ECityTimerStatus::Active;
                ActiveTimerHeap.HeapPush(Handle, FCityTimerHeapOrder(Timers));
            }
        }
        PendingTimerSet.Reset();
    }
    
    UpdateGameClock();
}

FCityCalendarDate UCityBuildingTimeSubSystem::GetInGameDate(float TimeScale) const
{
    FCityCalendarDate Date;
    
    int64 TotalGameSeconds = FMath::FloorToInt(InternalTime * (double)TimeScale);
    
    // 년, 월, 일, 시, 분 계산
    Date.Year   = (TotalGameSeconds / SecondsInYear) + 1;
    Date.Month  = ((TotalGameSeconds % SecondsInYear) / SecondsInMonth) + 1;
    Date.Day    = ((TotalGameSeconds % SecondsInMonth) / SecondsInDay) + 1;
    Date.Hour   = (TotalGameSeconds % SecondsInDay) / SecondsInHour;
    Date.Minute = (TotalGameSeconds % SecondsInHour) / SecondsInMinute;

    return Date;
}

void UCityBuildingTimeSubSystem::UpdateGameClock()
{
    int64 CurrentTotalMinutes = FMath::FloorToInt(InternalTime * (TimeRatio / 60.0f));

    // 게임 내 '분' 단위가 실제로 바뀌었을 때만 델리게이트 호출
    if (CurrentTotalMinutes > LastTotalMinutes)
    {
        LastTotalMinutes = CurrentTotalMinutes;
        
        // 달력 정보 계산 후 브로드캐스트
        FCityCalendarDate CurrentDate = GetInGameDate(TimeRatio);
        OnMinuteChanged.Broadcast(CurrentDate);
    }
}

FGameTimeTimerHandle UCityBuildingTimeSubSystem::InternalSetTimer(FGameTimeTimerHandle& InOutHandle, FCityTimeUnifiedDelegate&& InDelegate, float InRate, bool bInLoop, float InFirstDelay)
{
    if (InOutHandle.IsValid())
    {
        ClearTimer(InOutHandle);
    }

    if (InRate <= 0.f && InFirstDelay <= 0.f)
    {
        InOutHandle.Invalidate();
        return InOutHandle;
    }

    // 2. 타이머 데이터 생성
    FTimeSystemTimerData NewTimer;
    NewTimer.TimerDelegate = MoveTemp(InDelegate);
    NewTimer.Rate = InRate;
    NewTimer.bLoop = bInLoop;

    const float FirstDelay = (InFirstDelay >= 0.f) ? InFirstDelay : InRate;
    
    int32 NewIndex = Timers.Add(MoveTemp(NewTimer));
    
    FGameTimeTimerHandle NewHandle;
    NewHandle.Index = NewIndex;
    NewHandle.SerialNumber = ++LastSerialNumber;
    
    FTimeSystemTimerData& Data = Timers[NewIndex];
    Data.Handle = NewHandle;
    
    if (const void* Key = Data.TimerDelegate.GetBoundObject())
    {
        Data.ObjectKey = Key;
        ObjectToTimerHandles.FindOrAdd(Key).Add(NewHandle);
    }
    
    if (HasBeenTickedThisFrame())
    {
        Data.ExpireTime = InternalTime + (double)FirstDelay;
        Data.Status = ECityTimerStatus::Active;
        ActiveTimerHeap.HeapPush(NewHandle, FCityTimerHeapOrder(Timers));
    }
    else
    {
        Data.ExpireTime = (double)FirstDelay;
        Data.Status = ECityTimerStatus::Pending;
        PendingTimerSet.Add(NewHandle);
    }
    
    InOutHandle = NewHandle;
    return NewHandle;
}

FTimeSystemTimerData* UCityBuildingTimeSubSystem::FindTimerData(FGameTimeTimerHandle InHandle)
{
    if (InHandle.IsValid() && Timers.IsAllocated(InHandle.Index))
    {
        FTimeSystemTimerData& Data = Timers[InHandle.Index];
        if (Data.Handle.SerialNumber == InHandle.SerialNumber)
        {
            return &Data;
        }
    }
    return nullptr;
}

void UCityBuildingTimeSubSystem::UnPauseTimer(FGameTimeTimerHandle InHandle)
{
    if (FTimeSystemTimerData* Data = FindTimerData(InHandle))
    {
        if (Data->Status == ECityTimerStatus::Paused)
        {
            PausedTimers.Remove(InHandle);
            
            Data->ExpireTime += InternalTime;
            Data->Status = ECityTimerStatus::Active;

            /** * If resumed during a Tick (e.g., from another timer's callback),
             * move to PendingSet to keep the ActiveHeap stable.
             */
            if (HasBeenTickedThisFrame())
            {
                PendingTimerSet.Add(InHandle);
            }
            else
            {
                ActiveTimerHeap.HeapPush(InHandle, FCityTimerHeapOrder(Timers));
            }
        }
    }
}

void UCityBuildingTimeSubSystem::SetGameSpeed(float NewSpeed)
{
    GameSpeed = FMath::Max(0.0f, NewSpeed);
    GameSpeedChanged.Broadcast(GameSpeed);
}

void UCityBuildingTimeSubSystem::ClearAllTimersForObject(const UObject* Object)
{
    if (!Object) return;
    
    TSet<FGameTimeTimerHandle>* TargetHandles = ObjectToTimerHandles.Find(Object);
    if (!TargetHandles) return;
    
    for (FGameTimeTimerHandle Handle : *TargetHandles)
    {
        if (FTimeSystemTimerData* Data = FindTimerData(Handle))
        {
            if (Data->Status == ECityTimerStatus::Paused)
            {
                PausedTimers.Remove(Handle);
                Timers.RemoveAt(Handle.Index);
            }
            else
            {
                Data->Status = ECityTimerStatus::ActivePendingRemoval;
            }
        }
    }
    
    ObjectToTimerHandles.Remove(Object);
}

void UCityBuildingTimeSubSystem::CleanupHandle(const FGameTimeTimerHandle& Handle, const void* ObjectKey)
{
    if (ObjectKey)
    {
        if (TSet<FGameTimeTimerHandle>* HandleSet = ObjectToTimerHandles.Find(ObjectKey))
        {
            // Remove the specific serial number from the object's set
            HandleSet->Remove(Handle);

            // If the object has no more timers, remove the key to save memory
            if (HandleSet->Num() == 0)
            {
                ObjectToTimerHandles.Remove(ObjectKey);
            }
        }
    }
}

FGameTimeTimerHandle UCityBuildingTimeSubSystem::SetTimerForNextTick(TFunction<void()>&& Callback)
{
    FGameTimeTimerHandle DummyHandle;
    return InternalSetTimer(DummyHandle, FCityTimeUnifiedDelegate(MoveTemp(Callback)), 0.0f, false, 0.0f);
}

void UCityBuildingTimeSubSystem::ClearTimer(FGameTimeTimerHandle& InHandle)
{
    if (FTimeSystemTimerData* Data = FindTimerData(InHandle))
    {
        CleanupHandle(InHandle, Data->ObjectKey);
        
        if (Data->Status == ECityTimerStatus::Paused)
        {
            PausedTimers.Remove(InHandle);
            Timers.RemoveAt(InHandle.Index);
        }
        else if (Data->Status == ECityTimerStatus::Pending)
        {
            PendingTimerSet.Remove(InHandle);
            Timers.RemoveAt(InHandle.Index);
        }
        else
        {
            Data->Status = ECityTimerStatus::ActivePendingRemoval;
        }
    }
    InHandle.Invalidate();
}

void UCityBuildingTimeSubSystem::PauseTimer(FGameTimeTimerHandle InHandle)
{
    if (FTimeSystemTimerData* Data = FindTimerData(InHandle))
    {
        if (Data->Status == ECityTimerStatus::Active)
        {
            int32 HeapIndex = ActiveTimerHeap.Find(InHandle);
            if (HeapIndex != INDEX_NONE)
            {
                ActiveTimerHeap.HeapRemoveAt(HeapIndex, FCityTimerHeapOrder(Timers), EAllowShrinking::No);
                Data->ExpireTime -= InternalTime;
                Data->Status = ECityTimerStatus::Paused;
                PausedTimers.Add(InHandle);
            }
        }
    }
}

/** 특정 게임 날짜까지 남은 현실 지연 시간(초)을 계산하는 유틸리티 */
float UCityBuildingTimeSubSystem::CalculateRealDelayToDate(const FCityCalendarDate& TargetDate) const
{
    // 1. 목표 날짜를 총 게임 초(Game Seconds)로 변환
    int64 TargetGameSeconds = 0;
    TargetGameSeconds += (int64)(TargetDate.Year - 1) * SecondsInYear;
    TargetGameSeconds += (int64)(TargetDate.Month - 1) * SecondsInMonth;
    TargetGameSeconds += (int64)(TargetDate.Day - 1) * SecondsInDay;
    TargetGameSeconds += (int64)TargetDate.Hour * SecondsInHour;
    TargetGameSeconds += (int64)TargetDate.Minute * SecondsInMinute;

    // 2. 현재 누적된 총 게임 초 계산
    double CurrentGameSeconds = InternalTime * (double)TimeRatio;

    // 3. 게임 초 단위의 차이 계산
    double GameSecondsDifference = (double)TargetGameSeconds - CurrentGameSeconds;

    // 4. 현실 지연 시간으로 역산 (Difference / Ratio)
    float RealDelay = (float)(GameSecondsDifference / (double)TimeRatio);

    // 이미 지난 시간이라면 즉시 실행(0.0)되도록 보정
    return FMath::Max(0.0f, RealDelay);
}

float UCityBuildingTimeSubSystem::CalculateRealTime(const FCityCalendarDate& Date) const
{
    int64 TargetGameSeconds = 0;
    TargetGameSeconds += (int64)(Date.Year) * SecondsInYear;
    TargetGameSeconds += (int64)(Date.Month) * SecondsInMonth;
    TargetGameSeconds += (int64)(Date.Day) * SecondsInDay;
    TargetGameSeconds += (int64)Date.Hour * SecondsInHour;
    TargetGameSeconds += (int64)Date.Minute * SecondsInMinute;
    
    float RealDelay = (float)(TargetGameSeconds / (double)TimeRatio);
    
    return RealDelay;
}

double UCityBuildingTimeSubSystem::GetCurrentTime() const
{
    return (double)InternalTime * TimeRatio;
}

FCityCalendarDate UCityBuildingTimeSubSystem::GetCurrentDate() const
{
    FCityCalendarDate Date;

    // 1. 현실의 누적 초(InternalTime)를 게임 내 총 흐른 초로 변환
    // 공식: TotalGameSeconds = InternalTime * TimeRatio
    const double TotalGameSeconds = InternalTime * (double)TimeRatio;
    
    // 3. 정수형 초 단위로 변환 (소수점 버림)
    const int64 TotalSecondsInt = FMath::FloorToInt64(TotalGameSeconds);

    // 4. 각 달력 항목 역산
    // 년/월/일은 1부터 시작하므로 마지막에 +1을 해줍니다.
    Date.Year   = (TotalSecondsInt / SecondsInYear) + 1;
    Date.Month  = ((TotalSecondsInt % SecondsInYear) / SecondsInMonth) + 1;
    Date.Day    = ((TotalSecondsInt % SecondsInMonth) / SecondsInDay) + 1;
    Date.Hour   = (TotalSecondsInt % SecondsInDay) / SecondsInHour;
    Date.Minute = (TotalSecondsInt % SecondsInHour) / SecondsInMinute;

    return Date;
}


void UCityBuildingTimeSubSystem::InitializeFromSavedDate(const FCityCalendarDate& SavedDate)
{
    // 1. 모든 단위를 게임 내 '초' 단위로 환산
    int64 TotalGameSeconds = 0;
    TotalGameSeconds += (int64)(SavedDate.Year - 1) * SecondsInYear;
    TotalGameSeconds += (int64)(SavedDate.Month - 1) * SecondsInMonth;
    TotalGameSeconds += (int64)(SavedDate.Day - 1) * SecondsInDay;
    TotalGameSeconds += (int64)SavedDate.Hour * SecondsInHour;
    TotalGameSeconds += (int64)SavedDate.Minute * SecondsInMinute;

    // 2. 현실 시간(InternalTime)으로 역산하여 저장
    InternalTime = (double)TotalGameSeconds / (double)TimeRatio;

    // 3. 중복 갱신 방지를 위해 마지막 분(Minute) 기록 업데이트
    LastTotalMinutes = TotalGameSeconds / 60;

    // 4. 로드 즉시 UI와 다른 시스템들이 바뀐 시간을 알 수 있도록 브로드캐스트
    OnMinuteChanged.Broadcast(SavedDate);
}
