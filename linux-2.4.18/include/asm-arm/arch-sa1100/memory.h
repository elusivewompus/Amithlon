/*
 * linux/include/asm-arm/arch-sa1100/memory.h
 *
 * Copyright (C) 1999-2000 Nicolas Pitre <nico@cam.org>
 */

#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

#include <linux/config.h>

/*
 * Task size: 3GB
 */
#define TASK_SIZE	(0xc0000000UL)
#define TASK_SIZE_26	(0x04000000UL)

/*
 * This decides where the kernel will search for a free chunk of vm
 * space during mmap's.
 */
#define TASK_UNMAPPED_BASE (TASK_SIZE / 3)

/*
 * Page offset: 3GB
 */
#define PAGE_OFFSET	(0xc0000000UL)

/*
 * Physical DRAM offset is 0xc0000000 on the SA1100
 */
#define PHYS_OFFSET	(0xc0000000UL)

/*
 * We take advantage of the fact that physical and virtual address can be the
 * same.  The NUMA code is handling the large holes that might exist between
 * all memory banks.
 */
#define __virt_to_phys__is_a_macro
#define __phys_to_virt__is_a_macro
#define __virt_to_phys(x)	(x)
#define __phys_to_virt(x)	(x)

/*
 * Virtual view <-> DMA view memory address translations
 * virt_to_bus: Used to translate the virtual address to an
 *		address suitable to be passed to set_dma_addr
 * bus_to_virt: Used to convert an address for DMA operations
 *		to an address that the kernel can use.
 *
 * On the SA1100, bus addresses are equivalent to physical addresses.
 */
#define __virt_to_bus__is_a_macro
#define __bus_to_virt__is_a_macro
#define __virt_to_bus(x)	 __virt_to_phys(x)
#define __bus_to_virt(x)	 __phys_to_virt(x)

#ifdef CONFIG_DISCONTIGMEM
/*
 * Because of the wide memory address space between physical RAM banks on the 
 * SA1100, it's much convenient to use Linux's NUMA support to implement our 
 * memory map representation.  Assuming all memory nodes have equal access 
 * characteristics, we then have generic discontigous memory support.
 *
 * Of course, all this isn't mandatory for SA1100 implementations with only
 * one used memory bank.  For those, simply undefine CONFIG_DISCONTIGMEM.
 *
 * The nodes are matched with the physical memory bank addresses which are 
 * incidentally the same as virtual addresses.
 * 
 * 	node 0:  0xc0000000 - 0xc7ffffff
 * 	node 1:  0xc8000000 - 0xcfffffff
 * 	node 2:  0xd0000000 - 0xd7ffffff
 * 	node 3:  0xd8000000 - 0xdfffffff
 */

#define NR_NODES	4

/*
 * Given a kernel address, find the home node of the underlying memory.
 */
#define KVADDR_TO_NID(addr) \
		(((unsigned long)(addr) - 0xc0000000) >> 27)

/*
 * Given a physical address, convert it to a node id.
 */
#define PHYS_TO_NID(addr) KVADDR_TO_NID(__phys_to_virt(addr))

/*
 * Given a kaddr, ADDR_TO_MAPBASE finds the owning node of the memory
 * and returns the mem_map of that node.
 */
#define ADDR_TO_MAPBASE(kaddr) \
			NODE_MEM_MAP(KVADDR_TO_NID((unsigned long)(kaddr)))

/*
 * Given a kaddr, LOCAL_MEM_MAP finds the owning node of the memory
 * and returns the index corresponding to the appropriate page in the
 * node's mem_map.
 */
#define LOCAL_MAP_NR(kvaddr) \
	(((unsigned long)(kvaddr) & 0x07ffffff) >> PAGE_SHIFT)

/*
 * Given a kaddr, virt_to_page returns a pointer to the corresponding 
 * mem_map entry.
 */
#define virt_to_page(kaddr) \
	(ADDR_TO_MAPBASE(kaddr) + LOCAL_MAP_NR(kaddr))

/*
 * VALID_PAGE returns a non-zero value if given page pointer is valid.
 * This assumes all node's mem_maps are stored within the node they refer to.
 */
#define VALID_PAGE(page) \
({ unsigned int node = KVADDR_TO_NID(page); \
   ( (node < NR_NODES) && \
     ((unsigned)((page) - NODE_MEM_MAP(node)) < NODE_DATA(node)->node_size) ); \
})

#else

#define PHYS_TO_NID(addr)	(0)

#endif

#endif
