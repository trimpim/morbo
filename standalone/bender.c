/* -*- Mode: C -*- */

#include <pci.h>
#include <mbi.h>
#include <mbi2.h>
#include <util.h>
#include <elf.h>
#include <version.h>
#include <serial.h>
#include <bda.h>
#include <vga.h>
#include <plugin.h>

/* Configuration (set by command line parser) */
static uint64_t phys_max_relocate = 1ULL << 31; /* below 2G */

static bool be_promisc       = false;
static bool serial_fallback  = false;
static bool option_microcode = false;
static bool option_intel_hwp = false;

void
parse_cmdline(const char *cmdline)
{
  char *last_ptr = NULL;
  char cmdline_buf[256];
  char *token;
  unsigned i;

  strncpy(cmdline_buf, cmdline, sizeof(cmdline_buf));

  for (token = strtok_r(cmdline_buf, " ", &last_ptr), i = 0;
       token != NULL;
       token = strtok_r(NULL, " ", &last_ptr), i++) {

    if (strcmp(token, "promisc") == 0)
      be_promisc = true;
    else
    if (strcmp(token, "phys_max=256M") == 0)
      phys_max_relocate = 256ULL * 1024 * 1024;
    else
    if (strcmp(token, "vga") == 0)
      vga_init();
    else
    if (strcmp(token, "serial_fallback") == 0)
      serial_fallback = true;
    else
    if (strcmp(token, "microcode") == 0)
      option_microcode = true;
    else
    if (strcmp(token, "intel_hwp") == 0)
      option_intel_hwp = true;
  }
}

static void uart_init(bool const efi_boot)
{
  struct pci_device serial_ctrl = { 0, 0 };

  printf("Looking for serial controllers on the PCI bus...\n");

  if (pci_find_device_by_class(PCI_CLASS_SIMPLE_COMM,
                               be_promisc ? PCI_SUBCLASS_ANY : PCI_SUBCLASS_SERIAL_CTRL,
                               &serial_ctrl)) {
    printf("  found at %x.\n", serial_ctrl.cfg_address);
  } else {
    printf("  none found.\n");
  }

  uint16_t iobase = 0;

  if (serial_ctrl.cfg_address) {
    for (unsigned bar_no = 0; bar_no < 6; bar_no++) {
      uint32_t bar = pci_cfg_read_uint32(&serial_ctrl, PCI_CFG_BAR0 + 4*bar_no);
      if ((bar & PCI_BAR_TYPE_MASK) == PCI_BAR_TYPE_IO) {
        iobase = bar & PCI_BAR_IO_MASK;
        break;
      }
    }

    uint32_t command = pci_cfg_read_uint32(&serial_ctrl, PCI_CFG_CMD);

    if (!(command & PCI_CMD_IO))
      pci_cfg_write_uint8(&serial_ctrl, PCI_CFG_CMD, command | PCI_CMD_IO);

  }

  printf("A iobase=%x efi_boot=%u serial_fallback=%u\n", iobase, efi_boot, serial_fallback);
 
  /* no pci card and non efi boot */
  if (!iobase && !efi_boot) {
    if (serial_ports(get_bios_data_area()))
       iobase = get_bios_data_area()->com_port[0];
  }

  printf("B iobase=%x efi_boot=%u serial_fallback=%u\n", iobase, efi_boot, serial_fallback);
 
  /* no pci card and serial_fallback */
  if (!iobase && serial_fallback) {
    if (efi_boot)
      iobase = 0x3f8;

    if (!efi_boot && !serial_ports(get_bios_data_area()))
      iobase = 0x3f8;
  }

  printf("C iobase=%x efi_boot=%u serial_fallback=%u\n", iobase, efi_boot, serial_fallback);
 
  if (iobase) {
    serial_init(iobase);
    printf("\nBender %s\n", version_str);
  }

  /* BDA patching - UEFI case checks missing XXX */
  if (iobase) {
    printf("Patching BDA with I/O port 0x%x.\n", iobase);

    /* In UEFI case this memory may be occupied by something else
     * - several checks are missing here to avoid potential corruption XXX
     */
    uint16_t *com0_port      = (uint16_t *)(get_bios_data_area());
    uint16_t *equipment_word = &get_bios_data_area()->equipment;

    *com0_port      = iobase;
    *equipment_word = (*equipment_word & ~(0xF << 9)) | (1 << 9); /* One COM port available */
  }
}

int
main(uint32_t magic, void *multiboot)
{
  bool efi_boot = false;

  serial_init(0x3f8);

  if (magic == MBI_MAGIC) {
    struct mbi * mbi = (struct mbi *)multiboot;
    if ((mbi->flags & MBI_FLAG_CMDLINE) != 0)
      parse_cmdline((const char *)mbi->cmdline);
  } else
  if (magic == MBI2_MAGIC) {
    for (struct mbi2_tag *i = mbi2_first(multiboot); i; i = mbi2_next(i)) {
      switch (i->type) {
        case MBI2_TAG_CMDLINE:
          parse_cmdline((const char *)(i + 1));
          break;
        case MBI2_TAG_EFI_IMAGE_32:
        case MBI2_TAG_EFI_IMAGE_64:
          efi_boot = true;
          break;
      }
    }
  } else {
    printf("Not loaded by Multiboot-compliant loader. Bye.\n");
    return 1;
  }

  uart_init(efi_boot);

  printf("Bender: Hello World.\n");

  bool const smp = option_microcode || option_intel_hwp;
  if (smp)
     smp_install_code();

  if (option_microcode) flag_plugin_for_aps(PLUGIN_MICROCODE);
  if (option_intel_hwp) flag_plugin_for_aps(PLUGIN_INTEL_HWP);

  if (option_microcode) microcode_main(magic, multiboot);
  if (option_intel_hwp) intel_hwp_main(magic, multiboot);

  if (smp) {
     smp_main(magic, multiboot);
     /*
      * When we return here, one thread per processor core woke up,
      * executed all flagged plugins and went to halt finally.
      */
  }

  if (magic == MBI_MAGIC)
      return start_module((struct mbi *)multiboot, false, phys_max_relocate);
  else
  if (magic == MBI2_MAGIC)
      return start_module2(multiboot, false, phys_max_relocate);
  else
      return 1;
}
