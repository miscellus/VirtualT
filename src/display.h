/* display.h */

/* $Id: display.h,v 1.12 2015/02/24 20:19:17 kpettit1 Exp $ */

/*
 * Copyright 2004 Stephen Hurd and Ken Pettit
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#define MENU_HEIGHT	32
#define	TAB_HEIGHT	24

#ifdef __cplusplus
extern "C" {
#endif
extern int		gDelayUpdateKeys;
void			init_pref(void);
void			init_display(void);
void			deinit_display(void);
void			drawbyte(int driver, int column, int value);
void			lcdcommand(int driver, int value);
void			power_down();
void			process_windows_event();
void			display_cpu_speed(void);
void			display_map_mode(char *str);
void			show_error(const char*);
void			t200_command(unsigned char ir, unsigned char data);
unsigned char	t200_readport(unsigned char port);
void			handle_simkey(void);
void			switch_model(int);
void			init_other_windows(void);
void			enable_tpdd_log_menu(int bEnabled);

typedef int 	(*get_key_t)(int);
typedef int 	(*event_key_t)(void);

#endif
