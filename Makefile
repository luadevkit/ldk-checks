LUA ?= lua

sed = $(LUA) scripts/sed.lua

package = ldk-checks
version = 0.1.0-1
rockspec = rockspecs/$(package)-$(version).rockspec
sources = csrc/checks.c

makefile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
cwd := $(patsubst %/,%,$(dir $(makefile_path)))
cwd_unix = $(shell $(LUA) scripts/mkpath.lua $(cwd))

circleci = 	docker run --interactive --tty --rm \
	--volume //var/run/docker.sock:/var/run/docker.sock \
	--volume $(cwd):$(cwd_unix) \
	--volume $(HOME)/.circleci:/root/.circleci \
	--workdir $(cwd_unix) \
	circleci/picard@sha256:f340abec0d267609a4558a0ff74538227745ef350ffb664e9dbb39b1dfc20100 \
	circleci

.PHONY: $(rockspec) spec docs lint

default: spec

build-aux/config.ld: build-aux/config.ld.in
	$(sed) build-aux/config.ld.in build-aux/config.ld PACKAGE_NAME=$(package) PACKAGE_VERSION=$(version)

docs: build-aux/config.ld
	ldoc -c build-aux/config.ld .

$(rockspec): rockspecs/$(package)-scm.rockspec.in
	$(sed) rockspecs/$(package)-scm.rockspec.in $(rockspec) PACKAGE_NAME=$(package) PACKAGE_VERSION=$(version)

lint: $(rockspec)
	luacheck $(rockspec)
	luacheck spec

spec:
	busted -o plainTerminal

install-deps: $(rockspec)
	luarocks install --only-deps $(rockspec)

build: $(rockspec)
	luarocks make $(rockspec)

upload:
	luarocks upload --temp-key=$(LDK_LUAROCKS_KEY) $(rockspec)

 circleci-build:
	$(circleci) build --job build
