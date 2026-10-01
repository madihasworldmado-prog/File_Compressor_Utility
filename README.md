# FILE COMPRESSOR UTILITY

A **C++-based file and folder compression utility** that implements a custom `.apm` archive format and a self-contained **DEFLATE compression engine**. The project demonstrates practical concepts of file handling, data compression, archive management, and data integrity.

## Features

- Compress individual files into `.apm` archives
- Compress complete folders while preserving their structure
- Custom DEFLATE implementation
- Extract `.apm` archives
- Automatic **DEFLATE / STORE** method selection
- CRC32-based data integrity verification
- Compression and extraction progress indicators
- Archive and extraction validation
- Custom archive metadata
- No dependency on zlib or other external compression libraries

## Compression Algorithm

The project uses a custom implementation of **DEFLATE** through the `deflate_1.h` header.

The implementation combines:

- **LZ77 matching** for identifying repeated data
- **Fixed Huffman coding** for efficient symbol encoding
- A **32 KB sliding window**
- Hash-based matching with a limited hash chain
- Match lengths from **3 to 258 bytes**

The header provides the main file-level compression and decompression functions:

```cpp
bool DeflateCompressFile(const char *input,
                         const char *output);

bool DeflateDecompressFile(const char *input,
                           const char *output);
```

The implementation is self-contained and uses raw DEFLATE streams rather than linking against zlib.

## APM Archive Format

The application uses its own **`.apm` archive format** to store file data and metadata.

An archive contains information such as:

- Archive magic and version
- Filename or relative path
- Original file size
- Compressed size
- Compression method
- CRC32 checksum
- Stored file data

The application can use either **DEFLATE** or **STORE** depending on the resulting file size. If compression does not produce a smaller representation, storing the original data avoids unnecessary expansion.

## How It Works

```text
             USER INPUT
                 |
                 v
        FILE / FOLDER SELECTION
                 |
                 v
          FILE ANALYSIS
                 |
          +------+------+
          |             |
          v             v
        CRC32       FILE SIZE
          |             |
          +------+------+
                 |
                 v
        DEFLATE COMPRESSION
                 |
                 v
        COMPARE FILE SIZES
             /       \
            /         \
         Smaller?      No
          /              \
        Yes               v
         |              STORE
         v                |
      DEFLATE              |
          \               /
           +------+------+
                  |
                  v
             .APM ARCHIVE
                  |
                  v
              EXTRACTION
                  |
                  v
        SIZE + CRC32 CHECK
                  |
                  v
                OUTPUT
```

## Project Structure

```text
FILE-COMPRESSOR-UTILITY/
|
+-- Compressor_apm.cpp
+-- deflate_1.h
+-- README.md
+-- LICENSE
```

### `Compressor_apm.cpp`

The main application source file containing:

- User interface
- File and folder operations
- APM archive creation
- Archive extraction
- CRC32 calculation
- Compression-method selection
- Validation and progress display

### `deflate.h`

A custom, user-defined header containing the DEFLATE compression and decompression implementation. It includes bit-level reading/writing, fixed Huffman coding, LZ77 matching, length/distance encoding, and the public compression/decompression functions.

## Requirements

- Windows operating system
- C++ compiler with C++98/C++99 support
- No external compression library required

The custom DEFLATE implementation is designed for C++98/C++99 and does not depend on STL containers or external libraries.

## Usage

Run the compiled program and select an operation from the menu:

```text
1. Compress a file
2. Compress a folder
3. Extract an .apm file
4. Exit
```

### Compress a File

Select a file → the program calculates its CRC32 → attempts DEFLATE compression → compares the sizes → creates an `.apm` archive.

### Compress a Folder

Select a folder → files are processed recursively → relative paths and metadata are stored → an `.apm` archive is created.

### Extract an Archive

Select an `.apm` file → archive metadata is read → files are extracted → size and CRC32 are verified.

## Data Integrity

The project uses **CRC32** to verify extracted data. The checksum stored with the archive is compared with the checksum calculated from the extracted file. Combined with the original file-size check, this helps detect corrupted or inconsistent extracted data.

## Technical Specifications

| Component | Implementation |
|---|---|
| Language | C++ |
| Compression | DEFLATE |
| Matching | LZ77 |
| Coding | Fixed Huffman |
| Archive Format | Custom `.apm` |
| Integrity Check | CRC32 |
| Window Size | 32 KB |
| Minimum Match | 3 bytes |
| Maximum Match | 258 bytes |
| External Libraries | None |
| Target Platform | Windows |

## Educational Purpose

This project demonstrates the practical application of:

- File handling in C++
- Binary file operations
- Data compression
- LZ77 algorithms
- Huffman coding
- Bit-level programming
- Hash-based pattern matching
- Custom archive formats
- CRC32 checksums
- Folder and path management
- Data-integrity verification

## Future Enhancements

Possible future improvements include:

- Graphical User Interface
- Password protection and encryption
- Multiple compression algorithms
- Archive browsing and file management
- Compression statistics
- Cross-platform support
- Performance optimization for very large files

## Authors

**Developed as a College Computer Science Project**

**Project:** File Compressor Utility  
**Technology:** C++  
**Archive Format:** `.apm`  
**Compression:** Custom DEFLATE Implementation

---

> **Note:** `deflate_1.h` is a standalone DEFLATE implementation created for this project and is not a wrapper around zlib.
