#include "src/include/rm.h"

namespace PeterDB {
    RelationManager &RelationManager::instance() {
        static RelationManager _relation_manager = RelationManager();
        return _relation_manager;
    }

    RelationManager::RelationManager() = default;

    RelationManager::~RelationManager() = default;

    RelationManager::RelationManager(const RelationManager &) = default;

    RelationManager &RelationManager::operator=(const RelationManager &) = default;

    RC RelationManager::createCatalog() {
        // check if files already exist

        FILE *file;
        if ((file = std::fopen(tables.c_str(), "r")) || (file = std::fopen(columns.c_str(), "r"))) {
            // exists
            fclose(file);
            return FAILURE;
        }

        // create tables and columns file
        RC rc1 = rbfm->createFile(tables);
        RC rc2 = rbfm->createFile(columns);
        if (rc1 == FAILURE || rc2 == FAILURE) {return FAILURE;}

        // open files
        RID tableRID;
        RID columnRID;
        FileHandle tableFileHandle;
        FileHandle columnFileHandle;
        rc1 = rbfm->openFile(tables, tableFileHandle);
        rc2 = rbfm->openFile(columns, columnFileHandle);
        if (rc1 == FAILURE || rc2 == FAILURE) {return FAILURE;}

        // insert tuples
        std::vector<Attribute> tableRecordDescription;
        tableDesc(tableRecordDescription);
        std::vector<Attribute> columnRecordDescription;
        columnDesc(columnRecordDescription);

        tableCount++;
        insertTables(tableFileHandle, tableRecordDescription, tableRID, "Tables", "Tables");
        for (int i = 1; i <= tableRecordDescription.size(); i++) {
            insertColumns(columnFileHandle, columnRecordDescription, columnRID, tableRecordDescription[i].name, tableRecordDescription[i].type, tableRecordDescription[i].length, i);
        }

        tableCount++;
        insertTables(tableFileHandle, tableRecordDescription, tableRID, "Columns", "Columns");
        for (int i = 1; i <= columnRecordDescription.size(); i++) {
            insertColumns(columnFileHandle, columnRecordDescription, columnRID, columnRecordDescription[i].name, columnRecordDescription[i].type, columnRecordDescription[i].length, i);
        }

        // close files
        rc1 = rbfm->closeFile(tableFileHandle);
        rc2 = rbfm->closeFile((columnFileHandle));
        if (rc1 == FAILURE || rc2 == FAILURE) {return FAILURE;}

        catExists = true;
        return SUCCESS;
    }

    RC RelationManager::deleteCatalog() {
        //if (catExists == false) {           // catolog doesn't exist
        //    return SUCCESS;
        //}

        RC rc1 = rbfm->destroyFile(tables);
        RC rc2 = rbfm->destroyFile(columns);
        if (rc1 == FAILURE || rc2 == FAILURE) {return FAILURE;}
        tableCount = 0;
        catExists = false;

        return SUCCESS;
    }

    RC RelationManager::createTable(const std::string &tableName, const std::vector<Attribute> &attrs) {
        // Check if TABLES/COLUMNS or No catalog
        if (!catExists) {return FAILURE;}
        if (checkName(tableName) == FAILURE) {
            return FAILURE;
        }

        // Create table
        if (rbfm->createFile(tableName) == FAILURE) {
            return FAILURE;
        }

        // Open TABLES/COLUMNS
        FileHandle tableFileHandle;
        FileHandle columnFileHandle;
        RC rc1 = rbfm->openFile(tables, tableFileHandle);
        RC rc2 = rbfm->openFile(columns, columnFileHandle);
        if (rc1 == FAILURE || rc2 == FAILURE) {return FAILURE;}

        // Insert TABLES
        RID tableRID;
        std::vector<Attribute> tableRecordDescription;
        tableDesc(tableRecordDescription);
        tableCount++;
        insertTables(tableFileHandle, tableRecordDescription, tableRID, tableName, tableName);

        // Insert COLUMNS
        RID columnRID;
        std::vector<Attribute> columnRecordDescription;
        columnDesc(columnRecordDescription);
        for (int i = 0; i < attrs.size(); i++) {
            insertColumns(columnFileHandle, columnRecordDescription, columnRID, attrs[i].name, attrs[i].type, attrs[i].length, i+1);
        }

        // Close files
        rc1 = rbfm->closeFile(tableFileHandle);
        rc2 = rbfm->closeFile((columnFileHandle));
        if (rc1 == FAILURE || rc2 == FAILURE) {return FAILURE;}

        return SUCCESS;
    }

    RC RelationManager::deleteTable(const std::string &tableName) {
        // check name
        if (checkName(tableName) == FAILURE) {
            return FAILURE;
        }

        // delete file
        if (rbfm->destroyFile(tableName) == FAILURE) {
            return FAILURE;
        }

        // format tableName
        int l = tableName.length();
        void* value = malloc(l);
        formatStr(tableName, value);

        // remove from TABLES
        RM_ScanIterator rmsi_table;
        std::vector<std::string> attributes;
        attributes.push_back("table-id");
        scan(tables, "table-name", EQ_OP, value, attributes, rmsi_table);

        FileHandle tableFileHandle;
        RID tableRid;
        void* data = malloc(PAGE_SIZE);
        std::vector<Attribute> recordDescriptor;
        tableDesc(recordDescriptor);

        rbfm->openFile(tables, tableFileHandle);

        rmsi_table.getNextTuple(tableRid, data);
        //int tableId = *(int*)data;
        int tableId = *((int*)((char*)data + sizeof(char)));
        rbfm->deleteRecord(tableFileHandle, recordDescriptor, tableRid);

        rbfm->closeFile(tableFileHandle);
        attributes.clear();
        recordDescriptor.clear();

        // remove from COLUMNS
        RM_ScanIterator rmsi_column;
        attributes.push_back("column-name");
        scan(columns, "table-id", EQ_OP, &tableId, attributes, rmsi_column);

        FileHandle columnFileHandle;
        RID columnRid;
        columnDesc(recordDescriptor);

        rbfm->openFile(columns, columnFileHandle);

        while(rmsi_column.getNextTuple(columnRid, data) != RM_EOF) {
            rbfm->deleteRecord(columnFileHandle, recordDescriptor, columnRid);
        }

        rbfm->closeFile(columnFileHandle);

        //tableCount--;
        free(value);
        free(data);
        return SUCCESS;
    }

    RC RelationManager::getAttributes(const std::string &tableName, std::vector<Attribute> &attrs) {
        return createDesc(tableName, attrs);
    }

    RC RelationManager::insertTuple(const std::string &tableName, const void *data, RID &rid) {
        // open file
        FileHandle fileHandle;
        if (rbfm->openFile(tableName, fileHandle) == FAILURE) {return FAILURE;}

        // get record description
        std::vector<Attribute> recordDescription;
        createDesc(tableName, recordDescription);

        // insert record
        if (rbfm->insertRecord(fileHandle, recordDescription, data, rid) == FAILURE) {return FAILURE;}

        // close file
        if (rbfm->closeFile(fileHandle) == FAILURE) {return FAILURE;}

        return SUCCESS;
    }

    RC RelationManager::deleteTuple(const std::string &tableName, const RID &rid) {
        // open file
        FileHandle fileHandle;
        if (rbfm->openFile(tableName, fileHandle) == FAILURE) {return FAILURE;}

        // get record description
        std::vector<Attribute> recordDescription;
        createDesc(tableName, recordDescription);

        // delete record
        if (rbfm->deleteRecord(fileHandle, recordDescription, rid) == FAILURE) {return FAILURE;}

        // close file
        if (rbfm->closeFile(fileHandle) == FAILURE) {return FAILURE;}

        return SUCCESS;
    }

    RC RelationManager::updateTuple(const std::string &tableName, const void *data, const RID &rid) {
        // open file
        FileHandle fileHandle;
        if (rbfm->openFile(tableName, fileHandle) == FAILURE) {return FAILURE;}

        // get record description
        std::vector<Attribute> recordDescription;
        createDesc(tableName, recordDescription);

        // update record
        if (rbfm->updateRecord(fileHandle, recordDescription, data, rid) == FAILURE) {return FAILURE;}

        // close file
        if (rbfm->closeFile(fileHandle) == FAILURE) {return FAILURE;}

        return SUCCESS;
    }

    RC RelationManager::readTuple(const std::string &tableName, const RID &rid, void *data) {
        // open file
        FileHandle fileHandle;
        if (rbfm->openFile(tableName, fileHandle) == FAILURE) {return FAILURE;}

        // get record description
        std::vector<Attribute> recordDescription;
        createDesc(tableName, recordDescription);

        // read record
        if (rbfm->readRecord(fileHandle, recordDescription, rid, data) == FAILURE) {return FAILURE;}

        // close file
        if (rbfm->closeFile(fileHandle) == FAILURE) {return FAILURE;}

        return SUCCESS;
    }

    RC RelationManager::printTuple(const std::vector<Attribute> &attrs, const void *data, std::ostream &out) {

        if (rbfm->printRecord(attrs, data, out) == FAILURE) {return FAILURE;}

        return SUCCESS;
    }

    RC RelationManager::readAttribute(const std::string &tableName, const RID &rid, const std::string &attributeName,
                                      void *data) {
        // open file
        FileHandle fileHandle;
        if (rbfm->openFile(tableName, fileHandle) == FAILURE) {return FAILURE;}

        // get record description
        std::vector<Attribute> recordDescription;
        createDesc(tableName, recordDescription);

        // read record
        if (rbfm->readAttribute(fileHandle, recordDescription, rid, attributeName, data) == FAILURE) {return FAILURE;}

        // close file
        if (rbfm->closeFile(fileHandle) == FAILURE) {return FAILURE;}

        return SUCCESS;
    }

    RC RelationManager::scan(const std::string &tableName,
                             const std::string &conditionAttribute,
                             const CompOp compOp,
                             const void *value,
                             const std::vector<std::string> &attributeNames,
                             RM_ScanIterator &rm_ScanIterator) {
        // get file handle
        FileHandle fileHandle;
        if (rbfm->openFile(tableName, fileHandle) == FAILURE) {
            return FAILURE;
        }

        std::vector<Attribute> attributes;
        // get record descriptor
        if (tableName == tables) {
            tableDesc(attributes);
        } else if (tableName == columns) {
            columnDesc(attributes);
        } else {
            // create record description for all other tables
            createDesc(tableName, attributes);
        }

        // call rbfm scan
        RBFM_ScanIterator *_rbfmsi = new RBFM_ScanIterator();
        if (rbfm->scan(fileHandle, attributes, conditionAttribute, compOp, value, attributeNames, *_rbfmsi) == FAILURE){
            return FAILURE;
        }

        rm_ScanIterator.rbfmsi = _rbfmsi;

        // close file
        //if (rbfm->closeFile(fileHandle) == FAILURE) {
        //    return FAILURE;
        //}
        return SUCCESS;
    }

    RM_ScanIterator::RM_ScanIterator() = default;

    RM_ScanIterator::~RM_ScanIterator() = default;

    RC RM_ScanIterator::getNextTuple(RID &rid, void *data) {
        if (rbfmsi->getNextRecord(rid, data) != RBFM_EOF) {
            return SUCCESS;
        }
        return RM_EOF;
    }

    RC RM_ScanIterator::close() {
        if (rbfmsi != nullptr) {
            rbfmsi->close();
            delete rbfmsi;
        }
        return SUCCESS;
    }

    /**************Helpers**************/
    RC RelationManager::tableDesc(std::vector<Attribute> &recordDescriptor) {
        Attribute   tableId,
                    tableName,
                    fileName;
        // table id
        tableId.name = "table-id";
        tableId.type = TypeInt;
        tableId.length = sizeof(int);
        recordDescriptor.push_back(tableId);

        //table name
        tableName.name = "table-name";
        tableName.type = TypeVarChar;
        tableName.length = VARCHAR_SIZE;
        recordDescriptor.push_back(tableName);

        // file name
        fileName.name = "file-name";
        fileName.type = TypeVarChar;
        fileName.length = VARCHAR_SIZE;
        recordDescriptor.push_back(fileName);

        return SUCCESS;
    }

    RC RelationManager::columnDesc(std::vector<Attribute> &recordDescriptor) {
        Attribute   tableId,
                    columnName,
                    columnType,
                    columnLength,
                    columnPosition;
        // table id
        tableId.name = "table-id";
        tableId.type = TypeInt;
        tableId.length = sizeof(int);
        recordDescriptor.push_back(tableId);

        // column name
        columnName.name = "column-name";
        columnName.type = TypeVarChar;
        columnName.length = VARCHAR_SIZE;
        recordDescriptor.push_back(columnName);

        // column type
        columnType.name = "column-type";
        columnType.type = TypeInt;
        columnType.length = sizeof(int);
        recordDescriptor.push_back(columnType);

        // column length
        columnLength.name = "column-length";
        columnLength.type = TypeInt;
        columnLength.length = sizeof(int);
        recordDescriptor.push_back(columnLength);

        // column position
        columnPosition.name = "column-position";
        columnPosition.type = TypeInt;
        columnPosition.length = sizeof(int);
        recordDescriptor.push_back(columnPosition);

        return SUCCESS;
    }

    RC RelationManager::insertTables(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor, RID &rid, const std::string &tableName, const std::string &fileName) {
        int tableNameLen = tableName.length();
        int fileNameLen = fileName.length();
        int dataSize = sizeof(char) + sizeof(int)*3 + tableNameLen + fileNameLen;         // get size of data
        void *data = malloc(dataSize);

        // store info into data
        int tableId = tableCount;
        int offset = 0;
        char nullindicator = 0b00000000;
        memcpy(data, &nullindicator, sizeof(char));
        offset += sizeof(char);
        memcpy((char*)data + offset, &tableId, sizeof(int));
        offset += sizeof(int);
        memcpy((char*)data + offset, &tableNameLen, sizeof(int));
        offset += sizeof(int);
        memcpy((char*)data + offset, tableName.c_str(), tableNameLen);
        offset += tableNameLen;
        memcpy((char*)data + offset, &fileNameLen, sizeof(int));
        offset += sizeof(int);
        memcpy((char*)data + offset, fileName.c_str(), fileNameLen);

        if (rbfm->insertRecord(fileHandle, recordDescriptor, data, rid) == FAILURE) {
            free(data);
            return FAILURE;
        }
        free(data);
        return SUCCESS;
    }

    RC RelationManager::insertColumns(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor, RID &rid, const std::string &columnName, const int &columnType, const int &columnLength, const int &columnPosition) {
        int columnNameLen = columnName.length();
        int dataSize = sizeof(char) + sizeof(int)*5 + columnNameLen;
        void *data = malloc(dataSize);

        // store info into data
        int tableId = tableCount;
        int offset = 0;
        char nullindicator = 0b00000000;
        memcpy(data, &nullindicator, sizeof(char));
        offset += sizeof(char);
        memcpy((char*)data + offset, &tableId, sizeof(int));
        offset += sizeof(int);
        memcpy((char*)data + offset, &columnNameLen, sizeof(int));
        offset += sizeof(int);
        memcpy((char*)data + offset, columnName.c_str(), columnNameLen);
        offset += columnNameLen;
        memcpy((char*)data + offset, &columnType, sizeof(int));
        offset += sizeof(int);
        memcpy((char*)data + offset, &columnLength, sizeof(int));
        offset += sizeof(int);
        memcpy((char*)data + offset, &columnPosition, sizeof(int));

        if (rbfm->insertRecord(fileHandle, recordDescriptor, data, rid) == FAILURE) {
            free(data);
            return FAILURE;
        }

        free(data);
        return SUCCESS;
    }

    RC RelationManager::checkName(const std::string &tableName) {
        if (tableName == tables || tableName == columns) {
            return FAILURE;
        }
        return SUCCESS;
    }

    RC RelationManager::createDesc(const std::string &tableName, std::vector<Attribute> &recordDescriptor) {
/*
        // Open TABLES/COLUMNS
        FileHandle tableFileHandle;
        FileHandle columnFileHandle;
        RC rc1 = rbfm->openFile(tables, tableFileHandle);
        RC rc2 = rbfm->openFile(columns, columnFileHandle);
        if (rc1 == FAILURE || rc2 == FAILURE) {return FAILURE;}
*/

        if (tableName == tables) {
            tableDesc(recordDescriptor);
            return SUCCESS;
        }
        if (tableName == columns) {
            columnDesc(recordDescriptor);
            return SUCCESS;
        }

        // format tableName
        int l = tableName.length();
        void* value = malloc(l);
        memset(value, 0, l + sizeof(int) + 1);
        formatStr(tableName, value);


        // scan through TABLES to get table id
        std::vector<std::string> attributeNames;
        std::string attrName = "table-id";
        attributeNames.push_back(attrName);
        RM_ScanIterator rmsi_table;
        scan(tables, "table-name", EQ_OP, value, attributeNames, rmsi_table);
        free(value);

        RID rid;
        void* data = malloc(PAGE_SIZE);
        rmsi_table.getNextTuple(rid, data);
        //int tableId = *(int*)data;
        int tableId = *((int*)((char*)data + sizeof(char)));
        attributeNames.clear();                 // empty vector
        //free(data);

        // scan through COLUMNS to get each attribute
        attributeNames.push_back("column-name");
        attributeNames.push_back("column-type");
        attributeNames.push_back("column-length");
        RM_ScanIterator rmsi_column;
        scan(columns, "table-id", EQ_OP, &tableId, attributeNames, rmsi_column);

        //data = malloc(PAGE_SIZE);
        memset(data, 0, PAGE_SIZE);
        while (rmsi_column.getNextTuple(rid, data) != RM_EOF) {
            Attribute attribute;
            AttrType columnType;
            AttrLength columnLength;
            int offset = 1;             // set to skip over null indicator
            int varCharLength = 0;

            memcpy(&varCharLength, (char*)data + offset, sizeof(int));
            offset += sizeof(int);
            char columnNameArr[varCharLength+1];
            memcpy(&columnNameArr, (char*)data + offset, varCharLength);
            columnNameArr[varCharLength] = '\0';
            offset += varCharLength;
            memcpy(&columnType, (char*)data + offset, sizeof(int));
            offset += sizeof(int);
            memcpy(&columnLength, (char*)data + offset, sizeof(int));

            std::string columnName(columnNameArr);
            attribute.name = columnName;
            attribute.type = columnType;
            attribute.length = columnLength;
            recordDescriptor.push_back(attribute);
        }
/*
        // Close files
        rc1 = rbfm->closeFile(tableFileHandle);
        rc2 = rbfm->closeFile((columnFileHandle));
        if (rc1 == FAILURE || rc2 == FAILURE) {return FAILURE;}
*/
        free(data);
        return SUCCESS;
    }

    RC RelationManager::formatStr(const std::string &inBuffer, void* outBuffer) {
        unsigned len = inBuffer.length();
        memcpy(outBuffer, &len, sizeof(int));
        memcpy((char*)outBuffer + sizeof(int), inBuffer.c_str(), len);
        return SUCCESS;
    }
    /***********************************/


    // Extra credit work
    RC RelationManager::dropAttribute(const std::string &tableName, const std::string &attributeName) {
        return -1;
    }

    // Extra credit work
    RC RelationManager::addAttribute(const std::string &tableName, const Attribute &attr) {
        return -1;
    }

    // QE IX related
    RC RelationManager::createIndex(const std::string &tableName, const std::string &attributeName){
        return -1;
    }

    RC RelationManager::destroyIndex(const std::string &tableName, const std::string &attributeName){
        return -1;
    }

    // indexScan returns an iterator to allow the caller to go through qualified entries in index
    RC RelationManager::indexScan(const std::string &tableName,
                 const std::string &attributeName,
                 const void *lowKey,
                 const void *highKey,
                 bool lowKeyInclusive,
                 bool highKeyInclusive,
                 RM_IndexScanIterator &rm_IndexScanIterator){
        return -1;
    }


    RM_IndexScanIterator::RM_IndexScanIterator() = default;

    RM_IndexScanIterator::~RM_IndexScanIterator() = default;

    RC RM_IndexScanIterator::getNextEntry(RID &rid, void *key){
        return -1;
    }

    RC RM_IndexScanIterator::close(){
        return -1;
    }

} // namespace PeterDB