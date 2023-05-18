#ifndef __BRANCH_PREDICTOR_HPP__
#define __BRANCH_PREDICTOR_HPP__

#include <vector>
#include <bitset>
#include <cassert>

struct BranchPredictor {
    virtual bool predict(uint32_t pc) = 0;
    virtual void update(uint32_t pc, bool taken) = 0;
};

struct SaturatingBranchPredictor : public BranchPredictor {
    std::vector<std::bitset<2>> table;
    SaturatingBranchPredictor(int value) : table(1 << 14, value) {}

    bool predict(uint32_t pc) {
        int index = (pc & ((1 << 14) - 1));
        if (table[index].to_ulong() == 0 || table[index].to_ulong() == 1)
            return false;
        else
            return true;
        
    }

    void update(uint32_t pc, bool taken) {
        int index = (pc & ((1 << 14) - 1));
        if (taken)
        {
            if (table[index].to_ulong() == 0)
                table[index] = 1;
            else if (table[index].to_ulong() == 1)
                table[index] = 3;
        }
        else
        {
            if (table[index].to_ulong() == 3)
                table[index] = 2;
            else if (table[index].to_ulong() == 2)
                table[index] = 0;
        }
    }
};

struct BHRBranchPredictor : public BranchPredictor {
    std::vector<std::bitset<2>> bhrTable;
    std::bitset<2> bhr;
    BHRBranchPredictor(int value) : bhrTable(1 << 2, value), bhr(value) {}

    bool predict(uint32_t pc) {
        int index = bhr.to_ulong() << 2;
        if (bhrTable[index].to_ulong() == 0 || bhrTable[index].to_ulong() == 1)
            return false;
        else
            return true;
        return false;
    }

    void update(uint32_t pc, bool taken) {
        int index = bhr.to_ulong() << 2;
        if (taken)
        {
            if (bhrTable[index].to_ulong() == 0)
                bhrTable[index] = 1;
            else if (bhrTable[index].to_ulong() == 1)
                bhrTable[index] = 3;
        }
        else
        {
            if (bhrTable[index].to_ulong() == 3)
                bhrTable[index] = 2;
            else if (bhrTable[index].to_ulong() == 2)
                bhrTable[index] = 0;
        }
    }
};

struct SaturatingBHRBranchPredictor : public BranchPredictor {
    std::vector<std::bitset<2>> bhrTable;
    std::bitset<2> bhr;
    std::vector<std::bitset<2>> table;
    std::vector<std::bitset<2>> combination;
    SaturatingBHRBranchPredictor(int value, int size) : bhrTable(1 << 2, value), bhr(value), table(1 << 14, value), combination(size, value) {
        assert(size <= (1 << 16));
    }

    bool predict(uint32_t pc) {
        int index = bhr.to_ulong() << 2;
        int index2 = (pc & ((1 << 14) - 1));
        int index3 = (bhr.to_ulong() << 14) ^ (table[index2].to_ulong() << 2);
        if (combination[index3].to_ulong() == 0 || combination[index3].to_ulong() == 1)
            return false;
        else
            return true;
        return false;
    }

    void update(uint32_t pc, bool taken) {
        int index = bhr.to_ulong() << 2;
        int index2 = (pc & ((1 << 14) - 1));
        int index3 = (bhr.to_ulong() << 14) ^ (table[index2].to_ulong() << 2);
        if (taken)
        {
            if (bhrTable[index].to_ulong() == 0)
                bhrTable[index] = 1;
            else if (bhrTable[index].to_ulong() == 1)
                bhrTable[index] = 3;
            if (table[index2].to_ulong() == 0)
                table[index2] = 1;
            else if (table[index2].to_ulong() == 1)
                table[index2] = 3;
            if (combination[index3].to_ulong() == 0)
                combination[index3] = 1;
            else if (combination[index3].to_ulong() == 1)
                combination[index3] = 3;
        }
        else
        {
            if (bhrTable[index].to_ulong() == 3)
                bhrTable[index] = 2;
            else if (bhrTable[index].to_ulong() == 2)
                bhrTable[index] = 0;
            if (table[index2].to_ulong() == 3)
                table[index2] = 2;
            else if (table[index2].to_ulong() == 2)
                table[index2] = 0;
            if (combination[index3].to_ulong() == 3)
                combination[index3] = 2;
            else if (combination[index3].to_ulong() == 2)
                combination[index3] = 0;
        }
    }
};

#endif

