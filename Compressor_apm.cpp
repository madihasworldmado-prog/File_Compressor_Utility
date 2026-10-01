#include <iostream>
#include <fstream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdint.h>

#include <deflate.h>
using namespace std;
#ifdef _WIN32
#include <windows.h>
#endif


#define CLR_NORMAL 7
#define CLR_TITLE  11
#define CLR_MENU   14
#define CLR_INFO   10
#define CLR_ERROR  12
#define CLR_INPUT  15
#define CLR_DIM    8

void setColor(int color)
{
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, (WORD)color);
#else
    (void)color;
#endif
}

void clearScreen()
{
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void pauseScreen()
{
    setColor(CLR_DIM);
    cout << "\nPress ENTER to continue...";
    setColor(CLR_NORMAL);
    cin.get();
}

void showProgress(long long current,
                  long long total,
                  const char *label)
{
    static int lastPercent = -1;
    static const char *lastLabel = NULL;

    int percent = 0;
    int width = 30;
    int filled;
    int i;

    if (total > 0)
        percent = (int)((current * 100LL) / total);

    if (percent < 0)
        percent = 0;

    if (percent > 100)
        percent = 100;

    if (lastPercent == percent && lastLabel == label)
        return;

    lastPercent = percent;
    lastLabel = label;
    filled = (percent * width) / 100;

    setColor(CLR_INFO);

    cout << "\r" << label << " [";

    for (i = 0; i < width; ++i)
    {
        if (i < filled)
            cout << "#";
        else
            cout << "-";
    }

    cout << "] " << percent << "%";
    cout.flush();

    setColor(CLR_NORMAL);

    if (percent >= 100)
    {
        cout << "\n";
        cout.flush();
        lastPercent = -1;
        lastLabel = NULL;
    }
}

void compressionProgress(long long current,
                         long long total)
{
    showProgress(current, total, "Compressing");
}

void extractionProgress(long long current,
                        long long total)
{
    showProgress(current, total, "Extracting");
}


void printHeader()
{
    setColor(CLR_TITLE);

    cout << "========================================\n";
    cout << "          FILE COMPRESSOR UTILITY\n";
    cout << "========================================\n";

    setColor(CLR_NORMAL);
}

void printSuccess()
{
    setColor(CLR_INFO);
}

void printError()
{
    setColor(CLR_ERROR);
}

void printMenuItem(const char *number,
                   const char *text)
{
    setColor(CLR_MENU);
    cout << number;
    setColor(CLR_NORMAL);
    cout << ". " << text << "\n";
}


#define APM_MAGIC "apm1"
#define APM_VERSION 1

#define METHOD_STORE   0
#define METHOD_DEFLATE 1
#define METHOD_FOLDER  2
#define APM_FOLDER_VERSION 2

#define BUFFER_SIZE 65536


bool fileExists(const string &path)
{
    ifstream file(path.c_str(), ios::binary);
    return file.good();
}


uint64_t getFileSize(const string &path)
{
    ifstream file(path.c_str(), ios::binary);

    if (!file)
        return 0;

    file.seekg(0, ios::end);

    if (!file)
        return 0;

    return (uint64_t)file.tellg();
}


string trimQuotes(string path)
{
    while (!path.empty() &&
           (path[0] == ' ' || path[0] == '\t'))
    {
        path.erase(0, 1);
    }

    while (!path.empty() &&
           (path[path.size() - 1] == ' ' ||
            path[path.size() - 1] == '\t'))
    {
        path.erase(path.size() - 1);
    }

    if (path.size() >= 2 &&
        path[0] == '"' &&
        path[path.size() - 1] == '"')
    {
        path = path.substr(1, path.size() - 2);
    }

    return path;
}


string getFileName(const string &path)
{
    size_t slash1 = path.find_last_of('\\');
    size_t slash2 = path.find_last_of('/');
    size_t slash = string::npos;

    if (slash1 != string::npos &&
        slash2 != string::npos)
    {
        slash = (slash1 > slash2)
              ? slash1
              : slash2;
    }
    else if (slash1 != string::npos)
    {
        slash = slash1;
    }
    else
    {
        slash = slash2;
    }

    if (slash == string::npos)
        return path;

    return path.substr(slash + 1);
}


string getDirectory(const string &path)
{
    size_t slash1 = path.find_last_of('\\');
    size_t slash2 = path.find_last_of('/');
    size_t slash = string::npos;

    if (slash1 != string::npos &&
        slash2 != string::npos)
    {
        slash = (slash1 > slash2)
              ? slash1
              : slash2;
    }
    else if (slash1 != string::npos)
    {
        slash = slash1;
    }
    else
    {
        slash = slash2;
    }

    if (slash == string::npos)
        return ".";

    return path.substr(0, slash);
}


string makeArchivePath(const string &inputPath)
{
    string directory = getDirectory(inputPath);
    string filename = getFileName(inputPath);

    size_t dot = filename.find_last_of('.');

    string base;

    if (dot != string::npos && dot != 0)
        base = filename.substr(0, dot);
    else
        base = filename;

    if (directory == ".")
        return base + ".apm";

    return directory + "\\" + base + ".apm";
}


string makeUniquePath(const string &directory,
                      const string &filename)
{
    string path = directory + "\\" + filename;

    if (!fileExists(path))
        return path;

    string base = filename;
    string extension;

    size_t dot = filename.find_last_of('.');

    if (dot != string::npos)
    {
        base = filename.substr(0, dot);
        extension = filename.substr(dot);
    }
    else
    {
        extension = "";
    }

    int counter = 1;

    while (true)
    {
        char number[32];

        sprintf(number, "%d", counter);

        string newName =
            base + "_" +
            number +
            extension;

        path = directory + "\\" + newName;

        if (!fileExists(path))
            return path;

        ++counter;
    }
}



bool writeUInt32(ofstream &out, uint32_t value)
{
    unsigned char b[4];

    b[0] = (unsigned char)(value & 0xFFU);
    b[1] = (unsigned char)((value >> 8) & 0xFFU);
    b[2] = (unsigned char)((value >> 16) & 0xFFU);
    b[3] = (unsigned char)((value >> 24) & 0xFFU);

    out.write((char *)b, 4);

    return out.good();
}


bool writeUInt64(ofstream &out, uint64_t value)
{
    unsigned char b[8];
    int i;

    for (i = 0; i < 8; ++i)
    {
        b[i] =
            (unsigned char)
            ((value >> (8 * i)) & 0xFFU);
    }

    out.write((char *)b, 8);

    return out.good();
}


bool readUInt32(ifstream &in, uint32_t &value)
{
    unsigned char b[4];

    in.read((char *)b, 4);

    if (!in)
        return false;

    value =
        ((uint32_t)b[0]) |
        ((uint32_t)b[1] << 8) |
        ((uint32_t)b[2] << 16) |
        ((uint32_t)b[3] << 24);

    return true;
}


bool readUInt64(ifstream &in, uint64_t &value)
{
    unsigned char b[8];
    int i;

    in.read((char *)b, 8);

    if (!in)
        return false;

    value = 0;

    for (i = 0; i < 8; ++i)
    {
        value |=
            ((uint64_t)b[i] << (8 * i));
    }

    return true;
}



uint32_t crc32Update(uint32_t crc,
                     const unsigned char *data,
                     size_t length)
{
    size_t i;
    int bit;

    crc = crc ^ 0xFFFFFFFFUL;

    for (i = 0; i < length; ++i)
    {
        crc ^= (uint32_t)data[i];

        for (bit = 0; bit < 8; ++bit)
        {
            if (crc & 1U)
                crc = (crc >> 1) ^
                      0xEDB88320UL;
            else
                crc >>= 1;
        }
    }

    return crc ^ 0xFFFFFFFFUL;
}


bool calculateCRC32(const string &path,
                    uint32_t &result)
{
    ifstream in(path.c_str(), ios::binary);

    if (!in)
        return false;

    unsigned char buffer[BUFFER_SIZE];

    uint32_t crc = 0;

    while (in)
    {
        in.read(
            (char *)buffer,
            BUFFER_SIZE
        );

        streamsize bytes = in.gcount();

        if (bytes > 0)
        {
            crc = crc32Update(
                crc,
                buffer,
                (size_t)bytes
            );
        }
    }

    result = crc;

    return true;
}



bool compressFile(const string &inputPath)
{
    if (!fileExists(inputPath))
    {
        printError(); cout << "\nError: File does not exist.\n"; setColor(CLR_NORMAL);
        return false;
    }

    string filename =
        getFileName(inputPath);

    string outputPath =
        makeArchivePath(inputPath);

    string temporaryPath =
        outputPath + ".tmp";

    uint64_t originalSize =
        getFileSize(inputPath);

    uint32_t originalCRC;

    cout << "\nAnalyzing file...\n";

    if (!calculateCRC32(
            inputPath,
            originalCRC))
    {
        printError(); cout << "Error: Unable to read file.\n"; setColor(CLR_NORMAL);
        return false;
    }

    remove(temporaryPath.c_str());

    cout << "Compressing using DEFLATE...\n";

    compressionProgress(
        0,
        (long long)originalSize
    );

    if (!DeflateCompressFile(
            inputPath.c_str(),
            temporaryPath.c_str(),
            compressionProgress))
    {
        printError(); cout << "Compression failed.\n"; setColor(CLR_NORMAL);
        remove(temporaryPath.c_str());
        return false;
    }

    uint64_t compressedSize =
        getFileSize(temporaryPath);

    unsigned char method;

    
    if (compressedSize < originalSize)
    {
        method = METHOD_DEFLATE;
    }
    else
    {
        method = METHOD_STORE;

        remove(temporaryPath.c_str());

        ifstream storeIn(
            inputPath.c_str(),
            ios::binary
        );

        ofstream storeOut(
            temporaryPath.c_str(),
            ios::binary
        );

        if (!storeIn || !storeOut)
        {
            storeIn.close();
            storeOut.close();

            printError();
            cout << "Error preparing archive.\n";
            setColor(CLR_NORMAL);

            remove(temporaryPath.c_str());
            return false;
        }

        unsigned char storeBuffer[BUFFER_SIZE];
        uint64_t storeDone = 0;

        extractionProgress(0, (long long)originalSize);

        while (storeIn)
        {
            storeIn.read(
                (char *)storeBuffer,
                BUFFER_SIZE
            );

            streamsize bytes = storeIn.gcount();

            if (bytes > 0)
            {
                storeOut.write(
                    (char *)storeBuffer,
                    bytes
                );

                if (!storeOut)
                {
                    storeIn.close();
                    storeOut.close();
                    remove(temporaryPath.c_str());

                    printError();
                    cout << "Error writing temporary data.\n";
                    setColor(CLR_NORMAL);
                    return false;
                }

                storeDone += (uint64_t)bytes;

                extractionProgress(
                    (long long)storeDone,
                    (long long)originalSize
                );
            }
        }

        storeIn.close();
        storeOut.close();

        compressedSize =
            getFileSize(temporaryPath);
    }



    ofstream archive(
        outputPath.c_str(),
        ios::binary
    );

    if (!archive)
    {
        printError(); cout << "Error: Cannot create output file.\n"; setColor(CLR_NORMAL);
        remove(temporaryPath.c_str());
        return false;
    }

    archive.write(
        APM_MAGIC,
        4
    );

   
    archive.put(
        (char)APM_VERSION
    );

    archive.put(
        (char)method
    );

   
    uint32_t nameLength =
        (uint32_t)filename.length();

    if (!writeUInt32(
            archive,
            nameLength))
    {
        archive.close();
        remove(temporaryPath.c_str());
        remove(outputPath.c_str());
        return false;
    }

    
    if (!writeUInt64(
            archive,
            originalSize))
    {
        archive.close();
        remove(temporaryPath.c_str());
        remove(outputPath.c_str());
        return false;
    }

    
    if (!writeUInt64(
            archive,
            compressedSize))
    {
        archive.close();
        remove(temporaryPath.c_str());
        remove(outputPath.c_str());
        return false;
    }

    
    if (!writeUInt32(
            archive,
            originalCRC))
    {
        archive.close();
        remove(temporaryPath.c_str());
        remove(outputPath.c_str());
        return false;
    }

    
    archive.write(
        filename.c_str(),
        nameLength
    );

    if (!archive)
    {
        archive.close();
        remove(temporaryPath.c_str());
        remove(outputPath.c_str());
        return false;
    }



    ifstream temp(
        temporaryPath.c_str(),
        ios::binary
    );

    if (!temp)
    {
        archive.close();
        remove(temporaryPath.c_str());
        remove(outputPath.c_str());

        printError(); cout << "Error reading temporary data.\n"; setColor(CLR_NORMAL);
        return false;
    }

    char buffer[BUFFER_SIZE];

    while (temp)
    {
        temp.read(
            buffer,
            BUFFER_SIZE
        );

        streamsize bytes = temp.gcount();

        if (bytes > 0)
        {
            archive.write(
                buffer,
                bytes
            );
        }
    }

    temp.close();
    archive.close();

    remove(temporaryPath.c_str());

    if (!fileExists(outputPath))
    {
        printError(); cout << "Error creating archive.\n"; setColor(CLR_NORMAL);
        return false;
    }


   

    double reduction = 0.0;

    if (originalSize > 0)
    {
        reduction =
            (1.0 -
             ((double)compressedSize /
              (double)originalSize))
            * 100.0;
    }

    cout << "\n";
    cout << "========================================\n";
    printSuccess(); cout << "          COMPRESSION COMPLETE\n"; setColor(CLR_NORMAL);
    cout << "========================================\n";

    cout << "File              : "
         << filename << "\n";

    cout << "Original size     : "
         << originalSize
         << " bytes\n";

    cout << "Archive data      : "
         << compressedSize
         << " bytes\n";

    if (reduction >= 0.0)
    {
        cout << "Space saved       : "
             << reduction
             << "%\n";
    }
    else
    {
        cout << "Space saved       : 0%\n";
    }

    cout << "Method            : "
         << (method == METHOD_DEFLATE
             ? "DEFLATE"
             : "STORE")
         << "\n";

    cout << "Output             : "
         << outputPath << "\n";

    cout << "========================================\n";

    return true;
}


string relativePathFromRoot(const string &root,
                            const string &fullPath)
{
    string prefix = root;

    if (!prefix.empty() &&
        prefix[prefix.length() - 1] != '\\')
    {
        prefix += "\\";
    }

    if (fullPath.length() >= prefix.length() &&
        fullPath.compare(
            0,
            prefix.length(),
            prefix) == 0)
    {
        return fullPath.substr(prefix.length());
    }

    return getFileName(fullPath);
}



#ifdef _WIN32

bool isDirectoryPath(const string &path)
{
    DWORD attributes = GetFileAttributesA(path.c_str());

    if (attributes == INVALID_FILE_ATTRIBUTES)
        return false;

    return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}


bool createDirectoryTree(const string &path)
{
    if (path.empty() || path == ".")
        return true;

    if (isDirectoryPath(path))
        return true;

    string parent = getDirectory(path);

    if (parent != path &&
        parent != "." &&
        !createDirectoryTree(parent))
    {
        return false;
    }

    if (CreateDirectoryA(path.c_str(), NULL))
        return true;

    return isDirectoryPath(path);
}


string normalizeRelativePath(const string &path)
{
    string result = path;
    size_t i;

    for (i = 0; i < result.length(); ++i)
    {
        if (result[i] == '/')
            result[i] = '\\';
    }

    return result;
}


bool unsafeRelativePath(const string &path)
{
    string p = normalizeRelativePath(path);

    if (p.empty())
        return true;

    if (p[0] == '\\')
        return true;

    if (p.length() >= 2 && p[1] == ':')
        return true;

    
    if (p == ".." ||
        p.find("\\..\\") != string::npos ||
        p.find("../") != string::npos ||
        p.find("..\\") != string::npos)
    {
        return true;
    }

    return false;
}


int countFolderFiles(const string &folder)
{
    string searchPath = folder + "\\*";
    WIN32_FIND_DATAA data;
    HANDLE handle;
    int count = 0;

    handle = FindFirstFileA(
        searchPath.c_str(),
        &data
    );

    if (handle == INVALID_HANDLE_VALUE)
        return 0;

    do
    {
        string name = data.cFileName;

        if (name == "." || name == "..")
            continue;

        if (data.dwFileAttributes &
            FILE_ATTRIBUTE_REPARSE_POINT)
        {
            continue;
        }

        string fullPath =
            folder + "\\" + name;

        if (data.dwFileAttributes &
            FILE_ATTRIBUTE_DIRECTORY)
        {
            count += countFolderFiles(fullPath);
        }
        else
        {
            ++count;
        }

    } while (FindNextFileA(handle, &data));

    FindClose(handle);

    return count;
}


bool copyTempIntoArchive(
    const string &temporaryPath,
    ofstream &archive)
{
    ifstream temp(
        temporaryPath.c_str(),
        ios::binary
    );

    if (!temp)
        return false;

    char buffer[BUFFER_SIZE];

    while (temp)
    {
        temp.read(
            buffer,
            BUFFER_SIZE
        );

        streamsize bytes =
            temp.gcount();

        if (bytes > 0)
        {
            archive.write(
                buffer,
                bytes
            );

            if (!archive)
            {
                temp.close();
                return false;
            }
        }
    }

    temp.close();
    return true;
}


bool writeFolderFile(
    const string &root,
    const string &filePath,
    ofstream &archive,
    int fileNumber,
    int fileCount)
{
    string relativePath =
        normalizeRelativePath(
            relativePathFromRoot(
                root,
                filePath
            )
        );

    uint64_t originalSize =
        getFileSize(filePath);

    uint32_t originalCRC;

    string temporaryPath =
        filePath + ".apm_tmp";

    remove(temporaryPath.c_str());

    if (!calculateCRC32(
            filePath,
            originalCRC))
    {
        return false;
    }

    cout << "\n";
    setColor(CLR_TITLE);

    cout << "File "
         << fileNumber
         << " of "
         << fileCount
         << ": "
         << relativePath
         << "\n";

    setColor(CLR_NORMAL);

    compressionProgress(
        0,
        (long long)originalSize
    );

    if (!DeflateCompressFile(
            filePath.c_str(),
            temporaryPath.c_str(),
            compressionProgress))
    {
        remove(temporaryPath.c_str());
        return false;
    }

    uint64_t compressedSize =
        getFileSize(temporaryPath);

    unsigned char method;

    
    if (compressedSize < originalSize)
    {
        method = METHOD_DEFLATE;
    }
    else
    {
        method = METHOD_STORE;

        remove(temporaryPath.c_str());

        ifstream source(
            filePath.c_str(),
            ios::binary
        );

        ofstream stored(
            temporaryPath.c_str(),
            ios::binary
        );

        if (!source || !stored)
        {
            source.close();
            stored.close();
            remove(temporaryPath.c_str());
            return false;
        }

        unsigned char buffer[BUFFER_SIZE];
        uint64_t done = 0;

        while (source)
        {
            source.read(
                (char *)buffer,
                BUFFER_SIZE
            );

            streamsize bytes =
                source.gcount();

            if (bytes > 0)
            {
                stored.write(
                    (char *)buffer,
                    bytes
                );

                if (!stored)
                {
                    source.close();
                    stored.close();
                    remove(temporaryPath.c_str());
                    return false;
                }

                done += (uint64_t)bytes;

                showProgress(
                    (long long)done,
                    (long long)originalSize,
                    "Storing"
                );
            }
        }

        source.close();
        stored.close();

        compressedSize =
            getFileSize(temporaryPath);
    }

    uint32_t pathLength =
        (uint32_t)relativePath.length();

    
    if (!writeUInt32(
            archive,
            pathLength) ||
        !writeUInt64(
            archive,
            originalSize) ||
        !writeUInt64(
            archive,
            compressedSize) ||
        !writeUInt32(
            archive,
            originalCRC))
    {
        remove(temporaryPath.c_str());
        return false;
    }

    archive.put((char)method);

    archive.write(
        relativePath.c_str(),
        pathLength
    );

    if (!archive ||
        !copyTempIntoArchive(
            temporaryPath,
            archive))
    {
        remove(temporaryPath.c_str());
        return false;
    }

    remove(temporaryPath.c_str());

    return true;
}


bool addFolderContents(
    const string &root,
    const string &folder,
    ofstream &archive,
    int &fileNumber,
    int fileCount)
{
    string searchPath =
        folder + "\\*";

    WIN32_FIND_DATAA data;

    HANDLE handle =
        FindFirstFileA(
            searchPath.c_str(),
            &data
        );

    if (handle == INVALID_HANDLE_VALUE)
        return false;

    bool result = true;

    do
    {
        string name =
            data.cFileName;

        if (name == "." ||
            name == "..")
            continue;

        if (data.dwFileAttributes &
            FILE_ATTRIBUTE_REPARSE_POINT)
        {
            continue;
        }

        string fullPath =
            folder + "\\" + name;

        if (data.dwFileAttributes &
            FILE_ATTRIBUTE_DIRECTORY)
        {
            if (!addFolderContents(
                    root,
                    fullPath,
                    archive,
                    fileNumber,
                    fileCount))
            {
                result = false;
                break;
            }
        }
        else
        {
            ++fileNumber;

            if (!writeFolderFile(
                    root,
                    fullPath,
                    archive,
                    fileNumber,
                    fileCount))
            {
                result = false;
                break;
            }
        }

    } while (result &&
             FindNextFileA(handle, &data));

    FindClose(handle);

    return result;
}


bool compressFolder(const string &inputPath)
{
    if (!isDirectoryPath(inputPath))
    {
        printError();
        cout << "\nError: Folder does not exist.\n";
        setColor(CLR_NORMAL);
        return false;
    }

    string parent =
        getDirectory(inputPath);

    string folderName =
        getFileName(inputPath);

    if (folderName.empty())
        folderName = "Archive";

    string outputPath =
        makeUniquePath(
            parent,
            folderName + ".apm"
        );

    int fileCount =
        countFolderFiles(inputPath);

    if (fileCount <= 0)
    {
        printError();
        cout << "\nError: Folder contains no files.\n";
        setColor(CLR_NORMAL);
        return false;
    }

    ofstream archive(
        outputPath.c_str(),
        ios::binary
    );

    if (!archive)
    {
        printError();
        cout << "\nError: Cannot create folder archive.\n";
        setColor(CLR_NORMAL);
        return false;
    }

   
    archive.write(
        APM_MAGIC,
        4
    );

    archive.put(
        (char)APM_FOLDER_VERSION
    );

    archive.put(
        (char)METHOD_FOLDER
    );

    if (!writeUInt32(
            archive,
            (uint32_t)fileCount))
    {
        archive.close();
        remove(outputPath.c_str());
        return false;
    }

    cout << "\n";
    printSuccess();

    cout << "========================================\n";
    cout << "          FOLDER COMPRESSION\n";
    cout << "========================================\n";

    setColor(CLR_NORMAL);

    cout << "Folder: "
         << folderName
         << "\n";

    cout << "Files : "
         << fileCount
         << "\n";

    int fileNumber = 0;

    if (!addFolderContents(
            inputPath,
            inputPath,
            archive,
            fileNumber,
            fileCount))
    {
        archive.close();
        remove(outputPath.c_str());

        printError();
        cout << "\nFolder compression failed.\n";
        setColor(CLR_NORMAL);
        return false;
    }

    archive.close();

    cout << "\n";
    printSuccess();

    cout << "========================================\n";
    cout << "       FOLDER COMPRESSION COMPLETE\n";
    cout << "========================================\n";

    setColor(CLR_NORMAL);

    cout << "Folder            : "
         << folderName
         << "\n";

    cout << "Files             : "
         << fileCount
         << "\n";

    cout << "Output            : "
         << outputPath
         << "\n";

    cout << "Archive size      : "
         << getFileSize(outputPath)
         << " bytes\n";

    cout << "========================================\n";

    return true;
}


bool extractFolderArchive(
    ifstream &archive,
    const string &archivePath)
{
    uint32_t fileCount;

    if (!readUInt32(
            archive,
            fileCount) ||
        fileCount == 0)
    {
        printError();
        cout << "\nError: Invalid folder archive.\n";
        setColor(CLR_NORMAL);
        return false;
    }

    string parent =
        getDirectory(archivePath);

    string archiveName =
        getFileName(archivePath);

    size_t dot =
        archiveName.find_last_of('.');

    string folderName =
        (dot != string::npos)
        ? archiveName.substr(0, dot)
        : archiveName;

    string outputRoot =
        makeUniquePath(
            parent,
            folderName
        );

    if (!createDirectoryTree(outputRoot))
    {
        printError();
        cout << "\nError: Cannot create extraction folder.\n";
        setColor(CLR_NORMAL);
        return false;
    }

    cout << "\n";
    setColor(CLR_INFO);

    cout << "Extracting folder: "
         << folderName
         << "\n";

    setColor(CLR_NORMAL);

    uint32_t i;

    for (i = 0; i < fileCount; ++i)
    {
        uint32_t pathLength;
        uint64_t originalSize;
        uint64_t compressedSize;
        uint32_t storedCRC;
        int method;

        if (!readUInt32(
                archive,
                pathLength) ||
            !readUInt64(
                archive,
                originalSize) ||
            !readUInt64(
                archive,
                compressedSize) ||
            !readUInt32(
                archive,
                storedCRC))
        {
            printError();
            cout << "\nError: Damaged folder archive.\n";
            setColor(CLR_NORMAL);
            return false;
        }

        method = archive.get();

        if (pathLength == 0 ||
            pathLength > 32768 ||
            (method != METHOD_STORE &&
             method != METHOD_DEFLATE))
        {
            printError();
            cout << "\nError: Invalid folder entry.\n";
            setColor(CLR_NORMAL);
            return false;
        }

        string relativePath;

        relativePath.resize(
            pathLength
        );

        archive.read(
            &relativePath[0],
            pathLength
        );

        if (!archive ||
            unsafeRelativePath(
                relativePath))
        {
            printError();
            cout << "\nError: Unsafe or damaged path in archive.\n";
            setColor(CLR_NORMAL);
            return false;
        }

        string outputPath =
            outputRoot + "\\" +
            normalizeRelativePath(
                relativePath
            );

        string outputDirectory =
            getDirectory(outputPath);

        if (!createDirectoryTree(
                outputDirectory))
        {
            printError();
            cout << "\nError: Cannot create output directory.\n";
            setColor(CLR_NORMAL);
            return false;
        }

        string temporaryPath =
            outputPath + ".tmp";

        string compressedTemporary =
            temporaryPath + ".deflate";

        remove(temporaryPath.c_str());
        remove(compressedTemporary.c_str());

        cout << "\nFile "
             << (i + 1)
             << " of "
             << fileCount
             << ": "
             << relativePath
             << "\n";

        bool success = false;

        if (method == METHOD_STORE)
        {
            ofstream out(
                temporaryPath.c_str(),
                ios::binary
            );

            if (!out)
                return false;

            uint64_t remaining =
                compressedSize;

            uint64_t done = 0;

            unsigned char buffer[BUFFER_SIZE];

            extractionProgress(
                0,
                (long long)originalSize
            );

            while (remaining > 0)
            {
                uint32_t chunk =
                    (remaining > BUFFER_SIZE)
                    ? BUFFER_SIZE
                    : (uint32_t)remaining;

                archive.read(
                    (char *)buffer,
                    chunk
                );

                if ((uint32_t)archive.gcount()
                    != chunk)
                {
                    out.close();
                    remove(temporaryPath.c_str());
                    return false;
                }

                out.write(
                    (char *)buffer,
                    chunk
                );

                if (!out)
                {
                    out.close();
                    remove(temporaryPath.c_str());
                    return false;
                }

                remaining -= chunk;
                done += chunk;

                extractionProgress(
                    (long long)done,
                    (long long)originalSize
                );
            }

            out.close();

            success = true;
        }
        else
        {
            
            ofstream temp(
                compressedTemporary.c_str(),
                ios::binary
            );

            if (!temp)
                return false;

            uint64_t remaining =
                compressedSize;

            unsigned char buffer[BUFFER_SIZE];

            while (remaining > 0)
            {
                uint32_t chunk =
                    (remaining > BUFFER_SIZE)
                    ? BUFFER_SIZE
                    : (uint32_t)remaining;

                archive.read(
                    (char *)buffer,
                    chunk
                );

                if ((uint32_t)archive.gcount()
                    != chunk)
                {
                    temp.close();
                    remove(compressedTemporary.c_str());
                    return false;
                }

                temp.write(
                    (char *)buffer,
                    chunk
                );

                if (!temp)
                {
                    temp.close();
                    remove(compressedTemporary.c_str());
                    return false;
                }

                remaining -= chunk;
            }

            temp.close();

            success =
                DeflateDecompressFile(
                    compressedTemporary.c_str(),
                    temporaryPath.c_str(),
                    (long long)originalSize,
                    extractionProgress
                );

            remove(compressedTemporary.c_str());
        }

        if (!success)
        {
            remove(temporaryPath.c_str());

            printError();
            cout << "Extraction failed.\n";
            setColor(CLR_NORMAL);
            return false;
        }

        uint64_t extractedSize =
            getFileSize(
                temporaryPath
            );

        if (extractedSize != originalSize)
        {
            remove(temporaryPath.c_str());

            printError();
            cout << "Size verification failed.\n";
            setColor(CLR_NORMAL);
            return false;
        }

        uint32_t extractedCRC;

        if (!calculateCRC32(
                temporaryPath,
                extractedCRC) ||
            extractedCRC != storedCRC)
        {
            remove(temporaryPath.c_str());

            printError();
            cout << "Integrity verification failed.\n";
            setColor(CLR_NORMAL);
            return false;
        }

        remove(outputPath.c_str());

        if (rename(
                temporaryPath.c_str(),
                outputPath.c_str()) != 0)
        {
            remove(temporaryPath.c_str());

            printError();
            cout << "Error creating extracted file.\n";
            setColor(CLR_NORMAL);
            return false;
        }

        printSuccess();
        cout << "  Verified\n";
        setColor(CLR_NORMAL);
    }

    cout << "\n";
    printSuccess();

    cout << "========================================\n";
    cout << "       FOLDER EXTRACTION COMPLETE\n";
    cout << "========================================\n";

    setColor(CLR_NORMAL);

    cout << "Files extracted   : "
         << fileCount
         << "\n";

    cout << "Output folder     : "
         << outputRoot
         << "\n";

    cout << "Integrity         : VERIFIED\n";
    cout << "========================================\n";

    return true;
}

#endif 

bool extractFile(const string &archivePath)
{
    if (!fileExists(archivePath))
    {
        cout << "\nError: Archive does not exist.\n";
        return false;
    }

    ifstream archive(
        archivePath.c_str(),
        ios::binary
    );

    if (!archive)
    {
        printError(); cout << "Error opening archive.\n"; setColor(CLR_NORMAL);
        return false;
    }



    char magic[5];

    archive.read(
        magic,
        4
    );

    magic[4] = '\0';

    if (!archive ||
        strcmp(magic, APM_MAGIC) != 0)
    {
        cout << "\nError: Invalid apm archive.\n";
        return false;
    }


  

    int version =
        archive.get();

   
    if (version != APM_VERSION &&
        version != APM_FOLDER_VERSION)
    {
        printError();
        cout << "Error: Unsupported apm version.\n";
        setColor(CLR_NORMAL);
        return false;
    }


   
    int method =
        archive.get();

    if (method == EOF)
    {
        printError(); cout << "Error: Damaged archive.\n"; setColor(CLR_NORMAL);
        return false;
    }

#ifdef _WIN32
    if (version == APM_FOLDER_VERSION &&
        method == METHOD_FOLDER)
    {
        return extractFolderArchive(
            archive,
            archivePath
        );
    }
#endif

   
    if (version != APM_VERSION)
    {
        printError();
        cout << "Error: Invalid apm archive format.\n";
        setColor(CLR_NORMAL);
        return false;
    }


   
    uint32_t nameLength;
    uint64_t originalSize;
    uint64_t compressedSize;
    uint32_t storedCRC;

    if (!readUInt32(
            archive,
            nameLength) ||
        !readUInt64(
            archive,
            originalSize) ||
        !readUInt64(
            archive,
            compressedSize) ||
        !readUInt32(
            archive,
            storedCRC))
    {
        printError(); cout << "Error: Damaged archive header.\n"; setColor(CLR_NORMAL);
        return false;
    }

    if (nameLength == 0 ||
        nameLength > 4096)
    {
        printError(); cout << "Error: Invalid filename information.\n"; setColor(CLR_NORMAL);
        return false;
    }


   
    string filename;

    filename.resize(nameLength);

    archive.read(
        &filename[0],
        nameLength
    );

    if (!archive)
    {
        printError(); cout << "Error: Damaged archive.\n"; setColor(CLR_NORMAL);
        return false;
    }


    
    if (filename.find('\\') != string::npos ||
        filename.find('/') != string::npos ||
        filename.find(':') != string::npos ||
        filename == "." ||
        filename == "..")
    {
        printError(); cout << "Error: Unsafe filename in archive.\n"; setColor(CLR_NORMAL);
        return false;
    }


    string directory =
        getDirectory(archivePath);

    string outputPath =
        makeUniquePath(
            directory,
            filename
        );


   

    string temporaryPath =
        outputPath + ".tmp";

    remove(temporaryPath.c_str());

    bool success = false;

    cout << "\nExtracting...\n";

    extractionProgress(
        0,
        (long long)originalSize
    );

    if (method == METHOD_STORE)
    {
        ifstream source(
            archivePath.c_str(),
            ios::binary
        );

        if (!source)
        {
            printError(); cout << "Error opening archive.\n"; setColor(CLR_NORMAL);
            return false;
        }

        
        source.seekg(
            30 + (streamoff)nameLength,
            ios::beg
        );

        ofstream out(
            temporaryPath.c_str(),
            ios::binary
        );

        if (!out)
        {
            printError(); cout << "Cannot create output file.\n"; setColor(CLR_NORMAL);
            return false;
        }

        unsigned char buffer[BUFFER_SIZE];
        uint64_t remaining =
            compressedSize;

        while (remaining > 0)
        {
            uint32_t chunk =
                (remaining > BUFFER_SIZE)
                ? BUFFER_SIZE
                : (uint32_t)remaining;

            source.read(
                (char *)buffer,
                chunk
            );

            if ((uint32_t)source.gcount()
                != chunk)
            {
                out.close();
                remove(temporaryPath.c_str());

                printError(); cout << "Archive data is incomplete.\n"; setColor(CLR_NORMAL);
                return false;
            }

            out.write(
                (char *)buffer,
                chunk
            );

            if (!out)
            {
                out.close();
                remove(temporaryPath.c_str());

                cout << "Error writing extracted data.\n";
                return false;
            }

            remaining -= chunk;

            extractionProgress(
                (long long)(compressedSize - remaining),
                (long long)originalSize
            );
        }

        out.close();
        source.close();

        success = true;
    }
    else if (method == METHOD_DEFLATE){
        

        string compressedTemporary = temporaryPath + ".deflate";

        remove(compressedTemporary.c_str());

        ifstream source(archivePath.c_str(),ios::binary);

        ofstream compressedOut(compressedTemporary.c_str(),ios::binary);

        if (!source || !compressedOut){
            source.close();
            compressedOut.close();

            remove(compressedTemporary.c_str());

            cout << "Unable to prepare compressed data.\n";
            return false;
        }

        source.seekg(30 + (streamoff)nameLength,ios::beg);

        unsigned char buffer[BUFFER_SIZE];
        uint64_t remaining = compressedSize;

        while (remaining > 0){
            uint32_t chunk = (remaining > BUFFER_SIZE) ? BUFFER_SIZE : (uint32_t)remaining;

            source.read((char *)buffer,chunk);

            if ((uint32_t)source.gcount()!= chunk){
                source.close();
                compressedOut.close();

                remove(compressedTemporary.c_str());

                printError(); cout << "Archive data is incomplete.\n"; setColor(CLR_NORMAL);
                return false;
            }

            compressedOut.write((char *)buffer,chunk);

            if (!compressedOut){
                source.close();
                compressedOut.close();

                remove(compressedTemporary.c_str());

                cout << "Error preparing compressed data.\n";
                return false;
            }

            remaining -= chunk;
        }

        source.close();
        compressedOut.close();

        success = DeflateDecompressFile(
            compressedTemporary.c_str(),
            temporaryPath.c_str(),
            (long long)originalSize,
            extractionProgress
        );

        remove(compressedTemporary.c_str());
    }
    else{
        printError(); cout << "Error: Unknown compression method.\n"; setColor(CLR_NORMAL);
        return false;
    }


    if (!success){
        remove(temporaryPath.c_str());

        printError(); cout << "Extraction failed.\n"; setColor(CLR_NORMAL);
        return false;
    }


   

    uint64_t extractedSize = getFileSize(temporaryPath);

    if (extractedSize != originalSize){
        remove(temporaryPath.c_str());

        cout << "\nERROR: Extracted size does not match.\n";
        printError(); cout << "The archive may be corrupted.\n"; setColor(CLR_NORMAL);

        return false;
    }


    

    uint32_t extractedCRC;

    if (!calculateCRC32(temporaryPath,extractedCRC)){
        remove(temporaryPath.c_str());

        printError(); cout << "Integrity check failed.\n"; setColor(CLR_NORMAL);
        return false;
    }

    if (extractedCRC != storedCRC){
    	
        remove(temporaryPath.c_str());

        printError(); cout << "\nERROR: Archive integrity check failed.\n"; setColor(CLR_NORMAL);
        printError(); cout << "The archive may be corrupted.\n"; setColor(CLR_NORMAL);

        return false;
    }


   
    remove(outputPath.c_str());

    if (rename(temporaryPath.c_str(),outputPath.c_str()) != 0){
        /*
            rename() can fail on some Windows setups if the
            destination exists. Try removing and renaming once.
        */
        remove(outputPath.c_str());

        if (rename(temporaryPath.c_str(),outputPath.c_str()) != 0){

            remove(temporaryPath.c_str());

            cout << "Error creating extracted file.\n";
            return false;
        }
    }


   

    cout << "\n";
    cout << "========================================\n";
    printSuccess(); cout << "          EXTRACTION COMPLETE\n"; setColor(CLR_NORMAL);
    cout << "========================================\n";

    cout << "File              : "
         << filename << "\n";

    cout << "Original size     : "
         << originalSize
         << " bytes\n";

    printSuccess(); cout << "Integrity         : VERIFIED\n"; setColor(CLR_NORMAL);

    cout << "Output            : "
         << outputPath << "\n";

    cout << "========================================\n";
    return true;
}




void showMenu(){
    
	clearScreen();

    printHeader();

    cout << "\n";

    printMenuItem("1", "Compress a file");
    printMenuItem("2", "Compress a folder");
    printMenuItem("3", "Extract an apm file");
    printMenuItem("4", "Exit");

    cout << "\n";
    setColor(CLR_INPUT);
    cout << "Enter your choice: ";
    setColor(CLR_NORMAL);
}




int main()
{
    while (true){
        showMenu();

        string choice;

        getline(cin,choice);

        if (choice == "1"){
            string path;

            setColor(CLR_INPUT); cout << "\nEnter the path of the file:\n"; setColor(CLR_NORMAL);

            getline(cin,path);

            path = trimQuotes(path);

            compressFile(path);
        }
        
        else if (choice == "2"){
#ifdef _WIN32
            string path;

            setColor(CLR_INPUT);
            cout << "\nEnter the path of the folder:\n";
            setColor(CLR_NORMAL);

            getline(cin,path);

            path = trimQuotes(path);

            compressFolder(path);
#else
            printError();
            cout << "\nFolder compression is supported on Windows only.\n";
            setColor(CLR_NORMAL);
#endif
        }

        else if (choice == "3"){
            string path;

            setColor(CLR_INPUT);
            cout << "\nEnter the path of the apm file:\n";
            setColor(CLR_NORMAL);

            getline(cin,path);

            path = trimQuotes(path);

            extractFile(path);
        }

        else if (choice == "4"){
            setColor(CLR_DIM);
            cout << "\nExiting...\n";
            setColor(CLR_NORMAL);
            break;
        }
        
        else{
            printError(); cout << "\nInvalid choice. Please try again.\n"; setColor(CLR_NORMAL);
        }

        cout << "\nPress ENTER to continue...";
        cin.get();
    }
    return 0;
}
