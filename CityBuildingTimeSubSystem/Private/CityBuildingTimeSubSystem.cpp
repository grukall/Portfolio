// Copyright © Grukall 2026.The Otium™ is a trademark of Grukall. All Rights Reserved.


#include "CityBuildingTimeSubSystem.h"


void UCityBuildingTimeSubSystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);

    //퍼즈 상태로 시작
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

    //배속 반영된 만큼 DeltaTIme 더하기
    InternalTime += (double)DeltaTime * (double)GameSpeed;
    
    while (ActiveTimerHeap.Num() > 0)
    {
        FGameTimeTimerHandle TopHandle = ActiveTimerHeap.HeapTop();
        FTimeSystemTimerData& TopData = Timers[TopHandle.Index];

        //만료시간이 가장 가까운 타이머를 검사, 아직 시간이 안되었다면 다음 프레임으로 넘어간다.
        if (InternalTime < TopData.ExpireTime) break;

        ActiveTimerHeap.HeapPop(TopHandle, FCityTimerHeapOrder(Timers), EAllowShrinking::No);

        //타이머가 삭제 대기 중이라면, 삭제 후 다음 타이머로 넘어간다.
        if (TopData.Status == ECityTimerStatus::ActivePendingRemoval)
        {
            CleanupHandle(TopHandle, TopData.ObjectKey);
            Timers.RemoveAt(TopHandle.Index);
            continue;
        }
        
        // 객체가 유효한지 먼저 체크
        if (!TopData.TimerDelegate.IsBound())
        {
            // 객체가 죽었으므로 타이머를 즉시 폐기
            CleanupHandle(TopHandle, TopData.ObjectKey);
            Timers.RemoveAt(TopHandle.Index);
            continue; 
        }

        //loop 타이머 중, 두 번 이상 불려야 하는지 계산
        int32 CallCount = TopData.bLoop ? FMath::TruncToInt((InternalTime - TopData.ExpireTime) / (double)TopData.Rate) + 1 : 1;
        TopData.Status = ECityTimerStatus::Executing;

        //불러야 하는 횟수만큼 호출
        for (int32 i = 0; i < CallCount; ++i)
        {
            TopData.TimerDelegate.Execute();
            if (TopData.Status == ECityTimerStatus::ActivePendingRemoval) break;
        }

        //루프 타이머는 다시 힙에 넣는다.
        if (TopData.bLoop && TopData.Status != ECityTimerStatus::ActivePendingRemoval)
        {
            TopData.ExpireTime += (double)CallCount * (double)TopData.Rate;
            TopData.Status = ECityTimerStatus::Active;
            ActiveTimerHeap.HeapPush(TopHandle, FCityTimerHeapOrder(Timers));
        }

        //그렇지 않다면 제거
        else
        {
            CleanupHandle(TopHandle, TopData.ObjectKey);
            Timers.RemoveAt(TopHandle.Index);
        }
    }

    //이번 Tick에 타이머를 처리할 동안, 새롭게 생성되어 대기 중인 타이머들을 Heap에 넣는다.
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

    //0초 타이머는 허용하지 않는다.
    if (InRate <= 0.f && InFirstDelay <= 0.f)
    {
        InOutHandle.Invalidate();
        return InOutHandle;
    }

    //1. 타이머 데이터 생성
    //타이머 시스템 내에서 필요한 데이터를 들고 있다.
    FTimeSystemTimerData NewTimer;
    NewTimer.TimerDelegate = MoveTemp(InDelegate);
    NewTimer.Rate = InRate;
    NewTimer.bLoop = bInLoop;

    const float FirstDelay = (InFirstDelay >= 0.f) ? InFirstDelay : InRate;

    //SparsArray에 타이머 데이터 저장
    int32 NewIndex = Timers.Add(MoveTemp(NewTimer));

    //2. 타이머 핸들러 생성
    //타이머 호출자가 접근할 핸들러이다.
    FGameTimeTimerHandle NewHandle;
    NewHandle.Index = NewIndex;
    NewHandle.SerialNumber = ++LastSerialNumber;
    
    FTimeSystemTimerData& Data = Timers[NewIndex];
    Data.Handle = NewHandle;

    //객체를 TMap에 저장, 객체가 유효하지 않으면 일괄 해제를 위함
    if (const void* Key = Data.TimerDelegate.GetBoundObject())
    {
        Data.ObjectKey = Key;
        ObjectToTimerHandles.FindOrAdd(Key).Add(NewHandle);
    }

    //Tick이 이미 실행되었다면, Heap에 바로 넣는다.
    if (HasBeenTickedThisFrame())
    {
        Data.ExpireTime = InternalTime + (double)FirstDelay;
        Data.Status = ECityTimerStatus::Active;
        ActiveTimerHeap.HeapPush(NewHandle, FCityTimerHeapOrder(Timers));
    }

    //Tick 실행 전이라면, 대기 Set에 넣는다, Tick 마지막에 추가될 예정
    //Tick 중간에 Heap에 넣으면, Heap이 가장 빠른 타이머를 pop한다는 일관성이 깨진다.
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
