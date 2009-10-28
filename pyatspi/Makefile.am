pyatspi_PYTHON = \
	__init__.py \
	accessible.py \
	constants.py \
	event.py \
	registry.py \
	utils.py

if RELOCATE
    pyatspidir=$(pyexecdir)/pyatspi-corba/pyatspi
else
    pyatspidir=$(pyexecdir)/pyatspi
endif

if RELOCATE
install-exec-local:
	@$(MKDIR_P) $(DESTDIR)$(pyexecdir)
	cp pyatspi-corba.pth $(DESTDIR)$(pyexecdir)
uninstall-local:
	@rm $(DESTDIR)$(pyexecdir)/pyatspi-corba.pth
endif
