// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FSuperManagerModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
#pragma region ContentBrowserMenuExtention

	void InitCBMenuExtention();
	TSharedRef<FExtender> CustomCBMenuExtender(const TArray<FString>& SelectedPaths);
	TArray<FString> FolderPathsSelected;
	void AddCBMenuEntry(class FMenuBuilder& MenuBuilder);
	void OnDeleteUnusedAssetClicked();
	void OnDeleteEmptyFoldersButtonClicked();
	void OnAdvanceDeletionButtonClicked();

	// --- 新增的代码 ---
	void OnProcessSkeletalMeshesButtonClicked();
	// -----------------

	// --- 新增：资产右键菜单扩展 ---
	void InitCBAssetMenuExtention(); // 初始化资产菜单扩展
	TSharedRef<FExtender> CustomCBAssetMenuExtender(const TArray<FAssetData>& SelectedAssets); // 定义扩展逻辑
	void AddCBAssetMenuEntry(class FMenuBuilder& MenuBuilder); // 添加菜单项
	void OnCheckMaterialMismatch(); // 点击后执行的函数
	// ---------------------------
	void FixUpRedirectors();

#pragma endregion
#pragma region CustomEditorTab

	void RegisterAdvanceDeletionTab();
	TSharedRef<SDockTab> OnSpawnAdvanceDeletionTab(const FSpawnTabArgs& SpawnTabArgs);
	TSharedPtr<SDockTab> ConstructedDockTab;

	TArray< TSharedPtr <FAssetData> > GetAllAssetDataUnderSelectedFolder();

	void OnAdvanceDeletionTabClosed(TSharedRef<SDockTab> TabToClose);

#pragma endregion

#pragma region LevelEditorMenuExtension

	void InitLevelEditorExtention();
	TSharedRef<FExtender> CustomLevelEditorMenuExtender(const TSharedRef<FUICommandList> UICommandList, const TArray<AActor*> SelectedActors);
	void AddLevelEditorMenuEntry(class FMenuBuilder& MenuBuilder);

	void OnLockActorSelectionButtonClicked();
	void OnUnlockActorSelectionButtonClicked();
#pragma endregion

#pragma region SelectionLock
	void InitCustomSelectionEvent();
	void OnActorSelected(UObject* SelectedObject);
	void LockActorSelection(AActor* ActorToProcess);
	void UnlockActorSelection(AActor* ActorToProcess);

#pragma endregion
#pragma region SceneOutlinerExtension
	void InitSceneOutlinerColumnExtension();
	TSharedRef<class ISceneOutlinerColumn> OnCreateSelectionLockColumn(class ISceneOutliner& SceneOutliner);
#pragma endregion

#pragma region CustomEditorUICommands
	TSharedPtr<class FUICommandList> CustomUICommands;
	void InitCustomUICommands();
	void OnSelectionLockHotKeyPressed();
	void OnUnlockActorSelectionHotKeyPressed();
#pragma endregion

	TWeakObjectPtr<class UEditorActorSubsystem> WeakEditorActorSubsystem;
	bool GetEditorActorSubsystem();
public:
#pragma region ProccessDataForAdvanceDeletionTab
	bool DeleteSingleAssetForAssetList(const FAssetData& AssetDataToDelete);
	bool DeleteMultipleAssetsForAssetList(const TArray<FAssetData>& AssetsToDelete);
	void ListUnusedAssetsForAssetList(const TArray< TSharedPtr <FAssetData> >& AssetsDataToFilter, TArray< TSharedPtr <FAssetData> >& OutUnusedAssetsData);
	void ListSameNameAssetsForAssetList(const TArray< TSharedPtr <FAssetData> >& AssetsDataToFilter, TArray< TSharedPtr <FAssetData> >& OutSameNameAssetsData);
	void SyncCBToClickedAssetForAssetList(const FString& AssetPathToSync);
	void RefreshSceneOutliner();

#pragma endregion

	bool CheckIsActorSelectionLocked(AActor* ActorToProcess);
	void ProcessLockingForOutliner(AActor* ActorToProcess, bool bShouldLock);
};

