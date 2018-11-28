@echo off

dcamprof make-target -c BF-U3-ssf.json -p ektaspace-grid -p white -b 0.004 BF-U3-ektaspace-grid.ti3
dcamprof make-profile -c BF-U3-ssf.json -i StdA BF-U3-ektaspace-grid.ti3 BF-U3-ektaspace-grid-StdA.json > BF-U3-ektaspace-grid-StdA.txt 2>&1
dcamprof make-profile -c BF-U3-ssf.json -i D50  BF-U3-ektaspace-grid.ti3 BF-U3-ektaspace-grid-D50.json > BF-U3-ektaspace-grid-D50.txt 2>&1
dcamprof make-profile -c BF-U3-ssf.json -i D65  BF-U3-ektaspace-grid.ti3 BF-U3-ektaspace-grid-D65.json > BF-U3-ektaspace-grid-D65.txt 2>&1
dcamprof make-profile -c BF-U3-ssf.json -i D75  BF-U3-ektaspace-grid.ti3 BF-U3-ektaspace-grid-D75.json > BF-U3-ektaspace-grid-D75.txt 2>&1
dcamprof make-dcp BF-U3-ektaspace-grid-D50.json BF-U3-ektaspace-grid-lut.dcp
dcamprof make-dcp -L BF-U3-ektaspace-grid.json BF-U3-ektaspace-grid.dcp
dcamprof make-dcp BF-U3-ektaspace-grid-StdA.json BF-U3-ektaspace-grid-D65.json BF-U3-ektaspace-grid-dual-lut.dcp
dcamprof make-dcp -L BF-U3-ektaspace-grid-StdA.json BF-U3-ektaspace-grid-D65.json BF-U3-ektaspace-grid-dual.dcp
dcptool BF-U3-ektaspace-grid-dual-lut.dcp BF-U3-ektaspace-grid-dual-lut.xml
dcptool BF-U3-ektaspace-grid-dual.dcp BF-U3-ektaspace-grid-dual.xml


dcamprof make-target -c BF-U3-ssf.json -p pointer -p white -b 0.004 BF-U3-pointer.ti3
dcamprof make-profile -c BF-U3-ssf.json BF-U3-pointer.ti3 BF-U3-pointer.json > BF-U3-pointer.txt 2>&1
dcamprof make-dcp BF-U3-pointer.json BF-U3-pointer-lut.dcp
dcamprof make-dcp -L BF-U3-pointer.json BF-U3-pointer.dcp

dcamprof make-target -c BF-U3-ssf.json -p pointer-grid -p white -b 0.004 BF-U3-pointer-grid.ti3
dcamprof make-profile -c BF-U3-ssf.json BF-U3-pointer-grid.ti3 BF-U3-pointer-grid.json > BF-U3-pointer-grid.txt 2>&1
dcamprof make-dcp BF-U3-pointer-grid.json BF-U3-pointer-grid-lut.dcp
dcamprof make-dcp -L BF-U3-pointer-grid.json BF-U3-pointer-grid.dcp


dcamprof make-target -c BF-U3-ssf.json -p adobergb -p white -b 0.004 BF-U3-adobergb.ti3
dcamprof make-profile -c BF-U3-ssf.json BF-U3-adobergb.ti3 BF-U3-adobergb.json > BF-U3-adobergb.txt 2>&1
dcamprof make-dcp BF-U3-adobergb.json BF-U3-adobergb-lut.dcp
dcamprof make-dcp -L BF-U3-adobergb.json BF-U3-adobergb.dcp

dcamprof make-target -c BF-U3-ssf.json -p adobergb-grid -p white -b 0.004 BF-U3-adobergb-grid.ti3
dcamprof make-profile -c BF-U3-ssf.json BF-U3-adobergb-grid.ti3 BF-U3-adobergb-grid.json > BF-U3-adobergb-grid.txt 2>&1
dcamprof make-dcp BF-U3-adobergb-grid.json BF-U3-adobergb-grid-lut.dcp
dcamprof make-dcp -L BF-U3-adobergb-grid.json BF-U3-adobergb-grid.dcp

dcamprof make-target -c BF-U3-ssf.json -p srgb -p white -b 0.004 BF-U3-srgb.ti3
dcamprof make-profile -c BF-U3-ssf.json BF-U3-srgb.ti3 BF-U3-srgb.json > BF-U3-srgb.txt 2>&1
dcamprof make-dcp BF-U3-srgb.json BF-U3-srgb-lut.dcp
dcamprof make-dcp -L BF-U3-srgb.json BF-U3-srgb.dcp

dcamprof make-target -c BF-U3-ssf.json -p srgb-grid -p white -b 0.004 BF-U3-srgb-grid.ti3
dcamprof make-profile -c BF-U3-ssf.json BF-U3-srgb-grid.ti3 BF-U3-srgb-grid.json > BF-U3-srgb-grid.txt 2>&1
dcamprof make-dcp BF-U3-srgb-grid.json BF-U3-srgb-grid-lut.dcp
dcamprof make-dcp -L BF-U3-srgb-grid.json BF-U3-srgb-grid.dcp

dcamprof make-target -c BF-U3-ssf.json -p cc24 -p white -b 0.004 BF-U3-cc24.ti3
dcamprof make-profile -c BF-U3-ssf.json BF-U3-cc24.ti3 BF-U3-cc24.json > BF-U3-cc24.txt 2>&1
dcamprof make-dcp BF-U3-cc24.json BF-U3-cc24-lut.dcp
dcamprof make-dcp -L BF-U3-cc24.json BF-U3-cc24.dcp

dcamprof make-target -c BF-U3-ssf.json -p kuopio-natural -p white -b 0.004 BF-U3-kuopio-natural.ti3
dcamprof make-profile -c BF-U3-ssf.json BF-U3-kuopio-natural.ti3 BF-U3-kuopio-natural.json > BF-U3-kuopio-natural.txt 2>&1
dcamprof make-dcp BF-U3-kuopio-natural.json BF-U3-kuopio-natural-lut.dcp
dcamprof make-dcp -L BF-U3-kuopio-natural.json BF-U3-kuopio-natural.dcp

dcamprof make-target -c BF-U3-ssf.json -p munsell -p white -b 0.004 BF-U3-munsell.ti3
dcamprof make-profile -c BF-U3-ssf.json BF-U3-munsell.ti3 BF-U3-munsell.json > BF-U3-munsell.txt 2>&1
dcamprof make-dcp BF-U3-munsell.json BF-U3-munsell-lut.dcp
dcamprof make-dcp -L BF-U3-munsell.json BF-U3-munsell.dcp

dcamprof make-target -c BF-U3-ssf.json -p munsell-bright -p white -b 0.004 BF-U3-munsell-bright.ti3
dcamprof make-profile -c BF-U3-ssf.json BF-U3-munsell-bright.ti3 BF-U3-munsell-bright.json > BF-U3-munsell-bright.txt 2>&1
dcamprof make-dcp BF-U3-munsell-bright.json BF-U3-munsell-bright-lut.dcp
dcamprof make-dcp -L BF-U3-munsell-bright.json BF-U3-munsell-bright.dcp
