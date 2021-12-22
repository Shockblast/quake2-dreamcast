#define QGL
#include "../ref_gl/gl_local.h"

#include "../ref_gl/qgl.h"

void ( APIENTRY * qglPointParameterfEXT)( GLenum param, GLfloat value );
void ( APIENTRY * qglPointParameterfvEXT)( GLenum param, const GLfloat *value );
void ( APIENTRY * qglColorTableEXT)( int, int, int, int, int, const void * );

void ( APIENTRY * qglLockArraysEXT) (int , int);
void ( APIENTRY * qglUnlockArraysEXT) (void);

void ( APIENTRY * qglMTexCoord2fSGIS)( GLenum, GLfloat, GLfloat );
void ( APIENTRY * qglSelectTextureSGIS)( GLenum );

void ( APIENTRY * qglActiveTextureARB)( GLenum );
void ( APIENTRY * qglClientActiveTextureARB)( GLenum );

/*
** QGL_Shutdown
**
** Unloads the specified DLL then nulls out all the proc pointers.
*/
void QGL_Shutdown( void )
{
}


/*
** QGL_Init
**
** This is responsible for binding our qgl function pointers to 
** the appropriate GL stuff.  In Windows this means doing a 
** LoadLibrary and a bunch of calls to GetProcAddress.  On other
** operating systems we need to do the right thing, whatever that
** might be.
** 
*/

qboolean QGL_Init( const char *dllname )
{
	return true;
}
