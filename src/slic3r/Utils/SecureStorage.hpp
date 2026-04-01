#ifndef slic3r_SecureStorage_hpp_
#define slic3r_SecureStorage_hpp_

#include <string>
#include <optional>

namespace Slic3r {

/// Cross-platform secure storage for sensitive credentials.
/// Stores data in OS keyring/keychain with user-level access control.
///
/// Platform implementations:
/// - Windows: Windows Credential Manager (wincred.h)
/// - macOS: Keychain Services (Security.framework)
/// - Linux: libsecret / Secret Service API
class SecureStorage
{
public:
    /// Service name used for all OrcaSlicer credentials in keyring.
    static constexpr const char* SERVICE_NAME = "OrcaSlicer";

    /// Store a secret in the OS keyring.
    /// @param account Unique identifier for this secret (e.g., "UltiMaker_RefreshToken")
    /// @param secret The secret value to store
    /// @return true on success, false on failure
    static bool store(const std::string& account, const std::string& secret);

    /// Retrieve a secret from the OS keyring.
    /// @param account Unique identifier for the secret
    /// @return The secret value, or empty optional if not found/error
    static std::optional<std::string> retrieve(const std::string& account);

    /// Delete a secret from the OS keyring.
    /// @param account Unique identifier for the secret
    /// @return true on success (including if secret didn't exist), false on failure
    static bool remove(const std::string& account);

    /// Check if secure storage is available on this platform.
    /// @return true if keyring/keychain is accessible
    static bool is_available();

private:
    /// Build the target name for Windows Credential Manager.
    /// Format: "OrcaSlicer:AccountName"
    static std::string build_target_name(const std::string& account);
};

} // namespace Slic3r

#endif // slic3r_SecureStorage_hpp_
