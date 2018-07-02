NY: all
all: mstprepare
	make -C common
	make -C server
	make -C client

.PHONY: mstprepare
mstprepare: masstree-beta/GNUMakefile

masstree-beta/GNUMakefile: masstree-beta/bootstrap.sh masstree-beta/GNUmakefile.in
	cd masstree-beta && ./bootstrap.sh && ./configure

.PHONY: clean
clean:
	make -C common clean
	make -C server clean
	make -C client clean
