/*************************************************************************
 * Copyright (c) 2011 AT&T Intellectual Property 
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors: Details at https://graphviz.org
 *************************************************************************/

/*

XDOT DRAWING FUNCTIONS, maybe need to move them somewhere else
		for now keep them at the bottom
*/
#include "draw.h"
#include <cgraph/unreachable.h>
#include <common/colorprocs.h>
#include <common/types.h>
#include <common/utils.h>
#include "smyrna_utils.h"
#include <glcomp/glutils.h>
#include <math.h>
#include <stdlib.h>

#include <xdot/xdot.h>
#include "viewport.h"
#include "topfisheyeview.h"
#include "appmouse.h"
#include "hotkeymap.h"
#include "polytess.h"
#include <glcomp/glcompimage.h>


//delta values
static float dx = 0.0;
static float dy = 0.0;
#define LAYER_DIFF 0.001

static void DrawBezier(xdot_point* pts, int filled, int param)
{
    /*copied from NEHE */
    /*Written by: David Nikdel ( ogapo@ithink.net ) */
    double Ax = pts[0].x;
    double Ay = pts[0].y;
    double Az = pts[0].z;
    double Bx = pts[1].x;
    double By = pts[1].y;
    double Bz = pts[1].z;
    double Cx = pts[2].x;
    double Cy = pts[2].y;
    double Cz = pts[2].z;
    double Dx = pts[3].x;
    double Dy = pts[3].y;
    double Dz = pts[3].z;
    double X;
    double Y;
    double Z;
    int i = 0;			//loop index
    // Variable
    double a = 1.0;
    double b = 1.0 - a;
    /* Tell OGL to start drawing a line strip */
    glLineWidth(view->LineWidth);
    if (!filled) {

	if (param == 0)
	    glColor4f(view->penColor.R, view->penColor.G, view->penColor.B,
		      view->penColor.A);
	else if (param == 1)		//selected
	    glColor4f(view->selectedNodeColor.R, view->selectedNodeColor.G,
		      view->selectedNodeColor.B,
		      view->selectedNodeColor.A);
	glBegin(GL_LINE_STRIP);
    } else {
	if (param == 0)
	    glColor4f(view->fillColor.R, view->fillColor.G,
		      view->fillColor.B, view->penColor.A);
	else if (param == 1)		//selected
	    glColor4f(view->selectedNodeColor.R, view->selectedNodeColor.G,
		      view->selectedNodeColor.B,
		      view->selectedNodeColor.A);
	glBegin(GL_POLYGON);
    }
    /* We will not actually draw a curve, but we will divide the curve into small
       points and draw a line between each point. If the points are close enough, it
       will appear as a curved line. 20 points are plenty, and since the variable goes
       from 1.0 to 0.0 we must change it by 1/20 = 0.05 each time */
    for (i = 0; i <= 20; i++) {
	// Get a point on the curve
	X = Ax * a * a * a + Bx * 3 * a * a * b + Cx * 3 * a * b * b +
	    Dx * b * b * b;
	Y = Ay * a * a * a + By * 3 * a * a * b + Cy * 3 * a * b * b +
	    Dy * b * b * b;
	Z = Az * a * a * a + Bz * 3 * a * a * b + Cz * 3 * a * b * b +
	    Dz * b * b * b;
	// Draw the line from point to point (assuming OGL is set up properly)
	glVertex3d(X, Y, Z + view->Topview->global_z);
	// Change the variable
	a -= 0.05;
	b = 1.0 - a;
    }
// Tell OGL to stop drawing the line strip
    glEnd();
}

static void set_options(int param)
{

    int a=get_mode(view);
    if ((param == 1) && (a == 10) && (view->mouse.down == 1))	//selected, if there is move, move it
    {
	dx = view->mouse.GLinitPos.x-view->mouse.GLfinalPos.x;
	dy = view->mouse.GLinitPos.y-view->mouse.GLfinalPos.y;
    } else {
	dx = 0;
	dy = 0;
    }

}

static void DrawBeziers(sdot_op* o, int param)
{
    int filled;
    xdot_op *  op=&o->op;
    xdot_point* ps = op->u.bezier.pts;
    view->Topview->global_z += o->layer * LAYER_DIFF;

    if (op->kind == xd_filled_bezier)
	filled = 1;
    else
	filled = 0;

    for (size_t i = 1; i < op->u.bezier.cnt; i += 3) {
	DrawBezier(ps, filled, param);
	ps += 3;
    }
}

//Draws an ellpise made out of points.
//void DrawEllipse(xdot_point* xpoint,GLfloat xradius, GLfloat yradius,int filled)
static void DrawEllipse(sdot_op*  o, int param)
{
    int i = 0;
    int filled;
    xdot_op * op=&o->op;
    view->Topview->global_z += o->layer * LAYER_DIFF;
    set_options(param);
    double x = op->u.ellipse.x - dx;
    double y = op->u.ellipse.y - dy;
    double xradius = op->u.ellipse.w;
    double yradius = op->u.ellipse.h;
    if (op->kind == xd_filled_ellipse) {
	if (param == 0)
	    glColor4f(view->fillColor.R, view->fillColor.G,
		      view->fillColor.B, view->fillColor.A);
	if (param == 1)		//selected
	    glColor4f(view->selectedNodeColor.R, view->selectedNodeColor.G,
		      view->selectedNodeColor.B,
		      view->selectedNodeColor.A);

	filled = 1;
    } else {
	if (param == 0)
	    glColor4f(view->penColor.R, view->penColor.G, view->penColor.B,
		      view->penColor.A);
	if (param == 1)		//selected
	    glColor4f(view->selectedNodeColor.R, view->selectedNodeColor.G,
		      view->selectedNodeColor.B,
		      view->selectedNodeColor.A);

	filled = 0;
    }

    if (!filled)
	glBegin(GL_LINE_LOOP);
    else
	glBegin(GL_POLYGON);
    for (i = 0; i < 360; i = i + 1) {
	//convert degrees into radians
	float degInRad = (float) (i * DEG2RAD);
	glVertex3f((GLfloat) (x + cos(degInRad) * xradius),
		   (GLfloat) (y + sin(degInRad) * yradius), (GLfloat)view->Topview->global_z);
    }
    glEnd();
}

static void DrawPolygon(sdot_op * o, int param)
{
    xdot_op *  op=&o->op;
    view->Topview->global_z += o->layer * LAYER_DIFF;

    set_options(param);

    if (op->kind == xd_filled_polygon) {
	if (param == 0)
	    glColor4f(view->fillColor.R, view->fillColor.G,
		      view->fillColor.B, view->fillColor.A);
	if (param == 1)		//selected
	    glColor4f(view->selectedNodeColor.R, view->selectedNodeColor.G,
		      view->selectedNodeColor.B,
		      view->selectedNodeColor.A);
    } else {
	if (param == 0)
	    glColor4f(view->penColor.R, view->penColor.G, view->penColor.B,
		      view->penColor.A);
	if (param == 1)		//selected
	    glColor4f(view->selectedNodeColor.R, view->selectedNodeColor.G,
		      view->selectedNodeColor.B,
		      view->selectedNodeColor.A);

    }
    glLineWidth(view->LineWidth);
    drawTessPolygon(o);
}


static void DrawPolyline(sdot_op* o, int param)
{
    xdot_op * op=&o->op;
    view->Topview->global_z += o->layer * LAYER_DIFF;

    if (param == 0)
	glColor4f(view->penColor.R, view->penColor.G, view->penColor.B,
		  view->penColor.A);
    if (param == 1)		//selected
	glColor4f(view->selectedNodeColor.R, view->selectedNodeColor.G,
		  view->selectedNodeColor.B, view->selectedNodeColor.A);
    set_options(param);
    glLineWidth(view->LineWidth);
    glBegin(GL_LINE_STRIP);
    for (size_t i = 0; i < op->u.polyline.cnt; ++i) {
	glVertex3f((GLfloat) op->u.polyline.pts[i].x - dx,
		   (GLfloat) op->u.polyline.pts[i].y - dy,
		   (GLfloat)(op->u.polyline.pts[i].z + view->Topview->global_z));
    }
    glEnd();
}

static glCompColor GetglCompColor(char *color)
{
    gvcolor_t cl;
    glCompColor c;
    if (color != NULL) {
	colorxlate(color, &cl, RGBA_DOUBLE);
	c.R = (float) cl.u.RGBA[0];
	c.G = (float) cl.u.RGBA[1];
	c.B = (float) cl.u.RGBA[2];
	c.A = (float) cl.u.RGBA[3];
    } else {
	c.R = view->penColor.R;
	c.G = view->penColor.G;
	c.B = view->penColor.B;
	c.A = view->penColor.A;
    }
    return c;
}
static void SetFillColor(sdot_op*  o, int param)
{
    (void)param;

    xdot_op * op=&o->op;
    glCompColor c = GetglCompColor(op->u.color);
    view->fillColor.R = c.R;
    view->fillColor.G = c.G;
    view->fillColor.B = c.B;
    view->fillColor.A = c.A;
}
static void SetPenColor(sdot_op* o, int param)
{
    (void)param;

    glCompColor c;
    xdot_op * op=&o->op;
    c = GetglCompColor(op->u.color);
    view->penColor.R = c.R;
    view->penColor.G = c.G;
    view->penColor.B = c.B;
    view->penColor.A = c.A;
}

static void SetStyle(sdot_op* o, int param)
{
    (void)o;
    (void)param;
}

static sdot_op * font_op;

static void SetFont(sdot_op * o, int param)
{
	(void)param;

	font_op=o;
}

/*for now we only support png files in 2d space, no image rotation*/
static void InsertImage(sdot_op * o, int param)
{
    (void)param;

    float x,y;
    glCompImage *i;

    if(!o->obj)
	return;


    if(!o->img) {
	x = o->op.u.image.pos.x;
	y = o->op.u.image.pos.y;
	i = o->img = glCompImageNewFile(x, y, o->op.u.image.name);
	if (!o->img) {
	    fprintf (stderr, "Could not open file \"%s\" to read image.\n", o->op.u.image.name);
	    return;
	}
	i->width = o->op.u.image.pos.w;
	i->height = o->op.u.image.pos.h;
	i->common.functions.draw(i);
    }
}

// see usage in EmbedText
static int put(void *buffer, const char *s) {
  char **b = buffer;

  for (; *s != '\0'; ++s) {
    **b = *s;
    ++(*b);
  }

  return 0;
}

static void EmbedText(sdot_op* o, int param)
{
	(void)param;

	GLfloat x,y;
	glColor4f(view->penColor.R,view->penColor.G,view->penColor.B,view->penColor.A);
	view->Topview->global_z += o->layer * LAYER_DIFF + 0.05;
	switch (o->op.u.text.align)
	{
		case xd_left:
			x=o->op.u.text.x ;
			break;
		case xd_center:
			x=o->op.u.text.x - o->op.u.text.width / 2.0;
			break;
		case xd_right:
			x=o->op.u.text.x - o->op.u.text.width;
			break;
		default:
			UNREACHABLE();
	}
	y=o->op.u.text.y;
	if (!o->font)
	{
		// allocate a buffer large enough to hold the maximum escaped version of the
		// text
		char *escaped = calloc(sizeof(char), strlen(o->op.u.text.text) *
		                                     sizeof("&#xFFFFFFFF;") + 1);
		if (escaped == NULL)
		  return;

		// XML-escape the text
		const xml_flags_t flags = {.dash = 1, .nbsp = 1};
		char **ptr = &escaped;
		(void)xml_escape(o->op.u.text.text, flags, put, ptr);

		o->font = glNewFont(view->widgets, escaped, &view->penColor,
		                    font_op->op.u.font.name, font_op->op.u.font.size, 0);

		free(escaped);
	}
	glCompDrawText3D(o->font,x,y,view->Topview->global_z,o->op.u.text.width,font_op->op.u.font.size);

}

void drawBorders(ViewInfo * vi)
{
    if (vi->bdVisible) {
	glColor4f(vi->borderColor.R, vi->borderColor.G,
		  vi->borderColor.B, vi->borderColor.A);
	glLineWidth(2);
	glBegin(GL_LINE_STRIP);
	glVertex3d(vi->bdxLeft, vi->bdyBottom,-0.001);
	glVertex3d(vi->bdxRight, vi->bdyBottom,-0.001);
	glVertex3d(vi->bdxRight, vi->bdyTop,-0.001);
	glVertex3d(vi->bdxLeft, vi->bdyTop,-0.001);
	glVertex3d(vi->bdxLeft, vi->bdyBottom,-0.001);
	glEnd();
	glLineWidth(1);
    }
}

void drawCircle(float x, float y, float radius, float zdepth)
{
    int i;
    if (radius < 0.3)
	radius = 0.4f;
    glBegin(GL_POLYGON);
    for (i = 0; i < 360; i = i + 36) {
	float degInRad = (float) (i * DEG2RAD);
	glVertex3f((GLfloat) (x + cos(degInRad) * radius),
		   (GLfloat) (y + sin(degInRad) * radius),
		   (GLfloat)(zdepth + view->Topview->global_z));
    }

    glEnd();
}

drawfunc_t OpFns[] = {
    (drawfunc_t)DrawEllipse,
    (drawfunc_t)DrawPolygon,
    (drawfunc_t)DrawBeziers,
    (drawfunc_t)DrawPolyline,
    (drawfunc_t)EmbedText,
    (drawfunc_t)SetFillColor,
    (drawfunc_t)SetPenColor,
    (drawfunc_t)SetFont,
    (drawfunc_t)SetStyle,
    (drawfunc_t)InsertImage,
};

void draw_selpoly(glCompPoly* selPoly)
{
    int i;
    glColor4f(1,0,0,1);
    glBegin(GL_LINE_STRIP);
    for (i = 0;i <  selPoly->cnt ; i++)
    {
	glVertex3f(selPoly->pts[i].x,selPoly->pts[i].y,selPoly->pts[i].z);
    }
    glEnd();
    if(selPoly->cnt >0)
    {
        glBegin(GL_LINE_STRIP);
	glVertex3f(selPoly->pts[selPoly->cnt-1].x,selPoly->pts[selPoly->cnt-1].y,selPoly->pts[selPoly->cnt-1].z);
	glVertex3f(view->mouse.GLpos.x,view->mouse.GLpos.y,0);
	glEnd();
    }
}
