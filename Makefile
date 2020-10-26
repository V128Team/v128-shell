include common.mk

MODULES := wm
$(foreach mod,$(MODULES),$(eval $(call build_module,$(mod))))
