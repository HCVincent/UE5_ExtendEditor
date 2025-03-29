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
					if (Distance <= FoliageCleanupRadius)
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
				
				// Mark component as modified to ensure changes are saved
				HISMComponent->Modify();
				bActorModified = true;
			}
		}
		
		if (bActorModified)
		{
			// Mark the actor as modified so changes get saved
			Actor->Modify();
			Actor->MarkPackageDirty();
			bWorldModified = true;
			TotalRemovedInstances += RemovedInstances;
		}
	}
	
	// Make sure changes are reflected in the editor
	if (bWorldModified)
	{
		// Mark the world as dirty so it will be saved
		World->MarkPackageDirty();
		
		// Update the world to reflect changes
		FEditorDelegates::RefreshAllBrowsers.Broadcast();
		
		// Force save the current level
		// In UE5.3, we use FEditorFileUtils::SaveCurrentLevel()
		TArray<UPackage*> PackagesToSave;
		PackagesToSave.Add(World->GetPackage());
		FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, false, false);
		
		DebugHeader::ShowNInfo(FString::Printf(TEXT("Successfully removed and saved %d foliage instances within radius"),
			TotalRemovedInstances));
	}
	else
	{
		DebugHeader::ShowNInfo(TEXT("No foliage instances found within the specified radius"));
	}
}

bool UCleanActorWidget::GetEditorActorSubsystem()
{
	if (!EditorActorSubsystem) {
		EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
	}
	return EditorActorSubsystem != nullptr;
}
