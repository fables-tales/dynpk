import os, subprocess, re, stat, sys

class File:
    def __init__(self, source, dest, perms = None):
        self.source = source
        self.dest = dest
        self.perms = perms

    def __hash__(self):
        s = "%s %s %s" % (self.source, self.dest, str(self.perms))
        return s.__hash__()

    def __eq__(self, other):
        if ( self.source != other.source or
             self.dest != other.dest or
             self.perms != other.perms ):
            return False
        return True

class Entity:
    def __init__(self):
        pass

def get_elf_libs(fname):
    p = subprocess.Popen("ldd %s | grep '=>'" % fname,
                         shell = True,
                         stdout = subprocess.PIPE,
                         stderr = subprocess.PIPE)
    so, se = p.communicate()
    if p.wait() != 0:
        print se
        print "ERRROR: ldd \"%s\" failed -- aborting" % fname
        sys.exit(1)

    return re.findall( "/[^ ]+", so )

class Elf(Entity):
    def __init__(self, fname):
        self.fname = fname
        Entity.__init__(self)

    def _get_libs(self):
        return get_elf_libs(self.fname)

    def get_files(self):
        files = []

        for lib in self._get_libs():
            files.append( File( lib, lib[1:] ) )

        files.append( File( self.fname,
                            "bin/%s" % os.path.basename(self.fname) ) )
        return files


    def add_extras(self, basedir):
        pass

class InstalledRpm(Entity):
    def __init__(self, pkgname):
        self.pkgname = pkgname
        Entity.__init__(self)

    def get_files(self):
        p = subprocess.Popen( "rpm -ql %s" % self.pkgname,
                              stdout = subprocess.PIPE, shell = True)
        so, se = p.communicate()
        assert( p.wait() == 0 )

        files = []
        for x in so.splitlines():
            files.append( File( x, x[1:] ) )
        return files

    def add_extras(self, basedir):
        pass
