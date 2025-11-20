#include "GumroadLicenseManager.h"
#include <juce_core/juce_core.h>

//==============================================================================
// Gumroad License Manager Implementation
//==============================================================================

GumroadLicenseManager::GumroadLicenseManager(const juce::String& productId)
    : juce::Thread("GumroadLicenseVerifier"), productId(productId)
{
    startThread();
    loadLicenseState();
}

GumroadLicenseManager::~GumroadLicenseManager()
{
    signalThreadShouldExit();
    notify();
    waitForThreadToExit(5000);
}

void GumroadLicenseManager::verifyLicenseAsync(const juce::String& licenseKey, 
                                                bool incrementUses,
                                                VerificationCallback callback)
{
    if (licenseKey.trim().isEmpty())
        return;
    
    VerificationRequest request;
    request.licenseKey = licenseKey.trim().toUpperCase();
    request.incrementUses = incrementUses;
    request.callback = callback;
    
    {
        juce::ScopedLock lock(requestQueueLock);
        requestQueue.add(request);
    }
    
    notify(); // Wake up the thread
}

void GumroadLicenseManager::run()
{
    while (!threadShouldExit())
    {
        VerificationRequest request;
        bool hasRequest = false;
        
        // Get next request from queue
        {
            juce::ScopedLock lock(requestQueueLock);
            if (requestQueue.size() > 0)
            {
                request = requestQueue.removeAndReturn(0);
                hasRequest = true;
                verificationInProgress.store(true);
            }
        }
        
        if (hasRequest)
        {
            // Perform verification on background thread
            auto result = performVerification(request.licenseKey, request.incrementUses);
            
            // Update current license
            {
                juce::ScopedLock lock(requestQueueLock);
                currentLicense = result;
            }
            saveLicenseState();
            
            // Call callback on message thread if provided
            if (request.callback)
            {
                juce::MessageManager::callAsync([callback = request.callback, result]() {
                    callback(result);
                });
            }
            
            verificationInProgress.store(false);
        }
        else
        {
            // Wait for requests
            wait(500); // Check every 500ms
        }
    }
}

GumroadLicenseInfo GumroadLicenseManager::performVerification(const juce::String& licenseKey, bool incrementUses)
{
    GumroadLicenseInfo info;
    info.licenseKey = licenseKey;
    info.productId = productId;
    info.status = GumroadLicenseStatus::NetworkError;
    
    if (productId.isEmpty())
    {
        DBG("[Gumroad] Product ID not set!");
        info.status = GumroadLicenseStatus::VerificationFailed;
        return info;
    }
    
    // Prepare POST data as URL-encoded string
    juce::URL url("https://api.gumroad.com/v2/licenses/verify");
    juce::String postData;
    postData << "product_id=" << juce::URL::addEscapeChars(productId, true);
    postData << "&license_key=" << juce::URL::addEscapeChars(licenseKey, true);
    postData << "&increment_uses_count=" << (incrementUses ? "true" : "false");
    
    // Make HTTP POST request
    juce::String response;
    int statusCode = 0;
    
    DBG("[Gumroad] Verifying license key: " << licenseKey.substring(0, 8) << "..." << " for product: " << productId);
    DBG("[Gumroad] POST data: " << postData);
    
    try
    {
        // Create input stream with status code tracking (Gumroad returns 404 on failure)
        auto inputStream = url.withPOSTData(postData).createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withStatusCode(&statusCode)
                .withConnectionTimeoutMs(10000)
        );
        
        if (inputStream == nullptr)
        {
            DBG("[Gumroad] Failed to create input stream - network error or invalid URL");
            if (statusCode != 0)
            {
                DBG("[Gumroad] HTTP Status Code: " << statusCode);
                // Even if stream is null, we might have a status code (e.g., 404)
                if (statusCode == 404)
                {
                    // Try to read error response from error stream
                    auto errorStream = url.withPOSTData(postData).createInputStream(
                        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                            .withStatusCode(&statusCode)
                            .withConnectionTimeoutMs(10000)
                    );
                    if (errorStream != nullptr)
                    {
                        response = errorStream->readEntireStreamAsString();
                        DBG("[Gumroad] Error response: " << response);
                    }
                }
            }
            info.status = GumroadLicenseStatus::NetworkError;
            return info;
        }
        
        response = inputStream->readEntireStreamAsString();
        DBG("[Gumroad] HTTP Status Code: " << statusCode);
        DBG("[Gumroad] Received response, length: " << response.length());
    }
    catch (const std::exception& e)
    {
        DBG("[Gumroad] Exception during HTTP request: " << e.what());
        info.status = GumroadLicenseStatus::NetworkError;
        return info;
    }
    catch (...)
    {
        DBG("[Gumroad] Unknown exception during HTTP request");
        info.status = GumroadLicenseStatus::NetworkError;
        return info;
    }
    
    // Check for 404 status (Gumroad returns 404 when license doesn't exist)
    if (statusCode == 404)
    {
        DBG("[Gumroad] License not found (404) - Response: " << response);
        // Parse the error message from response
        auto json = juce::JSON::parse(response);
        juce::String errorMsg = "That license does not exist for the provided product.";
        if (json.hasProperty("message"))
        {
            errorMsg = json.getProperty("message", errorMsg).toString();
        }
        info.email = errorMsg;
        info.status = GumroadLicenseStatus::Invalid;
        return info;
    }
    
    if (response.isEmpty())
    {
        DBG("[Gumroad] Empty response from API (Status: " << statusCode << ")");
        info.status = GumroadLicenseStatus::NetworkError;
        return info;
    }
    
    DBG("[Gumroad] API Response: " << response);
    
    // Parse response
    return parseVerificationResponse(response, licenseKey);
}

GumroadLicenseInfo GumroadLicenseManager::parseVerificationResponse(const juce::String& jsonResponse, 
                                                                      const juce::String& licenseKey)
{
    GumroadLicenseInfo info;
    info.licenseKey = licenseKey;
    info.productId = productId;
    info.lastVerified = juce::Time::getCurrentTime();
    
    // Parse JSON response
    auto json = juce::JSON::parse(jsonResponse);
    
    // Check if JSON parsing failed
    if (json.isVoid() || json.isUndefined())
    {
        DBG("[Gumroad] Failed to parse JSON response. Raw response: " << jsonResponse);
        info.email = "Failed to parse API response. Check console for details.";
        info.status = GumroadLicenseStatus::VerificationFailed;
        return info;
    }
    
    if (!json.hasProperty("success"))
    {
        DBG("[Gumroad] Invalid API response format. Response: " << jsonResponse);
        info.email = "Invalid API response format. Check console for details.";
        info.status = GumroadLicenseStatus::VerificationFailed;
        return info;
    }
    
    bool success = json.getProperty("success", false);
    
    if (!success)
    {
        juce::String message = json.getProperty("message", "Unknown error").toString();
        DBG("[Gumroad] License verification failed: " << message);
        DBG("[Gumroad] Full JSON response: " << jsonResponse);
        
        // Store error message in license info for display
        if (message.isEmpty())
            message = "License verification failed. Response: " + jsonResponse.substring(0, 100);
        info.email = message; // Temporarily use email field to store error message
        info.status = GumroadLicenseStatus::Invalid;
        return info;
    }
    
    // Parse purchase information
    if (json.hasProperty("purchase"))
    {
        auto purchaseVar = json.getProperty("purchase", juce::var());
        if (auto* purchaseObj = purchaseVar.getDynamicObject())
        {
            auto emailVar = purchaseObj->getProperty("email");
            info.email = emailVar.toString();
            
            auto refundedVar = purchaseObj->getProperty("refunded");
            info.refunded = refundedVar.isBool() ? static_cast<bool>(refundedVar) : false;
            
            auto disputedVar = purchaseObj->getProperty("disputed");
            info.disputed = disputedVar.isBool() ? static_cast<bool>(disputedVar) : false;
            
            auto multiseatVar = purchaseObj->getProperty("is_multiseat_license");
            info.isMultiseatLicense = multiseatVar.isBool() ? static_cast<bool>(multiseatVar) : false;
            
            // Check for refund or dispute
            if (info.refunded)
            {
                info.status = GumroadLicenseStatus::Refunded;
                DBG("[Gumroad] License was refunded");
                return info;
            }
            
            if (info.disputed)
            {
                info.status = GumroadLicenseStatus::Disputed;
                DBG("[Gumroad] License is disputed");
                return info;
            }
        }
    }
    
    // Get uses count
    if (json.hasProperty("uses"))
    {
        info.usesCount = static_cast<int>(json.getProperty("uses", 0));
    }
    
    // License is valid
    info.status = GumroadLicenseStatus::Valid;
    DBG("[Gumroad] License verified successfully. Uses: " << info.usesCount);
    
    return info;
}

GumroadLicenseInfo GumroadLicenseManager::getCurrentLicense() const
{
    juce::ScopedLock lock(requestQueueLock);
    return currentLicense;
}

bool GumroadLicenseManager::isLicenseValid() const
{
    juce::ScopedLock lock(requestQueueLock);
    return currentLicense.isValid();
}

void GumroadLicenseManager::clearLicense()
{
    currentLicense = GumroadLicenseInfo();
    saveLicenseState();
}

void GumroadLicenseManager::saveLicenseState()
{
    auto props = getLicensePropertiesFile();
    if (!props)
        return;
    
    props->setValue("licenseKey", currentLicense.licenseKey);
    props->setValue("productId", currentLicense.productId);
    props->setValue("status", static_cast<int>(currentLicense.status));
    props->setValue("email", currentLicense.email);
    props->setValue("usesCount", currentLicense.usesCount);
    props->setValue("refunded", currentLicense.refunded);
    props->setValue("disputed", currentLicense.disputed);
    // Store time as milliseconds since epoch
    props->setValue("lastVerifiedMs", static_cast<int>(currentLicense.lastVerified.toMilliseconds() % 2147483647LL)); // Store as int (safe for reasonable dates)
    props->saveIfNeeded();
}

void GumroadLicenseManager::loadLicenseState()
{
    auto props = getLicensePropertiesFile();
    if (!props)
        return;
    
    currentLicense.licenseKey = props->getValue("licenseKey", "");
    currentLicense.productId = props->getValue("productId", productId); // Use provided productId if no stored one
    currentLicense.status = static_cast<GumroadLicenseStatus>(props->getIntValue("status", 0));
    currentLicense.email = props->getValue("email", "");
    currentLicense.usesCount = props->getIntValue("usesCount", 0);
    currentLicense.refunded = props->getBoolValue("refunded", false);
    currentLicense.disputed = props->getBoolValue("disputed", false);
    // Load time as milliseconds since epoch
    auto lastVerifiedMs = static_cast<juce::int64>(props->getIntValue("lastVerifiedMs", 0));
    currentLicense.lastVerified = juce::Time(lastVerifiedMs);
    
    // Re-verify if we have a license key but haven't verified recently (optional - can remove for offline mode)
    // For now, we'll just load the cached state
}

std::unique_ptr<juce::PropertiesFile> GumroadLicenseManager::getLicensePropertiesFile()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "Stepper";
    options.folderName = "Stepper";
    options.filenameSuffix = "license";
    options.osxLibrarySubFolder = "Application Support";
    options.storageFormat = juce::PropertiesFile::StorageFormat::storeAsXML;
    options.millisecondsBeforeSaving = 1000;
    return std::make_unique<juce::PropertiesFile>(options);
}

