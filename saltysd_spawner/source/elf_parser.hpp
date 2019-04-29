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
#include <vector>
#include <elf.h>      // Elf64_Shdr
#include <fcntl.h>

namespace elf_parser {

typedef struct {
    int section_index = 0; 
    std::intptr_t section_offset, section_addr;
    //std::string section_name;
    std::string section_type; 
    int section_size, section_ent_size, section_addr_align;
} section_t;

typedef struct {
    std::string segment_type;
    uint8_t segment_flags;
    long segment_offset, segment_virtaddr, segment_physaddr, segment_filesize, segment_memsize;
    int segment_align;
} segment_t;

class Elf_parser {
    public:
             
        Elf_parser(char* path)
        {
            Elf64_Shdr sh_strtab;
            //char* sh_strtab_p;
            
            load_error = 0;
            FILE* f = NULL;
            fp = NULL;
            
            f = fopen(path, "rb");
            if (!f)
            {
                load_error = 1;
                return;
            }
            
            fread(&ehdr, sizeof(ehdr), 1, f);
            /*{
                fclose(f);
                
                load_error = 2;
                return;
            }*/
            
            // Read symtab
            /*fseek(f, ehdr.e_shoff + ehdr.e_shstrndx * sizeof(Elf64_Shdr), SEEK_SET);
            fread(&sh_strtab, sizeof(sh_strtab), 1, f);
            
            sh_strtab_p = (char*)malloc(sh_strtab.sh_size);
            fseek(f, sh_strtab.sh_offset, SEEK_SET);
            fread(&sh_strtab_p, sh_strtab.sh_size, 1, f);*/

            // Read sections
            for (int i = 0; i < ehdr.e_shnum; ++i) {
                Elf64_Shdr read;
                fseek(f, ehdr.e_shoff + i * sizeof(Elf64_Shdr), SEEK_SET);
                fread(&read, sizeof(read), 1, f);
            
                section_t section;
                section.section_index= i;
                //section.section_name = std::string(sh_strtab_p + read.sh_name);
                section.section_type = get_section_type(read.sh_type);
                section.section_addr = read.sh_addr;
                section.section_offset = read.sh_offset;
                section.section_size = read.sh_size;
                section.section_ent_size = read.sh_entsize;
                section.section_addr_align = read.sh_addralign; 
                
                sections.push_back(section);
            }

            // Read segments
            for (int i = 0; i < ehdr.e_phnum; ++i) {
                Elf64_Phdr read;
                fseek(f, ehdr.e_phoff + i * sizeof(Elf64_Phdr), SEEK_SET);
                fread(&read, sizeof(read), 1, f);
                
                segment_t segment;
                segment.segment_type     = get_segment_type(read.p_type);
                segment.segment_offset   = read.p_offset;
                segment.segment_virtaddr = read.p_vaddr;
                segment.segment_physaddr = read.p_paddr;
                segment.segment_filesize = read.p_filesz;
                segment.segment_memsize  = read.p_memsz;
                segment.segment_flags    = get_segment_flags(read.p_flags);
                segment.segment_align    = read.p_align;
                
                segments.push_back(segment);
            }
            
            //free(sh_strtab_p);
            fp = f;
        }
        
        ~Elf_parser()
        {
            if (fp)
            {
                fclose(fp);
                fp = NULL;
            }
            load_error = 3;
        }
        
        std::vector<section_t>& get_sections() { return sections; };
        std::vector<segment_t>& get_segments() { return segments; };
        int get_load_error() { return load_error; }
        
        size_t read_segment(segment_t& seg, void* dst)
        {
            fseek(fp, seg.segment_offset, SEEK_SET);
            return fread(dst, seg.segment_filesize, 1, fp);
        }
        
        FILE* get_file()
        {
            return fp;
        }

    private:
        FILE* fp;
        Elf64_Ehdr ehdr;
        int load_error;
        std::vector<section_t> sections;
        std::vector<segment_t> segments;

        std::string get_section_type(int tt);

        std::string get_segment_type(uint32_t &seg_type);
        uint8_t get_segment_flags(uint32_t &seg_flags);
};

}
#endif
