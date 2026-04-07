#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"

#include "memlayout.h"
#include "wmap.h"

// ====================================================================
// Function Declarations
// ====================================================================

int validate_input(uint addr, int length, int flags, int fd, struct file *f);
struct mmap_region *fix_addr_place_mmap(struct proc *proc, uint addr, int length, int flags);
struct mmap_region *find_addr_place_mmap(struct proc *proc, int length, int flags);
struct mmap_region *find_unused_mmap(struct proc *proc);
void init_one_mmap(struct mmap_region *mmap);
void init_mmaps(struct proc *proc);
uint get_physical_page(struct proc *p, uint va, pte_t **pte);
int count_loaded_pages(struct proc *p, uint start, uint end);
int write_to_file(struct file *f, uint va, int offset, int n_bytes);
int remove_map(uint addr);

// ====================================================================
// System Calls
// ====================================================================

/**
 * @brief System call to create a new memory mapping in the process's address space.
 *
 * @return Returns the starting address of the mapping if successful, or FAILED if an error occurs.
 */
int sys_wmap(void) {
    uint addr;
    int length, flags;
    int fd;
    struct file *f;
    // Integer arguements
    if (argint(0, (int *)&addr) < 0 || argint(1, &length) < 0 || argint(2, &flags) < 0) {
        cprintf("wmap arg ERROR: size or flags\n");
        return FAILED;
    }
    // File descriptor arguement
    if (argfd(3, &fd, &f) < 0) {
        fd = -1;
        f = 0;
    } else {
        // NOTE: without filedup(), if the file is closed before pgfault handler
        // reads the file, xv6 will panic. Close it in munmap in case of full unmapping.
        f = filedup(f);
    }

    // Validate input
    if (validate_input(addr, length, flags, fd, f) < 0) {
        cprintf("wmap ERROR: invalid input\n");
        return FAILED;
    }

    struct proc *curproc = myproc();
    struct mmap_region *mmap;

    // Check process hasn't exceeded max mappings
    if (curproc->total_mmaps >= MAX_NMMAP) {
        cprintf("wmap ERROR: too many mappings\n");
        if (f) fileclose(f);
        return FAILED;
    }

    if (flags & MAP_FIXED) {
        mmap = fix_addr_place_mmap(curproc, addr, length, flags);
    } else {
        mmap = find_addr_place_mmap(curproc, length, flags);
    }

    if (mmap == 0) {
        cprintf("wmap ERROR: could not place mapping\n");
        if (f) fileclose(f);
        return FAILED;
    }

    // Initialize the mmap region fields
    mmap->length = length;
    mmap->flags = flags;
    mmap->fd = fd;
    mmap->f = f;
    mmap->refcnt = 0;
    mmap->n_loaded_pages = 0;
    curproc->total_mmaps++;
      
    return (int)mmap->addr;
}

/**
 * @brief System call to remove an existing memory mapping from the process's address space.
 *
 * @return Returns 0 on success, or FAILED if an error occurs.
 */
int sys_wunmap(void) {
    uint addr;
    // validate input
    if (argint(0, (int *)&addr) < 0) {
        cprintf("wunmap arg ERROR: addr\n");
        return FAILED;
    }
    if (addr % PGSIZE != 0) {
        cprintf("wunmap ERROR: addr not page aligned\n");
        return FAILED;
    }

    if (remove_map(addr) < 0) {
        cprintf("wunmap ERROR: unmap failed\n");
        return FAILED;
    }

    // TODO: implement your unmap here

    return 0;
}

/**
 * @brief System call to partially remove a part of existing memory mapping from the process's address space.
 *
 * @return Returns 0 on success, or FAILED if an error occurs.
 */
int sys_wpunmap(void) {
    uint addr;
    int length;
    // validate input
    if (argint(0, (int *)&addr) < 0 || argint(1, &length) < 0) {
        cprintf("wpunmap arg ERROR: size or flags\n");
        return FAILED;
    }
    if ((addr+length) % PGSIZE != 0) {
        cprintf("wpunmap ERROR: partial unmap addr end not page aligned\n");
        return FAILED;
    }
    if (length <= 0){
        cprintf("wpunmap ERROR: partial unmap length <= 0\n");
        return FAILED;
    }
    
    if (addr % PGSIZE != 0) {
        cprintf("wpunmap ERROR: addr not page aligned\n");
        return FAILED;
    }

    struct proc *curproc = myproc();

    // Find the mmap region that contains [addr, addr+length)
    struct mmap_region *mmap = curproc->mmap_head;
    while (mmap != 0) {
        if (mmap->addr <= addr && (uint)(mmap->addr + mmap->length) >= (addr + (uint)length)) {
            break;
        }
        mmap = mmap->next;
    }
    if (mmap == 0) {
        cprintf("wpunmap ERROR: no mapping found for addr 0x%x\n", addr);
        return FAILED;
    }

    // Unmap the physical pages in [addr, addr+length)
    uint va;
    for (va = addr; va < addr + (uint)length; va += PGSIZE) {
        pte_t *pte = walkpgdir(curproc->pgdir, (void *)va, 0);
        if (pte && (*pte & PTE_P)) {
            uint pa = PTE_ADDR(*pte);
            // Write back to file if MAP_SHARED file-backed
            if (!(mmap->flags & MAP_ANONYMOUS) && (mmap->flags & MAP_SHARED) && mmap->f) {
                int offset = va - mmap->addr;
                write_to_file(mmap->f, va, offset, PGSIZE);
            }
            kfree(P2V(pa));
            *pte = 0;
            mmap->n_loaded_pages--;
        }
    }

    // Shrink or split the mapping
    if (addr == mmap->addr) {
        // Remove from the front
        mmap->addr = addr + length;
        mmap->length -= length;
    } else if (addr + (uint)length == (uint)(mmap->addr + mmap->length)) {
        // Remove from the back
        mmap->length -= length;
    } else {
        // Split: original becomes [mmap->addr, addr), new covers [addr+length, end)
        struct mmap_region *new_mmap = find_unused_mmap(curproc);
        if (new_mmap == 0) {
            cprintf("wpunmap ERROR: no unused mmap slot for split\n");
            return FAILED;
        }
        uint new_addr = addr + length;
        int new_length = (mmap->addr + mmap->length) - new_addr;
        mmap->length = addr - mmap->addr;

        new_mmap->addr = new_addr;
        new_mmap->length = new_length;
        new_mmap->flags = mmap->flags;
        new_mmap->fd = mmap->fd;
        new_mmap->f = mmap->f ? filedup(mmap->f) : 0;
        new_mmap->refcnt = mmap->refcnt;
        new_mmap->n_loaded_pages = count_loaded_pages(curproc, new_addr, new_addr + new_length);
        mmap->n_loaded_pages = count_loaded_pages(curproc, mmap->addr, mmap->addr + mmap->length);

        // Insert new_mmap into linked list after mmap (sorted by addr)
        new_mmap->next = mmap->next;
        mmap->next = new_mmap;
        curproc->total_mmaps++;
    }

    switchuvm(curproc);

    return 0;
}


/**
 * @brief System call to retrieve detailed information about all memory mappings in the process's address space.
 *
 * @return Returns 0 on success, or FAILED if an error occurs.
 */
int sys_getwmapinfo(void) {
    struct wmapinfo *info;
    if (argptr(0, (void *)&info, sizeof(*info)) < 0) {
        cprintf("getwmapinfo arg ERROR\n");
        return FAILED;
    }

    struct proc *curproc = myproc();
    info->total_mmaps = curproc->total_mmaps;

    struct mmap_region *mmap = curproc->mmap_head;
    int i = 0;
    while (mmap != 0 && i < MAX_WMMAP_INFO) {
        info->addr[i]           = (int)mmap->addr;
        info->length[i]         = mmap->length;
        info->flags[i]          = mmap->flags;
        info->fd[i]             = mmap->fd;
        info->refcnt[i]         = mmap->refcnt;
        info->n_loaded_pages[i] = mmap->n_loaded_pages;
        i++;
        mmap = mmap->next;
    }

    return 0;
}

/**
 * @brief System call to retrieve information about the current page directory.
 *
 * @return Returns 0 on success, or FAILED if an error occurs.
 */
int sys_getpgdirinfo(void) {
    struct pgdirinfo *info;
    if (argptr(0, (void *)&info, sizeof(*info)) < 0) {
        return FAILED;
    }

    struct proc *curproc = myproc();
    pde_t *pgdir = curproc->pgdir;

    uint n = 0;
    // Walk through all user virtual address space pages (0 to KERNBASE)
    uint va;
    for (va = 0; va < KERNBASE; va += PGSIZE) {
        pte_t *pte = walkpgdir(pgdir, (void *)va, 0);
        if (pte && (*pte & PTE_P) && (*pte & PTE_U)) {
            n++;
            if (n <= MAX_UPAGE_INFO) {
                info->va[n - 1] = va;
                info->pa[n - 1] = PTE_ADDR(*pte);
            }
        }
    }
    info->n_upages = n;

    return 0;
}

// ====================================================================
// Functions related to wmap
// ====================================================================

/**
 * @brief validates input parameters for memory mapping.
 *
 * @return 0 if input is valid, or a negative error code if validation fails.
 */
int validate_input(uint addr, int length, int flags, int fd, struct file *f) {
    // length must be > 0
    if (length <= 0) {
        cprintf("validate_input ERROR: length <= 0\n");
        return -1;
    }
    // Must specify exactly one of MAP_SHARED or MAP_PRIVATE
    int shared = (flags & MAP_SHARED) ? 1 : 0;
    int priv   = (flags & MAP_PRIVATE) ? 1 : 0;
    if (shared == priv) {
        cprintf("validate_input ERROR: must specify exactly one of MAP_SHARED or MAP_PRIVATE\n");
        return -1;
    }
    // File-backed: need a valid file
    if (!(flags & MAP_ANONYMOUS) && f == 0) {
        cprintf("validate_input ERROR: file-backed mapping requires valid fd\n");
        return -1;
    }
    // MAP_FIXED: addr must be page-aligned and in wmap range
    if (flags & MAP_FIXED) {
        if (addr % PGSIZE != 0) {
            cprintf("validate_input ERROR: MAP_FIXED addr not page aligned\n");
            return -1;
        }
        if (addr < MMAPBASE || addr >= KERNBASE) {
            cprintf("validate_input ERROR: MAP_FIXED addr out of wmap range\n");
            return -1;
        }
        if (addr + (uint)length > KERNBASE) {
            cprintf("validate_input ERROR: MAP_FIXED mapping exceeds KERNBASE\n");
            return -1;
        }
    }
    return 0;
}

/**
 * @brief searches for an unused mmap structure in the process's mmap list
 * and returns a pointer to it.
 *
 * @return Pointer to the unused mmap structure if found, or NULL if not found.
 */
struct mmap_region *find_unused_mmap(struct proc *proc) {
    for (int i = 0; i < MAX_NMMAP; i++) {
        if (proc->mmaps[i].addr == (uint)-1) {
            return &proc->mmaps[i];
        }
    }
    return 0;
}

/**
 * @brief places an mmap structure in the process's mmap list at a fixed address
 * if the address is available and does not overlap with existing mappings.
 *
 * @param addr The fixed address for the mapping.
 * @param length The length of the mapping.
 * @return Pointer to the placed mmap structure if successful, or NULL if failed.
 */
struct mmap_region *fix_addr_place_mmap(struct proc *proc, uint addr, int length, int flags) {
    uint end = addr + (uint)length;

    // Check overlap with existing mappings
    struct mmap_region *cur = proc->mmap_head;
    while (cur != 0) {
        uint cend = cur->addr + (uint)cur->length;
        if (addr < cend && end > cur->addr) {
            cprintf("fix_addr_place_mmap ERROR: overlap with existing mapping\n");
            return 0;
        }
        cur = cur->next;
    }

    struct mmap_region *m = find_unused_mmap(proc);
    if (m == 0) return 0;

    m->addr = addr;

    // Insert into sorted linked list
    if (proc->mmap_head == 0 || proc->mmap_head->addr > addr) {
        m->next = proc->mmap_head;
        proc->mmap_head = m;
    } else {
        struct mmap_region *prev = proc->mmap_head;
        while (prev->next != 0 && prev->next->addr < addr) {
            prev = prev->next;
        }
        m->next = prev->next;
        prev->next = m;
    }

    return m;
}

/**
 * @brief finds a suitable address for the mapping and places an mmap structure
 * in the process's mmap list.
 *
 * @param length The length of the mapping.
 * @return Pointer to the placed mmap structure if successful, or NULL if failed.
 */
struct mmap_region *find_addr_place_mmap(struct proc *proc, int length, int flags) {
    uint start = MMAPBASE;
    struct mmap_region *cur = proc->mmap_head;

    // Walk through sorted list and find a gap
    while (cur != 0) {
        if (cur->addr >= start + (uint)length) {
            break; // gap found before cur
        }
        // Move start past this mapping
        uint cend = cur->addr + (uint)cur->length;
        if (cend > start) start = cend;
        // Round up to page
        if (start % PGSIZE != 0) start = PGROUNDUP(start);
        cur = cur->next;
    }

    if (start + (uint)length > KERNBASE) {
        cprintf("find_addr_place_mmap ERROR: no space\n");
        return 0;
    }

    return fix_addr_place_mmap(proc, start, length, flags);
}

// ====================================================================
// Functions related to unmapping
// ====================================================================

/**
 * @brief removes a memory mapping at a given virtual address from the process's
 * address space. It deallocates physical pages if necessary, decrements reference counts,
 * and updates the process's memory mapping list.
 *
 * @param addr The virtual address of the memory mapping to remove.
 * @return 0 if successful, or a negative error code if an error occurs.
 */
int remove_map(uint addr) {
    struct proc *curproc = myproc();

    // Find the mapping
    struct mmap_region *prev = 0;
    struct mmap_region *mmap = curproc->mmap_head;
    while (mmap != 0) {
        if (mmap->addr == addr) break;
        prev = mmap;
        mmap = mmap->next;
    }
    if (mmap == 0) {
        cprintf("remove_map ERROR: no mapping at 0x%x\n", addr);
        return -1;
    }

    // If shared and refcnt > 0, a child is using it — just decrement
    // (parent always exits after child per spec, so we handle below)

    // Unmap physical pages
    uint va;
    for (va = mmap->addr; va < (uint)(mmap->addr + mmap->length); va += PGSIZE) {
        pte_t *pte = walkpgdir(curproc->pgdir, (void *)va, 0);
        if (pte && (*pte & PTE_P)) {
            uint pa = PTE_ADDR(*pte);
            // Write back to file if MAP_SHARED file-backed
            if (!(mmap->flags & MAP_ANONYMOUS) && (mmap->flags & MAP_SHARED) && mmap->f) {
                int offset = va - mmap->addr;
                write_to_file(mmap->f, va, offset, PGSIZE);
            }
            // Only free if refcnt == 0 (no children sharing)
            if (mmap->refcnt == 0) {
                kfree(P2V(pa));
            }
            *pte = 0;
        }
    }

    // Close file if file-backed and refcnt == 0
    if (mmap->f && mmap->refcnt == 0) {
        fileclose(mmap->f);
        mmap->f = 0;
    }

    // Remove from linked list
    if (prev == 0) {
        curproc->mmap_head = mmap->next;
    } else {
        prev->next = mmap->next;
    }

    curproc->total_mmaps--;

    // Reset this slot
    init_one_mmap(mmap);

    switchuvm(curproc);
    return 0;
}

/**
 * @brief writes the contents of a memory region to a file at the specified offset.
 *
 * @return The number of bytes written if successful, or -1 if an error occurs.
 */
int write_to_file(struct file *f, uint va, int offset, int n_bytes) {
    int r;
    int max = ((MAXOPBLOCKS - 1 - 1 - 2) / 2) * 512;
    int i = 0;
    while (i < n_bytes) {
        int n1 = n_bytes - i;
        if (n1 > max)
            n1 = max;
        begin_op();
        ilock(f->ip);
        if ((r = writei(f->ip, (char *)va + i, offset, n1)) > 0)
            offset += r;
        iunlock(f->ip);
        end_op();

        if (r < 0)
            break;
        if (r != n1)
            panic("wmap: short filewrite");
        i += r;
    }
    r = (i == n_bytes ? n_bytes : -1);
    return r;
}

// ====================================================================
// Common Functions
// ====================================================================

/**
 * @brief resets the fields of an mmap_region struct to their default values.
 *
 * @param mmap Pointer to the mmap_region struct to reset.
 */
void init_one_mmap(struct mmap_region *mmap) {
    mmap->addr = -1;
    mmap->length = -1;
    mmap->flags = -1;
    mmap->fd = -1;
    mmap->f = 0;
    mmap->refcnt = 0;
    mmap->n_loaded_pages = 0;
    mmap->next = 0;
}

/**
 * @brief initializes memory maps for a process by resetting its mmap structures.
 *
 * @param proc Pointer to the process structure to initialize.
 */
void init_mmaps(struct proc *proc) {
    for (int i = 0; i < MAX_NMMAP; i++) {
        init_one_mmap(&proc->mmaps[i]);
    }
    proc->mmap_head = 0;
    proc->total_mmaps = 0;
}

/**
 * @brief retrieves the physical address of a page from its virtual address in the process's address space.
 *
 * @param va Virtual address of the page.
 * @param pte Pointer to the page table entry. this will be updated with the address of the page table entry.
 * @return Physical address of the page if found, or 0 if not found.
 */
uint get_physical_page(struct proc *p, uint va, pte_t **pte) {
    pte_t *pt = walkpgdir(p->pgdir, (void *)va, 0);
    if (pt == 0 || !(*pt & PTE_P)) return 0;
    if (pte) *pte = pt;
    return PTE_ADDR(*pt);
}

/**
 * @brief find the loaded pages number of a virtual address range
 *
 * @param start Virtual address start.
 * @param end Virtual address end.
 * @return number of loaded pages.
 */
int count_loaded_pages(struct proc *p, uint start, uint end) {
    int count = 0;
    uint va;
    for (va = start; va < end; va += PGSIZE) {
        pte_t *pte = walkpgdir(p->pgdir, (void *)va, 0);
        if (pte && (*pte & PTE_P)) count++;
    }
    return count;
}

// ====================================================================
// Functions related to demand allocation
// ====================================================================

/**
 * @brief This function is called when a page fault occurs. It allocates a physical page,
 * maps it to the corresponding virtual address, and reads content from a file if necessary.
 *
 * @param pgflt_vaddr The virtual address that caused the page fault.
 * @return Returns 0 on success, or FAILED if an error occurs.
 */
int handle_page_fault(uint pgflt_vaddr) {
    struct proc *curproc = myproc();
    uint va = PGROUNDDOWN(pgflt_vaddr);

    // Find the mmap region containing this address
    struct mmap_region *mmap = curproc->mmap_head;
    while (mmap != 0) {
        if (va >= mmap->addr && va < (uint)(mmap->addr + mmap->length)) {
            break;
        }
        mmap = mmap->next;
    }
    if (mmap == 0) {
        return FAILED; // Not a mapped region
    }

    // Allocate a physical page
    char *mem = kalloc();
    if (mem == 0) {
        cprintf("handle_page_fault ERROR: kalloc failed\n");
        return FAILED;
    }
    memset(mem, 0, PGSIZE);

    // Map the page
    if (mappages(curproc->pgdir, (void *)va, PGSIZE, V2P(mem), PTE_W | PTE_U) < 0) {
        kfree(mem);
        cprintf("handle_page_fault ERROR: mappages failed\n");
        return FAILED;
    }

    // If file-backed, read the file content into the page
    if (!(mmap->flags & MAP_ANONYMOUS) && mmap->f != 0) {
        int offset = va - mmap->addr;
        int r;
        ilock(mmap->f->ip);
        r = readi(mmap->f->ip, (char *)va, offset, PGSIZE);
        iunlock(mmap->f->ip);
        if (r < 0) {
            cprintf("handle_page_fault ERROR: readi failed\n");
            return FAILED;
        }
    }

    mmap->n_loaded_pages++;
    return 0;
}

// ====================================================================
// Functions related to fork
// ====================================================================

/**
 * @brief copies memory mappings from the parent process to the child process.
 * It also copies the physical pages if the memory mapping is MAP_PRIVATE.
 * It also increments the reference count of the memory mapping if it is MAP_SHARED.
 *
 * @return Returns 0 on success, or FAILED if an error occurs.
 */
int copy_maps(struct proc *parent, struct proc *child) {
    struct mmap_region *pmmap = parent->mmap_head;
    struct mmap_region *child_prev = 0;

    while (pmmap != 0) {
        // Find a free slot in child
        struct mmap_region *cmmap = find_unused_mmap(child);
        if (cmmap == 0) {
            cprintf("copy_maps ERROR: no free mmap slot in child\n");
            return FAILED;
        }

        cmmap->addr   = pmmap->addr;
        cmmap->length = pmmap->length;
        cmmap->flags  = pmmap->flags;
        cmmap->fd     = pmmap->fd;
        cmmap->f      = pmmap->f ? filedup(pmmap->f) : 0;
        cmmap->refcnt = 0;
        cmmap->n_loaded_pages = 0;
        cmmap->next   = 0;

        if (pmmap->flags & MAP_SHARED) {
            // Share physical pages: map same physical pages in child
            uint va;
            for (va = pmmap->addr; va < (uint)(pmmap->addr + pmmap->length); va += PGSIZE) {
                pte_t *pte = walkpgdir(parent->pgdir, (void *)va, 0);
                if (pte && (*pte & PTE_P)) {
                    uint pa = PTE_ADDR(*pte);
                    if (mappages(child->pgdir, (void *)va, PGSIZE, pa, PTE_W | PTE_U) < 0) {
                        cprintf("copy_maps ERROR: mappages failed (shared)\n");
                        return FAILED;
                    }
                    cmmap->n_loaded_pages++;
                }
            }
            cmmap->refcnt = 1;
        } else {
            // MAP_PRIVATE: copy physical pages
            uint va;
            for (va = pmmap->addr; va < (uint)(pmmap->addr + pmmap->length); va += PGSIZE) {
                pte_t *pte = walkpgdir(parent->pgdir, (void *)va, 0);
                if (pte && (*pte & PTE_P)) {
                    uint pa = PTE_ADDR(*pte);
                    char *mem = kalloc();
                    if (mem == 0) {
                        cprintf("copy_maps ERROR: kalloc failed (private)\n");
                        return FAILED;
                    }
                    memmove(mem, P2V(pa), PGSIZE);
                    if (mappages(child->pgdir, (void *)va, PGSIZE, V2P(mem), PTE_W | PTE_U) < 0) {
                        kfree(mem);
                        cprintf("copy_maps ERROR: mappages failed (private)\n");
                        return FAILED;
                    }
                    cmmap->n_loaded_pages++;
                }
            }
        }

        // Append to child's linked list (parent list is sorted, so child's will be too)
        if (child->mmap_head == 0) {
            child->mmap_head = cmmap;
            child_prev = cmmap;
        } else {
            child_prev->next = cmmap;
            child_prev = cmmap;
        }
        child->total_mmaps++;

        pmmap = pmmap->next;
    }

    return 0;
}

// ====================================================================
// Functions related to exit
// ====================================================================

/**
 * @brief deletes memory mappings of a process during its exit.
 * It removes mappings with zero reference count and resets the mmap_region struct.
 *
 * @return Returns 0 on success, or FAILED if an error occurs.
 */
int delete_mmaps(struct proc *curproc) {
    struct mmap_region *mmap = curproc->mmap_head;
    while (mmap != 0) {
        struct mmap_region *next = mmap->next;

        // Unmap physical pages
        uint va;
        for (va = mmap->addr; va < (uint)(mmap->addr + mmap->length); va += PGSIZE) {
            pte_t *pte = walkpgdir(curproc->pgdir, (void *)va, 0);
            if (pte && (*pte & PTE_P)) {
                uint pa = PTE_ADDR(*pte);
                // Write back only if MAP_SHARED file-backed AND no children sharing
                if (!(mmap->flags & MAP_ANONYMOUS) && (mmap->flags & MAP_SHARED)
                        && mmap->f && mmap->refcnt == 0) {
                    int offset = va - mmap->addr;
                    write_to_file(mmap->f, va, offset, PGSIZE);
                }
                if (mmap->refcnt == 0) {
                    kfree(P2V(pa));
                }
                *pte = 0;
            }
        }

        if ((mmap->flags & MAP_SHARED) && mmap->refcnt == 1 && curproc->parent != 0) {
            struct mmap_region *pmmap = curproc->parent->mmap_head;
            while (pmmap != 0) {
                if (pmmap->addr == mmap->addr) {
                    if (pmmap->refcnt > 0)
                        pmmap->refcnt--;  
                    break;
                }
                pmmap = pmmap->next;
            }
        }

        if (mmap->f && mmap->refcnt == 0) {
            fileclose(mmap->f);
            mmap->f = 0;
        }

        init_one_mmap(mmap);
        mmap = next;
    }
    curproc->mmap_head = 0;
    curproc->total_mmaps = 0;
    return 0;
}