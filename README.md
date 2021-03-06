# CSVR - wrapper utility for libcsv
This is a command line utility that uses [libcsv](http://sourceforge.net/projects/libcsv/) to parse CSV files and print the value of a given field. 

## Installation

    $ make
    $ chmod +x csvr
    $ [sudo] cp csvr /usr/bin

## Usage

    csvr [-s <value to set>] <row> <column offset/name> [<file>] 

* row: The row offset (starting with 0) to seek
* column offset/name: The column offset, or name of the column, to seek. If a string is given, the fist row of the CSV will be treated as the header row. 
* file: If no filename is supplied, parsing will be attempted on the standard input.
* -s: the value to change the field to (overwrites original file)

If the specified value exists, it will be read to the standard output.

## Changelog

### 0.2.1
* Fixed a bug with writing to CSV
* Fixed a segfault when no options were passed

### 0.2
* Added the ability to modify a field in the CSV with the -s option

### 0.1
* Added ability to read a specified field
