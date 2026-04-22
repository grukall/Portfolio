// Copyright © Grukall 2026.The Otium™ is a trademark of Grukall. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "CityBuildingTimerTypes.h"
#include "CityBuildingTimeSubSystem.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGameSpeedChanged, float, NewGameSpeed);
/**
 * UCityBuildingTimeSubSystem
 * * A high-performance world subsystem designed for medieval city-building simulations.
 * It provides an interface similar to the engine's FTimerManager but is optimized for 
 * large-scale simulations with support for custom game speed (Time Dilation).
 * * Features:
 * - O(log N) insertion and O(1) expiration check using a Min-Heap.
 * - O(1) data access using TSparseArray and indexed handles.
 * - Frame-drop compensation (Catch-up logic) for looping timers.
 * - Re-entrancy safety using GFrameCounter synchronization.
 */
UCLASS()
class CITYBUILDINGTIMESYSTEM_API UCityBuildingTimeSubSystem : public UWorldSubsystem, public FTickableGameObject
{
    GENERATED_BODY()

protected:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
public:
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual bool IsTickable() const override { return !IsTemplate(); }
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UCityBuildingTimeSubSystem, STATGROUP_Tickables); }
    
    
    
    // --- Public API ---
    
    //if Game Speed Changed, Broadcasting with changed speeds.
    UPROPERTY(BlueprintAssignable, BlueprintCallable)
    FGameSpeedChanged GameSpeedChanged;
    
    //Game Time Minute Changed
    UPROPERTY(BlueprintAssignable, BlueprintCallable)
    FOnMinuteChanged OnMinuteChanged;
    
    
    //Init Game CalendarData
    void InitializeFromSavedDate(const FCityCalendarDate& SavedDate);
    
    /**
     * Sets a timer to call a non-const native C++ member function.
     * * @param InOutHandle     Handle to the timer. If it already points to an active timer, it will be cleared before the new one is set.
     * @param InObj           The object instance on which to call the member function.
     * @param InTimerMethod   The function pointer to the member method (e.g., &AMyActor::MyFunction).
     * @param InRate          The time interval (in seconds) between activations.
     * @param bInLoop         If true, the timer will repeat indefinitely at the specified Rate.
     * @param InFirstDelay    Optional delay for the first execution. If < 0, InRate is used.
     * @return                A unique handle used to identify and manage this timer.
     */
    template<class UserClass>
    FORCEINLINE FGameTimeTimerHandle SetTimer(FGameTimeTimerHandle& InOutHandle, UserClass* InObj, typename TMemFunPtrType<false, UserClass, void()>::Type InTimerMethod, float InRate, bool bInLoop = false, float InFirstDelay = -1.f)
    {
        return InternalSetTimer(InOutHandle, FCityTimeUnifiedDelegate([InObj, InTimerMethod]() { (InObj->*InTimerMethod)(); }), InRate, bInLoop, InFirstDelay);
    }

    /**
     * Sets a timer to call a const native C++ member function.
     * * @param InOutHandle     Handle to the timer. Replaces existing timers associated with this handle.
     * @param InObj           The object instance (const) on which to call the member function.
     * @param InTimerMethod   The function pointer to the const member method.
     * @param InRate          The time interval (in seconds) between activations.
     * @param bInLoop         If true, the timer repeats.
     * @param InFirstDelay    Initial delay before the first firing.
     * @return                A unique handle for the new timer.
     */
    template<class UserClass>
    FORCEINLINE FGameTimeTimerHandle SetTimer(FGameTimeTimerHandle& InOutHandle, UserClass* InObj, typename TMemFunPtrType<true, UserClass, void()>::Type InTimerMethod, float InRate, bool bInLoop = false, float InFirstDelay = -1.f)
    {
        return InternalSetTimer(InOutHandle, FCityTimeUnifiedDelegate([InObj, InTimerMethod]() { (InObj->*InTimerMethod)(); }), InRate, bInLoop, InFirstDelay);
    }

    /**
     * Sets a timer to call a lambda or a TFunction.
     * * @param Callback        The TFunction or lambda to execute (e.g., [this](){ MyLogic(); }).
     * @param InRate          Frequency of execution in simulation seconds.
     * @param bInLoop         Whether the timer should repeat.
     * @param InFirstDelay    Initial delay. If < 0, uses InRate.
     * @return                A handle to the newly created timer.
     */
    FORCEINLINE FGameTimeTimerHandle SetTimer(TFunction<void()>&& Callback, float InRate, bool bInLoop, float InFirstDelay = -1.f)
    {
        FGameTimeTimerHandle DummyHandle;
        return InternalSetTimer(DummyHandle, FCityTimeUnifiedDelegate(MoveTemp(Callback)), InRate, bInLoop, InFirstDelay);
    }
    
    FORCEINLINE FGameTimeTimerHandle SetTimer(FGameTimeTimerHandle& InOutHandle, TFunction<void()>&& Callback, float InRate, bool bInLoop, float InFirstDelay = -1.f)
    {
        return InternalSetTimer(InOutHandle, FCityTimeUnifiedDelegate(MoveTemp(Callback)), InRate, bInLoop, InFirstDelay);
    }

    /**
     * Sets a timer to call a Blueprint Dynamic Delegate.
     * * @param InDelegate      The dynamic delegate to trigger (usually passed from Blueprint).
     * @param InRate          Frequency of execution in simulation seconds.
     * @param bInLoop         If true, the delegate triggers repeatedly.
     * @param InFirstDelay    Initial delay before the first trigger.
     * @return                A handle to the newly created timer.
     */
    FORCEINLINE FGameTimeTimerHandle SetTimer(FOnTimeSystemTimerExpired InDelegate, float InRate, bool bInLoop, float InFirstDelay = -1.f)
    {
        FGameTimeTimerHandle DummyHandle;
        return InternalSetTimer(DummyHandle, FCityTimeUnifiedDelegate(InDelegate), InRate, bInLoop, InFirstDelay);
    }

    /**
     * Sets a timer using a parameter structure for advanced configuration.
     * * @param InOutHandle     Handle to the timer. Will be replaced if already valid.
     * @param InDelegate      The delegate to trigger.
     * @param InRate          The interval in seconds.
     * @param Params          A structure containing bLoop, FirstDelay, and other advanced settings.
     * @return                A unique handle for the new timer.
     */
    FORCEINLINE FGameTimeTimerHandle SetTimer(FGameTimeTimerHandle& InOutHandle, FOnTimeSystemTimerExpired InDelegate, float InRate, const FGameTimeTimerParameters& Params)
    {
        return InternalSetTimer(InOutHandle, FCityTimeUnifiedDelegate(InDelegate), InRate, Params.bLoop, Params.FirstDelay);
    }

    /**
     * Sets a one-shot timer to execute a callback on the next logical frame tick.
     * * @param Callback        The logic to execute.
     * @return                A handle for the next-tick timer.
     */
    FGameTimeTimerHandle SetTimerForNextTick(TFunction<void()>&& Callback);
    
    /**
     * Immediately stops and removes the specified timer.
     * @param InHandle       The handle of the timer to clear. Will be invalidated upon success.
     */
    void ClearTimer(FGameTimeTimerHandle& InHandle);

    /**
     * Pauses the specified timer, preserving its remaining time.
     * @param InHandle       The handle of the timer to pause.
     */
    void PauseTimer(FGameTimeTimerHandle InHandle);

    /**
     * Resumes a previously paused timer.
     * @param InHandle       The handle of the timer to resume.
     */
    void UnPauseTimer(FGameTimeTimerHandle InHandle);

    /** Sets the simulation speed.*/
    UFUNCTION(BlueprintCallable, Category = "Manage Time Options")
    void SetGameSpeed(float NewSpeed);
    float GetGameSpeed() const { return GameSpeed; }

    /** Clears all timers bound to a specific object. Essential for cleanup when an Actor is destroyed. */
    UFUNCTION(BlueprintCallable, Category = "Manage Time Options")
    void ClearAllTimersForObject(const UObject* Object);
    
    /* Real Time 1 Seconds = Game Time 1 Seconds * Time Ratio */
    UFUNCTION(BlueprintCallable, Category = "Manage Time Options")
    void SetGameTimeRatio(float _TimeRatio) {TimeRatio = _TimeRatio;}
    
    void UpdateGameClock();
    
    // --- 게임 시간(달력) 기반 API ---
    
    /**특정 게임 날짜에 C++ 객체의 멤버 함수를 호출하는 버전입니다.*/
    template<class UserClass>
    FORCEINLINE FGameTimeTimerHandle SetTimerAtDate(FGameTimeTimerHandle& InOutHandle, UserClass* InObj, typename TMemFunPtrType<false, UserClass, void()>::Type InTimerMethod, const FCityCalendarDate& TargetDate)
    {
        float Delay = CalculateRealDelayToDate(TargetDate);
        return InternalSetTimer(InOutHandle, FCityTimeUnifiedDelegate([InObj, InTimerMethod]() { (InObj->*InTimerMethod)(); }), Delay, false, Delay);
    }
    
    //FCityCalenderDate의 시간이 현재로부터 현실 시간으로 몇초 뒤인지 계산
    float CalculateRealDelayToDate(const FCityCalendarDate& TargetDate) const;
    
    //FCityCalenderDate의 정량적 현실 시간이 몇초 뒤인지 계산
    float CalculateRealTime(const FCityCalendarDate &Date) const;
    
    //인 게임 시간 한 시간을 현실 시간으로 몇초인지
    float GetHour() const {return CalculateRealTime(FCityCalendarDate(0, 0, 0, 1, 0));}
    
    //게임 시작 후 지난 인게임 시간
    double GetCurrentTime() const;
    
    UFUNCTION(BlueprintCallable)
    FCityCalendarDate GetCurrentDate() const;

    
private:
    
    /* Real Time 1 Seconds = Game Time 1 Seconds * Time Ratio */
    float TimeRatio = 20.0f;
    
    const int64 SecondsInMinute = 60;
    const int64 SecondsInHour   = SecondsInMinute * 60;
    const int64 SecondsInDay    = SecondsInHour * 24;
    const int64 SecondsInMonth  = SecondsInDay * 30; // 30일 기준
    const int64 SecondsInYear   = SecondsInMonth * 12;

private:
    TSparseArray<FTimeSystemTimerData> Timers;
    TArray<FGameTimeTimerHandle> ActiveTimerHeap;
    TArray<FGameTimeTimerHandle> PendingTimerSet;
    TArray<FGameTimeTimerHandle> PausedTimers;
    TMap<const void*, TSet<FGameTimeTimerHandle>> ObjectToTimerHandles;
    
    double InternalTime = 0.0;
    float GameSpeed = 1.0f;
    uint64 LastSerialNumber = 0;
    uint64 LastTickedFrame = static_cast<uint64>(-1);
    int64 LastTotalMinutes = -1;

    
    FCityCalendarDate GetInGameDate(float TimeScale) const;
    FGameTimeTimerHandle InternalSetTimer(FGameTimeTimerHandle& InOutHandle, FCityTimeUnifiedDelegate&& InDelegate, float InRate, bool bInLoop, float InFirstDelay);
    FTimeSystemTimerData* FindTimerData(FGameTimeTimerHandle InHandle);
    void CleanupHandle(const FGameTimeTimerHandle& Handle, const void* ObjectKey);
    FORCEINLINE bool HasBeenTickedThisFrame() const { return (LastTickedFrame == GFrameCounter);}
};