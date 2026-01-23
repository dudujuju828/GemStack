#ifndef GIT_AUTO_COMMIT_H
#define GIT_AUTO_COMMIT_H

#include <string>
#include <optional>

// Configuration for auto-commit behavior
struct GitAutoCommitConfig {
    bool enabled = false;
    std::string messagePrefix = "[GemStack]";
    bool includePrompt = true;

    // Maximum length for commit subject line (after prefix)
    static constexpr size_t MAX_SUBJECT_LENGTH = 72;
};

// Encapsulates all git auto-commit functionality
class GitAutoCommit {
public:
    GitAutoCommit();
    explicit GitAutoCommit(const GitAutoCommitConfig& config);

    // Set/get configuration
    void setConfig(const GitAutoCommitConfig& config);
    GitAutoCommitConfig getConfig() const;

    // Apply CLI overrides (takes precedence over config file settings)
    void applyCliOverrides(
        std::optional<bool> forceEnabled,
        std::optional<std::string> prefixOverride,
        std::optional<bool> includePromptOverride
    );

    // Check if current directory is inside a git repository
    static bool isGitRepository();

    // Initialize a new git repository in the current directory
    static bool initializeRepository();

    // Check if there are uncommitted changes (staged, unstaged, or untracked)
    static bool hasUncommittedChanges();

    // Attempt to create an auto-commit if conditions are met
    // Returns true if commit was created, false otherwise
    bool maybeCommit(const std::string& promptSummary);

    // Get the effective enabled state (after CLI overrides)
    bool isEnabled() const;

private:
    GitAutoCommitConfig m_config;

    // CLI overrides (nullopt means use config value)
    std::optional<bool> m_cliEnabledOverride;
    std::optional<std::string> m_cliPrefixOverride;
    std::optional<bool> m_cliIncludePromptOverride;

    // Sanitize and format commit message subject
    // - Replaces newlines/tabs with spaces
    // - Trims extra whitespace
    // - Truncates to MAX_SUBJECT_LENGTH
    std::string formatCommitSubject(const std::string& promptSummary) const;

    // Escape string for safe use in git commit message
    static std::string escapeForGitMessage(const std::string& input);

    // Execute git commands
    static bool stageAllChanges();
    static bool createCommit(const std::string& message);
};

#endif // GIT_AUTO_COMMIT_H
