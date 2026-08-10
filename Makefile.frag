# sapi/phpweb/Makefile.frag

phpweb: $(SAPI_PHPWEB_PATH)

$(SAPI_PHPWEB_PATH): $(PHP_GLOBAL_OBJS) $(PHP_BINARY_OBJS) $(PHP_PHPWEB_OBJS)
	$(BUILD_PHPWEB)

install-phpweb: $(SAPI_PHPWEB_PATH)
	@echo "Installing phpweb binary: $(INSTALL_ROOT)$(bindir)/"
	@$(mkinstalldirs) $(INSTALL_ROOT)$(bindir)
	@$(INSTALL) -m 0755 $(SAPI_PHPWEB_PATH) $(INSTALL_ROOT)$(bindir)/$(program_prefix)phpweb$(program_suffix)
	@$(LIBTOOL) --mode=install $(INSTALL) -m 0755 $(SAPI_CLI_PATH) $(INSTALL_ROOT)$(bindir)/$(program_prefix)php$(program_suffix)$(EXEEXT)
