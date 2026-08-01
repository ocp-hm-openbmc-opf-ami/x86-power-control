#include "src/power_control.hpp"

// Undefine macros that clash with GTest names.
#ifdef FAIL
#undef FAIL
#endif
#ifdef ERROR
#undef ERROR
#endif

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <boost/asio/io_context.hpp>
#include <filesystem>
#include <fstream>

static constexpr const char* kStateFile = "/var/lib/power-control/state.json";
static constexpr const char* kPowerStateOn =
    "xyz.openbmc_project.State.Chassis.PowerState.On";
static constexpr const char* kPowerStateOff =
    "xyz.openbmc_project.State.Chassis.PowerState.Off";
static constexpr const char* kPolicyAlwaysOn =
    "xyz.openbmc_project.Control.Power.RestorePolicy.Policy.AlwaysOn";
static constexpr const char* kPolicyRestore =
    "xyz.openbmc_project.Control.Power.RestorePolicy.Policy.Restore";
static constexpr const char* kPolicyAlwaysOff =
    "xyz.openbmc_project.Control.Power.RestorePolicy.Policy.AlwaysOff";

// ─────────────────────────────────────────────────────────────────────────────
// PersistentState tests
// State dir is pre-created by the meson custom_target (sudo mkdir) before the
// test binary starts so the production global appState can initialize safely.
// ─────────────────────────────────────────────────────────────────────────────

namespace power_control
{
extern PersistentState appState;
}

class PersistentStateTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Reset to known Off value before each test.
        power_control::appState.set(
            power_control::PersistentState::Params::PowerState, kPowerStateOff);
    }
};

TEST_F(PersistentStateTest, Get_DefaultPowerStateIsOff)
{
    // Remove state file so a fresh instance must return the compiled-in default.
    std::filesystem::remove(kStateFile);
    power_control::PersistentState fresh;
    EXPECT_EQ(
        fresh.get(power_control::PersistentState::Params::PowerState),
        kPowerStateOff);
}

TEST_F(PersistentStateTest, GetSet_RoundTrip)
{
    power_control::appState.set(
        power_control::PersistentState::Params::PowerState, kPowerStateOn);
    EXPECT_EQ(
        power_control::appState.get(
            power_control::PersistentState::Params::PowerState),
        kPowerStateOn);
}

TEST_F(PersistentStateTest, Set_PersistsAcrossInstances)
{
    power_control::appState.set(
        power_control::PersistentState::Params::PowerState, kPowerStateOn);

    power_control::PersistentState second;
    EXPECT_EQ(
        second.get(power_control::PersistentState::Params::PowerState),
        kPowerStateOn);
}

TEST_F(PersistentStateTest, Get_ReturnsUpdatedValueAfterReset)
{
    power_control::appState.set(
        power_control::PersistentState::Params::PowerState, kPowerStateOn);
    power_control::appState.set(
        power_control::PersistentState::Params::PowerState, kPowerStateOff);
    EXPECT_EQ(
        power_control::appState.get(
            power_control::PersistentState::Params::PowerState),
        kPowerStateOff);
}

TEST_F(PersistentStateTest, Constructor_HandlesCorruptStateFile)
{
    std::ofstream bad(kStateFile);
    bad << "{not valid json";
    bad.close();

    EXPECT_NO_THROW({
        power_control::PersistentState recovered;
        EXPECT_EQ(
            recovered.get(power_control::PersistentState::Params::PowerState),
            kPowerStateOff);
    });
}

TEST_F(PersistentStateTest, Constructor_HandlesNonexistentStateFile)
{
    std::filesystem::remove(kStateFile);
    EXPECT_NO_THROW({
        power_control::PersistentState fresh;
        EXPECT_EQ(
            fresh.get(power_control::PersistentState::Params::PowerState),
            kPowerStateOff);
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// PowerRestoreController tests
// Only test setProperties paths that do NOT trigger invoke(): set only one of
// {policy, delay} so invokeIfReady() returns early (both must be set to fire).
// Tests that set both policy + delay are excluded because invoke() reaches
// setPowerState() which dereferences null D-Bus interface pointers.
// ─────────────────────────────────────────────────────────────────────────────

class PowerRestoreControllerTest : public ::testing::Test
{
  protected:
    boost::asio::io_context io;
    std::unique_ptr<power_control::PowerRestoreController> ctrl;

    void SetUp() override
    {
        ctrl = std::make_unique<power_control::PowerRestoreController>(io);
    }
};

TEST_F(PowerRestoreControllerTest, SetProperties_PolicyOnly_NoThrow)
{
    power_control::dbusPropertiesList props;
    props["PowerRestorePolicy"] = std::string(kPolicyAlwaysOn);
    EXPECT_NO_THROW(ctrl->setProperties(props));
}

TEST_F(PowerRestoreControllerTest, SetProperties_DelayOnly_NoThrow)
{
    power_control::dbusPropertiesList props;
    props["PowerRestoreDelay"] = static_cast<uint64_t>(5'000'000);
    EXPECT_NO_THROW(ctrl->setProperties(props));
}

TEST_F(PowerRestoreControllerTest, SetProperties_RestorePolicyOnly_NoThrow)
{
    power_control::dbusPropertiesList props;
    props["PowerRestorePolicy"] = std::string(kPolicyRestore);
    EXPECT_NO_THROW(ctrl->setProperties(props));
}

TEST_F(PowerRestoreControllerTest, SetProperties_AlwaysOffPolicyOnly_NoThrow)
{
    power_control::dbusPropertiesList props;
    props["PowerRestorePolicy"] = std::string(kPolicyAlwaysOff);
    EXPECT_NO_THROW(ctrl->setProperties(props));
}

TEST_F(PowerRestoreControllerTest, SetProperties_WrongTypeForPolicy_NoThrow)
{
    // uint64_t where string expected → logs error, no throw.
    power_control::dbusPropertiesList props;
    props["PowerRestorePolicy"] = static_cast<uint64_t>(99);
    EXPECT_NO_THROW(ctrl->setProperties(props));
}

TEST_F(PowerRestoreControllerTest, SetProperties_WrongTypeForDelay_NoThrow)
{
    // string where uint64_t expected → logs error, no throw.
    power_control::dbusPropertiesList props;
    props["PowerRestoreDelay"] = std::string("not_a_number");
    EXPECT_NO_THROW(ctrl->setProperties(props));
}

TEST_F(PowerRestoreControllerTest, SetProperties_UnknownProperty_NoThrow)
{
    power_control::dbusPropertiesList props;
    props["UnknownProp"] = std::string("value");
    EXPECT_NO_THROW(ctrl->setProperties(props));
}

TEST_F(PowerRestoreControllerTest, SetProperties_EmptyProps_NoThrow)
{
    EXPECT_NO_THROW(ctrl->setProperties({}));
}

TEST_F(PowerRestoreControllerTest, SetProperties_MultipleCallsPartial_NoThrow)
{
    // Each call sets only one field — invokeIfReady returns early each time.
    power_control::dbusPropertiesList p1;
    p1["PowerRestorePolicy"] = std::string(kPolicyAlwaysOn);
    EXPECT_NO_THROW(ctrl->setProperties(p1));

    power_control::dbusPropertiesList p2;
    p2["PowerRestorePolicy"] = std::string(kPolicyRestore);
    EXPECT_NO_THROW(ctrl->setProperties(p2));
}

// ─────────────────────────────────────────────────────────────────────────────
// PowerRestoreController::invoke() tests
// Setting both PowerRestorePolicy and PowerRestoreDelay (= 0 µs) in one
// setProperties() call makes invokeIfReady() compute a non-positive delay and
// call invoke() synchronously, exercising its branches without an async timer.
//
// Safety note: the global powerState is zero-initialized to PowerState::on.
// powerStateOn(Event::powerOnRequest) falls through to its default: branch,
// which calls only lg2::info() — no D-Bus interface pointer is dereferenced.
// setRestartCauseProperty() is a null-guard no-op (restartCauseIface is null).
// savePowerState() schedules a timer on the global io context that never runs.
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(PowerRestoreControllerTest, Invoke_AlwaysOff_NoThrow)
{
    power_control::dbusPropertiesList props;
    props["PowerRestorePolicy"] = std::string(kPolicyAlwaysOff);
    props["PowerRestoreDelay"] = static_cast<uint64_t>(0);
    EXPECT_NO_THROW(ctrl->setProperties(props));
}

TEST_F(PowerRestoreControllerTest, Invoke_AlwaysOff_Idempotent_SecondCallNoOp)
{
    power_control::dbusPropertiesList props;
    props["PowerRestorePolicy"] = std::string(kPolicyAlwaysOff);
    props["PowerRestoreDelay"] = static_cast<uint64_t>(0);
    ctrl->setProperties(props);
    // policyInvoked is now true; second call must not throw or side-effect.
    EXPECT_NO_THROW(ctrl->setProperties(props));
}

// powerStateOn(powerOnRequest) falls through to default: — no state change.
TEST_F(PowerRestoreControllerTest, Invoke_AlwaysOn_NoThrow)
{
    power_control::dbusPropertiesList props;
    props["PowerRestorePolicy"] = std::string(kPolicyAlwaysOn);
    props["PowerRestoreDelay"] = static_cast<uint64_t>(0);
    EXPECT_NO_THROW(ctrl->setProperties(props));
}

// wasPowerDropped() = false → Restore takes the else branch, no event sent.
TEST_F(PowerRestoreControllerTest, Invoke_Restore_PowerWasOff_NoThrow)
{
    power_control::appState.set(
        power_control::PersistentState::Params::PowerState, kPowerStateOff);
    power_control::dbusPropertiesList props;
    props["PowerRestorePolicy"] = std::string(kPolicyRestore);
    props["PowerRestoreDelay"] = static_cast<uint64_t>(0);
    EXPECT_NO_THROW(ctrl->setProperties(props));
}

// wasPowerDropped() = true → sendPowerControlEvent fires; powerStateOn falls
// through to default: (powerOnRequest has no explicit case in PowerState::on).
TEST_F(PowerRestoreControllerTest, Invoke_Restore_PowerWasOn_NoThrow)
{
    power_control::appState.set(
        power_control::PersistentState::Params::PowerState, kPowerStateOn);
    power_control::dbusPropertiesList props;
    props["PowerRestorePolicy"] = std::string(kPolicyRestore);
    props["PowerRestoreDelay"] = static_cast<uint64_t>(0);
    EXPECT_NO_THROW(ctrl->setProperties(props));
}
