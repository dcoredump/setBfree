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

/*
 * program.c
 * 17-sep-2004/FK Upgraded to pedal/lower/upper splitpoints.
 * 22-aug-2004/FK Added MIDIControllerPgmOffset parameter and config.
 * 21-aug-2004/FK Replaced include of preamp.h with overdrive.h.
 * 14-may-2004/FK Replacing rotsim module with whirl.
 * 10-may-2003/FK New syntax and parser in a separate file.
 * 2001-12-28/FK
 *
 * Manager for program change.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>

#include "tonegen.h"
#include "program.h"
#include "vibrato.h"
#include "main.h"
#include "midi.h"

#include "reverb.h"
#include "whirl.h"
#include "overdrive.h"

#define SET_TRUE 1
#define SET_NONE 0
#define SET_FALSE -1

#define MESSAGEBUFFERSIZE 256

#define FILE_BUFFER_SIZE 2048
#define NAMESZ 22

#define NFLAGS 1		/* The nof flag fields in Programme struct */



/* Flag bits used in the first field */

#define FL_INUSE  0x0001	/* Record is in use */

#define FL_DRAWBR 0x0002	/* Set drawbars */

#define FL_ATKENV 0x0004	/* Attack envelope */
#define FL_ATKCKL 0x0008	/* Attack Click level */
#define FL_ATKCKD 0x0010	/* Attack click duration */

#define FL_RLSENV 0x0020	/* Release envelope */
#define FL_RLSCKL 0x0040	/* Release level */
#define FL_RLSCKD 0x0080	/* Release duration */

#define FL_SCANNR 0x0100	/* Vibrato scanner modulation depth */

#define FL_PRCENA 0x0200	/* Percussion on/off */
#define FL_PRCVOL 0x0400	/* Percussion soft/normal */
#define FL_PRCSPD 0x0800	/* Percussion slow/fast */
#define FL_PRCHRM 0x1000	/* Percussion 2nd/3rd */

#define FL_OVRSEL 0x2000	/* Overdrive on/off */

#define FL_ROTENA 0x4000	/* Rotary on/off */
#define FL_ROTSPS 0x8000	/* Rotary speed select */

#define FL_RVBMIX 0x00010000	/* Reverb on/off */

#define FL_DRWRND 0x00020000	/* Randomize drawbars */
#define FL_KSPLTL 0x00040000	/* Keyboard split point lower/upper */

#define FL_LOWDRW 0x00080000	/* Lower manual drawbars */
#define FL_PDLDRW 0x00100000	/* Pedal drawbars */

#define FL_KSPLTP 0x00200000	/* Keyboard split point pedal/lower */

#define FL_TRA_PD 0x00400000	/* Transpose for pedal split region */
#define FL_TRA_LM 0x00800000	/* Transpose for lower split region */
#define FL_TRA_UM 0x01000000	/* Transpose for upper split region */
#define FL_TRANSP 0x02000000	/* Global transpose */
#define FL_TRCH_A 0x04000000	/* Channel A (upper) transpose */
#define FL_TRCH_B 0x08000000	/* Channel B (lower) transpose */
#define FL_TRCH_C 0x10000000	/* Channel C (pedal) transpose */

#define FL_VCRUPR 0x20000000	/* Vib/cho upper manual routing */
#define FL_VCRLWR 0x40000000	/* Vib/cho lower manual routing */

#define ANY_TRSP (FL_TRA_PL | FL_TRA_LM | FL_TRA_UM | FL_TRANSP | \
                  FL_TRCH_A | FL_TRCH_B | FL_TRCH_C)

/* Indices to the transpose array in struct _programme. */

#define TR_TRANSP 0		/* Global transpose value */
#define TR_CHNL_A 1		/* Channel A transpose */
#define TR_CHNL_B 2		/* Channel B transpose */
#define TR_CHNL_C 3		/* Channel C transpose */
#define TR_CHA_UM 4		/* Channel A upper split region */
#define TR_CHA_LM 5		/* Channel A lower split region */
#define TR_CHA_PD 6		/* Channel A pedal split region */

typedef struct _programme {
  char name [NAMESZ];
  unsigned int flags[NFLAGS];
  unsigned int drawbars[9];
  unsigned int lowerDrawbars[9];
  unsigned int pedalDrawbars[9];
  short        keyAttackEnvelope;
  float        keyAttackClickLevel;
  float        keyAttackClickDuration;
  short        keyReleaseEnvelope;
  float        keyReleaseClickLevel;
  float        keyReleaseClickDuration;
  short        scanner;
  short        percussionEnabled;
  short        percussionVolume;
  short        percussionSpeed;
  short        percussionHarmonic;
  short        overdriveSelect;
  short        rotaryEnabled;
  short        rotarySpeedSelect;
  float        reverbMix;
  short        keyboardSplitLower;
  short        keyboardSplitPedals;
  short        transpose[7];
} Programme;

/*
 * The   short scanner   field has the following bit assignments:
 *
 * 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
 * -----------------------------------------------
 *                                            0  1  Mod. depth select:  1
 *                                            1  0  Mod. depth select:  2
 *                                            1  1  Mod. depth select:  3
 *                          0                       vibrato
 *                          1                       chorus
 *                       0                          lower manual NO vib/cho
 *                       1                          lower manual to vib/cho
 *                     0                            upper manual NO vib/cho
 *                     1                            upper manual to vib/cho
 */

#ifdef PRG_MAIN
#define MAXPROGS 129		/* To cover 1-128 */
static Programme programmes[MAXPROGS];
#else
#include "defaultpgm.h"
#endif

static int displayPgmChanges = FALSE;

/* Property codes; used internally to identity the parameter controlled. */

enum propertyId {
  pr_Name,
  pr_Drawbars,
  pr_LowerDrawbars,
  pr_PedalDrawbars,
  pr_KeyAttackEnvelope,
  pr_KeyAttackClickLevel,
  pr_KeyAttackClickDuration,
  pr_KeyReleaseEnvelope,
  pr_KeyReleaseClickLevel,
  pr_KeyReleaseClickDuration,
  pr_Scanner,
  pr_VibratoUpper,
  pr_VibratoLower,
  pr_PercussionEnabled,
  pr_PercussionVolume,
  pr_PercussionSpeed,
  pr_PercussionHarmonic,
  pr_OverdriveSelect,
  pr_RotaryEnabled,
  pr_RotarySpeedSelect,
  pr_ReverbMix,
  pr_KeyboardSplitLower,
  pr_KeyboardSplitPedals,
  pr_TransposeSplitPedals,
  pr_TransposeSplitLower,
  pr_TransposeSplitUpper,
  pr_Transpose,
  pr_TransposeUpper,
  pr_TransposeLower,
  pr_TransposePedals,
  pr_void
};

typedef struct _symbolmap {
  char * propertyName;
  int property;
} SymbolMap;

/*
 * This table maps from the string keywords used in the .prg file
 * to the internal property symbols.
 */

static SymbolMap propertySymbols [] = {
  {"name",           pr_Name},
  {"drawbars",       pr_Drawbars},
  {"drawbarsupper",  pr_Drawbars},
  {"drawbarslower",  pr_LowerDrawbars},
  {"drawbarspedals", pr_PedalDrawbars},
  {"attackenv",      pr_KeyAttackEnvelope},
  {"attacklvl",      pr_KeyAttackClickLevel},
  {"attackdur",      pr_KeyAttackClickDuration},
  {"vibrato",        pr_Scanner},
  {"vibratoknob",    pr_Scanner},
  {"vibratoupper",   pr_VibratoUpper},
  {"vibratolower",   pr_VibratoLower},
  {"perc",           pr_PercussionEnabled},
  {"percvol",        pr_PercussionVolume},
  {"percspeed",      pr_PercussionSpeed},
  {"percharm",       pr_PercussionHarmonic},
  {"overdrive",      pr_OverdriveSelect},
  {"rotary",         pr_RotaryEnabled},
  {"rotaryspeed",    pr_RotarySpeedSelect},
  {"reverbmix",      pr_ReverbMix},
  {"keysplitlower",  pr_KeyboardSplitLower},
  {"keysplitpedals", pr_KeyboardSplitPedals},
  {"trssplitpedals", pr_TransposeSplitPedals},
  {"trssplitlower",  pr_TransposeSplitLower},
  {"trssplitupper",  pr_TransposeSplitUpper},
  {"transpose",      pr_Transpose},
  {"transposeupper", pr_TransposeUpper},
  {"transposelower", pr_TransposeLower},
  {"transposepedals",pr_TransposePedals},
  {NULL, pr_void}
};

/**
 * This is to compensate for MIDI controllers that number the programs
 * from 1 to 128 on their interface. Internally we use 0-127, as does
 * MIDI.
 */
#ifndef PRG_MAIN
static int MIDIControllerPgmOffset = 1;
#endif

/* ---------------------------------------------------------------- */

/**
 * Look up the property string and return the internal property value.
 */
static int getPropertyIndex (char * sym) {
  int i;
  for (i = 0; propertySymbols[i].propertyName != NULL; i++) {
    if (!strcasecmp (propertySymbols[i].propertyName, sym)) {
      return propertySymbols[i].property;
    }
  }
  return -1;
}

/**
 * Controls if the name of a new program is displayed or not.
 * doDisplay   TRUE = display changes, FALSE = do not display changes.
 */
void setDisplayPgmChanges (int doDisplay) {
  displayPgmChanges = doDisplay;
}

/* ======================================================================== */

/**
 * Prints a message followed by the given filename and linenumber.
 * Returns the given return code, so that it may be called in a return
 * statement from within a parsing function.
 */
static int stateMessage (char * fileName,
			 int lineNumber,
			 char * msg,
			 int code) {
  fprintf (stderr, "%s in file %s on line %d\n", msg, fileName, lineNumber);
  return code;
}

/**
 * Parses a drawbar registration.
 * @param drw        The drawbar registration string.
 * @param bar        Array of intergers where the registration is stored.
 * @param lineNumber The linenumber in the input file.
 * @param fileName   The name of the current input file.
 */
static int parseDrawbarRegistration (char * drw,
				     unsigned int bar[],
				     int    lineNumber,
				     char * fileName) {

  char msg[MESSAGEBUFFERSIZE];
  int bus = 0;
  char * t = drw;

  while (bus < 9) {
    if (*t == '\0') {
      sprintf (msg, "Drawbar registration incomplete '%s'", drw);
      return stateMessage (fileName, lineNumber, msg, -1);
    }
    if ((isspace (*t)) || (*t == '-') || (*t == '_')) {
      t++;
      continue;
    }
    if (('0' <= *t) && (*t <= '8')) {
      bar[bus] = *t - '0';
      t++;
      bus++;
      continue;
    }
    else {
      sprintf (msg, "Illegal char in drawbar registration '%c'", *t);
      return stateMessage (fileName, lineNumber, msg, -1);
    }
  }

  return 0;
}



/**
 * Return TRUE if the supplied string can be interpreted as an enabling arg.
 */
static int isAffirmative (char * value) {
  int n;
  if (!strcasecmp (value, "on")) return TRUE;
  if (!strcasecmp (value, "yes")) return TRUE;
  if (!strcasecmp (value, "true")) return TRUE;
  if (!strcasecmp (value, "enabled")) return TRUE;
  if (sscanf (value, "%d", &n) == 1) {
    if (n != 0) return TRUE;
  }
  return FALSE;
}

/**
 * Return TRUE if the supplied string can be interpreted as a disabling arg.
 */
static int isNegatory (char * value) {
  int n;
  if (!strcasecmp (value, "off")) return TRUE;
  if (!strcasecmp (value, "no")) return TRUE;
  if (!strcasecmp (value, "none")) return TRUE;
  if (!strcasecmp (value, "false")) return TRUE;
  if (!strcasecmp (value, "disabled")) return TRUE;
  if (sscanf (value, "%d", &n) == 1) {
    if (n == 0) return TRUE;
  }
  return FALSE;
}

/**
 * This function parses a transpose argument. It expects an integer in the
 * range -127 .. 127. It is a helper function to bindToProgram () below.
 */
static int parseTranspose (char * val, int * vp, char * msg) {
  if (sscanf (val, "%d", vp) == 0) {
    sprintf (msg, "Transpose: integer expected : '%s'", val);
    return -1;
  }
  else if (((*vp) < -127) || (127 < (*vp))) {
    sprintf (msg, "Transpose: argument out of range : '%s'", val);
    return -1;
  }
  return 0;
}

/**
 * This function is called from the syntax parser in file pgmParser.c.
 * Return: 0 OK, non-zero error.
 */
int bindToProgram (char * fileName,
		   int    lineNumber,
		   int    pgmnr,
		   char * sym,
		   char * val)
{
  static int previousPgmNr = -1;
  int prop;
  char msg[MESSAGEBUFFERSIZE];
  float fv;
  int iv;
  int rtn;
  Programme * PGM;

  /* Check the program number */

  if ((pgmnr < 0) || (MAXPROGS <= pgmnr)) {
    sprintf (msg, "Program number %d out of range", pgmnr);
    return stateMessage (fileName, lineNumber, msg, -1);
  }

  PGM = &(programmes[pgmnr]);

  /* If this is a new program number, clear the property flags */

  if (pgmnr != previousPgmNr) {
    PGM->flags[0] = 0;
    previousPgmNr = pgmnr;
  }

  /* Scan for a matching property symbol */

  prop = getPropertyIndex (sym);

  if (prop < 0) {
    sprintf (msg, "Unrecognized property '%s'", sym);
    return stateMessage (fileName, lineNumber, msg, -1);
  }

  switch (prop) {

  case pr_Name:
    strncpy (PGM->name, val, NAMESZ);
    PGM->name[NAMESZ-1] = '\0';
    PGM->flags[0] |= FL_INUSE;
    break;

  case pr_Drawbars:
    if (!strcasecmp (val, "random")) {
      PGM->flags[0] |= (FL_INUSE | FL_DRAWBR | FL_DRWRND);
    }
    else if (!parseDrawbarRegistration (val,
					PGM->drawbars,
					lineNumber,
					fileName)) {
      PGM->flags[0] |= (FL_INUSE|FL_DRAWBR);
    }
    else {
      return -1;
    }
    break;

  case pr_LowerDrawbars:
    if (!strcasecmp (val, "random")) {
      PGM->flags[0] |= (FL_INUSE | FL_LOWDRW | FL_DRWRND);
    } else if (!parseDrawbarRegistration (val,
					  PGM->lowerDrawbars,
					  lineNumber,
					  fileName)) {
      PGM->flags[0] |= (FL_INUSE | FL_LOWDRW);
    } else {
      return -1;
    }
    break;

  case pr_PedalDrawbars:
    if (!strcasecmp (val, "random")) {
      PGM->flags[0] |= (FL_INUSE | FL_PDLDRW | FL_DRWRND);
    } else if (!parseDrawbarRegistration (val,
					  PGM->pedalDrawbars,
					  lineNumber,
					  fileName)) {
      PGM->flags[0] |= (FL_INUSE | FL_PDLDRW);
    } else {
      return -1;
    }
    break;

  case pr_Scanner:
    if (!strcasecmp (val, "v1")) {
      PGM->scanner = (PGM->scanner & 0xFF00) | VIB1;	/* in scanner.h */
      PGM->flags[0] |= (FL_INUSE|FL_SCANNR);
    }
    else if (!strcasecmp (val, "v2")) {
      PGM->scanner = (PGM->scanner & 0xFF00) | VIB2;	/* in scanner.h */
      PGM->flags[0] |= (FL_INUSE|FL_SCANNR);
    }
    else if (!strcasecmp (val, "v3")) {
      PGM->scanner = (PGM->scanner & 0xFF00) | VIB3;	/* in scanner.h */
      PGM->flags[0] |= (FL_INUSE|FL_SCANNR);
    }
    else if (!strcasecmp (val, "c1")) {
      PGM->scanner = (PGM->scanner & 0xFF00) | CHO1;	/* in scanner.h */
      PGM->flags[0] |= (FL_INUSE|FL_SCANNR);
    }
    else if (!strcasecmp (val, "c2")) {
      PGM->scanner = (PGM->scanner & 0xFF00) | CHO2;	/* in scanner.h */
      PGM->flags[0] |= (FL_INUSE|FL_SCANNR);
    }
    else if (!strcasecmp (val, "c3")) {
      PGM->scanner = (PGM->scanner & 0xFF00) | CHO3;	/* in scanner.h */
      PGM->flags[0] |= (FL_INUSE|FL_SCANNR);
    }
    else {
      sprintf (msg, "Unrecognized vibrato value '%s'", val);
      return stateMessage (fileName, lineNumber, msg, -1);
    }
    break;

  case pr_VibratoUpper:
    if (isNegatory (val)) {
      PGM->scanner &= ~0x200;
      PGM->flags[0] |= (FL_INUSE|FL_VCRUPR);
    }
    else if (isAffirmative (val)) {
      PGM->scanner |= 0x200;
      PGM->flags[0] |= (FL_INUSE|FL_VCRUPR);
    }
    else {
      sprintf (msg, "Unrecognized keyword '%s'", val);
      return stateMessage (fileName, lineNumber, msg, -1);
    }
    break;			/* pr_VibratoUpper */

  case pr_VibratoLower:
    if (isNegatory (val)) {
      PGM->scanner &= ~0x100;
      PGM->flags[0] |= (FL_INUSE|FL_VCRLWR);
    }
    else if (isAffirmative (val)) {
      PGM->scanner |= 0x100;
      PGM->flags[0] |= (FL_INUSE|FL_VCRLWR);
    }
    else {
      sprintf (msg, "Unrecognized keyword '%s'", val);
      return stateMessage (fileName, lineNumber, msg, -1);
    }
    break;			/* pr_VibratoUpper */

  case pr_PercussionEnabled:
    if (isAffirmative (val)) {
      PGM->percussionEnabled = TRUE;
      PGM->flags[0] |= (FL_INUSE|FL_PRCENA);
    }
    else if (isNegatory (val)) {
      PGM->percussionEnabled = FALSE;
      PGM->flags[0] |= (FL_INUSE|FL_PRCENA);
    }
    else {
      sprintf (msg, "Unrecognized percussion enabled value '%s'", val);
      return stateMessage (fileName, lineNumber, msg, -1);
    }
    break;

  case pr_PercussionVolume:
    if (!strcasecmp (val, "normal") ||
	!strcasecmp (val, "high")   ||
	!strcasecmp (val, "hi")) {
      PGM->percussionVolume = FALSE;
      PGM->flags[0] |= (FL_INUSE|FL_PRCVOL);
    }
    else if (!strcasecmp (val, "soft") ||
	     !strcasecmp (val, "low")  ||
	     !strcasecmp (val, "lo")) {
      PGM->percussionVolume = TRUE;
      PGM->flags[0] |= (FL_INUSE|FL_PRCVOL);
    }
    else {
      sprintf (msg, "Unrecognized percussion volume argument '%s'", val);
      return stateMessage (fileName, lineNumber, msg, -1);
    }
    break;

  case pr_PercussionSpeed:
    if (!strcasecmp (val, "fast") ||
	!strcasecmp (val, "high") ||
	!strcasecmp (val, "hi")) {
      PGM->percussionSpeed = TRUE;
      PGM->flags[0] |= (FL_INUSE|FL_PRCSPD);
    }
    else if (!strcasecmp (val, "slow") ||
	     !strcasecmp (val, "low")  ||
	     !strcasecmp (val, "lo")) {
      PGM->percussionSpeed = FALSE;
      PGM->flags[0] |= (FL_INUSE|FL_PRCSPD);
    }
    else {
      sprintf (msg, "Unrecognized percussion speed argument '%s'", val);
      return stateMessage (fileName, lineNumber, msg, -1);
    }
    break;

  case pr_PercussionHarmonic:
    if (!strcasecmp (val, "second") ||
	!strcasecmp (val, "2nd")    ||
	!strcasecmp (val, "low")    ||
	!strcasecmp (val, "lo")) {
      PGM->percussionHarmonic = TRUE;
      PGM->flags[0] |= (FL_INUSE|FL_PRCHRM);
    }
    else if (!strcasecmp (val, "third") ||
	     !strcasecmp (val, "3rd")   ||
	     !strcasecmp (val, "high")  ||
	     !strcasecmp (val, "hi")) {
      PGM->percussionHarmonic = FALSE;
      PGM->flags[0] |= (FL_INUSE|FL_PRCHRM);
    }
    else {
      sprintf (msg, "Unrecognized percussion harmonic option '%s'", val);
      return stateMessage (fileName, lineNumber, msg, -1);
    }
    break;

  case pr_OverdriveSelect:
    if (isNegatory (val)) {
      PGM->overdriveSelect = TRUE;
      PGM->flags[0] |= (FL_INUSE|FL_OVRSEL);
    }
    else if (isAffirmative (val)) {
      PGM->overdriveSelect = FALSE;
      PGM->flags[0] |= (FL_INUSE|FL_OVRSEL);
    }
    else {
      sprintf (msg, "Unrecognized overdrive select argument '%s'", val);
      return stateMessage (fileName, lineNumber, msg, -1);
    }
    break;

  case pr_RotarySpeedSelect:
    if (!strcasecmp (val, "tremolo") ||
	!strcasecmp (val, "fast")    ||
	!strcasecmp (val, "high")    ||
	!strcasecmp (val, "hi")) {
      PGM->rotarySpeedSelect = WHIRL_FAST;
      PGM->flags[0] |= (FL_INUSE|FL_ROTSPS);
    }
    else if (!strcasecmp (val, "chorale") ||
	     !strcasecmp (val, "slow")    ||
	     !strcasecmp (val, "low")     ||
	     !strcasecmp (val, "lo")) {
      PGM->rotarySpeedSelect = WHIRL_SLOW;
      PGM->flags[0] |= (FL_INUSE|FL_ROTSPS);
    }
    else if (!strcasecmp (val, "stop")  ||
	     !strcasecmp (val, "zero")  ||
	     !strcasecmp (val, "break") ||
	     !strcasecmp (val, "stopped")) {
      PGM->rotarySpeedSelect = WHIRL_STOP;
      PGM->flags[0] |= (FL_INUSE|FL_ROTSPS);
    }
    else {
      sprintf (msg, "Unrecognized rotary speed argument '%s'", val);
      return stateMessage (fileName, lineNumber, msg, -1);
    }
    break;

  case pr_ReverbMix:
    PGM->flags[0] |= (FL_INUSE|FL_RVBMIX);
    if (sscanf (val, "%f", &fv) == 0) {
      sprintf (msg, "Unrecognized reverb mix value : '%s'", val);
      return stateMessage (fileName, lineNumber, msg, -1);
    }
    else if ((fv < 0.0) || (1.0 < fv)) {
      sprintf (msg, "Reverb mix value out of range : %f", fv);
      return stateMessage (fileName, lineNumber, msg, -1);
    }
    else {
      PGM->reverbMix = fv;
    }
    break;

  case pr_KeyboardSplitLower:
    PGM->flags[0] |= (FL_INUSE|FL_KSPLTL);
    if (sscanf (val, "%d", &iv) == 0) {
      sprintf (msg, "Lower split: unparsable MIDI note number : '%s'", val);
      return stateMessage (fileName, lineNumber, msg, -1);
    }
    else if ((iv < 0) || (127 < iv)) {
      sprintf (msg, "Lower split: MIDI note number out of range: '%s'", val);
      return stateMessage (fileName, lineNumber, msg, -1);
    }
    else {
      PGM->keyboardSplitLower = (short) iv;
    }
    break;

  case pr_KeyboardSplitPedals:
    PGM->flags[0] |= (FL_INUSE|FL_KSPLTP);
    if (sscanf (val, "%d", &iv) == 0) {
      sprintf (msg, "Pedal split: unparsable MIDI note number : '%s'", val);
      return stateMessage (fileName, lineNumber, msg, -1);
    }
    else if ((iv < 0) || (127 < iv)) {
      sprintf (msg, "Pedal split: MIDI note number out of range: '%s'", val);
      return stateMessage (fileName, lineNumber, msg, -1);
    }
    else {
      PGM->keyboardSplitPedals = (short) iv;
    }
    break;

    /*
     * A macro to avoid tedious repetition of code. The parameter F is
     * the bit in the 0th flag word for the parameter. The parameter I is
     * the index in the PGM->transpose[] array.
     * This macro is obviously very context-dependent and is therefore
     * undefined as soon as we do not need it anymore.
     */

#define SET_TRANSPOSE(F,I) PGM->flags[0] |= (FL_INUSE|(F)); \
  if ((rtn = parseTranspose (val, &iv, msg))) { \
    return stateMessage (fileName, lineNumber, msg, rtn); \
  } else { \
    PGM->transpose[(I)] = iv; \
  }

  case pr_TransposeSplitPedals:
    SET_TRANSPOSE(FL_TRA_PD, TR_CHA_PD);
    break;

  case pr_TransposeSplitLower:
    SET_TRANSPOSE(FL_TRA_LM, TR_CHA_LM);
    break;

  case pr_TransposeSplitUpper:
    SET_TRANSPOSE(FL_TRA_UM, TR_CHA_UM);
    break;

  case pr_Transpose:
    SET_TRANSPOSE(FL_TRANSP, TR_TRANSP);
    break;

  case pr_TransposeUpper:
    SET_TRANSPOSE(FL_TRCH_A, TR_CHNL_A);
    break;

  case pr_TransposeLower:
    SET_TRANSPOSE(FL_TRCH_B, TR_CHNL_B);
    break;

  case pr_TransposePedals:
    SET_TRANSPOSE(FL_TRCH_C, TR_CHNL_C);
    break;

#undef SET_TRANSPOSE

  } /* switch property */

  return 0;
}

#ifndef PRG_MAIN
/**
 * Installs random values in the supplied array.
 * @param drawbars  Array where to store drawbar settings.
 * @param buf       If non-NULL a display string of the values is stored.
 */
static void randomizeDrawbars (unsigned int drawbars [], char * buf) {
  int i;

  for (i = 0; i < 9; i++) {
    drawbars[i] = rand () % 9;
  }

  if (buf != NULL) {
    sprintf (buf, "%c%c%c %c%c%c%c %c%c",
	     '0' + drawbars[0],
	     '0' + drawbars[1],
	     '0' + drawbars[2],
	     '0' + drawbars[3],
	     '0' + drawbars[4],
	     '0' + drawbars[5],
	     '0' + drawbars[6],
	     '0' + drawbars[7],
	     '0' + drawbars[8]);
  }
}

/**
 * This is the routine called by the MIDI parser when it detects
 * a Program Change message.
 */
void installProgram (unsigned char uc) {
  int p = (int) uc;

  p += MIDIControllerPgmOffset;

  if ((0 < p) && (p < MAXPROGS)) {

    Programme * PGM = &(programmes[p]);
    unsigned int flags0 = PGM->flags[0];
    char display[128];

    if (flags0 & FL_INUSE) {

      strcpy (display, PGM->name);

      if (flags0 & FL_DRWRND) {
	char buf [32];

	if (flags0 & FL_DRAWBR) {
	  randomizeDrawbars (PGM->drawbars, buf);
	  strcat (display, "UPR:");
	  strcat (display, buf);
	}

	if (flags0 & FL_LOWDRW) {
	  randomizeDrawbars (PGM->lowerDrawbars, buf);
	  strcat (display, "LOW:");
	  strcat (display, buf);
	}

	if (flags0 & FL_PDLDRW) {
	  randomizeDrawbars (PGM->pedalDrawbars, buf);
	  strcat (display, "PDL:");
	  strcat (display, buf);
	}
      }

      if (displayPgmChanges) {
/*	fprintf (stderr, "%22c\r%s\r", ' ', PGM->name); */
	fprintf (stdout, "\r%s\r", display);
	fflush (stdout);
      }

      if (flags0 & FL_DRAWBR) {
	setDrawBars (0, PGM->drawbars);
      }

      if (flags0 & FL_LOWDRW) {
	setDrawBars (1, PGM->lowerDrawbars);
      }

      if (flags0 & FL_PDLDRW) {
	setDrawBars (2, PGM->pedalDrawbars);
      }

      /* Key attack click */

      /* Key release click */

      if (flags0 & FL_SCANNR) {
	setVibrato (PGM->scanner & 0x00FF);
      }

      if (flags0 & FL_VCRUPR) {
	setVibratoUpper (PGM->scanner & 0x200);
      }

      if (flags0 & FL_VCRLWR) {
	setVibratoLower (PGM->scanner & 0x100);
      }

      if (flags0 & FL_PRCENA) {
	setPercussionEnabled (PGM->percussionEnabled);
      }

      if (flags0 & FL_PRCVOL) {
	setPercussionVolume (PGM->percussionVolume);
      }

      if (flags0 & FL_PRCSPD) {
	setPercussionFast (PGM->percussionSpeed);
      }

      if (flags0 & FL_PRCHRM) {
	setPercussionFirst (PGM->percussionHarmonic);
      }

      if (flags0 & FL_OVRSEL) {
	setClean (PGM->overdriveSelect);
      }

      if (flags0 & FL_ROTENA) {
	/* Rotary enabled */
      }

      if (flags0 & FL_ROTSPS) {
	setRevSelect ((int) (PGM->rotarySpeedSelect));
      }

      if (flags0 & FL_RVBMIX) {
	setReverbMix (PGM->reverbMix);
      }

      if (flags0 & (FL_KSPLTL|FL_KSPLTP|FL_TRA_PD|FL_TRA_LM|FL_TRA_UM)) {
	int b;
	b  = (flags0 & FL_KSPLTP) ?  1 : 0;
	b |= (flags0 & FL_KSPLTL) ?  2 : 0;
	b |= (flags0 & FL_TRA_PD) ?  4 : 0;
	b |= (flags0 & FL_TRA_LM) ?  8 : 0;
	b |= (flags0 & FL_TRA_UM) ? 16 : 0;
	setKeyboardSplitMulti (b,
			       PGM->keyboardSplitPedals,
			       PGM->keyboardSplitLower,
			       PGM->transpose[TR_CHA_PD],
			       PGM->transpose[TR_CHA_LM],
			       PGM->transpose[TR_CHA_UM]);
      }

      if (flags0 & FL_TRANSP) {
	setKeyboardTranspose (PGM->transpose[TR_TRANSP]);
      }

      if (flags0 & FL_TRCH_A) {
	setKeyboardTransposeA (PGM->transpose[TR_CHNL_A]);
      }

      if (flags0 & FL_TRCH_B) {
	setKeyboardTransposeB (PGM->transpose[TR_CHNL_B]);
      }

      if (flags0 & FL_TRCH_C) {
	setKeyboardTransposeC (PGM->transpose[TR_CHNL_C]);
      }

    }
  }
}

static void setMIDIControllerPgmOffset (int offset) {
  if (offset<0 || offset>1) return;
  MIDIControllerPgmOffset = offset;
}


/**
 * Configures this modules.
 */
int pgmConfig (ConfigContext * cfg) {
  int ack = 0;
  int ival;
  if ((ack = getConfigParameter_i ("pgm.controller.offset",
				   cfg, &ival)) == 1) {
    setMIDIControllerPgmOffset (ival);
  }

  return ack;
}
#endif

static const ConfigDoc doc[] = {
  {"pgm.controller.offset", CFG_INT, "1", "Compensate for MIDI controllers that number the programs from 1 to 128. Internally we use 0-127, as does MIDI. range: [0,1]"},
  {NULL}
};

const ConfigDoc *pgmDoc () {
  return doc;
}


#define MAXROWS 18
#define MAXCOLS  4

/**
 * Displays the number and names of the loaded programmes in a multicolumn
 * list on the given output stream.
 */
void listProgrammes (FILE * fp) {
  int matrix [MAXROWS][MAXCOLS];
  int row;
  int col;
  int i;
  int mxUse = 0;
  int mxLimit = MAXROWS * MAXCOLS;

  fprintf(fp, "MIDI Program Table:\n");

  for (row = 0; row < MAXROWS; row++) {
    for (col = 0; col < MAXCOLS; col++) {
      matrix [row][col] = -1;
    }
  }

  for (i = row = col = 0; i < MAXPROGS; i++) {
    if (programmes[i].flags[0] & FL_INUSE) {
      if (mxUse < mxLimit) {
	matrix[row][col] = i;
	mxUse++;
	row++;
	if (MAXROWS <= row) {
	  row = 0;
	  col++;
	}
      }
    }
  }

  for (row = 0; row < MAXROWS; row++) {

    for (col = 0; col < MAXCOLS; col++) {
      int x = matrix[row][col];
      if (-1 < x) {
	fprintf (fp, "%3d:%-15.15s", x, programmes[x].name);
      }
      else {
	fprintf (fp, "%19s", " ");
      }
      if (col < 3) {
	fprintf (fp, " ");
      }
      else {
	fprintf (fp, "\n");
      }
    }
  }
}

/** walks through all available programs and counts the number of records
 * which are is in use.
 *
 * @param clear if set, all programs are erased
 */
int walkProgrammes (int clear) {
  int cnt=0;
  int i;
  for (i=0;i<MAXPROGS;++i) {
    if (clear) programmes[i].flags[0] &=~FL_INUSE;
    if (programmes[i].flags[0] & FL_INUSE) cnt++;
  }
  return cnt;
}

#ifdef PRG_MAIN
#include "pgmParser.h"

void hardcode_program (FILE * fp) {
  int i;
  fprintf(fp, "/* generated by programd */\n");
  fprintf(fp, "#define MAXPROGS (%d)\n", MAXPROGS);
  fprintf(fp, "static Programme programmes[MAXPROGS] = {\n");
  for (i=0;i<MAXPROGS;++i) {
    int j;
    fprintf(fp,"  {");
    fprintf(fp,"\"%s\", {", programmes[i].name);
    for (j=0;j<1;++j) fprintf(fp,"%u, ", programmes[i].flags[j]);
    fprintf(fp,"}, {");
    for (j=0;j<9;++j) fprintf(fp,"%u, ", programmes[i].drawbars[j]);
    fprintf(fp,"}, {");
    for (j=0;j<9;++j) fprintf(fp,"%u, ", programmes[i].lowerDrawbars[j]);
    fprintf(fp,"}, {");
    for (j=0;j<9;++j) fprintf(fp,"%u, ", programmes[i].pedalDrawbars[j]);
    fprintf(fp,"}, ");
    fprintf(fp,"%d, ", programmes[i].keyAttackEnvelope);
    fprintf(fp,"%f, ", programmes[i].keyAttackClickLevel);
    fprintf(fp,"%f, ", programmes[i].keyAttackClickDuration);
    fprintf(fp,"%d, ", programmes[i].keyReleaseEnvelope);
    fprintf(fp,"%f, ", programmes[i].keyReleaseClickLevel);
    fprintf(fp,"%f, ", programmes[i].keyReleaseClickDuration);
    fprintf(fp,"%d, ", programmes[i].scanner);
    fprintf(fp,"%d, ", programmes[i].percussionEnabled);
    fprintf(fp,"%d, ", programmes[i].percussionVolume);
    fprintf(fp,"%d, ", programmes[i].percussionSpeed);
    fprintf(fp,"%d, ", programmes[i].percussionHarmonic);
    fprintf(fp,"%d, ", programmes[i].overdriveSelect);
    fprintf(fp,"%d, ", programmes[i].rotaryEnabled);
    fprintf(fp,"%d, ", programmes[i].rotarySpeedSelect);
    fprintf(fp,"%f, ", programmes[i].reverbMix);
    fprintf(fp,"%d, ", programmes[i].keyboardSplitLower);
    fprintf(fp,"%d, ", programmes[i].keyboardSplitPedals);
    fprintf(fp,"{");
    for (j=0;j<7;++j) fprintf(fp,"%d, ", programmes[i].transpose[j]);
    fprintf(fp,"}");

    fprintf(fp,"},\n");
  }
  fprintf(fp,"};\n");
}

int main (int argc, char **argv) {
  if (argc < 2) return -1;
  if (loadProgrammeFile (argv[1]) != 0 /* P_OK */) return -1;
  listProgrammes(stderr);
  hardcode_program(stdout);
  return 0;
}
#endif
/* vi:set ts=8 sts=2 sw=2: */