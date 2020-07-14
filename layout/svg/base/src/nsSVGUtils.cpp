/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsSVGLength.h"
#include "nsIDOMDocument.h"
#include "nsIDOMSVGElement.h"
#include "nsIDOMSVGSVGElement.h"
#include "nsStyleCoord.h"
#include "nsPresContext.h"
#include "nsSVGCoordCtxProvider.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsLayoutAtoms.h"
#include "nsIURI.h"
#include "nsStyleStruct.h"
#include "nsIPresShell.h"
#include "nsSVGUtils.h"
#include "nsISVGGeometrySource.h"
#include "nsNetUtil.h"
#include "nsContentDLF.h"
#include "nsContentUtils.h"
#include "nsISVGRenderer.h"
#include "nsIDOMSVGRect.h"
#include "nsFrameList.h"
#include "nsISVGChildFrame.h"
#include "nsIDOMSVGMatrix.h"
#include "cairo.h"

#if defined(MOZ_SVG_RENDERER_GDIPLUS)
#include <windows.h>
#endif

static PRBool gSVGEnabled;
static PRBool gSVGRendererAvailable = PR_FALSE;
static const char SVG_PREF_STR[] = "svg.enabled";

PR_STATIC_CALLBACK(int)
SVGPrefChanged(const char *aPref, void *aClosure)
{
  PRBool prefVal = nsContentUtils::GetBoolPref(SVG_PREF_STR);
  if (prefVal == gSVGEnabled)
    return 0;

  gSVGEnabled = prefVal;
  if (gSVGRendererAvailable) {
    if (gSVGEnabled)
      nsContentDLF::RegisterSVG();
    else
      nsContentDLF::UnregisterSVG();
  }

  return 0;
}

PRBool
nsSVGUtils::SVGEnabled()
{
  static PRBool sInitialized = PR_FALSE;

  if (!sInitialized) {
    gSVGRendererAvailable = PR_TRUE;

#if defined(MOZ_SVG_RENDERER_GDIPLUS)
    HMODULE gdiplus, gkgdiplus;

    if ((gdiplus = LoadLibrary("gdiplus.dll")) == NULL)
      gSVGRendererAvailable = PR_FALSE;
  #endif

    /* check and register ourselves with the pref */
    gSVGEnabled = nsContentUtils::GetBoolPref(SVG_PREF_STR);
    nsContentUtils::RegisterPrefCallback(SVG_PREF_STR, SVGPrefChanged, nsnull);

    sInitialized = PR_TRUE;
  }

  return gSVGEnabled && gSVGRendererAvailable;
}

float
nsSVGUtils::CoordToFloat(nsPresContext *aPresContext, nsIContent *aContent,
			 const nsStyleCoord &aCoord)
{
  float val = 0.0f;

  switch (aCoord.GetUnit()) {
  case eStyleUnit_Factor:
    // user units
    val = aCoord.GetFactorValue();
    break;

  case eStyleUnit_Coord:
    val = aCoord.GetCoordValue() / aPresContext->ScaledPixelsToTwips();
    break;

  case eStyleUnit_Percent: {
      nsCOMPtr<nsIDOMSVGElement> element = do_QueryInterface(aContent);
      if (!element)
        break;
      nsCOMPtr<nsIDOMSVGSVGElement> owner;
      element->GetOwnerSVGElement(getter_AddRefs(owner));
      nsCOMPtr<nsSVGCoordCtxProvider> ctx = do_QueryInterface(owner);
    
      nsCOMPtr<nsISVGLength> length;
      NS_NewSVGLength(getter_AddRefs(length), aCoord.GetPercentValue() * 100.0f,
                      nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE);
    
      if (!ctx || !length)
        break;

      length->SetContext(nsRefPtr<nsSVGCoordCtx>(ctx->GetContextUnspecified()));
      length->GetValue(&val);
      break;
    }
  default:
    break;
  }

  return val;
}

float
nsSVGUtils::UserSpace(nsIContent *content,
                      nsIDOMSVGLength *length,
                      ctxDirection direction)
{
  PRUint16 units;
  float value;

  length->GetUnitType(&units);
  length->GetValueInSpecifiedUnits(&value);

  nsCOMPtr<nsISVGLength> val;
  NS_NewSVGLength(getter_AddRefs(val), value, units);
 
  nsCOMPtr<nsIDOMSVGElement> element = do_QueryInterface(content);
  nsCOMPtr<nsIDOMSVGSVGElement> svg;
  element->GetOwnerSVGElement(getter_AddRefs(svg));
  nsCOMPtr<nsSVGCoordCtxProvider> ctx = do_QueryInterface(svg);

  if (ctx) {
    switch (direction) {
    case X:
      val->SetContext(nsRefPtr<nsSVGCoordCtx>(ctx->GetContextX()));
      break;
    case Y:
      val->SetContext(nsRefPtr<nsSVGCoordCtx>(ctx->GetContextY()));
      break;
    case XY:
      val->SetContext(nsRefPtr<nsSVGCoordCtx>(ctx->GetContextUnspecified()));
      break;
    }
  }

  val->GetValue(&value);
  return value;
}

nsresult nsSVGUtils::GetReferencedFrame(nsIFrame **aRefFrame, nsCAutoString& uriSpec, nsIContent *aContent, 
                                        nsIPresShell *aPresShell)
{
  nsresult rv = NS_OK;
  *aRefFrame = nsnull;

  // Get the ID from the spec (no ID = an error)
  PRInt32 pos = uriSpec.FindChar('#');
  if (pos == -1) {
    NS_ASSERTION(pos != -1, "URI Spec not a reference");
    return NS_ERROR_FAILURE;
  }

  // Get the current document
  nsIDocument *myDoc = aContent->GetCurrentDoc();
  if (!myDoc) {
    NS_WARNING("Content doesn't reference a Document!");
    return NS_ERROR_FAILURE;
  }

  // Get our URI
  nsCOMPtr<nsIURI> myURI = myDoc->GetDocumentURI();

#ifdef DEBUG_scooter
    // Get the uri Spec
    nsCAutoString dSpec;
    myURI->GetSpec(dSpec);
    printf("Document URI = %s, target URI = %s\n",dSpec.get(), uriSpec.get());
#endif

  // Create a URI out of the target
  nsCAutoString aURISName;
  uriSpec.Left(aURISName, pos);
  nsCOMPtr<nsIURI> targetURI;
  NS_NewURI(getter_AddRefs(targetURI), aURISName, nsnull, myDoc->GetBaseURI());
  PRBool match;
  myURI->Equals(targetURI, &match);
  if (!match) {
    // Oops -- we don't support off-document references
    return NS_ERROR_FAILURE;
  }

  // At this point, we know we have a target within our document, but
  // it may not point to anything
  // Strip off the hash and get the name
  nsCAutoString aURICName;
  uriSpec.Right(aURICName, uriSpec.Length()-(pos + 1));

  // Get a unicode string
  nsAutoString  aURIName;
  CopyUTF8toUTF16(aURICName, aURIName);

  // Get the domDocument
  nsCOMPtr<nsIDOMDocument>domDoc = do_QueryInterface(myDoc);
  NS_ASSERTION(domDoc, "Content doesn't reference a dom Document");
  if (domDoc == nsnull) {
    return NS_ERROR_FAILURE;
  }

  // Get the element
  nsCOMPtr<nsIDOMElement> element;
  rv = domDoc->GetElementById(aURIName, getter_AddRefs(element));
  if (!NS_SUCCEEDED(rv) || element == nsnull) {
    return NS_ERROR_FAILURE;  
  }

  // Get the Primary Frame
  nsCOMPtr<nsIContent> aGContent = do_QueryInterface(element);
  NS_ASSERTION(aPresShell, "Get referenced SVG frame -- no pres shell provided");
  if (!aPresShell)
    return NS_ERROR_FAILURE;

  rv = aPresShell->GetPrimaryFrameFor(aGContent, aRefFrame);
  NS_ASSERTION(*aRefFrame, "Get referenced SVG frame -- can't find primary frame");
  if (!(*aRefFrame)) return NS_ERROR_FAILURE;
  return rv;
}

nsresult nsSVGUtils::GetPaintType(PRUint16 *aPaintType, const nsStyleSVGPaint& aPaint, 
                                  nsIContent *aContent, nsIPresShell *aPresShell )
{
  *aPaintType = aPaint.mType;
  // If the type is a Paint Server, determine what kind
  if (*aPaintType == nsISVGGeometrySource::PAINT_TYPE_SERVER) {
    nsIURI *aServer = aPaint.mPaint.mPaintServer;
    if (aServer == nsnull)
      return NS_ERROR_FAILURE;

    // Get the uri Spec
    nsCAutoString uriSpec;
    aServer->GetSpec(uriSpec);

    // Get the frame
    nsIFrame *aFrame = nsnull;
    nsresult rv;
    rv = nsSVGUtils::GetReferencedFrame(&aFrame, uriSpec, aContent, aPresShell);
    if (!NS_SUCCEEDED(rv) || !aFrame)
      return NS_ERROR_FAILURE;

    // Finally, figure out what type it is
    if (aFrame->GetType() == nsLayoutAtoms::svgLinearGradientFrame ||
        aFrame->GetType() == nsLayoutAtoms::svgRadialGradientFrame)
      *aPaintType = nsISVGGeometrySource::PAINT_TYPE_GRADIENT;
    else if (aFrame->GetType() == nsLayoutAtoms::svgPatternFrame)
      *aPaintType = nsISVGGeometrySource::PAINT_TYPE_PATTERN;
    else
      return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
nsSVGUtils::GetBBox(nsFrameList *aFrames, nsIDOMSVGRect **_retval)
{
  *_retval = nsnull;

  float minx, miny, maxx, maxy;
  minx = miny = FLT_MAX;
  maxx = maxy = -1.0 * FLT_MAX;

  nsCOMPtr<nsIDOMSVGRect> unionRect;

  nsIFrame* kid = aFrames->FirstChild();
  while (kid) {
    nsISVGChildFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame), (void**)&SVGFrame);
    if (SVGFrame) {
      nsCOMPtr<nsIDOMSVGRect> box;
      SVGFrame->GetBBox(getter_AddRefs(box));

      if (box) {
        float bminx, bminy, bmaxx, bmaxy, width, height;
        box->GetX(&bminx);
        box->GetY(&bminy);
        box->GetWidth(&width);
        box->GetHeight(&height);
        bmaxx = bminx+width;
        bmaxy = bminy+height;

        if (!unionRect)
          unionRect = box;
        minx = PR_MIN(minx, bminx);
        miny = PR_MIN(miny, bminy);
        maxx = PR_MAX(maxx, bmaxx);
        maxy = PR_MAX(maxy, bmaxy);
      }
    }
    kid = kid->GetNextSibling();
  }

  if (unionRect) {
    unionRect->SetX(minx);
    unionRect->SetY(miny);
    unionRect->SetWidth(maxx - minx);
    unionRect->SetHeight(maxy - miny);
    *_retval = unionRect;
    NS_ADDREF(*_retval);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

static cairo_surface_t *
GetCairoComputationalSurface()
{
  static cairo_surface_t *sCairoComputationalSurface = nsnull;

  if (!sCairoComputationalSurface)
    sCairoComputationalSurface =
      cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);

  return sCairoComputationalSurface;
}

static cairo_matrix_t
ConvertSVGMatrixToCairo(nsIDOMSVGMatrix *aMatrix)
{
  float A, B, C, D, E, F;
  aMatrix->GetA(&A);
  aMatrix->GetB(&B);
  aMatrix->GetC(&C);
  aMatrix->GetD(&D);
  aMatrix->GetE(&E);
  aMatrix->GetF(&F);
  cairo_matrix_t m = { A, B, C, D, E, F };
  return m;
}

PRBool
nsSVGUtils::HitTestRect(nsIDOMSVGMatrix *aMatrix,
                        float aRX, float aRY, float aRWidth, float aRHeight,
                        float aX, float aY)
{
  PRBool result = PR_TRUE;

  if (aMatrix) {
    cairo_matrix_t matrix = ConvertSVGMatrixToCairo(aMatrix);
    cairo_t *ctx = cairo_create(GetCairoComputationalSurface());
    cairo_set_tolerance(ctx, 1.0);

    cairo_set_matrix(ctx, &matrix);
    cairo_new_path(ctx);
    cairo_rectangle(ctx, aRX, aRY, aRWidth, aRHeight);
    cairo_identity_matrix(ctx);

    if (!cairo_in_fill(ctx, aX, aY))
      result = PR_FALSE;

    cairo_destroy(ctx);
  }

  return result;
}
