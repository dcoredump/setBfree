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

#ifndef REVERB_H
#define REVERB_H

#include "../src/cfgParser.h"
extern int reverbConfig (ConfigContext * cfg);
extern const ConfigDoc *reverbDoc ();

extern void setReverbInputGain (float g);

extern void setReverbOutputGain (float g);

extern void setReverbMix (float g);

extern void setReverbDry (float g);

extern void setReverbWet (float g);

extern void initReverb ();

extern float * reverb (const float * inbuf, float * outbuf, size_t bufferLengthSamples);

#endif /* REVERB_H */