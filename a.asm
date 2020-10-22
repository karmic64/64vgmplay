            .virtual $f0
waitcnt     .byte ?
dataptr     .word ?
waitptr     .word ?

irqa    .byte ?
irqy    .byte ?
irq1    .byte ?
            .endv
            
            .cpu "6502i"
            
CLOCK = 985248  ;PAL
;CLOCK = 1022727 ;NTSC
            
            * = $080d
            lda #$7f
            sta $dc0d
            lda #<nmi
            sta $fffa
            lda #>nmi
            sta $fffb
            lda #<irq
            sta $fffe
            lda #>irq
            sta $ffff
            lda #$35
            sta $01
            
            ldx #0
            txa
-           stx $df40
            nop
            nop
            sta $df50
            php
            plp
            php
            plp
            php
            plp
            php
            plp
            inx
            bne -
            
            ldx #0
            lda #$20
-           sta $0400,x
            sta $0500,x
            sta $0600,x
            sta $0700,x
            inx
            bne -
            
            lda #<regdata
            sta dataptr
            lda #>regdata
            sta dataptr+1
            lda #<waitdata+4
            sta waitptr
            lda #>waitdata+4
            sta waitptr+1
            lda #1
            sta waitcnt
            
            lda #<(CLOCK/44100) * 2
            sta $dc04
            lda #>(CLOCK/44100) * 2
            sta $dc05
            lda waitdata
            sta $dc06
            lda waitdata+1
            sta $dc07
            lda #$11 ;tma runs every cycle
            sta $dc0e
            lda #$51 ;tmb runs every xx samples
            sta $dc0f
            lda waitdata+2 ;set wait period for next cycle
            sta $dc06
            lda waitdata+3
            sta $dc07
            
            lda $dc0d
            lda #$82
            sta $dc0d
            cli
            
            
mainloop    lda #0
            sta $d020
            
            lax dataptr
            and #$0f
            tay
            lda conv,y
            sta $0403
            txa
            lsr
            lsr
            lsr
            lsr
            tay
            lda conv,y
            sta $0402
            lax dataptr+1
            and #$0f
            tay
            lda conv,y
            sta $0401
            txa
            lsr
            lsr
            lsr
            lsr
            tay
            lda conv,y
            sta $0400
            
            
            lax waitptr
            and #$0f
            tay
            lda conv,y
            sta $0409
            txa
            lsr
            lsr
            lsr
            lsr
            tay
            lda conv,y
            sta $0408
            lax waitptr+1
            and #$0f
            tay
            lda conv,y
            sta $0407
            txa
            lsr
            lsr
            lsr
            lsr
            tay
            lda conv,y
            sta $0406
            
            
            
-           lda $dc06
            pha
            lax $dc07
            and #$0f
            tay
            lda conv,y
            sta $0425
            txa
            lsr
            lsr
            lsr
            lsr
            tay
            lda conv,y
            sta $0424
            pla
            tax
            and #$0f
            tay
            lda conv,y
            sta $0427
            txa
            lsr
            lsr
            lsr
            lsr
            tay
            lda conv,y
            sta $0426
            
            
            lda waitcnt
            beq -
            dec waitcnt
            lda #$0f
            sta $d020
            
getloop     ldy #$00
            .if UNDER_IO
                dec $01
            .endif
            lda (dataptr),y
            .if UNDER_IO
                inc $01
            .endif
            cmp #$fe
            bcs next
            sta $df40
            iny
            .if UNDER_IO
                dec $01
            .endif
            lda (dataptr),y
            .if UNDER_IO
                inc $01
            .endif
            sta $df50
            tya
            sec
            adc dataptr
            sta dataptr
            bcc getloop
            inc dataptr+1
            bcs getloop
            
next        .if HAS_LOOP
                bne loopdata
            .else
                bne *
            .endif
            inc dataptr
            bne +
            inc dataptr+1
+           gne mainloop
loopdata    .if HAS_LOOP
                lda #<regdata_loop
                sta dataptr
                lda #>regdata_loop
                sta dataptr+1
                gne mainloop
            .endif
            
            



irq         sta irqa
            sty irqy
            .if UNDER_IO
                lda $01
                sta irq1
                lda #$35
                sta $01
            .endif
            ldy #0
            lda (waitptr),y
            iny
            ora (waitptr),y
            beq _loop
            lda (waitptr),y
            sta $dc07
            dey
            lda (waitptr),y
            sta $dc06
            lda #2
            clc
            adc waitptr
            sta waitptr
            bcc _end
            inc waitptr+1
            bcs _end
_loop       .if HAS_LOOP
                lda waitdata_loop
                sta $dc06
                lda waitdata_loop+1
                sta $dc07
                lda #<waitdata_loop+2
                sta waitptr
                lda #>waitdata_loop+2
                sta waitptr+1
                ;bne _end
            .else
                lda #$7f
                sta $dc0d
                ;bpl _end
            .endif
_end        inc waitcnt
            lda $dc0d
            .if UNDER_IO
                lda irq1
                sta $01
            .endif
            lda irqa
            ldy irqy
nmi         rti
            
            
            .enc "screen"
conv    .text "0123456789ABCDEF"
            
            ;insert your music data here
            .include "tune.asm"
            
                UNDER_IO = (* > $d000)
            
            
            