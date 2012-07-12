/* setBfree - DSP tonewheel organ
 *
 * Copyright (C) 2003-2004 Fredrik Kilander <fk@dsv.su.se>
 * Copyright (C) 2008-2012 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2012 Will Panther <pantherb@setbfree.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  
 */

#ifndef MIDI_H
#define MIDI_H

#include <jack/midiport.h>
#include "cfgParser.h"

#define MIDI_UTIL_SWELL 0
#define MIDI_UTIL_GREAT 1
#define MIDI_UTIL_PEDAL 2

#define MIDI_NOTEOFF		0x80
#define MIDI_NOTEON		0x90
#define MIDI_KEY_PRESSURE	0xA0
#define MIDI_CTL_CHANGE		0xB0
#define MIDI_PGM_CHANGE		0xC0
#define MIDI_CHN_PRESSURE	0xD0
#define MIDI_PITCH_BEND		0xE0
#define MIDI_SYSTEM_PREFIX	0xF0

extern void useMIDIControlFunction (char * cfname, void (* f) (unsigned char));

extern void setKeyboardSplitMulti (int flags,
				   int p_splitA_PL,
				   int p_splitA_UL,
				   int p_nshA_PL,
				   int p_nshA_UL,
				   int p_nshA_U);

extern void setKeyboardTransposeA (int t);
extern void setKeyboardTransposeB (int t);
extern void setKeyboardTransposeC (int t);
extern void setKeyboardTranspose (int t);

extern void midiPrimeControllerMapping ();

extern int midiConfig (ConfigContext * cfg);
extern const ConfigDoc *midiDoc ();

extern void setMIDINoteShift (char offset);

extern void initMidiTables();
extern int getCCFunctionId (const char * name);

#ifdef HAVE_ASEQ
extern void *aseq_run(void *arg);
extern int aseq_open(char *port_name);
extern void aseq_close(void);
#endif

extern void parse_jack_midi_event(jack_midi_event_t *ev);
extern void parse_lv2_midi_event(uint8_t *d, size_t l);

#endif /* MIDI_H */