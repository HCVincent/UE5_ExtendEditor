// Copyright Epic Games, Inc. All Rights Reserved.

#include "SuperManager.h"
#include "ContentBrowserModule.h"
#include "DebugHeader.h"
#include "EditorAssetLibrary.h"
#include "ObjectTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "SlateWidgets/AdvanceDeletionWidget.h"
#include "CustomStyle/SuperManagerStyle.h"
#include "LevelEditor.h"
#include "Engine/Selection.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "SceneOutlinerModule.h"
#include "CustomOutlinerColumn/OutlinerSelectionColumn.h"
#include "CustomUICommands/SuperManagerUICommands.h"
// --- 新增的引用 ---
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h" // 必须包含这个才能访问 FSkeletalMaterial
// -----------------

#include "EditorUtilityLibrary.h" // 用于获取选中的资产

#define LOCTEXT_NAMESPACE "FSuperManagerModule"

void FSuperManagerModule::StartupModule()
{
	FSuperManagerStyle::InitializeIcons();
	FSuperManagerUICommands::Register();
	InitCustomUICommands();
	InitCBMenuExtention();
	InitCBAssetMenuExtention(); // <--- 新增：初始化资产扩展
	InitLevelEditorExtention();
	InitCustomSelectionEvent();
	InitSceneOutlinerColumnExtension();
}



#pragma region ContentBrowserMenuExtention
void FSuperManagerModule::InitCBMenuExtention()
{
	FContentBrowserModule& ContentBrowserModule =
		FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	TArray<FContentBrowserMenuExtender_SelectedPaths>& ContentBrowserModuleMenuExtenders =
		ContentBrowserModule.GetAllPathViewContextMenuExtenders();
	FContentBrowserMenuExtender_SelectedPaths CustomCBMenuDelegate;
	ContentBrowserModuleMenuExtenders.Add(CustomCBMenuDelegate);

	/*FContentBrowserMenuExtender_SelectedPaths CustomCBMenuDelegate;
	CustomCBMenuDelegate.BindRaw(this,&FSuperManagerModule::CustomCBMenuExtender);
	ContentBrowserModuleMenuExtenders.Add(CustomCBMenuDelegate);*/
	ContentBrowserModuleMenuExtenders.Add(FContentBrowserMenuExtender_SelectedPaths::
		CreateRaw(this, &FSuperManagerModule::CustomCBMenuExtender));
}

TSharedRef<FExtender> FSuperManagerModule::CustomCBMenuExtender(const TArray<FString>& SelectedPaths)
{
	TSharedRef<FExtender> MenuExtender(new FExtender());
	if (SelectedPaths.Num() > 0)
	{
		MenuExtender->AddMenuExtension(FName("Delete"), //Extend hook, position to insert
			EExtensionHook::After, //Insert before or after
			TSharedPtr<FUICommandList>(), //Custom hot keys 
			FMenuExtensionDelegate::CreateRaw(this, &FSuperManagerModule::AddCBMenuEntry)); //Second binding, will define details for this menu entry

		FolderPathsSelected = SelectedPaths;
	}
	return MenuExtender;
}

void FSuperManagerModule::AddCBMenuEntry(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry
	(
		FText::FromString(TEXT("Delete Unused Assets")), //Title text for menu entry
		FText::FromString(TEXT("Safely delete all unused assets under folder")), //Tooltip text
		FSlateIcon(FSuperManagerStyle::GetStyleSetName(), "ContentBrowser.DeleteUnusedAssets"),	//Custom icon
		FExecuteAction::CreateRaw(this, &FSuperManagerModule::OnDeleteUnusedAssetClicked) //The actual function to excute
	);
	MenuBuilder.AddMenuEntry
	(
		FText::FromString(TEXT("Delete Empty Folders")), //Title text for menu entry
		FText::FromString(TEXT("Safely delete all empty folders")), //Tooltip text
		FSlateIcon(FSuperManagerStyle::GetStyleSetName(), "ContentBrowser.DeleteEmptyFolders"),	//Custom icon
		FExecuteAction::CreateRaw(this, &FSuperManagerModule::OnDeleteEmptyFoldersButtonClicked) //The actual function to excute
	);
	MenuBuilder.AddMenuEntry
	(
		FText::FromString(TEXT("Advance Deletion")), //Title text for menu entry
		FText::FromString(TEXT("List assets by specific condition in a tab for deleting")), //Tooltip text
		FSlateIcon(FSuperManagerStyle::GetStyleSetName(), "ContentBrowser.AdvanceDeletion"),	//Custom icon
		FExecuteAction::CreateRaw(this, &FSuperManagerModule::OnAdvanceDeletionButtonClicked) //The actual function to excute
	);
	// --- 新增的代码 ---
	MenuBuilder.AddMenuEntry
	(
		FText::FromString(TEXT("List Skeletal Meshes")), // 菜单名称
		FText::FromString(TEXT("Print names of all skeletal meshes in this folder recursively")), // 提示文本
		FSlateIcon(FSuperManagerStyle::GetStyleSetName(), "ContentBrowser.DeleteUnusedAssets"), // 暂时复用现有图标，或者填 FSlateIcon() 用默认的
		FExecuteAction::CreateRaw(this, &FSuperManagerModule::OnProcessSkeletalMeshesButtonClicked) // 绑定的函数
	);
	// -----------------
}

void FSuperManagerModule::OnDeleteUnusedAssetClicked()
{
	if (ConstructedDockTab.IsValid())
	{
		DebugHeader::ShowMsgDialog(EAppMsgType::Ok, TEXT("Please close advance deletion tab before this operation"));
		return;
	}
	if (FolderPathsSelected.Num() > 1)
	{
		DebugHeader::ShowMsgDialog(EAppMsgType::Ok, TEXT("You can only do this to one folder"));
		return;
	}
	TArray<FString> AssetsPathNames = UEditorAssetLibrary::ListAssets(FolderPathsSelected[0]);
	if (AssetsPathNames.Num() == 0)
	{
		DebugHeader::ShowMsgDialog(EAppMsgType::Ok, TEXT("No asset found under selected folder"), false);
		return;
	}
	EAppReturnType::Type ConfirmResult =
		DebugHeader::ShowMsgDialog(EAppMsgType::YesNo, TEXT("A total of ") + FString::FromInt(AssetsPathNames.Num())
			+ TEXT(" assets need to be checked.\nWould you like to procceed?"), false);
	if (ConfirmResult == EAppReturnType::No) return;

	FixUpRedirectors();

	TArray<FAssetData> UnusedAssetsDataArray;
	for (const FString& AssetPathName : AssetsPathNames)
	{
		//Don't touch root folder
		if (AssetPathName.Contains(TEXT("Developers")) ||
			AssetPathName.Contains(TEXT("Collections")) ||
			AssetPathName.Contains(TEXT("__ExternalActors__")) ||
			AssetPathName.Contains(TEXT("__ExternalObjects__")))
			{
				continue;
		}
		if (!UEditorAssetLibrary::DoesAssetExist(AssetPathName)) continue;

		// ����Ƿ�Ϊ�ؿ��ļ�
		FAssetData AssetData = UEditorAssetLibrary::FindAssetData(AssetPathName);
		if (AssetData.AssetClassPath.ToString() == TEXT("/Script/Engine.World"))  // ʹ�� AssetClassPath ����ʲ�����
		{
			continue;  // �����ؿ��ļ�
		}

		TArray<FString> AssetReferencers = UEditorAssetLibrary::FindPackageReferencersForAsset(AssetPathName);
		if (AssetReferencers.Num() == 0)
		{
			UnusedAssetsDataArray.Add(AssetData);
		}
	}

	if (UnusedAssetsDataArray.Num() > 0)
	{
		ObjectTools::DeleteAssets(UnusedAssetsDataArray);
	}
	else
	{
		DebugHeader::ShowMsgDialog(EAppMsgType::Ok, TEXT("No unused asset found under selected folder"),false);
	}
}

void FSuperManagerModule::OnDeleteEmptyFoldersButtonClicked()
{
	if (ConstructedDockTab.IsValid())
	{
		DebugHeader::ShowMsgDialog(EAppMsgType::Ok, TEXT("Please close advance deletion tab before this operation"));
		return;
	}
	FixUpRedirectors();
	TArray<FString> FolderPathsArray = UEditorAssetLibrary::ListAssets(FolderPathsSelected[0], true, true);
	uint32 Counter = 0;
	FString EmptyFolderPathsNames;
	TArray<FString> EmptyFoldersPathsArray;
	for (const FString& FolderPath : FolderPathsArray)
	{
		if (FolderPath.Contains(TEXT("Developers")) ||
			FolderPath.Contains(TEXT("Collections")) ||
			FolderPath.Contains(TEXT("__ExternalActors__")) ||
			FolderPath.Contains(TEXT("__ExternalObjects__")))
		{
			continue;
		}
		if (!UEditorAssetLibrary::DoesDirectoryExist(FolderPath)) continue;
		if (!UEditorAssetLibrary::DoesDirectoryHaveAssets(FolderPath))
		{
			EmptyFolderPathsNames.Append(FolderPath);
			EmptyFolderPathsNames.Append(TEXT("\n"));
			EmptyFoldersPathsArray.Add(FolderPath);
		}
	}
	if (EmptyFoldersPathsArray.Num() == 0)
	{
		DebugHeader::ShowMsgDialog(EAppMsgType::Ok, TEXT("No empty folder found under selected folder"), false);
		return;
	}
	EAppReturnType::Type ConfirmResult = DebugHeader::ShowMsgDialog(EAppMsgType::OkCancel,
		TEXT("Empty folders found in:\n") + EmptyFolderPathsNames + TEXT("\nWould you like to delete all?"), false);
	if (ConfirmResult == EAppReturnType::Cancel) return;

	for (const FString& EmptyFolderPath : EmptyFoldersPathsArray)
	{
		if (UEditorAssetLibrary::DeleteDirectory(EmptyFolderPath))
		{
			++Counter;
		}
		else
		{
			DebugHeader::Print(FString::Printf(TEXT("Failed to delete %s"), *EmptyFolderPath), FColor::Red);
		}
	}
	if (Counter > 0)
	{
		DebugHeader::ShowNInfo(TEXT("Successfully deleted ") + FString::FromInt(Counter) + TEXT("folders"));
	}
}

void FSuperManagerModule::OnAdvanceDeletionButtonClicked()
{
	FixUpRedirectors();
	FGlobalTabmanager::Get()->TryInvokeTab(FName("AdvanceDeletion"));
}

// SuperManager.cpp

void FSuperManagerModule::OnProcessSkeletalMeshesButtonClicked()
{
	if (FolderPathsSelected.Num() > 1)
	{
		DebugHeader::ShowMsgDialog(EAppMsgType::Ok, TEXT("You can only do this to one folder"));
		return;
	}

	const FString SelectedFolderPath = FolderPathsSelected[0];

	// 递归获取所有资产路径
	TArray<FString> AssetsPathNames = UEditorAssetLibrary::ListAssets(SelectedFolderPath, true, false);

	if (AssetsPathNames.Num() == 0)
	{
		DebugHeader::ShowMsgDialog(EAppMsgType::Ok, TEXT("No asset found under selected folder"), false);
		return;
	}

	FString OutputMessage = TEXT("找到了不匹配材质:\n");
	int32 ErrorCounter = 0;

	// 建议使用 ScopedSlowTask 显示进度条，因为 LoadAsset 可能较慢
	FScopedSlowTask SlowTask(AssetsPathNames.Num(), FText::FromString(TEXT("Checking Skeletal Mesh Materials...")));
	SlowTask.MakeDialog();

	for (const FString& AssetPathName : AssetsPathNames)
	{
		SlowTask.EnterProgressFrame(1.f);

		// 1. 快速过滤：先检查是不是 SkeletalMesh，避免加载无用资产
		FAssetData AssetData = UEditorAssetLibrary::FindAssetData(AssetPathName);
		if (!AssetData.IsValid() || AssetData.AssetClassPath.GetAssetName() != FName("SkeletalMesh"))
		{
			continue;
		}

		// 2. 加载资产 (必须加载才能访问材质槽)
		USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(UEditorAssetLibrary::LoadAsset(AssetPathName));
		if (!SkeletalMesh) continue;

		// 3. 检查材质槽
		const TArray<FSkeletalMaterial>& Materials = SkeletalMesh->GetMaterials();
		bool bHasIssue = false;
		FString AssetIssueString = FString::Printf(TEXT("\n[Asset]: %s\n[Path]: %s"),
			*SkeletalMesh->GetName(),
			*AssetData.PackageName.ToString());

		for (int32 i = 0; i < Materials.Num(); ++i)
		{
			const FSkeletalMaterial& SkeletalMaterial = Materials[i];

			// 获取槽名 (是在建模软件里给面命名的名字)
			FString SlotName = SkeletalMaterial.MaterialSlotName.ToString();

			// 获取实际赋予的材质
			UMaterialInterface* AssignedMaterial = SkeletalMaterial.MaterialInterface;

			// 检查 1: 材质是否为空
			if (!AssignedMaterial)
			{
				AssetIssueString.Append(FString::Printf(TEXT("\n  - Slot %d ('%s') is EMPTY"), i, *SlotName));
				bHasIssue = true;
				continue;
			}

			FString MaterialName = AssignedMaterial->GetName();

			// 检查 2: 材质名是否包含槽名 (区分大小写)
			// 必须完全相等 (严格模式)
			// ESearchCase::IgnoreCase 表示忽略大小写 (MI_Quinn_01 和 mi_quinn_01 算一样)
			// 如果你需要连大小写都必须一样，去掉 ESearchCase::IgnoreCase 即可
			if (!MaterialName.Equals(SlotName))
			{
				AssetIssueString.Append(FString::Printf(TEXT("\n  - Slot %d ('%s') 不匹配材质 ('%s')"),
					i, *SlotName, *MaterialName));
				bHasIssue = true;
			}
		}

		// 如果该资产有问题，记录下来
		if (bHasIssue)
		{
			OutputMessage.Append(AssetIssueString);
			ErrorCounter++;
		}
	}

	if (ErrorCounter > 0)
	{
		DebugHeader::ShowNInfo(TEXT("Found ") + FString::FromInt(ErrorCounter) + TEXT(" assets with issues. Check Output Log."));
		DebugHeader::PrtLog(OutputMessage);

		// 可选：弹窗提示
		DebugHeader::ShowMsgDialog(EAppMsgType::Ok, TEXT("找到了不匹配材质! 具体看output log窗口"));
	}
	else
	{
		DebugHeader::ShowNInfo(TEXT("All Skeletal Meshes look good!"));
	}
}

void FSuperManagerModule::FixUpRedirectors()
{
	TArray<UObjectRedirector*> RedirectorsToFixArray;
	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Emplace("/Game");
	Filter.ClassPaths.Add(UObjectRedirector::StaticClass()->GetClassPathName());
	TArray<FAssetData> OutRedirectors;
	AssetRegistryModule.Get().GetAssets(Filter, OutRedirectors);
	for (const FAssetData& RedirectorData : OutRedirectors)
	{
		if (UObjectRedirector* RedirectorToFix = Cast<UObjectRedirector>(RedirectorData.GetAsset()))
		{
			RedirectorsToFixArray.Add(RedirectorToFix);
		}
	}
	FAssetToolsModule& AssetToolsModule =
		FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	AssetToolsModule.Get().FixupReferencers(RedirectorsToFixArray);
}

#pragma region ContentBrowserMenuExtention

// ... 原有的文件夹扩展代码 ...

// 1. 初始化：注册资产视图的菜单扩展
void FSuperManagerModule::InitCBAssetMenuExtention()
{
	FContentBrowserModule& ContentBrowserModule =
		FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	// 获取 AssetView (资产视图) 的扩展列表
	TArray<FContentBrowserMenuExtender_SelectedAssets>& AssetMenuExtenders =
		ContentBrowserModule.GetAllAssetViewContextMenuExtenders();

	// 添加我们的委托
	AssetMenuExtenders.Add(FContentBrowserMenuExtender_SelectedAssets::
		CreateRaw(this, &FSuperManagerModule::CustomCBAssetMenuExtender));
}

// 2. 扩展器：决定何时显示菜单，以及在什么位置显示
TSharedRef<FExtender> FSuperManagerModule::CustomCBAssetMenuExtender(const TArray<FAssetData>& SelectedAssets)
{
	TSharedRef<FExtender> MenuExtender(new FExtender());

	// 只有当选中的资产包含 SkeletalMesh 时，才显示这个菜单
	bool bAnySkeletalMeshSelected = false;
	for (const FAssetData& Asset : SelectedAssets)
	{
		// 兼容 UE5.1+ 写法，如果是旧版本用 Asset.AssetClass
		if (Asset.AssetClassPath.GetAssetName() == FName("SkeletalMesh"))
		{
			bAnySkeletalMeshSelected = true;
			break;
		}
	}

	if (bAnySkeletalMeshSelected)
	{
		// 将菜单项添加到 "GetAssetActions" (通常是 Common Asset Actions 区域) 后面
		MenuExtender->AddMenuExtension(
			FName("CommonAssetActions"),
			EExtensionHook::After,
			TSharedPtr<FUICommandList>(),
			FMenuExtensionDelegate::CreateRaw(this, &FSuperManagerModule::AddCBAssetMenuEntry)
		);
	}

	return MenuExtender;
}

// 3. 构建菜单项
void FSuperManagerModule::AddCBAssetMenuEntry(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		FText::FromString(TEXT("Check Material Mismatch")), // 菜单名
		FText::FromString(TEXT("Check if material slot names match assigned materials")), // 提示
		FSlateIcon(FSuperManagerStyle::GetStyleSetName(), "ContentBrowser.DeleteUnusedAssets"), // 图标
		FExecuteAction::CreateRaw(this, &FSuperManagerModule::OnCheckSkeletalMeshMaterials) // 执行函数
	);
}

// 4. 执行函数：检查逻辑
// 在 SuperManager.cpp 中

void FSuperManagerModule::OnCheckSkeletalMeshMaterials()
{
	// 获取当前选中的所有资产
	TArray<FAssetData> SelectedAssets = UEditorUtilityLibrary::GetSelectedAssetData();

	FString AllErrorMessages = ""; // 用于收集所有的错误信息
	int32 ErrorCounter = 0;

	for (const FAssetData& AssetData : SelectedAssets)
	{
		// 再次确认类型
		if (AssetData.AssetClassPath.GetAssetName() != FName("SkeletalMesh")) continue;

		// 加载资产
		USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(AssetData.GetAsset());
		if (!SkeletalMesh) continue;

		const TArray<FSkeletalMaterial>& Materials = SkeletalMesh->GetMaterials();
		bool bHasIssue = false;

		// 准备当前资产的错误报告头部
		FString AssetIssueString = FString::Printf(TEXT("Asset: %s\n"), *SkeletalMesh->GetName());

		for (int32 i = 0; i < Materials.Num(); ++i)
		{
			const FSkeletalMaterial& SkeletalMaterial = Materials[i];
			FString SlotName = SkeletalMaterial.MaterialSlotName.ToString();
			UMaterialInterface* AssignedMaterial = SkeletalMaterial.MaterialInterface;

			// 检查 1: 材质为空
			if (!AssignedMaterial)
			{
				AssetIssueString.Append(FString::Printf(TEXT("  - Slot %d ('%s') is EMPTY\n"), i, *SlotName));
				bHasIssue = true;
			}
			else
			{
				FString MaterialName = AssignedMaterial->GetName();

				// 检查 2: 材质名不包含槽名
				// 必须完全相等 (严格模式)
				// ESearchCase::IgnoreCase 表示忽略大小写 (MI_Quinn_01 和 mi_quinn_01 算一样)
				// 如果你需要连大小写都必须一样，去掉 ESearchCase::IgnoreCase 即可
				if (!MaterialName.Equals(SlotName))
				{
					AssetIssueString.Append(FString::Printf(TEXT("  - 槽位 %d ('%s') 不匹配材质 ('%s')\n"),
						i, *SlotName, *MaterialName));
					bHasIssue = true;
				}
			}
		}

		// 如果当前资产有问题，将信息加入总报告中
		if (bHasIssue)
		{
			AllErrorMessages.Append(AssetIssueString);
			AllErrorMessages.Append(TEXT("\n")); // 资产之间空一行
			ErrorCounter++;
		}
	}

	if (ErrorCounter > 0)
	{
		// 直接弹窗显示收集到的所有错误信息
		// 注意：UE5.3+ FMessageDialog::Open 的 Title 参数直接传值 (FText)，不需要取地址 (&)
		const FText DialogTitle = FText::FromString(TEXT("Material Mismatch Report"));
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(AllErrorMessages), DialogTitle);
	}
	else
	{
		DebugHeader::ShowNInfo(TEXT("Selected Skeletal Meshes are good!"));
	}
}

#pragma endregion
#pragma endregion

#pragma region CustomEditorTab

void FSuperManagerModule::RegisterAdvanceDeletionTab()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(FName("AdvanceDeletion"),
		FOnSpawnTab::CreateRaw(this, &FSuperManagerModule::OnSpawnAdvanceDeletionTab))
		.SetDisplayName(FText::FromString(TEXT("Advance Deletion")))
		.SetIcon(FSlateIcon(FSuperManagerStyle::GetStyleSetName(), "ContentBrowser.AdvanceDeletion"));
}

TSharedRef<SDockTab> FSuperManagerModule::OnSpawnAdvanceDeletionTab(const FSpawnTabArgs&SpawnTabArgs)
{
	if (FolderPathsSelected.Num() == 0) return SNew(SDockTab).TabRole(ETabRole::NomadTab);
	ConstructedDockTab =
		SNew(SDockTab).TabRole(ETabRole::NomadTab)
		[
			SNew(SAdvanceDeletionTab)
				.AssetsDataToStore(GetAllAssetDataUnderSelectedFolder())
				.CurrentSelectedFolder(FolderPathsSelected[0])
		];

	ConstructedDockTab->SetOnTabClosed(
		SDockTab::FOnTabClosedCallback::CreateRaw(this, &FSuperManagerModule::OnAdvanceDeletionTabClosed));
	return ConstructedDockTab.ToSharedRef();
}

TArray<TSharedPtr<FAssetData>> FSuperManagerModule::GetAllAssetDataUnderSelectedFolder()
{
	TArray< TSharedPtr <FAssetData> > AvaiableAssetsData;
	TArray<FString> AssetsPathNames = UEditorAssetLibrary::ListAssets(FolderPathsSelected[0]);
	for (const FString& AssetPathName : AssetsPathNames)
	{
		//Don't touch root folder
		if (AssetPathName.Contains(TEXT("Developers")) ||
			AssetPathName.Contains(TEXT("Collections")) ||
			AssetPathName.Contains(TEXT("__ExternalActors__")) ||
			AssetPathName.Contains(TEXT("__ExternalObjects__")))
		{
			continue;
		}
		if (!UEditorAssetLibrary::DoesAssetExist(AssetPathName)) continue;
		const FAssetData Data = UEditorAssetLibrary::FindAssetData(AssetPathName);
		AvaiableAssetsData.Add(MakeShared<FAssetData>(Data));
	}
	return AvaiableAssetsData;
}

void FSuperManagerModule::OnAdvanceDeletionTabClosed(TSharedRef<SDockTab> TabToClose)
{
	if (ConstructedDockTab.IsValid())
	{
		ConstructedDockTab.Reset();
		FolderPathsSelected.Empty();
	}
}
#pragma endregion

#pragma region ProccessDataForAdvanceDeletionTab
bool FSuperManagerModule::DeleteSingleAssetForAssetList(const FAssetData& AssetDataToDelete)
{
	TArray<FAssetData> AssetDataForDeletion;
	AssetDataForDeletion.Add(AssetDataToDelete);
	if (ObjectTools::DeleteAssets(AssetDataForDeletion) > 0)
	{
		return true;
	}
	return false;
}
#pragma endregion



bool FSuperManagerModule::DeleteMultipleAssetsForAssetList(const TArray<FAssetData>& AssetsToDelete)
{
	if (ObjectTools::DeleteAssets(AssetsToDelete) > 0)
	{
		return true;
	}
	return false;
}

void FSuperManagerModule::ListUnusedAssetsForAssetList(const TArray<TSharedPtr<FAssetData>>& AssetsDataToFilter,
	TArray<TSharedPtr<FAssetData>>& OutUnusedAssetsData)
{
	OutUnusedAssetsData.Empty();
	for (const TSharedPtr<FAssetData>& DataSharedPtr : AssetsDataToFilter)
	{
		TArray<FString> AssetReferencers =
			UEditorAssetLibrary::FindPackageReferencersForAsset(DataSharedPtr->ObjectPath.ToString());
		if (AssetReferencers.Num() == 0)
		{
			OutUnusedAssetsData.Add(DataSharedPtr);
		}
	}
}

void FSuperManagerModule::ListSameNameAssetsForAssetList(const TArray<TSharedPtr<FAssetData>>& AssetsDataToFilter,
	TArray<TSharedPtr<FAssetData>>& OutSameNameAssetsData)
{
	OutSameNameAssetsData.Empty();
	//Multimap for supporting finding assets with same name
	TMultiMap<FString, TSharedPtr<FAssetData> > AssetsInfoMultiMap;
	for (const TSharedPtr<FAssetData>& DataSharedPtr : AssetsDataToFilter)
	{
		AssetsInfoMultiMap.Emplace(DataSharedPtr->AssetName.ToString(), DataSharedPtr);
	}
	for (const TSharedPtr<FAssetData>& DataSharedPtr : AssetsDataToFilter)
	{
		TArray< TSharedPtr <FAssetData> > OutAssetsData;
		AssetsInfoMultiMap.MultiFind(DataSharedPtr->AssetName.ToString(), OutAssetsData);
		if (OutAssetsData.Num() <= 1) continue;

		for (const TSharedPtr<FAssetData>& SameNameData : OutAssetsData)
		{
			if (SameNameData.IsValid())
			{
				OutSameNameAssetsData.AddUnique(SameNameData);
			}
		}
	}
}

void FSuperManagerModule::SyncCBToClickedAssetForAssetList(const FString& AssetPathToSync)
{
	TArray<FString> AssetsPathToSync;
	AssetsPathToSync.Add(AssetPathToSync);
	UEditorAssetLibrary::SyncBrowserToObjects(AssetsPathToSync);
}

#pragma region LevelEditorMenuExtension

void FSuperManagerModule::InitLevelEditorExtention()
{
	FLevelEditorModule& LevelEditorModule =
		FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));

	TSharedRef<FUICommandList> ExistingLevelCommands = LevelEditorModule.GetGlobalLevelEditorActions();
	ExistingLevelCommands->Append(CustomUICommands.ToSharedRef());

	TArray<FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors>& LevelEditorMenuExtenders =
		LevelEditorModule.GetAllLevelViewportContextMenuExtenders();
	LevelEditorMenuExtenders.Add(FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors::
		CreateRaw(this, &FSuperManagerModule::CustomLevelEditorMenuExtender));
}
TSharedRef<FExtender> FSuperManagerModule::CustomLevelEditorMenuExtender(const TSharedRef<FUICommandList> UICommandList,
	const TArray<AActor*> SelectedActors)
{
	TSharedRef<FExtender> MenuExtender = MakeShareable(new FExtender());

	if (SelectedActors.Num() > 0)
	{
		MenuExtender->AddMenuExtension(
			FName("ActorOptions"),
			EExtensionHook::Before,
			UICommandList,
			FMenuExtensionDelegate::CreateRaw(this, &FSuperManagerModule::AddLevelEditorMenuEntry)
		);
	}
	return MenuExtender;
}
void FSuperManagerModule::AddLevelEditorMenuEntry(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry
	(
		FText::FromString(TEXT("Lock Actor Selection")),
		FText::FromString(TEXT("Prevent actor from being selected")),
		FSlateIcon(FSuperManagerStyle::GetStyleSetName(), "LevelEditor.LockSelection"),
		FExecuteAction::CreateRaw(this, &FSuperManagerModule::OnLockActorSelectionButtonClicked)
	);
	MenuBuilder.AddMenuEntry
	(
		FText::FromString(TEXT("Unlock all actor Selection")),
		FText::FromString(TEXT("Remove the selection constraint on all actor")),
		FSlateIcon(FSuperManagerStyle::GetStyleSetName(), "LevelEditor.UnlockSelection"),
		FExecuteAction::CreateRaw(this, &FSuperManagerModule::OnUnlockActorSelectionButtonClicked)
	);
}
void FSuperManagerModule::OnLockActorSelectionButtonClicked()
{
	if (!GetEditorActorSubsystem()) return;
	TArray<AActor*> SelectedActors = WeakEditorActorSubsystem->GetSelectedLevelActors();
	if (SelectedActors.Num() == 0)
	{
		DebugHeader::ShowNInfo(TEXT("No actor selected"));
		return;
	}
	FString CurrentLockedActorNames = TEXT("Locked selection for:");
	for (AActor* SelectedActor : SelectedActors)
	{
		if (!SelectedActor) continue;
		LockActorSelection(SelectedActor);
		WeakEditorActorSubsystem->SetActorSelectionState(SelectedActor, false);
		CurrentLockedActorNames.Append(TEXT("\n"));
		CurrentLockedActorNames.Append(SelectedActor->GetActorLabel());
	}
	RefreshSceneOutliner();

	DebugHeader::ShowNInfo(CurrentLockedActorNames);
}
void FSuperManagerModule::OnUnlockActorSelectionButtonClicked()
{
	if (!GetEditorActorSubsystem()) return;
	TArray<AActor*> AllActorsInLevel = WeakEditorActorSubsystem->GetAllLevelActors();
	TArray<AActor*> AllLockedActors;
	for (AActor* ActorInLevel : AllActorsInLevel)
	{
		if (!ActorInLevel) continue;
		if (CheckIsActorSelectionLocked(ActorInLevel))
		{
			AllLockedActors.Add(ActorInLevel);
		}
	}
	if (AllLockedActors.Num() == 0)
	{
		DebugHeader::ShowNInfo(TEXT("No selection locked actor currently"));
		return;
	}
	FString UnlockedActorNames = TEXT("Lifted selection constraint for:");
	for (AActor* LockedActor : AllLockedActors)
	{
		UnlockActorSelection(LockedActor);
		UnlockedActorNames.Append(TEXT("\n"));
		UnlockedActorNames.Append(LockedActor->GetActorLabel());
	}
	RefreshSceneOutliner();
	DebugHeader::ShowNInfo(UnlockedActorNames);
}
#pragma endregion

#pragma region SelectionLock
void FSuperManagerModule::InitCustomSelectionEvent()
{
	USelection* UserSelection = GEditor->GetSelectedActors();
	UserSelection->SelectObjectEvent.AddRaw(this, &FSuperManagerModule::OnActorSelected);
}
void FSuperManagerModule::OnActorSelected(UObject* SelectedObject)
{
	if (!GetEditorActorSubsystem()) return;
	if (AActor* SelectedActor = Cast<AActor>(SelectedObject))
	{
		if (CheckIsActorSelectionLocked(SelectedActor))
		{
			//Deselect actor right away
			WeakEditorActorSubsystem->SetActorSelectionState(SelectedActor, false);
		}
	}
}

void FSuperManagerModule::LockActorSelection(AActor* ActorToProcess)
{
	if (!ActorToProcess) return;

	if (!ActorToProcess->ActorHasTag(FName("Locked")))
	{
		ActorToProcess->Tags.Add(FName("Locked"));
	}
}
void FSuperManagerModule::UnlockActorSelection(AActor* ActorToProcess)
{
	if (!ActorToProcess) return;

	if (ActorToProcess->ActorHasTag(FName("Locked")))
	{
		ActorToProcess->Tags.Remove(FName("Locked"));
	}
}
bool FSuperManagerModule::CheckIsActorSelectionLocked(AActor* ActorToProcess)
{
	if (!ActorToProcess) return false;
	return ActorToProcess->ActorHasTag(FName("Locked"));
}

void FSuperManagerModule::RefreshSceneOutliner()
{
	FLevelEditorModule& LevelEditorModule =
		FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	TSharedPtr<ISceneOutliner> SceneOutliner = LevelEditorModule.GetFirstLevelEditor()->GetMostRecentlyUsedSceneOutliner();
	if (SceneOutliner.IsValid())
	{
		SceneOutliner->FullRefresh();
	}
}

#pragma endregion

#pragma region SceneOutlinerExtension
void FSuperManagerModule::InitSceneOutlinerColumnExtension()
{
	FSceneOutlinerModule& SceneOutlinerModule =
		FModuleManager::LoadModuleChecked<FSceneOutlinerModule>(TEXT("SceneOutliner"));
	FSceneOutlinerColumnInfo SelectionLockColumnInfo(
		ESceneOutlinerColumnVisibility::Visible,
		1,
		FCreateSceneOutlinerColumn::CreateRaw(this, &FSuperManagerModule::OnCreateSelectionLockColumn)
	);
	SceneOutlinerModule.RegisterDefaultColumnType<FOutlinerSelectionLockColumn>(SelectionLockColumnInfo);
}
TSharedRef<ISceneOutlinerColumn> FSuperManagerModule::OnCreateSelectionLockColumn(ISceneOutliner& SceneOutliner)
{
	return MakeShareable(new FOutlinerSelectionLockColumn(SceneOutliner));
}
#pragma endregion

void FSuperManagerModule::ProcessLockingForOutliner(AActor* ActorToProcess, bool bShouldLock)
{
	if (!GetEditorActorSubsystem()) return;
	if (bShouldLock)
	{
		LockActorSelection(ActorToProcess);
		WeakEditorActorSubsystem->SetActorSelectionState(ActorToProcess, false);
		DebugHeader::ShowNInfo(TEXT("Locked selection for:\n") + ActorToProcess->GetActorLabel());
	}
	else
	{
		UnlockActorSelection(ActorToProcess);
		DebugHeader::ShowNInfo(TEXT("Removed selection lock for:\n") + ActorToProcess->GetActorLabel());
	}
}

#pragma region CustomEditorUICommands
void FSuperManagerModule::InitCustomUICommands()
{
	CustomUICommands = MakeShareable(new FUICommandList());
	CustomUICommands->MapAction(
		FSuperManagerUICommands::Get().LockActorSelection,
		FExecuteAction::CreateRaw(this, &FSuperManagerModule::OnSelectionLockHotKeyPressed)
	);
	CustomUICommands->MapAction(
		FSuperManagerUICommands::Get().UnlockActorSelection,
		FExecuteAction::CreateRaw(this, &FSuperManagerModule::OnUnlockActorSelectionHotKeyPressed)
	);
}
void FSuperManagerModule::OnSelectionLockHotKeyPressed()
{
	OnLockActorSelectionButtonClicked();
}
void FSuperManagerModule::OnUnlockActorSelectionHotKeyPressed()
{
	OnUnlockActorSelectionButtonClicked();
}
#pragma endregion

bool FSuperManagerModule::GetEditorActorSubsystem()
{
	if (!WeakEditorActorSubsystem.IsValid())
	{
		WeakEditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
	}
	return WeakEditorActorSubsystem.IsValid();
}

void FSuperManagerModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(FName("AdvanceDeletion"));
	FSuperManagerStyle::ShutDown();
	FSuperManagerUICommands::Unregister();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FSuperManagerModule, SuperManager)