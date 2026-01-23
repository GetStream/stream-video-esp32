# How to Get Your Stream Signaling URL

## Quick Answer

The Stream signaling URL format is:

```
wss://[your-app-name].getstream.io/video/ws
```

## Step-by-Step Guide

### Option 1: From Stream Dashboard (Easiest)

1. **Log in to Stream Dashboard**
   - Go to https://dashboard.getstream.io
   - Sign in with your account

2. **Select Your Video App**
   - Click on your Video app (or create one if you don't have one)

3. **Get Your App Name**
   - Look at the URL or app settings
   - Your app name is usually visible in the dashboard
   - Example: If your app is called "my-video-app", your URL would be:
     ```
     wss://my-video-app.getstream.io/video/ws
     ```

4. **Check App Settings**
   - Go to: **Settings** → **App Settings**
   - Look for "WebSocket URL" or "Signaling URL"
   - Some dashboards show this directly

### Option 2: From Your Backend/API

If you're using Stream's backend SDK to generate tokens, you can also get the URL from there:

**Node.js Example:**
```javascript
const { StreamVideoClient } = require('@stream-io/video-client-sdk');

const client = new StreamVideoClient({
  apiKey: 'your-api-key',
  token: 'user-token'
});

// The base URL is typically:
// wss://[app-name].getstream.io/video/ws
```

**Python Example:**
```python
from stream_video import StreamVideo

client = StreamVideo(api_key="your-api-key", token="user-token")
# Base URL format: wss://[app-name].getstream.io/video/ws
```

### Option 3: Construct It Manually

If you know your Stream app name, you can construct it:

1. **Find your app name** (from dashboard or API key)
2. **Format:** `wss://[app-name].getstream.io/video/ws`

**Example:**
- App name: `my-video-app`
- URL: `wss://my-video-app.getstream.io/video/ws`

## URL Format Details

### Standard Stream Cloud URL
```
wss://[app-name].getstream.io/video/ws
```

### Self-Hosted Stream URL
If you're self-hosting Stream:
```
wss://your-custom-domain.com/video/ws
```

### Regional URLs (if applicable)
Some Stream instances might use regional URLs:
```
wss://[app-name]-us-east.getstream.io/video/ws
wss://[app-name]-eu-west.getstream.io/video/ws
```

## How to Verify the URL

### Method 1: Test in Browser Console
You can test if the URL is correct by opening browser console and trying:
```javascript
const ws = new WebSocket('wss://your-app-name.getstream.io/video/ws');
ws.onopen = () => console.log('Connected!');
ws.onerror = (e) => console.error('Error:', e);
```

### Method 2: Check Stream Documentation
- Stream Video docs usually show the URL format
- Check your app's API documentation

### Method 3: Contact Stream Support
- If you can't find it, Stream support can help
- They can provide the exact URL for your app

## Common Mistakes

❌ **Wrong:** `https://your-app.getstream.io/video/ws` (should be `wss://`)
❌ **Wrong:** `wss://getstream.io/video/ws` (missing app name)
❌ **Wrong:** `wss://your-app.getstream.io/` (missing `/video/ws` path)

✅ **Correct:** `wss://your-app-name.getstream.io/video/ws`

## Example Configuration

In your `main.c` file:

```c
// Example 1: Standard Stream Cloud
#define STREAM_SIGNALING_URL "wss://my-video-app.getstream.io/video/ws"

// Example 2: Self-hosted
#define STREAM_SIGNALING_URL "wss://video.mydomain.com/video/ws"

// Example 3: With region
#define STREAM_SIGNALING_URL "wss://my-app-us-east.getstream.io/video/ws"
```

## Still Can't Find It?

1. **Check Stream Dashboard** → Your Video App → Settings
2. **Check Stream API Documentation** for your app
3. **Look at your backend code** that generates tokens (it might have the URL)
4. **Contact Stream Support** with your app name/API key

## Important Notes

- ✅ Always use `wss://` (secure WebSocket), not `ws://`
- ✅ The path `/video/ws` is standard for Stream Video
- ✅ Your app name is usually part of your API key or visible in dashboard
- ✅ The URL is specific to your Stream app (not shared across apps)

