/* Prevent dynamic libraries from being loaded from places we don't want */
/*  Copyright 2010 Robert Spanton <rspanton@zepler.net>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <link.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <unistd.h>

unsigned int la_version( unsigned int version )
{
	return 1;
}

unsigned int la_objopen( struct link_map *lmp, Lmid_t lmid, unsigned int *cookie )
{
	static char *prefix = NULL;

	if( prefix == NULL ) {
		prefix = getenv("AUDIT_PREFIX");
		if( prefix == NULL ) {
			fprintf( stderr, "ERROR: No audit prefix specified in AUDIT_PREFIX environment variable\n" );
			exit(1);
		}

		prefix = canonicalize_file_name( prefix );
		assert( prefix != NULL );
	}

	if( lmp->l_name[0] != 0 ) {
		if( strcmp( lmp->l_name, "ld-linux.so.2" ) == 0 ) {
			char exebuf[512];
			int r;
			r = readlink( "/proc/self/exe", exebuf, sizeof(exebuf) );
			assert( r != -1 );
			exebuf[r] = '\0';

			if( strncmp( exebuf, prefix, strlen(prefix) ) != 0 ) {
				fprintf( stderr, "Stopping binary from loading ld-linux.so.2\n" );
				fprintf( stderr, "in process \"%s\"\n", exebuf );
				exit(1);
			}
		} else {
			char *absname = canonicalize_file_name( lmp->l_name );
			if( absname == NULL ) {
				fprintf( stderr, "libaudit: ERROR: Couldn't canonicalize \"%s\"\n", lmp->l_name );
				exit(1);
			}

			if( strncmp( absname, prefix, strlen(prefix) ) != 0 ) {
				fprintf( stderr, "ERROR:\tAttempted to load dynamic library outside allowed prefix\n" );
				fprintf( stderr, "\tApplication attempted to load \"%s\"\n", lmp->l_name );
				fprintf( stderr, "\n\tAborting\n");
				exit(1);
			}

			free(absname);
		}
	}

	if( getenv( "AUDIT_DEBUG" ) )
		printf( "file: \"%s\" loaded\n", lmp->l_name );

	return LA_FLG_BINDTO | LA_FLG_BINDFROM;
}
