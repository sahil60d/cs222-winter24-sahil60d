#include "src/include/rbfm.h"

namespace PeterDB {
    RecordBasedFileManager &RecordBasedFileManager::instance() {
        static RecordBasedFileManager _rbf_manager = RecordBasedFileManager();
        return _rbf_manager;
    }

    RecordBasedFileManager::RecordBasedFileManager() = default;

    RecordBasedFileManager::~RecordBasedFileManager() = default;

    RecordBasedFileManager::RecordBasedFileManager(const RecordBasedFileManager &) = default;

    RecordBasedFileManager &RecordBasedFileManager::operator=(const RecordBasedFileManager &) = default;

    RC RecordBasedFileManager::createFile(const std::string &fileName) {
        return pfm->createFile(fileName);
    }

    RC RecordBasedFileManager::destroyFile(const std::string &fileName) {
        return pfm->destroyFile(fileName);
    }

    RC RecordBasedFileManager::openFile(const std::string &fileName, FileHandle &fileHandle) {
        return pfm->openFile(fileName, fileHandle);
    }

    RC RecordBasedFileManager::closeFile(FileHandle &fileHandle) {
        return pfm->closeFile(fileHandle);
    }

    RC RecordBasedFileManager::insertRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                            const void *data, RID &rid) {
        //Fails if record descriptor is empty
        if (recordDescriptor.size() == 0) {
            return FAILURE;
        }

        //calc record size
        int dataSize = 0;

        //calc num bytes for null indicator
        double numFields = recordDescriptor.size();
        double nbytes = (int)ceil(numFields/8);
        dataSize += nbytes;

        void* b = malloc(nbytes);
        memcpy(b, data, nbytes);
        //calc size of data
        for(int i = 0; i < recordDescriptor.size(); i++) {
            if (checkBit((char*)b, nbytes, i) == true) {
                continue;
            }
            if (recordDescriptor[i].type == TypeVarChar) {
                //calc size of vachar
                unsigned int varcharSize = *(unsigned int*)((char*)data + dataSize);
                dataSize += varcharSize;
            }
            //int/real/varchar all size 4
            dataSize += 4;
        }
        free(b);
        //find page with enough space
        PageNum pageNum = findPage(fileHandle, dataSize);

        //insert data into page
        void* pageBuffer = malloc(PAGE_SIZE);                                           //read page to buffer
        if (fileHandle.readPage(pageNum-1, pageBuffer) == FAILURE) {
            return FAILURE;
        }

        char* filePtr = (char*)pageBuffer;                                                  //Create file pointer
        char* tempPageInfo = filePtr + (PAGE_SIZE - sizeof(PageInfo));                      //Create pageInfo pointer
        PageInfo* pageInfo = (PageInfo*)tempPageInfo;

        Slot* slotDirectory;
        // look for empty slots
        if (pageInfo->emptySlots >= 1) {                                                    // reuse a slot
            unsigned i = 1;
            char* checkSlotptr = filePtr + (PAGE_SIZE - sizeof(PageInfo) - sizeof(Slot));
            while (i <= pageInfo->numSlots) {
                Slot* checkSlot = (Slot*)checkSlotptr;
                if (checkSlot->offset == -1 && checkSlot->length == -1) {                   // found an empty slot
                    break;
                } else {
                    checkSlotptr -= sizeof(Slot);
                    i++;
                }
            }
            slotDirectory = (Slot*)checkSlotptr;                                            // use existing slot

            rid.pageNum = pageNum;
            rid.slotNum = i;

            pageInfo->emptySlots--;                                                         // update number of empty slots in page
        } else {                                                                            // make a new slot
            pageInfo->numSlots++;
            char* tempSdirectory = filePtr + (PAGE_SIZE - sizeof(PageInfo) - pageInfo->numSlots*sizeof(Slot));     //Create slotDirectory pointer
            slotDirectory = (Slot*)tempSdirectory;

            rid.pageNum = pageNum;
            rid.slotNum = pageInfo->numSlots;
        }

        filePtr += pageInfo->freeSpaceOffset;                                               //move filePtr to start of free space
        memcpy(filePtr, data, dataSize);                                        //write record data to page
        //slotDirectory -= sizeof(Slot);                                                      //add slot to page
        slotDirectory->length = dataSize;
        slotDirectory->offset = pageInfo->freeSpaceOffset;
        pageInfo->freeSpaceOffset += dataSize;                                              //update page info

        //write page into file
        fileHandle.writePage(pageNum-1, pageBuffer);
        free(pageBuffer);
        return SUCCESS;
    }

    RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                          const RID &rid, void *data) {
        void* pageBuffer = malloc(PAGE_SIZE);
        if (fileHandle.readPage(rid.pageNum-1, pageBuffer) == FAILURE) {                      //Handle error
            return FAILURE;
        }

        char* filePtr = (char*)pageBuffer;
        char* temppageinfo = filePtr + (PAGE_SIZE - sizeof(PageInfo));
        PageInfo* pageInfo = (PageInfo*)temppageinfo;

        if (rid.slotNum > pageInfo->numSlots) {
            return FAILURE;
        }

        char* tempSdirectory = filePtr + (PAGE_SIZE - sizeof(PageInfo) - sizeof(Slot)*rid.slotNum);
        Slot* slotDirectory = (Slot*)tempSdirectory;

        // Slot unused
        if (slotDirectory->offset == -1 && slotDirectory->length == -1) {
            return FAILURE;
        }

        filePtr += slotDirectory->offset;
        memcpy(data, filePtr, slotDirectory->length);
        free(pageBuffer);
        return SUCCESS;
    }

    RC RecordBasedFileManager::deleteRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                            const RID &rid) {
        void* pageBuffer = malloc(PAGE_SIZE);
        if (fileHandle.readPage(rid.pageNum-1, pageBuffer) == FAILURE) {
            return FAILURE;
        }

        char* filePtr = (char*)pageBuffer;
        char* temppageinfo = filePtr + (PAGE_SIZE - sizeof(PageInfo));
        PageInfo* pageInfo = (PageInfo*)temppageinfo;                                   // Page info

        char* tempSdir = filePtr + (PAGE_SIZE - sizeof(PageInfo) - sizeof(Slot)*rid.slotNum);
        Slot* slotDir = (Slot*)tempSdir;                                           // slot directory of deleted record

        // case of first,only or last record
        if (rid.slotNum == 1 && pageInfo->numSlots == 1) {                             // only record
            // reset page information
            //pageInfo->numSlots = 0;
            pageInfo->freeSpaceOffset = 0;
        } else if (rid.slotNum == pageInfo->numSlots) {                                // last record
            // reset page information
            pageInfo->freeSpaceOffset -= slotDir->length;                              // move free space offset pointer to after previous record
            //pageInfo->numSlots--;                                                      // removes last slot from directory
        } else {
            // get old slot info
            //char* oldRecord = filePtr + slotDir->offset;                               // record to be deleted
            char* tempNextSdir = tempSdir - sizeof(Slot);
            Slot* nextSlotDir = (Slot*) tempNextSdir;                                  // slot directory of next record

            // get size of records to be moved
            unsigned sizeRecords = pageInfo->freeSpaceOffset - nextSlotDir->offset;

            // get record pointers
            filePtr += slotDir->offset;                                                 // record to be deleted
            char* moveRecords = (char*)pageBuffer;                                      // new pointer of record to move
            moveRecords += nextSlotDir->offset;                                         // records to be moved

            memcpy(filePtr, moveRecords, sizeRecords);                      // move records up
            pageInfo->freeSpaceOffset -= slotDir->length;                               // move free space offset

            // update record offsets
            for (int i = rid.slotNum + 1; i <= pageInfo->numSlots; i++) {
                nextSlotDir = (Slot*) tempNextSdir;
                nextSlotDir->offset -= slotDir->length;
                tempNextSdir -= sizeof(Slot);
            }
        }

        // update deleted slot
        slotDir->offset = -1;
        slotDir->length = -1;

        pageInfo->emptySlots++;                                                     // update number of empty slots in page

        // write page to file
        fileHandle.writePage(rid.pageNum-1, pageBuffer);
        free(pageBuffer);
        return SUCCESS;
    }

    RC RecordBasedFileManager::printRecord(const std::vector<Attribute> &recordDescriptor, const void *data,
                                           std::ostream &out) {
        //calc num bytes for null indicator
        double numFields = recordDescriptor.size();
        double nbytes = ceil(numFields/8);

        void* dest = malloc(nbytes);
        memcpy(dest, data, nbytes);
        char nullin = *(char*)dest;
        char* dataPtr = (char*)data;
        dataPtr += (int)nbytes;
        for(int i = 0; i < recordDescriptor.size(); i++) {
            if (i != 0) {
                out.write(", ", 2);
            }

            if (checkBit((char*)dest, nbytes, i) == true) {
                out << recordDescriptor[i].name << ": NULL";
                continue;
            }

            /*
            if ((nullin & ((1<<(7-i)))) != 0) {if ((nullin & ((1<<(7-i)))) != 0) {
                out << recordDescriptor[i].name << ": NULL";
                continue;
            }
            */
            unsigned int l;
            switch(recordDescriptor[i].type) {
                case TypeInt:
                    out << recordDescriptor[i].name << ": " << *(int*)dataPtr;
                    dataPtr += 4;
                    break;
                case TypeReal:
                    out << recordDescriptor[i].name << ": " << *(float*)dataPtr;
                    dataPtr += 4;
                    break;
                case TypeVarChar:
                    l = *(unsigned int*)(dataPtr);
                    dataPtr += 4;
                    out << recordDescriptor[i].name << ": ";
                    out.write(dataPtr, l);
                    dataPtr += l;
                    //out << std::endl;
                    break;
                default:
                    free(dest);
                    return FAILURE;
            }
        }
        out << std::endl;
        free(dest);
        return SUCCESS;
    }

    RC RecordBasedFileManager::updateRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                            const void *data, const RID &rid) {
        return -1;
    }

    RC RecordBasedFileManager::readAttribute(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                             const RID &rid, const std::string &attributeName, void *data) {
        return -1;
    }

    RC RecordBasedFileManager::scan(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                    const std::string &conditionAttribute, const CompOp compOp, const void *value,
                                    const std::vector<std::string> &attributeNames,
                                    RBFM_ScanIterator &rbfm_ScanIterator) {
        return -1;
    }

    PageNum RecordBasedFileManager::findPage(FileHandle &fileHandle, int size) {
        PageNum pageNum = fileHandle.getNumberOfPages();
        if (pageNum == 0) {
            return newPage(fileHandle);
        }
        //Check current page
        void* pageBuffer = malloc(PAGE_SIZE);
        if (fileHandle.readPage(pageNum-1, pageBuffer) == FAILURE) {
            free(pageBuffer);
            return FAILURE;
        }
        //Find free space
        char* tempPtr = (char*)pageBuffer + (PAGE_SIZE - sizeof(PageInfo));
        PageInfo* pageInfo = (PageInfo*)tempPtr;
        int availSize = PAGE_SIZE - (pageInfo->freeSpaceOffset + pageInfo->numSlots*sizeof(Slot) + sizeof(Slot) + sizeof(PageInfo));
        free(pageBuffer);
        if (size <= availSize) {
            //use current page
            //free(pageBuffer);
            return pageNum;
        } else {                    //Look through directory
            void* pageBuffer = malloc(PAGE_SIZE);
            for(unsigned i = 0; i <= fileHandle.getNumberOfPages()-1; i++) {
                //void* pageBuffer = malloc(PAGE_SIZE);
                fileHandle.readPage(i, pageBuffer);
                char*tempPtr = (char*)pageBuffer + (PAGE_SIZE - sizeof(PageInfo));
                PageInfo* pageInfo = (PageInfo*)tempPtr;
                int availSize = PAGE_SIZE - (pageInfo->freeSpaceOffset + pageInfo->numSlots*sizeof(Slot) + sizeof(Slot) + sizeof(PageInfo));
                if (size <= availSize) {
                    free(pageBuffer);
                    return i+1;
                }
            }
            //If no space create new page and return new page number
            free(pageBuffer);
            return newPage(fileHandle);
        }
    }

    PageNum RecordBasedFileManager::newPage(FileHandle &fileHandle) {
        //Append new page
        void* newPage = malloc(PAGE_SIZE);
        fileHandle.appendPage(newPage);

        //Add slot info to page
        char* slotDirectoryPtr = (char*)newPage;
        //move pointer to appropriate position
        slotDirectoryPtr += PAGE_SIZE - sizeof(PageInfo);
        //Create page info
        PageInfo pageInfo;
        pageInfo.freeSpaceOffset = 0;
        pageInfo.numSlots = 0;
        pageInfo.emptySlots = 0;
        memcpy(slotDirectoryPtr, &pageInfo, sizeof(PageInfo));

        //Write page info to new page
        PageNum pageNum = fileHandle.getNumberOfPages();
        fileHandle.writePage(pageNum-1, newPage);

        free(newPage);
        return pageNum;
    }

    bool RecordBasedFileManager::checkBit(char* bytes, int size, int index) {
        size_t byteIndex = index/8;
        size_t offset = index%8;

        if (byteIndex < size) {
            char t = bytes[byteIndex];
            bool test = (bytes[byteIndex] & (1 << (7-offset))) != 0;
            return (bytes[byteIndex] & (1 << (7-offset))) != 0;
        }
        return false;
    }

} // namespace PeterDB

