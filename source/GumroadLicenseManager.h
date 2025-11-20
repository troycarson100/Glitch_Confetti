#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <atomic>
#include <functional>

//==============================================================================
// Gumroad License Status
//==============================================================================
enum class GumroadLicenseStatus
{
    NotVerified,        // License hasn't been verified yet
    Valid,              // License is valid
    Invalid,            // License key is invalid
    Refunded,           // Purchase was refunded
    Disputed,           // Purchase is disputed
    NetworkError,       // Could not connect to Gumroad API
    VerificationFailed  // Verification failed for other reason
};

//==============================================================================
// Gumroad License Info
//==============================================================================
struct GumroadLicenseInfo
{
    GumroadLicenseStatus status = GumroadLicenseStatus::NotVerified;
    juce::String licenseKey;
    juce::String productId;
    juce::String email;
    int usesCount = 0;
    bool isMultiseatLicense = false;
    bool refunded = false;
    bool disputed = false;
    juce::Time lastVerified;
    
    bool isValid() const 
    { 
        return status == GumroadLicenseStatus::Valid && !refunded && !disputed; 
    }
};

//==============================================================================
// Gumroad License Manager
// Handles license verification through Gumroad API
//==============================================================================
class GumroadLicenseManager : public juce::Thread
{
public:
    // Callback function type for license verification completion
    using VerificationCallback = std::function<void(const GumroadLicenseInfo&)>;
    
    GumroadLicenseManager(const juce::String& productId);
    ~GumroadLicenseManager() override;
    
    // Verify a license key (asynchronous)
    // Callback will be called on the message thread when verification completes
    void verifyLicenseAsync(const juce::String& licenseKey, 
                           bool incrementUses = false,
                           VerificationCallback callback = nullptr);
    
    // Get current license info (thread-safe)
    GumroadLicenseInfo getCurrentLicense() const;
    
    // Check if verification is currently in progress
    bool isVerificationInProgress() const { return verificationInProgress.load(); }
    
    // Check if current license is valid
    bool isLicenseValid() const;
    
    // Clear license
    void clearLicense();
    
    // Save license state to persistent storage
    void saveLicenseState();
    
    // Load license state from persistent storage
    void loadLicenseState();
    
    // Set product ID (your Gumroad product ID)
    void setProductId(const juce::String& productId) { this->productId = productId; }
    
    // Get product ID
    juce::String getProductId() const { return productId; }
    
private:
    // Thread implementation for async HTTP requests
    void run() override;
    
    // Perform actual verification (called on background thread)
    GumroadLicenseInfo performVerification(const juce::String& licenseKey, bool incrementUses);
    
    // Parse JSON response from Gumroad API
    GumroadLicenseInfo parseVerificationResponse(const juce::String& jsonResponse, 
                                                  const juce::String& licenseKey);
    
    // Properties file helpers
    std::unique_ptr<juce::PropertiesFile> getLicensePropertiesFile();
    
    juce::String productId;
    GumroadLicenseInfo currentLicense;
    std::atomic<bool> verificationInProgress { false };
    
    // Queue for async verification requests
    struct VerificationRequest
    {
        juce::String licenseKey;
        bool incrementUses;
        VerificationCallback callback;
        
        VerificationRequest() : incrementUses(false) {}
    };
    
    mutable juce::CriticalSection requestQueueLock; // Mutable for const methods
    juce::Array<VerificationRequest> requestQueue;
};

