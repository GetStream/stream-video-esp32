# Token and Environment Configuration

This document explains how token expiry and environment configuration works in the ESP32 SDK, matching the Stream Android SDK pattern.

## Token Expiry

### Default Token Expiry
The ESP32 SDK uses the same default token expiry as the Android SDK:
- **Default: 1 hour (3600 seconds)**
- Defined as `STREAM_VIDEO_DEFAULT_TOKEN_EXPIRY_SECONDS`

### Token Generation
Tokens should be generated on your backend server (not on the ESP32 device) for security. The token should be a JWT that includes:

- **User ID**: Identifies the user
- **Expiry time**: `exp` claim in JWT (typically 1 hour)
- **Permissions**: Roles and capabilities
- **API Key**: Associated with your Stream app

### Token Validation
The ESP32 SDK performs basic token format validation, but full validation should be done server-side. When a token expires:

1. The Stream server will reject the connection
2. Your application should request a new token from your backend
3. Reconnect with the new token

## Environment Configuration

### Environment Types
The SDK supports three environment types (matching Android SDK pattern):

```c
typedef enum {
    STREAM_SIGNALING_ENV_PRODUCTION = 0,   // Production environment
    STREAM_SIGNALING_ENV_STAGING,          // Staging environment
    STREAM_SIGNALING_ENV_DEVELOPMENT,      // Development environment
} stream_signaling_environment_t;
```

### Base URL
The base URL is automatically determined based on the environment:

- **Production**: `getstream.io` (default)
- **Staging**: `getstream.io` (can be customized)
- **Development**: `getstream.io` (can be customized)

The signaling URL is constructed as:
```
wss://[app_name].[base_url]/video/ws
```

### Using Environment in Configuration

```c
stream_signaling_config_t config = {
    .api_key = "my-app:abc123",
    .user_id = "user123",
    .token = "jwt-token-here",
    .env = STREAM_SIGNALING_ENV_PRODUCTION,  // Set environment
    .base_url = NULL,  // NULL = use default for environment
    // ...
};
```

## Token Request Structure

When requesting a token from your backend, you can use this structure:

```c
stream_video_token_request_t token_req = {
    .api_key = "my-app:abc123",
    .user_id = "user123",
    .env = STREAM_VIDEO_ENV_PRODUCTION,
    .expiry_seconds = 3600,  // 1 hour (default)
    .custom_claims = NULL,   // Optional JSON string
};
```

Your backend should generate a JWT token with:
- `exp` claim set to current time + `expiry_seconds`
- User ID and permissions
- Signed with your Stream API secret

## Example: Requesting Token with Environment

```c
// Example: Request token from backend with environment
void request_token_from_backend(const char *user_id, 
                                stream_signaling_environment_t env,
                                char *token_out, size_t token_size)
{
    // Build request to your backend
    // Include environment name so backend knows which Stream app to use
    // Backend generates JWT with appropriate expiry (1 hour default)
    
    // Example HTTP request:
    // POST /api/stream/token
    // {
    //   "user_id": "user123",
    //   "environment": "production",  // or "staging", "development"
    //   "expiry_seconds": 3600
    // }
    
    // Backend returns JWT token
    // Store in token_out
}
```

## Matching Android SDK

The ESP32 SDK now matches the Android SDK pattern:

| Feature | Android SDK | ESP32 SDK |
|---------|-------------|-----------|
| Token Expiry | 1 hour default | 1 hour default (3600s) |
| Base URL | `getstream.io` | `getstream.io` |
| Environment | Via `geo` parameter | Via `env` parameter |
| URL Construction | Automatic from API key | Automatic from API key |

## Best Practices

1. **Generate tokens server-side**: Never generate tokens on the ESP32 device
2. **Use short-lived tokens**: 1 hour is recommended for production
3. **Refresh before expiry**: Request new token before current one expires
4. **Use production environment**: For production deployments
5. **Validate tokens**: Full validation should be server-side

## Token Expiry Handling

```c
// Check if token needs refresh
if (stream_video_is_token_expired(current_token)) {
    // Request new token from backend
    char new_token[512];
    request_token_from_backend(user_id, env, new_token, sizeof(new_token));
    
    // Update signaling client with new token
    // (Reconnect with new token)
}
```

## Summary

- ✅ **Token expiry**: 1 hour default (matches Android SDK)
- ✅ **Base URL**: `getstream.io` (matches Android SDK)
- ✅ **Environment support**: Production/Staging/Development
- ✅ **Automatic URL construction**: From API key + environment
- ✅ **Token generation**: Should be done server-side with environment name

