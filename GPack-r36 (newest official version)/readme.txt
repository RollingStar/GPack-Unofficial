GPack r34


Merge program for related images

To merge:
GPack [global_ops] [merge_ops] directory_containing_images

To unmerge:
GPack [global_ops] [unmerge_ops] some_name.grp

Other functionality:
GPack [global_ops]

[merge_ops]
No compression mode: --nocomp
    Use srep, but don't compress with 7z or tak (almost required for PS3,
    considering how big the images can be)

Quick compression mode: -q --quick
    Use weak compression settings (-mx1). Recommended when merging large files

Normal compression mode: --normal (default)
    Use reasonable compression settings (-mx5). Recommended for average merges

Slow compression mode: --slow
    Use strong compression settings (-mx9). Use only for small merges (CDs)

[unmerge_ops]
Test mode: -t --test
    Verify the integrity of the images contained in a merge

Unpack by index: -ui=#,...,# --unpack-index=#,...,#
    Comma delimited list containing index of image in grp to unpack.
    For example, -u=0,3,4 specifies that images on the 1st, 4th and 5th
    lines of the .grp should be unpacked

Unpack by tags: -ut=#,...,# --unpack-tags=#,...,#
    Comma delimited list containing tags to unpack.
    For example, -ut=!,pal specifies that any images with a tag of "!"
    or "pal" should be unpacked
    Wrap the entire -ut option in quotes if any tags have spaces

Unpack by partial filename matches: -um=#,...,# --unpack-matching=#,...,#
    Comma delimited list containing partial matches to filename to unpack.
    For example, -u=USA specifies that any images containing "USA"
    in the filename should be unpacked
    Wrap the entire -um option in quotes if any filename matches have spaces

Delete merged files: -d --delete
    Delete merged files if program executes successfully.
    NOTE: use at your own risk. Program may execute successfully yet not unpack
          every image. Incompatible with test or partial unpack options

[global_ops]
Reset ini: -r --reset-ini
    Overwrite existing options.ini file with default settings

Don't pause on completion: -s --script
    To be used when no user interaction is desired (ie from a script). Default
    behaviour is to make user press enter to exit program so that those merging
    by dragging have a chance to read the command window for status report

Set output directory: -o path
    Define where to put the output files of an encode or decode (default is the
    same directory as input

Output of a compressed merge is as follows:
 Some_Name.grp - Text file containing names, sizes and hashes of images
 Some_Name.7z  - contains compressed data
 Some_Name.tak - contains compressed audio data (present only if there's audio)
 Some_Name.pad - contains uncompressed random padding (present only if there's
                 random padding). Random padding may be present if the merge
                 contains images from systems handled in a special way

Output of an uncompressed merge is the same, except Some_Name.7z and
Some_Name.tak are not present. Instead, Some_Name.*.srep files are present,
where * could be many things dependent on the files being merged (def, 2048,
2324, 2336, 2352, 96, system)

Notes:
 <1> A WARNING indicates that a non-critical routine failed, execution
     continues as it may be recoverable. Does not affect resulting images
 <2> An ERROR indicates that a critical routine failed, execution stops.
     Any resulting images if created should be checked
 <3> Temporary files may need to be manually removed in the event of a
     WARNING or an ERROR
 <4> Images are deleted on a successful merge
 <5> Ensure you have enough hdd space before merging or unmerging
 <6> Always make a backup of data before use, to avoid potential data loss

