#include "UI/GloamPromptWidget.h"
#include "GloamsteadCharacter.h"
#include "Components/TextBlock.h"

void UGloamPromptWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!PromptText)
	{
		return;
	}

	FText Prompt = FText::GetEmpty();
	if (const AGloamsteadCharacter* Player = Cast<AGloamsteadCharacter>(GetOwningPlayerPawn()))
	{
		Prompt = Player->GetPlayerPromptText();
	}

	// EqualTo is the identity comparison for FText; only touch Slate when the line actually changes.
	if (Prompt.EqualTo(LastPrompt))
	{
		return;
	}
	LastPrompt = Prompt;

	// The automated harness composites the scene WITHOUT Slate — neither playtest_observe nor
	// HighResShot can photograph UMG — so the only way to attest that this HUD is live and showing
	// the right line is to log each transition. Change-gated above, so this is not per-frame spam.
	UE_LOG(LogTemp, Log, TEXT("GloamPrompt: showing \"%s\""), *Prompt.ToString());

	PromptText->SetText(Prompt);
	// Collapsed rather than Hidden so an empty prompt takes no layout space and leaves a clean frame.
	PromptText->SetVisibility(Prompt.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}
