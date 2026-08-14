.section .rodata
.balign 4

.global soundtrack_bcwav
.global soundtrack_bcwav_end
soundtrack_bcwav:
    .incbin "../data/soundtrack.bcwav"
soundtrack_bcwav_end:

.balign 4
.global Explosion_bcwav
.global Explosion_bcwav_end
Explosion_bcwav:
    .incbin "../data/Explosion.bcwav"
Explosion_bcwav_end:

.balign 4
.global PlayerDied_bcwav
.global PlayerDied_bcwav_end
PlayerDied_bcwav:
    .incbin "../data/PlayerDied.bcwav"
PlayerDied_bcwav_end:

.balign 4
.global PlayerHit_bcwav
.global PlayerHit_bcwav_end
PlayerHit_bcwav:
    .incbin "../data/PlayerHit.bcwav"
PlayerHit_bcwav_end:

.balign 4
.global Coin_bcwav
.global Coin_bcwav_end
Coin_bcwav:
    .incbin "../data/Coin.bcwav"
Coin_bcwav_end:

.balign 4
.global Coin1_bcwav
.global Coin1_bcwav_end
Coin1_bcwav:
    .incbin "../data/Coin1.bcwav"
Coin1_bcwav_end:

.balign 4
.global grapple_bcwav
.global grapple_bcwav_end
grapple_bcwav:
    .incbin "../data/grapple.bcwav"
grapple_bcwav_end:

.section .note.GNU-stack,"",%progbits
