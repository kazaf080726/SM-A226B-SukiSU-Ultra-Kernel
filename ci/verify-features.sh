#!/bin/bash
# Feature gate: fail the release if any promised feature is not actually
# compiled into the built kernel / packaged zip.
#
# This is deliberately stricter than ci/verify.sh (which checks image size and
# PathMask vermagic): every feature gets a config check, a symbol check and,
# where applicable, an Image string / packaged-zip check.
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${OUT:-$ROOT/out}"
VMLINUX="$OUT/vmlinux"
IMAGE="$OUT/arch/arm64/boot/Image.gz"
CONFIG="$OUT/.config"
DIST="$ROOT/dist"

fail=0
pass=0

ok()  { printf '  \033[32m✓\033[0m %s\n' "$1"; pass=$((pass + 1)); }
bad() { printf '  \033[31m✗\033[0m %s\n' "$1"; fail=$((fail + 1)); }

[ -f "$VMLINUX" ] || { echo "missing $VMLINUX"; exit 1; }
[ -f "$CONFIG" ]  || { echo "missing $CONFIG"; exit 1; }
if [ ! -f "$IMAGE" ]; then
    IMAGE="$OUT/arch/arm64/boot/Image"
fi
[ -f "$IMAGE" ] || { echo "missing Image/Image.gz"; exit 1; }

NM_LIST=$(mktemp)
STR_LIST=$(mktemp)
IMG_LIST=$(mktemp)
trap 'rm -f "$NM_LIST" "$STR_LIST" "$IMG_LIST"' EXIT

nm "$VMLINUX" 2>/dev/null > "$NM_LIST"
strings -a "$VMLINUX" 2>/dev/null > "$STR_LIST"
if [[ "$IMAGE" == *.gz ]]; then
    zcat "$IMAGE" 2>/dev/null | strings -a > "$IMG_LIST"
else
    strings -a "$IMAGE" 2>/dev/null > "$IMG_LIST"
fi

has_sym() { grep -qw -- "$1" "$NM_LIST"; }
has_str() { grep -qF -- "$1" "$IMG_LIST"; }
has_vstr() { grep -qF -- "$1" "$STR_LIST"; }

echo "=== 1. config options ==="
for c in \
    CONFIG_KSU \
    CONFIG_KSU_SUSFS \
    CONFIG_KSU_SUSFS_SUS_PATH \
    CONFIG_KSU_SUSFS_SUS_MOUNT \
    CONFIG_KSU_SUSFS_SUS_KSTAT \
    CONFIG_KSU_SUSFS_SPOOF_UNAME \
    CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG \
    CONFIG_KSU_SUSFS_OPEN_REDIRECT \
    CONFIG_KSU_SUSFS_SUS_MAP \
    CONFIG_KSU_SUSFS_HIDE_KSU_SUSFS_SYMBOLS \
    CONFIG_KSU_SUSFS_PRIORITY_UID_FILTER \
    CONFIG_NOMOUNT \
    CONFIG_SECURITY_SELINUX_QUERY_FILTER \
    CONFIG_SECURITY_SELINUX_HOOK_KPM \
    CONFIG_KALLSYMS_ALL
do
    if grep -qx "${c}=y" "$CONFIG"; then ok "$c"; else bad "$c is not =y"; fi
done

echo
echo "=== 2. SukiSU-Ultra ==="
for s in kernelsu_init ksu_allow ksu_extension_susfs_ops; do
    if has_sym "$s"; then ok "symbol $s"; else bad "missing symbol $s"; fi
done
if has_vstr 'v3.0.0'; then ok "KSU version string v3.0.0"; else bad "KSU version string missing"; fi

echo
echo "=== 3. SUSFS v2.4.0 ==="
for s in susfs_add_sus_path susfs_is_inode_sus_path susfs_add_open_redirect; do
    if has_sym "$s"; then ok "symbol $s"; else bad "missing symbol $s"; fi
done
if has_str 'v2.4.0'; then ok "SUSFS version string v2.4.0"; else bad "SUSFS version string missing"; fi

echo
echo "=== 4. NoMount v2.3.0 ==="
for s in nomount_init nm_hijacked_lookup nm_d_revalidate nm_dir_lookup nm_clear_state; do
    if has_sym "$s"; then ok "symbol $s"; else bad "missing symbol $s"; fi
done
if has_str 'NoMount: Loaded successfully'; then ok "NoMount init string"; else bad "NoMount init string missing"; fi

echo
echo "=== 5. selinux_hook / KPM engine ==="
for s in \
    hook_wrap kp_hook_before kp_hook_after kp_hook_find kp_hooks \
    snapshot_clean_policy try_load_clean_policydb_from_blob \
    selinux_hook_kpm_builtin_init dirtysepolicy_access_should_deny
do
    if has_sym "$s"; then ok "symbol $s"; else bad "missing symbol $s"; fi
done
if has_str '[selinux_hook]'; then ok "selinux_hook log strings"; else bad "selinux_hook log strings missing"; fi

echo
echo "=== 6. 12 wired hook targets ==="
for s in \
    sel_write_access sel_write_context sel_read_handle_status sel_mmap_handle_status \
    security_read_policy simple_read_from_buffer selinux_complete_init \
    selinux_setprocattr security_setprocattr selinux_status_update_policyload \
    context_struct_compute_av string_to_context_struct
do
    if has_sym "$s"; then ok "hook target $s"; else bad "hook target $s not compiled"; fi
done

echo
echo "=== 7. native SELinux filter (v4 layer) ==="
for s in selinux_query_filter_access selinux_query_filter_context selinux_query_filter_setprocattr; do
    if has_sym "$s"; then ok "symbol $s"; else bad "missing symbol $s"; fi
done

echo
echo "=== 8. anti-detection markers in Image ==="
kpimg=$(grep -icE 'kpimg|KernelPatch' "$IMG_LIST" || true)
[ "$kpimg" -eq 0 ] && ok "no kpimg/KernelPatch markers" || bad "found $kpimg kpimg/KernelPatch markers"

oldfp=$(grep -cF '0 0 0 0 1 0' "$IMG_LIST" || true)
[ "$oldfp" -eq 0 ] && ok "old /access fingerprint absent" || bad "old /access fingerprint present ($oldfp)"

newfp=$(grep -cF '0 ffffffff 0 ffffffff 1 0' "$IMG_LIST" || true)
[ "$newfp" -ge 1 ] && ok "realistic /access reply present" || bad "realistic /access reply missing"

echo
echo "=== 9. PathMask LKMs ==="
rel=$(cat "$OUT/include/config/kernel.release" 2>/dev/null || echo '?')
expected=$(grep -m1 -E "^${rel} SMP.* aarch64$" "$STR_LIST" || true)
[ -n "$expected" ] || expected="${rel} SMP preempt mod_unload modversions aarch64"
for ko in "$ROOT/pathmask/src/pathmask.ko" "$ROOT/pathmask/src/procguard.ko"; do
    if [ ! -f "$ko" ]; then bad "missing $(basename "$ko")"; continue; fi
    got=$(strings -a "$ko" | grep -m1 '^vermagic=' | cut -d= -f2-)
    if [ "$got" = "$expected" ]; then ok "$(basename "$ko") vermagic"; else bad "$(basename "$ko") vermagic '$got' != '$expected'"; fi
done
if nm "$ROOT/pathmask/src/pathmask.ko" 2>/dev/null | grep -qw getattr_entry && \
   nm "$ROOT/pathmask/src/pathmask.ko" 2>/dev/null | grep -qw getdents_entry; then
    ok "pathmask hooks present"
else
    bad "pathmask hooks missing"
fi

echo
echo "=== 10. packaged AnyKernel3 ==="
AK3=$(ls -1t "$DIST"/A226B-SukiSU-SUSFS240-NoMount-*.zip 2>/dev/null | grep -v -- '-modules-' | head -n1 || true)
if [ -z "$AK3" ]; then
    bad "no AnyKernel3 zip under dist/"
else
    if unzip -tq "$AK3" >/dev/null 2>&1; then ok "zip integrity: $(basename "$AK3")"; else bad "zip is corrupt: $(basename "$AK3")"; fi
    if unzip -p "$AK3" anykernel.sh 2>/dev/null | grep -q 'do.devicecheck=0'; then ok "do.devicecheck=0"; else bad "do.devicecheck=0 missing"; fi
    if unzip -p "$AK3" anykernel.sh 2>/dev/null | grep -q 'BLOCK=/dev/block/by-name/boot'; then ok "BLOCK=/dev/block/by-name/boot"; else bad "BLOCK path missing"; fi
    zhash=$(unzip -p "$AK3" Image.gz 2>/dev/null | sha256sum | cut -d' ' -f1)
    ihash=$(sha256sum "$IMAGE" | cut -d' ' -f1)
    if [ "$zhash" = "$ihash" ]; then ok "embedded Image.gz matches build"; else bad "embedded Image.gz differs from $IMAGE"; fi
fi
if ls "$DIST"/A226B-PathMask-*.zip >/dev/null 2>&1; then ok "PathMask zip present"; else bad "PathMask zip missing"; fi

echo
if [ "$fail" -ne 0 ]; then
    echo "FEATURE_VERIFY_FAILED: $fail failed, $pass passed"
    exit 1
fi
echo "FEATURE_VERIFY_OK: $pass checks passed"
