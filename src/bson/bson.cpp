#include <string>
#include "bsonobjbuilder.h"
#include "bsonobjiterator.h"

using namespace std;

#define q(x)

namespace _bson {

    namespace iter {
        // values herein represent size of the element of the corresponding type.
        // if >= 0x80, size is in the next dword + the value herein & 0x7f
        // 0xff = fall back to full code in BSONElement::_size()
        unsigned char sizeForBsonType[] = {
            0, // EOO
            8,  // double
            0x84,  // string
            0x80,  // object
            0x80,  // array
            0x85, // bindata - count the extra subtype byte
            0,  // undefined
            12, // oid
            1,  // bool
            8,  // date
            0,  // null
            -1,  // regex,
            -1,  // dbref,
            -1,  // code,
            -1,  // symbol,
            -1,  // codewscope,
            4,  // int,
            8,  // timestamp,
            8,  // long = 18
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // 32
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, //64
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,       //126
            0, // maxkey=127
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0 // minkey=-1
        };

        // helper function to skip over the BSON element's value and thus find the next 
        // element's start.
        const char * skipValue(const char *elem, const char *valueStart) {
            const char *p = valueStart;
            unsigned char type = (unsigned char)*elem;
            unsigned z = sizeForBsonType[type];
            q(log() << "skipping ahead z:" << z << " type:" << (unsigned)type << endl;)
            if (z < 0x80) {
                p += z;
                q(log() << "simple advance of " << z << endl;);
                q(log() << "next will be:" << (p  1) << '\n' << endl;);
            }
            else {
                if (z == 0xff) {
                    q(log() << "backcompat" << endl;);
                    bsonelement e(elem);
                    p += e.size();
                }
                else {
                    int len = *((int *)p);
                    int extra = (z & 0x7f);
                    len = extra;
                    q(log() << "will skip:" << len << " extra was:" << extra << endl;)
                    p += len;
                }
                q(if (*p) log() << "next will be:" << (p  1) << '\n' << endl;)
            }
            return p;
        }
    }

    int bsonelement::_valuesize() const { 
       // const char *p = valueStart;
        unsigned char tp = (unsigned char)type();
        unsigned z = iter::sizeForBsonType[tp ];
        unsigned sz = 0;
        if (z < 0x80) {
            sz += z;
        }
        else {
            if (z == 0xff) {
                q(log() << "backcompat" << endl;);
                sz += sizeOld() - fieldNameSize() - 1;
            }
            else {
                int len = *((int *)value());
                int extra = (z & 0x7f);
                len += extra;
                sz += len;
            }
        }
        return sz;
    }

    inline int bsonelement::size() const {
        if (totalSize >= 0)
            return totalSize;
        int x = _valuesize();
        totalSize = x + fieldNameSize() + 1;
        return totalSize;
    }

    inline int bsonelement::sizeOld() const {
        if (totalSize >= 0)
            return totalSize;

        int x = 0;
        switch (type()) {
        case EOO:
        case Undefined:
        case jstNULL:
        case MaxKey:
        case MinKey:
            break;
        case _bson::Bool:
            x = 1;
            break;
        case NumberInt:
            x = 4;
            break;
        case Timestamp:
        case _bson::Date:
        case NumberDouble:
        case NumberLong:
            x = 8;
            break;
        case jstOID:
            x = 12;
            break;
        case Symbol:
        case Code:
        case _bson::String:
            x = valuestrsize() + 4;
            break;
        case DBRef:
            x = valuestrsize() + 4 + 12;
            break;
        case CodeWScope:
        case Object:
        case _bson::Array:
            x = objsize();
            break;
        case BinData:
            x = valuestrsize() + 4 + 1/*subtype*/;
            break;
        case RegEx:
        {
            const char *p = value();
            size_t len1 = strlen(p);
            p = p + len1 + 1;
            size_t len2;
            len2 = strlen(p);
            x = (int)(len1 + 1 + len2 + 1);
        }
            break;
        default:
        {
            StringBuilder ss;
            ss << "bsonelement: bad type " << (int)type();
            std::string msg = ss.str();
            massert(10320, msg.c_str(), false);
        }
        }
        totalSize = x + fieldNameSize() + 1; // BSONType

        return totalSize;
    }

    bsonelement bsonobj::getField(const StringData& name) const {
        const char *_name = name.begin();
        unsigned name_sz = (unsigned)name.size();
        q(log() << _name << ' ' << name_sz << endl;);
        const char *p = objdata();
        int sz = objsize();
        q(log() << sz << endl);
        const char *end = p + sz - 1;
        p += 4;
        while (1) {
            const char *elem = p;
            const char *nul = ++p + name_sz;
            q(log() << "type " << (int)*elem << endl;)
                if (nul >= end) {
                q(log() << "end" << endl;)
                    break;

                }
            if (*nul == 0) {
                // potential match
                q(log() << "memcmp check " << *p << endl;)
                if (memcmp(_name, p, name_sz) == 0) {
                    q(log() << "match!" << endl;)
                    bsonelement e = bsonelement(name_sz + 1, elem);
                    q(log() << e.toString() << endl;)
                    return e;
                }
            }
            // else, mismatch.  skip to next
            q(log() << "skip ahead:" << p << endl;)
                while (1) {
                if ((*p++ == 0))   // both the loop unwinding here, and the use of 
                    break;                          // unlikely(), made a measurable difference in a 
                if ((*p++ == 0))   // quick ad hoc test (gcc 4.2.1)
                    break;

                }
            // skip the item's data
            p = iter::skipValue(elem, p);
            if (*p == 0) {
                q(log() << "*p==EOO" << endl;)
                break;
            }

        }
        q(log() << "returning BSONElement() (mismatch)" << endl;)
        return bsonelement();
    }


    const string bsonobjbuilder::numStrs[] = {
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
        "10", "11", "12", "13", "14", "15", "16", "17", "18", "19",
        "20", "21", "22", "23", "24", "25", "26", "27", "28", "29",
        "30", "31", "32", "33", "34", "35", "36", "37", "38", "39",
        "40", "41", "42", "43", "44", "45", "46", "47", "48", "49",
        "50", "51", "52", "53", "54", "55", "56", "57", "58", "59",
        "60", "61", "62", "63", "64", "65", "66", "67", "68", "69",
        "70", "71", "72", "73", "74", "75", "76", "77", "78", "79",
        "80", "81", "82", "83", "84", "85", "86", "87", "88", "89",
        "90", "91", "92", "93", "94", "95", "96", "97", "98", "99",
    };

    std::string OID::str() const { return toHexLower(data, kOIDSize); }

    // This is to ensure that bsonobjbuilder doesn't try to use numStrs before the strings have been constructed
    // I've tested just making numStrs a char[][], but the overhead of constructing the strings each time was too high
    // numStrsReady will be 0 until after numStrs is initialized because it is a static variable
    bool bsonobjbuilder::numStrsReady = (numStrs[0].size() > 0);

    // wrap this element up as a singleton object.
    bsonobj bsonelement::wrap() const {
        bsonobjbuilder b(size() + 6);
        b.append(*this);
        return b.obj();
    }

    bsonobj bsonelement::wrap(const StringData& newName) const {
        bsonobjbuilder b(size() + 6 + newName.size());
        b.appendAs(*this, newName);
        return b.obj();
    }
    /* add all the fields from the object specified to this object */
    bsonobjbuilder& bsonobjbuilder::appendElements(bsonobj x) {
        if (!x.isEmpty())
            _b.appendBuf(
            x.objdata() + 4,   // skip over leading length
            x.objsize() - 5);  // ignore leading length and trailing \0
        return *this;
    }

    std::string bsonelement::toString(bool includeFieldName, bool full) const {
        StringBuilder s;
        toString(s, includeFieldName, full);
        return s.str();
    }
    void bsonelement::toString(StringBuilder& s, bool includeFieldName, bool full, int depth) const {

        if (depth > bsonobj::maxToStringRecursionDepth) {
            // check if we want the full/complete string
            if (full) {
                StringBuilder s;
                s << "Reached maximum recursion depth of ";
                s << bsonobj::maxToStringRecursionDepth;
                uassert(16150, s.str(), full != true);
            }
            s << "...";
            return;
        }

        if (includeFieldName && type() != EOO)
            s << fieldName() << ": ";
        switch (type()) {
        case EOO:
            s << "EOO";
            break;
        case _bson::Date:
            s << "new Date(" << (long long)date() << ')';
            break;
        case RegEx: {
            s << "/" << regex() << '/';
            const char *p = regexFlags();
            if (p) s << p;
        }
            break;
        case NumberDouble:
            s.appendDoubleNice(number());
            break;
        case NumberLong:
            s << _numberLong();
            break;
        case NumberInt:
            s << _numberInt();
            break;
        case _bson::Bool:
            s << (boolean() ? "true" : "false");
            break;
        case Object:
            embeddedObject().toString(s, false, full, depth + 1);
            break;
        case _bson::Array:
            embeddedObject().toString(s, true, full, depth + 1);
            break;
        case Undefined:
            s << "undefined";
            break;
        case jstNULL:
            s << "null";
            break;
        case MaxKey:
            s << "MaxKey";
            break;
        case MinKey:
            s << "MinKey";
            break;
        case CodeWScope:
            s << "CodeWScope( "
                << codeWScopeCode() << ", " << codeWScopeObject().toString(false, full) << ")";
            break;
        case Code:
            if (!full &&  valuestrsize() > 80) {
                s.write(valuestr(), 70);
                s << "...";
            }
            else {
                s.write(valuestr(), valuestrsize() - 1);
            }
            break;
        case Symbol:
        case _bson::String:
            s << '"';
            if (!full &&  valuestrsize() > 160) {
                s.write(valuestr(), 150);
                s << "...\"";
            }
            else {
                s.write(valuestr(), valuestrsize() - 1);
                s << '"';
            }
            break;
        case DBRef:
            s << "DBRef('" << valuestr() << "',";
            {
                _bson::OID *x = (_bson::OID *) (valuestr() + valuestrsize());
                s << x->toString() << ')';
            }
            break;
        case jstOID:
            s << "ObjectId('";
            s << __oid().toString() << "')";
            break;
        case BinData:
            s << "BinData(" << binDataType() << ", ";
            {
                int len;
                const char *data = binDataClean(len);
                if (!full && len > 80) {
                    s << toHex(data, 70) << "...)";
                }
                else {
                    s << toHex(data, len) << ")";
                }
            }
            break;
        case Timestamp:
            s << "Timestamp " << timestampTime() << "|" << timestampInc();
            break;
        default:
            s << "?type=" << type();
            break;
        }
    }

    /* return has eoo() true if no match
    supports "." notation to reach into embedded objects
    */
    bsonelement bsonobj::getFieldDotted(const StringData& name) const {
        bsonelement e = getField(name);
        if (e.eoo()) {
            size_t dot_offset = name.find('.');
            if (dot_offset != std::string::npos) {
                StringData left = name.substr(0, dot_offset);
                StringData right = name.substr(dot_offset + 1);
                bsonobj sub = getObjectField(left);
                return sub.isEmpty() ? bsonelement() : sub.getFieldDotted(right);
            }
        }

        return e;
    }

    void bsonobj::toString(StringBuilder& s, bool isArray, bool full, int depth) const {
        if (isEmpty()) {
            s << (isArray ? "[]" : "{}");
            return;
        }

        s << (isArray ? "[ " : "{ ");
        bsonobjiterator i(*this);
        bool first = true;
        while (1) {
            massert(10327, "Object does not end with EOO", i.moreWithEOO());
            bsonelement e = i.next(true);
            massert(10328, "Invalid element size", e.size() > 0);
            massert(10329, "Element too large", e.size() < (1 << 30));
            int offset = (int)(e.rawdata() - this->objdata());
            massert(10330, "Element extends past end of object",
                e.size() + offset <= this->objsize());
            bool end = (e.size() + offset == this->objsize());
            if (e.eoo()) {
                massert(10331, "EOO Before end of object", end);
                break;
            }
            if (first)
                first = false;
            else
                s << ", ";
            e.toString(s, !isArray, full, depth);
        }
        s << (isArray ? " ]" : " }");
    }
    bsonobj bsonobj::getObjectField(const StringData& name) const {
        bsonelement e = getField(name);
        BSONType t = e.type();
        return t == Object || t == Array ? e.embeddedObject() : bsonobj();
    }
    bsonobj::bsonobj() {
        static char p[] = { /*size*/5, 0, 0, 0, /*eoo*/0 };
        _objdata = p;
    }
    /*bsonelement bsonobj::getField(const StringData& name) const {
        bsonobjiterator i(*this);
        while (i.more()) {
            bsonelement e = i.next();
            if (name == e.fieldName())
                return e;
        }
        return bsonelement();
    }*/
}
