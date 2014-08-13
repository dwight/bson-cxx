/*    Copyright 2009 10gen Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */


#include <limits>
#include <cmath>
//#include <boost/lexical_cast.hpp>
//#include <boost/static_assert.hpp>
//#include "bson_validate.h"
#include "oid.h"
//#include "optime.h"
#include "float_utils.h"
#include "base64.h"
//#include "embedded_builder.h"
//#include "md5.hpp"
//#include "str.h"
//#include "stringutils.h"
#include "time_support.h"
#include <set>
#include <vector>
#include "bsonelement.h"
#include "bsonobjiterator.h"
#include "bsonobjbuilder.h"
#include "parse_number.h"

namespace _bson {

    using std::dec;
    using std::endl;
    using std::hex;
    using std::numeric_limits;
    using std::set;
    using std::string;
    using std::stringstream;

    bsonelement eooElement;
#if 0
    // need to move to bson/, but has dependency on base64 so move that to bson/util/ first.
    inline string bsonelement::jsonString( JsonStringFormat format, bool includeFieldNames, int pretty ) const {
        int sign;

        stringstream s;
        if ( includeFieldNames )
            s << '"' << escape( fieldName() ) << "\" : ";
        switch ( type() ) {
        case mongo::String:
        case Symbol:
            s << '"' << escape( string(valuestr(), valuestrsize()-1) ) << '"';
            break;
        case NumberLong:
            if (format == TenGen) {
                s << "NumberLong(" << _numberLong() << ")";
            }
            else {
                s << "{ \"$numberLong\" : \"" << _numberLong() << "\" }";
            }
            break;
        case NumberInt:
            if(format == JS) {
                s << "NumberInt(" << _numberInt() << ")";
                break;
            }
        case NumberDouble:
            if ( number() >= -numeric_limits< double >::max() &&
                    number() <= numeric_limits< double >::max() ) {
                s.precision( 16 );
                s << number();
            }
            // This is not valid JSON, but according to RFC-4627, "Numeric values that cannot be
            // represented as sequences of digits (such as Infinity and NaN) are not permitted." so
            // we are accepting the fact that if we have such values we cannot output valid JSON.
            else if ( mongo::isNaN(number()) ) {
                s << "NaN";
            }
            else if ( mongo::isInf(number(), &sign) ) {
                s << ( sign == 1 ? "Infinity" : "-Infinity");
            }
            else {
                StringBuilder ss;
                ss << "Number " << number() << " cannot be represented in JSON";
                string message = ss.str();
                massert( 10311 ,  message.c_str(), false );
            }
            break;
        case mongo::Bool:
            s << ( boolean() ? "true" : "false" );
            break;
        case jstNULL:
            s << "null";
            break;
        case Undefined:
            if ( format == Strict ) {
                s << "{ \"$undefined\" : true }";
            }
            else {
                s << "undefined";
            }
            break;
        case Object:
            s << embeddedObject().jsonString( format, pretty );
            break;
        case mongo::Array: {
            if ( embeddedObject().isEmpty() ) {
                s << "[]";
                break;
            }
            s << "[ ";
            BSONObjIterator i( embeddedObject() );
            bsonelement e = i.next();
            if ( !e.eoo() ) {
                int count = 0;
                while ( 1 ) {
                    if( pretty ) {
                        s << '\n';
                        for( int x = 0; x < pretty; x++ )
                            s << "  ";
                    }

                    if (strtol(e.fieldName(), 0, 10) > count) {
                        s << "undefined";
                    }
                    else {
                        s << e.jsonString( format, false, pretty?pretty+1:0 );
                        e = i.next();
                    }
                    count++;
                    if ( e.eoo() )
                        break;
                    s << ", ";
                }
            }
            s << " ]";
            break;
        }
        case DBRef: {
            mongo::OID *x = (mongo::OID *) (valuestr() + valuestrsize());
            if ( format == TenGen )
                s << "Dbref( ";
            else
                s << "{ \"$ref\" : ";
            s << '"' << valuestr() << "\", ";
            if ( format != TenGen )
                s << "\"$id\" : ";
            s << '"' << *x << "\" ";
            if ( format == TenGen )
                s << ')';
            else
                s << '}';
            break;
        }
        case jstOID:
            if ( format == TenGen ) {
                s << "ObjectId( ";
            }
            else {
                s << "{ \"$oid\" : ";
            }
            s << '"' << __oid() << '"';
            if ( format == TenGen ) {
                s << " )";
            }
            else {
                s << " }";
            }
            break;
        case BinData: {
            const int len = *( reinterpret_cast<const int*>( value() ) );
            BinDataType type = BinDataType( *( reinterpret_cast<const unsigned char*>( value() ) +
                                               sizeof( int ) ) );
            s << "{ \"$binary\" : \"";
            const char *start = reinterpret_cast<const char*>( value() ) + sizeof( int ) + 1;
            base64::encode( s , start , len );
            s << "\", \"$type\" : \"" << hex;
            s.width( 2 );
            s.fill( '0' );
            s << type << dec;
            s << "\" }";
            break;
        }
        case mongo::Date:
            if (format == Strict) {
                Date_t d = date();
                s << "{ \"$date\" : ";
                // The two cases in which we cannot convert Date_t::millis to an ISO Date string are
                // when the date is too large to format (SERVER-13760), and when the date is before
                // the epoch (SERVER-11273).  Since Date_t internally stores millis as an unsigned
                // long long, despite the fact that it is logically signed (SERVER-8573), this check
                // handles both the case where Date_t::millis is too large, and the case where
                // Date_t::millis is negative (before the epoch).
                if (d.isFormatable()) {
                    s << "\"" << dateToISOStringLocal(date()) << "\"";
                }
                else {
                    s << "{ \"$numberLong\" : \"" << static_cast<long long>(d.millis) << "\" }";
                }
                s << " }";
            }
            else {
                s << "Date( ";
                if (pretty) {
                    Date_t d = date();
                    // The two cases in which we cannot convert Date_t::millis to an ISO Date string
                    // are when the date is too large to format (SERVER-13760), and when the date is
                    // before the epoch (SERVER-11273).  Since Date_t internally stores millis as an
                    // unsigned long long, despite the fact that it is logically signed
                    // (SERVER-8573), this check handles both the case where Date_t::millis is too
                    // large, and the case where Date_t::millis is negative (before the epoch).
                    if (d.isFormatable()) {
                        s << "\"" << dateToISOStringLocal(date()) << "\"";
                    }
                    else {
                        // FIXME: This is not parseable by the shell, since it may not fit in a
                        // float
                        s << d.millis;
                    }
                }
                else {
                    s << date().asInt64();
                }
                s << " )";
            }
            break;
        case RegEx:
            if ( format == Strict ) {
                s << "{ \"$regex\" : \"" << escape( regex() );
                s << "\", \"$options\" : \"" << regexFlags() << "\" }";
            }
            else {
                s << "/" << escape( regex() , true ) << "/";
                // FIXME Worry about alpha order?
                for ( const char *f = regexFlags(); *f; ++f ) {
                    switch ( *f ) {
                    case 'g':
                    case 'i':
                    case 'm':
                        s << *f;
                    default:
                        break;
                    }
                }
            }
            break;

        case CodeWScope: {
            BSONObj scope = codeWScopeObject();
            if ( ! scope.isEmpty() ) {
                s << "{ \"$code\" : \"" << escape(_asCode()) << "\" , "
                  << "\"$scope\" : " << scope.jsonString() << " }";
                break;
            }
        }

        case Code:
            s << "\"" << escape(_asCode()) << "\"";
            break;

        case Timestamp:
            if ( format == TenGen ) {
                s << "Timestamp( " << ( timestampTime() / 1000 ) << ", " << timestampInc() << " )";
            }
            else {
                s << "{ \"$timestamp\" : { \"t\" : " << ( timestampTime() / 1000 ) << ", \"i\" : " << timestampInc() << " } }";
            }
            break;

        case MinKey:
            s << "{ \"$minKey\" : 1 }";
            break;

        case MaxKey:
            s << "{ \"$maxKey\" : 1 }";
            break;

        default:
            StringBuilder ss;
            ss << "Cannot create a properly formatted JSON string with "
               << "element: " << toString() << " of type: " << type();
            string message = ss.str();
            massert( 10312 ,  message.c_str(), false );
        }
        return s.str();
    }

#endif

    /** transform a BSON array into a vector of BSONElements.
        we match array # positions with their vector position, and ignore
        any fields with non-numeric field names.
        */
    std::vector<bsonelement> bsonelement::Array() const {
        chk(_bson::Array);
        std::vector<bsonelement> v;
        bsonobjiterator i(Obj());
        while( i.more() ) {
            bsonelement e = i.next();
            const char *f = e.fieldName();

            unsigned u;
            Status status = _bson::parseNumberFromString( f, &u );
            if ( status.isOK() ) {
                verify( u < 1000000 );
                if( u >= v.size() )
                    v.resize(u+1);
                v[u] = e;
            }
            else {
                // ignore?
            }
        }
        return v;
    }



    /* Matcher --------------------------------------*/

    /* BSONObj ------------------------------------------------------------*/
#if 0
    string BSONObj::md5() const {
        md5digest d;
        md5_state_t st;
        md5_init(&st);
        md5_append( &st , (const md5_byte_t*)_objdata , objsize() );
        md5_finish(&st, d);
        return digestToString( d );
    }

    string BSONObj::jsonString( JsonStringFormat format, int pretty ) const {

        if ( isEmpty() ) return "{}";

        StringBuilder s;
        s << "{ ";
        BSONObjIterator i(*this);
        bsonelement e = i.next();
        if ( !e.eoo() )
            while ( 1 ) {
                s << e.jsonString( format, true, pretty?pretty+1:0 );
                e = i.next();
                if ( e.eoo() )
                    break;
                s << ",";
                if ( pretty ) {
                    s << '\n';
                    for( int x = 0; x < pretty; x++ )
                        s << "  ";
                }
                else {
                    s << " ";
                }
            }
        s << " }";
        return s.str();
    }

    bool bsonobj::valid() const {
        return validateBSON( objdata(), objsize() ).isOK();
    }

    int BSONObj::woCompare(const BSONObj& r, const Ordering &o, bool considerFieldName) const {
        if ( isEmpty() )
            return r.isEmpty() ? 0 : -1;
        if ( r.isEmpty() )
            return 1;

        BSONObjIterator i(*this);
        BSONObjIterator j(r);
        unsigned mask = 1;
        while ( 1 ) {
            // so far, equal...

            bsonelement l = i.next();
            bsonelement r = j.next();
            if ( l.eoo() )
                return r.eoo() ? 0 : -1;
            if ( r.eoo() )
                return 1;

            int x;
            {
                x = l.woCompare( r, considerFieldName );
                if( o.descending(mask) )
                    x = -x;
            }
            if ( x != 0 )
                return x;
            mask <<= 1;
        }
        return -1;
    }

    /* well ordered compare */
    int BSONObj::woCompare(const BSONObj &r, const BSONObj &idxKey,
                           bool considerFieldName) const {
        if ( isEmpty() )
            return r.isEmpty() ? 0 : -1;
        if ( r.isEmpty() )
            return 1;

        bool ordered = !idxKey.isEmpty();

        BSONObjIterator i(*this);
        BSONObjIterator j(r);
        BSONObjIterator k(idxKey);
        while ( 1 ) {
            // so far, equal...

            bsonelement l = i.next();
            bsonelement r = j.next();
            bsonelement o;
            if ( ordered )
                o = k.next();
            if ( l.eoo() )
                return r.eoo() ? 0 : -1;
            if ( r.eoo() )
                return 1;

            int x;
            /*
                        if( ordered && o.type() == String && strcmp(o.valuestr(), "ascii-proto") == 0 &&
                            l.type() == String && r.type() == String ) {
                            // note: no negative support yet, as this is just sort of a POC
                            x = _stricmp(l.valuestr(), r.valuestr());
                        }
                        else*/ {
                x = l.woCompare( r, considerFieldName );
                if ( ordered && o.number() < 0 )
                    x = -x;
            }
            if ( x != 0 )
                return x;
        }
        return -1;
    }

    /* well ordered compare */
    int BSONObj::woSortOrder(const BSONObj& other, const BSONObj& sortKey , bool useDotted ) const {
        if ( isEmpty() )
            return other.isEmpty() ? 0 : -1;
        if ( other.isEmpty() )
            return 1;

        uassert( 10060 ,  "woSortOrder needs a non-empty sortKey" , ! sortKey.isEmpty() );

        BSONObjIterator i(sortKey);
        while ( 1 ) {
            bsonelement f = i.next();
            if ( f.eoo() )
                return 0;

            bsonelement l = useDotted ? getFieldDotted( f.fieldName() ) : getField( f.fieldName() );
            if ( l.eoo() )
                l = staticNull.firstElement();
            bsonelement r = useDotted ? other.getFieldDotted( f.fieldName() ) : other.getField( f.fieldName() );
            if ( r.eoo() )
                r = staticNull.firstElement();

            int x = l.woCompare( r, false );
            if ( f.number() < 0 )
                x = -x;
            if ( x != 0 )
                return x;
        }
        return -1;
    }

    bool BSONObj::isPrefixOf( const BSONObj& otherObj ) const {
        BSONObjIterator a( *this );
        BSONObjIterator b( otherObj );

        while ( a.more() && b.more() ) {
            bsonelement x = a.next();
            bsonelement y = b.next();
            if ( x != y )
                return false;
        }

        return ! a.more();
    }

    template <typename BSONElementColl>
    void _getFieldsDotted( const BSONObj* obj, const StringData& name, BSONElementColl &ret, bool expandLastArray ) {
        bsonelement e = obj->getField( name );

        if ( e.eoo() ) {
            size_t idx = name.find( '.' );
            if ( idx != string::npos ) {
                StringData left = name.substr( 0, idx );
                StringData next = name.substr( idx + 1, name.size() );

                bsonelement e = obj->getField( left );

                if (e.type() == Object) {
                    e.embeddedObject().getFieldsDotted(next, ret, expandLastArray );
                }
                else if (e.type() == Array) {
                    bool allDigits = false;
                    if ( next.size() > 0 && isdigit( next[0] ) ) {
                        unsigned temp = 1;
                        while ( temp < next.size() && isdigit( next[temp] ) )
                            temp++;
                        allDigits = temp == next.size() || next[temp] == '.';
                    }
                    if (allDigits) {
                        e.embeddedObject().getFieldsDotted(next, ret, expandLastArray );
                    }
                    else {
                        BSONObjIterator i(e.embeddedObject());
                        while ( i.more() ) {
                            bsonelement e2 = i.next();
                            if (e2.type() == Object || e2.type() == Array)
                                e2.embeddedObject().getFieldsDotted(next, ret, expandLastArray );
                        }
                    }
                }
                else {
                    // do nothing: no match
                }
            }
        }
        else {
            if (e.type() == Array && expandLastArray) {
                BSONObjIterator i(e.embeddedObject());
                while ( i.more() )
                    ret.insert(i.next());
            }
            else {
                ret.insert(e);
            }
        }
    }

    void BSONObj::getFieldsDotted(const StringData& name, BSONElementSet &ret, bool expandLastArray ) const {
        _getFieldsDotted( this, name, ret, expandLastArray );
    }
    void BSONObj::getFieldsDotted(const StringData& name, BSONElementMSet &ret, bool expandLastArray ) const {
        _getFieldsDotted( this, name, ret, expandLastArray );
    }

    bsonelement BSONObj::getFieldDottedOrArray(const char *&name) const {
        const char *p = strchr(name, '.');

        bsonelement sub;

        if ( p ) {
            sub = getField( string(name, p-name) );
            name = p + 1;
        }
        else {
            sub = getField( name );
            name = name + strlen(name);
        }

        if ( sub.eoo() )
            return eooElement;
        else if ( sub.type() == Array || name[0] == '\0' )
            return sub;
        else if ( sub.type() == Object )
            return sub.embeddedObject().getFieldDottedOrArray( name );
        else
            return eooElement;
    }

    BSONObj BSONObj::extractFieldsUnDotted(const BSONObj& pattern) const {
        BSONObjBuilder b;
        BSONObjIterator i(pattern);
        while ( i.moreWithEOO() ) {
            bsonelement e = i.next();
            if ( e.eoo() )
                break;
            bsonelement x = getField(e.fieldName());
            if ( !x.eoo() )
                b.appendAs(x, "");
        }
        return b.obj();
    }

    BSONObj BSONObj::extractFields(const BSONObj& pattern , bool fillWithNull ) const {
        BSONObjBuilder b(32); // scanandorder.h can make a zillion of these, so we start the allocation very small
        BSONObjIterator i(pattern);
        while ( i.moreWithEOO() ) {
            bsonelement e = i.next();
            if ( e.eoo() )
                break;
            bsonelement x = getFieldDotted(e.fieldName());
            if ( ! x.eoo() )
                b.appendAs( x, e.fieldName() );
            else if ( fillWithNull )
                b.appendNull( e.fieldName() );
        }
        return b.obj();
    }

    BSONObj BSONObj::filterFieldsUndotted( const BSONObj &filter, bool inFilter ) const {
        BSONObjBuilder b;
        BSONObjIterator i( *this );
        while( i.moreWithEOO() ) {
            bsonelement e = i.next();
            if ( e.eoo() )
                break;
            bsonelement x = filter.getField( e.fieldName() );
            if ( ( x.eoo() && !inFilter ) ||
                    ( !x.eoo() && inFilter ) )
                b.append( e );
        }
        return b.obj();
    }

    bsonelement BSONObj::getFieldUsingIndexNames(const StringData& fieldName,
                                                 const BSONObj &indexKey) const {
        BSONObjIterator i( indexKey );
        int j = 0;
        while( i.moreWithEOO() ) {
            bsonelement f = i.next();
            if ( f.eoo() )
                return bsonelement();
            if ( f.fieldName() == fieldName )
                break;
            ++j;
        }
        BSONObjIterator k( *this );
        while( k.moreWithEOO() ) {
            bsonelement g = k.next();
            if ( g.eoo() )
                return bsonelement();
            if ( j == 0 ) {
                return g;
            }
            --j;
        }
        return bsonelement();
    }
#endif
    /* grab names of all the fields in this object */
    int bsonobj::getFieldNames(set<string>& fields) const {
        int n = 0;
        bsonobjiterator i(*this);
        while ( i.moreWithEOO() ) {
            bsonelement e = i.next();
            if ( e.eoo() )
                break;
            fields.insert(e.fieldName());
            n++;
        }
        return n;
    }
#if 0
    /* note: addFields always adds _id even if not specified
       returns n added not counting _id unless requested.
    */
    int bsonobj::addFields(BSONObj& from, set<string>& fields) {
        verify( isEmpty() && !isOwned() ); /* partial implementation for now... */

        BSONObjBuilder b;

        int N = fields.size();
        int n = 0;
        BSONObjIterator i(from);
        bool gotId = false;
        while ( i.moreWithEOO() ) {
            bsonelement e = i.next();
            const char *fname = e.fieldName();
            if ( fields.count(fname) ) {
                b.append(e);
                ++n;
                gotId = gotId || strcmp(fname, "_id")==0;
                if ( n == N && gotId )
                    break;
            }
            else if ( strcmp(fname, "_id")==0 ) {
                b.append(e);
                gotId = true;
                if ( n == N && gotId )
                    break;
            }
        }

        if ( n ) {
            *this = b.obj();
        }

        return n;
    }

    bool BSONObj::couldBeArray() const {
        BSONObjIterator i( *this );
        int index = 0;
        while( i.moreWithEOO() ){
            bsonelement e = i.next();
            if( e.eoo() ) break;

            // TODO:  If actually important, may be able to do int->char* much faster
            if( strcmp( e.fieldName(), ((string)( str::stream() << index )).c_str() ) != 0 )
                return false;
            index++;
        }
        return true;
    }
#endif
    /*bsonobj bsonobj::replaceFieldNames( const bsonobj &names ) const {
        bsonobjbuilder b;
        bsonobjiterator i( *this );
        bsonobjiterator j(names);
        bsonelement f = j.moreWithEOO() ? j.next() : bsonobj().firstElement();
        while( i.moreWithEOO() ) {
            bsonelement e = i.next();
            if ( e.eoo() )
                break;
            if ( !f.eoo() ) {
                b.appendAs( e, f.fieldName() );
                f = j.next();
            }
            else {
                b.append( e );
            }
        }
        return b.obj();
    }*/

    bool bsonobjbuilder::appendAsNumber( const StringData& fieldName , const string& data ) {
        if ( data.size() == 0 || data == "-" || data == ".")
            return false;

        unsigned int pos=0;
        if ( data[0] == '-' )
            pos++;

        bool hasDec = false;

        for ( ; pos<data.size(); pos++ ) {
            if ( isdigit(data[pos]) )
                continue;

            if ( data[pos] == '.' ) {
                if ( hasDec )
                    return false;
                hasDec = true;
                continue;
            }

            return false;
        }

        if ( hasDec ) {
            double d = atof( data.c_str() );
            append( fieldName , d );
            return true;
        }

        if ( data.size() < 8 ) {
            append( fieldName , atoi( data.c_str() ) );
            return true;
        }

        try {
            long long num = boost::lexical_cast<long long>( data );
            append( fieldName , num );
            return true;
        }
        catch(boost::bad_lexical_cast &) {
            return false;
        }
    }

    /* take a BSONType and return the name of that type as a char* */
    const char* typeName (BSONType type) {
        switch (type) {
            case MinKey: return "MinKey";
            case EOO: return "EOO";
            case NumberDouble: return "NumberDouble";
            case String: return "String";
            case Object: return "Object";
            case Array: return "Array";
            case BinData: return "BinaryData";
            case Undefined: return "Undefined";
            case jstOID: return "OID";
            case Bool: return "Bool";
            case Date: return "Date";
            case jstNULL: return "NULL";
            case RegEx: return "RegEx";
            case DBRef: return "DBRef";
            case Code: return "Code";
            case Symbol: return "Symbol";
            case CodeWScope: return "CodeWScope";
            case NumberInt: return "NumberInt32";
            case Timestamp: return "Timestamp";
            case NumberLong: return "NumberLong64";
            // JSTypeMax doesn't make sense to turn into a string; overlaps with highest-valued type
            case MaxKey: return "MaxKey";
            default: return "Invalid";
        }
    }
    
}