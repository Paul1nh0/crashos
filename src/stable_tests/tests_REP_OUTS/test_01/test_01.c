// This file is part of CrashOS and is released under GPLv2 (see crashos/LICENSE.md)
// Copyright Airbus Group
/*
 * test_1.c
 *
 *  Created on: 31 mai 2016
 *      Author: anais
 */

#include <core/test.h>
#include <core/init.h>

#include <core/print.h>
#include <core/start.h>

#include <core/segmentation.h>
#include <core/interrupts.h>
#include <core/page.h>
#include <core/serial_driver.h>

// Global and external variables
extern mem_info_t mem_info;

static segment_descriptor_t         gdt[10]; // 6 (default size) + 4 (for testing)
static task_state_segment_t         tss;
static uint32_t                     user_stack[128];
static uint32_t                     *r3esp = &user_stack[127];
static gdt_reg_t __gdtr, gdtr;
static uint16_t  __cs, __ds, __ss, __tr;

static pgd_struct 					pGd;
static ptb_struct 					pTb;
static uint32_t __cr3, __cr4;

static interrupt_descriptor_t       idt[256];
static idt_reg_t __idtr, idtr;

static uint16_t test_port = PORT_COM3;
static uint8_t* DS_addr;

static int init_test_01() {
	// Save
	get_gdtr( __gdtr );
	get_idtr( __idtr );
	get_tr( __tr );
	get_cr3( __cr3 );
	get_cr4( __cr4 );
	get_ds( __ds );
	get_ss( __ss );
	get_cs( __cs );

	// Init
	init_interrupts(idt, &idtr);
	init_work_mem();
	init_segmentation(gdt, &tss, r3esp, &gdtr);
	init_paging(&pGd, &pTb, __cr4);
	init_serial_uart(test_port);

	log("end_init\n");
	return 0;
}


static int test_test_01() {
	int i;
	uint8_t *page_a, *page_b;
	uint32_t ds_limit;

	// Error cases
	if (mem_info.next_free_pg == 0 && mem_info.mem_nb_free_pg < 3) {
		printf_log("Error : Not enough physical memory available.\n");
		return 1;
	}

	// Test
	// 2 physical pages for testing
	page_a = (uint8_t *) mem_info.next_free_pg;
	page_b = page_a + PAGE_SIZE;
	mem_info.mem_nb_free_pg -= 3;
	mem_info.next_free_pg += 3*PAGE_SIZE;
	for(i = 0; i < PAGE_SIZE; i++) {
	   page_a[i] = 'A';
	   page_b[i] = 'B';
	}
	__pg_set_entry( &(pTb.ptb[0]), PG_RW|PG_USR, page_number((uint32_t)page_b) );	// Setup page table entries
	__pg_set_entry( &(pTb.ptb[1]), PG_RW|PG_USR, page_number((uint32_t)page_a) );

	// 1 data segment for testing
	get_gdtr(gdtr);
	gdtr.limit += 4*sizeof(segment_descriptor_t);
	DS_addr = (uint8_t *) 0x400ff0; // address at the end of the first page
	set_desc_addr_type_limit(gdt[6], 0, 0, DS_addr, (1*PAGE_SIZE-1)>>12); // data RO pvl=0
	set_gdtr(gdtr);

	set_ds(get_kgdt_sel(6));
	asm volatile (  "mov %%ds, %%ebx\n"
					"lsl %%ebx, %%eax \n"
					"mov %%eax, %0"
					:"=m"(ds_limit)
					:
					:"memory" );

	set_ds( get_kgdt_sel(4) );
	printf_log("ds_limit : 0x%x\n", ds_limit);

	// REP OUTS instruction
	set_ds(get_kgdt_sel(6));
	asm volatile (  "mov $0x3e8, %%edx \n" /* serial port COM 3 */
					"mov $0x0, %%esi   \n" /* address of buffer */
					"mov $0x20, %%ecx  \n" /* number of byte to read in the buffer */
					"rep outsb (%%si), %%dx  \n"
					:::);

	set_ds( get_kgdt_sel(4) );
	printf_log("rep outsb done\n");
	return 0;
}

static int fini_test_01() {
	// Restore
	disable_paging();
	set_cr3( __cr3 );
	set_cr4( __cr4 );
	set_gdtr( __gdtr );
	set_idtr( __idtr );
	//set_tr( __tr );
	set_ds( __ds );
	set_ss( __ss );
	set_cs_m( __cs );
	return 0;
}

test_t test_01 = {
	.name = "test 01: reps out on non-consecutive physical pages",
	.desc = "Test behaviour of hypervisor when 'REP OUTS COM3' "
                "instruction is using a DS mapped on non consecutive "
                "physical pages.",
	.init = init_test_01,
	.test = test_test_01,
	.fini = fini_test_01,
};


DECLARE_TEST(test_01)


