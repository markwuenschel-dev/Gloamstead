#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGloamsteadSpineSmokeTest,
    "Gloamstead.Spine.Smoke",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGloamsteadSpineSmokeTest::RunTest(const FString& /*Parameters*/)
{
    TestTrue(TEXT("spine harness runner is alive"), true);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
