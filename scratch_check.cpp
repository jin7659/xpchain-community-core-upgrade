#include <iostream>
#include <vector>
#include <string>
#include <stdint.h>
#include <assert.h>

class arith_uint256 {
public:
    uint32_t pn[8];

    arith_uint256() { for(int i=0;i<8;i++) pn[i]=0; }
    arith_uint256(uint32_t b) { pn[0]=b; for(int i=1;i<8;i++) pn[i]=0; }

    arith_uint256& operator<<=(unsigned int shift) {
        arith_uint256 a(*this);
        for (int i = 0; i < 8; i++) pn[i] = 0;
        int k = shift / 32;
        shift = shift % 32;
        for (int i = 0; i < 8; i++) {
            if (i + k + 1 < 8 && shift != 0)
                pn[i + k + 1] |= (a.pn[i] >> (32 - shift));
            if (i + k < 8)
                pn[i + k] |= (a.pn[i] << shift);
        }
        return *this;
    }
    
    int CompareTo(const arith_uint256& b) const {
        for (int i = 7; i >= 0; i--) {
            if (pn[i] < b.pn[i]) return -1;
            if (pn[i] > b.pn[i]) return 1;
        }
        return 0;
    }
    bool operator>(const arith_uint256& b) const { return CompareTo(b) > 0; }
    bool operator==(uint32_t b) const {
        for(int i=7; i>=1; i--) if(pn[i]!=0) return false;
        return pn[0] == b;
    }
    
    arith_uint256& SetCompact(uint32_t nCompact, bool* pfNegative, bool* pfOverflow) {
        int nSize = nCompact >> 24;
        uint32_t nWord = nCompact & 0x007fffff;
        if (nSize <= 3) {
            nWord >>= 8 * (3 - nSize);
            *this = nWord;
        } else {
            *this = nWord;
            *this <<= 8 * (nSize - 3);
        }
        if (pfNegative) *pfNegative = nWord != 0 && (nCompact & 0x00800000) != 0;
        if (pfOverflow) *pfOverflow = nWord != 0 && ((nSize > 34) || (nWord > 0xff && nSize > 33) || (nWord > 0xffff && nSize > 32));
        return *this;
    }
};

int main() {
    arith_uint256 powLimit;
    powLimit.pn[7] = 0x7fffffff;
    for(int i=0;i<7;i++) powLimit.pn[i] = 0xffffffff;
    
    bool fNegative, fOverflow;
    arith_uint256 bnTarget;
    bnTarget.SetCompact(0x207fffff, &fNegative, &fOverflow);
    
    std::cout << "fNegative: " << fNegative << "\n";
    std::cout << "fOverflow: " << fOverflow << "\n";
    std::cout << "bnTarget == 0: " << (bnTarget == 0) << "\n";
    std::cout << "bnTarget > powLimit: " << (bnTarget > powLimit) << "\n";
    
    std::cout << "bnTarget.pn[7]: " << std::hex << bnTarget.pn[7] << "\n";
    std::cout << "powLimit.pn[7]: " << std::hex << powLimit.pn[7] << "\n";
    return 0;
}
