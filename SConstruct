# SConstruct file for example1 of bson-cxx library

env = Environment()

env.Append(CCFLAGS='-std=c++0x')

dep1 = [
#    "src/base64.cpp bson.cpp hex.cpp json.cpp parse_number.cpp time_support.cpp valid.cpp
    "src/bson/bson.cpp","src/bson/base64.cpp","src/bson/hex.cpp","src/bson/json.cpp",
    "src/bson/time_support.cpp",
    "src/bson/parse_number.cpp"
]

# example1 does not have a dependency on valid.cpp.  if you need that file for your project
# you could do something like this:
dep2 = [
    "src/bson/valid.cpp"
    ]

env.Program(target = 'example1', source = ["src/examples/example1.cpp"] + dep1)

