// Fill out your copyright notice in the Description page of Project Settings.

#include "IcarusCharacterMovement_Swim.h"
#include "IcarusCharacterMovement_Ground.h"
#include "IcarusCharacterMovement_Air.h"
#include "IcarusCharacterMovement.h"
#include "Player/IcarusPlayerCharacter.h"
#include "Player/IcarusPlayerController.h"
#include "Camera/IcarusCameraComponent.h"
#include "Volume/IcarusWaterVolume.h"
#include "Icarus.h"

UIcarusCharacterMovement_Swim::UIcarusCharacterMovement_Swim()
{
	bActive = false;
}

void UIcarusCharacterMovement_Swim::Initialize()
{
	Super::Initialize();

	EmergeCameraSettings.TargetArmLength = 600.f;
	EmergeCameraSettings.TargetArmTargetOffset = FVector(0.f, 0.f, IcarusPlayerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 10.f);
	EmergeCameraSettings.TargetArmTargetOffsetInterpSpeed = 0.f;
	EmergeCameraSettings.ViewPitchMin = -45.f;
	EmergeCameraSettings.ViewPitchMax = 25.f;

	ImmerseCameraSettings.TargetArmLength = 600.f;
	ImmerseCameraSettings.TargetArmTargetOffset = FVector::ZeroVector;
	ImmerseCameraSettings.TargetArmTargetOffsetInterpSpeed = 0.f;
	ImmerseCameraSettings.ViewPitchMin = -45.f;
	ImmerseCameraSettings.ViewPitchMax = 75.f;
	ImmerseCameraSettings.bEnableObjectsTranslucency = false;
}

void UIcarusCharacterMovement_Swim::Emerge()
{
	CHECK_FLOW(!bFellInWater);

	ActorClientMessage("Emerging...");
	bIsInSurfaceWater = true;
	IcarusCharacterMovement->StopMovementImmediately();

	// Pega control rotation
	FRotator ControlRotation = IcarusPlayerCharacter->GetControlRotation();
	FRotator NewRotation = IcarusPlayerCharacter->GetActorRotation();

	// Se player estiver se movendo
	if (bAlreadyImmerseOnce &&
		IcarusPlayerCharacter->GetIcarusCharacterMovement()->MovementGround->IsMoving())
	{
		NewRotation = ControlRotation.RotateVector(IcarusPlayerCharacter->GetIcarusCharacterMovement()->MovementGround->GetTargetDirection()).Rotation();
	}

	// Zera valores de pitch e roll
	NewRotation.Pitch = NewRotation.Roll = 0.f;

	// Aplica em swim target rotations
	SwimTargetRotation_Internal = NewRotation;
	SwimTargetRotation = SwimTargetRotation_Internal;
	IcarusPlayerCharacter->SetActorRotation(SwimTargetRotation);

	// Seta configuracao de camera
	IcarusPlayerCharacter->Camera->SetTransientSettings(EmergeCameraSettings);
	
	// Chama broadcast para emerge event.
	bFellEmerged = false;
	EmergeDelegate.Broadcast();
}

void UIcarusCharacterMovement_Swim::Immerse()
{
	ActorClientMessage("Immersing...");

	// Seta ja imergiu uma vez.
	bAlreadyImmerseOnce = true;

	// Seta superficie falso
	bIsInSurfaceWater = false;

	// Seta configuracao de camera
	IcarusPlayerCharacter->Camera->SetTransientSettings(ImmerseCameraSettings);

	// Adiciona impulso
	AddSwimImpulse(250.f);

	// Chama broadcast para emerge event. Se o procedimento de cair na agua nao foi criado
	bFellInWater = false;
	ImmerseDelegate.Broadcast();
}

void UIcarusCharacterMovement_Swim::Fall()
{
	ActorClientMessage("Caindo na agua...");
	bFellInWater = true;

	SwimTargetRotation_Internal = IcarusPlayerCharacter->GetActorRotation();
	SwimTargetRotation_Internal.Pitch = 0.f;
	SwimTargetRotation = SwimTargetRotation_Internal;
	IcarusPlayerCharacter->SetActorRotation(SwimTargetRotation_Internal);

	// Impulso ao cair
	FVector CurrentVelocity = IcarusPlayerCharacter->GetVelocity();
	ActorClientMessage("Velocidade atual: %s", *CurrentVelocity.ToString());

	// Zera velocity
	IcarusCharacterMovement->Velocity = FVector::ZeroVector;
	
	// Se velocidade atual em z for que -600
	if (CurrentVelocity.Z < -600.f)
	{
		// Aplica impulso
		FVector Impulse;
		// FVector Impulse = CurrentVelocity * (CurrentWaterVolume->bDiving ? 0.75f : 0.75f);
		if (CurrentWaterVolume->bDiving)
		{
			Impulse = CurrentVelocity * 0.75f;
		}
		else
		{
			FVector ImpulseDirection = FRotator(0.f, IcarusPlayerCharacter->GetActorRotation().Yaw, 0.f).Vector();
			Impulse = ImpulseDirection * 300.f;
			Impulse.Z = CurrentVelocity.Z * 0.75f;
		}
		IcarusCharacterMovement->AddImpulse(Impulse, true);
		ActorClientMessage("Impulso adicionado: %s", *Impulse.ToString());

		// Bloqueia update function
		bBlockTemporaryUpdate = true;

		// Cria timer para desbloquear a funcao de atualizacao
		IcarusPlayerCharacter->GetWorldTimerManager().ClearTimer(FTH_BlockTemporaryUpdate);
		IcarusPlayerCharacter->GetWorldTimerManager().SetTimer(FTH_BlockTemporaryUpdate,
			this,
			&UIcarusCharacterMovement_Swim::Reset_BlockTemporaryUpdate_Timer, 
			0.25f);

		// Chama delegate
		FallDelegate.Broadcast();
	}
	else
	{
		ActorClientMessage("Caindo na agua emergido...");

		bFellEmerged = true;
		bFellInWater = false;

		// Executa delegate
		FallEmergedDelegate.Broadcast();
	}
}

FVector UIcarusCharacterMovement_Swim::GetSwimImpulseValue(float Amount)
{
	// Rotaciona o control rotation baseado na direcao de movimento
	FVector LocalCurrentTargetDirection = IcarusPlayerCharacter->GetControlRotation().RotateVector(IcarusCharacterMovement->MovementGround->GetTargetDirection());

	// Se estiver na supperficie ou estiver caindo
	if (bIsInSurfaceWater || bFellInWater)
	{
		// Zera valor de impulso para baixo ou cima.
		// OBS: caso apertar para mergulhar (CurrentTargetDirection.Z < 0) entao bIsInSurfaceWater == false por estrutura em tick linha 740
		LocalCurrentTargetDirection.Z = 0.f;
	}
	return LocalCurrentTargetDirection * Amount;
}

void UIcarusCharacterMovement_Swim::AddSwimImpulse(float Amount)
{
	if (bActive)
	{
		FVector Impulse = GetSwimImpulseValue(Amount);
		IcarusCharacterMovement->AddImpulse(Impulse, true);
	}
}

void UIcarusCharacterMovement_Swim::SetIsInWater(bool bNewBool)
{
	if (IcarusCharacterMovement->HasBegunPlay())
	{
		// Chama delegates.
		if (bNewBool)
		{
			EnterTheWater.Broadcast();

			// Limpa timer de ativacao de movement air e desabilita movement air
			IcarusPlayerCharacter->GetWorldTimerManager().ClearTimer(FTH_EnableMovementAir);
			IcarusCharacterMovement->MovementAir->bActive = false;

			// Salva valor de camera lag position atual
			SavedCameraLagSpeed = IcarusPlayerCharacter->Arm->CameraLagSpeed;
		}
		else
		{
			LeaveTheWater.Broadcast();

			// Limpa timer e seta timer para ativacao de movement air
			IcarusPlayerCharacter->GetWorldTimerManager().ClearTimer(FTH_EnableMovementAir);
			IcarusPlayerCharacter->GetWorldTimerManager().SetTimer(FTH_EnableMovementAir,
				this,
				&UIcarusCharacterMovement_Swim::EnableMovementAir_Timer, 1.f);

			bAlreadyImmerseOnce = false;
			bFellInWater = false;
			bIsInSurfaceWater = false;
			IcarusPlayerCharacter->Arm->CameraLagSpeed = SavedCameraLagSpeed;
		}

		// Seta is in water
		bActive = bNewBool;

		// Seta valores core
		IcarusCharacterMovement->MovementGround->bActive = !bActive;

		// Atualiza movement ground target direction caso desativacao
		if (!bActive)
		{
			IcarusCharacterMovement->MovementGround->UpdateMovementTargetDirection();
			IcarusCharacterMovement->MovementGround->UpdateMovementTargetAngle();
		}

		// Seta tipo de root motion
		UIcarusSystemLibrary::SetPlayerRootMotionMode(IcarusPlayerCharacter, 
			bActive ? ERootMotionMode::IgnoreRootMotion : ERootMotionMode::RootMotionFromEverything);
	}
}

FVector UIcarusCharacterMovement_Swim::GetHeadLocation() const
{
	return IcarusPlayerCharacter->GetActorLocation() + FVector(0.f, 0.f, IcarusPlayerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}

FVector UIcarusCharacterMovement_Swim::GetFootLocation() const
{
	return IcarusPlayerCharacter->GetActorLocation() - FVector(0.f, 0.f, IcarusPlayerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}

bool UIcarusCharacterMovement_Swim::IsInCoastWater() const
{
	CHECK_FLOW_WITH_RETURN_NO_LOG(CurrentWaterVolume, false);
	//STATIC_CHECK_WITH_MESSAGE_WITH_VALUE(IcarusPlayerCharacter, CurrentWaterVolume, "UIcarusCharacterMovement_Swim::IsInCoastWater: water volume precisa ser valido.", false);
	return CurrentWaterVolume->GetBrushComponent()->IsOverlappingComponent(IcarusPlayerCharacter->GetCapsuleComponent()) && 
		!bActive;
}

bool UIcarusCharacterMovement_Swim::IsDiving() const
{
	return IS_VALID(CurrentWaterVolume) && CurrentWaterVolume->bDiving;
}

float UIcarusCharacterMovement_Swim::GetDivingDistanceAlpha() const
{
	return IS_VALID(CurrentWaterVolume) && CurrentWaterVolume->GetDivingDistanceAlpha();
}

void UIcarusCharacterMovement_Swim::EnableMovementAir_Timer()
{
	IcarusCharacterMovement->MovementAir->bActive = true;
}

void UIcarusCharacterMovement_Swim::Reset_BlockTemporaryUpdate_Timer()
{
	bBlockTemporaryUpdate = false;
}

void UIcarusCharacterMovement_Swim::EnableAdjustmentUnderWaterHighPitch()
{
	bUpdateAdjustmentUnderWaterHighPitch = true;
}

void UIcarusCharacterMovement_Swim::ClearAdjustmentUnderWaterHighPitch()
{
	IcarusPlayerCharacter->GetWorldTimerManager().ClearTimer(FTH_EnableAdjustmentUnderWaterHighPitch);
	bUpdateAdjustmentUnderWaterHighPitch = false;
}

void UIcarusCharacterMovement_Swim::Update_Internal(float DeltaTime)
{
	CHECK_FLOW_NO_LOG(!bBlockTemporaryUpdate);

	// Novo vector de direcao.
	FVector CurrentTargetDirection = IcarusCharacterMovement->MovementGround->GetTargetDirection();

	// Se estiver na superficie
	if (bIsInSurfaceWater)
	{
		// Se o z direction foi atualizado para mergulho < 0, desabilita surface water
		if (CurrentTargetDirection.Z < 0.f)
		{
			CurrentWaterVolume->Immerse();
			CurrentWaterVolume->bForcedImmerse = true;
		}
		else
		{
			CurrentTargetDirection.Z = 0.f;
		}
	}

	// Se estiver se movendo faz calculo para rotacao.
	if (CurrentTargetDirection.Size())
	{
		IcarusPlayerCharacter->Arm->CameraLagSpeed = FMath::FInterpConstantTo(IcarusPlayerCharacter->Arm->CameraLagSpeed, 5.f, DeltaTime, 1.f);

		// Limpa procedimento de ajuste de pitch alto.
		ClearAdjustmentUnderWaterHighPitch();

		// Novo vector de direcao.
		CurrentTargetDirection.Y *= -1;
		CurrentTargetDirection.Z *= -1;

		// Guarda control rotation
		FRotator ControlRotation = IcarusPlayerCharacter->GetControlRotation();

		// Verifica se movimentacao em x for pressionada
		if (CurrentTargetDirection.X == 0.f || bIsInSurfaceWater)
		{
			ControlRotation.Pitch = 0.f;
		}

		// Guarda rotacao target
		FRotator CurrentTargetDirectionRotation = CurrentTargetDirection.Rotation();
		SwimTargetRotation_Internal = (ControlRotation - CurrentTargetDirectionRotation).GetNormalized();
		SwimTargetRotation_Internal.Pitch -= 89.9f;

		// Adiciona impulso
		float NewSwimSpeed = bIsInSurfaceWater ? SwimSpeed * 1.25f : SwimSpeed;
		float SwimResult = NewSwimSpeed * DeltaTime;
		AddSwimImpulse(SwimResult);
	}
	else
	{
		IcarusPlayerCharacter->Arm->CameraLagSpeed = FMath::FInterpConstantTo(IcarusPlayerCharacter->Arm->CameraLagSpeed, 1.f, DeltaTime, 1.f);

		// Se estiver na superficie mantem pitch em 0.f
		if (!bIsInSurfaceWater)
		{
			// Se embaixo dagua o pitch for muito alpha, maior que -90, no caso, -180.f a 0.f
			if (!bUpdateAdjustmentUnderWaterHighPitch &&
				!IcarusPlayerCharacter->GetWorldTimerManager().IsTimerActive(FTH_EnableAdjustmentUnderWaterHighPitch) &&
				SwimTargetRotation_Internal.Pitch < -90.f)
			{
				IcarusPlayerCharacter->GetWorldTimerManager().SetTimer(FTH_EnableAdjustmentUnderWaterHighPitch,
					this,
					&UIcarusCharacterMovement_Swim::EnableAdjustmentUnderWaterHighPitch,
					2.f);
			}

			// Faz atualizacoes de high pitch.
			else if (bUpdateAdjustmentUnderWaterHighPitch)
			{
				// Adiciona offset para pitch com base em alpha. Quanto mais longe do pitch 0, mais rapido vai.
				SwimTargetRotation_Internal.Pitch += DeltaTime * 20.f;
			}

			// Se pitch for valido, maior que -90.f
			if (SwimTargetRotation_Internal.Pitch > -90.f)
			{
				ClearAdjustmentUnderWaterHighPitch();
			}
		}
		else
		{
			SwimTargetRotation_Internal.Pitch = SwimTargetRotation_Internal.Roll = 0.f;
		}
	}

	// Cria slerp
	SwimTargetRotation = FMath::RInterpTo(SwimTargetRotation, SwimTargetRotation_Internal, DeltaTime, 4.f);
	IcarusPlayerCharacter->SetActorRotation(SwimTargetRotation);
}