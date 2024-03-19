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
            rbfm->readAttributeFromRecord(recordDescriptor, projectAttrs[i], val, (char*)data + offset);

        }

    }

    RC Project::getAttributes(std::vector<Attribute> &attrs) const {
        attrs = recordDescriptor;
        return SUCCESS;
    }

    BNLJoin::BNLJoin(Iterator *leftIn, TableScan *rightIn, const Condition &condition, const unsigned int numPages) {

    }

    BNLJoin::~BNLJoin() {

    }

    RC BNLJoin::getNextTuple(void *data) {
        return -1;
    }

    RC BNLJoin::getAttributes(std::vector<Attribute> &attrs) const {
        return -1;
    }

    INLJoin::INLJoin(Iterator *leftIn, IndexScan *rightIn, const Condition &condition) {

    }

    INLJoin::~INLJoin() {

    }

    RC INLJoin::getNextTuple(void *data) {
        return -1;
    }

    RC INLJoin::getAttributes(std::vector<Attribute> &attrs) const {
        return -1;
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

    Aggregate::Aggregate(Iterator *input, const Attribute &aggAttr, AggregateOp op) {

    }

    Aggregate::Aggregate(Iterator *input, const Attribute &aggAttr, const Attribute &groupAttr, AggregateOp op) {

    }

    Aggregate::~Aggregate() {

    }

    RC Aggregate::getNextTuple(void *data) {
        return -1;
    }

    RC Aggregate::getAttributes(std::vector<Attribute> &attrs) const {
        return -1;
    }
} // namespace PeterDB
