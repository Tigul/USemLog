// Copyright 2017-2020, Institute for Artificial Intelligence - University of Bremen
// Author: Andrei Haidu (http://haidu.eu)

#include "Runtime/SLSymbolicLogger.h"
#include "Individuals/SLIndividualManager.h"
#include "Individuals/SLIndividualComponent.h"

#include "Utils/SLUuid.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Engine/Engine.h"

#include "Events/SLGoogleCharts.h"

#include "Events/SLContactEventHandler.h"
#include "Events/SLManipulatorContactEventHandler.h"
#include "Events/SLGraspEventHandler.h"
#include "Events/SLReachEventHandler.h"
#include "Events/SLPickAndPlaceEventsHandler.h"
#include "Events/SLContainerEventHandler.h"

#include "Monitors/SLContactShapeInterface.h"
#include "Monitors/SLManipulatorListener.h"
#include "Monitors/SLReachListener.h"
#include "Monitors/SLPickAndPlaceListener.h"
#include "Monitors/SLContainerListener.h"

#include "Owl/SLOwlExperimentStatics.h"

#if SL_WITH_MC_GRASP
#include "Events/SLFixationGraspEventHandler.h"
#include "MCGraspFixation.h"
#endif // SL_WITH_MC_GRASP

#if SL_WITH_SLICING
#include "Events/SLSlicingEventHandler.h"
#include "SlicingBladeComponent.h"
#endif // SL_WITH_SLICING

#if WITH_EDITOR
#include "Components/BillboardComponent.h"
#endif // WITH_EDITOR

// Sets default values
ASLSymbolicLogger::ASLSymbolicLogger()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	// Default values
	bIsInit = false;
	bIsStarted = false;
	bIsFinished = false;

	bUseIndependently = false;

#if WITH_EDITORONLY_DATA
	// Make manager sprite smaller (used to easily find the actor in the world)
	SpriteScale = 0.35;
	ConstructorHelpers::FObjectFinderOptional<UTexture2D> SpriteTexture(TEXT("/USemLog/Sprites/S_SLSymbolicLogger"));
	GetSpriteComponent()->Sprite = SpriteTexture.Get();
#endif // WITH_EDITORONLY_DATA
}

// Force call on finish
ASLSymbolicLogger::~ASLSymbolicLogger()
{
	if (!IsTemplate() && !bIsFinished && (bIsStarted || bIsInit))
	{
		FinishImpl(true);
	}
}

// Allow actors to initialize themselves on the C++ side
void ASLSymbolicLogger::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (bUseIndependently)
	{
		InitImpl();
	}
}

// Called when the game starts or when spawned
void ASLSymbolicLogger::BeginPlay()
{
	Super::BeginPlay();
	if (bUseIndependently)
	{
		if (StartParameters.StartTime == ESLLoggerStartTime::AtBeginPlay)
		{
			StartImpl();
		}
		else if (StartParameters.StartTime == ESLLoggerStartTime::AtNextTick)
		{
			FTimerDelegate TimerDelegateNextTick;
			TimerDelegateNextTick.BindLambda([this] {StartImpl(); });
			GetWorld()->GetTimerManager().SetTimerForNextTick(TimerDelegateNextTick);
		}
		else if (StartParameters.StartTime == ESLLoggerStartTime::AfterDelay)
		{
			FTimerHandle TimerHandle;
			FTimerDelegate TimerDelegateDelay;
			TimerDelegateDelay.BindLambda([this] {StartImpl(); });
			GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerDelegateDelay, StartParameters.StartDelay, false);
		}
		else if (StartParameters.StartTime == ESLLoggerStartTime::FromUserInput)
		{
			SetupInputBindings();
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("%s::%d Logger (%s) StartImpl() will not be called.."),
				*FString(__func__), __LINE__, *GetName());
		}
	}
}

// Called when actor removed from game or game ended
void ASLSymbolicLogger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	if (bUseIndependently && !bIsFinished)
	{
		FinishImpl();
	}
}

// Init logger (called when the logger is synced externally)
void ASLSymbolicLogger::Init(const FSLSymbolicLoggerParams& InLoggerParameters,
	const FSLLoggerLocationParams& InLocationParameters)
{
	if (bUseIndependently)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::%d Symbolic logger (%s) is set to work independetly, external calls will have no effect.."),
			*FString(__FUNCTION__), __LINE__, *GetName());
		return;
	}

	LoggerParameters = InLoggerParameters;
	LocationParameters = InLocationParameters;
	InitImpl();
}

// Start logger (called when the logger is synced externally)
void ASLSymbolicLogger::Start()
{
	if (bUseIndependently)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::%d Symbolic logger (%s) is set to work independetly, external calls will have no effect.."),
			*FString(__FUNCTION__), __LINE__, *GetName());
		return;
	}
	StartImpl();
}

// Finish logger (called when the logger is synced externally) (bForced is true if called from destructor)
void ASLSymbolicLogger::Finish(bool bForced)
{
	if (bUseIndependently)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::%d Symbolic logger (%s) is set to work independetly, external calls will have no effect.."),
			*FString(__FUNCTION__), __LINE__, *GetName());
		return;
	}
	FinishImpl();
}

// Init logger (called when the logger is used independently)
void ASLSymbolicLogger::InitImpl()
{
	if (bIsInit)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::%d Symbolic logger (%s) is already initialized.."), *FString(__FUNCTION__), __LINE__, *GetName());
		return;
	}

	if (!LocationParameters.bUseCustomTaskId)
	{
		LocationParameters.TaskId = FSLUuid::NewGuidInBase64Url();
	}

	if (!LocationParameters.bUseCustomEpisodeId)
	{
		LocationParameters.EpisodeId = FSLUuid::NewGuidInBase64Url();
	}

	// Make sure the individual manager is set and loaded
	if (!SetIndividualManager())
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%d Viz manager (%s) could not set the individual manager.."),
			*FString(__FUNCTION__), __LINE__, *GetName());
		return;
	}
	if (!IndividualManager->Load(false))
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%d Viz manager (%s) could not load the individual manager (%s).."),
			*FString(__FUNCTION__), __LINE__, *GetName(), *IndividualManager->GetName());
		return;
	}

	// Create the document template
	ExperimentDoc = CreateEventsDocTemplate(ESLOwlExperimentTemplate::Default, LocationParameters.EpisodeId);

	// Setup monitors
	if (!LoggerParameters.bSelectedEventsOnly)
	{
		InitContactMonitors();
		InitManipulatorContactMonitors();
		InitManipulatorFixationMonitors();
		InitSlicingMonitors();
	}
	else
	{
		if (LoggerParameters.bContact)
		{
			InitContactMonitors();
		}
		if (LoggerParameters.bGrasp || LoggerParameters.bContact)
		{
			InitManipulatorContactMonitors();
			if (LoggerParameters.bGrasp)
			{
				InitManipulatorFixationMonitors();
			}
		}
		if (LoggerParameters.bSlicing)
		{
			InitSlicingMonitors();
		}
	}

	if (LoggerParameters.bPublishToROS)
	{
		InitROSPublisher();
	}

	bIsInit = true;
	UE_LOG(LogTemp, Warning, TEXT("%s::%d Symbolic logger (%s) succesfully initialized at %.2f.."),
		*FString(__FUNCTION__), __LINE__, *GetName(), GetWorld()->GetTimeSeconds());
}

// Start logger (called when the logger is used independently)
void ASLSymbolicLogger::StartImpl()
{
	if (bIsStarted)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::%d Symbolic logger (%s) is already started.."), *FString(__FUNCTION__), __LINE__, *GetName());
		return;
	}

	if (!bIsInit)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::%d Symbolic logger (%s) is not initialized, cannot start.."), *FString(__FUNCTION__), __LINE__, *GetName());
		return;
	}

	// Start handlers
	for (auto& EvHandler : EventHandlers)
	{
		// Subscribe for given semantic events
		EvHandler->Start();

		// Bind resulting events
		EvHandler->OnSemanticEvent.BindUObject(
			this, &ASLSymbolicLogger::SemanticEventFinishedCallback);
	}

	// Start the semantic overlap areas
	for (auto& Listener : ContactShapes)
	{
		Listener->Start();
	}
	// Start the grasp listeners
	for (auto& Listener : GraspListeners)
	{
		Listener->Start();
	}
	// Start the pick and place listeners
	for (auto& Listener : PickAndPlaceListeners)
	{
		Listener->Start();
	}
	// Start the reach listeners
	for (auto& Listener : ReachListeners)
	{
		Listener->Start();
	}
	// Start the container listeners
	for (auto& Listener : ContainerListeners)
	{
		Listener->Start();
	}


	bIsStarted = true;
	UE_LOG(LogTemp, Warning, TEXT("%s::%d Symbolic logger (%s) succesfully started at %.2f.."),
		*FString(__FUNCTION__), __LINE__, *GetName(), GetWorld()->GetTimeSeconds());
}

// Finish logger (called when the logger is used independently) (bForced is true if called from destructor)
void ASLSymbolicLogger::FinishImpl(bool bForced)
{
	if (bIsFinished)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::%d Symbolic logger (%s) is already finished.."),
			*FString(__FUNCTION__), __LINE__, *GetName());
		return;
	}

	if (!bIsInit && !bIsStarted)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::%d Symbolic logger (%s) is not initialized nor started, cannot finish.."),
			*FString(__FUNCTION__), __LINE__, *GetName());
		return;
	}

	// Finish handlers pending events
	for (auto& EvHandler : EventHandlers)
	{
		EvHandler->Finish(Time, bForced);
	}
	EventHandlers.Empty();

	// Finish semantic overlap events publishing
	for (auto& SLContactShape : ContactShapes)
	{
		SLContactShape->Finish();
	}
	ContactShapes.Empty();

	// Finish the grasp listeners
	for (auto& SLManipulatorListener : GraspListeners)
	{
		SLManipulatorListener->Finish(bForced);
	}
	GraspListeners.Empty();

	// Start the reach listeners
	for (auto& SLReachListener : ReachListeners)
	{
		SLReachListener->Start();
	}
	ReachListeners.Empty();

	// Create the experiment owl doc	
	if (ExperimentDoc.IsValid())
	{
		for (const auto& Ev : FinishedEvents)
		{
			Ev->AddToOwlDoc(ExperimentDoc.Get());
		}

		// Add stored unique timepoints to doc
		ExperimentDoc->AddTimepointIndividuals();

		// Add stored unique objects to doc
		ExperimentDoc->AddObjectIndividuals();

		// Add experiment individual to doc
		ExperimentDoc->AddExperimentIndividual();
	}

	// Write events to file
	WriteToFile();

#if SL_WITH_ROSBRIDGE
	// Finish ROS Connection
	ROSPrologClient->Disconnect();
#endif // SL_WITH_ROSBRIDGE

	bIsStarted = false;
	bIsInit = false;
	bIsFinished = true;
	UE_LOG(LogTemp, Warning, TEXT("%s::%d Symbolic logger (%s) succesfully finished at %.2f.."),
		*FString(__FUNCTION__), __LINE__, *GetName(), GetWorld()->GetTimeSeconds());
}

// Bind user inputs
void ASLSymbolicLogger::SetupInputBindings()
{
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (UInputComponent* IC = PC->InputComponent)
		{
			IC->BindAction(StartParameters.UserInputActionName, IE_Pressed, this, &ASLSymbolicLogger::UserInputToggleCallback);
		}
	}
}

// Start input binding
void ASLSymbolicLogger::UserInputToggleCallback()
{
	if (bIsInit && !bIsStarted)
	{
		ASLSymbolicLogger::StartImpl();
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green, FString::Printf(TEXT("[%.2f] Symbolic logger (%s) started.."), GetWorld()->GetTimeSeconds(), *GetName()));
	}
	else if (bIsStarted && !bIsFinished)
	{
		ASLSymbolicLogger::FinishImpl();
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red, FString::Printf(TEXT("[%.2f] Symbolic logger (%s) finished.."), GetWorld()->GetTimeSeconds(), *GetName()));
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow, FString::Printf(TEXT("[%.2f] Symbolic logger (%s) logger finished, or not initalized.."), GetWorld()->GetTimeSeconds(), *GetName()));
	}
}

// Called when a semantic event is done
void ASLSymbolicLogger::SemanticEventFinishedCallback(TSharedPtr<ISLEvent> Event)
{
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, FString::Printf(TEXT("%s::%d %s"), *FString(__func__), __LINE__, *Event->ToString()));
	//UE_LOG(LogTemp, Error, TEXT(">> %s::%d %s"), *FString(__func__), __LINE__, *Event->ToString());
	FinishedEvents.Add(Event);

#if SL_WITH_ROSBRIDGE
	if (LoggerParameters.bPublishToROS)
	{
		ROSPrologClient->AddEventQuery(Event);
	}
#endif // SL_WITH_ROSBRIDGE
}

// Write data to file
void ASLSymbolicLogger::WriteToFile()
{
	const FString DirPath = FPaths::ProjectDir() + "/SL/" + LocationParameters.TaskId /*+ TEXT("/Episodes/")*/ + "/";

	// Write events timelines to file
	if (LoggerParameters.bWriteTimelines)
	{
		FSLGoogleChartsParameters Params;
		Params.bTooltips = true;
		FSLGoogleCharts::WriteTimelines(FinishedEvents, DirPath, LocationParameters.EpisodeId, Params);
	}

	// Write owl data to file
	if (ExperimentDoc.IsValid())
	{
		// Write experiment to file
		FString FullFilePath = DirPath + LocationParameters.EpisodeId + TEXT("_ED.owl");
		FPaths::RemoveDuplicateSlashes(FullFilePath);
		FFileHelper::SaveStringToFile(ExperimentDoc->ToString(), *FullFilePath);		
	}
}

// Create events doc template
TSharedPtr<FSLOwlExperiment> ASLSymbolicLogger::CreateEventsDocTemplate(ESLOwlExperimentTemplate TemplateType, const FString& InDocId)
{
	// Create unique semlog id for the document
	const FString DocId = InDocId.IsEmpty() ? FSLUuid::NewGuidInBase64Url() : InDocId;

	// Fill document with template values
	if (TemplateType == ESLOwlExperimentTemplate::Default)
	{
		return FSLOwlExperimentStatics::CreateDefaultExperiment(DocId);
	}
	else if (TemplateType == ESLOwlExperimentTemplate::IAI)
	{
		return FSLOwlExperimentStatics::CreateUEExperiment(DocId);
	}
	return MakeShareable(new FSLOwlExperiment());
}

// Get the reference or spawn a new individual manager
bool ASLSymbolicLogger::SetIndividualManager()
{
	if (IndividualManager && IndividualManager->IsValidLowLevel() && !IndividualManager->IsPendingKillOrUnreachable())
	{
		return true;
	}

	for (TActorIterator<ASLIndividualManager>Iter(GetWorld()); Iter; ++Iter)
	{
		if ((*Iter)->IsValidLowLevel() && !(*Iter)->IsPendingKillOrUnreachable())
		{
			IndividualManager = *Iter;
		}
	}

	// Spawning a new manager
	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = TEXT("SL_IndividualManager");
	IndividualManager = GetWorld()->SpawnActor<ASLIndividualManager>(SpawnParams);
#if WITH_EDITOR
	IndividualManager->SetActorLabel(TEXT("SL_IndividualManager"));
#endif // WITH_EDITOR
	return true;
}

// Helper function which checks if the individual data is loaded
bool ASLSymbolicLogger::IsValidAndLoaded(AActor* Actor)
{
	if (Actor == nullptr || !Actor->IsValidLowLevel() || Actor->IsPendingKillOrUnreachable())
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%d %s is not valid.."), *FString(__func__), __LINE__, *Actor->GetName());
		return false;
	}
	if (!GetWorld()->ContainsActor(Actor))
	{
		//UE_LOG(LogTemp, Error, TEXT("%s::%d %s is not from this world.."), *FString(__func__), __LINE__, *Actor->GetName());
		return false;
	}
	if (UActorComponent* ActComp = Actor->GetComponentByClass(USLIndividualComponent::StaticClass()))
	{
		if (CastChecked<USLIndividualComponent>(ActComp)->IsLoaded())
		{
			return true;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("%s::%d %s's individual is not loaded.."), *FString(__func__), __LINE__, *Actor->GetName());
		}
	}
	return false;
}

// Iterate contact monitors in the world
void ASLSymbolicLogger::InitContactMonitors()
{
	// Init all contact trigger handlers
	for (TObjectIterator<UShapeComponent> Itr; Itr; ++Itr)
	{
		//if (Itr->GetClass()->ImplementsInterface(USLContactShapeInterface::StaticClass()))
		if (ISLContactShapeInterface* ContactShape = Cast<ISLContactShapeInterface>(*Itr))
		{
			if (IsValidAndLoaded(Itr->GetOwner()))
			{
				ContactShape->Init(LoggerParameters.bSupportedBy);
				ContactShapes.Emplace(ContactShape);

				// Create a contact event handler 
				TSharedPtr<FSLContactEventHandler> CEHandler = MakeShareable(new FSLContactEventHandler());
				CEHandler->Init(*Itr);
				if (CEHandler->IsInit())
				{
					EventHandlers.Add(CEHandler);
					UE_LOG(LogTemp, Warning, TEXT("%s::%d CONTACT INIT %s "),
						*FString(__FUNCTION__), __LINE__, *Itr->GetFullName());
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("%s::%d Handler could not be init with parent %s.."),
						*FString(__func__), __LINE__, *Itr->GetName());
				}
			}
		}
	}
}

// Iterate and init the manipulator contact monitors in the world
void ASLSymbolicLogger::InitManipulatorContactMonitors()
{
	// Init all grasp listeners
	for (TObjectIterator<USLManipulatorListener> Itr; Itr; ++Itr)
	{
		if (IsValidAndLoaded(Itr->GetOwner()))
		{
			if (Itr->Init(LoggerParameters.bGrasp, LoggerParameters.bContact))
			{
				GraspListeners.Emplace(*Itr);
				TSharedPtr<FSLGraspEventHandler> GEHandler = MakeShareable(new FSLGraspEventHandler());
				GEHandler->Init(*Itr);
				if (GEHandler->IsInit())
				{
					EventHandlers.Add(GEHandler);
					UE_LOG(LogTemp, Warning, TEXT("%s::%d GRASP INIT %s "),
						*FString(__FUNCTION__), __LINE__, *Itr->GetFullName());
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("%s::%d Handler could not be init with parent %s.."),
						*FString(__func__), __LINE__, *Itr->GetName());
				}

				// The grasp listener can also publish contact events
				if (LoggerParameters.bContact)
				{
					TSharedPtr<FSLManipulatorContactEventHandler> MCEHandler = MakeShareable(new FSLManipulatorContactEventHandler());
					MCEHandler->Init(*Itr);
					if (MCEHandler->IsInit())
					{
						EventHandlers.Add(MCEHandler);
						UE_LOG(LogTemp, Warning, TEXT("%s::%d GRASP-CONTACT INIT %s "),
							*FString(__FUNCTION__), __LINE__, *Itr->GetFullName());
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("%s::%d Handler could not be init with parent %s.."),
							*FString(__func__), __LINE__, *Itr->GetName());
					}
				}
			}
		}
	}
}

// Iterate and init the manipulator fixation monitors in the world
void ASLSymbolicLogger::InitManipulatorFixationMonitors()
{
#if SL_WITH_MC_GRASP
	// Init fixation grasp listeners
	for (TObjectIterator<UMCGraspFixation> Itr; Itr; ++Itr)
	{
		if (IsValidAndLoaded(Itr->GetOwner()))
		{
			// Create a grasp event handler 
			TSharedPtr<FSLFixationGraspEventHandler> FGEHandler = MakeShareable(new FSLFixationGraspEventHandler());
			FGEHandler->Init(*Itr);
			if (FGEHandler->IsInit())
			{
				EventHandlers.Add(FGEHandler);
				UE_LOG(LogTemp, Warning, TEXT("%s::%d FIXATION-GRASP INIT %s "),
					*FString(__FUNCTION__), __LINE__, *Itr->GetFullName());
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("%s::%d Handler could not be init with parent %s.."),
					*FString(__func__), __LINE__, *Itr->GetName());
			}
		}
	}
#endif // SL_WITH_MC_GRASP
}

// Iterate and init the slicing monitors
void ASLSymbolicLogger::InitSlicingMonitors()
{
#if SL_WITH_SLICING
	for (TObjectIterator<USlicingBladeComponent> Itr; Itr; ++Itr)
	{
		// Make sure the object is in the world
		if (IsValidAndLoaded(Itr->GetOwner()))
		{
			// Create a Slicing event handler 
			TSharedPtr<FSLSlicingEventHandler> SEHandler = MakeShareable(new FSLSlicingEventHandler());
			SEHandler->Init(*Itr);
			if (SEHandler->IsInit())
			{
				EventHandlers.Add(SEHandler);
				UE_LOG(LogTemp, Warning, TEXT("%s::%d SLICING INIT %s "),
					*FString(__FUNCTION__), __LINE__, *Itr->GetFullName());
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("%s::%d Handler could not be init with parent %s.."),
					*FString(__func__), __LINE__, *Itr->GetName());
			}
		}
	}
#endif // SL_WITH_SLICING
}

// Publish data through ROS
void ASLSymbolicLogger::InitROSPublisher()
{
#if SL_WITH_ROSBRIDGE
	ROSPrologClient = NewObject<USLPrologClient>(this);
	ROSPrologClient->Init(WriterParams.ServerIp, WriterParams.ServerPort);
	FSLEntitiesManager::GetInstance()->SetPrologClient(ROSPrologClient);
#endif // SL_WITH_ROSBRIDGE
}

