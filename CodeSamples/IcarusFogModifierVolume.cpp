// Fill out your copyright notice in the Description page of Project Settings.

#include "IcarusFogModifierVolume.h"
#include "Icarus.h"

AIcarusFogModifierVolume::AIcarusFogModifierVolume()
{
	GetBrushComponent()->SetCollisionProfileName("StaticOverlapPawn");
	GetBrushComponent()->OnComponentBeginOverlap.AddUniqueDynamic(this, &AIcarusFogModifierVolume::VolumeBeginOverlap);
	GetBrushComponent()->OnComponentEndOverlap.AddUniqueDynamic(this, &AIcarusFogModifierVolume::VolumeEndOverlap);
	PrimaryActorTick.bCanEverTick = true;
}

void AIcarusFogModifierVolume::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickInterval(0.1f);
	SetActorTickEnabled(false);
}

void AIcarusFogModifierVolume::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bReturnToOriginal)
	{
		UpdateFogSettings(SavedFogSettings, DeltaTime);
		UpdateLightSettings(SavedLightSettings, DeltaTime);
	}
	else
	{
		UpdateFogSettings(FogSettings, DeltaTime);
		UpdateLightSettings(LightSettings, DeltaTime);
	}

	// Desabilita tick caso informacoes ja foram setadas
	if (IsActorTickEnabled())
	{
		if (IsEqualFog(FogSettings) && IsEqualLight(LightSettings) || 
			IsEqualFog(SavedFogSettings) && IsEqualLight(SavedLightSettings))
		{
			if (bRequestNewSettings)
			{
				SavedFogSettings = TargetFog;
				SavedLightSettings = TargetLight;
				bReturnToOriginal = bRequestNewSettings = false;
				ActorClientMessage("Aplicado procedimento de requisicao de novas informacoes.");
				ActorClientMessage("Aplicado informacoes de fog e luz.");
			}
			else
			{
				SetActorTickEnabled(false);
				ActorClientMessage("Procedimentos terminados.");
			}
		}
	}
}

void AIcarusFogModifierVolume::VolumeBeginOverlap(UPrimitiveComponent * OverlappedComponent, AActor * OtherActor, UPrimitiveComponent * OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult)
{
	if (UIcarusSystemLibrary::CheckOverlappingPlayer(OtherActor, OtherComp))
	{
		if (bReturnToOriginal)
		{
			ActorClientWarningMessage("Requisitado novas configuracoes enquanto procedimento de retorno executa.");
			bRequestNewSettings = true;
		}
		else
		{
			SavedFogSettings = TargetFog;
			SavedFogSettings.InterpSpeed = FogSettings.InterpSpeed;
			SavedLightSettings = TargetLight;
			SavedLightSettings.InterpSpeed = LightSettings.InterpSpeed;
	
			/*if (TargetLight)
			{
				UDirectionalLightComponent* DirectionalLightComponent = Cast<UDirectionalLightComponent>(TargetLight->GetLightComponent());
				if (DirectionalLightComponent)
				{
					DirectionalLightComponent->ShadowBias = SavedLightSettings.ShadowBias;
					DirectionalLightComponent->ContactShadowLength = SavedLightSettings.ContactShadow;
					DirectionalLightComponent->ShadowSharpen = SavedLightSettings.ShadowSharpen;
				}
			}
			*/
			ActorClientMessage("Aplicado informacoes de fog e luz.");
		}
		SetActorTickEnabled(true);
	}
}

void AIcarusFogModifierVolume::VolumeEndOverlap(UPrimitiveComponent * OverlappedComponent, AActor * OtherActor, UPrimitiveComponent * OtherComp, int32 OtherBodyIndex)
{
	if (UIcarusSystemLibrary::CheckOverlappingPlayer(OtherActor, OtherComp))
	{
		bRequestNewSettings = false;
		bReturnToOriginal = true;
		SetActorTickEnabled(true);
		ActorClientMessage("Aplicado procedimento de retorno de informacoes.");
	}
}

void AIcarusFogModifierVolume::UpdateFogSettings(const FIcarusFogSettings& Settings, float DeltaTime)
{
	CHECK_FLOW_NO_LOG(TargetFog);
	TargetFog->GetComponent()->FogDensity = FMath::FInterpConstantTo(TargetFog->GetComponent()->FogDensity,
		Settings.Density, DeltaTime, Settings.InterpSpeed);
	TargetFog->GetComponent()->MarkRenderStateDirty();
}

void AIcarusFogModifierVolume::UpdateLightSettings(const FIcarusLightSettings& Settings, float DeltaTime)
{
	CHECK_FLOW_NO_LOG(TargetLight);
	UDirectionalLightComponent * DirectionalLightComponent = Cast<UDirectionalLightComponent>(TargetLight->GetLightComponent());
	CHECK_FLOW_NO_LOG(IS_VALID(DirectionalLightComponent));

	// Volumetric scattering
	DirectionalLightComponent->VolumetricScatteringIntensity =
		FMath::FInterpConstantTo(DirectionalLightComponent->VolumetricScatteringIntensity,
			Settings.VolumetricScatteringIntensity,
			DeltaTime,
			Settings.InterpSpeed);

	// Shaft occlusion
	DirectionalLightComponent->bEnableLightShaftOcclusion = Settings.bEnableLightShaftOcclusion;
	DirectionalLightComponent->OcclusionMaskDarkness = FMath::FInterpConstantTo(DirectionalLightComponent->OcclusionMaskDarkness,
		Settings.OcclusionMaskDarkness,
		DeltaTime,
		Settings.InterpSpeed);
	DirectionalLightComponent->OcclusionDepthRange = FMath::FInterpConstantTo(DirectionalLightComponent->OcclusionDepthRange,
		Settings.OcclusionDepthRange,
		DeltaTime,
		Settings.InterpSpeed);

	// Shaft bloom
	DirectionalLightComponent->bEnableLightShaftBloom = Settings.bEnableLightShaftBloom;
	DirectionalLightComponent->BloomScale = FMath::FInterpConstantTo(DirectionalLightComponent->BloomScale,
		Settings.BloomScale,
		DeltaTime,
		Settings.InterpSpeed);

	DirectionalLightComponent->BloomThreshold = FMath::FInterpConstantTo(DirectionalLightComponent->BloomThreshold,
		Settings.BloomThreshold,
		DeltaTime,
		Settings.InterpSpeed);

	// Lerp cor
	DirectionalLightComponent->BloomTint.R = FMath::FInterpConstantTo(DirectionalLightComponent->BloomTint.R,
		Settings.BloomTint.R,
		DeltaTime,
		Settings.InterpSpeed);
	DirectionalLightComponent->BloomTint.G = FMath::FInterpConstantTo(DirectionalLightComponent->BloomTint.G,
		Settings.BloomTint.G,
		DeltaTime,
		Settings.InterpSpeed);
	DirectionalLightComponent->BloomTint.B = FMath::FInterpConstantTo(DirectionalLightComponent->BloomTint.B,
		Settings.BloomTint.B,
		DeltaTime,
		Settings.InterpSpeed);
	DirectionalLightComponent->BloomTint.A = FMath::FInterpConstantTo(DirectionalLightComponent->BloomTint.A,
		Settings.BloomTint.A,
		DeltaTime,
		Settings.InterpSpeed);

	//Intensity
	DirectionalLightComponent->Intensity = FMath::FInterpConstantTo(DirectionalLightComponent->Intensity,
		Settings.Intensity,
		DeltaTime,
		Settings.InterpSpeed);

	//ShadowBias
	DirectionalLightComponent->ShadowBias = FMath::FInterpConstantTo(DirectionalLightComponent->ShadowBias,
		Settings.ShadowBias,
		DeltaTime,
		Settings.InterpSpeed);

	//Sharpen
	DirectionalLightComponent->ShadowSharpen = FMath::FInterpConstantTo(DirectionalLightComponent->ShadowSharpen,
		Settings.ShadowSharpen,
		DeltaTime,
		Settings.InterpSpeed);

	//Contact shadow
	DirectionalLightComponent->ContactShadowLength = FMath::FInterpConstantTo(DirectionalLightComponent->ContactShadowLength,
		Settings.ContactShadow,
		DeltaTime,
		Settings.InterpSpeed);

	// Marca estado sujo
	DirectionalLightComponent->MarkRenderStateDirty();
}

bool AIcarusFogModifierVolume::IsEqualFog(const FIcarusFogSettings & Settings) const
{
	CHECK_FLOW_WITH_RETURN_NO_LOG(TargetFog, true);
	return TargetFog->GetComponent()->FogDensity == Settings.Density;
}

bool AIcarusFogModifierVolume::IsEqualLight(const FIcarusLightSettings & Settings) const
{
	CHECK_FLOW_WITH_RETURN_NO_LOG(TargetLight, true);
	UDirectionalLightComponent * DirectionalLightComponent = Cast<UDirectionalLightComponent>(TargetLight->GetLightComponent());
	CHECK_FLOW_WITH_RETURN_NO_LOG(IS_VALID(DirectionalLightComponent), true);
	return DirectionalLightComponent->VolumetricScatteringIntensity == Settings.VolumetricScatteringIntensity &&
		DirectionalLightComponent->bEnableLightShaftBloom == Settings.bEnableLightShaftBloom &&
		DirectionalLightComponent->BloomScale == Settings.BloomScale &&
		DirectionalLightComponent->BloomThreshold == Settings.BloomThreshold &&
		DirectionalLightComponent->BloomTint == Settings.BloomTint &&
		DirectionalLightComponent->bEnableLightShaftOcclusion == Settings.bEnableLightShaftOcclusion &&
		DirectionalLightComponent->OcclusionDepthRange == Settings.OcclusionDepthRange &&
		DirectionalLightComponent->OcclusionMaskDarkness == Settings.OcclusionMaskDarkness &&
		DirectionalLightComponent->Intensity == Settings.Intensity &&
		DirectionalLightComponent->ShadowBias == Settings.ShadowBias &&
		DirectionalLightComponent->ContactShadowLength == Settings.ContactShadow &&
		DirectionalLightComponent->ShadowSharpen == Settings.ShadowSharpen;
}

FIcarusFogSettings & FIcarusFogSettings::operator=(AExponentialHeightFog* Fog)
{
	CHECK_FLOW_WITH_RETURN_NO_LOG(Fog, *this);
	Density = Fog->GetComponent()->FogDensity;
	return *this;
}

FIcarusLightSettings & FIcarusLightSettings::operator=(ADirectionalLight* Light)
{
	CHECK_FLOW_WITH_RETURN_NO_LOG(Light, *this);
	UDirectionalLightComponent * DirectionalLightComponent = Cast<UDirectionalLightComponent>(Light->GetLightComponent());
	CHECK_FLOW_WITH_RETURN_NO_LOG(IS_VALID(DirectionalLightComponent), *this);
	VolumetricScatteringIntensity = DirectionalLightComponent->VolumetricScatteringIntensity;
	bEnableLightShaftBloom = DirectionalLightComponent->bEnableLightShaftBloom;
	BloomScale = DirectionalLightComponent->BloomScale;
	BloomThreshold = DirectionalLightComponent->BloomThreshold;
	BloomTint = DirectionalLightComponent->BloomTint;
	bEnableLightShaftOcclusion = DirectionalLightComponent->bEnableLightShaftOcclusion;
	OcclusionDepthRange = DirectionalLightComponent->OcclusionDepthRange;
	OcclusionMaskDarkness = DirectionalLightComponent->OcclusionMaskDarkness;
	Intensity = DirectionalLightComponent->Intensity;
	ShadowBias = DirectionalLightComponent->ShadowBias;
	ContactShadow = DirectionalLightComponent->ContactShadowLength;
	ShadowSharpen = DirectionalLightComponent->ShadowSharpen;
	return *this;
}
