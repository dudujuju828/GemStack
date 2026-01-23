#include <GitAutoCommit.h>
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <algorithm>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

GitAutoCommit::GitAutoCommit() : m_config() {}

GitAutoCommit::GitAutoCommit(const GitAutoCommitConfig& config) : m_config(config) {}

void GitAutoCommit::setConfig(const GitAutoCommitConfig& config) {
    m_config = config;
}

GitAutoCommitConfig GitAutoCommit::getConfig() const {
    return m_config;
}

void GitAutoCommit::applyCliOverrides(
    std::optional<bool> forceEnabled,
    std::optional<std::string> prefixOverride,
    std::optional<bool> includePromptOverride
) {
    m_cliEnabledOverride = forceEnabled;
    m_cliPrefixOverride = prefixOverride;
    m_cliIncludePromptOverride = includePromptOverride;
}

bool GitAutoCommit::isEnabled() const {
    // CLI override takes precedence
    if (m_cliEnabledOverride.has_value()) {
        return m_cliEnabledOverride.value();
    }
    return m_config.enabled;
}

bool GitAutoCommit::isGitRepository() {
#ifdef _WIN32
    int result = system("git rev-parse --is-inside-work-tree >nul 2>&1");
#else
    int result = system("git rev-parse --is-inside-work-tree >/dev/null 2>&1");
#endif
    return result == 0;
}

bool GitAutoCommit::initializeRepository() {
#ifdef _WIN32
    int result = system("git init >nul 2>&1");
#else
    int result = system("git init >/dev/null 2>&1");
#endif
    return result == 0;
}

bool GitAutoCommit::hasUncommittedChanges() {
    // Check for any changes using git status --porcelain
#ifdef _WIN32
    FILE* pipe = popen("git status --porcelain 2>nul", "r");
#else
    FILE* pipe = popen("git status --porcelain 2>/dev/null", "r");
#endif

    if (!pipe) {
        return false;
    }

    char buffer[128];
    bool hasChanges = (fgets(buffer, sizeof(buffer), pipe) != nullptr);
    pclose(pipe);

    return hasChanges;
}

std::string GitAutoCommit::escapeForGitMessage(const std::string& input) {
    std::string escaped;
    escaped.reserve(input.size() * 2);

    for (char c : input) {
        switch (c) {
            // Escape shell-sensitive characters
            case '"':
                escaped += "\\\"";
                break;
            case '\\':
                escaped += "\\\\";
                break;
            case '$':
                escaped += "\\$";
                break;
            case '`':
                escaped += "\\`";
                break;
            case '!':
                escaped += "\\!";
                break;
            // Replace potentially dangerous characters with space
            case ';':
            case '&':
            case '|':
            case '<':
            case '>':
            case '(':
            case ')':
            case '\n':
            case '\r':
            case '\t':
                escaped += ' ';
                break;
            default:
                escaped += c;
                break;
        }
    }

    return escaped;
}

std::string GitAutoCommit::formatCommitSubject(const std::string& promptSummary) const {
    // Determine effective prefix (CLI override takes precedence)
    std::string prefix = m_cliPrefixOverride.value_or(m_config.messagePrefix);

    // Determine if we should include prompt
    bool includePrompt = m_cliIncludePromptOverride.value_or(m_config.includePrompt);

    std::string subject;

    if (includePrompt && !promptSummary.empty()) {
        // Sanitize prompt summary: replace newlines/tabs with spaces
        std::string sanitized = promptSummary;
        for (char& c : sanitized) {
            if (c == '\n' || c == '\r' || c == '\t') {
                c = ' ';
            }
        }

        // Collapse multiple spaces into one
        std::string collapsed;
        bool lastWasSpace = false;
        for (char c : sanitized) {
            if (c == ' ') {
                if (!lastWasSpace) {
                    collapsed += c;
                    lastWasSpace = true;
                }
            } else {
                collapsed += c;
                lastWasSpace = false;
            }
        }

        // Trim leading/trailing spaces
        size_t start = collapsed.find_first_not_of(' ');
        size_t end = collapsed.find_last_not_of(' ');
        if (start != std::string::npos && end != std::string::npos) {
            collapsed = collapsed.substr(start, end - start + 1);
        }

        // Calculate available space for prompt after prefix
        size_t prefixLen = prefix.length() + 1;  // +1 for space between prefix and summary
        size_t maxSummaryLen = GitAutoCommitConfig::MAX_SUBJECT_LENGTH > prefixLen
            ? GitAutoCommitConfig::MAX_SUBJECT_LENGTH - prefixLen
            : 0;

        // Truncate if needed
        if (collapsed.length() > maxSummaryLen && maxSummaryLen > 3) {
            collapsed = collapsed.substr(0, maxSummaryLen - 3) + "...";
        } else if (maxSummaryLen <= 3) {
            collapsed = "";
        }

        subject = prefix;
        if (!collapsed.empty()) {
            subject += " " + collapsed;
        }
    } else {
        subject = prefix + " Auto-commit after prompt execution";
    }

    // Final safety truncation to ensure we never exceed max length
    if (subject.length() > GitAutoCommitConfig::MAX_SUBJECT_LENGTH) {
        subject = subject.substr(0, GitAutoCommitConfig::MAX_SUBJECT_LENGTH - 3) + "...";
    }

    return subject;
}

bool GitAutoCommit::stageAllChanges() {
#ifdef _WIN32
    int result = system("git add -A >nul 2>&1");
#else
    int result = system("git add -A >/dev/null 2>&1");
#endif
    return result == 0;
}

bool GitAutoCommit::createCommit(const std::string& message) {
    std::string escapedMessage = escapeForGitMessage(message);

#ifdef _WIN32
    std::string command = "git commit -m \"" + escapedMessage + "\" >nul 2>&1";
#else
    std::string command = "git commit -m \"" + escapedMessage + "\" >/dev/null 2>&1";
#endif

    int result = system(command.c_str());
    return result == 0;
}

bool GitAutoCommit::maybeCommit(const std::string& promptSummary) {
    // Check if auto-commit is enabled (considering CLI overrides)
    if (!isEnabled()) {
        return false;
    }

    // Check if we're in a git repository
    if (!isGitRepository()) {
        std::cout << "[GemStack] Repository not initialized. Initializing git repository..." << std::endl;
        if (!initializeRepository()) {
            std::cerr << "[GemStack] Auto-commit failed: could not initialize git repository" << std::endl;
            return false;
        }
    }

    // Check if there are changes to commit
    if (!hasUncommittedChanges()) {
        std::cout << "[GemStack] Auto-commit skipped: no changes detected" << std::endl;
        return false;
    }

    // Format the commit message
    std::string commitMessage = formatCommitSubject(promptSummary);

    std::cout << "[GemStack] Auto-committing changes..." << std::endl;

    // Stage all changes
    if (!stageAllChanges()) {
        std::cerr << "[GemStack] Auto-commit failed: could not stage changes" << std::endl;
        return false;
    }

    // Create the commit
    if (!createCommit(commitMessage)) {
        std::cerr << "[GemStack] Auto-commit failed: could not create commit" << std::endl;
        return false;
    }

    std::cout << "[GemStack] Auto-commit successful: " << commitMessage << std::endl;
    return true;
}
