LUA ?= lua

ver = $(LUA) scripts/ver.lua
format = $(LUA) scripts/format.lua
tidy = $(LUA) scripts/tidy.lua

rock_name = ldk-checks
rock_version = $(shell $(ver))
rockspec = rockspecs/$(rock_name)-$(rock_version)-1.rockspec
rockspec_dev = rockspecs/$(rock_name)-dev-1.rockspec
release_tag = v$(rock_version)

.PHONY: rockspec spec docs

default: spec

docs: build-aux/config.ld
	ldoc -c build-aux/config.ld .

lint: rockspec $(rockspec-dev)
	luacheck --quiet --formatter plain spec

spec: build
	luarocks test

install: $(rockspec)
	luarocks make --local $(rockspec)

build: $(rockspec-dev)
	luarocks make --local --no-install

publish: rockspec
	luarocks upload --temp-key=$(LDK_LUAROCKS_KEY) $(rockspec)

publish-force: rockspec
	luarocks upload --force --temp-key=$(LDK_LUAROCKS_KEY) $(rockspec)

changelog:
	git-chglog --output CHANGELOG.md --next-tag v$(rock_version)
	$(tidy) CHANGELOG.md

rockspec: $(rockspec_dev)
	luarocks new_version --dir rockspecs --tag $(release_tag) $(rockspec_dev)

pre-checkin: format rockspec docs changelog

major-version:
	$(ver) new major

minor-version:
	$(ver) new minor

patch-version:
	$(ver) new patch

tag:
	git tag -f $(release_tag)

untag:
	git tag -d $(release_tag)

format:
	$(format) $(rockspec_dev)

help:
	@echo Available targets:
	@echo "help                 Prints this help."
	@echo "docs                 Regenerates the rock documentaon."
	@echo "lint                 Runs the linter on the rockspec and all Lua code."
	@echo "spec                 Runs the test suite."
	@echo "install              Installs the rocks."
	@echo "build                Builds the rocks."
	@echo "publish              Publishes the rock."
	@echo "publish-force        Publishes the rock (force)."
	@echo "changelog            Regenerates CHANGELOG.md."
	@echo "rockspec             Creates the rockspec for the current version."
	@echo "pre-checkin          Runs the pre-checkin tasks."
	@echo "major-version        Increments the major version."
	@echo "minor-version        Increments the minor version."
	@echo "patch-version        Increments the patch version."
	@echo "tag                  Adds a release tag."
	@echo "untag                Removes the release tag."
	@echo "format               Formats the source code."
