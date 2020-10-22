#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define WAITDIV 2
#define CLOCK 3579545.0

enum CHIPS {OPL1 = 0, OPL2, AMT_CHIPS};
const char* CHIPNAMES[] = {"YM3526 OPL1", "YM3812 OPL2"};
const uint8_t CHIPCLKOFFS[] = {0x54, 0x50};
const uint8_t CHIPCMDS[] = {0x5b, 0x5a};
const uint8_t CHIPDUALCMDS[] = {0xab, 0xaa};



uint32_t get32(uint8_t* p)
{
    return ((*p) | 
            (*(p+1) << 8) |
            (*(p+2) << 16) |
            (*(p+3) << 24)
            );
}

uint32_t get24(uint8_t* p)
{
    return ((*p) | 
            (*(p+1) << 8) |
            (*(p+2) << 16)
            );
}

uint16_t get16(uint8_t* p)
{
    return ((*p) | 
            (*(p+1) << 8)
            );
}


int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        printf("Usage: %s vgmname outname\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    FILE *f = fopen(argv[1], "rb");
    if (!f)
    {
        printf("Could not open input file: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    fseek(f, 0, SEEK_END);
    size_t vgmsize = ftell(f);
    rewind(f);
    uint8_t *vgmdata = malloc(vgmsize);
    fread(vgmdata, 1, vgmsize, f);
    fclose(f);
    
    if (vgmsize < 0x41 || memcmp(vgmdata, "Vgm ", 4))
    {
        puts("Input file is not a VGM file");
        return EXIT_FAILURE;
    }
    if (vgmsize-0x04 != get32(vgmdata+0x04))
    {
        puts("WARNING: VGM size is not the same as reported in header");
    }
    if (get32(vgmdata+0x08) < 0x151)
    {
        puts("VGM files below version 1.51 are not supported");
        return EXIT_FAILURE;
    }
    
    uint8_t chipcnts[AMT_CHIPS];
    uint32_t chipclocks[AMT_CHIPS];
    for (int i = 0; i < AMT_CHIPS; i++)
    {
        uint32_t clk = get32(vgmdata+CHIPCLKOFFS[i]);
        if (clk)
        {
            if (clk & 0x40000000)
                chipcnts[i] = 2;
            else
                chipcnts[i] = 1;
            chipclocks[i] = clk & 0x3fffffff;
        }
        else
            chipcnts[i] = 0;
    }
    
    int chipcnt = 0;
    uint8_t chiplist[AMT_CHIPS*2];
    for (int i = 0; i < AMT_CHIPS; i++)
    {
        int c = chipcnts[i];
        if (c > 0)
            chiplist[chipcnt++] = i;
        if (c > 1)
            chiplist[chipcnt++] = i | 0x80;
    }
    
    if (!chipcnt)
    {
        puts("VGM has no compatible chips");
        return EXIT_FAILURE;
    }
    int selected = 0;
    if (chipcnt > 1)
    {
        puts("Multiple chips found!");
        for (int i = 0; i < chipcnt; i++)
        {
            uint8_t chipid = chiplist[i];
            printf("%i. %s%s\n", i+1, chipid & 0x80 ? "Second " : "", CHIPNAMES[chipid & 0x7f]);
        }
        while (1)
        {
            printf("Make your selection: ");
            int success = scanf("%i", &selected);
            if (!success)
            {
                puts("Could not parse input as integer");
                continue;
            }
            if (selected < 1 || selected > chipcnt)
            {
                puts("Input out of range");
                continue;
            }
        }
        selected--;
    }
    
    /* now read the vgm data */
    uint8_t chipid = chiplist[selected];
    printf("Converting %s%s\n", chipid & 0x80 ? "Second " : "", CHIPNAMES[chipid & 0x7f]);
    
    uint8_t chipcmd;
    if (chipid & 0x80)
        chipcmd = CHIPDUALCMDS[chipid & 0x7f];
    else
        chipcmd = CHIPCMDS[chipid];
    
    uint32_t chipclk = chipclocks[chipid & 0x7f];
    
    int loopenable = 0;
    uint32_t loopoffs = get32(vgmdata+0x1c);
    uint8_t *loopptr = NULL;
    if (loopoffs)
    {
        loopptr = vgmdata+0x1c+loopoffs;
        loopenable++;
    }
    
    uint8_t *dataptr;
    uint32_t dataoffs = get32(vgmdata+0x34);
    if (dataoffs)
        dataptr = vgmdata+0x34+dataoffs;
    else
        dataptr = vgmdata+0x40;
    uint8_t *eofptr = dataptr+vgmsize;
    
    
    uint8_t *regdata = NULL;
    uint16_t *waitdata = NULL;
    size_t regsize = 0;
    size_t waitsize = 0;
    size_t regcurpack = 0;
    
    
    uint8_t prvoldlo[9];
    uint8_t prvoldhi[9];
    uint8_t prvlo[9];
    uint8_t prvhi[9];
    memset(prvoldlo, 0xff, 9);
    memset(prvoldhi, 0xff, 9);
    memset(prvlo, 0xff, 9);
    memset(prvhi, 0xff, 9);
    
    
    
    uint32_t waitcnt = 0;
    int endflag = 0;
    while (1)
    {
        if (endflag || waitcnt >= 50)
        { /* waits less than xx samples are concatenated into the same "packet" */
            /* go back and correct pitches to c64 clock */
            for (int chn = 0; chn < 9; chn++)
            {
                size_t loset = -1;
                size_t hiset = -1;
                for (size_t i = regsize-2; i >= regcurpack && i < (-1 - 2); i -= 2)
                {
                    uint8_t c = regdata[i];
                    if (loset == -1 && c == (0xa0 | chn))
                        loset = i;
                    if (hiset == -1 && c == (0xb0 | chn))
                        hiset = i;
                }
                if (loset != -1 || hiset != -1)
                {
                    uint8_t loval = loset != -1 ? regdata[loset+1] : prvoldlo[chn];
                    uint8_t hival = hiset != -1 ? regdata[hiset+1] : prvoldhi[chn];
                    prvoldlo[chn] = loval;
                    prvoldhi[chn] = hival;
                    
                    uint16_t fnum = loval | ((hival & 0x03) << 8);
                    uint8_t block = (hival & 0b00011100) >> 2;
                    
                    double freq = ((chipclk/72.0) * fnum) / pow(2.0, 20.0-block);
                    
                    /* go through the blocks until a suitable fnum is found */
                    for (block = 0; block < 8; block++)
                    {
                        fnum = freq * pow(2.0, 20.0-block) / (CLOCK/72.0);
                        if (fnum < 0x200) break;
                    }
                    
                    if (block < 8) /* if it can't be converted, don't even bother */
                    {
                        uint8_t newloval = fnum & 0xff;
                        uint8_t newhival = (hival & 0b11100000) | (block << 2) | (fnum >> 8);
                        if (loset != -1)
                        {
                            regdata[loset+1] = newloval;
                        }
                        else if (newloval != prvlo[chn])
                        {
                            regdata = realloc(regdata, regsize+2);
                            regdata[regsize++] = 0xa0 | chn;
                            regdata[regsize++] = newloval;
                        }
                        if (hiset != -1)
                        {
                            regdata[hiset+1] = newhival;
                        }
                        else if (newhival != prvhi[chn])
                        {
                            regdata = realloc(regdata, regsize+2);
                            regdata[regsize++] = 0xb0 | chn;
                            regdata[regsize++] = newhival;
                        }
                        prvlo[chn] = newloval;
                        prvhi[chn] = newhival;
                    }
                }
            }
            
            regdata = realloc(regdata, regsize+1);
            regdata[regsize++] = 0xfe + endflag;
            regcurpack = regsize;
            waitdata = realloc(waitdata, (waitsize+1)*2);
            int effwaitcnt = (waitcnt/WAITDIV)-1;
            waitdata[waitsize++] = effwaitcnt > 0 ? effwaitcnt : 1;
            if (endflag) break;
            waitcnt = 0;
        }
        if (dataptr >= eofptr)
        {
            puts("Unexpected end of file");
            return EXIT_FAILURE;
        }
        if (loopenable == 1 && dataptr >= loopptr)
        {
            loopenable++;
            /* insert loop point markers */
            regdata = realloc(regdata, regsize+1);
            regdata[regsize++] = 0xfd;
            waitdata = realloc(waitdata, (waitsize+1)*2);
            waitdata[waitsize++] = 0;
        }
        uint8_t cmd = *(dataptr++);
        if (cmd == 0x66)
        {
            endflag++;
        }
        else if (cmd == 0x61)
        {
            waitcnt += get16(dataptr);
            dataptr += 2;
        }
        else if ((cmd & 0xf0) == 0x70)
        {
            waitcnt += (cmd & 0x0f) + 1;
        }
        else if (cmd == 0x62)
        {
            waitcnt += 735;
        }
        else if (cmd == 0x63)
        {
            waitcnt += 882;
        }
        else if (cmd == chipcmd)
        {
            regdata = realloc(regdata, regsize+2);
            regdata[regsize++] = *(dataptr++);
            regdata[regsize++] = *(dataptr++);
        }
        else if (cmd == 0x67)
        {
            dataptr += get32(dataptr+2)+6;
        }
        else if ((cmd & 0xf0) == 0x80)
        {
            waitcnt += (cmd & 0x0f);
        }
        else if (cmd >= 0x30 && cmd < 0x40)
        {
            dataptr++;
        }
        else if (cmd >= 0x40 && cmd < 0x60)
        {
            dataptr += 2;
        }
        else if (cmd >= 0xa0 && cmd < 0xc0)
        {
            dataptr += 2;
        }
        else if (cmd >= 0xc0 && cmd < 0xe0)
        {
            dataptr += 3;
        }
        else if (cmd >= 0xe0)
        {
            dataptr += 4;
        }
        else
        {
            printf("Unknown command %#02x at offset %#x\n", cmd, (uint32_t)(dataptr-vgmdata-1));
            return EXIT_FAILURE;
        }
    }
    
    
    f = fopen(argv[2], "w");
    
    fprintf(f, "HAS_LOOP = %i\n\n\n", loopenable);
    
    fputs("waitdata\n", f);
    if (!(loopenable && !waitdata[0]))
        fputs("    .word ", f);
    for (int i = 0; i < waitsize; i++)
    {
        if (!waitdata[i])
        {
            fseek(f, -1, SEEK_CUR); /* remove the last comma */
            fputs("\nwaitdata_loop  .word ", f);
        }
        else
            fprintf(f, "%i,", waitdata[i]);
    }
    fputs("0\n\n\n", f);
    
    fputs("regdata\n", f);
    if (!(loopenable && regdata[0] == 0xfd))
        fputs("    .byte ", f);
    for (int i = 0; i < regsize; i++)
    {
        if (regdata[i] == 0xfd)
        {
            fseek(f, -1, SEEK_CUR); /* remove the last comma */
            fputs("\nregdata_loop  .byte ", f);
        }
        else if (regdata[i] > 0xfd)
            fprintf(f, "%i,", regdata[i]);
        else
        {
            fprintf(f, "%i,%i,", regdata[i], regdata[i+1]);
            i++;
        }
    }
    fseek(f, -1, SEEK_CUR);
    fputc(' ', f);
    
    fclose(f);
    
    
}