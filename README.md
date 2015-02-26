# CSVR - wrapper utility for libcsv
This is a command line utility that uses [libcsv](http://sourceforge.net/projects/libcsv/) to parse CSV files and print the value of a given field. 

## Installation

    $ make
    $ chmod +x csvr
    $ [sudo] cp csvr /usr/bin

## Usage

    csvr -r <row> -c <column> | <column name> [<file>] 

-r: The row offset (starting with 0) to seek
-c: The column offset, or name of the column, to seek. If a string is given, the fist row of the CSV will be treated as the header row. 
<file>: If no filename is supplied, parsing will be attempted on the standard input.

If the specified value exists, it will be read to the standard output.

At some point, I will add the ability to set a field's value.

## Changelog
### 0.1
* Added ability to read a specified field
