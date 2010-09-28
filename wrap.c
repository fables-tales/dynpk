#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <libgen.h>
#include <limits.h>

static char* find_mypath( void )
{
	size_t size = 100;
	char *buf = NULL;

	/* Loop until we reach the maximum path length */
	while (1) {
		int r;
		buf = realloc( buf, size );

		r = readlink( "/proc/self/exe", buf, size );
		if( r < size ) {
			buf[r] = '\0';
			return buf;
		}

		size += 100;
		assert( size < PATH_MAX );
	}
}

int main( int argc, char** argv )
{
	char *exebuf = find_mypath();
	char *fcr_base, *wrapbin;

	/* Remove the prefix from this */
	fcr_base = getenv( "FAKECHROOT_BASE" );
	if( fcr_base == NULL ) {
		fprintf( stderr, "wrap: Not inside fakechroot -- aborting\n" );
		return 1;
	}

	if( strncmp( exebuf, fcr_base, strlen(fcr_base) ) != 0 ) {
		fprintf( stderr, "wrap: Not within FAKECHROOT_BASE -- aborting.\n" );
		return 1;
	}

	/* Create the wrapped binary path  */
	const char* my_abs = exebuf + strlen(fcr_base);
	wrapbin = malloc( strlen(fcr_base) + strlen("/wrap") + strlen(my_abs) + 1 );
	assert( wrapbin != NULL );
	wrapbin[0] = '\0';

	strcat( wrapbin, fcr_base );
	strcat( wrapbin, "/wrap" );
	strcat( wrapbin, my_abs );

	/* Create the path of the fakechroot's ld-linux.so.2 */
	char *fakeld = malloc( strlen(fcr_base) + strlen("/lib/ld-linux.so.2") + 1 );
	assert( fakeld != NULL );
	fakeld[0] = '\0';
	strcat( fakeld, fcr_base );
	strcat( fakeld, "/lib/ld-linux.so.2" );

	/* Create the argument list for the new child process */
	char **new_args = malloc( (argc + 2) * sizeof(char*) );

	new_args[0] = "ld-linux.so.2";
	new_args[1] = wrapbin;

	/* Argument list */
	memmove( new_args+2, argv+1, (argc-1) * sizeof(char*) );

	new_args[argc+1] = NULL;

	if( getenv( "WRAP_DEBUG") != NULL ) {
		fprintf( stderr, "wrap executing \"%s\" with args:\n", fakeld );

		int i;
		for( i=0; new_args[i] != NULL; i++ )
			printf( "\t%i: \"%s\"\n", i, new_args[i] );
	}

	if( execvp( fakeld, new_args ) == -1 ) {
		fprintf( stderr, "wrap failed to execute program :/\n" );
		return 1;
	}

	return 0;
}
