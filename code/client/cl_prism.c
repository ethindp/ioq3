/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "client.h"
#include <prism.h>

cvar_t	*cl_prism;

static PrismContext	*prismContext;
static PrismBackend	*prismBackend;
static qboolean		prismReady;
static qboolean		prismHasOutput;
static qboolean		prismHasStop;
// Speak actually completed lines, not chunks
static char		prismLine[MAXPRINTMSG];
static int		prismLineLen;


/*
====================
CL_Prism_Init
====================
*/
void CL_Prism_Init( void ) {
	PrismError	err;
	uint64_t	features;
	cl_prism = Cvar_Get( "cl_prism", "1", CVAR_ARCHIVE );
	prismReady = qfalse;
	prismContext = NULL;
	prismBackend = NULL;
	prismLineLen = 0;
	if ( !cl_prism->integer ) {
		return;
	}
	prismContext = prism_init( NULL );
	if ( !prismContext ) {
		Com_Printf( "Prism: initialization failed; speech disabled\n" );
		return;
	}
	prismBackend = prism_registry_create_best( prismContext );
	if ( !prismBackend ) {
		Com_Printf( "Prism: no speech backend available; speech disabled\n" );
		prism_shutdown( prismContext );
		prismContext = NULL;
		return;
	}
	err = prism_backend_initialize( prismBackend );
	if ( err != PRISM_OK && err != PRISM_ERROR_ALREADY_INITIALIZED ) {
		Com_Printf( "Prism: backend '%s' failed to initialize: %s\n",
			prism_backend_name( prismBackend ), prism_error_string( err ) );
		prism_backend_free( prismBackend );
		prismBackend = NULL;
		prism_shutdown( prismContext );
		prismContext = NULL;
		return;
	}
	features = prism_backend_get_features( prismBackend );
	prismHasOutput = ( features & PRISM_BACKEND_SUPPORTS_OUTPUT ) != 0;
	prismHasStop = ( features & PRISM_BACKEND_SUPPORTS_STOP ) != 0;
	prismReady = qtrue;
	Com_Printf( "Prism: speech enabled via '%s'\n", prism_backend_name( prismBackend ) );
}


/*
====================
CL_Prism_Shutdown
====================
*/
void CL_Prism_Shutdown( void ) {
	if ( prismBackend ) {
		if ( prismHasStop ) {
			(void)prism_backend_stop( prismBackend );
		}
		prism_backend_free( prismBackend );
		prismBackend = NULL;
	}
	if ( prismContext ) {
		prism_shutdown( prismContext );
		prismContext = NULL;
	}
	prismReady = qfalse;
	prismLineLen = 0;
}


/*
====================
CL_Prism_SpeakLine

Speaks a completely buffered line. Do NOT call com_print* here.
====================
*/
static void CL_Prism_SpeakLine( void ) {
	char	line[MAXPRINTMSG];
	if ( prismLineLen <= 0 ) {
		return;
	}
	if ( prismLineLen >= sizeof( line ) ) {
		prismLineLen = sizeof( line ) - 1;
	}
	Com_Memcpy( line, prismLine, prismLineLen );
	line[prismLineLen] = '\0';
	prismLineLen = 0;
	Q_CleanStr( line );
	if ( !line[0] ) {
		return;
	}
	if ( prismHasOutput ) {
		(void)prism_backend_output( prismBackend, line, qfalse );
	} else {
		(void)prism_backend_speak( prismBackend, line, qfalse );
	}
}


/*
====================
CL_Prism_Print

Buffers console text and speaks completed lines.  Runs on the main thread
only; PrismBackend instances are not thread-safe.
====================
*/
void CL_Prism_Print( const char *txt ) {
	static qboolean	recursive = qfalse;
	const char	*s;
	if ( !prismReady || !txt || !cl_prism->integer ) {
		return;
	}
	if ( recursive ) {
		return;
	}
	recursive = qtrue;
	for ( s = txt ; *s ; s++ ) {
		if ( *s == '\n' || *s == '\r' ) {
			CL_Prism_SpeakLine();
		} else if ( prismLineLen < sizeof( prismLine ) - 1 ) {
			prismLine[prismLineLen++] = *s;
		} else {
			CL_Prism_SpeakLine();
			prismLine[prismLineLen++] = *s;
		}
	}
	recursive = qfalse;
}

/*
====================
CL_Prism_Speak

Speaks text on demand (menu focus, etc).  Unlike console output this may
interrupt whatever is currently speaking, so fast menu navigation doesn't
queue stale items.
====================
*/
void CL_Prism_Speak( const char *text, qboolean interrupt ) {
	char	clean[MAXPRINTMSG];
	if ( !prismReady || !text || !cl_prism->integer ) {
		return;
	}
	Q_strncpyz( clean, text, sizeof( clean ) );
	Q_CleanStr( clean );
	if ( !clean[0] ) {
		return;
	}
	if ( prismHasOutput ) {
		(void)prism_backend_output( prismBackend, clean, interrupt );
	} else {
		(void)prism_backend_speak( prismBackend, clean, interrupt );
	}
}

void CL_Prism_Speak_Char(char ch, qboolean interrupt) {
	char txt[2];
	txt[0] = ch;
	txt[1] = '\0';
	CL_Prism_Speak(txt, interrupt);
}
