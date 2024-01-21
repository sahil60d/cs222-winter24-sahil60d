#include "src/include/pfm.h"

namespace PeterDB {
    PagedFileManager &PagedFileManager::instance() {
        static PagedFileManager _pf_manager = PagedFileManager();
        return _pf_manager;
    }

    PagedFileManager::PagedFileManager() = default;

    PagedFileManager::~PagedFileManager() = default;

    PagedFileManager::PagedFileManager(const PagedFileManager &) = default;

    PagedFileManager &PagedFileManager::operator=(const PagedFileManager &) = default;

    RC PagedFileManager::createFile(const std::string &fileName) {
        //Create File pointer
        FILE* file;
        if (file = std::fopen(fileName.c_str(), "r")) {      //File already exists
            std::fclose(file);
            return FAILURE;
        } else {                                            //FiLE name doesn't exist
            //Create file, handle fail
            if (file = std::fopen(fileName.c_str(), "w")) {
                std::fclose(file);
                return SUCCESS;
            } else {
                return FAILURE;
            }
        }
    }

    RC PagedFileManager::destroyFile(const std::string &fileName) {
        //Delete file, handle fail
        if (std::remove(fileName.c_str()) != 0) {
            //Fails to delete file
            return FAILURE;
        }
        //Successfully delete file
        return SUCCESS;
    }

    RC PagedFileManager::openFile(const std::string &fileName, FileHandle &fileHandle) {
        //Check if file exists
        if (access(fileName.c_str(), F_OK) == 0) {
            if (fileHandle.initFileHandle(fileName) == FAILURE) {
                return FAILURE;
            }
            fileHandle.readPageCounter = 0;
            fileHandle.writePageCounter = 0;
            fileHandle.appendPageCounter = 0;
            return SUCCESS;
        }
        //File doesn't exist
        return FAILURE;
    }

    RC PagedFileManager::closeFile(FileHandle &fileHandle) {
        return fileHandle.closeFileHandle();
    }

    FileHandle::FileHandle() {
        readPageCounter = 0;
        writePageCounter = 0;
        appendPageCounter = 0;
    }

    FileHandle::~FileHandle() = default;

    RC FileHandle::readPage(PageNum pageNum, void *data) {
        return -1;
    }

    RC FileHandle::writePage(PageNum pageNum, const void *data) {
        return -1;
    }

    RC FileHandle::appendPage(const void *data) {
        return -1;
    }

    unsigned FileHandle::getNumberOfPages() {
        return -1;
    }

    RC FileHandle::collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount) {
        return -1;
    }

    RC FileHandle::checkFptr() {
        if (!fptr) {
            return SUCCESS;
        }
        return FAILURE;
    }

    RC FileHandle::initFileHandle(const std::string &fileName) {
            if (fptr) {
                return FAILURE;
            }

            fptr = std::fopen(fileName.c_str(), "r+");

            //Check for fails: handle in use or file fails to open
            if (!fptr) {
                return FAILURE;
            }
            return SUCCESS;
    }

    RC FileHandle::closeFileHandle() {
        if (fclose(fptr) != 0) {
            return FAILURE;
        }
        return SUCCESS;
    }

} // namespace PeterDB