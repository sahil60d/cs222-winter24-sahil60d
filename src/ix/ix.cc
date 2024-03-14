#include "src/include/ix.h"
#include "src/include/pfm.h"
#include "src/include/qe.h"
#include "src/include/rbfm.h"

namespace PeterDB {
    IndexManager &IndexManager::instance() {
        static IndexManager _index_manager = IndexManager();
        return _index_manager;
    }

    RC IndexManager::createFile(const std::string &fileName) {
        if (access(fileName.c_str(), F_OK) == -1) {
            // File doesn't exist
            FILE *file =
                    fopen(fileName.c_str(), "wb"); // Open file in write + binary mode
            if (file == NULL)
                // failed
                return FAILURE;

            fclose(file);
            return SUCCESS;
        }
        return FAILURE;
    }

    RC IndexManager::destroyFile(const std::string &fileName) {
        // Delete file, handle fail
        if (std::remove(fileName.c_str()) != 0) {
            // Fails to delete file
            return FAILURE;
        }
        // Successfully delete file
        return SUCCESS;
    }

    RC IndexManager::openFile(const std::string &fileName,
                              IXFileHandle &ixFileHandle) {
        // Check if file exists
        if (access(fileName.c_str(), F_OK) == 0) {
            // link handle to file
            if (ixFileHandle.initFileHandle(fileName) == FAILURE) {
                return FAILURE;
            }
            return SUCCESS;
        }
        // File doesn't exist
        return FAILURE;
    }

    RC IndexManager::closeFile(IXFileHandle &ixFileHandle) {
        return ixFileHandle.closeFileHandle();
    }

    RC IndexManager::insertEntry(IXFileHandle &ixFileHandle,
                                 const Attribute &attribute, const void *key,
                                 const RID &rid) {
        void *page = malloc(PAGE_SIZE);

        // get root page
        PageNum rootPage = findRoot(ixFileHandle);
        if (ixFileHandle.readPage(rootPage, page) == FAILURE) {
            return FAILURE;
        }

        // create key descriptor
        KeyDesc keyDesc;
        keyDesc.key = malloc(PAGE_SIZE);

        // check node
        NodeDesc nodeDesc;
        memcpy(&nodeDesc, (char *)page + PAGE_SIZE - sizeof(NodeDesc),
               sizeof(NodeDesc));

        // check type; insert to leaf, traverse in non-leaf
        TreeOp resultOP;
        if (nodeDesc.type == LEAF) { // Root is Leaf node
            // insert into leaf node
            // return is tree operation was used
            // if split occured update tree path
            resultOP = insertLeaf(ixFileHandle, attribute, key, rid, rootPage, keyDesc, page);
        } else { // Non-Leaf
            resultOP = searchInsert(ixFileHandle, attribute, key, rid, rootPage,
                                           keyDesc, page);
        }

        // check if root was split. Create new root node with pushed up key
        if (resultOP == Split_OP) {
            // Create new root page and node desc
            PageNum newRootnum = findFree(ixFileHandle);
            NodeDesc newRootDesc;
            newRootDesc.type = NON_LEAF;
            newRootDesc.size = sizeof(KeyDesc) + keyDesc.keySize;

            void *newRootPage = malloc(PAGE_SIZE);
            memcpy((char*)newRootPage + (PAGE_SIZE - sizeof(NodeDesc)), &newRootDesc, sizeof(NodeDesc));

            // write key to page
            memcpy((char*)newRootPage, &keyDesc, sizeof(KeyDesc));
            memcpy((char*)newRootPage + sizeof(KeyDesc), keyDesc.key, keyDesc.keySize);

            // update root page pointer
            newRoot(ixFileHandle, newRootnum);

            // write page
            if (ixFileHandle.writePage(newRootnum, newRootPage) == FAILURE) { return FAILURE; }

            free(newRootPage);
        }

        free(keyDesc.key);
        //free(page);
        return SUCCESS;
    }

    PageNum IndexManager::findRoot(IXFileHandle &ixFileHandle) {
        void *page = malloc(PAGE_SIZE);
        memset(page, 0, PAGE_SIZE);
        PageNum root = 2; // Root page is 1 if tree hasn't been initialized

        // if root doesn't exist create one
        if (ixFileHandle.ixAppendPageCounter == 0) {
            // create node descriptor for root node
            NodeDesc nodeDesc;
            nodeDesc.type = LEAF;
            nodeDesc.size = 0;

            memcpy((char *)page + PAGE_SIZE - sizeof(NodeDesc), &nodeDesc,
                   sizeof(NodeDesc));

            // create page to store root page num
            void *ptrPage = malloc(PAGE_SIZE);
            memcpy(ptrPage, &root, sizeof(PageNum));
            ixFileHandle.appendPage(ptrPage); // page 1;
            free(ptrPage);

            // create root page
            ixFileHandle.appendPage(page); // page 2;
        } else {
            // read the root num from page 0
            ixFileHandle.readPage(1, page);
            memcpy(&root, (char *)page, sizeof(PageNum));
        }

        free(page);
        return root;
    }

    TreeOp IndexManager::insertLeaf(IXFileHandle &ixfileHandle,
                                    const Attribute &attribute, const void *key,
                                    const RID &rid, PageNum pageNum,
                                    KeyDesc &keyDesc, void *page) {
        // Default operation is error
        TreeOp op = No_OP;

        // get node description
        NodeDesc nodeDesc;
        memcpy(&nodeDesc, (char *)page + PAGE_SIZE - sizeof(NodeDesc),
               sizeof(NodeDesc));

        // New split page
        void *newPage = malloc(PAGE_SIZE);
        unsigned offset = 0;
        bool inserted = false;

        // split page if adding exceds upper threshold
        if (nodeDesc.size + keySize(attribute, key) + sizeof(DataDesc) + sizeof(RID) >
            UPPER_THRES) {
            op = Split_OP;

            // get offset of half the data
            while (offset < nodeDesc.size / 2) {
                DataDesc data;
                memcpy(&data, (char *)page + offset, sizeof(DataDesc));
                offset += sizeof(DataDesc) + data.keySize + (data.numRIDs * sizeof(RID));
            }

            // find/create new page for split
            PageNum freeNode = findFree(ixfileHandle);

            // add split data to new node
            // create new node descriptor
            NodeDesc newNodeDesc;
            newNodeDesc.type = LEAF;
            newNodeDesc.size = nodeDesc.size - offset;
            newNodeDesc.prev = pageNum;
            newNodeDesc.next = nodeDesc.next;

            // update original node descriptor
            nodeDesc.size = offset;
            nodeDesc.next = freeNode;

            // update the prev of the node that points to the new split node
            if (newNodeDesc.next != INVALID_PAGE) {
                void *tempPage = malloc(PAGE_SIZE);
                NodeDesc updateNode;
                ixfileHandle.readPage(newNodeDesc.next, tempPage);
                // copy node desriptor, updated it, write it back to page
                memcpy(&updateNode, (char *)tempPage + PAGE_SIZE - sizeof(NodeDesc),
                       sizeof(NodeDesc));
                updateNode.prev = freeNode;
                memcpy((char *)newPage + PAGE_SIZE - sizeof(NodeDesc), &updateNode,
                       sizeof(NodeDesc));
                ixfileHandle.writePage(newNodeDesc.next, tempPage);
                free(tempPage);
            }

            // update node descriptors
            memcpy((char *)page + PAGE_SIZE - sizeof(NodeDesc), &nodeDesc,
                   sizeof(NodeDesc));
            memcpy((char *)newPage + PAGE_SIZE - sizeof(nodeDesc), &newNodeDesc,
                   sizeof(NodeDesc));

            // Move data
            memcpy((char *)newPage, (char *)page + offset, newNodeDesc.size);

            // get first key from new node
            DataDesc firstKey;
            memcpy(&firstKey, (char *)newPage, sizeof(DataDesc));

            keyDesc.keySize = firstKey.keySize;
            memcpy(keyDesc.key, (char *)newPage + (sizeof(DataDesc) - sizeof(void*)), keyDesc.keySize);
            keyDesc.left = pageNum;   // orginal node
            keyDesc.right = freeNode; // new split node

            // write pages
            ixfileHandle.writePage(pageNum, page);
            ixfileHandle.writePage(freeNode, newPage);

            // check if entry should go in new split node
            // if entry key is greater than new nodes first key, update nodeDesc and
            // page to represet the split node
            if (compareKey(attribute, key, keyDesc.key) >= 0) {
                pageNum = freeNode;
                memcpy(page, newPage, PAGE_SIZE); // copy page
                memcpy(&nodeDesc, (char *)page + PAGE_SIZE - sizeof(NodeDesc),
                       sizeof(NodeDesc)); // copy node descriptor
            }

            free(newPage);
        }

        // insert entry
        offset = 0;
        DataDesc data;

        while (offset < nodeDesc.size) {
            // check every data descriptor in node
            memcpy(&data, (char *)page + offset, sizeof(DataDesc));
            data.key = malloc(data.keySize);
            memcpy(data.key, (char *)page + offset + sizeof(DataDesc), data.keySize);

            // compare key to each key in node
            // <0 -> insert <key,rid>, creates new key at offset
            // =0 -> insert rid to existing key
            // >0 -> increment offset to check next key, use "inserted" after while loop
            // to check if entry was added, if not create new <key,rid> at end
            int comp = compareKey(attribute, key, data.key);

            if (comp == 0) {
                // insert rid to exsisting key
                // get data size
                unsigned dataSize =
                        data.keySize + (data.numRIDs * sizeof(RID)) + sizeof(DataDesc);

                // move existing data back to add rid
                memmove((char *)page + offset + dataSize + sizeof(RID),
                        (char *)page + offset + dataSize,
                        nodeDesc.size - offset - dataSize);

                // add RID to key
                memcpy((char *)page + offset + dataSize, &rid, sizeof(RID));

                // update page info
                data.numRIDs++;
                nodeDesc.size += sizeof(RID);
                memcpy((char *)page + offset, &data, sizeof(DataDesc));
                memcpy((char *)page + PAGE_SIZE - sizeof(NodeDesc), &nodeDesc,
                       sizeof(NodeDesc));

                inserted = true;
                free(data.key);
                break;

            } else if (comp < 0) {
                // insert new <key,rid> at offset
                // get size of data to be moved
                unsigned moveData = nodeDesc.size - offset;

                // create new <key,rid>
                // add actual key and RID when adding data to node
                DataDesc newData;
                newData.numRIDs = 1;
                newData.keySize = keySize(attribute, key);

                // move data back to make space
                unsigned addSize =
                        newData.keySize + (newData.numRIDs * sizeof(RID)) + sizeof(DataDesc);
                memmove((char *)page + offset + addSize, (char *)page + offset, moveData);

                // write in new data + data.key + RID
                memcpy((char *)page + offset, &newData,
                       sizeof(DataDesc)); // data descriptor
                memcpy((char *)page + offset + sizeof(DataDesc), key,
                       newData.keySize); // key
                memcpy((char *)page + offset + sizeof(DataDesc) + newData.keySize, &rid,
                       sizeof(RID)); // rid

                // update page info
                nodeDesc.size += addSize;
                memcpy((char *)page + PAGE_SIZE - sizeof(NodeDesc), &nodeDesc,
                       sizeof(NodeDesc));

                inserted = true;
                free(data.key);
                break;
            }

            // increment offset to check next key
            offset += data.keySize + (data.numRIDs * sizeof(RID)) + sizeof(DataDesc);
            free(data.key); // free for reuse or break
        }

        // check if inserted, if not create new <key,rid>
        if (!inserted) {
            // add to the end of the node
            DataDesc newData;
            newData.numRIDs = 1;
            newData.keySize = keySize(attribute, key);
            memcpy((char *)page + offset, &newData,
                   sizeof(DataDesc)); // data descriptor
            memcpy((char *)page + offset + sizeof(DataDesc), key,
                   newData.keySize); // key
            memcpy((char *)page + offset + sizeof(DataDesc) + newData.keySize, &rid,
                   sizeof(RID)); // rid

            // update page info
            nodeDesc.size +=
                    newData.keySize + (newData.numRIDs * sizeof(RID) + sizeof(DataDesc));
            memcpy((char *)page + PAGE_SIZE - sizeof(NodeDesc), &nodeDesc,
                   sizeof(NodeDesc));
        }

        ixfileHandle.writePage(pageNum, page);
        //free(newPage);
        return op;
    }

    TreeOp IndexManager::searchInsert(IXFileHandle &ixfileHandle,
                                      const Attribute &attribute, const void *key,
                                      const RID &rid, PageNum pageNum,
                                      KeyDesc &keyDesc, void *page) {
        // get page info
        NodeDesc nodeDesc;
        memcpy(&nodeDesc, (char *)page + PAGE_SIZE - sizeof(NodeDesc),
               sizeof(NodeDesc));

        // search the tree to find the which page to insert to
        KeyDesc currKey;
        PageNum currPage;

        unsigned offset = 0;
        while (1) {
            memcpy(&currKey, (char *)page + offset, sizeof(KeyDesc));
            currKey.key = malloc(currKey.keySize);
            memcpy(currKey.key, (char *)page + sizeof(KeyDesc), currKey.keySize);

            // compare entry key to current key
            // <0 -> left node
            // else -> right node
            // last key -> right node
            // >=0 is the same as <0 for the next key, so it will be checked then
            int comp = compareKey(attribute, key, currKey.key);

            if (comp < 0) {
                // left node
                currPage = currKey.left;
                break;
            }

            if ((offset + (sizeof(KeyDesc) + currKey.keySize)) == nodeDesc.size) {
                currPage = currKey.right;
                break;
            }

            offset += sizeof(KeyDesc) + currKey.keySize;
        }

        // check if parent needs to be split on entry
        TreeOp parentSplitNeeded = No_OP;
        if (nodeDesc.size + keySize(attribute, key) > UPPER_THRES) {
            parentSplitNeeded = Split_OP;
        }

        // if node is leaf use insert leaf, else recursive call searchInsert
        void *checkPage = malloc(PAGE_SIZE);
        NodeDesc checkNode;
        KeyDesc newKeyDesc;
        newKeyDesc.key = malloc(PAGE_SIZE);
        TreeOp insertOp;

        ixfileHandle.readPage(currPage, checkPage);
        memcpy(&checkNode, (char *)checkPage + PAGE_SIZE - sizeof(NodeDesc),
               sizeof(NodeDesc));

        if (checkNode.type == LEAF) {
            insertOp = insertLeaf(ixfileHandle, attribute, key, rid, currPage,
                                  newKeyDesc, checkPage);
        }
        if (checkNode.type == NON_LEAF) {
            insertOp = searchInsert(ixfileHandle, attribute, key, rid, currPage,
                                    newKeyDesc, checkPage);
        }

        if (insertOp == Split_OP) {
            // update parent nodes
            // move up key into parent, split in case of overflow (parentSplitNeeded)
            // if no split, add key and update right siblings left node pointer

            if (parentSplitNeeded == Split_OP) {
                // split parent
                void *newNode = malloc(PAGE_SIZE);
                unsigned off = 0;

                // get offset of half the data
                while (off < nodeDesc.size / 2) {
                    KeyDesc checkKey;
                    memcpy(&checkKey, (char*)page + off, sizeof(KeyDesc));
                    off += sizeof(KeyDesc) + checkKey.keySize;
                }

                // find/create new page for split
                //PageNum freeNode = findFree(ixfileHandle);

                // add split data to new node
                // create new node descriptor
                NodeDesc newNodeDesc;
                newNodeDesc.type = NON_LEAF;
                newNodeDesc.size = nodeDesc.size - off;
                newNodeDesc.next = findFree(ixfileHandle);

                // update original node descriptor
                nodeDesc.size = off;

                // move data
                memcpy((char*)newNode, (char*)page + off, newNodeDesc.size);

                // get first key from new node
                //KeyDesc firstKey;
                //memcpy(&firstKey, (char*)newNode, sizeof(KeyDesc));
                //firstKey.left = pageNum;
                //firstKey.right = newNodeDesc.next;

                memcpy(&keyDesc, (char*)newNode, sizeof(KeyDesc));
                keyDesc.left = pageNum;
                keyDesc.right = newNodeDesc.next;

                //keyDesc.keySize = firstKey.keySize;
                memcpy(keyDesc.key, (char*)newNode + sizeof(KeyDesc), keyDesc.keySize);

                // key don't get copied in internal nodes so remove it.
                memmove((char*)newNode, (char*)newNode + sizeof(KeyDesc) + keyDesc.keySize, newNodeDesc.size - (sizeof(KeyDesc) + keyDesc.keySize));
                newNodeDesc.size -= sizeof(KeyDesc) + keyDesc.keySize;

                // write pages
                ixfileHandle.writePage(pageNum, page);
                ixfileHandle.writePage(newNodeDesc.next, newNode);

                // node has been split, determine which node and where to enter key based on offset value
                // offset < nodeDesc.size -> original node, put it where it is
                // offset = nodeDesc.size -> new node, subtract nodeDesc.size from offset and enter

                if (offset >= nodeDesc.size) {
                    offset -= nodeDesc.size;

                    // add key
                    memmove((char*) newNode + offset + sizeof(KeyDesc) + newKeyDesc.keySize, (char*)newNode + offset, nodeDesc.size - offset);
                    memcpy((char*)newNode + offset, &newKeyDesc, sizeof(KeyDesc));
                    memcpy((char*)newNode + offset + sizeof(KeyDesc), newKeyDesc.key, newKeyDesc.keySize);

                    // update right sibling
                    KeyDesc rightSib;
                    memcpy(&rightSib, (char*)newNode + offset + sizeof(KeyDesc) + newKeyDesc.keySize, sizeof(KeyDesc));
                    rightSib.left = newKeyDesc.right;
                    memcpy((char*)newNode + offset + sizeof(KeyDesc) + newKeyDesc.keySize, &rightSib, sizeof(KeyDesc));

                    // update node info (size)
                    newNodeDesc.size += sizeof(KeyDesc) + newKeyDesc.keySize;
                    memcpy((char*)newNode + (PAGE_SIZE - sizeof(NodeDesc)), &nodeDesc, sizeof(NodeDesc));

                    // write page
                    ixfileHandle.writePage(nodeDesc.next, newNode);
                } else {
                    memmove((char*)page + offset + newKeyDesc.keySize + sizeof(KeyDesc), (char*)page + offset, nodeDesc.size - offset);
                    memcpy((char*)page + offset, &newKeyDesc, sizeof(KeyDesc));
                    memcpy((char*)page + offset + sizeof(KeyDesc), newKeyDesc.key, newKeyDesc.keySize);

                    // update right sibling
                    KeyDesc rightSib;
                    memcpy(&rightSib, (char*)page + offset + sizeof(KeyDesc) + newKeyDesc.keySize, sizeof(KeyDesc));
                    rightSib.left = newKeyDesc.right;
                    memcpy((char*)page + offset + sizeof(KeyDesc) + newKeyDesc.keySize, &rightSib, sizeof(KeyDesc));

                    // update node info (size)
                    nodeDesc.size += sizeof(KeyDesc) + newKeyDesc.keySize;
                    memcpy((char*)page + (PAGE_SIZE - sizeof(NodeDesc)), &nodeDesc, sizeof(NodeDesc));

                    // write page
                    ixfileHandle.writePage(pageNum, page);
                }

            } else {
                // no overflow in parent node
                //add key (move remaining keys right, then add keyDesc, then add key val)
                memmove((char*)page + offset + newKeyDesc.keySize + sizeof(KeyDesc), (char*)page + offset, nodeDesc.size - offset);
                memcpy((char*)page + offset, &newKeyDesc, sizeof(KeyDesc));
                memcpy((char*)page + offset + sizeof(KeyDesc), newKeyDesc.key, newKeyDesc.keySize);

                // update right sibling
                KeyDesc rightSib;
                memcpy(&rightSib, (char*)page + offset + sizeof(KeyDesc) + newKeyDesc.keySize, sizeof(KeyDesc));
                rightSib.left = newKeyDesc.right;
                memcpy((char*)page + offset + sizeof(KeyDesc) + newKeyDesc.keySize, &rightSib, sizeof(KeyDesc));

                // update node info (size)
                nodeDesc.size += sizeof(KeyDesc) + newKeyDesc.keySize;
                memcpy((char*)page + (PAGE_SIZE - sizeof(NodeDesc)), &nodeDesc, sizeof(NodeDesc));

                // write page
                ixfileHandle.writePage(pageNum, page);
            }
        }

        free(currKey.key);
        free(checkPage);

        // return parentSplitNeeded in case root needs to be split.
        if (insertOp == Split_OP && parentSplitNeeded == Split_OP) {
            return Split_OP;
        } else {
            return No_OP;
        }
    }

    PageNum IndexManager::findFree(IXFileHandle &ixfileHandle) {
        void *page = malloc(PAGE_SIZE);

        // Check each node (page), free if size == 0
        for (int i = 2; i <= ixfileHandle.getNumNodes(); i++) {
            // Get page info
            ixfileHandle.readPage(i, page);

            NodeDesc nodeDesc;
            memcpy(&nodeDesc, (char *)page + PAGE_SIZE - sizeof(NodeDesc),
                   sizeof(nodeDesc));

            if (nodeDesc.size == 0) {
                // node is free
                free(page);
                return (PageNum)i;
            }
        }

        // no free page exists, create new one
        free(page);
        return newNode(ixfileHandle, LEAF);
    }

    PageNum IndexManager::newNode(IXFileHandle &ixfileHandle, NodeType type) {
        void *page = malloc(PAGE_SIZE);
        NodeDesc nodeDesc;
        nodeDesc.type = type;
        nodeDesc.size = 0;
        memcpy((char *)page + PAGE_SIZE - sizeof(NodeDesc), &nodeDesc,
               sizeof(NodeDesc));
        ixfileHandle.appendPage(page);
        free(page);
        return ixfileHandle.getNumNodes();
    }

    int IndexManager::compareKey(const Attribute &attribute, const void *A,
                                 const void *B) {
        int intA, intB;
        float floatA, floatB;
        const double epsilon = 1e-9; // used for percision with floats
        switch (attribute.type) {
            case TypeInt:
                memcpy(&intA, A, sizeof(int));
                memcpy(&intB, B, sizeof(int));
                return intA - intB;

            case TypeReal:
                memcpy(&floatA, A, sizeof(float));
                memcpy(&floatB, B, sizeof(float));
                if (std::fabs(floatA - floatB) < epsilon) {
                    return 0; // equal
                } else if (floatA > floatB) {
                    return 1; // a is greater
                } else {
                    return -1; // a is less
                }
                break;

            case TypeVarChar:
                memcpy(&intA, A, sizeof(int));
                memcpy(&intB, B, sizeof(int));
                std::string strA((char *)A + sizeof(int), intA);
                std::string strB((char *)B + sizeof(int), intB);
                return strA.compare(strB);
        }
    }

    unsigned IndexManager::keySize(const Attribute &attribute, const void *key) {
        unsigned size = 0;

        // int, real = 4 bytes
        // Varchar -> [length][string]
        switch (attribute.type) {
            case TypeInt:
                return sizeof(int);
            case TypeReal:
                return sizeof(float);
            case TypeVarChar:
                memcpy(&size, key, sizeof(int));
                return sizeof(int) + size;
        }
    }
/*
RC IndexManager::shiftTree(IXFileHandle &ixfileHandle, Attribute &attribute,
                           KeyDesc &keyDesc) {
  // creates/updates parent node
  PageNum parent;
  parent = findFree(ixfileHandle);
  if (newRoot(ixfileHandle, parent) == FAILURE) {
    return FAILURE;
  }

  Node

}
*/
    RC IndexManager::newRoot(IXFileHandle &ixfileHandle, PageNum pageNum) {
        void *data = malloc(PAGE_SIZE);

        // read and update page 0 (points to root)
        if (ixfileHandle.readPage(1, data) == FAILURE) {
            return FAILURE;
        }

        memcpy((char *)data, &pageNum, sizeof(PageNum));

        if (ixfileHandle.writePage(1, data) == FAILURE) {
            return FAILURE;
        }

        free(data);
        return SUCCESS;
    }

    RC IndexManager::deleteEntry(IXFileHandle &ixFileHandle,
                                 const Attribute &attribute, const void *key,
                                 const RID &rid) {
        // get root node
        void *page = malloc(PAGE_SIZE);
        PageNum root = findRoot(ixFileHandle);
        if (ixFileHandle.readPage(root, page) == FAILURE) {
            return FAILURE;
        }

        // read root info
        NodeDesc nodeDesc;
        memcpy(&nodeDesc, (char *)page + PAGE_SIZE - sizeof(NodeDesc),
               sizeof(NodeDesc));

        if (nodeDesc.type == LEAF) {
            // root is leaf, find rid and delete
            DataDesc data;
            unsigned offset = 0;
            while (offset < nodeDesc.size) {
                memcpy(&data, (char *)page + offset, sizeof(DataDesc));
                data.key = malloc(data.keySize);
                memcpy(data.key, (char *)page + offset + sizeof(DataDesc), data.keySize);

                if (compareKey(attribute, key, data.key) == 0) {
                    // match
                    RID temp_rid;
                    for (int i = 0; i < data.numRIDs; i++) {
                        unsigned lenToRID =
                                sizeof(DataDesc) + data.keySize + (i * sizeof(RID));
                        memcpy(&temp_rid, (char *)page + offset + lenToRID, sizeof(RID));
                        if (temp_rid.pageNum == rid.pageNum && temp_rid.slotNum == rid.slotNum) {

                            // found rid, update page
                            unsigned remainSize = nodeDesc.size - offset - lenToRID - sizeof(RID);
                            memmove((char *)page + offset + lenToRID,(char *)page + offset + lenToRID + sizeof(RID),sizeof(RID));

                            data.numRIDs--;
                            memcpy((char*)page + offset, &data, sizeof(DataDesc));
                            nodeDesc.size -= sizeof(RID);

                            // if no more rids exist, also delete page
                            if (data.numRIDs == 0) {
                                memmove((char*)page + offset, (char*)page + offset + sizeof(DataDesc) + data.keySize, sizeof(DataDesc) + data.keySize);
                                nodeDesc.size -= sizeof(DataDesc) + data.keySize;
                            }

                            // write to disk
                            memcpy((char*)page + (PAGE_SIZE - sizeof(NodeDesc)), &nodeDesc, sizeof(NodeDesc));
                            ixFileHandle.writePage(root, page);

                            free(data.key);
                            free(page);
                            return SUCCESS;
                        }
                    }
                }
                // get next data
                offset += data.keySize + (data.numRIDs * sizeof(RID) + sizeof(DataDesc));
            }
            //free(data.key);
            free(page);
            return FAILURE;
        }

        if (nodeDesc.type == NON_LEAF) {
            PageNum leaf = checkChild(ixFileHandle, attribute, key, root);
            if (ixFileHandle.readPage(leaf, page) == FAILURE) {
                return FAILURE;
            }

            DataDesc data;
            unsigned offset = 0;
            while (offset <= nodeDesc.size) {
                memcpy(&data, (char *)page + offset, sizeof(DataDesc));
                data.key = malloc(data.keySize);
                memcpy(&data.key, (char *)page + offset + sizeof(DataDesc), data.keySize);

                if (compareKey(attribute, key, data.key) == 0) {
                    // match
                    RID temp_rid;
                    for (int i = 0; i < data.numRIDs; i++) {
                        unsigned lenToRID =
                                sizeof(DataDesc) + data.keySize + (i * sizeof(RID));
                        memcpy(&temp_rid, (char *)page + offset + lenToRID, sizeof(RID));
                        if (temp_rid.pageNum == rid.pageNum &&
                            temp_rid.slotNum == rid.slotNum) {
                            // found rid, update page
                            unsigned remainSize =
                                    nodeDesc.size - offset - lenToRID - sizeof(RID);
                            memmove((char *)page + offset + lenToRID,
                                    (char *)page + offset + lenToRID + sizeof(RID),
                                    sizeof(RID));

                            data.numRIDs--;
                            nodeDesc.size -= sizeof(RID);
                            free(data.key);
                            free(page);
                            return SUCCESS;
                        }
                    }
                }
                // get next data
                offset += data.keySize + (data.numRIDs * sizeof(RID) + sizeof(DataDesc));
            }
            free(data.key);
            free(page);
            return FAILURE;
        }

        free(page);
        return FAILURE;
    }

    PageNum IndexManager::checkChild(IXFileHandle &ixFileHandle,
                                     const Attribute &attribute, const void *key,
                                     PageNum pageNum) {
        void *page = malloc(PAGE_SIZE);
        ixFileHandle.readPage(pageNum, page);

        NodeDesc nodeDesc;
        memcpy(&nodeDesc, (char *)page + PAGE_SIZE - sizeof(NodeDesc),
               sizeof(NodeDesc));

        if (nodeDesc.type == LEAF) {
            DataDesc data;
            unsigned offset = 0;
            while (offset <= nodeDesc.size) {
                memcpy(&data, (char *)page + offset, sizeof(DataDesc));
                data.key = malloc(data.keySize);
                memcpy(&data.key, (char *)page + offset + sizeof(DataDesc), data.keySize);

                if (compareKey(attribute, key, data.key) == 0) {
                    // match
                    free(data.key);
                    free(page);
                    return pageNum;
                }
                // get next data
                offset += data.keySize + (data.numRIDs * sizeof(RID) + sizeof(DataDesc));
            }
            free(data.key);
            free(page);
            return FAILURE;
        }

        KeyDesc checkKey;
        unsigned offset = 0;
        while (offset <= nodeDesc.size) {
            memcpy(&checkKey, (char *)page + offset, sizeof(KeyDesc));
            checkKey.key = malloc(checkKey.keySize);
            memcpy(checkKey.key, (char *)page + offset + sizeof(KeyDesc),
                   checkKey.keySize);

            if (compareKey(attribute, key, checkKey.key) < 0) {
                free(checkKey.key);
                return checkChild(ixFileHandle, attribute, key, checkKey.left);
            }

            if (offset == nodeDesc.size) {
                free(checkKey.key);
                return checkChild(ixFileHandle, attribute, key, checkKey.right);
            }

            offset += sizeof(KeyDesc) + checkKey.keySize;
        }

        free(page);
        return FAILURE;
    }

    RC IndexManager::scan(IXFileHandle &ixFileHandle, const Attribute &attribute,
                          const void *lowKey, const void *highKey,
                          bool lowKeyInclusive, bool highKeyInclusive,
                          IX_ScanIterator &ix_ScanIterator) {

        // check if exists (append count == 0)
        if (ixFileHandle.ixAppendPageCounter == 0) {
            return FAILURE;
        }

        ix_ScanIterator.ixFileHandle = &ixFileHandle;
        ix_ScanIterator.attribute = attribute;
        ix_ScanIterator.lowKey = (char *)lowKey;
        ix_ScanIterator.highKey = (char *)highKey;
        ix_ScanIterator.lowKeyInclusive = lowKeyInclusive;
        ix_ScanIterator.highKeyInclusive = highKeyInclusive;

        //IndexManager im = IndexManager::instance();

        // get low and high keys
        if (lowKey != nullptr) {
            int length = keySize(attribute, lowKey);
            ix_ScanIterator.lowKey = malloc(length);
            memcpy(&ix_ScanIterator.lowKey, lowKey, length);
        } else {
            // set equal to negative infinity
            ix_ScanIterator.lowNull = true;
            float neg_inf = NEG_INF;
            ix_ScanIterator.lowKey = malloc(sizeof(float));
            memcpy(&ix_ScanIterator.lowKey, &neg_inf, sizeof(float));
        }

        if (highKey != nullptr) {
            int length = keySize(attribute, highKey);
            ix_ScanIterator.highKey = malloc(length);
            memcpy(ix_ScanIterator.highKey, highKey, length);
        } else {
            // set equal to positive infinity
            ix_ScanIterator.highNull = true;
            float pos_inf = POS_INF;
            ix_ScanIterator.highKey = malloc(sizeof(float));
            memcpy(ix_ScanIterator.highKey, &pos_inf, sizeof(float));
        }

        // read root page
        // store first pageNum and <key,rid> pair
        PageNum root = findRoot(ixFileHandle);
        ix_ScanIterator.currPage = root;

        if (ix_ScanIterator.lowNull) {
            // find first
            void *page = malloc(PAGE_SIZE);
            NodeDesc nodeDesc;
            KeyDesc keyDesc;
            while (true) {
                ixFileHandle.readPage(ix_ScanIterator.currPage, page);
                memcpy(&nodeDesc, (char *)page + PAGE_SIZE - sizeof(NodeDesc),
                       sizeof(NodeDesc));
                if (nodeDesc.type == LEAF) {
                    free(page);
                    break;
                }
                memcpy(&keyDesc, (char *)page, sizeof(keyDesc));
                ix_ScanIterator.currPage = keyDesc.left;
            }
        } else {
            ix_ScanIterator.currPage =
                    checkChild(ixFileHandle, attribute, lowKey, root);
        }

        ix_ScanIterator.currData = 0;
        ix_ScanIterator.currRID = 0;

        // found page, now find  first data if lowkey is not NULL
        if (!ix_ScanIterator.lowNull) {
            void *page = malloc(PAGE_SIZE);
            ixFileHandle.readPage(ix_ScanIterator.currPage, page);
            NodeDesc nodeDesc;
            memcpy(&nodeDesc, (char *) page + (PAGE_SIZE - sizeof(NodeDesc)), sizeof(NodeDesc));

            DataDesc data;
            while (ix_ScanIterator.currData < nodeDesc.size) {
                memcpy(&data, (char *) page + ix_ScanIterator.currData, sizeof(DataDesc));
                void *checkKey = malloc(data.keySize);
                memcpy(&checkKey, (char *) page + ix_ScanIterator.currData + sizeof(DataDesc), data.keySize);

                // if inclusive, check equal (and greater for safety), else greater
                int comp = compareKey(attribute, checkKey, lowKey);
                if (ix_ScanIterator.lowKeyInclusive) {
                    // included
                    if (comp >= 0) {
                        free(checkKey);
                        return SUCCESS;
                    }
                } else {
                    // not included
                    if (comp > 0) {
                        free(checkKey);
                        return SUCCESS;
                    }
                }

                // go to next data offset
                ix_ScanIterator.currData += sizeof(DataDesc) + data.keySize + (data.numRIDs * sizeof(RID));
            }

            free(page);
            return FAILURE;
        }

        return SUCCESS;
    }

    RC IndexManager::printBTree(IXFileHandle &ixFileHandle,
                                const Attribute &attribute,
                                std::ostream &out) const {
        PageNum root = ixFileHandle.getRoot();
        return printBTreeNode(ixFileHandle, attribute, out, root);
    }
/*
// get root page
void *page = malloc(PAGE_SIZE);
PageNum root = ixFileHandle.getRoot();
// read root page
if (ixFileHandle.readPage(root, page) == FAILURE) {
return FAILURE;
}

// create vector of order of page number to read from
std::vector<PageNum> order;
ixFileHandle.dfs(order, root);

std::vector<PageNum> uniqueOrder;
for (auto i : order) {
auto it = std::find(order.begin(), order.end(), i);
if (it == order.end()) {
uniqueOrder.push_back(i);
}
}

ElementType type = KEYS;
for (auto pageNum : order) {
if (ixFileHandle.readPage(pageNum, page) == FAILURE) {
return FAILURE;
}
NodeDesc nodeDesc;
memcpy(&nodeDesc, (char *)page + PAGE_SIZE - sizeof(NodeDesc),
sizeof(NodeDesc));

if (nodeDesc.type == LEAF) {
out << "{\"keys\": [";

int offset = 0;
DataDesc data;
while (offset < nodeDesc.size) {
if (offset != 0) {
out << ",";
}
memcpy(&data, (char *)page + offset, sizeof(DataDesc));
void *printKey = malloc(data.keySize);
memcpy(printKey, (char *)page + offset + sizeof(DataDesc),
sizeof(data.keySize));
out << "\"";
printKey(attribute, printKey, out);
out << ":[";
RID rid;
for (int i = 0; i < data.numRIDs; i++) {
memcpy(&rid,
(char *)page + offset + data.keySize +
(i * sizeof(RID) + sizeof(DataDesc)),
sizeof(RID));
out << "(" << rid.pageNum << "," << rid.slotNum << ")";
if (i != data.numRIDs - 1) {
out << ", ";
}
}

out << "]\"";
offset +=
data.keySize + (data.numRIDs * sizeof(RID)) + sizeof(DataDesc);
free(printKey);
}
out << "]}\"";
}

if (nodeDesc.type == NON_LEAF) {
(type == KEYS) ? out << "{"
<< "\"keys\""
<< ":["
: out << "{"
<< "\"children\""
<< ":[";

if (type == KEYS) {
int offset = 0;

}

type = (type == KEYS) ? CHILD : KEYS;
}
}
}
*/

    RC IndexManager::printBTreeNode(IXFileHandle &ixFileHandle,
                                    const Attribute &attribute, std::ostream &out,
                                    PageNum pageNum) const {
        // get root page
        void *page = malloc(PAGE_SIZE);
        // PageNum root = ixFileHandle.getRoot();
        // read root page
        if (ixFileHandle.readPage(pageNum, page) == FAILURE) {
            return FAILURE;
        }
        // get root nodeDesc
        NodeDesc nodeDesc;
        memcpy(&nodeDesc, (char *)page + PAGE_SIZE - sizeof(NodeDesc),
               sizeof(NodeDesc));

        if (nodeDesc.type == LEAF) {
            out << "{\"keys\": [";

            int offset = 0;
            DataDesc data;
            while (offset < nodeDesc.size) {
                if (offset != 0) {
                    out << ",";
                }
                memcpy(&data, (char *)page + offset, sizeof(DataDesc));
                data.key = malloc(data.keySize);
                memcpy(data.key, (char *)page + offset + sizeof(DataDesc),
                       sizeof(data.keySize));
                out << "\"";
                printKey(attribute, data.key, out);
                out << ":[";
                RID rid;
                for (int i = 0; i < data.numRIDs; i++) {
                    memcpy(&rid,
                           (char *)page + offset + data.keySize +
                           (i * sizeof(RID) + sizeof(DataDesc)),
                           sizeof(RID));
                    out << "(" << rid.pageNum << "," << rid.slotNum << ")";
                    if (i != data.numRIDs - 1) {
                        out << ", ";
                    }
                }

                out << "]\"";
                offset += data.keySize + (data.numRIDs * sizeof(RID)) + sizeof(DataDesc);
                free(data.key);
            }
            out << "]}\"";
            return SUCCESS;
        }

        if (nodeDesc.type == NON_LEAF) {

            std::vector<PageNum> subTree;

            out << "{\"keys\":[";

            KeyDesc key;
            unsigned offset = 0;
            while (offset < nodeDesc.size) {
                if (offset != 0) {
                    out << ",";
                }
                memcpy(&key, (char *)page + offset, sizeof(KeyDesc));
                key.key = malloc(key.keySize);
                memcpy(key.key, (char *)page + offset + sizeof(KeyDesc), key.keySize);

                out << "\"";
                printKey(attribute, key.key, out);
                out << "\"";

                subTree.push_back(key.left);
                free(key.key);
                offset += key.keySize + sizeof(KeyDesc);
            }
            subTree.push_back(key.right);

            out << "],\n";
            out << "\"children\":[";

            for (int i = 0; i < subTree.size(); i++) {
                printBTreeNode(ixFileHandle, attribute, out, i);
                if (i != subTree.size() - 1) {
                    out << ",\n";
                }
            }
        }
        out << "]}" << std::endl;
        return SUCCESS;
    }

    RC IndexManager::printKey(const Attribute &attribute, const void *key,
                              std::ostream &out) const {
        switch (attribute.type) {
            case TypeInt:
                out << *(int *)key;
                return SUCCESS;
            case TypeReal:
                out << *(float *)key;
                return SUCCESS;
            case TypeVarChar:
                int length;
                memcpy(&length, key, sizeof(int));
                std::string strKey((char *)key + sizeof(int), length);
                out << strKey.c_str();
                return SUCCESS;
        }
    }

    IX_ScanIterator::IX_ScanIterator() {}

    IX_ScanIterator::~IX_ScanIterator() {}

    RC IX_ScanIterator::getNextEntry(RID &rid, void *key) {
        // start from saved <key,rid>
        // go until highKey

//        if (currKey == -1 || currRID == -1 || currData == -1) {
//            // scan never called to initialize
//            return FAILURE;
//        }

        if (currPage == INVALID_PAGE) {
            return IX_EOF;
        }

        void *page = malloc(PAGE_SIZE);

        // read current page
        if (ixFileHandle->readPage(currPage, page) == FAILURE) { return FAILURE; }

        NodeDesc nodeDesc;
        memcpy(&nodeDesc, (char*)page + (PAGE_SIZE - sizeof(NodeDesc)), sizeof(NodeDesc));

        if (nodeDesc.type != LEAF) { return FAILURE; }

//        DataDesc data;
//        unsigned offset = currData + currRID;
//        while(offset < nodeDesc.size) {
//            // get data descriptor
//            memcpy(&data, (char*)page + currData, sizeof(DataDesc));
//            memcpy(&key, (char*)page + offset + sizeof(DataDesc), data.keySize);
//
//            if (IM.compareKey(attribute, key, highKey) >= 0) {
//                free(page);
//                return IX_EOF;
//            }
//
//            // copy RID
//            memcpy(&rid, (char*)page + currData + data.keySize + sizeof(DataDesc) + (currRID * sizeof(RID)), sizeof(RID));
//
//
//        }
//
//        // move to next page
//        currPage = nodeDesc.next;
//        if (currPage == INVALID_PAGE) {
//            free(page);
//            return IX_EOF;
//        }
//
//        currData = 0;
//        currRID = 0;

        // get data descriptor and key value
        DataDesc data;
        memcpy(&data, (char*)page + currData, sizeof(DataDesc));
        memcpy(key, (char*)page + currData + sizeof(DataDesc), data.keySize);

        // if key is >= high key -> no entries left to satisfy range
        if (highKeyInclusive && IM.compareKey(attribute, key, highKey) > 0) {
                free(page);
                return IX_EOF;
            }

        if (!highKeyInclusive && IM.compareKey(attribute, key, highKey) >= 0) {
            free(page);
            return IX_EOF;
        }

        // copy RID
        memcpy(&rid, (char*)page + currData + data.keySize + sizeof(DataDesc) + (currRID * sizeof(RID)), sizeof(RID));

        // move to next RID, check if spills to next key
        currRID++;
        if (currRID >= data.numRIDs) {
            // move to next key
            currData += sizeof(DataDesc) + data.keySize + (data.numRIDs * sizeof(RID));
            currRID = 0;
        }

        // check if data is at end of page
        if (currData >= nodeDesc.size) {
            currPage = nodeDesc.next;
            currData = 0;
            currRID = 0;
        }

        return SUCCESS;
    }

    RC IX_ScanIterator::close() {
        //free(lowKey);
        //free(highKey);
        return SUCCESS;
    }

    IXFileHandle::IXFileHandle() {
        ixReadPageCounter = 0;
        ixWritePageCounter = 0;
        ixAppendPageCounter = 0;
    }

    IXFileHandle::~IXFileHandle() {}

    RC IXFileHandle::collectCounterValues(unsigned &readPageCount,
                                          unsigned &writePageCount,
                                          unsigned &appendPageCount) {
        readPageCount = ixReadPageCounter;
        writePageCount = ixWritePageCounter;
        appendPageCount = ixAppendPageCounter;
        return SUCCESS;
    }

    RC IXFileHandle::readPage(PageNum pageNum, void *data) {
        ixReadPageCounter++;
        return fileHandle.readPage(pageNum - 1, data);
    }

    RC IXFileHandle::writePage(PageNum pageNum, const void *data) {
        ixWritePageCounter++;
        return fileHandle.writePage(pageNum - 1, data);
    }

    RC IXFileHandle::appendPage(const void *data) {
        ixAppendPageCounter++;
        return fileHandle.appendPage(data);
    }

    PageNum IXFileHandle::getRoot() {
        PageNum root;
        void *page = malloc(PAGE_SIZE);
        fileHandle.readPage(0, page);
        memcpy(&root, (char *)page, sizeof(PageNum));
        return root;
    }

    RC IXFileHandle::dfs(std::vector<PageNum> &order, PageNum curr) {
        void *page = malloc(PAGE_SIZE);
        if (readPage(curr, page) == FAILURE) {
            return FAILURE;
        }
        NodeDesc nodeDesc;
        memcpy(&nodeDesc, (char *)page + PAGE_SIZE - sizeof(NodeDesc),
               sizeof(NodeDesc));

        order.push_back(curr);

        if (nodeDesc.type == LEAF) {
            return SUCCESS;
        }

        unsigned offset = 0;
        while (offset <= nodeDesc.size) {
            KeyDesc key;
            memcpy(&key, (char *)page + offset, sizeof(KeyDesc));
            offset += sizeof(KeyDesc) + key.keySize;

            dfs(order, key.left);

            dfs(order, key.right);
        }

        return SUCCESS;
    }

    unsigned IXFileHandle::getNumNodes() { return fileHandle.getNumberOfPages(); }

    RC IXFileHandle::initFileHandle(const std::string &fileName) {
        return fileHandle.initFileHandle(fileName);
    }

    RC IXFileHandle::closeFileHandle() { return fileHandle.closeFileHandle(); }

} // namespace PeterDB
