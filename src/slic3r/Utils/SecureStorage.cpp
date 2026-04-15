#include "SecureStorage.hpp"

#include <boost/log/trivial.hpp>
#include <vector>

// Platform-specific includes
#ifdef _WIN32
    #include <windows.h>
    #include <wincred.h>
#elif defined(__APPLE__)
    #include <Security/Security.h>
    #include <CoreFoundation/CoreFoundation.h>
#elif defined(SLIC3R_HAS_LIBSECRET)
    #include <libsecret/secret.h>
#endif

namespace Slic3r {

std::string SecureStorage::build_target_name(const std::string& account)
{
    return std::string(SERVICE_NAME) + ":" + account;
}

// ============================================================================
// Windows Implementation - Windows Credential Manager
// ============================================================================
#ifdef _WIN32

bool SecureStorage::store(const std::string& account, const std::string& secret)
{
    if (secret.empty()) {
        BOOST_LOG_TRIVIAL(warning) << "SecureStorage: Attempted to store empty secret for " << account;
        return false;
    }

    std::string target_name = build_target_name(account);
    
    // Windows Credential Manager uses UTF-16 for target name
    int target_name_len = MultiByteToWideChar(CP_UTF8, 0, target_name.c_str(), -1, nullptr, 0);
    std::wstring target_name_w(target_name_len, 0);
    MultiByteToWideChar(CP_UTF8, 0, target_name.c_str(), -1, &target_name_w[0], target_name_len);

    // Copy secret to a mutable buffer (CredWrite requires non-const pointer)
    std::vector<BYTE> secret_buffer(secret.begin(), secret.end());

    CREDENTIALW cred = {};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = const_cast<LPWSTR>(target_name_w.c_str());
    cred.CredentialBlobSize = static_cast<DWORD>(secret_buffer.size());
    cred.CredentialBlob = secret_buffer.data();
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;  // User-specific, stays on this machine only
    cred.UserName = const_cast<LPWSTR>(target_name_w.c_str());  // Use target name as username

    BOOL result = CredWriteW(&cred, 0);
    if (!result) {
        DWORD error = GetLastError();
        BOOST_LOG_TRIVIAL(error) << "SecureStorage: CredWrite failed for " << account 
                                  << ", error=" << error;
        return false;
    }

    BOOST_LOG_TRIVIAL(debug) << "SecureStorage: Stored credential for " << account;
    return true;
}

std::optional<std::string> SecureStorage::retrieve(const std::string& account)
{
    std::string target_name = build_target_name(account);
    
    // Windows Credential Manager uses UTF-16 for target name
    int target_name_len = MultiByteToWideChar(CP_UTF8, 0, target_name.c_str(), -1, nullptr, 0);
    std::wstring target_name_w(target_name_len, 0);
    MultiByteToWideChar(CP_UTF8, 0, target_name.c_str(), -1, &target_name_w[0], target_name_len);

    PCREDENTIALW cred = nullptr;
    BOOL result = CredReadW(target_name_w.c_str(), CRED_TYPE_GENERIC, 0, &cred);
    
    if (!result) {
        DWORD error = GetLastError();
        if (error != ERROR_NOT_FOUND) {
            BOOST_LOG_TRIVIAL(error) << "SecureStorage: CredRead failed for " << account 
                                      << ", error=" << error;
        }
        return std::nullopt;
    }

    std::string secret(reinterpret_cast<const char*>(cred->CredentialBlob), 
                        cred->CredentialBlobSize);
    CredFree(cred);
    
    BOOST_LOG_TRIVIAL(debug) << "SecureStorage: Retrieved credential for " << account;
    return secret;
}

bool SecureStorage::remove(const std::string& account)
{
    std::string target_name = build_target_name(account);
    
    // Windows Credential Manager uses UTF-16 for target name
    int target_name_len = MultiByteToWideChar(CP_UTF8, 0, target_name.c_str(), -1, nullptr, 0);
    std::wstring target_name_w(target_name_len, 0);
    MultiByteToWideChar(CP_UTF8, 0, target_name.c_str(), -1, &target_name_w[0], target_name_len);

    BOOL result = CredDeleteW(target_name_w.c_str(), CRED_TYPE_GENERIC, 0);
    if (!result) {
        DWORD error = GetLastError();
        if (error == ERROR_NOT_FOUND) {
            // Not found is success for delete
            BOOST_LOG_TRIVIAL(debug) << "SecureStorage: Credential not found for " << account;
            return true;
        }
        BOOST_LOG_TRIVIAL(error) << "SecureStorage: CredDelete failed for " << account 
                                  << ", error=" << error;
        return false;
    }

    BOOST_LOG_TRIVIAL(debug) << "SecureStorage: Removed credential for " << account;
    return true;
}

bool SecureStorage::is_available()
{
    // Windows Credential Manager is always available on Windows 2000+
    return true;
}

// ============================================================================
// macOS Implementation - Keychain Services
// ============================================================================
#elif defined(__APPLE__)

bool SecureStorage::store(const std::string& account, const std::string& secret)
{
    if (secret.empty()) {
        BOOST_LOG_TRIVIAL(warning) << "SecureStorage: Attempted to store empty secret for " << account;
        return false;
    }

    std::string service = SERVICE_NAME;
    
    // First, try to delete any existing item (to handle updates cleanly)
    // SecItemAdd returns errSecDuplicateItem if item exists
    remove(account);
    
    CFStringRef service_ref = CFStringCreateWithCString(kCFAllocatorDefault, 
                                                         service.c_str(), 
                                                         kCFStringEncodingUTF8);
    CFStringRef account_ref = CFStringCreateWithCString(kCFAllocatorDefault, 
                                                         account.c_str(), 
                                                         kCFStringEncodingUTF8);
    CFDataRef secret_ref = CFDataCreate(kCFAllocatorDefault, 
                                         reinterpret_cast<const UInt8*>(secret.data()), 
                                         secret.size());
    
    if (!service_ref || !account_ref || !secret_ref) {
        BOOST_LOG_TRIVIAL(error) << "SecureStorage: Failed to create CF strings for " << account;
        if (service_ref) CFRelease(service_ref);
        if (account_ref) CFRelease(account_ref);
        if (secret_ref) CFRelease(secret_ref);
        return false;
    }

    // Create query dictionary for adding
    const void* keys[] = {
        kSecClass,
        kSecAttrService,
        kSecAttrAccount,
        kSecValueData
    };
    const void* values[] = {
        kSecClassGenericPassword,
        service_ref,
        account_ref,
        secret_ref
    };
    
    CFDictionaryRef query = CFDictionaryCreate(kCFAllocatorDefault,
                                                 keys, values,
                                                 4,  // number of key-value pairs
                                                 &kCFTypeDictionaryKeyCallBacks,
                                                 &kCFTypeDictionaryValueCallBacks);
    
    OSStatus status = SecItemAdd(query, nullptr);
    
    CFRelease(query);
    CFRelease(service_ref);
    CFRelease(account_ref);
    CFRelease(secret_ref);
    
    if (status != errSecSuccess) {
        BOOST_LOG_TRIVIAL(error) << "SecureStorage: SecItemAdd failed for " << account 
                                  << ", status=" << status;
        return false;
    }

    BOOST_LOG_TRIVIAL(debug) << "SecureStorage: Stored credential for " << account;
    return true;
}

std::optional<std::string> SecureStorage::retrieve(const std::string& account)
{
    std::string service = SERVICE_NAME;
    
    CFStringRef service_ref = CFStringCreateWithCString(kCFAllocatorDefault, 
                                                         service.c_str(), 
                                                         kCFStringEncodingUTF8);
    CFStringRef account_ref = CFStringCreateWithCString(kCFAllocatorDefault, 
                                                         account.c_str(), 
                                                         kCFStringEncodingUTF8);
    
    if (!service_ref || !account_ref) {
        BOOST_LOG_TRIVIAL(error) << "SecureStorage: Failed to create CF strings for " << account;
        if (service_ref) CFRelease(service_ref);
        if (account_ref) CFRelease(account_ref);
        return std::nullopt;
    }

    // Create query dictionary
    const void* keys[] = {
        kSecClass,
        kSecAttrService,
        kSecAttrAccount,
        kSecReturnData,
        kSecMatchLimit
    };
    const void* values[] = {
        kSecClassGenericPassword,
        service_ref,
        account_ref,
        kCFBooleanTrue,
        kSecMatchLimitOne
    };
    
    CFDictionaryRef query = CFDictionaryCreate(kCFAllocatorDefault,
                                                 keys, values,
                                                 5,  // number of key-value pairs
                                                 &kCFTypeDictionaryKeyCallBacks,
                                                 &kCFTypeDictionaryValueCallBacks);
    
    CFDataRef result = nullptr;
    OSStatus status = SecItemCopyMatching(query, reinterpret_cast<CFTypeRef*>(&result));
    
    CFRelease(query);
    CFRelease(service_ref);
    CFRelease(account_ref);
    
    if (status != errSecSuccess) {
        if (status != errSecItemNotFound) {
            BOOST_LOG_TRIVIAL(error) << "SecureStorage: SecItemCopyMatching failed for " << account 
                                      << ", status=" << status;
        }
        return std::nullopt;
    }

    std::string secret(reinterpret_cast<const char*>(CFDataGetBytePtr(result)), 
                        CFDataGetLength(result));
    CFRelease(result);
    
    BOOST_LOG_TRIVIAL(debug) << "SecureStorage: Retrieved credential for " << account;
    return secret;
}

bool SecureStorage::remove(const std::string& account)
{
    std::string service = SERVICE_NAME;
    
    CFStringRef service_ref = CFStringCreateWithCString(kCFAllocatorDefault, 
                                                         service.c_str(), 
                                                         kCFStringEncodingUTF8);
    CFStringRef account_ref = CFStringCreateWithCString(kCFAllocatorDefault, 
                                                         account.c_str(), 
                                                         kCFStringEncodingUTF8);
    
    if (!service_ref || !account_ref) {
        BOOST_LOG_TRIVIAL(error) << "SecureStorage: Failed to create CF strings for " << account;
        if (service_ref) CFRelease(service_ref);
        if (account_ref) CFRelease(account_ref);
        return false;
    }

    // Create query dictionary
    const void* keys[] = {
        kSecClass,
        kSecAttrService,
        kSecAttrAccount
    };
    const void* values[] = {
        kSecClassGenericPassword,
        service_ref,
        account_ref
    };
    
    CFDictionaryRef query = CFDictionaryCreate(kCFAllocatorDefault,
                                                 keys, values,
                                                 3,  // number of key-value pairs
                                                 &kCFTypeDictionaryKeyCallBacks,
                                                 &kCFTypeDictionaryValueCallBacks);
    
    OSStatus status = SecItemDelete(query);
    
    CFRelease(query);
    CFRelease(service_ref);
    CFRelease(account_ref);
    
    if (status != errSecSuccess && status != errSecItemNotFound) {
        BOOST_LOG_TRIVIAL(error) << "SecureStorage: SecItemDelete failed for " << account 
                                  << ", status=" << status;
        return false;
    }

    BOOST_LOG_TRIVIAL(debug) << "SecureStorage: Removed credential for " << account;
    return true;
}

bool SecureStorage::is_available()
{
    // Keychain is always available on macOS
    return true;
}

// ============================================================================
// Linux Implementation - libsecret
// ============================================================================
#elif defined(SLIC3R_HAS_LIBSECRET)

// Schema definition for libsecret
static const SecretSchema* get_secret_schema()
{
    static SecretSchema schema = {
        "org.orcaslicer credential",
        SECRET_SCHEMA_NONE,
        {
            { "service", SECRET_SCHEMA_ATTRIBUTE_STRING },
            { "account", SECRET_SCHEMA_ATTRIBUTE_STRING },
            { nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING }
        }
    };
    return &schema;
}

bool SecureStorage::store(const std::string& account, const std::string& secret)
{
    if (secret.empty()) {
        BOOST_LOG_TRIVIAL(warning) << "SecureStorage: Attempted to store empty secret for " << account;
        return false;
    }

    GError* error = nullptr;
    
    gboolean result = secret_password_store_sync(
        get_secret_schema(),
        SECRET_COLLECTION_DEFAULT,  // Use default collection
        build_target_name(account).c_str(),  // Display label
        secret.c_str(),
        nullptr,  // Cancellable
        &error,
        "service", SERVICE_NAME,
        "account", account.c_str(),
        nullptr
    );
    
    if (!result) {
        if (error) {
            BOOST_LOG_TRIVIAL(error) << "SecureStorage: secret_password_store failed for " << account 
                                      << ", error=" << error->message;
            g_error_free(error);
        } else {
            BOOST_LOG_TRIVIAL(error) << "SecureStorage: secret_password_store failed for " << account;
        }
        return false;
    }

    BOOST_LOG_TRIVIAL(debug) << "SecureStorage: Stored credential for " << account;
    return true;
}

std::optional<std::string> SecureStorage::retrieve(const std::string& account)
{
    GError* error = nullptr;
    
    gchar* secret = secret_password_lookup_sync(
        get_secret_schema(),
        nullptr,  // Cancellable
        &error,
        "service", SERVICE_NAME,
        "account", account.c_str(),
        nullptr
    );
    
    if (error) {
        BOOST_LOG_TRIVIAL(error) << "SecureStorage: secret_password_lookup failed for " << account 
                                  << ", error=" << error->message;
        g_error_free(error);
        return std::nullopt;
    }
    
    if (!secret) {
        // Not found is not an error
        return std::nullopt;
    }
    
    std::string result(secret);
    secret_password_free(secret);
    
    BOOST_LOG_TRIVIAL(debug) << "SecureStorage: Retrieved credential for " << account;
    return result;
}

bool SecureStorage::remove(const std::string& account)
{
    GError* error = nullptr;
    
    gboolean result = secret_password_clear_sync(
        get_secret_schema(),
        nullptr,  // Cancellable
        &error,
        "service", SERVICE_NAME,
        "account", account.c_str(),
        nullptr
    );
    
    if (error) {
        BOOST_LOG_TRIVIAL(error) << "SecureStorage: secret_password_clear failed for " << account 
                                  << ", error=" << error->message;
        g_error_free(error);
        return false;
    }
    
    BOOST_LOG_TRIVIAL(debug) << "SecureStorage: Removed credential for " << account;
    return true;
}

bool SecureStorage::is_available()
{
    // Check if Secret Service is available (GNOME Keyring, KWallet, etc.)
    // We do this by trying to get a secret service proxy
    GError* error = nullptr;
    
    SecretService* service = secret_service_get_sync(
        SECRET_SERVICE_LOAD_COLLECTIONS | SECRET_SERVICE_OPEN_SESSION,
        nullptr,  // Cancellable
        &error
    );
    
    if (error) {
        BOOST_LOG_TRIVIAL(warning) << "SecureStorage: Secret service not available: " << error->message;
        g_error_free(error);
        return false;
    }
    
    bool result = (service != nullptr);
    if (service) {
        g_object_unref(service);
    }
    
    return result;
}

#else

bool SecureStorage::store(const std::string& account, const std::string& secret)
{
    (void)secret;
    BOOST_LOG_TRIVIAL(warning)
        << "SecureStorage: No supported secure backend compiled in, cannot store credential for " << account;
    return false;
}

std::optional<std::string> SecureStorage::retrieve(const std::string& account)
{
    BOOST_LOG_TRIVIAL(warning)
        << "SecureStorage: No supported secure backend compiled in, cannot retrieve credential for " << account;
    return std::nullopt;
}

bool SecureStorage::remove(const std::string& account)
{
    BOOST_LOG_TRIVIAL(warning)
        << "SecureStorage: No supported secure backend compiled in, cannot remove credential for " << account;
    return false;
}

bool SecureStorage::is_available()
{
    return false;
}

#endif // Platform selection

} // namespace Slic3r
