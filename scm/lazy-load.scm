;;; lazy-load.scm: Lazy IM loading support
;;;
;;; Copyright (c) 2005 uim Project http://uim.freedesktop.org/
;;;
;;; All rights reserved.
;;;
;;; Redistribution and use in source and binary forms, with or without
;;; modification, are permitted provided that the following conditions
;;; are met:
;;; 1. Redistributions of source code must retain the above copyright
;;;    notice, this list of conditions and the following disclaimer.
;;; 2. Redistributions in binary form must reproduce the above copyright
;;;    notice, this list of conditions and the following disclaimer in the
;;;    documentation and/or other materials provided with the distribution.
;;; 3. Neither the name of authors nor the names of its contributors
;;;    may be used to endorse or promote products derived from this software
;;;    without specific prior written permission.
;;;
;;; THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
;;; ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
;;; IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
;;; ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
;;; FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
;;; DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
;;; OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
;;; HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
;;; LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
;;; OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
;;; SUCH DAMAGE.
;;;;

(require "util.scm")

(define stub-im-generate-init-handler
  (lambda (name module-name)
    (lambda (id fake-im fake-arg)
      (let ((stub-im (retrieve-im name)))
	(and (require-module module-name)
	     (let* ((im (retrieve-im name))
		    (init-handler (im-init-handler im))
		    (arg (im-init-arg im))
		    (context (if (not (eq? init-handler
					   (im-init-handler stub-im)))
				 (init-handler id im arg)
				 (begin
				   (error "stub IM actualization failed")
				   #f))))
	       context))))))

(define register-stub-im
  (lambda (name lang encoding name-label short-desc module-name)
    (if (or (not (retrieve-im name))
	    (not (im-key-press-handler (retrieve-im name))))
	(let ((init-handler (stub-im-generate-init-handler name module-name)))
	  (register-im
	   name
	   lang
	   encoding
	   name-label
	   short-desc
	   #f ;; arg
	   init-handler
	   #f ;; release-handler
	   #f ;; mode-handler
	   #f ;; press-key-handler
	   #f ;; release-key-handler
	   #f ;; reset-handler
	   #f ;; get-candidate-handler
	   #f ;; set-candidate-index-handler
	   #f ;; prop-activate-handler
	   )
	  (im-set-module-name! (retrieve-im name) module-name)))))

;; TODO: rewrite test
;; side effect: invoke require-module for all installed IM modules
(define stub-im-generate-stub-im-list
  (lambda (im-names)
    (let ((orig-enabled-im-list enabled-im-list))
      (set! enabled-im-list ())  ;; enable all IMs
      (for-each require-module installed-im-module-list)
      (set! enabled-im-list orig-enabled-im-list))
    (map (lambda (name)
	   (let* ((im (retrieve-im name))
		  (name-str (symbol->string name)))
	     (string-append
	      "(if (memq '" name-str " enabled-im-list)\n"
	      "    (if enable-lazy-loading?\n"
	      "        (register-stub-im\n"
	      "         '" name-str "\n"
	      "         \"" (im-lang im) "\"\n"
	      "         \"" (im-encoding im) "\"\n"
	      "         \"" (im-name-label im) "\"\n"
	      "         \"" (im-short-desc im) "\"\n"
	      "         \"" (im-module-name im) "\")\n"
	      "        (require-module \"" (im-module-name im) "\")))\n")))
	 im-names)))

;; side effect: invoke require-module for all IM listed in
;; installed-im-module-list
(define stub-im-generate-all-stub-im-list
  (lambda ()
    (for-each require-module installed-im-module-list)
    (stub-im-generate-stub-im-list (map im-name
					(reverse im-list)))))
