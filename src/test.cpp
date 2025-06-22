#include <iostream>
#include <string>
#include <filesystem> // For std::filesystem::path (C++17 and later)

// --- Option 1: Using std::string manipulation (works for all C++ versions) ---
std::string modifyFilenameString(const std::string& filename, const std::string& suffix) {
    size_t lastDotPos = filename.rfind('.');
    if (lastDotPos == std::string::npos) { // No extension
        return filename + suffix;
    }
    std::string baseName = filename.substr(0, lastDotPos);
    std::string extension = filename.substr(lastDotPos);
    return baseName + suffix + extension;
}

// --- Option 2: Using std::filesystem (C++17 and later, more robust) ---
std::string modifyFilenameFilesystem(const std::string& filenameStr, const std::string& suffix) {
    std::filesystem::path filePath(filenameStr);
    std::string stem = filePath.stem().string(); // Gets the filename without the extension
    std::string extension = filePath.extension().string(); // Gets the extension (including the dot)
    return stem + suffix + extension;
}

int main(int argc, char *argv[]) {
    std::string originalFilename = argv[1]; //"dump_proxy_all.bin";
    std::string suffixToAppend = "_10p";

    // Using string manipulation
    std::string modifiedFilename1 = modifyFilenameString(originalFilename, suffixToAppend);
    std::cout << "Original (string): " << originalFilename << std::endl;
    std::cout << "Modified (string): " << modifiedFilename1 << std::endl;

    std::cout << "---" << std::endl;

    // Using std::filesystem (C++17 and later)
    // std::string modifiedFilename2 = modifyFilenameFilesystem(originalFilename, suffixToAppend);
    // std::cout << "Original (filesystem): " << originalFilename << std::endl;
    // std::cout << "Modified (filesystem): " << modifiedFilename2 << std::endl;

    // // Example with no extension
    // std::string noExtFilename = "archive_data";
    // std::string modifiedNoExt1 = modifyFilenameString(noExtFilename, "_v2");
    // std::cout << "\nOriginal (no ext, string): " << noExtFilename << std::endl;
    // std::cout << "Modified (no ext, string): " << modifiedNoExt1 << std::endl;

    // std::string modifiedNoExt2 = modifyFilenameFilesystem(noExtFilename, "_v2");
    // std::cout << "Original (no ext, filesystem): " << noExtFilename << std::endl;
    // std::cout << "Modified (no ext, filesystem): " << modifiedNoExt2 << std::endl;


    return 0;
}