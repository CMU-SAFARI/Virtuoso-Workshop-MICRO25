#pragma once

#include <vector>
#include <tuple>
#include <iostream>
#include <fstream>
#include "fixed_types.h"


struct Range
{
    IntPtr vpn;
    IntPtr bounds;
    IntPtr offset;
};

struct PhysicalRange{
    IntPtr ppn;
    IntPtr bounds;
};

class VMA
{
    private:
        IntPtr vbase;
        IntPtr vend;
        std::vector<Range> physical_ranges;
        bool allocated;
        int successful_offsetbased_allocations; // This is useful for tracking the number of successful allocations in the VMA

        IntPtr physical_offset; // This is useful for tracking the physical offset that we allocated the first page of the VMA at: used by Alverti et al. ISCA 2020


    public:
        VMA(IntPtr base, IntPtr end){
            vbase = base;
            vend = end;
            allocated = false;
            physical_offset = -1; // Initialize to -1 to indicate that no physical offset has been set yet
            successful_offsetbased_allocations = 0; // Initialize to 0
        }
        ~VMA()
        {
            physical_ranges.clear();
        }
        
        bool contains(IntPtr address){
            return (address >= vbase && address < vend);
        }

        void addPhysicalRange(Range range){
            physical_ranges.push_back(range);
        }
        void setAllocated(bool alloc){
            allocated = alloc;
        }
        bool isAllocated(){
            return allocated;
        }
        IntPtr getBase(){
            return vbase;
        }
        IntPtr getEnd(){
            return vend;
        }

        
        std::vector<Range> getPhysicalRanges(){
            return physical_ranges;
        }
        IntPtr getPhysicalOffset(){
            return physical_offset;
        }
        void setPhysicalOffset(IntPtr offset){
            physical_offset = offset;
        }
        void printVMA(){
            std::cout << "VMA: " << vbase << " - " << vend << std::endl;
            for (auto range : physical_ranges){
                std::cout << "Physical Range: " << range.vpn << " - " << range.bounds << " - " << range.offset << std::endl;
            }
        }

        int getSuccessfulOffsetBasedAllocations() const {
            return successful_offsetbased_allocations;
        }
        void incrementSuccessfulOffsetBasedAllocations() {
            successful_offsetbased_allocations++;
        }
        void resetSuccessfulOffsetBasedAllocations() {
            successful_offsetbased_allocations = 0;
        }
        void setSuccessfulOffsetBasedAllocations(int count) {
            successful_offsetbased_allocations = count;
        }


};


