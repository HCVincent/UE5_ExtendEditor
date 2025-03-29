// Fill out your copyright notice in the Description page of Project Settings.


#include "ActorActions/CleanActorWidget.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Engine/StaticMeshActor.h"
#include "DebugHeader.h"

void UCleanActorWidget::CleanActorsFromPoint()
{
	if (!GetEditorActorSubsystem()) return;
	
	if (CleanupRadius <= 0)
	{
		DebugHeader::ShowNInfo(TEXT("Cleanup radius must be greater than 0"));
		return;
	}
	
	TArray<AActor*> AllLevelActors = EditorActorSubsystem->GetAllLevelActors();
	int32 DeletedActorsCount = 0;
	
	for (AActor* Actor : AllLevelActors)
	{
		if (!Actor) continue;
		
		// Check if the actor is a static mesh actor
		AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(Actor);
		if (!StaticMeshActor) continue;
		
		// Calculate distance from point to actor
		float Distance = FVector::Distance(Actor->GetActorLocation(), CleanupCenter);
		
		// If actor is within radius, delete it
		if (Distance <= CleanupRadius)
		{
			if (EditorActorSubsystem->DestroyActor(Actor))
			{
				DeletedActorsCount++;
			}
		}
	}
	
	if (DeletedActorsCount > 0)
	{
		DebugHeader::ShowNInfo(TEXT("Successfully deleted ") + 
			FString::FromInt(DeletedActorsCount) + TEXT(" static mesh actors within radius"));
	}
	else
	{
		DebugHeader::ShowNInfo(TEXT("No static mesh actors found within the specified radius"));
	}
}

bool UCleanActorWidget::GetEditorActorSubsystem()
{
	if (!EditorActorSubsystem) {
		EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
	}
	return EditorActorSubsystem != nullptr;
}
