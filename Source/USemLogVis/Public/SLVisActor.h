// Copyright 2018, Institute for Artificial Intelligence - University of Bremen
// Author: Andrei Haidu (http://haidu.eu)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SLVisActor.generated.h"

UCLASS(ClassGroup = (SL), hidecategories = (HLOD, Mobile, Cooking, AssetUserData), meta = (DisplayName = "SL Vision Actor"))
class USEMLOGVIS_API ASLVisActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ASLVisActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

private:
	// Semantic Vis logger component
	UPROPERTY(EditAnywhere, Category = "Semantic Logger")
	class USLVisManager* SLVisManager;
	
};