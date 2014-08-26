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

    inline bsonobj bsonobj::getObjectField(const StringData& name) const {
        bsonelement e = getField(name);
        BSONType t = e.type();
        return t == Object || t == Array ? e.object() : bsonobj();
    }
    inline bsonobj::bsonobj() {
        static char p[] = { /*size*/5, 0, 0, 0, /*eoo*/0 };
        _objdata = p;
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

    inline bsonobj bsonelement::embeddedObjectUserCheck() const {
        if ( (isObject()) )
            return bsonobj(value());
        std::stringstream ss;
        ss << "invalid parameter: expected an object (" << fieldName() << ")";
        uasserted( 10065 , ss.str() );
        return bsonobj(); // never reachable
    }

    inline bsonobj bsonelement::object() const {
        verify( isObject() );
        return bsonobj(value());
    }

    inline bsonobj bsonelement::codeWScopeObject() const {
        verify( type() == CodeWScope );
        int strSizeWNull = *(int *)( value() + 4 );
        return bsonobj( value() + 4 + 4 + strSizeWNull );
    }

    // deep (full) equality
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
    
    /* add all the fields from the object specified to this object if they don't exist */
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
    inline std::string bsonobj::toString(bool isArray, bool full) const {
        if ( isEmpty() ) return (isArray ? "[]" : "{}");
        StringBuilder s;
        toString(s, isArray, full);
        return s.str();
    }

}
