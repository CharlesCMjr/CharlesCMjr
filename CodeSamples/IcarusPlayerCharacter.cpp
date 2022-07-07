// Fill out your copyright notice in the Description page of Project Settings.

#include "IcarusPlayerCharacter.h"
#include "Serialization/IcarusSerializationManager.h"
#include "Camera/IcarusCameraPoint.h"
#include "Camera/IcarusCameraComponent.h"
#include "Camera/IcarusCameraModifierVolume.h"
#include "Interaction/IcarusInteractionActor_Hold.h"
#include "Interaction/IcarusInteractionManager.h"
#include "Movement/IcarusCharacterMovement.h"
#include "Climb/IcarusLedgeManipulation.h"
#include "IcarusPlayerController.h"
#include "IcarusFootPlacementComponent.h"
#include "Movement/IcarusCharacterMovement_Input.h"
#include "Movement/IcarusCharacterMovement_Swim.h"
#include "Volume/IcarusWaterVolume.h"
#include "Runtime/UMG/Public/Components/WidgetComponent.h"
#include "Icarus.h"

// Inicia falso var se jogador ja comecou
bool AIcarusPlayerCharacter::bPlayerHasAlreadyBegun = false;

// Sets default values
AIcarusPlayerCharacter::AIcarusPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UIcarusCharacterMovement>(ACharacter::CharacterMovementComponentName))
{
	bIsPlayer = true;

 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	bAllowTickBeforeBeginPlay = false;

	// Habilita overlap events para mesh
	GetMesh()->bGenerateOverlapEvents = true;

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...

	// Cria componente de interacao
	InteractionManager = CreateDefaultSubobject<UIcarusInteractionManager>("InteractionManager");

	// Create spring arm
	Arm = CreateDefaultSubobject<USpringArmComponent>("Arm");
	Arm->SetupAttachment(RootComponent);
	Arm->bUsePawnControlRotation = true;
	Arm->bDoCollisionTest = false;

	// Create camera
	Camera = CreateDefaultSubobject<UIcarusCameraComponent>("Camera");
	Camera->SetupAttachment(Arm, USpringArmComponent::SocketName);

	// Seta target arm length
	Arm->TargetArmLength = Camera->Settings.TargetArmLength;

	// Instancia ledge manipulation
	LedgeManipulation = CreateDefaultSubobject<UIcarusLedgeManipulation>("LedgeManipulation");

	// Instancia component FootPlacementComponent
	FootPlacement = CreateDefaultSubobject<UIcarusFootPlacementComponent>("FootPlacement");
}

// Called when the game starts or when spawned
void AIcarusPlayerCharacter::BeginPlay()
{
	CHECK_WITH_MESSAGE(!bPlayerHasAlreadyBegun, TEXT("O begin play do jogador ja foi iniciado, apenas um IcarusPlayerCharacter pode ser instanciado. Remova os adicionais no level."));

	// Pega serialization data
	CurrentSerializationData = UIcarusSerializationManager::LoadCurrentSerializationData(this);
	if (!IS_VALID(CurrentSerializationData))
	{
		ActorClientWarningMessage("AIcarusPlayerCharacter::BeginPlay: there is no serialization data. Creating one...");
		CurrentSerializationData = UIcarusSerializationManager::CreateSerializationData(this, "FirstData");
		UIcarusSerializationManager::SaveSerializationData(this, CurrentSerializationData);
	}
	UIcarusSerializationManager::SpawnSerializatedActors(this, CurrentSerializationData);

	// Pega player camera manager
	PlayerCameraManager = UGameplayStatics::GetPlayerCameraManager(this, PLAYER_ID);
	check(PlayerCameraManager);

	// Pega player controller
	IcarusPlayerController = Cast<AIcarusPlayerController>(GetController());
	CHECK_WITH_MESSAGE(IcarusPlayerController, TEXT("AIcarusPlayerCharacter: Falha ao pegar player controller"));

	// Instancia auto interp para camera fade de death
	AutoInterp_DeathCameraFade = GetWorld()->SpawnActor<AAutoInterp>();

	// Carrega e aplica configuracoes
	IcarusSettings = UIcarusSystemLibrary::LoadConfigSettings();
	UIcarusSystemLibrary::ApplyConfigSettings(this, IcarusSettings);

	// Attacha fog do mundo no personagem
	for (TActorIterator<AExponentialHeightFog> Fog(GetWorld()); Fog; ++Fog)
	{
		if (Fog->GetName() == WorldHeightFogName)
		{
			ActorClientMessage("Attached fog: %s", *WorldHeightFogName);
			Fog->AttachToActor(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		}
	}

	// Cria instancias de material para player
	MeshMaterialInstances = UIcarusSystemLibrary::CreateDynamicMaterialInstancesForMesh(GetMesh());

	// Aplica instances para mesh
	for (size_t Index = 0; Index < MeshMaterialInstances.Num(); ++Index)
	{
		GetMesh()->SetMaterial(Index, MeshMaterialInstances[Index]);
	}
	
	// SUper, executa de icaruscharacter, getserializationvalues
	Super::BeginPlay();

	// SUPER JA EXECUTADO

	// Atualiza overlaps de componentes overlapando a capsula
	TArray<UPrimitiveComponent*> OverlappedComponents;
	GetCapsuleComponent()->GetOverlappingComponents(OverlappedComponents);

	// Ordena pegando o owner icarus camera volume, e retorna lambda por prioridade menor.
	OverlappedComponents.StableSort(
		[](const UPrimitiveComponent& V1, const UPrimitiveComponent& V2) {
			AIcarusCameraModifierVolume * Volume1 = Cast<AIcarusCameraModifierVolume>(V1.GetOwner());
			AIcarusCameraModifierVolume * Volume2 = Cast<AIcarusCameraModifierVolume>(V2.GetOwner());
			if (IS_VALID(Volume1) && IS_VALID(Volume2))
			{
				return Volume1->Priority_ToPlayerBeginPlay > Volume2->Priority_ToPlayerBeginPlay;
			}
			return false;
		}
	);
	for (auto& Component : OverlappedComponents)
	{
		AIcarusInteractionActor * InteractionActor = Cast<AIcarusInteractionActor>(Component->GetOwner());
		if (IS_VALID(InteractionActor))
		{
			InteractionActor->IcarusPlayerCharacter = this;
			InteractionActor->IcarusInteractionManager = InteractionManager;
			InteractionActor->IcarusPlayerController = Cast<AIcarusPlayerController>(GetController());
		}
		Component->OnComponentBeginOverlap.Broadcast(Component, this, GetCapsuleComponent(), -1, false, FHitResult());
		ActorClientMessage("Overlapped component: %s", *Component->GetName());
	}

	// Seta true variavel estatica para impedir outro fluxo em player character
	// No caso, esse begin play executa apenas uma vez
	bPlayerHasAlreadyBegun = true;

	// Aplica timer para serializacao periodica
	/*if (bEnablePeriodicSerialize)
	{
		GetWorldTimerManager().SetTimer(FTH_PeriodicSerialize, this, &AIcarusPlayerCharacter::PeriodicSerialize_Timer, PeriodicSerializeTime, true);
	}*/

	// Pega water volume e seta
	/*<AActor*> OutActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AIcarusWaterVolume::StaticClass(), OutActors);
	if (OutActors.Num())
	{
		GetIcarusCharacterMovement()->MovementSwim->CurrentWaterVolume = Cast<AIcarusWaterVolume>(OutActors[0]);
	}*/
}

void AIcarusPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Desativa player ja comecou
	bPlayerHasAlreadyBegun = false;
	Super::EndPlay(EndPlayReason);
}

// Called every frame
void AIcarusPlayerCharacter::Tick(float DeltaTime)
{
	// Atualiza correcao de camera sensitivity relativo ao fps
	UpdateCameraSensitivityCorrection(DeltaTime);

	// Super
	Super::Tick( DeltaTime );
}

// Called to bind functionality to input
void AIcarusPlayerCharacter::SetupPlayerInputComponent(class UInputComponent* InputComponentParam)
{
	Super::SetupPlayerInputComponent(InputComponentParam);

	// Instancia objeto de input de movimentos
	IcarusCharacterMovement_Input = NewObject<UIcarusCharacterMovement_Input>(GetIcarusCharacterMovement(), "IcarusCharacterMovement_Input");
	
	// TODO
	InputComponentParam->BindAction("UseItem", EInputEvent::IE_Pressed, InteractionManager, &UIcarusInteractionManager::StartInteraction_Input);
	InputComponentParam->BindAction("UseItem", EInputEvent::IE_Released, InteractionManager, &UIcarusInteractionManager::StopInteraction_Input);
	InputComponentParam->BindAction("HoldInteraction", EInputEvent::IE_Pressed, InteractionManager, &UIcarusInteractionManager::HoldInteraction_Pressed);
	InputComponentParam->BindAction("HoldInteraction", EInputEvent::IE_Released, InteractionManager, &UIcarusInteractionManager::HoldInteraction_Released);

	// MOVEMENT
	InputComponentParam->BindAction("MoveForward", EInputEvent::IE_Pressed, IcarusCharacterMovement_Input, &UIcarusCharacterMovement_Input::MoveForward_Pressed);
	InputComponentParam->BindAction("MoveBackward", EInputEvent::IE_Pressed, IcarusCharacterMovement_Input, &UIcarusCharacterMovement_Input::MoveBackward_Pressed);
	InputComponentParam->BindAction("MoveRight", EInputEvent::IE_Pressed, IcarusCharacterMovement_Input, &UIcarusCharacterMovement_Input::MoveRight_Pressed);
	InputComponentParam->BindAction("MoveLeft", EInputEvent::IE_Pressed, IcarusCharacterMovement_Input, &UIcarusCharacterMovement_Input::MoveLeft_Pressed);

	InputComponentParam->BindAction("MoveForward", EInputEvent::IE_Released, IcarusCharacterMovement_Input, &UIcarusCharacterMovement_Input::MoveForward_Released);
	InputComponentParam->BindAction("MoveBackward", EInputEvent::IE_Released, IcarusCharacterMovement_Input, &UIcarusCharacterMovement_Input::MoveBackward_Released);
	InputComponentParam->BindAction("MoveRight", EInputEvent::IE_Released, IcarusCharacterMovement_Input, &UIcarusCharacterMovement_Input::MoveRight_Released);
	InputComponentParam->BindAction("MoveLeft", EInputEvent::IE_Released, IcarusCharacterMovement_Input, &UIcarusCharacterMovement_Input::MoveLeft_Released);

	InputComponentParam->BindAxis("MoveForwardAxis", IcarusCharacterMovement_Input, &UIcarusCharacterMovement_Input::MoveForward_Axis);
	InputComponentParam->BindAxis("MoveRightAxis", IcarusCharacterMovement_Input, &UIcarusCharacterMovement_Input::MoveRight_Axis);

	InputComponentParam->BindAction("ToggleForceWalk", EInputEvent::IE_Pressed, IcarusCharacterMovement_Input, &UIcarusCharacterMovement_Input::ToggleForceWalk);

	// AXIS INPUT
	InputComponentParam->BindAxis("MoveCameraYaw", IcarusCharacterMovement_Input, &UIcarusCharacterMovement_Input::MoveCameraYaw);
	InputComponentParam->BindAxis("MoveCameraPitch", IcarusCharacterMovement_Input, &UIcarusCharacterMovement_Input::MoveCameraPitch);

	// ACTION INPUT
	InputComponentParam->BindAction("Rest", EInputEvent::IE_Pressed, IcarusCharacterMovement_Input, &UIcarusCharacterMovement_Input::Rest);
	InputComponentParam->BindAction("ResetCamera", EInputEvent::IE_Pressed, Camera, &UIcarusCameraComponent::ResetCamera);
	InputComponentParam->BindAction("Jump", EInputEvent::IE_Pressed, IcarusCharacterMovement_Input, &UIcarusCharacterMovement_Input::Jump_Pressed);
	InputComponentParam->BindAction("Jump", EInputEvent::IE_Released, IcarusCharacterMovement_Input, &UIcarusCharacterMovement_Input::Jump_Released);
	InputComponentParam->BindAction("SwimUp", EInputEvent::IE_Pressed, IcarusCharacterMovement_Input, &UIcarusCharacterMovement_Input::SwimUp_Pressed);
	InputComponentParam->BindAction("SwimUp", EInputEvent::IE_Released, IcarusCharacterMovement_Input, &UIcarusCharacterMovement_Input::SwimUp_Released);
	InputComponentParam->BindAction("SwimDown", EInputEvent::IE_Pressed, IcarusCharacterMovement_Input, &UIcarusCharacterMovement_Input::SwimDown_Pressed);
	InputComponentParam->BindAction("SwimDown", EInputEvent::IE_Released, IcarusCharacterMovement_Input, &UIcarusCharacterMovement_Input::SwimDown_Released);
	InputComponentParam->BindAction("ToggleDebug", EInputEvent::IE_Pressed, this, &AIcarusPlayerCharacter::ToggleDebug);
}

void AIcarusPlayerCharacter::ToggleDebug()
{
	UIcarusSystemLibrary::SetDebug(!UIcarusSystemLibrary::IsDebug());
}

UIcarusCharacterMovement * AIcarusPlayerCharacter::GetIcarusCharacterMovement()
{
	UIcarusCharacterMovement * Movement = Cast<UIcarusCharacterMovement>(GetCharacterMovement());
	return Movement;
}

UIcarusCharacterMovement * AIcarusPlayerCharacter::GetIcarusCharacterMovement_Const() const
{
	UIcarusCharacterMovement * Movement = Cast<UIcarusCharacterMovement>(GetCharacterMovement());
	return Movement;
}

void AIcarusPlayerCharacter::GetSerializationValues_Implementation(UIcarusSerializationData * SerializationData)
{
	CHECK_FLOW(SerializationData);

	FVector NewLocation = UIcarusSerializationData::GetDataVar_FVector("PlayerLocation", SerializationData);
	if (NewLocation != FVector::ZeroVector)
	{
		FHitResult OutHit;

		// Cria trace para ajustar a posicao.
		if (UKismetSystemLibrary::SphereTraceSingle(GetWorld(),
			NewLocation,
			NewLocation - FVector(0.f, 0.f, 100000000.f),
			32.f,
			UEngineTypes::ConvertToTraceType(ECollisionChannel::ECC_Visibility),
			false,
			TArray<AActor*>(),
			UIcarusSystemLibrary::DrawDebugTrace(false),
			OutHit,
			true))
		{
			SetActorLocation(OutHit.ImpactPoint + FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
		}
		else
		{
			SetActorLocation(NewLocation);
		}
	}
	
	FRotator NewRotation = UIcarusSerializationData::GetDataVar_FRotator("PlayerRotation", SerializationData);
	if (NewRotation != FRotator::ZeroRotator)
	{
		NewRotation.Pitch = NewRotation.Roll = 0;
		SetActorRotation(NewRotation);
	}

	Super::GetSerializationValues_Implementation(SerializationData);
}

void AIcarusPlayerCharacter::SetSerializationValues_Implementation(UIcarusSerializationData * SerializationData)
{
	CHECK_FLOW(SerializationData);

	float SecondsPlayed = GetWorld()->RealTimeSeconds;
	float HoursPlayed = UIcarusSerializationData::GetDataVar_float("HoursPlayed", SerializationData) + (SecondsPlayed / 3600.f);
	UIcarusSerializationData::SetDataVar_float("HoursPlayed", SerializationData, HoursPlayed);

	Super::SetSerializationValues_Implementation(SerializationData);
}

UIcarusSerializationData * AIcarusPlayerCharacter::GetCurrentSerializationData()
{
	return CurrentSerializationData;
}

void AIcarusPlayerCharacter::Kill(EDeathType DeathType)
{
	CHECK_FLOW(!bDead);

	// Desabilita componente de movimento.
	GetIcarusCharacterMovement()->Deactivate();

	KillDelegate.Broadcast(DeathType);
	bDead = true;

	AutoInterp_DeathCameraFade->OnManualCameraFadeCompleted.RemoveAll(this);
	AutoInterp_DeathCameraFade->OnManualCameraFadeCompleted.AddUniqueDynamic(this, &AIcarusPlayerCharacter::Reborn);
	AutoInterp_DeathCameraFade->UpdateManualCameraFade({ PlayerCameraManager });
}

void AIcarusPlayerCharacter::Reborn()
{
	CHECK_FLOW(bDead);

	UIcarusSerializationManager::SaveSerializationData(this, CurrentSerializationData);

	Checkpoint();
	RebornDelegate.Broadcast();
	bDead = false;

	AutoInterp_DeathCameraFade->OnManualCameraFadeCompleted.RemoveAll(this);
	AutoInterp_DeathCameraFade->UpdateManualCameraFade({ PlayerCameraManager }, true);

	GetIcarusCharacterMovement()->Activate(true);
}

void AIcarusPlayerCharacter::Checkpoint()
{
	FVector PlayerLocation = UIcarusSerializationData::GetDataVar_FVector("PlayerLocation", CurrentSerializationData);
	FRotator PlayerRotation = UIcarusSerializationData::GetDataVar_FRotator("PlayerRotation", CurrentSerializationData);
	FHitResult OutHit;

	// Trace para ajustar a posicao.
	if (UKismetSystemLibrary::SphereTraceSingle(GetWorld(),
		PlayerLocation,
		PlayerLocation - FVector(0.f, 0.f, 100000000.f),
		32.f,
		UEngineTypes::ConvertToTraceType(ECollisionChannel::ECC_Visibility),
		false,
		TArray<AActor*>(),
		UIcarusSystemLibrary::DrawDebugTrace(false),
		OutHit,
		true))
	{
		SetActorLocationAndRotation(OutHit.ImpactPoint + FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), PlayerRotation);
	}
	else
	{
		SetActorLocationAndRotation(PlayerLocation + FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), PlayerRotation);
	}
}

bool AIcarusPlayerCharacter::IsDead() const
{
	return bDead;
}

void AIcarusPlayerCharacter::SetPeriodicSerialize(bool bNewValue, float Time)
{
	if (bPlayerHasAlreadyBegun)
	{
		if (bNewValue)
		{
			if (!bEnablePeriodicSerialize || (PeriodicSerializeTime != Time))
			{
				GetWorldTimerManager().ClearTimer(FTH_PeriodicSerialize);
				GetWorldTimerManager().SetTimer(FTH_PeriodicSerialize, this, &AIcarusPlayerCharacter::PeriodicSerialize_Timer, Time, true);
			}
		}
		else
		{
			GetWorldTimerManager().ClearTimer(FTH_PeriodicSerialize);
		}
		bEnablePeriodicSerialize = bNewValue;
		PeriodicSerializeTime = Time;
	}
}

void AIcarusPlayerCharacter::UpdateCameraSensitivityCorrection(float DeltaTime)
{
	UIcarusSystemLibrary::SetCameraSensitivity(this, 
		IcarusSettings.SensitivityY * (DeltaTime / IcarusSettings.SensitivityYTime),
		IcarusSettings.SensitivityX * (DeltaTime / IcarusSettings.SensitivityXTime),
		IcarusSettings.bInvertCameraX, IcarusSettings.bInvertCameraY);
}

void AIcarusPlayerCharacter::PeriodicSerialize_Timer()
{
	CHECK_FLOW_NO_LOG(!UIcarusSystemLibrary::IsDebug());
	if (!GetVelocity().Size() && 
		!InteractionManager->IsInteracting())
	{
		UKismetSystemLibrary::PrintString(GetWorld(), "Running periodic serialize...", true, true, FLinearColor(0.f, 0.8f, 0.2f), PeriodicSerializeTime);
		UIcarusSerializationManager::SaveSerializationData(this, CurrentSerializationData);
	}
}