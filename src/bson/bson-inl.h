/** @file bsoninlines.h
          a goal here is that the most common bson methods can be used inline-only, a la boost.
          thus some things are inline that wouldn't necessarily be otherwise.
*/

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

#pragma once

#include <map>
#include <limits>
#include "float_utils.h"
#include "hex.h"

namespace _bson {

    /* must be same type when called, unless both sides are #s 
       this large function is in header to facilitate inline-only use of bson
    */
    inline int compareElementValues(const bsonelement& l, const bsonelement& r) {
        int f;

        switch ( l.type() ) {
        case EOO:
        case Undefined: // EOO and Undefined are same canonicalType
        case jstNULL:
        case MaxKey:
        case MinKey:
            f = l.canonicalType() - r.canonicalType();
            if ( f<0 ) return -1;
            return f==0 ? 0 : 1;
        case Bool:
            return *l.value() - *r.value();
        case Timestamp:
            // unsigned compare for timestamps - note they are not really dates but (ordinal + time_t)
            if ( l.date() < r.date() )
                return -1;
            return l.date() == r.date() ? 0 : 1;
        case Date:
            {
                long long a = (long long) l.Date().millis;
                long long b = (long long) r.Date().millis;
                if( a < b ) 
                    return -1;
                return a == b ? 0 : 1;
            }
        case NumberLong:
            if( r.type() == NumberLong ) {
                long long L = l._numberLong();
                long long R = r._numberLong();
                if( L < R ) return -1;
                if( L == R ) return 0;
                return 1;
            }
            goto dodouble;
        case NumberInt:
            if( r.type() == NumberInt ) {
                int L = l._numberInt();
                int R = r._numberInt();
                if( L < R ) return -1;
                return L == R ? 0 : 1;
            }
            // else fall through
        case NumberDouble: 
dodouble:
            {
                double left = l.number();
                double right = r.number();
                if( left < right ) 
                    return -1;
                if( left == right )
                    return 0;
                if( isNaN(left) )
                    return isNaN(right) ? 0 : -1;
                return 1;
            }
        case jstOID:
            return memcmp(l.value(), r.value(), 12);
        case Code:
        case Symbol:
        case String:
            /* todo: a utf sort order version one day... */
            {
                // we use memcmp as we allow zeros in UTF8 strings
                int lsz = l.valuestrsize();
                int rsz = r.valuestrsize();
                int common = std::min(lsz, rsz);
                int res = memcmp(l.valuestr(), r.valuestr(), common);
                if( res ) 
                    return res;
                // longer string is the greater one
                return lsz-rsz;
            }
        case Object:
        case Array:
            return l.embeddedObject().woCompare( r.embeddedObject() );
        case DBRef: {
            int lsz = l.valuesize();
            int rsz = r.valuesize();
            if ( lsz - rsz != 0 ) return lsz - rsz;
            return memcmp(l.value(), r.value(), lsz);
        }
        case BinData: {
            int lsz = l.objsize(); // our bin data size in bytes, not including the subtype byte
            int rsz = r.objsize();
            if ( lsz - rsz != 0 ) return lsz - rsz;
            return memcmp(l.value()+4, r.value()+4, lsz+1 /*+1 for subtype byte*/);
        }
        case RegEx: {
            int c = strcmp(l.regex(), r.regex());
            if ( c )
                return c;
            return strcmp(l.regexFlags(), r.regexFlags());
        }
        case CodeWScope : {
            f = l.canonicalType() - r.canonicalType();
            if ( f )
                return f;
            f = strcmp( l.codeWScopeCode() , r.codeWScopeCode() );
            if ( f )
                return f;
            f = strcmp( l.codeWScopeScopeDataUnsafe() , r.codeWScopeScopeDataUnsafe() );
            if ( f )
                return f;
            return 0;
        }
        default:
            verify( false);
        }
        return -1;
    }

    /* wo = "well ordered" 
       note: (mongodb related) : this can only change in behavior when index version # changes
    */
    inline int bsonelement::woCompare( const bsonelement &e,
                                bool considerFieldName ) const {
        int lt = (int) canonicalType();
        int rt = (int) e.canonicalType();
        int x = lt - rt;
        if( x != 0 && (!isNumber() || !e.isNumber()) )
            return x;
        if ( considerFieldName ) {
            x = strcmp(fieldName(), e.fieldName());
            if ( x != 0 )
                return x;
        }
        x = compareElementValues(*this, e);
        return x;
    }

/*    inline bsonobjiterator bsonobj::begin() const {
        return bsonobjiterator(*this);
    }
    */
    inline bsonobj bsonelement::embeddedObjectUserCheck() const {
        if ( (isABSONObj()) )
            return bsonobj(value());
        std::stringstream ss;
        ss << "invalid parameter: expected an object (" << fieldName() << ")";
        uasserted( 10065 , ss.str() );
        return bsonobj(); // never reachable
    }

    inline bsonobj bsonelement::embeddedObject() const {
        verify( isABSONObj() );
        return bsonobj(value());
    }

    inline bsonobj bsonelement::codeWScopeObject() const {
        verify( type() == CodeWScope );
        int strSizeWNull = *(int *)( value() + 4 );
        return bsonobj( value() + 4 + 4 + strSizeWNull );
    }

    // deep (full) equality
    /*inline bool bsonobj::equal(const bsonobj &rhs) const {
        bsonobjiterator i(*this);
        bsonobjiterator j(rhs);
        bsonelement l,r;
        do {
            // so far, equal...
            l = i.next();
            r = j.next();
            if ( l.eoo() )
                return r.eoo();
        } while( l == r );
        return false;
    }
    */
    inline NOINLINE_DECL void bsonobj::_assertInvalid() const {
        StringBuilder ss;
        int os = objsize();
        ss << "bsonobj size: " << os << " (0x" << integerToHex( os ) << ") is invalid. "
           << "Size must be between 0 and " << BSONObjMaxInternalSize
           << "(" << ( BSONObjMaxInternalSize/(1024*1024) ) << "MB)";
        try {
            bsonelement e = firstElement();
            ss << " First element: " << e.toString();
        }
        catch ( ... ) { }
        massert( 10334 , ss.str() , 0 );
    }

    /* the idea with NOINLINE_DECL here is to keep this from inlining in the
       getOwned() method.  the presumption being that is better.
    */
/*    inline NOINLINE_DECL bsonobj bsonobj::copy() const {
        Holder *h = (Holder*) malloc(objsize() + sizeof(unsigned));
        h->zero();
        memcpy(h->data, objdata(), objsize());
        return bsonobj(h);
    }

    inline bsonobj bsonobj::getOwned() const {
        if ( isOwned() )
            return *this;
        return copy();
    }
    */
    /*
    inline void bsonobj::getFields(unsigned n, const char **fieldNames, bsonelement *fields) const { 
        bsonobjiterator i(*this);
        while ( i.more() ) {
            bsonelement e = i.next();
            const char *p = e.fieldName();
            for( unsigned i = 0; i < n; i++ ) {
                if( strcmp(p, fieldNames[i]) == 0 ) {
                    fields[i] = e;
                    break;
                }
            }
        }
    }*/
    
    /*
    inline int bsonobj::getIntField(const StringData& name) const {
        bsonelement e = getField(name);
        return e.isNumber() ? (int) e.number() : std::numeric_limits< int >::min();
    }

    inline bool bsonobj::getBoolField(const StringData& name) const {
        bsonelement e = getField(name);
        return e.type() == Bool ? e.boolean() : false;
    }

    inline const char * bsonobj::getStringField(const StringData& name) const {
        bsonelement e = getField(name);
        return e.type() == String ? e.valuestr() : "";
    }
    */
    /* add all the fields from the object specified to this object if they don't exist */
    #if 0
    inline bsonobjbuilder& bsonobjbuilder::appendElementsUnique(bsonobj x) {
        std::set<std::string> have;
        {
            bsonobjiterator i = iterator();
            while ( i.more() )
                have.insert( i.next().fieldName() );
        }
        
        bsonobjiterator it(x);
        while ( it.more() ) {
            bsonelement e = it.next();
            if ( have.count( e.fieldName() ) )
                continue;
            append(e);
        }
        return *this;
    }

#endif

    inline bool bsonobj::isValid() const {
        int x = objsize();
        return x > 0 && x <= BSONObjMaxInternalSize;
    }

    inline bool bsonobj::getObjectID(bsonelement& e) const {
        bsonelement f = getField("_id");
        if( !f.eoo() ) {
            e = f;
            return true;
        }
        return false;
    }
    /*
    inline BSONObjBuilderValueStream::BSONObjBuilderValueStream( bsonobjbuilder * builder ) {
        _builder = builder;
    }

    template<class T>
    inline bsonobjbuilder& BSONObjBuilderValueStream::operator<<( T value ) {
        _builder->append(_fieldName, value);
        _fieldName = StringData();
        return *_builder;
    }

    inline bsonobjbuilder& BSONObjBuilderValueStream::operator<<( const bsonelement& e ) {
        _builder->appendAs( e , _fieldName );
        _fieldName = StringData();
        return *_builder;
    }

    inline BufBuilder& BSONObjBuilderValueStream::subobjStart() {
        StringData tmp = _fieldName;
        _fieldName = StringData();
        return _builder->subobjStart(tmp);
    }

    inline BufBuilder& BSONObjBuilderValueStream::subarrayStart() {
        StringData tmp = _fieldName;
        _fieldName = StringData();
        return _builder->subarrayStart(tmp);
    }

    inline Labeler BSONObjBuilderValueStream::operator<<( const Labeler::Label &l ) {
        return Labeler( l, this );
    }

    inline void BSONObjBuilderValueStream::endField( const StringData& nextFieldName ) {
        if ( haveSubobj() ) {
            verify( _fieldName.rawData() );
            _builder->append( _fieldName, subobj()->done() );
            _subobj.reset();
        }
        _fieldName = nextFieldName;
    }

    inline bsonobjbuilder *BSONObjBuilderValueStream::subobj() {
        if ( !haveSubobj() )
            _subobj.reset( new bsonobjbuilder() );
        return _subobj.get();
    }
    */
#if 0
    template<class T> inline
    bsonobjbuilder& Labeler::operator<<( T value ) {
        s_->subobj()->append( l_.l_, value );
        return *s_->_builder;
    }

    inline
    bsonobjbuilder& Labeler::operator<<( const bsonelement& e ) {
        s_->subobj()->appendAs( e, l_.l_ );
        return *s_->_builder;
    }
    // {a: {b:1}} -> {a.b:1}
    void nested2dotted(bsonobjbuilder& b, const bsonobj& obj, const std::string& base="");
    inline bsonobj nested2dotted(const bsonobj& obj) {
        bsonobjbuilder b;
        nested2dotted(b, obj);
        return b.obj();
    }

    // {a.b:1} -> {a: {b:1}}
    void dotted2nested(bsonobjbuilder& b, const bsonobj& obj);
    inline bsonobj dotted2nested(const bsonobj& obj) {
        bsonobjbuilder b;
        dotted2nested(b, obj);
        return b.obj();
    }
#endif

#if 0
    inline bsonobjiterator bsonobjbuilder::iterator() const {
        const char * s = _b.buf() + _offset;
        const char * e = _b.buf() + _b.len();
        return bsonobjiterator(s, e);
    }
    inline bool bsonobjbuilder::hasField( const StringData& name ) const {
        bsonobjiterator i = iterator();
        while ( i.more() )
            if ( name == i.next().fieldName() )
                return true;
        return false;
    }

    /* WARNING: nested/dotted conversions are not 100% reversible
     * nested2dotted(dotted2nested({a.b: {c:1}})) -> {a.b.c: 1}
     * also, dotted2nested ignores order
     */

    typedef std::map<std::string, bsonelement> BSONMap;
    inline BSONMap bson2map(const bsonobj& obj) {
        BSONMap m;
        bsonobjiterator it(obj);
        while (it.more()) {
            bsonelement e = it.next();
            m[e.fieldName()] = e;
        }
        return m;
    }

    struct BSONElementFieldNameCmp {
        bool operator()( const bsonelement &l, const bsonelement &r ) const {
            return strcmp( l.fieldName() , r.fieldName() ) <= 0;
        }
    };

    typedef std::set<bsonelement, BSONElementFieldNameCmp> BSONSortedElements;
    inline BSONSortedElements bson2set( const bsonobj& obj ) {
        BSONSortedElements s;
        bsonobjiterator it(obj);
        while ( it.more() )
            s.insert( it.next() );
        return s;
    }

#endif

    inline std::string bsonobj::toString(bool isArray, bool full) const {
        if ( isEmpty() ) return (isArray ? "[]" : "{}");
        StringBuilder s;
        toString(s, isArray, full);
        return s.str();
    }

    inline int bsonelement::size( int maxLen ) const {
        if ( totalSize >= 0 )
            return totalSize;

        int remain = maxLen - fieldNameSize() - 1;

        int x = 0;
        switch ( type() ) {
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
            massert( 10313 ,  "Insufficient bytes to calculate element size", maxLen == -1 || remain > 3 );
            x = valuestrsize() + 4;
            break;
        case CodeWScope:
            massert( 10314 ,  "Insufficient bytes to calculate element size", maxLen == -1 || remain > 3 );
            x = objsize();
            break;

        case DBRef:
            massert( 10315 ,  "Insufficient bytes to calculate element size", maxLen == -1 || remain > 3 );
            x = valuestrsize() + 4 + 12;
            break;
        case Object:
        case _bson::Array:
            massert( 10316 ,  "Insufficient bytes to calculate element size", maxLen == -1 || remain > 3 );
            x = objsize();
            break;
        case BinData:
            massert( 10317 ,  "Insufficient bytes to calculate element size", maxLen == -1 || remain > 3 );
            x = valuestrsize() + 4 + 1/*subtype*/;
            break;
        case RegEx: {
            const char *p = value();
            size_t len1 = ( maxLen == -1 ) ? strlen( p ) : (size_t)strnlen( p, remain );
            //massert( 10318 ,  "Invalid regex string", len1 != -1 ); // ERH - 4/28/10 - don't think this does anything
            p = p + len1 + 1;
            size_t len2;
            if( maxLen == -1 )
                len2 = strlen( p );
            else {
                size_t x = remain - len1 - 1;
                verify( x <= 0x7fffffff );
                len2 = strnlen( p, (int) x );
            }
            //massert( 10319 ,  "Invalid regex options string", len2 != -1 ); // ERH - 4/28/10 - don't think this does anything
            x = (int) (len1 + 1 + len2 + 1);
        }
        break;
        default: {
            StringBuilder ss;
            ss << "bsonelement: bad type " << (int) type();
            std::string msg = ss.str();
            massert( 13655 , msg.c_str(),false);
        }
        }
        totalSize =  x + fieldNameSize() + 1; // BSONType

        return totalSize;
    }
}
