// Fill out your copyright notice in the Description page of Project Settings.

#include "IcarusCharacterMovement_Climb.h"
#include "IcarusCharacterMovement_Ground.h"
#include "IcarusCharacterMovement_Air.h"
#include "Climb/IcarusLedgeManipulation.h"
#include "IcarusCharacterMovement.h"
#include "Player/IcarusPlayerCharacter.h"
#include "Player/IcarusPlayerController.h"
#include "Runtime/Engine/Classes/Animation/AnimNode_StateMachine.h"
#include "Icarus.h"

UIcarusCharacterMovement_Climb::UIcarusCharacterMovement_Climb() 
{
}

void UIcarusCharacterMovement_Climb::Initialize()
{
	Super::Initialize();

	IcarusPlayerCharacter->LandedDelegate.AddUniqueDynamic(this, &UIcarusCharacterMovement_Climb::OnLanded);

	SearchedLedgeComponent = IcarusPlayerCharacter->FindComponentByClass<UIcarusLedgeManipulation>();

	check(SearchedLedgeComponent);
}

bool UIcarusCharacterMovement_Climb::SetMovementState(EMovementState NewState, bool bForce)
{
	CHECK_FLOW_WITH_RETURN_NO_LOG(Super::SetMovementState(NewState, bForce), false);

	switch (NewState)
	{
		case EMovementState::EMS_Climb:
		{
			DISABLE_MOTION
			IcarusCharacterMovement->MovementGround->bActive = false;
			IcarusCharacterMovement->MovementAir->bActive = false;
			break;
		}
	}

	return true;
}

FVector UIcarusCharacterMovement_Climb::GetAimWorldDirection()
{
	return IcarusPlayerCharacter->GetTransform().TransformVector(ClimbAimDirection.GetSafeNormal());
}

void UIcarusCharacterMovement_Climb::SetState(TEnumAsByte<EClimbState> State)
{
	CurrentState = State;
}

void UIcarusCharacterMovement_Climb::SetInClimb(bool State)
{
	if (State)
	{
		bIsInClimb = true;
		IcarusCharacterMovement->SetMovementState(EMovementState::EMS_Climb);
	}
	else
	{
		bIsInClimb = false;
	}
}

bool UIcarusCharacterMovement_Climb::CheckNearlyFirstGrabbablePoint(FVector& Point, AIcarusLedgeBase*& PointLedge)
{
	if (bIsInGroundGrabbing)
	{
		if (SearchedLedgeComponent->GetDistanceFromPointToHeadReference(CurrentGrabPoint) <= DistanceToExecuteNearlyGrab)
		{
			float AngleResult;
			if (SearchedLedgeComponent->CheckForwardAngle(CurrentGrabLedge, AngleResult) && GetCurrentState() != ECS_Exiting)
			{
				PointLedge = CurrentGrabLedge;
				Point = CurrentGrabPoint;
				return true;
			}
		}
	}
	else
	{
		TArray<FHitResult> Results;
		TArray<AIcarusLedgeBase*> SearchedLedges = SearchedLedgeComponent->GetAllLedgeActors(150.0f, Results);
		for (AIcarusLedgeBase* Ledge : SearchedLedges)
		{
			if (IS_VALID(Ledge))
			{
				FVector SearchedPoint = SearchedLedgeComponent->GetCharacterClosestPointOnLedgeActor(Ledge);
				float DotResult;

				if (
					SearchedLedgeComponent->GetDistanceFromPointToHeadReference(SearchedPoint) <= DistanceToExecuteNearlyGrab
					&& SearchedLedgeComponent->CheckJumpVelocityToTarget(SearchedPoint, DotResult)
					&& IcarusCharacterMovement->CurrentMovementState == EMovementState::EMS_Jump
					)

				{
					float AngleResult;
					if (SearchedLedgeComponent->CheckForwardAngle(Ledge, AngleResult) && GetCurrentState() != ECS_Exiting)
					{
						PointLedge = Ledge;
						Point = SearchedPoint;
						return true;
					}
				}
			}
		}
	}

	Point = FVector::ZeroVector;
	return false;
}

void UIcarusCharacterMovement_Climb::Update_CurrentPoint()
{
	if (!bIsInClimb || !bIsGrabbed) return;
	if (CAN_UPDATE_POSITION_ON_EDGE(CurrentState) && CurrentGrabLedge)
	{
		CurrentGrabPoint = SearchedLedgeComponent->GetCharacterClosestPointOnLedgeActor(CurrentGrabLedge);
	}
}

void UIcarusCharacterMovement_Climb::Update_NearlyGrabCheck()
{
	if (bIsInClimb) return;
	FVector FirstPoint;
	AIcarusLedgeBase* FirstLedge;

	if (CheckNearlyFirstGrabbablePoint(FirstPoint, FirstLedge) && GetCurrentState() != ECS_Exiting && !bGroundGrabAnimationPlaying)
	{
		SetInClimb(true);
		CurrentGrabLedge = FirstLedge;
		CurrentGrabPoint = FirstPoint;
		IcarusCharacterMovement->SetMovementMode(EMovementMode::MOVE_Flying);
		IcarusCharacterMovement->StopMovementImmediately();
		if (IsGroundGrabbing()) { /*InterruptPlayerInterpolation();*/ bIsInGroundGrabbing = false; }
		FirstGrabPoint = AddLedgeOffsetsToPoint(SearchedLedgeComponent->GetTransformedOutLimitPoint(FirstLedge, FirstPoint, LimitEdgeOffset), FirstLedge);
		InterpolingMovePlayer(FirstGrabPoint, (FirstLedge->GetSegmentNormal()*-1.0f).Rotation(), NearlyGrabInterpSpeed, bNearlyGrabConstantInterp,&UIcarusCharacterMovement_Climb::FirstGrab_InterpResult);
		VerifyAimLimits();
	}
}

void UIcarusCharacterMovement_Climb::Update_GroundGrabCheck()
{
	bCanGroundGrab = false;
	if (bIsInClimb || !IcarusCharacterMovement->MovementGround->bActive) return;
	CurrentGrabLedge = nullptr;
	AIcarusLedgeBase* LedgeActor = IcarusPlayerCharacter->LedgeManipulation->GetDirectionalNearlyLedgeActor(MaxGroundGrabDistance, FVector(0.0f, 0.0f, 1.0f), 0.8f);
	if (!LedgeActor) return;

	FVector Point = IcarusPlayerCharacter->LedgeManipulation->GetCharacterClosestPointOnLedgeActor(LedgeActor);
	float UnderLedgeAmount;
	if (IcarusPlayerCharacter->LedgeManipulation->CheckUnderLedgeAmount(Point, UnderLedgeAmount))
	{
		float AngleResult;
		if (SearchedLedgeComponent->CheckForwardAngle(LedgeActor, AngleResult) && SearchedLedgeComponent->CheckSufficientHeight(Point, 20.0f))
		{
			bCanGroundGrab = true;
			CurrentGrabPoint = Point;
			CurrentGrabLedge = LedgeActor;
			VerifyAimLimits();
		}
	}
}

void UIcarusCharacterMovement_Climb::Update_InterpolingMovePlayer()
{
	if (bInterpolingMovingPlayer)
	{
		//ClientMessage("InterpPoint: %s | InterpSpeed: %f | DistanceRemaining: %f", *PlayerInterpPoint.ToString(), PlayerInterpSpeed, CurrentInterpDistanceRemaining);
		IcarusPlayerCharacter->SetActorRotation(
			(bPlayerInterpUseConstant) ? FMath::RInterpConstantTo(IcarusPlayerCharacter->GetActorRotation(), PlayerInterpRotator, GetWorld()->GetDeltaSeconds(), PlayerInterpSpeed)
			: FMath::RInterpTo(IcarusPlayerCharacter->GetActorRotation(), PlayerInterpRotator, GetWorld()->GetDeltaSeconds(), PlayerInterpSpeed), ETeleportType::TeleportPhysics);
		IcarusPlayerCharacter->SetActorLocation(
			(bPlayerInterpUseConstant) ? FMath::VInterpConstantTo(IcarusPlayerCharacter->GetActorLocation(), PlayerInterpPoint, GetWorld()->GetDeltaSeconds(), PlayerInterpSpeed) : FMath::VInterpTo(IcarusPlayerCharacter->GetActorLocation(), PlayerInterpPoint, GetWorld()->GetDeltaSeconds(), PlayerInterpSpeed)
			, false, (FHitResult*)nullptr, ETeleportType::TeleportPhysics);

		CurrentInterpDistanceRemaining = (IcarusPlayerCharacter->GetActorLocation() - PlayerInterpPoint).Size();

		if (IcarusPlayerCharacter->GetActorRotation().Equals(PlayerInterpRotator, 0.01f) && IcarusPlayerCharacter->GetActorLocation().Equals(PlayerInterpPoint, 0.01f))
		{
			if(PlayerInterpFinalAction) (this->*PlayerInterpFinalAction)();
			CurrentInterpDistanceRemaining = 0.0f;
			InterruptPlayerInterpolation();
		}
	}
}

void UIcarusCharacterMovement_Climb::Update_AimLimit()
{
	if (!bIsInClimb) return;
	if (CHECK_AIM_ONLY_LEFT && GetCurrentState() == ECS_IdleLimit_Left)
	{
		AIcarusLedgeBase* Ledge = nullptr;
		FVector Point;
		if (SearchedLedgeComponent->GetPointByDirection(GetAimWorldDirection(), CurrentGrabPoint, Lateral_Jump_MinDistance, Lateral_Jump_MaxDistance, Ledge, Point))
		{
			bCanAimToLeft = true;
		}
	}
	else
	{
		bCanAimToLeft = false;
	}
	if (CHECK_AIM_ONLY_RIGHT && GetCurrentState() == ECS_IdleLimit_Right)
	{
		AIcarusLedgeBase* Ledge = nullptr;
		FVector Point;
		if (SearchedLedgeComponent->GetPointByDirection(GetAimWorldDirection(), CurrentGrabPoint, Lateral_Jump_MinDistance, Lateral_Jump_MaxDistance, Ledge, Point))
		{
			bCanAimToRight = true;
		}
	}
	else
	{
		bCanAimToRight = false;
	}
}

void UIcarusCharacterMovement_Climb::VerifyAimLimits()
{
	/*if (!CurrentGrabLedge) return;
	AIcarusLedgeBase* Ledge = nullptr;
	FVector Point;
	DrawDebugLine(IcarusPlayerCharacter->GetWorld(),
		CurrentGrabLedge->GetEndPointWorldCoordinate(),
		CurrentGrabLedge->GetEndPointWorldCoordinate() + (CurrentGrabLedge->GetTransform().TransformVectorNoScale(FVector(-1.0f, 0.0f, 0.0f))*100.0f),
		FColor::Black,
		false,
		4.0f,
		0,
		2.0f);
	bCanAimToLeft = SearchedLedgeComponent->GetPointByDirection(CurrentGrabLedge->GetTransform().TransformVectorNoScale(FVector(-1.0f, 0.0f, 0.0f)), CurrentGrabLedge->GetActorLocation(), Lateral_Jump_MinDistance, Lateral_Jump_MaxDistance, Ledge, Point);
	bCanAimToRight = SearchedLedgeComponent->GetPointByDirection(CurrentGrabLedge->GetTransform().TransformVectorNoScale(FVector(1.0f, 0.0f, 0.0f)), CurrentGrabLedge->GetEndPointWorldCoordinate(), Lateral_Jump_MinDistance, Lateral_Jump_MaxDistance, Ledge, Point);*/
}

void UIcarusCharacterMovement_Climb::Update_CheckSegmentLimit()
{
	if (CurrentGrabLedge && bIsGrabbed && bIsInClimb)
	{
		bool OutLeft = false;
		bool OutRight = false;
		FVector ResultPoint  = SearchedLedgeComponent->GetTransformedOutLimitPoint(CurrentGrabLedge, CurrentGrabPoint, LimitEdgeOffset, OutLeft, OutRight);
		if ((GetCurrentState() == EClimbState::ECS_ToLeft || GetCurrentState() == EClimbState::ECS_DefaultIdle) && OutLeft)
		{
			DISABLE_MOTION
			IcarusCharacterMovement->StopMovementImmediately();
			ClientMessage("%s", *ResultPoint.ToString());
			IcarusPlayerCharacter->SetActorLocation(AddLedgeOffsetsToPoint(ResultPoint, CurrentGrabLedge));
			SetState(ECS_IdleLimit_Left);
		}

		if ((GetCurrentState() == EClimbState::ECS_ToRight || GetCurrentState() == EClimbState::ECS_DefaultIdle) && OutRight)
		{
			DISABLE_MOTION
			IcarusCharacterMovement->StopMovementImmediately();
			ClientMessage("%s", *ResultPoint.ToString());
			IcarusPlayerCharacter->SetActorLocation(AddLedgeOffsetsToPoint(ResultPoint, CurrentGrabLedge));
			SetState(ECS_IdleLimit_Right);
		}
	}
}

void UIcarusCharacterMovement_Climb::FirstGrab_InterpResult()
{
	IcarusPlayerCharacter->IcarusPlayerController->ClientPlayCameraShake(FirstLedgeGrabShake);
	EnableMovement();
	bIsGrabbed = true;
	SetState(ECS_DefaultIdle);
	if ((NextStateOnJump == ECS_IdleLimit_Left || NextStateOnJump == ECS_DefaultIdle) && CHECK_AIM_ONLY_RIGHT) SetState(ECS_ToRight); ENABLE_MOTION;
	if ((NextStateOnJump == ECS_IdleLimit_Right || NextStateOnJump == ECS_DefaultIdle) && CHECK_AIM_ONLY_LEFT) SetState(ECS_ToLeft); ENABLE_MOTION;
}

void UIcarusCharacterMovement_Climb::FirstGrabGround_InterpResult()
{
	bIsInGroundGrabbing = false;
}

void UIcarusCharacterMovement_Climb::InterruptPlayerInterpolation()
{
	bInterpolingMovingPlayer = false;
	PlayerInterpPoint = FVector::ZeroVector;
	PlayerInterpRotator = FRotator::ZeroRotator;
	PlayerInterpFinalAction = nullptr;
}

void UIcarusCharacterMovement_Climb::InterpolingMovePlayer(FVector Point, FRotator Rotation, float InterpSpeed, bool UseConstant, void (UIcarusCharacterMovement_Climb::*FinalAction)())
{
	CurrentGrabLedge->RemoveCollisionToPawn();
	bInterpolingMovingPlayer = true;
	PlayerInterpPoint = Point;
	PlayerInterpRotator = Rotation;
	PlayerInterpFinalAction = FinalAction;
	PlayerInterpSpeed = InterpSpeed;
	bPlayerInterpUseConstant = UseConstant;
}

EClimbState UIcarusCharacterMovement_Climb::GetCurrentState()
{
	return CurrentState;
}

bool UIcarusCharacterMovement_Climb::CanGroundGrab()
{
	return bCanGroundGrab;
}

bool UIcarusCharacterMovement_Climb::IsGroundGrabbing()
{
	return bIsInGroundGrabbing;
}

void UIcarusCharacterMovement_Climb::DisableMovement()
{
	bEnableLedgeMovement = false;
}

void UIcarusCharacterMovement_Climb::EnableMovement()
{
	bEnableLedgeMovement = true;
}

bool UIcarusCharacterMovement_Climb::IsPlayerInterpolling()
{
	return bInterpolingMovingPlayer;
}

float UIcarusCharacterMovement_Climb::CurrentLedgeLeftDistanceRemaining()
{
	if (!CurrentGrabLedge || !bIsInClimb) return 0.0f;
	return SearchedLedgeComponent->DistanceRemaningToLeftLimit(CurrentGrabLedge, CurrentGrabPoint, LimitEdgeOffset);
}

float UIcarusCharacterMovement_Climb::CurrentLedgeRightDistanceRemaining()
{
	if (!CurrentGrabLedge || !bIsInClimb) return 0.0f;
	return SearchedLedgeComponent->DistanceRemaningToRightLimit(CurrentGrabLedge, CurrentGrabPoint, LimitEdgeOffset);
}

void UIcarusCharacterMovement_Climb::OnLanded(const FHitResult & Hit)
{
	SetState(ECS_DefaultIdle);
	ClimbAimDirection = FVector::ZeroVector;
	bCanGroundGrab = false;
	ClimbLandDelegate.Broadcast();
}

void UIcarusCharacterMovement_Climb::InputRightPressed()
{	
	if (!bActive) return;
	ClimbAimDirection.Y = 1.0f;
	if (!bIsInClimb || !bEnableLedgeMovement) return;
	if (!bIsGrabbed) return;
	ENABLE_MOTION
	if (GetCurrentState() == ECS_IdleLimit_Right) return;
	SetState(ECS_ToRight);
}

void UIcarusCharacterMovement_Climb::InputRightRelease()
{
	if (!bActive) return;
	ClimbAimDirection.Y = 0.0f;
	if (!bIsInClimb || !bEnableLedgeMovement) return;
	if (CurrentState != ECS_ToRight) return;
	SetState(ECS_DefaultIdle);
}

void UIcarusCharacterMovement_Climb::InputLeftPressed()
{
	if (!bActive) return;
	ClimbAimDirection.Y = -1.0f;
	if (!bIsInClimb || !bEnableLedgeMovement) return;
	if (!bIsGrabbed) return;
	ENABLE_MOTION
	if (GetCurrentState() == ECS_IdleLimit_Left) return;
	SetState(ECS_ToLeft);
}

void UIcarusCharacterMovement_Climb::InputLeftRelease()
{	
	if (!bActive) return;
	ClimbAimDirection.Y = 0.0f;
	if (!bIsInClimb || !bEnableLedgeMovement) return;
	if (CurrentState != ECS_ToLeft) return;
	SetState(ECS_DefaultIdle);
}

void UIcarusCharacterMovement_Climb::InputUpPressed()
{
	if (!bActive) return;
	ClimbAimDirection.Z = 1.0f;
	if (!bIsInClimb || !bEnableLedgeMovement) return;
}

void UIcarusCharacterMovement_Climb::InputUpRelease()
{
	if (!bActive) return;
	ClimbAimDirection.Z = 0.0f;
	if (!bIsInClimb || !bEnableLedgeMovement) return;
}

void UIcarusCharacterMovement_Climb::InputDownPressed()
{
	if (!bActive) return;
	ClimbAimDirection.Z = -1.0f;
	if (!bIsInClimb || !bEnableLedgeMovement) return;
}

void UIcarusCharacterMovement_Climb::InputDownRelease()
{
	if (!bActive) return;
	ClimbAimDirection.Z = 0.0f;
	if (!bIsInClimb || !bEnableLedgeMovement) return;
}

void UIcarusCharacterMovement_Climb::InputJumpPressed()
{
	InputJumpPressed_DirectionalJump();
	InputJumpPressed_JumpWrongTry();
	InputJumpPressed_GroundGrab();
	InputJumpPressed_FinalLedgeVault();
}

void UIcarusCharacterMovement_Climb::InputJumpReleased() {}

void UIcarusCharacterMovement_Climb::InputJumpPressed_JumpWrongTry()
{
	if (!bIsInClimb || !bEnableLedgeMovement) return;
	if (CurrentGrabLedge->bIsFinalLedge) return;
	if (CurrentState == EClimbState::ECS_DefaultIdle && CurrentState != EClimbState::ECS_JumpTry && (CHECK_AIM_UP || CHECK_ZERO_AIM))
	{
		FString State = UIcarusSystemLibrary::GetCurrentActiveAnimBlueprintState(IcarusPlayerCharacter->GetMesh(), "ClimbMovement");
		if (State == "Idle" || State == "IdleLimit")
		{
			SetState(EClimbState::ECS_JumpTry);
			DISABLE_MOTION
			IcarusCharacterMovement->StopMovementImmediately();
		}
	}
}

void UIcarusCharacterMovement_Climb::InputJumpPressed_FinalLedgeVault()
{
	if (!bIsInClimb || !bEnableLedgeMovement) return;
	if ((CHECK_AIM_UP || CHECK_ZERO_AIM) &&
		(CurrentState == EClimbState::ECS_DefaultIdle ||
		CurrentState == EClimbState::ECS_IdleLimit_Left ||
		CurrentState == EClimbState::ECS_IdleLimit_Right) &&
		CurrentGrabLedge->bIsFinalLedge)
	{
		ENABLE_MOTION
		SetState(ECS_Vaulting);
	}
}

void UIcarusCharacterMovement_Climb::InputJumpPressed_GroundGrab()
{
	if (bIsInClimb) return;
	if (!CanGroundGrab() || STATE_CANNOT_GROUND_JUMP) return;

	bIsInGroundGrabbing = true;
	bGroundGrabAnimationPlaying = true;
	IcarusCharacterMovement->SetMovementMode(EMovementMode::MOVE_Flying);
	IcarusCharacterMovement->StopMovementImmediately();
	IcarusCharacterMovement->SetMovementState(EMovementState::EMS_Climb);
	FirstGrabPoint = AddLedgeOffsetsToPoint(SearchedLedgeComponent->GetTransformedOutLimitPoint(CurrentGrabLedge, CurrentGrabPoint, LimitEdgeOffset), CurrentGrabLedge);
	FTimerHandle Handle;
	GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([&]()
	{
		InterpolingMovePlayer(FirstGrabPoint, (CurrentGrabLedge->GetSegmentNormal()*-1.0f).Rotation(), GroundGrabInterpSpeed, bGroundGrabConstantInterp, &UIcarusCharacterMovement_Climb::FirstGrabGround_InterpResult);
		bGroundGrabAnimationPlaying = false;
		Handle.Invalidate();
	}), GroundGrabDelay, false);
	
}

void UIcarusCharacterMovement_Climb::InputJumpPressed_DirectionalJump()
{
	if (!bIsInClimb || !bEnableLedgeMovement) return;
	if (CHECK_ZERO_AIM) return;
	FString Machine_CurrentState = UIcarusSystemLibrary::GetCurrentActiveAnimBlueprintState(IcarusPlayerCharacter->GetMesh(), "ClimbMovement");
	
	AIcarusLedgeBase* Ledge = nullptr;
	FVector Point;
	bool bCheckOK = false;
	float CurrentDelay = -1.0f;
	float CurrentSpeed = -1.0f;
	bool CurrentUseConstant = false;
	NextStateOnJump = ECS_DefaultIdle;

	if (CHECK_AIM_ONLY_UP && (CurrentState == ECS_DefaultIdle || CurrentState == ECS_IdleLimit_Left || CurrentState == ECS_IdleLimit_Right)
	&& SearchedLedgeComponent->GetPointByDirection(GetAimWorldDirection(), CurrentGrabPoint, Up_Jump_MinDistance, Up_Jump_MaxDistance, Ledge, Point)
	&& (Machine_CurrentState == "Idle" || Machine_CurrentState == "IdleLimit" || Machine_CurrentState == "ToRight" || Machine_CurrentState == "ToLeft"))
	{
		SetState(ECS_JumpToUp);
		bCheckOK = true;
		CurrentDelay = UpJumpDelay;
		CurrentSpeed = UpJumpInterpSpeed;
		CurrentUseConstant = bUpJumpConstantInterp;
	}
	else if (CHECK_AIM_ONLY_DOWN && (CurrentState == ECS_DefaultIdle || CurrentState == ECS_IdleLimit_Left || CurrentState == ECS_IdleLimit_Right)
	&& SearchedLedgeComponent->GetPointByDirection(GetAimWorldDirection(), CurrentGrabPoint, Down_Jump_MinDistance, Down_Jump_MaxDistance, Ledge, Point)
	&& (Machine_CurrentState == "Idle" || Machine_CurrentState == "IdleLimit" || Machine_CurrentState == "ToRight" || Machine_CurrentState == "ToLeft"))
	{
		SetState(ECS_JumpToDown);
		bCheckOK = true;
		CurrentDelay = DownJumpDelay;
		CurrentSpeed = DownJumpInterpSpeed;
		CurrentUseConstant = bDownJumpConstantInterp;
	}
	else if (CHECK_AIM_ONLY_LEFT && CurrentState == ECS_IdleLimit_Left &&
		SearchedLedgeComponent->GetPointByDirection(GetAimWorldDirection(), CurrentGrabPoint, Lateral_Jump_MinDistance, Lateral_Jump_MaxDistance, Ledge, Point)
		&& Machine_CurrentState == "LimitLeft")
	{
		SetState(ECS_JumpToLeft);
		bCheckOK = true;
		CurrentDelay = LateralJumpDelay;
		CurrentSpeed = LateralJumpInterpSpeed;
		CurrentUseConstant = bLateralJumpConstantInterp;
		NextStateOnJump = ECS_IdleLimit_Right;
	}
	else if (CHECK_AIM_ONLY_RIGHT && CurrentState == ECS_IdleLimit_Right &&
	SearchedLedgeComponent->GetPointByDirection(GetAimWorldDirection(), CurrentGrabPoint, Lateral_Jump_MinDistance, Lateral_Jump_MaxDistance, Ledge, Point)
	&& Machine_CurrentState == "LimitRight")
	{
		SetState(ECS_JumpToRight);
		bCheckOK = true;
		CurrentDelay = LateralJumpDelay;
		CurrentSpeed = LateralJumpInterpSpeed;
		CurrentUseConstant = bLateralJumpConstantInterp;
		NextStateOnJump = ECS_IdleLimit_Left;
	}
	else if (CHECK_AIM_ONLY_DOWN)
	{
		if(bIsGrabbed && (Machine_CurrentState == "Idle" || Machine_CurrentState == "IdleLimit")) LedgeExit();
	}

	if (bCheckOK)
	{
		ENABLE_MOTION
		DisableMovement();
		bIsGrabbed = false;
		FTimerHandle Handle;
		GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateUObject(this, &UIcarusCharacterMovement_Climb::InLedgeJumpAction, Point, Ledge, CurrentSpeed, CurrentUseConstant), CurrentDelay, false);	
	}
}

void UIcarusCharacterMovement_Climb::InLedgeJumpAction(FVector Point, AIcarusLedgeBase* Ledge, float Speed, bool bUseConstant)
{
	DISABLE_MOTION;
	IcarusCharacterMovement->StopMovementImmediately();
	bMovingInterLedges = true;
	FVector FinalPoint = AddLedgeOffsetsToPoint(SearchedLedgeComponent->GetTransformedOutLimitPoint(Ledge, Point, LimitEdgeOffset), Ledge);
	InterpolingMovePlayer(FinalPoint, (Ledge->GetSegmentNormal()*-1.0f).Rotation(), Speed, bUseConstant, &UIcarusCharacterMovement_Climb::InterLedge_Land);
	CurrentGrabLedge = Ledge;
	CurrentGrabPoint = Point;
	VerifyAimLimits();
}

void UIcarusCharacterMovement_Climb::InterLedge_Land()
{
	bIsGrabbed = true;
	bMovingInterLedges = false;
	SetState(NextStateOnJump);
	EnableMovement();
	if ((NextStateOnJump == ECS_IdleLimit_Left || NextStateOnJump == ECS_DefaultIdle) && CHECK_AIM_ONLY_RIGHT) SetState(ECS_ToRight);
	if ((NextStateOnJump == ECS_IdleLimit_Right || NextStateOnJump == ECS_DefaultIdle) && CHECK_AIM_ONLY_LEFT) SetState(ECS_ToLeft);
	IcarusCharacterMovement->StopMovementImmediately();
}

void UIcarusCharacterMovement_Climb::LedgeExit()
{
	if (GetCurrentState() == ECS_Exiting || !bIsGrabbed) return;
	IcarusCharacterMovement->MovementAir->SetUpdateAirControl(false);
	DefaultsClimbExit();
	SetState(ECS_Exiting);
	IcarusCharacterMovement->SetMovementState(EMovementState::EMS_Jump);
	IcarusCharacterMovement->SetMovementMode(EMovementMode::MOVE_Falling);
}

void UIcarusCharacterMovement_Climb::DefaultsClimbExit()
{
	SetInClimb(false);
	bIsGrabbed = false;
	if(CurrentGrabLedge) CurrentGrabLedge->RestoreCollisionToPawn();
	CurrentGrabLedge = nullptr;
	bIsInGroundGrabbing = false;
	IcarusCharacterMovement->MovementGround->bActive = true;
	IcarusCharacterMovement->MovementAir->bActive = true;
}

void UIcarusCharacterMovement_Climb::FinalVaultExit()
{
	DefaultsClimbExit();
	if (CHECK_AIM_LEFT || CHECK_AIM_RIGHT || CHECK_AIM_UP)
	{
		IcarusCharacterMovement->SetMovementState(EMovementState::EMS_Moving);
		IcarusCharacterMovement->SetMovementMode(EMovementMode::MOVE_Walking);
	}
	else
	{
		IcarusCharacterMovement->SetMovementState(EMovementState::EMS_Stop);
		IcarusCharacterMovement->SetMovementMode(EMovementMode::MOVE_Walking);
	}

	SetState(ECS_DefaultIdle);
}

FVector UIcarusCharacterMovement_Climb::AddLedgeOffsetsToPoint(FVector Point, AIcarusLedgeBase* Ledge)
{
	return SearchedLedgeComponent->AddHeadReferenceCapsuleOffset(SearchedLedgeComponent->AddLedgeCapsuleOffset(Point,Ledge));
}

void UIcarusCharacterMovement_Climb::Update_Internal(float DeltaTime)
{
	Update_CurrentPoint();
	Update_NearlyGrabCheck();
	Update_GroundGrabCheck();
	Update_InterpolingMovePlayer();
	Update_CheckSegmentLimit();	
	Update_AimLimit();
}