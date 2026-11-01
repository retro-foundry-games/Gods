; Gods: the original menu program with the keyboard in its input (a patch of the original, in memory).
;
; The original's input routine ($01E39C) puts the joystick byte at $1E5AC with `move.b d1,($1e5ac).l` at $01E420.  That
; instruction is replaced by a call of `input` below, which sets the bits of the four cursor keys and of the space bar
; (the first fire button) from the table of the keys that are down, which the original's keyboard interrupt keeps at $1E63A by
; raw key code; then it does what the replaced instruction did.  `input` is put in the 178 unused bytes at $1E608.
; The patch is built by scripts/build_keyboard_patch.py and applied by host/gods.lua (flag `keyboard`).
; It is the same change as the one in input_poll of the C menu (input.c).
; (the call is `jsr $1e608.l`: 4EB9 0001E608, written over the 6 bytes of the move at $1E420)
        org $1e608
input:  tst.b ($1e686).l          ; cursor up: key $4C
        beq.b .down
        bset #0,d1
.down:  tst.b ($1e687).l          ; cursor down: key $4D
        beq.b .left
        bset #1,d1
.left:  tst.b ($1e689).l          ; cursor left: key $4F
        beq.b .right
        bset #2,d1
.right: tst.b ($1e688).l          ; cursor right: key $4E
        beq.b .fire
        bset #3,d1
.fire:  tst.b ($1e67a).l          ; space bar: key $40, the first fire button
        beq.b .store
        bset #7,d1
        bset #0,($1e504).l
        bset #0,($1e50a).l
.store: move.b d1,($1e5ac).l
        rts
