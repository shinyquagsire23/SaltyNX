#include "loadelf.h"

#include <istream>
#include <streambuf>

#include "useful.h"
#include "elf_parser.hpp"

using namespace elf_parser;

u64 find_memory(Handle debug, u64 size, u8 perm)
{
    u64 addr = 0;
    while (1)
    {
        MemoryInfo info;
        u32 pageinfo;
        Result ret = svcQueryDebugProcessMemory(&info, &pageinfo, debug, addr);
        
        if (info.perm == perm && info.size >= size && (info.type == MemType_CodeMutable || info.type == MemType_CodeStatic))
            return info.addr;

        addr = info.addr + info.size;
        
        if (!addr || ret) break;
    }
    
    return 0;
}

Result load_elf(Handle debug, uint64_t* start, uint8_t* elf_data, u32 elf_size)
{
    Result ret = 0;

    Elf_parser elf(elf_data);

    segment_t text_seg = elf.get_segments()[0];
    segment_t data_seg = elf.get_segments()[1];
    u64 text_addr = find_memory(debug, text_seg.phdr->p_memsz, Perm_Rx);
    u64 data_addr = find_memory(debug, data_seg.phdr->p_memsz, Perm_Rw);
    write_log(".text to %llx, .data to %llx\n", text_addr, data_addr);
    
    elf.relocate_segment(0, text_addr);
    elf.relocate_segment(1, data_addr);
    
    *start = text_addr;
    
    //TODO: keep backups
    ret = svcWriteDebugProcessMemory(debug, text_seg.data, text_addr, text_seg.phdr->p_filesz);
    ret = svcWriteDebugProcessMemory(debug, data_seg.data, data_addr, data_seg.phdr->p_filesz);
    
    return ret;
}
