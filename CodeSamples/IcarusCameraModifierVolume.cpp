// Fill out your copyright notice in the Description page of Project Settings.

#include "IcarusCameraModifierVolume.h"
#include "IcarusPlayerCharacter.h"
#include "Movement/IcarusCharacterMovement.h"
#include "Movement/IcarusCharacterMovement_Swim.h"
#include "Movement/IcarusCharacterMovement_Ground.h"
#include "Icarus.h"

// SET_BLOCK_CLASS_FLOW_VAR(AIcarusCameraModifierVolume, BeginOverlap, true);
// SET_BLOCK_CLASS_FLOW_VAR(AIcarusCameraModifierVolume, EndOverlap, true);
bool AIcarusCameraModifierVolume::bOverlaping = false;
bool AIcarusCameraModifierVolume::bKeepInWorld_Internal = false;

AIcarusCameraModifierVolume::AIcarusCameraModifierVolume()
{
	GetBrushComponent()->SetCollisionProfileName("StaticOverlapPawn");
	GetBrushComponent()->OnComponentBeginOverlap.AddUniqueDynamic(this, &AIcarusCameraModifierVolume::BeginOverlap);
	GetBrushComponent()->OnComponentEndOverlap.AddUniqueDynamic(this, &AIcarusCameraModifierVolume::EndOverlap);

	SET_BLOCK_FLOW_VAR(AIcarusCameraModifierVolume, BeginOverlap, false);
	SET_BLOCK_FLOW_VAR(AIcarusCameraModifierVolume, EndOverlap, true);
}

void AIcarusCameraModifierVolume::BeginPlay()
{
	bKeepInWorld_Internal = false;

	// SUper
	Super::BeginPlay();
}

void AIcarusCameraModifierVolume::BeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult)
{
	// Verifica clear
	if (bClearKeepInWorld)
	{
		bKeepInWorld_Internal = false;
	}

	// Se keep in world internal for true
	if (bKeepInWorld_Internal)
	{
		ActorClientWarningMessage("bKeepInWorld is set, this overlap will not execute.");
		return;
	}

	AIcarusPlayerCharacter * IcarusCharacter = Cast<AIcarusPlayerCharacter>(UIcarusSystemLibrary::CheckOverlappingPlayer(OtherActor, OtherComp));
	if (IS_VALID(IcarusCharacter))
	{
		UIcarusCameraComponent * IcarusCameraComponent = IcarusCharacter->Camera;
		if (IS_VALID(IcarusCameraComponent))
		{
			if (!GET_BLOCK_FLOW_VAR(AIcarusCameraModifierVolume, BeginOverlap))
			{
				// Se for para sobrescrever
				if (bOverrideSettings)
				{
					// Se for para focar em player ao atualizar camera point
					if (IcarusSettings.CameraPoint_Actor)
					{
						IcarusCameraComponent->CalculateIntermediateCameraPoint(IcarusSettings, this);
					}

					// Pega o ultimo volume adicionado
					AIcarusCameraModifierVolume * LastVolumeAdded = nullptr;
					if (IcarusCameraComponent->ModifierVolumes.Num())
					{
						LastVolumeAdded = IcarusCameraComponent->ModifierVolumes.Last();
					}

					// Adiciona na arrayde volumes
					IcarusCameraComponent->ModifierVolumes.Add(this);

					// Se volume anterior existir.
					if (IS_VALID(LastVolumeAdded))
					{
						if (Priority_ToMultiOverlap > LastVolumeAdded->Priority_ToMultiOverlap)
						{
							IcarusCameraComponent->CurrentModifierVolume = this;
							IcarusCameraComponent->SetTransientSettings(IcarusSettings, false);
						}
					}
					else
					{
						IcarusCameraComponent->CurrentModifierVolume = this;
						IcarusCameraComponent->SetTransientSettings(IcarusSettings, false);
					}
				}
				if (bOverrideFocusSystemSettings)
				{
					IcarusCameraComponent->VolumeTransientFocusSystemSettings.Add(IcarusFocusSettings);
					IcarusCameraComponent->SetTransientFocusSettings(IcarusFocusSettings, false);
				}

				// Se actor new settings ou location
				if (IcarusFocusSettings.Location != FVector::ZeroVector || IS_VALID(IcarusFocusSettings.Actor))
				{
					IcarusCameraComponent->StartFocus(IcarusFocusSettings.Actor);
				}

				// Seta overlapando
				bOverlaping = true;

				RESET_DO_ONCE(AIcarusCameraModifierVolume, EndOverlap);
			}
		}
	}

	// Seta keep internal
	bKeepInWorld_Internal = bKeepInWorld;
}

void AIcarusCameraModifierVolume::EndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// Se keep in world internal for true
	if (bKeepInWorld_Internal)
	{
		return;
	}

	AIcarusPlayerCharacter * IcarusCharacter = UIcarusSystemLibrary::CheckOverlappingPlayer(OtherActor, OtherComp);
	if (IS_VALID(IcarusCharacter))
	{
		UIcarusCameraComponent * IcarusCameraComponent = IcarusCharacter->Camera;
		if (IS_VALID(IcarusCameraComponent))
		{
			if(!GET_BLOCK_FLOW_VAR(AIcarusCameraModifierVolume, EndOverlap))
			{
				// Se existir actor em settings.
				if (IS_VALID(IcarusSettings.CameraPoint_Actor))
				{
					UIcarusSystemLibrary::IgnorePlayerCamera(this, false);
				}

				// Desseta focus se estiver focado e se valores para focus sao validos
				if (IcarusCameraComponent->IsFocusing() &&
					(IS_VALID(IcarusFocusSettings.Actor) || IcarusFocusSettings.Location != FVector::ZeroVector))
				{
					IcarusCameraComponent->StopFocus();
				}

				// Adiciona na arrayde volumes
				IcarusCameraComponent->ModifierVolumes.Remove(this);

				// Se for para sobrescrever
				if (bOverrideSettings)
				{
					IcarusCameraComponent->ResetTransientSettings(false);
				}
				if (bOverrideFocusSystemSettings)
				{
					// Reseta focus settings transient
					IcarusCameraComponent->VolumeTransientFocusSystemSettings.RemoveAt(LAST_INDEX(IcarusCameraComponent->VolumeTransientFocusSystemSettings));
					IcarusCameraComponent->ResetTransientFocusSettings(false);
				}

				// Deseta overlapando
				bOverlaping = false;

				RESET_DO_ONCE(AIcarusCameraModifierVolume, BeginOverlap);
			}
		}
	}
}