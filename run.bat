@echo off

@echo Building program...
gcc src/tsv_main.c -o bin/tsv_main.exe


@echo Running program with %1 probability... 
bin\tsv_main.exe %1