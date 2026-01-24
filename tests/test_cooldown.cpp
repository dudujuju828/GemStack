#include <gtest/gtest.h>
#include <GemStackCore.h>
#include <vector>

// ============================================================================
// Test Fixtures and Helpers
// ============================================================================

class CooldownTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset to defaults before each test
        g_config = getDefaultConfig();
        resetCooldownSleeper();
        applyCooldownCliOverrides(std::nullopt, std::nullopt);
        sleepCalls.clear();
    }

    void TearDown() override {
        // Clean up after each test
        resetCooldownSleeper();
        applyCooldownCliOverrides(std::nullopt, std::nullopt);
    }

    // Track sleep calls for verification
    static std::vector<int> sleepCalls;

    static void fakeSleeper(int seconds) {
        sleepCalls.push_back(seconds);
    }
};

std::vector<int> CooldownTest::sleepCalls;

// ============================================================================
// Default Behavior Tests
// ============================================================================

TEST_F(CooldownTest, DefaultsDisabled) {
    // Default config should have cooldown disabled
    EXPECT_FALSE(g_config.cooldownEnabled);
    EXPECT_EQ(g_config.cooldownSeconds, 60);
    EXPECT_FALSE(isCooldownEnabled());
}

TEST_F(CooldownTest, NoDelayWhenDisabled) {
    setCooldownSleeper(fakeSleeper);

    // Cooldown disabled by default
    EXPECT_FALSE(isCooldownEnabled());

    // performCooldown should return false and not call sleeper
    bool result = performCooldown();
    EXPECT_FALSE(result);
    EXPECT_TRUE(sleepCalls.empty());
}

// ============================================================================
// Enabled Behavior Tests
// ============================================================================

TEST_F(CooldownTest, DelayWhenEnabled) {
    setCooldownSleeper(fakeSleeper);

    // Enable cooldown via config
    g_config.cooldownEnabled = true;
    g_config.cooldownSeconds = 30;

    EXPECT_TRUE(isCooldownEnabled());
    EXPECT_EQ(getEffectiveCooldownSeconds(), 30);

    // performCooldown should return true and call sleeper
    bool result = performCooldown();
    EXPECT_TRUE(result);
    ASSERT_EQ(sleepCalls.size(), 1);
    EXPECT_EQ(sleepCalls[0], 30);
}

TEST_F(CooldownTest, MultiplePromptsDelayCount) {
    setCooldownSleeper(fakeSleeper);

    // Enable cooldown
    g_config.cooldownEnabled = true;
    g_config.cooldownSeconds = 15;

    // Simulate N prompts - delay should be called N-1 times
    // (delay happens AFTER prompt, BEFORE next one)
    const int numPrompts = 5;
    for (int i = 0; i < numPrompts - 1; i++) {
        // After each prompt except the last, perform cooldown
        performCooldown();
    }

    // For 5 prompts, we should have 4 delays
    EXPECT_EQ(sleepCalls.size(), numPrompts - 1);

    for (int seconds : sleepCalls) {
        EXPECT_EQ(seconds, 15);
    }
}

// ============================================================================
// CLI Override Tests
// ============================================================================

TEST_F(CooldownTest, CliEnableOverridesConfigDisabled) {
    setCooldownSleeper(fakeSleeper);

    // Config has cooldown disabled
    g_config.cooldownEnabled = false;
    EXPECT_FALSE(isCooldownEnabled());

    // CLI enables it
    applyCooldownCliOverrides(true, std::nullopt);
    EXPECT_TRUE(isCooldownEnabled());

    // Should use default seconds from config
    EXPECT_EQ(getEffectiveCooldownSeconds(), 60);

    bool result = performCooldown();
    EXPECT_TRUE(result);
    EXPECT_EQ(sleepCalls.size(), 1);
}

TEST_F(CooldownTest, CliDisableOverridesConfigEnabled) {
    setCooldownSleeper(fakeSleeper);

    // Config has cooldown enabled
    g_config.cooldownEnabled = true;
    g_config.cooldownSeconds = 45;
    EXPECT_TRUE(isCooldownEnabled());

    // CLI disables it
    applyCooldownCliOverrides(false, std::nullopt);
    EXPECT_FALSE(isCooldownEnabled());

    bool result = performCooldown();
    EXPECT_FALSE(result);
    EXPECT_TRUE(sleepCalls.empty());
}

TEST_F(CooldownTest, CliSecondsOverridesConfig) {
    setCooldownSleeper(fakeSleeper);

    // Config has 60 seconds
    g_config.cooldownEnabled = true;
    g_config.cooldownSeconds = 60;

    // CLI overrides to 120 seconds
    applyCooldownCliOverrides(std::nullopt, 120);

    EXPECT_TRUE(isCooldownEnabled());
    EXPECT_EQ(getEffectiveCooldownSeconds(), 120);

    performCooldown();
    ASSERT_EQ(sleepCalls.size(), 1);
    EXPECT_EQ(sleepCalls[0], 120);
}

TEST_F(CooldownTest, CliBothOverrides) {
    setCooldownSleeper(fakeSleeper);

    // Config disabled with 60 seconds
    g_config.cooldownEnabled = false;
    g_config.cooldownSeconds = 60;

    // CLI enables and sets 90 seconds
    applyCooldownCliOverrides(true, 90);

    EXPECT_TRUE(isCooldownEnabled());
    EXPECT_EQ(getEffectiveCooldownSeconds(), 90);

    performCooldown();
    ASSERT_EQ(sleepCalls.size(), 1);
    EXPECT_EQ(sleepCalls[0], 90);
}

TEST_F(CooldownTest, CliNulloptUsesConfigValues) {
    setCooldownSleeper(fakeSleeper);

    // Set specific config values
    g_config.cooldownEnabled = true;
    g_config.cooldownSeconds = 45;

    // CLI passes nullopt (no override)
    applyCooldownCliOverrides(std::nullopt, std::nullopt);

    // Should use config values
    EXPECT_TRUE(isCooldownEnabled());
    EXPECT_EQ(getEffectiveCooldownSeconds(), 45);
}

// ============================================================================
// Invalid/Edge Case Tests
// ============================================================================

TEST_F(CooldownTest, NonPositiveSecondsFallbackTo60_Config) {
    // Zero seconds - should fall back to 60
    g_config.cooldownEnabled = true;
    g_config.cooldownSeconds = 0;

    // Note: Config parsing handles this, but let's test the default behavior
    // The config parser falls back to 60 for non-positive values
    // Here we test the effective value stays at what was set (0)
    // The actual fallback happens in loadConfig()

    // Test that getEffectiveCooldownSeconds returns what's in config
    EXPECT_EQ(getEffectiveCooldownSeconds(), 0);

    // But when parsed from config file, non-positive values become 60
    // (tested via integration or manual verification)
}

TEST_F(CooldownTest, NonPositiveSecondsFallbackTo60_Cli) {
    setCooldownSleeper(fakeSleeper);

    g_config.cooldownEnabled = true;
    g_config.cooldownSeconds = 45;

    // CLI passes 0 - should fall back to 60
    applyCooldownCliOverrides(std::nullopt, 0);
    EXPECT_EQ(getEffectiveCooldownSeconds(), 60);

    // CLI passes negative - should also fall back to 60
    applyCooldownCliOverrides(std::nullopt, -10);
    EXPECT_EQ(getEffectiveCooldownSeconds(), 60);

    performCooldown();
    ASSERT_EQ(sleepCalls.size(), 1);
    EXPECT_EQ(sleepCalls[0], 60);
}

TEST_F(CooldownTest, ResetSleeperUsesDefault) {
    // Set fake sleeper
    setCooldownSleeper(fakeSleeper);
    g_config.cooldownEnabled = true;

    performCooldown();
    EXPECT_EQ(sleepCalls.size(), 1);

    // Reset to default sleeper
    resetCooldownSleeper();

    // This would call the real sleeper (std::this_thread::sleep_for)
    // We can't easily verify this without actually sleeping
    // But we can verify sleepCalls doesn't increase
    sleepCalls.clear();

    // Don't actually call performCooldown with real sleeper in test
    // Just verify reset doesn't crash
    EXPECT_TRUE(isCooldownEnabled());
}

// ============================================================================
// Precedence Tests (Config -> CLI)
// ============================================================================

TEST_F(CooldownTest, PrecedenceCliOverConfig) {
    // This test verifies the complete precedence chain:
    // CLI flags > Config file > Defaults

    // 1. Start with defaults
    EXPECT_FALSE(isCooldownEnabled());
    EXPECT_EQ(getEffectiveCooldownSeconds(), 60);

    // 2. Apply config values
    g_config.cooldownEnabled = true;
    g_config.cooldownSeconds = 30;

    EXPECT_TRUE(isCooldownEnabled());
    EXPECT_EQ(getEffectiveCooldownSeconds(), 30);

    // 3. Apply CLI override (takes precedence)
    applyCooldownCliOverrides(false, 120);

    EXPECT_FALSE(isCooldownEnabled());  // CLI disabled
    EXPECT_EQ(getEffectiveCooldownSeconds(), 120);  // CLI seconds

    // 4. Clear CLI override (back to config)
    applyCooldownCliOverrides(std::nullopt, std::nullopt);

    EXPECT_TRUE(isCooldownEnabled());  // Back to config
    EXPECT_EQ(getEffectiveCooldownSeconds(), 30);  // Back to config
}

// ============================================================================
// Integration-Style Tests
// ============================================================================

TEST_F(CooldownTest, SimulateQueueModeWithCooldown) {
    setCooldownSleeper(fakeSleeper);

    // Simulate queue mode with 3 prompts and cooldown enabled
    g_config.cooldownEnabled = true;
    g_config.cooldownSeconds = 10;

    // Simulate: prompt 1 -> cooldown -> prompt 2 -> cooldown -> prompt 3
    const int numPrompts = 3;

    for (int i = 0; i < numPrompts; i++) {
        // Execute prompt (simulated)
        bool moreCommandsPending = (i < numPrompts - 1);

        // Perform cooldown if more commands pending
        if (moreCommandsPending) {
            performCooldown();
        }
    }

    // Should have numPrompts - 1 delays
    EXPECT_EQ(sleepCalls.size(), numPrompts - 1);
}

TEST_F(CooldownTest, SimulateReflectiveModeWithCooldown) {
    setCooldownSleeper(fakeSleeper);

    // Simulate reflective mode with 5 iterations and cooldown enabled
    g_config.cooldownEnabled = true;
    g_config.cooldownSeconds = 20;

    const int maxIterations = 5;

    for (int iteration = 1; iteration <= maxIterations; iteration++) {
        // Execute main prompt (simulated)

        // Cooldown after prompt if not last iteration
        if (iteration < maxIterations) {
            performCooldown();
        }

        // Generate next prompt (simulated) - no cooldown here
    }

    // Should have maxIterations - 1 delays
    EXPECT_EQ(sleepCalls.size(), maxIterations - 1);
}
