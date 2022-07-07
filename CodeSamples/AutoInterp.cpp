// Fill out your copyright notice in the Description page of Project Settings.

#include "AutoInterp.h"
#include "Camera/IcarusCameraComponent.h"
#include "Icarus.h"

AAutoInterp::AAutoInterp()
{
	PrimaryActorTick.bCanEverTick = true;

	Update.Add("LOCATION", false);
	Update.Add("ROTATION", false);
	Update.Add("LOCATION_AND_ROTATION", false);
	Update.Add("MANUAL_CAMERA_FADE", false);

	TimeAccumulated.Add("LOCATION", 0.f);
	TimeAccumulated.Add("ROTATION", 0.f);
	TimeAccumulated.Add("LOCATION_AND_ROTATION", 0.f);
	TimeAccumulated.Add("MANUAL_CAMERA_FADE", 0.f);
}

void AAutoInterp::BeginPlay()
{
	// Super
	Super::BeginPlay();

	SetActorTickEnabled(false);
}

// Funcao tick.
void AAutoInterp::Tick(float DeltaTime)
{
	if (Update["LOCATION"])
	{
		UpdateLocation_Internal(DeltaTime);
	}
	if (Update["ROTATION"])
	{
		UpdateRotation_Internal(DeltaTime);
	}
	if (Update["LOCATION_AND_ROTATION"])
	{
		UpdateLocationAndRotation_Internal(DeltaTime);
	}
	if (Update["MANUAL_CAMERA_FADE"])
	{
		UpdateManualCameraFade_Internal(DeltaTime);
	}
	if (!Update["LOCATION"] && !Update["ROTATION"] && !Update["LOCATION_AND_ROTATION"] && !Update["MANUAL_CAMERA_FADE"])
	{
		if (bDestroyOnFinish)
		{
			Destroy();
		}
		else
		{
			SetActorTickEnabled(false);
		}
	}

	// Super
	Super::Tick(DeltaTime);
}

void AAutoInterp::UpdateLocation(FAutoInterpData Data)
{
	CHECK_FLOW_NO_LOG(Data.Actor);

	TargetData = Data;
	TimeAccumulated["LOCATION"] = 0.f;
	Update["LOCATION"] = true;
	SetActorTickEnabled(true);
}

void AAutoInterp::UpdateRotation(FAutoInterpData Data)
{
	CHECK_FLOW_NO_LOG(Data.Actor);

	TargetData = Data;
	TimeAccumulated["ROTATION"] = 0.f;
	Update["ROTATION"] = true;
	SetActorTickEnabled(true);
}

void AAutoInterp::UpdateLocationAndRotation(FAutoInterpData Data)
{
	CHECK_FLOW_NO_LOG(Data.Actor);

	TargetData = Data;
	TimeAccumulated["LOCATION_AND_ROTATION"] = 0.f;
	Update["LOCATION_AND_ROTATION"] = true;
	SetActorTickEnabled(true);
}

void AAutoInterp::UpdateManualCameraFade(FAutoInterpData Data, bool bInvert)
{
	CHECK_FLOW_NO_LOG(Data.Actor);

	TargetData = Data;
	TargetPlayerCameraManager = Cast<APlayerCameraManager>(TargetData.Actor);
	TimeAccumulated["MANUAL_CAMERA_FADE"] = 0.f;
	TargetInvert = bInvert;
	Update["MANUAL_CAMERA_FADE"] = true;
	SetActorTickEnabled(true);
}

void AAutoInterp::UpdateLocationAndRotation_Internal(float DeltaTime)
{
	FRotator CurrentRotation = UKismetMathLibrary::RLerp(TargetData.Actor->GetActorRotation(), TargetData.Rotation, TimeAccumulated["LOCATION_AND_ROTATION"] * TargetData.Speed, true);
	FVector CurrentLocation = FMath::LerpStable(TargetData.Actor->GetActorLocation(), TargetData.Location, TimeAccumulated["LOCATION_AND_ROTATION"] * TargetData.Speed);
	CurrentLocation.Z = TargetData.Actor->GetActorLocation().Z;

	TargetData.Actor->SetActorLocationAndRotation(CurrentLocation, CurrentRotation, TargetData.bSweep);

	if (TimeAccumulated["LOCATION_AND_ROTATION"] >= TargetData.Duration ||
		(CurrentLocation.Equals(TargetData.Location, TargetData.LocationErrorTolerance) && CurrentRotation.Equals(TargetData.Rotation, TargetData.RotationErrorTolerance)))
	{
		if (TargetData.bUseHardSetOnErrorTolerance)
		{
			TargetData.Actor->SetActorLocationAndRotation(TargetData.Location, TargetData.Rotation, TargetData.bSweep);
		}
		Update["LOCATION_AND_ROTATION"] = false;
		OnLocationAndRotationCompleted.Broadcast();
	}

	TimeAccumulated["LOCATION_AND_ROTATION"] += DeltaTime ;
}

void AAutoInterp::UpdateManualCameraFade_Internal(float DeltaTime)
{
	float CurrentValue = FMath::InterpSinIn(0.f, 1.f, TimeAccumulated["MANUAL_CAMERA_FADE"] * TargetData.Speed);

	if (TargetInvert)
	{
		CurrentValue = 1.f - CurrentValue;
	}

	TargetPlayerCameraManager->SetManualCameraFade(CurrentValue, TargetData.Color, false);

	if (TimeAccumulated["MANUAL_CAMERA_FADE"] >= TargetData.Duration)
	{
		Update["MANUAL_CAMERA_FADE"] = false;
		OnManualCameraFadeCompleted.Broadcast();
	}

	TimeAccumulated["MANUAL_CAMERA_FADE"] += DeltaTime ;
}

void AAutoInterp::UpdateLocation_Internal(float DeltaTime)
{
	FVector CurrentLocation = FMath::LerpStable(TargetData.Actor->GetActorLocation(), TargetData.Location, TimeAccumulated["LOCATION"] * TargetData.Speed);
	CurrentLocation.Z = TargetData.Actor->GetActorLocation().Z;

	TargetData.Actor->SetActorLocation(CurrentLocation, TargetData.bSweep);

	if (TimeAccumulated["LOCATION"] >= TargetData.Duration)
	{
		if (TargetData.bUseHardSetOnErrorTolerance)
		{
			TargetData.Actor->SetActorLocation(TargetData.Location, TargetData.bSweep);
		}
		Update["LOCATION"] = false;
		OnLocationCompleted.Broadcast();
	}

	TimeAccumulated["LOCATION"] += DeltaTime ;
}

void AAutoInterp::UpdateRotation_Internal(float DeltaTime)
{
	FRotator CurrentRotation = UKismetMathLibrary::RLerp(TargetData.Actor->GetActorRotation(), TargetData.Rotation, TimeAccumulated["ROTATION"] * TargetData.Speed, true);

	TargetData.Actor->SetActorRotation(CurrentRotation);

	if (TimeAccumulated["ROTATION"] >= TargetData.Duration)
	{
		if (TargetData.bUseHardSetOnErrorTolerance)
		{
			TargetData.Actor->SetActorRotation(TargetData.Rotation);
		}
		Update["LOCATION"] = false;
		OnRotationCompleted.Broadcast();
	}

	TimeAccumulated["ROTATION"] += DeltaTime ;
}