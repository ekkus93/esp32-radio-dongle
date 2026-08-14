#include "radio_h4.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static radio_h4_packet_t parse_one(radio_h4_parser_t *parser, const uint8_t *data, size_t len)
{
    radio_h4_packet_t packet;
    size_t consumed = 0u;
    assert(radio_h4_parser_feed(parser, data, len, &consumed, &packet) == RADIO_H4_PACKET_READY);
    assert(consumed == len);
    return packet;
}

static void test_command(void)
{
    radio_h4_parser_t parser;
    radio_h4_parser_init(&parser);
    const uint8_t bytes[] = {0x01, 0x03, 0x0c, 0x00};
    const radio_h4_packet_t packet = parse_one(&parser, bytes, sizeof(bytes));
    assert(packet.len == sizeof(bytes));
    assert(memcmp(packet.bytes, bytes, sizeof(bytes)) == 0);
    assert(radio_h4_validate_complete(bytes, sizeof(bytes)) == RADIO_H4_OK);
}

static void test_event(void)
{
    radio_h4_parser_t parser;
    radio_h4_parser_init(&parser);
    const uint8_t bytes[] = {0x04, 0x0e, 0x04, 0x01, 0x03, 0x0c, 0x00};
    const radio_h4_packet_t packet = parse_one(&parser, bytes, sizeof(bytes));
    assert(packet.len == sizeof(bytes));
    assert(radio_h4_validate_complete(bytes, sizeof(bytes)) == RADIO_H4_OK);
}

static void test_acl(void)
{
    radio_h4_parser_t parser;
    radio_h4_parser_init(&parser);
    const uint8_t bytes[] = {0x02, 0x01, 0x20, 0x03, 0x00, 0xaa, 0xbb, 0xcc};
    const radio_h4_packet_t packet = parse_one(&parser, bytes, sizeof(bytes));
    assert(packet.len == sizeof(bytes));
    assert(radio_h4_validate_complete(bytes, sizeof(bytes)) == RADIO_H4_OK);
}

static void test_sco(void)
{
    radio_h4_parser_t parser;
    radio_h4_parser_init(&parser);
    const uint8_t bytes[] = {0x03, 0x01, 0x00, 0x02, 0xaa, 0xbb};
    const radio_h4_packet_t packet = parse_one(&parser, bytes, sizeof(bytes));
    assert(packet.len == sizeof(bytes));
    assert(radio_h4_validate_complete(bytes, sizeof(bytes)) == RADIO_H4_OK);
}

static void test_fragmented(void)
{
    radio_h4_parser_t parser;
    radio_h4_parser_init(&parser);
    radio_h4_packet_t packet;
    size_t consumed = 0u;
    const uint8_t first[] = {0x04, 0x0e};
    const uint8_t second[] = {0x01};
    const uint8_t third[] = {0x00};

    assert(radio_h4_parser_feed(&parser, first, sizeof(first), &consumed, &packet) == RADIO_H4_OK);
    assert(consumed == sizeof(first));
    assert(radio_h4_parser_feed(&parser, second, sizeof(second), &consumed, &packet) == RADIO_H4_OK);
    assert(consumed == sizeof(second));
    assert(radio_h4_parser_feed(&parser, third, sizeof(third), &consumed, &packet) ==
           RADIO_H4_PACKET_READY);
    assert(packet.len == 4u);
}

static void test_back_to_back(void)
{
    radio_h4_parser_t parser;
    radio_h4_parser_init(&parser);
    radio_h4_packet_t packet;
    size_t consumed = 0u;
    const uint8_t bytes[] = {0x01, 0x03, 0x0c, 0x00, 0x04, 0x0e, 0x00};

    assert(radio_h4_parser_feed(&parser, bytes, sizeof(bytes), &consumed, &packet) ==
           RADIO_H4_PACKET_READY);
    assert(consumed == 4u);

    size_t consumed_second = 0u;
    assert(radio_h4_parser_feed(&parser, bytes + consumed, sizeof(bytes) - consumed,
                                &consumed_second, &packet) == RADIO_H4_PACKET_READY);
    assert(consumed_second == 3u);
}

static void test_invalid_type_fails_closed(void)
{
    radio_h4_parser_t parser;
    radio_h4_parser_init(&parser);
    radio_h4_packet_t packet;
    size_t consumed = 0u;
    const uint8_t bad[] = {0xff};
    const uint8_t good[] = {0x01, 0x03, 0x0c, 0x00};

    assert(radio_h4_parser_feed(&parser, bad, sizeof(bad), &consumed, &packet) ==
           RADIO_H4_ERR_PACKET_TYPE);
    assert(radio_h4_validate_complete(bad, sizeof(bad)) == RADIO_H4_ERR_PACKET_TYPE);
    assert(radio_h4_parser_feed(&parser, good, sizeof(good), &consumed, &packet) ==
           RADIO_H4_ERR_FAILED_STATE);
    radio_h4_parser_reset(&parser);
    assert(radio_h4_parser_feed(&parser, good, sizeof(good), &consumed, &packet) ==
           RADIO_H4_PACKET_READY);
}

static void test_oversized_acl_fails_closed(void)
{
    radio_h4_parser_t parser;
    radio_h4_parser_init(&parser);
    radio_h4_packet_t packet;
    size_t consumed = 0u;
    const uint16_t too_big = RADIO_H4_ACL_MAX_PAYLOAD + 1u;
    const uint8_t header[] = {0x02, 0x01, 0x20, (uint8_t)(too_big & 0xffu),
                              (uint8_t)(too_big >> 8)};

    assert(radio_h4_parser_feed(&parser, header, sizeof(header), &consumed, &packet) ==
           RADIO_H4_ERR_LENGTH);
    assert(radio_h4_validate_complete(header, sizeof(header)) == RADIO_H4_ERR_LENGTH);
}

static void test_truncated(void)
{
    radio_h4_parser_t parser;
    radio_h4_parser_init(&parser);
    radio_h4_packet_t packet;
    size_t consumed = 0u;
    const uint8_t partial[] = {0x04, 0x0e, 0x02, 0x01};

    assert(radio_h4_parser_feed(&parser, partial, sizeof(partial), &consumed, &packet) ==
           RADIO_H4_OK);
    assert(radio_h4_validate_complete(partial, sizeof(partial)) == RADIO_H4_ERR_TRUNCATED);
    assert(radio_h4_parser_finish(&parser) == RADIO_H4_ERR_TRUNCATED);
    assert(radio_h4_parser_finish(&parser) == RADIO_H4_ERR_FAILED_STATE);
}

static void test_trailing_bytes_rejected(void)
{
    const uint8_t too_long[] = {0x04, 0x0e, 0x00, 0xff};
    assert(radio_h4_validate_complete(too_long, sizeof(too_long)) == RADIO_H4_ERR_LENGTH);
}

static void test_queue_exhaustion(void)
{
    radio_h4_queue_t queue;
    radio_h4_queue_init(&queue);
    const radio_h4_packet_t packet = {.bytes = {0x01, 0x03, 0x0c, 0x00}, .len = 4u};

    for (size_t i = 0u; i < RADIO_H4_QUEUE_CAPACITY; ++i) {
        assert(radio_h4_queue_push(&queue, &packet) == RADIO_H4_OK);
    }
    assert(queue.high_water == RADIO_H4_QUEUE_CAPACITY);
    assert(radio_h4_queue_push(&queue, &packet) == RADIO_H4_ERR_QUEUE_FULL);
    assert(queue.full_count == 1u);

    const radio_h4_packet_t *front = NULL;
    assert(radio_h4_queue_peek(&queue, &front) == RADIO_H4_OK);
    assert(front != NULL && front->len == 4u);

    for (size_t i = 0u; i < RADIO_H4_QUEUE_CAPACITY; ++i) {
        assert(radio_h4_queue_pop(&queue) == RADIO_H4_OK);
    }
    assert(radio_h4_queue_pop(&queue) == RADIO_H4_ERR_QUEUE_EMPTY);
}

int main(void)
{
    test_command();
    test_event();
    test_acl();
    test_sco();
    test_fragmented();
    test_back_to_back();
    test_invalid_type_fails_closed();
    test_oversized_acl_fails_closed();
    test_truncated();
    test_trailing_bytes_rejected();
    test_queue_exhaustion();
    puts("radio_h4 tests passed");
    return 0;
}
