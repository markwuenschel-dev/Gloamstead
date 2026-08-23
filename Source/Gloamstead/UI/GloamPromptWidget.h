#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GloamPromptWidget.generated.h"

class UTextBlock;

/**
 * Contextual prompt HUD: shows what the player can do right now, or nothing.
 *
 * The text itself is decided by AGloamsteadCharacter::GetPlayerPromptText(); this only polls and
 * displays it. Kept in C++ rather than a Blueprint binding graph so the widget asset needs no graph
 * at all — a WidgetBlueprint parented to this class with a TextBlock named "PromptText" is enough,
 * and BindWidget makes a rename a compile error instead of a silently blank HUD.
 */
UCLASS(Abstract)
class GLOAMSTEAD_API UGloamPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	/** Required in the Blueprint child. Named binding, so a mismatch fails to compile. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PromptText;

private:
	/** Avoids rebuilding the Slate text every frame when the prompt has not changed. */
	FText LastPrompt;
};
