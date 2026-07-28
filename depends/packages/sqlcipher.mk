package=sqlcipher
$(package)_version=4.5.6
$(package)_download_path=https://github.com/sqlcipher/sqlcipher/archive/refs/tags
$(package)_file_name=v$($(package)_version).tar.gz
$(package)_sha256_hash=e4a527e38e67090c1d2dc41df28270d16c15f7ca5210a3e7ec4c4b8fda36e28f
$(package)_dependencies=openssl

define $(package)_set_vars
$(package)_cflags=-DSQLITE_HAS_CODEC -DSQLITE_TEMP_STORE=2 -DSQLITE_EXTRA_INIT=sqlcipher_extra_init -DSQLITE_EXTRA_SHUTDOWN=sqlcipher_extra_shutdown
$(package)_config_opts=--disable-shared --enable-static --enable-tempstore=yes
$(package)_config_opts+=--disable-shell --disable-readline
$(package)_config_opts+=--with-crypto-lib=none
$(package)_config_opts_linux=--with-pic
endef

define $(package)_preprocess_cmds
  sed -i.old 's/SQLITE_TEMP_STORE 1/SQLITE_TEMP_STORE 2/' configure || true
endef

define $(package)_config_cmds
  $($(package)_autoconf) CFLAGS="$$($(package)_cflags)" CPPFLAGS="$$($(package)_cppflags)" LDFLAGS="$$($(package)_ldflags) -lcrypto"
endef

define $(package)_build_cmds
  $(MAKE) libsqlcipher.la
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) lib_install && mkdir -p $($(package)_staging_prefix_dir)/include/sqlcipher $($(package)_staging_prefix_dir)/lib/pkgconfig && cp sqlite3.h sqlite3ext.h $($(package)_staging_prefix_dir)/include/sqlcipher/ && cp sqlcipher.pc $($(package)_staging_prefix_dir)/lib/pkgconfig/
endef
