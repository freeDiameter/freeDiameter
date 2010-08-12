#
# Software License Agreement (BSD License)
#
# Copyright (c) 2010, WIDE Project and NICT
# All rights reserved.
#
# See LICENSE file from freeDiameter source package for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=lksctp-tools
PKG_VERSION:=1.0.11
PKG_RELEASE:=1

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=@SF/lksctp/
# PKG_MD5SUM:=

PKG_BUILD_DEPENDS:=binutils
PKG_FIXUP:=libtool
PKG_INSTALL:=1

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/kernel.mk

define KernelPackage/sctp
  SUBMENU:=Network Support
  TITLE:=SCTP support
  KCONFIG:= CONFIG_IP_SCTP=m
  FILES:=$(LINUX_DIR)/net/sctp/sctp.$(LINUX_KMOD_SUFFIX)
  AUTOLOAD:=$(call AutoLoad,20,sctp)
  DEPENDS:=@LINUX_2_6
endef

define KernelPackage/sctp/description
 Kernel modules for SCTP support
endef

define Package/sctp
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=SCTP user-land library
  URL:=http://lksctp.sourceforge.net
  DEPENDS:=+kmod-sctp
endef

define Package/sctp/description
 SCTP user-land library for sctp_* functions (when socket API is too limited).
endef

TARGET_LDFLAGS := -L$(STAGING_DIR)/usr/lib $(TARGET_LDFLAGS)

define Package/sctp/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) \
		$(PKG_INSTALL_DIR)/usr/lib/libsctp.so* \
		$(1)/usr/lib/
endef

$(eval $(call BuildPackage,sctp))
$(eval $(call KernelPackage,sctp))
