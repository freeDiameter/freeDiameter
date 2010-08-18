#
# Software License Agreement (BSD License)
#
# Copyright (c) 2010, WIDE Project and NICT
# All rights reserved.
#
# See LICENSE file from freeDiameter source package for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=freeDiameter
PKG_REV:=502
PKG_VERSION:=r$(PKG_REV)
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=hg
PKG_SOURCE_VERSION:=$(PKG_REV)
PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_VERSION)
PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=http://www.freediameter.net/hg/freeDiameter
# PKG_MD5SUM:=

PKG_FIXUP:=libtool
PKG_INSTALL:=1

include $(INCLUDE_DIR)/package.mk

define Package/freeDiameter
  SECTION:=freeDiameter
  CATEGORY:=Network
  TITLE:=freeDiameter
  URL:=http://www.freediameter.net
  DEPENDS:=+sctp +libgnutls +libpthread +kmod-ipv6
endef

define Package/freeDiameter/description
 freeDiameter + RADIUS/Diameter gateway extension package.
endef

define Package/freeDiameter/conffiles
/etc/freeDiameter/freeDiameter.conf
/etc/freeDiameter/rgw.conf
endef

define Build/Configure
    IN_OPENWRT=1 \
    AR="$(TARGET_CROSS)ar" \
    AS="$(TARGET_CC) -c $(TARGET_CFLAGS)" \
    LD="$(TARGET_CROSS)ld" \
    NM="$(TARGET_CROSS)nm" \
    CC="$(TARGET_CC)" \
    GCC="$(TARGET_CC)" \
    CXX="$(TARGET_CROSS)g++" \
    RANLIB="$(TARGET_CROSS)ranlib" \
    STRIP="$(TARGET_CROSS)strip" \
    OBJCOPY="$(TARGET_CROSS)objcopy" \
    OBJDUMP="$(TARGET_CROSS)objdump" \
    TARGET_CPPFLAGS="$(TARGET_CPPFLAGS)" \
    TARGET_CFLAGS="$(TARGET_CFLAGS)" \
    TARGET_LDFLAGS="$(TARGET_LDFLAGS)" \
    cmake \
		-DCMAKE_PREFIX_PATH:PATH=$(STAGING_DIR)/usr \
    		-DCMAKE_INSTALL_PREFIX:PATH=/usr \
                -DBUILD_TESTING:BOOL=OFF \
		-DCMAKE_BUILD_TYPE:STRING=DebianPackage \
		-DDEFAULT_CONF_PATH:PATH=/etc/freeDiameter \
		-DBUILD_APP_RADGW:BOOL=ON \
		-DBUILD_DBG_MONITOR:BOOL=ON \
		VERBOSE=1 \
	$(PKG_BUILD_DIR)/CMakeLists.txt 
endef

TARGET_LDFLAGS := -L$(STAGING_DIR)/usr/lib $(TARGET_LDFLAGS)
define Package/freeDiameter/install
	# binaries
	$(INSTALL_DIR) $(1)/usr/bin
	$(CP) \
		$(PKG_INSTALL_DIR)/usr/bin/freeDiameterd* \
		$(1)/usr/bin/
	# libraries & extensions
	$(INSTALL_DIR) $(1)/usr/lib/
	$(CP) \
		$(PKG_INSTALL_DIR)/usr/lib/* \
		$(1)/usr/lib/
	
	# configuration files
	$(INSTALL_DIR) $(1)/etc/freeDiameter
	$(INSTALL_CONF) \
		$(PKG_BUILD_DIR)/doc/freediameter.conf.sample \
		$(1)/etc/freeDiameter/freeDiameter.conf
	$(SED) 's,TLS_Cred,#TLS_Cred,g' 		   $(1)/etc/freeDiameter/freeDiameter.conf
	echo "" 					>> $(1)/etc/freeDiameter/freeDiameter.conf
	echo "### OPENWRT specific"		 	>> $(1)/etc/freeDiameter/freeDiameter.conf
	echo "TLS_Cred = \"/etc/freeDiameter/freeDiameter.pem\", \"/etc/freeDiameter/freeDiameter.key\";" \
							>> $(1)/etc/freeDiameter/freeDiameter.conf
	echo "TLS_DH_Bits = 768;" 			>> $(1)/etc/freeDiameter/freeDiameter.conf
	echo "LoadExtension = \"dict_nasreq.fdx\";" 	>> $(1)/etc/freeDiameter/freeDiameter.conf
	echo "LoadExtension = \"dict_eap.fdx\";" 	>> $(1)/etc/freeDiameter/freeDiameter.conf
	echo "LoadExtension = \"app_radgw.fdx\":\"rgw.conf\";" \
							>> $(1)/etc/freeDiameter/freeDiameter.conf
	echo "Identity = \"localhost.localdomain\";" 	>> $(1)/etc/freeDiameter/freeDiameter.conf

	
	$(INSTALL_CONF) \
		$(PKG_BUILD_DIR)/doc/app_radgw.conf.sample \
		$(1)/etc/freeDiameter/rgw.conf
	$(SED) 's,RGWX,#RGWX,g' 		  	   $(1)/etc/freeDiameter/rgw.conf
	echo "" 					>> $(1)/etc/freeDiameter/rgw.conf
	echo "### OPENWRT specific"		 	>> $(1)/etc/freeDiameter/rgw.conf
	echo "  RGWX = \"auth.rgwx\" : auth;"		>> $(1)/etc/freeDiameter/rgw.conf
	echo "  RGWX = \"acct.rgwx\" : acct;"		>> $(1)/etc/freeDiameter/rgw.conf
	echo "" 					>> $(1)/etc/freeDiameter/rgw.conf
	echo "  cli = 127.0.0.1 / \"secret key\" ;" 	>> $(1)/etc/freeDiameter/rgw.conf
	echo "  auth_server_ip4 = 127.0.0.1;" 		>> $(1)/etc/freeDiameter/rgw.conf
	echo "  auth_server_ip6 = ::1 ;" 		>> $(1)/etc/freeDiameter/rgw.conf
	echo "  acct_server_ip4 = 127.0.0.1;" 		>> $(1)/etc/freeDiameter/rgw.conf
	echo "  acct_server_ip6 = ::1 ;" 		>> $(1)/etc/freeDiameter/rgw.conf
endef

define Package/freeDiameter/postinst
#!/bin/sh
if [ ! -f "/usr/bin/certtool" ]; then
   echo "certtool is not installed, skipping creation of default certificate."
   exit 0
fi
if [ ! -f "/etc/freeDiameter/freeDiameter.pem" ]; then
   if [ ! -f "/etc/freeDiameter/freeDiameter.key" ]; then 
      echo "Creating a new private key for freeDiameter TLS layer, please wait"
      certtool -p --outfile /etc/freeDiameter/freeDiameter.key
   fi
   echo "Creating a new certificate for freeDiameter TLS layer, please enter the appropriate values for your access point" 
   certtool -s --load-privkey /etc/freeDiameter/freeDiameter.key --outfile /etc/freeDiameter/freeDiameter.pem
fi
endef

$(eval $(call BuildPackage,freeDiameter))
