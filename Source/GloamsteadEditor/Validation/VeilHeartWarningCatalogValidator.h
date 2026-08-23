#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"
#include "VeilHeartWarningCatalogValidator.generated.h"

/**
 * Editor-only asset validator for the authored Heart warning catalog.
 *
 * UEditorValidatorBase instances are discovered by the editor validation
 * subsystem; this class intentionally stays in GloamsteadEditor so neither
 * runtime gameplay nor shipped builds depend on DataValidation.
 */
UCLASS(Transient)
class GLOAMSTEADEDITOR_API UVeilHeartWarningCatalogValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

public:
	virtual bool CanValidateAsset_Implementation(
		const FAssetData& InAssetData,
		UObject* InAsset,
		FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(
		const FAssetData& InAssetData,
		UObject* InAsset,
		FDataValidationContext& InContext) override;
};
