#pragma once
#include <cstdint>

enum Flags
{
    SYN  = 1,
    ACK  = 2,
    FIN  = 4,
    DATA = 8
};

struct TransportHeader
{
    uint32_t seqNumber;
    uint32_t ackNumber;
    uint8_t flags;
};

