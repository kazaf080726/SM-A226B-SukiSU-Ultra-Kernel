### AnyKernel3 Ramdisk Mod Script
## SukiSU-Ultra + SUSFS 2.4.0 + NoMount for Samsung Galaxy A22 5G (SM-A226B / a22x)

### AnyKernel setup
# global properties
properties() { '
kernel.string=SukiSU-Ultra + SUSFS 2.4.0 + NoMount by A226B
do.devicecheck=0
do.modules=1
do.systemless=1
do.cleanup=1
do.cleanuponabort=0
device.name1=a22x
device.name2=SM-A226B
device.name3=a22xub
device.name4=a22xeur
device.name5=
supported.versions=
supported.patchlevels=
supported.vendorpatchlevels=
'; } # end properties

### AnyKernel install
## boot files attributes
boot_attributes() {
set_perm_recursive 0 0 755 644 $RAMDISK/*;
set_perm_recursive 0 0 750 750 $RAMDISK/init* $RAMDISK/sbin;
} # end attributes

# boot shell variables
BLOCK=/dev/block/by-name/boot;
IS_SLOT_DEVICE=0;
RAMDISK_COMPRESSION=auto;
PATCH_VBMETA_FLAG=auto;

# import functions/variables and setup patching - see for reference (DO NOT REMOVE)
. tools/ak3-core.sh;

# boot install
dump_boot;

write_boot;
## end boot install
