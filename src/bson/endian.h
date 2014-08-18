/** note : endian support isn't done, but work is started / sketched out */

#pragma once

namespace _bson {

    // no PDP or anything like that at the moment, just big/little.
    //const bool big = true;
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wstring-compare"
#endif
    const bool big = ((unsigned short&)"a") >= 0x8000;

    // todo add right intrinsics for gcc

#if defined(_WIN32)
    // this will be optimized to the right thing as big is known at compile time
    inline unsigned endian(unsigned x) {
        return big ? _byteswap_ulong(x) : x;
    }
    inline short endian_short(short x) {
        return big ? _byteswap_ushort(x) : x;
    }
    inline long long endian_ll(long long x) {
        return big ? _byteswap_uint64(x) : x;
    }
#else
    inline long long endian_ll(long long x) {
        return big ? __builtin_bswap64(x) : x;
    }
    inline unsigned endian(unsigned x) {
        return big ? __builtin_bswap32(x) : x;
    }
    inline short endian_short(short x) {
        return big ? __builtin_bswap16(x) : x;
    }
#endif

    inline int endian_int(int x) {
        return endian(x);
    }
    inline double endian_d(double x) {
        long long z = endian_ll((long long&)x);
        return (double&)z;
    }
    inline int readInt(const char *p) {
        return (int) endian(*reinterpret_cast<const unsigned *>(p));
    }

}
