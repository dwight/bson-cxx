This is a standalone BSON library for C++.

The library is at this time experimental, but if you find it useful, great.

## Notes

* The code herein originates as a fork from the MongoDB C++ driver.  Anything that isn't 
simply bson was then eliminated to get a standalone bson implementation.  Some things were 
removed just for simplicity.  Other changes have been made along the way too.

* The namespace for the module is _bson.  The underscore is present as it is anticipated
that one day there will be an official bson driver perhaps, and we don't want to collide
with that name-wise.

* In current form the code uses a touch of C++11 things. The main reason for this is simply 
to have absolutely no outside dependencies; for example in C++11 we have unique_ptr in the 
standard library, and that is used herein.  The C++11 specific code is minimal it would be 
quite easy to make the library work with older C++ editions.

## Notes for those who have used the MongoDB C++ Driver

* bsonobj behaves differently here. The MongoDB driver version has a build in smart pointer 
implementation.  That was removed here; bsonobj herein is simply a "view" on a bson data
buffer in memory, and bsonobj itself does no memory management of it.  The notion is that this 
is simpler and you can easily wrap them as needed yourself.

* To avoid confusion as to what implementation is being used, the classes herein use lowercase
names, unlike the mongodb c++ driver library.
