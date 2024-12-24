// Fill out your copyright notice in the Description page of Project Settings.
#include "ActorActions/QuickActorActionsWidget.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "DebugHeader.h"
void UQuickActorActionsWidget::SelectAllActorsWithSimilarName()
{
	if (!GetEditorActorSubsystem()) return;
	TArray<AActor*> SelectedActors = EditorActorSubsystem->GetSelectedLevelActors();
	uint32 SelectionCounter = 0;
	if (SelectedActors.Num() == 0)
	{
		DebugHeader::ShowNInfo(TEXT("No actor selected"));
		return;
	}
	if (SelectedActors.Num() > 1)
	{
		DebugHeader::ShowNInfo(TEXT("You can only select one actor"));
		return;
	}
	FString SelectedActorName = SelectedActors[0]->GetActorLabel();
	const FString NameToSearch = SelectedActorName.LeftChop(4);
	TArray<AActor*> AllLeveActors = EditorActorSubsystem->GetAllLevelActors();
	for (AActor* ActorInLevel : AllLeveActors)
	{
		if (!ActorInLevel) continue;
		if (ActorInLevel->GetActorLabel().Contains(NameToSearch, SearchCase))
		{
			EditorActorSubsystem->SetActorSelectionState(ActorInLevel, true);
			SelectionCounter++;
		}
	}
	if (SelectionCounter > 0)
	{
		DebugHeader::ShowNInfo(TEXT("Successfully selected ") +
			FString::FromInt(SelectionCounter) + TEXT(" actors"));
	}
	else
	{
		DebugHeader::ShowNInfo(TEXT("No actor with similar name found"));
	}
}

void UQuickActorActionsWidget::DuplicateActors()
{
	if (!GetEditorActorSubsystem()) return;
	TArray<AActor*> SelectedActors = EditorActorSubsystem->GetSelectedLevelActors();
	uint32 Counter = 0;
	if (SelectedActors.Num() == 0)
	{
		DebugHeader::ShowNInfo(TEXT("No actor selected"));
		return;
	}
	if (NumberOfDuplicates <= 0 || OffsetDist == 0)
	{
		DebugHeader::ShowNInfo(TEXT("Did not specify a number of duplications or an offset distance"));
		return;
	}

	// Ensure rotation angle is between 0 and 360
	DuplicationRotationAngle = FMath::Fmod(DuplicationRotationAngle, 360.0f);
	if (DuplicationRotationAngle < 0)
	{
		DuplicationRotationAngle += 360.0f;
	}

	const float AngleInRadians = FMath::DegreesToRadians(DuplicationRotationAngle);
	const float CosTheta = FMath::Cos(AngleInRadians);
	const float SinTheta = FMath::Sin(AngleInRadians);

	for (AActor* SelectedActor : SelectedActors)
	{
		if (!SelectedActor) continue;
		for (int32 i = 0; i < NumberOfDuplicates; i++)
		{
			AActor* DuplicatedActor =
				EditorActorSubsystem->DuplicateActor(SelectedActor, SelectedActor->GetWorld());
			if (!DuplicatedActor) continue;

			const float DuplicationOffsetDist = (i + 1) * OffsetDist;
			FVector OffsetVector;

			switch (AxisForDuplication)
			{
			case E_DuplicationAxis::EDA_XAxis:
				// Rotate around Z axis in XY plane
				OffsetVector = FVector(
					DuplicationOffsetDist * CosTheta,
					DuplicationOffsetDist * SinTheta,
					0.f);
				break;
			case E_DuplicationAxis::EDA_YAxis:
				// Rotate around Z axis in XY plane, but starting from Y axis
				OffsetVector = FVector(
					-DuplicationOffsetDist * SinTheta,
					DuplicationOffsetDist * CosTheta,
					0.f);
				break;
			case E_DuplicationAxis::EDA_ZAxis:
				// Rotate around Y axis in XZ plane
				OffsetVector = FVector(
					DuplicationOffsetDist * SinTheta,
					0.f,
					DuplicationOffsetDist * CosTheta);
				break;
			default:
				continue;
			}

			DuplicatedActor->AddActorWorldOffset(OffsetVector);
			EditorActorSubsystem->SetActorSelectionState(DuplicatedActor, true);
			Counter++;
		}
	}
	if (Counter > 0)
	{
		DebugHeader::ShowNInfo(TEXT("Successfully duplicated ") +
			FString::FromInt(Counter) + TEXT(" actors"));
	}
}

void UQuickActorActionsWidget::RandomizeActorTransform()
{
	const bool bConditionNotSet =
		!RandomActorRotation.bRandomizeRotYaw &&
		!RandomActorRotation.bRandomizeRotPitch &&
		!RandomActorRotation.bRandomizeRotRoll &&
		!bRandomizeScale &&
		!bRandomizeOffset;;
	if (bConditionNotSet)
	{
		DebugHeader::ShowNInfo(TEXT("No variation condition specified"));
		return;
	}
	if (!GetEditorActorSubsystem()) return;
	TArray<AActor*> SelectedActors = EditorActorSubsystem->GetSelectedLevelActors();
	uint32 Counter = 0;
	if (SelectedActors.Num() == 0)
	{
		DebugHeader::ShowNInfo(TEXT("No actor selected"));
		return;
	}
	for (AActor* SelectedActor : SelectedActors)
	{
		if (!SelectedActor) continue;
		if (RandomActorRotation.bRandomizeRotYaw)
		{
			const float RandomRotYawValue = FMath::RandRange(RandomActorRotation.RotYawMin, RandomActorRotation.RotYawMax);

			SelectedActor->AddActorWorldRotation(FRotator(0.f, RandomRotYawValue, 0.f));
		}
		if (RandomActorRotation.bRandomizeRotPitch)
		{
			const float RandomRotPitchValue = FMath::RandRange(RandomActorRotation.RotPitchMin, RandomActorRotation.RotPitchMax);
			SelectedActor->AddActorWorldRotation(FRotator(RandomRotPitchValue, 0.f, 0.f));
		}
		if (RandomActorRotation.bRandomizeRotRoll)
		{
			const float RandomRotRollValue = FMath::RandRange(RandomActorRotation.RotRollMin, RandomActorRotation.RotRollMax);
			SelectedActor->AddActorWorldRotation(FRotator(0.f, 0.f, RandomRotRollValue));
		}
		if (bRandomizeScale)
		{
			SelectedActor->SetActorScale3D(FVector(FMath::RandRange(ScaleMin, ScaleMax)));
		}
		if (bRandomizeOffset)
		{
			const float RandomOffsetValue = FMath::RandRange(OffsetMin, OffsetMax);
			SelectedActor->AddActorWorldOffset(FVector(RandomOffsetValue, RandomOffsetValue, 0.f));
		}
		Counter++;
	}
	if (Counter > 0)
	{
		DebugHeader::ShowNInfo(TEXT("Successfully set ") +
			FString::FromInt(Counter) + TEXT(" actors"));
	}
}

bool UQuickActorActionsWidget::GetEditorActorSubsystem()
{
	if (!EditorActorSubsystem)
	{
		EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
	}

	return EditorActorSubsystem != nullptr;
}