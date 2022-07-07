// Fill out your copyright notice in the Description page of Project Settings.

#include "IcarusCharacterMovement_Base.h"
#include "IcarusCharacterMovement.h"
#include "Player/IcarusPlayerCharacter.h"
#include "Player/IcarusPlayerController.h"
#include "Icarus.h"

UIcarusCharacterMovement_Base::UIcarusCharacterMovement_Base()
{
	bActive = true;
}

void UIcarusCharacterMovement_Base::Initialize()
{
	// Pega outer convertido.
	IcarusCharacterMovement = Cast<UIcarusCharacterMovement>(GetOuter());
	check(IcarusCharacterMovement);

	// Pega icarus player character
	IcarusPlayerCharacter = Cast<AIcarusPlayerCharacter>(IcarusCharacterMovement->GetOwner());
	check(IcarusPlayerCharacter);

	// Pega icarus player controller
	IcarusPlayerController = Cast<AIcarusPlayerController>(IcarusPlayerCharacter->GetController());
	check(IcarusPlayerController);
}

bool UIcarusCharacterMovement_Base::SetMovementState(EMovementState NewState, bool bForce)
{
	CHECK_WITH_MESSAGE_WITH_RETURN(bForce, FString::Printf(TEXT("%s::SetMovementState: so pode ser chamada por %s."), *GetName(), *IcarusCharacterMovement->GetName()), false);
	return bActive;
}

void UIcarusCharacterMovement_Base::Update(float DeltaTime)
{
	if (bActive)
	{
		Update_Internal(DeltaTime);
	}
}
