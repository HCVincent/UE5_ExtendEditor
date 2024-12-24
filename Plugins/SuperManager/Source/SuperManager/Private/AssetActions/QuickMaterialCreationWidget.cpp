// Fill out your copyright notice in the Description page of Project Settings.
#include "AssetActions/QuickMaterialCreationWidget.h"
#include "DebugHeader.h"
#include "EditorUtilityLibrary.h"
#include "EditorAssetLibrary.h"
#include "AssetToolsModule.h"
#include "Factories/MaterialFactoryNew.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"

#pragma region QuickMaterialCreationCore

void UQuickMaterialCreationWidget::CreateMaterialFromSelectedTextures()
{
	if (bCustomMaterialName)
	{
		if (MaterialName.IsEmpty() || MaterialName.Equals(TEXT("M_")))
		{
			DebugHeader::ShowMsgDialog(EAppMsgType::Ok, TEXT("Please enter a valid name"));
			return;
		}
	}
	TArray<FAssetData> SelectedAssetsData = UEditorUtilityLibrary::GetSelectedAssetData();
	TArray<UTexture2D*> SelectedTexturesArray;
	FString SelectedTextureFolderPath;
	uint32 PinsConnectedCounter = 0;

	if (!ProcessSelectedData(SelectedAssetsData, SelectedTexturesArray, SelectedTextureFolderPath)) { MaterialName = TEXT("M_"); return; }

	DebugHeader::Print(SelectedTextureFolderPath, FColor::Cyan);
	if (CheckIsNameUsed(SelectedTextureFolderPath, MaterialName)) { MaterialName = TEXT("M_"); return; }
	UMaterial* CreatedMaterial = CreateMaterialAsset(MaterialName, SelectedTextureFolderPath);
	if (!CreatedMaterial)
	{
		DebugHeader::ShowMsgDialog(EAppMsgType::Ok, TEXT("Failed to create material"));
		return;
	}

	for (UTexture2D* SelectedTexture : SelectedTexturesArray)
	{
		if (!SelectedTexture) continue;
		Default_CreateMaterialNodes(CreatedMaterial, SelectedTexture, PinsConnectedCounter);
		if (PinsConnectedCounter > 0)
		{
			DebugHeader::ShowNInfo(TEXT("Successfully connected ")
				+ FString::FromInt(PinsConnectedCounter) + (TEXT(" pins")));
		}

		if (bCreateMaterialInstance)
		{
			CreateMaterialInstanceAsset(CreatedMaterial, MaterialName, SelectedTextureFolderPath);
		}
	}

	MaterialName = TEXT("M_");
}
//Process the selected data, will filter out textures,and return false if non-texture selected
bool UQuickMaterialCreationWidget::ProcessSelectedData(const TArray<FAssetData>& SelectedDataToProccess,
	TArray<UTexture2D*>& OutSelectedTexturesArray, FString& OutSelectedTexturePackagePath)
{
	if (SelectedDataToProccess.Num() == 0)
	{
		DebugHeader::ShowMsgDialog(EAppMsgType::Ok, TEXT("No texture selected"));
		return false;
	}
	bool bMaterialNameSet = false;
	for (const FAssetData& SelectedData : SelectedDataToProccess)
	{
		UObject* SelectedAsset = SelectedData.GetAsset();
		if (!SelectedAsset) continue;
		UTexture2D* SelectedTexture = Cast<UTexture2D>(SelectedAsset);
		if (!SelectedTexture)
		{
			DebugHeader::ShowMsgDialog(EAppMsgType::Ok, TEXT("Please select only textures\n") +
				SelectedAsset->GetName() + TEXT(" is not a texture"));
			return false;
		}
		OutSelectedTexturesArray.Add(SelectedTexture);
		if (OutSelectedTexturePackagePath.IsEmpty())
		{
			OutSelectedTexturePackagePath = SelectedData.PackagePath.ToString();
		}
		if (!bCustomMaterialName && !bMaterialNameSet)
		{
			MaterialName = SelectedAsset->GetName();
			MaterialName.RemoveFromStart(TEXT("T_"));
			MaterialName.InsertAt(0, TEXT("M_"));
			bMaterialNameSet = true;
		}
	}
	return true;
}

//Will return true if the material name is used by asset under the specified folder
bool UQuickMaterialCreationWidget::CheckIsNameUsed(const FString& FolderPathToCheck,
	const FString& MaterialNameToCheck)
{
	TArray<FString> ExistingAssetsPaths = UEditorAssetLibrary::ListAssets(FolderPathToCheck, false);
	for (const FString& ExistingAssetPath : ExistingAssetsPaths)
	{
		const FString ExistingAssetName = FPaths::GetBaseFilename(ExistingAssetPath);
		if (ExistingAssetName.Equals(MaterialNameToCheck))
		{
			DebugHeader::ShowMsgDialog(EAppMsgType::Ok, MaterialNameToCheck +
				TEXT(" is already used by asset"));
			return true;
		}
	}
	return false;
}


UMaterial* UQuickMaterialCreationWidget::CreateMaterialAsset(const FString& NameOfTheMaterial, const FString& PathToPutMaterial)
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	UMaterialFactoryNew* MaterialFactory = NewObject<UMaterialFactoryNew>();
	UObject* CreatedObject = AssetToolsModule.Get().CreateAsset(NameOfTheMaterial, PathToPutMaterial,
		UMaterial::StaticClass(), MaterialFactory);
	return Cast<UMaterial>(CreatedObject);
}

void UQuickMaterialCreationWidget::Default_CreateMaterialNodes(UMaterial* CreatedMaterial,
	UTexture2D* SelectedTexture, uint32& PinsConnectedCounter)
{
	if (bUseParameterizedTextures)
	{
		UMaterialExpressionTextureSampleParameter2D* TextureParameterNode =
			NewObject<UMaterialExpressionTextureSampleParameter2D>(CreatedMaterial);
		if (!TextureParameterNode) return;

		if (!CreatedMaterial->HasBaseColorConnected())
		{
			if (TryConnectBaseColor(TextureParameterNode, SelectedTexture, CreatedMaterial))
			{
				PinsConnectedCounter++;
				return;
			}
		}

		if (!CreatedMaterial->HasMetallicConnected())
		{
			if (TryConnectMetalic(TextureParameterNode, SelectedTexture, CreatedMaterial))
			{
				PinsConnectedCounter++;
				return;
			}
		}

		if (!CreatedMaterial->HasRoughnessConnected())
		{
			if (TryConnectRoughness(TextureParameterNode, SelectedTexture, CreatedMaterial))
			{
				PinsConnectedCounter++;
				return;
			}
		}
		if (!CreatedMaterial->HasNormalConnected())
		{
			if (TryConnectNormal(TextureParameterNode, SelectedTexture, CreatedMaterial))
			{
				PinsConnectedCounter++;
				return;
			}
		}
		if (!CreatedMaterial->HasAmbientOcclusionConnected())
		{
			if (TryConnectAO(TextureParameterNode, SelectedTexture, CreatedMaterial))
			{
				PinsConnectedCounter++;
				return;
			}
		}
	}
	else
	{
		UMaterialExpressionTextureSample* TextureSampleNode =
			NewObject<UMaterialExpressionTextureSample>(CreatedMaterial);
		if (!TextureSampleNode) return;

		if (!CreatedMaterial->HasBaseColorConnected())
		{
			if (TryConnectBaseColor(TextureSampleNode, SelectedTexture, CreatedMaterial))
			{
				PinsConnectedCounter++;
				return;
			}
		}

		if (!CreatedMaterial->HasMetallicConnected())
		{
			if (TryConnectMetalic(TextureSampleNode, SelectedTexture, CreatedMaterial))
			{
				PinsConnectedCounter++;
				return;
			}
		}

		if (!CreatedMaterial->HasRoughnessConnected())
		{
			if (TryConnectRoughness(TextureSampleNode, SelectedTexture, CreatedMaterial))
			{
				PinsConnectedCounter++;
				return;
			}
		}
		if (!CreatedMaterial->HasNormalConnected())
		{
			if (TryConnectNormal(TextureSampleNode, SelectedTexture, CreatedMaterial))
			{
				PinsConnectedCounter++;
				return;
			}
		}
		if (!CreatedMaterial->HasAmbientOcclusionConnected())
		{
			if (TryConnectAO(TextureSampleNode, SelectedTexture, CreatedMaterial))
			{
				PinsConnectedCounter++;
				return;
			}
		}
	}
	DebugHeader::Print(TEXT("Failed to connect the texture: ") + SelectedTexture->GetName(), FColor::Red);
}
#pragma endregion

#pragma region CreateMaterialNodesConnectPins
bool UQuickMaterialCreationWidget::TryConnectBaseColor(UMaterialExpressionTextureSample* TextureSampleNode,
	UTexture2D* SelectedTexture, UMaterial* CreatedMaterial)
{
	for (const FString& BaseColorName : BaseColorArray)
	{
		if (SelectedTexture->GetName().Contains(BaseColorName))
		{
			if (UMaterialExpressionTextureSampleParameter2D* ParameterNode = Cast<UMaterialExpressionTextureSampleParameter2D>(TextureSampleNode))
			{
				ParameterNode->ParameterName = FName(TEXT("BaseColor"));
				ParameterNode->Texture = SelectedTexture;
				CreatedMaterial->GetExpressionCollection().AddExpression(ParameterNode);
				CreatedMaterial->GetExpressionInputForProperty(MP_BaseColor)->Connect(0, ParameterNode);
			}
			else
			{
				TextureSampleNode->Texture = SelectedTexture;
				CreatedMaterial->GetExpressionCollection().AddExpression(TextureSampleNode);
				CreatedMaterial->GetExpressionInputForProperty(MP_BaseColor)->Connect(0, TextureSampleNode);
			}
			CreatedMaterial->PostEditChange();
			TextureSampleNode->MaterialExpressionEditorX -= 600;
			return true;
		}
	}
	return false;
}

bool UQuickMaterialCreationWidget::TryConnectMetalic(UMaterialExpressionTextureSample* TextureSampleNode,
	UTexture2D* SelectedTexture, UMaterial* CreatedMaterial)
{
	for (const FString& MetalicName : MetallicArray)
	{
		if (SelectedTexture->GetName().Contains(MetalicName))
		{
			SelectedTexture->CompressionSettings = TextureCompressionSettings::TC_Default;
			SelectedTexture->SRGB = false;
			SelectedTexture->PostEditChange();

			if (UMaterialExpressionTextureSampleParameter2D* ParameterNode = Cast<UMaterialExpressionTextureSampleParameter2D>(TextureSampleNode))
			{
				ParameterNode->ParameterName = FName(TEXT("Metallic"));
				ParameterNode->Texture = SelectedTexture;
				ParameterNode->SamplerType = EMaterialSamplerType::SAMPLERTYPE_LinearColor;
				CreatedMaterial->GetExpressionCollection().AddExpression(ParameterNode);
				CreatedMaterial->GetExpressionInputForProperty(MP_Metallic)->Connect(0, ParameterNode);
			}
			else
			{
				TextureSampleNode->Texture = SelectedTexture;
				TextureSampleNode->SamplerType = EMaterialSamplerType::SAMPLERTYPE_LinearColor;
				CreatedMaterial->GetExpressionCollection().AddExpression(TextureSampleNode);
				CreatedMaterial->GetExpressionInputForProperty(MP_Metallic)->Connect(0, TextureSampleNode);
			}
			CreatedMaterial->PostEditChange();
			TextureSampleNode->MaterialExpressionEditorX -= 600;
			TextureSampleNode->MaterialExpressionEditorY += 240;
			return true;
		}
	}
	return false;
}

bool UQuickMaterialCreationWidget::TryConnectRoughness(UMaterialExpressionTextureSample* TextureSampleNode,
	UTexture2D* SelectedTexture, UMaterial* CreatedMaterial)
{
	for (const FString& RoughnessName : RoughnessArray)
	{
		if (SelectedTexture->GetName().Contains(RoughnessName))
		{
			SelectedTexture->CompressionSettings = TextureCompressionSettings::TC_Default;
			SelectedTexture->SRGB = false;
			SelectedTexture->PostEditChange();

			if (UMaterialExpressionTextureSampleParameter2D* ParameterNode = Cast<UMaterialExpressionTextureSampleParameter2D>(TextureSampleNode))
			{
				ParameterNode->ParameterName = FName(TEXT("Roughness"));
				ParameterNode->Texture = SelectedTexture;
				ParameterNode->SamplerType = EMaterialSamplerType::SAMPLERTYPE_LinearColor;
				CreatedMaterial->GetExpressionCollection().AddExpression(ParameterNode);
				CreatedMaterial->GetExpressionInputForProperty(MP_Roughness)->Connect(0, ParameterNode);
			}
			else
			{
				TextureSampleNode->Texture = SelectedTexture;
				TextureSampleNode->SamplerType = EMaterialSamplerType::SAMPLERTYPE_LinearColor;
				CreatedMaterial->GetExpressionCollection().AddExpression(TextureSampleNode);
				CreatedMaterial->GetExpressionInputForProperty(MP_Roughness)->Connect(0, TextureSampleNode);
			}
			CreatedMaterial->PostEditChange();
			TextureSampleNode->MaterialExpressionEditorX -= 600;
			TextureSampleNode->MaterialExpressionEditorY += 480;
			return true;
		}
	}
	return false;
}

bool UQuickMaterialCreationWidget::TryConnectNormal(UMaterialExpressionTextureSample* TextureSampleNode,
	UTexture2D* SelectedTexture, UMaterial* CreatedMaterial)
{
	for (const FString& NormalName : NormalArray)
	{
		if (SelectedTexture->GetName().Contains(NormalName))
		{
			if (UMaterialExpressionTextureSampleParameter2D* ParameterNode = Cast<UMaterialExpressionTextureSampleParameter2D>(TextureSampleNode))
			{
				ParameterNode->ParameterName = FName(TEXT("Normal"));
				ParameterNode->Texture = SelectedTexture;
				ParameterNode->SamplerType = EMaterialSamplerType::SAMPLERTYPE_Normal;
				CreatedMaterial->GetExpressionCollection().AddExpression(ParameterNode);
				CreatedMaterial->GetExpressionInputForProperty(MP_Normal)->Connect(0, ParameterNode);
			}
			else
			{
				TextureSampleNode->Texture = SelectedTexture;
				TextureSampleNode->SamplerType = EMaterialSamplerType::SAMPLERTYPE_Normal;
				CreatedMaterial->GetExpressionCollection().AddExpression(TextureSampleNode);
				CreatedMaterial->GetExpressionInputForProperty(MP_Normal)->Connect(0, TextureSampleNode);
			}
			CreatedMaterial->PostEditChange();
			TextureSampleNode->MaterialExpressionEditorX -= 600;
			TextureSampleNode->MaterialExpressionEditorY += 720;
			return true;
		}
	}
	return false;
}

bool UQuickMaterialCreationWidget::TryConnectAO(UMaterialExpressionTextureSample* TextureSampleNode,
	UTexture2D* SelectedTexture, UMaterial* CreatedMaterial)
{
	for (const FString& AOName : AmbientOcclusionArray)
	{
		if (SelectedTexture->GetName().Contains(AOName))
		{
			SelectedTexture->CompressionSettings = TextureCompressionSettings::TC_Default;
			SelectedTexture->SRGB = false;
			SelectedTexture->PostEditChange();

			if (UMaterialExpressionTextureSampleParameter2D* ParameterNode = Cast<UMaterialExpressionTextureSampleParameter2D>(TextureSampleNode))
			{
				ParameterNode->ParameterName = FName(TEXT("AmbientOcclusion"));
				ParameterNode->Texture = SelectedTexture;
				ParameterNode->SamplerType = EMaterialSamplerType::SAMPLERTYPE_LinearColor;
				CreatedMaterial->GetExpressionCollection().AddExpression(ParameterNode);
				CreatedMaterial->GetExpressionInputForProperty(MP_AmbientOcclusion)->Connect(0, ParameterNode);
			}
			else
			{
				TextureSampleNode->Texture = SelectedTexture;
				TextureSampleNode->SamplerType = EMaterialSamplerType::SAMPLERTYPE_LinearColor;
				CreatedMaterial->GetExpressionCollection().AddExpression(TextureSampleNode);
				CreatedMaterial->GetExpressionInputForProperty(MP_AmbientOcclusion)->Connect(0, TextureSampleNode);
			}
			CreatedMaterial->PostEditChange();
			TextureSampleNode->MaterialExpressionEditorX -= 600;
			TextureSampleNode->MaterialExpressionEditorY += 960;
			return true;
		}
	}
	return false;
}

UMaterialInstanceConstant* UQuickMaterialCreationWidget::CreateMaterialInstanceAsset(UMaterial* CreatedMaterial,
	FString NameOfMaterialInstance, const FString& PathToPutMI)
{
	NameOfMaterialInstance.RemoveFromStart(TEXT("M_"));
	NameOfMaterialInstance.InsertAt(0, TEXT("MI_"));
	UMaterialInstanceConstantFactoryNew* MIFactoryNew = NewObject<UMaterialInstanceConstantFactoryNew>();
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	UObject* CreatedObject = AssetToolsModule.Get().CreateAsset(NameOfMaterialInstance, PathToPutMI,
		UMaterialInstanceConstant::StaticClass(), MIFactoryNew);
	if (UMaterialInstanceConstant* CreatedMI = Cast<UMaterialInstanceConstant>(CreatedObject))
	{
		CreatedMI->SetParentEditorOnly(CreatedMaterial);
		CreatedMI->PostEditChange();
		CreatedMaterial->PostEditChange();
		return CreatedMI;
	}
	return nullptr;
}

UMaterialExpressionTextureSampleParameter2D* UQuickMaterialCreationWidget::CreateTextureParameter(UMaterial* Material, FName ParameterName, UTexture2D* Texture)
{
	if (!Material || !Texture) return nullptr;

	UMaterialExpressionTextureSampleParameter2D* ParameterNode = NewObject<UMaterialExpressionTextureSampleParameter2D>(Material);
	if (!ParameterNode) return nullptr;

	ParameterNode->ParameterName = ParameterName;
	ParameterNode->Texture = Texture;
	Material->GetExpressionCollection().AddExpression(ParameterNode);

	return ParameterNode;
}

void UQuickMaterialCreationWidget::CreateMaterialInstanceFromParent()
{
	if (!ParentMaterial)
	{
		DebugHeader::ShowMsgDialog(EAppMsgType::Ok, TEXT("Please select a parent material"));
		return;
	}

	if (bCustomMaterialName)
	{
		if (MaterialName.IsEmpty() || MaterialName.Equals(TEXT("M_")))
		{
			DebugHeader::ShowMsgDialog(EAppMsgType::Ok, TEXT("Please enter a valid name"));
			return;
		}
	}

	TArray<FAssetData> SelectedAssetsData = UEditorUtilityLibrary::GetSelectedAssetData();
	TArray<UTexture2D*> SelectedTexturesArray;
	FString SelectedTextureFolderPath;

	if (!ProcessSelectedData(SelectedAssetsData, SelectedTexturesArray, SelectedTextureFolderPath))
	{
		MaterialName = TEXT("M_");
		return;
	}

	if (CheckIsNameUsed(SelectedTextureFolderPath, MaterialName))
	{
		MaterialName = TEXT("M_");
		return;
	}

	if (CreateMaterialInstanceFromSelectedTextures(ParentMaterial, SelectedTexturesArray, SelectedTextureFolderPath))
	{
		DebugHeader::ShowMsgDialog(EAppMsgType::Ok, TEXT("Successfully created material instance"));
	}
	else
	{
		DebugHeader::ShowMsgDialog(EAppMsgType::Ok, TEXT("Failed to create material instance"));
	}

	MaterialName = TEXT("M_");
}

bool UQuickMaterialCreationWidget::CreateMaterialInstanceFromSelectedTextures(UMaterial* ParentMat,
	const TArray<UTexture2D*>& SelectedTextures, const FString& TargetPath)
{
	if (!ParentMat || SelectedTextures.Num() == 0) return false;

	FString NewMIName = MaterialName;
	NewMIName.RemoveFromStart(TEXT("M_"));
	NewMIName.InsertAt(0, TEXT("MI_"));

	UMaterialInstanceConstantFactoryNew* MIFactoryNew = NewObject<UMaterialInstanceConstantFactoryNew>();
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	
	UObject* CreatedObject = AssetToolsModule.Get().CreateAsset(NewMIName, TargetPath,
		UMaterialInstanceConstant::StaticClass(), MIFactoryNew);

	if (UMaterialInstanceConstant* CreatedMI = Cast<UMaterialInstanceConstant>(CreatedObject))
	{
		CreatedMI->SetParentEditorOnly(ParentMat);

		// Try to set texture parameters based on texture names
		for (UTexture2D* SelectedTexture : SelectedTextures)
		{
			if (!SelectedTexture) continue;

			for (const FString& BaseColorName : BaseColorArray)
			{
				if (SelectedTexture->GetName().Contains(BaseColorName))
				{
					TrySetTextureParameter(CreatedMI, TEXT("BaseColor"), SelectedTexture);
					break;
				}
			}

			for (const FString& MetallicName : MetallicArray)
			{
				if (SelectedTexture->GetName().Contains(MetallicName))
				{
					TrySetTextureParameter(CreatedMI, TEXT("Metallic"), SelectedTexture);
					break;
				}
			}

			for (const FString& RoughnessName : RoughnessArray)
			{
				if (SelectedTexture->GetName().Contains(RoughnessName))
				{
					TrySetTextureParameter(CreatedMI, TEXT("Roughness"), SelectedTexture);
					break;
				}
			}

			for (const FString& NormalName : NormalArray)
			{
				if (SelectedTexture->GetName().Contains(NormalName))
				{
					TrySetTextureParameter(CreatedMI, TEXT("Normal"), SelectedTexture);
					break;
				}
			}

			for (const FString& AOName : AmbientOcclusionArray)
			{
				if (SelectedTexture->GetName().Contains(AOName))
				{
					TrySetTextureParameter(CreatedMI, TEXT("AmbientOcclusion"), SelectedTexture);
					break;
				}
			}
		}

		CreatedMI->PostEditChange();
		ParentMat->PostEditChange();
		return true;
	}

	return false;
}

void UQuickMaterialCreationWidget::TrySetTextureParameter(UMaterialInstanceConstant* MaterialInstance,
	const FName ParameterName, UTexture2D* Texture)
{
	if (!MaterialInstance || !Texture) return;

	FMaterialParameterInfo ParamInfo(ParameterName);
	UTexture* ExistingTexture = nullptr;
	MaterialInstance->GetTextureParameterValue(ParamInfo, ExistingTexture);

	if (ExistingTexture != Texture)
	{
		MaterialInstance->SetTextureParameterValueEditorOnly(ParamInfo, Texture);
	}
}