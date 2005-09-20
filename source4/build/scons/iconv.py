# Based on the M4 macro by Bruno Haible.

def exists(env):
	return True

def generate(env):
	env['custom_tests']['CheckIconv'] = CheckIconv

def _CheckIconvPath(context,path):
	# Some systems have iconv in libc, some have it in libiconv (OSF/1 and
  	# those with the standalone portable libiconv installed).
	context.Message("checking for iconv in " + path)

	main = """
int main()
{
	iconv_t cd = iconv_open("","");
    iconv(cd,NULL,NULL,NULL,NULL);
    iconv_close(cd);
	return 0;
}"""

	have_giconv_iconv = context.TryLink("""
#include <stdlib.h>
#include <giconv.h>
""" + main, '.c')
	if have_giconv_iconv:
		context.Result(1)
		return ("giconv.h", "")

	have_iconv_iconv = context.TryLink("""
#include <stdlib.h>
#include <iconv.h>
""" + main, '.c')

	if have_iconv_iconv:
		context.Result(1)
		return ("iconv.h", "")

	#FIXME: Add -lgiconv
	have_giconv_lib_iconv = context.TryLink("""
#include <stdlib.h>
#include <giconv.h>
""" + main, '.c')
	if have_giconv_lib_iconv:
		context.Result(1)
		return ("giconv.h", "-lgiconv")

	#FIXME: Add -liconv
	have_iconv_lib_iconv = context.TryLink("""
#include <stdlib.h>
#include <iconv.h>
"""+main,'.c')

	if have_iconv_lib_iconv:
		context.Result(1)
		return ("iconv.h", "-liconv")

	return None

def CheckIconv(context):
	context.Message("checking for iconv")
	
	look_dirs = ['/usr','/usr/local','/sw']

	for p in look_dirs:
		_CheckIconvPath(context,p) #FIXME: Handle return value

	if context.TryRun("""
#include <iconv.h>
main() {
       iconv_t cd = iconv_open("ASCII", "UCS-2LE");
       if (cd == 0 || cd == (iconv_t)-1) return -1;
       return 0;
}
""", '.c'):
		context.Result(1)
		return (1,[])
	
	context.Result(0)
	return (0,[])
