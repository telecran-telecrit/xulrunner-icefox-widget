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
 * The Initial Developer of the Original Code is
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
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

#include "nsBlockFrame.h"
#include "nsIDOMSVGGElement.h"
#include "nsPresContext.h"
#include "nsISVGChildFrame.h"
#include "nsISVGContainerFrame.h"
#include "nsISVGRendererCanvas.h"
#include "nsWeakReference.h"
#include "nsISVGValue.h"
#include "nsISVGValueObserver.h"
#include "nsIDOMSVGTransformable.h"
#include "nsIDOMSVGAnimTransformList.h"
#include "nsIDOMSVGTransformList.h"
#include "nsIDOMSVGAnimatedLength.h"
#include "nsIDOMSVGLength.h"
#include "nsIDOMSVGForeignObjectElem.h"
#include "nsIDOMSVGMatrix.h"
#include "nsIDOMSVGSVGElement.h"
#include "nsIDOMSVGPoint.h"
#include "nsSpaceManager.h"
#include "nsISVGRendererRegion.h"
#include "nsISVGRenderer.h"
#include "nsISVGOuterSVGFrame.h"
#include "nsTransform2D.h"
#include "nsSVGPoint.h"
#include "nsSVGRect.h"
#include "nsSVGMatrix.h"
#include "nsLayoutAtoms.h"

typedef nsBlockFrame nsSVGForeignObjectFrameBase;

class nsSVGForeignObjectFrame : public nsSVGForeignObjectFrameBase,
                                public nsISVGContainerFrame,
                                public nsISVGChildFrame,
                                public nsISVGValueObserver,
                                public nsSupportsWeakReference
{
  friend nsresult
  NS_NewSVGForeignObjectFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame);
protected:
  nsSVGForeignObjectFrame();
  virtual ~nsSVGForeignObjectFrame();
  nsresult Init();
  
  // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }  
public:
  // nsIFrame:  
  NS_IMETHOD Init(nsPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsStyleContext*  aContext,
                  nsIFrame*        aPrevInFlow);

  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD  AppendFrames(nsIAtom*        aListName,
                           nsIFrame*       aFrameList);
  
  NS_IMETHOD  InsertFrames(nsIAtom*        aListName,
                           nsIFrame*       aPrevFrame,
                           nsIFrame*       aFrameList);
  
  NS_IMETHOD  RemoveFrame(nsIAtom*        aListName,
                          nsIFrame*       aOldFrame);
  
  NS_IMETHOD  ReplaceFrame(nsIAtom*        aListName,
                           nsIFrame*       aOldFrame,
                           nsIFrame*       aNewFrame);

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::svgForeignObjectFrame
   */
  // XXX Need to make sure that any of the code examining
  // frametypes, particularly code looking at block and area
  // also handles foreignObject before we return our own frametype
  // virtual nsIAtom* GetType() const;
  virtual PRBool IsFrameOfType(PRUint32 aFlags) const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGForeignObject"), aResult);
  }
#endif

  // nsISVGValueObserver
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable,
                                     nsISVGValue::modificationType aModType);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable,
                                     nsISVGValue::modificationType aModType);

  // nsISupportsWeakReference
  // implementation inherited from nsSupportsWeakReference
  
  // nsISVGChildFrame interface:
  NS_IMETHOD PaintSVG(nsISVGRendererCanvas* canvas, const nsRect& dirtyRectTwips);
  NS_IMETHOD GetFrameForPointSVG(float x, float y, nsIFrame** hit);  
  NS_IMETHOD_(already_AddRefed<nsISVGRendererRegion>) GetCoveredRegion();
  NS_IMETHOD InitialUpdate();
  NS_IMETHOD NotifyCanvasTMChanged();
  NS_IMETHOD NotifyRedrawSuspended();
  NS_IMETHOD NotifyRedrawUnsuspended();
  NS_IMETHOD SetMatrixPropagation(PRBool aPropagate);
  NS_IMETHOD GetBBox(nsIDOMSVGRect **_retval);
  
  // nsISVGContainerFrame interface:
  nsISVGOuterSVGFrame *GetOuterSVGFrame();
  already_AddRefed<nsIDOMSVGMatrix> GetCanvasTM();
  already_AddRefed<nsSVGCoordCtxProvider> GetCoordContextProvider();
  
protected:
  // implementation helpers:
  void Update();
  already_AddRefed<nsISVGRendererRegion> DoReflow();
  float GetPxPerTwips();
  float GetTwipsPerPx();
  void TransformPoint(float& x, float& y);
  void TransformVector(float& x, float& y);

  PRBool mIsDirty;
  nsCOMPtr<nsIDOMSVGLength> mX;
  nsCOMPtr<nsIDOMSVGLength> mY;
  nsCOMPtr<nsIDOMSVGLength> mWidth;
  nsCOMPtr<nsIDOMSVGLength> mHeight;
  nsCOMPtr<nsIDOMSVGMatrix> mCanvasTM;
  PRBool mPropagateTransform;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGForeignObjectFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame)
{
  *aNewFrame = nsnull;
  
  nsCOMPtr<nsIDOMSVGForeignObjectElement> foreignObject = do_QueryInterface(aContent);
  if (!foreignObject) {
#ifdef DEBUG
    printf("warning: trying to construct an SVGForeignObjectFrame for a content element that doesn't support the right interfaces\n");
#endif
    return NS_ERROR_FAILURE;
  }
  
  nsSVGForeignObjectFrame* it = new (aPresShell) nsSVGForeignObjectFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;

  return NS_OK;
}

nsSVGForeignObjectFrame::nsSVGForeignObjectFrame()
  : mIsDirty(PR_TRUE), mPropagateTransform(PR_TRUE)
{
}

nsSVGForeignObjectFrame::~nsSVGForeignObjectFrame()
{
//   nsCOMPtr<nsIDOMSVGTransformable> transformable = do_QueryInterface(mContent);
//   NS_ASSERTION(transformable, "wrong content element");
//   nsCOMPtr<nsIDOMSVGAnimatedTransformList> transforms;
//   transformable->GetTransform(getter_AddRefs(transforms));
//   nsCOMPtr<nsISVGValue> value = do_QueryInterface(transforms);
//   NS_ASSERTION(value, "interface not found");
//   if (value)
//     value->RemoveObserver(this);
  nsCOMPtr<nsISVGValue> value;
  if (mX && (value = do_QueryInterface(mX)))
      value->RemoveObserver(this);
  if (mY && (value = do_QueryInterface(mY)))
      value->RemoveObserver(this);
  if (mWidth && (value = do_QueryInterface(mWidth)))
      value->RemoveObserver(this);
  if (mHeight && (value = do_QueryInterface(mHeight)))
      value->RemoveObserver(this);
}

nsresult nsSVGForeignObjectFrame::Init()
{
  nsCOMPtr<nsIDOMSVGForeignObjectElement> foreignObject = do_QueryInterface(mContent);
  NS_ASSERTION(foreignObject, "wrong content element");
  
  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    foreignObject->GetX(getter_AddRefs(length));
    length->GetAnimVal(getter_AddRefs(mX));
    NS_ASSERTION(mX, "no x");
    if (!mX) return NS_ERROR_FAILURE;
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(mX);
    if (value)
      value->AddObserver(this);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    foreignObject->GetY(getter_AddRefs(length));
    length->GetAnimVal(getter_AddRefs(mY));
    NS_ASSERTION(mY, "no y");
    if (!mY) return NS_ERROR_FAILURE;
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(mY);
    if (value)
      value->AddObserver(this);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    foreignObject->GetWidth(getter_AddRefs(length));
    length->GetAnimVal(getter_AddRefs(mWidth));
    NS_ASSERTION(mWidth, "no width");
    if (!mWidth) return NS_ERROR_FAILURE;
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(mWidth);
    if (value)
      value->AddObserver(this);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> length;
    foreignObject->GetHeight(getter_AddRefs(length));
    length->GetAnimVal(getter_AddRefs(mHeight));
    NS_ASSERTION(mHeight, "no height");
    if (!mHeight) return NS_ERROR_FAILURE;
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(mHeight);
    if (value)
      value->AddObserver(this);
  }
  
// XXX 
//   nsCOMPtr<nsIDOMSVGTransformable> transformable = do_QueryInterface(mContent);
//   NS_ASSERTION(transformable, "wrong content element");
//   nsCOMPtr<nsIDOMSVGAnimatedTransformList> transforms;
//   transformable->GetTransform(getter_AddRefs(transforms));
//   nsCOMPtr<nsISVGValue> value = do_QueryInterface(transforms);
//   NS_ASSERTION(value, "interface not found");
//   if (value)
//     value->AddObserver(this);

  // XXX for some reason updating fails when done here. Why is this too early?
  // anyway - we use a less desirable mechanism now of updating in paint().
//  Update(); 
  
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGForeignObjectFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGChildFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGContainerFrame)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
NS_INTERFACE_MAP_END_INHERITING(nsSVGForeignObjectFrameBase)


//----------------------------------------------------------------------
// nsIFrame methods
NS_IMETHODIMP
nsSVGForeignObjectFrame::Init(nsPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsStyleContext*  aContext,
                  nsIFrame*        aPrevInFlow)
{
  nsresult rv;
  rv = nsSVGForeignObjectFrameBase::Init(aPresContext, aContent, aParent,
                             aContext, aPrevInFlow);

  Init();

  return rv;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::Reflow(nsPresContext*          aPresContext,
                                nsHTMLReflowMetrics&     aDesiredSize,
                                const nsHTMLReflowState& aReflowState,
                                nsReflowStatus&          aStatus)
{
#ifdef DEBUG
  printf("nsSVGForeignObjectFrame(%p)::Reflow\n", this);
#endif
  
  float twipsPerPx = GetTwipsPerPx();
  
  NS_ENSURE_TRUE(mX && mY && mWidth && mHeight, NS_ERROR_FAILURE);

  float x, y, width, height;
  mX->GetValue(&x);
  mY->GetValue(&y);
  mWidth->GetValue(&width);
  mHeight->GetValue(&height);

  // transform x,y,width,height according to the current canvastm:
  // XXX we're ignoring rotation at the moment

  // (x, y): (left, top) -> (center_x, center_y)
  x+=width/2.0f;
  y+=height/2.0f;

  // (x, y): (cx, cy) -> (cx', cy')
  TransformPoint(x, y);
  
  // find new ex & ey unit vectors
  float e1x = 1.0f, e1y = 0.0f, e2x = 0.0f, e2y = 1.0f;
  TransformVector(e1x, e1y);
  TransformVector(e2x, e2y);
  // adopt the scale of them for (w,h)
  width  *= (float)sqrt(e1x*e1x + e1y*e1y);
  height *= (float)sqrt(e2x*e2x + e2y*e2y);
  
  // (x, y): (cx', cy') -> (left', top')
  x -= width/2.0f;
  y -= height/2.0f;

  // move ourselves to (x,y):
  SetPosition(nsPoint((nscoord) (x*twipsPerPx), (nscoord) (y*twipsPerPx)));
  // Xxx: if zewe have a view, move that 
  
  // create a new reflow state, setting our max size to (width,height):
  nsSize availableSpace((nscoord)(width*twipsPerPx), (nscoord)(height*twipsPerPx));
  nsHTMLReflowState sizedReflowState(aPresContext,
                                     aReflowState,
                                     this,
                                     availableSpace);

  // XXX Not sure the dirty-state should be cleared here. We should
  // re-examine how the reflow is driven from nsSVGOuterSVGFrame. I
  // think we're missing a call to DidReflow somewhere ?
  mState &= ~NS_FRAME_IS_DIRTY;
  
  // leverage our base class' reflow function to do all the work:
  return nsSVGForeignObjectFrameBase::Reflow(aPresContext, aDesiredSize,
                                             sizedReflowState, aStatus);
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::AppendFrames(nsIAtom*        aListName,
                                      nsIFrame*       aFrameList)
{
#ifdef DEBUG
  printf("**nsSVGForeignObjectFrame::AppendFrames()\n");
#endif
	nsresult rv;
	rv = nsSVGForeignObjectFrameBase::AppendFrames(aListName, aFrameList);
	Update();
	return rv;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::InsertFrames(nsIAtom*        aListName,
                                      nsIFrame*       aPrevFrame,
                                      nsIFrame*       aFrameList)
{
#ifdef DEBUG
  printf("**nsSVGForeignObjectFrame::InsertFrames()\n");
#endif
	nsresult rv;
	rv = nsSVGForeignObjectFrameBase::InsertFrames(aListName, aPrevFrame,
                                                   aFrameList);
	Update();
	return rv;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::RemoveFrame(nsIAtom*        aListName,
                                     nsIFrame*       aOldFrame)
{
	nsresult rv;
	rv = nsSVGForeignObjectFrameBase::RemoveFrame(aListName, aOldFrame);
	Update();
	return rv;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::ReplaceFrame(nsIAtom*        aListName,
                                      nsIFrame*       aOldFrame,
                                      nsIFrame*       aNewFrame)
{
#ifdef DEBUG
  printf("**nsSVGForeignObjectFrame::ReplaceFrame()\n");
#endif
	nsresult rv;
	rv = nsSVGForeignObjectFrameBase::ReplaceFrame(aListName, aOldFrame,
                                                   aNewFrame);
	Update();
	return rv;
}

// XXX Need to make sure that any of the code examining
// frametypes, particularly code looking at block and area
// also handles foreignObject before we return our own frametype
// nsIAtom *
// nsSVGForeignObjectFrame::GetType() const
// {
//   return nsLayoutAtoms::svgForeignObjectFrame;
// }

PRBool
nsSVGForeignObjectFrame::IsFrameOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~(nsIFrame::eSVG | nsIFrame::eSVGForeignObject));
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods:

NS_IMETHODIMP
nsSVGForeignObjectFrame::WillModifySVGObservable(nsISVGValue* observable,
                                                 nsISVGValue::modificationType aModType)
{
  return NS_OK;
}


NS_IMETHODIMP
nsSVGForeignObjectFrame::DidModifySVGObservable (nsISVGValue* observable,
                                                 nsISVGValue::modificationType aModType)
{
  Update();
  
  return NS_OK;
}


//----------------------------------------------------------------------
// nsISVGChildFrame methods

NS_IMETHODIMP
nsSVGForeignObjectFrame::PaintSVG(nsISVGRendererCanvas* canvas, const nsRect& dirtyRectTwips)
{
  if (mIsDirty) {
    nsCOMPtr<nsISVGRendererRegion> region = DoReflow();
  }

  nsPresContext *presContext = GetPresContext();

  nsRect r(mRect);
  if (!r.IntersectRect(dirtyRectTwips, r))
    return PR_TRUE;
  
  float pxPerTwips = GetPxPerTwips();
  r.x*=pxPerTwips;
  r.y*=pxPerTwips;
  r.width*=pxPerTwips;
  r.height*=pxPerTwips;
  
  nsCOMPtr<nsIRenderingContext> ctx;
  canvas->LockRenderingContext(r, getter_AddRefs(ctx));

  if (!ctx) {
    NS_WARNING("Can't render foreignObject element!");
    return NS_ERROR_FAILURE;
  }
  
  nsRect dirtyRect = dirtyRectTwips;
  dirtyRect.x -= mRect.x;
  dirtyRect.y -= mRect.y;

  // As described in nsContainerFrame, don't use PushState/PopState;
  // instead save/restore the modified transform components:
  float xMatrix;
  float yMatrix;
  nsTransform2D *theTransform;
  ctx->GetCurrentTransform(theTransform);
  theTransform->GetTranslation(&xMatrix, &yMatrix);
  ctx->Translate(mRect.x, mRect.y);
  
  nsSVGForeignObjectFrameBase::Paint(presContext,
                                     *ctx,
                                     dirtyRect,
                                     NS_FRAME_PAINT_LAYER_BACKGROUND,
                                     0);
  
  nsSVGForeignObjectFrameBase::Paint(presContext,
                                     *ctx,
                                     dirtyRect,
                                     NS_FRAME_PAINT_LAYER_FLOATS,
                                     0);
  
  nsSVGForeignObjectFrameBase::Paint(presContext,
                                     *ctx,
                                     dirtyRect,
                                     NS_FRAME_PAINT_LAYER_FOREGROUND,
                                     0);

  theTransform->SetTranslation(xMatrix, yMatrix);
  ctx = nsnull;
  canvas->UnlockRenderingContext();
  
  return NS_OK;
}


NS_IMETHODIMP
nsSVGForeignObjectFrame::GetFrameForPointSVG(float x, float y, nsIFrame** hit)
{
  *hit = nsnull;

  nsPoint p( (nscoord)(x*GetTwipsPerPx()),
             (nscoord)(y*GetTwipsPerPx()));

  nsresult rv;

  rv = nsSVGForeignObjectFrameBase::GetFrameForPoint(p,
                                               NS_FRAME_PAINT_LAYER_FOREGROUND,
                                               hit);
  if (NS_SUCCEEDED(rv) && *hit) return rv;

  rv = nsSVGForeignObjectFrameBase::GetFrameForPoint(p,
                                               NS_FRAME_PAINT_LAYER_FLOATS,
                                               hit);
  if (NS_SUCCEEDED(rv) && *hit) return rv;

  return nsSVGForeignObjectFrameBase::GetFrameForPoint(p,
                                               NS_FRAME_PAINT_LAYER_BACKGROUND,
                                               hit);
}

NS_IMETHODIMP_(already_AddRefed<nsISVGRendererRegion>)
nsSVGForeignObjectFrame::GetCoveredRegion()
{
  // get a region from our mRect
  
  //  if (mRect.width==0 || mRect.height==0) return nsnull;
  nsISVGOuterSVGFrame *outerSVGFrame = GetOuterSVGFrame();
  if (!outerSVGFrame) {
    NS_ERROR("null outerSVGFrame");
    return nsnull;
  }
  
  nsCOMPtr<nsISVGRenderer> renderer;
  outerSVGFrame->GetRenderer(getter_AddRefs(renderer));
  
  float pxPerTwips = GetPxPerTwips();

  nsISVGRendererRegion *region = nsnull;
  renderer->CreateRectRegion((mRect.x-1) * pxPerTwips,
                             (mRect.y-1) * pxPerTwips,
                             (mRect.width+2) * pxPerTwips,
                             (mRect.height+2) * pxPerTwips,
                             &region);
  NS_ASSERTION(region, "could not create region");
  return region;
  
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::InitialUpdate()
{
//  Update();
  return NS_OK;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::NotifyCanvasTMChanged()
{
  mCanvasTM = nsnull;
  Update();
  return NS_OK;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::NotifyRedrawSuspended()
{
  return NS_OK;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::NotifyRedrawUnsuspended()
{
  if (mIsDirty) {
    nsCOMPtr<nsISVGRendererRegion> dirtyRegion = DoReflow();
    if (dirtyRegion) {
      nsISVGOuterSVGFrame *outerSVGFrame = GetOuterSVGFrame();
      if (outerSVGFrame)
        outerSVGFrame->InvalidateRegion(dirtyRegion, PR_TRUE);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::SetMatrixPropagation(PRBool aPropagate)
{
  mPropagateTransform = aPropagate;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::GetBBox(nsIDOMSVGRect **_retval)
{
  *_retval = nsnull;

  float x[4], y[4], width, height;
  mX->GetValue(&x[0]);
  mY->GetValue(&y[0]);
  mWidth->GetValue(&width);
  mHeight->GetValue(&height);

  x[1] = x[0] + width;
  y[1] = y[0];
  x[2] = x[0] + width;
  y[2] = y[0] + height;
  x[3] = x[0];
  y[3] = y[0] + height;

  TransformPoint(x[0], y[0]);
  TransformPoint(x[1], y[1]);
  TransformPoint(x[2], y[2]);
  TransformPoint(x[3], y[3]);

  float xmin, xmax, ymin, ymax;
  xmin = xmax = x[0];
  ymin = ymax = y[0];
  for (int i=1; i<4; i++) {
    if (x[i] < xmin)
      xmin = x[i];
    if (y[i] < ymin)
      ymin = y[i];
    if (x[i] > xmax)
      xmax = x[i];
    if (y[i] > ymax)
      ymax = y[i];
  }

  return NS_NewSVGRect(_retval, xmin, ymin, xmax - xmin, ymax - ymin);
}

//----------------------------------------------------------------------
// nsISVGContainerFrame methods:

nsISVGOuterSVGFrame *
nsSVGForeignObjectFrame::GetOuterSVGFrame()
{
  NS_ASSERTION(mParent, "null parent");

  nsISVGContainerFrame *containerFrame;
  mParent->QueryInterface(NS_GET_IID(nsISVGContainerFrame), (void**)&containerFrame);
  if (!containerFrame) {
    NS_ERROR("invalid parent");
    return nsnull;
  }
  
  return containerFrame->GetOuterSVGFrame();  
}

already_AddRefed<nsIDOMSVGMatrix>
nsSVGForeignObjectFrame::GetCanvasTM()
{
  if (!mPropagateTransform) {
    nsIDOMSVGMatrix *retval;
    NS_NewSVGMatrix(&retval);
    return retval;
  }

  if (!mCanvasTM) {
    // get our parent's tm and append local transforms (if any):
    NS_ASSERTION(mParent, "null parent");
    nsISVGContainerFrame *containerFrame;
    mParent->QueryInterface(NS_GET_IID(nsISVGContainerFrame), (void**)&containerFrame);
    if (!containerFrame) {
      NS_ERROR("invalid parent");
      return nsnull;
    }
    nsCOMPtr<nsIDOMSVGMatrix> parentTM = containerFrame->GetCanvasTM();
    NS_ASSERTION(parentTM, "null TM");

    // got the parent tm, now check for local tm:
    nsCOMPtr<nsIDOMSVGMatrix> localTM;
    {
      nsCOMPtr<nsIDOMSVGTransformable> transformable = do_QueryInterface(mContent);
      NS_ASSERTION(transformable, "wrong content element");
      nsCOMPtr<nsIDOMSVGAnimatedTransformList> atl;
      transformable->GetTransform(getter_AddRefs(atl));
      NS_ASSERTION(atl, "null animated transform list");
      nsCOMPtr<nsIDOMSVGTransformList> transforms;
      atl->GetAnimVal(getter_AddRefs(transforms));
      NS_ASSERTION(transforms, "null transform list");
      PRUint32 numberOfItems;
      transforms->GetNumberOfItems(&numberOfItems);
      if (numberOfItems>0)
        transforms->GetConsolidationMatrix(getter_AddRefs(localTM));
    }
    
    if (localTM)
      parentTM->Multiply(localTM, getter_AddRefs(mCanvasTM));
    else
      mCanvasTM = parentTM;
  }

  nsIDOMSVGMatrix* retval = mCanvasTM.get();
  NS_IF_ADDREF(retval);
  return retval;
}

already_AddRefed<nsSVGCoordCtxProvider>
nsSVGForeignObjectFrame::GetCoordContextProvider()
{
  NS_ASSERTION(mParent, "null parent");
  
  nsISVGContainerFrame *containerFrame;
  mParent->QueryInterface(NS_GET_IID(nsISVGContainerFrame), (void**)&containerFrame);
  if (!containerFrame) {
    NS_ERROR("invalid container");
    return nsnull;
  }

  return containerFrame->GetCoordContextProvider();  
}


//----------------------------------------------------------------------
// Implementation helpers

void nsSVGForeignObjectFrame::Update()
{
#ifdef DEBUG
  printf("**nsSVGForeignObjectFrame::Update()\n");
#endif

  mIsDirty = PR_TRUE;

  nsISVGOuterSVGFrame *outerSVGFrame = GetOuterSVGFrame();
  if (!outerSVGFrame) {
    NS_ERROR("null outerSVGFrame");
    return;
  }
  
  PRBool suspended;
  outerSVGFrame->IsRedrawSuspended(&suspended);
  if (!suspended) {
    nsCOMPtr<nsISVGRendererRegion> dirtyRegion = DoReflow();
    if (dirtyRegion) {
      outerSVGFrame->InvalidateRegion(dirtyRegion, PR_TRUE);
    }
  }  
}

already_AddRefed<nsISVGRendererRegion>
nsSVGForeignObjectFrame::DoReflow()
{
#ifdef DEBUG
  printf("**nsSVGForeignObjectFrame::DoReflow()\n");
#endif

  nsPresContext *presContext = GetPresContext();

  // remember the area we have to invalidate after this reflow:
  nsCOMPtr<nsISVGRendererRegion> area_before = GetCoveredRegion();
  NS_ASSERTION(area_before, "could not get covered region");
  
  // initiate a synchronous reflow here and now:  
  nsSize availableSpace(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  nsCOMPtr<nsIRenderingContext> renderingContext;
  nsIPresShell* presShell = presContext->PresShell();
  NS_ASSERTION(presShell, "null presShell");
  presShell->CreateRenderingContext(this,getter_AddRefs(renderingContext));
  
  // XXX we always pass this off as an initial reflow. is that a problem?
  nsHTMLReflowState reflowState(presContext, this, eReflowReason_Initial,
                                renderingContext, availableSpace);

  nsSpaceManager* spaceManager = new nsSpaceManager(presShell, this);
  if (!spaceManager) {
    NS_ERROR("Could not create space manager");
    return nsnull;
  }
  reflowState.mSpaceManager = spaceManager;
   
  nsHTMLReflowMetrics desiredSize(nsnull);
  nsReflowStatus status;
  
  WillReflow(presContext);
  Reflow(presContext, desiredSize, reflowState, status);
  SetSize(nsSize(desiredSize.width, desiredSize.height));
  DidReflow(presContext, &reflowState, NS_FRAME_REFLOW_FINISHED);

  mIsDirty = PR_FALSE;

  nsCOMPtr<nsISVGRendererRegion> area_after = GetCoveredRegion();
  nsISVGRendererRegion *dirtyRegion;
  area_before->Combine(area_after, &dirtyRegion);

  return dirtyRegion;
}

float nsSVGForeignObjectFrame::GetPxPerTwips()
{
  float val = GetTwipsPerPx();
  
  NS_ASSERTION(val!=0.0f, "invalid px/twips");  
  if (val == 0.0) val = 1e-20f;
  
  return 1.0f/val;
}

float nsSVGForeignObjectFrame::GetTwipsPerPx()
{
  return GetPresContext()->ScaledPixelsToTwips();
}

void nsSVGForeignObjectFrame::TransformPoint(float& x, float& y)
{
  nsCOMPtr<nsIDOMSVGMatrix> ctm = GetCanvasTM();
  if (!ctm)
    return;

  // XXX this is absurd! we need to add another method (interface
  // even?) to nsIDOMSVGMatrix to make this easier. (something like
  // nsIDOMSVGMatrix::TransformPoint(float*x,float*y))
  
  nsCOMPtr<nsIDOMSVGPoint> point;
  NS_NewSVGPoint(getter_AddRefs(point), x, y);
  if (!point)
    return;

  nsCOMPtr<nsIDOMSVGPoint> xfpoint;
  point->MatrixTransform(ctm, getter_AddRefs(xfpoint));
  if (!xfpoint)
    return;

  xfpoint->GetX(&x);
  xfpoint->GetY(&y);
}

void nsSVGForeignObjectFrame::TransformVector(float& x, float& y)
{
  // XXX This is crazy. What we really want is
  // nsIDOMSVGMatrix::TransformVector(x,y);
  
  float x0=0.0f, y0=0.0f;
  TransformPoint(x0, y0);
  TransformPoint(x,y);
  x -= x0;
  y -= y0;
}



