## ***** BEGIN LICENSE BLOCK *****
 # Version: MPL 1.1/GPL 2.0/LGPL 2.1
 #
 # The contents of this file are subject to the Mozilla Public License Version
 # 1.1 (the "License"); you may not use this file except in compliance with
 # the License. You may obtain a copy of the License at
 # http://www.mozilla.org/MPL/
 #
 # Software distributed under the License is distributed on an "AS IS" basis,
 # WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 # for the specific language governing rights and limitations under the
 # License.
 #
 # The Original Code is mozilla.org code.
 #
 # The Initial Developer of the Original Code is
 # dwmw2@infradead.org (David Woodhouse).
 # Portions created by the Initial Developer are Copyright (C) 2006
 # the Initial Developer. All Rights Reserved.
 #
 # Contributor(s):
 #
 # Alternatively, the contents of this file may be used under the terms of
 # either the GNU General Public License Version 2 or later (the "GPL"), or
 # the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 # in which case the provisions of the GPL or the LGPL are applicable instead
 # of those above. If you wish to allow use of your version of this file only
 # under the terms of either the GPL or the LGPL, and not to allow others to
 # use your version of this file under the terms of the MPL, indicate your
 # decision by deleting the provisions above and replace them with the notice
 # and other provisions required by the GPL or the LGPL. If you do not delete
 # the provisions above, a recipient may use your version of this file under
 # the terms of any one of the MPL, the GPL or the LGPL.
 #
 # ***** END LICENSE BLOCK *****

.set r0,0; .set r1,1; .set RTOC,2; .set r3,3; .set r4,4
.set r5,5; .set r6,6; .set r7,7; .set r8,8; .set r9,9
.set r10,10; .set r11,11; .set r12,12; .set r13,13; .set r14,14
.set r15,15; .set r16,16; .set r17,17; .set r18,18; .set r19,19
.set r20,20; .set r21,21; .set r22,22; .set r23,23; .set r24,24
.set r25,25; .set r26,26; .set r27,27; .set r28,28; .set r29,29
.set r30,30; .set r31,31
.set f0,0; .set f1,1; .set f2,2; .set f3,3; .set f4,4
.set f5,5; .set f6,6; .set f7,7; .set f8,8; .set f9,9
.set f10,10; .set f11,11; .set f12,12; .set f13,13; .set f14,14
.set f15,15; .set f16,16; .set f17,17; .set f18,18; .set f19,19
.set f20,20; .set f21,21; .set f22,22; .set f23,23; .set f24,24
.set f25,25; .set f26,26; .set f27,27; .set f28,28; .set f29,29
.set f30,30; .set f31,31

        .section ".text"
        .align 2
        .globl SharedStub
        .section ".opd","aw"
        .align 3

SharedStub:
        .quad   .SharedStub,.TOC.@tocbase
        .previous
        .type   SharedStub,@function

.SharedStub:
        mflr    r0

        std     r4, -56(r1)                     # Save all GPRS
        std     r5, -48(r1)
        std     r6, -40(r1)
        std     r7, -32(r1)
        std     r8, -24(r1)
        std     r9, -16(r1)
        std     r10, -8(r1)

        stfd    f13, -64(r1)                    # ... and FPRS
        stfd    f12, -72(r1)
        stfd    f11, -80(r1)
        stfd    f10, -88(r1)
        stfd    f9, -96(r1)
        stfd    f8, -104(r1)
        stfd    f7, -112(r1)
        stfd    f6, -120(r1)
        stfd    f5, -128(r1)
        stfd    f4, -136(r1)
        stfd    f3, -144(r1)
        stfd    f2, -152(r1)
        stfd    f1, -160(r1)

        subi    r6,r1,56                        # r6 --> gprData
        subi    r7,r1,160                       # r7 --> fprData
        addi    r5,r1,112                       # r5 --> extra stack args

        std     r0, 16(r1)
	
        stdu    r1,-288(r1)
                                                # r3 has the 'self' pointer
                                                # already

        mr      r4,r11                          # r4 is methodIndex selector,
                                                # passed via r11 in the
                                                # nsNSStubBase::StubXX() call

        bl      PrepareAndDispatch
        nop

        ld      1,0(r1)                         # restore stack
        ld      r0,16(r1)                       # restore LR
        mtlr    r0
        blr

        .size   SharedStub,.-.SharedStub

        # Magic indicating no need for an executable stack
        .section .note.GNU-stack, "", @progbits ; .previous
