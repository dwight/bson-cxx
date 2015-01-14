/* 
    g++ example1.cpp ../bson/json.cpp ../bson/bson.cpp ../bson/time_support.cpp ../bson/parse_number.cpp  ../bson/base64.cpp 

    vstudio: see build/examples/examples.sln
 */

#include <iostream>
#include <string>
#include "../bson/json.h"
#include "../bson/bsonobjbuilder.h"

using namespace std;
using namespace _bson;

void go1() { 
    // example of the use of fromjson
    bsonobjbuilder b;
    stringstream s("{x:3,y:\"hello world\"}");
    bsonobj o = fromjson(s, b);
    cout << o.toString() << endl;
}

void go2() { 
    // example of using bsonobjbuilder and of nesting documents
    bsonobjbuilder b;
    b.append("x", 3.14);
    b.append("y", true);
    bsonobjbuilder a;
    a.append("x", 2);
    a.append("yy", b.obj());
    bsonobj o = a.obj();
    cout << o.toString() << endl;

    // reading value of the "x" field from the bson object 
    cout << "x: " << o["x"].Int() << endl;

    // "dot notation" example
    cout << "yy.y: " << o.getFieldDotted("yy.y").Bool() << endl;
}

void go() { 
    go1();
    go2();
}

int main(int argc, char* argv[])
{
    // the bson library throws exceptions, for example when encountering malformed data.
    // thus the try catch block
    try {
        go();
    }
    catch (std::exception& e) {
        cerr << "exception ";
        cerr << e.what() << endl;
        return EXIT_FAILURE;
    }
    return 0;
}

