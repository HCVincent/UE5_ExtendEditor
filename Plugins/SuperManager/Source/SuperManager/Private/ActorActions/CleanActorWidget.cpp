// Fill out your copyright notice in the Description page of Project Settings.


#include "ActorActions/CleanActorWidget.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Engine/StaticMeshActor.h"
#include "DebugHeader.h"
#include "InstancedFoliageActor.h"
#include "EngineUtils.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Editor.h"
#include "FileHelpers.h"

void UCleanActorWidget::CleanActorsFromPoint()
{
	if (!GetEditorActorSubsystem()) return;
	
	if (CleanupRadius <= 0)
	{
		DebugHeader::ShowNInfo(TEXT("Cleanup radius must be greater than 0"));
		return;
	}
	
	// Begin an undoable transaction
	GEditor->BeginTransaction(FText::FromString(TEXT("Clean Static Mesh Actors")));
	
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
			// Record this actor for undo before destroying it
			Actor->Modify();
			
			if (EditorActorSubsystem->DestroyActor(Actor))
			{
				DeletedActorsCount++;
			}
		}
	}
	
	// End the transaction
	GEditor->EndTransaction();
	
	if (DeletedActorsCount > 0)
	{
		DebugHeader::ShowNInfo(TEXT("Successfully deleted ") + 
			FString::FromInt(DeletedActorsCount) + TEXT(" static mesh actors within radius. Use Ctrl+Z to undo if needed."));
	}
	else
	{
		DebugHeader::ShowNInfo(TEXT("No static mesh actors found within the specified radius"));
	}
}

void UCleanActorWidget::CleanFoliageActorsFromPoint()
{
	if (!GetEditorActorSubsystem()) return;
	
	if (FoliageCleanupRadius <= 0)
	{
		DebugHeader::ShowNInfo(TEXT("Foliage cleanup radius must be greater than 0"));
		return;
	}
	
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		DebugHeader::ShowNInfo(TEXT("Failed to get world context"));
		return;
	}
	
	// Begin an undoable transaction
	GEditor->BeginTransaction(FText::FromString(TEXT("Clean Foliage Instances")));
	
	int32 TotalRemovedInstances = 0;
	bool bWorldModified = false;
	
	// Use actor iteration to find all foliage actors
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor) continue;
		
		// Look for actors with hierarchical instanced static mesh components
		TArray<UHierarchicalInstancedStaticMeshComponent*> HISMComponents;
		Actor->GetComponents<UHierarchicalInstancedStaticMeshComponent>(HISMComponents);
		
		// Skip if no HISM components found (not a foliage actor or similar)
		if (HISMComponents.Num() == 0) continue;
		
		int32 RemovedInstances = 0;
		bool bActorModified = false;
		
		// Process each HISM component
		for (UHierarchicalInstancedStaticMeshComponent* HISMComponent : HISMComponents)
		{
			if (!HISMComponent) continue;
			
			// Record component for undo
			HISMComponent->Modify();
			
			// Get instance count
			const int32 InstanceCount = HISMComponent->GetInstanceCount();
			TArray<int32> InstancesToRemove;
			
			// Check each instance
			for (int32 InstanceIndex = 0; InstanceIndex < InstanceCount; InstanceIndex++)
			{
				FTransform InstanceTransform;
				if (HISMComponent->GetInstanceTransform(InstanceIndex, InstanceTransform, true))
				{
					float Distance = FVector::Distance(InstanceTransform.GetLocation(), FoliageCleanupCenter);
					
					// If bCleanInsideRadius is true, remove instances inside the radius.
					// If false, remove instances outside the radius.
					if ((bCleanInsideRadius && Distance <= FoliageCleanupRadius) || 
						(!bCleanInsideRadius && Distance > FoliageCleanupRadius))
					{
						InstancesToRemove.Add(InstanceIndex);
					}
				}
			}
			
			// Remove instances in reverse order to maintain instance indices
			if (InstancesToRemove.Num() > 0)
			{
				// Sort in descending order to remove from end to start
				InstancesToRemove.Sort([](const int32& A, const int32& B) { return A > B; });
				
				for (int32 IndexToRemove : InstancesToRemove)
				{
					HISMComponent->RemoveInstance(IndexToRemove);
					RemovedInstances++;
				}
				
				bActorModified = true;
			}
		}
		
		if (bActorModified)
		{
			// Mark the actor as modified
			Actor->Modify();
			Actor->MarkPackageDirty();
			bWorldModified = true;
			TotalRemovedInstances += RemovedInstances;
		}
	}
	
	// End the transaction
	GEditor->EndTransaction();
	
	// Make sure changes are reflected in the editor
	if (bWorldModified)
	{
		// Mark the world as dirty so changes are tracked
		World->MarkPackageDirty();
		
		// Update the world to reflect changes
		FEditorDelegates::RefreshAllBrowsers.Broadcast();
		
		FString CleanupLocation = bCleanInsideRadius ? TEXT("within") : TEXT("outside");
		DebugHeader::ShowNInfo(FString::Printf(TEXT("Successfully removed %d foliage instances %s radius. Use Ctrl+Z to undo if needed."),
			TotalRemovedInstances, *CleanupLocation));
	}
	else
	{
		FString CleanupLocation = bCleanInsideRadius ? TEXT("within") : TEXT("outside");
		DebugHeader::ShowNInfo(FString::Printf(TEXT("No foliage instances found %s the specified radius"), 
			*CleanupLocation));
	}
}

bool UCleanActorWidget::GetEditorActorSubsystem()
{
	if (!EditorActorSubsystem) {
		EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
	}
	return EditorActorSubsystem != nullptr;
}
