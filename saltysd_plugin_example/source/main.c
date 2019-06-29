#include <switch_min.h>

#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/iosupport.h>
#include <sys/reent.h>
#include <switch_min/kernel/ipc.h>

#include "useful.h"
#include "crc32.h"

#include "saltysd_core.h"
#include "saltysd_ipc.h"
#include "saltysd_dynamic.h"

// FileHandle == nn::fs::detail::FileSystemAccessor*

typedef struct FileSystemAccessor
{
    void* unk1; // nn::fs::detail::FileAccessor
    void* unk2;
    void* unk3;
    void* mutex; // nn::os::Mutex
    uint64_t val1;
    uint64_t mode;
} FileSystemAccessor;

extern uint64_t _ZN2nn2fs8ReadFileEPmNS0_10FileHandleElPvm(uint64_t idk1, uint64_t idk2, uint64_t idk3, uint64_t idk4, uint64_t idk5) LINKABLE;
extern uint64_t _ZN2nn2fs8ReadFileENS0_10FileHandleElPvm(uint64_t handle, uint64_t offset, uint64_t out, uint64_t size) LINKABLE;
extern uint64_t _ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci(uint64_t handle, char* path, uint64_t mode) LINKABLE;
extern void* bsearch (const void* key, const void* base,
               size_t num, size_t size,
               int (*compar)(const void*,const void*)) LINKABLE;

typedef struct arc_header
{
    uint64_t magic;
    uint64_t offset_1;
    uint64_t offset_2;
    uint64_t offset_3;
    uint64_t offset_4;
    uint64_t offset_5;
    uint64_t offset_6;
} arc_header;

typedef struct offset5_header
{
    uint64_t total_size;
    uint32_t folder_entries;
    uint32_t file_entries;
    uint32_t hash_entries;
} offset5_header;

typedef struct offset4_header
{
    uint32_t total_size;
    uint32_t entries_big;
    uint32_t entries_bigfiles_1;
    uint32_t tree_entries;
    
    uint32_t suboffset_entries;
    uint32_t file_lookup_entries;
    uint32_t folder_hash_entries;
    uint32_t tree_entries_2;
    
    uint32_t entries_bigfiles_2;
    uint32_t post_suboffset_entries;
    uint32_t alloc_alignment;
    uint32_t unk10;
    
    uint8_t weird_hash_entries;
    uint8_t unk11;
    uint8_t unk12;
    uint8_t unk13;
} offset4_header;

typedef struct offset4_ext_header
{
    uint32_t bgm_unk_movie_entries;
    uint32_t entries;
    uint32_t entries_2;
    uint32_t num_files;
} offset4_ext_header;

typedef struct entry_triplet
{
    uint64_t hash : 40;
    uint64_t meta : 24;
    uint32_t meta2;
} __attribute__((packed)) entry_triplet;

typedef struct entry_pair
{
    uint64_t hash : 40;
    uint64_t meta : 24;
} __attribute__((packed)) entry_pair;

typedef struct file_pair
{
    uint64_t size;
    uint64_t offset;
} file_pair;

typedef struct big_hash_entry
{
    entry_pair path;
    entry_pair folder;
    entry_pair parent;
    entry_pair hash4;
    uint32_t suboffset_start;
    uint32_t num_files;
    uint32_t unk3;
    uint16_t unk4;
    uint16_t unk5;
    uint8_t unk6;
    uint8_t unk7;
    uint8_t unk8;
    uint8_t unk9;
} big_hash_entry;

typedef struct big_file_entry
{
    uint64_t offset;
    uint32_t decomp_size;
    uint32_t comp_size;
    uint32_t suboffset_index;
    uint32_t files;
    uint32_t unk3;
} __attribute__((packed)) big_file_entry;

typedef struct file_entry
{
    uint32_t offset;
    uint32_t comp_size;
    uint32_t decomp_size;
    uint32_t flags;
} file_entry;

typedef struct tree_entry
{
    entry_pair path;
    entry_pair ext;
    entry_pair folder;
    entry_pair file;
    uint32_t suboffset_index;
    uint32_t flags;
} tree_entry;

typedef struct folder_tree_entry
{
    entry_pair path;
    entry_pair parent;
    entry_pair folder;
    uint32_t idx1;
    uint32_t idx2;
} folder_tree_entry;

typedef struct mini_tree_entry
{
    entry_pair path;
    entry_pair folder;
    entry_pair file;
    entry_pair ext;
} mini_tree_entry;

typedef struct hash_bucket
{
    uint32_t index;
    uint32_t num_entries;
} hash_bucket;

typedef struct offset4_structs
{
    void* off4_data;
    offset4_header* header;
    offset4_ext_header* ext_header;
    entry_triplet* bulkfile_category_info;
    entry_pair* bulkfile_hash_lookup;
    entry_triplet* bulk_files_by_name;
    uint32_t* bulkfile_lookup_to_fileidx;
    file_pair* file_pairs;
    entry_triplet* weird_hashes;
    big_hash_entry* big_hashes;
    big_file_entry* big_files;
    entry_pair* folder_hash_lookup;
    tree_entry* tree_entries;
    file_entry* suboffset_entries;
    file_entry* post_suboffset_entries;
    entry_pair* folder_to_big_hash;
    hash_bucket* file_lookup_buckets;
    entry_pair* file_lookup;
    entry_pair* numbers3;
} offset4_structs;

typedef struct offset5_structs
{
    void* off5_data;
    offset5_header* header;
    entry_pair* folderhash_to_foldertree;
    folder_tree_entry* folder_tree;
    entry_pair* entries_13;
    uint32_t* numbers;
    mini_tree_entry* tree_entries;
} offset5_structs;

typedef struct arc_section
{
    uint32_t data_start;
    uint32_t decomp_size;
    uint32_t comp_size;
    uint32_t zstd_comp_size;
} arc_section;

#define TREE_ALIGN_MASK           0x0fffe0
#define TREE_ALIGN_LSHIFT         (5)
#define TREE_SUBOFFSET_MASK       0x000003
#define TREE_SUBOFFSET_IDX        0x000000
#define TREE_SUBOFFSET_EXT_ADD1   0x000001
#define TREE_SUBOFFSET_EXT_ADD2   0x000002
#define TREE_REDIR                0x200000
#define TREE_UNK                  0x100000

#define SUBOFFSET_TREE_IDX_MASK     0x00FFFFFF
#define SUBOFFSET_REDIR             0x40000000
#define SUBOFFSET_UNK_BIT29         0x20000000
#define SUBOFFSET_UNK_BIT27         0x08000000
#define SUBOFFSET_UNK_BIT26         0x04000000

#define SUBOFFSET_COMPRESSION       0x07000000
#define SUBOFFSET_DECOMPRESSED      0x00000000
#define SUBOFFSET_UND               0x01000000
#define SUBOFFSET_COMPRESSED_LZ4    0x02000000
#define SUBOFFSET_COMPRESSED_ZSTD   0x03000000


u32 __nx_applet_type = AppletType_None;

static char g_heap[0x8000];

Handle orig_main_thread;
void* orig_ctx;
void* orig_saved_lr;

FileSystemAccessor** data_arc_handles = NULL;
u32 num_data_arc_handles = 0;
arc_header arc_head;
offset4_structs off4_structs;
offset5_structs off5_structs;
offset4_header off4_header_orig;

uint64_t hash40(const void* data, size_t len)
{
    return crc32(data, len) | (len & 0xFF) << 32;
}

int hash40_compar(const void* a, const void* b)
{
    uint64_t hash1 = *(uint64_t*)a & 0xFFFFFFFFFFLL;
    uint64_t hash2 = *(uint64_t*)b & 0xFFFFFFFFFFLL;

    if (hash1 < hash2) return -1;
    else if (hash1 == hash2) return 0;
    else return 1;
}

#if 0
void print_entry_pair(entry_pair* pair)
{
    printf("%010llx %06llx (%s)\n", pair->hash, pair->meta, unhash[pair->hash].c_str());
}

void print_entry_triplet(entry_triplet* triplet)
{
    printf("%010llx %06llx %08x (%s)\n", triplet->hash, triplet->meta, triplet->meta2, unhash[triplet->hash].c_str());
}

void print_tree_entry(tree_entry* entry)
{
    printf("%06x: ", entry - off4_structs.tree_entries);
    print_entry_pair(&entry->path);
    printf("        ");
    print_entry_pair(&entry->ext);
    printf("        ");
    print_entry_pair(&entry->folder);
    printf("        ");
    print_entry_pair(&entry->file);
    printf("        suboffset index %08x flags %08x\n", entry->suboffset_index, entry->flags);
}

void print_folder_tree_entry(folder_tree_entry* entry)
{
    printf("%06x: ", entry - off5_structs.folder_tree);
    print_entry_pair(&entry->path);
    printf("        ");
    print_entry_pair(&entry->parent);
    printf("        ");
    print_entry_pair(&entry->folder);
    printf("        ");
    print_entry_pair(&entry->folder);
    printf("        %08x %08x\n", entry->idx1, entry->idx2);
}

void print_mini_tree_entry(mini_tree_entry* entry)
{
    printf("%06x: ", entry - off5_structs.tree_entries);
    print_entry_pair(&entry->path);
    printf("        ");
    print_entry_pair(&entry->folder);
    printf("        ");
    print_entry_pair(&entry->file);
    printf("        ");
    print_entry_pair(&entry->ext);
}

void print_big_hash(big_hash_entry* entry)
{
    printf("path %010llx %06llx, ", entry->path.hash, entry->path.meta);
    printf("folder %010llx %06llx, ", entry->folder.hash, entry->folder.meta);
    printf("parent %010llx %06llx, ", entry->parent.hash, entry->parent.meta);
    printf("hash4 %010llx %06llx, ", entry->hash4.hash, entry->hash4.meta);
    printf("suboffset %08x files %08x %08x %04x %04x %02x %02x %02x %02x (path %s, folder %s, parent %s, %s)\n", entry->suboffset_start, entry->num_files, entry->unk3, entry->unk4, entry->unk5, entry->unk6, entry->unk7, entry->unk8, entry->unk9, unhash[entry->path.hash].c_str(), unhash[entry->folder.hash].c_str(), unhash[entry->parent.hash].c_str(), unhash[entry->hash4.hash].c_str());
}

void print_big_file(big_file_entry* entry)
{
    printf("%016llx decomp %08x comp %08x suboffset_index %08x files %08x unk3 %08x\n", entry->offset, entry->decomp_size, entry->comp_size, entry->suboffset_index, entry->files, entry->unk3);
}

void print_suboffset(file_entry* entry)
{
    printf("%08x %08x %08x %08x\n", entry->offset, entry->comp_size, entry->decomp_size, entry->flags);
}
#endif

tree_entry* file_lookup(const char* path)
{
    uint64_t hash = hash40(path, strlen(path));
    hash_bucket bucket = off4_structs.file_lookup_buckets[(hash % off4_structs.file_lookup_buckets->num_entries) + 1];
    entry_pair* found = (entry_pair*)bsearch(&hash, &off4_structs.file_lookup[bucket.index], bucket.num_entries, sizeof(entry_pair), hash40_compar);
    
    return &off4_structs.tree_entries[found->meta];
}


void calc_offset4_structs(offset4_structs* off4, uint32_t buckets)
{
    off4->bulkfile_category_info = (entry_triplet*)&off4->ext_header[1];
    off4->bulkfile_hash_lookup = (entry_pair*)&off4->bulkfile_category_info[off4->ext_header->bgm_unk_movie_entries];
    off4->bulk_files_by_name = (entry_triplet*)&off4->bulkfile_hash_lookup[off4->ext_header->entries];
    off4->bulkfile_lookup_to_fileidx = (uint32_t*)&off4->bulk_files_by_name[off4->ext_header->entries];
    off4->file_pairs = (file_pair*)&off4->bulkfile_lookup_to_fileidx[off4->ext_header->entries_2];
    off4->weird_hashes = (entry_triplet*)&off4->file_pairs[off4->ext_header->num_files];
    off4->big_hashes = (big_hash_entry*)&off4->weird_hashes[off4->header->weird_hash_entries];
    off4->big_files = (big_file_entry*)&off4->big_hashes[off4->header->entries_big];
    off4->folder_hash_lookup = (entry_pair*)&off4->big_files[off4->header->entries_bigfiles_1 + off4->header->entries_bigfiles_2];
    off4->tree_entries = (tree_entry*)&off4->folder_hash_lookup[off4->header->folder_hash_entries];
    off4->suboffset_entries = (file_entry*)&off4->tree_entries[off4->header->tree_entries];
    off4->folder_to_big_hash = (entry_pair*)&off4->suboffset_entries[off4->header->suboffset_entries + off4->header->post_suboffset_entries];
    off4->file_lookup_buckets = (hash_bucket*)&off4->folder_to_big_hash[off4->header->entries_big];
    
    if (buckets == 0)
        buckets = off4->file_lookup_buckets->num_entries;
    
    off4->file_lookup = (entry_pair*)&off4->file_lookup_buckets[buckets+1];
    off4->numbers3 = (entry_pair*)&off4->file_lookup[off4->header->file_lookup_entries];
}

void calc_offset5_structs(offset5_structs* off5)
{
    off5->folderhash_to_foldertree = (entry_pair*)&off5->header[1];
    off5->folder_tree = (folder_tree_entry*)&off5->folderhash_to_foldertree[off5->header->folder_entries];
    off5->entries_13 = (entry_pair*)&off5->folder_tree[off5->header->folder_entries];
    off5->numbers = (uint32_t*)&off5->entries_13[off5->header->hash_entries];
    off5->tree_entries = (mini_tree_entry*)&off5->numbers[off5->header->file_entries];
}

void expand_subfiles_bighash_bigfile()
{
    offset4_structs newvals = off4_structs;
    
    uint32_t old_big, old_bigfiles_2, old_post;
    old_big = newvals.header->entries_big;
    old_bigfiles_2 = newvals.header->entries_bigfiles_2;
    old_post = newvals.header->post_suboffset_entries;
    
    newvals.header->entries_big += 1;
    newvals.header->entries_bigfiles_2 += 1;
    newvals.header->post_suboffset_entries += 5;
    
    calc_offset4_structs(&newvals, off4_structs.file_lookup_buckets->num_entries);
    
    memmove(newvals.numbers3, off4_structs.numbers3, off4_structs.header->tree_entries * sizeof(entry_pair));
    memmove(newvals.file_lookup, off4_structs.file_lookup, off4_structs.header->file_lookup_entries * sizeof(entry_pair));
    memmove(newvals.file_lookup_buckets, off4_structs.file_lookup_buckets, (off4_structs.file_lookup_buckets->num_entries+1) * sizeof(hash_bucket));
    memmove(newvals.folder_to_big_hash, off4_structs.folder_to_big_hash, off4_structs.header->entries_big * sizeof(entry_pair));
    memmove(newvals.suboffset_entries, off4_structs.suboffset_entries, (off4_structs.header->suboffset_entries + off4_structs.header->post_suboffset_entries) * sizeof(file_entry));
    memmove(newvals.tree_entries, off4_structs.tree_entries, off4_structs.header->tree_entries * sizeof(tree_entry));
    memmove(newvals.folder_hash_lookup, off4_structs.folder_hash_lookup, off4_structs.header->folder_hash_entries * sizeof(entry_pair));
    memmove(newvals.big_files, off4_structs.big_files, (off4_structs.header->entries_bigfiles_1 + off4_structs.header->entries_bigfiles_2) * sizeof(big_file_entry));
    memmove(newvals.big_hashes, off4_structs.big_hashes, off4_structs.header->entries_big * sizeof(big_hash_entry));
    memmove(newvals.weird_hashes, off4_structs.weird_hashes, off4_structs.header->weird_hash_entries * sizeof(entry_triplet));
    memmove(newvals.file_pairs, off4_structs.file_pairs, off4_structs.ext_header->entries_2 * sizeof(file_pair));
    memmove(newvals.bulkfile_lookup_to_fileidx, off4_structs.bulkfile_lookup_to_fileidx, off4_structs.ext_header->entries_2 * sizeof(uint32_t));
    memmove(newvals.bulk_files_by_name, off4_structs.bulk_files_by_name, off4_structs.ext_header->entries * sizeof(entry_triplet));
    memmove(newvals.bulkfile_hash_lookup, off4_structs.bulkfile_hash_lookup, off4_structs.ext_header->entries * sizeof(entry_pair));
    memmove(newvals.bulkfile_category_info, off4_structs.bulkfile_category_info, off4_structs.ext_header->bgm_unk_movie_entries * sizeof(entry_triplet));

    off4_structs = newvals;
    
    tree_entry* wolf = file_lookup("prebuilt:/nro/release/lua2cpp_wolf.nro");
    
    
    big_hash_entry* bighash = &off4_structs.big_hashes[wolf->path.meta];
    big_file_entry* bigfile = &off4_structs.big_files[bighash->path.meta];
    file_entry* suboffset = &off4_structs.suboffset_entries[wolf->suboffset_index];
    
    uint32_t new_suboffset_idx = off4_structs.header->suboffset_entries + old_post;
    big_hash_entry* bighash_new = &off4_structs.big_hashes[old_big];
    big_file_entry* bigfile_new = &off4_structs.big_files[off4_structs.header->entries_bigfiles_1 + old_bigfiles_2];
    file_entry* suboffset_new = &off4_structs.suboffset_entries[new_suboffset_idx];
    
   *bighash_new = *bighash;
   *bigfile_new = *bigfile;
   *suboffset_new = *suboffset;
    
    wolf->path.meta = old_big;
    bighash_new->path.meta = off4_structs.header->entries_bigfiles_1 + old_bigfiles_2;
    bighash_new->suboffset_start = new_suboffset_idx;
    bighash_new->num_files = 1;
    
    
    bigfile_new->offset += (suboffset_new->offset * 4);
    bigfile_new->decomp_size = suboffset_new->decomp_size;
    bigfile_new->comp_size = suboffset_new->comp_size;
    bigfile_new->suboffset_index = new_suboffset_idx;
    bigfile_new->files = 1;
    
    suboffset_new->offset = 0;
    wolf->suboffset_index = new_suboffset_idx;
    
    // weird stuff
    //suboffset_new->flags &= ~SUBOFFSET_COMPRESSION;
    //suboffset_new->flags |= SUBOFFSET_DECOMPRESSED;
    //suboffset_new->comp_size = suboffset_new->decomp_size;
    
    /*bigfile_new->offset = 0x08000000000000 - arc_head.offset_2;*/
    //bigfile_new->decomp_size = suboffset_new->decomp_size;
    //bigfile_new->comp_size = suboffset_new->comp_size;
}

void __libnx_init(void* ctx, Handle main_thread, void* saved_lr)
{
    extern char* fake_heap_start;
    extern char* fake_heap_end;

    fake_heap_start = &g_heap[0];
    fake_heap_end   = &g_heap[sizeof g_heap];
    
    orig_ctx = ctx;
    orig_main_thread = main_thread;
    orig_saved_lr = saved_lr;
    
    // Call constructors.
    void __libc_init_array(void);
    __libc_init_array();
}

void __attribute__((weak)) NORETURN __libnx_exit(int rc)
{
    // Call destructors.
    void __libc_fini_array(void);
    __libc_fini_array();

    SaltySD_printf("SaltySD Plugin: jumping to %p\n", orig_saved_lr);

    __nx_exit(0, orig_saved_lr);
}

void* bsearch_intercept(const void* key, const void* base, size_t num, size_t size, int (*compar)(const void*,const void*))
{
    void* ret = bsearch(key, base, num, size, compar);
    if (*(u32*)key == 0xe03bdf27 || *(u32*)key == 0xccbff852 || *(u32*)key == 0x5cce92a7 || *(u32*)key == 0x5cce92a7 || *(u32*)key == 0x9b741175)
        debug_log("bsearch(%p (%llx), %p, %u, 0x%zx, %p) -> %p\n", key, *(u64*)key & 0xFFFFFFFFFF, base, num, size, compar, ret);
    return ret;
}

uint64_t OpenFile_intercept(FileSystemAccessor** handle, char* path, uint64_t mode)
{
    uint64_t ret = _ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci(handle, path, mode);
    debug_log("SaltySD Plugin: OpenFile(%llx, \"%s\", %llx) -> %llx,%p\n", handle, path, mode, ret, *handle);
    
    //TODO: closing
    if (!strcmp(path, "rom:/data.arc"))
    {
        data_arc_handles = realloc(data_arc_handles, ++num_data_arc_handles * sizeof(FileSystemAccessor*));
        data_arc_handles[num_data_arc_handles-1] = *handle;
    }

    return ret;
}

uint64_t ReadFile_intercept(uint64_t idk1, FileSystemAccessor* idk2, uint64_t idk3, uint64_t idk4, uint64_t idk5)
{
    uint64_t ret = _ZN2nn2fs8ReadFileEPmNS0_10FileHandleElPvm(idk1, idk2, idk3, idk4, idk5);
    debug_log("SaltySD Plugin: ReadFile(%llx, %llx, %llx, %llx, %llx) -> %llx\n", idk1, idk2, idk3, idk4, idk5, ret);
    return ret;
}

uint64_t ReadFile_intercept2(FileSystemAccessor* handle, uint64_t offset, uint8_t* out, uint64_t size)
{
    if (offset == arc_head.offset_4 + sizeof(offset4_header))
    {
        size -= 0x200000; // alloc adjust
    }
    
    if (offset == 0xb7720d08)
    {
        debug_log("WOLF\nSaltySD Plugin: ReadFile2(%p, %llx, %p, %llx)\n", handle, offset, out, size);

        /*void** tp = (void**)((u8*)armGetTls() + 0x1F8);
        *tp = malloc(0x1000);
        
        FILE* f = fopen("sdmc:/SaltySD/smash/prebuilt/nro/release/lua2cpp_wolf.nro", "rb");
        if (f)
        {
            fseek(f, offset - 0xb7720d08, SEEK_SET);
            size_t read = fread(out, size, 1, f);
            fclose(f);
            
            debug_log("read %zx bytes\n", read);
        }
        else
        {
            debug_log("Failed to open sdmc:/SaltySD/smash/prebuilt/nro/release/lua2cpp_wolf.nro\n");
        }
        
        free(tp);
        return 0;*/
    }

    uint64_t ret = _ZN2nn2fs8ReadFileENS0_10FileHandleElPvm(handle, offset, out, size);
    debug_log("SaltySD Plugin: ReadFile2(%p, %llx, %p, %llx) -> %llx\n", handle, offset, out, size, ret);

    u32 index = 0;
    for (index = 0; index < num_data_arc_handles; index++)
    {
        if (handle == data_arc_handles[index]) break;
    }
    
    // Handle not data.arc
    if (index == num_data_arc_handles) return ret;
    
    if (offset == 0)
    {
        arc_head = *(arc_header*)out;
        
        debug_log("Magic: %016llx\n", arc_head.magic);
        debug_log("Offset 1: %016llx\n", arc_head.offset_1);
        debug_log("Offset 2: %016llx\n", arc_head.offset_2);
        debug_log("Offset 3: %016llx\n", arc_head.offset_3);
        debug_log("Offset 4: %016llx\n", arc_head.offset_4);
        debug_log("Offset 5: %016llx\n", arc_head.offset_5);
        debug_log("Offset 6: %016llx\n\n", arc_head.offset_6);
    }
    else if (offset == arc_head.offset_4)
    {
        off4_header_orig = *(offset4_header*)(out); 
        
        off4_structs.off4_data = (out);
        off4_structs.header = (offset4_header*)off4_structs.off4_data;
        off4_structs.header->total_size += 0x200000;
        //newvals.header->entries_big += 1;
        //newvals.header->entries_bigfiles_2 += 1;
        //newvals.header->post_suboffset_entries += 5;
    }
    else if (offset == arc_head.offset_4 + sizeof(offset4_header))
    {
        off4_structs.off4_data = (out - sizeof(offset4_header));
        off4_structs.header = (offset4_header*)off4_structs.off4_data;
        off4_structs.ext_header = (offset4_ext_header*)(off4_structs.off4_data + sizeof(offset4_header));
        
        debug_log("Offset 4 Header:\n");
        debug_log("Total size: %08x\n", off4_structs.header->total_size);
        debug_log("Big hash entries: %08x\n", off4_structs.header->entries_big);
        debug_log("Big files 1: %08x\n", off4_structs.header->entries_bigfiles_1);
        debug_log("File Tree Entries: %08x\n", off4_structs.header->tree_entries);
        
        debug_log("Suboffset entries: %08x\n", off4_structs.header->suboffset_entries);
        debug_log("File lookup entries: %08x\n", off4_structs.header->file_lookup_entries);
        debug_log("Folder hash entries: %08x\n", off4_structs.header->folder_hash_entries);
        debug_log("File Tree Entries 2: %08x\n", off4_structs.header->tree_entries_2);
        debug_log("Big files 2: %08x\n", off4_structs.header->entries_bigfiles_2);
        debug_log("Post-suboffset entries: %08x\n", off4_structs.header->post_suboffset_entries);
        debug_log("Default alloc alignment: %08x\n", off4_structs.header->alloc_alignment);
        debug_log("Unk 10: %08x\n", off4_structs.header->unk10);
        debug_log("Unk 11: %08x\n\n", off4_structs.header->unk11);
        
        debug_log("Offset 4 Extended Header:\n");
        debug_log("Hash table 1 entries: %08x\n", off4_structs.ext_header->bgm_unk_movie_entries);
        debug_log("Hash table 2/3 entries: %08x\n", off4_structs.ext_header->entries);
        debug_log("Number table entries: %08x\n", off4_structs.ext_header->entries_2);
        debug_log("Num files: %08x\n\n", off4_structs.ext_header->num_files);

        calc_offset4_structs(&off4_structs, 0);
        expand_subfiles_bighash_bigfile();
    }
    else if (offset == arc_head.offset_5 + sizeof(u64))
    {
        //TODO: this moves
        off5_structs.off5_data = (out - sizeof(u64));
        off5_structs.header = (offset5_header*)off5_structs.off5_data;
        
        debug_log("\nOffset 5 Header:\n");
        debug_log("Total size %016llx\n", off5_structs.header->total_size);
        debug_log("Folder Entries: %08x\n", off5_structs.header->folder_entries);
        debug_log("File Entries: %08x\n", off5_structs.header->file_entries);
        debug_log("Something 2: %08x\n", off5_structs.header->hash_entries);
    }

    return ret;
}

int main(int argc, char *argv[])
{
    SaltySD_printf("SaltySD Plugin: alive\n");
    
    char* ver = "Ver. %d.%d.%d";
    u64 dst_3 = SaltySDCore_findCode(ver, strlen(ver));
    if (dst_3)
    {
        SaltySD_Memcpy(dst_3, "SaltySD yeet", 13);
    }
    
    SaltySDCore_ReplaceImport("_ZN2nn2fs8ReadFileEPmNS0_10FileHandleElPvm", ReadFile_intercept);
    SaltySDCore_ReplaceImport("_ZN2nn2fs8ReadFileENS0_10FileHandleElPvm", ReadFile_intercept2);
    SaltySDCore_ReplaceImport("_ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci", OpenFile_intercept);
    SaltySDCore_ReplaceImport("bsearch", bsearch_intercept);

    __libnx_exit(0);
}

