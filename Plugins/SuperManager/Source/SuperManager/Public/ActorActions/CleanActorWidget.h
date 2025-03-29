// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
#include "CleanActorWidget.generated.h"

/**
 * 
 */
UCLASS()
class SUPERMANAGER_API UCleanActorWidget : public UEditorUtilityWidget
{
	GENERATED_BODY()

public:

#pragma region CleanActor
	UFUNCTION(BlueprintCallable, Category = "ActorClean")
	void CleanActorsFromPoint();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActorClean")
	FVector CleanupCenter = FVector::ZeroVector;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActorClean", meta = (ClampMin = "0.0"))
	float CleanupRadius = 500.f;
#pragma endregion

#pragma region CleanOverlappedActor
	
#pragma endregion

private:
	UPROPERTY()
	class UEditorActorSubsystem* EditorActorSubsystem;
	
	bool GetEditorActorSubsystem();
	
};
