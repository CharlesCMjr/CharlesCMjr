// Fill out your copyright notice in the Description page of Project Settings.

#include "IcarusCharacterMovement_Slide.h"
#include "IcarusCharacterMovement_Ground.h"
#include "IcarusCharacterMovement_Air.h"
#include "IcarusCharacterMovement.h"
#include "IcarusPlayerAnimInstance.h"
#include "IcarusPlayerCharacter.h"
#include "IcarusSlideVolume.h"
#include "FMODStudio/Classes/FMODBlueprintStatics.h"
#include "FMODStudio/Classes/FMODAudioComponent.h"
#include "Icarus.h"


FVector UIcarusCharacterMovement_Slide::GetCurrentTotalForces()
{
	return CurrentTotalForces;
}

FVector UIcarusCharacterMovement_Slide::GetCurrentDirectionInput()
{
	return CurrentDirectionInput;
}

FVector UIcarusCharacterMovement_Slide::GetCurrentDirectionLateralImpulse()
{
	return CurrentDirectionLateralImpulse;
}

FVector UIcarusCharacterMovement_Slide::GetCurrentDirectionImpulse()
{
	return CurrentDirectionImpulse;
}

void UIcarusCharacterMovement_Slide::GetTransformedImpactNormal(FVector & LocalImpactNormal, FVector &  WorldImpactNormal)
{
	//Caso nao exista volume, retorna zero para world e local impact normal.
	if (!CurrentVolumeReference)
	{
		WorldImpactNormal = LocalImpactNormal = FVector::ZeroVector;
		return;
	}

	//impact normal world.
	WorldImpactNormal = SurfaceTrace().ImpactNormal;

	//converte impact normal para local do box
	LocalImpactNormal = UKismetMathLibrary::InverseTransformDirection(CurrentVolumeReference->GetActorTransform(), WorldImpactNormal);
}

float UIcarusCharacterMovement_Slide::GetBoxCurrentRate()
{
	//Sai do update caso nao o tenha a volume overlapado nao exista
	if (!CurrentVolumeReference) return -1.f;

	//Pega box extent
	FVector Origin, BoxExtent;
	CurrentVolumeReference->GetActorBounds(false, Origin, BoxExtent);

	//Calcula ponto final e inicial do box
	FinalBoxPoint = CurrentVolumeReference->GetActorLocation() + CurrentVolumeReference->DirectionImpulse->GetComponentRotation().Vector()*BoxExtent;
	InitialBoxPoint = CurrentVolumeReference->GetActorLocation() + CurrentVolumeReference->DirectionImpulse->GetComponentRotation().Vector()*-1 * BoxExtent;

	//Calcula tamanho do vetor com base na distancia entre o ponto e a capsula
	float LenghtInitial = ((InitialBoxPoint - IcarusPlayerCharacter->GetCapsuleComponent()->GetComponentLocation())*CurrentVolumeReference->DirectionImpulse->GetComponentRotation().Vector()).Size();
	float LenghtFinal = ((FinalBoxPoint - IcarusPlayerCharacter->GetCapsuleComponent()->GetComponentLocation())*CurrentVolumeReference->DirectionImpulse->GetComponentRotation().Vector()).Size();
	float TotalBoxLenght = LenghtInitial + LenghtFinal;

	return (1.0f - (FMath::Clamp(LenghtFinal / TotalBoxLenght, 0.0f, 1.0f)));
}

float UIcarusCharacterMovement_Slide::GetAlphaDirection()
{
	//Caso seja menor que zero retorna zero
	if (TargetInputRotation < 0)
	{
		LastAlphaDirection = 0.0f;
	}

	//Caso seja maior que zero retorna 1
	if (TargetInputRotation > 0)
	{
		LastAlphaDirection = 1.0f;
	}

	//Retorna o valor atual para nao haver mudanca na animacao do state.
	return LastAlphaDirection;
}

void UIcarusCharacterMovement_Slide::Initialize() 
{
	Super::Initialize();

	CastChecked<UIcarusPlayerAnimInstance>(IcarusPlayerCharacter->GetMesh()->GetAnimInstance())->AntiFlowSlide_Delegate.BindUObject(this, &UIcarusCharacterMovement_Slide::Event_ExitingFallState);
}

bool UIcarusCharacterMovement_Slide::IsInSlideSystem()
{
	//Encapsulamento bIsInSlideVolume
	return bIsInSlideVolume;
}

bool UIcarusCharacterMovement_Slide::IsSliding()
{
	if (!IsInSlideSystem()) return false;

	//Verifica se o movement state atual eh sliding e retorna true.
	if (IcarusCharacterMovement->CurrentMovementState == EMS_Sliding)
	{
		return true;
	}

	return false;
}

bool UIcarusCharacterMovement_Slide::IsInAntiFlowAngle()
{
	//Encapsulamento bIsInAntiFlow
	if (!IsInSlideSystem()) return false;

	return bIsInAntiFlow;
}

void UIcarusCharacterMovement_Slide::CheckIsInAntiFlowAngle()
{
	if (!IsInSlideSystem()) bIsInAntiFlow = false;

	//Local para onde ele esta "pressionando"
	FVector VelocityDirection = (IcarusPlayerCharacter->IcarusCharacterMovement->Velocity*FVector(1.f, 1.f, 0.f)).GetSafeNormal();

	//Dot da velocidade com a direcao do impulso do volume
	float ForwardFlowDot = FVector::DotProduct(RawCurrentDirectionImpulse*FVector(1.f, 1.f, 0.f), VelocityDirection);

	//Dot angle.
	ForwardFlowDotAngle = UKismetMathLibrary::DegAcos(ForwardFlowDot);

	//Se for maior que 90, ou seja, estiver tentando entrar contra o fluxo de impulso
	//se for menor que 90 executa imediatamente
	if (ForwardFlowDotAngle < 90.0f)
	{
		bIsInAntiFlow = false;
	}
	else
	{
		bIsInAntiFlow = true;
	}
}

void UIcarusCharacterMovement_Slide::ChangeOtherMovementTypesActivation(bool Active)
{
	if (Active)
	{
		//Timer pra rodar FrontEndTimer para dar tempo de rodar anime de end front
		if (FrontEndTH.IsValid() == false)
		{
			FVector SavedVelocity = IcarusCharacterMovement->Velocity.GetSafeNormal();
			IcarusPlayerCharacter->GetWorld()->GetTimerManager().SetTimer(FrontEndTH, this, &UIcarusCharacterMovement_Slide::FrontEndTimer, FrontEndEnable_TH, false);
			IcarusPlayerCharacter->GetMesh()->GetAnimInstance()->RootMotionMode = ERootMotionMode::RootMotionFromEverything;
		}
	}
	else
	{
		//Seta moviment state pra sliding e retira rootmotion
		IcarusCharacterMovement->SetMovementState(EMovementState::EMS_Sliding);
		IcarusPlayerCharacter->GetMesh()->GetAnimInstance()->RootMotionMode = ERootMotionMode::IgnoreRootMotion;
		IcarusCharacterMovement->MovementGround->MovementAlpha = 0.0f;

		//Ativa ou desativa sistemas de movimento em chao e ar.
		IcarusCharacterMovement->MovementGround->bActive = Active;
		IcarusCharacterMovement->MovementAir->bActive = Active;
	}
}

void UIcarusCharacterMovement_Slide::ActiveSlideState(AIcarusSlideVolume * OverlappedVolume)
{
	
	if (!IcarusPlayerCharacter) return;

	//Seta CurrentVolumeReference
	CurrentVolumeReference = OverlappedVolume;

	if (bIsInSlideVolume) return;

	//Sai do sistema caso nao exista volume
	if (!CurrentVolumeReference) return;

	//Reseta notify para virar mesh
	bNotify_AntiFlow_NegateDirection = false;

	//Seta que esta dentro do volume de slide
	bIsInSlideVolume = true;

	//Seta a direction impulse
	RawCurrentDirectionImpulse = CurrentVolumeReference->DirectionImpulse->GetComponentRotation().Vector();

	CurrentDirectionImpulse = RawCurrentDirectionImpulse;

	//Salva vars.
	SaveVarsOnOverlap();
}

void UIcarusCharacterMovement_Slide::DeactiveSlideState(AIcarusSlideVolume * OverlappedVolume)
{
	if (OverlappedVolume != CurrentVolumeReference || !bIsInSlideVolume) return;

	//Restaura interp da anim bp que faz o blend entre as duas poses
	Cast<UIcarusPlayerAnimInstance>(IcarusPlayerCharacter->GetMesh()->GetAnimInstance())->LastAlphaSlide = 0.0f;

	//Seta pra true os sistemas que foram desativados, e recupera root motion
	ChangeOtherMovementTypesActivation(true);

	//Reseta rotacao do personagem que foi alterada durante a entra no sistema
	ResetCharacterRotation();

	//Reseta in slide bool
	bIsInSlideVolume = false;

	//Reseta anti flow bool
	bIsInAntiFlow = false;

	//Reseta bool que libera o update da rotacao.
	bCanUpdateCapsuleRotation = false;

	// Anula CurrentVolumeReference
	CurrentVolumeReference = NULL;

	//Reseta particula
	for (UParticleSystemComponent * P : SpawnedSlidingParticle)
	{
		if (IS_VALID(P)) P->Deactivate();
	}
	
	//Stopa som de loop sliding
	if (IS_VALID(SpawnedSlidingSound)) SpawnedSlidingSound->Stop();
		
	//Reseta ultimo input clicado.
	LastInputRotation = 0.0f;

	//Restaura vars
	ResetVars();


}

void UIcarusCharacterMovement_Slide::CreateFakeCapsuleCollision()
{
	//Cria component de capsula para substituir colisao da original
	if (!IS_VALID(SlideCapsuleCollision))
	{
		SlideCapsuleCollision = NewObject<UCapsuleComponent>(IcarusCharacterMovement, "SlideCapsuleCollision");
		if (SlideCapsuleCollision)
		{
			SlideCapsuleCollision->RegisterComponent();
			SlideCapsuleCollision->AttachToComponent(IcarusPlayerCharacter->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			SlideCapsuleCollision->SetCapsuleHalfHeight(IcarusPlayerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
			SlideCapsuleCollision->SetCapsuleRadius(IcarusPlayerCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius());
			SlideCapsuleCollision->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
		}
	}
}

void UIcarusCharacterMovement_Slide::AntiFlowInitSlide()
{
	//Stopa totalmente o movemento.
	IcarusCharacterMovement->StopMovementImmediately();

	//Cria o timer que vai executar o FallAdvanceTimer, tempo necessario para a anime de fall anti flow executar o inicio.
	if (AntiFlowFallTH.IsValid() == false)
	{
		IcarusPlayerCharacter->GetWorld()->GetTimerManager().SetTimer(AntiFlowFallTH, this, &UIcarusCharacterMovement_Slide::AntiFlowFallTimer, AF_TimerTickEnable_TH, false);
	}

	//Cria timer handle e delegate por lambda pra gerar som e particula quando entra em anti flow.
	FTimerHandle AF_SoundParticle_TH;
	FTimerDelegate Delegate_AF_SoundParticle_TH;

	Delegate_AF_SoundParticle_TH.BindLambda([this, &AF_SoundParticle_TH]
	{
		if (!CurrentVolumeReference) return;
		if (CurrentVolumeReference->AF_EntryParticle) UGameplayStatics::SpawnEmitterAtLocation(IcarusPlayerCharacter->GetWorld(),
			CurrentVolumeReference->AF_EntryParticle,
			FTransform(IcarusPlayerCharacter->GetMesh()->GetSocketLocation("C_Hips_01")),
			true);

		if(CurrentVolumeReference->AF_EntrySound) UFMODBlueprintStatics::PlayEventAtLocation(IcarusPlayerCharacter->GetMesh(),
			CurrentVolumeReference->AF_EntrySound,
			FTransform(IcarusPlayerCharacter->GetMesh()->GetSocketLocation("C_Hips_01")),
			true);

		AF_SpawnParticle();

		if (IS_VALID(SpawnedSlidingSound)) SpawnedSlidingSound->Stop();

		SpawnedSlidingSound = (CurrentVolumeReference->AF_SlidingSound) ? UFMODBlueprintStatics::PlayEventAttached(CurrentVolumeReference->AF_SlidingSound,
			IcarusPlayerCharacter->GetMesh(), 
			"C_Hips_01", 
			FVector(0.0f), 
			EAttachLocation::SnapToTarget, 
			true, 
			true) : nullptr;

		AF_SoundParticle_TH.Invalidate();
	});

	IcarusPlayerCharacter->GetWorld()->GetTimerManager().SetTimer(AF_SoundParticle_TH, Delegate_AF_SoundParticle_TH, 0.25f, false);
}

void UIcarusCharacterMovement_Slide::NormalFlowInitSlide()
{
	//Stopa totalmente o movemento.
	IcarusCharacterMovement->StopMovementImmediately();

	//seta impulso iniical e frictions para normal flow.
	IcarusCharacterMovement->Velocity = RawCurrentDirectionImpulse*CurrentVolumeReference->InitialSlideForce;
	IcarusCharacterMovement->BrakingFrictionFactor = 1.0f;
	IcarusCharacterMovement->BrakingFriction = 0.0f;
	IcarusCharacterMovement->BrakingDecelerationWalking = 0.0f;
	IcarusCharacterMovement->GroundFriction =0.0f;
	IcarusCharacterMovement->Mass = 1.0f;

	//Dita que a capsula ja pode ser rotacionada.
	bCanUpdateCapsuleRotation = true;

	//Cria timer handle e delegate por lambda pra gerar som e particula quando entra em normal flow.
	FTimerHandle NF_SoundParticle_TH;
	FTimerDelegate Delegate_NF_SoundParticle_TH;

	Delegate_NF_SoundParticle_TH.BindLambda([&]()
	{
		if (!CurrentVolumeReference) return;
		if(CurrentVolumeReference->EntryParticle) UGameplayStatics::SpawnEmitterAtLocation(IcarusPlayerCharacter->GetWorld(),
		CurrentVolumeReference->EntryParticle,
		FTransform(IcarusPlayerCharacter->GetMesh()->GetSocketLocation("C_Hips_01")),
		true);

		if (CurrentVolumeReference->EntrySound) UFMODBlueprintStatics::PlayEventAtLocation(IcarusPlayerCharacter->GetMesh(),
			CurrentVolumeReference->EntrySound,
			FTransform(IcarusPlayerCharacter->GetMesh()->GetSocketLocation("C_Hips_01")),
			true);

		SpawnParticle();

		if (IS_VALID(SpawnedSlidingSound)) SpawnedSlidingSound->Stop();

		SpawnedSlidingSound = (CurrentVolumeReference->SlidingSound) ? UFMODBlueprintStatics::PlayEventAttached(CurrentVolumeReference->SlidingSound,
			IcarusPlayerCharacter->GetMesh(),
			"C_Hips_01",
			FVector(0.0f),
			EAttachLocation::SnapToTarget,
			true,
			true) : nullptr;

		NF_SoundParticle_TH.Invalidate();
	});

	IcarusPlayerCharacter->GetWorld()->GetTimerManager().SetTimer(NF_SoundParticle_TH, Delegate_NF_SoundParticle_TH, 0.5f, false);
}

void UIcarusCharacterMovement_Slide::SpawnParticle()
{
	SpawnedSlidingParticle[0] = (CurrentVolumeReference->HipsSlidingParticle) ? UGameplayStatics::SpawnEmitterAttached(CurrentVolumeReference->HipsSlidingParticle,
		IcarusPlayerCharacter->GetMesh(),
		"C_Hips_01",
		FVector(0.0f),
		FRotator(0.0f),
		EAttachLocation::SnapToTarget,
		false) : nullptr;

	SpawnedSlidingParticle[1] = (CurrentVolumeReference->AnkleSlidingParticle) ? UGameplayStatics::SpawnEmitterAttached(CurrentVolumeReference->AnkleSlidingParticle,
		IcarusPlayerCharacter->GetMesh(),
		"L_Ankle_01",
		FVector(0.0f),
		FRotator(0.0f),
		EAttachLocation::SnapToTarget,
		false) : nullptr;

	SpawnedSlidingParticle[2] = (CurrentVolumeReference->KneeSlidingParticle) ? UGameplayStatics::SpawnEmitterAttached(CurrentVolumeReference->KneeSlidingParticle,
		IcarusPlayerCharacter->GetMesh(),
		"R_Knee_01",
		FVector(0.0f),
		FRotator(0.0f),
		EAttachLocation::SnapToTarget,
		false) : nullptr;
}

void UIcarusCharacterMovement_Slide::AF_SpawnParticle()
{
	SpawnedSlidingParticle[0] = (CurrentVolumeReference->AF_HipsSlidingParticle) ? UGameplayStatics::SpawnEmitterAttached(CurrentVolumeReference->AF_HipsSlidingParticle,
		IcarusPlayerCharacter->GetMesh(),
		"C_Hips_01",
		FVector(0.0f),
		FRotator(0.0f),
		EAttachLocation::SnapToTarget,
		false) : nullptr;

	SpawnedSlidingParticle[1] = (CurrentVolumeReference->AF_AnkleSlidingParticle) ? UGameplayStatics::SpawnEmitterAttached(CurrentVolumeReference->AF_AnkleSlidingParticle,
		IcarusPlayerCharacter->GetMesh(),
		"L_Ankle_01",
		FVector(0.0f),
		FRotator(0.0f),
		EAttachLocation::SnapToTarget,
		false) : nullptr;

	SpawnedSlidingParticle[2] = (CurrentVolumeReference->AF_KneeSlidingParticle) ? UGameplayStatics::SpawnEmitterAttached(CurrentVolumeReference->AF_KneeSlidingParticle,
		IcarusPlayerCharacter->GetMesh(),
		"R_Knee_01",
		FVector(0.0f),
		FRotator(0.0f),
		EAttachLocation::SnapToTarget,
		false) : nullptr;
}

void UIcarusCharacterMovement_Slide::AntiFlowFallTimer()
{
	//seta impulso iniical e frictions para anti flow.
	IcarusCharacterMovement->Velocity = RawCurrentDirectionImpulse*CurrentVolumeReference->AF_InitialSlideForce;
	IcarusCharacterMovement->BrakingFrictionFactor = 1.0f;
	IcarusCharacterMovement->BrakingFriction = 0.0f;
	IcarusCharacterMovement->BrakingDecelerationWalking = 0.0f;
	IcarusCharacterMovement->GroundFriction = 0.0f;
	IcarusCharacterMovement->Mass = 1.0f;

	//Dita que a capsula ja pode ser rotacionada.
	bCanUpdateCapsuleRotation = true;

	//Invalida timer do FallAdvanceTimer()
	AntiFlowFallTH.Invalidate();
}

void UIcarusCharacterMovement_Slide::FrontEndTimer()
{
	//Seta moviment state pra moving e recupera rootmotion
	IcarusCharacterMovement->SetMovementState(EMovementState::EMS_None);

	//Ativa ou desativa sistemas de movimento em chao e ar.
	IcarusCharacterMovement->MovementGround->bActive = true;
	IcarusCharacterMovement->MovementAir->bActive = true;

	//Invalida timer do FallAdvanceTimer()
	FrontEndTH.Invalidate();
}

bool UIcarusCharacterMovement_Slide::IsInsideAntiFlowArea()
{
	//Verifica se ta dentro do rate de anti slide e retorna true, indicando que esta em anti flow area.
	if (!CurrentVolumeReference) return false;

	return (GetBoxCurrentRate() <= CurrentVolumeReference->AF_RateToAntiSlide) ? true : false;
}

bool UIcarusCharacterMovement_Slide::IsInsideFrictionArea()
{
	//Verifica se ta dentro do rate de friction e retorna true, indicando que esta em friction area.
	if (!CurrentVolumeReference) return false;

	return false; //((GetBoxCurrentRate() >= CurrentVolumeReference->RateToGrowFriction) && CurrentVolumeReference->bUseRateToGrowFriction) ? true : false;
}

void UIcarusCharacterMovement_Slide::SaveVarsOnOverlap()
{
	SavedGroundFriction = IcarusCharacterMovement->GroundFriction;
	SavedBrakingDecelerationWalking = IcarusCharacterMovement->BrakingDecelerationWalking;
	SavedBrakingFriction = IcarusCharacterMovement->BrakingFriction;
	SavedBrakingFrictionFactor = IcarusCharacterMovement->BrakingFrictionFactor;
	SavedCharacterMass = IcarusCharacterMovement->Mass;
}

void UIcarusCharacterMovement_Slide::ResetVars()
{
	IcarusCharacterMovement->GroundFriction = SavedGroundFriction;
	IcarusCharacterMovement->BrakingDecelerationWalking = SavedBrakingDecelerationWalking;
	IcarusCharacterMovement->BrakingFriction = SavedBrakingFriction;
	IcarusCharacterMovement->BrakingFrictionFactor = SavedBrakingFrictionFactor;
	IcarusCharacterMovement->Mass = SavedCharacterMass;
}

void UIcarusCharacterMovement_Slide::ResetCharacterRotation()
{
	IcarusPlayerCharacter->GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
}

FHitResult UIcarusCharacterMovement_Slide::SurfaceTrace()
{
	//Aplica trace da capsula pra superficie
	FVector TraceDirection = FVector(0.0f, 0.0f, -1.0f);
	FVector StartTrace = IcarusPlayerCharacter->GetCapsuleComponent()->GetComponentLocation();
	FVector EndTrace = StartTrace + (TraceDirection * SurfaceTraceLenght);
	TArray<AActor*> ActorsToIgnore;
	FHitResult OutHit;
	UKismetSystemLibrary::SphereTraceSingle(IcarusPlayerCharacter, StartTrace, EndTrace, SurfaceTraceRadius, ETraceTypeQuery::TraceTypeQuery1, false, ActorsToIgnore, EDrawDebugTrace::ForOneFrame, OutHit, true);

	return OutHit;
}

void UIcarusCharacterMovement_Slide::Event_ExitingFallState()
{
	bNotify_AntiFlow_NegateDirection = true;
	bNotify_AntiFlow_ResetInterp = true;
	Update_Internal(GetWorld()->GetDeltaSeconds());
	bNotify_AntiFlow_ResetInterp = false;
}

float UIcarusCharacterMovement_Slide::GetSurfaceAngle(FVector ImpactNormal)
{
	return UIcarusMathLibrary::DotProductAngle(ImpactNormal, FVector(0.0f, 0.0f, 1.0f));
}

void UIcarusCharacterMovement_Slide::UpdateVerifyStartSliding()
{
	if (IsSliding()) return;

	//Verifica se esta no ar e desativa para aguardar o delegate de on landed agir.
	if (IcarusCharacterMovement->MovementMode == EMovementMode::MOVE_Falling) return;

	//Check do angulo de entrada no start sliding.
	CheckIsInAntiFlowAngle();

	if (IsInsideAntiFlowArea())
	{
		if (IsInAntiFlowAngle())
		{
			//Caso esteja em anti flow area e anti flow anhle, inicia anti flow slide
			ChangeOtherMovementTypesActivation(false);
			AntiFlowInitSlide();

		}
		else
		{
			//Caso esteja em anti flow area e nao em anti flow angle, inicia normal fow slide
			ChangeOtherMovementTypesActivation(false);
			NormalFlowInitSlide();
		}
	}
}

void UIcarusCharacterMovement_Slide::UpdateVerticalMeshRotation(FVector ImpactNormal, float DeltaTime)
{
	if (!bCanUpdateCapsuleRotation) return;

	if (IsSliding())
	{

		FRotator NewRotator;

		float SurfaceAngle = (IsInAntiFlowAngle() && !bNotify_AntiFlow_NegateDirection) ? -1.0f*GetSurfaceAngle(ImpactNormal) : GetSurfaceAngle(ImpactNormal);

		NewRotator = FRotator(
			IcarusPlayerCharacter->GetMesh()->RelativeRotation.Pitch,
			IcarusPlayerCharacter->GetMesh()->RelativeRotation.Yaw,
			SurfaceAngle);

		//Cria o interpolate da nova rotacao vertical
		FRotator InterpRotator = UKismetMathLibrary::RInterpTo(IcarusPlayerCharacter->GetMesh()->RelativeRotation, 
			NewRotator, 
			DeltaTime, 
			VerticalRotationAdjustSpeed);

		if (bNotify_AntiFlow_ResetInterp) InterpRotator = NewRotator;

		IcarusPlayerCharacter->GetMesh()->SetRelativeRotation(InterpRotator, false, (FHitResult*)nullptr, ETeleportType::TeleportPhysics);
	}
}


void UIcarusCharacterMovement_Slide::UpdateHorizontalMeshRotation(float DeltaTime)
{
	if (!bCanUpdateCapsuleRotation) return;

	if (IsSliding())
	{
		//Pega rotacao da arrow do volume
		FRotator TransformedFrontRawDirection = RawCurrentDirectionImpulse.Rotation();

		FRotator FrontRotator, InterpRotator;

		//Rotacao padrao pra ficar reto.
		float DefaulYaw = (IsInAntiFlowAngle() && !bNotify_AntiFlow_NegateDirection) ? 90.0f : -90.0f;

		//Pega variacao baseado nas somas das forcas que acaba afetando a velocity nativa.
		float RotationVariation = TransformedFrontRawDirection.Yaw - IcarusCharacterMovement->Velocity.Rotation().Yaw;

		FrontRotator = FRotator(
			IcarusPlayerCharacter->GetMesh()->GetComponentRotation().Pitch, 
			TransformedFrontRawDirection.Yaw + DefaulYaw - RotationVariation,
			IcarusPlayerCharacter->GetMesh()->GetComponentRotation().Roll);

		//Executa interp e seta world rotation da mesh
		InterpRotator = UKismetMathLibrary::RInterpTo(IcarusPlayerCharacter->GetMesh()->GetComponentRotation(), FrontRotator, DeltaTime, VerticalRotationAdjustSpeed);
		IcarusPlayerCharacter->SetActorRotation((IcarusPlayerCharacter->GetVelocity()*FVector(1.0f, 1.0f, 0.0f)).Rotation(), ETeleportType::TeleportPhysics);
		if (bNotify_AntiFlow_ResetInterp) InterpRotator = FrontRotator;
		IcarusPlayerCharacter->GetMesh()->SetWorldRotation(InterpRotator, false, (FHitResult*)nullptr, ETeleportType::TeleportPhysics);
	
	}
}

void UIcarusCharacterMovement_Slide::UpdateFrictionDistanceBased()
{
	//Update do local de friction.
}

void UIcarusCharacterMovement_Slide::UpdateLateralMovement(FVector ImpactNormal)
{
	if (IsSliding())
	{
		//Pega somente o apontamento pras laterais (x) ImpactNormal eh convertido pra local do volume.
		FVector ResultantForce
		(
			ImpactNormal.X,
			0.0f,
			0.0f
		);

		//Clampa pra um angulo, para garantir que a velocidade nao vai se tornar muito forte pra lateral.
		ResultantForce.X = FMath::Clamp(ResultantForce.X, -0.55f, 0.55f);
		//Cria forca resultante e transforma de volta para espaco de world.
		FVector ResultantLocalDirection = ResultantForce*CurrentVolumeReference->LateralSlideForce;
		CurrentDirectionLateralImpulse = UKismetMathLibrary::TransformDirection(CurrentVolumeReference->GetActorTransform(), ResultantLocalDirection);
	}
}

void UIcarusCharacterMovement_Slide::UpdateMovement()
{
	if (!CurrentVolumeReference) return;
	//Soma forcas e adiciona no character movement.
	CurrentTotalForces = CurrentDirectionImpulse*CurrentVolumeReference->SlideForce + CurrentDirectionInput + CurrentDirectionLateralImpulse;
	IcarusCharacterMovement->AddForce(CurrentTotalForces);
}

void UIcarusCharacterMovement_Slide::UpdateMovementByUserInput( float DeltaTime)
{
	if (!IsInSlideSystem()) return;

	//1 para se a camera esta nas costas, -1 para camera na frente
	float CameraDotVolumeRotation = ( FVector::DotProduct(IcarusPlayerCharacter->GetControlRotation().Vector(), CurrentVolumeReference->DirectionImpulse->GetComponentRotation().Vector()) > 0.0f ) ? 1.f : -1.f;


	if (IsInAntiFlowAngle() && !bNotify_AntiFlow_NegateDirection) return;

	//Caso esteja clicando para direita
	if (IcarusCharacterMovement->MovementGround->MovementMap["RIGHT"])
	{
		TargetInputRotation = -1.0f*CameraDotVolumeRotation;
	}
	//Caso esteja clicando para esquerda
	else if (IcarusCharacterMovement->MovementGround->MovementMap["LEFT"])
	{
		TargetInputRotation = 1.0f*CameraDotVolumeRotation;
	}
	//Caso nao esteja clicando nenhum
	else
	{
		TargetInputRotation = 0.0f;
	}
	
	//Ultima rotacao para efetuar o interp
	LastInputRotation = FMath::FInterpTo(LastInputRotation, TargetInputRotation, DeltaTime, InputResponsivity);

	//Crossproduct para ver a direcao do impulso
	FVector ResultantLocalDirection = FVector(InputIntensity * LastInputRotation, 0.0f, 0.0f);
	CurrentDirectionInput = UKismetMathLibrary::TransformDirection(CurrentVolumeReference->GetActorTransform(), ResultantLocalDirection);
}

void UIcarusCharacterMovement_Slide::Update_Internal(float DeltaTime)
{
	//Pega impact normal do trace
	FVector WorldImpactNormal, LocalImpactNormal;
	GetTransformedImpactNormal(LocalImpactNormal, WorldImpactNormal);

	//Verifica a entrada para o estado de sliding
	UpdateVerifyStartSliding();

	//Sequencia de updates.
	UpdateMovementByUserInput(DeltaTime);
	UpdateLateralMovement(LocalImpactNormal);
	UpdateMovement();


	UpdateVerticalMeshRotation(WorldImpactNormal, DeltaTime);
	UpdateHorizontalMeshRotation(DeltaTime);

}