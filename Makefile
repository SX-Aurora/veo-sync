
GCC = gcc
NCC = /opt/nec/ve/bin/ncc
DEBUG = -g
CFLAGS = -I/opt/nec/ve/veos/include -I. $(DEBUG)
LDSOFLAGS = -pthread -L/opt/nec/ve/veos/lib64 -Wl,-rpath=/opt/nec/ve/veos/lib64 -lveo

ALL: libveo_sync.so test_barrier test_sync libvedummy.so

libveo_sync.so: veo_sync.o
	$(GCC) -shared -fPIC -o $@ $< $(DEBUG) $(LDSOFLAGS) 

veo_sync.o: veo_sync.c veo_sync.h
	$(GCC) -fPIC $(CFLAGS) -o $@ -c $<

test_barrier: test_barrier.c libveo_sync.so veo_sync.h
	$(GCC) -o $@ $< $(CFLAGS) -pthread -L/opt/nec/ve/veos/lib64 -L$(CURDIR) \
		-Wl,-rpath=/opt/nec/ve/veos/lib64 -Wl,-rpath=$(CURDIR) -lveo -lveo_sync

test_sync: test_sync.c libveo_sync.so veo_sync.h
	$(GCC) -o $@ $< $(CFLAGS) -pthread -L/opt/nec/ve/veos/lib64 -L$(CURDIR) \
		-Wl,-rpath=/opt/nec/ve/veos/lib64 -Wl,-rpath=$(CURDIR) -lveo -lveo_sync

libvedummy.so: libvedummy.c
	$(NCC) -shared -fpic -pthread -o $@ $<

test: test_barrier test_sync
	./test_barrier
	./test_sync

clean:
	rm -f *.o *.so test_sync
