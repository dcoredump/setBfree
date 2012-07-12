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

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "reverb.h"

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#define B3R_URI "http://gareus.org/oss/lv2/b_reverb"

typedef enum {
  B3R_INPUT      = 0,
  B3R_OUTPUT     = 1,
  B3R_MIX        = 2,
} PortIndex;

typedef struct {
  float* input;
  float* output;

  float* mix;
} B3R;

double SampleRateD = 22050.0;

static LV2_Handle
instantiate(const LV2_Descriptor*     descriptor,
            double                    rate,
            const char*               bundle_path,
            const LV2_Feature* const* features)
{
  B3R* b3r = (B3R*)malloc(sizeof(B3R));
  SampleRateD = rate;
  initReverb();

  return (LV2_Handle)b3r;
}

static void
connect_port(LV2_Handle instance,
             uint32_t   port,
             void*      data)
{
  B3R* b3r = (B3R*)instance;

  switch ((PortIndex)port) {
    case B3R_INPUT:
      b3r->input = (float*)data;
      break;
    case B3R_OUTPUT:
      b3r->output = (float*)data;
      break;
    case B3R_MIX:
      b3r->mix = (float*)data;
      break;
  }
}

static void
activate(LV2_Handle instance)
{
}

static void
run(LV2_Handle instance, uint32_t n_samples)
{
  B3R* b3r = (B3R*)instance;

  const float* const input  = b3r->input;
  float* const       output = b3r->output;

  setReverbMix (*(b3r->mix));
  reverb(input, output, n_samples);
}

static void
deactivate(LV2_Handle instance)
{
}

static void
cleanup(LV2_Handle instance)
{
  free(instance);
}

const void*
extension_data(const char* uri)
{
  return NULL;
}

static const LV2_Descriptor descriptor = {
  B3R_URI,
  instantiate,
  connect_port,
  activate,
  run,
  deactivate,
  cleanup,
  extension_data
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
  switch (index) {
  case 0:
    return &descriptor;
  default:
    return NULL;
  }
}
/* vi:set ts=8 sts=2 sw=2: */