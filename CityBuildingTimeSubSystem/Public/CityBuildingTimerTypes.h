// CityBuildingTimeTypes.h
#pragma once

#include "CoreMinimal.h"
#include "CityBuildingTimerTypes.generated.h"

DECLARE_DYNAMIC_DELEGATE(FOnTimeSystemTimerExpired);

UENUM()
enum class ECityTimerStatus : uint8
{
	Pending,                
	Active,               
	Paused,               
	Executing,             
	ActivePendingRemoval   
};

struct FCityTimeUnifiedDelegate
{
	TFunction<void()> FuncDelegate;
	FOnTimeSystemTimerExpired DynamicDelegate;

	// 기본 생성자
	FCityTimeUnifiedDelegate() : FuncDelegate(nullptr) {}

	// C++ TFunction용 생성자
	FCityTimeUnifiedDelegate(TFunction<void()>&& InFunc) 
		: FuncDelegate(MoveTemp(InFunc)) {}

	// 블루프린트 Dynamic Delegate용 생성자
	FCityTimeUnifiedDelegate(const FOnTimeSystemTimerExpired& InDynDel) 
		: DynamicDelegate(InDynDel) {}

	bool IsBound() const { return FuncDelegate != nullptr || DynamicDelegate.IsBound(); }
    
	void Execute() const 
	{
		if (FuncDelegate) FuncDelegate();
		else if (DynamicDelegate.IsBound()) DynamicDelegate.ExecuteIfBound();
	}

	const void* GetBoundObject() const 
	{ 
		return DynamicDelegate.IsBound() ? DynamicDelegate.GetUObject() : nullptr; 
	}
};

USTRUCT(BlueprintType)
struct FGameTimeTimerHandle
{
	GENERATED_BODY()
	
	uint32 Index = INDEX_NONE;
	uint64 SerialNumber = 0;

	bool IsValid() const { return SerialNumber > 0 && Index != INDEX_NONE; }
	void Invalidate() { SerialNumber = 0; Index = INDEX_NONE; }
	bool operator==(const FGameTimeTimerHandle& Other) const
{ 
		return SerialNumber == Other.SerialNumber && Index == Other.Index; 
	}
};

FORCEINLINE uint32 GetTypeHash(const FGameTimeTimerHandle& Handle)
{
	uint32 Hash = GetTypeHash(Handle.Index);
	return HashCombine(Hash, GetTypeHash(Handle.SerialNumber));
}

struct FTimeSystemTimerData
{
	FGameTimeTimerHandle Handle;
	float Rate;
	double ExpireTime;
	bool bLoop;
	ECityTimerStatus Status;
	FCityTimeUnifiedDelegate TimerDelegate;
	const void* ObjectKey = nullptr;

	FTimeSystemTimerData() : Rate(0.f), ExpireTime(0.0), bLoop(false), Status(ECityTimerStatus::Active) {}
};

struct FCityTimerHeapOrder
{
	const TSparseArray<FTimeSystemTimerData>& Timers;
	
	FCityTimerHeapOrder(const TSparseArray<FTimeSystemTimerData>& InTimers) : Timers(InTimers) {}
	bool operator()(const FGameTimeTimerHandle& A, const FGameTimeTimerHandle& B) const
	{
		return Timers[A.Index].ExpireTime < Timers[B.Index].ExpireTime;
	}
};

USTRUCT(BlueprintType)
struct FGameTimeTimerParameters
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FirstDelay = -1.f;

	/** 루핑 타이머가 한 프레임에 최대 한 번만 실행될지 여부 (시뮬레이션 정밀도 조절용) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bMaxOncePerFrame = false;
};

USTRUCT(BlueprintType)
struct FCityCalendarDate
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Time", meta = (ClampMin = "0", ClampMax = "1000"))
	int32 Year = 0;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Time", meta = (ClampMin = "0", ClampMax = "364"))
	int32 Month = 0;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Time", meta = (ClampMin = "0", ClampMax = "31"))
	int32 Day = 1;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Time", meta = (ClampMin = "0", ClampMax = "23"))
	int32 Hour = 0;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Time", meta = (ClampMin = "0", ClampMax = "59"))
	int32 Minute = 0;

	// Year, Month, Day - Hour:Minute) */
	FString ToString() const
	{
		return FString::Printf(TEXT("Year %d, Month %d, Day %d - %02d:%02d"), 
			Year, Month, Day, Hour, Minute);
	}
	
	FCityCalendarDate(){}
	explicit FCityCalendarDate(const int32 _Year, const int32 _Month, const int32 _Day, const int32 _Hour, const int32 _Minute)
		: Year(_Year), Month(_Month), Day(_Day), Hour(_Hour), Minute(_Minute){}
	
	bool operator>(const FCityCalendarDate &D2) const
	{
		if (Year != D2.Year) return Year > D2.Year;
		if (Month != D2.Month) return Month > D2.Month;
		if (Day != D2.Day) return Day > D2.Day;
		if (Hour != D2.Hour) return Hour > D2.Hour;
		return Minute > D2.Minute;
	}
	
	bool operator<(const FCityCalendarDate &D2) const
	{
		if (Year != D2.Year) return Year < D2.Year;
		if (Month != D2.Month) return Month < D2.Month;
		if (Day != D2.Day) return Day < D2.Day;
		if (Hour != D2.Hour) return Hour < D2.Hour;
		return Minute < D2.Minute;
	}
	
	bool operator<=(const FCityCalendarDate &D2) const
	{
		return !(*this > D2);
	}
	
	bool operator>=(const FCityCalendarDate &D2) const
	{
		return !(*this < D2);
	}
	
	bool operator==(const FCityCalendarDate& Other) const
	{
		if (Year == Other.Year && Month == Other.Month && Day == Other.Day && Hour == Other.Hour && Minute == Other.Minute)
			return true;
		return false;
	}
	
	FCityCalendarDate& operator+=(const FCityCalendarDate& Other)
	{
	    Minute += Other.Minute;
	    int32 ExtraHours = Minute / 60;
	    Minute %= 60;
		
	    Hour += Other.Hour + ExtraHours;
	    int32 ExtraDays = Hour / 24;
	    Hour %= 24;
		
	    Day += Other.Day + ExtraDays;
		
	    while (Day > 30)
	    {
	       Day -= 30;
	       Month++;
	    }
		
	    Month += Other.Month;
		
	    while (Month > 12)
	    {
	       Month -= 12;
	       Year++;
	    }

	    Year += Other.Year;

	    return *this;
	}
	
	FCityCalendarDate& operator-=(const FCityCalendarDate& Other)
	{
	    Year -= Other.Year;
	    Month -= Other.Month;
	    Day -= Other.Day;
	    Hour -= Other.Hour;
	    Minute -= Other.Minute;
		
	    while (Minute < 0)
	    {
	       Minute += 60;
	       Hour--;
	    }
		
	    while (Hour < 0)
	    {
	       Hour += 24;
	       Day--;
	    }
		
	    while (Day < 1)
	    {
	       Day += 30;
	       Month--;
	    }
	    while (Month < 1)
	    {
	       Month += 12;
	       Year--;
	    }

	    return *this;
	}

	FCityCalendarDate operator-(const FCityCalendarDate& Other) const
	{
	    FCityCalendarDate Result = *this;
	    Result -= Other;
	    return Result;
	}
	
	FCityCalendarDate operator+(const FCityCalendarDate& Other) const
	{
	    FCityCalendarDate Result = *this;
	    Result += Other;
	    return Result;
	}
	
	
	//24시간 더함
	void SetNextDay()
	{
		Day++;
		if (Day > 30)
		{
			Day = 1;
			Month++;
		}
		
		if (Month > 12)
		{
			Month = 1;
			Year++;
		}
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMinuteChanged, const FCityCalendarDate&, NewDate);