#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../GumroadLicenseManager.h"
#include "../FontManager.h"

//==============================================================================
// Gumroad License Dialog UI Component
// Allows users to enter and validate Gumroad license keys
//==============================================================================
class GumroadLicenseDialog
{
public:
    using DismissedCallback = std::function<void()>;
    
    static void showDialog(juce::Component* parentComponent, GumroadLicenseManager* licenseManager, DismissedCallback onDismissed = nullptr)
    {
        if (licenseManager == nullptr)
            return;
        
        auto* content = new LicenseDialogContent(*licenseManager, onDismissed);
        
        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(content);
        options.dialogTitle = "Enter License Key";
        options.dialogBackgroundColour = juce::Colour(0xFF2B2D31);
        // Allow closing the dialog (it will reappear if license is invalid)
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = false;
        options.resizable = false;
        options.useBottomRightCornerResizer = false;
        // Make sure dialog can receive keyboard focus
        options.dialogBackgroundColour = juce::Colour(0xFF2B2D31);
        
        if (parentComponent != nullptr)
            options.componentToCentreAround = parentComponent;
        
        // Launch dialog asynchronously
        options.launchAsync();
    }
    
private:
    class LicenseDialogContent : public juce::Component,
                                 public juce::Timer
    {
    public:
        LicenseDialogContent(GumroadLicenseManager& manager, DismissedCallback onDismissed = nullptr) 
            : licenseManager(manager), dismissedCallback(onDismissed)
        {
            setSize(550, 400);
            
            // Make sure this component can receive focus and keyboard input
            setWantsKeyboardFocus(true);
            setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
            
            // Get current license info
            currentInfo = licenseManager.getCurrentLicense();
            
            // Title label
            titleLabel.setText("Stepper License", juce::dontSendNotification);
            titleLabel.setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 24.0f, juce::Font::bold));
            titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
            titleLabel.setJustificationType(juce::Justification::centred);
            addAndMakeVisible(titleLabel);
            
            // Instructions label
            instructionsLabel.setText("Enter your Gumroad license key:", juce::dontSendNotification);
            instructionsLabel.setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 14.0f, juce::Font::plain));
            instructionsLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.8f));
            instructionsLabel.setJustificationType(juce::Justification::centred);
            addAndMakeVisible(instructionsLabel);
            
            // Format hint label
            formatHintLabel.setText("License keys are provided after purchase on Gumroad. Sample keys will not work.", juce::dontSendNotification);
            formatHintLabel.setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 12.0f, juce::Font::italic));
            formatHintLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.6f));
            formatHintLabel.setJustificationType(juce::Justification::centred);
            addAndMakeVisible(formatHintLabel);
            
            // License key text editor
            licenseKeyEditor.setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 16.0f, juce::Font::plain));
            licenseKeyEditor.setMultiLine(false);
            licenseKeyEditor.setReturnKeyStartsNewLine(false);
            licenseKeyEditor.setText(currentInfo.licenseKey, juce::dontSendNotification);
            licenseKeyEditor.setSelectAllWhenFocused(true);
            licenseKeyEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
            licenseKeyEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF1E1F22));
            licenseKeyEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xFF101113));
            licenseKeyEditor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xFF4A9EFF));
            licenseKeyEditor.onReturnKey = [this] { validateAndClose(); };
            addAndMakeVisible(licenseKeyEditor);
            
            // Give focus to the text editor
            licenseKeyEditor.grabKeyboardFocus();
            
            // Status label
            statusLabel.setFont(FontManager::getInstance().getFont("AlteHaasGroteskBold", 13.0f, juce::Font::plain));
            statusLabel.setJustificationType(juce::Justification::centred);
            statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
            updateStatusLabel();
            addAndMakeVisible(statusLabel);
            
            // OK button
            okButton.setButtonText("Verify & Close");
            okButton.onClick = [this] { validateAndClose(); };
            addAndMakeVisible(okButton);
            
            // Clear button
            clearButton.setButtonText("Clear License");
            clearButton.onClick = [this] { clearLicense(); };
            addAndMakeVisible(clearButton);
            
            // Cancel button - allow closing (dialog will reappear if license is invalid)
            cancelButton.setButtonText("Cancel");
            cancelButton.onClick = [this] {
                if (dismissedCallback)
                    dismissedCallback();
                if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                    dw->exitModalState(0);
            };
            addAndMakeVisible(cancelButton);
            
            // Start timer to update status while verifying
            startTimer(100);
        }
        
        ~LicenseDialogContent() override
        {
            stopTimer();
        }
        
        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour(0xFF2B2D31));
            
            // Draw border
            g.setColour(juce::Colour(0xFF101113));
            g.drawRect(getLocalBounds(), 1);
        }
        
        void visibilityChanged() override
        {
            // When dialog becomes visible, give focus to text editor
            if (isShowing())
            {
                licenseKeyEditor.grabKeyboardFocus();
            }
        }
        
        void resized() override
        {
            auto bounds = getLocalBounds().reduced(30);
            
            titleLabel.setBounds(bounds.removeFromTop(40));
            bounds.removeFromTop(10);
            
            instructionsLabel.setBounds(bounds.removeFromTop(25));
            bounds.removeFromTop(5);
            
            formatHintLabel.setBounds(bounds.removeFromTop(20));
            bounds.removeFromTop(20);
            
            licenseKeyEditor.setBounds(bounds.removeFromTop(40));
            bounds.removeFromTop(15);
            
            statusLabel.setBounds(bounds.removeFromTop(40));
            bounds.removeFromTop(30);
            
            auto buttonRow = bounds.removeFromTop(35);
            auto buttonWidth = 120;
            auto spacing = 10;
            
            okButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
            buttonRow.removeFromLeft(spacing);
            clearButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
            buttonRow.removeFromLeft(spacing);
            cancelButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
        }
        
        void timerCallback() override
        {
            // Update status while verifying
            if (licenseManager.isVerificationInProgress())
            {
                statusLabel.setText("Verifying license...", juce::dontSendNotification);
                statusLabel.setColour(juce::Label::textColourId, juce::Colours::yellow);
                repaint();
            }
            else
            {
                // Update with current license info
                currentInfo = licenseManager.getCurrentLicense();
                updateStatusLabel();
            }
        }
        
    private:
        GumroadLicenseManager& licenseManager;
        GumroadLicenseInfo currentInfo;
        DismissedCallback dismissedCallback;
        
        juce::Label titleLabel;
        juce::Label instructionsLabel;
        juce::Label formatHintLabel;
        juce::TextEditor licenseKeyEditor;
        juce::Label statusLabel;
        juce::TextButton okButton;
        juce::TextButton clearButton;
        juce::TextButton cancelButton;
        
        void validateAndClose()
        {
            auto key = licenseKeyEditor.getText().trim();
            if (key.isEmpty())
            {
                statusLabel.setText("Please enter a license key", juce::dontSendNotification);
                statusLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
                return;
            }
            
            // Verify license (asynchronous)
            auto safeThis = juce::Component::SafePointer<LicenseDialogContent>(this);
            licenseManager.verifyLicenseAsync(key, false, [safeThis](const GumroadLicenseInfo& info) {
                if (safeThis == nullptr)
                    return; // Fix: dialog may have been closed during verification
                
                auto* self = safeThis.getComponent();
                self->currentInfo = info;
                
                if (info.isValid())
                {
                    self->statusLabel.setText("License verified successfully!", juce::dontSendNotification);
                    self->statusLabel.setColour(juce::Label::textColourId, juce::Colours::green);
                    
                    // Close after a brief delay
                    juce::Timer::callAfterDelay(1000, [safeThis] {
                        if (auto* selfPtr = safeThis.getComponent())
                            if (auto* dw = selfPtr->findParentComponentOfClass<juce::DialogWindow>())
                                dw->exitModalState(1);
                    });
                }
                else if (info.status == GumroadLicenseStatus::Refunded)
                {
                    self->statusLabel.setText("License was refunded", juce::dontSendNotification);
                    self->statusLabel.setColour(juce::Label::textColourId, juce::Colours::red);
                }
                else if (info.status == GumroadLicenseStatus::Disputed)
                {
                    self->statusLabel.setText("License is disputed", juce::dontSendNotification);
                    self->statusLabel.setColour(juce::Label::textColourId, juce::Colours::red);
                }
                else if (info.status == GumroadLicenseStatus::NetworkError)
                {
                    self->statusLabel.setText("Network error - please check your connection", juce::dontSendNotification);
                    self->statusLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
                }
                else if (info.status == GumroadLicenseStatus::VerificationFailed)
                {
                    juce::String errorMsg = "Verification failed";
                    if (!info.email.isEmpty())
                        errorMsg = info.email;
                    self->statusLabel.setText(errorMsg, juce::dontSendNotification);
                    self->statusLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
                }
                else
                {
                    juce::String errorMsg = "Invalid license key";
                    if (!info.email.isEmpty() && info.status == GumroadLicenseStatus::Invalid)
                        errorMsg = info.email;
                    self->statusLabel.setText(errorMsg, juce::dontSendNotification);
                    self->statusLabel.setColour(juce::Label::textColourId, juce::Colours::red);
                }
                
                self->updateStatusLabel();
                self->repaint();
            });
            
            // Show verifying status immediately
            statusLabel.setText("Verifying license...", juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, juce::Colours::yellow);
            repaint();
        }
        
        void clearLicense()
        {
            licenseManager.clearLicense();
            licenseKeyEditor.clear();
            currentInfo = GumroadLicenseInfo();
            updateStatusLabel();
            statusLabel.setText("License cleared", juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        }
        
        void updateStatusLabel()
        {
            if (currentInfo.isValid())
            {
                juce::String statusText = "License valid";
                if (currentInfo.usesCount > 0)
                    statusText << " (Uses: " << currentInfo.usesCount << ")";
                if (!currentInfo.email.isEmpty())
                    statusText << " - " << currentInfo.email;
                    
                statusLabel.setText(statusText, juce::dontSendNotification);
                statusLabel.setColour(juce::Label::textColourId, juce::Colours::green);
            }
            else if (currentInfo.status == GumroadLicenseStatus::Refunded)
            {
                statusLabel.setText("License was refunded", juce::dontSendNotification);
                statusLabel.setColour(juce::Label::textColourId, juce::Colours::red);
            }
            else if (currentInfo.status == GumroadLicenseStatus::Disputed)
            {
                statusLabel.setText("License is disputed", juce::dontSendNotification);
                statusLabel.setColour(juce::Label::textColourId, juce::Colours::red);
            }
            else if (currentInfo.licenseKey.isEmpty())
            {
                statusLabel.setText("No license key entered", juce::dontSendNotification);
                statusLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.6f));
            }
        }
    };
};

