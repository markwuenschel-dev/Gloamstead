#include "Data/GloamsteadMeshForgeTypes.h"
#include "Systems/GloamsteadForgeEvidence.h" // reuse ReadGitCommit for provenance
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"
#include "HAL/FileManager.h"

namespace
{
	bool GMFIsSha256(const FString& Value)
	{
		if (Value.Len() != 64) { return false; }
		for (const TCHAR Ch : Value) { if (!FChar::IsHexDigit(Ch)) { return false; } }
		return true;
	}

	bool GMFIsGeneratedVersionRoot(const FString& Root)
	{
		static const FString Prefix = TEXT("/Game/Gloamstead/Generated/Biomes/Sanctuary/");
		return Root.StartsWith(Prefix) && Root.Len() > Prefix.Len() && !Root.EndsWith(TEXT("/"));
	}
}

// ===== Tokens =====

FString GMFProxyTypeToken(EGMFProxyType Type)
{
	switch (Type)
	{
	case EGMFProxyType::Heart:              return TEXT("Heart");
	case EGMFProxyType::RitualPoint:        return TEXT("RitualPoint");
	case EGMFProxyType::LanternRestore:     return TEXT("LanternRestore");
	case EGMFProxyType::InteractionRadius:  return TEXT("InteractionRadius");
	case EGMFProxyType::PathCue:            return TEXT("PathCue");
	case EGMFProxyType::NightFeedback:      return TEXT("NightFeedback");
	case EGMFProxyType::CorruptionFeedback: return TEXT("CorruptionFeedback");
	default:                                return TEXT("Unknown");
	}
}

FString GMFProviderTypeToken(EGMFProviderType Type)
{
	switch (Type)
	{
	case EGMFProviderType::EnginePrimitiveRuntimeProxy:  return TEXT("engine_primitive_runtime_proxy");
	case EGMFProviderType::GeneratedOwnedMeshForgeAsset:  return TEXT("generated_owned_meshforge_asset");
	default:                                             return TEXT("unknown");
	}
}

FString GMFOwnershipClassToken(EGMFOwnershipClass Ownership)
{
	switch (Ownership)
	{
	case EGMFOwnershipClass::CodeOwnedRuntimeProxy: return TEXT("code_owned_runtime_proxy");
	case EGMFOwnershipClass::GeneratedOwned:        return TEXT("generated_owned");
	default:                                        return TEXT("unknown");
	}
}

FString GMFSourceSystemToken(EGMFSourceSystem System)
{
	switch (System)
	{
	case EGMFSourceSystem::VeilHeart:       return TEXT("VeilHeart");
	case EGMFSourceSystem::PCGSubsystem:    return TEXT("PCGSubsystem");
	case EGMFSourceSystem::RitualPlacement: return TEXT("RitualPlacement");
	case EGMFSourceSystem::DayNight:        return TEXT("DayNight");
	case EGMFSourceSystem::NightRuntime:    return TEXT("NightRuntime");
	case EGMFSourceSystem::None:
	default:                                return TEXT("None");
	}
}

// ===== Validation (fail closed; empty result = valid) =====

TArray<FString> GMFValidateDescriptor(const FGloamsteadMeshForgeProviderDescriptor& D)
{
	TArray<FString> Codes;
	if (D.ProviderId.IsEmpty()) { Codes.Add(TEXT("GMF002")); } // provider not declared

	if (D.ProviderType == EGMFProviderType::EnginePrimitiveRuntimeProxy)
	{
		// A runtime-primitive provider may not claim generated ownership or generated-asset capability.
		if (D.OwnershipClass != EGMFOwnershipClass::CodeOwnedRuntimeProxy) { Codes.Add(TEXT("GMF014")); }
		if (D.bSupportsGeneratedAssets) { Codes.Add(TEXT("GMF014")); }
		if (!D.bSupportsRuntimePrimitives) { Codes.Add(TEXT("GMF014")); }
	}
	else if (D.ProviderType == EGMFProviderType::GeneratedOwnedMeshForgeAsset)
	{
		if (D.OwnershipClass != EGMFOwnershipClass::GeneratedOwned) { Codes.Add(TEXT("GMF003")); }
		if (!D.bSupportsGeneratedAssets) { Codes.Add(TEXT("GMF014")); }
	}
	return Codes;
}

TArray<FString> GMFValidateInstance(const FGloamsteadMeshForgeProxyInstance& I)
{
	TArray<FString> Codes;

	if (I.Spec.ProxyId.IsEmpty()) { Codes.Add(TEXT("GMF008")); }              // proxy spec invalid
	if (I.Binding.SourceSystem == EGMFSourceSystem::None) { Codes.Add(TEXT("GMF004")); } // source binding invalid

	// Provider / ownership overclaim.
	if (I.ProviderType == EGMFProviderType::EnginePrimitiveRuntimeProxy &&
		I.OwnershipClass == EGMFOwnershipClass::GeneratedOwned)
	{
		Codes.Add(TEXT("GMF014")); // engine primitive claims generated ownership
	}

	// Generated-asset honesty.
	const bool bHasPath = !I.GeneratedAssetPath.IsEmpty();
	if (I.bRuntimeOnly && bHasPath) { Codes.Add(TEXT("GMF015")); }                                  // runtime-only + asset path
	if (I.ProviderType == EGMFProviderType::EnginePrimitiveRuntimeProxy && bHasPath) { Codes.Add(TEXT("GMF015")); }
	if (I.OwnershipClass == EGMFOwnershipClass::GeneratedOwned && !bHasPath) { Codes.Add(TEXT("GMF015")); } // generated but no path
	if (I.OwnershipClass == EGMFOwnershipClass::GeneratedOwned && I.bRuntimeOnly) { Codes.Add(TEXT("GMF015")); }
	if (I.OwnershipClass == EGMFOwnershipClass::CodeOwnedRuntimeProxy && !I.bRuntimeOnly) { Codes.Add(TEXT("GMF015")); }
	if (I.ProviderType == EGMFProviderType::GeneratedOwnedMeshForgeAsset)
	{
		if (I.OwnershipClass != EGMFOwnershipClass::GeneratedOwned || I.bRuntimeOnly) { Codes.AddUnique(TEXT("GMF015")); }
		if (!GMFIsGeneratedVersionRoot(I.GeneratedVersionRoot)
			|| !I.GeneratedAssetPath.StartsWith(I.GeneratedVersionRoot + TEXT("/")))
		{
			Codes.AddUnique(TEXT("GMF020"));
		}
		if (I.GeneratedBundleId.IsEmpty()) { Codes.AddUnique(TEXT("GMF018")); }
		if (!GMFIsSha256(I.GeneratedReceiptSha256)) { Codes.AddUnique(TEXT("GMF019")); }
		if (!GMFIsSha256(I.GeneratedObjectSha256) || I.GeneratedOwnershipId.IsEmpty() || I.GeneratedLicenseId.IsEmpty())
		{
			Codes.AddUnique(TEXT("GMF021"));
		}
		if (!I.bSpawned || !I.bVisibleProxyCreated) { Codes.AddUnique(TEXT("GMF024")); }
	}

	// A proxy cannot claim it is visible without having spawned.
	if (I.bVisibleProxyCreated && !I.bSpawned) { Codes.Add(TEXT("GMF009")); }

	return Codes;
}

TArray<FString> GMFValidateReport(const FGloamsteadMeshForgeVisibilityReport& R)
{
	TArray<FString> Codes;

	if (R.bBinaryContentTouched) { Codes.Add(TEXT("GMF016")); } // source-only wave: no binary content

	// Report-level provider/ownership honesty.
	if (R.ProviderType == EGMFProviderType::EnginePrimitiveRuntimeProxy)
	{
		if (R.OwnershipClass != EGMFOwnershipClass::CodeOwnedRuntimeProxy) { Codes.Add(TEXT("GMF014")); }
		if (R.GeneratedAssetCount > 0) { Codes.Add(TEXT("GMF015")); } // no generated assets on a runtime provider
		if (R.RuntimeOnlyProxyCount != R.ProxyCount) { Codes.Add(TEXT("GMF015")); }
	}
	else if (R.ProviderType == EGMFProviderType::GeneratedOwnedMeshForgeAsset)
	{
		if (R.OwnershipClass != EGMFOwnershipClass::GeneratedOwned) { Codes.AddUnique(TEXT("GMF003")); }
		if (R.RuntimeOnlyProxyCount != 0) { Codes.AddUnique(TEXT("GMF015")); }
		if (!GMFIsGeneratedVersionRoot(R.ActiveGeneratedVersionRoot)) { Codes.AddUnique(TEXT("GMF020")); }
		if (R.ActiveGeneratedBundleId.IsEmpty()) { Codes.AddUnique(TEXT("GMF018")); }
		if (!GMFIsSha256(R.ActiveGeneratedReceiptSha256)) { Codes.AddUnique(TEXT("GMF019")); }
		if (R.ProxyCount != R.Proxies.Num())
		{
			Codes.AddUnique(TEXT("GMF015"));
		}
		int32 VerifiedVisibleGeneratedAssets = 0;
		for (const FGloamsteadMeshForgeProxyInstance& Instance : R.Proxies)
		{
			if (Instance.ProviderType == EGMFProviderType::GeneratedOwnedMeshForgeAsset
				&& !Instance.GeneratedAssetPath.IsEmpty()
				&& Instance.bSpawned && Instance.bVisibleProxyCreated)
			{
				++VerifiedVisibleGeneratedAssets;
			}
		}
		if (R.GeneratedAssetCount != VerifiedVisibleGeneratedAssets
			|| R.GeneratedAssetCount != R.ProxyCount)
		{
			Codes.AddUnique(TEXT("GMF024"));
		}
	}

	// Coverage: a readable sanctuary needs the Heart and at least one ritual point visible.
	if (R.HeartProxyCount <= 0) { Codes.Add(TEXT("GMF010")); }
	if (R.RitualPointProxyCount <= 0) { Codes.Add(TEXT("GMF011")); }

	// Per-proxy overclaim rolls up into the report.
	for (const FGloamsteadMeshForgeProxyInstance& I : R.Proxies)
	{
		for (const FString& C : GMFValidateInstance(I)) { Codes.AddUnique(C); }
		if (I.FailureCodes.Num() > 0) { Codes.AddUnique(TEXT("GMF023")); }
		if (R.ProviderType == EGMFProviderType::GeneratedOwnedMeshForgeAsset)
		{
			if (I.GeneratedVersionRoot != R.ActiveGeneratedVersionRoot) { Codes.AddUnique(TEXT("GMF020")); }
			if (I.GeneratedBundleId != R.ActiveGeneratedBundleId) { Codes.AddUnique(TEXT("GMF018")); }
			if (!I.GeneratedReceiptSha256.Equals(R.ActiveGeneratedReceiptSha256, ESearchCase::IgnoreCase))
			{
				Codes.AddUnique(TEXT("GMF019"));
			}
		}
	}

	return Codes;
}

// ===== JSON reports =====

namespace
{
	double R4(double V) { return FMath::RoundToDouble(V * 10000.0) / 10000.0; }

	TSharedPtr<FJsonObject> VectorJson(const FVector& V)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetNumberField(TEXT("x"), R4(V.X));
		O->SetNumberField(TEXT("y"), R4(V.Y));
		O->SetNumberField(TEXT("z"), R4(V.Z));
		return O;
	}

	TSharedPtr<FJsonObject> InstanceJson(const FGloamsteadMeshForgeProxyInstance& I)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("proxy_id"), I.Spec.ProxyId);
		O->SetStringField(TEXT("proxy_type"), GMFProxyTypeToken(I.Spec.ProxyType));
		O->SetStringField(TEXT("provider_type"), GMFProviderTypeToken(I.ProviderType));
		O->SetStringField(TEXT("ownership_class"), GMFOwnershipClassToken(I.OwnershipClass));
		O->SetStringField(TEXT("source_system"), GMFSourceSystemToken(I.Binding.SourceSystem));
		O->SetStringField(TEXT("source_actor"), I.Binding.SourceObject.IsValid() ? I.Binding.SourceObject->GetName() : FString());
		O->SetNumberField(TEXT("source_point_index"), I.Binding.SourcePointIndex);
		O->SetStringField(TEXT("ritual_type"), GetRitualTypeDisplayName(I.Binding.RitualType));
		O->SetStringField(TEXT("night_type"), GetNightConsequenceTypeDisplayName(I.Binding.NightType));
		O->SetObjectField(TEXT("world_location"), VectorJson(I.Binding.WorldLocation));
		O->SetBoolField(TEXT("spawned"), I.bSpawned);
		O->SetBoolField(TEXT("visible_proxy_created"), I.bVisibleProxyCreated);
		O->SetBoolField(TEXT("interaction_relevant"), I.Spec.bInteractionRelevant);
		O->SetStringField(TEXT("generated_asset_role"), I.Spec.GeneratedAssetRole.ToString());
		O->SetStringField(TEXT("generated_asset_state"), GACStateToken(I.Spec.GeneratedAssetState));
		O->SetStringField(TEXT("projected_day_phase"), I.Spec.ProjectedDayPhase.ToString());
		O->SetNumberField(TEXT("projected_wetness"), I.Spec.ProjectedWetness);
		O->SetStringField(TEXT("projected_warning_tag"), I.Spec.ProjectedWarningTag.ToString());
		O->SetBoolField(TEXT("runtime_only"), I.bRuntimeOnly);
		if (I.GeneratedAssetPath.IsEmpty()) { O->SetField(TEXT("generated_asset_path"), MakeShared<FJsonValueNull>()); }
		else { O->SetStringField(TEXT("generated_asset_path"), I.GeneratedAssetPath); }
		O->SetStringField(TEXT("generated_version_root"), I.GeneratedVersionRoot);
		O->SetStringField(TEXT("generated_bundle_id"), I.GeneratedBundleId);
		O->SetStringField(TEXT("generated_receipt_sha256"), I.GeneratedReceiptSha256);
		O->SetStringField(TEXT("generated_object_sha256"), I.GeneratedObjectSha256);
		O->SetStringField(TEXT("generated_ownership_id"), I.GeneratedOwnershipId);
		O->SetStringField(TEXT("generated_license_id"), I.GeneratedLicenseId);
		TArray<TSharedPtr<FJsonValue>> Fc;
		for (const FString& C : I.FailureCodes) { Fc.Add(MakeShared<FJsonValueString>(C)); }
		O->SetArrayField(TEXT("failure_codes"), Fc);
		return O;
	}

	bool WriteJson(const TSharedPtr<FJsonObject>& Root, const FString& Path)
	{
		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
		return FFileHelper::SaveStringToFile(Out, *Path);
	}
}

FString GloamsteadMeshForgeReport::DefaultReportDir()
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("procedural"), TEXT("reports"), TEXT("gloamstead_meshforge"));
}

bool GloamsteadMeshForgeReport::WriteReports(const FGloamsteadMeshForgeVisibilityReport& Report,
	const FGloamsteadMeshForgeProviderDescriptor& Descriptor, const FString& OutDir, FString& OutPrimaryPath)
{
	IFileManager::Get().MakeDirectory(*OutDir, /*Tree*/ true);
	const FString Now = FDateTime::UtcNow().ToIso8601();
	const FString GitSha = GloamsteadForgeEvidence::ReadGitCommit();

	// --- visibility_proxy_report.json (primary) ---
	TSharedPtr<FJsonObject> Vis = MakeShared<FJsonObject>();
	Vis->SetStringField(TEXT("schema"), TEXT("GloamsteadMeshForgeVisibilityReport/v2"));
	Vis->SetStringField(TEXT("report_id"), Report.ReportId);
	Vis->SetStringField(TEXT("created_at"), Now);
	Vis->SetStringField(TEXT("git_sha"), GitSha);
	Vis->SetStringField(TEXT("provider_type"), GMFProviderTypeToken(Report.ProviderType));
	Vis->SetStringField(TEXT("ownership_class"), GMFOwnershipClassToken(Report.OwnershipClass));
	Vis->SetNumberField(TEXT("proxy_count"), Report.ProxyCount);
	Vis->SetNumberField(TEXT("heart_proxy_count"), Report.HeartProxyCount);
	Vis->SetNumberField(TEXT("ritual_point_proxy_count"), Report.RitualPointProxyCount);
	Vis->SetNumberField(TEXT("lantern_proxy_count"), Report.LanternProxyCount);
	Vis->SetNumberField(TEXT("interaction_radius_proxy_count"), Report.InteractionRadiusProxyCount);
	Vis->SetNumberField(TEXT("night_feedback_proxy_count"), Report.NightFeedbackProxyCount);
	Vis->SetNumberField(TEXT("generated_asset_count"), Report.GeneratedAssetCount);
	Vis->SetNumberField(TEXT("runtime_only_proxy_count"), Report.RuntimeOnlyProxyCount);
	Vis->SetStringField(TEXT("active_generated_version_root"), Report.ActiveGeneratedVersionRoot);
	Vis->SetStringField(TEXT("active_generated_bundle_id"), Report.ActiveGeneratedBundleId);
	Vis->SetStringField(TEXT("active_generated_receipt_sha256"), Report.ActiveGeneratedReceiptSha256);
	Vis->SetBoolField(TEXT("binary_content_touched"), Report.bBinaryContentTouched);
	{
		TArray<TSharedPtr<FJsonValue>> Fc;
		for (const FString& C : Report.FailureCodes) { Fc.Add(MakeShared<FJsonValueString>(C)); }
		Vis->SetArrayField(TEXT("failure_codes"), Fc);
	}
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const FGloamsteadMeshForgeProxyInstance& I : Report.Proxies) { Arr.Add(MakeShared<FJsonValueObject>(InstanceJson(I))); }
		Vis->SetArrayField(TEXT("proxies"), Arr);
	}
	OutPrimaryPath = FPaths::Combine(OutDir, TEXT("visibility_proxy_report.json"));
	bool bOk = WriteJson(Vis, OutPrimaryPath);

	// --- provider_report.json ---
	TSharedPtr<FJsonObject> Prov = MakeShared<FJsonObject>();
	Prov->SetStringField(TEXT("schema"), TEXT("GloamsteadMeshForgeProviderReport/v1"));
	Prov->SetStringField(TEXT("created_at"), Now);
	Prov->SetStringField(TEXT("provider_id"), Descriptor.ProviderId);
	Prov->SetStringField(TEXT("provider_type"), GMFProviderTypeToken(Descriptor.ProviderType));
	Prov->SetStringField(TEXT("ownership_class"), GMFOwnershipClassToken(Descriptor.OwnershipClass));
	Prov->SetBoolField(TEXT("supports_runtime_primitives"), Descriptor.bSupportsRuntimePrimitives);
	Prov->SetBoolField(TEXT("supports_generated_assets"), Descriptor.bSupportsGeneratedAssets);
	Prov->SetBoolField(TEXT("can_spawn_heart_proxy"), Descriptor.bCanSpawnHeartProxy);
	Prov->SetBoolField(TEXT("can_spawn_ritual_point_proxy"), Descriptor.bCanSpawnRitualPointProxy);
	Prov->SetBoolField(TEXT("can_spawn_interaction_radius_proxy"), Descriptor.bCanSpawnInteractionRadiusProxy);
	Prov->SetBoolField(TEXT("can_spawn_night_feedback_proxy"), Descriptor.bCanSpawnNightFeedbackProxy);
	bOk &= WriteJson(Prov, FPaths::Combine(OutDir, TEXT("provider_report.json")));

	// --- source_binding_report.json ---
	TSharedPtr<FJsonObject> Src = MakeShared<FJsonObject>();
	Src->SetStringField(TEXT("schema"), TEXT("GloamsteadMeshForgeSourceBindingReport/v1"));
	Src->SetStringField(TEXT("created_at"), Now);
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const FGloamsteadMeshForgeProxyInstance& I : Report.Proxies)
		{
			TSharedPtr<FJsonObject> B = MakeShared<FJsonObject>();
			B->SetStringField(TEXT("proxy_id"), I.Spec.ProxyId);
			B->SetStringField(TEXT("source_system"), GMFSourceSystemToken(I.Binding.SourceSystem));
			B->SetStringField(TEXT("source_actor"), I.Binding.SourceObject.IsValid() ? I.Binding.SourceObject->GetName() : FString());
			B->SetNumberField(TEXT("source_point_index"), I.Binding.SourcePointIndex);
			B->SetBoolField(TEXT("location_resolved"), I.Binding.bLocationResolved);
			B->SetObjectField(TEXT("world_location"), VectorJson(I.Binding.WorldLocation));
			Arr.Add(MakeShared<FJsonValueObject>(B));
		}
		Src->SetArrayField(TEXT("bindings"), Arr);
	}
	bOk &= WriteJson(Src, FPaths::Combine(OutDir, TEXT("source_binding_report.json")));

	return bOk;
}
