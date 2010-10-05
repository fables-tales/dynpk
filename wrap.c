
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>

#define BASE_FILE ".fakechroot-base"

/* Discover what our real path is */
static char* find_mypath( void )
{
	size_t size = 100;
	char *buf = NULL;

	/* Loop until we reach the maximum path length */
	while (1) {
		int r;
		buf = realloc( buf, size );
		assert( buf != NULL );

		r = readlink( "/proc/self/exe", buf, size );
		if( r < size ) {
			buf[r] = '\0';
			return buf;
		}

		size += 100;
		assert( size < PATH_MAX );
	}
}

/* Return true if the given file exists */
static bool file_exists( const char *path )
{
	int fd = open( path, 0, O_RDONLY );

	if( fd == -1 )
		return false;

	close(fd);
	return true;
}

/* Find the base of the fakechroot */
static char* find_chroot_base( const char* exepath )
{
	char *ret, *dir = strdup( exepath );
	/* Keep dropping directories until we get to one with 
	   a file called '.fakechroot-base' in it */

	while(1) {
		char *tfname = NULL;
		dirname( dir );

		/* Append the '.fakechroot-base' filename */
		asprintf( &tfname, "%s/%s", dir, BASE_FILE );
		assert( tfname != NULL );

		if( file_exists(tfname) ) {
			/* We've found the fakechroot base */
			free(tfname);
			break;
		}

		/* Abort if we've reached the bottom */
		if( strlen(dir) == 1 ) {
			fprintf( stderr, "wrap: Cannot find fakechroot base -- aborting\n" );
			exit(1);
		}
	
		free(tfname);
	}

	if( getenv( "WRAP_DEBUG" ) != NULL )
		fprintf( stderr, "wrap: found base: %s\n", dir );
	/* No need to have all the cruft hanging around: */
	ret = strdup(dir);
	free(dir);
	return ret;
}

static char* str_replace( const char* haystack, const char* from, const char* to )
{
	char *s = strdup(haystack);
	size_t from_len = strlen(from);
	size_t to_len = strlen(to);
	char* p;

	while( (p = strstr(s, from )) != NULL ) {
		char *n = malloc( strlen(s) - from_len + to_len + 1 );
		assert( n != NULL );

		n[0] = '\0';
		strncat( n, s, p-s );
		strcat( n, to );
		strcat( n, p + from_len );

		free(s);
		s = n;
	}

	return s;
}

/* Configure the environment from the dynpk.config file*/
static void conf_fakeenv( const char* exepath )
{
	char *chroot_base = find_chroot_base(exepath);
	FILE *conf_file;
	char *conf_fname;
	
	char *linebuf;
	size_t linebuf_size;
	ssize_t len;

	/* Open the configuration file to get the environment config out */
	asprintf( &conf_fname, "%s/dynpk.config", chroot_base );
	assert( conf_fname != NULL );

	conf_file = fopen( conf_fname, "r" );
	if(conf_file == NULL) {
		fprintf( stderr, "wrap error: Couldn't open \"%s\"\n", conf_fname );
		exit(1);
	}

	free(conf_fname);

	linebuf_size = 200;
	linebuf = malloc(linebuf_size);
	assert( linebuf != NULL );

	while( (len = getline( &linebuf, &linebuf_size, conf_file )) != -1 ) {
		char *line, *equals, *varname, *val;

		if(len == 0)
			continue;

		line = linebuf;
		while( *line != '\0' && isspace(*line) )
			line++;

		if( line[0] == '#' || line[0] == '\0' )
			continue;

		equals = strstr( line, "=" );
		if( equals == NULL ) {
			fprintf( stderr, "wrap error: No '=' in line \"%s\"\n", line );
			exit(1);
		}

		*equals = '\0';
		varname = line;
		val = equals + 1;
		if( *val == '\0' ) {
			fprintf( stderr, "wrap error: Nothing after '='\n" );
			exit(1);
		}

		/* Remove the trailing newline from val */
		int l = strlen(val);
		if( val[l-1] == '\n' )
			val[l-1] = '\0';

		val = str_replace( val, "$FAKECHROOT_BASE", chroot_base );

		if( getenv( "WRAP_DEBUG" ) != NULL )
			printf( "Setting \"%s\" to \"%s\"\n", varname, val );

		setenv( varname, val, 1 );
		free(val);
	}
	free(linebuf);

	fclose(conf_file);
}

int main( int argc, char** argv )
{
	char *exebuf = find_mypath();
	char *fcr_base, *wrapbin;

	/* Remove the prefix from this */
	fcr_base = getenv( "FAKECHROOT_BASE" );
	if( fcr_base == NULL ) {
		conf_fakeenv(exebuf);

		fcr_base = getenv( "FAKECHROOT_BASE" );
		assert( fcr_base != NULL );
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

	free(exebuf);
	exebuf = NULL;

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
