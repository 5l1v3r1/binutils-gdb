#name: Discarded dynamic relocation section
#ld: -shared -T reloc-discard.ld
#readelf: -r --use-dynamic
#target: [check_shared_lib_support]
#source: reloc-discard.s
#xfail: nds32*-*-* tic6x-*-*
# Need to figure out how to pass `-fpic' for NDS32 or `-mpic -mpid=near'
# for TI C6X targets to GAS for this test.

There are no dynamic relocations in this file\.
