// Copyright 2017-2020, Institute for Artificial Intelligence - University of Bremen
// Author: Andrei Haidu (http://haidu.eu)

#include "Individuals/SLConstraintIndividual.h"
#include "PhysicsEngine/PhysicsConstraintActor.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"

// Utils 
#include "Individuals/SLIndividualUtils.h"

// Ctor
USLConstraintIndividual::USLConstraintIndividual()
{
	ConstraintActor1 = nullptr;
	ConstraintIndividual1 = nullptr;
	ConstraintActor2 = nullptr;
	ConstraintIndividual2 = nullptr;
}

// Called before destroying the object.
void USLConstraintIndividual::BeginDestroy()
{
	SetIsInit(false);
	Super::BeginDestroy();
}

// Create and set the dynamic material, the owners visual component
void USLConstraintIndividual::PostInitProperties()
{
	Super::PostInitProperties();
	//Init();
}

// Set pointer to the semantic owner
bool USLConstraintIndividual::Init(bool bReset)
{
	if (bReset)
	{
		InitReset();
	}

	if (IsInit())
	{
		return true;
	}

	SetIsInit(Super::Init() && InitImpl());
	return IsInit();
}

// Load semantic data
bool USLConstraintIndividual::Load(bool bReset, bool bTryImport)
{
	if (bReset)
	{
		LoadReset();
	}

	if (IsLoaded())
	{
		return true;
	}

	if (!IsInit())
	{
		if (!Init(bReset))
		{
			UE_LOG(LogTemp, Log, TEXT("%s::%d Cannot load component individual %s, init fails.."),
				*FString(__FUNCTION__), __LINE__, *GetFullName());
			return false;
		}
	}

	SetIsLoaded(Super::Load() && LoadImpl(bTryImport));
	return IsLoaded();
}

// Get class name, virtual since each invidiual type will have different name
FString USLConstraintIndividual::CalcDefaultClassValue() const
{
	if (IsInit())
	{
		if (APhysicsConstraintActor* PCA = Cast<APhysicsConstraintActor>(ParentActor))
		{
			if (UPhysicsConstraintComponent* PCC = PCA->GetConstraintComp())
			{
				if (PCC->ConstraintInstance.GetLinearXMotion() != ELinearConstraintMotion::LCM_Locked ||
					PCC->ConstraintInstance.GetLinearYMotion() != ELinearConstraintMotion::LCM_Locked ||
					PCC->ConstraintInstance.GetLinearZMotion() != ELinearConstraintMotion::LCM_Locked)
				{
					return "LinearJoint";
				}
				else if (PCC->ConstraintInstance.GetAngularSwing1Motion() != EAngularConstraintMotion::ACM_Locked ||
					PCC->ConstraintInstance.GetAngularSwing2Motion() != EAngularConstraintMotion::ACM_Locked ||
					PCC->ConstraintInstance.GetAngularTwistMotion() != EAngularConstraintMotion::ACM_Locked)
				{
					return "RevoluteJoint";
				}
				else
				{
					return "FixedJoint";
				}
			}
		}
	}
	return GetTypeName();
}

// Clear all values of the individual
void USLConstraintIndividual::InitReset()
{
	ConstraintActor1 = nullptr;
	ConstraintActor2 = nullptr;
	ConstraintIndividual1 = nullptr;
	ConstraintIndividual2 = nullptr;
	SetIsInit(false);
	ClearDelegates();
	Super::InitReset();
}

// Clear all data of the individual
void USLConstraintIndividual::LoadReset()
{
	Super::LoadReset();
}

// Clear any bound delegates (called when init is reset)
void USLConstraintIndividual::ClearDelegates()
{
	Super::ClearDelegates();
}

// Private init implementation
bool USLConstraintIndividual::InitImpl()
{
	if (HasValidConstaintEntities())
	{
		return true;
	}

	if (APhysicsConstraintActor* PCA = Cast<APhysicsConstraintActor>(ParentActor))
	{
		if (UPhysicsConstraintComponent* PCC = PCA->GetConstraintComp())
		{
			/* Actor and individual 1 */
			if (PCC->ConstraintActor1)
			{
				ConstraintActor1 = PCC->ConstraintActor1;
				if (USLBaseIndividual* BI1 = FSLIndividualUtils::GetIndividualObject(ConstraintActor1))
				{
					ConstraintIndividual1 = BI1;
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("%s::%d %s constraint components ConstraintActor1 does not have an individual, init failed.."),
						*FString(__FUNCTION__), __LINE__, *GetFullName());
					return false;
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("%s::%d %s constraint components ConstraintActor1 not set, init failed.."),
					*FString(__FUNCTION__), __LINE__, *GetFullName());
				return false;
			}

			/* Actor and individual 2 */
			if (PCC->ConstraintActor2)
			{
				ConstraintActor2 = PCC->ConstraintActor2;
				if (USLBaseIndividual* BI2 = FSLIndividualUtils::GetIndividualObject(ConstraintActor2))
				{
					ConstraintIndividual2 = BI2;
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("%s::%d %s constraint components ConstraintActor2 does not have an individual, init failed.."),
						*FString(__FUNCTION__), __LINE__, *GetFullName());
					return false;
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("%s::%d %s constraint components ConstraintActor2 not set, init failed.."),
					*FString(__FUNCTION__), __LINE__, *GetFullName());
				return false;
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("%s::%d %s parent actor is not a physics constraint actor, this should not happen, init failed.."),
			*FString(__FUNCTION__), __LINE__, *GetFullName());
		return false;
	}

	return HasValidConstaintEntities();
}

// Private load implementation
bool USLConstraintIndividual::LoadImpl(bool bTryImport)
{
#if WITH_EDITORONLY_DATA
	ParentActor->SpriteScale = 0.4;
	ParentActor->MarkComponentsRenderStateDirty();
#endif // WITH_EDITORONLY_DATA
	return true;
}
