This is a standalone BSON ("binary JSON") library for C++. (See [bsonspec.org](http://www.bsonspec.org/) for more information on the BSON format.)

The library is at this time a bit early stage, but if you find it useful, that's great.

## Notes

* The code herein originates as a fork from the MongoDB C++ driver.  Anything that isn't 
simply bson was then eliminated to get a standalone bson implementation.  Some things were 
removed just for simplicity.  Other changes have been made along the way too.

* The namespace for the module is `_bson`.  The underscore is present as it is anticipated
that one day there will be an official bson driver perhaps, and we don't want to collide
with that name-wise.

* In current form the code uses a touch of C++11. The main reason for this is simply 
to have absolutely no outside dependencies; for example in C++11 we have unique_ptr in the 
standard library, and that is used herein.  The C++11 specific code is minimal it would be 
quite easy to make the library work with older C++ editions.

* Endian-awareness has been added, is preliminary, needs testing.

## Notes for those who have used the MongoDB C++ Driver

* The `_bson::bsonobj` class here behaves differently than the driver's `mongo::BSONObj`. The MongoDB driver version has a build in smart pointer 
implementation.  That was removed here; `bsonobj` herein is simply a "view" on a bson data
buffer in memory, and bsonobj itself does no memory management of it.  The notion is that this 
is simpler and you can easily wrap them as needed yourself.

* To avoid confusion as to what implementation is being used, the classes herein use lowercase
names, unlike the MongoDB C++ driver library. If you see things in uppercase, they should probably be downcased for consistency eventually...

## License

Apache 2.0.

## Building

As mentioned above, the library uses `std::unique_ptr` among other things. Thus, use `-std=c++0x` on the compiler command line. (If that doesn't work for your project, I'd recommend using the BSON implementation in the MongoDB C++ driver instead.)

For Windows, see `build/example1/example1.vcxproj` for an example of settings that work.  (Tested on Visual Studio 2013.)

## Support

* [https://groups.google.com/forum/#!forum/bson]

## See Also

The [bsontools](https://github.com/dwight/bsontools) command line utilities project which uses this library.  Likely useful for any BSON user regardless of language.

