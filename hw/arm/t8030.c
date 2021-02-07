/*
 * iPhone 11 - T8030
 *
 * Copyright (c) 2019 Johnathan Afek <jonyafek@me.com>
 * Copyright (c) 2021 Nguyen Hoang Trung (TrungNguyen1909)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "hw/arm/boot.h"
#include "exec/address-spaces.h"
#include "hw/misc/unimp.h"
#include "sysemu/sysemu.h"
#include "sysemu/reset.h"
#include "qemu/error-report.h"
#include "hw/platform-bus.h"

#include "hw/arm/t8030.h"

#include "hw/arm/exynos4210.h"

#define T8030_SECURE_RAM_SIZE (0x100000)
#define T8030_PHYS_BASE (0x40000000)
#define CPU_IMPL_REG_BASE (0x210050000)
#define CPM_IMPL_REG_BASE (0x210e40000)
#define T8030_MAX_DEVICETREE_SIZE (0x40000)
#define NOP_INST (0xd503201f)
#define MOV_W0_01_INST (0x52800020)
#define MOV_X13_0_INST (0xd280000d)
#define RET_INST (0xd65f03c0)
#define RETAB_INST (0xd65f0fff)

#define T8030_CPREG_FUNCS(name)                                                    \
    static uint64_t T8030_cpreg_read_##name(CPUARMState *env,                      \
                                            const ARMCPRegInfo *ri)                \
    {                                                                              \
        T8030CPU *tcpu = (T8030CPU *)ri->opaque;                  \
        return tcpu->T8030_CPREG_VAR_NAME(name);                                    \
    }                                                                              \
    static void T8030_cpreg_write_##name(CPUARMState *env, const ARMCPRegInfo *ri, \
                                         uint64_t value)                           \
    {                                                                              \
        T8030CPU *tcpu = (T8030CPU *)ri->opaque;                  \
        tcpu->T8030_CPREG_VAR_NAME(name) = value;                                   \
    }

#define T8030_CPREG_DEF(p_name, p_op0, p_op1, p_crn, p_crm, p_op2, p_access) \
    {                                                                        \
        .cp = CP_REG_ARM64_SYSREG_CP,                                        \
        .name = #p_name, .opc0 = p_op0, .crn = p_crn, .crm = p_crm,          \
        .opc1 = p_op1, .opc2 = p_op2, .access = p_access, .type = ARM_CP_IO, \
        .state = ARM_CP_STATE_AA64, .readfn = T8030_cpreg_read_##p_name,     \
        .writefn = T8030_cpreg_write_##p_name                                \
    }

T8030_CPREG_FUNCS(ARM64_REG_HID11)
T8030_CPREG_FUNCS(ARM64_REG_HID3)
T8030_CPREG_FUNCS(ARM64_REG_HID5)
T8030_CPREG_FUNCS(ARM64_REG_HID4)
T8030_CPREG_FUNCS(ARM64_REG_HID8)
T8030_CPREG_FUNCS(ARM64_REG_HID7)
T8030_CPREG_FUNCS(ARM64_REG_LSU_ERR_STS)
T8030_CPREG_FUNCS(PMC0)
T8030_CPREG_FUNCS(PMC1)
T8030_CPREG_FUNCS(PMCR1)
T8030_CPREG_FUNCS(PMSR)
T8030_CPREG_FUNCS(L2ACTLR_EL1)
T8030_CPREG_FUNCS(ARM64_REG_APCTL_EL1)
T8030_CPREG_FUNCS(ARM64_REG_KERNELKEYLO_EL1)
T8030_CPREG_FUNCS(ARM64_REG_KERNELKEYHI_EL1)
T8030_CPREG_FUNCS(ARM64_REG_EHID4)
T8030_CPREG_FUNCS(S3_4_c15_c0_5)
T8030_CPREG_FUNCS(S3_4_c15_c1_3)
T8030_CPREG_FUNCS(S3_4_c15_c1_4)
T8030_CPREG_FUNCS(ARM64_REG_IPI_SR)
T8030_CPREG_FUNCS(ARM64_REG_CYC_OVRD)
T8030_CPREG_FUNCS(ARM64_REG_ACC_CFG)
T8030_CPREG_FUNCS(ARM64_REG_VMSA_LOCK_EL1)
T8030_CPREG_FUNCS(S3_6_c15_c1_0)
T8030_CPREG_FUNCS(S3_6_c15_c1_1)
T8030_CPREG_FUNCS(S3_6_c15_c1_2)
T8030_CPREG_FUNCS(S3_6_c15_c1_5)
T8030_CPREG_FUNCS(S3_6_c15_c1_6)
T8030_CPREG_FUNCS(S3_6_c15_c1_7)
T8030_CPREG_FUNCS(S3_6_c15_c3_0)
T8030_CPREG_FUNCS(S3_6_c15_c3_1)
T8030_CPREG_FUNCS(S3_6_c15_c8_0)
T8030_CPREG_FUNCS(S3_6_c15_c8_1)
T8030_CPREG_FUNCS(S3_6_c15_c8_2)
T8030_CPREG_FUNCS(S3_6_c15_c8_3)
T8030_CPREG_FUNCS(S3_6_c15_c9_1)
T8030_CPREG_FUNCS(UPMPCM)
T8030_CPREG_FUNCS(UPMCR0)
T8030_CPREG_FUNCS(UPMSR)

// This is the same as the array for kvm, but without
// the L2ACTLR_EL1, which is already defined in TCG.
// Duplicating this list isn't a perfect solution,
// but it's quick and reliable.
static const ARMCPRegInfo T8030_cp_reginfo_tcg[] = {
    // Apple-specific registers
    T8030_CPREG_DEF(ARM64_REG_HID11, 3, 0, 15, 13, 0, PL1_RW),
    T8030_CPREG_DEF(ARM64_REG_HID3, 3, 0, 15, 3, 0, PL1_RW),
    T8030_CPREG_DEF(ARM64_REG_HID5, 3, 0, 15, 5, 0, PL1_RW),
    T8030_CPREG_DEF(ARM64_REG_HID4, 3, 0, 15, 4, 0, PL1_RW),
    T8030_CPREG_DEF(ARM64_REG_EHID4, 3, 0, 15, 4, 1, PL1_RW),
    T8030_CPREG_DEF(ARM64_REG_HID8, 3, 0, 15, 8, 0, PL1_RW),
    T8030_CPREG_DEF(ARM64_REG_HID7, 3, 0, 15, 7, 0, PL1_RW),
    T8030_CPREG_DEF(ARM64_REG_LSU_ERR_STS, 3, 3, 15, 0, 0, PL1_RW),
    T8030_CPREG_DEF(PMC0, 3, 2, 15, 0, 0, PL1_RW),
    T8030_CPREG_DEF(PMC1, 3, 2, 15, 1, 0, PL1_RW),
    T8030_CPREG_DEF(PMCR1, 3, 1, 15, 1, 0, PL1_RW),
    T8030_CPREG_DEF(PMSR, 3, 1, 15, 13, 0, PL1_RW),
    T8030_CPREG_DEF(ARM64_REG_APCTL_EL1, 3, 4, 15, 0, 4, PL1_RW),
    T8030_CPREG_DEF(ARM64_REG_KERNELKEYLO_EL1, 3, 4, 15, 1, 0, PL1_RW),
    T8030_CPREG_DEF(ARM64_REG_KERNELKEYHI_EL1, 3, 4, 15, 1, 1, PL1_RW),
    T8030_CPREG_DEF(S3_4_c15_c0_5, 3, 4, 15, 0, 5, PL1_RW),
    T8030_CPREG_DEF(S3_4_c15_c1_3, 3, 4, 15, 1, 3, PL1_RW),
    T8030_CPREG_DEF(S3_4_c15_c1_4, 3, 4, 15, 1, 4, PL1_RW),
    T8030_CPREG_DEF(ARM64_REG_IPI_SR, 3, 5, 15, 1, 1, PL1_RW),
    T8030_CPREG_DEF(ARM64_REG_CYC_OVRD, 3, 5, 15, 5, 0, PL1_RW),
    T8030_CPREG_DEF(ARM64_REG_ACC_CFG, 3, 5, 15, 4, 0, PL1_RW),
    T8030_CPREG_DEF(ARM64_REG_VMSA_LOCK_EL1, 3, 4, 15, 1, 2, PL1_RW),
    T8030_CPREG_DEF(S3_6_c15_c1_0, 3, 6, 15, 1, 0, PL1_RW),
    T8030_CPREG_DEF(S3_6_c15_c1_1, 3, 6, 15, 1, 1, PL1_RW),
    T8030_CPREG_DEF(S3_6_c15_c1_2, 3, 6, 15, 1, 2, PL1_RW),
    T8030_CPREG_DEF(S3_6_c15_c1_5, 3, 6, 15, 1, 5, PL1_RW),
    T8030_CPREG_DEF(S3_6_c15_c1_6, 3, 6, 15, 1, 6, PL1_RW),
    T8030_CPREG_DEF(S3_6_c15_c1_7, 3, 6, 15, 1, 7, PL1_RW),
    T8030_CPREG_DEF(S3_6_c15_c3_0, 3, 6, 15, 3, 0, PL1_RW),
    T8030_CPREG_DEF(S3_6_c15_c3_1, 3, 6, 15, 3, 1, PL1_RW),
    T8030_CPREG_DEF(S3_6_c15_c8_0, 3, 6, 15, 8, 0, PL1_RW),
    T8030_CPREG_DEF(S3_6_c15_c8_1, 3, 6, 15, 8, 1, PL1_RW),
    T8030_CPREG_DEF(S3_6_c15_c8_2, 3, 6, 15, 8, 2, PL1_RW),
    T8030_CPREG_DEF(S3_6_c15_c8_3, 3, 6, 15, 8, 3, PL1_RW),
    T8030_CPREG_DEF(S3_6_c15_c9_1, 3, 6, 15, 9, 1, PL1_RW),
    T8030_CPREG_DEF(UPMPCM, 3, 7, 15, 5, 4, PL1_RW),
    T8030_CPREG_DEF(UPMCR0, 3, 7, 15, 0, 4, PL1_RW),
    T8030_CPREG_DEF(UPMSR, 3, 7, 15, 6, 4, PL1_RW),

    REGINFO_SENTINEL,
};

static uint32_t g_nop_inst = NOP_INST;
static uint32_t g_mov_w0_01_inst = MOV_W0_01_INST;
static uint32_t g_mov_x13_0_inst = MOV_X13_0_INST;
static uint32_t g_ret_inst = RET_INST;
static uint32_t g_retab_inst = RETAB_INST;

static void T8030_add_cpregs(T8030CPU* tcpu)
{
    ARMCPU *cpu = tcpu->cpu;

    tcpu->T8030_CPREG_VAR_NAME(ARM64_REG_HID11) = 0;
    tcpu->T8030_CPREG_VAR_NAME(ARM64_REG_HID3) = 0;
    tcpu->T8030_CPREG_VAR_NAME(ARM64_REG_HID5) = 0;
    tcpu->T8030_CPREG_VAR_NAME(ARM64_REG_HID8) = 0;
    tcpu->T8030_CPREG_VAR_NAME(ARM64_REG_HID7) = 0;
    tcpu->T8030_CPREG_VAR_NAME(ARM64_REG_LSU_ERR_STS) = 0;
    tcpu->T8030_CPREG_VAR_NAME(PMC0) = 0;
    tcpu->T8030_CPREG_VAR_NAME(PMC1) = 0;
    tcpu->T8030_CPREG_VAR_NAME(PMCR1) = 0;
    tcpu->T8030_CPREG_VAR_NAME(PMSR) = 0;
    tcpu->T8030_CPREG_VAR_NAME(L2ACTLR_EL1) = 0;
    tcpu->T8030_CPREG_VAR_NAME(ARM64_REG_APCTL_EL1) = 2;
    tcpu->T8030_CPREG_VAR_NAME(ARM64_REG_KERNELKEYLO_EL1) = 0;
    tcpu->T8030_CPREG_VAR_NAME(ARM64_REG_KERNELKEYHI_EL1) = 0;
    define_arm_cp_regs_with_opaque(cpu, T8030_cp_reginfo_tcg, tcpu);
}

static void T8030_create_s3c_uart(const T8030MachineState *tms, Chardev *chr)
{
    qemu_irq irq;
    DeviceState *d;
    SysBusDevice *s;
    hwaddr base;
    //first fetch the uart mmio address
    DTBNode *child = get_dtb_child_node_by_name(tms->device_tree, "arm-io");
    assert(child != NULL);
    child = get_dtb_child_node_by_name(child, "uart0");
    assert(child != NULL);
    //make sure this node has the boot-console prop
    DTBProp* prop = get_dtb_prop(child, "boot-console");
    assert(prop != NULL);
    prop = get_dtb_prop(child, "reg");
    assert(prop != NULL);
    hwaddr *uart_offset = (hwaddr *)prop->value;
    base = tms->soc_base_pa + uart_offset[0];

    //hack for now. create a device that is not used just to have a dummy
    //unused interrupt
    d = qdev_new(TYPE_PLATFORM_BUS_DEVICE);
    s = SYS_BUS_DEVICE(d);
    sysbus_init_irq(s, &irq);
    //pass a dummy irq as we don't need nor want interrupts for this UART
    DeviceState *dev = exynos4210_uart_create(base, 256, 0, chr, irq);
    assert(dev!=NULL);
}

static void T8030_patch_kernel(AddressSpace *nsas)
{
    //KTRR
    //rorgn_stash_range
    address_space_rw(nsas, vtop_static(0xFFFFFFF007B4A53C),
                     MEMTXATTRS_UNSPECIFIED, (uint8_t *)&g_ret_inst,
                     sizeof(g_ret_inst), 1);
    //rorgn_lockdown
    address_space_rw(nsas, vtop_static(0xFFFFFFF007B4AECC),
                     MEMTXATTRS_UNSPECIFIED, (uint8_t *)&g_ret_inst,
                     sizeof(g_ret_inst), 1);
    //gxf_enable
    address_space_rw(nsas, vtop_static(0xFFFFFFF00811CE98),
                     MEMTXATTRS_UNSPECIFIED, (uint8_t *)&g_nop_inst,
                     sizeof(g_nop_inst), 1);
    //pmap_ppl_locked_down = 1
    address_space_rw(nsas, vtop_static(0xFFFFFFF007B5A5A8),
                     MEMTXATTRS_UNSPECIFIED, (uint8_t *)&g_nop_inst,
                     sizeof(g_nop_inst), 1);
}

static void T8030_memory_setup(MachineState *machine)
{
    uint64_t used_ram_for_blobs = 0;
    hwaddr kernel_low;
    hwaddr kernel_high;
    hwaddr virt_base;
    hwaddr dtb_pa;
    hwaddr dtb_va;
    uint64_t dtb_size;
    hwaddr kbootargs_pa;
    hwaddr top_of_kernel_data_pa;
    hwaddr mem_size;
    hwaddr remaining_mem_size;
    hwaddr allocated_ram_pa;
    hwaddr phys_ptr;
    hwaddr phys_pc;
    hwaddr ramfb_pa = 0;
    video_boot_args v_bootargs = {0};
    T8030MachineState *tms = T8030_MACHINE(machine);
    MemoryRegion* sysmem = tms->sysmem;
    AddressSpace* nsas = tms->cpus[0].nsas;

    //setup the memory layout:

    //At the beginning of the non-secure ram we have the raw kernel file.
    //After that we have the static trust cache.
    //After that we have all the kernel sections.
    //After that we have ramdosk
    //After that we have the device tree
    //After that we have the kernel boot args
    //After that we have the rest of the RAM

    macho_file_highest_lowest_base(tms->kernel_filename, T8030_PHYS_BASE,
                                   &virt_base, &kernel_low, &kernel_high);

    g_virt_base = virt_base;
    g_phys_base = T8030_PHYS_BASE;
    phys_ptr = T8030_PHYS_BASE;
    fprintf(stderr, "g_virt_base: 0x" TARGET_FMT_lx "\ng_phys_base: 0x" TARGET_FMT_lx "\n", g_virt_base, g_phys_base);
    fprintf(stderr, "kernel_low: 0x" TARGET_FMT_lx "\nkernel_high: 0x" TARGET_FMT_lx "\n", kernel_low, kernel_high);

    // //now account for the trustcache
    phys_ptr += align_64k_high(0x2000000);
    hwaddr trustcache_pa = phys_ptr;
    hwaddr trustcache_size = 0;
    macho_load_raw_file("static_tc", nsas, sysmem,
                        "trustcache.T8030", trustcache_pa,
                        &trustcache_size);
    fprintf(stderr, "trustcache_addr: %p\ntrustcache_size: 0x%lx\n", trustcache_pa, trustcache_size);
    phys_ptr += align_64k_high(trustcache_size);

    //now account for the loaded kernel
    arm_load_macho(tms->kernel_filename, nsas, sysmem, "kernel.T8030",
                   T8030_PHYS_BASE, virt_base, kernel_low,
                   kernel_high, &phys_pc);
    tms->kpc_pa = phys_pc;
    used_ram_for_blobs += (align_64k_high(kernel_high) - kernel_low);

    T8030_patch_kernel(nsas);

    phys_ptr = align_64k_high(vtop_static(kernel_high));

    //now account for device tree
    dtb_pa = phys_ptr;

    dtb_va = ptov_static(phys_ptr);
    phys_ptr += align_64k_high(T8030_MAX_DEVICETREE_SIZE);
    used_ram_for_blobs += align_64k_high(T8030_MAX_DEVICETREE_SIZE);
    //now account for the ramdisk
    tms->ramdisk_file_dev.pa = 0;
    hwaddr ramdisk_size = 0;
    if (0 != tms->ramdisk_filename[0])
    {
        tms->ramdisk_file_dev.pa = phys_ptr;
        macho_map_raw_file(tms->ramdisk_filename, nsas, sysmem,
                           "ramdisk_raw_file.T8030", tms->ramdisk_file_dev.pa,
                           &tms->ramdisk_file_dev.size);
        tms->ramdisk_file_dev.size = align_64k_high(tms->ramdisk_file_dev.size);
        ramdisk_size = tms->ramdisk_file_dev.size;
        phys_ptr += tms->ramdisk_file_dev.size;
        fprintf(stderr, "ramdisk addr: 0x" TARGET_FMT_lx "\n", tms->ramdisk_file_dev.pa);
        fprintf(stderr, "ramdisk size: 0x" TARGET_FMT_lx "\n", tms->ramdisk_file_dev.size);
    }
    
    //now account for kernel boot args
    used_ram_for_blobs += align_64k_high(sizeof(struct xnu_arm64_boot_args));
    kbootargs_pa = phys_ptr;
    tms->kbootargs_pa = kbootargs_pa;
    phys_ptr += align_64k_high(sizeof(struct xnu_arm64_boot_args));
    tms->extra_data_pa = phys_ptr;
    allocated_ram_pa = phys_ptr;
    
    if (tms->use_ramfb)
    {
        ramfb_pa = ((hwaddr) & ((AllocatedData *)tms->extra_data_pa)->ramfb[0]);
        xnu_define_ramfb_device(nsas, ramfb_pa);
        xnu_get_video_bootargs(&v_bootargs, ramfb_pa);
    }

    phys_ptr += align_64k_high(sizeof(AllocatedData));
    top_of_kernel_data_pa = phys_ptr;
    remaining_mem_size = machine->ram_size - used_ram_for_blobs;
    mem_size = allocated_ram_pa - T8030_PHYS_BASE + remaining_mem_size;
    tms->dram_base = T8030_PHYS_BASE;
    tms->dram_size = mem_size;

    fprintf(stderr, "mem_size: 0x" TARGET_FMT_lx "\n", mem_size);
    fprintf(stderr, "dram-base: 0x" TARGET_FMT_lx "\n", tms->dram_base);
    fprintf(stderr, "dram-size: 0x" TARGET_FMT_lx "\n", tms->dram_size);

    macho_load_dtb(tms->device_tree, nsas, sysmem, "dtb.T8030",
                   dtb_pa, &dtb_size,
                   tms->ramdisk_file_dev.pa, ramdisk_size,
                   trustcache_pa, trustcache_size,
                   tms->dram_base, tms->dram_size);
    assert(dtb_size <= T8030_MAX_DEVICETREE_SIZE);

    macho_setup_bootargs("k_bootargs.T8030", nsas, sysmem, kbootargs_pa,
                         virt_base, T8030_PHYS_BASE, mem_size,
                         top_of_kernel_data_pa, dtb_va, dtb_size,
                         v_bootargs, tms->kern_args);

    allocate_ram(sysmem, "T8030.ram", allocated_ram_pa, remaining_mem_size);
}

static void cpu_impl_reg_write(void *opaque,
                  hwaddr addr,
                  uint64_t data,
                  unsigned size){
    T8030CPU* cpu = (T8030CPU*) opaque;
    fprintf(stderr, "CPU %u cpu-impl-reg WRITE @ 0x" TARGET_FMT_lx " value: 0x" TARGET_FMT_lx "\n", cpu->cpu_id, addr, data);
}
static uint64_t cpu_impl_reg_read(void *opaque,
                     hwaddr addr,
                     unsigned size){
    T8030CPU* cpu = (T8030CPU*) opaque;
    fprintf(stderr, "CPU %u cpu-impl-reg READ @ 0x" TARGET_FMT_lx "\n", cpu->cpu_id, addr);
    return 0;
}
static const MemoryRegionOps cpu_impl_reg_ops = {
    .write = cpu_impl_reg_write,
    .read = cpu_impl_reg_read,
};

static void cpm_impl_reg_write(void *opaque,
                  hwaddr addr,
                  uint64_t data,
                  unsigned size){
    cluster* cpm = (cluster*) opaque;
    fprintf(stderr, "Cluster %u cpm-impl-reg WRITE @ 0x" TARGET_FMT_lx " value: 0x" TARGET_FMT_lx "\n", cpm->id, addr, data);
}
static uint64_t cpm_impl_reg_read(void *opaque,
                     hwaddr addr,
                     unsigned size){
    cluster* cpm = (cluster*) opaque;
    fprintf(stderr, "Cluster %u cpm-impl-reg READ @ 0x" TARGET_FMT_lx "\n", cpm->id, addr);
    return 0;
}
static const MemoryRegionOps cpm_impl_reg_ops = {
    .write = cpm_impl_reg_write,
    .read = cpm_impl_reg_read,
};

static void T8030_cpu_setup(MachineState *machine)
{
    T8030MachineState *tms = T8030_MACHINE(machine);
    tms->clusters[0].base = CPM_IMPL_REG_BASE;
    tms->clusters[0].type = 0x45; // E
    tms->clusters[0].id = 0;
    tms->clusters[0].mr = g_new(MemoryRegion, 1);
    memory_region_init_io(tms->clusters[0].mr, OBJECT(machine), &cpm_impl_reg_ops, &tms->clusters[0], "cpm-impl-reg", 0x10000);
    memory_region_add_subregion(tms->sysmem, tms->clusters[0].base, tms->clusters[0].mr);
    tms->clusters[1].base = CPM_IMPL_REG_BASE + 0x10000;
    tms->clusters[1].type = 0x50; // P
    tms->clusters[1].id = 1;
    tms->clusters[1].mr = g_new(MemoryRegion, 1);
    memory_region_init_io(tms->clusters[1].mr, OBJECT(machine), &cpm_impl_reg_ops, &tms->clusters[1], "cpm-impl-reg", 0x10000);
    memory_region_add_subregion(tms->sysmem, tms->clusters[1].base,tms->clusters[1].mr);

    DTBNode* root = get_dtb_child_node_by_name(tms->device_tree, "cpus");
    for(int i=0;i<MAX_CPU;i++){
        if (i>= machine->smp.cpus) {
            break;
        }
        Object *cpuobj = object_new(machine->cpu_type);
        tms->cpus[i].cpu = ARM_CPU(cpuobj);
        CPUState *cs = CPU(tms->cpus[i].cpu);

        object_property_set_link(cpuobj, "memory", OBJECT(tms->sysmem), &error_abort);
        // object_property_set_link(cpuobj, "tag-memory", OBJECT(tms->tagmem),
        //                         &error_abort);
    
        //set secure monitor to false
        object_property_set_bool(cpuobj, "has_el3", false, NULL);

        object_property_set_bool(cpuobj, "has_el2", false, NULL);

        if(i>0){
            object_property_set_bool(cpuobj, "start-powered-off", true,
                                         NULL);
        }

        qdev_realize(DEVICE(cpuobj), NULL, &error_fatal);
        
        tms->cpus[i].cpu_id = i;
        tms->cpus[i].nsas = cpu_get_address_space(cs, ARMASIdx_NS);

        tms->cpus[i].impl_reg = g_new(MemoryRegion, 1);
        memory_region_init_io(tms->cpus[i].impl_reg, cpuobj, &cpu_impl_reg_ops, &tms->cpus[i], "cpu-impl-reg", 0x10000);

        hwaddr cpu_impl_reg_addr = CPU_IMPL_REG_BASE + (0x10000) * tms->cpus[i].cpu_id;

        memory_region_add_subregion(tms->sysmem, cpu_impl_reg_addr, tms->cpus[i].impl_reg);
        
        qdev_connect_gpio_out(DEVICE(cpuobj), GTIMER_VIRT,
                          qdev_get_gpio_in(DEVICE(cpuobj), ARM_CPU_FIQ));
        T8030_add_cpregs(&tms->cpus[i]);
        
        object_unref(cpuobj);
    }
    //currently support only a single CPU and thus
    //use no interrupt controller and wire IRQs from devices directly to the CPU
}

static void T8030_bootargs_setup(MachineState *machine)
{
    T8030MachineState *tms = T8030_MACHINE(machine);
    tms->bootinfo.firmware_loaded = true;
}

static void T8030_cpu_reset(void *opaque)
{
    T8030MachineState *tms = T8030_MACHINE((MachineState *)opaque);
    ARMCPU *cpu = ARM_CPU(first_cpu);
    CPUState *cs = CPU(cpu);
    CPUARMState *env = &cpu->env;

    cpu_reset(cs);

    env->xregs[0] = tms->kbootargs_pa;
    env->pc = tms->kpc_pa;
}

static void T8030_machine_init(MachineState *machine)
{
    T8030MachineState *tms = T8030_MACHINE(machine);

    tms->sysmem = get_system_memory();
    tms->tagmem = g_new(MemoryRegion, 1);
    memory_region_init(tms->tagmem, OBJECT(machine), "tag-memory", UINT64_MAX / 32);
    
    tms->device_tree = load_dtb_from_file(tms->dtb_filename);
    DTBNode *child = get_dtb_child_node_by_name(tms->device_tree, "arm-io");
    assert(child != NULL);
    DTBProp *prop = get_dtb_prop(child, "ranges");
    assert(prop != NULL);
    hwaddr *ranges = (hwaddr *)prop->value;
    tms->soc_base_pa = ranges[1];

    T8030_cpu_setup(machine);

    T8030_memory_setup(machine);

    AllocatedData *allocated_data = (AllocatedData *)tms->extra_data_pa;

    T8030_create_s3c_uart(tms, serial_hd(0));

    T8030_bootargs_setup(machine);

    qemu_register_reset(T8030_cpu_reset, tms);
}

static void T8030_set_ramdisk_filename(Object *obj, const char *value,
                                       Error **errp)
{
    T8030MachineState *tms = T8030_MACHINE(obj);

    g_strlcpy(tms->ramdisk_filename, value, sizeof(tms->ramdisk_filename));
}

static char *T8030_get_ramdisk_filename(Object *obj, Error **errp)
{
    T8030MachineState *tms = T8030_MACHINE(obj);
    return g_strdup(tms->ramdisk_filename);
}

static void T8030_set_kernel_filename(Object *obj, const char *value,
                                      Error **errp)
{
    T8030MachineState *tms = T8030_MACHINE(obj);

    g_strlcpy(tms->kernel_filename, value, sizeof(tms->kernel_filename));
}

static char *T8030_get_kernel_filename(Object *obj, Error **errp)
{
    T8030MachineState *tms = T8030_MACHINE(obj);
    return g_strdup(tms->kernel_filename);
}

static void T8030_set_dtb_filename(Object *obj, const char *value,
                                   Error **errp)
{
    T8030MachineState *tms = T8030_MACHINE(obj);

    g_strlcpy(tms->dtb_filename, value, sizeof(tms->dtb_filename));
}

static char *T8030_get_dtb_filename(Object *obj, Error **errp)
{
    T8030MachineState *tms = T8030_MACHINE(obj);
    return g_strdup(tms->dtb_filename);
}

static void T8030_set_kern_args(Object *obj, const char *value,
                                Error **errp)
{
    T8030MachineState *tms = T8030_MACHINE(obj);

    g_strlcpy(tms->kern_args, value, sizeof(tms->kern_args));
}

static char *T8030_get_kern_args(Object *obj, Error **errp)
{
    T8030MachineState *tms = T8030_MACHINE(obj);
    return g_strdup(tms->kern_args);
}

static void T8030_set_xnu_ramfb(Object *obj, const char *value,
                                Error **errp)
{
    T8030MachineState *tms = T8030_MACHINE(obj);
    if (strcmp(value, "on") == 0)
        tms->use_ramfb = true;
    else
    {
        if (strcmp(value, "off") != 0)
            fprintf(stderr, "NOTE: the value of xnu-ramfb is not valid,\
the framebuffer will be disabled.\n");
        tms->use_ramfb = false;
    }
}

static char *T8030_get_xnu_ramfb(Object *obj, Error **errp)
{
    T8030MachineState *tms = T8030_MACHINE(obj);
    if (tms->use_ramfb)
        return g_strdup("on");
    else
        return g_strdup("off");
}

static void T8030_instance_init(Object *obj)
{
    object_property_add_str(obj, "ramdisk-filename", T8030_get_ramdisk_filename,
                            T8030_set_ramdisk_filename);
    object_property_set_description(obj, "ramdisk-filename",
                                    "Set the ramdisk filename to be loaded");

    object_property_add_str(obj, "kernel-filename", T8030_get_kernel_filename,
                            T8030_set_kernel_filename);
    object_property_set_description(obj, "kernel-filename",
                                    "Set the kernel filename to be loaded");

    object_property_add_str(obj, "dtb-filename", T8030_get_dtb_filename, T8030_set_dtb_filename);
    object_property_set_description(obj, "dtb-filename",
                                    "Set the dev tree filename to be loaded");

    object_property_add_str(obj, "kern-cmd-args", T8030_get_kern_args,
                            T8030_set_kern_args);
    object_property_set_description(obj, "kern-cmd-args",
                                    "Set the XNU kernel cmd args");

    object_property_add_str(obj, "xnu-ramfb",
                            T8030_get_xnu_ramfb,
                            T8030_set_xnu_ramfb);
    object_property_set_description(obj, "xnu-ramfb",
                                    "Turn on the display framebuffer");
}

static void T8030_machine_class_init(ObjectClass *klass, void *data)
{
    MachineClass *mc = MACHINE_CLASS(klass);
    mc->desc = "T8030";
    mc->init = T8030_machine_init;
    mc->max_cpus = MAX_CPU;
    //this disables the error message "Failed to query for block devices!"
    //when starting qemu - must keep at least one device
    //mc->no_sdcard = 1;
    mc->no_floppy = 1;
    mc->no_cdrom = 1;
    mc->no_parallel = 1;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-a72");
    mc->minimum_page_bits = 12;
}

static const TypeInfo T8030_machine_info = {
    .name = TYPE_T8030_MACHINE,
    .parent = TYPE_MACHINE,
    .instance_size = sizeof(T8030MachineState),
    .class_size = sizeof(T8030MachineClass),
    .class_init = T8030_machine_class_init,
    .instance_init = T8030_instance_init,
};

static void T8030_machine_types(void)
{
    type_register_static(&T8030_machine_info);
}

type_init(T8030_machine_types)
