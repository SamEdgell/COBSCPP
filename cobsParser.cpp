// Application Libraries.
#include "cobsParser.hpp"


/*
 * Encodes input data using COBS encoding.
 *
 * @param   input: Data to encode.
 * @param   inputSize: Total number of bytes in data.
 * @param   output: Location to store encoded data.
 *
 * @return  Total amount of encoded bytes.
 */
uint32_t
COBSParser::encodeMessage(const uint8_t* input, const uint32_t inputSize, std::vector<uint8_t>& output)
{
    const uint8_t crc = calculateCRC(input, inputSize); // First, calculate the CRC.
    const uint32_t messageSize = (inputSize + 1U); // The total message size to encode is the input size +1 for the CRC byte.

    /*
     * We know the size of the complete message, we can work out the expected size after encoding.
     * COBS will only store 254 bytes of data before needing to add an additional code byte should the size of the message exceed a blocks max length.
     */
    const size_t blockOverheadBytes = (messageSize / 254U); // If the buffer is greater than 254 bytes, calculate the total number of overhead bytes that signal new blocks.
    const size_t expectedLen = (messageSize + blockOverheadBytes + 2U); // +2 accounts for the first blocks overhead byte, and the ASCII_NULL byte to signal end of frame.

    // Clear and resize output buffer to expected length.
    output.clear();
    output.resize(expectedLen);

    // Encode data.
    uint8_t *encodedMessage = output.data(); // Pointer to the output buffer which we will be writing the encoded message to. Pointing at output[0].
    uint8_t *overheadByte = encodedMessage++; // The overhead byte will always be at the first position of a block, here we assign it to output[0] and increment the encoded message pointer to output[1] in preparation for data to be inserted.
    uint8_t overheadCount = 0x01; // A new block is about to start, at a minimum there is always 1 byte in a block, hence its count is set to 1 here.

    // Iterate through each byte in the message to encode.
    for (uint32_t i = 0; i < messageSize; ++i)
    {
        const uint8_t byte = (i < inputSize) ? input[i] : crc; // Iterate through the input byte until we are at the end, then add the CRC for encoding to complete the encoded message.

        if (byte != ASCII_NULL)
        {
            *encodedMessage++ = byte; // Byte is not ASCII_NULL, add it to the encoded message current position then shift it to over to the next address in preparation for the next byte.
            overheadCount++; // Increment because we have added a valid byte.
        }

        // If we have reached a ASCII_NULL, or filled the block (a block can only contain 254 bytes), terminate this block and restart with a new one.
        if ((byte == ASCII_NULL) || (overheadCount == MAX_BLOCK_SIZE))
        {
            *overheadByte = overheadCount; // Update the overhead byte for this block.
            overheadCount = 0x01; // Reset the overhead count.
            overheadByte = encodedMessage++; // The next overhead byte now moves to where the encoded message is currently sat at because the encoding makes sure the overhead byte is first in a block, the encoded message is shifted to the next position.
        }
    }

    *overheadByte = overheadCount; // Update the overhead count for the final block.
    *encodedMessage++ = ASCII_NULL; // Finally, append the ASCII_NULL signalling end of frame.

    const size_t actualLen = (encodedMessage - output.data()); // Calculate the total number of encoded bytes = encodedMessage[x] - output[0].

    /*
     * Resize again, even though it has been done previously, this is because I expect the resize to shrink the vector, if it was to expand, performance would be impacted
     * because the vector would need to allocate new memory and copy across the existing data and therefore, this is why the ASCII_NULL byte is added in the first resize also.
     */
    output.resize(actualLen);

    return static_cast<uint32_t>(actualLen);
}


/*
 * Decodes input data using COBS decoding.
 *
 * @param   input: Data to decode.
 *
 * @return  True if decoded message is validated, else false.
 */
bool
COBSParser::decodeMessage(const std::vector<uint8_t>& input)
{
    std::vector<uint8_t> output; // Create an output buffer to store the decoded message. Not directly using m_message because I don't want to fill it with an unvalidated message should the decoding of this input data fail.
    output.reserve(input.size()); // In theory, the output buffer will never be larger than the input buffer, so assign that size for now.

    const uint8_t *encodedMessage = input.data(); // Point at the input data in preparation to iterate through each byte.
    uint8_t overheadCount = MAX_BLOCK_SIZE; // Initial value unused until the first iteration has finished.
    const uint8_t *encodedMessageEnd = (input.data() + input.size()); // Locate the end of the encoded message so we know when to stop iterating.

    // Setting blockSize to zero here will force reading of the first byte (which is the first overhead byte) in the encoded message.
    for (uint8_t blockSize = 0; encodedMessage < encodedMessageEnd; --blockSize)
    {
        // Are there still bytes left in this current block?
        if (blockSize > 0U)
        {
            output.push_back(*encodedMessage++); // Copy byte from input to output.
        }
        else
        {
            // We are starting a new block, read the current byte to determine the size of the block.
            blockSize = *encodedMessage++;

            if (blockSize == ASCII_NULL)
            {
                // End of frame reached.
                break;
            }

            // If the previous block size was partial (!= MAX_BLOCK_SIZE). This implies it was terminated by a ASCII_NULL, so we must re-insert that ASCII_NULL now.
            if (overheadCount != MAX_BLOCK_SIZE)
            {
                output.push_back(ASCII_NULL);
            }

            overheadCount = blockSize; // Byte is not end of frame, update the next block size.
        }
    }

    // A valid message must contain at least one byte for the CRC, do not process further if this is not the case.
    if (output.empty())
    {
        return false;
    }

    const uint8_t receivedCRC = output.back(); // The last byte in the decoded message is the CRC.
    output.pop_back(); // Remove the CRC from the message for the CRC calculation. This does not remove the CRC byte from the address it's sitting in, it simply reduces the vector's size to ignore the CRC.

    const uint8_t calculatedCRC = calculateCRC(output.data(), output.size());

    if (calculatedCRC == receivedCRC)
    {
        m_message = std::move(output); // We transfer the output buffer to m_message once it has been validated. This ensures m_message only ever contains validated messages.
        return true;
    }
    else
    {
        return false;
    }
}


// Private methods.


/*
 * Performs CRC of the data.
 *
 * @param   data: The data to calculate CRC.
 * @param   len: The total amount of bytes in this data.
 *
 * @return  The CRC calculation.
 */
uint8_t
COBSParser::calculateCRC(const uint8_t* const data, const uint32_t len)
{
    uint8_t crc = 0U;

    for (uint32_t i = 0U; i < len; i++)
    {
        crc ^= data[i];
    }

    return crc;
}
