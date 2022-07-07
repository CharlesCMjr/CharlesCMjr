
#include "IcarusLedgeManipulation.h"
#include "Icarus.h"

UIcarusLedgeManipulation::UIcarusLedgeManipulation()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UIcarusLedgeManipulation::BeginPlay()
{
	Super::BeginPlay();
}

void UIcarusLedgeManipulation::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

UCapsuleComponent * UIcarusLedgeManipulation::GetCapsuleReference()
{
	return (IS_VALID(GetOwnerReference())) ? GetOwnerReference()->GetCapsuleComponent() : nullptr;
}

ACharacter * UIcarusLedgeManipulation::GetOwnerReference()
{
	return Cast<ACharacter>(GetOwner());
}

TArray<AIcarusLedgeBase*> UIcarusLedgeManipulation::GetAllLedgeActors(float Radius, TArray<FHitResult> & HitResult)
{
	FVector PlayerCapsulePosition = GetCapsuleReference()->GetComponentLocation();
	
	UClass * FilterClass = AIcarusLedgeBase::StaticClass();
	HitResult  = UIcarusSystemLibrary::SpherePointTrace(GetOwner(), PlayerCapsulePosition, Radius, ECollisionChannel::ECC_GameTraceChannel1, FilterClass, true, false);
	TArray<AIcarusLedgeBase*> ReturnedLedges;
	
	for (FHitResult Hit : HitResult)
	{
		AIcarusLedgeBase* C_Ledge = Cast<AIcarusLedgeBase>(Hit.Actor.Get());
		if(C_Ledge) ReturnedLedges.Add(C_Ledge);
	}
	
	return ReturnedLedges;
}

AIcarusLedgeBase* UIcarusLedgeManipulation::GetNearlyLedgeActor(float Radius)
{
	FVector PlayerCapsulePosition = GetCapsuleReference()->GetComponentLocation();

	TArray<FHitResult> Results;
	TArray<AIcarusLedgeBase*> SearchedLedges = GetAllLedgeActors(Radius, Results);
	AActor * NearlyActor = UIcarusSystemLibrary::GetMostCloseActor(PlayerCapsulePosition, Results, EDistanceSearchBy::EDS_ImpactPoint);
	return Cast<AIcarusLedgeBase>(NearlyActor);
}

AIcarusLedgeBase* UIcarusLedgeManipulation::GetDirectionalNearlyLedgeActor(float Radius, FVector Direction, float MinDotAngle)
{
	FVector PlayerCapsulePosition = GetCapsuleReference()->GetComponentLocation();

	TArray<FHitResult> Results;
	TArray<AIcarusLedgeBase*> SearchedLedges = GetAllLedgeActors(Radius, Results);
	TArray<FHitResult> ResultsAux;
	for (FHitResult Hit : Results)
	{
		float Dot = FVector::DotProduct(GetPointDirectionToHead(Hit.ImpactPoint), Direction.GetSafeNormal());
		if (Dot > MinDotAngle)
		{
			ResultsAux.Add(Hit);
		}
	}
	AActor * NearlyActor = UIcarusSystemLibrary::GetMostCloseActor(PlayerCapsulePosition, ResultsAux, EDistanceSearchBy::EDS_ImpactPoint);
	return Cast<AIcarusLedgeBase>(NearlyActor);
}

FVector UIcarusLedgeManipulation::GetNextRightPoint(AIcarusLedgeBase* Ledge, FVector CurrentPoint, float Distance)
{
	if (!Ledge) return FVector::ZeroVector;

	FVector PointOnLedge = GetClosestPointOnLedgeActor(CurrentPoint, Ledge);
	FVector LocalPointOnLedge = Ledge->GetActorTransform().InverseTransformPosition(PointOnLedge);

	if (LocalPointOnLedge.X <= Ledge->EndPoint.X)
	{
		return Ledge->GetEndPointWorldCoordinate();
	}

	return PointOnLedge + (Ledge->GetSegmentDirection() * Distance);
}

FVector UIcarusLedgeManipulation::GetNextLeftPoint(AIcarusLedgeBase* Ledge, FVector CurrentPoint, float Distance)
{
	if (!Ledge) return FVector::ZeroVector;

	FVector PointOnLedge = GetClosestPointOnLedgeActor(CurrentPoint, Ledge);
	FVector LocalPointOnLedge = Ledge->GetActorTransform().InverseTransformPosition(PointOnLedge);

	if (LocalPointOnLedge.X >= 0.0f)
	{
		return Ledge->GetActorLocation();
	}

	return PointOnLedge + (Ledge->GetSegmentDirection() * -1.0f * Distance);
}

FVector UIcarusLedgeManipulation::GetLocalPointOnEdge(AIcarusLedgeBase* Ledge, FVector Point)
{
	return Ledge->GetActorTransform().InverseTransformPosition(Point);
}

FVector UIcarusLedgeManipulation::GetWorldPointOnEdge(AIcarusLedgeBase* Ledge, FVector Point)
{
	return Ledge->GetActorTransform().TransformPosition(Point);
}

FVector UIcarusLedgeManipulation::GetTransformedOutLimitPoint(AIcarusLedgeBase* Ledge, FVector Point, float EdgeLimit, bool &bOutOfLeft, bool &bOutOfRight)
{
	FVector PointLocalOnLedge = GetLocalPointOnEdge(Ledge, Point);

	if (FMath::Abs( PointLocalOnLedge.X ) < EdgeLimit)
	{
		PointLocalOnLedge.X = -EdgeLimit;
		bOutOfLeft = true;
	}

	if (FMath::Abs(PointLocalOnLedge.X) > (FMath::Abs(Ledge->EndPoint.X) - EdgeLimit))
	{
		PointLocalOnLedge.X = Ledge->EndPoint.X + EdgeLimit;
		bOutOfRight = true;
	}

	FVector PointWorldOnLedge = GetWorldPointOnEdge(Ledge, PointLocalOnLedge);

	return PointWorldOnLedge;
}

FVector UIcarusLedgeManipulation::GetTransformedOutLimitPoint(AIcarusLedgeBase* Ledge, FVector Point, float EdgeLimit)
{
	bool AuxBool;
	return GetTransformedOutLimitPoint(Ledge, Point, EdgeLimit, AuxBool, AuxBool);
}

float UIcarusLedgeManipulation::DistanceRemaningToLeftLimit(AIcarusLedgeBase* Ledge, FVector CurrentPoint, float EdgeLimit)
{
	FVector Limit = GetTransformedOutLimitPoint(Ledge, Ledge->GetActorLocation(), EdgeLimit);
	return (Limit - CurrentPoint).Size();
}

float UIcarusLedgeManipulation::DistanceRemaningToRightLimit(AIcarusLedgeBase* Ledge, FVector CurrentPoint, float EdgeLimit)
{
	FVector Limit = GetTransformedOutLimitPoint(Ledge, Ledge->GetEndPointWorldCoordinate(), EdgeLimit);
	return (Limit - CurrentPoint).Size();
}

bool UIcarusLedgeManipulation::GetPointByDirection(FVector Direction, FVector CurrentPoint, float MinDistance, float MaxDistance, AIcarusLedgeBase* &LedgeResult, FVector &PointResult)
{
	TArray<FHitResult> Results;
	TArray<AIcarusLedgeBase*> SearchedLedges = GetAllLedgeActors(256.0f, Results);

	float AuxBestDistance = MaxDistance;

	for (AIcarusLedgeBase* Ledge : SearchedLedges)
	{
		FVector PointOnEdge = GetCharacterClosestPointOnLedgeActor(Ledge);
		float Distance = (PointOnEdge - CurrentPoint).Size();
		float DotPoint = FVector::DotProduct(Direction.GetSafeNormal(), (PointOnEdge - CurrentPoint).GetSafeNormal());
		if (DotPoint > 0.9 && Distance >= MinDistance && Distance <= MaxDistance && Distance <= AuxBestDistance)
		{
			AuxBestDistance = Distance;
			LedgeResult = Ledge;
			PointResult = PointOnEdge;
		}
	}

	return (IS_VALID(LedgeResult));
}

FVector UIcarusLedgeManipulation::GetPointDirectionToHead(FVector PointOnEdge)
{
	return (PointOnEdge - GetHeadPointReference()).GetSafeNormal();
}

float UIcarusLedgeManipulation::GetSegmentCoveredDistance(AIcarusLedgeBase* Ledge, FVector CurrentPoint, float ExtremityOffset)
{
	if (!Ledge) return 0.0f;

	FVector PointOnLedge = GetClosestPointOnLedgeActor(CurrentPoint, Ledge);
	FVector LocalPointOnLedge = Ledge->GetActorTransform().InverseTransformPosition(PointOnLedge);
	return FMath::GetMappedRangeValueClamped(FVector2D(0.0f - ExtremityOffset, Ledge->EndPoint.X + ExtremityOffset), FVector2D(0.0f, 1.0f), LocalPointOnLedge.X);
}

float UIcarusLedgeManipulation::GetSegmentLenght(AIcarusLedgeBase* Ledge)
{
	if (!Ledge) return 0.0f;

	return Ledge->EndPoint.X;
}

FVector UIcarusLedgeManipulation::GetClosestPointOnLedgeActor(FVector Point, AIcarusLedgeBase* LedgeActor)
{
	FVector Result = FMath::ClosestPointOnSegment(Point, LedgeActor->GetActorLocation(), LedgeActor->GetEndPointWorldCoordinate());
	return Result;
}

FVector UIcarusLedgeManipulation::GetCharacterClosestPointOnLedgeActor(AIcarusLedgeBase* LedgeActor)
{
	if (!LedgeActor) return FVector();
	FVector Result = FMath::ClosestPointOnSegment(GetHeadPointReference(), LedgeActor->GetActorLocation(), LedgeActor->GetEndPointWorldCoordinate());
	return Result;
}

float UIcarusLedgeManipulation::GetDistanceFromPointToHeadReference(FVector Point)
{
	return (GetHeadPointReference() - Point).Size();
}

FVector UIcarusLedgeManipulation::GetHeadPointReference()
{
	return
	GetOwnerReference()->GetActorTransform().TransformPosition(
		FVector(0.0f, 0.0f,
		(GetOwnerReference()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight())
		)
	);
}

FVector UIcarusLedgeManipulation::GetRootBonePointReference()
{
	return GetOwnerReference()->GetMesh()->GetSocketLocation(RootReferenceBone);
}

FVector UIcarusLedgeManipulation::AddLedgeCapsuleOffset(FVector Point, AIcarusLedgeBase * Ledge)
{
	Point += (GetCapsuleReference()->GetScaledCapsuleRadius() + 7.0f) * Ledge->GetSegmentNormal();
	return Point;
}

FVector UIcarusLedgeManipulation::AddHeadReferenceCapsuleOffset(FVector Point)
{
	Point -= ((GetHeadPointReference() - GetCapsuleReference()->GetComponentLocation()) + CapsuleZOffset) * FVector(0.0f, 0.0f, 1.0f);
	return Point;
}

FVector UIcarusLedgeManipulation::GetRootBoneClosestPointOnLedgeActor(AIcarusLedgeBase * Ledge)
{
	FVector Result = FMath::ClosestPointOnSegment(GetRootBonePointReference(), Ledge->GetActorLocation(), Ledge->GetEndPointWorldCoordinate());
	return Result;
}

bool UIcarusLedgeManipulation::CheckSufficientHeight(FVector PointToCheck, float Gap)
{
	return ((PointToCheck.Z - GetHeadPointReference().Z) > Gap);
}

bool UIcarusLedgeManipulation::CheckUnderLedgeAmount(FVector PointToCheck, float &DotResult)
{
	FVector TargetDirection = (GetCapsuleReference()->GetComponentLocation() - PointToCheck).GetSafeNormal();
	DotResult = FVector::DotProduct(TargetDirection, FVector{0.0f, 0.0f, -1.0f});
	return (DotResult > UnderLedgeDotCheck);
}

bool UIcarusLedgeManipulation::CheckJumpVelocityToTarget(FVector PointToCheck, float &DotResult)
{
	FVector Velocity = GetOwnerReference()->GetCharacterMovement()->Velocity.GetSafeNormal();
	FVector TargetDirection = (GetHeadPointReference() - PointToCheck).GetSafeNormal();
	DotResult = FMath::Abs(FVector::DotProduct(TargetDirection, Velocity));
	return (DotResult > JumpVelocityDotCheck);
}

bool UIcarusLedgeManipulation::CheckForwardAngle(AIcarusLedgeBase* Ledge, float &AngleResult)
{
	FVector LedgeNormal = Ledge->GetSegmentNormal();
	FVector2D Diff = (FVector2D(LedgeNormal)).GetSafeNormal(); //FVector2D(GetOwnerReference()->GetCapsuleComponent()->GetComponentLocation() - GetCharacterClosestPointOnLedgeActor(Ledge)).GetSafeNormal();
	AngleResult = FVector2D::DotProduct(FVector2D(GetOwnerReference()->GetCapsuleComponent()->GetForwardVector()), Diff);
	return (AngleResult <= ForwardAngleDotCheck);
}