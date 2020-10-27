# 64vgmplay
OPL1/2 VGM converter/player for SFX Sound Expander/FM-YAM.

Making a C64 executable is a multi-step process:
- First, run the "convert" utility, which will create an `.include`able file from a VGM file for use with the assembler. `convert vgmname outname`
- Now, take a look at the bottom of the assembly file, and change the filename after `.include` to the name of the file you just exported. Assemble the file with 64tass.
- You should probably crunch it now with Exomizer or something else. Start address is $080d.

Your VGM file must use at least one OPL1 or OPL2 to be usable. If more than one chip fits these qualifications, you will be given a choice of which one to log. Any other chips' commands will simply be ignored.

Remember to have an SFX Sound Expander/FM-YAM enabled in your emulator/plugged into your machine when you run the file.

Remember that there is only 64k of space available to the C64- if the assembler warns you about processor program counter overflow, your VGM is too large. There is no compression per se, but the data format used by the player will result in data that is about 3/4 the size of the VGM, for any standard single-chip VGM. So, be careful with any files above around 70kb.

This play routine is only intended to generate standalone executables, not for demos. If you want to use FM-enhanced music in a production, consider the Edlib D00 player by Mr. Mouse.
