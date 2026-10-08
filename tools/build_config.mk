# Caller sets CONFIG_STAMP, CONFIG_ARGS, CONFIG_OUTPUTS and CONFIG_PATTERNS
# after defining all object/output lists. CONFIG_ARGS uses quote_config below
# for each complete argument (including embedded spaces). Parsing is read-only.

PYTHON ?= python3
CONFIG_INCLUDE := $(lastword $(MAKEFILE_LIST))
CONFIG_HELPER := $(dir $(CONFIG_INCLUDE))build_config.py
quote_config = '$(subst ','"'"',$(1))'
CONFIG_ARGS += --file=$(call quote_config,$(firstword $(MAKEFILE_LIST))) \
               --file=$(call quote_config,$(CONFIG_INCLUDE)) \
               --file=$(call quote_config,$(CONFIG_HELPER))
CONFIG_CHANGED := $(shell $(PYTHON) $(call quote_config,$(CONFIG_HELPER)) \
  $(call quote_config,$(CONFIG_STAMP)) --check $(CONFIG_ARGS))
ifeq ($(filter changed unchanged,$(CONFIG_CHANGED)),)
$(error Cannot check build configuration)
endif

.PHONY: config-force
config-force:

$(CONFIG_STAMP): config-force
	$(PYTHON) $(call quote_config,$(CONFIG_HELPER)) $(call quote_config,$@) --update $(CONFIG_ARGS) \
	  $(foreach pattern,$(CONFIG_PATTERNS),--remove=$(call quote_config,$(pattern)))

$(CONFIG_OUTPUTS): $(CONFIG_STAMP)

ifeq ($(CONFIG_CHANGED),changed)
# Force the current requested outputs even if system make has cached their
# timestamps before the stamp recipe invalidates them. Other outputs disappear
# so a subsequent partial build cannot mistake their old configuration for new.
$(CONFIG_OUTPUTS): config-force
endif
