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
        return PagedFileManager::instance().createFile(fileName);
    }

    RC RecordBasedFileManager::destroyFile(const std::string &fileName) {
        return PagedFileManager::instance().destroyFile(fileName);
    }

    RC RecordBasedFileManager::openFile(const std::string &fileName, FileHandle &fileHandle) {
        return PagedFileManager::instance().openFile(fileName, fileHandle);
    }

    RC RecordBasedFileManager::closeFile(FileHandle &fileHandle) {
        return PagedFileManager::instance().closeFile(fileHandle);
    }

    RC RecordBasedFileManager::insertRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                            const void *data, RID &rid) {
        //Fails if record descriptor is empty
        if (recordDescriptor.size() == 0) {
            return FAILURE;
        }

        //calc record size
        void *newData = malloc(PAGE_SIZE);
        unsigned dataSize = reformatData(recordDescriptor, data, newData);

        //find page with enough space
        PageNum pageNum = findPage(fileHandle, dataSize);

        //insert data into page
        void *pageBuffer = malloc(PAGE_SIZE);                                           //read page to buffer
        if (fileHandle.readPage(pageNum - 1, pageBuffer) == FAILURE) {
            free(newData);
            free(pageBuffer);
            return FAILURE;
        }

        char *filePtr = (char *) pageBuffer;                                                  //Create file pointer
        char *tempPageInfo = filePtr + (PAGE_SIZE - sizeof(PageInfo));                      //Create pageInfo pointer
        PageInfo *pageInfo = (PageInfo *) tempPageInfo;

        Slot *slotDirectory;
        // look for empty slots
        if (pageInfo->emptySlots >= 1) {                                                    // reuse a slot
            unsigned i = 1;
            char *checkSlotptr = filePtr + (PAGE_SIZE - sizeof(PageInfo) - sizeof(Slot));
            while (i <= pageInfo->numSlots) {
                Slot *checkSlot = (Slot *) checkSlotptr;
                if (checkSlot->offset == -1 && checkSlot->length == -1) {                   // found an empty slot
                    break;
                } else {
                    checkSlotptr -= sizeof(Slot);
                    i++;
                }
            }
            slotDirectory = (Slot *) checkSlotptr;                                            // use existing slot

            rid.pageNum = pageNum;
            rid.slotNum = i;

            pageInfo->emptySlots--;                                                         // update number of empty slots in page
        } else {                                                                            // make a new slot
            pageInfo->numSlots++;
            char *tempSdirectory = filePtr + (PAGE_SIZE - sizeof(PageInfo) -
                                              pageInfo->numSlots * sizeof(Slot));     //Create slotDirectory pointer
            slotDirectory = (Slot *) tempSdirectory;

            rid.pageNum = pageNum;
            rid.slotNum = pageInfo->numSlots;
        }

        filePtr += pageInfo->freeSpaceOffset;                                               //move filePtr to start of free space
        memcpy(filePtr, newData, dataSize);                                        //write record data to page
        //slotDirectory -= sizeof(Slot);                                                      //add slot to page
        slotDirectory->length = dataSize;
        slotDirectory->offset = pageInfo->freeSpaceOffset;
        slotDirectory->isTomb = false;
        pageInfo->freeSpaceOffset += dataSize;                                              //update page info

        //write page into file
        fileHandle.writePage(pageNum - 1, pageBuffer);
        free(newData);
        free(pageBuffer);
        return SUCCESS;
    }

    RC RecordBasedFileManager::readRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                          const RID &rid, void *data) {
        void *pageBuffer = malloc(PAGE_SIZE);
        if (fileHandle.readPage(rid.pageNum - 1, pageBuffer) == FAILURE) {                      //Handle error
            free(pageBuffer);
            return FAILURE;
        }

        char *filePtr = (char *) pageBuffer;
        char *temppageinfo = filePtr + (PAGE_SIZE - sizeof(PageInfo));
        PageInfo *pageInfo = (PageInfo *) temppageinfo;

        if (rid.slotNum > pageInfo->numSlots) {
            free(pageBuffer);
            return FAILURE;
        }

        char *tempSdirectory = filePtr + (PAGE_SIZE - sizeof(PageInfo) - sizeof(Slot) * rid.slotNum);
        Slot *slotDirectory = (Slot *) tempSdirectory;

        // Slot unused
        if (slotDirectory->offset == -1 && slotDirectory->length == -1) {
            free(pageBuffer);
            return FAILURE;
        }

        // Slot tombstone
        if (slotDirectory->isTomb) {
            //get new rid
            char *tempRID = filePtr + slotDirectory->offset;
            const RID tombRID = *((RID *) tempRID);
            free(pageBuffer);
            readRecord(fileHandle, recordDescriptor, tombRID, data);
            return SUCCESS;
        }

        // Read data and reformat
        filePtr += slotDirectory->offset;
        int recordSize = slotDirectory->length;
        double numFields = recordDescriptor.size();
        double nbytes = (int) ceil(numFields / 8);

        void *readData = malloc(recordSize);
        memcpy(readData, filePtr, recordSize);
        char *readDataPtr = (char *) readData;
        readDataPtr += sizeof(unsigned);                       // skip number of fields
        memcpy(data, readDataPtr, nbytes);
        char *tdata = (char *) data;
        tdata += (int) nbytes;
        readDataPtr += (int) nbytes + (int) (numFields * sizeof(unsigned));
        recordSize -= (int)(sizeof(unsigned) + int(nbytes) + (int) (numFields * sizeof(unsigned)));
        memcpy(tdata, readDataPtr, recordSize);
        //int test = sizeof(data);

        free(readData);
        free(pageBuffer);
        return SUCCESS;
    }

    RC RecordBasedFileManager::deleteRecord(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                            const RID &rid) {
        void *pageBuffer = malloc(PAGE_SIZE);
        if (fileHandle.readPage(rid.pageNum - 1, pageBuffer) == FAILURE) {
            free(pageBuffer);
            return FAILURE;
        }

        char *filePtr = (char *) pageBuffer;
        char *temppageinfo = filePtr + (PAGE_SIZE - sizeof(PageInfo));
        PageInfo *pageInfo = (PageInfo *) temppageinfo;                                   // Page info

        char *tempSdir = filePtr + (PAGE_SIZE - sizeof(PageInfo) - sizeof(Slot) * rid.slotNum);
        Slot *slotDir = (Slot *) tempSdir;                                           // slot directory of deleted record

        // Slot tombstone
        if (slotDir->isTomb) {
            //get new rid
            char *tempRID = filePtr + slotDir->offset;
            const RID tombRID = *((RID *) tempRID);
            //free(pageBuffer);
            deleteRecord(fileHandle, recordDescriptor, tombRID);

            //return SUCCESS;
        }

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
            char *tempNextSdir = tempSdir - sizeof(Slot);
            Slot *nextSlotDir = (Slot *) tempNextSdir;                                  // slot directory of next record

            // get size of records to be moved
            unsigned sizeRecords = pageInfo->freeSpaceOffset - nextSlotDir->offset;

            // get record pointers
            filePtr += slotDir->offset;                                                 // record to be deleted
            char *moveRecords = (char *) pageBuffer;                                      // new pointer of record to move
            moveRecords += nextSlotDir->offset;                                         // records to be moved

            memcpy(filePtr, moveRecords, sizeRecords);                      // move records up
            pageInfo->freeSpaceOffset -= slotDir->length;                               // move free space offset

            tempNextSdir = (char *) pageBuffer;
            tempNextSdir += PAGE_SIZE - (sizeof(PageInfo) + sizeof(Slot));
            //nextSlotDir = (Slot*)tempNextSdir;
            // update record offsets
            for (int i = 1; i <= pageInfo->numSlots; i++) {
                nextSlotDir = (Slot *) tempNextSdir;
                if (nextSlotDir->offset > slotDir->offset) {
                    nextSlotDir->offset -= slotDir->length;
                }
                tempNextSdir -= sizeof(Slot);
            }
        }

        // update deleted slot
        slotDir->offset = -1;
        slotDir->length = -1;
        slotDir->isTomb = false;

        pageInfo->emptySlots++;                                                     // update number of empty slots in page

        // write page to file
        fileHandle.writePage(rid.pageNum - 1, pageBuffer);
        free(pageBuffer);
        return SUCCESS;
    }

    RC RecordBasedFileManager::printRecord(const std::vector<Attribute> &recordDescriptor, const void *data,
                                           std::ostream &out) {
        //calc num bytes for null indicator
        double numFields = recordDescriptor.size();
        double nbytes = ceil(numFields / 8);

        void *dest = malloc(nbytes);
        memcpy(dest, data, nbytes);
        char nullin = *(char *) dest;
        char *dataPtr = (char *) data;
        dataPtr += (int) nbytes;
        for (int i = 0; i < recordDescriptor.size(); i++) {
            if (i != 0) {
                out.write(", ", 2);
            }

            if (checkBit((char *) dest, nbytes, i) == true) {
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
            switch (recordDescriptor[i].type) {
                case TypeInt:
                    out << recordDescriptor[i].name << ": " << *(int *) dataPtr;
                    dataPtr += 4;
                    break;
                case TypeReal:
                    out << recordDescriptor[i].name << ": " << *(float *) dataPtr;
                    dataPtr += 4;
                    break;
                case TypeVarChar:
                    l = *(unsigned int *) (dataPtr);
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
        // open page
        void *pageBuffer = malloc(PAGE_SIZE);
        if (fileHandle.readPage(rid.pageNum - 1, pageBuffer) == FAILURE) {
            free(pageBuffer);
            return FAILURE;
        }

        // get page info
        char *filePtr = (char *) pageBuffer;
        char *tempPageInfo = filePtr + (PAGE_SIZE - sizeof(PageInfo));
        PageInfo *pageInfo = (PageInfo *) tempPageInfo;

        // get slot info
        char *tempSlotDir = filePtr + (PAGE_SIZE - sizeof(PageInfo) - sizeof(Slot) * rid.slotNum);
        Slot *slotDir = (Slot *) tempSlotDir;

        // get available size in page
        unsigned availSize =
                PAGE_SIZE - (pageInfo->freeSpaceOffset + pageInfo->numSlots * sizeof(Slot) + sizeof(PageInfo));

        // get size of old data
        int oldDataSize = slotDir->length;

        // get size of new data and reformat
        void *newData = malloc(PAGE_SIZE);
        unsigned newDataSize = reformatData(recordDescriptor, data, newData);

        // Check cases: is smaller, is larger (enough space in page), is larger (not enough space in page), is equal
        if (oldDataSize > newDataSize) {                                                    // is smaller
            // write new data in space of old data
            char *updatePtr = filePtr + slotDir->offset;
            char *nextPtr = updatePtr + slotDir->length;
            memcpy((void *) updatePtr, newData, newDataSize);

            // update that slot
            unsigned sizeRecords = pageInfo->freeSpaceOffset - (slotDir->offset + slotDir->length);
            slotDir->length = newDataSize;

            // compact page
            char *newPtr = updatePtr + slotDir->length;
            memcpy(newPtr, nextPtr, sizeRecords);

            // move free pointer offset and update remain slots
            pageInfo->freeSpaceOffset -= slotDir->length;

            char *tns = filePtr + (PAGE_SIZE - sizeof(PageInfo) - sizeof(Slot));
            Slot *nextSlot;
            for (unsigned i = 1; i <= pageInfo->numSlots; i++) {
                nextSlot = (Slot *) tns;
                if (nextSlot->offset > slotDir->offset) {
                    nextSlot->offset -= slotDir->length;
                }
                tns -= sizeof(Slot);
            }

        } else if (oldDataSize < newDataSize &&
                   availSize >= newDataSize - oldDataSize) {     // is larger (enough space in page)
            unsigned extraSpace = newDataSize - oldDataSize;
            // move records back
            char *old = filePtr + slotDir->offset;
            char *movePtr = old + slotDir->length;
            char *newPtr = movePtr + extraSpace;
            unsigned sizeRecords = pageInfo->freeSpaceOffset - (slotDir->offset + slotDir->length);
            memcpy(newPtr, movePtr, sizeRecords);

            // write new data
            memcpy((void *) old, newData, newDataSize);
            slotDir->length = newDataSize;

            // update free space pointer and slots
            pageInfo->freeSpaceOffset += extraSpace;

            char *tns = filePtr + (PAGE_SIZE - sizeof(PageInfo) - sizeof(Slot));
            Slot *nextSlot;
            for (unsigned i = 1; i <= pageInfo->numSlots; i++) {
                nextSlot = (Slot *) tns;
                if (nextSlot->offset > slotDir->offset) {
                    nextSlot->offset += extraSpace;
                }
                tns -= sizeof(Slot);
            }
        } else if (oldDataSize < newDataSize &&
                   availSize < newDataSize - oldDataSize) {      // is larger (not enough space in page)
            // insert record in new page
            RID tombRID;
            insertRecord(fileHandle, recordDescriptor, data, tombRID);

            // compact size
            char *tomb = filePtr + slotDir->offset;
            char *movePtr = tomb + slotDir->length;
            char *newPtr = tomb + sizeof(RID);
            unsigned sizeRecords = pageInfo->freeSpaceOffset - (slotDir->offset + slotDir->length);
            memcpy(newPtr, movePtr, sizeRecords);

            // write rid info
            memcpy(tomb, &tombRID, sizeof(RID));

            // update slot
            unsigned savedSpace = slotDir->length - sizeof(RID);
            pageInfo->freeSpaceOffset -= savedSpace;
            slotDir->length = sizeof(RID);
            slotDir->isTomb = true;

            char *tns = filePtr + (PAGE_SIZE - sizeof(PageInfo) - sizeof(Slot));
            Slot *nextSlot;
            for (unsigned i = 1; i <= pageInfo->numSlots; i++) {
                nextSlot = (Slot *) tns;
                if (nextSlot->offset > slotDir->offset) {
                    nextSlot->offset -= savedSpace;
                }
                tns -= sizeof(Slot);
            }
        } else {                                                                            // same size
            char *old = filePtr + slotDir->offset;
            memcpy(old, newData, newDataSize);
        }

        // write to page
        fileHandle.writePage(rid.pageNum - 1, pageBuffer);
        free(newData);
        free(pageBuffer);
        return SUCCESS;
    }

    RC RecordBasedFileManager::readAttribute(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                             const RID &rid, const std::string &attributeName, void *data) {
        // read the record
        void* recordBuffer = malloc(PAGE_SIZE);
        //readRecord(fileHandle, recordDescriptor, rid, recordBuffer);

        void *pageBuffer = malloc(PAGE_SIZE);
        if (fileHandle.readPage(rid.pageNum - 1, pageBuffer) == FAILURE) {                      //Handle error
            free(recordBuffer);
            free(pageBuffer);
            return FAILURE;
        }

        char *filePtr = (char *) pageBuffer;
        char *temppageinfo = filePtr + (PAGE_SIZE - sizeof(PageInfo));
        PageInfo *pageInfo = (PageInfo *) temppageinfo;

        if (rid.slotNum > pageInfo->numSlots) {
            free(recordBuffer);
            free(pageBuffer);
            return FAILURE;
        }

        char *tempSdirectory = filePtr + (PAGE_SIZE - sizeof(PageInfo) - sizeof(Slot) * rid.slotNum);
        Slot *slotDirectory = (Slot *) tempSdirectory;

        // Slot unused
        if (slotDirectory->offset == -1 && slotDirectory->length == -1) {
            free(recordBuffer);
            free(pageBuffer);
            return FAILURE;
        }

        // Slot tombstone
        if (slotDirectory->isTomb) {
            //get new rid
            char *tempRID = filePtr + slotDirectory->offset;
            const RID tombRID = *((RID *) tempRID);
            free(recordBuffer);
            free(pageBuffer);
            readRecord(fileHandle, recordDescriptor, tombRID, data);
            return SUCCESS;
        }

        // read record
        filePtr += slotDirectory->offset;
        int recordSize = slotDirectory->length;
        memcpy(recordBuffer, filePtr, recordSize);

        // read the attribute
        //void* attrData = malloc(PAGE_SIZE);
        if (readAttributeFromRecord(recordDescriptor, attributeName, recordBuffer, data) == FAILURE) {
            //free(attrData);
            free(pageBuffer);
            free(recordBuffer);
            return FAILURE;
        }
/*
        // add null indicator
        char nullindicator;
        if (attrData == nullptr) {
            nullindicator = 0b10000000;
        } else {
            nullindicator = 0b00000000;
        }

        memcpy(data, &nullindicator, sizeof(char));



        memcpy((char*)data + sizeof(char), attrData, *((int*)attrData) + sizeof(int));
*/

        free(pageBuffer);
        free(recordBuffer);
        return SUCCESS;
    }

    RC RecordBasedFileManager::scan(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor,
                                    const std::string &conditionAttribute, const CompOp compOp, const void *value,
                                    const std::vector<std::string> &attributeNames,
                                    RBFM_ScanIterator &rbfm_ScanIterator) {
        // write data to scan interator
        rbfm_ScanIterator.fileHandle = fileHandle;
        rbfm_ScanIterator.recordDescriptor = recordDescriptor;
        rbfm_ScanIterator.conditionAttribute = conditionAttribute;
        rbfm_ScanIterator.compOp = compOp;
        //rbfm_ScanIterator.value = value;
        rbfm_ScanIterator.attributeNames = attributeNames;

        // get Attribute info
        if (!conditionAttribute.empty()) {              // not empty attribute
            rbfm_ScanIterator.conditionAttributeNum = NO_OP;    // in case no match
            for (int i = 0; i < recordDescriptor.size(); i++) {
                if (recordDescriptor[i].name == conditionAttribute) {       // found match
                    rbfm_ScanIterator.conditionAttributeNum = i;
                    rbfm_ScanIterator.conditionAttributeType = recordDescriptor[i].type;
                    break;
                }
            }
        } else {
            rbfm_ScanIterator.conditionAttributeNum = NO_OP;   // empty attribute condition
            rbfm_ScanIterator.saveRID.pageNum = 1;
            rbfm_ScanIterator.saveRID.slotNum = 1;

            return SUCCESS;
        }

        // get value

        if (rbfm_ScanIterator.conditionAttributeType == TypeVarChar) {                   // get varchar value
            //int t = *((int*)value);
            rbfm_ScanIterator.value = malloc(*((int*)value));                   // size of entire value, including length int
            memset(rbfm_ScanIterator.value, 0, *((int*)value) + 1);
            memcpy(rbfm_ScanIterator.value, (char*)value + sizeof(int), *((int*)value));     // copy value
        } else {               // type Int/Real
            rbfm_ScanIterator.value = malloc(sizeof(int));
            memcpy(rbfm_ScanIterator.value, value, sizeof(int));
        }


        //rbfm_ScanIterator.value = &value;
        // Set save RID to first slot on first page
        rbfm_ScanIterator.saveRID.pageNum = 1;
        rbfm_ScanIterator.saveRID.slotNum = 1;

        return SUCCESS;
    }

    PageNum RecordBasedFileManager::findPage(FileHandle &fileHandle, int size) {
        PageNum pageNum = fileHandle.getNumberOfPages();
        if (pageNum == 0) {
            return newPage(fileHandle);
        }
        //Check current page
        void *pageBuffer = malloc(PAGE_SIZE);
        if (fileHandle.readPage(pageNum - 1, pageBuffer) == FAILURE) {
            free(pageBuffer);
            return FAILURE;
        }
        //Find free space
        char *tempPtr = (char *) pageBuffer + (PAGE_SIZE - sizeof(PageInfo));
        PageInfo *pageInfo = (PageInfo *) tempPtr;
        int availSize = PAGE_SIZE - (pageInfo->freeSpaceOffset + pageInfo->numSlots * sizeof(Slot) + sizeof(Slot) +
                                     sizeof(PageInfo));
        free(pageBuffer);
        if (size <= availSize) {
            //use current page
            //free(pageBuffer);
            return pageNum;
        } else {                    //Look through directory
            void *pageBuff = malloc(PAGE_SIZE);
            for (unsigned i = 0; i <= fileHandle.getNumberOfPages() - 1; i++) {
                //void* pageBuffer = malloc(PAGE_SIZE);
                fileHandle.readPage(i, pageBuff);
                char *tempPtr = (char *) pageBuff + (PAGE_SIZE - sizeof(PageInfo));
                PageInfo *pageInfo = (PageInfo *) tempPtr;
                int availSize = PAGE_SIZE -
                                (pageInfo->freeSpaceOffset + pageInfo->numSlots * sizeof(Slot) + sizeof(Slot) +
                                 sizeof(PageInfo));
                if (size <= availSize) {
                    free(pageBuff);
                    return i + 1;
                }
            }
            //If no space create new page and return new page number
            //free(pageBuffer);
            return newPage(fileHandle);
        }
    }

    PageNum RecordBasedFileManager::newPage(FileHandle &fileHandle) {
        //Append new page
        void *newPage = malloc(PAGE_SIZE);
        fileHandle.appendPage(newPage);

        //Add slot info to page
        char *slotDirectoryPtr = (char *) newPage;
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
        fileHandle.writePage(pageNum - 1, newPage);

        free(newPage);
        return pageNum;
    }

    bool RecordBasedFileManager::checkBit(char *bytes, int size, int index) {
        size_t byteIndex = index / 8;
        size_t offset = index % 8;

        if (byteIndex < size) {
            char t = bytes[byteIndex];
            bool test = (bytes[byteIndex] & (1 << (7 - offset))) != 0;
            return (bytes[byteIndex] & (1 << (7 - offset))) != 0;
        }
        return false;
    }

    unsigned RecordBasedFileManager::getRecordSize(const std::vector<Attribute> &recordDescriptor, const void *data) {
        unsigned dataSize = 0;
        double numFields = recordDescriptor.size();
        double nbytes = (int) ceil(numFields / 8);
        dataSize += nbytes;

        void *b = malloc(nbytes);
        memcpy(b, data, nbytes);

        for (unsigned i = 0; i < recordDescriptor.size(); i++) {
            if (checkBit((char *) b, nbytes, i) == true) {
                continue;
            }
            if (recordDescriptor[i].type == TypeVarChar) {
                unsigned int varcharSize = *(unsigned int *) ((char *) data + dataSize);
                dataSize += varcharSize;
            }
            dataSize += 4;
        }
        free(b);
        return dataSize;
    }

    unsigned RecordBasedFileManager::reformatData(const std::vector<Attribute> &recordDescriptor, const void *inBuffer, void *outBuffer) {
        unsigned dataSize = 0;
        unsigned dSize = 0;
        //calc num bytes for null indicator
        double numFields = recordDescriptor.size();
        double nbytes = (int) ceil(numFields / 8);
        dataSize += nbytes;

        int offsets[(unsigned) numFields];                       // array of offsets for each field in record

        void *b = malloc(nbytes);
        memcpy(b, inBuffer, nbytes);
        //calc size of data
        for (int i = 0; i < recordDescriptor.size(); i++) {
            if (checkBit((char *) b, nbytes, i) == true) {
                offsets[i] = dataSize;          // point to same place as prev
                continue;
            }
            if (recordDescriptor[i].type == TypeVarChar) {
                //calc size of vachar
                unsigned int varcharSize = *(unsigned int *) ((char *) inBuffer + dataSize);
                dataSize += varcharSize;
                dSize += varcharSize;
            }
            //int/real/varchar all size 4
            dataSize += 4;
            dSize += 4;
            offsets[i] = dataSize;
        }
        dataSize += sizeof(unsigned) + (int) (numFields * sizeof(unsigned));
        /* Create Record Format */
        // Format == [num Fields] + [null bit indicator] + [field offsets] + [data]

        void* newData = malloc(dataSize);
        char* newDataPtr = (char*) newData;
        //char *newDataPtr = (char *) outBuffer;
        int *nf = (int *) newDataPtr;
        *nf = numFields;
        //*newDataPtr = numFields;
        newDataPtr += sizeof(unsigned);
        void *nbi = (void *) newDataPtr;
        memcpy(nbi, b, nbytes);
        //(char*)newDataPtr;
        newDataPtr += (int) nbytes;
        //(unsigned*)newDataPtr;
        for (unsigned i = 0; i < numFields; i++) {
            int *off = (int *) newDataPtr;
            *off = offsets[i];
            newDataPtr += sizeof(unsigned);
        }
        (void *) newDataPtr;
        memcpy(newDataPtr, (char *) inBuffer + (int) nbytes, dSize);
        memcpy(outBuffer, newData, dataSize);
        free(newData);
        free(b);
        return dataSize;
    }

    RC RecordBasedFileManager::readAttributeFromRecord(const std::vector<Attribute> &recordDescriptor,
                                                       const std::string &attributeName, const void *recordData,
                                                       void *attributeData) {
        for (unsigned i = 0; i < recordDescriptor.size(); i++) {
            if (recordDescriptor[i].name == attributeName) {                // names match
                // get null indicator bit
                double numFields = recordDescriptor.size();
                double nbytes = ceil(numFields/8);
                char* recordPtr = (char*)recordData;
                recordPtr += sizeof(unsigned);                              // skip num fields int
                void* nullByte = malloc(nbytes);
                memcpy(nullByte, (void*)recordPtr, nbytes);
                recordPtr += (int)nbytes;

                // check if that field data is null
                if (checkBit((char*)nullByte, nbytes, i) == true) {
                    char nullindicator = 0b10000000;
                    memcpy(attributeData, &nullindicator, sizeof(char));
                    //attributeData = nullptr;
                    free(nullByte);
                    return SUCCESS;
                }

                // get attribute data ;offset for field points to nest field
                unsigned sizeOffsets = numFields * sizeof(unsigned);
                int offset = (i==0) ? 0 : *((int*)recordPtr + (i-1))-1;
                recordPtr += (i==0) ? sizeOffsets : sizeOffsets + offset;
                char nullindicator = 0b00000000;
                memcpy(attributeData, &nullindicator, sizeof(char));

                if (recordDescriptor[i].type == TypeVarChar) {
                    int varCharLength;
                    memcpy(&varCharLength, (void*)recordPtr, sizeof(int));                  // get length of var char
                    memcpy((char*)attributeData + sizeof(char), (void*)recordPtr, varCharLength + sizeof(int));   // copy attribute data
                } else {            // tpye Int/Real
                    memcpy((char*)attributeData + sizeof(char), (void*)recordPtr, sizeof(int));
                }
                free(nullByte);
                return SUCCESS;
            }
        }
        return FAILURE;
    }

    RC RBFM_ScanIterator::getRecord(RID &rid, void* recordData) {
        RecordBasedFileManager &rbfm = RecordBasedFileManager::instance();
        if (rbfm.readRecord(fileHandle, recordDescriptor, rid, recordData) == FAILURE) {
            return FAILURE;
        }

        return SUCCESS;
    }

    RC RBFM_ScanIterator::getAttribute(RID &rid, void* attributeData) {
        RecordBasedFileManager &rbfm = RecordBasedFileManager::instance();
        if (rbfm.readAttribute(fileHandle, recordDescriptor, rid, conditionAttribute,attributeData) == FAILURE) {
            return FAILURE;
        }

        return SUCCESS;
    }

    RC RBFM_ScanIterator::extractAttributes(void *data) {
        // return to data only the requested attributes
        // include null-indicator only on requested attributes
        void* returnData = malloc(PAGE_SIZE);
        memset(returnData, 0, PAGE_SIZE);
        int attr[attributeNames.size()];
        std::fill(attr, attr + attributeNames.size(), 0);

        RecordBasedFileManager &rbfm = RecordBasedFileManager::instance();
        int offset = 0;
        for (int i = 0; i < recordDescriptor.size(); i++) {
            for (int j = 0; j < attributeNames.size(); j++) {
                if (recordDescriptor[i].name == attributeNames[j]) {
                    // extract
                    void* attributeData = malloc(PAGE_SIZE);
                    if (rbfm.readAttribute(fileHandle, recordDescriptor, saveRID, attributeNames[j], attributeData) == FAILURE) {
                        free(attributeData);
                        return FAILURE;
                    }

                    // check if attribute is null
                    void* nb = malloc(sizeof(char));
                    memcpy(nb, attributeData, sizeof(char));
                    if (rbfm.checkBit((char*)nb, sizeof(char), 0)) {
                        attr[j] = 1;
                    }
                    free(nb);

                    if (recordDescriptor[i].type == TypeVarChar) {
                        int varCharLength;
                        memcpy(&varCharLength, (char*)attributeData + sizeof(char), sizeof(int));                  // get length of var char
                        memcpy((char*)returnData + offset, (char*)attributeData + sizeof(char), varCharLength + sizeof(int));   // copy attribute data
                        offset += varCharLength + sizeof(int);
                    } else {            // type Int/Real
                        memcpy((char*)returnData + offset, (char*)attributeData + sizeof(char), sizeof(int));
                        offset += sizeof(int);
                    }
                    free(attributeData);
                }
            }
        }

        // add null indicator to front of data
        double size = ceil((double)attributeNames.size()/8);
        for (int i = 0; i < size; i++) {
            *((char*)data + i) = 0b00000000;
        }

        for (int i : attr) {
            if (attr[i] == 1) {
                double nb = floor((double)i / 8);
                *((char *) data + (int)nb) |= 0b10000000 >> (i % 8);
            } else {
                double nb = floor((double)i / 8);
                *((char *) data + (int)nb) |= 0b00000000 >> (i % 8);
            }
        }

        memcpy((char*)data + (int)size, returnData, offset);

        free(returnData);
        return SUCCESS;
    }


    RC RBFM_ScanIterator::getNextRecord(RID &rid, void *data) {

        if (iterated) {
            return RBFM_EOF;
        }

        int exitStatus = RBFM_EOF;
        void* pageBuffer = malloc(PAGE_SIZE);
        //void* recordData = malloc(PAGE_SIZE);
        void* attributeData = malloc(PAGE_SIZE);

        // step through the pages in file
        while(saveRID.pageNum <= fileHandle.getNumberOfPages()) {
            // Read page
            if (this->fileHandle.readPage(saveRID.pageNum-1, pageBuffer) == FAILURE) {
                free(pageBuffer);
                //free(recordData);
                free(attributeData);
                return FAILURE;
            }

            char* filePtr = (char*) pageBuffer;
            PageInfo* pageInfo = (PageInfo*)(filePtr + PAGE_SIZE - sizeof(PageInfo));

            // check each record in page
            while (saveRID.slotNum <= pageInfo->numSlots) {

                //memset(recordData, 0, PAGE_SIZE);
                memset(attributeData, 0, PAGE_SIZE);

                /*
                // read the record
                if (getRecord(saveRID, recordData) == FAILURE) {
                    exitStatus = RBFM_EOF;
                }
*/
                bool compStatus = false;

                if (compOp != NO_OP) {
                    // read attribute
                    if (getAttribute(saveRID, attributeData) == FAILURE) {                      // read attribute
                        exitStatus = RBFM_EOF;
                    }

                    // compare attribute
                    compStatus = compareAttribute(attributeData);
                }

                if (compStatus || compOp == NO_OP) {
                    // found record
                    extractAttributes(data);                    // only requested attributes
                    rid.slotNum = saveRID.slotNum;
                    rid.pageNum = saveRID.pageNum;
                    exitStatus = SUCCESS;
                    findNext(pageInfo);                         // move save rid to next record
                    break;
                }

                // record not found, go to next record in page
                saveRID.slotNum++;
            }

            // record found
            if (exitStatus == SUCCESS) {
                break;
            }

            // record not found, go to next page
            saveRID.slotNum = 1;
            saveRID.pageNum++;

        }

        free(attributeData);
        //free(recordData);
        free(pageBuffer);
        return exitStatus;
    }

    RC RBFM_ScanIterator::findNext(PageInfo *pageInfo) {
        if (saveRID.slotNum < pageInfo->numSlots) {
            saveRID.slotNum++;
        } else if (saveRID.slotNum == pageInfo->numSlots && saveRID.pageNum < fileHandle.getNumberOfPages()) {
            saveRID.slotNum = 1;
            saveRID.pageNum++;
        } else {
            iterated = true;
        }
        return SUCCESS;
    }

    RC RBFM_ScanIterator::compareAttribute(const void* attributeData) {
        if (compOp == NO_OP) {
            return true;
        }
        if (conditionAttributeType == TypeVarChar) {
            //char* compareData;
            return compareVarChar((char*)attributeData + sizeof(int) + sizeof(char));

        } else if (conditionAttributeType == TypeInt) {
            int compareData;
            memcpy(&compareData, (char*)attributeData + sizeof(char), sizeof(int));
            return compareNum(compareData, *(int*)value);

        } else if (conditionAttributeType == TypeReal) {
            float compareData;
            memcpy(&compareData, (char*)attributeData + sizeof(char), sizeof(int));
            return compareNum(compareData, *(float*)value);

        } else {
            return FAILURE;
        }
    }

    // Compares Int and Real
    template <typename T>
    bool RBFM_ScanIterator::compareNum(T a, T b) {
        if (compOp == EQ_OP) {
            return a == b;
        } else if (compOp == LT_OP) {
            return a < b;
        } else if (compOp == LE_OP) {
            return a <= b;
        } else if (compOp == GT_OP) {
            return a > b;
        } else if (compOp == GE_OP) {
            return a >= b;
        } else if (compOp == NE_OP) {
            return a != b;
        } else if (compOp == NO_OP) {
            return true;
        } else {
            return false;
        }
    }

    // Compares VarChar
    bool RBFM_ScanIterator::compareVarChar(const char* attributeData) {
        // add null terminators
        size_t valSize = std::strlen((char*)value);
        size_t attSize = std::strlen(attributeData);
        char* valArr = new char[valSize + 1];
        char* attArr = new char[attSize + 1];
        std::strncpy(valArr, (char*)value, valSize);
        std::strncpy(attArr, attributeData, attSize);
        valArr[valSize] = '\0';
        attArr[attSize] = '\0';

        if (compOp == EQ_OP) {
            return (strcmp(attArr, valArr) == 0);
        } else if (compOp == LT_OP) {
            return (strcmp(attArr, valArr) < 0);
        } else if (compOp == LE_OP) {
            return (strcmp(attArr, valArr) <= 0);
        } else if (compOp == GT_OP) {
            return (strcmp(attArr, valArr) > 0);
        } else if (compOp == GE_OP) {
            return (strcmp(attArr, valArr) >= 0);
        } else if (compOp == NE_OP) {
            return (strcmp(attArr, valArr) != 0);
        } else if (compOp == NO_OP) {
            return true;
        } else {
            return false;
        }
    }

    RC RBFM_ScanIterator::close() {
        PagedFileManager::instance().closeFile((fileHandle));
        free(value);
        return SUCCESS;
    }

}