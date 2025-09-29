#!/bin/bash
# Force Audio Unit cache refresh
sudo launchctl unload /System/Library/LaunchDaemons/com.apple.audio.AudioComponentRegistrar.plist
sudo launchctl load /System/Library/LaunchDaemons/com.apple.audio.AudioComponentRegistrar.plist
echo "Audio Unit cache refreshed"
