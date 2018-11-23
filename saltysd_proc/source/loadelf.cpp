#include "loadelf.h"

#include <istream>
#include <streambuf>

#include "useful.h"
#include "elf_parser.hpp"

using namespace elf_parser;

u64 find_memory(Handle debug, u64 min, u64 size, u8 perm)
{
    u64 addr = 0;
    while (1)
    {
        MemoryInfo info;
        u32 pageinfo;
        Result ret = svcQueryDebugProcessMemory(&info, &pageinfo, debug, addr);
        
        if (info.perm == perm && info.size >= size && (info.type == MemType_CodeMutable || info.type == MemType_CodeStatic) && info.addr >= min)
            return info.addr;

        addr = info.addr + info.size;
        
        if (!addr || ret) break;
    }
    
    return 0;
}

Result load_elf_debug(Handle debug, uint64_t* start, uint8_t* elf_data, u32 elf_size)
{
    Result ret = 0;

    Elf_parser elf(elf_data);

    segment_t text_seg = elf.get_segments()[0];
    segment_t data_seg = elf.get_segments()[1];
    u64 text_addr = find_memory(debug, 0, text_seg.phdr->p_memsz, Perm_Rx);
    u64 data_addr = find_memory(debug, text_addr, data_seg.phdr->p_memsz, Perm_Rw);
    write_log(".text to %llx, .data to %llx\n", text_addr, data_addr);
    
    elf.relocate_segment(0, text_addr);
    elf.relocate_segment(1, data_addr);
    
    *start = text_addr;
    
    //TODO: keep backups
    ret = svcWriteDebugProcessMemory(debug, text_seg.data, text_addr, text_seg.phdr->p_filesz);
    ret = svcWriteDebugProcessMemory(debug, data_seg.data, data_addr, data_seg.phdr->p_filesz);
    
    return ret;
}

Result load_elf_proc(Handle proc, uint64_t pid, uint64_t heap, uint64_t* start, uint8_t* elf_data, u32 elf_size)
{
    Result ret;
    Handle debug;
    
    Elf_parser elf(elf_data);
    
    // Figure out our number of pages
    u64 min_vaddr = -1, max_vaddr = 0;
    for (auto seg : elf.get_segments())
    {
        u64 min = seg.phdr->p_vaddr;
        u64 max = seg.phdr->p_vaddr + (seg.phdr->p_memsz + 0xFFF & ~0xFFF);
        if (min < min_vaddr)
            min_vaddr = min;

        if (max > max_vaddr)
            max_vaddr = max;
    }
    
    // Debug the process to write into the heap addr provided
    // Note: Could probably just use buffer descs for this but whatever.
    ret = svcDebugActiveProcess(&debug, pid);
    if (ret) return ret;

    for (auto seg : elf.get_segments())
    {
        ret = svcWriteDebugProcessMemory(debug, seg.data, heap + seg.phdr->p_vaddr, seg.phdr->p_filesz);
        if (ret) break;
    }
    
    ret = svcCloseHandle(debug);
    if (ret) return ret;
    
    // Unmap heap, map new code
    ret = svcMapProcessCodeMemory(proc, 0x800000000, heap, (max_vaddr - min_vaddr));
    if (ret) return ret;
    
    // Adjust permissions and then return
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

        ret = svcSetProcessMemoryPermission(proc, 0x800000000 + seg.phdr->p_vaddr, seg.phdr->p_memsz + 0xFFF & ~0xFFF, perms);
    }
    
    *start = 0x800000000;
    
    return ret;
}
