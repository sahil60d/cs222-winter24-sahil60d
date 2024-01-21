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
            //link handle to file
            if (fileHandle.initFileHandle(fileName) == FAILURE) {
                return FAILURE;
            }
            //Set counters
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
        //Reserve first page for counters
        //pageNum++;

        //Move file pointer to appropriate page
        fseek(fptr, pageNum*PAGE_SIZE, SEEK_SET);

        //Read page into data buffer; handle error
        if (fread(data, 1, PAGE_SIZE, fptr) != PAGE_SIZE) {
            return FAILURE;
        }

        readPageCounter++;
        return SUCCESS;
    }

    RC FileHandle::writePage(PageNum pageNum, const void *data) {
        //Reserve first page for counters
        //pageNum++;

        //Move file pointer to appropriate page
        fseek(fptr, pageNum*PAGE_SIZE, SEEK_SET);

        //Write data into page; handle error if page doesn't exist
        if (fwrite(data, 1, PAGE_SIZE, fptr) != PAGE_SIZE) {
            return FAILURE;
        }

        writePageCounter++;
        return SUCCESS;
    }

    RC FileHandle::appendPage(const void *data) {
        //Move file pointer to end of file
        fseek(fptr, 0, SEEK_END);

        //Write data; handle error
        if (fwrite(data, 1, PAGE_SIZE, fptr) != PAGE_SIZE) {
            return FAILURE;
        }

        appendPageCounter++;
        return SUCCESS;
    }

    unsigned FileHandle::getNumberOfPages() {
        return appendPageCounter;
    }

    RC FileHandle::collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount) {
        readPageCount = readPageCounter;
        writePageCount = writePageCounter;
        appendPageCount = appendPageCounter;
        return SUCCESS;
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