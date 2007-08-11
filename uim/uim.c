/*

  Copyright (c) 2003-2007 uim Project http://code.google.com/p/uim/

  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. Neither the name of authors nor the names of its contributors
     may be used to endorse or promote products derived from this software
     without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
  OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
  SUCH DAMAGE.

*/

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "uim.h"
#include "uim-internal.h"
#include "uim-util.h"
#include "uim-im-switcher.h"
#include "uim-scm.h"
#include "uim-scm-abbrev.h"


enum uim_result {
  FAILED = -1,
  OK = 0
};

static void fatal_error_hook(void);

static void *uim_init_internal(void *dummy);
struct uim_get_candidate_args {
  uim_context uc;
  int index;
  int enum_hint;
};
static void *uim_get_candidate_internal(struct uim_get_candidate_args *args);
static uim_lisp get_nth_im(uim_context uc, int nth);
#ifdef ENABLE_ANTHY_STATIC
void uim_anthy_plugin_instance_init(void);
void uim_anthy_plugin_instance_quit(void);
#endif
#ifdef ENABLE_ANTHY_UTF8_STATIC
void uim_anthy_utf8_plugin_instance_init(void);
void uim_anthy_utf8_plugin_instance_quit(void);
#endif

static uim_bool uim_initialized;
static uim_lisp protected0, protected1;


/****************************************************************
 * Core APIs                                                    *
 ****************************************************************/
static void
fatal_error_hook(void)
{
  /* error message is already printed by the Scheme interpreter */
  uim_fatal_error(NULL);
}

int
uim_init(void)
{
  int ret;
  char *sys_load_path;

  if (uim_initialized)
    return OK;

  uim_init_error();

  if (uim_catch_error_begin())
    return FAILED;

  sys_load_path = (uim_issetugid()) ? NULL : getenv("LIBUIM_SYSTEM_SCM_FILES");
  uim_scm_init(sys_load_path);
  uim_scm_set_fatal_error_hook(fatal_error_hook);

  ret = (int)uim_scm_call_with_gc_ready_stack((uim_gc_gate_func_ptr)uim_init_internal, NULL);

  uim_catch_error_end();

  return ret;
}

static void *
uim_init_internal(void *dummy)
{
  char *scm_files;

  protected0 = uim_scm_f();
  protected1 = uim_scm_f();
  uim_scm_gc_protect(&protected0);
  uim_scm_gc_protect(&protected1);

  uim_init_im_subrs();
  uim_init_intl_subrs();
  uim_init_util_subrs();
  uim_init_key_subrs();
  uim_init_rk_subrs();
  uim_init_plugin();
#ifdef ENABLE_ANTHY_STATIC
  uim_anthy_plugin_instance_init();
#endif
#ifdef ENABLE_ANTHY_UTF8_STATIC
  uim_anthy_utf8_plugin_instance_init();
#endif

  if (uim_issetugid()) {
    scm_files = SCM_FILES;
  } else {
    scm_files = getenv("LIBUIM_SCM_FILES");
    scm_files = (scm_files) ? scm_files : SCM_FILES;
  }
  uim_scm_set_lib_path(scm_files);

  uim_scm_require_file("init.scm");

  uim_initialized = UIM_TRUE;

  return (void *)OK;
}

void
uim_quit(void)
{
  if (!uim_initialized)
    return;

  if (uim_catch_error_begin()) {
    /* Leave uim_initialized uncleared to keep libuim disabled. */
    return;
  }

  uim_quit_plugin();
#ifdef ENABLE_ANTHY_STATIC
  uim_anthy_plugin_instance_quit();
#endif
#ifdef ENABLE_ANTHY_UTF8_STATIC
  uim_anthy_utf8_plugin_instance_quit();
#endif
  uim_scm_quit();
  uim_initialized = UIM_FALSE;
}

uim_context
uim_create_context(void *ptr,
		   const char *enc,
		   const char *lang,
		   const char *engine,
		   struct uim_code_converter *conv,
		   void (*commit_cb)(void *ptr, const char *str))
{
  uim_context uc;
  uim_lisp lang_, engine_;

  if (uim_catch_error_begin())
    return NULL;

  assert(uim_scm_gc_any_contextp());

  uc = uim_malloc(sizeof(*uc));
  memset(uc, 0, sizeof(*uc));

  /* encoding handlings */
  if (!enc)
    enc = "UTF-8";
  uc->client_encoding = uim_strdup(enc);
  uc->conv_if = (conv) ? conv : uim_iconv;

  /* variables */
  uc->is_enabled = UIM_TRUE;

  /* core callbacks */
  uc->commit_cb = commit_cb;

  /* foreign context objects */
  uc->ptr = ptr;

  protected0 = lang_ = (lang) ? MAKE_SYM(lang) : uim_scm_f();
  protected1 = engine_ = (engine) ? MAKE_SYM(engine) : uim_scm_f();
  uc->sc = uim_scm_f(); /* failsafe */
  uc->sc = uim_scm_callf("create-context", "poo", uc, lang_, engine_);
  uim_scm_gc_protect(&uc->sc);
  uim_scm_callf("setup-context", "o", uc->sc);

  uim_catch_error_end();

  return uc;
}

void
uim_release_context(uim_context uc)
{
  int i;

  if (uim_catch_error_begin())
    return;

  assert(uim_scm_gc_any_contextp());
  assert(uc);

  uim_scm_callf("release-context", "p", uc);
  uim_scm_gc_unprotect(&uc->sc);
  if (uc->outbound_conv)
    uc->conv_if->release(uc->outbound_conv);
  if (uc->inbound_conv)
    uc->conv_if->release(uc->inbound_conv);
  for (i = 0; i < uc->nr_modes; i++) {
    free(uc->modes[i]);
    uc->modes[i] = NULL;
  }
  free(uc->propstr);
  free(uc->modes);
  free(uc->client_encoding);
  free(uc);

  uim_catch_error_end();
}

void
uim_reset_context(uim_context uc)
{
  if (uim_catch_error_begin())
    return;

  assert(uim_scm_gc_any_contextp());
  assert(uc);

  uim_scm_callf("reset-handler", "p", uc);

  uim_catch_error_end();
}

void
uim_focus_in_context(uim_context uc)
{
  if (uim_catch_error_begin())
    return;

  assert(uim_scm_gc_any_contextp());
  assert(uc);

  uim_scm_callf("focus-in-handler", "p", uc);

  uim_catch_error_end();
}

void
uim_focus_out_context(uim_context uc)
{
  if (uim_catch_error_begin())
    return;

  assert(uim_scm_gc_any_contextp());
  assert(uc);

  uim_scm_callf("focus-out-handler", "p", uc);

  uim_catch_error_end();
}

void
uim_place_context(uim_context uc)
{
  if (uim_catch_error_begin())
    return;

  assert(uim_scm_gc_any_contextp());
  assert(uc);

  uim_scm_callf("place-handler", "p", uc);

  uim_catch_error_end();
}

void
uim_displace_context(uim_context uc)
{
  if (uim_catch_error_begin())
    return;

  assert(uim_scm_gc_any_contextp());
  assert(uc);

  uim_scm_callf("displace-handler", "p", uc);

  uim_catch_error_end();
}

void
uim_set_preedit_cb(uim_context uc,
		   void (*clear_cb)(void *ptr),
		   void (*pushback_cb)(void *ptr, int attr, const char *str),
		   void (*update_cb)(void *ptr))
{
  if (uim_catch_error_begin())
    return;

  assert(uim_scm_gc_any_contextp());
  assert(uc);

  uc->preedit_clear_cb = clear_cb;
  uc->preedit_pushback_cb = pushback_cb;
  uc->preedit_update_cb = update_cb;

  uim_catch_error_end();
}

void
uim_set_candidate_selector_cb(uim_context uc,
                              void (*activate_cb)(void *ptr,
                                                  int nr, int display_limit),
                              void (*select_cb)(void *ptr, int index),
                              void (*shift_page_cb)(void *ptr, int direction),
                              void (*deactivate_cb)(void *ptr))
{
  if (uim_catch_error_begin())
    return;

  assert(uim_scm_gc_any_contextp());
  assert(uc);

  uc->candidate_selector_activate_cb   = activate_cb;
  uc->candidate_selector_select_cb     = select_cb;
  uc->candidate_selector_deactivate_cb = deactivate_cb;
  uc->candidate_selector_shift_page_cb = shift_page_cb;

  uim_catch_error_end();
}

uim_candidate
uim_get_candidate(uim_context uc, int index, int accel_enumeration_hint)
{
  struct uim_get_candidate_args args;
  uim_candidate cand;

  if (uim_catch_error_begin())
    return NULL;

  assert(uim_scm_gc_any_contextp());
  assert(uc);
  assert(index >= 0);
  assert(accel_enumeration_hint >= 0);

  args.uc = uc;
  args.index = index;
  args.enum_hint = accel_enumeration_hint;

  cand = (uim_candidate)uim_scm_call_with_gc_ready_stack((uim_gc_gate_func_ptr)uim_get_candidate_internal, &args);

  uim_catch_error_end();

  return cand;
}

static void *
uim_get_candidate_internal(struct uim_get_candidate_args *args)
{
  uim_context uc;
  uim_candidate cand;
  uim_lisp triple;
  const char *str, *head, *ann;

  uc = args->uc;
  triple = uim_scm_callf("get-candidate", "pii",
			 uc, args->index, args->enum_hint);
  uim_scm_ensure(uim_scm_length(triple) == 3);

  cand = uim_malloc(sizeof(*cand));
  memset(cand, 0, sizeof(*cand));

  str  = uim_scm_refer_c_str(CAR(triple));
  head = uim_scm_refer_c_str(CAR(CDR(triple)));
  ann  = uim_scm_refer_c_str(CAR(CDR(CDR((triple)))));
  cand->str           = uc->conv_if->convert(uc->outbound_conv, str);
  cand->heading_label = uc->conv_if->convert(uc->outbound_conv, head);
  cand->annotation    = uc->conv_if->convert(uc->outbound_conv, ann);

  return (void *)cand;
}

/* Accepts NULL candidates that produced by an error on uim_get_candidate(). */
const char *
uim_candidate_get_cand_str(uim_candidate cand)
{
  if (uim_catch_error_begin())
    return "";

  assert(uim_scm_gc_any_contextp());
  if (!cand)
    uim_fatal_error("null candidate");

  uim_catch_error_end();

  return cand->str;
}

const char *
uim_candidate_get_heading_label(uim_candidate cand)
{
  if (uim_catch_error_begin())
    return "";

  assert(uim_scm_gc_any_contextp());
  if (!cand)
    uim_fatal_error("null candidate");

  uim_catch_error_end();

  return cand->heading_label;
}

const char *
uim_candidate_get_annotation_str(uim_candidate cand)
{
  if (uim_catch_error_begin())
    return "";

  assert(uim_scm_gc_any_contextp());
  if (!cand)
    uim_fatal_error("null candidate");

  uim_catch_error_end();

  return cand->annotation;
}

void
uim_candidate_free(uim_candidate cand)
{
  if (uim_catch_error_begin())
    return;

  assert(uim_scm_gc_any_contextp());
  if (!cand)
    uim_fatal_error("null candidate");

  free(cand->str);
  free(cand->heading_label);
  free(cand->annotation);
  free(cand);

  uim_catch_error_end();
}

int
uim_get_candidate_index(uim_context uc)
{
  if (uim_catch_error_begin())
    return 0;

  assert(uim_scm_gc_any_contextp());
  assert(uc);

  uim_catch_error_end();

  return 0;
}

void
uim_set_candidate_index(uim_context uc, int nth)
{
  if (uim_catch_error_begin())
    return;

  assert(uim_scm_gc_any_contextp());
  assert(uc);
  assert(nth >= 0);

  uim_scm_callf("set-candidate-index", "pi", uc, nth);

  uim_catch_error_end();
}

void
uim_set_text_acquisition_cb(uim_context uc,
			    int (*acquire_cb)(void *ptr,
					      enum UTextArea text_id,
					      enum UTextOrigin origin,
					      int former_len, int latter_len,
					      char **former, char **latter),
			    int (*delete_cb)(void *ptr, enum UTextArea text_id,
				    	     enum UTextOrigin origin,
					     int former_len, int latter_len))
{
  if (uim_catch_error_begin())
    return;

  assert(uim_scm_gc_any_contextp());
  assert(uc);

  uc->acquire_text_cb = acquire_cb;
  uc->delete_text_cb = delete_cb;

  uim_catch_error_end();
}

uim_bool
uim_input_string(uim_context uc, const char *str)
{
  uim_bool ret;
  uim_lisp consumed;
  char *conv;

  if (uim_catch_error_begin())
    return UIM_FALSE;

  assert(uim_scm_gc_any_contextp());
  assert(uc);
  assert(str);

  conv = uc->conv_if->convert(uc->inbound_conv, str);
  if (conv) {
    protected0 =
      consumed = uim_scm_callf("input-string-handler", "ps", uc, conv);
    free(conv);

    ret = uim_scm_c_bool(consumed);
  } else {
    ret = UIM_FALSE;
  }

  uim_catch_error_end();

  return ret;
}

/****************************************************************
 * Optional APIs                                                *
 ****************************************************************/
void
uim_set_configuration_changed_cb(uim_context uc,
				 void (*changed_cb)(void *ptr))
{
  if (uim_catch_error_begin())
    return;

  assert(uim_scm_gc_any_contextp());
  assert(uc);

  uc->configuration_changed_cb = changed_cb;

  uim_catch_error_end();
}

void
uim_set_im_switch_request_cb(uim_context uc,
			     void (*sw_app_im_cb)(void *ptr, const char *name),
			     void (*sw_system_im_cb)(void *ptr,
                                                     const char *name))
{
  if (uim_catch_error_begin())
    return;

  assert(uim_scm_gc_any_contextp());
  assert(uc);

  uc->switch_app_global_im_cb = sw_app_im_cb;
  uc->switch_system_global_im_cb = sw_system_im_cb;

  uim_catch_error_end();
}

void
uim_switch_im(uim_context uc, const char *engine)
{
  if (uim_catch_error_begin())
    return;

  /* related to the commit log of r1400:

     We should not add the API uim_destroy_context(). We should move
     IM-switching feature into Scheme instead. It removes the context
     management code related to IM-switching duplicated in several IM
     bridges. Refer the implementation of GtkIMMulticontext as example
     of proxy-like IM-switcher. It does IM-switching without extending
     immodule API. We should follow its design to make our API simple.
     -- 2004-10-05 YamaKen
  */
  assert(uim_scm_gc_any_contextp());
  assert(uc);
  assert(engine);

  uim_scm_callf("uim-switch-im", "py", uc, engine);

  uim_catch_error_end();
}

const char *
uim_get_current_im_name(uim_context uc)
{
  uim_lisp im, ret;
  const char *name;

  if (uim_catch_error_begin())
    return "direct";

  assert(uim_scm_gc_any_contextp());
  assert(uc);

  protected0 = im = uim_scm_callf("uim-context-im", "p", uc);
  protected1 = ret = uim_scm_callf("im-name", "o", im);
  name = uim_scm_refer_c_str(ret);

  uim_catch_error_end();

  return name;
}

const char *
uim_get_default_im_name(const char *localename)
{
  uim_lisp ret;
  const char *name;

  if (uim_catch_error_begin())
    return "direct";

  assert(uim_scm_gc_any_contextp());
  assert(localename);

  protected0 = ret = uim_scm_callf("uim-get-default-im-name", "s", localename);
  name = uim_scm_refer_c_str(ret);

  uim_catch_error_end();

  return name;
}

const char *
uim_get_im_name_for_locale(const char *localename)
{
  uim_lisp ret;
  const char *name;

  if (uim_catch_error_begin())
    return "direct";

  assert(uim_scm_gc_any_contextp());
  assert(localename);

  protected0 =
    ret = uim_scm_callf("uim-get-im-name-for-locale", "s", localename);
  name = uim_scm_refer_c_str(ret);

  uim_catch_error_end();

  return name;
}

/****************************************************************
 * Legacy 'mode' API                                            *
 ****************************************************************/
int
uim_get_nr_modes(uim_context uc)
{
  if (uim_catch_error_begin())
    return 0;

  assert(uim_scm_gc_any_contextp());
  assert(uc);

  uim_catch_error_end();

  return uc->nr_modes;
}

const char *
uim_get_mode_name(uim_context uc, int nth)
{
  if (uim_catch_error_begin())
    return NULL;

  assert(uim_scm_gc_any_contextp());
  assert(uc);
  assert(nth >= 0);
  assert(nth < uc->nr_modes);

  uim_catch_error_end();

  return uc->modes[nth];
}

int
uim_get_current_mode(uim_context uc)
{
  if (uim_catch_error_begin())
    return 0;

  assert(uim_scm_gc_any_contextp());
  assert(uc);

  uim_catch_error_end();

  return uc->mode;
}

void
uim_set_mode(uim_context uc, int mode)
{
  if (uim_catch_error_begin())
    return;

  assert(uim_scm_gc_any_contextp());
  assert(uc);
  assert(mode >= 0);

  uc->mode = mode;
  uim_scm_callf("mode-handler", "pi", uc, mode);

  uim_catch_error_end();
}

void
uim_set_mode_cb(uim_context uc, void (*update_cb)(void *ptr, int mode))
{
  if (uim_catch_error_begin())
    return;

  assert(uim_scm_gc_any_contextp());
  assert(uc);

  uim_catch_error_end();

  uc->mode_update_cb = update_cb;
}

void
uim_set_mode_list_update_cb(uim_context uc, void (*update_cb)(void *ptr))
{
  if (uim_catch_error_begin())
    return;

  assert(uim_scm_gc_any_contextp());
  assert(uc);

  uim_catch_error_end();

  uc->mode_list_update_cb = update_cb;
}

/****************************************************************
 * Legacy 'property list' API                                   *
 ****************************************************************/
void
uim_set_prop_list_update_cb(uim_context uc,
			    void (*update_cb)(void *ptr, const char *str))
{
  if (uim_catch_error_begin())
    return;

  assert(uim_scm_gc_any_contextp());
  assert(uc);

  uc->prop_list_update_cb = update_cb;

  uim_catch_error_end();
}

/* Obsolete */
void
uim_set_prop_label_update_cb(uim_context uc,
			     void (*update_cb)(void *ptr, const char *str))
{
  if (uim_catch_error_begin())
    return;

  assert(uim_scm_gc_any_contextp());
  assert(uc);

  uim_catch_error_end();
}

void
uim_prop_list_update(uim_context uc)
{
  if (uim_catch_error_begin())
    return;

  assert(uim_scm_gc_any_contextp());
  assert(uc);

  if (uc->propstr && uc->prop_list_update_cb)
    uc->prop_list_update_cb(uc->ptr, uc->propstr);

  uim_catch_error_end();
}

/* Obsolete */
void
uim_prop_label_update(uim_context uc)
{
  if (uim_catch_error_begin())
    return;

  assert(uim_scm_gc_any_contextp());
  assert(uc);

  uim_catch_error_end();
}

void
uim_prop_activate(uim_context uc, const char *str)
{
  if (uim_catch_error_begin())
    return;

  assert(uim_scm_gc_any_contextp());
  assert(uc);
  assert(str);
      
  uim_scm_callf("prop-activate-handler", "ps", uc, str);

  uim_catch_error_end();
}

/****************************************************************
 * Legacy 'custom' API                                          *
 ****************************************************************/
/* Tentative name. I followed above uim_prop_update_custom, but prop 
would not be proper to this function. */
/*
 * As I described in doc/HELPER-PROTOCOL, it had wrongly named by my
 * misunderstanding about what is the 'property' of uim. It should be
 * renamed along with corresponding procol names when an appropriate
 * time has come.  -- YamaKen 2005-09-12
 */
/** Update custom value from property message.
 * Update custom value from property message. All variable update is
 * validated by custom APIs rather than arbitrary sexp
 * evaluation. Custom symbol \a custom is quoted in sexp string to be
 * restricted to accept symbol literal only. This prevents arbitrary
 * sexp evaluation.
 */
void
uim_prop_update_custom(uim_context uc, const char *custom, const char *val)
{
  if (uim_catch_error_begin())
    return;

  assert(uim_scm_gc_any_contextp());
  assert(uc);
  assert(custom);
  assert(val);

  uim_scm_callf("custom-set-handler", "pyo",
                uc, custom, uim_scm_eval_c_string(val));

  uim_catch_error_end();
}

uim_bool
uim_prop_reload_configs(void)
{
  if (uim_catch_error_begin())
    return UIM_FALSE;

  assert(uim_scm_gc_any_contextp());

  /* FIXME: handle return value properly. */
  uim_scm_callf("custom-reload-user-configs", "");

  uim_catch_error_end();

  return UIM_TRUE;
}

/****************************************************************
 * Legacy nth-index based IM management APIs                    *
 ****************************************************************/
int
uim_get_nr_im(uim_context uc)
{
  uim_lisp n_;
  int n;

  if (uim_catch_error_begin())
    return 0;

  assert(uim_scm_gc_any_contextp());
  assert(uc);

  protected0 = n_ = uim_scm_callf("uim-n-convertible-ims", "p", uc);
  n = uim_scm_c_int(n_);

  uim_catch_error_end();

  return n;
}

static uim_lisp
get_nth_im(uim_context uc, int nth)
{
  assert(uim_scm_gc_any_contextp());
  assert(uc);
  assert(nth >= 0);

  return uim_scm_callf("uim-nth-convertible-im", "pi", uc, nth);
}

const char *
uim_get_im_name(uim_context uc, int nth)
{
  uim_lisp im, str_;
  const char *str;

  if (uim_catch_error_begin())
    return NULL;

  protected0 = im = get_nth_im(uc, nth);
  protected1 = str_ = uim_scm_callf("im-name", "o", im);
  str = uim_scm_refer_c_str(str_);

  uim_catch_error_end();

  return str;
}

const char *
uim_get_im_language(uim_context uc, int nth)
{
  uim_lisp im, str_;
  const char *str;

  if (uim_catch_error_begin())
    return NULL;

  protected0 = im = get_nth_im(uc, nth);
  protected1 = str_ = uim_scm_callf("im-lang", "o", im);
  str = uim_scm_refer_c_str(str_);

  uim_catch_error_end();

  return str;
}

const char *
uim_get_im_encoding(uim_context uc, int nth)
{
  uim_lisp im, str_;
  const char *str;

  if (uim_catch_error_begin())
    return NULL;

  protected0 = im = get_nth_im(uc, nth);
  protected1 = str_ = uim_scm_callf("im-encoding", "o", im);
  str = uim_scm_refer_c_str(str_);

  uim_catch_error_end();

  return str;
}

const char *
uim_get_im_short_desc(uim_context uc, int nth)
{
  uim_lisp im, short_desc;
  const char *str;

  if (uim_catch_error_begin())
    return NULL;

  protected0 = im = get_nth_im(uc, nth);
  protected1 = short_desc = uim_scm_callf("im-short-desc", "o", im);
  str = UIM_SCM_FALSEP(short_desc) ? "-" : uim_scm_refer_c_str(short_desc);

  uim_catch_error_end();

  return str;
}
