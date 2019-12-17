// Copyright 2019, Institute for Artificial Intelligence - University of Bremen
// Author: Andrei Haidu (http://haidu.eu)

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "SLVisionCamera.generated.h"

/**
 * Virtual camera logging the transformations of the cameras that need to re-render the scene
 */
UCLASS()
class USEMLOG_API ASLVisionCamera : public ACameraActor
{
	GENERATED_BODY()

	// Sets default values for this actor's properties
	ASLVisionCamera();

public:
	// Get the semantic class name of the virtual camera
	FString GetClassName();

	// Get the unique id of the virtual camera
	FString GetId();

private:
	// Semantic class name of the virtual vision camera
	FString ClassName;

	// Semantic uinque id of the virtual vision camera
	FString Id;
};