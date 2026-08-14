#ifndef RADIO_LINK_CONFIG_H
#define RADIO_LINK_CONFIG_H

/*
 * One shared source of truth for the inter-MCU HCI UART rate.
 * V1 starts conservatively; V1-901 may raise this only after measured
 * bidirectional hardware stress testing.
 */
#define RADIO_HCI_UART_BAUD 115200

#if RADIO_HCI_UART_BAUD < 115200 || RADIO_HCI_UART_BAUD > 4000000
#error "RADIO_HCI_UART_BAUD is outside the supported project range"
#endif

#endif
