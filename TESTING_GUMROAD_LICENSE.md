# Testing Gumroad License Integration

## Quick Test Steps

### 1. Build and Run the Plugin
```bash
# Build the plugin in your IDE (Xcode/Visual Studio/etc.)
# Then run the Standalone version or load in your DAW
```

### 2. Test Opening the License Dialog
- **Mac**: Press `Cmd + L`
- **Windows/Linux**: Press `Ctrl + L`
- The dialog should appear with a license key input field

### 3. Test Scenarios

#### Test 1: Invalid License Key
1. Open the license dialog (`Cmd+L` / `Ctrl+L`)
2. Enter an invalid license key (e.g., "test123" or "invalid-key")
3. Click "Verify & Close" or press Enter
4. **Expected**: Status should show "Invalid license key" in red

#### Test 2: Empty License Key
1. Open the license dialog
2. Leave the field empty
3. Click "Verify & Close"
4. **Expected**: Status should show "Please enter a license key" in orange

#### Test 3: Network Error (Optional)
1. Disconnect from internet (or use firewall to block network access)
2. Open the license dialog
3. Enter any license key
4. Click "Verify & Close"
5. **Expected**: Status should show "Network error - please check your connection" in orange

#### Test 4: Valid License Key (If Available)
To get a real license key for testing:
- **Option A**: If you have a test product in Gumroad, purchase it yourself to get a license key
- **Option B**: Make a real sale to your Gumroad product, then use that license key
- **Option C**: Use Gumroad's sandbox/test mode if available

1. Open the license dialog
2. Enter a valid license key from Gumroad
3. Click "Verify & Close"
4. **Expected**: Status should show "License verified successfully!" in green, and dialog should auto-close after 1 second

#### Test 5: Startup Check
1. Make sure you have **no valid license** stored (or clear it first)
2. Launch the plugin
3. **Expected**: License dialog should automatically appear after ~1.5 seconds

#### Test 6: Clear License
1. Open the license dialog
2. If you have a license stored, click "Clear License"
3. **Expected**: License field should clear, status should show "License cleared"

#### Test 7: Persistent Storage
1. Enter and verify a valid license key
2. Close and restart the plugin
3. **Expected**: License should still be valid (no dialog on startup)

## Debug Output

To see debug messages, check your console/log output for messages like:
```
[Gumroad] Verifying license key: YOUR_KEY for product: YJv8qP_umZv8fuNaDD5dQg==
[Gumroad] License verified successfully. Uses: X
[Gumroad] Valid license found on startup: email@example.com
```

## Testing with a Real License Key

If you want to test with an actual license key:

1. **Create a Test Sale** (if you haven't already):
   - Go to your Gumroad product page
   - Use the "Test Purchase" feature or make a $0 sale to yourself
   - After the sale, Gumroad will generate a license key
   - Copy that license key and use it in the plugin

2. **Enable License Keys** (if not already):
   - Go to your Gumroad product edit page
   - Make sure "Generate a unique license key per sale" is enabled
   - Save the product

## Common Issues

- **"License manager not available"**: Product ID might not be set correctly. Check `PluginProcessor.cpp` line ~55.
- **Dialog doesn't appear**: Make sure the plugin has keyboard focus when pressing `Cmd+L` / `Ctrl+L`
- **Network timeouts**: If you see network errors, check your internet connection and firewall settings
- **Product ID mismatch**: Make sure the product ID in code matches your Gumroad product ID

## Next Steps After Testing

Once everything works:
1. ✅ Make sure license keys are enabled in Gumroad product settings
2. ✅ Share your plugin with beta testers
3. ✅ They'll receive license keys after purchase on Gumroad
4. ✅ They can enter their keys using `Cmd+L` / `Ctrl+L`



