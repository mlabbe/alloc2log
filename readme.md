# alloc2log #

Log all allocs and frees in a Linux program by intercepting all library calls.  No code recompile necessary; instrument the process, not the code. 

## Project Status ##

Pre-0.1.  Use at your own risk.

## Usage ##

    python3 build.jfdi              # Download build software
	python3 jfdi.py                 # Build
	./run.sh ./bin/linux/alloctest  # run binary, output logs
	ls -t a2l-*.log                 # view resulting allocations log
	
