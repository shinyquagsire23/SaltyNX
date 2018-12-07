#include "loadelf.h"

#include <istream>
#include <streambuf>

#include "useful.h"
#include "elf_parser.hpp"

using namespace elf_parser;

#define AddrSpace_32 0
#define AddrSpace_36 1
#define AddrSpace_32nomap 2
#define AddrSpace_39 3

struct ProcInfo
{
    char name[12];
    u32 category;
    u64 tid;
    u64 codeAddr;
    u32 codePages;
    u32 isA64 : 1;
    u32 addrSpace : 3;
    u32 enableDebug : 1;
    u32 enableAslr : 1;
    u32 useSysMemBlocks : 1;
    u32 poolPartition : 4;
    u32 unused : 22;
    u32 resLimitHandle;
    u32 personalHeapNumPages;
};

u32 pcaps[24] =
{
    0x1FFFFFEF, 
    0x3FFFFFEF, 
    0x5FFFFFEF, 
    0x7FFFFFEF, 
    0x9FFFFFEF, 
    0xA0001FEF, 
    0x030073B7, 
    0x00001FFF, 
    0x00183FFF, 
    0x02007FFF, 
    0x0006FFFF, 
    0x0600067F, 
    0x0600077F, 
    0x07000F7F, 
    0xFFFFFFFF, 
    0xFFFFFFFF, 
    0xFFFFFFFF, 
    0xFFFFFFFF, 
    0xFFFFFFFF, 
    0xFFFFFFFF, 
    0xFFFFFFFF, 
    0xFFFFFFFF, 
    0xFFFFFFFF, 
    0xFFFFFFFF, 
};

Result load_elf(uint8_t* elf_data, u32 elf_size)
{
    Result ret;
    Handle proc;
    struct ProcInfo procInfo;
    
    Elf_parser elf(elf_data);
    
    // Figure out our number of pages
    u64 min_vaddr = -1, max_vaddr = 0;
    for (auto seg : elf.get_segments())
    {
        u64 min = seg.segment_virtaddr;
        u64 max = seg.segment_virtaddr + (seg.segment_memsize + 0xFFF & ~0xFFF);
        if (min < min_vaddr)
            min_vaddr = min;

        if (max > max_vaddr)
            max_vaddr = max;
    }
    
    strcpy(procInfo.name, "SaltySD");
    procInfo.category = 1; // builtin
    procInfo.tid = 0x0100000000535344;
    procInfo.codeAddr = 0x800000000;
    procInfo.codePages = (max_vaddr - min_vaddr) / 0x1000;
    procInfo.isA64 = true;
    procInfo.addrSpace = AddrSpace_39;
    procInfo.enableDebug = true;
    procInfo.enableAslr = false;
    procInfo.useSysMemBlocks = false;
    procInfo.poolPartition = 2;
    
    procInfo.resLimitHandle = 0;
    procInfo.personalHeapNumPages = 0;
    
    // Create the process and map its memory for writing
    ret = svcCreateProcess(&proc, &procInfo, pcaps, 24);
    if (ret) return ret;
    SaltySD_printf("SaltySD Spawner: got handle %x for process\n", proc);
    
    void* test = virtmemReserve(procInfo.codePages * 0x1000);
    ret = svcMapProcessMemory(test, proc, procInfo.codeAddr, procInfo.codePages * 0x1000);
    if (ret) return ret;

    for (auto seg : elf.get_segments())
    {
        memcpy(test +  seg.segment_virtaddr, &elf_data[seg.segment_offset], seg.segment_filesize);
    }
    
    ret = svcUnmapProcessMemory(test, proc, procInfo.codeAddr, procInfo.codePages * 0x1000);
    if (ret) return ret;
    
    // Adjust permissions and then launch
    for (auto seg : elf.get_segments())
    {
        u8 perms = 0;
        for (auto c : seg.segment_flags)
        {
            switch (c)
            {
                case 'R':
                    perms |= Perm_R;
                    break;
                case 'W':
                    perms |= Perm_W;
                    break;
                case 'E':
                    perms |= Perm_X;
                    break;
            }
        }

        ret = svcSetProcessMemoryPermission(proc, procInfo.codeAddr + seg.segment_virtaddr, seg.segment_memsize + 0xFFF & ~0xFFF, perms);
    }
    
    ret = svcStartProcess(proc, 49, 3, 0x10000);
    return ret;
}
