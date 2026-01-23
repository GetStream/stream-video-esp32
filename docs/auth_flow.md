# Authentication Flow - Matching Android Demo App

This document explains the authentication flow that matches the Stream Android SDK demo app.

## Configuration Values (from Android Demo App)

### Token Expiry
```c
// 7 days = 7 * 24 * 60 * 60 seconds
#define STREAM_VIDEO_DEFAULT_TOKEN_EXPIRY_SECONDS (7 * 24 * 60 * 60)
```

### Base URL
```c
// Matches Android demo app
#define STREAM_VIDEO_DEFAULT_BASE_URL "pronto.getstream.io"
```

## Auth Data Request

### Endpoint
```
GET https://pronto.getstream.io/api/auth/create-token
```

### Query Parameters
- `environment` (String): Environment name (e.g., "production", "staging", "development")
- `user_id` (String, optional): User ID (can be NULL)
- `exp` (Int): Token expiry in seconds (default: 7 days)

### Request Example
```c
stream_video_auth_request_t auth_req = {
    .environment = "production",
    .user_id = "user123",  // Optional: can be NULL
    .exp = STREAM_VIDEO_DEFAULT_TOKEN_EXPIRY_SECONDS,  // 7 days
};

stream_video_request_auth_data(&auth_req, on_auth_data_received, NULL);
```

### Response Format
```json
{
    "userId": "user123",
    "apiKey": "my-app:abc123",
    "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9..."
}
```

## Creating User Object

After receiving auth data, create a user object:

```c
stream_video_auth_data_t auth_data;  // Received from backend

// Create user with id and role (minimum required)
// Matches Android SDK: User(id = authData.userId, role = "admin")
stream_video_user_t user;
stream_video_create_user_from_auth(
    &auth_data,
    STREAM_VIDEO_USER_ROLE_ADMIN,  // Default role: admin
    &user
);
```

## Complete Flow

```c
// 1. Request auth data from backend
stream_video_auth_request_t auth_req = {
    .environment = "production",
    .user_id = NULL,  // Optional
    .exp = STREAM_VIDEO_DEFAULT_TOKEN_EXPIRY_SECONDS,  // 7 days
};

stream_video_request_auth_data(&auth_req, on_auth_received, NULL);

// 2. In callback, create user
void on_auth_received(const stream_video_auth_data_t *auth_data, void *user_data) {
    // Create user object
    stream_video_user_t user;
    stream_video_create_user_from_auth(auth_data, STREAM_VIDEO_USER_ROLE_ADMIN, &user);
    
    // 3. Use auth_data for signaling
    stream_signaling_config_t config = {
        .api_key = auth_data->api_key,
        .user_id = auth_data->user_id,
        .token = auth_data->token,
        .env = STREAM_SIGNALING_ENV_PRODUCTION,
        // ...
    };
    
    // Create and connect signaling client
    stream_signaling_client_handle_t client;
    stream_signaling_client_create(&config, &client);
    stream_signaling_client_connect(client);
}
```

## Summary

- ✅ **Token Expiry**: 7 days (matches Android demo app)
- ✅ **Base URL**: `pronto.getstream.io` (matches Android demo app)
- ✅ **Auth Endpoint**: `GET api/auth/create-token` with environment parameter
- ✅ **User Creation**: User(id = authData.userId, role = "admin")
- ✅ **Environment Support**: Pass environment name when requesting token

