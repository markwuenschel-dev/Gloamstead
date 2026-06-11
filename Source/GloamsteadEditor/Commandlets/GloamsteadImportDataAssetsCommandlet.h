#pragma once

#include "Commandlets/Commandlet.h"
#include "GloamsteadImportDataAssetsCommandlet.generated.h"

UCLASS()
class GLOAMSTEADEDITOR_API UGloamsteadImportDataAssetsCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UGloamsteadImportDataAssetsCommandlet();

	virtual int32 Main(const FString& Params) override;
};
