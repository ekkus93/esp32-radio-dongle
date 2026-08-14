#ifndef RADIO_H4_H
#define RADIO_H4_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RADIO_H4_TYPE_COMMAND 0x01u
#define RADIO_H4_TYPE_ACL 0x02u
#define RADIO_H4_TYPE_SCO 0x03u
#define RADIO_H4_TYPE_EVENT 0x04u

#define RADIO_H4_COMMAND_MAX_PAYLOAD 255u
#define RADIO_H4_EVENT_MAX_PAYLOAD 255u
#define RADIO_H4_ACL_MAX_PAYLOAD 2048u
#define RADIO_H4_SCO_MAX_PAYLOAD 255u
#define RADIO_H4_MAX_HEADER_SIZE 5u
#define RADIO_H4_MAX_PACKET_SIZE (RADIO_H4_MAX_HEADER_SIZE + RADIO_H4_ACL_MAX_PAYLOAD)

#define RADIO_H4_QUEUE_CAPACITY 8u

typedef enum {
    RADIO_H4_OK = 0,
    RADIO_H4_PACKET_READY = 1,
    RADIO_H4_ERR_ARGUMENT = -1,
    RADIO_H4_ERR_PACKET_TYPE = -2,
    RADIO_H4_ERR_LENGTH = -3,
    RADIO_H4_ERR_TRUNCATED = -4,
    RADIO_H4_ERR_FAILED_STATE = -5,
    RADIO_H4_ERR_QUEUE_FULL = -6,
    RADIO_H4_ERR_QUEUE_EMPTY = -7,
} radio_h4_result_t;

typedef struct {
    uint8_t bytes[RADIO_H4_MAX_PACKET_SIZE];
    size_t len;
} radio_h4_packet_t;

typedef struct {
    uint8_t bytes[RADIO_H4_MAX_PACKET_SIZE];
    size_t used;
    size_t expected;
    size_t header_size;
    bool failed;
} radio_h4_parser_t;

typedef struct {
    radio_h4_packet_t slots[RADIO_H4_QUEUE_CAPACITY];
    size_t head;
    size_t tail;
    size_t count;
    size_t high_water;
    uint32_t full_count;
} radio_h4_queue_t;

void radio_h4_parser_init(radio_h4_parser_t *parser);
void radio_h4_parser_reset(radio_h4_parser_t *parser);
radio_h4_result_t radio_h4_parser_feed(radio_h4_parser_t *parser, const uint8_t *data,
                                       size_t data_len, size_t *consumed,
                                       radio_h4_packet_t *packet_out);
radio_h4_result_t radio_h4_parser_finish(radio_h4_parser_t *parser);
radio_h4_result_t radio_h4_validate_complete(const uint8_t *data, size_t data_len);

bool radio_h4_packet_type_supported(uint8_t packet_type);
size_t radio_h4_header_size(uint8_t packet_type);
size_t radio_h4_payload_limit(uint8_t packet_type);

void radio_h4_queue_init(radio_h4_queue_t *queue);
radio_h4_result_t radio_h4_queue_push(radio_h4_queue_t *queue, const radio_h4_packet_t *packet);
radio_h4_result_t radio_h4_queue_peek(const radio_h4_queue_t *queue,
                                      const radio_h4_packet_t **packet);
radio_h4_result_t radio_h4_queue_pop(radio_h4_queue_t *queue);

#ifdef __cplusplus
}
#endif

#endif
