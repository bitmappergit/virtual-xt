# VirtualXT

A portable IBM PC/XT emulator written in C.

This project is a merge of Andreas Jonsson's VPC16 emulator and Adrian Cable's excellent 8086tiny emulator.

## Emulated Hardware

* Intel 8086 CPU
* 1MB RAM
* 3.5" floppy disk controller (720KB/1.44MB)
* Disk controller (hard drive size of up to 528MB)
* CGA/Hercules graphics card with 720x348 2-color and 320x200 4-color graphics, and CGA 80 x 25 16-color text mode support
* Keyboard controller with 83-key XT-style keyboard
* PC speaker

## Building a Hard Disk Image

* Generate a flat file (under 528MB), filled with zero bytes. `fsutil file createnew hd.img 10485760`

* Boot the emulator with a floppy, run `FDISK` to partition the hard disk and reboot.

* Format the disk using `FORMAT C:` (or `FORMAT C: /S` to create a bootable disk)

If you want to modify the disk image from your host system you can use [OSFMount](https://www.osforensics.com/tools/mount-disk-images.html).

## TODO

* Serial Mouse
* EGA Graphics
* AdLib Sound (unlikely)
* 80286 Instructionset (unlikely)