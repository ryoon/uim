#! /usr/bin/env sscm -C EUC-CN
;; -*- buffer-file-coding-system: euc-jp -*-

;;  FileName : test-enc-eucgeneric.scm
;;  About    : unit test for EUC string
;;
;;  Copyright (C) 2005 Kazuki Ohta <mover AT hct.zaq.ne.jp>
;;
;;  All rights reserved.
;;
;;  Redistribution and use in source and binary forms, with or without
;;  modification, are permitted provided that the following conditions
;;  are met:
;;
;;  1. Redistributions of source code must retain the above copyright
;;     notice, this list of conditions and the following disclaimer.
;;  2. Redistributions in binary form must reproduce the above copyright
;;     notice, this list of conditions and the following disclaimer in the
;;     documentation and/or other materials provided with the distribution.
;;  3. Neither the name of authors nor the names of its contributors
;;     may be used to endorse or promote products derived from this software
;;     without specific prior written permission.
;;
;;  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
;;  IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
;;  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
;;  PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
;;  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
;;  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
;;  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
;;  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
;;  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
;;  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
;;  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

(load "./test/unittest.scm")

(define tn test-name)

;; This file provides a fallback test unit for all EUC systems.  It's
;; just a copy of test-enc-eucjp.scm with EUCJP-specific character
;; sequences removed, so some characters may be undefined in other EUC
;; systems.
(assert-equal? "string 1" "���ͤˤ�" (string #\�� #\�� #\�� #\��))
(assert-equal? "list->string 1" "3����" (list->string '(#\3 #\�� #\��)))
(assert-equal? "string->list 1" '(#\�� #\�� #\��) (string->list "������"))

;; since single shift is only supported in EUC-JP in SigScheme, the JIS X 0201
;; kana character is replaced to JIS x 0208.  -- YamaKen 2005-11-25
(assert-equal? "string-ref 1" #\��  (string-ref "��hi�����" 3))

(assert-equal? "make-string 1" "����������"   (make-string 5 #\��))
(assert-equal? "string-copy 1"     "����"   (string-copy "����"))
(assert-equal? "string-set! 1"     "��˶�"   (string-set!
                                               (string-copy "��ˤ�")
                                               2
                                               #\��))

(define str1 "����ah˽\\˽n!����!")
(define str1-list '(#\�� #\�� #\a #\h #\˽ #\\ #\˽ #\n #\! #\�� #\�� #\!))

(assert-equal? "string 2" str1 (apply string str1-list))
(assert-equal? "list->string 2" str1-list (string->list str1))

;; SRFI-75
(tn "SRFI-75")
(assert-parseable   (tn) "#\\x63")
(assert-parse-error (tn) "#\\u0063")
(assert-parse-error (tn) "#\\U00000063")

(assert-parseable   (tn) "\"\\x63\"")
(assert-parse-error (tn) "\"\\u0063\"")
(assert-parse-error (tn) "\"\\U00000063\"")

(assert-parseable   (tn) "'a")
(assert-parse-error (tn) "'��")

(total-report)
