# Example Configuration Guide

This guide explains how to configure the example to select user, environment, and call ID, matching the Android demo app pattern.

## Configuration Options

The example allows you to configure:

1. **User Selection** - Which user to authenticate as
2. **Environment Selection** - Which environment to use (production/staging/development)
3. **Call Selection** - Which call to join (call_type and call_id)

## Configuration in `complete_example.c`

Edit the configuration section at the top of the file:

```c
// ============================================================================
// Configuration - Set these values (matches Android demo app pattern)
// ============================================================================

// User and Environment Selection (for auth request)
#define STREAM_ENVIRONMENT "production"  // "production", "staging", or "development"
#define STREAM_USER_ID "user123"         // User ID to authenticate as (can be NULL for auto-generated)

// Call Selection (for joining call)
#define STREAM_CALL_TYPE "default"       // Call type (e.g., "default", "livestream")
#define STREAM_CALL_ID "call123"        // Call ID to join (can be NULL to create new call)
```

## User Selection

### Option 1: Specify User ID
```c
#define STREAM_USER_ID "user123"  // Authenticate as this user
```

### Option 2: Auto-Generate User ID
```c
#define STREAM_USER_ID NULL  // Backend will generate a user ID
```

**What happens:**
- If `STREAM_USER_ID` is set, the auth request includes `user_id` parameter
- If `STREAM_USER_ID` is NULL, backend generates a user ID
- Matches Android demo app: you can choose which user to authenticate as

## Environment Selection

```c
#define STREAM_ENVIRONMENT "production"  // or "staging" or "development"
```

**What happens:**
- Environment is passed to auth request: `GET api/auth/create-token?environment=production`
- Backend returns credentials for the selected environment
- Matches Android demo app: you can choose which environment to use

## Call Selection

### Call Type
```c
#define STREAM_CALL_TYPE "default"  // or "livestream", etc.
```

**What happens:**
- Call type is used in join call URL: `/video/calls/{call_type}/{call_id}/join`
- Different call types may have different configurations

### Call ID

#### Option 1: Join Existing Call
```c
#define STREAM_CALL_ID "call123"  // Join this specific call
```

#### Option 2: Create New Call
```c
#define STREAM_CALL_ID NULL  // Create a new call
```

**What happens:**
- If `STREAM_CALL_ID` is set, joins that specific call
- If `STREAM_CALL_ID` is NULL, creates a new call (if `create=true`)
- Matches Android demo app: you can choose which call to join or create a new one

## Complete Flow

```
1. Request Auth Data
   ├─ Environment: STREAM_ENVIRONMENT
   └─ User ID: STREAM_USER_ID (or NULL)
   
2. Connect to Coordinator
   └─ Uses auth data from step 1
   
3. Join Call
   ├─ Call Type: STREAM_CALL_TYPE
   └─ Call ID: STREAM_CALL_ID (or NULL)
   
4. Connect to SFU
   └─ Uses SFU credentials from join call response
```

## Example Configurations

### Example 1: Join Existing Call as Specific User
```c
#define STREAM_ENVIRONMENT "production"
#define STREAM_USER_ID "alice"
#define STREAM_CALL_TYPE "default"
#define STREAM_CALL_ID "meeting-room-1"
```

### Example 2: Create New Call as Auto-Generated User
```c
#define STREAM_ENVIRONMENT "staging"
#define STREAM_USER_ID NULL  // Auto-generate
#define STREAM_CALL_TYPE "default"
#define STREAM_CALL_ID NULL  // Create new
```

### Example 3: Join Livestream Call
```c
#define STREAM_ENVIRONMENT "production"
#define STREAM_USER_ID "broadcaster"
#define STREAM_CALL_TYPE "livestream"
#define STREAM_CALL_ID "live-event-2024"
```

## Matching Android Demo App

The Android demo app allows you to:
- ✅ Choose user (via user ID input)
- ✅ Choose environment (via dropdown)
- ✅ Choose call ID (via input field)

Our ESP32 example matches this by:
- ✅ `STREAM_USER_ID` - Choose user (or NULL for auto-generated)
- ✅ `STREAM_ENVIRONMENT` - Choose environment
- ✅ `STREAM_CALL_ID` - Choose call ID (or NULL to create new)

## Runtime Configuration (Future Enhancement)

Currently, configuration is done via `#define` at compile time. For runtime configuration (like Android demo app), you could:

1. Store configuration in NVS (Non-Volatile Storage)
2. Accept configuration via serial/USB
3. Accept configuration via WiFi/HTTP API
4. Use menuconfig (ESP-IDF configuration system)

This would allow changing user, environment, and call ID without recompiling.
