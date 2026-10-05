/*
HOST_LOADER.C — Nintendo Switch

Loads the guest ELF image. This is nearly identical to the Android version:
the guest image format is the same (statically linked AArch64 ELF at
HALO_GUEST_IMAGE_BASE). The only difference is we use memcpy + libnx SVC
calls for memory permissions instead of mmap/mprotect.
*/

#include "host.h"

#include <switch.h>
#include <string.h>

/* Minimal ELF64 definitions (avoid pulling in Linux elf.h) */
#define EI_MAG0   0
#define ELFMAG0   0x7f
#define ELFMAG1   'E'
#define ELFMAG2   'L'
#define ELFMAG3   'F'
#define EI_CLASS  4
#define ELFCLASS64 2
#define EM_AARCH64 183
#define ET_EXEC   2
#define PT_LOAD   1
#define PF_X      0x1
#define PF_W      0x2

typedef struct {
	unsigned char e_ident[16];
	uint16_t e_type, e_machine;
	uint32_t e_version;
	uint64_t e_entry, e_phoff, e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
} Elf64_Ehdr;

typedef struct {
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align;
} Elf64_Phdr;

struct host_guest_image host_image;

static void missing_import(void)
{
	host_fatal("the guest called a host function that is not available");
}

int host_load_image(const void *file, size_t size)
{
	const Elf64_Ehdr *elf = file;
	const Elf64_Phdr *segments;
	uint64_t low = ~0ULL, high = 0, writable = ~0ULL, readonly_end = 0;
	const struct halo_guest_header *header;
	uint64_t *table;
	const char *name;
	uint32_t count, index;
	int missing = 0;

	if (size < sizeof(*elf) ||
	    elf->e_ident[EI_MAG0] != ELFMAG0 ||
	    elf->e_ident[1] != ELFMAG1 ||
	    elf->e_ident[2] != ELFMAG2 ||
	    elf->e_ident[3] != ELFMAG3 ||
	    elf->e_ident[EI_CLASS] != ELFCLASS64 ||
	    elf->e_machine != EM_AARCH64 ||
	    elf->e_type != ET_EXEC ||
	    elf->e_phentsize != sizeof(Elf64_Phdr) || !elf->e_phnum ||
	    elf->e_phnum > 128 || elf->e_phoff > size ||
	    elf->e_phnum > (size - elf->e_phoff) / sizeof(Elf64_Phdr))
	{
		host_logf(HOST_LOG_ERROR,
			"the guest image is not an AArch64 executable");
		return -1;
	}

	segments = (const Elf64_Phdr *)((const char *)file + elf->e_phoff);
	for (index = 0; index < elf->e_phnum; index++)
	{
		if (segments[index].p_type != PT_LOAD)
			continue;
		const Elf64_Phdr *segment = &segments[index];
		if (!segment->p_memsz || segment->p_filesz > segment->p_memsz ||
		    segment->p_offset > size || segment->p_filesz > size - segment->p_offset ||
		    segment->p_vaddr < HALO_GUEST_IMAGE_BASE || segment->p_vaddr >= HOST_GUEST_IMAGE_LIMIT ||
		    segment->p_memsz > HOST_GUEST_IMAGE_LIMIT - segment->p_vaddr ||
		    (segment->p_flags & (PF_X | PF_W)) == (PF_X | PF_W))
			return -1;
		for (unsigned previous = 0; previous < index; previous++)
		{
			const Elf64_Phdr *other = &segments[previous];
			if (other->p_type == PT_LOAD && segment->p_vaddr < other->p_vaddr + other->p_memsz &&
			    other->p_vaddr < segment->p_vaddr + segment->p_memsz)
				return -1;
		}
		if (segment->p_flags & PF_W)
		{
			if (segment->p_vaddr < writable) writable = segment->p_vaddr;
		}
		else if (segment->p_vaddr + segment->p_memsz > readonly_end)
			readonly_end = segment->p_vaddr + segment->p_memsz;
		if (segments[index].p_vaddr < low)
			low = segments[index].p_vaddr;
		if (segments[index].p_vaddr + segments[index].p_memsz > high)
			high = segments[index].p_vaddr + segments[index].p_memsz;
	}
	low &= ~0xfffULL;
	high = (high + 0xfff) & ~0xfffULL;

	if (low != HALO_GUEST_IMAGE_BASE || high > HOST_GUEST_IMAGE_LIMIT ||
	    writable <= low || writable >= high || (writable & 0xfff) || readonly_end > writable)
	{
		host_logf(HOST_LOG_ERROR, "the guest image spans %llx-%llx",
			(unsigned long long)low, (unsigned long long)high);
		return -1;
	}

	if (host_memory_initialize((uint32_t)low, (uint32_t)(high - low), (uint32_t)(writable - low)) != 0)
		return -1;

	/* Executable segments are copied through the CodeMemory writable alias.
	 * Data/BSS has a separate fixed RW mapping; no writes go through RX. */
	for (index = 0; index < elf->e_phnum; index++)
	{
		const Elf64_Phdr *segment = &segments[index];

		if (segment->p_type != PT_LOAD)
			continue;
		void *destination = host_native_image_pointer((uint32_t)segment->p_vaddr, segment->p_memsz);
		if (!destination) return -1;
		memcpy(destination,
			(const char *)file + segment->p_offset,
			segment->p_filesz);
	}

	host_native_image_flush();
	header = (const struct halo_guest_header *)(uintptr_t)low;
	if (header->magic != HALO_GUEST_MAGIC ||
	    header->abi_version != HALO_GUEST_ABI_VERSION)
	{
		host_logf(HOST_LOG_ERROR,
			"the guest image header does not match this host");
		return -1;
	}
	/* The imports table must be writable; names/count and entry code must
	 * reside in the validated image, before following any guest pointer. */
	if (header->import_count < low || header->import_count > writable - sizeof(uint32_t) ||
	    header->import_names < low || header->import_names >= writable ||
	    header->import_table < writable || header->import_table > high ||
	    (header->import_table & 7)) return -1;

	/* Fill the import table */
	table = (uint64_t *)(uintptr_t)header->import_table;
	name = (const char *)(uintptr_t)header->import_names;
	count = *(const uint32_t *)(uintptr_t)header->import_count;
	if (count > (high - header->import_table) / sizeof(uint64_t)) return -1;

	for (index = 0; index < count; index++)
	{
		size_t remaining = writable - (uintptr_t)name;
		const char *end = memchr(name, 0, remaining);
		if (!end) return -1;
		void *function = host_resolve_import(name);

		if (!function && !strncmp(name, "hostgl_", 7))
			function = host_gl_resolve(name + 7);
		if (!function)
		{
			host_logf(HOST_LOG_WARN,
				"guest import %s is not available", name);
			function = (void *)missing_import;
			missing++;
		}
		table[index] = (uint64_t)(uintptr_t)function;
		name = end + 1;
	}

	host_logf(HOST_LOG_INFO,
		"guest image %08llx-%08llx, %u imports (%d unavailable)",
		(unsigned long long)low, (unsigned long long)high,
		count, missing);

	host_image.header = header;
	host_image.base = (uint32_t)low;
	host_image.end = (uint32_t)high;
	host_native_image_flush();

	return 0;
}
