#include "Misc/AutomationTest.h"
#include "Systems/GloamsteadForgeEvidence.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	const FString ShaA = TEXT("1111111111111111111111111111111111111111");
	const FString ShaB = TEXT("abcdefabcdefabcdefabcdefabcdefabcdefabcd");

	struct FGitEvidenceScratch
	{
		FGitEvidenceScratch()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("GloamsteadGitEvidenceTests"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			IFileManager::Get().MakeDirectory(*Root, true);
		}

		~FGitEvidenceScratch()
		{
			IFileManager::Get().DeleteDirectory(*Root, false, true);
		}

		FString Path(const FString& Relative) const
		{
			return FPaths::Combine(Root, Relative);
		}

		void Directory(const FString& Relative) const
		{
			IFileManager::Get().MakeDirectory(*Path(Relative), true);
		}

		bool Write(const FString& Relative, const FString& Contents) const
		{
			const FString FullPath = Path(Relative);
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(FullPath), true);
			return FFileHelper::SaveStringToFile(Contents, *FullPath);
		}

		FString Root;
	};

	bool Read(const FString& ProjectRoot, FString& OutCommit, FString& OutBranch)
	{
		OutCommit = TEXT("sentinel");
		OutBranch = TEXT("sentinel");
		return GloamsteadForgeEvidence::ReadGitIdentityForProjectRoot(ProjectRoot, OutCommit, OutBranch);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamsteadGitEvidenceValidLayoutsTest,
	"Gloamstead.ForgeEvidence.GitIdentity.ValidLayouts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamsteadGitEvidenceValidLayoutsTest::RunTest(const FString& Parameters)
{
	FGitEvidenceScratch Scratch;
	FString Commit;
	FString Branch;

	// Ordinary repository, CRLF symbolic HEAD, loose ref.
	Scratch.Directory(TEXT("ordinary/.git/refs/heads/feature"));
	TestTrue(TEXT("write ordinary HEAD"), Scratch.Write(TEXT("ordinary/.git/HEAD"), TEXT("ref: refs/heads/feature/evidence\r\n")));
	TestTrue(TEXT("write ordinary ref"), Scratch.Write(TEXT("ordinary/.git/refs/heads/feature/evidence"), ShaA + TEXT("\n")));
	TestTrue(TEXT("ordinary identity resolves"), Read(Scratch.Path(TEXT("ordinary")), Commit, Branch));
	TestEqual(TEXT("ordinary commit"), Commit, ShaA);
	TestEqual(TEXT("ordinary branch"), Branch, FString(TEXT("feature/evidence")));

	// Detached HEAD supports either Git object format and intentionally has no branch.
	Scratch.Directory(TEXT("detached/.git"));
	TestTrue(TEXT("write detached HEAD"), Scratch.Write(TEXT("detached/.git/HEAD"), ShaB + TEXT("\n")));
	TestTrue(TEXT("detached identity resolves"), Read(Scratch.Path(TEXT("detached")), Commit, Branch));
	TestEqual(TEXT("detached commit"), Commit, ShaB);
	TestTrue(TEXT("detached branch is empty"), Branch.IsEmpty());

	// A linked worktree authenticates its administrative directory through both commondir and
	// the worktree gitdir back-pointer. A worktree-local loose ref takes precedence.
	const FString LinkedProject = Scratch.Path(TEXT("linked"));
	const FString LinkedDotGit = FPaths::Combine(LinkedProject, TEXT(".git"));
	const FString Admin = Scratch.Path(TEXT("repository/.git/worktrees/linked"));
	Scratch.Directory(TEXT("linked"));
	Scratch.Directory(TEXT("repository/.git/worktrees/linked/refs/heads"));
	Scratch.Directory(TEXT("repository/.git/refs/heads"));
	TestTrue(TEXT("write linked .git"), Scratch.Write(TEXT("linked/.git"), FString::Printf(TEXT("gitdir: %s\r\n"), *Admin)));
	TestTrue(TEXT("write linked commondir"), Scratch.Write(TEXT("repository/.git/worktrees/linked/commondir"), TEXT("../..\n")));
	TestTrue(TEXT("write linked back-pointer"), Scratch.Write(TEXT("repository/.git/worktrees/linked/gitdir"), LinkedDotGit + TEXT("\r\n")));
	TestTrue(TEXT("write linked HEAD"), Scratch.Write(TEXT("repository/.git/worktrees/linked/HEAD"), TEXT("ref: refs/heads/linked-branch\n")));
	TestTrue(TEXT("write common loose ref"), Scratch.Write(TEXT("repository/.git/refs/heads/linked-branch"), ShaA + TEXT("\n")));
	TestTrue(TEXT("write worktree loose ref"), Scratch.Write(TEXT("repository/.git/worktrees/linked/refs/heads/linked-branch"), ShaB + TEXT("\r\n")));
	TestTrue(TEXT("linked identity resolves"), Read(LinkedProject, Commit, Branch));
	TestEqual(TEXT("worktree loose ref wins"), Commit, ShaB);
	TestEqual(TEXT("linked branch"), Branch, FString(TEXT("linked-branch")));

	// Relative gitdir indirection and an exact packed-refs match are also supported.
	const FString PackedProject = Scratch.Path(TEXT("packed-checkout"));
	const FString PackedDotGit = FPaths::Combine(PackedProject, TEXT(".git"));
	Scratch.Directory(TEXT("packed-checkout"));
	Scratch.Directory(TEXT("packed-repository/.git/worktrees/packed"));
	TestTrue(TEXT("write relative .git"), Scratch.Write(TEXT("packed-checkout/.git"), TEXT("gitdir: ../packed-repository/.git/worktrees/packed\n")));
	TestTrue(TEXT("write packed commondir"), Scratch.Write(TEXT("packed-repository/.git/worktrees/packed/commondir"), TEXT("../..\r\n")));
	TestTrue(TEXT("write packed back-pointer"), Scratch.Write(TEXT("packed-repository/.git/worktrees/packed/gitdir"), PackedDotGit + TEXT("\n")));
	TestTrue(TEXT("write packed HEAD"), Scratch.Write(TEXT("packed-repository/.git/worktrees/packed/HEAD"), TEXT("ref: refs/heads/main\r\n")));
	TestTrue(TEXT("write packed refs"), Scratch.Write(TEXT("packed-repository/.git/packed-refs"), FString::Printf(TEXT("# pack-refs with: peeled fully-peeled\n%s refs/heads/main\n"), *ShaA)));
	TestTrue(TEXT("packed linked identity resolves"), Read(PackedProject, Commit, Branch));
	TestEqual(TEXT("packed linked commit"), Commit, ShaA);
	TestEqual(TEXT("packed linked branch"), Branch, FString(TEXT("main")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamsteadGitEvidenceCurrentCheckoutTest,
	"Gloamstead.ForgeEvidence.GitIdentity.CurrentCheckout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamsteadGitEvidenceCurrentCheckoutTest::RunTest(const FString& Parameters)
{
	FString Commit;
	FString Branch;
	TestTrue(
		TEXT("the running project's ordinary or linked-worktree metadata resolves"),
		Read(FPaths::ProjectDir(), Commit, Branch));
	TestTrue(TEXT("current checkout commit is a full object id"), Commit.Len() == 40 || Commit.Len() == 64);
	for (const TCHAR Character : Commit)
	{
		TestTrue(TEXT("current checkout commit contains only hexadecimal characters"), FChar::IsHexDigit(Character));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGloamsteadGitEvidenceHostileLayoutsTest,
	"Gloamstead.ForgeEvidence.GitIdentity.HostileLayouts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamsteadGitEvidenceHostileLayoutsTest::RunTest(const FString& Parameters)
{
	FGitEvidenceScratch Scratch;
	FString Commit;
	FString Branch;

	auto ExpectRejected = [this, &Commit, &Branch](const TCHAR* Label, const FString& ProjectRoot)
	{
		TestFalse(Label, Read(ProjectRoot, Commit, Branch));
		TestTrue(FString(Label) + TEXT(" clears commit"), Commit.IsEmpty());
		TestTrue(FString(Label) + TEXT(" clears branch"), Branch.IsEmpty());
	};

	Scratch.Directory(TEXT("malformed-pointer"));
	Scratch.Write(TEXT("malformed-pointer/.git"), TEXT("git-dir: somewhere\n"));
	ExpectRejected(TEXT("malformed gitdir rejected"), Scratch.Path(TEXT("malformed-pointer")));

	Scratch.Directory(TEXT("escaped-pointer"));
	Scratch.Directory(TEXT("unrelated"));
	Scratch.Write(TEXT("escaped-pointer/.git"), TEXT("gitdir: ../unrelated\n"));
	ExpectRejected(TEXT("gitdir outside authenticated metadata rejected"), Scratch.Path(TEXT("escaped-pointer")));

	const FString BadCommonProject = Scratch.Path(TEXT("bad-common"));
	const FString BadCommonDotGit = FPaths::Combine(BadCommonProject, TEXT(".git"));
	const FString BadCommonAdmin = Scratch.Path(TEXT("bad-repository/.git/worktrees/bad-common"));
	Scratch.Directory(TEXT("bad-common"));
	Scratch.Directory(TEXT("bad-repository/.git/worktrees/bad-common"));
	Scratch.Write(TEXT("bad-common/.git"), FString::Printf(TEXT("gitdir: %s\n"), *BadCommonAdmin));
	Scratch.Write(TEXT("bad-repository/.git/worktrees/bad-common/gitdir"), BadCommonDotGit + TEXT("\n"));
	Scratch.Write(TEXT("bad-repository/.git/worktrees/bad-common/commondir"), TEXT(".\n"));
	Scratch.Write(TEXT("bad-repository/.git/worktrees/bad-common/HEAD"), ShaA + TEXT("\n"));
	ExpectRejected(TEXT("commondir cycle rejected"), BadCommonProject);

	const FString WrongBackProject = Scratch.Path(TEXT("wrong-back"));
	const FString WrongBackAdmin = Scratch.Path(TEXT("wrong-repository/.git/worktrees/wrong-back"));
	Scratch.Directory(TEXT("wrong-back"));
	Scratch.Directory(TEXT("wrong-repository/.git/worktrees/wrong-back"));
	Scratch.Write(TEXT("wrong-back/.git"), FString::Printf(TEXT("gitdir: %s\n"), *WrongBackAdmin));
	Scratch.Write(TEXT("wrong-repository/.git/worktrees/wrong-back/gitdir"), Scratch.Path(TEXT("spoof/.git")) + TEXT("\n"));
	Scratch.Write(TEXT("wrong-repository/.git/worktrees/wrong-back/commondir"), TEXT("../..\n"));
	Scratch.Write(TEXT("wrong-repository/.git/worktrees/wrong-back/HEAD"), ShaA + TEXT("\n"));
	ExpectRejected(TEXT("mismatched back-pointer rejected"), WrongBackProject);

	Scratch.Directory(TEXT("bad-detached/.git"));
	Scratch.Write(TEXT("bad-detached/.git/HEAD"), TEXT("not-a-sha\n"));
	ExpectRejected(TEXT("non-SHA detached HEAD rejected"), Scratch.Path(TEXT("bad-detached")));

	Scratch.Directory(TEXT("bad-loose/.git/refs/heads"));
	Scratch.Write(TEXT("bad-loose/.git/HEAD"), TEXT("ref: refs/heads/main\n"));
	Scratch.Write(TEXT("bad-loose/.git/refs/heads/main"), TEXT("ref: refs/heads/other\n"));
	ExpectRejected(TEXT("non-SHA loose ref rejected"), Scratch.Path(TEXT("bad-loose")));

	Scratch.Directory(TEXT("traversal/.git"));
	Scratch.Write(TEXT("traversal/.git/HEAD"), TEXT("ref: refs/heads/../../outside\n"));
	Scratch.Write(TEXT("outside"), ShaA + TEXT("\n"));
	ExpectRejected(TEXT("ref traversal rejected"), Scratch.Path(TEXT("traversal")));

	Scratch.Directory(TEXT("packed-suffix/.git"));
	Scratch.Write(TEXT("packed-suffix/.git/HEAD"), TEXT("ref: refs/heads/main\n"));
	Scratch.Write(TEXT("packed-suffix/.git/packed-refs"), FString::Printf(TEXT("%s refs/remotes/origin/refs/heads/main\n"), *ShaA));
	ExpectRejected(TEXT("packed-ref suffix spoof rejected"), Scratch.Path(TEXT("packed-suffix")));

	Scratch.Directory(TEXT("missing-head/.git"));
	ExpectRejected(TEXT("missing HEAD rejected"), Scratch.Path(TEXT("missing-head")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
