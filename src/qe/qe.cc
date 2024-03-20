#include "src/include/qe.h"

namespace PeterDB {

    //RC getAttr(void *inBuffer, void *outBuffer, std::vector<Attribute> recordDescriptor, int attrPos)

    bool compareAttrs(CompOp op, AttrType type, const void *attr, const void *cond) {

        int a = *(int*) attr;
        int b = *(int*) cond;
        float fa = *(float*) attr;
        float fb = *(float*) cond;
        float e = 1e-5;
        int attrLen = *(int*)attr;
        int condLen = *(int*)cond;
        std::string sa((char*)attr + sizeof(int), attrLen);
        std::string sb((char*)cond + sizeof(int), condLen);

        switch (type) {
            case TypeInt:
                switch (op) {

                    case EQ_OP:
                        return a == b;
                        break;
                    case LT_OP:
                        return a < b;
                        break;
                    case LE_OP:
                        return a <= b;
                        break;
                    case GT_OP:
                        return a > b;
                        break;
                    case GE_OP:
                        return a >= b;
                        break;
                    case NE_OP:
                        return a != b;
                        break;
                    case NO_OP:
                        return true;
                        break;
                }
                break;
            case TypeReal:
                switch (op) {
                    case EQ_OP:
                        return fabs(fa - fb) < e;
                        break;
                    case LT_OP:
                        return (fb - fa) > e;
                        break;
                    case LE_OP:
                        return !((fa - fb) > e);
                        break;
                    case GT_OP:
                        return (fa - fb) > e;
                        break;
                    case GE_OP:
                        return !((fb - fa) > e);
                        break;
                    case NE_OP:
                        return !(fabs(fa - fb) < e);
                        break;
                    case NO_OP:
                        return true;
                        break;
                }

                break;
            case TypeVarChar:
                switch (op) {

                    case EQ_OP:
                        return sa == sb;
                        break;
                    case LT_OP:
                        return sa < sb;
                        break;
                    case LE_OP:
                        return sa <= sb;
                        break;
                    case GT_OP:
                        return sa > sb;
                        break;
                    case GE_OP:
                        return sa >= sb;
                        break;
                    case NE_OP:
                        return sa != sb;
                        break;
                    case NO_OP:
                        return true;
                        break;
                }
                break;
        }
    }

    RC makeNullIndicator(const std::vector<Attribute> &recordDescriptor, const std::vector<std::string> attrs, const void *data, void *bytes) {
        int nullByte;
        int nullSize = ceil((double)attrs.size()/8);
        memset(bytes,0,nullSize);

        int offset = 0;
        for (int i = 0; i < recordDescriptor.size(); i++) {
            if (recordDescriptor[i].name == attrs[offset]) {
                nullByte = floor((double)i/8);
                if ((*((char*)data + nullByte) & (128>>(i%8))) != 0) {
                    nullByte = floor((double)offset/8);
                    *((uint8_t*) bytes + nullByte) |= (0b10000000 >> (offset&8));
                }
                offset++;
            }
        }
        return SUCCESS;
    }

    RC joinAttrs(const std::vector<Attribute> &lhs, const std::vector<Attribute> &rhs, std::vector<Attribute> &attrs) {
        for (int i = 0; i < lhs.size(); i++) {
            attrs.push_back(lhs[i]);
        }
        for (int i = 0; i < rhs.size(); i++) {
            attrs.push_back(rhs[i]);
        }
        return SUCCESS;
    }

    RC concatTuples(const std::vector<Attribute> &lhsAttrs, const std::vector<Attribute> &rhsAttrs, const void *lhsTuple, const void *rhsTuple, void *data){};

    Filter::Filter(Iterator *input, const Condition &condition) : iter(input) {
        // just in case
        recordDescriptor.clear();
        //

        //iter = input;
        iter->getAttributes(recordDescriptor);
        this->condition = condition.rhsValue.data;
        type = condition.rhsValue.type;
        op = condition.op;
        attrName = condition.lhsAttr;

        // create space for value
        for(attributePosition = 0; attributePosition < recordDescriptor.size(); attributePosition++) {
            if (recordDescriptor[attributePosition].name == attrName) {
                val = malloc(recordDescriptor[attributePosition].length);
                break;
            }
        }

    }

    Filter::~Filter() {
        free(val);
    }

    RC Filter::getNextTuple(void *data) {
        while(iter->getNextTuple(data) != QE_EOF) {
            rbfm->readAttributeFromRecord(recordDescriptor, attrName, data, val);
            if (compareAttrs(op, type, val, condition)) {
                return SUCCESS;
            }
        }
        return FAILURE;
    }

    RC Filter::getAttributes(std::vector<Attribute> &attrs) const {
        attrs = recordDescriptor;
        return SUCCESS;
    }

    Project::Project(Iterator *input, const std::vector<std::string> &attrNames) : iter(input) {
        iter->getAttributes(recordDescriptor);
        projectAttrs = attrNames;
        val = malloc(PAGE_SIZE);
    }

    Project::~Project() {
        free(val);
    }

    RC Project::getNextTuple(void *data) {
        if (iter->getNextTuple(val) == FAILURE) { return FAILURE; }

        // create null indicator
        makeNullIndicator(recordDescriptor, projectAttrs, val, data);

        int offset = ceil(projectAttrs.size()/8);
        for (int i = 0; i < projectAttrs.size(); i++) {
            //rbfm->readAttributeFromRecord(recordDescriptor, projectAttrs[i], val, (char*)data + offset);
            readAttr(projectAttrs[i], data);
            for (int j = 0; j < recordDescriptor.size(); j++) {
                if (recordDescriptor[j].name == projectAttrs[i]) {
                    switch (recordDescriptor[j].type) {

                        case TypeInt:
                            offset += sizeof(int);
                            break;
                        case TypeReal:
                            offset += sizeof(float);
                            break;
                        case TypeVarChar:
                            offset += sizeof(int) + *(int*)((char*)data + offset);
                            break;
                    }
                }
            }
        }
        return SUCCESS;

    }

    RC Project::readAttr(const std::string &attributeName, void *attributeData) {
        void *temp = malloc(PAGE_SIZE);
        rbfm->readAttributeFromRecord(recordDescriptor, attributeName, val, temp);
        int size;
        for (int j = 0; j < recordDescriptor.size(); j++) {
            if (recordDescriptor[j].name == attributeName) {
                switch (recordDescriptor[j].type) {

                    case TypeInt:
                        size = sizeof(int);
                        break;
                    case TypeReal:
                        size = sizeof(float);
                        break;
                    case TypeVarChar:
                        size = sizeof(int) + *(int *) ((char *) temp);
                        break;
                }
            }
        }
        memcpy(attributeData, (char*)temp + sizeof(char), size);
        return SUCCESS;
    }

    RC Project::getAttributes(std::vector<Attribute> &attrs) const {
        attrs = recordDescriptor;
        return SUCCESS;
    }

    BNLJoin::BNLJoin(Iterator *leftIn, TableScan *rightIn, const Condition &condition, const unsigned int numPages) : lhsIter(leftIn), rhsIter(rightIn) {
        this->condition = condition;
        this-> numPages = numPages;

        leftIn->getAttributes(lhsAttrs);
        rightIn->getAttributes(rhsAttrs);

        lhsTuple = malloc(PAGE_SIZE);
        rhsTuple = malloc(PAGE_SIZE);

        first = true;
    }

    BNLJoin::~BNLJoin() {
        pages.clear();
        dups->clear();
        free(lhsTuple);
        free(rhsTuple);
    }

    RC BNLJoin::getNextTuple(void *data) {
        if (first == true && (rhsIter->getNextTuple(rhsTuple) == QE_EOF || loadLeftBlock() == QE_EOF)) {
            return QE_EOF;
        }

        if (pages.size() == 0) { return FAILURE; }

        bool cont = true;
        int lit = 0;
        do {
            if (lit == pages.size()) {
                if (rhsIter->getNextTuple(rhsTuple) == QE_EOF) {
                    if (loadLeftBlock() == QE_EOF) {
                        return FAILURE;
                    } else {
                        rhsIter->setIterator();
                        rhsIter->getNextTuple(rhsTuple);
                    }
                }
                lit = 0;
            }
            lit++;
            lhsTuple = pages[lit];
        } while (cont);
    }

    RC BNLJoin::getAttributes(std::vector<Attribute> &attrs) const {
        return joinAttrs(lhsAttrs, rhsAttrs, attrs);
    }

    RC BNLJoin::loadLeftBlock() {
        pages.clear();
        memset(lhsTuple, 0, PAGE_SIZE);

        if (lhsIter->getNextTuple(lhsTuple) == QE_EOF) {
            free(lhsTuple);
            return QE_EOF;
        }

        for (int i = 0; i < numPages; i++) {
            void *page = malloc(PAGE_SIZE);
            lhsIter->getNextTuple(page);
            pages.push_back(page);
        }
    }

    INLJoin::INLJoin(Iterator *leftIn, IndexScan *rightIn, const Condition &condition) : lhsIter(leftIn), rhsIter(rightIn) {
        this->condition = condition;

        leftIn->getAttributes(lhsAttrs);
        rightIn->getAttributes(rhsAttrs);

        lhsTuple = malloc(PAGE_SIZE);
        rhsTuple = malloc(PAGE_SIZE);

        first = true;
    }

    INLJoin::~INLJoin() {
        free(lhsTuple);
        free(rhsTuple);
    }

    RC INLJoin::getNextTuple(void *data) {
        if (rhsIter->getNextTuple(rhsTuple) != QE_EOF) {
            // return tuple
            concatTuples(lhsAttrs, rhsAttrs, lhsTuple, rhsTuple, data);
            return SUCCESS;
        }
        do {
            if (rhsIter->getNextTuple(rhsTuple) == QE_EOF) { return QE_EOF; }

            //setScan(lhsTuple);
        } while (lhsIter->getNextTuple(lhsTuple) != QE_EOF);
        concatTuples(lhsAttrs, rhsAttrs, lhsTuple, rhsTuple, data);
    }

    RC INLJoin::getAttributes(std::vector<Attribute> &attrs) const {
        return joinAttrs(lhsAttrs, rhsAttrs, attrs);
    }

    GHJoin::GHJoin(Iterator *leftIn, Iterator *rightIn, const Condition &condition, const unsigned int numPartitions) {

    }

    GHJoin::~GHJoin() {

    }

    RC GHJoin::getNextTuple(void *data) {
        return -1;
    }

    RC GHJoin::getAttributes(std::vector<Attribute> &attrs) const {
        return -1;
    }

    Aggregate::Aggregate(Iterator *input, const Attribute &aggAttr, AggregateOp op) : iter(input) {
        this->aggAttr = aggAttr;
        this->op = op;
        input->getAttributes(recordDescriptor);
        found = false;

        // get attribute position
        for (attrPos = 0; attrPos < recordDescriptor.size(); attrPos++) {
            if (recordDescriptor[attrPos].name == aggAttr.name) {
                break;
            }
        }
    }

    Aggregate::Aggregate(Iterator *input, const Attribute &aggAttr, const Attribute &groupAttr, AggregateOp op) {

    }

    Aggregate::~Aggregate() {

    }

    RC Aggregate::getNextTuple(void *data) {
        // check if already scanned
        if (found) {
            return QE_EOF;
        }

        switch (op) {

            case MIN:
                return getMIN(data);
                break;
            case MAX:
                return getMAX(data);
                break;
            case COUNT:
                return getCOUNT(data);
                break;
            case SUM:
                return getSUM(data);
                break;
            case AVG:
                return getAVG(data);
                break;
        }
        return QE_EOF;
    }

    RC Aggregate::getMIN(void *data) {
        void *page = malloc(PAGE_SIZE);
        void *currMIN = malloc(sizeof(float));

        float tempMax = POS_INF;
        memcpy(currMIN, &tempMax, sizeof(float));

        // get next on all the data
        while (iter->getNextTuple(page) != QE_EOF) {
            // set pointer to attribute we are reading
            char *attrPtr = (char*)page;
            int offset = 0;

            while (offset < attrPos) {
                // increment pointer based on field types
                if (recordDescriptor[offset].type == TypeVarChar) {
                    // varchar
                    int length;
                    memcpy(&length, attrPtr, sizeof(int));
                    attrPtr += length + sizeof(int);
                } else {
                    // int or float
                    attrPtr += sizeof(int);
                }
                offset++;
            }

            // compare the field
            float curr;
            float check;
            memcpy(&curr, currMIN, sizeof(float));
            memcpy(&check, attrPtr, sizeof(float));

            if (check < curr) {
                memcpy(currMIN, attrPtr, sizeof(float));
            }
        }

        // compy currMin to data, set found
        memcpy(data, currMIN, sizeof(float));
        found = true;

        free(page);
        free(currMIN);
        return SUCCESS;
    }

    RC Aggregate::getMAX(void *data) {
        void *page = malloc(PAGE_SIZE);
        void *currMAX = malloc(sizeof(float));

        float tempMin = NEG_INF;
        memcpy(currMAX, &tempMin, sizeof(float));

        // get next on all the data
        while (iter->getNextTuple(page) != QE_EOF) {
            // set pointer to attribute we are reading
            char *attrPtr = (char*)page;
            int offset = 0;

            while (offset < attrPos) {
                // increment pointer based on field types
                if (recordDescriptor[offset].type == TypeVarChar) {
                    // varchar
                    int length;
                    memcpy(&length, attrPtr, sizeof(int));
                    attrPtr += length + sizeof(int);
                } else {
                    // int or float
                    attrPtr += sizeof(int);
                }
                offset++;
            }

            // compare the field
            float curr;
            float check;
            memcpy(&curr, currMAX, sizeof(float));
            memcpy(&check, attrPtr, sizeof(float));

            if (check > curr) {
                memcpy(currMAX, attrPtr, sizeof(float));
            }
        }

        // compy currMax to data, set found
        memcpy(data, currMAX, sizeof(float));
        found = true;

        free(page);
        free(currMAX);
        return SUCCESS;
    }

    RC Aggregate::getCOUNT(void *data) {
        // scan all values and increment counter
        void *page = malloc(PAGE_SIZE);
        float count = 0;
        while (iter->getNextTuple(page) != QE_EOF) {
            count++;
        }

        memcpy(data, &count, sizeof(float));
        found = true;

        free(page);
        return SUCCESS;
    }

    RC Aggregate::getSUM(void *data) {
        void *page = malloc(PAGE_SIZE);
        void *currSUM = malloc(sizeof(float));

        float tempSUM = 0.0;
        memcpy(currSUM, &tempSUM, sizeof(float));

        // get next on all the data
        while (iter->getNextTuple(page) != QE_EOF) {
            // set pointer to attribute we are reading
            char *attrPtr = (char*)page;
            int offset = 0;

            while (offset < attrPos) {
                // increment pointer based on field types
                if (recordDescriptor[offset].type == TypeVarChar) {
                    // varchar
                    int length;
                    memcpy(&length, attrPtr, sizeof(int));
                    attrPtr += length + sizeof(int);
                } else {
                    // int or float
                    attrPtr += sizeof(int);
                }
                offset++;
            }

            // add to sum
            *(float*)currSUM += *(float*)attrPtr;
        }

        // compy currSum to data, set found
        memcpy(data, currSUM, sizeof(float));
        found = true;

        free(page);
        free(currSUM);
        return SUCCESS;
    }

    RC Aggregate::getAVG(void *data) {
        void *page = malloc(PAGE_SIZE);
        void *currSUM = malloc(sizeof(float));

        float tempSUM = 0.0;
        memcpy(currSUM, &tempSUM, sizeof(float));

        float count = 0.0;
        // get next on all the data
        while (iter->getNextTuple(page) != QE_EOF) {
            // set pointer to attribute we are reading
            char *attrPtr = (char*)page;
            int offset = 0;

            while (offset < attrPos) {
                // increment pointer based on field types
                if (recordDescriptor[offset].type == TypeVarChar) {
                    // varchar
                    int length;
                    memcpy(&length, attrPtr, sizeof(int));
                    attrPtr += length + sizeof(int);
                } else {
                    // int or float
                    attrPtr += sizeof(int);
                }
                offset++;
            }

            // add to sum
            *(float*)currSUM += *(float*)attrPtr;

            count++;
        }

        // calc average using count and currSum
        float average = (*(float*)currSUM) / count;
        memcpy(data, &average, sizeof(float));

        found = true;

        free(page);
        free(currSUM);
        return SUCCESS;
    }

    RC Aggregate::getAttributes(std::vector<Attribute> &attrs) const {
        std::string attrName = recordDescriptor[attrPos].name;
        Attribute returnAttr;

        switch (op) {

            case MIN:
                returnAttr.name = "MIN(" + attrName + ")";
                break;
            case MAX:
                returnAttr.name = "MAX(" + attrName + ")";
                break;
            case COUNT:
                returnAttr.name = "COUNT(" + attrName + ")";
                break;
            case SUM:
                returnAttr.name = "SUN(" + attrName + ")";
                break;
            case AVG:
                returnAttr.name = "AVG(" + attrName + ")";
                break;
        }
        attrs.push_back(returnAttr);
        return SUCCESS;
    }
} // namespace PeterDB
