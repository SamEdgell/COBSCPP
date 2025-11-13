#pragma once

// Standard Libraries.
#include <cstddef>
#include <cstdint>
#include <vector>


class COBSParser
{
    public:
        COBSParser(){}

        static constexpr uint8_t ASCII_NULL = 0x00U;
        static constexpr uint32_t MAX_FRAME_SIZE = 1024U; // Need to put a limit on the frame size to identify syncing issues.

        uint32_t encodeMessage(const uint8_t* input, const uint32_t inputSize, std::vector<uint8_t>& output);
        bool decodeMessage(const std::vector<uint8_t>& output);
        const uint8_t* getMessage(void) const { return m_message.data(); }

    private:
        static constexpr uint8_t MAX_BLOCK_SIZE = 0xFFU;

        std::vector<uint8_t> m_message;

        uint8_t calculateCRC(const uint8_t* const data, const uint32_t size);
};
