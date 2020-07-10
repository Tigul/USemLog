// Copyright 2017-2020, Institute for Artificial Intelligence - University of Bremen
// Author: Andrei Haidu (http://haidu.eu)

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Individuals/SLPerceivableIndividual.h"
#include "SLBoneIndividual.generated.h"

// Forward declaration
class USkeletalMeshComponent;
class USLSkeletalDataComponent;

/**
 *
 */
UCLASS(ClassGroup = SL)
class USEMLOG_API USLBoneIndividual : public USLPerceivableIndividual
{
    GENERATED_BODY()
public:
    // Ctor
    USLBoneIndividual();

    // Called before destroying the object.
    virtual void BeginDestroy() override;

    // Called after the C++ constructor and after the properties have been initialized, including those loaded from config.
    virtual void PostInitProperties() override;

    // Set the parameters required when initalizing the individual
    bool PreInit(int32 NewBoneIndex, bool bReset = false);

    // Check if the individual is pre initalized
    bool IsPreInit() const { return bIsPreInit; };

    // Init asset references (bForced forces re-initialization)
    virtual bool Init(bool bReset = false);

    // Load semantic data (bForced forces re-loading)
    virtual bool Load(bool bReset = false, bool bTryImport = false);

    //// Save values externally
    //virtual bool ExportValues(bool bOverwrite = false);

    //// Load values externally
    //virtual bool ImportValues(bool bOverwrite = false);


    // Get the type name as string
    virtual FString GetTypeName() const override { return FString("BoneIndividual"); };

    /* Begin Perceivable individual interface */
    // Apply visual mask material
    virtual bool ApplyMaskMaterials(bool bPrioritizeChildren = false) override;

    // Apply original materials
    virtual bool ApplyOriginalMaterials() override;
    /* End Perceivable individual interface */

    // Calculate the current bone transform
    bool CacheCurrentBoneTransform();

    // Get the cached bone transform
    FTransform GetCachedTranform() const { return CachedTransform; };

protected:
    // Get class name, virtual since each invidiual type will have different name
    virtual FString CalcDefaultClassValue() const override;

    // Clear all values of the individual
    virtual void InitReset() override;

    // Clear all data of the individual
    virtual void LoadReset() override;

    // Clear any bound delegates (called when init is reset)
    virtual void ClearDelegateBounds() override;

    // Set pointer to parent actor
    virtual bool SetParentActor() override;

    // Check if skeleltal bone description component is available
    bool HasValidSkeletalDataComponent() const;

    // Check if the material index is valid
    bool HasValidMaterialIndex() const;

    // Check if the bone index is valid
    bool HasValidBoneIndex() const;

private:
    // Set dependencies
    bool InitImpl();

    // Set data
    bool LoadImpl(bool bTryImport = true);

    // Check if the static mesh component is set
    bool HasValidSkeletalMesh() const;

    // Set sekeletal mesh
    bool SetSkeletalMesh();

protected:
    // Pre init
    UPROPERTY(VisibleAnywhere, Category = "SL")
    bool bIsPreInit;

    // Mask material index
    UPROPERTY(VisibleAnywhere, Category = "SL")
    int32 MaterialIndex;

    // Bone index
    UPROPERTY(VisibleAnywhere, Category = "SL")
    int32 BoneIndex;

    // Parent skeletal mesh
    UPROPERTY()
    USkeletalMeshComponent* SkeletalMeshComponent;

    // Parent skeletal mesh data
    UPROPERTY()
    USLSkeletalDataComponent* SkelDataComp;

    // Cached transform
    FTransform CachedTransform;


    //// FName of the bone
    //UPROPERTY()
    //FName BoneName;

    //// Cached transform of the bone
    //FVector CachedLocation;

    //// Cached transform of the bone
    //FVector CachedQuat;
};
