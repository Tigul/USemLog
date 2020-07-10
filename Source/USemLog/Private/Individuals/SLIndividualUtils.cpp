// Copyright 2017-2020, Institute for Artificial Intelligence - University of Bremen
// Author: Andrei Haidu (http://haidu.eu)

#include "Individuals/SLIndividualUtils.h"
#include "Individuals/SLBaseIndividual.h"
#include "Individuals/SLConstraintIndividual.h"
#include "Individuals/SLRigidIndividual.h"
#include "Individuals/SLSkyIndividual.h"
#include "Individuals/SLSkeletalIndividual.h"
#include "Individuals/SLBoneIndividual.h"
#include "Individuals/SLVirtualViewIndividual.h"
#include "Individuals/SLIndividualManager.h"

#include "Individuals/SLIndividualComponent.h"
#include "Individuals/SLIndividualTextInfoComponent.h"

#include "EngineUtils.h"
#include "Kismet2/ComponentEditorUtils.h" // GenerateValidVariableName

#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicsConstraintActor.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

#include "Vision/SLVirtualCameraView.h"

#include "Atmosphere/AtmosphericFog.h"

// Skeletal data asset search
#include "AssetRegistryModule.h"
#include "Skeletal/SLSkeletalDataAsset.h"

// Utils
#include "Utils/SLUuid.h"

#include <bson/bson.h>
#if SL_WITH_LIBMONGO_C
THIRD_PARTY_INCLUDES_START
	#if PLATFORM_WINDOWS
	#include "Windows/AllowWindowsPlatformTypes.h"
	#include <bson/bson.h>
	#include "Windows/HideWindowsPlatformTypes.h"
	#else
	#include <bson/bson.h>
	#endif // #if PLATFORM_WINDOWS
THIRD_PARTY_INCLUDES_END
#endif // SL_WITH_LIBMONGO_C

// Get the individual component from the actor (nullptr if it does not exist)
USLIndividualComponent* FSLIndividualUtils::GetIndividualComponent(AActor* Owner)
{
	if (UActorComponent* AC =Owner->GetComponentByClass(USLIndividualComponent::StaticClass()))
	{
		return CastChecked<USLIndividualComponent>(AC);
	}
	return nullptr;
}

// Get the individual object from the actor (nullptr if it does not exist)
USLBaseIndividual* FSLIndividualUtils::GetIndividualObject(AActor* Owner)
{
	if (USLIndividualComponent* IC = GetIndividualComponent(Owner))
	{
		return IC->GetIndividualObject();
	}
	return nullptr;
}

// Get class name of actor (if not known use label name if bDefaultToLabelName is true)
FString FSLIndividualUtils::GetIndividualClassName(USLIndividualComponent* IndividualComponent, bool bDefaultToLabelName)
{
	AActor* CompOwner = IndividualComponent->GetOwner();
	if (AStaticMeshActor* SMA = Cast<AStaticMeshActor>(CompOwner))
	{
		if (UStaticMeshComponent* SMC = SMA->GetStaticMeshComponent())
		{
			FString ClassName = SMC->GetStaticMesh()->GetFullName();
			int32 FindCharPos;
			ClassName.FindLastChar('.', FindCharPos);
			ClassName.RemoveAt(0, FindCharPos + 1);
			if (!ClassName.RemoveFromStart(TEXT("SM_")))
			{
				UE_LOG(LogTemp, Warning, TEXT("%s::%d %s StaticMesh has no SM_ prefix in its name.."),
					*FString(__func__), __LINE__, *CompOwner->GetName());
			}
			return ClassName;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("%s::%d %s has no SMC.."),
				*FString(__func__), __LINE__, *CompOwner->GetName());
			return FString();
		}
	}
	else if (ASkeletalMeshActor* SkMA = Cast<ASkeletalMeshActor>(CompOwner))
	{
		if (USkeletalMeshComponent* SkMC = SkMA->GetSkeletalMeshComponent())
		{
			FString ClassName = SkMC->SkeletalMesh->GetFullName();
			int32 FindCharPos;
			ClassName.FindLastChar('.', FindCharPos);
			ClassName.RemoveAt(0, FindCharPos + 1);
			ClassName.RemoveFromStart(TEXT("SK_"));
			return ClassName;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("%s::%d %s has no SkMC.."),
				*FString(__func__), __LINE__, *CompOwner->GetName());
			return FString();
		}
	}
	else if (ASLVirtualCameraView* VCA = Cast<ASLVirtualCameraView>(CompOwner))
	{
		static const FString TagType = "SemLog";
		static const FString TagKey = "Class";
		FString ClassName = "View";
	
		// Check attachment actor
		if (AActor* AttAct = CompOwner->GetAttachParentActor())
		{
			if (CompOwner->GetAttachParentSocketName() != NAME_None)
			{
				return CompOwner->GetAttachParentSocketName().ToString() + ClassName;
			}
			else
			{
				FString AttParentClass = FSLTagIO::GetValue(AttAct, TagType, TagKey);
				if (!AttParentClass.IsEmpty())
				{
					return AttParentClass + ClassName;
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("%s::%d Attached parent %s has no semantic class (yet?).."),
						*FString(__func__), __LINE__, *AttAct->GetName());
					return ClassName;
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("%s::%d %s is not attached to any actor.."),
				*FString(__func__), __LINE__, *CompOwner->GetName());
			return ClassName;
		}
	}
	else if (APhysicsConstraintActor* PCA = Cast<APhysicsConstraintActor>(CompOwner))
	{
		FString ClassName = "Joint";
	
		if (UPhysicsConstraintComponent* PCC = PCA->GetConstraintComp())
		{
			if (PCC->ConstraintInstance.GetLinearXMotion() != ELinearConstraintMotion::LCM_Locked ||
				PCC->ConstraintInstance.GetLinearYMotion() != ELinearConstraintMotion::LCM_Locked ||
				PCC->ConstraintInstance.GetLinearZMotion() != ELinearConstraintMotion::LCM_Locked)
			{
				return "Linear" + ClassName;
			}
			else if (PCC->ConstraintInstance.GetAngularSwing1Motion() != EAngularConstraintMotion::ACM_Locked ||
				PCC->ConstraintInstance.GetAngularSwing2Motion() != EAngularConstraintMotion::ACM_Locked ||
				PCC->ConstraintInstance.GetAngularTwistMotion() != EAngularConstraintMotion::ACM_Locked)
			{
				return "Revolute" + ClassName;
			}
			else
			{
				return "Fixed" + ClassName;
			}
		}
		return ClassName;
	}
	else if (AAtmosphericFog* AAF = Cast<AAtmosphericFog>(CompOwner))
	{
		return "AtmosphericFog";
	}
	else if (CompOwner->GetName().Contains("SkySphere"))
	{
		return "SkySphere";
	}
	else if (bDefaultToLabelName)
	{
		return CompOwner->GetActorLabel();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::%d Could not get the semantic class name for %s .."),
			*FString(__func__), __LINE__, *CompOwner->GetName());
		return FString();
	}
}

// Create default individual object depending on the owner type (returns nullptr if failed)
USLBaseIndividual* FSLIndividualUtils::CreateIndividualObject(UObject* Outer, AActor* Owner)
{
	// Set semantic individual type depending on owner
	if (Owner->IsA(AStaticMeshActor::StaticClass()))
	{
		return NewObject<USLBaseIndividual>(Outer, USLRigidIndividual::StaticClass());
	}
	else if (Owner->IsA(APhysicsConstraintActor::StaticClass()))
	{
		return NewObject<USLBaseIndividual>(Outer, USLConstraintIndividual::StaticClass());
	}
	else if (Owner->IsA(ASLVirtualCameraView::StaticClass()))
	{
		return NewObject<USLBaseIndividual>(Outer, USLVirtualViewIndividual::StaticClass());
	}
	else if (Owner->IsA(ASkeletalMeshActor::StaticClass()))
	{
		return NewObject<USLBaseIndividual>(Outer, USLSkeletalIndividual::StaticClass());
	}
	else if (Owner->IsA(AAtmosphericFog::StaticClass()) || Owner->GetName().Contains("SkySphere"))
	{
		return NewObject<USLBaseIndividual>(Outer, USLSkyIndividual::StaticClass());
	}
	else
	{
		//UE_LOG(LogTemp, Error, TEXT("%s::%d unsuported actor type for creating a semantic individual %s-%s.."),
		//	*FString(__FUNCTION__), __LINE__, *Owner->GetClass()->GetName(), *Owner->GetName());
	}
	return nullptr;
}

// Convert individual to the given type
bool FSLIndividualUtils::ConvertIndividualObject(USLBaseIndividual*& IndividualObject, TSubclassOf<class USLBaseIndividual> ConvertToClass)
{
	//if (ConvertToClass && IndividualObject && !IndividualObject->IsPendingKill())
	//{
	//	if (IndividualObject->GetClass() != ConvertToClass)
	//	{
	//		// TODO cache common individual data to copy to the newly created individual
	//		UObject* Outer = IndividualObject->GetOuter();
	//		IndividualObject->ConditionalBeginDestroy();
	//		IndividualObject = NewObject<USLBaseIndividual>(Outer, ConvertToClass);
	//		return true;
	//	}
	//	else
	//	{
	//		//UE_LOG(LogTemp, Error, TEXT("%s::%d Same class type (%s-%s), no conversion is required.."),
	//		//	*FString(__FUNCTION__), __LINE__, *IndividualObject->GetClass()->GetName(), *ConvertToClass->GetName());
	//	}
	//}
	return false;
}

// Generate a new bson oid as string, empty string if fails
FString FSLIndividualUtils::NewOIdAsString()
{
#if SL_WITH_LIBMONGO_C
	bson_oid_t new_oid;
	bson_oid_init(&new_oid, NULL);
	char oid_str[25];
	bson_oid_to_string(&new_oid, oid_str);
	return FString(UTF8_TO_TCHAR(oid_str));
#elif
	return FString();
#endif // #if PLATFORM_WINDOWS
	return FString();
}

// Find the skeletal data asset for the individual
USLSkeletalDataAsset* FSLIndividualUtils::FindSkeletalDataAsset(AActor* Owner)
{
	if (ASkeletalMeshActor* SkMA = Cast<ASkeletalMeshActor>(Owner))
	{
		if (USkeletalMeshComponent* SkMC = SkMA->GetSkeletalMeshComponent())
		{
			// Get skeletal mesh name (SK_ABC)
			FString SkelAssetName = SkMC->SkeletalMesh->GetFullName();
			int32 FindCharPos;
			SkelAssetName.FindLastChar('.', FindCharPos);
			SkelAssetName.RemoveAt(0, FindCharPos + 1);

			// Find data asset (SLSK_ABC)
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			TArray<FAssetData> AssetData;
			FARFilter Filter;
			Filter.PackagePaths.Add("/USemLog/Skeletal");
			Filter.ClassNames.Add(USLSkeletalDataAsset::StaticClass()->GetFName());
			AssetRegistryModule.Get().GetAssets(Filter, AssetData);

			// Search for the results
			for (const auto& AD : AssetData)
			{
				if (AD.AssetName.ToString().Contains(SkelAssetName))
				{
					return Cast<USLSkeletalDataAsset>(AD.GetAsset());
				}
			}
		}
	}
	return nullptr;
}


/* Individuals */
// Get the semantic individual manager from the world or create a new one if none are available
ASLIndividualManager* FSLIndividualUtils::GetOrCreateNewIndividualManager(UWorld* World, bool bCreateNew)
{
	int32 ActNum = 0;
	ASLIndividualManager* Manager = nullptr;
	for (TActorIterator<ASLIndividualManager> ActItr(World); ActItr; ++ActItr)
	{
		Manager = *ActItr;
		ActNum++;
	}
	if (ActNum > 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::%d There are %ld individual managers in the world, the should only be one.."),
			*FString(__FUNCTION__), __LINE__, ActNum);
	}
	else if (ActNum == 0 && bCreateNew)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s::%d There are no individual managers in the world, spawning one.."),
			*FString(__FUNCTION__), __LINE__);
		FActorSpawnParameters Params;
		//Params.Name = FName(TEXT("SL_IndividualManager"));
		Manager = World->SpawnActor<ASLIndividualManager>(Params);
#if WITH_EDITOR
		Manager->SetActorLabel(TEXT("SL_IndividualManager"));
#endif // WITH_EDITOR
		World->MarkPackageDirty();
	}
	return Manager;
}

// Add individual components to all supported actors in the world
int32 FSLIndividualUtils::CreateIndividualComponents(UWorld* World)
{
	int32 Num = 0;
	for (TActorIterator<AActor> ActItr(World); ActItr; ++ActItr)
	{
		if (USLIndividualComponent* IC = AddNewIndividualComponent(*ActItr))
		{
			Num++;
		}
	}
	return Num;
}

// Add individual components to all supported actors from the selection
int32 FSLIndividualUtils::CreateIndividualComponents(const TArray<AActor*>& Actors)
{
	int32 Num = 0;
	for (const auto& Act : Actors)
	{
		if (USLIndividualComponent* IC = AddNewIndividualComponent(Act))
		{
			Num++;
		}
	}
	return Num;
}

// Destroy individual components of all actors in the world
int32 FSLIndividualUtils::DestroyIndividualComponents(UWorld* World)
{
	int32 Num = 0;
	for (TActorIterator<AActor> ActItr(World); ActItr; ++ActItr)
	{
		if (DestroyIndividualComponent(*ActItr))
		{
			Num++;
		}
	}
	return Num;
}

// Destroy individual components of the selected actors
int32 FSLIndividualUtils::DestroyIndividualComponents(const TArray<AActor*>& Actors)
{
	int32 Num = 0;
	for (const auto& Act : Actors)
	{
		if (DestroyIndividualComponent(Act))
		{
			Num++;
		}
	}
	return Num;
}

// Call init on all individual components in the world
int32 FSLIndividualUtils::InitIndividualComponents(UWorld* World, bool bReset)
{
	int32 Num = 0;
	for (TActorIterator<AActor> ActItr(World); ActItr; ++ActItr)
	{
		if (InitIndividualComponent(*ActItr, bReset))
		{
			Num++;
		}
	}
	return Num;
}

// Call init on selected individual components
int32 FSLIndividualUtils::InitIndividualComponents(const TArray<AActor*>& Actors, bool bReset)
{
	int32 Num = 0;
	for (const auto& Act : Actors)
	{
		if (InitIndividualComponent(Act, bReset))
		{
			Num++;
		}
	}
	return Num;
}

// Call load on all individual components in the world
int32 FSLIndividualUtils::LoadIndividualComponents(UWorld* World, bool bReset, bool bTryImport)
{
	int32 Num = 0;
	for (TActorIterator<AActor> ActItr(World); ActItr; ++ActItr)
	{
		if (LoadIndividualComponent(*ActItr, bReset, bTryImport))
		{
			Num++;
		}
	}
	return Num;
}

// Call load on selected individual components
int32 FSLIndividualUtils::LoadIndividualComponents(const TArray<AActor*>& Actors, bool bReset, bool bTryImport)
{
	int32 Num = 0;
	for (const auto& Act : Actors)
	{
		if (LoadIndividualComponent(Act, bReset, bTryImport))
		{
			Num++;
		}
	}
	return Num;
}

/* Functionalities */
// Call toggle mask visiblitiy on all individual components int the world
int32 FSLIndividualUtils::ToggleVisualMaskVisibility(UWorld* World, bool bPrioritizeChildren)
{
	int32 Num = 0;
	for (TActorIterator<AActor> ActItr(World); ActItr; ++ActItr)
	{
		if (ToggleVisualMaskVisibility(*ActItr, bPrioritizeChildren))
		{
			Num++;
		}
	}
	return Num;
}

// Call toggle mask visiblitiy on selected individual components
int32 FSLIndividualUtils::ToggleVisualMaskVisibility(const TArray<AActor*>& Actors, bool bPrioritizeChildren)
{
	int32 Num = 0;
	for (const auto& Act : Actors)
	{
		if (ToggleVisualMaskVisibility(Act, bPrioritizeChildren))
		{
			Num++;
		}
	}
	return Num;
}

/* Values */
/* Ids */
// Write ids for all individuals in the world
int32 FSLIndividualUtils::WriteIds(UWorld* World, bool bOverwrite)
{
	int32 Num = 0;
	for (TActorIterator<AActor> ActItr(World); ActItr; ++ActItr)
	{
		if (WriteId(*ActItr, bOverwrite))
		{
			Num++;
		}
	}
	return Num;
}

// Write ids for selected individuals
int32 FSLIndividualUtils::WriteIds(const TArray<AActor*>& Actors, bool bOverwrite)
{
	int32 Num = 0;
	for (const auto& Act : Actors)
	{
		if (WriteId(Act, bOverwrite))
		{
			Num++;
		}
	}
	return Num;
}

// Clear ids for all individuals in the world
int32 FSLIndividualUtils::ClearIds(UWorld* World)
{
	int32 Num = 0;
	for (TActorIterator<AActor> ActItr(World); ActItr; ++ActItr)
	{
		if (ClearId(*ActItr))
		{
			Num++;
		}
	}
	return Num;
}

// Clear ids for selected individuals
int32 FSLIndividualUtils::ClearIds(const TArray<AActor*>& Actors)
{
	int32 Num = 0;
	for (const auto& Act : Actors)
	{
		if (ClearId(Act))
		{
			Num++;
		}
	}
	return Num;
}

/* Classes */
// Write default class values to all individuals in the world
int32 FSLIndividualUtils::WriteClasses(UWorld* World, bool bOverwrite)
{
	int32 Num = 0;
	for (TActorIterator<AActor> ActItr(World); ActItr; ++ActItr)
	{
		if (WriteClass(*ActItr, bOverwrite))
		{
			Num++;
		}
	}
	return Num;
}

// Write default class values to selected individuals
int32 FSLIndividualUtils::WriteClasses(const TArray<AActor*>& Actors, bool bOverwrite)
{
	int32 Num = 0;
	for (const auto& Act : Actors)
	{
		if (WriteClass(Act, bOverwrite))
		{
			Num++;
		}
	}
	return Num;
}

// Clear class values of all individuals in the world
int32 FSLIndividualUtils::ClearClasses(UWorld* World)
{
	int32 Num = 0;
	for (TActorIterator<AActor> ActItr(World); ActItr; ++ActItr)
	{
		if (ClearClass(*ActItr))
		{
			Num++;
		}
	}
	return Num;
}

// Clear class values of all selected individulals
int32 FSLIndividualUtils::ClearClasses(const TArray<AActor*>& Actors)
{
	int32 Num = 0;
	for (const auto& Act : Actors)
	{
		if (ClearClass(Act))
		{
			Num++;
		}
	}
	return Num;
}

// Add unique masks for all the visual individuals
int32 FSLIndividualUtils::WriteUniqueVisualMasks(UWorld* World, bool bOverwrite)
{
	int32 Num = 0;
	TArray<FColor> ConsumedColors = GetAllConsumedVisualMaskColorsInWorld(World);
	for (TActorIterator<AActor> ActItr(World); ActItr; ++ActItr)
	{
		if (WriteUniqueVisualMask(*ActItr, ConsumedColors, bOverwrite))
		{
			Num++;
		}
	}
	return Num;
}

// Add unique masks for selected individuals by checking against the values in the world
int32 FSLIndividualUtils::WriteUniqueVisualMasks(const TArray<AActor*>& Actors, bool bOverwrite)
{
	int32 Num = 0;
	if (Actors.Num())
	{	
		TArray<FColor> ConsumedColors = GetAllConsumedVisualMaskColorsInWorld(Actors[0]->GetWorld());
		for (const auto& Act : Actors)
		{
			if (WriteUniqueVisualMask(Act, ConsumedColors, bOverwrite))
			{
				Num++;
			}
		}
	}
	return Num;
}

// Clear all visual mask values
int32 FSLIndividualUtils::ClearVisualMasks(UWorld* World)
{
	int32 Num = 0;
	for (TActorIterator<AActor> ActItr(World); ActItr; ++ActItr)
	{
		if (ClearVisualMask(*ActItr))
		{
			Num++;
		}
	}
	return Num;
}

// Clear selection visual mask values
int32 FSLIndividualUtils::ClearVisualMasks(const TArray<AActor*>& Actors)
{
	int32 Num = 0;
	if (Actors.Num())
	{
		for (const auto& Act : Actors)
		{
			if (ClearVisualMask(Act))
			{
				Num++;
			}
		}
	}
	return Num;
}


///* Visual mask */
//// Write unique visual masks for all visual individuals in the world
//int32 FSLIndividualUtils::WriteVisualMasks(const TSet<USLIndividualComponent*>& IndividualComponents, bool bOverwrite)
//{
//	int32 NumNewMasks = 0;
//	TArray<FColor> ConsumedColors = GetConsumedVisualMaskColors(IndividualComponents);
//	for (const auto& IC : IndividualComponents)
//	{
//		//if (USLPerceivableIndividual* VI = IC->GetCastedIndividualObject<USLPerceivableIndividual>())
//		//{
//		//	if (AddVisualMask(VI, ConsumedColors, bOverwrite))
//		//	{
//		//		NumNewMasks++;
//		//	}
//		//}
//
//		// TODO check for children
//	}
//	return NumNewMasks;
//}
//
//// Write unique visual masks for visual individuals from the actos in the array
//int32 FSLIndividualUtils::WriteVisualMasks(const TSet<USLIndividualComponent*>& IndividualComponentsSelection,
//	const TSet<USLIndividualComponent*>& RegisteredIndividualComponents,
//	bool bOverwrite)
//{
//	int32 NumNewMasks = 0;
//	TArray<FColor> ConsumedColors = GetConsumedVisualMaskColors(RegisteredIndividualComponents);
//	for (const auto& IC : IndividualComponentsSelection)
//	{
//		//if (USLPerceivableIndividual* VI = IC->GetCastedIndividualObject<USLPerceivableIndividual>())
//		//{
//		//	if (AddVisualMask(VI, ConsumedColors, bOverwrite))
//		//	{
//		//		NumNewMasks++;
//		//	}
//		//}
//		//
//		//// TODO check for children
//		//if (USLSkeletalIndividual* SkI = IC->GetCastedIndividualObject<USLSkeletalIndividual>())
//		//{
//
//		//}
//	}
//	return NumNewMasks;
//}
//
//bool FSLIndividualUtils::ClearVisualMask(USLIndividualComponent* IndividualComponent)
//{
//	//if (USLPerceivableIndividual* SI = IndividualComponent->GetCastedIndividualObject<USLPerceivableIndividual>())
//	//{
//	//	SI->ClearVisualMaskValue();
//	//	return true;
//	//}
//
//	// TODO skeletal
//	// TODO robot
//
//	return false;
//}


/* Private: Individuals */
// Create and add new individual component
USLIndividualComponent* FSLIndividualUtils::AddNewIndividualComponent(AActor* Actor, bool bTryInitAndLoad)
{
	// Check if the actor type is supported and there is no other existing component
	if (CanHaveIndividualComponent(Actor) && !HasIndividualComponent(Actor))
	{
		Actor->Modify();

		// Create an appropriate name for the new component (avoid duplicates)
		FName NewComponentName = *FComponentEditorUtils::GenerateValidVariableName(
			USLIndividualComponent::StaticClass(), Actor);

		// Get the set of owned components that exists prior to instancing the new component.
		TInlineComponentArray<UActorComponent*> PreInstanceComponents;
		Actor->GetComponents(PreInstanceComponents);

		// Create a new component
		USLIndividualComponent* NewComp = NewObject<USLIndividualComponent>(Actor, NewComponentName, RF_Transactional);

		// Make visible in the components list in the editor
		Actor->AddInstanceComponent(NewComp);
		//Actor->AddOwnedComponent(NewComp);

		//NewComp->OnComponentCreated();
		NewComp->RegisterComponent();

		// Register any new components that may have been created during construction of the instanced component, but were not explicitly registered.
		TInlineComponentArray<UActorComponent*> PostInstanceComponents;
		Actor->GetComponents(PostInstanceComponents);
		for (UActorComponent* ActorComponent : PostInstanceComponents)
		{
			if (!ActorComponent->IsRegistered() && ActorComponent->bAutoRegister && !ActorComponent->IsPendingKill() && !PreInstanceComponents.Contains(ActorComponent))
			{
				ActorComponent->RegisterComponent();
			}
		}

		Actor->RerunConstructionScripts();

		/* Try initializing and loading the components right after createion (this will not work for all individuals) */
		if (bTryInitAndLoad)
		{
			if (!NewComp->Init(true))
			{
				UE_LOG(LogTemp, Warning, TEXT("%s::%d Individual component %s could not be init right after creating it.. "),
					*FString(__FUNCTION__), __LINE__, *NewComp->GetFullName());				
			}
			else
			{
				if (!NewComp->Load(true, true))
				{
					UE_LOG(LogTemp, Warning, TEXT("%s::%d Individual component %s could not be loaded right after creating it.. "),
						*FString(__FUNCTION__), __LINE__, *NewComp->GetFullName());
				}
			}
		}
		return NewComp;
	}
	return nullptr;
}

// Check if actor supports individual components
bool FSLIndividualUtils::CanHaveIndividualComponent(AActor* Actor)
{
	return Actor->IsA(AStaticMeshActor::StaticClass())
		|| Actor->IsA(ASkeletalMeshActor::StaticClass())
		|| Actor->IsA(APhysicsConstraintActor::StaticClass())
		|| Actor->IsA(AAtmosphericFog::StaticClass())
		|| Actor->IsA(ASLVirtualCameraView::StaticClass())
		|| Actor->GetName().Contains("SkySphere");
}

// Check if actor already has an individual component
bool FSLIndividualUtils::HasIndividualComponent(AActor* Actor)
{
	return Actor->GetComponentByClass(USLIndividualComponent::StaticClass()) != nullptr;
}

// Destroy individual component of the actor
bool FSLIndividualUtils::DestroyIndividualComponent(AActor* Actor)
{
	if (UActorComponent* AC = Actor->GetComponentByClass(USLIndividualComponent::StaticClass()))
	{
		Actor->Modify();
		Actor->RemoveInstanceComponent(AC);
		AC->ConditionalBeginDestroy();
		return true;
	}
	return false;
}

// Call init on the individual component of the actor
bool FSLIndividualUtils::InitIndividualComponent(AActor* Actor, bool bReset)
{
	if (UActorComponent* AC = Actor->GetComponentByClass(USLIndividualComponent::StaticClass()))
	{
		USLIndividualComponent* IC = CastChecked<USLIndividualComponent>(AC);
		return IC->Init(bReset);
	}
	return false;
}

// Call load on the individual component of the actor
bool FSLIndividualUtils::LoadIndividualComponent(AActor* Actor, bool bReset, bool bTryImport)
{
	if (UActorComponent* AC = Actor->GetComponentByClass(USLIndividualComponent::StaticClass()))
	{
		USLIndividualComponent* IC = CastChecked<USLIndividualComponent>(AC);
		return IC->Load(bReset, bTryImport);
	}
	return false;
}

/* Individuals functionalities Private */
bool FSLIndividualUtils::ToggleVisualMaskVisibility(AActor* Actor, bool bPrioritizeChildren)
{
	if (UActorComponent* AC = Actor->GetComponentByClass(USLIndividualComponent::StaticClass()))
	{
		USLIndividualComponent* IC = CastChecked<USLIndividualComponent>(AC);
		return IC->ToggleVisualMaskVisibility(bPrioritizeChildren);
	}
	return false;
}


/* Private: Individual values */
/* Ids */
// Write unqiue identifier for the individual
bool FSLIndividualUtils::WriteId(AActor* Actor, bool bOverwrite)
{
	if (UActorComponent* AC = Actor->GetComponentByClass(USLIndividualComponent::StaticClass()))
	{
		USLIndividualComponent* IC = CastChecked<USLIndividualComponent>(AC);		
		return IC->WriteId(bOverwrite);
	}
	return false;
}

// Clear unqiue identifier of the individual
bool FSLIndividualUtils::ClearId(AActor* Actor)
{
	if (UActorComponent* AC = Actor->GetComponentByClass(USLIndividualComponent::StaticClass()))
	{
		USLIndividualComponent* IC = CastChecked<USLIndividualComponent>(AC);
		return IC->ClearId();
	}
	return false;
}

/* Class */
// Write default class value
bool FSLIndividualUtils::WriteClass(AActor* Actor, bool bOverwrite)
{
	if (UActorComponent* AC = Actor->GetComponentByClass(USLIndividualComponent::StaticClass()))
	{
		USLIndividualComponent* IC = CastChecked<USLIndividualComponent>(AC);
		return IC->WriteClass(bOverwrite);
	}
	return false;
}

// Clear class value
bool FSLIndividualUtils::ClearClass(AActor* Actor)
{
	if (UActorComponent* AC = Actor->GetComponentByClass(USLIndividualComponent::StaticClass()))
	{
		USLIndividualComponent* IC = CastChecked<USLIndividualComponent>(AC);
		return IC->ClearClass();
	}
	return false;
}

/* Visual Mask */
// Add unique visual mask color (colors if it has children) to the individual of the actor
bool FSLIndividualUtils::WriteUniqueVisualMask(AActor* Actor, TArray<FColor>& ConsumedColors, bool bOverwrite)
{
	static const int32 NumTrials = 255;
	static const int32 MinManhattanDist = 17;

	if (UActorComponent* AC = Actor->GetComponentByClass(USLIndividualComponent::StaticClass()))
	{
		USLIndividualComponent* IC = CastChecked<USLIndividualComponent>(AC);
		if (USLPerceivableIndividual* VI = IC->GetCastedIndividualObject<USLPerceivableIndividual>())
		{
			bool bRetVal = false;
			if (!VI->IsVisualMaskValueSet() || bOverwrite)
			{
				FColor NewUniqueColor = GenerateRandomUniqueColor(ConsumedColors, NumTrials, MinManhattanDist);
				if (NewUniqueColor != FColor::Black)
				{
					VI->SetVisualMaskValue(NewUniqueColor.ToHex());
					bRetVal = true;
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("%s::%d Could not generate a new unique visual mask for %s .."),
						*FString(__func__), __LINE__, *Actor->GetName());
				}
			}

			if (USLSkeletalIndividual* SkI = Cast<USLSkeletalIndividual>(VI))
			{
				for (const auto& BI : SkI->GetBoneIndividuals())
				{
					if (!BI->IsVisualMaskValueSet() || bOverwrite)
					{
						FColor NewUniqueColor = GenerateRandomUniqueColor(ConsumedColors, NumTrials, MinManhattanDist);
						if (NewUniqueColor != FColor::Black)
						{
							BI->SetVisualMaskValue(NewUniqueColor.ToHex());
							bRetVal = true;
						}
						else
						{
							UE_LOG(LogTemp, Error, TEXT("%s::%d Could not generate a new unique visual mask for %s's bone %s .."),
								*FString(__func__), __LINE__, *Actor->GetName(), *BI->GetFullName());
						}
					}
				}
			}

			// TODO robot
			return bRetVal;
		}
	}
	return false;
}

// Clear visual mask of the actor (children as well if any)
bool FSLIndividualUtils::ClearVisualMask(AActor* Actor)
{
	if (UActorComponent* AC = Actor->GetComponentByClass(USLIndividualComponent::StaticClass()))
	{
		USLIndividualComponent* IC = CastChecked<USLIndividualComponent>(AC);
		return IC->ClearVisualMask();
	}
	return false;
}

/* Visual Mask Helpers */
// Get an array of all used visual mask colors in the world
TArray<FColor> FSLIndividualUtils::GetAllConsumedVisualMaskColorsInWorld(UWorld* World)
{
	TArray<FColor> ConsumedMaskColors;
	for (TActorIterator<AActor> ActItr(World); ActItr; ++ActItr)
	{
		if (UActorComponent* AC = ActItr->GetComponentByClass(USLIndividualComponent::StaticClass()))
		{
			USLIndividualComponent* IC = CastChecked<USLIndividualComponent>(AC);
			if (USLPerceivableIndividual* VI = Cast<USLPerceivableIndividual>(IC->GetIndividualObject()))
			{
				if (VI->IsVisualMaskValueSet())
				{
					ConsumedMaskColors.Add(FColor::FromHex(VI->GetVisualMaskValue()));
				}

				// Add bone values if skeletal
				if (USLSkeletalIndividual* SkI = Cast<USLSkeletalIndividual>(VI))
				{
					for (const auto* BI : SkI->GetBoneIndividuals())
					{
						if (BI->IsVisualMaskValueSet())
						{
							ConsumedMaskColors.Add(FColor::FromHex(BI->GetVisualMaskValue()));
						}
					}
				}

				// TODO robot individual
			}
		}
	}
	return ConsumedMaskColors;
}

// Generate random colors until a unique one if found (return black if failed)
FColor FSLIndividualUtils::GenerateRandomUniqueColor(TArray<FColor>& ConsumedColors, int32 NumTrials, int32 MinManhattanDist)
{
	// Avoid colors close to black or white
	static const uint8 MinDistToBlack = 23;
	static const uint8 MinDistToWhite = 23;

	for (int32 TrialIdx = 0; TrialIdx < NumTrials; ++TrialIdx)
	{
		// Generate a random color that differs of black
		//FColor RC = FColor::MakeRandomColor(); // Pretty colors, but does not generate many options
		FColor RandColor = CreateRandomRGBColor();

		// Avoid very dark or very bright colors
		if (AreColorsAlmostEqual(RandColor, FColor::Black, MinDistToBlack) ||
			AreColorsAlmostEqual(RandColor, FColor::White, MinDistToWhite))
		{
			//UE_LOG(LogTemp, Error, TEXT("%s::%d Got a very dark or very bright (reserved) color, hex=%s, trying again.."),
			//	*FString(__func__), __LINE__, *RandColor.ToHex());
			continue;
		}

		/* Nested lambda for the array FindByPredicate functionality */
		const auto EqualWithToleranceLambda = [RandColor, MinManhattanDist](const FColor& Item)
		{
			return AreColorsAlmostEqual(RandColor, Item, MinManhattanDist);
		};

		// Check that the randomly generated color is not in the consumed color array
		if (!ConsumedColors.FindByPredicate(EqualWithToleranceLambda))
		{
			ConsumedColors.Emplace(RandColor);
			return RandColor;
		}
	}
	//UE_LOG(LogTemp, Error, TEXT("%s::%d Could not generate a unique color for, saving as black.."), *FString(__func__), __LINE__);
	return FColor::Black;
}


///* Private helpers */
//// Add visual mask
//bool FSLIndividualUtils::AddVisualMask(USLPerceivableIndividual* Individual, TArray<FColor>& ConsumedColors, bool bOverwrite)
//{
//	static const int32 NumTrials = 100;
//	static const int32 MinManhattanDist = 29;
//
//	if (!Individual->IsVisualMaskValueSet())
//	{
//		// Generate new color
//		FColor NewColor = CreateNewUniqueColorRand(ConsumedColors, NumTrials, MinManhattanDist);
//		if (NewColor == FColor::Black)
//		{
//			UE_LOG(LogTemp, Error, TEXT("%s::%d Could not generate a new visual mask for %s .."),
//				*FString(__func__), __LINE__, *Individual->GetOuter()->GetName());
//			return false;
//		}
//		else
//		{
//			Individual->SetVisualMaskValue(NewColor.ToHex());
//			return true;
//		}
//	}
//	else if(bOverwrite)
//	{
//		// Remove previous color from the consumed array
//		int32 ConsumedColorIdx = ConsumedColors.Find(FColor::FromHex(Individual->GetVisualMaskValue()));
//		if (ConsumedColorIdx != INDEX_NONE)
//		{
//			ConsumedColors.RemoveAt(ConsumedColorIdx);
//		}
//		else
//		{
//			UE_LOG(LogTemp, Error, TEXT("%s::%d To be overwritten color of %s is not in the consumed colors array, this should not happen  .."),
//				*FString(__func__), __LINE__, *Individual->GetOuter()->GetName());
//		}
//
//		// Generate new color
//		FColor NewColor = CreateNewUniqueColorRand(ConsumedColors, NumTrials, MinManhattanDist);
//		if (NewColor == FColor::Black)
//		{
//			UE_LOG(LogTemp, Error, TEXT("%s::%d Could not generate a new visual mask for %s .."),
//				*FString(__func__), __LINE__, *Individual->GetOuter()->GetName());
//			return false;
//		}
//		else
//		{
//			Individual->SetVisualMaskValue(NewColor.ToHex());
//			return true;
//		}
//	}
//	return false;
//}

///* Private */
///* Visual mask generation */
//// Get all used up visual masks in the world
//TArray<FColor> FSLIndividualUtils::GetConsumedVisualMaskColors(const TSet<USLIndividualComponent*>& IndividualComponents)
//{
//	TArray<FColor> ConsumedColors;
//
//	/* Static mesh actors */
//	for (const auto& IC : IndividualComponents)
//	{
//		//if (USLPerceivableIndividual* VI = IC->GetCastedIndividualObject<USLPerceivableIndividual>())
//		//{
//		//	if (VI->IsVisualMaskValueSet())
//		//	{
//		//		ConsumedColors.Add(FColor::FromHex(VI->GetVisualMaskValue()));
//		//	}
//		//}
//	}
//
//	/* Skeletal mesh actors */
//	// TODO
//
//	/* Robot actors */
//	// TODO
//
//	return ConsumedColors;
//}
//
//// Create a new unique color by randomization
//FColor FSLIndividualUtils::CreateNewUniqueColorRand(TArray<FColor>& ConsumedColors, int32 NumTrials, int32 MinManhattanDist)
//{
//	// Constants
//	static const uint8 MinDistToBlack = 37;
//	static const uint8 MinDistToWhite = 23;
//
//	for (int32 TrialIdx = 0; TrialIdx < NumTrials; ++TrialIdx)
//	{
//		// Generate a random color that differs of black
//		//FColor RC = FColor::MakeRandomColor(); // Pretty colors, but not many
//		FColor RandColor = CreateRandomRGBColor();
//
//		// Avoid very dark or very bright colors
//		if (AreColorsAlmostEqual(RandColor, FColor::Black, MinDistToBlack) || 
//			AreColorsAlmostEqual(RandColor, FColor::White, MinDistToWhite))
//		{
//			UE_LOG(LogTemp, Error, TEXT("%s::%d Got a very dark or very bright (reserved) color, hex=%s, trying again.."),
//				*FString(__func__), __LINE__, *RandColor.ToHex());
//			continue;
//		}
//
//		/* Nested lambda for the array FindByPredicate functionality */
//		const auto EqualWithToleranceLambda = [RandColor, MinManhattanDist](const FColor& Item)
//		{
//			return AreColorsAlmostEqual(RandColor, Item, MinManhattanDist);
//		};
//
//		// Check that the randomly generated color is not in the consumed color array
//		if (!ConsumedColors.FindByPredicate(EqualWithToleranceLambda))
//		{
//			ConsumedColors.Emplace(RandColor);
//			return RandColor;
//		}
//	}
//	UE_LOG(LogTemp, Error, TEXT("%s::%d Could not generate a unique color, saving as black.."), *FString(__func__), __LINE__);
//	return FColor::Black;
//}
//
