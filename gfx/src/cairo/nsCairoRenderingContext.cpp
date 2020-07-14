/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@pavlov.net>
 *   Joe Hewitt <hewitt@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCairoRenderingContext.h"
#include "nsCairoDeviceContext.h"
#include "nsCairoSurfaceManager.h"

#include "nsRect.h"
#include "nsString.h"
#include "nsTransform2D.h"
#include "nsIRegion.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsGfxCIID.h"

#include "imgIContainer.h"
#include "gfxIImageFrame.h"
#include "nsIImage.h"

#include "nsCairoFontMetrics.h"
#include "nsCairoDrawingSurface.h"
#include "nsCairoRegion.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static NS_DEFINE_CID(kRegionCID, NS_REGION_CID);


//////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS1(nsCairoRenderingContext, nsIRenderingContext)

nsCairoRenderingContext::nsCairoRenderingContext() :
    mLineStyle(nsLineStyle_kNone),
    mCairo(nsnull)
{
}

nsCairoRenderingContext::~nsCairoRenderingContext()
{
    if (mCairo)
        cairo_destroy(mCairo);
}

//////////////////////////////////////////////////////////////////////
//// nsIRenderingContext

NS_IMETHODIMP
nsCairoRenderingContext::Init(nsIDeviceContext* aContext, nsIWidget *aWidget)
{
    nsCairoDeviceContext *cairoDC = NS_STATIC_CAST(nsCairoDeviceContext*, aContext);

    mDeviceContext = aContext;
    mWidget = aWidget;

    mDrawingSurface = new nsCairoDrawingSurface();
    nsNativeWidget nativeWidget = aWidget->GetNativeData(NS_NATIVE_WIDGET);
    mDrawingSurface->Init (cairoDC, nativeWidget);

    mCairo = cairo_create ();
    cairo_set_target_surface (mCairo, mDrawingSurface->GetCairoSurface());

    return (CommonInit());
}

NS_IMETHODIMP
nsCairoRenderingContext::Init(nsIDeviceContext* aContext, nsIDrawingSurface *aSurface)
{
    nsCairoDrawingSurface *cds = (nsCairoDrawingSurface *) aSurface;

    mDeviceContext = aContext;
    mWidget = nsnull;

    mDrawingSurface = (nsCairoDrawingSurface *) cds;

    mCairo = cairo_create ();
    cairo_set_target_surface (mCairo, mDrawingSurface->GetCairoSurface());

    return (CommonInit());
}

NS_IMETHODIMP
nsCairoRenderingContext::CommonInit(void)
{
    mClipRegion = new nsCairoRegion();

    float app2dev;
    app2dev = mDeviceContext->AppUnitsToDevUnits();
    Scale(app2dev, app2dev);

    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::Reset(void)
{
    cairo_surface_t *surf = cairo_current_target_surface (mCairo);
    cairo_t *cairo = cairo_create ();
    cairo_set_target_surface (cairo, surf);
    cairo_destroy (mCairo);
    mCairo = cairo;

    mClipRegion = new nsCairoRegion();

    return (CommonInit());
}

NS_IMETHODIMP
nsCairoRenderingContext::GetDeviceContext(nsIDeviceContext *& aDeviceContext)
{
    aDeviceContext = mDeviceContext;
    NS_IF_ADDREF(aDeviceContext);
    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::LockDrawingSurface(PRInt32 aX, PRInt32 aY,
                                            PRUint32 aWidth, PRUint32 aHeight,
                                            void **aBits, PRInt32 *aStride,
                                            PRInt32 *aWidthBytes,
                                            PRUint32 aFlags)
{
    if (mOffscreenSurface)
        return mOffscreenSurface->Lock(aX, aY, aWidth, aHeight, aBits, aStride, aWidthBytes, aFlags);
    else
        return mDrawingSurface->Lock(aX, aY, aWidth, aHeight, aBits, aStride, aWidthBytes, aFlags);

    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::UnlockDrawingSurface(void)
{
    if (mOffscreenSurface)
        return mOffscreenSurface->Unlock();
    else
        return mDrawingSurface->Unlock();

    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::SelectOffScreenDrawingSurface(nsIDrawingSurface *aSurface)
{
    NS_WARNING("not implemented, because I don't know how to unselect the offscreen surface");
    return NS_ERROR_FAILURE;

    mOffscreenSurface = NS_STATIC_CAST(nsCairoDrawingSurface *, aSurface);
    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::GetDrawingSurface(nsIDrawingSurface **aSurface)
{
    if (mOffscreenSurface)
        *aSurface = mOffscreenSurface;
    else if (mDrawingSurface)
        *aSurface = mDrawingSurface;

    NS_IF_ADDREF (*aSurface);
    
    return NS_OK;
}

NS_IMETHODIMP 
nsCairoRenderingContext::PushTranslation(PushedTranslation* aState)
{
    // XXX this is slow!
    PushState();
    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::PopTranslation(PushedTranslation* aState)
{
    // XXX this is slow!
    PopState();
    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::GetHints(PRUint32& aResult)
{
    aResult = 0;

    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::PushState()
{
    cairo_save (mCairo);
    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::PopState()
{
    cairo_restore (mCairo);
    return NS_OK;
}

//
// clipping
//

NS_IMETHODIMP
nsCairoRenderingContext::IsVisibleRect(const nsRect& aRect, PRBool &aIsVisible)
{
    // XXX
    aIsVisible = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::GetClipRect(nsRect &aRect, PRBool &aHasLocalClip)
{
    NS_ERROR("not used anywhere");
    return NS_OK;
}

void
nsCairoRenderingContext::DoCairoClip()
{
    nsRegionComplexity cplx;
    mClipRegion->GetRegionComplexity(cplx);

    cairo_init_clip (mCairo);

    float pixToTwip = mDeviceContext->DevUnitsToAppUnits();

    if (cplx == eRegionComplexity_rect) {
        PRInt32 x, y, w, h;
        mClipRegion->GetBoundingBox(&x, &y, &w, &h);
        cairo_new_path (mCairo);
        cairo_rectangle (mCairo,
                         double(x * pixToTwip),
                         double(y * pixToTwip),
                         double(w * pixToTwip),
                         double(h * pixToTwip));
        cairo_clip (mCairo);
    } else if (cplx == eRegionComplexity_complex) {
        nsRegionRectSet *rects = nsnull;
        nsresult rv = mClipRegion->GetRects (&rects);
        if (NS_FAILED(rv) || !rects)
            return;

        cairo_new_path (mCairo);
        for (PRUint32 i = 0; i < rects->mRectsLen; i++) {
            cairo_rectangle (mCairo,
                             double (rects->mRects[i].x * pixToTwip),
                             double (rects->mRects[i].y * pixToTwip),
                             double (rects->mRects[i].width * pixToTwip),
                             double (rects->mRects[i].height * pixToTwip));
        }

        cairo_clip (mCairo);

        mClipRegion->FreeRects (rects);
    }
}

NS_IMETHODIMP
nsCairoRenderingContext::SetClipRect(const nsRect& aRect, nsClipCombine aCombine)
{
    // transform rect by current transform matrix and clip
    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::SetClipRegion(const nsIRegion& aRegion,
                                       nsClipCombine aCombine)
{
    // region is in device coords, no transformation!
    // how do we do that with cairo?

    switch(aCombine)
    {
    case nsClipCombine_kIntersect:
        mClipRegion->Intersect (aRegion);
        break;
    case nsClipCombine_kReplace:
        mClipRegion->SetTo (aRegion);
        break;
    case nsClipCombine_kUnion:
    case nsClipCombine_kSubtract:
        // these two are never used in mozilla
    default:
        NS_WARNING("aCombine type passed that should never be used\n");
        break;
    }

    DoCairoClip();

    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::CopyClipRegion(nsIRegion &aRegion)
{
    aRegion.SetTo(*mClipRegion);
    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::GetClipRegion(nsIRegion **aRegion)
{
    *aRegion = new nsCairoRegion();
    (*aRegion)->SetTo(*mClipRegion);
    NS_ADDREF(*aRegion);
    return NS_OK;
}

// only gets called from the caret blinker.
NS_IMETHODIMP
nsCairoRenderingContext::FlushRect(const nsRect& aRect)
{
    return FlushRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP
nsCairoRenderingContext::FlushRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
    return NS_OK;
}

//
// other junk
//

NS_IMETHODIMP
nsCairoRenderingContext::SetLineStyle(nsLineStyle aLineStyle)
{
    static double dash[] = {5.0, 5.0};
    static double dot[] = {1.0, 1.0};

    switch (aLineStyle) {
        case nsLineStyle_kNone:
            // XX what is this supposed to be? invisible line?
            break;
        case nsLineStyle_kSolid:
            cairo_set_dash (mCairo, nsnull, 0, 0);
            break;
        case nsLineStyle_kDashed:
            cairo_set_dash (mCairo, dash, 2, 0);
            break;
        case nsLineStyle_kDotted:
            cairo_set_dash (mCairo, dot, 2, 0);
            break;
        default:
            NS_ERROR("SetLineStyle: Invalid line style");
            break;
    }

    mLineStyle = aLineStyle;
    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::GetLineStyle(nsLineStyle &aLineStyle)
{
    NS_ERROR("not used anywhere");
    aLineStyle = mLineStyle;
    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::SetPenMode(nsPenMode aPenMode)
{
    // aPenMode == nsPenMode_kNone, nsPenMode_kInverted.
    NS_ERROR("not used anywhere");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCairoRenderingContext::GetPenMode(nsPenMode &aPenMode)
{
    //NS_ERROR("not used anywhere, but used in a debug thing");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCairoRenderingContext::SetColor(nscolor aColor)
{
    mColor = aColor;

    PRUint8 r = NS_GET_R(aColor);
    PRUint8 g = NS_GET_G(aColor);
    PRUint8 b = NS_GET_B(aColor);
    PRUint8 a = NS_GET_A(aColor);

    cairo_set_rgb_color (mCairo,
                         (double) r / 255.0,
                         (double) g / 255.0,
                         (double) b / 255.0);
    cairo_set_alpha (mCairo, (double) a / 255.0);

    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::GetColor(nscolor &aColor) const
{
    aColor = mColor;
    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::Translate(nscoord aX, nscoord aY)
{
//    fprintf (stderr, "++ Xlate: %g %g\n", (double)aX, (double)aY);
    cairo_translate (mCairo, (double) aX, (double) aY);
    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::Scale(float aSx, float aSy)
{
//    fprintf (stderr, "++ Scale: %g %g\n", (double)aSx, (double)aSy);
    cairo_scale (mCairo, (double) aSx, (double) aSy);
    return NS_OK;
}

void
nsCairoRenderingContext::UpdateTempTransformMatrix()
{
    double a, b, c, d, tx, ty;
    cairo_matrix_t *mat = cairo_matrix_create();
    cairo_current_matrix (mCairo, mat);
    cairo_matrix_get_affine (mat, &a, &b, &c, &d, &tx, &ty);
    /*****
     * Cairo matrix layout:   gfx matrix layout:
     * | a  b  0 |            | m00 m01  0 |
     * | c  d  0 |            | m10 m11  0 |
     * | tx ty 1 |            | m20 m21  1 |
     *****/

    cairo_matrix_destroy (mat);

    NS_ASSERTION(b == 0 && c == 0, "Can't represent Cairo matrix to Gfx");
    mTempTransform.SetToTranslate(tx, ty);
    mTempTransform.AddScale(a, d);
}

nsTransform2D&
nsCairoRenderingContext::CurrentTransform()
{
    UpdateTempTransformMatrix();
    return mTempTransform;
}

NS_IMETHODIMP
nsCairoRenderingContext::GetCurrentTransform(nsTransform2D *&aTransform)
{
    UpdateTempTransformMatrix();
    aTransform = &mTempTransform;
    return NS_OK;
}

void
nsCairoRenderingContext::TransformCoord (nscoord *aX, nscoord *aY)
{
    double x = double(*aX);
    double y = double(*aY);
    cairo_transform_point (mCairo, &x, &y);
    *aX = nscoord(x);
    *aY = nscoord(y);
}

NS_IMETHODIMP
nsCairoRenderingContext::CreateDrawingSurface(const nsRect &aBounds,
                                              PRUint32 aSurfFlags,
                                              nsIDrawingSurface* &aSurface)
{
    nsCairoDrawingSurface *cds = new nsCairoDrawingSurface();
    nsIDeviceContext *dc = mDeviceContext.get();
    cds->Init (NS_STATIC_CAST(nsCairoDeviceContext *, dc),
               aBounds.width, aBounds.height,
               PR_FALSE);
    aSurface = (nsIDrawingSurface*) cds;
    NS_ADDREF(aSurface);

    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::DestroyDrawingSurface(nsIDrawingSurface *aDS)
{
    NS_RELEASE(aDS);

    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
    // XXX are these twips?
    cairo_new_path (mCairo);
    cairo_move_to (mCairo, (double) aX0, (double) aY0);
    cairo_line_to (mCairo, (double) aX1, (double) aY1);
    cairo_stroke (mCairo);

    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::DrawRect(const nsRect& aRect)
{
    return DrawRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP
nsCairoRenderingContext::DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
    cairo_new_path (mCairo);
    cairo_rectangle (mCairo, (double) aX, (double) aY, (double) aWidth, (double) aHeight);
    cairo_stroke (mCairo);
    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::FillRect(const nsRect& aRect)
{
    return FillRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP
nsCairoRenderingContext::FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
    cairo_new_path (mCairo);
    cairo_rectangle (mCairo, (double) aX, (double) aY, (double) aWidth, (double) aHeight);
    cairo_fill (mCairo);
    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::InvertRect(const nsRect& aRect)
{
    return InvertRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP
nsCairoRenderingContext::InvertRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
    cairo_operator_t op = cairo_current_operator(mCairo);
    cairo_set_operator(mCairo, CAIRO_OPERATOR_XOR);
    nsresult rv = FillRect(aX, aY, aWidth, aHeight);
    cairo_set_operator(mCairo, op);
    return rv;
}

PRBool
nsCairoRenderingContext::DoCairoDrawPolygon (const nsPoint aPoints[],
                                             PRInt32 aNumPoints)
{
    if (aNumPoints < 2)
        return PR_FALSE;

    cairo_new_path (mCairo);
    cairo_move_to (mCairo, (double) aPoints[0].x, (double) aPoints[0].y);
    for (PRInt32 i = 1; i < aNumPoints; ++i) {
        cairo_line_to (mCairo, (double) aPoints[i].x, (double) aPoints[i].y);
    }
    return PR_TRUE;
}


NS_IMETHODIMP
nsCairoRenderingContext::DrawPolyline(const nsPoint aPoints[],
                                      PRInt32 aNumPoints)
{
    if (!DoCairoDrawPolygon(aPoints, aNumPoints))
        return NS_OK;

    cairo_stroke (mCairo);
    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::DrawPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
    // I'm assuming the difference between a polygon and a polyline is that this one
    // implicitly closes

    if (!DoCairoDrawPolygon(aPoints, aNumPoints))
        return NS_OK;

    cairo_close_path(mCairo);
    cairo_stroke(mCairo);
    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
    if (!DoCairoDrawPolygon(aPoints, aNumPoints))
        return NS_OK;

    cairo_close_path(mCairo);
    cairo_fill(mCairo);

    return NS_OK;
}

void
nsCairoRenderingContext::DoCairoDrawEllipse (double aX, double aY, double aWidth, double aHeight)
{
    cairo_new_path (mCairo);
    cairo_move_to (mCairo, aX + aWidth/2.0, aY);
    cairo_curve_to (mCairo,
                    aX + aWidth, aY,
                    aX + aWidth, aY,
                    aX + aWidth, aY + aHeight/2.0);
    cairo_curve_to (mCairo,
                    aX + aWidth, aY + aHeight,
                    aX + aWidth, aY + aHeight,
                    aX + aWidth/2.0, aY);
    cairo_curve_to (mCairo,
                    aX, aY + aHeight,
                    aX, aY + aHeight,
                    aX, aY + aHeight/2.0);
    cairo_curve_to (mCairo,
                    aX, aY,
                    aX, aY,
                    aX + aWidth/2.0, aY);
}

NS_IMETHODIMP
nsCairoRenderingContext::DrawEllipse(const nsRect& aRect)
{
    return DrawEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP
nsCairoRenderingContext::DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
    DoCairoDrawEllipse ((double) aX, (double) aY, (double) aWidth, (double) aHeight);
    cairo_stroke (mCairo);

    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::FillEllipse(const nsRect& aRect)
{
    return FillEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP
nsCairoRenderingContext::FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
    DoCairoDrawEllipse ((double) aX, (double) aY, (double) aWidth, (double) aHeight);
    cairo_fill (mCairo);

    return NS_OK;
}

void
nsCairoRenderingContext::DoCairoDrawArc (double aX, double aY, double aWidth, double aHeight,
                                         double aStartAngle, double aEndAngle)
{
    cairo_new_path (mCairo);

    // XXX - cairo can't do arcs with differing start and end radii.
    // I guess it's good that no code ever draws arcs.
}

NS_IMETHODIMP
nsCairoRenderingContext::DrawArc(const nsRect& aRect,
                                 float aStartAngle, float aEndAngle)
{
    return DrawArc(aRect.x, aRect.y, aRect.width, aRect.height, aStartAngle, aEndAngle);
}

NS_IMETHODIMP
nsCairoRenderingContext::DrawArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                 float aStartAngle, float aEndAngle)
{
    NS_ERROR("not used");
    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::FillArc(const nsRect& aRect,
                                 float aStartAngle, float aEndAngle)
{
    return FillArc(aRect.x, aRect.y, aRect.width, aRect.height, aStartAngle, aEndAngle);
}

NS_IMETHODIMP
nsCairoRenderingContext::FillArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                 float aStartAngle, float aEndAngle)
{
    NS_ERROR("not used");
    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::CopyOffScreenBits(nsIDrawingSurface *aSrcSurf,
                                           PRInt32 aSrcX, PRInt32 aSrcY,
                                           const nsRect &aDestBounds,
                                           PRUint32 aCopyFlags)
{
    nsCairoDrawingSurface *cds = (nsCairoDrawingSurface *) aSrcSurf;

    fprintf (stderr, "***** nsCairoRenderingContext::CopyOffScreenBits: [%p] %d,%d -> %d,%d %dx%d\n",
             aSrcSurf, aSrcX, aSrcY, aDestBounds.x, aDestBounds.y, aDestBounds.width, aDestBounds.height);

    PRUint32 srcWidth, srcHeight;
    cds->GetDimensions (&srcWidth, &srcHeight);

    double sx, sy, dx, dy, dw, dh;
    // figure out pixel positions
    sx = double(aSrcX);
    sy = double(aSrcY);
    dx = double(aDestBounds.x);
    dy = double(aDestBounds.y);
    dw = double(aDestBounds.width);
    dh = double(aDestBounds.height);

    if (aCopyFlags & NS_COPYBITS_XFORM_SOURCE_VALUES) {
        cairo_transform_point (mCairo, &sx, &sy);
    }

    if (aCopyFlags & NS_COPYBITS_XFORM_DEST_VALUES) {
        cairo_transform_point (mCairo, &dx, &dy);
        cairo_transform_distance (mCairo, &dw, &dh);
    }

    fprintf (stderr, "***** --> %g,%g [%d,%d] -> %g,%g %gx%g\n", sx, sy, srcWidth, srcHeight, dx, dy, dw, dh);
    cairo_save (mCairo);
    cairo_init_clip (mCairo);

    cairo_set_rgb_color (mCairo, 0.0, 0.0, 1.0);
    cairo_set_alpha (mCairo, 0.5);

    cairo_rectangle(mCairo, dx, dy, dw, dh);
    cairo_fill(mCairo);

    // args are in pixels!!
    cairo_identity_matrix (mCairo);

    // clip to target
    cairo_rectangle(mCairo, dx, dy, dw, dh);
    cairo_clip(mCairo);

    cairo_scale(mCairo, dw / double(srcWidth), dh / double(srcHeight));
    cairo_translate(mCairo, dx - sx, dy - sy);

//    cairo_show_surface(mCairo, src, srcWidth, srcHeight);

    cairo_restore (mCairo);

    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::RetrieveCurrentNativeGraphicData(PRUint32 * ngd)
{
    NS_WARNING ("not implemented");
    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::UseBackbuffer(PRBool* aUseBackbuffer)
{
    *aUseBackbuffer = PR_FALSE;
    // *aUseBackbuffer = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::GetBackbuffer(const nsRect &aRequestedSize,
                                       const nsRect &aMaxSize,
                                       PRBool aForBlending,
                                       nsIDrawingSurface* &aBackbuffer)
{
    if (!mBackBufferSurface) {
#if 1
        nsIDeviceContext *dc = mDeviceContext.get();
        mBackBufferSurface = new nsCairoDrawingSurface ();
        mBackBufferSurface->Init (NS_STATIC_CAST(nsCairoDeviceContext*, dc),
                                  aMaxSize.width, aMaxSize.height,
                                  PR_FALSE);
#else
        mBackBufferSurface = mDrawingSurface;
#endif
    }

    aBackbuffer = mBackBufferSurface;
    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::ReleaseBackbuffer(void)
{
    mBackBufferSurface = NULL;
    return NS_OK;
//    NS_WARNING("ReleaseBackbuffer: not implemented");
//    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCairoRenderingContext::DestroyCachedBackbuffer(void)
{
    mBackBufferSurface = NULL;
    return NS_OK;
//    NS_WARNING("DestroyCachedBackbuffer: not implemented");
//    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCairoRenderingContext::DrawImage(imgIContainer *aImage,
                                   const nsRect &aSrcRect,
                                   const nsRect &aDestRect)
{
    nsCOMPtr<gfxIImageFrame> iframe;
    aImage->GetCurrentFrame(getter_AddRefs(iframe));
    if (!iframe) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIImage> img(do_GetInterface(iframe));
    if (!img) return NS_ERROR_FAILURE;

    // For Bug 87819
    // iframe may want image to start at different position, so adjust
    nsRect iframeRect;
    iframe->GetRect(iframeRect);
  
    // aSrcRect is always in appunits, it has
    // nothing to do with the current transform
    float app2dev;
    app2dev = mDeviceContext->AppUnitsToDevUnits();
    nsRect sr;
    sr.x = NSToIntRound(app2dev*aSrcRect.x);
    sr.y = NSToIntRound(app2dev*aSrcRect.y);
    sr.width = NSToIntRound(app2dev*aSrcRect.XMost()) - sr.x;
    sr.height = NSToIntRound(app2dev*aSrcRect.YMost()) - sr.y;
    
    nsRect dr = aDestRect;
    if (iframeRect != sr) {
        double xscale = double(aDestRect.width)/sr.width;
        double yscale = double(aDestRect.height)/sr.height;
        nsRect scaledUpIRect;
        scaledUpIRect.x = NSToCoordRound(iframeRect.x*xscale);
        scaledUpIRect.y = NSToCoordRound(iframeRect.y*yscale);
        scaledUpIRect.width = NSToCoordRound(iframeRect.XMost()*xscale) - scaledUpIRect.x;
        scaledUpIRect.height = NSToCoordRound(iframeRect.YMost()*yscale) - scaledUpIRect.y;

        sr.IntersectRect(sr, iframeRect);
        dr.IntersectRect(dr, scaledUpIRect);
    }

    return img->Draw(*this, mDrawingSurface, sr.x - iframeRect.x, sr.y - iframeRect.y,
                     sr.width, sr.height,
                     dr.x, dr.y, dr.width, dr.height);
}

NS_IMETHODIMP
nsCairoRenderingContext::DrawTile(imgIContainer *aImage,
                                  nscoord aXOffset, nscoord aYOffset,
                                  const nsRect * aTargetRect)
{
    nscoord width, height;
    aImage->GetWidth(&width);
    aImage->GetHeight(&height);

    if (width == 0 || height == 0)
        return PR_FALSE;

    nsCOMPtr<gfxIImageFrame> iframe;
    aImage->GetCurrentFrame(getter_AddRefs(iframe));
    if (!iframe) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIImage> img(do_GetInterface(iframe));
    if (!img) return NS_ERROR_FAILURE;

    // For Bug 87819
    // iframe may want image to start at different position, so adjust
    nsRect iframeRect;
    iframe->GetRect(iframeRect);
  
    // offsets are always in appunits, they have
    // nothing to do with the current transform
    float app2dev;
    app2dev = mDeviceContext->AppUnitsToDevUnits();
    nsPoint s;
    s.x = NSToIntRound(app2dev*aXOffset);
    s.y = NSToIntRound(app2dev*aYOffset);
    
    PRInt32 padx = width - iframeRect.width;
    PRInt32 pady = height - iframeRect.height;

    return img->DrawTile(*this, mDrawingSurface, s.x - iframeRect.x, s.y - iframeRect.y,
                         padx, pady, *aTargetRect);
}

//
// text junk
//

NS_IMETHODIMP
nsCairoRenderingContext::SetFont(const nsFont& aFont, nsIAtom* aLangGroup)
{
    nsCOMPtr<nsIFontMetrics> newMetrics;
    nsresult rv = mDeviceContext->GetMetricsFor(aFont, aLangGroup, *getter_AddRefs(newMetrics));
    mFontMetrics = NS_REINTERPRET_CAST(nsICairoFontMetrics*, newMetrics.get());
    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::SetFont(nsIFontMetrics *aFontMetrics)
{
    mFontMetrics = NS_STATIC_CAST(nsICairoFontMetrics*, aFontMetrics);
    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::GetFontMetrics(nsIFontMetrics *&aFontMetrics)
{
    aFontMetrics = mFontMetrics;
    NS_IF_ADDREF(aFontMetrics);
    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::GetWidth(char aC, nscoord &aWidth)
{
    if (aC == ' ' && mFontMetrics)
        return mFontMetrics->GetSpaceWidth(aWidth);

    return GetWidth(&aC, 1, aWidth);
}

NS_IMETHODIMP
nsCairoRenderingContext::GetWidth(PRUnichar aC, nscoord &aWidth,
                               PRInt32 *aFontID)
{
    return GetWidth(&aC, 1, aWidth, aFontID);
}

NS_IMETHODIMP
nsCairoRenderingContext::GetWidth(const nsString& aString, nscoord &aWidth,
                               PRInt32 *aFontID)
{
    return GetWidth(aString.get(), aString.Length(), aWidth, aFontID);
}

NS_IMETHODIMP
nsCairoRenderingContext::GetWidth(const char* aString, nscoord& aWidth)
{
    return GetWidth(aString, strlen(aString), aWidth);
}

NS_IMETHODIMP
nsCairoRenderingContext::GetWidth(const char* aString, PRUint32 aLength,
                                  nscoord& aWidth)
{
    if (aLength == 0) {
        aWidth = 0;
        return NS_OK;
    }

    return mFontMetrics->GetWidth(aString, aLength, aWidth);
}

NS_IMETHODIMP
nsCairoRenderingContext::GetWidth(const PRUnichar *aString, PRUint32 aLength,
                                  nscoord &aWidth, PRInt32 *aFontID)
{
    if (aLength == 0) {
        aWidth = 0;
        return NS_OK;
    }

    return mFontMetrics->GetWidth(aString, aLength, aWidth, aFontID);
}

NS_IMETHODIMP
nsCairoRenderingContext::GetTextDimensions(const char* aString, PRUint32 aLength,
                                           nsTextDimensions& aDimensions)
{
  mFontMetrics->GetMaxAscent(aDimensions.ascent);
  mFontMetrics->GetMaxDescent(aDimensions.descent);
  return GetWidth(aString, aLength, aDimensions.width);
}

NS_IMETHODIMP
nsCairoRenderingContext::GetTextDimensions(const PRUnichar* aString,
                                           PRUint32 aLength,
                                           nsTextDimensions& aDimensions,
                                           PRInt32* aFontID)
{
  mFontMetrics->GetMaxAscent(aDimensions.ascent);
  mFontMetrics->GetMaxDescent(aDimensions.descent);
  return GetWidth(aString, aLength, aDimensions.width, aFontID);
}

#if defined(_WIN32) || defined(XP_OS2) || defined(MOZ_X11) || defined(XP_BEOS)
NS_IMETHODIMP
nsCairoRenderingContext::GetTextDimensions(const char*       aString,
                                           PRInt32           aLength,
                                           PRInt32           aAvailWidth,
                                           PRInt32*          aBreaks,
                                           PRInt32           aNumBreaks,
                                           nsTextDimensions& aDimensions,
                                           PRInt32&          aNumCharsFit,
                                           nsTextDimensions& aLastWordDimensions,
                                           PRInt32*          aFontID)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCairoRenderingContext::GetTextDimensions(const PRUnichar*  aString,
                                           PRInt32           aLength,
                                           PRInt32           aAvailWidth,
                                           PRInt32*          aBreaks,
                                           PRInt32           aNumBreaks,
                                           nsTextDimensions& aDimensions,
                                           PRInt32&          aNumCharsFit,
                                           nsTextDimensions& aLastWordDimensions,
                                           PRInt32*          aFontID)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
#endif

#ifdef MOZ_MATHML
NS_IMETHODIMP 
nsCairoRenderingContext::GetBoundingMetrics(const char*        aString,
                                            PRUint32           aLength,
                                            nsBoundingMetrics& aBoundingMetrics)
{
  return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::GetBoundingMetrics(const PRUnichar*   aString,
                                         PRUint32           aLength,
                                         nsBoundingMetrics& aBoundingMetrics,
                                         PRInt32*           aFontID)
{
    return NS_OK;
}
#endif // MOZ_MATHML

NS_IMETHODIMP
nsCairoRenderingContext::DrawString(const char *aString, PRUint32 aLength,
                                    nscoord aX, nscoord aY,
                                    const nscoord* aSpacing)
{
    return mFontMetrics->DrawString(aString, aLength, aX, aY, aSpacing,
                                    this, mDrawingSurface);

#if 0
    NS_WARNING("DrawString 1");
    cairo_move_to(mCairo, double(aX), double(aY));
    cairo_new_path (mCairo);
    cairo_text_path (mCairo, (const unsigned char *) aString);
    cairo_fill (mCairo);
    cairo_move_to(mCairo, -double(aX), -double(aY));
#endif
    return NS_OK;
}

NS_IMETHODIMP
nsCairoRenderingContext::DrawString(const PRUnichar *aString, PRUint32 aLength,
                                    nscoord aX, nscoord aY,
                                    PRInt32 aFontID,
                                    const nscoord* aSpacing)
{
    return mFontMetrics->DrawString(aString, aLength, aX, aY, aFontID,
                                    aSpacing, this, mDrawingSurface);
}

NS_IMETHODIMP
nsCairoRenderingContext::DrawString(const nsString& aString,
                                    nscoord aX, nscoord aY,
                                    PRInt32 aFontID,
                                    const nscoord* aSpacing)
{
    return DrawString(aString.get(), aString.Length(),
                      aX, aY, aFontID, aSpacing);
}

NS_IMETHODIMP
nsCairoRenderingContext::GetClusterInfo(const PRUnichar *aText,
                                       PRUint32 aLength,
                                       PRUint8 *aClusterStarts)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

PRInt32
nsCairoRenderingContext::GetPosition(const PRUnichar *aText,
                                    PRUint32 aLength,
                                    nsPoint aPt)
{
  return -1;
}

NS_IMETHODIMP
nsCairoRenderingContext::GetRangeWidth(const PRUnichar *aText,
                                      PRUint32 aLength,
                                      PRUint32 aStart,
                                      PRUint32 aEnd,
                                      PRUint32 &aWidth)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCairoRenderingContext::GetRangeWidth(const char *aText,
                                      PRUint32 aLength,
                                      PRUint32 aStart,
                                      PRUint32 aEnd,
                                      PRUint32 &aWidth)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCairoRenderingContext::RenderEPS(const nsRect& aRect, FILE *aDataFile)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
