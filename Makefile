LUA ?= lua

sed = $(LUA) scripts/sed.lua
mkver = $(LUA) scripts/mkver.lua
mkpath = $(LUA) scripts/mkpath.lua

rock_name = ldk-checks
rock_version = $(shell $(mkver))
rockspec = rockspecs/$(rock_name)-$(rock_version)-1.rockspec
sources = csrc/checks.c

makefile_path = $(abspath $(lastword $(MAKEFILE_LIST)))
cwd = $(patsubst %/,%,$(dir $(makefile_path)))
cwd_unix = $(shell $(mkpath) $(cwd))

circleci = 	docker run --interactive --tty --rm \
	--volume //var/run/docker.sock:/var/run/docker.sock \
	--volume $(cwd):$(cwd_unix) \
	--volume $(HOME)/.circleci:/root/.circleci \
	--workdir $(cwd_unix) \
	circleci/picard@sha256:f340abec0d267609a4558a0ff74538227745ef350ffb664e9dbb39b1dfc20100

.PHONY: $(rockspec) spec docs lint

default: spec

build-aux/config.ld: build-aux/config.ld.in
	$(sed) build-aux/config.ld.in build-aux/config.ld ROCK_NAME=$(rock_name) ROCK_VERSION=$(rock_version)

docs: build-aux/config.ld
	ldoc -c build-aux/config.ld .

lint: $(rockspec)
	luacheck $(rockspec)
	luacheck spec

spec:
	busted -o plainTerminal

build: $(rockspec)
	luarocks make $(rockspec)

 circleci-build:
	$(circleci) circleci build --job build

circleci-shell:
	$(circleci)

rockspec-dev:
	$(sed) rockspecs/$(rock_name).rockspec.in rockspecs/$(rock_name)-dev-1.rockspec ROCK_NAME=$(rock_name) ROCK_VERSION=dev

rockspec: $(rockspec)

$(rockspec): rockspecs/$(rock_name).rockspec.in
	$(sed) rockspecs/$(rock_name).rockspec.in $(rockspec) ROCK_NAME=$(rock_name) ROCK_VERSION=$(rock_version)

publish: $(rockspec)
	luarocks upload --temp-key=$(LDK_LUAROCKS_KEY) $(rockspec)
