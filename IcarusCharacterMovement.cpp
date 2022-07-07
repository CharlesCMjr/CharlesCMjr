// Fill out your copyright notice in the Description page of Project Settings.

#include "IcarusCharacterMovement.h"
#include "IcarusPlayerCharacter.h"
#include "IcarusPlayerController.h"
#include "Camera/IcarusCameraComponent.h"
#include "Interaction/IcarusInteractionManager.h"
#include "Interaction/IcarusInteractionActor.h"
#include "Climb/IcarusLedgeManipulation.h"
#include "IcarusCharacterMovement_Air.h"
#include "IcarusCharacterMovement_Climb.h"
#include "IcarusCharacterMovement_Ground.h"
#include "IcarusCharacterMovement_Swim.h"
#include "IcarusCharacterMovement_Slide.h"
#include "IcarusCharacterMovement_Vault.h"
#include "Volume/IcarusWaterVolume.h"
#include "Icarus.h"

#if !UE_BUILD_SHIPPING

// Icarus Characters CVars
namespace IcarusCharacterMovementCVars
{
	//Variavel para visualizar as variaveis do climb
	static int32 VisualizeClimb = 0;
	static int32 VisualizeVault = 0;
	static int32 VisualizeIcarusMovement = 0;
	static int32 VisualizeIcarusMovement_Swim = 0;
	static int32 VisualizeIcarusSlide = 0;
	static FAutoConsoleVariableRef CVarVisualizeClimb(
		TEXT("p.VisualizeClimb"),
		VisualizeClimb,
		TEXT("Visualiza variaveis do climb.\n")
		TEXT("0: Disable, 1: Enable"),
		ECVF_Default);
	static FAutoConsoleVariableRef CVarVisualizeSlide(
		TEXT("p.VisualizeSlide"),
		VisualizeIcarusSlide,
		TEXT("Visualiza variaveis do slide.\n")
		TEXT("0: Disable, 1: Enable"),
		ECVF_Default);
	static FAutoConsoleVariableRef CVarVisualizeIcarusMovement(
		TEXT("p.VisualizeIcarusMovement"),
		VisualizeIcarusMovement,
		TEXT("Visualiza variaveis do movimento.\n")
		TEXT("0: Disable, 1: Enable"),
		ECVF_Default);
	static FAutoConsoleVariableRef CVarVisualizeIcarusMovement_Swim(
		TEXT("p.VisualizeIcarusMovement_Swim"),
		VisualizeIcarusMovement_Swim,
		TEXT("Visualiza variaveis do movimento.\n")
		TEXT("0: Disable, 1: Enable"),
		ECVF_Default); 
	static FAutoConsoleVariableRef CVarVisualizeVault(
		TEXT("p.VisualizeVault"),
		VisualizeVault,
		TEXT("Visualiza variaveis de vault.\n")
		TEXT("0: Disable, 1: Enable"),
		ECVF_Default);
}

#endif // WITH_EDITOR

UIcarusCharacterMovement::UIcarusCharacterMovement()
{
}

void UIcarusCharacterMovement::BeginPlay()
{
	// Pega icarus player
	IcarusPlayerCharacter = Cast<AIcarusPlayerCharacter>(GetOwner());
	CHECK_WITH_MESSAGE(IcarusPlayerCharacter, TEXT("UIcarusCharacterMovement: failed to get icarus player character owner."));

	IcarusPlayerController = Cast<AIcarusPlayerController>(IcarusPlayerCharacter->GetController());
	CHECK_WITH_MESSAGE(IcarusPlayerController, TEXT("UIcarusCharacterMovement: current icarus player character doesn't have icarus player controller."));

	CHECK_WITH_MESSAGE(MovementAir, TEXT("UIcarusCharacterMovement: objeto do sistema de movimento no ar eh invalido."));
	CHECK_WITH_MESSAGE(MovementClimb, TEXT("UIcarusCharacterMovement: objeto do sistema de movimento de climb eh invalido."));
	CHECK_WITH_MESSAGE(MovementVault, TEXT("UIcarusCharacterMovement: objeto do sistema de movimento de vault eh invalido."));
	CHECK_WITH_MESSAGE(MovementGround, TEXT("UIcarusCharacterMovement: objeto do sistema de movimento no chao eh invalido."));
	CHECK_WITH_MESSAGE(MovementSwim, TEXT("UIcarusCharacterMovement: objeto do sistema de movimento na agua eh invalido."));
	CHECK_WITH_MESSAGE(MovementSwim, TEXT("UIcarusCharacterMovement: objeto do sistema de slide eh invalido."));

	// Inicia sistema
	MovementAir->Initialize();
	MovementClimb->Initialize();
	MovementVault->Initialize();
	MovementGround->Initialize();
	MovementSwim->Initialize();
	MovementSwim->bActive = false;
	MovementSlide->Initialize();

	// Super
	Super::BeginPlay();

	// Seta valor de 
	DefaultWaterMovementMode = EMovementMode::MOVE_Flying;
}

void UIcarusCharacterMovement::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	// Super
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Se estiver ignorando input, nao executa atualizacao de estado.
	if (!bIgnoreMoveInput_Internal)
	{
		// Atualiza update de climb
		MovementClimb->Update(DeltaTime);

		// Atualiza update de climb
		MovementVault->Update(DeltaTime);

		// Se nao estiver em climb movement, habilita updates de outros sistemas
		if (!MovementClimb->bIsInClimb)
		{
			// Atualiza update de ground
			if (CurrentMovementState != EMovementState::EMS_Jump &&
				CurrentMovementState != EMovementState::EMS_Land)
			{
				MovementGround->Update(DeltaTime);
			}
			
			// Atualiza update de swim
			MovementSwim->Update(DeltaTime);

			// Atualiza update de air
			MovementAir->Update(DeltaTime);

			if (MovementSlide->IsInSlideSystem())
			{
				// Atualiza update slide
				MovementSlide->Update(DeltaTime);
			}
		}
	}

#if !UE_BUILD_SHIPPING
	// Debug, visualiza informacoes de climb.
	if (IcarusCharacterMovementCVars::VisualizeClimb && MovementClimb->bActive)
	{
		VisualizeClimb();
	}

	// Debug, visualiza informacoes de movimento.
	if (IcarusCharacterMovementCVars::VisualizeIcarusMovement)
	{
		VisualizeIcarusMovement();
	}

	// Debug, visualiza informacoes de movimento swim.
	if (IcarusCharacterMovementCVars::VisualizeIcarusMovement_Swim)
	{
		VisualizeIcarusMovement_Swim();
	}

	// Debug, visualiza informacoes de slide.
	if (IcarusCharacterMovementCVars::VisualizeIcarusSlide)
	{
		VisualizeSlide();
	}

	if (IcarusCharacterMovementCVars::VisualizeVault)
	{
		VisualizeVault();
	}

#endif
}

void UIcarusCharacterMovement::SetMovementState(TEnumAsByte<EMovementState> NewState)
{
	//CHECK_FLOW(!(IcarusPlayerCharacter->InteractionManager->IsInteracting()));

	// Pega condicao para novo estado, se condicao falhar sai da funcao.
	if (!GetStateCondition(NewState))
	{
		return;
	}

	bIsSettingMovementState = true;

	MovementAir->SetMovementState(NewState, true);
	MovementClimb->SetMovementState(NewState, true);
	MovementGround->SetMovementState(NewState, true);
	MovementSwim->SetMovementState(NewState, true);
	MovementSlide->SetMovementState(NewState, true);
	MovementVault->SetMovementState(NewState, true);

	CurrentMovementState = NewState;
	bIsSettingMovementState = false;
	PostStateChanged();
}

bool UIcarusCharacterMovement::GetStateCondition(EMovementState NewState)
{
	if (CurrentMovementState == NewState)
	{
		return false;
	}

	// Se player nao estiver interagindo com ator de interacao grabable mas esta interagindo.
	bool bNoGrabInteracting = IcarusPlayerCharacter->InteractionManager->IsInteracting() &&
		IcarusPlayerCharacter->InteractionManager->GetCurrentInteractionActor()->InteractionType != EInteractionType::Grab;

	switch (NewState)
	{
	case EMovementState::EMS_Start:
		return CurrentMovementState == EMovementState::EMS_Stop;
	case EMovementState::EMS_InterruptStart:
		return (CurrentMovementState == EMovementState::EMS_Start || CurrentMovementState == EMovementState::EMS_QuickStart) && !bNoGrabInteracting;
	case EMovementState::EMS_QuickStart:
		return (CurrentMovementState == EMovementState::EMS_Moving || CurrentMovementState == EMovementState::EMS_Start) && !bNoGrabInteracting;
	case EMovementState::EMS_Stop:
		return CurrentMovementState != EMovementState::EMS_Rest;
	case EMovementState::EMS_Rest:
		//return !MovementGround->IsMoving() && MovementMode == EMovementMode::MOVE_Walking;
		return false;
	case EMovementState::EMS_Jump:
		return CurrentMovementState != EMovementState::EMS_Land;
	case EMovementState::EMS_InterruptStart_HighAngle:
	case EMovementState::EMS_InterruptStart_UnstableMovementAlpha:
	{
		return !bNoGrabInteracting;
	}
	}
	return true;
}

void UIcarusCharacterMovement::PostStateChanged()
{
	/*switch (CurrentMovementState)
	{
	}*/
	PostStateChangedDelegate.Broadcast(CurrentMovementState);
}

#if !UE_BUILD_SHIPPING

void UIcarusCharacterMovement::VisualizeVault()
{
	CHECK_FLOW_NO_LOG(IcarusPlayerCharacter);
	if (IcarusCharacterMovementCVars::VisualizeVault)
	{
		FVector CapsuleLocation = IcarusPlayerCharacter->GetCapsuleComponent()->GetComponentLocation();
		DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 100.0f), TEXT("############ VAULT DATA ##############"), nullptr, FColor::Purple, 0.0f, true);
		DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 90.0f), TEXT("bCanVault: ") + BOOL_TO_STRING(MovementVault->bCanVault), nullptr, FColor::Blue, 0.0f, true);
		DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 80.0f), FString::Printf(TEXT("LeftDetectedPoint: %s"), *MovementVault->LeftDetectedPoint.ToString()), nullptr, FColor::Blue, 0.0f, true);
		DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 70.0f), FString::Printf(TEXT("RightDetectedPoint: %s"), *MovementVault->RightDetectedPoint.ToString()), nullptr, FColor::Blue, 0.0f, true);
		DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 60.0f), FString::Printf(TEXT("LeftDetectedNormal: %s"), *MovementVault->LeftDetectedNormal.ToString()), nullptr, FColor::Blue, 0.0f, true);
		DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 50.0f), FString::Printf(TEXT("RightDetectedNormal: %s"), *MovementVault->RightDetectedNormal.ToString()), nullptr, FColor::Blue, 0.0f, true);
		DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 40.0f), FString::Printf(TEXT("CentralPoint: %s"), (MovementVault->bCanVault) ? *MovementVault->RightDetectedNormal.ToString() : TEXT("None")), nullptr, FColor::Blue, 0.0f, true);
		DrawDebugSphere(GetWorld(), MovementVault->GetCentralPoint(), 5.0f, 8, FColor::Black, false, -1.f, false, 2.0f);

		DrawDebugDirectionalArrow(
			GetWorld(),
			(MovementVault->GetCentralPoint() + MovementVault->CapsuleOffset(MovementVault->GetCentralNormal())),
			(MovementVault->GetCentralPoint() + MovementVault->CapsuleOffset(MovementVault->GetCentralNormal())) + (MovementVault->GetCentralNormal()*50.0f),
			-1.0f,
			FColor::Emerald,
			false,
			5.0f,
			0,
			2.0f
		);
	}
}

void UIcarusCharacterMovement::VisualizeClimb()
{
	CHECK_FLOW_NO_LOG(IcarusPlayerCharacter); 
	FVector CapsuleLocation = IcarusPlayerCharacter->GetCapsuleComponent()->GetComponentLocation();
	TArray<FHitResult> Results;
	TArray<AIcarusLedgeBase*> Ledges = IcarusPlayerCharacter->LedgeManipulation->GetAllLedgeActors(256.0f, Results);
	if (IcarusCharacterMovementCVars::VisualizeClimb == 1 || IcarusCharacterMovementCVars::VisualizeClimb == 3)
	{
		DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 100.0f), TEXT("############ CLIMB DATA ##############"), nullptr, FColor::Purple, 0.0f, true);
		DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 90.0f), TEXT("Climb-state: ") + ENUM_TO_STRING(EClimbState, MovementClimb->GetCurrentState()), nullptr, FColor::Blue, 0.0f, true);
		DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 80.0f), FString::Printf(TEXT("ClimbAimDirection: %s"), *MovementClimb->ClimbAimDirection.ToString()), nullptr, FColor::Blue, 0.0f, true);
		DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 70.0f), TEXT("bCanGroundGrab bool: ") + BOOL_TO_STRING(MovementClimb->CanGroundGrab()), nullptr, FColor::Red, 0.0f, true);
		DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 60.0f), TEXT("bIsInClimb bool: ") + BOOL_TO_STRING(MovementClimb->bIsInClimb), nullptr, FColor::Red, 0.0f, true);
		DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 50.0f), TEXT("bIsGrabbed bool: ") + BOOL_TO_STRING(MovementClimb->bIsGrabbed), nullptr, FColor::Red, 0.0f, true);
		DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 40.0f), TEXT("bIsGroundGrabbing bool: ") + BOOL_TO_STRING(MovementClimb->IsGroundGrabbing()), nullptr, FColor::Red, 0.0f, true);
		DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 30.0f), FString::Printf(TEXT("Grab Ledge: %s"), (MovementClimb->CurrentGrabLedge) ? *MovementClimb->CurrentGrabLedge->GetName() : TEXT("")), nullptr, FColor::Blue, 0.0f, true);
		DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 20.0f), FString::Printf(TEXT("Grab Point: %s"), *MovementClimb->CurrentGrabPoint.ToString()), nullptr, FColor::Blue, 0.0f, true);
		DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 10.0f), FString::Printf(TEXT("First Grab Point: %s"), *MovementClimb->FirstGrabPoint.ToString()), nullptr, FColor::Blue, 0.0f, true);
		DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 00.0f), TEXT("Root Motion: ") + ENUM_TO_STRING(ERootMotionMode::Type, IcarusPlayerCharacter->GetMesh()->GetAnimInstance()->RootMotionMode), nullptr, FColor::Blue, 0.0f, true);
	}
	
	if (IcarusCharacterMovementCVars::VisualizeClimb == 2 || IcarusCharacterMovementCVars::VisualizeClimb == 3)
	{
		for (AIcarusLedgeBase* Ledge : Ledges)
		{
			float Angle, JumpVelDot, UnderDot;
			FVector ClosePoint = IcarusPlayerCharacter->LedgeManipulation->GetCharacterClosestPointOnLedgeActor(Ledge);
			bool CheckForwardAngle = IcarusPlayerCharacter->LedgeManipulation->CheckForwardAngle(Ledge, Angle);
			bool CheckJumpVelDot = IcarusPlayerCharacter->LedgeManipulation->CheckJumpVelocityToTarget(ClosePoint, JumpVelDot);
			bool CheckUnderLedge = IcarusPlayerCharacter->LedgeManipulation->CheckUnderLedgeAmount(ClosePoint, UnderDot);
			DrawDebugLine(GetWorld(), Ledge->GetActorLocation(), Ledge->GetEndPointWorldCoordinate(), FColor::Blue, false, -1.0f, (uint8)'\000', 2.0f);
			DrawDebugSphere(GetWorld(), IcarusPlayerCharacter->LedgeManipulation->GetCharacterClosestPointOnLedgeActor(Ledge), 10.0f, 8, FColor::Blue, false, -1.f, false, 2.0f);
			DrawDebugDirectionalArrow(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 50.0f), MovementClimb->GetAimWorldDirection()*150.0f + CapsuleLocation + FVector(0.0f, 0.0f, 50.0f), 5.0f, FColor::Green, false, -1.0f, (uint8)'\000', 3.0f);
			DrawDebugString(GetWorld(), ClosePoint + FVector(0.0f, 0.0f, 50.0f), TEXT("############ LEDGE DATA ##############"), nullptr, FColor::Purple, 0.0f, true);
			DrawDebugString(GetWorld(), ClosePoint + FVector(0.0f, 0.0f, 40.0f), FString::Printf(TEXT("NAME: %s"), *Ledge->GetName()), nullptr, FColor::Blue, 0.0f, true);
			DrawDebugString(GetWorld(), ClosePoint + FVector(0.0f, 0.0f, 30.0f), TEXT("Forward Angle bool: ") + BOOL_TO_STRING(CheckForwardAngle), nullptr, FColor::Red, 0.0f, true);
			DrawDebugString(GetWorld(), ClosePoint + FVector(0.0f, 0.0f, 20.0f), FString::Printf(TEXT("Forward Angle: %f"), Angle), nullptr, FColor::Blue, 0.0f, true);
			DrawDebugString(GetWorld(), ClosePoint + FVector(0.0f, 0.0f, 10.0f), TEXT("JumpVelocity Dot bool: ") + BOOL_TO_STRING(CheckJumpVelDot), nullptr, FColor::Red, 0.0f, true);
			DrawDebugString(GetWorld(), ClosePoint + FVector(0.0f, 0.0f, 0.0f), FString::Printf(TEXT("JumpVelocity Dot: %f"), JumpVelDot), nullptr, FColor::Blue, 0.0f, true);
			DrawDebugString(GetWorld(), ClosePoint + FVector(0.0f, 0.0f, -10.0f), TEXT("UnderLedge Dot bool: ") + BOOL_TO_STRING(CheckUnderLedge), nullptr, FColor::Red, 0.0f, true);
			DrawDebugString(GetWorld(), ClosePoint + FVector(0.0f, 0.0f, -20.0f), FString::Printf(TEXT("UnderLedge Dot: %f"), UnderDot), nullptr, FColor::Blue, 0.0f, true);
		}
	}

	if (IcarusCharacterMovementCVars::VisualizeClimb == 4)
	{
		for (AIcarusLedgeBase* Ledge : Ledges)
		{
			float Angle, JumpVelDot, UnderDot;
			FVector ClosePoint = IcarusPlayerCharacter->LedgeManipulation->GetCharacterClosestPointOnLedgeActor(Ledge);
			bool CheckForwardAngle = IcarusPlayerCharacter->LedgeManipulation->CheckForwardAngle(Ledge, Angle);
			bool CheckJumpVelDot = IcarusPlayerCharacter->LedgeManipulation->CheckJumpVelocityToTarget(ClosePoint, JumpVelDot);
			bool CheckUnderLedge = IcarusPlayerCharacter->LedgeManipulation->CheckUnderLedgeAmount(ClosePoint, UnderDot);
			DrawDebugLine(GetWorld(), Ledge->GetActorLocation(), Ledge->GetEndPointWorldCoordinate(), FColor::Blue, false, -1.0f, (uint8)'\000', 2.0f);
			DrawDebugSphere(GetWorld(), IcarusPlayerCharacter->LedgeManipulation->GetCharacterClosestPointOnLedgeActor(Ledge), 10.0f, 8, FColor::Blue, false, -1.f, false, 2.0f);
			DrawDebugDirectionalArrow(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 50.0f), MovementClimb->GetAimWorldDirection()*150.0f + CapsuleLocation + FVector(0.0f, 0.0f, 50.0f), 5.0f, FColor::Green, false, -1.0f, (uint8)'\000', 3.0f);
			DrawDebugString(GetWorld(), ClosePoint + FVector(0.0f, 0.0f, 50.0f), TEXT("############ LEDGE DATA ##############"), nullptr, FColor::Purple, 0.0f, true);
			DrawDebugString(GetWorld(), ClosePoint + FVector(0.0f, 0.0f, 40.0f), FString::Printf(TEXT("ToCharacter Dot: %f"), FVector::DotProduct(MovementClimb->GetAimWorldDirection().GetSafeNormal(), (ClosePoint - MovementClimb->CurrentGrabPoint).GetSafeNormal())), nullptr, FColor::Orange, 0.0f, true);
			DrawDebugString(GetWorld(), ClosePoint + FVector(0.0f, 0.0f, 30.0f), FString::Printf(TEXT("Distance from head ref: %f"), IcarusPlayerCharacter->LedgeManipulation->GetDistanceFromPointToHeadReference(ClosePoint)), nullptr, FColor::Orange, 0.0f, true);
			DrawDebugDirectionalArrow(GetWorld(), ClosePoint, MovementClimb->GetAimWorldDirection()*25.0f + ClosePoint, 5.f, FColor::Green, false, -1.0f, (uint8)'\000', 3.0f);
			DrawDebugDirectionalArrow(GetWorld(), ClosePoint, (ClosePoint - MovementClimb->CurrentGrabPoint).GetSafeNormal()*25.0f + ClosePoint, 5.f, FColor::Silver, false, -1.0f, (uint8)'\000', 3.0f);
		}
	}
}

void UIcarusCharacterMovement::VisualizeSlide()
{
	CHECK_FLOW_NO_LOG(IcarusPlayerCharacter);
	FVector CapsuleLocation = IcarusPlayerCharacter->GetCapsuleComponent()->GetComponentLocation();
	DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 175.0f), TEXT("bCanUpdateCapsuleRotation bool: ") + BOOL_TO_STRING(MovementSlide->bCanUpdateCapsuleRotation), nullptr, FColor::Blue, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 160.0f), TEXT("bInSlide bool: ") + BOOL_TO_STRING(MovementSlide->IsInSlideSystem()), nullptr, FColor::Blue, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 140.0f), FString::Printf(TEXT("Initial box: %s"),*MovementSlide->InitialBoxPoint.ToString()), nullptr, FColor::Blue, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 120.0f), FString::Printf(TEXT("Final box: %s"), *MovementSlide->FinalBoxPoint.ToString()), nullptr, FColor::White, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 100.0f), FString::Printf(TEXT("Angulo Entrada: %f"), MovementSlide->ForwardFlowDotAngle), nullptr, FColor::Red, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 90.0f), TEXT("bIsInAntiFlow: ") + BOOL_TO_STRING(MovementSlide->bIsInAntiFlow), nullptr, FColor::Red, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 80.0f), FString::Printf(TEXT("TargetInputRotation: %f"),  MovementSlide->TargetInputRotation), nullptr, FColor::Blue, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 70.0f), TEXT("bCanUpdateCapsuleRotation: ") + BOOL_TO_STRING(MovementSlide->bCanUpdateCapsuleRotation), nullptr, FColor::Red, 0.0f, true);
	DrawDebugDirectionalArrow(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 50.0f), MovementSlide->GetCurrentTotalForces().GetSafeNormal()*FVector(1.0f, 1.0f, 0.0f)*150.0f + CapsuleLocation + FVector(0.0f, 0.0f, 50.0f), 5.0f, FColor::Green,false, -1.0f ,(uint8)'\000', 3.0f);
	DrawDebugDirectionalArrow(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 50.0f), MovementSlide->GetCurrentDirectionImpulse().GetSafeNormal()*FVector(1.0f, 1.0f, 0.0f)*150.0f + CapsuleLocation + FVector(0.0f, 0.0f, 50.0f), 5.0f, FColor::Blue, false, -1.0f, (uint8)'\000', 3.0f);
	DrawDebugDirectionalArrow(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 50.0f), MovementSlide->GetCurrentDirectionInput().GetSafeNormal()*FVector(1.0f, 1.0f, 0.0f)*150.0f + CapsuleLocation + FVector(0.0f, 0.0f, 50.0f), 5.0f, FColor::Red, false, -1.0f, (uint8)'\000', 3.0f);
	DrawDebugDirectionalArrow(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 50.0f), MovementSlide->GetCurrentDirectionLateralImpulse().GetSafeNormal()*FVector(1.0f, 1.0f, 0.0f)*150.0f + CapsuleLocation + FVector(0.0f, 0.0f, 50.0f), 5.0f, FColor::Black, false, -1.0f, (uint8)'\000', 3.0f);
	DrawDebugDirectionalArrow(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 50.0f), Velocity.GetSafeNormal()*150.0f + CapsuleLocation + FVector(0.0f, 0.0f, 50.0f), 5.0f, FColor::Yellow, false, -1.0f, (uint8)'\000', 3.0f);
	DrawDebugSphere(GetWorld(), MovementSlide->InitialBoxPoint, 10.0f, 8, FColor::Blue, false, -1.f, false, 1.0f);
	DrawDebugSphere(GetWorld(), MovementSlide->FinalBoxPoint, 10.0f, 8, FColor::White, false, -1.f, false, 1.0f);
	DrawDebugLine(GetWorld(), MovementSlide->InitialBoxPoint, CapsuleLocation, FColor::Blue);
	DrawDebugLine(GetWorld(), MovementSlide->FinalBoxPoint, CapsuleLocation, FColor::White);
}

void UIcarusCharacterMovement::VisualizeIcarusMovement()
{
	CHECK_FLOW_NO_LOG(IcarusPlayerCharacter);

	FVector CapsuleLocation = IcarusPlayerCharacter->GetCapsuleComponent()->GetComponentLocation();
	
	DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 110.0f), TEXT("############ SYSTEM STATES ##############"), nullptr, FColor::Purple, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 100.0f), TEXT("Movement air: ") + BOOL_TO_STRING(MovementAir->bActive), nullptr, MovementAir->bActive ? FColor::Green : FColor::Red, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 90.0f), TEXT("Movement climb: ") + BOOL_TO_STRING(MovementClimb->bActive), nullptr, MovementClimb->bActive ? FColor::Green : FColor::Red, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 80.0f), TEXT("Movement ground: ") + BOOL_TO_STRING(MovementGround->bActive), nullptr, MovementGround->bActive ? FColor::Green : FColor::Red, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 70.0f), TEXT("Movement swim: ") + BOOL_TO_STRING(MovementSwim->bActive), nullptr, MovementSwim->bActive ? FColor::Green : FColor::Red, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 60.0f), TEXT("Movement vault: ") + BOOL_TO_STRING(MovementVault->bActive), nullptr, MovementVault->bActive ? FColor::Green : FColor::Red, 0.0f, true);
	
	DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 50.0f), TEXT("Movement mode: ") + ENUM_TO_STRING(EMovementMode, MovementMode), nullptr, FColor::Orange, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 40.0f), TEXT("Movement state: ") + ENUM_TO_STRING(EMovementState, CurrentMovementState), nullptr, FColor::Orange, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 30.0f), TEXT("############ GROUND ##############"), nullptr, FColor::Purple, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 20.0f), TEXT("Target direction: ") + MovementGround->CurrentTargetDirection.ToString(), nullptr, FColor::Yellow, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 10.0f), TEXT("Movement target angle: ") + FString::Printf(TEXT("%f"), MovementGround->MovementTargetAngle), nullptr, FColor::White, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation, TEXT("Forward rotation angle: ") + FString::Printf(TEXT("%f"), MovementGround->ForwardRotationAngle), nullptr, FColor::White, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation - FVector(0.0f, 0.0f, 10.0f), TEXT("Movement alpha: ") + FString::Printf(TEXT("%f"), MovementGround->MovementAlpha), nullptr, FColor::White, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation - FVector(0.0f, 0.0f, 20.0f), TEXT("Last movement alpha: ") + FString::Printf(TEXT("%f"), MovementGround->LastMovementAlpha), nullptr, FColor::White, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation - FVector(0.0f, 0.0f, 30.0f), TEXT("Is moving to ? ") + FString(MovementGround->IsMovingTo() ? "TRUE" : "FALSE"), nullptr, FColor::White, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation - FVector(0.0f, 0.0f, 40.0f), TEXT("Is stable axis ? ") + BOOL_TO_STRING(MovementGround->IsStableAxis()), nullptr, FColor::White, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation - FVector(0.0f, 0.0f, 50.0f), TEXT("Is stable movement alpha ? ") + BOOL_TO_STRING(MovementGround->IsStableMovementAlpha()), nullptr, FColor::White, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation - FVector(0.0f, 0.0f, 60.0f), TEXT("Moving time: ") + FString::Printf(TEXT("%f"), MovementGround->GetMovingTime()), nullptr, FColor::White, 0.0f, true);
	DrawDebugDirectionalArrow(GetWorld(), CapsuleLocation, CapsuleLocation + (FRotator(0.f, MovementGround->RotatorTargetDirection.Yaw, 0.f).Vector() * 100.0), 25.f, FColor::Yellow, false, -1.f, 0, 2.f);
	DrawDebugDirectionalArrow(GetWorld(), CapsuleLocation, CapsuleLocation + (IcarusPlayerCharacter->GetControlRotation().Vector() * FVector(250.f, 250.f, 0.f)), 40.f, FColor::Red, false, -1.f, 0, 2.f);
}

void UIcarusCharacterMovement::VisualizeIcarusMovement_Swim()
{
	CHECK_FLOW_NO_LOG(IcarusPlayerCharacter && MovementSwim->CurrentWaterVolume);

	FVector CapsuleLocation = IcarusPlayerCharacter->GetCapsuleComponent()->GetComponentLocation();

	DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 100.0f), TEXT("############ SWIM ##############"), nullptr, FColor::Purple, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 90.f), TEXT("Is in water? ") + BOOL_TO_STRING(MovementSwim->bActive), nullptr, FColor::White, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 80.f), TEXT("Is in coast of water? ") + BOOL_TO_STRING(MovementSwim->IsInCoastWater()), nullptr, FColor::White, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 70.0f), TEXT("Is in surface? ") + BOOL_TO_STRING(MovementSwim->bIsInSurfaceWater), nullptr, FColor::White, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 60.0f), TEXT("Is fell in water? ") + BOOL_TO_STRING(MovementSwim->bFellInWater), nullptr, FColor::White, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 50.0f), TEXT("Is fell emerged in water? ") + BOOL_TO_STRING(MovementSwim->bFellEmerged), nullptr, FColor::White, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 40.f), TEXT("Is waiting for dive? ") + BOOL_TO_STRING(MovementSwim->bWaitingForDive), nullptr, FColor::White, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 30.0f), TEXT("Is swim up pressed? ") + BOOL_TO_STRING(MovementSwim->bSwimUpPressed), nullptr, FColor::White, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 20.f), TEXT("Is diving? ") + BOOL_TO_STRING(MovementSwim->CurrentWaterVolume->bDiving), nullptr, FColor::White, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation + FVector(0.0f, 0.0f, 10.0f), TEXT("Is water volume activated? ") + BOOL_TO_STRING(MovementSwim->CurrentWaterVolume->bIsActivated), nullptr, FColor::White, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation, TEXT("Is water volume enable surface checks? ") + BOOL_TO_STRING(MovementSwim->CurrentWaterVolume->bEnableSurfaceChecks), nullptr, FColor::White, 0.0f, true);
	DrawDebugString(GetWorld(), CapsuleLocation - FVector(0.0f, 0.0f, 10.0f), TEXT("CurrentSurfaceSign: ") + FString::Printf(TEXT("%f"), MovementSwim->CurrentWaterVolume->CurrentSurfaceSign), nullptr, FColor::White, 0.0f, true);
}

#endif // WITH_EDITOR
