/**
 * @file stream_video_protobuf.h
 * @brief Protobuf serialization/deserialization utilities
 */

#ifndef STREAM_VIDEO_PROTOBUF_H
#define STREAM_VIDEO_PROTOBUF_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "stream_video.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Buffer for protobuf serialization
 */
typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
} stream_video_protobuf_buffer_t;

/**
 * @brief Allocate a protobuf buffer
 * 
 * @param capacity Initial capacity
 * @return stream_video_protobuf_buffer_t* Buffer handle, or NULL on error
 */
stream_video_protobuf_buffer_t* stream_video_protobuf_buffer_alloc(size_t capacity);

/**
 * @brief Free a protobuf buffer
 * 
 * @param buffer Buffer to free
 */
void stream_video_protobuf_buffer_free(stream_video_protobuf_buffer_t *buffer);

/**
 * @brief Serialize a protobuf message to buffer
 * 
 * @param message Pointer to message structure
 * @param message_size Size of message structure
 * @param buffer Output buffer (will be allocated if NULL)
 * @return stream_video_error_t Error code
 */
stream_video_error_t stream_video_protobuf_serialize(
    const void *message,
    size_t message_size,
    stream_video_protobuf_buffer_t **buffer
);

/**
 * @brief Deserialize a protobuf message from buffer
 * 
 * @param buffer Input buffer
 * @param message_type Message type identifier
 * @param message_out Output message structure
 * @param message_size Size of message structure
 * @return stream_video_error_t Error code
 */
stream_video_error_t stream_video_protobuf_deserialize(
    const stream_video_protobuf_buffer_t *buffer,
    uint32_t message_type,
    void *message_out,
    size_t message_size
);

#ifdef __cplusplus
}
#endif

#endif // STREAM_VIDEO_PROTOBUF_H

