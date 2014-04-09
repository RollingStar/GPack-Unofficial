IMPORTANT: Needs to be placed in the same directory as GPack.

GPackMerger r2

Create a merged set from a custom dat and uncompressed roms

Usage:
GPackMerger dat dir_containing_roms dir_to_contain_merged_files [log_file]

Tips for use:
<1> The set of uncompressed roms must be correct and match dat (program does
    not correct naming differences). Run through a dat program before
    execution. They must also be in the ROOT of dir_containing_roms

<2> Ensure that there is enough space on the drive which will contain the
    merged files.

<3> If the program detects something wrong (or something you may not want to
    do), it will ask if you want to stop

<4> For very large systems you may have to split into multiple GPackMerger
    executions (some systems can't fit on one hdd, let alone the space
    required for temporary and final merged files
