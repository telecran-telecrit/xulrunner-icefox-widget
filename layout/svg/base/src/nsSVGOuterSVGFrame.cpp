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

//#include "nsHTMLContainerFrame.h"
#include "nsContainerFrame.h"
#include "nsCSSRendering.h"
#include "nsISVGSVGElement.h"
#include "nsPresContext.h"
#include "nsIDOMSVGAnimatedLength.h"
#include "nsIDOMSVGLength.h"
#include "nsISVGContainerFrame.h"
#include "nsISVGChildFrame.h"
#include "nsISVGOuterSVGFrame.h"
#include "nsISVGRendererCanvas.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsWeakReference.h"
#include "nsISVGValue.h"
#include "nsISVGValueObserver.h"
#include "nsHTMLParts.h"
#include "nsReflowPath.h"
#include "nsISVGRenderer.h"
#include "nsISVGRendererRegion.h"
#include "nsIServiceManager.h"
#include "nsISVGRectangleSink.h"
#include "nsISVGValueUtils.h"
#include "nsIDOMSVGRect.h"
#include "nsIDOMSVGNumber.h"
#include "nsSVGCoordCtxProvider.h"
#if defined(DEBUG) && defined(SVG_DEBUG_PRINTING)
#include "nsIDeviceContext.h"
#include "nsTransform2D.h"
#endif
#include "nsISVGEnum.h"
#include "nsIDOMSVGPoint.h"
#include "nsIDOMSVGZoomAndPan.h"
#include "nsIDOMSVGAnimatedRect.h"
#include "nsIDOMSVGFitToViewBox.h"
#include "nsSVGRect.h"
#include "nsLayoutAtoms.h"
#include "nsIDocument.h"

////////////////////////////////////////////////////////////////////////
// VMRectInvalidator: helper class for invalidating rects on the viewmanager.
// used in nsSVGOuterSVGFrame::InvalidateRegion

class VMRectInvalidator : public nsISVGRectangleSink
{  
protected:
  friend already_AddRefed<nsISVGRectangleSink> CreateVMRectInvalidator(nsIViewManager* vm,
                                                                       nsIView* view,
                                                                       int twipsPerPx);
  VMRectInvalidator(nsIViewManager* vm, nsIView* view, int twipsPerPx); 

public:
  // nsISupports interface:
  NS_DECL_ISUPPORTS

  // nsISVGRectangleSink interface:
  NS_DECL_NSISVGRECTANGLESINK
private:
  nsCOMPtr<nsIViewManager> mViewManager;
  nsIView* mView;
  int mTwipsPerPx;
};

//----------------------------------------------------------------------
// Implementation:

VMRectInvalidator::VMRectInvalidator(nsIViewManager* vm, nsIView* view,
                                     int twipsPerPx)
    : mViewManager(vm), mView(view), mTwipsPerPx(twipsPerPx)
{
}

already_AddRefed<nsISVGRectangleSink>
CreateVMRectInvalidator(nsIViewManager* vm, nsIView* view, int twipsPerPx)
{
  nsISVGRectangleSink* retval = new VMRectInvalidator(vm, view, twipsPerPx);
  NS_IF_ADDREF(retval);
  return retval;
}


//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF(VMRectInvalidator)
NS_IMPL_RELEASE(VMRectInvalidator)

NS_INTERFACE_MAP_BEGIN(VMRectInvalidator)
  NS_INTERFACE_MAP_ENTRY(nsISVGRectangleSink)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
// nsISVGRectangleSink methods:

/* void sinkRectangle (in float x, in float y, in float width, in float height); */
NS_IMETHODIMP
VMRectInvalidator::SinkRectangle(float x, float y, float width, float height)
{
#ifdef DEBUG
  // printf("invalidating %f %f %f %f\n", x,y,width,height);
#endif
  nsRect rect((nscoord)(x*mTwipsPerPx), (nscoord)(y*mTwipsPerPx),
              (nscoord)(width*mTwipsPerPx), (nscoord)(height*mTwipsPerPx));
  mViewManager->UpdateView(mView, rect, NS_VMREFRESH_NO_SYNC);
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// nsSVGOuterSVGFrame class

//typedef nsHTMLContainerFrame nsSVGOuterSVGFrameBase;
typedef nsContainerFrame nsSVGOuterSVGFrameBase;

class nsSVGOuterSVGFrame : public nsSVGOuterSVGFrameBase,
                           public nsISVGOuterSVGFrame,
                           public nsISVGContainerFrame,
                           public nsISVGValueObserver,
                           public nsSupportsWeakReference,
                           public nsSVGCoordCtxProvider
{
  friend nsresult
  NS_NewSVGOuterSVGFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame);
protected:
  nsSVGOuterSVGFrame();
  virtual ~nsSVGOuterSVGFrame();
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

  NS_IMETHOD  DidReflow(nsPresContext*   aPresContext,
                        const nsHTMLReflowState*  aReflowState,
                        nsDidReflowStatus aStatus);


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

  NS_IMETHOD  AttributeChanged(nsIContent*     aChild,
                               PRInt32         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               PRInt32         aModType);

  NS_IMETHOD  GetFrameForPoint(const nsPoint& aPoint, 
                               nsFramePaintLayer aWhichLayer,
                               nsIFrame**     aFrame);

  
  NS_IMETHOD  Paint(nsPresContext* aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect& aDirtyRect,
                    nsFramePaintLayer aWhichLayer,
                    PRUint32 aFlags = 0);

  PRBool CanPaintBackground() { return PR_FALSE; }

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::svgOuterSVGFrame
   */
  virtual nsIAtom* GetType() const;
  virtual PRBool IsFrameOfType(PRUint32 aFlags) const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const
  {
    return MakeFrameName(NS_LITERAL_STRING("SVGOuterSVG"), aResult);
  }
#endif

  // nsISVGValueObserver
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable,
                                     nsISVGValue::modificationType aModType);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable,
                                     nsISVGValue::modificationType aModType);

  // nsISupportsWeakReference
  // implementation inherited from nsSupportsWeakReference
  
  // nsISVGOuterSVGFrame interface:
  NS_IMETHOD InvalidateRegion(nsISVGRendererRegion* region, PRBool bRedraw);
  NS_IMETHOD IsRedrawSuspended(PRBool* isSuspended);
  NS_IMETHOD GetRenderer(nsISVGRenderer**renderer);

  // nsISVGSVGFrame interface:
  NS_IMETHOD SuspendRedraw();
  NS_IMETHOD UnsuspendRedraw();
  NS_IMETHOD NotifyViewportChange();
  
  // nsISVGContainerFrame interface:
  nsISVGOuterSVGFrame*GetOuterSVGFrame();
  already_AddRefed<nsIDOMSVGMatrix> GetCanvasTM();
  already_AddRefed<nsSVGCoordCtxProvider> GetCoordContextProvider();
  
protected:
  // implementation helpers:
  void InitiateReflow();
  
  float GetPxPerTwips();
  float GetTwipsPerPx();

  void AddAsWidthHeightObserver();
  void RemoveAsWidthHeightObserver();

  void CalculateAvailableSpace(nsRect *maxRect, nsRect *preferredRect,
                               nsPresContext* aPresContext,
                               const nsHTMLReflowState& aReflowState);
  
//  nsIView* mView;
  PRUint32 mRedrawSuspendCount;
  PRBool mNeedsReflow;
  PRBool mViewportInitialized;
  nsCOMPtr<nsISVGRenderer> mRenderer;
  nsCOMPtr<nsIDOMSVGMatrix> mCanvasTM;

  // zoom and pan
  nsCOMPtr<nsISVGEnum>      mZoomAndPan;
  nsCOMPtr<nsIDOMSVGPoint>  mCurrentTranslate;
  nsCOMPtr<nsIDOMSVGNumber> mCurrentScale;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGOuterSVGFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame)
{
  *aNewFrame = nsnull;
  
  nsCOMPtr<nsIDOMSVGSVGElement> svgElement = do_QueryInterface(aContent);
  if (!svgElement) {
#ifdef DEBUG
    printf("warning: trying to construct an SVGOuterSVGFrame for a content element that doesn't support the right interfaces\n");
#endif
    return NS_ERROR_FAILURE;
  }

  nsSVGOuterSVGFrame* it = new (aPresShell) nsSVGOuterSVGFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;

  return NS_OK;
}

nsSVGOuterSVGFrame::nsSVGOuterSVGFrame()
    : mRedrawSuspendCount(0),
      mNeedsReflow(PR_FALSE),
      mViewportInitialized(PR_FALSE)
{
}

nsSVGOuterSVGFrame::~nsSVGOuterSVGFrame()
{
#ifdef DEBUG
//  printf("~nsSVGOuterSVGFrame %p\n", this);
#endif

  if (mZoomAndPan)
    NS_REMOVE_SVGVALUE_OBSERVER(mZoomAndPan);

  RemoveAsWidthHeightObserver();
}

nsresult nsSVGOuterSVGFrame::Init()
{
#if (defined(MOZ_SVG_RENDERER_GDIPLUS) + \
     defined(MOZ_SVG_RENDERER_LIBART) + \
     defined(MOZ_SVG_RENDERER_CAIRO) > 1)
#error "Multiple SVG renderers. Please choose one manually."
#elif defined(MOZ_SVG_RENDERER_GDIPLUS)  
  mRenderer = do_CreateInstance(NS_SVG_RENDERER_GDIPLUS_CONTRACTID);
#elif defined(MOZ_SVG_RENDERER_LIBART)
  mRenderer = do_CreateInstance(NS_SVG_RENDERER_LIBART_CONTRACTID);
#elif defined(MOZ_SVG_RENDERER_CAIRO)
  mRenderer = do_CreateInstance(NS_SVG_RENDERER_CAIRO_CONTRACTID);
#else
#error "No SVG renderer."
#endif
  NS_ASSERTION(mRenderer, "could not get renderer");

  // we are an *outer* svg element, so this frame will become the
  // coordinate context for our content element:
  float mmPerPx = GetTwipsPerPx() / TWIPS_PER_POINT_FLOAT / (72.0f * 0.03937f);
  SetCoordCtxMMPerPx(mmPerPx, mmPerPx);
  
  nsCOMPtr<nsISVGSVGElement> SVGElement = do_QueryInterface(mContent);
  NS_ASSERTION(SVGElement, "wrong content element");
  SVGElement->SetParentCoordCtxProvider(this);

  // we only care about our content's zoom and pan values if it's the root element
  nsIDocument* doc = mContent->GetCurrentDoc();
  if (doc && doc->GetRootContent() == mContent) {
    SVGElement->GetZoomAndPanEnum(getter_AddRefs(mZoomAndPan));
    NS_ADD_SVGVALUE_OBSERVER(mZoomAndPan);
    SVGElement->GetCurrentTranslate(getter_AddRefs(mCurrentTranslate));
    SVGElement->GetCurrentScaleNumber(getter_AddRefs(mCurrentScale));
  }

  AddAsWidthHeightObserver();
  SuspendRedraw();
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGOuterSVGFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGContainerFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGOuterSVGFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGSVGFrame)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY(nsSVGCoordCtxProvider)
NS_INTERFACE_MAP_END_INHERITING(nsSVGOuterSVGFrameBase)

//----------------------------------------------------------------------
// nsIFrame methods

NS_IMETHODIMP
nsSVGOuterSVGFrame::Init(nsPresContext*  aPresContext,
                         nsIContent*     aContent,
                         nsIFrame*       aParent,
                         nsStyleContext* aContext,
                         nsIFrame*       aPrevInFlow)
{
  nsresult rv;
  rv = nsSVGOuterSVGFrameBase::Init(aPresContext, aContent, aParent,
                                    aContext, aPrevInFlow);

  Init();

  return rv;
}


  
//----------------------------------------------------------------------
// reflowing

NS_IMETHODIMP
nsSVGOuterSVGFrame::Reflow(nsPresContext*          aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus)
{
#if defined(DEBUG) && defined(SVG_DEBUG_PRINTING)
  {
    printf("nsSVGOuterSVGFrame(%p)::Reflow()[\n",this);
    float twipsPerScPx = aPresContext->ScaledPixelsToTwips();
    float twipsPerPx = aPresContext->PixelsToTwips();
    printf("tw/sc(px)=%f tw/px=%f\n", twipsPerScPx, twipsPerPx);
    printf("]\n");
  }
#endif
  
  // check whether this reflow request is targeted at us or a child
  // frame (e.g. a foreignObject):
  if (aReflowState.reason == eReflowReason_Incremental) {
    nsReflowPath::iterator iter = aReflowState.path->FirstChild();
    nsReflowPath::iterator end = aReflowState.path->EndChildren();

    for ( ; iter != end; ++iter) {
      // The actual target of this reflow is one of our child
      // frames. Since SVG as such doesn't use reflow, this will
      // probably be the child of a <foreignObject>. Some HTML|XUL
      // content frames target reflow events at themselves when they
      // need to be redrawn in response to e.g. a style change. For
      // correct visual updating, we must make sure the reflow
      // reaches its intended target.
        
      // Since it is an svg frame (probably an nsSVGForeignObjectFrame),
      // we might as well pass in our aDesiredSize and aReflowState
      // objects - they are ignored by svg frames:
      nsSize availSpace(0, 0); // XXXwaterson probably wrong!
      nsHTMLReflowState state(aPresContext, aReflowState, *iter, availSpace);
      (*iter)->Reflow (aPresContext,
                       aDesiredSize,
                       state,
                       aStatus);

      // XXX do we really have to return our metrics although we're
      // not affected by the reflow? Is there a way of telling our
      // parent that we don't want anything changed?
      aDesiredSize.width  = mRect.width;
      aDesiredSize.height = mRect.height;
      aDesiredSize.ascent = aDesiredSize.height;
      aDesiredSize.descent = 0;
    }

    if (! aReflowState.path->mReflowCommand) {
      // We're not the target of the incremental reflow, so just bail.
      aStatus = NS_FRAME_COMPLETE;
      return NS_OK;
    }
  }
  
  //  SVG CR 20001102: When the SVG content is embedded inline within
  //  a containing document, and that document is styled using CSS,
  //  then if there are CSS positioning properties specified on the
  //  outermost 'svg' element that are sufficient to establish the
  //  width of the viewport, then these positioning properties
  //  establish the viewport's width; otherwise, the width attribute
  //  on the outermost 'svg' element establishes the viewport's width.
  //  Similarly, if there are CSS positioning properties specified on
  //  the outermost 'svg' element that are sufficient to establish the
  //  height of the viewport, then these positioning properties
  //  establish the viewport's height; otherwise, the height attribute
  //  on the outermost 'svg' element establishes the viewport's
  //  height.
#ifdef DEBUG
  // printf("--- nsSVGOuterSVGFrame(%p)::Reflow(frame:%p,reason:%d) ---\n",this,aReflowState.frame,aReflowState.reason);
#endif
  
  nsCOMPtr<nsISVGSVGElement> SVGElement = do_QueryInterface(mContent);
  NS_ENSURE_TRUE(SVGElement, NS_ERROR_FAILURE);

  float pxPerTwips = GetPxPerTwips();
  float twipsPerPx = GetTwipsPerPx();

  // The width/height attribs given on the <svg>-element might be
  // percentage values of the parent viewport. We will set the parent
  // coordinate context dimensions to the available space.

  nsRect maxRect, preferredRect;
  CalculateAvailableSpace(&maxRect, &preferredRect, aPresContext, aReflowState);
  float preferredWidth = preferredRect.width * pxPerTwips;
  float preferredHeight = preferredRect.height * pxPerTwips;

  SuspendRedraw(); 
  
  // As soon as we set the coordinate context, the width/height
  // attributes might emit change-notifications. We don't want those
  // right now:
  RemoveAsWidthHeightObserver();

  nsCOMPtr<nsIDOMSVGRect> r;
  NS_NewSVGRect(getter_AddRefs(r), 0, 0, preferredWidth, preferredHeight);
  SetCoordCtxRect(r);
  
#ifdef DEBUG
  // some debug stuff:
//   {
//     nsRect r=aPresContext->GetVisibleArea();
//     printf("******* aw: %d, ah: %d visiw: %d, visih: %d\n",
//            aReflowState.availableWidth,
//            aReflowState.availableHeight,
//            r.width, r.height);
//     printf("******* cw: %d, ch: %d \n    cmaxw: %d, cmaxh: %d\n",
//            aReflowState.mComputedWidth,
//            aReflowState.mComputedHeight,
//            aReflowState.mComputedMaxWidth,
//            aReflowState.mComputedMaxHeight);

//     if (aReflowState.parentReflowState) {
//       printf("******* parent aw: %d, parent ah: %d \n",
//              aReflowState.parentReflowState->availableWidth,
//              aReflowState.parentReflowState->availableHeight);
//       printf("******* parent cw: %d, parent ch: %d \n  parent cmaxw: %d, parent cmaxh: %d\n",
//              aReflowState.parentReflowState->mComputedWidth,
//              aReflowState.parentReflowState->mComputedHeight,
//              aReflowState.parentReflowState->mComputedMaxWidth,
//              aReflowState.parentReflowState->mComputedMaxHeight);
//     }
//   }
#endif

  // now that the parent coord ctx dimensions have been set, the
  // width/height attributes will be valid.
  // Let's work out our desired dimensions.

  float width;
  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> animLength;
    SVGElement->GetWidth(getter_AddRefs(animLength));
    NS_ENSURE_TRUE(animLength, NS_ERROR_FAILURE);
    nsCOMPtr<nsIDOMSVGLength> length;
    animLength->GetAnimVal(getter_AddRefs(length));
    NS_ENSURE_TRUE(length, NS_ERROR_FAILURE);
    
    length->GetValue(&width);
    
    aDesiredSize.width = (int)(width*twipsPerPx);
  }

  float height;
  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> animLength;
    SVGElement->GetHeight(getter_AddRefs(animLength));
    NS_ENSURE_TRUE(animLength, NS_ERROR_FAILURE);
    nsCOMPtr<nsIDOMSVGLength> length;
    animLength->GetAnimVal(getter_AddRefs(length));
    NS_ENSURE_TRUE(length, NS_ERROR_FAILURE);
    
    length->GetValue(&height);
    
    aDesiredSize.height = (int)(height*twipsPerPx);
  }
  

  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;
  
  // XXX add in CSS borders ??

  aStatus = NS_FRAME_COMPLETE;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);

  AddAsWidthHeightObserver();
  
  UnsuspendRedraw();
  
  return NS_OK;
}

NS_IMETHODIMP
nsSVGOuterSVGFrame::DidReflow(nsPresContext*   aPresContext,
                              const nsHTMLReflowState*  aReflowState,
                              nsDidReflowStatus aStatus)
{
  nsresult rv = nsSVGOuterSVGFrameBase::DidReflow(aPresContext,aReflowState,aStatus);

  if (!mViewportInitialized) {
    // it is now
    mViewportInitialized = PR_TRUE;

    // call InitialUpdate() on all frames:
    nsIFrame* kid = mFrames.FirstChild();
    while (kid) {
      nsISVGChildFrame* SVGFrame=nsnull;
      kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
      if (SVGFrame) {
        SVGFrame->InitialUpdate(); 
      }
      kid = kid->GetNextSibling();
    }
    
    UnsuspendRedraw();
  }
  
  return rv;
}

//----------------------------------------------------------------------
// container methods

NS_IMETHODIMP
nsSVGOuterSVGFrame::AppendFrames(nsIAtom*        aListName,
                                 nsIFrame*       aFrameList)
{
  // append == insert at end:
  return InsertFrames(aListName, mFrames.LastChild(), aFrameList);  
}

NS_IMETHODIMP
nsSVGOuterSVGFrame::InsertFrames(nsIAtom*        aListName,
                                 nsIFrame*       aPrevFrame,
                                 nsIFrame*       aFrameList)
{
  // memorize last new frame
  nsIFrame* lastNewFrame = nsnull;
  {
    nsFrameList tmpList(aFrameList);
    lastNewFrame = tmpList.LastChild();
  }
  
  // Insert the new frames
  mFrames.InsertFrames(this, aPrevFrame, aFrameList);

  SuspendRedraw();

  // call InitialUpdate() on all new frames:
  
  nsIFrame* end = nsnull;
  if (lastNewFrame)
    end = lastNewFrame->GetNextSibling();

  for (nsIFrame* kid = aFrameList; kid != end;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame=nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->InitialUpdate(); 
    }
  }

  UnsuspendRedraw();
  
  return NS_OK;
}

NS_IMETHODIMP
nsSVGOuterSVGFrame::RemoveFrame(nsIAtom*        aListName,
                                nsIFrame*       aOldFrame)
{
  nsCOMPtr<nsISVGRendererRegion> dirty_region;
  
  nsISVGChildFrame* SVGFrame=nsnull;
  aOldFrame->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);

  if (SVGFrame)
    dirty_region = SVGFrame->GetCoveredRegion();

  PRBool result = mFrames.DestroyFrame(GetPresContext(), aOldFrame);

  nsISVGOuterSVGFrame* outerSVGFrame = GetOuterSVGFrame();
  NS_ASSERTION(outerSVGFrame, "no outer svg frame");
  if (dirty_region && outerSVGFrame)
    outerSVGFrame->InvalidateRegion(dirty_region, PR_TRUE);

  NS_ASSERTION(result, "didn't find frame to delete");
  return result ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSVGOuterSVGFrame::ReplaceFrame(nsIAtom*        aListName,
                                 nsIFrame*       aOldFrame,
                                 nsIFrame*       aNewFrame)
{
  NS_NOTYETIMPLEMENTED("nsSVGOuterSVGFrame::ReplaceFrame");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSVGOuterSVGFrame::AttributeChanged(nsIContent*     aChild,
                                     PRInt32         aNameSpaceID,
                                     nsIAtom*        aAttribute,
                                     PRInt32         aModType)
{
#ifdef DEBUG
//  {
//    nsAutoString str;
//    aAttribute->ToString(str);
//    printf("** nsSVGOuterSVGFrame::AttributeChanged(%s)\n",
//           NS_LossyConvertUCS2toASCII(str).get());
//  }
#endif
  return NS_OK;
}


nsresult
nsSVGOuterSVGFrame::GetFrameForPoint(const nsPoint& aPoint,
                                     nsFramePaintLayer aWhichLayer,
                                     nsIFrame**     aFrame)
{
  // XXX This algorithm is really bad. Because we only have a
  // singly-linked list we have to test each and every SVG element for
  // a hit. What we really want is a double-linked list.
  

  *aFrame = nsnull;
  if (aWhichLayer != NS_FRAME_PAINT_LAYER_FOREGROUND) return NS_ERROR_FAILURE;
  
  float x = GetPxPerTwips() * ( aPoint.x - mRect.x);
  float y = GetPxPerTwips() * ( aPoint.y - mRect.y);
  
  PRBool inThisFrame = mRect.Contains(aPoint);
  
  if (!inThisFrame || !mRenderer) {
    return NS_ERROR_FAILURE;
  }

  *aFrame = this;
  nsIFrame* hit = nsnull;
  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame=nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      nsresult rv = SVGFrame->GetFrameForPointSVG(x, y, &hit);
      if (NS_SUCCEEDED(rv) && hit) {
        *aFrame = hit;
        // return NS_OK; can't return. we need reverse order but only
        // have a singly linked list...
      }
    }
  }
    
  return NS_OK;
}



//----------------------------------------------------------------------
// painting

NS_IMETHODIMP
nsSVGOuterSVGFrame::Paint(nsPresContext* aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect& aDirtyRect,
                          nsFramePaintLayer aWhichLayer,
                          PRUint32 aFlags)
{
  
//    if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
//      if (GetStyleDisplay()->IsVisible() && mRect.width && mRect.height) {
//        // Paint our background and border
//        const nsStyleBorder* border = GetStyleBorder();
//        const nsStylePadding* padding = GetStylePadding();
//        const nsStyleOutline* outline = GetStyleOutline();

//        nsRect  rect(0, 0, mRect.width, mRect.height);
// //       nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
// //                                       aDirtyRect, rect, *border, *padding, PR_TRUE);
//        nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
//                                    aDirtyRect, rect, *border, mStyleContext, 0);
//        nsCSSRendering::PaintOutline(aPresContext, aRenderingContext, this,
//                                    aDirtyRect, rect, *border, *outline, mStyleContext, 0);
      
//      }
//    }

  if (aWhichLayer != NS_FRAME_PAINT_LAYER_FOREGROUND) return NS_OK;
  if (aDirtyRect.width<=0 || aDirtyRect.height<=0) return NS_OK;

#if defined(DEBUG) && defined(SVG_DEBUG_PRINTING)
  {
    nsCOMPtr<nsIDeviceContext>  dx;
    aRenderingContext.GetDeviceContext(*getter_AddRefs(dx));
    float zoom,tzoom,scale;
    dx->GetZoom(zoom);
    dx->GetTextZoom(tzoom);
    dx->GetCanonicalPixelScale(scale);
    printf("nsSVGOuterSVGFrame(%p)::Paint()[ z=%f tz=%f ps=%f\n",this,zoom,tzoom,scale);
    printf("dirtyrect= %d, %d, %d, %d\n", aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height);
    nsTransform2D* xform;
    aRenderingContext.GetCurrentTransform(xform);
    printf("translation=(%f,%f)\n", xform->GetXTranslation(), xform->GetYTranslation());
    float sx=1.0f,sy=1.0f;
    xform->TransformNoXLate(&sx,&sy);
    printf("scale=(%f,%f)\n", sx, sy);
    float twipsPerScPx = aPresContext->ScaledPixelsToTwips();
    float twipsPerPx = aPresContext->PixelsToTwips();
    printf("tw/sc(px)=%f tw/px=%f\n", twipsPerScPx, twipsPerPx);
    int fontsc;
    aPresContext->GetFontScaler(&fontsc);
    printf("font scale=%d\n",fontsc);
    printf("]\n");
  }
#endif
  
  // initialize Mozilla rendering context
  aRenderingContext.PushState();
  
  aRenderingContext.SetClipRect(aDirtyRect, nsClipCombine_kIntersect);

#if defined(DEBUG) && defined(SVG_DEBUG_PAINT_TIMING)
  PRTime start = PR_Now();
#endif

  float pxPerTwips = GetPxPerTwips();
  int x0 = (int)(aDirtyRect.x*pxPerTwips);
  int y0 = (int)(aDirtyRect.y*pxPerTwips);
  int x1 = (int)ceil((aDirtyRect.x+aDirtyRect.width)*pxPerTwips);
  int y1 = (int)ceil((aDirtyRect.y+aDirtyRect.height)*pxPerTwips);
  NS_ASSERTION(x0>=0 && y0>=0, "unexpected negative coordinates");
  NS_ASSERTION(x1-x0>0 && y1-y0>0, "zero sized dirtyRect");
  nsRect dirtyRectPx(x0, y0, x1-x0, y1-y0);

  // If we don't have a renderer due to the component failing
  // to load (gdi+ or cairo not available), indicate to the user
  // what's going on by drawing a red "X" at the appropriate spot.
  if (!mRenderer) {
    aRenderingContext.SetColor(NS_RGB(255,0,0));
    aRenderingContext.DrawLine(mRect.x, mRect.y,
                               mRect.x + mRect.width, mRect.y + mRect.height);
    aRenderingContext.DrawLine(mRect.x + mRect.width, mRect.y,
                               mRect.x, mRect.y + mRect.height);
    aRenderingContext.PopState();
    return NS_OK;
  }

  nsCOMPtr<nsISVGRendererCanvas> canvas;
  mRenderer->CreateCanvas(&aRenderingContext, aPresContext, dirtyRectPx,
                          getter_AddRefs(canvas));

  // paint children:
  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame=nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame)
      SVGFrame->PaintSVG(canvas, aDirtyRect);
  }
  
  canvas->Flush();

  canvas = nsnull;

#if defined(DEBUG) && defined(SVG_DEBUG_PAINT_TIMING)
  PRTime end = PR_Now();
  printf("SVG Paint Timing: %f ms\n", (end-start)/1000.0);
#endif
  
  aRenderingContext.PopState();
  
  return NS_OK;
  // see if we have to draw a selection frame around this container
  //return nsFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
}

nsIAtom *
nsSVGOuterSVGFrame::GetType() const
{
  return nsLayoutAtoms::svgOuterSVGFrame;
}

PRBool
nsSVGOuterSVGFrame::IsFrameOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~nsIFrame::eSVG);
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods:

NS_IMETHODIMP
nsSVGOuterSVGFrame::WillModifySVGObservable(nsISVGValue* observable,
                                            nsISVGValue::modificationType aModType)
{
  return NS_OK;
}


NS_IMETHODIMP
nsSVGOuterSVGFrame::DidModifySVGObservable(nsISVGValue* observable,
                                           nsISVGValue::modificationType aModType)
{
  mNeedsReflow = PR_TRUE;
  if (mRedrawSuspendCount==0) {
    InitiateReflow();
  }
  
  return NS_OK;
}


//----------------------------------------------------------------------
// nsISVGOuterSVGFrame methods:

NS_IMETHODIMP
nsSVGOuterSVGFrame::InvalidateRegion(nsISVGRendererRegion* region, PRBool bRedraw)
{
//  NS_ASSERTION(mView, "need a view!");
//  if (!mView) return NS_ERROR_FAILURE;
  
  if (!region && !bRedraw) return NS_OK;

  // just ignore invalidates if painting is suppressed by the shell
  PRBool suppressed = PR_FALSE;
  GetPresContext()->PresShell()->IsPaintingSuppressed(&suppressed);
  if (suppressed) return NS_OK;
  
  nsIView* view = GetClosestView();
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);

  nsIViewManager* vm = view->GetViewManager();

  vm->BeginUpdateViewBatch();
  if (region) {
    nsCOMPtr<nsISVGRectangleSink> sink = CreateVMRectInvalidator(vm, view,
                                                                 (int)(GetTwipsPerPx()+0.5f));
    NS_ASSERTION(sink, "could not create rectangle sink for viewmanager");
    if (sink)
      region->GetRectangleScans(sink);
  }
  vm->EndUpdateViewBatch(NS_VMREFRESH_NO_SYNC);
  
  return NS_OK;
}

NS_IMETHODIMP
nsSVGOuterSVGFrame::IsRedrawSuspended(PRBool* isSuspended)
{
  *isSuspended = (mRedrawSuspendCount>0) || !mViewportInitialized;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGOuterSVGFrame::GetRenderer(nsISVGRenderer**renderer)
{
  *renderer = mRenderer;
  NS_IF_ADDREF(*renderer);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGSVGFrame methods:


NS_IMETHODIMP
nsSVGOuterSVGFrame::SuspendRedraw()
{
  if (!mRenderer)
    return NS_OK;

#ifdef DEBUG
  //printf("suspend redraw (count=%d)\n", mRedrawSuspendCount);
#endif
  if (++mRedrawSuspendCount != 1)
    return NS_OK;

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame=nsnull;
    CallQueryInterface(kid, &SVGFrame);
    if (SVGFrame) {
      SVGFrame->NotifyRedrawSuspended();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGOuterSVGFrame::UnsuspendRedraw()
{
  if (!mRenderer)
    return NS_OK;

#ifdef DEBUG
//  printf("unsuspend redraw (count=%d)\n", mRedrawSuspendCount);
#endif
  if (--mRedrawSuspendCount > 0)
    return NS_OK;
  
  NS_ASSERTION(mRedrawSuspendCount >=0, "unbalanced suspend count!");
  
  // If we need to reflow, do so before we update any of our
  // children. Reflows are likely to affect the display of children:
  if (mNeedsReflow)
    InitiateReflow();
  
  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame=nsnull;
    CallQueryInterface(kid, &SVGFrame);
    if (SVGFrame) {
      SVGFrame->NotifyRedrawUnsuspended();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSVGOuterSVGFrame::NotifyViewportChange()
{
  if (!mRenderer)
    return NS_OK;

  // no point in doing anything when were not init'ed yet:
  if (!mViewportInitialized) return NS_OK;

  // make sure canvas transform matrix gets (lazily) recalculated:
  mCanvasTM = nsnull;
  
  // inform children
  SuspendRedraw();
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGChildFrame* SVGFrame=nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame)
      SVGFrame->NotifyCanvasTMChanged(); 
    kid = kid->GetNextSibling();
  }
  UnsuspendRedraw();
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGContainerFrame methods:

nsISVGOuterSVGFrame *
nsSVGOuterSVGFrame::GetOuterSVGFrame()
{
  return this;
}

already_AddRefed<nsIDOMSVGMatrix>
nsSVGOuterSVGFrame::GetCanvasTM()
{
  if (!mCanvasTM) {
    nsCOMPtr<nsIDOMSVGSVGElement> svgElement = do_QueryInterface(mContent);
    NS_ASSERTION(svgElement, "wrong content element");
    svgElement->GetViewboxToViewportTransform(getter_AddRefs(mCanvasTM));

    if (mZoomAndPan) {
      // our content is the document element so we must premultiply the values
      // of it's currentScale and currentTranslate properties
      PRUint16 val;
      mZoomAndPan->GetIntegerValue(val);
      if (val == nsIDOMSVGZoomAndPan::SVG_ZOOMANDPAN_MAGNIFY) {
        nsCOMPtr<nsIDOMSVGMatrix> zoomPanMatrix;
        nsCOMPtr<nsIDOMSVGMatrix> temp;
        float scale, x, y;
        mCurrentScale->GetValue(&scale);
        mCurrentTranslate->GetX(&x);
        mCurrentTranslate->GetY(&y);
        svgElement->CreateSVGMatrix(getter_AddRefs(zoomPanMatrix));
        zoomPanMatrix->Translate(x, y, getter_AddRefs(temp));
        temp->Scale(scale, getter_AddRefs(zoomPanMatrix));
        zoomPanMatrix->Multiply(mCanvasTM, getter_AddRefs(temp));
        temp.swap(mCanvasTM);
      }
    }
  }
  nsIDOMSVGMatrix* retval = mCanvasTM.get();
  NS_IF_ADDREF(retval);
  return retval;
}

already_AddRefed<nsSVGCoordCtxProvider>
nsSVGOuterSVGFrame::GetCoordContextProvider()
{
  NS_ASSERTION(mContent, "null parent");

  // Our <svg> content element is the CoordContextProvider for our children:
  nsSVGCoordCtxProvider *provider;
  CallQueryInterface(mContent, &provider);

  return provider;  
}

//----------------------------------------------------------------------
// Implementation helpers

float nsSVGOuterSVGFrame::GetPxPerTwips()
{
  float val = GetTwipsPerPx();
  
  NS_ASSERTION(val!=0.0f, "invalid px/twips");  
  if (val == 0.0) val = 1e-20f;
  
  return 1.0f/val;
}

float nsSVGOuterSVGFrame::GetTwipsPerPx()
{
  return GetPresContext()->ScaledPixelsToTwips();
}

void nsSVGOuterSVGFrame::InitiateReflow()
{
  mNeedsReflow = PR_FALSE;
  
  // Generate a reflow command to reflow ourselves
  nsIPresShell* presShell = GetPresContext()->PresShell();
  presShell->AppendReflowCommand(this, eReflowType_ReflowDirty, nsnull);
  // XXXbz why is this synchronously flushing reflows, exactly?  If it
  // needs to, why is it not using the presshell's reflow batching
  // instead of hacking its own?
  presShell->FlushPendingNotifications(Flush_OnlyReflow);  
}


void nsSVGOuterSVGFrame::AddAsWidthHeightObserver()
{
  nsCOMPtr<nsIDOMSVGSVGElement> svgElement = do_QueryInterface(mContent);
  NS_ASSERTION(svgElement, "wrong content element");  
 
  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> animLength;
    svgElement->GetWidth(getter_AddRefs(animLength));
    NS_ASSERTION(animLength, "could not get <svg>:width");
    nsCOMPtr<nsIDOMSVGLength> length;
    animLength->GetAnimVal(getter_AddRefs(length));
    NS_ASSERTION(length, "could not get <svg>:width:animval");
    NS_ADD_SVGVALUE_OBSERVER(length);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> animLength;
    svgElement->GetHeight(getter_AddRefs(animLength));
    NS_ASSERTION(animLength, "could not get <svg>:height");
    nsCOMPtr<nsIDOMSVGLength> length;
    animLength->GetAnimVal(getter_AddRefs(length));
    NS_ASSERTION(length, "could not get <svg>:height:animval");
    NS_ADD_SVGVALUE_OBSERVER(length);
  }  
}

void nsSVGOuterSVGFrame::RemoveAsWidthHeightObserver()
{
  nsCOMPtr<nsIDOMSVGSVGElement> svgElement = do_QueryInterface(mContent);
  NS_ASSERTION(svgElement, "wrong content element");  

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> animLength;
    svgElement->GetWidth(getter_AddRefs(animLength));
    NS_ASSERTION(animLength, "could not get <svg>:width");
    nsCOMPtr<nsIDOMSVGLength> length;
    animLength->GetAnimVal(getter_AddRefs(length));
    NS_ASSERTION(length, "could not get <svg>:width:animval");
    NS_REMOVE_SVGVALUE_OBSERVER(length);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> animLength;
    svgElement->GetHeight(getter_AddRefs(animLength));
    NS_ASSERTION(animLength, "could not get <svg>:height");
    nsCOMPtr<nsIDOMSVGLength> length;
    animLength->GetAnimVal(getter_AddRefs(length));
    NS_ASSERTION(length, "could not get <svg>:height:animval");
    NS_REMOVE_SVGVALUE_OBSERVER(length);
  }  
}

void
nsSVGOuterSVGFrame::CalculateAvailableSpace(nsRect *maxRect,
                                            nsRect *preferredRect,
                                            nsPresContext* aPresContext,
                                            const nsHTMLReflowState& aReflowState)
{
  *preferredRect = aPresContext->GetVisibleArea();
  
  if (aReflowState.availableWidth != NS_INTRINSICSIZE)
    maxRect->width = aReflowState.availableWidth;
  else if (aReflowState.parentReflowState &&
           aReflowState.parentReflowState->mComputedWidth != NS_INTRINSICSIZE)
    maxRect->width = aReflowState.parentReflowState->mComputedWidth;
  else
    maxRect->width = NS_MAXSIZE;
  
  if (aReflowState.availableHeight != NS_INTRINSICSIZE)
    maxRect->height = aReflowState.availableHeight;    
  else if (aReflowState.parentReflowState &&
           aReflowState.parentReflowState->mComputedHeight != NS_INTRINSICSIZE)
    maxRect->height = aReflowState.parentReflowState->mComputedHeight;
  else
    maxRect->height = NS_MAXSIZE;

  if (preferredRect->width > maxRect->width)
    preferredRect->width = maxRect->width;
  if (preferredRect->height > maxRect->height)
    preferredRect->height = maxRect->height;
}  
