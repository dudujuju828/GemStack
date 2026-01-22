#include <gtest/gtest.h>
#include <GitAutoCommit.h>

// ============================================================================
// Configuration Tests
// ============================================================================

TEST(GitAutoCommitConfig, DefaultValues) {
    GitAutoCommit committer;
    GitAutoCommitConfig config = committer.getConfig();

    // Verify defaults
    EXPECT_FALSE(config.enabled);
    EXPECT_EQ(config.messagePrefix, "[GemStack]");
    EXPECT_TRUE(config.includePrompt);
}

TEST(GitAutoCommitConfig, SetConfig) {
    GitAutoCommit committer;
    GitAutoCommitConfig newConfig;
    newConfig.enabled = true;
    newConfig.messagePrefix = "[AI]";
    newConfig.includePrompt = false;

    committer.setConfig(newConfig);
    GitAutoCommitConfig config = committer.getConfig();

    EXPECT_TRUE(config.enabled);
    EXPECT_EQ(config.messagePrefix, "[AI]");
    EXPECT_FALSE(config.includePrompt);
}

TEST(GitAutoCommitConfig, ConstructorWithConfig) {
    GitAutoCommitConfig initialConfig;
    initialConfig.enabled = true;
    initialConfig.messagePrefix = "[Test]";
    initialConfig.includePrompt = false;

    GitAutoCommit committer(initialConfig);
    GitAutoCommitConfig config = committer.getConfig();

    EXPECT_TRUE(config.enabled);
    EXPECT_EQ(config.messagePrefix, "[Test]");
    EXPECT_FALSE(config.includePrompt);
}

// ============================================================================
// CLI Override Tests
// ============================================================================

TEST(GitAutoCommitOverrides, EnableOverride) {
    GitAutoCommit committer;
    
    // Default is disabled
    EXPECT_FALSE(committer.isEnabled());

    // Override to enable
    committer.applyCliOverrides(true, std::nullopt, std::nullopt);
    EXPECT_TRUE(committer.isEnabled());
    
    // Config should remain unchanged
    EXPECT_FALSE(committer.getConfig().enabled);
}

TEST(GitAutoCommitOverrides, DisableOverride) {
    GitAutoCommitConfig config;
    config.enabled = true;
    GitAutoCommit committer(config);
    
    // Default is enabled via config
    EXPECT_TRUE(committer.isEnabled());

    // Override to disable
    committer.applyCliOverrides(false, std::nullopt, std::nullopt);
    EXPECT_FALSE(committer.isEnabled());
}

TEST(GitAutoCommitOverrides, NoOverride) {
    GitAutoCommit committer;
    
    // Default is disabled
    EXPECT_FALSE(committer.isEnabled());

    // Pass nullopt (no override)
    committer.applyCliOverrides(std::nullopt, std::nullopt, std::nullopt);
    EXPECT_FALSE(committer.isEnabled());

    // Change config to enabled
    GitAutoCommitConfig config;
    config.enabled = true;
    committer.setConfig(config);
    
    // Should now be enabled since no override is active
    // Note: applyCliOverrides sets the overrides. If we called it with nullopt, it might clear previous overrides or set them to null.
    // Let's verify behavior. Based on header: "Apply CLI overrides". It likely stores them.
    // If we call it again with nullopt, does it clear them?
    // The header says: "CLI overrides (nullopt means use config value)"
    // So passing nullopt should indeed revert to using config value.
    committer.applyCliOverrides(std::nullopt, std::nullopt, std::nullopt);
    EXPECT_TRUE(committer.isEnabled());
}

// Since we cannot easily access private members or the effective prefix/includePrompt directly 
// without getters for them (getConfig returns the base config, not the effective one),
// we might be limited in what we can test for prefix/includePrompt overrides unless
// we modify the class to expose "getEffectiveConfig()" or similar.
// However, we can infer `isEnabled()` because there is an accessor for it.
// Checking `GitAutoCommit.h`, there isn't a `getEffectivePrefix()` method.
// So we can only test `isEnabled()` for overrides with the current public API.
// Wait, `getConfig()` returns `m_config`. `applyCliOverrides` stores values in `m_cli...` members.
// There is no public method to see the effective prefix or includePrompt.
// That is a limitation of the current API for testing.
// We will stick to testing `isEnabled()` which is the most critical logic.

