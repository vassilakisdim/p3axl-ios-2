/*
 * Minimal UEFI definitions for the Mu-Silicium XNU loader (freestanding).
 */
#ifndef XNULOADER_EFI_H
#define XNULOADER_EFI_H

#include <stdint.h>
#include <stddef.h>

#define EFIAPI
typedef uint64_t efi_status;
typedef void *efi_handle;
typedef uint16_t char16;

#define EFI_ERROR_BIT        0x8000000000000000ULL
#define EFI_ERROR(s)         (((s) & EFI_ERROR_BIT) != 0)

typedef struct {
	uint32_t a; uint16_t b; uint16_t c; uint8_t d[8];
} efi_guid;

typedef struct {
	uint64_t signature; uint32_t revision; uint32_t header_size; uint32_t crc32; uint32_t reserved;
} efi_table_header;

struct efi_text_output;
typedef struct efi_text_output {
	efi_status (EFIAPI *reset)(struct efi_text_output *self, uint8_t extended);
	efi_status (EFIAPI *output_string)(struct efi_text_output *self, const char16 *str);
	void *test_string;
	void *query_mode;
	void *set_mode;
	efi_status (EFIAPI *set_attribute)(struct efi_text_output *self, uint64_t attr);
	efi_status (EFIAPI *clear_screen)(struct efi_text_output *self);
} efi_text_output;

enum { EfiLoaderData = 2, EfiConventionalMemory = 7 };
enum { AllocateAnyPages = 0, AllocateMaxAddress = 1, AllocateAddress = 2 };

typedef struct {
	uint32_t type; uint32_t pad; uint64_t phys_start; uint64_t virt_start; uint64_t pages; uint64_t attribute;
} efi_memory_descriptor;

typedef struct {
	efi_table_header hdr;
	void *raise_tpl, *restore_tpl;
	efi_status (EFIAPI *allocate_pages)(int type, int mem_type, uint64_t pages, uint64_t *addr);
	efi_status (EFIAPI *free_pages)(uint64_t addr, uint64_t pages);
	efi_status (EFIAPI *get_memory_map)(uint64_t *size, void *map, uint64_t *key, uint64_t *desc_size, uint32_t *desc_ver);
	efi_status (EFIAPI *allocate_pool)(int type, uint64_t size, void **buf);
	efi_status (EFIAPI *free_pool)(void *buf);
	void *create_event, *set_timer, *wait_for_event;
	efi_status (EFIAPI *signal_event)(void *event);
	efi_status (EFIAPI *close_event)(void *event);
	void *check_event;
	void *install_protocol_interface, *reinstall_protocol_interface, *uninstall_protocol_interface;
	efi_status (EFIAPI *handle_protocol)(efi_handle h, const efi_guid *guid, void **iface);
	void *reserved, *register_protocol_notify, *locate_handle, *locate_device_path, *install_configuration_table;
	void *load_image, *start_image, *exit, *unload_image;
	efi_status (EFIAPI *exit_boot_services)(efi_handle image, uint64_t map_key);
	void *get_next_monotonic_count;
	efi_status (EFIAPI *stall)(uint64_t microseconds);
	efi_status (EFIAPI *set_watchdog_timer)(uint64_t timeout, uint64_t code, uint64_t size, char16 *data);
	void *connect_controller, *disconnect_controller, *open_protocol, *close_protocol, *open_protocol_information;
	void *protocols_per_handle;
	efi_status (EFIAPI *locate_handle_buffer)(int type, const efi_guid *guid, void *key, uint64_t *count, efi_handle **buf);
	efi_status (EFIAPI *locate_protocol)(const efi_guid *guid, void *registration, void **iface);
} efi_boot_services;

typedef struct {
	efi_table_header hdr;
	void *get_time, *set_time, *get_wakeup_time, *set_wakeup_time;
	void *set_virtual_address_map, *convert_pointer;
	void *get_variable, *get_next_variable_name, *set_variable;
	void *get_next_high_mono_count;
	void (EFIAPI *reset_system)(int type, efi_status status, uint64_t size, void *data);
} efi_runtime_services;

typedef struct {
	efi_table_header hdr;
	char16 *vendor; uint32_t revision; uint32_t pad;
	efi_handle con_in_handle; void *con_in;
	efi_handle con_out_handle; efi_text_output *con_out;
	efi_handle stderr_handle; efi_text_output *std_err;
	efi_runtime_services *rt;
	efi_boot_services *bs;
	uint64_t nr_tables;
	struct { efi_guid guid; void *table; } *tables;
} efi_system_table;

typedef struct {
	uint32_t version, width, height, pixel_format;
	uint32_t pixel_mask[4];
	uint32_t pixels_per_scanline;
} efi_gop_mode_info;

typedef struct {
	uint32_t max_mode, mode;
	efi_gop_mode_info *info;
	uint64_t info_size;
	uint64_t fb_base;
	uint64_t fb_size;
} efi_gop_mode;

typedef struct efi_gop {
	efi_status (EFIAPI *query_mode)(struct efi_gop *self, uint32_t mode, uint64_t *info_size, efi_gop_mode_info **info);
	efi_status (EFIAPI *set_mode)(struct efi_gop *self, uint32_t mode);
	void *blt;
	efi_gop_mode *mode;
} efi_gop;

#define EFI_GOP_GUID { 0x9042a9de, 0x23dc, 0x4a38, { 0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a } }

#endif /* XNULOADER_EFI_H */
