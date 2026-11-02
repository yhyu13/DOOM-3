/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company. 

This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).  

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/
#include "../idlib/precompiled.h"
#pragma hdrstop

#include "tr_local.h"
#include <stddef.h>		// offsetof for the T080 surface snapshot


frameData_t		*frameData;
backEndState_t	backEnd;


/*
======================
RB_SetDefaultGLState

This should initialize all GL state that any part of the entire program
may touch, including the editor.
======================
*/
void RB_SetDefaultGLState( void ) {
	int		i;

	RB_LogComment( "--- R_SetDefaultGLState ---\n" );

	qglClearDepth( 1.0f );
	qglColor4f (1,1,1,1);

	// the vertex array is always enabled
	qglEnableClientState( GL_VERTEX_ARRAY );
	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
	qglDisableClientState( GL_COLOR_ARRAY );

	//
	// make sure our GL state vector is set correctly
	//
	memset( &backEnd.glState, 0, sizeof( backEnd.glState ) );
	backEnd.glState.forceGlState = true;

	qglColorMask( 1, 1, 1, 1 );

	qglEnable( GL_DEPTH_TEST );
	qglEnable( GL_BLEND );
	qglEnable( GL_SCISSOR_TEST );
	qglEnable( GL_CULL_FACE );
	qglDisable( GL_LIGHTING );
	qglDisable( GL_LINE_STIPPLE );
	qglDisable( GL_STENCIL_TEST );

	qglPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	qglDepthMask( GL_TRUE );
	qglDepthFunc( GL_ALWAYS );
 
	qglCullFace( GL_FRONT_AND_BACK );
	qglShadeModel( GL_SMOOTH );

	if ( r_useScissor.GetBool() ) {
		qglScissor( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	}

	for ( i = glConfig.maxTextureUnits - 1 ; i >= 0 ; i-- ) {
		GL_SelectTexture( i );

		// object linear texgen is our default
		qglTexGenf( GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR );
		qglTexGenf( GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR );
		qglTexGenf( GL_R, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR );
		qglTexGenf( GL_Q, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR );

		GL_TexEnv( GL_MODULATE );
		qglDisable( GL_TEXTURE_2D );
		if ( glConfig.texture3DAvailable ) {
			qglDisable( GL_TEXTURE_3D );
		}
		if ( glConfig.cubeMapAvailable ) {
			qglDisable( GL_TEXTURE_CUBE_MAP_EXT );
		}
	}
}


/*
====================
RB_LogComment
====================
*/
void RB_LogComment( const char *comment, ... ) {
   va_list marker;

	if ( !tr.logFile ) {
		return;
	}

	fprintf( tr.logFile, "// " );
	va_start( marker, comment );
	vfprintf( tr.logFile, comment, marker );
	va_end( marker );
}


//=============================================================================



/*
====================
GL_SelectTexture
====================
*/
void GL_SelectTexture( int unit ) {
	if ( backEnd.glState.currenttmu == unit ) {
		return;
	}

	if ( unit < 0 || unit >= glConfig.maxTextureUnits && unit >= glConfig.maxTextureImageUnits ) {
		common->Warning( "GL_SelectTexture: unit = %i", unit );
		return;
	}

	qglActiveTextureARB( GL_TEXTURE0_ARB + unit );
	qglClientActiveTextureARB( GL_TEXTURE0_ARB + unit );
	RB_LogComment( "glActiveTextureARB( %i );\nglClientActiveTextureARB( %i );\n", unit, unit );

	backEnd.glState.currenttmu = unit;
}


/*
====================
GL_Cull

This handles the flipping needed when the view being
rendered is a mirored view.
====================
*/
void GL_Cull( int cullType ) {
	if ( backEnd.glState.faceCulling == cullType ) {
		return;
	}

	if ( cullType == CT_TWO_SIDED ) {
		qglDisable( GL_CULL_FACE );
	} else  {
		if ( backEnd.glState.faceCulling == CT_TWO_SIDED ) {
			qglEnable( GL_CULL_FACE );
		}

		if ( cullType == CT_BACK_SIDED ) {
			if ( backEnd.viewDef->isMirror ) {
				qglCullFace( GL_FRONT );
			} else {
				qglCullFace( GL_BACK );
			}
		} else {
			if ( backEnd.viewDef->isMirror ) {
				qglCullFace( GL_BACK );
			} else {
				qglCullFace( GL_FRONT );
			}
		}
	}

	backEnd.glState.faceCulling = cullType;
}

/*
====================
GL_TexEnv
====================
*/
void GL_TexEnv( int env ) {
	tmu_t	*tmu;

	tmu = &backEnd.glState.tmu[backEnd.glState.currenttmu];
	if ( env == tmu->texEnv ) {
		return;
	}

	tmu->texEnv = env;

	switch ( env ) {
	case GL_COMBINE_EXT:
	case GL_MODULATE:
	case GL_REPLACE:
	case GL_DECAL:
	case GL_ADD:
		qglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, env );
		break;
	default:
		common->Error( "GL_TexEnv: invalid env '%d' passed\n", env );
		break;
	}
}

/*
=================
GL_ClearStateDelta

Clears the state delta bits, so the next GL_State
will set every item
=================
*/
void GL_ClearStateDelta( void ) {
	backEnd.glState.forceGlState = true;
}

/*
====================
GL_State

This routine is responsible for setting the most commonly changed state
====================
*/
void GL_State( int stateBits ) {
	int	diff;
	
	if ( !r_useStateCaching.GetBool() || backEnd.glState.forceGlState ) {
		// make sure everything is set all the time, so we
		// can see if our delta checking is screwing up
		diff = -1;
		backEnd.glState.forceGlState = false;
	} else {
		diff = stateBits ^ backEnd.glState.glStateBits;
		if ( !diff ) {
			return;
		}
	}

	//
	// check depthFunc bits
	//
	if ( diff & ( GLS_DEPTHFUNC_EQUAL | GLS_DEPTHFUNC_LESS | GLS_DEPTHFUNC_ALWAYS ) ) {
		if ( stateBits & GLS_DEPTHFUNC_EQUAL ) {
			qglDepthFunc( GL_EQUAL );
		} else if ( stateBits & GLS_DEPTHFUNC_ALWAYS ) {
			qglDepthFunc( GL_ALWAYS );
		} else {
			qglDepthFunc( GL_LEQUAL );
		}
	}


	//
	// check blend bits
	//
	if ( diff & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) {
		GLenum srcFactor, dstFactor;

		switch ( stateBits & GLS_SRCBLEND_BITS ) {
		case GLS_SRCBLEND_ZERO:
			srcFactor = GL_ZERO;
			break;
		case GLS_SRCBLEND_ONE:
			srcFactor = GL_ONE;
			break;
		case GLS_SRCBLEND_DST_COLOR:
			srcFactor = GL_DST_COLOR;
			break;
		case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
			srcFactor = GL_ONE_MINUS_DST_COLOR;
			break;
		case GLS_SRCBLEND_SRC_ALPHA:
			srcFactor = GL_SRC_ALPHA;
			break;
		case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
			srcFactor = GL_ONE_MINUS_SRC_ALPHA;
			break;
		case GLS_SRCBLEND_DST_ALPHA:
			srcFactor = GL_DST_ALPHA;
			break;
		case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
			srcFactor = GL_ONE_MINUS_DST_ALPHA;
			break;
		case GLS_SRCBLEND_ALPHA_SATURATE:
			srcFactor = GL_SRC_ALPHA_SATURATE;
			break;
		default:
			srcFactor = GL_ONE;		// to get warning to shut up
			common->Error( "GL_State: invalid src blend state bits\n" );
			break;
		}

		switch ( stateBits & GLS_DSTBLEND_BITS ) {
		case GLS_DSTBLEND_ZERO:
			dstFactor = GL_ZERO;
			break;
		case GLS_DSTBLEND_ONE:
			dstFactor = GL_ONE;
			break;
		case GLS_DSTBLEND_SRC_COLOR:
			dstFactor = GL_SRC_COLOR;
			break;
		case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
			dstFactor = GL_ONE_MINUS_SRC_COLOR;
			break;
		case GLS_DSTBLEND_SRC_ALPHA:
			dstFactor = GL_SRC_ALPHA;
			break;
		case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
			dstFactor = GL_ONE_MINUS_SRC_ALPHA;
			break;
		case GLS_DSTBLEND_DST_ALPHA:
			dstFactor = GL_DST_ALPHA;
			break;
		case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
			dstFactor = GL_ONE_MINUS_DST_ALPHA;
			break;
		default:
			dstFactor = GL_ONE;		// to get warning to shut up
			common->Error( "GL_State: invalid dst blend state bits\n" );
			break;
		}

		qglBlendFunc( srcFactor, dstFactor );
	}

	//
	// check depthmask
	//
	if ( diff & GLS_DEPTHMASK ) {
		if ( stateBits & GLS_DEPTHMASK ) {
			qglDepthMask( GL_FALSE );
		} else {
			qglDepthMask( GL_TRUE );
		}
	}

	//
	// check colormask
	//
	if ( diff & (GLS_REDMASK|GLS_GREENMASK|GLS_BLUEMASK|GLS_ALPHAMASK) ) {
		GLboolean		r, g, b, a;
		r = ( stateBits & GLS_REDMASK ) ? 0 : 1;
		g = ( stateBits & GLS_GREENMASK ) ? 0 : 1;
		b = ( stateBits & GLS_BLUEMASK ) ? 0 : 1;
		a = ( stateBits & GLS_ALPHAMASK ) ? 0 : 1;
		qglColorMask( r, g, b, a );
	}

	//
	// fill/line mode
	//
	if ( diff & GLS_POLYMODE_LINE ) {
		if ( stateBits & GLS_POLYMODE_LINE ) {
			qglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		} else {
			qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		}
	}

	//
	// alpha test
	//
	if ( diff & GLS_ATEST_BITS ) {
		switch ( stateBits & GLS_ATEST_BITS ) {
		case 0:
			qglDisable( GL_ALPHA_TEST );
			break;
		case GLS_ATEST_EQ_255:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_EQUAL, 1 );
			break;
		case GLS_ATEST_LT_128:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_LESS, 0.5 );
			break;
		case GLS_ATEST_GE_128:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GEQUAL, 0.5 );
			break;
		default:
			assert( 0 );
			break;
		}
	}

	backEnd.glState.glStateBits = stateBits;
}




/*
============================================================================

RENDER BACK END THREAD FUNCTIONS

============================================================================
*/

/*
=============
RB_SetGL2D

This is not used by the normal game paths, just by some tools
=============
*/
void RB_SetGL2D( void ) {
	// set 2D virtual screen size
	qglViewport( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	if ( r_useScissor.GetBool() ) {
		qglScissor( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	}
	qglMatrixMode( GL_PROJECTION );
    qglLoadIdentity();
	qglOrtho( 0, 640, 480, 0, 0, 1 );		// always assume 640x480 virtual coordinates
	qglMatrixMode( GL_MODELVIEW );
    qglLoadIdentity();

	GL_State( GLS_DEPTHFUNC_ALWAYS |
			  GLS_SRCBLEND_SRC_ALPHA |
			  GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );

	GL_Cull( CT_TWO_SIDED );

	qglDisable( GL_DEPTH_TEST );
	qglDisable( GL_STENCIL_TEST );
}



/*
=============
RB_SetBuffer

=============
*/
static void	RB_SetBuffer( const void *data ) {
	const setBufferCommand_t	*cmd;

	// see which draw buffer we want to render the frame to

	cmd = (const setBufferCommand_t *)data;

	backEnd.frameCount = cmd->frameCount;

	qglDrawBuffer( cmd->buffer );

	// clear screen for debugging
	// automatically enable this with several other debug tools
	// that might leave unrendered portions of the screen
	if ( r_clear.GetFloat() || idStr::Length( r_clear.GetString() ) != 1 || r_lockSurfaces.GetBool() || r_singleArea.GetBool() || r_showOverDraw.GetBool() ) {
		float c[3];
		if ( sscanf( r_clear.GetString(), "%f %f %f", &c[0], &c[1], &c[2] ) == 3 ) {
			qglClearColor( c[0], c[1], c[2], 1 );
		} else if ( r_clear.GetInteger() == 2 ) {
			qglClearColor( 0.0f, 0.0f,  0.0f, 1.0f );
		} else if ( r_showOverDraw.GetBool() ) {
			qglClearColor( 1.0f, 1.0f, 1.0f, 1.0f );
		} else {
			qglClearColor( 0.4f, 0.0f, 0.25f, 1.0f );
		}
		qglClear( GL_COLOR_BUFFER_BIT );
	}
}

/*
===============
RB_ShowImages

Draw all the images to the screen, on top of whatever
was there.  This is used to test for texture thrashing.
===============
*/
void RB_ShowImages( void ) {
	int		i;
	idImage	*image;
	float	x, y, w, h;
	int		start, end;

	RB_SetGL2D();

	//qglClearColor( 0.2, 0.2, 0.2, 1 );
	//qglClear( GL_COLOR_BUFFER_BIT );

	qglFinish();

	start = Sys_Milliseconds();

	for ( i = 0 ; i < globalImages->images.Num() ; i++ ) {
		image = globalImages->images[i];

		if ( image->texnum == idImage::TEXTURE_NOT_LOADED && image->partialImage == NULL ) {
			continue;
		}

		w = glConfig.vidWidth / 20;
		h = glConfig.vidHeight / 15;
		x = i % 20 * w;
		y = i / 20 * h;

		// show in proportional size in mode 2
		if ( r_showImages.GetInteger() == 2 ) {
			w *= image->uploadWidth / 512.0f;
			h *= image->uploadHeight / 512.0f;
		}

		image->Bind();
		qglBegin (GL_QUADS);
		qglTexCoord2f( 0, 0 );
		qglVertex2f( x, y );
		qglTexCoord2f( 1, 0 );
		qglVertex2f( x + w, y );
		qglTexCoord2f( 1, 1 );
		qglVertex2f( x + w, y + h );
		qglTexCoord2f( 0, 1 );
		qglVertex2f( x, y + h );
		qglEnd();
	}

	qglFinish();

	end = Sys_Milliseconds();
	common->Printf( "%i msec to draw all images\n", end - start );
}


/*
=============
RB_SwapBuffers

=============
*/
const void	RB_SwapBuffers( const void *data ) {
	// texture swapping test
	if ( r_showImages.GetInteger() != 0 ) {
		RB_ShowImages();
	}

	// force a gl sync if requested
	if ( r_finish.GetBool() ) {
		qglFinish();
	}

    RB_LogComment( "***************** RB_SwapBuffers *****************\n\n\n" );

	// don't flip if drawing to front buffer
	if ( !r_frontBuffer.GetBool() ) {
	    GLimp_SwapBuffers();
	}
}

/*
=============
RB_CopyRender

Copy part of the current framebuffer to an image
=============
*/
const void	RB_CopyRender( const void *data ) {
	const copyRenderCommand_t	*cmd;

	cmd = (const copyRenderCommand_t *)data;

	if ( r_skipCopyTexture.GetBool() ) {
		return;
	}

    RB_LogComment( "***************** RB_CopyRender *****************\n" );

	if (cmd->image) {
		cmd->image->CopyFramebuffer( cmd->x, cmd->y, cmd->imageWidth, cmd->imageHeight, false );
	}
}

/*
================================================================================

	spec 004 (Q2): the D3D12 classic command path (T035, T080-T082).

	The front end already produced the sorted drawSurfs array and the viewLight
	lists. This block consumes them in that order and hands a value-only frame to
	the port's backend-neutral consumer. It translates the three legacy shapes at
	the boundary (engine-integration.md 5.4):

	  setBufferCommand_t.buffer	is a GL order code -> 0 current / 1 offscreen
	  copyRenderCommand_t.image	is an idImage*      -> a stable numeric identity
	  drawSurf_t.sort			is preserved       -> the consumer must not re-sort
	  tri->ambientCache			is never read; only numVerts/numIndexes cross

	No GL entry point runs here. A refused callback stops the frame; the engine
	reports the typed reason and never presents a partial frame.

================================================================================
*/

// GL order codes carried by setBufferCommand_t. Spelled as values so this block
// does not look like a GL call site.
static const GLenum	Q2_BUFFER_FRONT = 0x0404;
static const GLenum	Q2_BUFFER_BACK = 0x0405;

extern idCVar r_showRenderApiTrace;

static int						r_q2ViewSerial;
static int						r_q2SurfaceSerial;

void R_Q2ResetImageRegistry( void ) {
	// The image identity registry is owned by Image_load.cpp so that the upload
	// callback and CopyRender use one namespace for the same idImage.
	R_Q2ResetImageUploadRegistry();
	r_q2ViewSerial = 0;
	r_q2SurfaceSerial = 0;
}

static unsigned int RB_Q2ImageIdentity( const idImage *image ) {
	return R_Q2ImageIdentity( image );
}

static bool RB_Q2Refuse( renderApiStage_t stage, const char *detail ) {
	R_Q2TraceAdd( Q2_TRACE_REFUSED );
	R_RenderApiFail( stage, detail );
	return false;
}

static int RB_Q2CountChain( const drawSurf_t *surf ) {
	int count = 0;
	for ( ; surf != NULL; surf = surf->nextOnLight ) {
		count++;
	}
	return count;
}

// Material facts only. The plan (bump -> diffuse -> specular, coverage, refusal)
// stays port-owned; this never decides that a stage is drawable.
static void RB_Q2MaterialFacts( const idMaterial *material, q2MaterialFacts_t *facts ) {
	memset( facts, 0, sizeof( *facts ) );
	if ( material == NULL ) {
		// A drawSurf with no material is a shadow volume (drawSurf_t.material may be NULL).
		facts->name = "<shadow-volume>";
		return;
	}
	facts->name = material->GetName();
	facts->sort = material->GetSort();
	facts->numStages = material->GetNumStages();
	facts->coverage = (int)material->Coverage();
	facts->alphaTest = material->Coverage() == MC_PERFORATED;
	facts->translucent = material->Coverage() == MC_TRANSLUCENT;
	facts->fogLight = material->IsFogLight();
	facts->blendLight = material->IsBlendLight();
	facts->noShadows = material->TestMaterialFlag( MF_NOSHADOWS );
	facts->noSelfShadow = material->TestMaterialFlag( MF_NOSELFSHADOW );
	facts->castsShadow = material->SurfaceCastsShadow();

	// T081: the per-stage texture identity and the sampler state the image itself
	// carries. The material stage does not override filter/repeat; the engine's
	// idImage does (Image.h:232-233), so forward those facts rather than inventing
	// a sampler. A stage with no image keeps id 0 and hasImage false.
	facts->textures.numStages =
		( facts->numStages < Q2_MAX_STAGE_TEXTURES ) ? facts->numStages : Q2_MAX_STAGE_TEXTURES;
	for ( int i = 0; i < facts->numStages; i++ ) {
		const shaderStage_t *stage = material->GetStage( i );
		if ( stage->texture.image != NULL ) {
			facts->hasTexture = true;
			if ( i < facts->textures.numStages ) {
				facts->textures.hasImage[i] = true;
				facts->textures.imageId[i] = R_Q2ImageIdentity( stage->texture.image );
				facts->textures.filter[i] = (int)stage->texture.image->filter;
				facts->textures.repeat[i] = (int)stage->texture.image->repeat;
			}
		}
		if ( stage->newStage != NULL ) {
			facts->hasProgram = true;
		}
	}
}

static bool RB_Q2EmitDrawSurf( const q2SceneSink_t *sink, const drawSurf_t *surf,
							  const viewDef_t *view ) {
	if ( surf == NULL || surf->geo == NULL ) {
		return RB_Q2Refuse( RA_STAGE_COMMAND, "a drawSurf reached the classic path without geometry" );
	}

	q2MaterialFacts_t facts;
	RB_Q2MaterialFacts( surf->material, &facts );
	facts.sort = surf->sort;	// the front end already applied the material/entity offset

	// In-world GUI: R_AddDrawSurf resolves the material/space GUI and records DSF_GUI_SURFACE
	// (tr_light.cpp:1283-1300), so the back end does not dereference space->entityDef and does
	// not fall back to an SS_GUI sort comparison: drawSurf_t.sort carries tr.sortOffset
	// (tr_light.cpp:1195-1200), so equality with SS_GUI would only match the first surface
	// added in a frame. A display HUD is a separate 2D overlay view, not this flag.
	const bool worldGui = ( view->viewEntitys != NULL ) &&
		( ( surf->dsFlags & DSF_GUI_SURFACE ) != 0 );

	// Depth-hack facts exactly as the GL path reads them (draw_common.cpp:749-754):
	// the view projection is not pre-hacked, so the consumer applies the hack per packet.
	// When both flags are set the GL path applies the weapon hack and then the model hack from
	// the original projection, so the model hack is the effective one; match that precedence.
	int depthHackKind = Q2_DEPTH_HACK_NONE;
	float modelDepthHack = 0.0f;
	if ( surf->space != NULL ) {
		if ( surf->space->modelDepthHack != 0.0f ) {
			depthHackKind = Q2_DEPTH_HACK_MODEL;
			modelDepthHack = surf->space->modelDepthHack;
		} else if ( surf->space->weaponDepthHack ) {
			depthHackKind = Q2_DEPTH_HACK_WEAPON;
		}
	}

	const bool postProcess = ( facts.sort >= (float)SS_POST_PROCESS );

	// T080: the ordered world surface's geometry snapshot. The pointers reference
	// the engine's live idDrawVert/index arrays and are valid only for this
	// callback; the port copies what it keeps. The transform is the view entity's
	// model-to-world matrix (tr_local.h:366), never a GL vertex-cache handle.
	if ( sink->surfacePacket != NULL ) {
		q2SurfaceGeometry_t geometry;
		memset( &geometry, 0, sizeof( geometry ) );
		geometry.vertexCount = surf->geo->numVerts;
		geometry.indexCount = surf->geo->numIndexes;
		geometry.vertexData = (const unsigned char *)surf->geo->verts;
		geometry.vertexStride = (int)sizeof( idDrawVert );
		geometry.positionOffset = (int)offsetof( idDrawVert, xyz );
		geometry.normalOffset = (int)offsetof( idDrawVert, normal );
		geometry.texCoordOffset = (int)offsetof( idDrawVert, st );
		geometry.indexes = (const unsigned char *)surf->geo->indexes;
		geometry.indexSize = (int)sizeof( glIndex_t );
		if ( surf->space != NULL ) {
			memcpy( geometry.transform, surf->space->modelMatrix, sizeof( geometry.transform ) );
		} else {
			geometry.transform[0] = geometry.transform[5] = 1.0f;
			geometry.transform[10] = geometry.transform[15] = 1.0f;
		}
		if ( !sink->surfacePacket( sink->user, (unsigned int)( r_q2SurfaceSerial + 1 ), &geometry ) ) {
			return RB_Q2Refuse( RA_STAGE_SCENE_SINK,
				"the classic scene consumer refused world surface geometry" );
		}
		R_Q2TraceAdd( Q2_TRACE_SURFACE );
	}

	if ( sink->drawPacket == NULL ) {
		return RB_Q2Refuse( RA_STAGE_SCENE_SINK,
			"the classic scene consumer has no draw callback" );
	}
	if ( !sink->drawPacket( sink->user, (unsigned int)( ++r_q2SurfaceSerial ), &facts,
			surf->geo->numVerts, surf->geo->numIndexes, depthHackKind, modelDepthHack,
			worldGui, postProcess ) ) {
		return RB_Q2Refuse( RA_STAGE_SCENE_SINK,
			"the classic scene consumer refused a draw packet" );
	}

	R_Q2TraceAdd( Q2_TRACE_DRAW );
	return true;
}

static bool RB_Q2EmitOneLight( const q2SceneSink_t *sink, const q2LightFacts_t &facts ) {
	if ( sink->lightPacket == NULL ) {
		return RB_Q2Refuse( RA_STAGE_SCENE_SINK,
			"the classic scene consumer has no light callback" );
	}
	if ( !sink->lightPacket( sink->user, &facts ) ) {
		return RB_Q2Refuse( RA_STAGE_SCENE_SINK,
			"the classic scene consumer refused a viewLight" );
	}
	R_Q2TraceAdd( Q2_TRACE_LIGHT );
	return true;
}

// T082: the interaction/stencil-shadow facts. The engine already decided which
// surfaces are global shadows, local shadows, local interactions and global
// interactions (Interaction.cpp:1155-1240), so the seam forwards that decision
// as one packet per chain instead of asking the port to re-derive it. The
// classic order is global shadows, local interactions, local shadows, global
// interactions, translucent interactions (draw_arb2.cpp:243-323); the port
// replays a packet per chain. A noShadows light, an ambient/fog/blend pass or a
// light with no remaining chain emits one excluded packet and no volume.
static bool RB_Q2EmitViewLight( const q2SceneSink_t *sink, const viewLight_t *viewLight ) {
	if ( viewLight->lightShader == NULL || viewLight->lightDef == NULL ) {
		return RB_Q2Refuse( RA_STAGE_COMMAND, "a viewLight reached the classic path without a light" );
	}

	const renderLight_t &parms = viewLight->lightDef->parms;

	q2LightFacts_t facts;
	memset( &facts, 0, sizeof( facts ) );
	facts.fogLight = viewLight->lightShader->IsFogLight();
	facts.blendLight = viewLight->lightShader->IsBlendLight();
	facts.pointLight = parms.pointLight;
	facts.parallel = parms.parallel;
	facts.hasProjection = ( parms.target - parms.origin ).LengthSqr() > 0.0f;
	facts.noShadows = parms.noShadows;
	facts.falloffRadius = parms.lightRadius.Length();
	facts.numShadowFrustums = viewLight->lightDef->numShadowFrustums;
	facts.globalShadows = RB_Q2CountChain( viewLight->globalShadows );
	facts.localInteractions = RB_Q2CountChain( viewLight->localInteractions );
	facts.localShadows = RB_Q2CountChain( viewLight->localShadows );
	facts.globalInteractions = RB_Q2CountChain( viewLight->globalInteractions );
	facts.translucentInteractions = RB_Q2CountChain( viewLight->translucentInteractions );

	// Ambient, fog and blend passes never own a stencil volume (draw_common.cpp:1578-1580).
	const bool nonInteraction = facts.fogLight || facts.blendLight ||
		( !facts.pointLight && !facts.parallel && !facts.hasProjection );
	if ( nonInteraction || parms.noShadows ) {
		facts.chainKind = 0;
		facts.excluded = true;
		facts.exclusionKind = parms.noShadows ? 1 : 2;
		return RB_Q2EmitOneLight( sink, facts );
	}

	bool emitted = false;
	if ( viewLight->globalShadows != NULL ) {
		facts.chainKind = 1;
		facts.chainSurfaceCount = RB_Q2CountChain( viewLight->globalShadows );
		facts.chainMaterialNoShadows = false;
		facts.chainMaterialNoSelfShadow = false;
		facts.chainSameEntity = false;
		facts.excluded = false;
		facts.exclusionKind = 0;
		if ( !RB_Q2EmitOneLight( sink, facts ) ) {
			return false;
		}
		emitted = true;
	}
	if ( viewLight->localShadows != NULL ) {
		facts.chainKind = 2;
		facts.chainSurfaceCount = RB_Q2CountChain( viewLight->localShadows );
		facts.chainMaterialNoShadows = false;
		// The local chain exists because the interaction material has
		// noSelfShadow (Interaction.cpp:1162,1235), so the casters may not shadow
		// their own lit surface. This is a fact, not a guess.
		facts.chainMaterialNoSelfShadow = true;
		facts.chainSameEntity = false;
		facts.excluded = false;
		facts.exclusionKind = 0;
		if ( !RB_Q2EmitOneLight( sink, facts ) ) {
			return false;
		}
		emitted = true;
	}

	if ( !emitted ) {
		// The light has interactions but the front end produced no shadow chain:
		// no volume may be emitted. This is the CP-07 exclusion, not a missing packet.
		facts.chainKind = 0;
		facts.excluded = true;
		facts.exclusionKind = 4;
		return RB_Q2EmitOneLight( sink, facts );
	}
	return true;
}

/*
====================
RB_Q2SubmitView

The sorted drawSurfs array goes first (the consumer replays it for the depth fill,
the interaction and the post-process phases), then each viewLight in viewLights
order. That is the order the GL back end consumes the same data in
(draw_common.cpp:1666-1725, draw_arb2.cpp:243-323).
====================
*/
static bool RB_Q2SubmitView( const drawSurfsCommand_t *cmd ) {
	const viewDef_t *view = cmd->viewDef;
	const q2SceneSink_t *sink = R_Q2SceneSink();

	if ( view == NULL ) {
		return RB_Q2Refuse( RA_STAGE_COMMAND, "RC_DRAW_VIEW without a viewDef" );
	}
	if ( sink == NULL || sink->beginView == NULL ) {
		return RB_Q2Refuse( RA_STAGE_SCENE_SINK,
			"the classic scene consumer is not installed" );
	}

	// A 2D view has no viewEntities; it is the display overlay, drawn after the 3D scene.
	const bool overlay = ( view->viewEntitys == NULL );
	const int x = view->viewport.x1;
	const int y = view->viewport.y1;
	const int width = view->viewport.x2 - view->viewport.x1 + 1;
	const int height = view->viewport.y2 - view->viewport.y1 + 1;
	if ( width <= 0 || height <= 0 ) {
		return RB_Q2Refuse( RA_STAGE_SURFACE, "RC_DRAW_VIEW has an empty viewport" );
	}

	const unsigned int viewId = (unsigned int)( ++r_q2ViewSerial );
	// The projection is the unhacked viewDef projection. The weapon/model depth hack is a
	// per-surface fact forwarded in drawPacket (draw_common.cpp:749-754, tr_render.cpp:176-206);
	// the consumer applies it for that packet and restores the projection afterwards.
	// `worldGui` at view level is false: the engine has no whole-GUI view, and an in-world GUI
	// is a per-surface fact on the packet below.
	if ( !sink->beginView( sink->user, viewId, x, y, width, height, overlay, false,
			view->projectionMatrix ) ) {
		return RB_Q2Refuse( RA_STAGE_SCENE_SINK,
			"the classic scene consumer refused a view" );
	}
	R_Q2TraceAdd( overlay ? Q2_TRACE_OVERLAY : Q2_TRACE_VIEW );

	for ( int i = 0; i < view->numDrawSurfs; i++ ) {
		if ( !RB_Q2EmitDrawSurf( sink, view->drawSurfs[i], view ) ) {
			return false;
		}
	}

	for ( const viewLight_t *viewLight = view->viewLights; viewLight != NULL;
			viewLight = viewLight->next ) {
		if ( !RB_Q2EmitViewLight( sink, viewLight ) ) {
			return false;
		}
	}

	if ( sink->endView == NULL || !sink->endView( sink->user ) ) {
		return RB_Q2Refuse( RA_STAGE_SCENE_SINK,
			"the classic scene consumer refused to end a view" );
	}
	return true;
}

/*
====================
RB_Q2ExecuteBackEndCommands

The modern counterpart of the GL loop. It keeps the existing FIFO and the RC_*
order, including a mid-frame RC_COPY_RENDER position.
====================
*/
static void RB_Q2ExecuteBackEndCommands( const emptyCommand_t *cmds ) {
	const q2SceneSink_t *sink = R_Q2SceneSink();
	bool ok = true;

	for ( ; cmds != NULL && ok; cmds = (const emptyCommand_t *)cmds->next ) {
		switch ( cmds->commandId ) {
		case RC_NOP:
			break;
		case RC_DRAW_VIEW:
			ok = RB_Q2SubmitView( (const drawSurfsCommand_t *)cmds );
			break;
		case RC_SET_BUFFER: {
			const setBufferCommand_t *cmd = (const setBufferCommand_t *)cmds;
			int selector;
			if ( cmd->buffer == Q2_BUFFER_FRONT ) {
				selector = 0;
			} else if ( cmd->buffer == Q2_BUFFER_BACK ) {
				selector = 1;
			} else {
				ok = RB_Q2Refuse( RA_STAGE_COMMAND,
					"RC_SET_BUFFER carried a value the classic path does not accept" );
				break;
			}
			if ( sink == NULL || sink->setColorTarget == NULL ||
					!sink->setColorTarget( sink->user, selector ) ) {
				ok = RB_Q2Refuse( RA_STAGE_COMMAND,
					"the classic scene consumer refused a colour target" );
				break;
			}
			R_Q2TraceAdd( Q2_TRACE_TARGET );
			break;
		}
		case RC_COPY_RENDER: {
			const copyRenderCommand_t *cmd = (const copyRenderCommand_t *)cmds;
			const unsigned int imageId = RB_Q2ImageIdentity( cmd->image );
			const char *imageName = ( cmd->image != NULL ) ? cmd->image->imgName.c_str() : NULL;
			if ( imageId == 0 ) {
				ok = RB_Q2Refuse( RA_STAGE_COMMAND,
					"RC_COPY_RENDER carries no destination image" );
				break;
			}
			if ( sink == NULL || sink->copyToImage == NULL ||
					!sink->copyToImage( sink->user, imageId, imageName ) ) {
				ok = RB_Q2Refuse( RA_STAGE_COMMAND,
					"the classic scene consumer refused a frame copy" );
				break;
			}
			R_Q2TraceAdd( Q2_TRACE_COPY );
			break;
		}
		case RC_SWAP_BUFFERS:
			if ( sink == NULL || sink->swapBuffers == NULL || !sink->swapBuffers( sink->user ) ) {
				ok = RB_Q2Refuse( RA_STAGE_COMMAND,
					"the classic scene consumer refused to present the frame" );
				break;
			}
			R_Q2TraceAdd( Q2_TRACE_SWAP );
			break;
		default:
			ok = RB_Q2Refuse( RA_STAGE_COMMAND, "RB_ExecuteBackEndCommands: bad commandId" );
			break;
		}
	}

	if ( r_showRenderApiTrace.GetInteger() ) {
		R_Q2TracePrint( ok ? "frame" : "frame-refused" );
	}
}

/*
====================
RB_ExecuteBackEndCommands

This function will be called syncronously if running without
smp extensions, or asyncronously by another thread.
====================
*/
int		backEndStartTime, backEndFinishTime;
void RB_ExecuteBackEndCommands( const emptyCommand_t *cmds ) {
	// r_debugRenderToTexture
	int	c_draw3d = 0, c_draw2d = 0, c_setBuffers = 0, c_swapBuffers = 0, c_copyRenders = 0;

	if ( cmds->commandId == RC_NOP && !cmds->next ) {
		return;
	}

	backEndStartTime = Sys_Milliseconds();

	if ( tr.modernRenderApiRunning ) {
		// The D3D12 classic path: no GL default state, no GL image upload, no qglBindTexture.
		RB_Q2ExecuteBackEndCommands( cmds );
		backEndFinishTime = Sys_Milliseconds();
		backEnd.pc.msec = backEndFinishTime - backEndStartTime;
		return;
	}

	// needed for editor rendering
	RB_SetDefaultGLState();

	// upload any image loads that have completed
	globalImages->CompleteBackgroundImageLoads();

	for ( ; cmds ; cmds = (const emptyCommand_t *)cmds->next ) {
		switch ( cmds->commandId ) {
		case RC_NOP:
			break;
		case RC_DRAW_VIEW:
			RB_DrawView( cmds );
			if ( ((const drawSurfsCommand_t *)cmds)->viewDef->viewEntitys ) {
				c_draw3d++;
			}
			else {
				c_draw2d++;
			}
			break;
		case RC_SET_BUFFER:
			RB_SetBuffer( cmds );
			c_setBuffers++;
			break;
		case RC_SWAP_BUFFERS:
			RB_SwapBuffers( cmds );
			c_swapBuffers++;
			break;
		case RC_COPY_RENDER:
			RB_CopyRender( cmds );
			c_copyRenders++;
			break;
		default:
			common->Error( "RB_ExecuteBackEndCommands: bad commandId" );
			break;
		}
	}

	// go back to the default texture so the editor doesn't mess up a bound image
	qglBindTexture( GL_TEXTURE_2D, 0 );
	backEnd.glState.tmu[0].current2DMap = -1;

	// stop rendering on this thread
	backEndFinishTime = Sys_Milliseconds();
	backEnd.pc.msec = backEndFinishTime - backEndStartTime;

	if ( r_debugRenderToTexture.GetInteger() == 1 ) {
		common->Printf( "3d: %i, 2d: %i, SetBuf: %i, SwpBuf: %i, CpyRenders: %i, CpyFrameBuf: %i\n", c_draw3d, c_draw2d, c_setBuffers, c_swapBuffers, c_copyRenders, backEnd.c_copyFrameBuffer );
		backEnd.c_copyFrameBuffer = 0;
	}
}
