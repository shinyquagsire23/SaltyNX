// MIT License

// Copyright (c) 2018 finixbit

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef H_ELF_PARSER
#define H_ELF_PARSER

#include <iostream>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <fcntl.h>    /* O_RDONLY */
#include <sys/stat.h> /* For the size of the file. , fstat */
#ifdef ELFPARSE_MMAP
#include <sys/mman.h> /* mmap, MAP_PRIVATE */
#endif
#include <vector>
#include <elf.h>      // Elf64_Shdr
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

namespace elf_parser {

typedef struct {
    Elf64_Shdr *shdr;
    uint8_t* data;

    int section_index = 0; 
    std::string section_name;
    std::string section_type;
} section_t;

typedef struct {
    Elf64_Phdr *phdr;
    uint8_t* data;

    std::string segment_type, segment_flags;
} segment_t;

typedef struct {
    Elf64_Sym* sym;
    int symbol_num = 0;
    std::string symbol_name, symbol_section;  
    
    
    std::string get_type_str() {
        switch(ELF32_ST_TYPE(sym->st_info)) {
            case 0: return "NOTYPE";
            case 1: return "OBJECT";
            case 2: return "FUNC";
            case 3: return "SECTION";
            case 4: return "FILE";
            case 6: return "TLS";
            case 7: return "NUM";
            case 10: return "LOOS";
            case 12: return "HIOS";
            default: return "UNKNOWN";
        }
    }

    std::string get_bind_str() {
        switch(ELF32_ST_BIND(sym->st_info)) {
            case 0: return "LOCAL";
            case 1: return "GLOBAL";
            case 2: return "WEAK";
            case 3: return "NUM";
            case 10: return "UNIQUE";
            case 12: return "HIOS";
            case 13: return "LOPROC";
            default: return "UNKNOWN";
        }
    }

    std::string get_visibility_str() {
        switch(ELF32_ST_VISIBILITY(sym->st_other)) {
            case 0: return "DEFAULT";
            case 1: return "INTERNAL";
            case 2: return "HIDDEN";
            case 3: return "PROTECTED";
            default: return "UNKNOWN";
        }
    }

    std::string get_index_str() {
        switch(sym->st_shndx) {
            case SHN_ABS: return "ABS";
            case SHN_COMMON: return "COM";
            case SHN_UNDEF: return "UND";
            case SHN_XINDEX: return "COM";
            default: return std::to_string(sym->st_shndx);
        }
    }

} symbol_t;

typedef struct {
    Elf64_Rela* rela;
    std::string   relocation_section_name;
    uint64_t section_idx, relocation_plt_address;
    
    uint64_t get_symbol_value(const std::vector<symbol_t> &syms) {
        uint64_t sym_val = 0;
        for(auto &sym: syms) {
            if(sym.symbol_num == ELF64_R_SYM(rela->r_info)) {
                sym_val = sym.sym->st_value;
                break;
            }
        }
        
        return sym_val;
    }
    
    std::string get_symbol_name(const std::vector<symbol_t> &syms) {
        std::string sym_name;
        for(auto &sym: syms) {
            if(sym.symbol_num == ELF64_R_SYM(rela->r_info)) {
                sym_name = sym.symbol_name;
                break;
            }
        }
        
        return sym_name;
    }
    
    std::string get_relocation_type() {
        switch(ELF64_R_TYPE(rela->r_info)) {
            case 1: return "R_X86_64_32";
            case 2: return "R_X86_64_PC32";
            case 5: return "R_X86_64_COPY";
            case 6: return "R_X86_64_GLOB_DAT";
            case 7:  return "R_X86_64_JUMP_SLOT";
            case 257: return "R_AARCH64_ABS64";
            case 258: return "R_AARCH64_ABS32";
            case 259: return "R_AARCH64_ABS16";
            case 261: return "R_AARCH64_PREL32";
            default: return "OTHERS";
        }
    }

} relocation_t;


class Elf_parser {
    public:
        Elf_parser(uint8_t* data): m_program_path(""), m_mmap_program(data) {}
        Elf_parser (std::string &program_path): m_program_path{program_path} {   
            load_memory_map();
        }
        ~Elf_parser ()
        {
#if ELFPARSE_MMAP
            if (m_mmap_size)
            {
                msync(m_mmap_program, m_mmap_size, MS_SYNC);
                munmap(m_mmap_program, m_mmap_size);
                close(fd);
            }
#endif
        }
        
        std::vector<section_t> get_sections();
        std::vector<segment_t> get_segments();
        std::vector<symbol_t> get_symbols();
        std::vector<relocation_t> get_relocations();
        uint8_t *get_memory_map();
        
        void relocate_segment(int num, uint64_t new_addr);
        
    private:
        void load_memory_map();

        std::string get_section_type(int tt);

        std::string get_segment_type(uint32_t &seg_type);
        std::string get_segment_flags(uint32_t &seg_flags);

        int fd;
        std::string m_program_path; 
        uint8_t *m_mmap_program;
        size_t m_mmap_size;
};

}
#endif
