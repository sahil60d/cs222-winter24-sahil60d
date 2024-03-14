#ifndef _ix_h_
#define _ix_h_

#include <limits>
#include <string>
#include <vector>

#include "pfm.h"
#include "rbfm.h" // for some type declarations only, e.g., RID and Attribute

#define IX_EOF (-1) // end of the index scan
#define LEAF 1
#define NON_LEAF 2
#define INVALID_PAGE (-1)
#define UPPER_THRES (PAGE_SIZE - sizeof(NodeDesc))
#define POS_INF std::numeric_limits<float>::infinity()
#define NEG_INF (-std::numeric_limits<float>::infinity())
#define IM IndexManager::instance()

//#define DIR_SIZE                                   #include "src/include/ix.h"                            \
  PAGE_SIZE / sizeof(PageNum) // 1024, number of PageNums that can be stored in
// the directory

namespace PeterDB {
    typedef char NodeType;
    typedef unsigned PageSize;
    typedef char ElementType;

    typedef enum { Split_OP, No_OP, Error_OP } TreeOp;

// Information about the Node (page)
    typedef struct {
        NodeType type;               // Leaf=1, Non-Leaf=2
        PageSize size;               // size doesn't include Node Descriptor
        PageNum prev = INVALID_PAGE; // previous leaf node in linked list
        PageNum next = INVALID_PAGE; // next leaf node in linked list
    } NodeDesc;

// Information about the Key
    typedef struct {
        PageNum left;
        PageNum right;
        unsigned keySize;
        void *key;
    } KeyDesc;

// Information about RIDs for specific key
    typedef struct {
        unsigned numRIDs;
        unsigned keySize;
        void *key;
    } DataDesc;

    class IX_ScanIterator;

    class IXFileHandle;

    class IndexManager {

    public:
        static IndexManager &instance();

        // Create an index file.
        RC createFile(const std::string &fileName);

        // Delete an index file.
        RC destroyFile(const std::string &fileName);

        // Open an index and return an ixFileHandle.
        RC openFile(const std::string &fileName, IXFileHandle &ixFileHandle);

        // Close an ixFileHandle for an index.
        RC closeFile(IXFileHandle &ixFileHandle);

        // Insert an entry into the given index that is indicated by the given
        // ixFileHandle.
        RC insertEntry(IXFileHandle &ixFileHandle, const Attribute &attribute,
                       const void *key, const RID &rid);

        // Delete an entry from the given index that is indicated by the given
        // ixFileHandle.
        RC deleteEntry(IXFileHandle &ixFileHandle, const Attribute &attribute,
                       const void *key, const RID &rid);

        // Initialize and IX_ScanIterator to support a range search
        RC scan(IXFileHandle &ixFileHandle, const Attribute &attribute,
                const void *lowKey, const void *highKey, bool lowKeyInclusive,
                bool highKeyInclusive, IX_ScanIterator &ix_ScanIterator);

        // Print the B+ tree in pre-order (in a JSON record format)
        RC printBTree(IXFileHandle &ixFileHandle, const Attribute &attribute,
                      std::ostream &out) const;

        // Compare 2 keys:
        //    a = b -> return = 0
        //    a > b -> return > 0
        //    a < b -> return < 0
        int compareKey(const Attribute &attribute, const void *A, const void *B);

    protected:
        IndexManager() = default;  // Prevent construction
        ~IndexManager() = default; // Prevent unwanted destruction
        IndexManager(const IndexManager &) =
        default; // Prevent construction by copying
        IndexManager &operator=(const IndexManager &) = default; // Prevent assignment

    private:
        // Return Page number of Root Page, creates one if needed.
        PageNum findRoot(IXFileHandle &ixFileHandle);

        // Insert <key,rid> into leaf node
        TreeOp insertLeaf(IXFileHandle &ixfileHandle, const Attribute &attribute,
                          const void *key, const RID &rid, PageNum pageNum,
                          KeyDesc &keyDesc, void *page);

        // Insert <key,rid> into lead node after searching tree
        TreeOp searchInsert(IXFileHandle &ixfileHandle, const Attribute &attribute,
                            const void *key, const RID &rid, PageNum pageNum,
                            KeyDesc &keyDesc, void *page);

        // Get size of inputed key
        unsigned keySize(const Attribute &attribute, const void *key);

        // Find a free page (node)
        PageNum findFree(IXFileHandle &ixfileHandle);

        // Create new page (node), can be leaf or non-leaf
        PageNum newNode(IXFileHandle &ixfileHandle, NodeType type);

        // Update page that points to root node with new pageNum
        RC newRoot(IXFileHandle &ixfileHandle, PageNum pageNum);

        // Prints key of any type
        RC printKey(const Attribute &attribute, const void *key,
                    std::ostream &out) const;

        // Recursively Prints the B Tree
        RC printBTreeNode(IXFileHandle &ixFileHandle, const Attribute &attribute,
                          std::ostream &out, PageNum pageNum) const;

        // get child pagae num
        PageNum checkChild(IXFileHandle &ixFileHandle, const Attribute &attribute,
                           const void *key, PageNum pageNum);
    };
    class IX_ScanIterator {
    public:
        IXFileHandle *ixFileHandle;
        Attribute attribute;
        void *lowKey;
        void *highKey;
        bool lowKeyInclusive;
        bool highKeyInclusive;
        bool lowNull = false;
        bool highNull = false;
        //IndexManager &im;
        PageNum currPage;
        unsigned currKey = -1;
        unsigned currData = -1;
        unsigned currRID = -1;

        // Constructor
        IX_ScanIterator();

        // Destructor
        ~IX_ScanIterator();

        // Get next matching entry
        RC getNextEntry(RID &rid, void *key);

        // Terminate index scan
        RC close();
    };

    class IXFileHandle {
    public:
        // variables to keep counter for each operation
        unsigned ixReadPageCounter;
        unsigned ixWritePageCounter;
        unsigned ixAppendPageCounter;

        // Constructor
        IXFileHandle();

        // Destructor
        ~IXFileHandle();

        // Put the current counter values of associated PF FileHandles into
        // variables
        RC collectCounterValues(unsigned &readPageCount, unsigned &writePageCount,
                                unsigned &appendPageCount);

        /******************************* Helpers ******************************/
        // create pfm funcationality
        RC readPage(PageNum pageNum, void *data);
        RC writePage(PageNum pageNum, const void *data);
        RC appendPage(const void *data);
        unsigned getNumNodes();

        // RC deletePage(PageNum pageNum);

        // Initialize File Handle
        RC initFileHandle(const std::string &fileName);

        // Close File Handle
        RC closeFileHandle();

        // Get root
        PageNum getRoot();

        RC dfs(std::vector<PageNum> &order, PageNum curr);

    private:
        FileHandle fileHandle;
    };
} // namespace PeterDB
#endif // _ix_h_
