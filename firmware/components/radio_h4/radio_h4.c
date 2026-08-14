#include "radio_h4.h"

#include <string.h>

static uint16_t read_le16(const uint8_t *bytes) {
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
}

bool radio_h4_packet_type_supported(uint8_t packet_type) {
    return packet_type == RADIO_H4_TYPE_COMMAND || packet_type == RADIO_H4_TYPE_ACL ||
           packet_type == RADIO_H4_TYPE_SCO || packet_type == RADIO_H4_TYPE_EVENT;
}

size_t radio_h4_header_size(uint8_t packet_type) {
    switch (packet_type) {
    case RADIO_H4_TYPE_COMMAND:
        return 4u; /* type + opcode(2) + parameter length */
    case RADIO_H4_TYPE_ACL:
        return 5u; /* type + handle/flags(2) + data length(2) */
    case RADIO_H4_TYPE_SCO:
        return 4u; /* type + handle/flags(2) + data length */
    case RADIO_H4_TYPE_EVENT:
        return 3u; /* type + event code + parameter length */
    default:
        return 0u;
    }
}

size_t radio_h4_payload_limit(uint8_t packet_type) {
    switch (packet_type) {
    case RADIO_H4_TYPE_COMMAND:
        return RADIO_H4_COMMAND_MAX_PAYLOAD;
    case RADIO_H4_TYPE_ACL:
        return RADIO_H4_ACL_MAX_PAYLOAD;
    case RADIO_H4_TYPE_SCO:
        return RADIO_H4_SCO_MAX_PAYLOAD;
    case RADIO_H4_TYPE_EVENT:
        return RADIO_H4_EVENT_MAX_PAYLOAD;
    default:
        return 0u;
    }
}

static size_t declared_payload_length_from_bytes(const uint8_t *bytes) {
    switch (bytes[0]) {
    case RADIO_H4_TYPE_COMMAND:
        return bytes[3];
    case RADIO_H4_TYPE_ACL:
        return read_le16(&bytes[3]);
    case RADIO_H4_TYPE_SCO:
        return bytes[3];
    case RADIO_H4_TYPE_EVENT:
        return bytes[2];
    default:
        return 0u;
    }
}

static size_t declared_payload_length(const radio_h4_parser_t *parser) {
    return declared_payload_length_from_bytes(parser->bytes);
}

radio_h4_result_t radio_h4_validate_complete(const uint8_t *data, size_t data_len) {
    if (data == NULL || data_len == 0u) {
        return RADIO_H4_ERR_ARGUMENT;
    }
    if (!radio_h4_packet_type_supported(data[0])) {
        return RADIO_H4_ERR_PACKET_TYPE;
    }

    const size_t header_size = radio_h4_header_size(data[0]);
    if (data_len < header_size) {
        return RADIO_H4_ERR_TRUNCATED;
    }

    const size_t payload_len = declared_payload_length_from_bytes(data);
    if (payload_len > radio_h4_payload_limit(data[0]) ||
        header_size + payload_len > RADIO_H4_MAX_PACKET_SIZE) {
        return RADIO_H4_ERR_LENGTH;
    }
    if (data_len != header_size + payload_len) {
        return data_len < header_size + payload_len ? RADIO_H4_ERR_TRUNCATED : RADIO_H4_ERR_LENGTH;
    }

    return RADIO_H4_OK;
}

void radio_h4_parser_init(radio_h4_parser_t *parser) {
    if (parser != NULL) {
        memset(parser, 0, sizeof(*parser));
    }
}

void radio_h4_parser_reset(radio_h4_parser_t *parser) { radio_h4_parser_init(parser); }

static void parser_start_next_packet(radio_h4_parser_t *parser) {
    parser->used = 0u;
    parser->expected = 0u;
    parser->header_size = 0u;
}

radio_h4_result_t radio_h4_parser_feed(radio_h4_parser_t *parser, const uint8_t *data,
                                       size_t data_len, size_t *consumed,
                                       radio_h4_packet_t *packet_out) {
    if (parser == NULL || consumed == NULL || packet_out == NULL ||
        (data == NULL && data_len != 0u)) {
        return RADIO_H4_ERR_ARGUMENT;
    }

    *consumed = 0u;
    packet_out->len = 0u;

    if (parser->failed) {
        return RADIO_H4_ERR_FAILED_STATE;
    }

    while (*consumed < data_len) {
        const uint8_t byte = data[*consumed];
        (*consumed)++;

        if (parser->used == 0u) {
            if (!radio_h4_packet_type_supported(byte)) {
                parser->failed = true;
                return RADIO_H4_ERR_PACKET_TYPE;
            }
            parser->header_size = radio_h4_header_size(byte);
        }

        if (parser->used >= sizeof(parser->bytes)) {
            parser->failed = true;
            return RADIO_H4_ERR_LENGTH;
        }
        parser->bytes[parser->used++] = byte;

        if (parser->expected == 0u && parser->used == parser->header_size) {
            const size_t payload_len = declared_payload_length(parser);
            const size_t payload_limit = radio_h4_payload_limit(parser->bytes[0]);
            if (payload_len > payload_limit ||
                parser->header_size + payload_len > sizeof(parser->bytes)) {
                parser->failed = true;
                return RADIO_H4_ERR_LENGTH;
            }
            parser->expected = parser->header_size + payload_len;
        }

        if (parser->expected != 0u && parser->used == parser->expected) {
            memcpy(packet_out->bytes, parser->bytes, parser->used);
            packet_out->len = parser->used;
            parser_start_next_packet(parser);
            return RADIO_H4_PACKET_READY;
        }
    }

    return RADIO_H4_OK;
}

radio_h4_result_t radio_h4_parser_finish(radio_h4_parser_t *parser) {
    if (parser == NULL) {
        return RADIO_H4_ERR_ARGUMENT;
    }
    if (parser->failed) {
        return RADIO_H4_ERR_FAILED_STATE;
    }
    if (parser->used != 0u) {
        parser->failed = true;
        return RADIO_H4_ERR_TRUNCATED;
    }
    return RADIO_H4_OK;
}

void radio_h4_queue_init(radio_h4_queue_t *queue) {
    if (queue != NULL) {
        memset(queue, 0, sizeof(*queue));
    }
}

radio_h4_result_t radio_h4_queue_push(radio_h4_queue_t *queue, const radio_h4_packet_t *packet) {
    if (queue == NULL || packet == NULL || packet->len == 0u ||
        packet->len > RADIO_H4_MAX_PACKET_SIZE) {
        return RADIO_H4_ERR_ARGUMENT;
    }
    if (queue->count == RADIO_H4_QUEUE_CAPACITY) {
        queue->full_count++;
        return RADIO_H4_ERR_QUEUE_FULL;
    }

    queue->slots[queue->tail] = *packet;
    queue->tail = (queue->tail + 1u) % RADIO_H4_QUEUE_CAPACITY;
    queue->count++;
    if (queue->count > queue->high_water) {
        queue->high_water = queue->count;
    }
    return RADIO_H4_OK;
}

radio_h4_result_t radio_h4_queue_peek(const radio_h4_queue_t *queue,
                                      const radio_h4_packet_t **packet) {
    if (queue == NULL || packet == NULL) {
        return RADIO_H4_ERR_ARGUMENT;
    }
    if (queue->count == 0u) {
        *packet = NULL;
        return RADIO_H4_ERR_QUEUE_EMPTY;
    }
    *packet = &queue->slots[queue->head];
    return RADIO_H4_OK;
}

radio_h4_result_t radio_h4_queue_pop(radio_h4_queue_t *queue) {
    if (queue == NULL) {
        return RADIO_H4_ERR_ARGUMENT;
    }
    if (queue->count == 0u) {
        return RADIO_H4_ERR_QUEUE_EMPTY;
    }
    queue->head = (queue->head + 1u) % RADIO_H4_QUEUE_CAPACITY;
    queue->count--;
    return RADIO_H4_OK;
}
