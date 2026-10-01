#ifndef DEFLATE_H
#define DEFLATE_H

/*
    deflate.h
    ------------------------------------------------------------
    Self-contained C++98/C++99 DEFLATE implementation.

    - No zlib
    - No STL containers
    - No external libraries
    - Raw DEFLATE stream (RFC 1951)
    - Fixed-Huffman DEFLATE blocks
    - Greedy LZ77 matching with a small hash chain
    - Matching decompressor
    - Designed to be used by the .apm container

    Public functions:
        bool DeflateCompressFile(const char *input,
                                 const char *output);

        bool DeflateDecompressFile(const char *input,
                                   const char *output);

    NOTE:
        This is a standalone implementation of the DEFLATE
        format, not a copy/link against zlib.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DF_WINDOW       32768
#define DF_MIN_MATCH    3
#define DF_MAX_MATCH    258
#define DF_HASH_SIZE    65536
#define DF_MAX_CHAIN    64

/*
    Progress callback:
        current = bytes/units actually processed
        total   = total bytes/units expected

    The callback is optional. Pass NULL when no progress display
    is required.
*/
typedef void (*DFProgressCallback)(long long current,
                                   long long total);

/* ------------------------------------------------------------
   Small helpers
   ------------------------------------------------------------ */

static unsigned int df_reverse_bits(unsigned int value, int count)
{
    unsigned int r = 0;
    int i;

    for (i = 0; i < count; ++i)
    {
        r = (r << 1) | (value & 1U);
        value >>= 1;
    }

    return r;
}

/* ------------------------------------------------------------
   Bit writer
   ------------------------------------------------------------ */

class DFBitWriter
{
private:
    FILE *fp;
    unsigned int bitBuffer;
    int bitCount;

public:
    DFBitWriter()
    {
        fp = NULL;
        bitBuffer = 0;
        bitCount = 0;
    }

    void attach(FILE *f)
    {
        fp = f;
        bitBuffer = 0;
        bitCount = 0;
    }

    bool writeBits(unsigned int value, int count)
    {
        bitBuffer |= (value << bitCount);
        bitCount += count;

        while (bitCount >= 8)
        {
            unsigned char c =
                (unsigned char)(bitBuffer & 0xFFU);

            if (fwrite(&c, 1, 1, fp) != 1)
                return false;

            bitBuffer >>= 8;
            bitCount -= 8;
        }

        return true;
    }

    bool flush()
    {
        if (bitCount > 0)
        {
            unsigned char c =
                (unsigned char)(bitBuffer & 0xFFU);

            if (fwrite(&c, 1, 1, fp) != 1)
                return false;

            bitBuffer = 0;
            bitCount = 0;
        }

        return true;
    }
};

/* ------------------------------------------------------------
   Bit reader
   ------------------------------------------------------------ */

class DFBitReader
{
private:
    FILE *fp;
    unsigned int bitBuffer;
    int bitCount;

public:
    DFBitReader()
    {
        fp = NULL;
        bitBuffer = 0;
        bitCount = 0;
    }

    void attach(FILE *f)
    {
        fp = f;
        bitBuffer = 0;
        bitCount = 0;
    }

    bool readBits(int count, unsigned int &value)
    {
        value = 0;

        while (bitCount < count)
        {
            unsigned char c;

            if (fread(&c, 1, 1, fp) != 1)
                return false;

            bitBuffer |= ((unsigned int)c << bitCount);
            bitCount += 8;
        }

        value = bitBuffer & ((1U << count) - 1U);

        bitBuffer >>= count;
        bitCount -= count;

        return true;
    }

    void alignByte()
    {
        bitBuffer = 0;
        bitCount = 0;
    }
};

/* ------------------------------------------------------------
   Fixed Huffman code generation

   DEFLATE fixed codes:
       literal/length 0..143   : 8 bits
       literal/length 144..255 : 9 bits
       literal/length 256..279 : 7 bits
       literal/length 280..287 : 8 bits

   The actual bit representation is reversed because DEFLATE
   transmits Huffman codes least-significant bit first.
   ------------------------------------------------------------ */

static int df_fixed_len(int symbol)
{
    if (symbol <= 143)
        return 8;

    if (symbol <= 255)
        return 9;

    if (symbol <= 279)
        return 7;

    return 8;
}

static unsigned int df_fixed_code(int symbol)
{
    unsigned int code;

    if (symbol <= 143)
        code = 0x30U + (unsigned int)symbol;
    else if (symbol <= 255)
        code = 0x190U + (unsigned int)(symbol - 144);
    else if (symbol <= 279)
        code = (unsigned int)(symbol - 256);
    else
        code = 0xC0U + (unsigned int)(symbol - 280);

    return df_reverse_bits(code, df_fixed_len(symbol));
}

/* ------------------------------------------------------------
   Length tables
   ------------------------------------------------------------ */

static const int df_length_base[29] =
{
    3, 4, 5, 6, 7, 8, 9, 10,
    11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115,
    131, 163, 195, 227, 258
};

static const int df_length_extra[29] =
{
    0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4,
    5, 5, 5, 5, 0
};

/* ------------------------------------------------------------
   Distance tables
   ------------------------------------------------------------ */

static const int df_distance_base[30] =
{
    1, 2, 3, 4, 5, 7, 9, 13,
    17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049,
    3073, 4097, 6145, 8193, 12289, 16385,
    24577
};

static const int df_distance_extra[30] =
{
    0, 0, 0, 0, 1, 1, 2, 2,
    3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10,
    11, 11, 12, 12, 13, 13
};

/* ------------------------------------------------------------
   Convert length to DEFLATE symbol.
   ------------------------------------------------------------ */

static void df_encode_length(int length,
                             int &symbol,
                             int &extraValue,
                             int &extraBits)
{
    int i;

    for (i = 0; i < 29; ++i)
    {
        int base = df_length_base[i];
        int maxValue;

        if (df_length_extra[i] == 0)
            maxValue = base;
        else
            maxValue =
                base +
                ((1 << df_length_extra[i]) - 1);

        if (length <= maxValue)
        {
            symbol = 257 + i;
            extraBits = df_length_extra[i];
            extraValue = length - base;
            return;
        }
    }

    symbol = 285;
    extraBits = 0;
    extraValue = 0;
}

/* ------------------------------------------------------------
   Convert distance to DEFLATE symbol.
   ------------------------------------------------------------ */

static void df_encode_distance(int distance,
                               int &symbol,
                               int &extraValue,
                               int &extraBits)
{
    int i;

    for (i = 0; i < 30; ++i)
    {
        int base = df_distance_base[i];
        int maxValue =
            base + ((1 << df_distance_extra[i]) - 1);

        if (distance <= maxValue)
        {
            symbol = i;
            extraBits = df_distance_extra[i];
            extraValue = distance - base;
            return;
        }
    }

    symbol = 29;
    extraBits = 13;
    extraValue = distance - 24577;
}

/* ------------------------------------------------------------
   Hash used by the LZ77 matcher.
   ------------------------------------------------------------ */

static unsigned int df_hash3(const unsigned char *data,
                             int pos,
                             int size)
{
    unsigned int a;
    unsigned int b;
    unsigned int c;

    if (pos + 2 >= size)
        return 0;

    a = data[pos];
    b = data[pos + 1];
    c = data[pos + 2];

    return ((a * 251U) ^
            (b * 911U) ^
            (c * 3571U)) &
           (DF_HASH_SIZE - 1);
}

/* ------------------------------------------------------------
   Find a match.

   head[] contains the newest position for each 3-byte hash.
   prev[] forms a linked chain of previous matching positions.

   This deliberately uses plain arrays rather than vector/STL.
   ------------------------------------------------------------ */

static int df_find_match(const unsigned char *data,
                         int size,
                         int pos,
                         int *head,
                         int *prev,
                         int &bestDistance)
{
    int hash;
    int candidate;
    int bestLength = 0;
    int chainCount = 0;

    bestDistance = 0;

    if (pos + DF_MIN_MATCH > size)
        return 0;

    hash = (int)df_hash3(data, pos, size);

    candidate = head[hash];

    while (candidate >= 0 &&
           chainCount < DF_MAX_CHAIN)
    {
        int distance = pos - candidate;
        int length = 0;

        if (distance > DF_WINDOW)
            break;

        while (length < DF_MAX_MATCH &&
               pos + length < size)
        {
            int source = candidate + length;

            /*
                For overlapping matches, DEFLATE allows the
                decoder to read bytes that were just produced.
                This means the source may be before candidate's
                original 3-byte sequence.
            */
            if (source >= pos)
                source = candidate +
                         (length % distance);

            if (data[source] != data[pos + length])
                break;

            ++length;
        }

        if (length >= DF_MIN_MATCH &&
            length > bestLength)
        {
            bestLength = length;
            bestDistance = distance;

            if (length == DF_MAX_MATCH)
                break;
        }

        candidate = prev[candidate];
        ++chainCount;
    }

    return bestLength;
}

/* ------------------------------------------------------------
   Emit a fixed-Huffman literal/length symbol.
   ------------------------------------------------------------ */

static bool df_write_litlen(DFBitWriter &bw,
                            int symbol)
{
    return bw.writeBits(
        df_fixed_code(symbol),
        df_fixed_len(symbol)
    );
}

/* ------------------------------------------------------------
   Emit a fixed distance symbol.

   Fixed distance codes have 5-bit canonical values 0..29,
   transmitted LSB-first.
   ------------------------------------------------------------ */

static bool df_write_distance(DFBitWriter &bw,
                              int symbol)
{
    return bw.writeBits(
        df_reverse_bits((unsigned int)symbol, 5),
        5
    );
}

/* ------------------------------------------------------------
   DEFLATE compressor.

   The complete input is loaded into memory. This keeps the
   implementation simple and avoids STL containers.

   The output is one final fixed-Huffman block.
   ------------------------------------------------------------ */

static bool DeflateCompressBuffer(const unsigned char *data,
                                   int size,
                                   FILE *fp,
                                   DFProgressCallback callback)
{
    DFBitWriter bw;
    int *head;
    int *prev;
    int i;
    int pos = 0;

    bw.attach(fp);

    if (callback != NULL)
        callback(0, (long long)size);

    /*
        BFINAL = 1
        BTYPE  = 01 (fixed Huffman)

        Written LSB-first:
            1 + 01 = binary 011
            which is written as value 3, 3 bits.
    */
    if (!bw.writeBits(3, 3))
        return false;

    head = new int[DF_HASH_SIZE];

    if (head == NULL)
        return false;

    prev = NULL;

    if (size > 0)
    {
        prev = new int[size];

        if (prev == NULL)
        {
            delete [] head;
            return false;
        }
    }

    for (i = 0; i < DF_HASH_SIZE; ++i)
        head[i] = -1;

    for (i = 0; i < size; ++i)
        prev[i] = -1;

    while (pos < size)
    {
        int length = 0;
        int distance = 0;

        /*
            Search only when at least three bytes remain.
        */
        if (pos + DF_MIN_MATCH <= size)
        {
            length = df_find_match(
                data,
                size,
                pos,
                head,
                prev,
                distance
            );
        }

        if (length >= DF_MIN_MATCH)
        {
            int lengthSymbol;
            int lengthExtra;
            int lengthExtraBits;

            int distanceSymbol;
            int distanceExtra;
            int distanceExtraBits;

            df_encode_length(
                length,
                lengthSymbol,
                lengthExtra,
                lengthExtraBits
            );

            df_encode_distance(
                distance,
                distanceSymbol,
                distanceExtra,
                distanceExtraBits
            );

            if (!df_write_litlen(
                    bw,
                    lengthSymbol))
            {
                delete [] prev;
                delete [] head;
                return false;
            }

            if (lengthExtraBits > 0)
            {
                if (!bw.writeBits(
                        (unsigned int)lengthExtra,
                        lengthExtraBits))
                {
                    delete [] prev;
                    delete [] head;
                    return false;
                }
            }

            if (!df_write_distance(
                    bw,
                    distanceSymbol))
            {
                delete [] prev;
                delete [] head;
                return false;
            }

            if (distanceExtraBits > 0)
            {
                if (!bw.writeBits(
                        (unsigned int)distanceExtra,
                        distanceExtraBits))
                {
                    delete [] prev;
                    delete [] head;
                    return false;
                }
            }

            /*
                Add every position covered by the match to
                the dictionary.
            */
            for (i = 0; i < length; ++i)
            {
                int p = pos + i;

                if (p + 2 < size)
                {
                    int h =
                        (int)df_hash3(
                            data,
                            p,
                            size
                        );

                    prev[p] = head[h];
                    head[h] = p;
                }
            }

            pos += length;

            if (callback != NULL)
                callback((long long)pos,
                         (long long)size);
        }
        else
        {
            int literal = data[pos];

            if (!df_write_litlen(
                    bw,
                    literal))
            {
                delete [] prev;
                delete [] head;
                return false;
            }

            if (pos + 2 < size)
            {
                int h =
                    (int)df_hash3(
                        data,
                        pos,
                        size
                    );

                prev[pos] = head[h];
                head[h] = pos;
            }

            ++pos;

            if (callback != NULL)
                callback((long long)pos,
                         (long long)size);
        }
    }

    /*
        End-of-block symbol 256.
    */
    if (!df_write_litlen(bw, 256))
    {
        delete [] prev;
        delete [] head;
        return false;
    }

    if (!bw.flush())
    {
        delete [] prev;
        delete [] head;
        return false;
    }

    delete [] prev;
    delete [] head;

    return true;
}

/* ------------------------------------------------------------
   Huffman decoding helpers.

   Decode fixed literal/length symbols by reading one bit at a
   time and comparing against the fixed-code ranges.

   This is slower than a lookup table but much easier to audit
   and keeps the implementation small and C++99-friendly.
   ------------------------------------------------------------ */

static int df_decode_fixed_litlen(DFBitReader &br)
{
    unsigned int code = 0;
    int length;

    for (length = 1; length <= 9; ++length)
    {
        unsigned int bit;

        if (!br.readBits(1, bit))
            return -1;

        code |= (bit << (length - 1));

        {
            int symbol;

            for (symbol = 0; symbol <= 287; ++symbol)
            {
                if (df_fixed_len(symbol) == length)
                {
                    if (df_fixed_code(symbol) == code)
                        return symbol;
                }
            }
        }
    }

    return -1;
}

static int df_decode_fixed_distance(DFBitReader &br)
{
    unsigned int value;

    if (!br.readBits(5, value))
        return -1;

    /*
        Fixed distance symbols are simply 5-bit values,
        transmitted LSB-first.
    */
    value = df_reverse_bits(value, 5);

    if (value > 29)
        return -1;

    return (int)value;
}

/* ------------------------------------------------------------
   Decode one fixed-Huffman DEFLATE block.

   Output is written while decoding, and a 32K sliding window
   is maintained so back references can be copied correctly.
   ------------------------------------------------------------ */

static bool df_decode_fixed_block(DFBitReader &br,
                                  FILE *out,
                                  long long totalOutputSize,
                                  long long &currentOutput,
                                  DFProgressCallback callback)
{
    unsigned char window[DF_WINDOW];
    unsigned int windowPos = 0;
    unsigned int totalOutput = 0;

    memset(window, 0, sizeof(window));

    while (true)
    {
        int symbol =
            df_decode_fixed_litlen(br);

        if (symbol < 0)
            return false;

        if (symbol < 256)
        {
            unsigned char c =
                (unsigned char)symbol;

            if (fwrite(&c, 1, 1, out) != 1)
                return false;

            window[windowPos] = c;
            windowPos =
                (windowPos + 1) % DF_WINDOW;

            ++totalOutput;

            ++currentOutput;

            if (callback != NULL)
                callback(currentOutput,
                         totalOutputSize);
        }
        else if (symbol == 256)
        {
            return true;
        }
        else if (symbol >= 257 &&
                 symbol <= 285)
        {
            int lengthIndex = symbol - 257;
            int length =
                df_length_base[lengthIndex];

            int extraBits =
                df_length_extra[lengthIndex];

            unsigned int extraValue = 0;

            if (extraBits > 0)
            {
                if (!br.readBits(
                        extraBits,
                        extraValue))
                    return false;

                length +=
                    (int)extraValue;
            }

            {
                int distanceSymbol =
                    df_decode_fixed_distance(br);

                int distance;
                int distanceExtraBits;
                unsigned int distanceExtra = 0;
                int i;

                if (distanceSymbol < 0)
                    return false;

                distance =
                    df_distance_base[
                        distanceSymbol
                    ];

                distanceExtraBits =
                    df_distance_extra[
                        distanceSymbol
                    ];

                if (distanceExtraBits > 0)
                {
                    if (!br.readBits(
                            distanceExtraBits,
                            distanceExtra))
                        return false;

                    distance +=
                        (int)distanceExtra;
                }

                if (distance <= 0 ||
                    distance > DF_WINDOW ||
                    distance > (int)totalOutput)
                    return false;

                for (i = 0; i < length; ++i)
                {
                    unsigned int source =
                        (windowPos +
                         DF_WINDOW -
                         (unsigned int)distance)
                        % DF_WINDOW;

                    unsigned char c =
                        window[source];

                    if (fwrite(&c, 1, 1, out) != 1)
                        return false;

                    window[windowPos] = c;

                    windowPos =
                        (windowPos + 1) %
                        DF_WINDOW;

                    ++totalOutput;

                    ++currentOutput;

                    if (callback != NULL)
                        callback(currentOutput,
                                 totalOutputSize);
                }
            }
        }
        else
        {
            return false;
        }
    }
}

/* ------------------------------------------------------------
   Public file compressor.

   Returns true on success.
   ------------------------------------------------------------ */

bool DeflateCompressFile(const char *input,
                         const char *output,
                         DFProgressCallback callback = NULL)
{
    FILE *in;
    FILE *out;
    unsigned char *data;
    long fileSize;
    size_t readCount;
    bool result;

    in = fopen(input, "rb");

    if (in == NULL)
        return false;

    if (fseek(in, 0, SEEK_END) != 0)
    {
        fclose(in);
        return false;
    }

    fileSize = ftell(in);

    if (fileSize < 0)
    {
        fclose(in);
        return false;
    }

    if (fseek(in, 0, SEEK_SET) != 0)
    {
        fclose(in);
        return false;
    }

    data = NULL;

    if (fileSize > 0)
    {
        data =
            new unsigned char[
                (unsigned long)fileSize
            ];

        if (data == NULL)
        {
            fclose(in);
            return false;
        }

        readCount =
            fread(
                data,
                1,
                (size_t)fileSize,
                in
            );

        if (readCount != (size_t)fileSize)
        {
            delete [] data;
            fclose(in);
            return false;
        }
    }

    fclose(in);

    out = fopen(output, "wb");

    if (out == NULL)
    {
        delete [] data;
        return false;
    }

    /*
        Raw DEFLATE stream.
    */
    result =
        DeflateCompressBuffer(
            data,
            (int)fileSize,
            out,
            callback
        );

    fclose(out);

    delete [] data;

    if (!result)
    {
        remove(output);
        return false;
    }

    return true;
}

/* ------------------------------------------------------------
   Public file decompressor.

   This implementation accepts the raw DEFLATE stream generated
   by DeflateCompressFile().
   ------------------------------------------------------------ */

bool DeflateDecompressFile(const char *input,
                           const char *output,
                           long long totalOutputSize = 0,
                           DFProgressCallback callback = NULL)
{
    FILE *in;
    FILE *out;
    DFBitReader br;
    unsigned int finalBlock;
    unsigned int blockType;
    long long currentOutput = 0;
    bool result;

    in = fopen(input, "rb");

    if (in == NULL)
        return false;

    out = fopen(output, "wb");

    if (out == NULL)
    {
        fclose(in);
        return false;
    }

    br.attach(in);

    result = true;

    while (true)
    {
        if (!br.readBits(1, finalBlock))
        {
            result = false;
            break;
        }

        if (!br.readBits(2, blockType))
        {
            result = false;
            break;
        }

        /*
            This implementation deliberately supports fixed
            Huffman blocks (BTYPE=01), which is what our encoder
            generates.
        */
        if (blockType == 1)
        {
            if (!df_decode_fixed_block(
                    br,
                    out,
                    totalOutputSize,
                    currentOutput,
                    callback))
            {
                result = false;
                break;
            }
        }
        else
        {
            /*
                Stored and dynamic blocks are not needed by the
                matching encoder. Reject them rather than silently
                producing incorrect output.
            */
            result = false;
            break;
        }

        if (finalBlock)
            break;
    }

    fclose(out);
    fclose(in);

    if (!result)
        remove(output);

    return result;
}

#endif