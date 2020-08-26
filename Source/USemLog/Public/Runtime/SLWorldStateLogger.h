// Copyright 2017-2020, Institute for Artificial Intelligence - University of Bremen
// Author: Andrei Haidu (http://haidu.eu)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "Runtime/SLLoggerStructs.h"
#include "SLWorldStateLogger.generated.h"

/* Holds the data needed to setup the world state logger */
USTRUCT()
struct FSLWorldStateLoggerParams
{
	GENERATED_BODY();

	// Update rate of the logger
	UPROPERTY(EditAnywhere, Category = "Semantic Logger")
	float UpdateRate = 0.f;

	// Min squared linear distance to log an individual
	UPROPERTY(EditAnywhere, Category = "Semantic Logger")
	float MinLinearDistanceSquared = 0.25f;

	// Min angular distance in order to log an individual
	UPROPERTY(EditAnywhere, Category = "Semantic Logger")
	float MinAngularDistance = 0.1; // rad

	// Database Server Ip
	UPROPERTY(EditAnywhere, Category = "Semantic Logger")
	FString ServerIp = TEXT("127.0.0.1");

	// Database server port num
	UPROPERTY(EditAnywhere, Category = "Semantic Logger", meta = (ClampMin = 0, ClampMax = 65535))
	uint16 ServerPort = 27017;
};

/**
 * Subsymbolic data logger
 */
UCLASS(ClassGroup = (SL), DisplayName = "SL World State Logger")
class USEMLOG_API ASLWorldStateLogger : public AInfo
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ASLWorldStateLogger();

	// Force call on finish
	~ASLWorldStateLogger();

protected:
	// Allow actors to initialize themselves on the C++ side
	virtual void PostInitializeComponents() override;

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called when actor removed from game or game ended
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	// Init logger (called when the logger is synced externally)
	void Init(const FSLWorldStateLoggerParams& InLoggerParameters, const FSLLoggerLocationParams& InLocationParams);

	// Start logger (called when the logger is synced externally)
	void Start();

	// Finish logger (called when the logger is synced externally) (bForced is true if called from destructor)
	void Finish(bool bForced = false);

	// Get init state
	bool IsInit() const { return bIsInit; };

	// Get started state
	bool IsStarted() const { return bIsStarted; };

	// Get finished state
	bool IsFinished() const { return bIsFinished; };

protected:
	// Init logger (called when the logger is used independently)
	void InitImpl();

	// Start logger (called when the logger is used independently)
	void StartImpl();

	// Finish logger (called when the logger is used independently) (bForced is true if called from destructor)
	void FinishImpl(bool bForced = false);

	// Setup user input bindings
	void SetupInputBindings();

	// Start/finish logger from user input
	void UserInputToggleCallback();

private:
	// If true the logger will start on its own (instead of being started by the manager)
	UPROPERTY(EditAnywhere, Category = "Semantic Logger")
	bool bUseIndependently;

	// Logger parameters
	UPROPERTY(EditAnywhere, Category = "Semantic Logger", meta = (editcondition = "bUseIndependently"))
	FSLWorldStateLoggerParams LoggerParameters;

	// Location parameters
	UPROPERTY(EditAnywhere, Category = "Semantic Logger", meta = (editcondition = "bUseIndependently"))
	FSLLoggerLocationParams LocationParameters;

	// Logger start parameters
	UPROPERTY(EditAnywhere, Category = "Semantic Logger", meta = (editcondition = "bUseIndependently"))
	FSLLoggerStartParams StartParameters;

	// True when ready to log
	bool bIsInit;

	// True when active
	bool bIsStarted;

	// True when done logging
	bool bIsFinished;
};
