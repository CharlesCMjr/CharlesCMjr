
#include "IcarusLedgeBase.h"
#include "Engine/StaticMeshSocket.h"
#include "Icarus.h"

AIcarusLedgeBase::AIcarusLedgeBase()
{

	PrimaryActorTick.bCanEverTick = true;
	bRunConstructionScriptOnDrag = false;
	// Inicializacao dos componentes
	SceneCoordinateComponent = CreateDefaultSubobject<USceneComponent>("SceneCoordinate");
	RootComponent = SceneCoordinateComponent;

	ArrowNormalComponent = CreateDefaultSubobject<UArrowComponent>("ArrowNormalComponent");
	ArrowNormalComponent->SetupAttachment(RootComponent);

	Box = CreateDefaultSubobject<UBoxComponent>("Box");
	Box->SetBoxExtent(FVector(50.0f));
	Box->SetCollisionProfileName("Climbable");
	Box->SetupAttachment(RootComponent);

	LeftSphereContactAttach = CreateDefaultSubobject<USphereComponent>("LeftSphereContactAttach");
	LeftSphereContactAttach->SetupAttachment(RootComponent);
	LeftSphereContactAttach->SetSphereRadius(50.0f, false);
	//LeftSphereContactAttach->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	LeftSphereContactAttach->SetVisibility(false);

	RightSphereContactAttach = CreateDefaultSubobject<USphereComponent>("RightSphereContactAttach");
	RightSphereContactAttach->SetupAttachment(RootComponent);
	RightSphereContactAttach->SetSphereRadius(50.0f, false);
	//RightSphereContactAttach->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	RightSphereContactAttach->SetVisibility(false);
}

void AIcarusLedgeBase::BeginPlay()
{
	Super::BeginPlay();

	LeftSphereContactAttach->DestroyComponent();
	RightSphereContactAttach->DestroyComponent();
}

void AIcarusLedgeBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SetAttachmentOnContruction();
}

void AIcarusLedgeBase::RemoveCollisionToPawn()
{
	Box->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);
}

void AIcarusLedgeBase::RestoreCollisionToPawn()
{
	Box->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Block);
}

void AIcarusLedgeBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

FVector AIcarusLedgeBase::GetEndPointWorldCoordinate()
{
	return GetActorLocation() + (GetSegmentLenght()*GetSegmentDirection());
}

FVector AIcarusLedgeBase::GetSegmentDirection()
{
	FVector Direction = GetActorRotation().Vector() * -1.0f;
	return Direction;
}

float AIcarusLedgeBase::GetSegmentLenght()
{
	FVector ActorWorldLocation = GetActorLocation();
	FVector EndPointWorldLocation = ActorWorldLocation + EndPoint;
	FVector Diference = EndPointWorldLocation - ActorWorldLocation;
	return (Diference.Size());
}

FVector AIcarusLedgeBase::GetSegmentNormal()
{
	return ArrowNormalComponent->GetComponentRotation().Vector().GetSafeNormal();
}

void AIcarusLedgeBase::SetAttachmentOnContruction()
{
	if (IS_VALID(StartAttachedLedge))
	{
		SetActorLocation(StartAttachedLedge->GetEndPointWorldCoordinate());
		StartAttachedLedge->EndAttachedLedge = this;
	}

	if (IS_VALID(EndAttachedLedge))
	{
		SetActorLocation(EndAttachedLedge->GetActorLocation() - (GetEndPointWorldCoordinate() - GetActorLocation()));
		EndAttachedLedge->StartAttachedLedge = this;
	}
}