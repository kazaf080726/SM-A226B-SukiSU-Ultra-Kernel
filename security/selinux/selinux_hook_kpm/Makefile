# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_SECURITY_SELINUX_HOOK_KPM) += selinux_hook.o kpm_hooks.o

ccflags-y += -I$(objtree)/security/selinux
ccflags-y += -I$(srctree)/security/selinux
ccflags-y += -I$(srctree)/security/selinux/include
ccflags-y += -I$(srctree)/security/selinux/selinux_hook_kpm
