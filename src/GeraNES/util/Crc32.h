#pragma once

#include <cstddef>
#include <cstdint>
#include <iomanip>

class Crc32 {

private:

    uint32_t m_crc;

public:

    Crc32(uint32_t start = 0) {
        m_crc = ~start;
    }

    void add(const char ch) {
        add(&ch, 1);
    }

    void add(const char *s, size_t n) {

        for(size_t i=0;i<n;i++) {
            char ch=s[i];
            for(size_t j=0;j<8;j++) {
                uint32_t b=(ch^m_crc)&1;
                m_crc>>=1;
                if(b) m_crc=m_crc^0xEDB88320;
                ch>>=1;
            }
        }
	}

    uint32_t get() {
        return ~m_crc;
    }

    std::string getStr() {
        return toString(get());
    }

    static uint32_t calc(const char *s,size_t n) {
        Crc32 c;
        c.add(s,n);
        return c.get();
    }

    static std::string toString(uint32_t crc) {
        std::stringstream ss;
        ss << std::uppercase << std::setfill('0') << std::setw(8) << std::hex << crc;
        return ss.str();
    }
    
};
