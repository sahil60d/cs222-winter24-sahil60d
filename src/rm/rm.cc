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
        std::string tables = "Tables";
        std::string columns = "Columns";
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


        //rc1 = rbfm->insertRecord(tableFileHandle, tableRecordDescription, )
        return -1;
    }

    RC RelationManager::deleteCatalog() {
        return -1;
    }

    RC RelationManager::createTable(const std::string &tableName, const std::vector<Attribute> &attrs) {
        return -1;
    }

    RC RelationManager::deleteTable(const std::string &tableName) {
        return -1;
    }

    RC RelationManager::getAttributes(const std::string &tableName, std::vector<Attribute> &attrs) {
        return -1;
    }

    RC RelationManager::insertTuple(const std::string &tableName, const void *data, RID &rid) {
        return -1;
    }

    RC RelationManager::deleteTuple(const std::string &tableName, const RID &rid) {
        return -1;
    }

    RC RelationManager::updateTuple(const std::string &tableName, const void *data, const RID &rid) {
        return -1;
    }

    RC RelationManager::readTuple(const std::string &tableName, const RID &rid, void *data) {
        return -1;
    }

    RC RelationManager::printTuple(const std::vector<Attribute> &attrs, const void *data, std::ostream &out) {
        return -1;
    }

    RC RelationManager::readAttribute(const std::string &tableName, const RID &rid, const std::string &attributeName,
                                      void *data) {
        return -1;
    }

    RC RelationManager::scan(const std::string &tableName,
                             const std::string &conditionAttribute,
                             const CompOp compOp,
                             const void *value,
                             const std::vector<std::string> &attributeNames,
                             RM_ScanIterator &rm_ScanIterator) {
        return -1;
    }

    RM_ScanIterator::RM_ScanIterator() = default;

    RM_ScanIterator::~RM_ScanIterator() = default;

    RC RM_ScanIterator::getNextTuple(RID &rid, void *data) { return RM_EOF; }

    RC RM_ScanIterator::close() { return -1; }

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
        int dataSize = sizeof(int)*3 + tableNameLen + fileNameLen;         // get size of data
        void *data = malloc(dataSize);

        // store info into data
        int tableId = 0; //FIX
        int offset = 0;
        memcpy(data, &tableId, sizeof(int));
        offset += sizeof(int);
        memcpy((char*)data + offset, &tableNameLen, sizeof(int));
        offset += sizeof(int);
        memcpy((char*)data + offset, tableName.c_str(), tableNameLen);
        offset += tableNameLen;
        memcpy((char*)data + offset, &fileNameLen, sizeof(int));
        offset += sizeof(int);
        memcpy((char*)data + offset, fileName.c_str(), fileNameLen);

        if (rbfm->insertRecord(fileHandle, recordDescriptor, data, rid) == FAILURE) {
            return FAILURE;
        }

        return SUCCESS;
    }

    RC RelationManager::insertColumns(FileHandle &fileHandle, const std::vector<Attribute> &recordDescriptor, RID &rid, const std::string &columnName, const int &columnType, const int &columnLength, const int &columnPosition) {

        return FAILURE;
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