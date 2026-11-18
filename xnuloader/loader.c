/*
 * Mu-Silicium UEFI XNU Loader for Google Pixel 3a XL (bonito, SDM670)
 *
 * Launched by Mu-Silicium's BootManagerLib (XnuLoaderMain) from the "XNUP"
 * payload container at 0xF0000000.
 *
 * Displays boot progress via UEFI SimpleFbDxe / GraphicsConsoleDxe, stages
 * the pre-rebased XNU 8796 kernel + Apple Device Tree (bonito.adt) + RAMDisk
 * into the contiguous XNU physical window at 0xA6000000, flushes caches,
 * turns off the MMU, and jumps to XNU's start.s entry point.
 */
#include "efi.h"

#define PAYLOAD_BASE        0xF0000000ULL
#define PAYLOAD_MAGIC       0x50554E58u /* "XNUP" */
#define SDE_CTL0_START_REG  0x0AE0201CULL
#define APSS_WDT_ENABLE_REG 0x17980008ULL

struct payload_header {
	uint32_t magic;
	uint32_t size;
};

struct payload_entry {
	char name[16];
	uint32_t size;
	uint32_t reserved;
};

/* Pre-rebased SWEETXNU payload format produced by tools/mkpayload.py */
struct sweet_seg {
	uint64_t vmaddr;
	uint64_t vmsize;
	uint64_t file_off;
	uint64_t file_size;
};

struct sweet_header {
	char     magic[8];       /* "SWEETXNU" */
	uint32_t version;
	uint32_t nsegs;
	uint64_t virt_base;
	uint64_t entry_va;
	uint64_t adt_off;
	uint64_t adt_size;
	uint64_t kernel_end_va;
	uint64_t phys_base;
	uint64_t mem_size;
	char     cmdline[608];
	struct sweet_seg segs[];
};

/* XNU arm64 boot_args (pexpert/pexpert/arm64/boot.h) */
struct boot_video {
	uint64_t v_baseAddr;
	uint64_t v_display;
	uint64_t v_rowBytes;
	uint64_t v_width;
	uint64_t v_height;
	uint64_t v_depth;
};

#define BOOT_LINE_LENGTH 608

struct boot_args {
	uint16_t revision;
	uint16_t version;
	uint64_t virtBase;
	uint64_t physBase;
	uint64_t memSize;
	uint64_t topOfKernelData;
	struct boot_video video;
	uint32_t machineType;
	uint64_t deviceTreeP;
	uint32_t deviceTreeLength;
	char     commandLine[BOOT_LINE_LENGTH];
	uint64_t bootFlags;
	uint64_t memSizeActual;
};

static efi_system_table *ST;
static efi_boot_services *BS;

void *
memset(void *dst, int c, size_t n)
{
	uint8_t *d = dst;
	while (n--) {
		*d++ = (uint8_t)c;
	}
	return dst;
}

void *
memcpy(void *dst, const void *src, size_t n)
{
	uint8_t *d = dst;
	const uint8_t *s = src;
	if ((((uintptr_t)d | (uintptr_t)s) & 7) == 0) {
		uint64_t *dw = (uint64_t *)d;
		const uint64_t *sw = (const uint64_t *)s;
		for (; n >= 8; n -= 8) {
			*dw++ = *sw++;
		}
		d = (uint8_t *)dw;
		s = (const uint8_t *)sw;
	}
	while (n--) {
		*d++ = *s++;
	}
	return dst;
}

static int
str_eq(const char *a, const char *b)
{
	while (*a && *a == *b) {
		a++, b++;
	}
	return *a == *b;
}

static void
kick_panel(void)
{
	__asm__ volatile ("dsb sy" ::: "memory");
	*(volatile uint32_t *)(uintptr_t)SDE_CTL0_START_REG = 1;
}

static void
put_char(char c)
{
	char16 w[3] = { 0, 0, 0 };
	if (ST->con_out != NULL) {
		if (c == '\n') {
			w[0] = '\r';
			w[1] = '\n';
		} else {
			w[0] = (char16)c;
		}
		ST->con_out->output_string(ST->con_out, w);
		if (c == '\n') {
			kick_panel();
		}
	}
}

static void
print(const char *fmt, ...)
{
	__builtin_va_list ap;
	__builtin_va_start(ap, fmt);
	for (; *fmt; fmt++) {
		if (*fmt != '%') {
			put_char(*fmt);
			continue;
		}
		fmt++;
		if (*fmt == 's') {
			const char *s = __builtin_va_arg(ap, const char *);
			while (*s) {
				put_char(*s++);
			}
		} else if (*fmt == 'x' || *fmt == 'd') {
			uint64_t v = __builtin_va_arg(ap, uint64_t);
			unsigned base = (*fmt == 'x') ? 16 : 10;
			char tmp[24];
			int i = 0;
			do {
				tmp[i++] = "0123456789abcdef"[v % base];
				v /= base;
			} while (v);
			while (i) {
				put_char(tmp[--i]);
			}
		} else {
			put_char(*fmt);
		}
	}
	__builtin_va_end(ap);
	kick_panel();
}

#define READ_SYSREG(name) ({ uint64_t _v; __asm__ volatile ("mrs %0, " #name : "=r" (_v)); _v; })

static void
flush_dcache(uint64_t start, uint64_t end)
{
	uint64_t line = 4ULL << ((READ_SYSREG(CTR_EL0) >> 16) & 0xF);
	for (uint64_t p = start & ~(line - 1); p < end; p += line) {
		__asm__ volatile ("dc civac, %0" : : "r" (p));
	}
	__asm__ volatile ("dsb sy");
}

__attribute__((noreturn)) static void
jump_to_kernel(uint64_t entry_pa, uint64_t boot_args_pa)
{
	__asm__ volatile (
		"msr daifset, #0xf\n"
		"dsb sy\n"
		"mrs x2, sctlr_el1\n"
		"bic x2, x2, #0x1\n"    /* M: MMU off */
		"bic x2, x2, #0x4\n"    /* C: data cache off */
		"bic x2, x2, #0x1000\n" /* I: instruction cache off */
		"msr sctlr_el1, x2\n"
		"isb\n"
		"ic iallu\n"
		"dsb sy\n"
		"isb\n"
		"mov x0, %1\n"
		"br %0\n"
		: : "r" (entry_pa), "r" (boot_args_pa) : "x0", "x2"
	);
	__builtin_unreachable();
}

extern char __ImageBase;

static const uint8_t *
find_xnup_entry(const char *want_name, uint32_t *out_size)
{
	const struct payload_header *ph = (const struct payload_header *)(uintptr_t)PAYLOAD_BASE;
	if (ph->magic != PAYLOAD_MAGIC) {
		return NULL;
	}
	const uint8_t *p = (const uint8_t *)(ph + 1);
	const uint8_t *pend = p + ph->size;
	while (p + sizeof(struct payload_entry) <= pend) {
		const struct payload_entry *e = (const struct payload_entry *)p;
		if (e->name[0] == 0) {
			break;
		}
		if (str_eq(e->name, want_name)) {
			*out_size = e->size;
			return p + sizeof(*e);
		}
		p += sizeof(*e) + ((e->size + 15u) & ~15u);
	}
	return NULL;
}

efi_status EFIAPI
efi_main(efi_handle image, efi_system_table *st)
{
	(void)image;
	ST = st;
	BS = st->bs;

	/* Disable UEFI and Qualcomm APSS watchdogs */
	BS->set_watchdog_timer(0, 0, 0, NULL);
	*(volatile uint32_t *)(uintptr_t)APSS_WDT_ENABLE_REG = 0;

	/* Keep Mu-Silicium boot logo visible briefly before switching to UEFI console */
	BS->stall(1200000);

	if (ST->con_out != NULL && ST->con_out->set_attribute != NULL && ST->con_out->clear_screen != NULL) {
		ST->con_out->set_attribute(ST->con_out, 0x0F); /* White on Black */
		ST->con_out->clear_screen(ST->con_out);
		kick_panel();
	}

	print("\n");
	print("====================================================\n");
	print("  Mu-Silicium UEFI + XNULOADER (Pixel 3a XL bonito)\n");
	print("  Target: iOS 16 (XNU 8796.0.0.100.25 ARM64_BONITO)\n");
	print("====================================================\n\n");

	print("CPU: CurrentEL=%d MIDR=0x%x MPIDR=0x%x CNTFRQ=%d Hz\n",
	    READ_SYSREG(CurrentEL) >> 2,
	    READ_SYSREG(MIDR_EL1),
	    READ_SYSREG(MPIDR_EL1) & 0xFFFFFF,
	    READ_SYSREG(CNTFRQ_EL0));

	static const efi_guid gop_guid = EFI_GOP_GUID;
	efi_gop *gop = NULL;
	uint64_t fb_base = 0x9C000000ULL;
	uint32_t fb_w = 1080, fb_h = 2160, fb_stride = 4320;
	if (!EFI_ERROR(BS->locate_protocol(&gop_guid, NULL, (void **)&gop)) &&
	    gop != NULL && gop->mode != NULL && gop->mode->info != NULL) {
		fb_base   = gop->mode->fb_base;
		fb_w      = gop->mode->info->width;
		fb_h      = gop->mode->info->height;
		fb_stride = gop->mode->info->pixels_per_scanline * 4;
	}
	print("GOP: %d x %d (stride %d) at 0x%x\n",
	    (uint64_t)fb_w, (uint64_t)fb_h, (uint64_t)fb_stride, fb_base);

	uint32_t sweet_size = 0;
	const uint8_t *sweet_blob = find_xnup_entry("sweetxnu", &sweet_size);
	if (sweet_blob == NULL || sweet_size < sizeof(struct sweet_header)) {
		print("ERROR: 'sweetxnu' payload not found in XNUP container at 0x%x!\n", PAYLOAD_BASE);
		for (;;) {
			BS->stall(1000000);
		}
	}

	const struct sweet_header *hdr = (const struct sweet_header *)sweet_blob;
	print("Payload: SWEETXNU v%d, %d segments (%d bytes)\n",
	    (uint64_t)hdr->version, (uint64_t)hdr->nsegs, (uint64_t)sweet_size);
	print("  virt_base : 0x%x\n", hdr->virt_base);
	print("  entry_va  : 0x%x\n", hdr->entry_va);
	print("  phys_base : 0x%x (size 0x%x)\n", hdr->phys_base, hdr->mem_size);
	print("  cmdline   : %s\n\n", hdr->cmdline);

	int64_t slide = (int64_t)(hdr->phys_base - hdr->virt_base);

	/* Copy pre-rebased Mach-O segments and RAMDisk into [phys_base, ...) */
	for (uint32_t i = 0; i < hdr->nsegs; i++) {
		const struct sweet_seg *s = &hdr->segs[i];
		uint64_t dst_pa = (uint64_t)((int64_t)s->vmaddr + slide);
		print("  seg[%d]: VA 0x%x -> PA 0x%x (filesz 0x%x, vmsz 0x%x)\n",
		    (uint64_t)i, s->vmaddr, dst_pa, s->file_size, s->vmsize);
		memset((void *)(uintptr_t)dst_pa, 0, (size_t)s->vmsize);
		memcpy((void *)(uintptr_t)dst_pa, sweet_blob + s->file_off, (size_t)s->file_size);
	}

	/* Stage Apple Device Tree (bonito.adt) and boot_args after kernel_end_va */
	uint64_t page_mask = 0x3FFFULL; /* 16 KiB page alignment */
	uint64_t adt_va = (hdr->kernel_end_va + page_mask) & ~page_mask;
	uint64_t adt_pa = (uint64_t)((int64_t)adt_va + slide);
	uint64_t adt_len_aligned = (hdr->adt_size + page_mask) & ~page_mask;
	memcpy((void *)(uintptr_t)adt_pa, sweet_blob + hdr->adt_off, (size_t)hdr->adt_size);

	uint64_t ba_va = adt_va + adt_len_aligned;
	uint64_t ba_pa = (uint64_t)((int64_t)ba_va + slide);
	uint64_t top_pa = ba_pa + 0x4000ULL;

	struct boot_args *ba = (struct boot_args *)(uintptr_t)ba_pa;
	memset(ba, 0, sizeof(*ba));
	ba->revision         = 2;
	ba->version          = 2;
	ba->virtBase         = hdr->virt_base;
	ba->physBase         = hdr->phys_base;
	ba->memSize          = hdr->mem_size;
	ba->memSizeActual    = hdr->mem_size;
	ba->topOfKernelData  = top_pa;
	ba->deviceTreeP      = adt_va;
	ba->deviceTreeLength = (uint32_t)hdr->adt_size;
	ba->video.v_baseAddr = fb_base;
	ba->video.v_display  = 0; /* 0 = verbose text console enabled in XNU */
	ba->video.v_rowBytes = fb_stride;
	ba->video.v_width    = fb_w;
	ba->video.v_height   = fb_h;
	ba->video.v_depth    = 32;
	memcpy(ba->commandLine, hdr->cmdline, BOOT_LINE_LENGTH);

	uint64_t entry_pa = (uint64_t)((int64_t)hdr->entry_va + slide);
	print("\nADT staged at PA 0x%x (VA 0x%x, %d bytes)\n", adt_pa, adt_va, hdr->adt_size);
	print("boot_args at PA 0x%x, topOfKernelData 0x%x\n", ba_pa, top_pa);
	print("Jumping to XNU entry PA 0x%x in 2 seconds...\n", entry_pa);

	BS->stall(2000000);

	/* Clean panic log region, XNU window, framebuffer, and loader code/stack to physical RAM */
	memset((void *)(uintptr_t)(hdr->phys_base + hdr->mem_size), 0, 0x100000ULL);
	flush_dcache(hdr->phys_base + hdr->mem_size, hdr->phys_base + hdr->mem_size + 0x100000ULL);
	flush_dcache(hdr->phys_base, top_pa + 0x100000ULL);
	flush_dcache(fb_base, fb_base + (uint64_t)fb_stride * fb_h);
	flush_dcache((uint64_t)(uintptr_t)&__ImageBase, (uint64_t)(uintptr_t)&__ImageBase + 0x40000ULL);
	uint64_t sp_now = (uint64_t)(uintptr_t)__builtin_frame_address(0);
	flush_dcache(sp_now - 0x4000ULL, sp_now + 0x4000ULL);
	kick_panel();

	jump_to_kernel(entry_pa, ba_pa);
	return 0;
}
