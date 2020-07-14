/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsCOMPtr.h"
#include "nsFrame.h"
#include "nsFrameList.h"
#include "nsLineLayout.h"
#include "nsIContent.h"
#include "nsContentUtils.h"
#include "nsIAtom.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsStyleContext.h"
#include "nsReflowPath.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIScrollableFrame.h"
#include "nsPresContext.h"
#include "nsCRT.h"
#include "nsGUIEvent.h"
#include "nsIDOMEvent.h"
#include "nsPLDOMEvent.h"
#include "nsStyleConsts.h"
#include "nsIPresShell.h"
#include "prlog.h"
#include "prprf.h"
#include <stdarg.h>
#include "nsFrameManager.h"
#include "nsCSSRendering.h"
#include "nsLayoutUtils.h"
#ifdef ACCESSIBILITY
#include "nsIAccessible.h"
#endif

#include "nsIDOMText.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMHTMLHRElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDeviceContext.h"
#include "nsIEditorDocShell.h"
#include "nsIEventStateManager.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsIFrameSelection.h"
#include "nsHTMLParts.h"
#include "nsLayoutAtoms.h"
#include "nsCSSAnonBoxes.h"
#include "nsCSSPseudoElements.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLContentSink.h" 
#include "nsCSSFrameConstructor.h"

#include "nsFrameTraversal.h"
#include "nsStyleChangeList.h"
#include "nsIDOMRange.h"
#include "nsITableLayout.h"    //selection neccesity
#include "nsITableCellLayout.h"//  "
#include "nsITextControlFrame.h"
#include "nsINameSpaceManager.h"
#include "nsIPercentHeightObserver.h"
#include "nsTextTransformer.h"

// For triple-click pref
#include "nsIServiceManager.h"
#include "nsISelectionImageService.h"
#include "imgIContainer.h"
#include "imgIRequest.h"
#include "gfxIImageFrame.h"
#include "nsILookAndFeel.h"
#include "nsLayoutCID.h"
#include "nsWidgetsCID.h"     // for NS_LOOKANDFEEL_CID
#include "nsUnicharUtils.h"
#include "nsLayoutErrors.h"
#include "nsHTMLContainerFrame.h"
#include "nsBoxLayoutState.h"

static NS_DEFINE_CID(kSelectionImageService, NS_SELECTIONIMAGESERVICE_CID);
static NS_DEFINE_CID(kLookAndFeelCID,  NS_LOOKANDFEEL_CID);
static NS_DEFINE_CID(kWidgetCID, NS_CHILD_CID);

// Struct containing cached metrics for box-wrapped frames.
struct nsBoxLayoutMetrics
{
  nsSize mPrefSize;
  nsSize mMinSize;
  nsSize mMaxSize;

  nsSize mBlockMinSize;
  nsSize mBlockPrefSize;
  nscoord mBlockAscent;

  nscoord mFlex;
  nscoord mAscent;

  nsSize mLastSize;
  nsSize mOverflow;

  PRPackedBool mIncludeOverflow;
  PRPackedBool mWasCollapsed;
  PRPackedBool mStyleChange;
};

// Some Misc #defines
#define SELECTION_DEBUG        0
#define FORCE_SELECTION_UPDATE 1
#define CALC_DEBUG             0


#include "nsICaret.h"
#include "nsILineIterator.h"

//non Hack prototypes
#if 0
static void RefreshContentFrames(nsPresContext* aPresContext, nsIContent * aStartContent, nsIContent * aEndContent);
#endif

#include "prenv.h"

// start nsIFrameDebug

#ifdef NS_DEBUG
static PRBool gShowFrameBorders = PR_FALSE;

void nsIFrameDebug::ShowFrameBorders(PRBool aEnable)
{
  gShowFrameBorders = aEnable;
}

PRBool nsIFrameDebug::GetShowFrameBorders()
{
  return gShowFrameBorders;
}

static PRBool gShowEventTargetFrameBorder = PR_FALSE;

void nsIFrameDebug::ShowEventTargetFrameBorder(PRBool aEnable)
{
  gShowEventTargetFrameBorder = aEnable;
}

PRBool nsIFrameDebug::GetShowEventTargetFrameBorder()
{
  return gShowEventTargetFrameBorder;
}

/**
 * Note: the log module is created during library initialization which
 * means that you cannot perform logging before then.
 */
static PRLogModuleInfo* gLogModule;

static PRLogModuleInfo* gFrameVerifyTreeLogModuleInfo;

static PRBool gFrameVerifyTreeEnable = PRBool(0x55);

PRBool
nsIFrameDebug::GetVerifyTreeEnable()
{
  if (gFrameVerifyTreeEnable == PRBool(0x55)) {
    if (nsnull == gFrameVerifyTreeLogModuleInfo) {
      gFrameVerifyTreeLogModuleInfo = PR_NewLogModule("frameverifytree");
      gFrameVerifyTreeEnable = 0 != gFrameVerifyTreeLogModuleInfo->level;
      printf("Note: frameverifytree is %sabled\n",
             gFrameVerifyTreeEnable ? "en" : "dis");
    }
  }
  return gFrameVerifyTreeEnable;
}

void
nsIFrameDebug::SetVerifyTreeEnable(PRBool aEnabled)
{
  gFrameVerifyTreeEnable = aEnabled;
}

static PRLogModuleInfo* gStyleVerifyTreeLogModuleInfo;

static PRBool gStyleVerifyTreeEnable = PRBool(0x55);

PRBool
nsIFrameDebug::GetVerifyStyleTreeEnable()
{
  if (gStyleVerifyTreeEnable == PRBool(0x55)) {
    if (nsnull == gStyleVerifyTreeLogModuleInfo) {
      gStyleVerifyTreeLogModuleInfo = PR_NewLogModule("styleverifytree");
      gStyleVerifyTreeEnable = 0 != gStyleVerifyTreeLogModuleInfo->level;
      printf("Note: styleverifytree is %sabled\n",
             gStyleVerifyTreeEnable ? "en" : "dis");
    }
  }
  return gStyleVerifyTreeEnable;
}

void
nsIFrameDebug::SetVerifyStyleTreeEnable(PRBool aEnabled)
{
  gStyleVerifyTreeEnable = aEnabled;
}

PRLogModuleInfo*
nsIFrameDebug::GetLogModuleInfo()
{
  if (nsnull == gLogModule) {
    gLogModule = PR_NewLogModule("frame");
  }
  return gLogModule;
}

void
nsIFrameDebug::RootFrameList(nsPresContext* aPresContext, FILE* out, PRInt32 aIndent)
{
  if((nsnull == aPresContext) || (nsnull == out))
    return;

  nsIPresShell *shell = aPresContext->GetPresShell();
  if (nsnull != shell) {
    nsIFrame* frame = shell->FrameManager()->GetRootFrame();
    if(nsnull != frame) {
      nsIFrameDebug* debugFrame;
      nsresult rv;
      rv = frame->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&debugFrame);
      if(NS_SUCCEEDED(rv))
        debugFrame->List(aPresContext, out, aIndent);
    }
  }
}
#endif
// end nsIFrameDebug


// frame image selection drawing service implementation
class SelectionImageService : public nsISelectionImageService
{
public:
  SelectionImageService();
  virtual ~SelectionImageService();
  NS_DECL_ISUPPORTS
  NS_DECL_NSISELECTIONIMAGESERVICE
private:
  nsresult CreateImage(nscolor aImageColor, imgIContainer *aContainer);
  nsCOMPtr<imgIContainer> mContainer;
  nsCOMPtr<imgIContainer> mDisabledContainer;
};

NS_IMPL_ISUPPORTS1(SelectionImageService, nsISelectionImageService)

SelectionImageService::SelectionImageService()
{
}

SelectionImageService::~SelectionImageService()
{
}

NS_IMETHODIMP
SelectionImageService::GetImage(PRInt16 aSelectionValue, imgIContainer **aContainer)
{
  *aContainer = nsnull;

  nsCOMPtr<imgIContainer>* container = &mContainer;
  nsILookAndFeel::nsColorID colorID;
  if (aSelectionValue == nsISelectionController::SELECTION_ON) {
    colorID = nsILookAndFeel::eColor_TextSelectBackground;
  } else if (aSelectionValue == nsISelectionController::SELECTION_ATTENTION) {
    colorID = nsILookAndFeel::eColor_TextSelectBackgroundAttention;
  } else {
    container = &mDisabledContainer;
    colorID = nsILookAndFeel::eColor_TextSelectBackgroundDisabled;
  }

  if (!*container) {
    nsresult result;
    *container = do_CreateInstance("@mozilla.org/image/container;1", &result);
    if (NS_FAILED(result))
      return result;

    nscolor color = NS_RGB(255, 255, 255);
    nsCOMPtr<nsILookAndFeel> look = do_GetService(kLookAndFeelCID);
    if (look)
      look->GetColor(colorID, color);
    CreateImage(color, *container);
  }

  *aContainer = *container; 
  NS_ADDREF(*aContainer);
  return NS_OK;
}

NS_IMETHODIMP
SelectionImageService::Reset()
{
  mContainer = 0;
  mDisabledContainer = 0;
  return NS_OK;
}

#define SEL_IMAGE_WIDTH 32
#define SEL_IMAGE_HEIGHT 32
#define SEL_ALPHA_AMOUNT 128

nsresult
SelectionImageService::CreateImage(nscolor aImageColor, imgIContainer *aContainer)
{
  if (aContainer)
  {
    nsresult result = aContainer->Init(SEL_IMAGE_WIDTH,SEL_IMAGE_HEIGHT,nsnull);
    if (NS_SUCCEEDED(result))
    {
      nsCOMPtr<gfxIImageFrame> image = do_CreateInstance("@mozilla.org/gfx/image/frame;2",&result);
      if (NS_SUCCEEDED(result) && image)
      {
        image->Init(0, 0, SEL_IMAGE_WIDTH, SEL_IMAGE_HEIGHT, gfxIFormats::RGB_A8, 24);
        aContainer->AppendFrame(image);

        PRUint32 bpr, abpr;
        image->GetImageBytesPerRow(&bpr);
        image->GetAlphaBytesPerRow(&abpr);

        //its better to temporarily go after heap than put big data on stack
        unsigned char *row_data = (unsigned char *)malloc(bpr);
        if (!row_data)
          return NS_ERROR_OUT_OF_MEMORY;
        unsigned char *alpha = (unsigned char *)malloc(abpr);
        if (!alpha)
        {
          free (row_data);
          return NS_ERROR_OUT_OF_MEMORY;
        }
        unsigned char *data = row_data;

        PRInt16 i;
        for (i = 0; i < SEL_IMAGE_WIDTH; i++)
        {
#if defined(XP_WIN) || defined(XP_OS2)
          *data++ = NS_GET_B(aImageColor);
          *data++ = NS_GET_G(aImageColor);
          *data++ = NS_GET_R(aImageColor);
#else
#if defined(XP_MAC) || defined(XP_MACOSX)
          *data++ = 0;
#endif
          *data++ = NS_GET_R(aImageColor);
          *data++ = NS_GET_G(aImageColor);
          *data++ = NS_GET_B(aImageColor);
#endif
        }

        memset((void *)alpha, SEL_ALPHA_AMOUNT, abpr);

        for (i = 0; i < SEL_IMAGE_HEIGHT; i++)
        {
          image->SetAlphaData(alpha, abpr, i*abpr);
          image->SetImageData(row_data,  bpr, i*bpr);
        }
        free(row_data);
        free(alpha);
        return NS_OK;
      }
    } 
  }
  return NS_ERROR_FAILURE;
}


nsresult NS_NewSelectionImageService(nsISelectionImageService** aResult)
{
  *aResult = new SelectionImageService;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}


//end selection service

// a handy utility to set font
void SetFontFromStyle(nsIRenderingContext* aRC, nsStyleContext* aSC) 
{
  const nsStyleFont* font = aSC->GetStyleFont();
  const nsStyleVisibility* visibility = aSC->GetStyleVisibility();

  aRC->SetFont(font->mFont, visibility->mLangGroup);
}

nsresult
NS_NewEmptyFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsFrame* it = new (aPresShell) nsFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

MOZ_DECL_CTOR_COUNTER(nsFrame)

// Overloaded new operator. Initializes the memory to 0 and relies on an arena
// (which comes from the presShell) to perform the allocation.
void* 
nsFrame::operator new(size_t sz, nsIPresShell* aPresShell) CPP_THROW_NEW
{
  // Check the recycle list first.
  void* result = aPresShell->AllocateFrame(sz);
  
  if (result) {
    memset(result, 0, sz);
  }

  return result;
}

// Overridden to prevent the global delete from being called, since the memory
// came out of an nsIArena instead of the global delete operator's heap.
void 
nsFrame::operator delete(void* aPtr, size_t sz)
{
  // Don't let the memory be freed, since it will be recycled
  // instead. Don't call the global operator delete.

  // Stash the size of the object in the first four bytes of the
  // freed up memory.  The Destroy method can then use this information
  // to recycle the object.
  size_t* szPtr = (size_t*)aPtr;
  *szPtr = sz;
}


nsFrame::nsFrame()
{
  MOZ_COUNT_CTOR(nsFrame);

  mState = NS_FRAME_FIRST_REFLOW | NS_FRAME_IS_DIRTY;
}

nsFrame::~nsFrame()
{
  MOZ_COUNT_DTOR(nsFrame);

  NS_IF_RELEASE(mContent);
  if (mStyleContext)
    mStyleContext->Release();
}

/////////////////////////////////////////////////////////////////////////////
// nsISupports

nsresult nsFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(aInstancePtr, "null out param");

#ifdef DEBUG
  if (aIID.Equals(NS_GET_IID(nsIFrameDebug))) {
    *aInstancePtr = NS_STATIC_CAST(void*,NS_STATIC_CAST(nsIFrameDebug*,this));
    return NS_OK;
  }
#endif

  if (aIID.Equals(NS_GET_IID(nsIFrame)) ||
      aIID.Equals(NS_GET_IID(nsISupports))) {
    *aInstancePtr = NS_STATIC_CAST(void*,NS_STATIC_CAST(nsIFrame*,this));
    return NS_OK;
  }

  *aInstancePtr = nsnull;
  return NS_NOINTERFACE;
}

nsrefcnt nsFrame::AddRef(void)
{
  NS_WARNING("not supported for frames");
  return 1;
}

nsrefcnt nsFrame::Release(void)
{
  NS_WARNING("not supported for frames");
  return 1;
}

/////////////////////////////////////////////////////////////////////////////
// nsIFrame

NS_IMETHODIMP
nsFrame::Init(nsPresContext*  aPresContext,
              nsIContent*      aContent,
              nsIFrame*        aParent,
              nsStyleContext*  aContext,
              nsIFrame*        aPrevInFlow)
{
  mContent = aContent;
  mParent = aParent;

  if (aContent) {
    NS_ADDREF(aContent);
    aContent->SetMayHaveFrame(PR_TRUE);
    NS_ASSERTION(mContent->MayHaveFrame(), "SetMayHaveFrame failed?");
  }

  if (aPrevInFlow) {
    // Make sure the general flags bits are the same
    nsFrameState state = aPrevInFlow->GetStateBits();

    // Make bits that are currently off (see constructor) the same:
    mState |= state & (NS_FRAME_REPLACED_ELEMENT |
                       NS_FRAME_SELECTED_CONTENT |
                       NS_FRAME_INDEPENDENT_SELECTION |
                       NS_FRAME_IS_SPECIAL);
  }
  if (mParent) {
    nsFrameState state = mParent->GetStateBits();

    // Make bits that are currently off (see constructor) the same:
    mState |= state & (NS_FRAME_INDEPENDENT_SELECTION |
                       NS_FRAME_GENERATED_CONTENT);
  }
  SetStyleContext(aPresContext, aContext);

  if (IsBoxWrapped())
    InitBoxMetrics(PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP nsFrame::SetInitialChildList(nsPresContext* aPresContext,
                                           nsIAtom*        aListName,
                                           nsIFrame*       aChildList)
{
  // XXX This shouldn't be getting called at all, but currently is for backwards
  // compatility reasons...
#if 0
  NS_ERROR("not a container");
  return NS_ERROR_UNEXPECTED;
#else
  NS_ASSERTION(nsnull == aChildList, "not a container");
  return NS_OK;
#endif
}

NS_IMETHODIMP
nsFrame::AppendFrames(nsIAtom*        aListName,
                      nsIFrame*       aFrameList)
{
  NS_PRECONDITION(PR_FALSE, "not a container");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsFrame::InsertFrames(nsIAtom*        aListName,
                      nsIFrame*       aPrevFrame,
                      nsIFrame*       aFrameList)
{
  NS_PRECONDITION(PR_FALSE, "not a container");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsFrame::RemoveFrame(nsIAtom*        aListName,
                     nsIFrame*       aOldFrame)
{
  NS_PRECONDITION(PR_FALSE, "not a container");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsFrame::ReplaceFrame(nsIAtom*        aListName,
                      nsIFrame*       aOldFrame,
                      nsIFrame*       aNewFrame)
{
  NS_PRECONDITION(PR_FALSE, "not a container");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsFrame::Destroy(nsPresContext* aPresContext)
{
  // Get the view pointer now before the frame properties disappear
  // when we call NotifyDestroyingFrame()
  nsIView* view = GetView();

  nsIPresShell *shell = aPresContext->GetPresShell();
  if (shell) {
    if (mState & NS_FRAME_OUT_OF_FLOW) {
      nsPlaceholderFrame* placeholder
        = shell->FrameManager()->GetPlaceholderFrameFor(this);
      if (placeholder) {
        NS_WARNING("Deleting out of flow without tearing down placeholder relationship");
        if (placeholder->GetOutOfFlowFrame()) {
          NS_ASSERTION(placeholder->GetOutOfFlowFrame() == this,
                       "no-one told the frame manager about this");
          shell->FrameManager()->UnregisterPlaceholderFrame(placeholder);
          placeholder->SetOutOfFlowFrame(nsnull);
        }
      }
    }

    shell->NotifyDestroyingFrame(this);

    if ((mState & NS_FRAME_EXTERNAL_REFERENCE) ||
        (mState & NS_FRAME_SELECTED_CONTENT)) {
      shell->ClearFrameRefs(this);
    }
  }

  //XXX Why is this done in nsFrame instead of some frame class
  // that actually loads images?
  aPresContext->StopImagesFor(this);

  if (view) {
    // Break association between view and frame
    view->SetClientData(nsnull);
    
    // Destroy the view
    view->Destroy();
  }

  // Deleting the frame doesn't really free the memory, since we're using an
  // arena for allocation, but we will get our destructors called.
  delete this;

  // Now that we're totally cleaned out, we need to add ourselves to the presshell's
  // recycler.
  size_t* sz = (size_t*)this;
  shell->FreeFrame(*sz, (void*)this);

  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetOffsets(PRInt32 &aStart, PRInt32 &aEnd) const
{
  aStart = 0;
  aEnd = 0;
  return NS_OK;
}

// Subclass hook for style post processing
NS_IMETHODIMP nsFrame::DidSetStyleContext(nsPresContext* aPresContext)
{
  return NS_OK;
}

NS_IMETHODIMP  nsFrame::CalcBorderPadding(nsMargin& aBorderPadding) const {
  NS_ASSERTION(mStyleContext!=nsnull,"null style context");
  if (mStyleContext) {
    nsStyleBorderPadding bpad;
    mStyleContext->GetBorderPaddingFor(bpad);
    if (!bpad.GetBorderPadding(aBorderPadding)) {
      const nsStylePadding* paddingStyle = GetStylePadding();
      paddingStyle->CalcPaddingFor(this, aBorderPadding);
      const nsStyleBorder* borderStyle = GetStyleBorder();
      aBorderPadding += borderStyle->GetBorder();
    }
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsStyleContext*
nsFrame::GetAdditionalStyleContext(PRInt32 aIndex) const
{
  NS_PRECONDITION(aIndex >= 0, "invalid index number");
  return nsnull;
}

void
nsFrame::SetAdditionalStyleContext(PRInt32 aIndex, 
                                   nsStyleContext* aStyleContext)
{
  NS_PRECONDITION(aIndex >= 0, "invalid index number");
}

// Child frame enumeration

nsIAtom*
nsFrame::GetAdditionalChildListName(PRInt32 aIndex) const
{
  NS_PRECONDITION(aIndex >= 0, "invalid index number");
  return nsnull;
}

nsIFrame*
nsFrame::GetFirstChild(nsIAtom* aListName) const
{
  return nsnull;
}

static nsIFrame*
GetActiveSelectionFrame(nsIFrame* aFrame)
{
  nsIView* mouseGrabber;
  aFrame->GetPresContext()->GetViewManager()->GetMouseEventGrabber(mouseGrabber);
  if (mouseGrabber) {
    nsIFrame* activeFrame = nsLayoutUtils::GetFrameFor(mouseGrabber);
    if (activeFrame) {
      return activeFrame;
    }
  }
    
  return aFrame;
}

PRInt16
nsFrame::DisplaySelection(nsPresContext* aPresContext, PRBool isOkToTurnOn)
{
  PRInt16 selType = nsISelectionController::SELECTION_OFF;

  nsCOMPtr<nsISelectionController> selCon;
  nsresult result = GetSelectionController(aPresContext, getter_AddRefs(selCon));
  if (NS_SUCCEEDED(result) && selCon) {
    result = selCon->GetDisplaySelection(&selType);
    if (NS_SUCCEEDED(result) && (selType != nsISelectionController::SELECTION_OFF)) {
      // Check whether style allows selection.
      PRBool selectable;
      IsSelectable(&selectable, nsnull);
      if (!selectable) {
        selType = nsISelectionController::SELECTION_OFF;
        isOkToTurnOn = PR_FALSE;
      }
    }
    if (isOkToTurnOn && (selType == nsISelectionController::SELECTION_OFF)) {
      selCon->SetDisplaySelection(nsISelectionController::SELECTION_ON);
      selType = nsISelectionController::SELECTION_ON;
    }
  }
  return selType;
}

void
nsFrame::SetOverflowClipRect(nsIRenderingContext& aRenderingContext)
{
  // 'overflow-clip' only applies to block-level elements and replaced
  // elements that have 'overflow' set to 'hidden', and it is relative
  // to the content area and applies to content only (not border or background)
  const nsStyleBorder* borderStyle = GetStyleBorder();
  const nsStylePadding* paddingStyle = GetStylePadding();
  
  // Start with the 'auto' values and then factor in user specified values
  nsRect  clipRect(0, 0, mRect.width, mRect.height);

  // XXX We don't support the 'overflow-clip' property yet, so just use the
  // content area (which is the default value) as the clip shape

  clipRect.Deflate(borderStyle->GetBorder());
  // XXX We need to handle percentage padding
  nsMargin padding;
  if (paddingStyle->GetPadding(padding)) {
    clipRect.Deflate(padding);
  }
#ifdef DEBUG
  else {
    NS_WARNING("Percentage padding and CLIP overflow don't mix yet");
  }
#endif

  // Set updated clip-rect into the rendering context
  aRenderingContext.SetClipRect(clipRect, nsClipCombine_kIntersect);
}

/********************************************************
* Refreshes each content's frame
*********************************************************/

NS_IMETHODIMP
nsFrame::Paint(nsPresContext*      aPresContext,
               nsIRenderingContext& aRenderingContext,
               const nsRect&        aDirtyRect,
               nsFramePaintLayer    aWhichLayer,
               PRUint32             aFlags)
{
  if (aWhichLayer != NS_FRAME_PAINT_LAYER_FOREGROUND)
    return NS_OK;
  
  nsresult result; 
  nsIPresShell *shell = aPresContext->PresShell();

  PRInt16 displaySelection = nsISelectionDisplay::DISPLAY_ALL;
  if (!(aFlags & nsISelectionDisplay::DISPLAY_IMAGES))
  {
    result = shell->GetSelectionFlags(&displaySelection);
    if (NS_FAILED(result))
      return result;
    if (!(displaySelection & nsISelectionDisplay::DISPLAY_FRAMES))
      return NS_OK;
  }

//check frame selection state
  PRBool isSelected;
  isSelected = (GetStateBits() & NS_FRAME_SELECTED_CONTENT) == NS_FRAME_SELECTED_CONTENT;
//if not selected then return 
  if (!isSelected)
    return NS_OK; //nothing to do

//get the selection controller
  nsCOMPtr<nsISelectionController> selCon;
  result = GetSelectionController(aPresContext, getter_AddRefs(selCon));
  PRInt16 selectionValue;
  selCon->GetDisplaySelection(&selectionValue);
  displaySelection = selectionValue > nsISelectionController::SELECTION_HIDDEN;
//check display selection state.
  if (!displaySelection)
    return NS_OK; //if frame does not allow selection. do nothing


  nsIContent *newContent = mContent->GetParent();

  //check to see if we are anonymous content
  PRInt32 offset = 0;
  if (newContent) {
    // XXXbz there has GOT to be a better way of determining this!
    offset = newContent->IndexOf(mContent);
  }

  SelectionDetails *details;
  if (NS_SUCCEEDED(result) && shell)
  {
    nsCOMPtr<nsIFrameSelection> frameSelection;
    if (NS_SUCCEEDED(result) && selCon)
    {
      frameSelection = do_QueryInterface(selCon); //this MAY implement
    }
    if (!frameSelection)
      frameSelection = shell->FrameSelection();
    result = frameSelection->LookUpSelection(newContent, offset, 
                                             1, &details, PR_FALSE);//look up to see what selection(s) are on this frame
  }
  
  if (details)
  {
    nsRect rect = GetRect();
    rect.width-=2;
    rect.height-=2;
    rect.x=1; //we are in the coordinate system of the frame now with regards to the rendering context.
    rect.y=1;

    nsCOMPtr<nsISelectionImageService> imageService;
    imageService = do_GetService(kSelectionImageService, &result);
    if (NS_SUCCEEDED(result) && imageService)
    {
      nsCOMPtr<imgIContainer> container;
      imageService->GetImage(selectionValue, getter_AddRefs(container));
      if (container)
      {
        nsRect rect(0, 0, mRect.width, mRect.height);
        rect.IntersectRect(rect,aDirtyRect);
        aRenderingContext.DrawTile(container,0,0,&rect);
      }
    }

    
    
    SelectionDetails *deletingDetails = details;
    while ((deletingDetails = details->mNext) != nsnull) {
      delete details;
      details = deletingDetails;
    }
    delete details;
  }
  return NS_OK;
}

void
nsFrame::PaintSelf(nsPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   PRIntn               aSkipSides,
                   PRBool               aUsePrintBackgroundSettings)
{
  // The visibility check belongs here since child elements have the
  // opportunity to override the visibility property and display even if
  // their parent is hidden.

  PRBool isVisible;
  if (mRect.height == 0 || mRect.width == 0 ||
      NS_FAILED(IsVisibleForPainting(aPresContext, aRenderingContext,
                                     PR_TRUE, &isVisible)) ||
      !isVisible) {
    return;
  }

  // Paint our background and border
  const nsStyleBorder* border = GetStyleBorder();
  const nsStylePadding* padding = GetStylePadding();
  const nsStyleOutline* outline = GetStyleOutline();

  nsRect rect(0, 0, mRect.width, mRect.height);
  nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                  aDirtyRect, rect, *border, *padding,
                                  aUsePrintBackgroundSettings);
  nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                              aDirtyRect, rect, *border, mStyleContext,
                              aSkipSides);
  nsCSSRendering::PaintOutline(aPresContext, aRenderingContext, this,
                               aDirtyRect, rect, *border, *outline,
                               mStyleContext, 0);
}

nsresult
nsIFrame::CreateWidgetForView(nsIView* aView)
{
  return aView->CreateWidget(kWidgetCID);
}

/**
  *
 */
NS_IMETHODIMP  
nsFrame::GetContentForEvent(nsPresContext* aPresContext,
                            nsEvent* aEvent,
                            nsIContent** aContent)
{
  *aContent = GetContent();
  NS_IF_ADDREF(*aContent);
  return NS_OK;
}

void
nsFrame::FireDOMEvent(const nsAString& aDOMEventName, nsIContent *aContent)
{
  nsCOMPtr<nsIDOMNode> domNode = do_QueryInterface(aContent ? aContent : mContent);
  
  if (domNode) {
    nsPLDOMEvent *event = new nsPLDOMEvent(domNode, aDOMEventName);
    if (event && NS_FAILED(event->PostDOMEvent())) {
      PL_DestroyEvent(event);
    }
  }
}

/**
  *
 */
NS_IMETHODIMP
nsFrame::HandleEvent(nsPresContext* aPresContext, 
                     nsGUIEvent*     aEvent,
                     nsEventStatus*  aEventStatus)
{
  switch (aEvent->message)
  {
  case NS_MOUSE_MOVE:
    {
      HandleDrag(aPresContext, aEvent, aEventStatus);
    }break;
  case NS_MOUSE_LEFT_BUTTON_DOWN:
    {
      HandlePress(aPresContext, aEvent, aEventStatus);
    }break;
  case NS_MOUSE_LEFT_BUTTON_UP:
    {
      HandleRelease(aPresContext, aEvent, aEventStatus);
    } break;
  default:
    break;
  }//end switch
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetDataForTableSelection(nsIFrameSelection *aFrameSelection, 
                                  nsIPresShell *aPresShell, nsMouseEvent *aMouseEvent, 
                                  nsIContent **aParentContent, PRInt32 *aContentOffset, PRInt32 *aTarget)
{
  if (!aFrameSelection || !aPresShell || !aMouseEvent || !aParentContent || !aContentOffset || !aTarget)
    return NS_ERROR_NULL_POINTER;

  *aParentContent = nsnull;
  *aContentOffset = 0;
  *aTarget = 0;

  PRInt16 displaySelection;
  nsresult result = aPresShell->GetSelectionFlags(&displaySelection);
  if (NS_FAILED(result))
    return result;

  PRBool selectingTableCells = PR_FALSE;
  aFrameSelection->GetTableCellSelection(&selectingTableCells);

  // DISPLAY_ALL means we're in an editor.
  // If already in cell selection mode, 
  //  continue selecting with mouse drag or end on mouse up,
  //  or when using shift key to extend block of cells
  //  (Mouse down does normal selection unless Ctrl/Cmd is pressed)
  PRBool doTableSelection =
     displaySelection == nsISelectionDisplay::DISPLAY_ALL && selectingTableCells &&
     (aMouseEvent->message == NS_MOUSE_MOVE ||
      aMouseEvent->message == NS_MOUSE_LEFT_BUTTON_UP || 
      aMouseEvent->isShift);

  if (!doTableSelection)
  {  
    // In Browser, special 'table selection' key must be pressed for table selection
    // or when just Shift is pressed and we're already in table/cell selection mode
#if defined(XP_MAC) || defined(XP_MACOSX)
    doTableSelection = aMouseEvent->isMeta || (aMouseEvent->isShift && selectingTableCells);
#else
    doTableSelection = aMouseEvent->isControl || (aMouseEvent->isShift && selectingTableCells);
#endif
  }
  if (!doTableSelection) 
    return NS_OK;

  // Get the cell frame or table frame (or parent) of the current content node
  nsIFrame *frame = this;
  PRBool foundCell = PR_FALSE;
  PRBool foundTable = PR_FALSE;

  // Get the limiting node to stop parent frame search
  nsCOMPtr<nsIContent> limiter;
  result = aFrameSelection->GetLimiter(getter_AddRefs(limiter));

  //We don't initiate row/col selection from here now,
  //  but we may in future
  //PRBool selectColumn = PR_FALSE;
  //PRBool selectRow = PR_FALSE;

  while (frame && NS_SUCCEEDED(result))
  {
    // Check for a table cell by querying to a known CellFrame interface
    nsITableCellLayout *cellElement;
    result = (frame)->QueryInterface(NS_GET_IID(nsITableCellLayout), (void **)&cellElement);
    if (NS_SUCCEEDED(result) && cellElement)
    {
      foundCell = PR_TRUE;
      //TODO: If we want to use proximity to top or left border
      //      for row and column selection, this is the place to do it
      break;
    }
    else
    {
      // If not a cell, check for table
      // This will happen when starting frame is the table or child of a table,
      //  such as a row (we were inbetween cells or in table border)
      nsITableLayout *tableElement;
      result = (frame)->QueryInterface(NS_GET_IID(nsITableLayout), (void **)&tableElement);
      if (NS_SUCCEEDED(result) && tableElement)
      {
        foundTable = PR_TRUE;
        //TODO: How can we select row when along left table edge
        //  or select column when along top edge?
        break;
      } else {
        frame = frame->GetParent();
        result = NS_OK;
        // Stop if we have hit the selection's limiting content node
        if (frame && frame->GetContent() == limiter.get())
          break;
      }
    }
  }
  // We aren't in a cell or table
  if (!foundCell && !foundTable) return NS_OK;

  nsIContent* tableOrCellContent = frame->GetContent();
  if (!tableOrCellContent) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> parentContent = tableOrCellContent->GetParent();
  if (!parentContent) return NS_ERROR_FAILURE;

  PRInt32 offset = parentContent->IndexOf(tableOrCellContent);
  // Not likely?
  if (offset < 0) return NS_ERROR_FAILURE;

  // Everything is OK -- set the return values
  *aParentContent = parentContent;
  NS_ADDREF(*aParentContent);

  *aContentOffset = offset;

#if 0
  if (selectRow)
    *aTarget = nsISelectionPrivate::TABLESELECTION_ROW;
  else if (selectColumn)
    *aTarget = nsISelectionPrivate::TABLESELECTION_COLUMN;
  else 
#endif
  if (foundCell)
    *aTarget = nsISelectionPrivate::TABLESELECTION_CELL;
  else if (foundTable)
    *aTarget = nsISelectionPrivate::TABLESELECTION_TABLE;

  return NS_OK;
}

/*
NS_IMETHODIMP
nsFrame::FrameOrParentHasSpecialSelectionStyle(PRUint8 aSelectionStyle, nsIFrame* *foundFrame)
{
  nsIFrame* thisFrame = this;
  
  while (thisFrame)
  {
    if (thisFrame->GetStyleUserInterface()->mUserSelect == aSelectionStyle)
    {
      *foundFrame = thisFrame;
      return NS_OK;
    }
  
    thisFrame = thisFrame->GetParent();
  }
  
  *foundFrame = nsnull;
  return NS_OK;
}
*/

NS_IMETHODIMP
nsFrame::IsSelectable(PRBool* aSelectable, PRUint8* aSelectStyle) const
{
  if (!aSelectable) //its ok if aSelectStyle is null
    return NS_ERROR_NULL_POINTER;

  // Like 'visibility', we must check all the parents: if a parent
  // is not selectable, none of its children is selectable.
  //
  // The -moz-all value acts similarly: if a frame has 'user-select:-moz-all',
  // all its children are selectable, even those with 'user-select:none'.
  //
  // As a result, if 'none' and '-moz-all' are not present in the frame hierarchy,
  // aSelectStyle returns the first style that is not AUTO. If these values
  // are present in the frame hierarchy, aSelectStyle returns the style of the
  // topmost parent that has either 'none' or '-moz-all'.
  //
  // For instance, if the frame hierarchy is:
  //    AUTO     -> _MOZ_ALL -> NONE -> TEXT,     the returned value is _MOZ_ALL
  //    TEXT     -> NONE     -> AUTO -> _MOZ_ALL, the returned value is NONE
  //    _MOZ_ALL -> TEXT     -> AUTO -> AUTO,     the returned value is _MOZ_ALL
  //    AUTO     -> CELL     -> TEXT -> AUTO,     the returned value is TEXT
  //
  PRUint8 selectStyle  = NS_STYLE_USER_SELECT_AUTO;
  nsIFrame* frame      = (nsIFrame*)this;

  while (frame) {
    const nsStyleUIReset* userinterface = frame->GetStyleUIReset();
    switch (userinterface->mUserSelect) {
      case NS_STYLE_USER_SELECT_ALL:
      case NS_STYLE_USER_SELECT_NONE:
      case NS_STYLE_USER_SELECT_MOZ_ALL:
        // override the previous values
        selectStyle = userinterface->mUserSelect;
        break;
      default:
        // otherwise return the first value which is not 'auto'
        if (selectStyle == NS_STYLE_USER_SELECT_AUTO) {
          selectStyle = userinterface->mUserSelect;
        }
        break;
    }
    frame = frame->GetParent();
  }

  // convert internal values to standard values
  if (selectStyle == NS_STYLE_USER_SELECT_AUTO)
    selectStyle = NS_STYLE_USER_SELECT_TEXT;
  else
  if (selectStyle == NS_STYLE_USER_SELECT_MOZ_ALL)
    selectStyle = NS_STYLE_USER_SELECT_ALL;
  else
  if (selectStyle == NS_STYLE_USER_SELECT_MOZ_NONE)
    selectStyle = NS_STYLE_USER_SELECT_NONE;

  // return stuff
  if (aSelectable)
    *aSelectable = (selectStyle != NS_STYLE_USER_SELECT_NONE);
  if (aSelectStyle)
    *aSelectStyle = selectStyle;
  if (mState & NS_FRAME_GENERATED_CONTENT)
    *aSelectable = PR_FALSE;
  return NS_OK;
}

PRBool
ContentContainsPoint(nsPresContext *aPresContext,
                     nsIContent *aContent,
                     nsPoint *aPoint,
                     nsIView *aRelativeView)
{
  nsIPresShell *presShell = aPresContext->GetPresShell();

  if (!presShell) return PR_FALSE;

  nsIFrame *frame = nsnull;

  nsresult rv = presShell->GetPrimaryFrameFor(aContent, &frame);

  if (NS_FAILED(rv) || !frame) return PR_FALSE;

  nsIView *frameView = nsnull;
  nsPoint offsetPoint;

  // Get the view that contains the content's frame.

  rv = frame->GetOffsetFromView(offsetPoint, &frameView);

  if (NS_FAILED(rv) || !frameView) return PR_FALSE;

  // aPoint is relative to aRelativeView's upper left corner! Make sure
  // that our point is in the same view space our content frame's
  // rects are in.

  nsPoint point = *aPoint + aRelativeView->GetOffsetTo(frameView);

  // Now check to see if the point is within the bounds of the
  // content's primary frame, or any of it's continuation frames.

  while (frame) {
    // Get the frame's rect and make it relative to the
    // upper left corner of it's parent view.

    nsRect frameRect = frame->GetRect();
    frameRect.x = offsetPoint.x;
    frameRect.y = offsetPoint.y;

    if (frameRect.Contains(point)) {
      // point is within this frame's rect!
      return PR_TRUE;
    }

    frame = frame->GetNextInFlow();

  }

  return PR_FALSE;
}

/**
  * Handles the Mouse Press Event for the frame
 */
NS_IMETHODIMP
nsFrame::HandlePress(nsPresContext* aPresContext, 
                     nsGUIEvent*     aEvent,
                     nsEventStatus*  aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

  //We often get out of sync state issues with mousedown events that
  //get interrupted by alerts/dialogs.
  //Check with the ESM to see if we should process this one
  PRBool eventOK;
  aPresContext->EventStateManager()->EventStatusOK(aEvent, &eventOK);
  if (!eventOK) 
    return NS_OK;

  nsresult rv;
  nsIPresShell *shell = aPresContext->GetPresShell();
  if (!shell)
    return NS_ERROR_FAILURE;

  // if we are in Navigator and the click is in a draggable node, we don't want
  // to start selection because we don't want to interfere with a potential
  // drag of said node and steal all its glory.
  PRInt16 isEditor = 0;
  shell->GetSelectionFlags ( &isEditor );
  //weaaak. only the editor can display frame selction not just text and images
  isEditor = isEditor == nsISelectionDisplay::DISPLAY_ALL;

  nsKeyEvent* keyEvent = (nsKeyEvent*)aEvent;
  if (!isEditor && !keyEvent->isAlt) {
    
    for (nsIContent* content = mContent; content;
         content = content->GetParent()) {
      if ( nsContentUtils::ContentIsDraggable(content) ) {
        // coordinate stuff is the fix for bug #55921
        nsIView *dummyView = 0;
        nsRect frameRect = mRect;
        nsPoint offsetPoint;

        GetOffsetFromView(offsetPoint, &dummyView);

        frameRect.x = offsetPoint.x;
        frameRect.y = offsetPoint.y;

        if (frameRect.x <= aEvent->point.x && (frameRect.x + frameRect.width >= aEvent->point.x) &&
            frameRect.y <= aEvent->point.y && (frameRect.y + frameRect.height >= aEvent->point.y))
          return NS_OK;
      }
    }
  } // if browser, not editor

  // check whether style allows selection
  // if not, don't tell selection the mouse event even occurred.  
  PRBool  selectable;
  PRUint8 selectStyle;
  rv = IsSelectable(&selectable, &selectStyle);
  if (NS_FAILED(rv)) return rv;
  
  // check for select: none
  if (!selectable)
    return NS_OK;

  // When implementing NS_STYLE_USER_SELECT_ELEMENT, NS_STYLE_USER_SELECT_ELEMENTS and
  // NS_STYLE_USER_SELECT_TOGGLE, need to change this logic
  PRBool useFrameSelection = (selectStyle == NS_STYLE_USER_SELECT_TEXT);

  if (!IsMouseCaptured(aPresContext))
    CaptureMouse(aPresContext, PR_TRUE);

  PRInt16 displayresult = nsISelectionController::SELECTION_OFF;
  nsCOMPtr<nsISelectionController> selCon;
  rv = GetSelectionController(aPresContext, getter_AddRefs(selCon));
  //get the selection controller
  if (NS_SUCCEEDED(rv) && selCon) 
  {
    selCon->GetDisplaySelection(&displayresult);
    if (displayresult == nsISelectionController::SELECTION_OFF)
      return NS_OK;//nothing to do we cannot affect selection from here
  }

  //get the frame selection from sel controller

  // nsFrameState  state = GetStateBits();
  // if (state & NS_FRAME_INDEPENDENT_SELECTION) 
  nsCOMPtr<nsIFrameSelection> frameselection;

  if (useFrameSelection)
    frameselection = do_QueryInterface(selCon); //this MAY implement

  if (!frameselection)//if we must get it from the pres shell's
    frameselection = shell->FrameSelection();

  if (!frameselection)
    return NS_ERROR_FAILURE;

  nsMouseEvent *me = (nsMouseEvent *)aEvent;

#if defined(XP_MAC) || defined(XP_MACOSX)
  if (me->isControl)
    return NS_OK;//short ciruit. hard coded for mac due to time restraints.
#endif
    
  if (me->clickCount >1 )
  {
    rv = frameselection->SetMouseDownState( PR_TRUE );
  
    frameselection->SetMouseDoubleDown(PR_TRUE);
    return HandleMultiplePress(aPresContext, aEvent, aEventStatus);
  }

  nsCOMPtr<nsIContent> content;
  PRInt32 startOffset = 0, endOffset = 0;
  PRBool  beginFrameContent = PR_FALSE;

  rv = GetContentAndOffsetsFromPoint(aPresContext, aEvent->point, getter_AddRefs(content), startOffset, endOffset, beginFrameContent);
  // do we have CSS that changes selection behaviour?
  PRBool changeSelection = PR_FALSE;
  {
    nsCOMPtr<nsIContent>  selectContent;
    PRInt32   newStart, newEnd;
    if (NS_SUCCEEDED(frameselection->AdjustOffsetsFromStyle(this, &changeSelection, getter_AddRefs(selectContent), &newStart, &newEnd))
      && changeSelection)
    {
      content = selectContent;
      startOffset = newStart;
      endOffset = newEnd;
    }
  }

  if (NS_FAILED(rv))
    return rv;

  // Let Ctrl/Cmd+mouse down do table selection instead of drag initiation
  nsCOMPtr<nsIContent>parentContent;
  PRInt32  contentOffset;
  PRInt32 target;
  rv = GetDataForTableSelection(frameselection, shell, me, getter_AddRefs(parentContent), &contentOffset, &target);
  if (NS_SUCCEEDED(rv) && parentContent)
  {
    rv = frameselection->SetMouseDownState( PR_TRUE );
    if (NS_FAILED(rv)) return rv;
  
    return frameselection->HandleTableSelection(parentContent, contentOffset, target, me);
  }

  PRBool supportsDelay = PR_FALSE;

  frameselection->GetDelayCaretOverExistingSelection(&supportsDelay);
  frameselection->SetDelayedCaretData(0);

  if (supportsDelay)
  {
    // Check if any part of this frame is selected, and if the
    // user clicked inside the selected region. If so, we delay
    // starting a new selection since the user may be trying to
    // drag the selected region to some other app.

    SelectionDetails *details = 0;
    PRBool isSelected = ((GetStateBits() & NS_FRAME_SELECTED_CONTENT) == NS_FRAME_SELECTED_CONTENT);

    if (isSelected)
    {
      rv = frameselection->LookUpSelection(content, 0, endOffset, &details, PR_FALSE);

      if (NS_FAILED(rv))
        return rv;

      //
      // If there are any details, check to see if the user clicked
      // within any selected region of the frame.
      //

      if (details)
      {
        SelectionDetails *curDetail = details;

        while (curDetail)
        {
          //
          // If the user clicked inside a selection, then just
          // return without doing anything. We will handle placing
          // the caret later on when the mouse is released. We ignore
          // the spellcheck selection.
          //
          if (curDetail->mType != nsISelectionController::SELECTION_SPELLCHECK &&
              curDetail->mStart <= startOffset && endOffset <= curDetail->mEnd)
          {
            delete details;
            rv = frameselection->SetMouseDownState( PR_FALSE );

            if (NS_FAILED(rv))
              return rv;

            return frameselection->SetDelayedCaretData(me);
          }

          curDetail = curDetail->mNext;
        }

        delete details;
      }
    }
  }

  rv = frameselection->SetMouseDownState( PR_TRUE );

  if (NS_FAILED(rv))
    return rv;

  rv = frameselection->HandleClick(content, startOffset , endOffset, me->isShift, PR_FALSE, beginFrameContent);

  if (NS_FAILED(rv))
    return rv;

  if (startOffset != endOffset)
    frameselection->MaintainSelection();

  if (isEditor && !me->isShift && (endOffset - startOffset) == 1)
  {
    // A single node is selected and we aren't extending an existing
    // selection, which means the user clicked directly on an object.
    // Check if the user clicked in a -moz-user-select:all subtree,
    // image, or hr. If so, we want to give the drag and drop
    // code a chance to execute so we need to turn off selection extension
    // when processing mouse move/drag events that follow this mouse
    // down event.

    PRBool disableDragSelect = PR_FALSE;

    if (changeSelection)
    {
      // The click hilited a -moz-user-select:all subtree.
      //
      // XXX: We really should be able to just do a:
      //
      //        disableDragSelect = PR_TRUE;
      //
      //      but we are working around the fact that in some cases,
      //      selection selects a -moz-user-select:all subtree even
      //      when the click was outside of the subtree. An example of
      //      this case would be when the subtree is at the end of a
      //      line and the user clicks to the right of it. In this case
      //      I would expect the caret to be placed next to the root of
      //      the subtree, but right now the whole subtree gets selected.
      //      This means that we have to do geometric frame containment
      //      checks on the point to see if the user truly clicked
      //      inside the subtree.
      
      nsIView *view = nsnull;
      nsPoint dummyPoint;

      // aEvent->point is relative to the upper left corner of the
      // frame's parent view. Unfortunately, the only way to get
      // the parent view is to call GetOffsetFromView().

      GetOffsetFromView(dummyPoint, &view);

      // Now check to see if the point is truly within the bounds
      // of any of the frames that make up the -moz-user-select:all subtree:

      if (view)
        disableDragSelect = ContentContainsPoint(aPresContext, content,
                                                 &aEvent->point, view);
    }
    else
    {
      // Check if click was in an image.

      nsIContent* frameContent = GetContent();
      nsCOMPtr<nsIDOMHTMLImageElement> img(do_QueryInterface(frameContent));

      disableDragSelect = img != nsnull;

      if (!img)
      {
        // Check if click was in an hr.

        nsCOMPtr<nsIDOMHTMLHRElement> hr(do_QueryInterface(frameContent));
        disableDragSelect = hr != nsnull;
      }
    }

    if (disableDragSelect)
    {
      // Click was in one of our draggable objects, so disable
      // selection extension during mouse moves.

      rv = frameselection->SetMouseDownState( PR_FALSE );
    }
  }

  return rv;
}
 
/**
  * Multiple Mouse Press -- line or paragraph selection -- for the frame.
  * Wouldn't it be nice if this didn't have to be hardwired into Frame code?
 */
NS_IMETHODIMP
nsFrame::HandleMultiplePress(nsPresContext* aPresContext, 
                             nsGUIEvent*     aEvent,
                             nsEventStatus*  aEventStatus)
{
  NS_ENSURE_ARG_POINTER(aEventStatus);
  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

  nsresult rv;
  if (DisplaySelection(aPresContext) == nsISelectionController::SELECTION_OFF) {
    return NS_OK;
  }

  // Find out whether we're doing line or paragraph selection.
  // Currently, triple-click selects line unless the user sets
  // browser.triple_click_selects_paragraph; quadruple-click
  // selects paragraph, if any platform actually implements it.
  PRBool selectPara = PR_FALSE;
  nsMouseEvent *me = (nsMouseEvent *)aEvent;
  if (!me) return NS_OK;

#if 0 // Paragraph selection is currently broken - so disable it
  if (me->clickCount == 4)
    selectPara = PR_TRUE;
  else
#endif
  if (me->clickCount == 3)
  {
    selectPara =
      nsContentUtils::GetBoolPref("browser.triple_click_selects_paragraph");
  }
  else
    return NS_OK;
#ifdef DEBUG_akkana
  if (selectPara) printf("Selecting Paragraph\n");
  else printf("Selecting Line\n");
#endif

  // Line or paragraph selection:
  PRInt32 startPos = 0;
  PRInt32 contentOffsetEnd = 0;
  nsCOMPtr<nsIContent> newContent;
  PRBool beginContent = PR_FALSE;
  rv = GetContentAndOffsetsFromPoint(aPresContext,
                                     aEvent->point,
                                     getter_AddRefs(newContent),
                                     startPos,
                                     contentOffsetEnd,
                                     beginContent);
  if (NS_FAILED(rv)) return rv;
  
  
  return PeekBackwardAndForward(selectPara ? eSelectParagraph
                                           : eSelectBeginLine,
                                selectPara ? eSelectParagraph
                                           : eSelectEndLine,
                                startPos, aPresContext, PR_TRUE);
}

NS_IMETHODIMP
nsFrame::PeekBackwardAndForward(nsSelectionAmount aAmountBack,
                                nsSelectionAmount aAmountForward,
                                PRInt32 aStartPos,
                                nsPresContext* aPresContext,
                                PRBool aJumpLines)
{
  nsCOMPtr<nsISelectionController> selcon;
  nsresult rv = GetSelectionController(aPresContext, getter_AddRefs(selcon));
  if (NS_FAILED(rv)) return rv;

  nsIPresShell *shell = aPresContext->GetPresShell();
  if (!shell || !selcon)
    return NS_ERROR_NOT_INITIALIZED;

  // Use peek offset one way then the other:
  nsCOMPtr<nsIContent> startContent;
  nsCOMPtr<nsIDOMNode> startNode;
  nsCOMPtr<nsIContent> endContent;
  nsCOMPtr<nsIDOMNode> endNode;
  nsPeekOffsetStruct startpos;
  startpos.SetData(shell,
                   0, 
                   aAmountBack,
                   eDirPrevious,
                   aStartPos,
                   PR_FALSE,
                   PR_TRUE,
                   aJumpLines,
                   PR_TRUE,  //limit on scrolled views
                   PR_FALSE);
  rv = PeekOffset(aPresContext, &startpos);
  if (NS_FAILED(rv))
    return rv;
  nsPeekOffsetStruct endpos;
  endpos.SetData(shell,
                 0, 
                 aAmountForward,
                 eDirNext,
                 aStartPos,
                 PR_FALSE,
                 PR_FALSE,
                 aJumpLines,
                 PR_TRUE,  //limit on scrolled views
                 PR_FALSE);
  rv = PeekOffset(aPresContext, &endpos);
  if (NS_FAILED(rv))
    return rv;

  endNode = do_QueryInterface(endpos.mResultContent, &rv);
  if (NS_FAILED(rv))
    return rv;
  startNode = do_QueryInterface(startpos.mResultContent, &rv);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsISelection> selection;
  if (NS_SUCCEEDED(selcon->GetSelection(nsISelectionController::SELECTION_NORMAL,
                                        getter_AddRefs(selection)))){
    rv = selection->Collapse(startNode,startpos.mContentOffset);
    if (NS_FAILED(rv))
      return rv;
    rv = selection->Extend(endNode,endpos.mContentOffset);
    if (NS_FAILED(rv))
      return rv;
  }
  //no release 

  // maintain selection
  nsCOMPtr<nsIFrameSelection> frameselection = do_QueryInterface(selcon); //this MAY implement
  if (!frameselection)
    frameselection = aPresContext->PresShell()->FrameSelection();

  return frameselection->MaintainSelection();
}

// Figure out which view we should point capturing at, given that drag started
// in this frame.
static nsIView* GetNearestCapturingView(nsIFrame* aFrame) {
  nsIView* view = nsnull;
  while (!(view = aFrame->GetMouseCapturer()) && aFrame->GetParent()) {
    aFrame = aFrame->GetParent();
  }
  if (!view) {
    // Use the root view. The root frame always has the root view.
    view = aFrame->GetView();
  }
  NS_ASSERTION(view, "No capturing view found");
  return view;
}

NS_IMETHODIMP nsFrame::HandleDrag(nsPresContext* aPresContext, 
                                  nsGUIEvent*     aEvent,
                                  nsEventStatus*  aEventStatus)
{
  PRBool  selectable;
  PRUint8 selectStyle;
  IsSelectable(&selectable, &selectStyle);
  if (!selectable)
    return NS_OK;
  if (DisplaySelection(aPresContext) == nsISelectionController::SELECTION_OFF) {
    return NS_OK;
  }
  nsIPresShell *presShell = aPresContext->PresShell();

  nsCOMPtr<nsIFrameSelection> frameselection;
  nsCOMPtr<nsISelectionController> selCon;
  
  nsresult result = GetSelectionController(aPresContext,
                                           getter_AddRefs(selCon));
  if (NS_SUCCEEDED(result) && selCon)
  {
    frameselection = do_QueryInterface(selCon); //this MAY implement
  }
  if (!frameselection)
    frameselection = presShell->FrameSelection();

  PRBool mouseDown = PR_FALSE;
  if (NS_SUCCEEDED(frameselection->GetMouseDownState(&mouseDown)) && !mouseDown)
    return NS_OK;            

  frameselection->StopAutoScrollTimer();
  // Check if we are dragging in a table cell
  nsCOMPtr<nsIContent> parentContent;
  PRInt32 contentOffset;
  PRInt32 target;
  nsMouseEvent *me = (nsMouseEvent *)aEvent;
  result = GetDataForTableSelection(frameselection, presShell, me,
                                    getter_AddRefs(parentContent),
                                    &contentOffset, &target);      

  if (NS_SUCCEEDED(result) && parentContent)
    frameselection->HandleTableSelection(parentContent, contentOffset, target, me);
  else
    frameselection->HandleDrag(aPresContext, this, aEvent->point);

  nsIView* captureView = GetNearestCapturingView(this);
  if (captureView) {
    // Get the view that aEvent->point is relative to. This is disgusting.
    nsPoint dummyPoint;
    nsIView* eventView;
    GetOffsetFromView(dummyPoint, &eventView);
    nsPoint pt = aEvent->point + eventView->GetOffsetTo(captureView);
    frameselection->StartAutoScrollTimer(aPresContext, captureView, pt, 30);
  }

  return NS_OK;
}

static void
GetFrameSelectionFor(nsIFrame* aFrame, nsIFrameSelection** aFrameSel, nsISelectionController** aSelCon)
{
  *aSelCon = nsnull;
  *aFrameSel = nsnull;
  nsresult result = aFrame->GetSelectionController(aFrame->GetPresContext(), aSelCon);
  if (NS_SUCCEEDED(result) && *aSelCon)
    CallQueryInterface(*aSelCon, aFrameSel);
  if (!*aFrameSel)
    NS_IF_ADDREF(*aFrameSel = aFrame->GetPresContext()->GetPresShell()->FrameSelection());
}

/**
 * This static method handles part of the nsFrame::HandleRelease in a way
 * which doesn't rely on the nsFrame object to stay alive.
 */
static nsresult
HandleFrameSelection(nsIFrameSelection* aFrameSelection,
                     nsIContent*        aContent,
                     PRInt32            aStartOffset,
                     PRInt32            aEndOffset,
                     PRBool             aBeginFrameContent,
                     PRBool             aHandleTableSel,
                     PRInt32            aContentOffsetForTableSel,
                     PRInt32            aTargetForTableSel,
                     nsGUIEvent*        aEvent,
                     nsEventStatus*     aEventStatus)
{
  if (!aFrameSelection) {
    return NS_OK;
  }

  nsresult rv = NS_OK;

  if (nsEventStatus_eConsumeNoDefault != *aEventStatus && aContent) {
    if (!aHandleTableSel) {
      nsMouseEvent* me = nsnull;
      aFrameSelection->GetDelayedCaretData(&me);
      if (!me) {
        return NS_ERROR_FAILURE;
      }
      // We are doing this to simulate what we would have done on HandlePress
      aFrameSelection->SetMouseDownState(PR_TRUE);
      rv = aFrameSelection->HandleClick(aContent, aStartOffset, aEndOffset,
                                        me->isShift, PR_FALSE,
                                        aBeginFrameContent);
      if (NS_FAILED(rv)) {
        return rv;
      }
    } else {
      aFrameSelection->SetMouseDownState(PR_FALSE);
      aFrameSelection->HandleTableSelection(aContent,
                                            aContentOffsetForTableSel,
                                            aTargetForTableSel,
                                            (nsMouseEvent *)aEvent);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
    aFrameSelection->SetDelayedCaretData(0);
  }

  aFrameSelection->SetMouseDownState(PR_FALSE);
  aFrameSelection->StopAutoScrollTimer();
  return NS_OK;
}

NS_IMETHODIMP nsFrame::HandleRelease(nsPresContext* aPresContext,
                                     nsGUIEvent*    aEvent,
                                     nsEventStatus* aEventStatus)
{
  nsIFrame* activeFrame = GetActiveSelectionFrame(this);

  // We can unconditionally stop capturing because
  // we should never be capturing when the mouse button is up
  CaptureMouse(aPresContext, PR_FALSE);

  PRBool selectionOff =
    (DisplaySelection(aPresContext) == nsISelectionController::SELECTION_OFF);

  nsCOMPtr<nsISelectionController> selCon;
  nsCOMPtr<nsIFrameSelection> frameselection;
  nsCOMPtr<nsIContent> content;
  PRInt32 contentOffsetForTableSel = 0;
  PRInt32 targetForTableSel = 0;
  PRBool handleTableSelection = PR_TRUE;
  PRInt32 startOffset = 0;
  PRInt32 endOffset = 0;
  PRBool beginFrameContent = PR_FALSE;
  nsresult rv = NS_OK;

  if (!selectionOff) {
    GetFrameSelectionFor(this, getter_AddRefs(frameselection), getter_AddRefs(selCon));
    if (nsEventStatus_eConsumeNoDefault != *aEventStatus && frameselection) {
      PRBool supportsDelay = PR_FALSE;
      frameselection->GetDelayCaretOverExistingSelection(&supportsDelay);
      if (supportsDelay) {
        // Check if the frameselection recorded the mouse going down.
        // If not, the user must have clicked in a part of the selection.
        // Place the caret before continuing!
        PRBool mouseDown = PR_FALSE;
        rv = frameselection->GetMouseDownState(&mouseDown);
        if (NS_FAILED(rv)) {
          return rv;
        }

        nsMouseEvent* me = nsnull;
        rv = frameselection->GetDelayedCaretData(&me);

        if (NS_SUCCEEDED(rv) && !mouseDown && me && me->clickCount < 2) {
          handleTableSelection = PR_FALSE;

          rv = GetContentAndOffsetsFromPoint(aPresContext, me->point,
                                             getter_AddRefs(content),
                                             startOffset, endOffset,
                                             beginFrameContent);
          if (NS_FAILED(rv)) {
            return rv;
          }

          // do we have CSS that changes selection behaviour?
          PRBool changeSelection = PR_FALSE;
          nsCOMPtr<nsIContent> selectContent;
          PRInt32 newStart = 0, newEnd = 0;
          nsresult rv2 =
            frameselection->AdjustOffsetsFromStyle(this, &changeSelection,
                                                   getter_AddRefs(selectContent),
                                                   &newStart, &newEnd);
          if (NS_SUCCEEDED(rv2) && changeSelection) {
            content = selectContent;
            startOffset = newStart;
            endOffset = newEnd;
          }
        } else {
          GetDataForTableSelection(frameselection,
                                   aPresContext->GetPresShell(),
                                   (nsMouseEvent *)aEvent,
                                   getter_AddRefs(content),
                                   &contentOffsetForTableSel,
                                   &targetForTableSel);
        }
      }
    }
  }

  // We might be capturing in some other document and the event just happened to
  // trickle down here. Make sure that document's frame selection is notified.
  if (activeFrame != this &&
      NS_STATIC_CAST(nsFrame*, activeFrame)->DisplaySelection(activeFrame->GetPresContext())
        != nsISelectionController::SELECTION_OFF) {
    nsCOMPtr<nsIFrameSelection> activeSelection;
    nsCOMPtr<nsISelectionController> activeSelCon;
    GetFrameSelectionFor(activeFrame, getter_AddRefs(activeSelection),
                         getter_AddRefs(activeSelCon));
    activeSelection->SetMouseDownState(PR_FALSE);
    activeSelection->StopAutoScrollTimer();
  }

  // Do not call any methods of the current object after this point!!!
  // The object is perhaps dead!

  return selectionOff
    ? NS_OK
    : HandleFrameSelection(frameselection, content, startOffset, endOffset,
                           beginFrameContent, handleTableSelection,
                           contentOffsetForTableSel, targetForTableSel,
                           aEvent, aEventStatus);
}


nsresult nsFrame::GetContentAndOffsetsFromPoint(nsPresContext* aCX,
                                                const nsPoint&  aPoint,
                                                nsIContent **   aNewContent,
                                                PRInt32&        aContentOffset,
                                                PRInt32&        aContentOffsetEnd,
                                                PRBool&         aBeginFrameContent)
{
  if (!aNewContent)
    return NS_ERROR_NULL_POINTER;

  // Traverse through children and look for the best one to give this
  // to if it fails the getposition call, make it yourself also only
  // look at primary list
  nsIFrame *closestFrame = nsnull;
  nsIView *view = GetClosestView();
  nsIFrame *kid = GetFirstChild(nsnull);

  if (kid) {
#define HUGE_DISTANCE 999999 //some HUGE number that will always fail first comparison

    PRInt32 closestXDistance = HUGE_DISTANCE;
    PRInt32 closestYDistance = HUGE_DISTANCE;

    while (nsnull != kid) {

      // Skip over generated content kid frames, or frames
      // that don't have a proper parent-child relationship!

      PRBool skipThisKid = (kid->GetStateBits() & NS_FRAME_GENERATED_CONTENT) != 0;
#if 0
      if (!skipThisKid) {
        // The frame's content is not generated. Now check
        // if it is anonymous content!

        nsIContent* kidContent = kid->GetContent();
        if (kidContent) {
          nsCOMPtr<nsIContent> content = kidContent->GetParent();

          if (content) {
            PRInt32 kidCount = content->ChildCount();
            PRInt32 kidIndex = content->IndexOf(kidContent);

            // IndexOf() should return -1 for the index if it doesn't
            // find kidContent in it's child list.

            if (kidIndex < 0 || kidIndex >= kidCount) {
              // Must be anonymous content! So skip it!
              skipThisKid = PR_TRUE;
            }
          }
        }
      }
#endif //XXX we USED to skip anonymous content i dont think we should anymore leaving this here as a flah

      if (skipThisKid) {
        kid = kid->GetNextSibling();
        continue;
      }

      // Kid frame has content that has a proper parent-child
      // relationship. Now see if the aPoint inside it's bounding
      // rect or close by.

      nsPoint offsetPoint(0,0);
      nsIView * kidView = nsnull;
      kid->GetOffsetFromView(offsetPoint, &kidView);

      nsRect rect = kid->GetRect();
      rect.x = offsetPoint.x;
      rect.y = offsetPoint.y;

      nscoord fromTop = aPoint.y - rect.y;
      nscoord fromBottom = aPoint.y - rect.y - rect.height;

      PRInt32 yDistance;
      if (fromTop > 0 && fromBottom < 0)
        yDistance = 0;
      else
        yDistance = PR_MIN(abs(fromTop), abs(fromBottom));

      if (yDistance <= closestYDistance && rect.width > 0 && rect.height > 0)
      {
        if (yDistance < closestYDistance)
          closestXDistance = HUGE_DISTANCE;

        nscoord fromLeft = aPoint.x - rect.x;
        nscoord fromRight = aPoint.x - rect.x - rect.width;

        PRInt32 xDistance;
        if (fromLeft > 0 && fromRight < 0)
          xDistance = 0;
        else
          xDistance = PR_MIN(abs(fromLeft), abs(fromRight));

        if (xDistance == 0 && yDistance == 0)
        {
          closestFrame = kid;
          break;
        }

        if (xDistance < closestXDistance || (xDistance == closestXDistance && rect.x <= aPoint.x))
        {
          // If we are only near (not directly over) then don't traverse a frame with independent
          // selection (e.g. text and list controls) unless we're already inside such a frame,
          // except in "browsewithcaret" mode, bug 268497.
          if (!(kid->GetStateBits() & NS_FRAME_INDEPENDENT_SELECTION) ||
              (GetStateBits() & NS_FRAME_INDEPENDENT_SELECTION) ||
              nsContentUtils::GetBoolPref("accessibility.browsewithcaret")) {
            closestXDistance = xDistance;
            closestYDistance = yDistance;
            closestFrame     = kid;
          }
        }
        // else if (xDistance > closestXDistance)
        //   break;//done
      }
    
      kid = kid->GetNextSibling();
    }
    if (closestFrame) {

      // If we cross a view boundary, we need to adjust
      // the coordinates because GetPosition() expects
      // them to be relative to the closest view.

      nsPoint newPoint     = aPoint;
      nsIView *closestView = closestFrame->GetClosestView();

      if (closestView && view != closestView)
        newPoint -= closestView->GetOffsetTo(view);

      // printf("      0x%.8x   0x%.8x  %4d  %4d\n",
      //        closestFrame, closestView, closestXDistance, closestYDistance);

      return closestFrame->GetContentAndOffsetsFromPoint(aCX, newPoint, aNewContent,
                                                         aContentOffset, aContentOffsetEnd,aBeginFrameContent);
    }
  }

  if (!mContent)
    return NS_ERROR_NULL_POINTER;

  nsPoint offsetPoint;
  GetOffsetFromView(offsetPoint, &view);
  nsRect thisRect = GetRect();
  thisRect.x = offsetPoint.x;
  thisRect.y = offsetPoint.y;

  NS_IF_ADDREF(*aNewContent = mContent->GetParent());
  if (*aNewContent){
    
    PRInt32 contentOffset(aContentOffset); //temp to hold old value in case of failure
    
    contentOffset = (*aNewContent)->IndexOf(mContent);
    if (contentOffset < 0) 
    {
      return NS_ERROR_FAILURE;
    }
    aContentOffset = contentOffset; //its clear save the result

    aBeginFrameContent = PR_TRUE;
    if (thisRect.Contains(aPoint))
      aContentOffsetEnd = aContentOffset +1;
    else 
    {
      //if we are a collapsed frame then dont check to see if we need to skip past this content
      //see bug http://bugzilla.mozilla.org/show_bug.cgi?id=103888
      if (thisRect.width && thisRect.height && ((thisRect.x + thisRect.width) < aPoint.x  || thisRect.y > aPoint.y))
      {
        aBeginFrameContent = PR_FALSE;
        aContentOffset++;
      }
      aContentOffsetEnd = aContentOffset;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetCursor(const nsPoint& aPoint,
                   nsIFrame::Cursor& aCursor)
{
  FillCursorInformationFromStyle(GetStyleUserInterface(), aCursor);
  if (NS_STYLE_CURSOR_AUTO == aCursor.mCursor) {
    aCursor.mCursor = NS_STYLE_CURSOR_DEFAULT;
  }


  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetFrameForPoint(const nsPoint& aPoint,
                          nsFramePaintLayer aWhichLayer,
                          nsIFrame** aFrame)
{
  if ((aWhichLayer == NS_FRAME_PAINT_LAYER_FOREGROUND) &&
      (mRect.Contains(aPoint))) {
    if (GetStyleVisibility()->IsVisible()) {
      *aFrame = this;
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

// Resize and incremental reflow

// nsIHTMLReflow member functions

NS_IMETHODIMP
nsFrame::WillReflow(nsPresContext* aPresContext)
{
#ifdef DEBUG_dbaron_off
  // bug 81268
  NS_ASSERTION(!(mState & NS_FRAME_IN_REFLOW),
               "nsFrame::WillReflow: frame is already in reflow");
#endif

  NS_FRAME_TRACE_MSG(NS_FRAME_TRACE_CALLS,
                     ("WillReflow: oldState=%x", mState));
  mState |= NS_FRAME_IN_REFLOW;
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::DidReflow(nsPresContext*           aPresContext,
                   const nsHTMLReflowState*  aReflowState,
                   nsDidReflowStatus         aStatus)
{
  NS_FRAME_TRACE_MSG(NS_FRAME_TRACE_CALLS,
                     ("nsFrame::DidReflow: aStatus=%d", aStatus));
  if (NS_FRAME_REFLOW_FINISHED == aStatus) {
    mState &= ~(NS_FRAME_IN_REFLOW | NS_FRAME_FIRST_REFLOW | NS_FRAME_IS_DIRTY |
                NS_FRAME_HAS_DIRTY_CHILDREN);
  }

  // Notify the percent height observer if this is an initial or resize reflow (XXX
  // it should probably be any type of reflow, but this would need further testing)
  // and there is a percent height but no computed height. The observer may be able to
  // initiate another reflow with a computed height. This happens in the case where a table
  // cell has no computed height but can fabricate one when the cell height is known.
  if (aReflowState && (aReflowState->mPercentHeightObserver)           && // an observer
      ((eReflowReason_Initial == aReflowState->reason) ||                 // initial or resize reflow
       (eReflowReason_Resize  == aReflowState->reason))                &&
      ((NS_UNCONSTRAINEDSIZE == aReflowState->mComputedHeight) ||         // no computed height 
       (0                    == aReflowState->mComputedHeight))        && 
      aReflowState->mStylePosition                                     && // percent height
      (eStyleUnit_Percent == aReflowState->mStylePosition->mHeight.GetUnit())) {

    nsIFrame* prevInFlow = GetPrevInFlow();
    if (!prevInFlow) { // 1st in flow
      aReflowState->mPercentHeightObserver->NotifyPercentHeight(*aReflowState);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFrame::CanContinueTextRun(PRBool& aContinueTextRun) const
{
  // By default, a frame will *not* allow a text run to be continued
  // through it.
  aContinueTextRun = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::Reflow(nsPresContext*          aPresContext,
                nsHTMLReflowMetrics&     aDesiredSize,
                const nsHTMLReflowState& aReflowState,
                nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsFrame", aReflowState.reason);
  aDesiredSize.width = 0;
  aDesiredSize.height = 0;
  aDesiredSize.ascent = 0;
  aDesiredSize.descent = 0;
  if (aDesiredSize.mComputeMEW) {
    aDesiredSize.mMaxElementWidth = 0;
  }
  aStatus = NS_FRAME_COMPLETE;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::AdjustFrameSize(nscoord aExtraSpace, nscoord& aUsedSpace)
{
  aUsedSpace = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::TrimTrailingWhiteSpace(nsPresContext* aPresContext,
                                nsIRenderingContext& aRC,
                                nscoord& aDeltaWidth,
                                PRBool& aLastCharIsJustifiable)
{
  aDeltaWidth = 0;
  aLastCharIsJustifiable = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::CharacterDataChanged(nsPresContext* aPresContext,
                              nsIContent*     aChild,
                              PRBool          aAppend)
{
  NS_NOTREACHED("should only be called for text frames");
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::AttributeChanged(nsIContent*     aChild,
                          PRInt32         aNameSpaceID,
                          nsIAtom*        aAttribute,
                          PRInt32         aModType)
{
  return NS_OK;
}

// Flow member functions

NS_IMETHODIMP nsFrame::IsSplittable(nsSplittableType& aIsSplittable) const
{
  aIsSplittable = NS_FRAME_NOT_SPLITTABLE;
  return NS_OK;
}

nsIFrame* nsFrame::GetPrevInFlow() const
{
  return nsnull;
}

NS_IMETHODIMP nsFrame::SetPrevInFlow(nsIFrame* aPrevInFlow)
{
  // Ignore harmless requests to set it to NULL
  if (aPrevInFlow) {
    NS_ERROR("not splittable");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  return NS_OK;
}

nsIFrame* nsFrame::GetNextInFlow() const
{
  return nsnull;
}

NS_IMETHODIMP nsFrame::SetNextInFlow(nsIFrame*)
{
  NS_ERROR("not splittable");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsIView*
nsIFrame::GetParentViewForChildFrame(nsIFrame* aFrame) const
{
  return GetClosestView();
}

// Associated view object
nsIView*
nsIFrame::GetView() const
{
  // Check the frame state bit and see if the frame has a view
  if (!(GetStateBits() & NS_FRAME_HAS_VIEW))
    return nsnull;

  // Check for a property on the frame
  nsresult rv;
  void *value = GetProperty(nsLayoutAtoms::viewProperty, &rv);

  NS_ENSURE_SUCCESS(rv, nsnull);
  NS_ASSERTION(value, "frame state bit was set but frame has no view");
  return NS_STATIC_CAST(nsIView*, value);
}

/* virtual */ nsIView*
nsIFrame::GetViewExternal() const
{
  return GetView();
}

nsresult
nsIFrame::SetView(nsIView* aView)
{
  if (aView) {
    aView->SetClientData(this);

    // Set a property on the frame
    nsresult rv = SetProperty(nsLayoutAtoms::viewProperty, aView, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);

    // Set the frame state bit that says the frame has a view
    AddStateBits(NS_FRAME_HAS_VIEW);

    // Let all of the ancestors know they have a descendant with a view.
    for (nsIFrame* f = GetParent();
         f && !(f->GetStateBits() & NS_FRAME_HAS_CHILD_WITH_VIEW);
         f = f->GetParent())
      f->AddStateBits(NS_FRAME_HAS_CHILD_WITH_VIEW);
  }

  return NS_OK;
}

nsIFrame* nsIFrame::GetAncestorWithViewExternal() const
{
  return GetAncestorWithView();
}

// Find the first geometric parent that has a view
nsIFrame* nsIFrame::GetAncestorWithView() const
{
  for (nsIFrame* f = mParent; nsnull != f; f = f->GetParent()) {
    if (f->HasView()) {
      return f;
    }
  }
  return nsnull;
}

// virtual
nsPoint nsIFrame::GetOffsetToExternal(const nsIFrame* aOther) const
{
  return GetOffsetTo(aOther);
}

nsPoint nsIFrame::GetOffsetTo(const nsIFrame* aOther) const
{
  NS_PRECONDITION(aOther,
                  "Must have frame for destination coordinate system!");
  // Note that if we hit a view while walking up the frame tree we need to stop
  // and switch to traversing the view tree so that we will deal with scroll
  // views properly.
  nsPoint offset(0, 0);
  const nsIFrame* f;
  for (f = this; !f->HasView() && f != aOther; f = f->GetParent()) {
    offset += f->GetPosition();
  }
  
  if (f != aOther) {
    // We found a view.  Switch to the view tree
    nsPoint toViewOffset(0, 0);
    nsIView* otherView = aOther->GetClosestView(&toViewOffset);
    offset += f->GetView()->GetOffsetTo(otherView) - toViewOffset;
  }
  
  return offset;
}

// virtual
nsIntRect nsIFrame::GetScreenRectExternal() const
{
  return GetScreenRect();
}

nsIntRect nsIFrame::GetScreenRect() const
{
  nsIntRect retval(0,0,0,0);
  nsPoint toViewOffset(0,0);
  nsIView* view = GetClosestView(&toViewOffset);

  if (view) {
    nsPoint toWidgetOffset(0,0);
    nsIWidget* widget = view->GetNearestWidget(&toWidgetOffset);

    if (widget) {
      nsRect ourRect = mRect;
      ourRect.MoveTo(toViewOffset + toWidgetOffset);
      ourRect.ScaleRoundOut(GetPresContext()->TwipsToPixels());
      // Is it safe to pass the same rect for both args of WidgetToScreen?
      // It's not clear, so let's not...
      nsIntRect ourPxRect(ourRect.x, ourRect.y, ourRect.width, ourRect.height);
      
      widget->WidgetToScreen(ourPxRect, retval);
    }
  }

  return retval;
}

// Returns the offset from this frame to the closest geometric parent that
// has a view. Also returns the containing view or null in case of error
NS_IMETHODIMP nsFrame::GetOffsetFromView(nsPoint&  aOffset,
                                         nsIView** aView) const
{
  NS_PRECONDITION(nsnull != aView, "null OUT parameter pointer");
  nsIFrame* frame = (nsIFrame*)this;

  *aView = nsnull;
  aOffset.MoveTo(0, 0);
  do {
    aOffset += frame->GetPosition();
    frame = frame->GetParent();
  } while (frame && !frame->HasView());
  if (frame)
    *aView = frame->GetView();
  return NS_OK;
}

// The (x,y) value of the frame's upper left corner is always
// relative to its parentFrame's upper left corner, unless
// its parentFrame has a view associated with it, in which case, it
// will be relative to the upper left corner of the view returned
// by a call to parentFrame->GetView().
//
// This means that while drilling down the frame hierarchy, from
// parent to child frame, we sometimes need to take into account
// crossing these view boundaries, because the coordinate system
// changes from parent frame coordinate system, to the associated
// view's coordinate system.
//
// GetOriginToViewOffset() is a utility method that returns the
// offset necessary to map a point, relative to the frame's upper
// left corner, into the coordinate system of the view associated
// with the frame.
//
// If there is no view associated with the frame, or the view is
// not a descendant of the frame's parent view (ex: scrolling popup menu),
// the offset returned will be (0,0).

NS_IMETHODIMP nsFrame::GetOriginToViewOffset(nsPoint&        aOffset,
                                             nsIView**       aView) const
{
  nsresult rv = NS_OK;

  aOffset.MoveTo(0,0);

  if (aView)
    *aView = nsnull;

  if (HasView()) {
    nsIView *view = GetView();
    nsIView *parentView = nsnull;
    nsPoint offsetToParentView;
    rv = GetOffsetFromView(offsetToParentView, &parentView);

    if (NS_SUCCEEDED(rv)) {
      nsPoint viewOffsetFromParent(0,0);
      nsIView *pview = view;

      nsIViewManager* vVM = view->GetViewManager();

      while (pview && pview != parentView) {
        viewOffsetFromParent += pview->GetPosition();

        nsIView *tmpView = pview->GetParent();
        if (tmpView && vVM != tmpView->GetViewManager()) {
          // Don't cross ViewManager boundaries!
          // XXXbz why not?
          break;
        }
        pview = tmpView;
      }

#ifdef DEBUG_KIN
      if (pview != parentView) {
        // XXX: At this point, pview is probably null since it traversed
        //      all the way up view's parent hierarchy and did not run across
        //      parentView. In the future, instead of just returning an offset
        //      of (0,0) for this case, we may want offsetToParentView to
        //      include the offset from the parentView to the top of the
        //      view hierarchy which would make both offsetToParentView and
        //      viewOffsetFromParent, offsets to the global coordinate space.
        //      We'd have to investigate any perf impact this would have before
        //      checking in such a change, so for now we just return (0,0).
        //      -- kin    
        NS_WARNING("view is not a descendant of parentView!");
      }
#endif // DEBUG

      if (pview == parentView)
        aOffset = offsetToParentView - viewOffsetFromParent;

      if (aView)
        *aView = view;
    }
  }

  return rv;
}

/* virtual */ PRBool
nsIFrame::AreAncestorViewsVisible() const
{
  for (nsIView* view = GetClosestView(); view; view = view->GetParent()) {
    if (view->GetVisibility() == nsViewVisibility_kHide) {
      return PR_FALSE;
    }
  }
  return PR_TRUE;
}

nsIWidget*
nsIFrame::GetWindow() const
{
  return GetClosestView()->GetNearestWidget(nsnull);
}

nsIAtom*
nsFrame::GetType() const
{
  return nsnull;
}

PRBool
nsIFrame::IsLeaf() const
{
  return PR_TRUE;
}

void
nsIFrame::Invalidate(const nsRect& aDamageRect,
                     PRBool        aImmediate) const
{
  if (aDamageRect.IsEmpty()) {
    return;
  }

  // Don't allow invalidates to do anything when
  // painting is suppressed.
  nsIPresShell *shell = GetPresContext()->GetPresShell();
  if (shell) {
    PRBool suppressed = PR_FALSE;
    shell->IsPaintingSuppressed(&suppressed);
    if (suppressed)
      return;
  }

  nsRect damageRect(aDamageRect);

  PRUint32 flags = aImmediate ? NS_VMREFRESH_IMMEDIATE : NS_VMREFRESH_NO_SYNC;
  if (HasView()) {
    nsIView* view = GetView();
    view->GetViewManager()->UpdateView(view, damageRect, flags);
  } else {
    nsRect    rect(damageRect);
    nsPoint   offset;
  
    nsIView *view;
    GetOffsetFromView(offset, &view);
    NS_ASSERTION(view, "no view");
    rect += offset;
    view->GetViewManager()->UpdateView(view, rect, flags);
  }
}

static nsRect ComputeOutlineRect(const nsIFrame* aFrame, PRBool* aAnyOutline,
                                 const nsRect& aOverflowRect) {
  const nsStyleOutline* outline = aFrame->GetStyleOutline();
  PRUint8 outlineStyle = outline->GetOutlineStyle();
  nsRect r = aOverflowRect;
  *aAnyOutline = PR_FALSE;
  if (outlineStyle != NS_STYLE_BORDER_STYLE_NONE) {
    nscoord width;
#ifdef DEBUG
    PRBool result = 
#endif
      outline->GetOutlineWidth(width);
    NS_ASSERTION(result, "GetOutlineWidth had no cached outline width");
    if (width > 0) {
      nscoord offset;
      outline->GetOutlineOffset(offset);
      nscoord inflateBy = PR_MAX(width + offset, 0);
      r.Inflate(inflateBy, inflateBy);
      *aAnyOutline = PR_TRUE;
    }
  }
  return r;
}

nsRect
nsIFrame::GetOverflowRect() const
{
  // Note that in some cases the overflow area might not have been
  // updated (yet) to reflect any outline set on the frame or the area
  // of child frames. That's OK because any reflow that updates these
  // areas will invalidate the appropriate area, so any (mis)uses of
  // this method will be fixed up.
  nsRect* storedOA = NS_CONST_CAST(nsIFrame*, this)
    ->GetOverflowAreaProperty(PR_FALSE);
  if (storedOA) {
    return *storedOA;
  } else {
    return nsRect(nsPoint(0, 0), GetSize());
  }
}
  
void
nsFrame::CheckInvalidateSizeChange(nsPresContext* aPresContext,
                                   nsHTMLReflowMetrics& aDesiredSize,
                                   const nsHTMLReflowState& aReflowState)
{
  if (aDesiredSize.width == mRect.width
      && aDesiredSize.height == mRect.height)
    return;

  // Below, we invalidate the old frame area (or, in the case of
  // outline, combined area) if the outline, border or background
  // settings indicate that something other than the difference
  // between the old and new areas needs to be painted. We are
  // assuming that the difference between the old and new areas will
  // be invalidated by some other means. That also means invalidating
  // the old frame area is the same as invalidating the new frame area
  // (since in either case the UNION of old and new areas will be
  // invalidated)

  // Invalidate the entire old frame+outline if the frame has an outline
  PRBool anyOutline;
  nsRect r = ComputeOutlineRect(this, &anyOutline,
                                aDesiredSize.mOverflowArea);
  if (anyOutline) {
    Invalidate(r);
    return;
  }

  // Invalidate the old frame borders if the frame has borders. Those borders
  // may be moving.
  const nsStyleBorder* border = GetStyleBorder();
  NS_FOR_CSS_SIDES(side) {
    if (border->GetBorderWidth(side) != 0) {
      Invalidate(nsRect(0, 0, mRect.width, mRect.height));
      return;
    }
  }

  // Invalidate the old frame background if the frame has a background
  // whose position depends on the size of the frame
  const nsStyleBackground* background = GetStyleBackground();
  if (background->mBackgroundFlags &
      (NS_STYLE_BG_X_POSITION_PERCENT | NS_STYLE_BG_Y_POSITION_PERCENT)) {
    Invalidate(nsRect(0, 0, mRect.width, mRect.height));
    return;
  }
}

// Define the MAX_FRAME_DEPTH to be the ContentSink's MAX_REFLOW_DEPTH plus
// 4 for the frames above the document's frames: 
//  the Viewport, GFXScroll, ScrollPort, and Canvas
#define MAX_FRAME_DEPTH (MAX_REFLOW_DEPTH+4)

PRBool
nsFrame::IsFrameTreeTooDeep(const nsHTMLReflowState& aReflowState,
                            nsHTMLReflowMetrics& aMetrics)
{
  if (aReflowState.mReflowDepth >  MAX_FRAME_DEPTH) {
    mState |= NS_FRAME_IS_UNFLOWABLE;
    mState &= ~NS_FRAME_OUTSIDE_CHILDREN;
    aMetrics.width = 0;
    aMetrics.height = 0;
    aMetrics.ascent = 0;
    aMetrics.descent = 0;
    aMetrics.mCarriedOutBottomMargin.Zero();
    aMetrics.mOverflowArea.x = 0;
    aMetrics.mOverflowArea.y = 0;
    aMetrics.mOverflowArea.width = 0;
    aMetrics.mOverflowArea.height = 0;
    if (aMetrics.mComputeMEW) {
      aMetrics.mMaxElementWidth = 0;
    }
    return PR_TRUE;
  }
  mState &= ~NS_FRAME_IS_UNFLOWABLE;
  return PR_FALSE;
}

// Style sizing methods
/* virtual */ PRBool nsFrame::IsContainingBlock() const
{
  const nsStyleDisplay* display = GetStyleDisplay();

  // Absolute positioning causes |display->mDisplay| to be set to block,
  // if needed.
  return display->mDisplay == NS_STYLE_DISPLAY_BLOCK || 
         display->mDisplay == NS_STYLE_DISPLAY_LIST_ITEM ||
         display->mDisplay == NS_STYLE_DISPLAY_TABLE_CELL;
}

#ifdef NS_DEBUG

PRInt32 nsFrame::ContentIndexInContainer(const nsIFrame* aFrame)
{
  PRInt32 result = -1;

  nsIContent* content = aFrame->GetContent();
  if (content) {
    nsIContent* parentContent = content->GetParent();
    if (parentContent) {
      result = parentContent->IndexOf(content);
    }
  }

  return result;
}

#ifdef DEBUG_waterson

/**
 * List a single frame to stdout. Meant to be called from gdb.
 */
void
DebugListFrame(nsPresContext* aPresContext, nsIFrame* aFrame)
{
  ((nsFrame*) aFrame)->List(aPresContext, stdout, 0);
  printf("\n");
}

/**
 * List a frame tree to stdout. Meant to be called from gdb.
 */
void
DebugListFrameTree(nsPresContext* aPresContext, nsIFrame* aFrame)
{
  nsIFrameDebug* fdbg;
  aFrame->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**) &fdbg);
  if (fdbg)
    fdbg->List(aPresContext, stdout, 0);
}

#endif


// Debugging
NS_IMETHODIMP
nsFrame::List(nsPresContext* aPresContext, FILE* out, PRInt32 aIndent) const
{
  IndentBy(out, aIndent);
  ListTag(out);
#ifdef DEBUG_waterson
  fprintf(out, " [parent=%p]", NS_STATIC_CAST(void*, mParent));
#endif
  if (HasView()) {
    fprintf(out, " [view=%p]", NS_STATIC_CAST(void*, GetView()));
  }
  fprintf(out, " {%d,%d,%d,%d}", mRect.x, mRect.y, mRect.width, mRect.height);
  if (0 != mState) {
    fprintf(out, " [state=%08x]", mState);
  }
  nsIFrame* prevInFlow = GetPrevInFlow();
  nsIFrame* nextInFlow = GetNextInFlow();
  if (nsnull != prevInFlow) {
    fprintf(out, " prev-in-flow=%p", NS_STATIC_CAST(void*, prevInFlow));
  }
  if (nsnull != nextInFlow) {
    fprintf(out, " next-in-flow=%p", NS_STATIC_CAST(void*, nextInFlow));
  }
  fprintf(out, " [content=%p]", NS_STATIC_CAST(void*, mContent));
  nsFrame* f = NS_CONST_CAST(nsFrame*, this);
  nsRect* overflowArea = f->GetOverflowAreaProperty(PR_FALSE);
  if (overflowArea) {
    fprintf(out, " [overflow=%d,%d,%d,%d]", overflowArea->x, overflowArea->y,
            overflowArea->width, overflowArea->height);
  }
  fputs("\n", out);
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Frame"), aResult);
}

NS_IMETHODIMP_(nsFrameState)
nsFrame::GetDebugStateBits() const
{
  // We'll ignore these flags for the purposes of comparing frame state:
  //
  //   NS_FRAME_EXTERNAL_REFERENCE
  //     because this is set by the event state manager or the
  //     caret code when a frame is focused. Depending on whether
  //     or not the regression tests are run as the focused window
  //     will make this value vary randomly.
#define IRRELEVANT_FRAME_STATE_FLAGS NS_FRAME_EXTERNAL_REFERENCE

#define FRAME_STATE_MASK (~(IRRELEVANT_FRAME_STATE_FLAGS))

  return GetStateBits() & FRAME_STATE_MASK;
}

nsresult
nsFrame::MakeFrameName(const nsAString& aType, nsAString& aResult) const
{
  aResult = aType;
  if (mContent && !mContent->IsContentOfType(nsIContent::eTEXT)) {
    nsAutoString buf;
    mContent->Tag()->ToString(buf);
    aResult.Append(NS_LITERAL_STRING("(") + buf + NS_LITERAL_STRING(")"));
  }
  char buf[40];
  PR_snprintf(buf, sizeof(buf), "(%d)", ContentIndexInContainer(this));
  AppendASCIItoUTF16(buf, aResult);
  return NS_OK;
}

void
nsFrame::XMLQuote(nsString& aString)
{
  PRInt32 i, len = aString.Length();
  for (i = 0; i < len; i++) {
    PRUnichar ch = aString.CharAt(i);
    if (ch == '<') {
      nsAutoString tmp(NS_LITERAL_STRING("&lt;"));
      aString.Cut(i, 1);
      aString.Insert(tmp, i);
      len += 3;
      i += 3;
    }
    else if (ch == '>') {
      nsAutoString tmp(NS_LITERAL_STRING("&gt;"));
      aString.Cut(i, 1);
      aString.Insert(tmp, i);
      len += 3;
      i += 3;
    }
    else if (ch == '\"') {
      nsAutoString tmp(NS_LITERAL_STRING("&quot;"));
      aString.Cut(i, 1);
      aString.Insert(tmp, i);
      len += 5;
      i += 5;
    }
  }
}
#endif

PRBool
nsFrame::ParentDisablesSelection() const
{
/*
  // should never be called now
  nsIFrame* parent = GetParent();
  if (parent) {
	  PRBool selectable;
	  parent->IsSelectable(selectable);
    return (selectable ? PR_FALSE : PR_TRUE);
  }
  return PR_FALSE;
*/
/*
  PRBool selected;
  if (NS_FAILED(GetSelected(&selected)))
    return PR_FALSE;
  if (selected)
    return PR_FALSE; //if this frame is selected and no one has overridden the selection from "higher up"
                     //then no one below us will be disabled by this frame.
  nsIFrame* target = GetParent();
  if (target)
    return ((nsFrame *)target)->ParentDisablesSelection();
  return PR_FALSE; //default this does not happen
  */
  
  return PR_FALSE;
}

nsresult 
nsFrame::GetSelectionForVisCheck(nsPresContext * aPresContext, nsISelection** aSelection)
{
  *aSelection = nsnull;
  nsresult rv = NS_OK;

  // start by checking to see if we are paginated which probably means
  // we are in print preview or printing
  if (aPresContext->IsPaginated()) {
    // now see if we are rendering selection only
    if (aPresContext->IsRenderingOnlySelection()) {
      // Check the quick way first (typically only leaf nodes)
      PRBool isSelected = (mState & NS_FRAME_SELECTED_CONTENT) == NS_FRAME_SELECTED_CONTENT;
      // if we aren't selected in the mState,
      // we could be a container so check to see if we are in the selection range
      // this is a expensive
      if (!isSelected) {
        nsIPresShell *shell = aPresContext->GetPresShell();
        if (shell) {
          nsCOMPtr<nsISelectionController> selcon(do_QueryInterface(shell));
          if (selcon) {
            rv = selcon->GetSelection(nsISelectionController::SELECTION_NORMAL, aSelection);
          }
        }
      }
    }
  } 

  return rv;
}


NS_IMETHODIMP
nsFrame::IsVisibleForPainting(nsPresContext *     aPresContext, 
                              nsIRenderingContext& aRenderingContext,
                              PRBool               aCheckVis,
                              PRBool*              aIsVisible)
{
  // first check to see if we are visible
  if (aCheckVis) {
    if (!GetStyleVisibility()->IsVisible()) {
      *aIsVisible = PR_FALSE;
      return NS_OK;
    }
  }

  // Start by assuming we are visible and need to be painted
  *aIsVisible = PR_TRUE;

  // NOTE: GetSelectionforVisCheck checks the pagination to make sure we are printing
  // In otherwords, the selection will ALWAYS be null if we are not printing, meaning
  // the visibility will be TRUE in that case
  nsCOMPtr<nsISelection> selection;
  nsresult rv = GetSelectionForVisCheck(aPresContext, getter_AddRefs(selection));
  if (NS_SUCCEEDED(rv) && selection) {
    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(mContent));
    selection->ContainsNode(node, PR_TRUE, aIsVisible);
  }

  return rv;
}

/* virtual */ PRBool
nsFrame::IsEmpty()
{
  return PR_FALSE;
}

/* virtual */ PRBool
nsFrame::IsSelfEmpty()
{
  return PR_FALSE;
}

NS_IMETHODIMP
nsFrame::GetSelectionController(nsPresContext *aPresContext, nsISelectionController **aSelCon)
{
  if (!aPresContext || !aSelCon)
    return NS_ERROR_INVALID_ARG;

  nsIFrame *frame = this;
  while (frame && (frame->GetStateBits() & NS_FRAME_INDEPENDENT_SELECTION)) {
    nsITextControlFrame *tcf;
    if (NS_SUCCEEDED(frame->QueryInterface(NS_GET_IID(nsITextControlFrame),(void**)&tcf))) {
      return tcf->GetSelectionContr(aSelCon);
    }
    frame = frame->GetParent();
  }

  nsIPresShell *shell = aPresContext->GetPresShell();
  if (shell) {
    nsCOMPtr<nsISelectionController> selCon = do_QueryInterface(shell);
    NS_IF_ADDREF(*aSelCon = selCon);
  }

  return NS_OK;
}



#ifdef NS_DEBUG
NS_IMETHODIMP
nsFrame::DumpRegressionData(nsPresContext* aPresContext, FILE* out, PRInt32 aIndent, PRBool aIncludeStyleData)
{
  IndentBy(out, aIndent);
  fprintf(out, "<frame va=\"%ld\" type=\"", PRUptrdiff(this));
  nsAutoString name;
  GetFrameName(name);
  XMLQuote(name);
  fputs(NS_LossyConvertUCS2toASCII(name).get(), out);
  fprintf(out, "\" state=\"%d\" parent=\"%ld\">\n",
          GetDebugStateBits(), PRUptrdiff(mParent));

  aIndent++;
  DumpBaseRegressionData(aPresContext, out, aIndent, aIncludeStyleData);
  aIndent--;

  IndentBy(out, aIndent);
  fprintf(out, "</frame>\n");

  return NS_OK;
}

void
nsFrame::DumpBaseRegressionData(nsPresContext* aPresContext, FILE* out, PRInt32 aIndent, PRBool aIncludeStyleData)
{
  if (nsnull != mNextSibling) {
    IndentBy(out, aIndent);
    fprintf(out, "<next-sibling va=\"%ld\"/>\n", PRUptrdiff(mNextSibling));
  }

  if (HasView()) {
    IndentBy(out, aIndent);
    fprintf(out, "<view va=\"%ld\">\n", PRUptrdiff(GetView()));
    aIndent++;
    // XXX add in code to dump out view state too...
    aIndent--;
    IndentBy(out, aIndent);
    fprintf(out, "</view>\n");
  }

  if(aIncludeStyleData) {
    if(mStyleContext) {
      IndentBy(out, aIndent);
      fprintf(out, "<stylecontext va=\"%ld\">\n", PRUptrdiff(mStyleContext));
      aIndent++;
      // Dump style context regression data
      mStyleContext->DumpRegressionData(aPresContext, out, aIndent);
      aIndent--;
      IndentBy(out, aIndent);
      fprintf(out, "</stylecontext>\n");
    }
  }

  IndentBy(out, aIndent);
  fprintf(out, "<bbox x=\"%d\" y=\"%d\" w=\"%d\" h=\"%d\"/>\n",
          mRect.x, mRect.y, mRect.width, mRect.height);

  // Now dump all of the children on all of the child lists
  nsIFrame* kid;
  nsIAtom* list = nsnull;
  PRInt32 listIndex = 0;
  do {
    kid = GetFirstChild(list);
    if (kid) {
      IndentBy(out, aIndent);
      if (nsnull != list) {
        nsAutoString listName;
        list->ToString(listName);
        fprintf(out, "<child-list name=\"");
        XMLQuote(listName);
        fputs(NS_LossyConvertUCS2toASCII(listName).get(), out);
        fprintf(out, "\">\n");
      }
      else {
        fprintf(out, "<child-list>\n");
      }
      aIndent++;
      while (kid) {
        nsIFrameDebug*  frameDebug;

        if (NS_SUCCEEDED(kid->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
          frameDebug->DumpRegressionData(aPresContext, out, aIndent, aIncludeStyleData);
        }
        kid = kid->GetNextSibling();
      }
      aIndent--;
      IndentBy(out, aIndent);
      fprintf(out, "</child-list>\n");
    }
    list = GetAdditionalChildListName(listIndex++);
  } while (nsnull != list);
}

NS_IMETHODIMP
nsFrame::VerifyTree() const
{
  NS_ASSERTION(0 == (mState & NS_FRAME_IN_REFLOW), "frame is in reflow");
  return NS_OK;
}
#endif

/*this method may.. invalidate if the state was changed or if aForceRedraw is PR_TRUE
  it will not update immediately.*/
NS_IMETHODIMP
nsFrame::SetSelected(nsPresContext* aPresContext, nsIDOMRange *aRange, PRBool aSelected, nsSpread aSpread)
{
/*
  if (aSelected && ParentDisablesSelection())
    return NS_OK;
*/

  // check whether style allows selection
  PRBool  selectable;
  IsSelectable(&selectable, nsnull);
  if (!selectable)
    return NS_OK;

/*
  if (eSpreadDown == aSpread){
    nsIFrame* kid = GetFirstChild(nsnull);
    while (nsnull != kid) {
      kid->SetSelected(nsnull,aSelected,aSpread);
      kid = kid->GetNextSibling();
    }
  }
*/
  if ( aSelected ){
    AddStateBits(NS_FRAME_SELECTED_CONTENT);
  }
  else
    RemoveStateBits(NS_FRAME_SELECTED_CONTENT);

  // Repaint this frame subtree's entire area
  Invalidate(GetOverflowRect(), PR_FALSE);

#ifdef IBMBIDI
  PRInt32 start, end;
  nsIFrame* frame = GetNextSibling();
  if (frame) {
    GetFirstLeaf(aPresContext, &frame);
    GetOffsets(start, end);
    if (start && end) {
      frame->SetSelected(aPresContext, aRange, aSelected, aSpread);
    }
  }
#endif // IBMBIDI

  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetSelected(PRBool *aSelected) const
{
  if (!aSelected )
    return NS_ERROR_NULL_POINTER;
  *aSelected = (PRBool)(mState & NS_FRAME_SELECTED_CONTENT);
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetPointFromOffset(nsPresContext* inPresContext, nsIRenderingContext* inRendContext, PRInt32 inOffset, nsPoint* outPoint)
{
  NS_PRECONDITION(outPoint != nsnull, "Null parameter");
  nsPoint bottomLeft(0, 0);
  if (mContent)
  {
    nsIContent* newContent = mContent->GetParent();
    if (newContent){
      PRInt32 newOffset = newContent->IndexOf(mContent);

      if (inOffset > newOffset)
        bottomLeft.x = GetRect().width;
    }
  }
  *outPoint = bottomLeft;
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetChildFrameContainingOffset(PRInt32 inContentOffset, PRBool inHint, PRInt32* outFrameContentOffset, nsIFrame **outChildFrame)
{
  NS_PRECONDITION(outChildFrame && outFrameContentOffset, "Null parameter");
  *outFrameContentOffset = (PRInt32)inHint;
  //the best frame to reflect any given offset would be a visible frame if possible
  //i.e. we are looking for a valid frame to place the blinking caret 
  nsRect rect = GetRect();
  if (!rect.width || !rect.height)
  {
    //if we have a 0 width or height then lets look for another frame that possibly has
    //the same content.  If we have no frames in flow then just let us return 'this' frame
    nsIFrame* nextFlow = GetNextInFlow();
    if (nextFlow)
      return nextFlow->GetChildFrameContainingOffset(inContentOffset, inHint, outFrameContentOffset, outChildFrame);
  }
  *outChildFrame = this;
  return NS_OK;
}

//
// What I've pieced together about this routine:
// Starting with a block frame (from which a line frame can be gotten)
// and a line number, drill down and get the first/last selectable
// frame on that line, depending on aPos->mDirection.
// aOutSideLimit != 0 means ignore aLineStart, instead work from
// the end (if > 0) or beginning (if < 0).
//
nsresult
nsFrame::GetNextPrevLineFromeBlockFrame(nsPresContext* aPresContext,
                                        nsPeekOffsetStruct *aPos,
                                        nsIFrame *aBlockFrame, 
                                        PRInt32 aLineStart, 
                                        PRInt8 aOutSideLimit
                                        )
{
  //magic numbers aLineStart will be -1 for end of block 0 will be start of block
  if (!aBlockFrame || !aPos)
    return NS_ERROR_NULL_POINTER;

  aPos->mResultFrame = nsnull;
  aPos->mResultContent = nsnull;
  aPos->mPreferLeft = (aPos->mDirection == eDirNext);

   nsresult result;
  nsCOMPtr<nsILineIteratorNavigator> it; 
  result = aBlockFrame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator),getter_AddRefs(it));
  if (NS_FAILED(result) || !it)
    return result;
  PRInt32 searchingLine = aLineStart;
  PRInt32 countLines;
  result = it->GetNumLines(&countLines);
  if (aOutSideLimit > 0) //start at end
    searchingLine = countLines;
  else if (aOutSideLimit <0)//start at begining
    searchingLine = -1;//"next" will be 0  
  else 
    if ((aPos->mDirection == eDirPrevious && searchingLine == 0) || 
       (aPos->mDirection == eDirNext && searchingLine >= (countLines -1) )){
      //we need to jump to new block frame.
           return NS_ERROR_FAILURE;
    }
  PRInt32 lineFrameCount;
  nsIFrame *resultFrame = nsnull;
  nsIFrame *farStoppingFrame = nsnull; //we keep searching until we find a "this" frame then we go to next line
  nsIFrame *nearStoppingFrame = nsnull; //if we are backing up from edge, stop here
  nsIFrame *firstFrame;
  nsIFrame *lastFrame;
  nsRect  rect;
  PRBool isBeforeFirstFrame, isAfterLastFrame;
  PRBool found = PR_FALSE;
  while (!found)
  {
    if (aPos->mDirection == eDirPrevious)
      searchingLine --;
    else
      searchingLine ++;
    if ((aPos->mDirection == eDirPrevious && searchingLine < 0) || 
       (aPos->mDirection == eDirNext && searchingLine >= countLines ))
    {
      //we need to jump to new block frame.
      return NS_ERROR_FAILURE;
    }
    PRUint32 lineFlags;
    result = it->GetLine(searchingLine, &firstFrame, &lineFrameCount,
                         rect, &lineFlags);
    if (!lineFrameCount) 
      continue;
    if (NS_SUCCEEDED(result)){
      lastFrame = firstFrame;
      for (;lineFrameCount > 1;lineFrameCount --){
        //result = lastFrame->GetNextSibling(&lastFrame, searchingLine);
        result = it->GetNextSiblingOnLine(lastFrame, searchingLine);
        if (NS_FAILED(result) || !lastFrame){
          NS_ASSERTION(0,"should not be reached nsFrame\n");
          return NS_ERROR_FAILURE;
        }
      }
      GetLastLeaf(aPresContext, &lastFrame);

      if (aPos->mDirection == eDirNext){
        nearStoppingFrame = firstFrame;
        farStoppingFrame = lastFrame;
      }
      else{
        nearStoppingFrame = lastFrame;
        farStoppingFrame = firstFrame;
      }
      nsPoint offset;
      nsIView * view; //used for call of get offset from view
      aBlockFrame->GetOffsetFromView(offset,&view);
      nscoord newDesiredX  = aPos->mDesiredX - offset.x;//get desired x into blockframe coordinates!
      result = it->FindFrameAt(searchingLine, newDesiredX, &resultFrame, &isBeforeFirstFrame, &isAfterLastFrame);
      if(NS_FAILED(result))
        continue;
    }

    if (NS_SUCCEEDED(result) && resultFrame)
    {
      nsCOMPtr<nsILineIteratorNavigator> newIt; 
      //check to see if this is ANOTHER blockframe inside the other one if so then call into its lines
      result = resultFrame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator),getter_AddRefs(newIt));
      if (NS_SUCCEEDED(result) && newIt)
      {
        aPos->mResultFrame = resultFrame;
        return NS_OK;
      }
      //resultFrame is not a block frame

      nsCOMPtr<nsIBidirectionalEnumerator> frameTraversal;
      result = NS_NewFrameTraversal(getter_AddRefs(frameTraversal), EXTENSIVE,
                                    aPresContext, resultFrame, aPos->mScrollViewStop);
      if (NS_FAILED(result))
        return result;
      nsISupports *isupports = nsnull;
      nsIFrame *storeOldResultFrame = resultFrame;
      while ( !found ){
        nsPresContext *context = aPos->mShell->GetPresContext();
        nsPoint point;
        point.x = aPos->mDesiredX;

        nsRect tempRect = resultFrame->GetRect();
        nsPoint offset;
        nsIView * view; //used for call of get offset from view
        result = resultFrame->GetOffsetFromView(offset, &view);
        if (NS_FAILED(result))
          return result;
        point.y = tempRect.height + offset.y;

        //special check. if we allow non-text selection then we can allow a hit location to fall before a table. 
        //otherwise there is no way to get and click signal to fall before a table (it being a line iterator itself)
        PRInt16 isEditor = 0;
        nsIPresShell *shell = aPresContext->GetPresShell();
        if (!shell)
          return NS_ERROR_FAILURE;
        shell->GetSelectionFlags ( &isEditor );
        isEditor = isEditor == nsISelectionDisplay::DISPLAY_ALL;
        if ( isEditor ) 
        {
          if (resultFrame->GetType() == nsLayoutAtoms::tableOuterFrame)
          {
            if (((point.x - offset.x + tempRect.x)<0) ||  ((point.x - offset.x+ tempRect.x)>tempRect.width))//off left/right side
            {
              nsIContent* content = resultFrame->GetContent();
              if (content)
              {
                nsIContent* parent = content->GetParent();
                if (parent)
                {
                  aPos->mResultContent = parent;
                  aPos->mContentOffset = parent->IndexOf(content);
                  aPos->mPreferLeft = PR_FALSE;
                  if ((point.x - offset.x+ tempRect.x)>tempRect.width)
                  {
                    aPos->mContentOffset++;//go to end of this frame
                    aPos->mPreferLeft = PR_TRUE;
                  }
                  aPos->mContentOffsetEnd = aPos->mContentOffset;
                  //result frame is the result frames parent.
                  aPos->mResultFrame = resultFrame->GetParent();
                  return NS_POSITION_BEFORE_TABLE;
                }
              }
            }
          }
        }

        if (!resultFrame->HasView())
        {
          rect = resultFrame->GetRect();
          result = resultFrame->GetContentAndOffsetsFromPoint(context,point,
                                        getter_AddRefs(aPos->mResultContent),
                                        aPos->mContentOffset,
                                        aPos->mContentOffsetEnd,
                                        aPos->mPreferLeft);
          if (NS_SUCCEEDED(result))
          {
            PRBool selectable;
            resultFrame->IsSelectable(&selectable, nsnull);
            if (selectable)
            {
              found = PR_TRUE;
              break;
            }
          }
        }

        if (aPos->mDirection == eDirPrevious && (resultFrame == farStoppingFrame))
          break;
        if (aPos->mDirection == eDirNext && (resultFrame == nearStoppingFrame))
          break;
        //always try previous on THAT line if that fails go the other way
        result = frameTraversal->Prev();
        if (NS_FAILED(result))
          break;
        result = frameTraversal->CurrentItem(&isupports);
        if (NS_FAILED(result) || !isupports)
          return result;
        //we must CAST here to an nsIFrame. nsIFrame doesnt really follow the rules
        resultFrame = (nsIFrame *)isupports;
      }

      if (!found){
        resultFrame = storeOldResultFrame;
        result = NS_NewFrameTraversal(getter_AddRefs(frameTraversal), LEAF,
                                      aPresContext, resultFrame, aPos->mScrollViewStop);
      }
      while ( !found ){
        nsPresContext *context = aPos->mShell->GetPresContext();

        nsPoint point;
        point.x = aPos->mDesiredX;
        point.y = 0;

        result = resultFrame->GetContentAndOffsetsFromPoint(context,point,
                                          getter_AddRefs(aPos->mResultContent), aPos->mContentOffset,
                                          aPos->mContentOffsetEnd, aPos->mPreferLeft);
        if (NS_SUCCEEDED(result))
        {
          PRBool selectable;
          resultFrame->IsSelectable(&selectable, nsnull);
          if (selectable)
          {
            found = PR_TRUE;
            if (resultFrame == farStoppingFrame)
              aPos->mPreferLeft = PR_FALSE;
            else
              aPos->mPreferLeft = PR_TRUE;
            break;
          }
        }
        if (aPos->mDirection == eDirPrevious && (resultFrame == nearStoppingFrame))
          break;
        if (aPos->mDirection == eDirNext && (resultFrame == farStoppingFrame))
          break;
        //previous didnt work now we try "next"
        result = frameTraversal->Next();
        if (NS_FAILED(result))
          break;
        result = frameTraversal->CurrentItem(&isupports);
        if (NS_FAILED(result) || !isupports)
          break;
        //we must CAST here to an nsIFrame. nsIFrame doesnt really follow the rules
        resultFrame = (nsIFrame *)isupports;
      }
      aPos->mResultFrame = resultFrame;
    }
    else {
        //we need to jump to new block frame.
      aPos->mAmount = eSelectLine;
      aPos->mStartOffset = 0;
      aPos->mEatingWS = PR_FALSE;
      aPos->mPreferLeft = !(aPos->mDirection == eDirNext);
      if (aPos->mDirection == eDirPrevious)
        aPos->mStartOffset = -1;//start from end
     return aBlockFrame->PeekOffset(aPresContext, aPos);
    }
  }
  return NS_OK;
}

nsPeekOffsetStruct nsIFrame::GetExtremeCaretPosition(PRBool aStart)
{
  nsPeekOffsetStruct result;

  result.mResultContent = this->GetContent();
  result.mContentOffset = 0;

  nsIFrame *resultFrame = this;

  if (aStart)
    nsFrame::GetFirstLeaf(GetPresContext(), &resultFrame);
  else
    nsFrame::GetLastLeaf(GetPresContext(), &resultFrame);

  NS_ASSERTION(resultFrame,"result frame for carent positioning is Null!");

  if (!resultFrame)
    return result;

  // there should be some more validity checks here, or earlier in the code,
  // in case we get to to some 'dummy' frames at the end of the content
    
  nsIContent* content = resultFrame->GetContent();

  NS_ASSERTION(resultFrame,"result frame content for carent positioning is Null!");

  if (!content)
    return result;
  
  // special case: if this is not a textnode,
  // position the caret to the offset of its parent instead
  // (position the caret to non-text element may make the caret missing)

  if (!content->IsContentOfType(nsIContent::eTEXT)) {
    // special case in effect
    nsIContent* parent = content->GetParent();
    NS_ASSERTION(parent,"element has no parent!");
    if (parent) {
      result.mResultContent = parent;
      result.mContentOffset = parent->IndexOf(content);
      if (!aStart)
        result.mContentOffset++; // go to end of this frame
      return result;
    }
  }

  result.mResultContent = content;

  PRInt32 start, end;
  nsresult rv;
  rv = resultFrame->GetOffsets(start,end);
  if (NS_SUCCEEDED(rv)) {
    result.mContentOffset = aStart ? start : end;
  }

  return result;
}

// Get a frame which can contain a line iterator
// (which generally means it's a block frame).
static nsILineIterator*
GetBlockFrameAndLineIter(nsIFrame* aFrame, nsIFrame** aBlockFrame)
{
  nsILineIterator* it;
  nsIFrame *blockFrame = aFrame;

  blockFrame = blockFrame->GetParent();
  if (!blockFrame) //if at line 0 then nothing to do
    return 0;
  nsresult result = blockFrame->QueryInterface(NS_GET_IID(nsILineIterator), (void**)&it);
  if (NS_SUCCEEDED(result) && it)
  {
    if (aBlockFrame)
      *aBlockFrame = blockFrame;
    return it;
  }

  while (blockFrame)
  {
    blockFrame = blockFrame->GetParent();
    if (blockFrame) {
      result = blockFrame->QueryInterface(NS_GET_IID(nsILineIterator),
                                          (void**)&it);
      if (NS_SUCCEEDED(result) && it)
      {
        if (aBlockFrame)
          *aBlockFrame = blockFrame;
        return it;
      }
    }
  }
  return 0;
}

nsresult
nsFrame::PeekOffsetParagraph(nsPresContext* aPresContext,
                             nsPeekOffsetStruct *aPos)
{
#ifdef DEBUG_paragraph
  printf("Selecting paragraph\n");
#endif
  nsIFrame* blockFrame;
  nsCOMPtr<nsILineIterator> iter (getter_AddRefs(GetBlockFrameAndLineIter(this, &blockFrame)));
  if (!blockFrame || !iter)
    return NS_ERROR_UNEXPECTED;

  PRInt32 thisLine;
  nsresult result = iter->FindLineContaining(this, &thisLine);
#ifdef DEBUG_paragraph
  printf("Looping %s from line %d\n",
         aPos->mDirection == eDirPrevious ? "back" : "forward",
         thisLine);
#endif
  if (NS_FAILED(result) || thisLine < 0)
    return result ? result : NS_ERROR_UNEXPECTED;

  // Now, theoretically, we should be able to loop over lines
  // looking for lines that end in breaks.
  PRInt32 di = (aPos->mDirection == eDirPrevious ? -1 : 1);
  for (PRInt32 i = thisLine; ; i += di)
  {
    nsIFrame* firstFrameOnLine;
    PRInt32 numFramesOnLine;
    nsRect lineBounds;
    PRUint32 lineFlags;
    if (i >= 0)
    {
      result = iter->GetLine(i, &firstFrameOnLine, &numFramesOnLine,
                             lineBounds, &lineFlags);
      if (NS_FAILED(result) || !firstFrameOnLine || !numFramesOnLine)
      {
#ifdef DEBUG_paragraph
        printf("End loop at line %d\n", i);
#endif
        break;
      }
    }
    if (lineFlags & NS_LINE_FLAG_ENDS_IN_BREAK || i < 0)
    {
      // Fill in aPos with the info on the new position
#ifdef DEBUG_paragraph
      printf("Found a paragraph break at line %d\n", i);
#endif

      // Save the old direction, but now go one line back the other way
      nsDirection oldDirection = aPos->mDirection;
      if (oldDirection == eDirPrevious)
        aPos->mDirection = eDirNext;
      else
        aPos->mDirection = eDirPrevious;
#ifdef SIMPLE /* nope */
      result = GetNextPrevLineFromeBlockFrame(aPresContext,
                                              aPos,
                                              blockFrame,
                                              i,
                                              0);
      if (NS_FAILED(result))
        printf("GetNextPrevLineFromeBlockFrame failed\n");

#else /* SIMPLE -- alas, nope */
      int edgeCase = 0;//no edge case. this should look at thisLine
      PRBool doneLooping = PR_FALSE;//tells us when no more block frames hit.
      //this part will find a frame or a block frame. if its a block frame
      //it will "drill down" to find a viable frame or it will return an error.
      do {
        result = GetNextPrevLineFromeBlockFrame(aPresContext,
                                                aPos, 
                                                blockFrame, 
                                                thisLine, 
                                                edgeCase //start from thisLine
          );
        if (aPos->mResultFrame == this)//we came back to same spot! keep going
        {
          aPos->mResultFrame = nsnull;
          if (aPos->mDirection == eDirPrevious)
            thisLine--;
          else
            thisLine++;
        }
        else
          doneLooping = PR_TRUE; //do not continue with while loop
        if (NS_SUCCEEDED(result) && aPos->mResultFrame)
        {
          result = aPos->mResultFrame->QueryInterface(NS_GET_IID(nsILineIterator),
                                                      getter_AddRefs(iter));
          if (NS_SUCCEEDED(result) && iter)//we've struck another block element!
          {
            doneLooping = PR_FALSE;
            if (aPos->mDirection == eDirPrevious)
              edgeCase = 1;//far edge, search from end backwards
            else
              edgeCase = -1;//near edge search from beginning onwards
            thisLine=0;//this line means nothing now.
            //everything else means something so keep looking "inside" the block
            blockFrame = aPos->mResultFrame;

          }
          else
            result = NS_OK;//THIS is to mean that everything is ok to the containing while loop
        }
      } while (!doneLooping);

#endif /* SIMPLE -- alas, nope */

      // Restore old direction before returning:
      aPos->mDirection = oldDirection;
      return result;
    }
  }

  return NS_OK;
}

// Line and paragraph selection (and probably several other cases)
// can get a containing frame from a line iterator, but then need
// to "drill down" to get the content and offset corresponding to
// the last child subframe.  Hence:
// Alas, this doesn't entirely work; it's blocked by some style changes.
static nsresult
DrillDownToEndOfLine(nsIFrame* aFrame, PRInt32 aLineNo, PRInt32 aLineFrameCount,
                     nsRect& aUsedRect, nsPeekOffsetStruct* aPos)
{
  if (!aFrame)
    return NS_ERROR_UNEXPECTED;
  nsresult rv = NS_ERROR_FAILURE;
  PRBool found = PR_FALSE;
  nsCOMPtr<nsIAtom> frameType;
  while (!found)  // this loop searches for a valid point to leave the peek offset struct.
  {
    nsIFrame *nextFrame = aFrame;
    nsIFrame *currentFrame = aFrame;
    PRInt32 i;
    for (i=1; i<aLineFrameCount && nextFrame; i++) //already have 1st frame
    {
		  currentFrame = nextFrame;
      // If we do GetNextSibling, we don't go far enough
      // (is aLineFrameCount too small?)
      // If we do GetNextInFlow, we hit a null.
      nextFrame = currentFrame->GetNextSibling();
    }
    if (!nextFrame) //premature leaving of loop.
    {
      nextFrame = currentFrame; //back it up. lets show a warning
      NS_WARNING("lineFrame Count lied to us from nsILineIterator!\n");
    }
    if (!nextFrame->GetRect().width) //this can happen with BR frames and or empty placeholder frames.
    {
      //if we do hit an empty frame then back up the current frame to the frame before it if there is one.
      nextFrame = currentFrame; 
    }
      
    nsPoint offsetPoint; //used for offset of result frame
    nsIView * view; //used for call of get offset from view
    nextFrame->GetOffsetFromView(offsetPoint, &view);

    offsetPoint.x += 2* aUsedRect.width; //2* just to be sure we are off the edge
    // This doesn't seem very efficient since GetPosition
    // has to do a binary search.

    nsPresContext *context = aPos->mShell->GetPresContext();
    PRInt32 endoffset;
    rv = nextFrame->GetContentAndOffsetsFromPoint(context,
                                                  offsetPoint,
                                                  getter_AddRefs(aPos->mResultContent),
                                                  aPos->mContentOffset,
                                                  endoffset,
                                                  aPos->mPreferLeft);
    if (NS_SUCCEEDED(rv))
      return PR_TRUE;

#ifdef DEBUG_paragraph
    NS_ASSERTION(PR_FALSE, "Looping around in PeekOffset\n");
#endif
    aLineFrameCount--;
    if (aLineFrameCount == 0)
      break;//just fail out
  }
  return rv;
}


NS_IMETHODIMP
nsFrame::PeekOffset(nsPresContext* aPresContext, nsPeekOffsetStruct *aPos)
{
  if (!aPos || !aPos->mShell)
    return NS_ERROR_NULL_POINTER;
  nsresult result = NS_ERROR_FAILURE; 
  PRInt32 endoffset;
  nsPoint point;
  point.x = aPos->mDesiredX;
  point.y = 0;
  switch (aPos->mAmount){
  case eSelectCharacter : case eSelectWord:
    {
      if (mContent)
      {
        nsIContent* newContent = mContent->GetParent();
        if (newContent){
          aPos->mResultContent = newContent;

          PRInt32 newOffset = newContent->IndexOf(mContent);

          if (aPos->mDirection == eDirNext)
            aPos->mContentOffset = newOffset + 1;
          else
            aPos->mContentOffset = newOffset;//to beginning of frame

          nsTextTransformer::Initialize();
          if (nsTextTransformer::GetWordSelectEatSpaceAfter() &&
              aPos->mDirection == eDirNext && aPos->mEatingWS)
          {
            //If we want to stop at beginning of the next word
            //GetFrameFromDirection should not return NS_ERROR_FAILURE
            //at end of line
            aPos->mEatingWS = PR_FALSE;	
            result = GetFrameFromDirection(aPresContext, aPos);
            aPos->mEatingWS = PR_TRUE;
          }
          else
            result = GetFrameFromDirection(aPresContext, aPos);

          if (NS_FAILED(result))
            return result;
          PRBool selectable = PR_FALSE;
          if (aPos->mResultFrame)
            aPos->mResultFrame->IsSelectable(&selectable, nsnull);
          if (NS_FAILED(result) || !aPos->mResultFrame || !selectable)
          {
            return result?result:NS_ERROR_FAILURE;
          }
          return aPos->mResultFrame->PeekOffset(aPresContext, aPos);
        }
      }
      break;
    }//drop into no amount
    case eSelectNoAmount:
    {
      nsPresContext *context = aPos->mShell->GetPresContext();
      if (!context)
        return NS_OK;
      result = GetContentAndOffsetsFromPoint(context,point,
                             getter_AddRefs(aPos->mResultContent),
                             aPos->mContentOffset,
                             endoffset,
                             aPos->mPreferLeft);
    }break;
    case eSelectLine :
    {
      nsCOMPtr<nsILineIteratorNavigator> iter; 
      nsIFrame *blockFrame = this;
      nsIFrame *thisBlock = this;
      PRInt32   thisLine;

      while (NS_FAILED(result)){
        thisBlock = blockFrame;
        blockFrame = blockFrame->GetParent();
        if (!blockFrame) //if at line 0 then nothing to do
          return NS_OK;
        result = blockFrame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator),getter_AddRefs(iter));
        while (NS_FAILED(result) && blockFrame)
        {
          thisBlock = blockFrame;
          blockFrame = blockFrame->GetParent();
          result = NS_OK;
          if (blockFrame) {
            result = blockFrame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator),getter_AddRefs(iter));
          }
        }
        //this block is now one child down from blockframe
        if (NS_FAILED(result) || !iter || !blockFrame || !thisBlock)
        {
          return ((result) ? result : NS_ERROR_FAILURE);
        }

        if (thisBlock->GetStateBits() & NS_FRAME_OUT_OF_FLOW)
        {
          //if we are searching for a frame that is not in flow we will not find it. 
          //we must instead look for its placeholder
          thisBlock =
            aPresContext->FrameManager()->GetPlaceholderFrameFor(thisBlock);

          if (!thisBlock)
            return NS_ERROR_FAILURE;
        }

        result = iter->FindLineContaining(thisBlock, &thisLine);

        if (NS_FAILED(result))
           return result;

        if (thisLine < 0) 
          return  NS_ERROR_FAILURE;

        int edgeCase = 0;//no edge case. this should look at thisLine
        PRBool doneLooping = PR_FALSE;//tells us when no more block frames hit.
        //this part will find a frame or a block frame. if its a block frame
        //it will "drill down" to find a viable frame or it will return an error.
        nsIFrame *lastFrame = this;
        do {
          result = GetNextPrevLineFromeBlockFrame(aPresContext,
                                                  aPos, 
                                                  blockFrame, 
                                                  thisLine, 
                                                  edgeCase //start from thisLine
            );
          if (NS_SUCCEEDED(result) && (!aPos->mResultFrame || aPos->mResultFrame == lastFrame))//we came back to same spot! keep going
          {
            aPos->mResultFrame = nsnull;
            if (aPos->mDirection == eDirPrevious)
              thisLine--;
            else
              thisLine++;
          }
          else //if failure or success with different frame.
            doneLooping = PR_TRUE; //do not continue with while loop

          lastFrame = aPos->mResultFrame; //set last frame 

          if (NS_SUCCEEDED(result) && aPos->mResultFrame 
            && blockFrame != aPos->mResultFrame)// make sure block element is not the same as the one we had before
          {
/* SPECIAL CHECK FOR TABLE NAVIGATION
  tables need to navigate also and the frame that supports it is nsTableRowGroupFrame which is INSIDE
  nsTableOuterFrame.  if we have stumbled onto an nsTableOuter we need to drill into nsTableRowGroup
  if we hit a header or footer thats ok just go into them,
*/
            PRBool searchTableBool = PR_FALSE;
            if (aPos->mResultFrame->GetType() == nsLayoutAtoms::tableOuterFrame ||
                aPos->mResultFrame->GetType() == nsLayoutAtoms::tableCellFrame)
            {
              nsIFrame *frame = aPos->mResultFrame->GetFirstChild(nsnull);
              //got the table frame now
              while(frame) //ok time to drill down to find iterator
              {
                result = frame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator),
                                                          getter_AddRefs(iter));
                if (NS_SUCCEEDED(result))
                {
                  aPos->mResultFrame = frame;
                  searchTableBool = PR_TRUE;
                  break; //while(frame)
                }
                frame = frame->GetFirstChild(nsnull);
              }
            }
            if (!searchTableBool)
              result = aPos->mResultFrame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator),
                                                        getter_AddRefs(iter));
            if (NS_SUCCEEDED(result) && iter)//we've struck another block element!
            {
              doneLooping = PR_FALSE;
              if (aPos->mDirection == eDirPrevious)
                edgeCase = 1;//far edge, search from end backwards
              else
                edgeCase = -1;//near edge search from beginning onwards
              thisLine=0;//this line means nothing now.
              //everything else means something so keep looking "inside" the block
              blockFrame = aPos->mResultFrame;

            }
            else
            {
              result = NS_OK;//THIS is to mean that everything is ok to the containing while loop
              break;
            }
          }
        } while (!doneLooping);
      }
      break;
    }

    case eSelectParagraph:
      return PeekOffsetParagraph(aPresContext, aPos);

    case eSelectBeginLine:
    case eSelectEndLine:
    {
      nsCOMPtr<nsILineIteratorNavigator> it; 
      nsIFrame* thisBlock = this;
      nsIFrame* blockFrame = GetParent();
      if (!blockFrame) //if at line 0 then nothing to do
        return NS_OK;
      result = blockFrame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator),getter_AddRefs(it));
      while (NS_FAILED(result) && blockFrame)
      {
        thisBlock = blockFrame;
        blockFrame = blockFrame->GetParent();
        result = NS_OK;
        if (blockFrame) {
          result = blockFrame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator),getter_AddRefs(it));
        }
      }
      if (NS_FAILED(result) || !it || !blockFrame || !thisBlock)
        return result;
      //this block is now one child down from blockframe

      PRInt32   thisLine;
      result = it->FindLineContaining(thisBlock, &thisLine);
      if (NS_FAILED(result) || thisLine < 0 )
        return result;

      PRInt32 lineFrameCount;
      nsIFrame *firstFrame;
      nsRect  usedRect; 
      PRUint32 lineFlags;
      result = it->GetLine(thisLine, &firstFrame, &lineFrameCount,usedRect,
                           &lineFlags);

      if (eSelectBeginLine == aPos->mAmount)
      {
        nsPresContext *context = aPos->mShell->GetPresContext();
        if (!context)
          return NS_OK;

        while (firstFrame)
        {
          nsPoint offsetPoint; //used for offset of result frame
          nsIView * view; //used for call of get offset from view
          firstFrame->GetOffsetFromView(offsetPoint, &view);

          offsetPoint.x = 0;//all the way to the left
          result = firstFrame->GetContentAndOffsetsFromPoint(context,
                                                             offsetPoint,
                                           getter_AddRefs(aPos->mResultContent),
                                           aPos->mContentOffset,
                                           endoffset,
                                           aPos->mPreferLeft);
          if (NS_SUCCEEDED(result))
            break;
          result = it->GetNextSiblingOnLine(firstFrame,thisLine);
          if (NS_FAILED(result))
            break;
        }
      }
      else  // eSelectEndLine
      {
        // We have the last frame, but we need to drill down
        // to get the last offset in the last content represented
        // by that frame.
        return DrillDownToEndOfLine(firstFrame, thisLine, lineFrameCount,
                                    usedRect, aPos);
      }
      return result;
    }
    break;

    default: 
    {
      if (NS_SUCCEEDED(result))
        result = aPos->mResultFrame->PeekOffset(aPresContext, aPos);
    }
  }                          
  return result;
}


NS_IMETHODIMP
nsFrame::CheckVisibility(nsPresContext* , PRInt32 , PRInt32 , PRBool , PRBool *, PRBool *)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


PRInt32
nsFrame::GetLineNumber(nsIFrame *aFrame)
{
  nsIFrame *blockFrame = aFrame;
  nsIFrame *thisBlock;
  PRInt32   thisLine;
  nsCOMPtr<nsILineIteratorNavigator> it; 
  nsresult result = NS_ERROR_FAILURE;
  while (NS_FAILED(result) && blockFrame)
  {
    thisBlock = blockFrame;
    blockFrame = blockFrame->GetParent();
    result = NS_OK;
    if (blockFrame) {
      result = blockFrame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator),getter_AddRefs(it));
    }
  }
  if (!blockFrame || !it)
    return NS_ERROR_FAILURE;
  result = it->FindLineContaining(thisBlock, &thisLine);
  if (NS_FAILED(result))
    return -1;
  return thisLine;
}


//this will use the nsFrameTraversal as the default peek method.
//this should change to use geometry and also look to ALL the child lists
//we need to set up line information to make sure we dont jump across line boundaries
NS_IMETHODIMP
nsFrame::GetFrameFromDirection(nsPresContext* aPresContext, nsPeekOffsetStruct *aPos)
{
  nsIFrame *blockFrame = this;
  nsIFrame *thisBlock;
  PRInt32   thisLine;
  nsCOMPtr<nsILineIteratorNavigator> it; 
  PRBool preferLeft = aPos->mPreferLeft;
  nsresult result = NS_ERROR_FAILURE;
  while (NS_FAILED(result) && blockFrame)
  {
    thisBlock = blockFrame;
    blockFrame = blockFrame->GetParent();
    result = NS_OK;
    if (blockFrame) {
      result = blockFrame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator),getter_AddRefs(it));
    }
  }
  if (!blockFrame || !it)
    return NS_ERROR_FAILURE;
  result = it->FindLineContaining(thisBlock, &thisLine);
  if (NS_FAILED(result))
    return result;

  nsIFrame *traversedFrame = this;

  nsIFrame *firstFrame;
  nsIFrame *lastFrame;
  nsRect  nonUsedRect;
  PRInt32 lineFrameCount;
  PRUint32 lineFlags;

#ifdef IBMBIDI
  /* Check whether the visual and logical order of the frames are different */
  PRBool lineIsReordered = PR_FALSE;
  nsIFrame *firstVisual;
  nsIFrame *lastVisual;
  PRBool lineIsRTL;
  PRBool lineJump = PR_FALSE;

  it->GetDirection(&lineIsRTL);
  result = it->CheckLineOrder(thisLine, &lineIsReordered, &firstVisual, &lastVisual);
  if (NS_FAILED(result))
    return result;

  if (lineIsReordered) {
    firstFrame = firstVisual;
    lastFrame = lastVisual;
  }
  else
#endif
  {
    result = it->GetLine(thisLine, &firstFrame, &lineFrameCount,nonUsedRect,
                         &lineFlags);
    if (NS_FAILED(result))
      return result;

    lastFrame = firstFrame;
    for (;lineFrameCount > 1;lineFrameCount --){
      result = it->GetNextSiblingOnLine(lastFrame, thisLine);
      if (NS_FAILED(result) || !lastFrame){
        NS_ASSERTION(0,"should not be reached nsFrame\n");
        return NS_ERROR_FAILURE;
      }
    }
  }

  GetFirstLeaf(aPresContext, &firstFrame);
  GetLastLeaf(aPresContext, &lastFrame);
  //END LINE DATA CODE
  if 
#ifndef IBMBIDI  // jumping lines in RTL paragraphs
      ((aPos->mDirection == eDirNext && lastFrame == this)
       ||(aPos->mDirection == eDirPrevious && firstFrame == this))
#else
      (((lineIsRTL) &&
        ((aPos->mDirection == eDirNext && firstFrame == this)
         ||(aPos->mDirection == eDirPrevious && lastFrame == this)))
       ||
       ((!lineIsRTL) &&
        ((aPos->mDirection == eDirNext && lastFrame == this)
         ||(aPos->mDirection == eDirPrevious && firstFrame == this))))
#endif
  {
    if (aPos->mJumpLines != PR_TRUE)
      return NS_ERROR_FAILURE;//we are done. cannot jump lines
#ifdef IBMBIDI
    lineJump = PR_TRUE;
#endif
    if (aPos->mAmount != eSelectWord)
    {
      preferLeft = (PRBool)!preferLeft;//drift to other side
      aPos->mAmount = eSelectNoAmount;
    }
    else{
      if (aPos->mEatingWS)//done finding what we wanted
        return NS_ERROR_FAILURE;
      if (aPos->mDirection == eDirNext)
      {
        preferLeft = (PRBool)!preferLeft;//drift to other side
      }
#ifdef IBMBIDI
      if (lineIsRTL)
        aPos->mAmount = eSelectNoAmount;
#endif
    }

  }
  if (aPos->mAmount == eSelectDir)
    aPos->mAmount = eSelectNoAmount;//just get to next frame.
  nsCOMPtr<nsIBidirectionalEnumerator> frameTraversal;
#ifdef IBMBIDI
  result = NS_NewFrameTraversal(getter_AddRefs(frameTraversal),
                                lineIsReordered ? VISUAL : LEAF,
                                aPresContext, 
                                traversedFrame, aPos->mScrollViewStop);
#else
  //if we are a container frame we MUST init with last leaf for eDirNext
  //
  result = NS_NewFrameTraversal(getter_AddRefs(frameTraversal), LEAF, aPresContext, traversedFrame, aPos->mScrollViewStop);
#endif
  if (NS_FAILED(result))
    return result;
  nsISupports *isupports = nsnull;
#ifdef IBMBIDI
  nsIFrame *newFrame;

  for (;;) {
    if (lineIsRTL) 
      if (aPos->mDirection == eDirPrevious)
        result = frameTraversal->Next();
      else 
        result = frameTraversal->Prev();
    else
#endif
      if (aPos->mDirection == eDirNext)
        result = frameTraversal->Next();
      else 
        result = frameTraversal->Prev();
    
  if (NS_FAILED(result))
    return result;
  result = frameTraversal->CurrentItem(&isupports);
  if (NS_FAILED(result))
    return result;
  if (!isupports)
    return NS_ERROR_NULL_POINTER;
  //we must CAST here to an nsIFrame. nsIFrame doesnt really follow the rules
  //for speed reasons
#ifndef IBMBIDI
  nsIFrame *newFrame = (nsIFrame *)isupports;
#else
  
  newFrame = (nsIFrame *)isupports;
  nsIContent *content = newFrame->GetContent();
  if (!lineJump && (content == mContent))
  {
    //we will continue if this is NOT a text node. 
    //in the case of a text node since that does not mean we are stuck. it could mean a change in style for
    //the text node.  in the case of a hruleframe with generated before and after content, we do not
    //want the splittable generated frame to get us stuck on an HR
    if (nsLayoutAtoms::textFrame != newFrame->GetType())
      continue;  //we should NOT be getting stuck on the same piece of content on the same line. skip to next line.
  }
  PRBool isBidiGhostFrame = (newFrame->GetRect().IsEmpty() &&
                             (newFrame->GetStateBits() & NS_FRAME_IS_BIDI));
  if (isBidiGhostFrame)
  {
    // If the rectangle is empty and the NS_FRAME_IS_BIDI flag is set, this is most likely 
    // a non-renderable frame created at the end of the line by Bidi reordering.
    lineJump = PR_TRUE;
    aPos->mAmount = eSelectNoAmount;
  }
  else
  {
    PRBool selectable =  PR_TRUE; //usually fine
    newFrame->IsSelectable(&selectable, nsnull);
    if (selectable)
      break; // for (;;)
  }
  PRBool newLineIsRTL = PR_FALSE;
  if (lineJump) {
    blockFrame = newFrame;
    nsresult result = NS_ERROR_FAILURE;
    while (NS_FAILED(result) && blockFrame)
    {
      thisBlock = blockFrame;
      blockFrame = blockFrame->GetParent();
      result = NS_OK;
      if (blockFrame) {
        result = blockFrame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator), getter_AddRefs(it));
      }
    }
    if (!blockFrame || !it)
      return NS_ERROR_FAILURE;
    result = it->FindLineContaining(thisBlock, &thisLine);
    if (NS_FAILED(result))
      return result;
    it->GetDirection(&newLineIsRTL);

    result = it->CheckLineOrder(thisLine, &lineIsReordered, &firstVisual, &lastVisual);
    if (NS_FAILED(result))
      return result;

    if (lineIsReordered)
    {
      firstFrame = firstVisual;
      lastFrame = lastVisual;
    }
    else
    {
      result = it->GetLine(thisLine, &firstFrame, &lineFrameCount, nonUsedRect,
                           &lineFlags);
      if (NS_FAILED(result))
        return result;
      if (!firstFrame)
        return NS_ERROR_FAILURE;

      lastFrame = firstFrame;
      for (;lineFrameCount > 1;lineFrameCount --){
        result = it->GetNextSiblingOnLine(lastFrame, thisLine);
        if (NS_FAILED(result) || !lastFrame){
          NS_ASSERTION(0,"should not be reached nsFrame\n");
          return NS_ERROR_FAILURE;
        }
      }
    }

    GetFirstLeaf(aPresContext, &firstFrame);
    GetLastLeaf(aPresContext, &lastFrame);

    if (newLineIsRTL) {
      if (aPos->mDirection == eDirPrevious)
        newFrame = firstFrame;
      else
        newFrame = lastFrame;
    }
    else {
      if (aPos->mDirection == eDirNext)
        newFrame = firstFrame;
      else
        newFrame = lastFrame;
      }
    }
    PRBool selectable =  PR_TRUE; //usually fine
    newFrame->IsSelectable(&selectable, nsnull);
    if (!selectable)
      lineJump = PR_FALSE;
  } // for (;;) we will break from inside
#endif // IBMBIDI
  if (aPos->mDirection == eDirNext)
    aPos->mStartOffset = 0;
  else
    aPos->mStartOffset = -1;
#ifdef IBMBIDI
  PRUint8 oldLevel = NS_GET_EMBEDDING_LEVEL(this);
  PRUint8 newLevel = NS_GET_EMBEDDING_LEVEL(newFrame);
  if (newLevel & 1) // The new frame is RTL, go to the other end
    aPos->mStartOffset = -1 - aPos->mStartOffset;

  if ((aPos->mAmount == eSelectNoAmount) && ((newLevel & 1) != (oldLevel & 1)))  {
    preferLeft = (PRBool)!preferLeft;
  }
#endif
  aPos->mResultFrame = newFrame;
  aPos->mPreferLeft = preferLeft;
  return NS_OK;
}

nsIView* nsIFrame::GetClosestView(nsPoint* aOffset) const
{
  nsPoint offset(0,0);
  for (const nsIFrame *f = this; f; f = f->GetParent()) {
    if (f->HasView()) {
      if (aOffset)
        *aOffset = offset;
      return f->GetView();
    }
    offset += f->GetPosition();
  }

  NS_NOTREACHED("No view on any parent?  How did that happen?");
  return nsnull;
}


NS_IMETHODIMP
nsFrame::ReflowDirtyChild(nsIPresShell* aPresShell, nsIFrame* aChild)
{
  NS_ASSERTION(0, "nsFrame::ReflowDirtyChild() should never be called.");  
  return NS_ERROR_NOT_IMPLEMENTED;    
}


#ifdef ACCESSIBILITY
NS_IMETHODIMP
nsFrame::GetAccessible(nsIAccessible** aAccessible)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
#endif

// Destructor function for the overflow area property
static void
DestroyRectFunc(void*           aFrame,
                nsIAtom*        aPropertyName,
                void*           aPropertyValue,
                void*           aDtorData)
{
  delete (nsRect*)aPropertyValue;
}

nsRect*
nsIFrame::GetOverflowAreaProperty(PRBool aCreateIfNecessary) 
{
  if (!((GetStateBits() & NS_FRAME_OUTSIDE_CHILDREN) || aCreateIfNecessary)) {
    return nsnull;
  }

  nsPropertyTable *propTable = GetPresContext()->PropertyTable();
  void *value = propTable->GetProperty(this,
                                       nsLayoutAtoms::overflowAreaProperty);

  if (value) {
    return (nsRect*)value;  // the property already exists
  } else if (aCreateIfNecessary) {
    // The property isn't set yet, so allocate a new rect, set the property,
    // and return the newly allocated rect
    nsRect*  overflow = new nsRect(0, 0, 0, 0);
    propTable->SetProperty(this, nsLayoutAtoms::overflowAreaProperty,
                           overflow, DestroyRectFunc, nsnull);
    return overflow;
  }

  return nsnull;
}

void 
nsIFrame::FinishAndStoreOverflow(nsRect* aOverflowArea, nsSize aNewSize)
{
  // This is now called FinishAndStoreOverflow() instead of 
  // StoreOverflow() because frame-generic ways of adding overflow
  // can happen here, e.g. CSS2 outline.
  // If we find more things other than outline that need to be added,
  // we should think about starting a new method like GetAdditionalOverflow()
  NS_ASSERTION(aNewSize.width == 0 || aNewSize.height == 0 ||
               aOverflowArea->Contains(nsRect(nsPoint(0, 0), aNewSize)),
               "Computed overflow area must contain frame bounds");

  PRBool geometricOverflow =
    aOverflowArea->x < 0 || aOverflowArea->y < 0 ||
    aOverflowArea->XMost() > aNewSize.width || aOverflowArea->YMost() > aNewSize.height;
  // Clear geometric overflow area if we clip our children
  NS_ASSERTION((GetStyleDisplay()->mOverflowY == NS_STYLE_OVERFLOW_CLIP) ==
               (GetStyleDisplay()->mOverflowX == NS_STYLE_OVERFLOW_CLIP),
               "If one overflow is clip, the other should be too");
  if (geometricOverflow &&
      GetStyleDisplay()->mOverflowX == NS_STYLE_OVERFLOW_CLIP) {
    *aOverflowArea = nsRect(nsPoint(0, 0), aNewSize);
    geometricOverflow = PR_FALSE;
  }

  PRBool hasOutline;
  nsRect outlineRect(ComputeOutlineRect(this, &hasOutline, *aOverflowArea));

  if (hasOutline || geometricOverflow) {
    // Throw out any overflow if we're -moz-hidden-unscrollable
    mState |= NS_FRAME_OUTSIDE_CHILDREN;
    nsRect* overflowArea = GetOverflowAreaProperty(PR_TRUE); 
    NS_ASSERTION(overflowArea, "should have created rect");
    *aOverflowArea = *overflowArea = outlineRect;
  } 
  else {
    if (mState & NS_FRAME_OUTSIDE_CHILDREN) {
      // remove the previously stored overflow area 
      DeleteProperty(nsLayoutAtoms::overflowAreaProperty);
    }
    mState &= ~NS_FRAME_OUTSIDE_CHILDREN;
  }   
}

void
nsFrame::ConsiderChildOverflow(nsRect&   aOverflowArea,
                               nsIFrame* aChildFrame)
{
  const nsStyleDisplay* disp = GetStyleDisplay();
  // check here also for hidden as table frames (table, tr and td) currently 
  // don't wrap their content into a scrollable frame if overflow is specified
  if (!disp->IsTableClip()) {
    nsRect* overflowArea = aChildFrame->GetOverflowAreaProperty();
    if (overflowArea) {
      nsRect childOverflow(*overflowArea);
      childOverflow.MoveBy(aChildFrame->GetPosition());
      aOverflowArea.UnionRect(aOverflowArea, childOverflow);
    }
    else {
      aOverflowArea.UnionRect(aOverflowArea, aChildFrame->GetRect());
    }
  }
}

NS_IMETHODIMP 
nsFrame::GetParentStyleContextFrame(nsPresContext* aPresContext,
                                    nsIFrame**      aProviderFrame,
                                    PRBool*         aIsChild)
{
  return DoGetParentStyleContextFrame(aPresContext, aProviderFrame, aIsChild);
}


/**
 * This function takes a "special" frame and _if_ that frame is the
 * anonymous block crated by an ib split it returns the split inline
 * as aSpecialSibling.  This is needed because the split inline's
 * style context is the parent of the anonymous block's srtyle context.
 *
 * If aFrame is not the anonymous block, aSpecialSibling is not
 * touched.
 */
static nsresult
GetIBSpecialSibling(nsPresContext* aPresContext,
                    nsIFrame* aFrame,
                    nsIFrame** aSpecialSibling)
{
  NS_PRECONDITION(aFrame, "Must have a non-null frame!");
  NS_ASSERTION(aFrame->GetStateBits() & NS_FRAME_IS_SPECIAL,
               "GetIBSpecialSibling should not be called on a non-special frame");
  
  // Find the first-in-flow of the frame.  (Ugh.  This ends up
  // being O(N^2) when it is called O(N) times.)
  aFrame = aFrame->GetFirstInFlow();

  /*
   * Now look up the nsLayoutAtoms::IBSplitSpecialPrevSibling
   * property, which is only set on the anonymous block frames we're
   * interested in.
   */
  nsresult rv;
  nsIFrame *specialSibling = NS_STATIC_CAST(nsIFrame*,
                             aPresContext->PropertyTable()->GetProperty(aFrame,
                               nsLayoutAtoms::IBSplitSpecialPrevSibling, &rv));

  if (NS_OK == rv) {
    NS_ASSERTION(specialSibling, "null special sibling");
    *aSpecialSibling = specialSibling;
  }

  return NS_OK;
}

static PRBool
IsTablePseudo(nsIAtom* aPseudo)
{
  return
    aPseudo == nsCSSAnonBoxes::tableOuter ||
    aPseudo == nsCSSAnonBoxes::table ||
    aPseudo == nsCSSAnonBoxes::tableRowGroup ||
    aPseudo == nsCSSAnonBoxes::tableRow ||
    aPseudo == nsCSSAnonBoxes::tableCell ||
    aPseudo == nsCSSAnonBoxes::tableColGroup ||
    aPseudo == nsCSSAnonBoxes::tableCol;
}

/**
 * Get the parent, corrected for the mangled frame tree resulting from
 * having a block within an inline.  The result only differs from the
 * result of |GetParent| when |GetParent| returns an anonymous block
 * that was created for an element that was 'display: inline' because
 * that element contained a block.
 *
 * Also skip anonymous scrolled-content parents; inherit directly from the
 * outer scroll frame.
 */
static nsresult
GetCorrectedParent(nsPresContext* aPresContext, nsIFrame* aFrame,
                   nsIFrame** aSpecialParent)
{
  nsIFrame *parent = aFrame->GetParent();
  *aSpecialParent = parent;
  if (parent) {
    nsIAtom* pseudo = aFrame->GetStyleContext()->GetPseudoType();

    // if this frame itself is not scrolled-content, then skip any scrolled-content
    // parents since they're basically anonymous as far as the style system goes
    if (pseudo != nsCSSAnonBoxes::scrolledContent) {
      while (parent->GetStyleContext()->GetPseudoType() ==
             nsCSSAnonBoxes::scrolledContent) {
        parent = parent->GetParent();
      }
    }

    // If the frame is not a table pseudo frame, we want to move up
    // the tree till we get to a non-table-pseudo frame.
    if (!IsTablePseudo(pseudo)) {
      while (IsTablePseudo(parent->GetStyleContext()->GetPseudoType())) {
        parent = parent->GetParent();
      }
    }

    if (parent->GetStateBits() & NS_FRAME_IS_SPECIAL) {
      GetIBSpecialSibling(aPresContext, parent, aSpecialParent);
    } else {
      *aSpecialParent = parent;
    }
  }

  return NS_OK;
}

nsresult
nsFrame::DoGetParentStyleContextFrame(nsPresContext* aPresContext,
                                      nsIFrame**      aProviderFrame,
                                      PRBool*         aIsChild)
{
  *aIsChild = PR_FALSE;
  *aProviderFrame = nsnull;
  if (mContent && !mContent->GetParent() &&
      !GetStyleContext()->GetPseudoType()) {
    // we're a frame for the root.  We have no style context parent.
    return NS_OK;
  }
  
  if (!(mState & NS_FRAME_OUT_OF_FLOW)) {
    /*
     * If this frame is the anonymous block created when an inline
     * with a block inside it got split, then the parent style context
     * is on the first of the three special frames.  We can get to it
     * using GetIBSpecialSibling
     */
    if (mState & NS_FRAME_IS_SPECIAL) {
      GetIBSpecialSibling(aPresContext, this, aProviderFrame);
      if (*aProviderFrame)
        return NS_OK;
    }

    // If this frame is one of the blocks that split an inline, we must
    // return the "special" inline parent, i.e., the parent that this
    // frame would have if we didn't mangle the frame structure.
    return GetCorrectedParent(aPresContext, this, aProviderFrame);
  }

  // For out-of-flow frames, we must resolve underneath the
  // placeholder's parent.
  nsIFrame *placeholder =
    aPresContext->FrameManager()->GetPlaceholderFrameFor(this);
  if (!placeholder) {
    NS_NOTREACHED("no placeholder frame for out-of-flow frame");
    GetCorrectedParent(aPresContext, this, aProviderFrame);
    return NS_ERROR_FAILURE;
  }
  return NS_STATIC_CAST(nsFrame*, placeholder)->
    GetParentStyleContextFrame(aPresContext, aProviderFrame, aIsChild);
}

//-----------------------------------------------------------------------------------




void
nsFrame::GetLastLeaf(nsPresContext* aPresContext, nsIFrame **aFrame)
{
  if (!aFrame || !*aFrame)
    return;
  nsIFrame *child = *aFrame;
  //if we are a block frame then go for the last line of 'this'
  while (1){
    child = child->GetFirstChild(nsnull);
    if (!child)
      return;//nothing to do
    nsIFrame* siblingFrame;
    nsIContent* content;
    //ignore anonymous elements, e.g. mozTableAdd* mozTableRemove*
    //see bug 278197 comment #12 #13 for details
    while ((siblingFrame = child->GetNextSibling()) &&
           (content = siblingFrame->GetContent()) &&
           !content->IsNativeAnonymous())
      child = siblingFrame;
    *aFrame = child;
  }
}

void
nsFrame::GetFirstLeaf(nsPresContext* aPresContext, nsIFrame **aFrame)
{
  if (!aFrame || !*aFrame)
    return;
  nsIFrame *child = *aFrame;
  while (1){
    child = child->GetFirstChild(nsnull);
    if (!child)
      return;//nothing to do
    *aFrame = child;
  }
}

NS_IMETHODIMP
nsFrame::CaptureMouse(nsPresContext* aPresContext, PRBool aGrabMouseEvents)
{
  // get its view
  nsIView* view = GetNearestCapturingView(this);
  if (!view) {
    return NS_ERROR_FAILURE;
  }

  nsIViewManager* viewMan = view->GetViewManager();
  if (!viewMan) {
    return NS_ERROR_FAILURE;
  }

  if (aGrabMouseEvents) {
    PRBool result;
    viewMan->GrabMouseEvents(view, result);
  } else {
    PRBool result;
    viewMan->GrabMouseEvents(nsnull, result);
  }

  return NS_OK;
}

PRBool
nsFrame::IsMouseCaptured(nsPresContext* aPresContext)
{
    // get its view
  nsIView* view = GetNearestCapturingView(this);
  
  if (view) {
    nsIViewManager* viewMan = view->GetViewManager();

    if (viewMan) {
        nsIView* grabbingView;
        viewMan->GetMouseEventGrabber(grabbingView);
        if (grabbingView == view)
          return PR_TRUE;
    }
  }

  return PR_FALSE;
}

nsresult
nsIFrame::SetProperty(nsIAtom*           aPropName,
                      void*              aPropValue,
                      NSPropertyDtorFunc aPropDtorFunc,
                      void*              aDtorData)
{
  return GetPresContext()->PropertyTable()->
    SetProperty(this, aPropName, aPropValue, aPropDtorFunc, aDtorData);
}

void* 
nsIFrame::GetProperty(nsIAtom* aPropName, nsresult* aStatus) const
{
  return GetPresContext()->PropertyTable()->GetProperty(this, aPropName,
                                                        aStatus);
}

/* virtual */ void* 
nsIFrame::GetPropertyExternal(nsIAtom* aPropName, nsresult* aStatus) const
{
  return GetProperty(aPropName, aStatus);
}

nsresult
nsIFrame::DeleteProperty(nsIAtom* aPropName) const
{
  return GetPresContext()->PropertyTable()->DeleteProperty(this, aPropName);
}

void*
nsIFrame::UnsetProperty(nsIAtom* aPropName, nsresult* aStatus) const
{
  return GetPresContext()->PropertyTable()->UnsetProperty(this, aPropName,
                                                          aStatus);
}

/* virtual */ const nsStyleStruct*
nsFrame::GetStyleDataExternal(nsStyleStructID aSID) const
{
  NS_ASSERTION(mStyleContext, "unexpected null pointer");
  return mStyleContext->GetStyleData(aSID);
}

/* virtual */ PRBool
nsIFrame::IsFocusable(PRInt32 *aTabIndex, PRBool aWithMouse)
{
  PRInt32 tabIndex = -1;
  if (aTabIndex) {
    *aTabIndex = -1; // Default for early return is not focusable
  }
  PRBool isFocusable = PR_FALSE;

  if (mContent && mContent->IsContentOfType(nsIContent::eELEMENT) &&
      AreAncestorViewsVisible()) {
    const nsStyleVisibility* vis = GetStyleVisibility();
    if (vis->mVisible != NS_STYLE_VISIBILITY_COLLAPSE &&
        vis->mVisible != NS_STYLE_VISIBILITY_HIDDEN) {
      if (mContent->IsContentOfType(nsIContent::eHTML)) {
        nsCOMPtr<nsISupports> container(GetPresContext()->GetContainer());
        nsCOMPtr<nsIEditorDocShell> editorDocShell(do_QueryInterface(container));
        if (editorDocShell) {
          PRBool isEditable;
          editorDocShell->GetEditable(&isEditable);
          if (isEditable) {
            return NS_OK;  // Editor content is not focusable
          }
        }
      }
      const nsStyleUserInterface* ui = GetStyleUserInterface();
      if (ui->mUserFocus != NS_STYLE_USER_FOCUS_IGNORE &&
          ui->mUserFocus != NS_STYLE_USER_FOCUS_NONE) {
        // Pass in default tabindex of -1 for nonfocusable and 0 for focusable
        tabIndex = 0;
      }
      isFocusable = mContent->IsFocusable(&tabIndex);
      if (!isFocusable && !aWithMouse &&
          GetType() == nsLayoutAtoms::scrollFrame &&
          mContent->IsContentOfType(nsIContent::eHTML) &&
          !mContent->IsNativeAnonymous() && mContent->GetParent() &&
          !mContent->HasAttr(kNameSpaceID_None, nsHTMLAtoms::tabindex)) {
        // Elements with scrollable view are focusable with script & tabbable
        // Otherwise you couldn't scroll them with keyboard, which is
        // an accessibility issue (e.g. Section 508 rules)
        // However, we don't make them to be focusable with the mouse,
        // because the extra focus outlines are considered unnecessarily ugly.
        // When clicked on, the selection position within the element 
        // will be enough to make them keyboard scrollable.
        nsCOMPtr<nsIScrollableFrame> scrollFrame = do_QueryInterface(this);
        if (scrollFrame) {
          nsIScrollableFrame::ScrollbarStyles styles =
            scrollFrame->GetScrollbarStyles();
          if (styles.mVertical == NS_STYLE_OVERFLOW_SCROLL ||
              styles.mVertical == NS_STYLE_OVERFLOW_AUTO ||
              styles.mHorizontal == NS_STYLE_OVERFLOW_SCROLL ||
              styles.mHorizontal == NS_STYLE_OVERFLOW_AUTO) {
            // Scroll bars will be used for overflow
            isFocusable = PR_TRUE;
            tabIndex = 0;
          }
        }
      }
    }
  }

  if (aTabIndex) {
    *aTabIndex = tabIndex;
  }
  return isFocusable;
}

/* static */
void nsFrame::FillCursorInformationFromStyle(const nsStyleUserInterface* ui,
                                             nsIFrame::Cursor& aCursor)
{
  aCursor.mCursor = ui->mCursor;
  aCursor.mHaveHotspot = PR_FALSE;
  aCursor.mHotspotX = aCursor.mHotspotY = 0.0f;

  for (nsCursorImage *item = ui->mCursorArray,
                 *item_end = ui->mCursorArray + ui->mCursorArrayLength;
       item < item_end; ++item) {
    PRUint32 status;
    nsresult rv = item->mImage->GetImageStatus(&status);
    if (NS_SUCCEEDED(rv) && (status & imgIRequest::STATUS_FRAME_COMPLETE)) {
      // This is the one we want
      item->mImage->GetImage(getter_AddRefs(aCursor.mContainer));
      aCursor.mHaveHotspot = item->mHaveHotspot;
      aCursor.mHotspotX = item->mHotspotX;
      aCursor.mHotspotY = item->mHotspotY;
      break;
    }
  }
}

PRBool
nsFrame::HasStyleChange()
{
  return BoxMetrics()->mStyleChange;
}

void
nsFrame::SetStyleChangeFlag(PRBool aDirty)
{
  nsBox::SetStyleChangeFlag(aDirty);
  BoxMetrics()->mStyleChange = PR_TRUE;
}

NS_IMETHODIMP
nsFrame::NeedsRecalc()
{
  nsBoxLayoutMetrics *metrics = BoxMetrics();

  SizeNeedsRecalc(metrics->mPrefSize);
  SizeNeedsRecalc(metrics->mMinSize);
  SizeNeedsRecalc(metrics->mMaxSize);
  SizeNeedsRecalc(metrics->mBlockPrefSize);
  SizeNeedsRecalc(metrics->mBlockMinSize);
  CoordNeedsRecalc(metrics->mFlex);
  CoordNeedsRecalc(metrics->mAscent);
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetOverflow(nsSize& aOverflow)
{
  aOverflow = BoxMetrics()->mOverflow;
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::SetIncludeOverflow(PRBool aInclude)
{
  BoxMetrics()->mIncludeOverflow = aInclude;
  return NS_OK;
}

NS_IMETHODIMP
nsFrame::RefreshSizeCache(nsBoxLayoutState& aState)
{

  // Ok we need to compute our minimum, preferred, and maximum sizes.
  // 1) Maximum size. This is easy. Its infinite unless it is overloaded by CSS.
  // 2) Preferred size. This is a little harder. This is the size the block would be 
  //      if it were laid out on an infinite canvas. So we can get this by reflowing
  //      the block with and INTRINSIC width and height. We can also do a nice optimization
  //      for incremental reflow. If the reflow is incremental then we can pass a flag to 
  //      have the block compute the preferred width for us! Preferred height can just be
  //      the minimum height;
  // 3) Minimum size. This is a toughy. We can pass the block a flag asking for the max element
  //    size. That would give us the width. Unfortunately you can only ask for a maxElementSize
  //    during an incremental reflow. So on other reflows we will just have to use 0.
  //    The min height on the other hand is fairly easy we need to get the largest
  //    line height. This can be done with the line iterator.

  // if we do have a reflow state
  nsresult rv = NS_OK;
  const nsHTMLReflowState* reflowState = aState.GetReflowState();
  if (reflowState) {
    nsPresContext* presContext = aState.PresContext();
    nsReflowStatus status = NS_FRAME_COMPLETE;
    nsHTMLReflowMetrics desiredSize(PR_FALSE);
    nsReflowReason reason;

    // See if we an set the max element size and return the reflow states new reason. Sometimes reflow states need to 
    // be changed. Incremental dirty reflows targeted at us can be converted to Resize if we are not dirty. So make sure
    // we look at the reason returned.
    nsReflowPath *path = nsnull;
    PRBool canSetMaxElementWidth = CanSetMaxElementWidth(aState, reason, &path);

    NS_ASSERTION(reason != eReflowReason_Incremental || path,
                 "HandleIncrementalReflow should have changed the reason to dirty.");

    // If we don't have any HTML constraints and its a resize, then nothing in the block
    // could have changed, so no refresh is necessary.
    nsBoxLayoutMetrics* metrics = BoxMetrics();
    if (!DoesNeedRecalc(metrics->mBlockPrefSize) && reason == eReflowReason_Resize)
      return NS_OK;

    // get the old rect.
    nsRect oldRect = GetRect();

    // the rect we plan to size to.
    nsRect rect(oldRect);
    
    // if we can set the maxElementSize then 
    // tell the metrics we want it. And also tell it we want
    // to compute the max width. This will allow us to get the min width and the pref width.
    if (canSetMaxElementWidth) {
       desiredSize.mFlags |= NS_REFLOW_CALC_MAX_WIDTH;
       desiredSize.mComputeMEW = PR_TRUE;
    } else {
      // if we can't set the maxElementSize. Then we must reflow
      // uncontrained.
      rect.width = NS_UNCONSTRAINEDSIZE;
      rect.height = NS_UNCONSTRAINEDSIZE;
    }

    // Create a child reflow state, fix-up the reason and the
    // incremental reflow path.
    nsHTMLReflowState childReflowState(*reflowState);
    childReflowState.reason = reason;
    childReflowState.path   = path;

    // do the nasty.
    rv = BoxReflow(aState,
                   presContext, 
                   desiredSize, 
                   childReflowState, 
                   status,
                   rect.x,
                   rect.y,
                   rect.width,
                   rect.height);

    nsRect newRect = GetRect();

    // make sure we draw any size change
    if (reason == eReflowReason_Incremental && (oldRect.width != newRect.width || oldRect.height != newRect.height)) {
     newRect.x = 0;
     newRect.y = 0;
     Redraw(aState, &newRect);
    }

    // if someone asked the nsBoxLayoutState to get the max size lets handle that.
    nscoord* stateMaxElementWidth = aState.GetMaxElementWidth();

    // the max element size is the largest height and width
    if (stateMaxElementWidth) {
      if (metrics->mBlockMinSize.width > *stateMaxElementWidth)
        *stateMaxElementWidth = metrics->mBlockMinSize.width;
    }
 
    metrics->mBlockMinSize.height = 0;
    // if we can use the maxElmementSize then lets use it
    // if not then just use the desired.
    if (canSetMaxElementWidth) {
      metrics->mBlockPrefSize.width  = desiredSize.mMaximumWidth;
      metrics->mBlockMinSize.width   = desiredSize.mMaxElementWidth; 
      // ok we need the max ascent of the items on the line. So to do this
      // ask the block for its line iterator. Get the max ascent.
      nsCOMPtr<nsILineIterator> lines = do_QueryInterface(NS_STATIC_CAST(nsIFrame*, this));
      if (lines) 
      {
        metrics->mBlockMinSize.height = 0;
        int count = 0;
        nsIFrame* firstFrame = nsnull;
        PRInt32 framesOnLine;
        nsRect lineBounds;
        PRUint32 lineFlags;

        do {
           lines->GetLine(count, &firstFrame, &framesOnLine, lineBounds, &lineFlags);
 
           if (lineBounds.height > metrics->mBlockMinSize.height)
             metrics->mBlockMinSize.height = lineBounds.height;

           count++;
        } while(firstFrame);
      }

      metrics->mBlockPrefSize.height  = metrics->mBlockMinSize.height;
    } else {
      metrics->mBlockPrefSize.width = desiredSize.width;
      metrics->mBlockPrefSize.height = desiredSize.height;
      // this sucks. We could not get the width.
      metrics->mBlockMinSize.width = 0;
      metrics->mBlockMinSize.height = desiredSize.height;
    }

    metrics->mBlockAscent = desiredSize.ascent;

#ifdef DEBUG_adaptor
    printf("min=(%d,%d), pref=(%d,%d), ascent=%d\n", metrics->mBlockMinSize.width,
                                                     metrics->mBlockMinSize.height,
                                                     metrics->mBlockPrefSize.width,
                                                     metrics->mBlockPrefSize.height,
                                                     metrics->mBlockAscent);
#endif
  }

  return rv;
}

NS_IMETHODIMP
nsFrame::GetPrefSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  // If the size is cached, and there are no HTML constraints that we might
  // be depending on, then we just return the cached size.
  nsBoxLayoutMetrics *metrics = BoxMetrics();
  if (!DoesNeedRecalc(metrics->mPrefSize)) {
     aSize = metrics->mPrefSize;
     return NS_OK;
  }

  aSize.width = 0;
  aSize.height = 0;

  PRBool isCollapsed = PR_FALSE;
  IsCollapsed(aState, isCollapsed);
  if (isCollapsed) {
    return NS_OK;
  } else {
    // get our size in CSS.
    PRBool completelyRedefined = nsIBox::AddCSSPrefSize(aState, this, metrics->mPrefSize);

    // Refresh our caches with new sizes.
    if (!completelyRedefined) {
       RefreshSizeCache(aState);
       metrics->mPrefSize = metrics->mBlockPrefSize;

       // notice we don't need to add our borders or padding
       // in. Thats because the block did it for us.
       // but we do need to add insets so debugging will work.
       AddInset(metrics->mPrefSize);
       nsIBox::AddCSSPrefSize(aState, this, metrics->mPrefSize);
    }
  }

  aSize = metrics->mPrefSize;

  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetMinSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  // Don't use the cache if we have HTMLReflowState constraints --- they might have changed
  nsBoxLayoutMetrics *metrics = BoxMetrics();
  if (!DoesNeedRecalc(metrics->mMinSize)) {
     aSize = metrics->mMinSize;
     return NS_OK;
  }

  aSize.width = 0;
  aSize.height = 0;

  PRBool isCollapsed = PR_FALSE;
  IsCollapsed(aState, isCollapsed);
  if (isCollapsed) {
    return NS_OK;
  } else {
    // get our size in CSS.
    PRBool completelyRedefined = nsIBox::AddCSSMinSize(aState, this, metrics->mMinSize);

    // Refresh our caches with new sizes.
    if (!completelyRedefined) {
       RefreshSizeCache(aState);
       metrics->mMinSize = metrics->mBlockMinSize;
       AddInset(metrics->mMinSize);
       nsIBox::AddCSSMinSize(aState, this, metrics->mMinSize);
    }
  }

  aSize = metrics->mMinSize;

  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetMaxSize(nsBoxLayoutState& aState, nsSize& aSize)
{
  // Don't use the cache if we have HTMLReflowState constraints --- they might have changed
  nsBoxLayoutMetrics *metrics = BoxMetrics();
  if (!DoesNeedRecalc(metrics->mMaxSize)) {
     aSize = metrics->mMaxSize;
     return NS_OK;
  }

  aSize.width = NS_INTRINSICSIZE;
  aSize.height = NS_INTRINSICSIZE;

  PRBool isCollapsed = PR_FALSE;
  IsCollapsed(aState, isCollapsed);
  if (isCollapsed) {
    return NS_OK;
  } else {
    metrics->mMaxSize.width = NS_INTRINSICSIZE;
    metrics->mMaxSize.height = NS_INTRINSICSIZE;
    nsBox::GetMaxSize(aState, metrics->mMaxSize);
  }

  aSize = metrics->mMaxSize;

  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetFlex(nsBoxLayoutState& aState, nscoord& aFlex)
{
  nsBoxLayoutMetrics *metrics = BoxMetrics();
  if (!DoesNeedRecalc(metrics->mFlex)) {
     aFlex = metrics->mFlex;
     return NS_OK;
  }

  metrics->mFlex = 0;
  nsBox::GetFlex(aState, metrics->mFlex);

  aFlex = metrics->mFlex;

  return NS_OK;
}

NS_IMETHODIMP
nsFrame::GetAscent(nsBoxLayoutState& aState, nscoord& aAscent)
{
  nsBoxLayoutMetrics *metrics = BoxMetrics();
  if (!DoesNeedRecalc(metrics->mAscent)) {
    aAscent = metrics->mAscent;
    return NS_OK;
  }

  PRBool isCollapsed = PR_FALSE;
  IsCollapsed(aState, isCollapsed);
  if (isCollapsed) {
    metrics->mAscent = 0;
  } else {
    // Refresh our caches with new sizes.
    RefreshSizeCache(aState);
    metrics->mAscent = metrics->mBlockAscent;
    nsMargin m(0, 0, 0, 0);
    GetInset(m);
    metrics->mAscent += m.top;
  }

  aAscent = metrics->mAscent;

  return NS_OK;
}

nsresult
nsFrame::DoLayout(nsBoxLayoutState& aState)
{
  nsRect ourRect(mRect);

  const nsHTMLReflowState* reflowState = aState.GetReflowState();
  nsPresContext* presContext = aState.PresContext();
  nsReflowStatus status = NS_FRAME_COMPLETE;
  nsHTMLReflowMetrics desiredSize(PR_FALSE);
  nsresult rv = NS_OK;
 
  if (reflowState) {

    nscoord* currentMEW = aState.GetMaxElementWidth();

    if (currentMEW) {
      desiredSize.mComputeMEW = PR_TRUE;
    }

    rv = BoxReflow(aState, presContext, desiredSize, *reflowState, status,
                   ourRect.x, ourRect.y, ourRect.width, ourRect.height);

    if (currentMEW && desiredSize.mMaxElementWidth > *currentMEW) {
      *currentMEW = desiredSize.mMaxElementWidth;
    }

    PRBool collapsed = PR_FALSE;
    IsCollapsed(aState, collapsed);
    if (collapsed) {
      SetSize(nsSize(0, 0));
    } else {

      // if our child needs to be bigger. This might happend with
      // wrapping text. There is no way to predict its height until we
      // reflow it. Now that we know the height reshuffle upward.
      if (desiredSize.width > ourRect.width ||
          desiredSize.height > ourRect.height) {

#ifdef DEBUG_GROW
        DumpBox(stdout);
        printf(" GREW from (%d,%d) -> (%d,%d)\n",
               ourRect.width, ourRect.height,
               desiredSize.width, desiredSize.height);
#endif

        if (desiredSize.width > ourRect.width)
          ourRect.width = desiredSize.width;

        if (desiredSize.height > ourRect.height)
          ourRect.height = desiredSize.height;
      }

      // ensure our size is what we think is should be. Someone could have
      // reset the frame to be smaller or something dumb like that. 
      SetSize(nsSize(ourRect.width, ourRect.height));
    }
  }

  SyncLayout(aState);

  return rv;
}

// Truncate the reflow path by pruning the subtree containing the
// specified frame. This ensures that we don't accidentally
// incrementally reflow a frame twice.
// XXXwaterson We could be more efficient by remembering the parent in
// FindReflowPathFor.
static void
PruneReflowPathFor(nsIFrame *aFrame, nsReflowPath *aReflowPath)
{
  nsReflowPath::iterator iter, end = aReflowPath->EndChildren();
  for (iter = aReflowPath->FirstChild(); iter != end; ++iter) {
    if (*iter == aFrame) {
      aReflowPath->Remove(iter);
      break;
    }

    PruneReflowPathFor(aFrame, iter.get());
  }
}

nsresult
nsFrame::BoxReflow(nsBoxLayoutState& aState,
                   nsPresContext*           aPresContext,
                   nsHTMLReflowMetrics&     aDesiredSize,
                   const nsHTMLReflowState& aReflowState,
                   nsReflowStatus&          aStatus,
                   nscoord                  aX,
                   nscoord                  aY,
                   nscoord                  aWidth,
                   nscoord                  aHeight,
                   PRBool                   aMoveFrame)
{
  DO_GLOBAL_REFLOW_COUNT("nsBoxToBlockAdaptor", aReflowState.reason);

#ifdef DEBUG_REFLOW
  nsAdaptorAddIndents();
  printf("Reflowing: ");
  nsFrame::ListTag(stdout, mFrame);
  printf("\n");
  gIndent2++;
#endif

  //printf("width=%d, height=%d\n", aWidth, aHeight);
  /*
  nsIBox* parent;
  GetParentBox(&parent);

 // if (parent->GetStateBits() & NS_STATE_CURRENTLY_IN_DEBUG)
  //   printf("In debug\n");
  */

  nsBoxLayoutMetrics *metrics = BoxMetrics();
  aStatus = NS_FRAME_COMPLETE;

  PRBool redrawAfterReflow = PR_FALSE;
  PRBool needsReflow = PR_FALSE;
  PRBool redrawNow = PR_FALSE;
  nsReflowReason reason;
  nsReflowPath *path = nsnull;

  HandleIncrementalReflow(aState, 
                          aReflowState, 
                          reason,
                          &path,
                          redrawNow,
                          needsReflow, 
                          redrawAfterReflow, 
                          aMoveFrame);

  // If the NS_REFLOW_CALC_MAX_WIDTH flag is set on the nsHTMLReflowMetrics,
  // then we need to do a reflow so that aDesiredSize.mMaximumWidth will be set
  // correctly.
  needsReflow = needsReflow || (aDesiredSize.mFlags & NS_REFLOW_CALC_MAX_WIDTH);

  if (redrawNow)
     Redraw(aState);

  // if we don't need a reflow then 
  // lets see if we are already that size. Yes? then don't even reflow. We are done.
  if (!needsReflow) {
      
      if (aWidth != NS_INTRINSICSIZE && aHeight != NS_INTRINSICSIZE) {
      
          // if the new calculated size has a 0 width or a 0 height
          if ((metrics->mLastSize.width == 0 || metrics->mLastSize.height == 0) && (aWidth == 0 || aHeight == 0)) {
               needsReflow = PR_FALSE;
               aDesiredSize.width = aWidth; 
               aDesiredSize.height = aHeight; 
               SetSize(nsSize(aDesiredSize.width, aDesiredSize.height));
          } else {
            aDesiredSize.width = metrics->mLastSize.width;
            aDesiredSize.height = metrics->mLastSize.height;

            // remove the margin. The rect of our child does not include it but our calculated size does.
            nscoord calcWidth = aWidth; 
            nscoord calcHeight = aHeight; 
            // don't reflow if we are already the right size
            if (metrics->mLastSize.width == calcWidth && metrics->mLastSize.height == calcHeight)
                  needsReflow = PR_FALSE;
            else
                  needsReflow = PR_TRUE;
   
          }
      } else {
          // if the width or height are intrinsic alway reflow because
          // we don't know what it should be.
         needsReflow = PR_TRUE;
      }
  }
                             
  // ok now reflow the child into the spacers calculated space
  if (needsReflow) {

    nsMargin border(0,0,0,0);
    GetBorderAndPadding(border);


    aDesiredSize.width = 0;
    aDesiredSize.height = 0;

    nsSize size(aWidth, aHeight);

    // create a reflow state to tell our child to flow at the given size.
    if (size.height != NS_INTRINSICSIZE) {
        size.height -= (border.top + border.bottom);
        if (size.height < 0)
          size.height = 0;
    }

    if (size.width != NS_INTRINSICSIZE) {
        size.width -= (border.left + border.right);
        if (size.width < 0)
          size.width = 0;
    }

    // Create with a reason of resize, and then change the `reason'
    // and `path' appropriately (since for incremental reflow, we'll
    // be mangling it so completely).
    nsHTMLReflowState reflowState(aPresContext, aReflowState, this,
                                  nsSize(size.width, NS_INTRINSICSIZE),
                                  eReflowReason_Resize);
    reflowState.reason = reason;
    reflowState.path   = path;

    // XXX this needs to subtract out the border and padding of mFrame since it is content size
    reflowState.mComputedWidth = size.width;
    reflowState.mComputedHeight = size.height;

    // if we were marked for style change.
    // 1) see if we are just supposed to do a resize if so convert to a style change. Kill 2 birds
    //    with 1 stone.
    // 2) If the command is incremental. See if its style change. If it is everything is ok if not
    //    we need to do a second reflow with the style change.
    // XXXwaterson This logic seems _very_ squirrely.
    if (metrics->mStyleChange) {
      if (reflowState.reason == eReflowReason_Resize) {
         // maxElementSize does not work on style change reflows.
         // so remove it if set.
         // XXXwaterson why doesn't MES computation work with a style change reflow?
         aDesiredSize.mComputeMEW = PR_FALSE;

         reflowState.reason = eReflowReason_StyleChange;
      }
      else if (reason == eReflowReason_Incremental) {
        PRBool reflowChild = PR_TRUE;

        if (path->mReflowCommand &&
            path->FirstChild() == path->EndChildren() &&
            path->mReflowCommand->Type() == eReflowType_StyleChanged) {
          // There's an incremental reflow targeted directly at our
          // frame, and our frame only (i.e., none of our descendants
          // are targets).
          reflowChild = PR_FALSE;
        }

        if (reflowChild) {
#ifdef DEBUG_waterson
          printf("*** nsBoxToBlockAdaptor::Reflow: performing extra reflow on child frame\n");
#endif

#ifdef DEBUG_REFLOW
          nsAdaptorAddIndents();
          printf("Size=(%d,%d)\n",reflowState.mComputedWidth, reflowState.mComputedHeight);
          nsAdaptorAddIndents();
          nsAdaptorPrintReason(reflowState);
          printf("\n");
#endif

          WillReflow(aPresContext);
          Reflow(aPresContext, aDesiredSize, reflowState, aStatus);
          DidReflow(aPresContext, &reflowState, NS_FRAME_REFLOW_FINISHED);
          reflowState.mComputedWidth = aDesiredSize.width - (border.left + border.right);
          reflowState.availableWidth = reflowState.mComputedWidth;
          reflowState.reason = eReflowReason_StyleChange;
          reflowState.path = nsnull;
        }
      }

      metrics->mStyleChange = PR_FALSE;
    }

    #ifdef DEBUG_REFLOW
      nsAdaptorAddIndents();
      printf("Size=(%d,%d)\n",reflowState.mComputedWidth, reflowState.mComputedHeight);
      nsAdaptorAddIndents();
      nsAdaptorPrintReason(reflowState);
      printf("\n");
    #endif

       // place the child and reflow
    WillReflow(aPresContext);

    Reflow(aPresContext, aDesiredSize, reflowState, aStatus);

    NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");

    // Save the ascent.  (bug 103925)
    PRBool isCollapsed = PR_FALSE;
    IsCollapsed(aState, isCollapsed);
    if (isCollapsed) {
      metrics->mAscent = 0;
    } else {
      metrics->mAscent = aDesiredSize.ascent;
    }

   // printf("width: %d, height: %d\n", aDesiredSize.mCombinedArea.width, aDesiredSize.mCombinedArea.height);

    // see if the overflow option is set. If it is then if our child's bounds overflow then
    // we will set the child's rect to include the overflow size.
       if (GetStateBits() & NS_FRAME_OUTSIDE_CHILDREN) {
         // make sure we store the overflow size

         // This kinda sucks. We should be able to handle the case
         // where there's overflow above or to the left of the
         // origin. But for now just chop that stuff off.
         metrics->mOverflow.width = aDesiredSize.mOverflowArea.XMost();
         metrics->mOverflow.height = aDesiredSize.mOverflowArea.YMost();

         // include the overflow size in our child's rect?
         if (metrics->mIncludeOverflow) {
             //printf("OutsideChildren width=%d, height=%d\n", aDesiredSize.mOverflowArea.width, aDesiredSize.mOverflowArea.height);
             aDesiredSize.width = aDesiredSize.mOverflowArea.XMost();
             if (aDesiredSize.width <= aWidth)
               aDesiredSize.height = aDesiredSize.mOverflowArea.YMost();
             else {
              if (aDesiredSize.width > aWidth)
              {
                 reflowState.mComputedWidth = aDesiredSize.width - (border.left + border.right);
                 reflowState.availableWidth = reflowState.mComputedWidth;
                 reflowState.reason = eReflowReason_Resize;
                 reflowState.path = nsnull;
                 DidReflow(aPresContext, &reflowState, NS_FRAME_REFLOW_FINISHED);
                 #ifdef DEBUG_REFLOW
                  nsAdaptorAddIndents();
                  nsAdaptorPrintReason(reflowState);
                  printf("\n");
                 #endif
                 WillReflow(aPresContext);
                 Reflow(aPresContext, aDesiredSize, reflowState, aStatus);
                 if (GetStateBits() & NS_FRAME_OUTSIDE_CHILDREN)
                    aDesiredSize.height = aDesiredSize.mOverflowArea.YMost();

              }
             }
         }
       } else {
         metrics->mOverflow.width  = aDesiredSize.width;
         metrics->mOverflow.height = aDesiredSize.height;
       }

    if (redrawAfterReflow) {
       nsRect r = GetRect();
       r.width = aDesiredSize.width;
       r.height = aDesiredSize.height;
       Redraw(aState, &r);
    }

    PRBool changedSize = PR_FALSE;

    if (metrics->mLastSize.width != aDesiredSize.width || metrics->mLastSize.height != aDesiredSize.height)
       changedSize = PR_TRUE;
  
    PRUint32 layoutFlags = aState.LayoutFlags();
    nsContainerFrame::FinishReflowChild(this, aPresContext, &reflowState,
                                        aDesiredSize, aX, aY, layoutFlags | NS_FRAME_NO_MOVE_FRAME);
  } else {
    aDesiredSize.ascent = metrics->mBlockAscent;
  }

  // Clip the path we just reflowed, so that we don't incrementally
  // reflow it again: subsequent reflows will be treated as resize
  // reflows.
  if (path)
    PruneReflowPathFor(path->mFrame, aReflowState.path);
  
#ifdef DEBUG_REFLOW
  if (aHeight != NS_INTRINSICSIZE && aDesiredSize.height != aHeight)
  {
          nsAdaptorAddIndents();
          printf("*****got taller!*****\n");
         
  }
  if (aWidth != NS_INTRINSICSIZE && aDesiredSize.width != aWidth)
  {
          nsAdaptorAddIndents();
          printf("*****got wider!******\n");
         
  }
#endif

  if (aWidth == NS_INTRINSICSIZE)
     aWidth = aDesiredSize.width;

  if (aHeight == NS_INTRINSICSIZE)
     aHeight = aDesiredSize.height;

  metrics->mLastSize.width = aDesiredSize.width;
  metrics->mLastSize.height = aDesiredSize.height;

#ifdef DEBUG_REFLOW
  gIndent2--;
#endif

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

// Look for aFrame in the specified reflow path's tree, returning the
// reflow path node corresponding to the frame if we find it.
static nsReflowPath *
FindReflowPathFor(nsIFrame *aFrame, nsReflowPath *aReflowPath)
{
  nsReflowPath::iterator iter, end = aReflowPath->EndChildren();
  for (iter = aReflowPath->FirstChild(); iter != end; ++iter) {
    if (*iter == aFrame)
      return iter.get();

    nsReflowPath *subtree = FindReflowPathFor(aFrame, iter.get());
    if (subtree)
      return subtree;
  }

  return nsnull;
}

void
nsFrame::HandleIncrementalReflow(nsBoxLayoutState& aState, 
                                 const nsHTMLReflowState& aReflowState,
                                 nsReflowReason& aReason,
                                 nsReflowPath** aReflowPath,
                                 PRBool& aRedrawNow,
                                 PRBool& aNeedsReflow,
                                 PRBool& aRedrawAfterReflow,
                                 PRBool& aMoveFrame)
{
  nsFrameState childState = GetStateBits();

  aReason = aReflowState.reason;

    // handle or different types of reflow
  switch(aReason)
  {
   // if the child we are reflowing is the child we popped off the incremental 
   // reflow chain then we need to reflow it no matter what.
   // if its not the child we got from the reflow chain then this child needs reflow
   // because as a side effect of the incremental child changing size it needs to be resized.
   // This will happen a lot when a box that contains 2 children with different flexibilities
   // if on child gets bigger the other is affected because it is proprotional to the first.
   // so it might need to be resized. But we don't need to reflow it. If it is already the
   // needed size then we will do nothing. 
   case eReflowReason_Incremental: {

      // Grovel through the reflow path's children to find the path
      // that corresponds to the current frame. If we can't find a
      // child, then we'll convert the reflow to a dirty reflow,
      // below.
      nsReflowPath *path = FindReflowPathFor(this, aReflowState.path);
      if (path) {
          aNeedsReflow = PR_TRUE;

          // Return the path that we've found so that HTML incremental
          // reflow can proceed normally.
          if (aReflowPath)
            *aReflowPath = path;

          // if we hit the target then we have used up the chain.
          // next time a layout 
          break;
      } 

      // fall into dirty if the incremental child was use. It should be treated as a 
   }

   // if its dirty then see if the child we want to reflow is dirty. If it is then
   // mark it as needing to be reflowed.
   case eReflowReason_Dirty: {
        // XXX nsBlockFrames don't seem to be able to handle a reason of Dirty. So we  
        // send down a resize instead. If we did send down the dirty we would have wrapping problems. If you 
        // look at the main page it will initially come up ok but will have a unneeded horizontal 
        // scrollbar if you resize it will fix it self. The real fix is to fix block frame but
        // this will fix it for beta3.
        if (childState & NS_FRAME_FIRST_REFLOW) 
           aReason = eReflowReason_Initial;
        else
           aReason = eReflowReason_Resize;

        // get the frame state to see if it needs reflow
        aNeedsReflow = BoxMetrics()->mStyleChange || (childState & NS_FRAME_IS_DIRTY) || (childState & NS_FRAME_HAS_DIRTY_CHILDREN);

        // but of course by definition dirty reflows are supposed to redraw so
        // lets signal that we need to do that. We want to do it after as well because
        // the object may have changed size.
        if (aNeedsReflow) {
           aRedrawNow = PR_TRUE;
           aRedrawAfterReflow = PR_TRUE;
           //printf("Redrawing!!!/n");
        }

   } break;

   // if the a resize reflow then it doesn't need to be reflowed. Only if the size is different
   // from the new size would we actually do a reflow
   case eReflowReason_Resize:
       // blocks sometimes send resizes even when its children are dirty! We need to make sure we
       // repair in these cases. So check the flags here.
       aNeedsReflow = BoxMetrics()->mStyleChange || (childState & NS_FRAME_IS_DIRTY) || (childState & NS_FRAME_HAS_DIRTY_CHILDREN);
   break;

   // if its an initial reflow we must place the child.
   // otherwise we might think it was already placed when it wasn't
   case eReflowReason_Initial:
       aMoveFrame = PR_TRUE;
       aNeedsReflow = PR_TRUE;
   break;

   default:
       aNeedsReflow = PR_TRUE;
 
  }
}

PRBool
nsFrame::GetWasCollapsed(nsBoxLayoutState& aState)
{
  return BoxMetrics()->mWasCollapsed;
}

void
nsFrame::SetWasCollapsed(nsBoxLayoutState& aState, PRBool aCollapsed)
{
  BoxMetrics()->mWasCollapsed = aCollapsed;
}

PRBool 
nsFrame::CanSetMaxElementWidth(nsBoxLayoutState& aState, nsReflowReason& aReason, nsReflowPath **aReflowPath)
{
      PRBool redrawAfterReflow = PR_FALSE;
      PRBool needsReflow = PR_FALSE;
      PRBool redrawNow = PR_FALSE;
      PRBool move = PR_TRUE;
      const nsHTMLReflowState* reflowState = aState.GetReflowState();

      HandleIncrementalReflow(aState, 
                              *reflowState, 
                              aReason,
                              aReflowPath,
                              redrawNow,
                              needsReflow, 
                              redrawAfterReflow, 
                              move);

      // only  incremental reflows can handle maxelementsize being set.
      if (reflowState->reason == eReflowReason_Incremental) {
        nsReflowPath *path = *aReflowPath;
        if (path && path->mReflowCommand &&
            path->mReflowCommand->Type() == eReflowType_StyleChanged) {
          // MaxElement doesn't work on style change reflows.. :-(
          // XXXwaterson why?
          return PR_FALSE;
        }

        return PR_TRUE;
      }

      return PR_FALSE;
}

nsBoxLayoutMetrics*
nsFrame::BoxMetrics() const
{
  nsBoxLayoutMetrics* metrics =
    NS_STATIC_CAST(nsBoxLayoutMetrics*, GetProperty(nsLayoutAtoms::boxMetricsProperty));
  NS_ASSERTION(metrics, "A box layout method was called but InitBoxMetrics was never called");
  return metrics;
}

NS_IMETHODIMP
nsFrame::SetParent(const nsIFrame* aParent)
{
  PRBool wasBoxWrapped = IsBoxWrapped();
  nsIFrame::SetParent(aParent);
  if (!wasBoxWrapped && IsBoxWrapped())
    InitBoxMetrics(PR_TRUE);
  else if (wasBoxWrapped && !IsBoxWrapped())
    DeleteProperty(nsLayoutAtoms::boxMetricsProperty);

  if (aParent && aParent->IsBoxFrame()) {
    PRBool needsWidget = PR_FALSE;
    aParent->ChildrenMustHaveWidgets(needsWidget);
    if (needsWidget) {
        nsHTMLContainerFrame::CreateViewForFrame(this, nsnull, PR_TRUE);
        nsIView* view = GetView();
        if (!view->HasWidget())
          CreateWidgetForView(view);
    }
  }

  return NS_OK;
}

static void
DeleteBoxMetrics(void    *aObject,
                 nsIAtom *aPropertyName,
                 void    *aPropertyValue,
                 void    *aData)
{
  delete NS_STATIC_CAST(nsBoxLayoutMetrics*, aPropertyValue);
}

void
nsFrame::InitBoxMetrics(PRBool aClear)
{
  if (aClear)
    DeleteProperty(nsLayoutAtoms::boxMetricsProperty);

  nsBoxLayoutMetrics *metrics = new nsBoxLayoutMetrics();
  SetProperty(nsLayoutAtoms::boxMetricsProperty, metrics, DeleteBoxMetrics);

  NeedsRecalc();
  metrics->mBlockAscent = 0;
  metrics->mLastSize.SizeTo(0, 0);
  metrics->mOverflow.SizeTo(0, 0);
  metrics->mIncludeOverflow = PR_TRUE;
  metrics->mWasCollapsed = PR_FALSE;
  metrics->mStyleChange = PR_FALSE;
}

// Box layout debugging
#ifdef DEBUG_REFLOW
PRInt32 gIndent2 = 0;

void
nsAdaptorAddIndents()
{
    for(PRInt32 i=0; i < gIndent2; i++)
    {
        printf(" ");
    }
}

void
nsAdaptorPrintReason(nsHTMLReflowState& aReflowState)
{
    char* reflowReasonString;

    switch(aReflowState.reason) 
    {
        case eReflowReason_Initial:
          reflowReasonString = "initial";
          break;

        case eReflowReason_Resize:
          reflowReasonString = "resize";
          break;
        case eReflowReason_Dirty:
          reflowReasonString = "dirty";
          break;
        case eReflowReason_StyleChange:
          reflowReasonString = "stylechange";
          break;
        case eReflowReason_Incremental: 
        {
            switch (aReflowState.reflowCommand->Type()) {
              case eReflowType_StyleChanged:
                 reflowReasonString = "incremental (StyleChanged)";
              break;
              case eReflowType_ReflowDirty:
                 reflowReasonString = "incremental (ReflowDirty)";
              break;
              default:
                 reflowReasonString = "incremental (Unknown)";
            }
        }                             
        break;
        default:
          reflowReasonString = "unknown";
          break;
    }

    printf("%s",reflowReasonString);
}

#endif
#ifdef DEBUG_LAYOUT
void
nsFrame::GetBoxName(nsAutoString& aName)
{
   nsIFrameDebug*  frameDebug;
   nsAutoString name;
   if (NS_SUCCEEDED(QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
      frameDebug->GetFrameName(name);
   }

  aName = name;
}
#endif

#ifdef NS_DEBUG
static void
GetTagName(nsFrame* aFrame, nsIContent* aContent, PRIntn aResultSize,
           char* aResult)
{
  const char *nameStr = "";
  if (aContent) {
    aContent->Tag()->GetUTF8String(&nameStr);
  }
  PR_snprintf(aResult, aResultSize, "%s@%p", nameStr, aFrame);
}

void
nsFrame::Trace(const char* aMethod, PRBool aEnter)
{
  if (NS_FRAME_LOG_TEST(gLogModule, NS_FRAME_TRACE_CALLS)) {
    char tagbuf[40];
    GetTagName(this, mContent, sizeof(tagbuf), tagbuf);
    PR_LogPrint("%s: %s %s", tagbuf, aEnter ? "enter" : "exit", aMethod);
  }
}

void
nsFrame::Trace(const char* aMethod, PRBool aEnter, nsReflowStatus aStatus)
{
  if (NS_FRAME_LOG_TEST(gLogModule, NS_FRAME_TRACE_CALLS)) {
    char tagbuf[40];
    GetTagName(this, mContent, sizeof(tagbuf), tagbuf);
    PR_LogPrint("%s: %s %s, status=%scomplete%s",
                tagbuf, aEnter ? "enter" : "exit", aMethod,
                NS_FRAME_IS_NOT_COMPLETE(aStatus) ? "not" : "",
                (NS_FRAME_REFLOW_NEXTINFLOW & aStatus) ? "+reflow" : "");
  }
}

void
nsFrame::TraceMsg(const char* aFormatString, ...)
{
  if (NS_FRAME_LOG_TEST(gLogModule, NS_FRAME_TRACE_CALLS)) {
    // Format arguments into a buffer
    char argbuf[200];
    va_list ap;
    va_start(ap, aFormatString);
    PR_vsnprintf(argbuf, sizeof(argbuf), aFormatString, ap);
    va_end(ap);

    char tagbuf[40];
    GetTagName(this, mContent, sizeof(tagbuf), tagbuf);
    PR_LogPrint("%s: %s", tagbuf, argbuf);
  }
}

void
nsFrame::VerifyDirtyBitSet(nsIFrame* aFrameList)
{
  for (nsIFrame*f = aFrameList; f; f = f->GetNextSibling()) {
    NS_ASSERTION(f->GetStateBits() & NS_FRAME_IS_DIRTY, "dirty bit not set");
  }
}

// Start Display Reflow
#ifdef DEBUG

MOZ_DECL_CTOR_COUNTER(DR_cookie)

DR_cookie::DR_cookie(nsPresContext*          aPresContext,
                     nsIFrame*                aFrame, 
                     const nsHTMLReflowState& aReflowState,
                     nsHTMLReflowMetrics&     aMetrics,
                     nsReflowStatus&          aStatus)
  :mPresContext(aPresContext), mFrame(aFrame), mReflowState(aReflowState), mMetrics(aMetrics), mStatus(aStatus)
{
  MOZ_COUNT_CTOR(DR_cookie);
  mValue = nsFrame::DisplayReflowEnter(aPresContext, mFrame, mReflowState);
}

DR_cookie::~DR_cookie()
{
  MOZ_COUNT_DTOR(DR_cookie);
  nsFrame::DisplayReflowExit(mPresContext, mFrame, mMetrics, mStatus, mValue);
}

struct DR_FrameTypeInfo;
struct DR_FrameTreeNode;
struct DR_Rule;

struct DR_State
{
  DR_State();
  ~DR_State();
  void Init();
  void AddFrameTypeInfo(nsIAtom* aFrameType,
                        const char* aFrameNameAbbrev,
                        const char* aFrameName);
  DR_FrameTypeInfo* GetFrameTypeInfo(nsIAtom* aFrameType);
  DR_FrameTypeInfo* GetFrameTypeInfo(char* aFrameName);
  void InitFrameTypeTable();
  DR_FrameTreeNode* CreateTreeNode(nsIFrame*                aFrame,
                                   const nsHTMLReflowState& aReflowState);
  void FindMatchingRule(DR_FrameTreeNode& aNode);
  PRBool RuleMatches(DR_Rule&          aRule,
                     DR_FrameTreeNode& aNode);
  PRBool GetToken(FILE* aFile,
                  char* aBuf);
  DR_Rule* ParseRule(FILE* aFile);
  void ParseRulesFile();
  void AddRule(nsVoidArray& aRules,
               DR_Rule&     aRule);
  PRBool IsWhiteSpace(int c);
  PRBool GetNumber(char*    aBuf, 
                 PRInt32&  aNumber);
  void PrettyUC(nscoord aSize,
                char*   aBuf);
  void DisplayFrameTypeInfo(nsIFrame* aFrame,
                            PRInt32   aIndent);
  void DeleteTreeNode(DR_FrameTreeNode& aNode);

  PRBool      mInited;
  PRBool      mActive;
  PRInt32     mCount;
  nsVoidArray mWildRules;
  PRInt32     mAssert;
  PRInt32     mIndentStart;
  PRBool      mIndentUndisplayedFrames;
  nsVoidArray mFrameTypeTable;
  PRBool      mDisplayPixelErrors;

  // reflow specific state
  nsVoidArray mFrameTreeLeaves;
};

static DR_State *DR_state; // the one and only DR_State

struct DR_RulePart 
{
  DR_RulePart(nsIAtom* aFrameType) : mFrameType(aFrameType), mNext(0) {}
  void Destroy();

  nsIAtom*     mFrameType;
  DR_RulePart* mNext;
};

void DR_RulePart::Destroy()
{
  if (mNext) {
    mNext->Destroy();
  }
  delete this;
}

MOZ_DECL_CTOR_COUNTER(DR_Rule)

struct DR_Rule 
{
  DR_Rule() : mLength(0), mTarget(nsnull), mDisplay(PR_FALSE) {
    MOZ_COUNT_CTOR(DR_Rule);
  }
  ~DR_Rule() {
    if (mTarget) mTarget->Destroy();
    MOZ_COUNT_DTOR(DR_Rule);
  }
  void AddPart(nsIAtom* aFrameType);

  PRUint32      mLength;
  DR_RulePart*  mTarget;
  PRBool        mDisplay;
};

void DR_Rule::AddPart(nsIAtom* aFrameType)
{
  DR_RulePart* newPart = new DR_RulePart(aFrameType);
  newPart->mNext = mTarget;
  mTarget = newPart;
  mLength++;
}

MOZ_DECL_CTOR_COUNTER(DR_FrameTypeInfo)

struct DR_FrameTypeInfo
{
  DR_FrameTypeInfo(nsIAtom* aFrmeType, const char* aFrameNameAbbrev, const char* aFrameName);
  ~DR_FrameTypeInfo() { 
      MOZ_COUNT_DTOR(DR_FrameTypeInfo);
      PRInt32 numElements;
      numElements = mRules.Count();
      for (PRInt32 i = numElements - 1; i >= 0; i--) {
        delete (DR_Rule *)mRules.ElementAt(i);
      }
   }

  nsIAtom*    mType;
  char        mNameAbbrev[16];
  char        mName[32];
  nsVoidArray mRules;
};

DR_FrameTypeInfo::DR_FrameTypeInfo(nsIAtom* aFrameType, 
                                   const char* aFrameNameAbbrev, 
                                   const char* aFrameName)
{
  mType = aFrameType;
  strcpy(mNameAbbrev, aFrameNameAbbrev);
  strcpy(mName, aFrameName);
  MOZ_COUNT_CTOR(DR_FrameTypeInfo);
}

MOZ_DECL_CTOR_COUNTER(DR_FrameTreeNode)

struct DR_FrameTreeNode
{
  DR_FrameTreeNode(nsIFrame* aFrame, DR_FrameTreeNode* aParent) : mFrame(aFrame), mParent(aParent), mDisplay(0), mIndent(0)
  {
    MOZ_COUNT_CTOR(DR_FrameTreeNode);
  }

  ~DR_FrameTreeNode()
  {
    MOZ_COUNT_DTOR(DR_FrameTreeNode);
  }

  nsIFrame*         mFrame;
  DR_FrameTreeNode* mParent;
  PRBool            mDisplay;
  PRUint32          mIndent;
};

// DR_State implementation

MOZ_DECL_CTOR_COUNTER(DR_State)

DR_State::DR_State() 
: mInited(PR_FALSE), mActive(PR_FALSE), mCount(0), mAssert(-1), mIndentStart(0), 
  mIndentUndisplayedFrames(PR_FALSE), mDisplayPixelErrors(PR_FALSE)
{
  MOZ_COUNT_CTOR(DR_State);
}

void DR_State::Init() 
{
  char* env = PR_GetEnv("GECKO_DISPLAY_REFLOW_ASSERT");
  PRInt32 num;
  if (env) {
    if (GetNumber(env, num)) 
      mAssert = num;
    else 
      printf("GECKO_DISPLAY_REFLOW_ASSERT - invalid value = %s", env);
  }

  env = PR_GetEnv("GECKO_DISPLAY_REFLOW_INDENT_START");
  if (env) {
    if (GetNumber(env, num)) 
      mIndentStart = num;
    else 
      printf("GECKO_DISPLAY_REFLOW_INDENT_START - invalid value = %s", env);
  }

  env = PR_GetEnv("GECKO_DISPLAY_REFLOW_INDENT_UNDISPLAYED_FRAMES");
  if (env) {
    if (GetNumber(env, num)) 
      mIndentUndisplayedFrames = num;
    else 
      printf("GECKO_DISPLAY_REFLOW_INDENT_UNDISPLAYED_FRAMES - invalid value = %s", env);
  }

  env = PR_GetEnv("GECKO_DISPLAY_REFLOW_FLAG_PIXEL_ERRORS");
  if (env) {
    if (GetNumber(env, num)) 
      mDisplayPixelErrors = num;
    else 
      printf("GECKO_DISPLAY_REFLOW_FLAG_PIXEL_ERRORS - invalid value = %s", env);
  }

  InitFrameTypeTable();
  ParseRulesFile();
  mInited = PR_TRUE;
}

DR_State::~DR_State()
{
  MOZ_COUNT_DTOR(DR_State);
  PRInt32 numElements, i;
  numElements = mWildRules.Count();
  for (i = numElements - 1; i >= 0; i--) {
    delete (DR_Rule *)mWildRules.ElementAt(i);
  }
  numElements = mFrameTreeLeaves.Count();
  for (i = numElements - 1; i >= 0; i--) {
    delete (DR_FrameTreeNode *)mFrameTreeLeaves.ElementAt(i);
  }
  numElements = mFrameTypeTable.Count();
  for (i = numElements - 1; i >= 0; i--) {
    delete (DR_FrameTypeInfo *)mFrameTypeTable.ElementAt(i);
  }
}

PRBool DR_State::GetNumber(char*     aBuf, 
                           PRInt32&  aNumber)
{
  if (sscanf(aBuf, "%d", &aNumber) > 0) 
    return PR_TRUE;
  else 
    return PR_FALSE;
}

PRBool DR_State::IsWhiteSpace(int c) {
  return (c == ' ') || (c == '\t') || (c == '\n') || (c == '\r');
}

PRBool DR_State::GetToken(FILE* aFile,
                          char* aBuf)
{
  PRBool haveToken = PR_FALSE;
  aBuf[0] = 0;
  // get the 1st non whitespace char
  int c = -1;
  for (c = getc(aFile); (c > 0) && IsWhiteSpace(c); c = getc(aFile)) {
  }

  if (c > 0) {
    haveToken = PR_TRUE;
    aBuf[0] = c;
    // get everything up to the next whitespace char
    PRInt32 cX;
    for (cX = 1, c = getc(aFile); ; cX++, c = getc(aFile)) {
      if (c < 0) { // EOF
        ungetc(' ', aFile); 
        break;
      }
      else {
        if (IsWhiteSpace(c)) {
          break;
        }
        else {
          aBuf[cX] = c;
        }
      }
    }
    aBuf[cX] = 0;
  }
  return haveToken;
}

DR_Rule* DR_State::ParseRule(FILE* aFile)
{
  char buf[128];
  PRInt32 doDisplay;
  DR_Rule* rule = nsnull;
  while (GetToken(aFile, buf)) {
    if (GetNumber(buf, doDisplay)) {
      if (rule) { 
        rule->mDisplay = (PRBool)doDisplay;
        break;
      }
      else {
        printf("unexpected token - %s \n", buf);
      }
    }
    else {
      if (!rule) {
        rule = new DR_Rule;
      }
      if (strcmp(buf, "*") == 0) {
        rule->AddPart(nsnull);
      }
      else {
        DR_FrameTypeInfo* info = GetFrameTypeInfo(buf);
        if (info) {
          rule->AddPart(info->mType);
        }
        else {
          printf("invalid frame type - %s \n", buf);
        }
      }
    }
  }
  return rule;
}

void DR_State::AddRule(nsVoidArray& aRules,
                       DR_Rule&     aRule)
{
  PRInt32 numRules = aRules.Count();
  for (PRInt32 ruleX = 0; ruleX < numRules; ruleX++) {
    DR_Rule* rule = (DR_Rule*)aRules.ElementAt(ruleX);
    NS_ASSERTION(rule, "program error");
    if (aRule.mLength > rule->mLength) {
      aRules.InsertElementAt(&aRule, ruleX);
      return;
    }
  }
  aRules.AppendElement(&aRule);
}

void DR_State::ParseRulesFile()
{
  char* path = PR_GetEnv("GECKO_DISPLAY_REFLOW_RULES_FILE");
  if (path) {
    FILE* inFile = fopen(path, "r");
    if (inFile) {
      for (DR_Rule* rule = ParseRule(inFile); rule; rule = ParseRule(inFile)) {
        if (rule->mTarget) {
          nsIAtom* fType = rule->mTarget->mFrameType;
          if (fType) {
            DR_FrameTypeInfo* info = GetFrameTypeInfo(fType);
            if (info) {
              AddRule(info->mRules, *rule);
            }
          }
          else {
            AddRule(mWildRules, *rule);
          }
          mActive = PR_TRUE;
        }
      }
    }
  }
}


void DR_State::AddFrameTypeInfo(nsIAtom* aFrameType,
                                const char* aFrameNameAbbrev,
                                const char* aFrameName)
{
  mFrameTypeTable.AppendElement(new DR_FrameTypeInfo(aFrameType, aFrameNameAbbrev, aFrameName));
}

DR_FrameTypeInfo* DR_State::GetFrameTypeInfo(nsIAtom* aFrameType)
{
  PRInt32 numEntries = mFrameTypeTable.Count();
  NS_ASSERTION(numEntries != 0, "empty FrameTypeTable");
  for (PRInt32 i = 0; i < numEntries; i++) {
    DR_FrameTypeInfo* info = (DR_FrameTypeInfo*)mFrameTypeTable.ElementAt(i);
    if (info && (info->mType == aFrameType)) {
      return info;
    }
  }
  return (DR_FrameTypeInfo*)mFrameTypeTable.ElementAt(numEntries - 1); // return unknown frame type
}

DR_FrameTypeInfo* DR_State::GetFrameTypeInfo(char* aFrameName)
{
  PRInt32 numEntries = mFrameTypeTable.Count();
  NS_ASSERTION(numEntries != 0, "empty FrameTypeTable");
  for (PRInt32 i = 0; i < numEntries; i++) {
    DR_FrameTypeInfo* info = (DR_FrameTypeInfo*)mFrameTypeTable.ElementAt(i);
    if (info && ((strcmp(aFrameName, info->mName) == 0) || (strcmp(aFrameName, info->mNameAbbrev) == 0))) {
      return info;
    }
  }
  return (DR_FrameTypeInfo*)mFrameTypeTable.ElementAt(numEntries - 1); // return unknown frame type
}

void DR_State::InitFrameTypeTable()
{  
  AddFrameTypeInfo(nsLayoutAtoms::areaFrame,             "area",      "area");
  AddFrameTypeInfo(nsLayoutAtoms::blockFrame,            "block",     "block");
  AddFrameTypeInfo(nsLayoutAtoms::brFrame,               "br",        "br");
  AddFrameTypeInfo(nsLayoutAtoms::bulletFrame,           "bullet",    "bullet");
  AddFrameTypeInfo(nsLayoutAtoms::gfxButtonControlFrame, "button",    "gfxButtonControl");
  AddFrameTypeInfo(nsLayoutAtoms::HTMLCanvasFrame,       "HTMLCanvas","HTMLCanvas");
  AddFrameTypeInfo(nsLayoutAtoms::subDocumentFrame,      "subdoc",    "subDocument");
  AddFrameTypeInfo(nsLayoutAtoms::imageFrame,            "img",       "image");
  AddFrameTypeInfo(nsLayoutAtoms::inlineFrame,           "inline",    "inline");
  AddFrameTypeInfo(nsLayoutAtoms::letterFrame,           "letter",    "letter");
  AddFrameTypeInfo(nsLayoutAtoms::lineFrame,             "line",      "line");
  AddFrameTypeInfo(nsLayoutAtoms::listControlFrame,      "select",    "select");
  AddFrameTypeInfo(nsLayoutAtoms::objectFrame,           "obj",       "object");
  AddFrameTypeInfo(nsLayoutAtoms::pageFrame,             "page",      "page");
  AddFrameTypeInfo(nsLayoutAtoms::placeholderFrame,      "place",     "placeholder");
  AddFrameTypeInfo(nsLayoutAtoms::positionedInlineFrame, "posInline", "positionedInline");
  AddFrameTypeInfo(nsLayoutAtoms::canvasFrame,           "canvas",    "canvas");
  AddFrameTypeInfo(nsLayoutAtoms::rootFrame,             "root",      "root");
  AddFrameTypeInfo(nsLayoutAtoms::scrollFrame,           "scroll",    "scroll");
  AddFrameTypeInfo(nsLayoutAtoms::tableCaptionFrame,     "caption",   "tableCaption");
  AddFrameTypeInfo(nsLayoutAtoms::tableCellFrame,        "cell",      "tableCell");
  AddFrameTypeInfo(nsLayoutAtoms::bcTableCellFrame,      "bcCell",    "bcTableCell");
  AddFrameTypeInfo(nsLayoutAtoms::tableColFrame,         "col",       "tableCol");
  AddFrameTypeInfo(nsLayoutAtoms::tableColGroupFrame,    "colG",      "tableColGroup");
  AddFrameTypeInfo(nsLayoutAtoms::tableFrame,            "tbl",       "table");
  AddFrameTypeInfo(nsLayoutAtoms::tableOuterFrame,       "tblO",      "tableOuter");
  AddFrameTypeInfo(nsLayoutAtoms::tableRowGroupFrame,    "rowG",      "tableRowGroup");
  AddFrameTypeInfo(nsLayoutAtoms::tableRowFrame,         "row",       "tableRow");
  AddFrameTypeInfo(nsLayoutAtoms::textInputFrame,        "textCtl",   "textInput");
  AddFrameTypeInfo(nsLayoutAtoms::textFrame,             "text",      "text");
  AddFrameTypeInfo(nsLayoutAtoms::viewportFrame,         "VP",        "viewport");
  AddFrameTypeInfo(nsnull,                               "unknown",   "unknown");
}


void DR_State::DisplayFrameTypeInfo(nsIFrame* aFrame,
                                    PRInt32   aIndent)
{ 
  DR_FrameTypeInfo* frameTypeInfo = GetFrameTypeInfo(aFrame->GetType());
  if (frameTypeInfo) {
    for (PRInt32 i = 0; i < aIndent; i++) {
      printf(" ");
    }
    if(!strcmp(frameTypeInfo->mNameAbbrev, "unknown")) {
      nsAutoString  name;
      nsIFrameDebug*  frameDebug;
      if (NS_SUCCEEDED(aFrame->QueryInterface(NS_GET_IID(nsIFrameDebug), (void**)&frameDebug))) {
       frameDebug->GetFrameName(name);
       printf("%s %p ", NS_LossyConvertUCS2toASCII(name).get(), (void*)aFrame);
      }
      else {
        printf("%s %p ", frameTypeInfo->mNameAbbrev, (void*)aFrame);
      }
    }
    else {
      printf("%s %p ", frameTypeInfo->mNameAbbrev, (void*)aFrame);
    }
  }
}

PRBool DR_State::RuleMatches(DR_Rule&          aRule,
                             DR_FrameTreeNode& aNode)
{
  NS_ASSERTION(aRule.mTarget, "program error");

  DR_RulePart* rulePart;
  DR_FrameTreeNode* parentNode;
  for (rulePart = aRule.mTarget->mNext, parentNode = aNode.mParent;
       rulePart && parentNode;
       rulePart = rulePart->mNext, parentNode = parentNode->mParent) {
    if (rulePart->mFrameType) {
      if (parentNode->mFrame) {
        if (rulePart->mFrameType != parentNode->mFrame->GetType()) {
          return PR_FALSE;
        }
      }
      else NS_ASSERTION(PR_FALSE, "program error");
    }
    // else wild card match
  }
  return PR_TRUE;
}

void DR_State::FindMatchingRule(DR_FrameTreeNode& aNode)
{
  if (!aNode.mFrame) {
    NS_ASSERTION(PR_FALSE, "invalid DR_FrameTreeNode \n");
    return;
  }

  PRBool matchingRule = PR_FALSE;

  DR_FrameTypeInfo* info = GetFrameTypeInfo(aNode.mFrame->GetType());
  NS_ASSERTION(info, "program error");
  PRInt32 numRules = info->mRules.Count();
  for (PRInt32 ruleX = 0; ruleX < numRules; ruleX++) {
    DR_Rule* rule = (DR_Rule*)info->mRules.ElementAt(ruleX);
    if (rule && RuleMatches(*rule, aNode)) {
      aNode.mDisplay = rule->mDisplay;
      matchingRule = PR_TRUE;
      break;
    }
  }
  if (!matchingRule) {
    PRInt32 numWildRules = mWildRules.Count();
    for (PRInt32 ruleX = 0; ruleX < numWildRules; ruleX++) {
      DR_Rule* rule = (DR_Rule*)mWildRules.ElementAt(ruleX);
      if (rule && RuleMatches(*rule, aNode)) {
        aNode.mDisplay = rule->mDisplay;
        break;
      }
    }
  }

  if (aNode.mParent) {
    aNode.mIndent = aNode.mParent->mIndent;
    if (aNode.mDisplay || mIndentUndisplayedFrames) {
      aNode.mIndent++;
    }
  }
}
    
DR_FrameTreeNode* DR_State::CreateTreeNode(nsIFrame*                aFrame,
                                           const nsHTMLReflowState& aReflowState)
{
  // find the frame of the parent reflow state (usually just the parent of aFrame)
  const nsHTMLReflowState* parentRS = aReflowState.parentReflowState;
  nsIFrame* parentFrame = (parentRS) ? parentRS->frame : nsnull;

  // find the parent tree node leaf
  DR_FrameTreeNode* parentNode = nsnull;
  
  DR_FrameTreeNode* lastLeaf = nsnull;
  if(mFrameTreeLeaves.Count())
    lastLeaf = (DR_FrameTreeNode*)mFrameTreeLeaves.ElementAt(mFrameTreeLeaves.Count() - 1);
  if (lastLeaf) {
    for (parentNode = lastLeaf; parentNode && (parentNode->mFrame != parentFrame); parentNode = parentNode->mParent) {
    }
  }
  DR_FrameTreeNode* newNode = new DR_FrameTreeNode(aFrame, parentNode);
  FindMatchingRule(*newNode);
  if (lastLeaf && (lastLeaf == parentNode)) {
    mFrameTreeLeaves.RemoveElementAt(mFrameTreeLeaves.Count() - 1);
  }
  mFrameTreeLeaves.AppendElement(newNode);
  mCount++;

  return newNode;
}

void DR_State::PrettyUC(nscoord aSize,
                        char*   aBuf)
{
  if (NS_UNCONSTRAINEDSIZE == aSize) {
    strcpy(aBuf, "UC");
  }
  else {
    if ((nscoord)0xdeadbeefU == aSize)
    {
      strcpy(aBuf, "deadbeef");
    }
    else {
      sprintf(aBuf, "%d", aSize);
    }
  }
}

void DR_State::DeleteTreeNode(DR_FrameTreeNode& aNode)
{
  mFrameTreeLeaves.RemoveElement(&aNode);
  PRInt32 numLeaves = mFrameTreeLeaves.Count();
  if ((0 == numLeaves) || (aNode.mParent != (DR_FrameTreeNode*)mFrameTreeLeaves.ElementAt(numLeaves - 1))) {
    mFrameTreeLeaves.AppendElement(aNode.mParent);
  }
  // delete the tree node 
  delete &aNode;
}

static void
CheckPixelError(nscoord aSize,
                float   aPixelToTwips)
{
  if (NS_UNCONSTRAINEDSIZE != aSize) {
    if ((aSize % NSToCoordRound(aPixelToTwips)) > 0) {
      printf("VALUE %d is not a whole pixel \n", aSize);
    }
  }
}

static void DisplayReflowEnterPrint(nsPresContext*          aPresContext,
                                    nsIFrame*                aFrame,
                                    const nsHTMLReflowState& aReflowState,
                                    DR_FrameTreeNode&        aTreeNode,
                                    PRBool                   aChanged)
{
  if (aTreeNode.mDisplay) {
    DR_state->DisplayFrameTypeInfo(aFrame, aTreeNode.mIndent);

    char width[16];
    char height[16];

    DR_state->PrettyUC(aReflowState.availableWidth, width);
    DR_state->PrettyUC(aReflowState.availableHeight, height);
    if (aReflowState.path && aReflowState.path->mReflowCommand) {
      const char *incr_reason;
      switch(aReflowState.path->mReflowCommand->Type()) {
        case eReflowType_ContentChanged:
          incr_reason = "incr. (Content)";
          break;
        case eReflowType_StyleChanged:
          incr_reason = "incr. (Style)";
          break;
        case eReflowType_ReflowDirty:
          incr_reason = "incr. (Dirty)";
          break;
        default:
          incr_reason = "incr. (Unknown)";
      }
      printf("r=%d %s a=%s,%s ", aReflowState.reason, incr_reason, width, height);
    }
    else {
      printf("r=%d a=%s,%s ", aReflowState.reason, width, height);
    }

    DR_state->PrettyUC(aReflowState.mComputedWidth, width);
    DR_state->PrettyUC(aReflowState.mComputedHeight, height);
    printf("c=%s,%s ", width, height);

    nsIFrame* inFlow = aFrame->GetPrevInFlow();
    if (inFlow) {
      printf("pif=%p ", (void*)inFlow);
    }
    inFlow = aFrame->GetNextInFlow();
    if (inFlow) {
      printf("nif=%p ", (void*)inFlow);
    }
    if (aChanged) 
      printf("CHANGED \n");
    else 
      printf("cnt=%d \n", DR_state->mCount);
    if (DR_state->mDisplayPixelErrors) {
      float p2t = aPresContext->ScaledPixelsToTwips();
      CheckPixelError(aReflowState.availableWidth, p2t);
      CheckPixelError(aReflowState.availableHeight, p2t);
      CheckPixelError(aReflowState.mComputedWidth, p2t);
      CheckPixelError(aReflowState.mComputedHeight, p2t);
    }
  }
}

void* nsFrame::DisplayReflowEnter(nsPresContext*          aPresContext,
                                  nsIFrame*                aFrame,
                                  const nsHTMLReflowState& aReflowState)
{
  if (!DR_state->mInited) DR_state->Init();
  if (!DR_state->mActive) return nsnull;

  NS_ASSERTION(aFrame, "invalid call");

  DR_FrameTreeNode* treeNode = DR_state->CreateTreeNode(aFrame, aReflowState);
  if (treeNode) {
    DisplayReflowEnterPrint(aPresContext, aFrame, aReflowState, *treeNode, PR_FALSE);
  }
  return treeNode;
}

void nsFrame::DisplayReflowExit(nsPresContext*      aPresContext,
                                nsIFrame*            aFrame,
                                nsHTMLReflowMetrics& aMetrics,
                                nsReflowStatus       aStatus,
                                void*                aFrameTreeNode)
{
  if (!DR_state->mActive) return;

  NS_ASSERTION(aFrame, "DisplayReflowExit - invalid call");
  if (!aFrameTreeNode) return;

  DR_FrameTreeNode* treeNode = (DR_FrameTreeNode*)aFrameTreeNode;
  if (treeNode->mDisplay) {
    DR_state->DisplayFrameTypeInfo(aFrame, treeNode->mIndent);

    char width[16];
    char height[16];
    char x[16];
    char y[16];
    DR_state->PrettyUC(aMetrics.width, width);
    DR_state->PrettyUC(aMetrics.height, height);
    printf("d=%s,%s ", width, height);

    if (aMetrics.mComputeMEW) {
      DR_state->PrettyUC(aMetrics.mMaxElementWidth, width);
      printf("me=%s ", width);
    }
    if (aMetrics.mFlags & NS_REFLOW_CALC_MAX_WIDTH) {
      DR_state->PrettyUC(aMetrics.mMaximumWidth, width);
      printf("m=%s ", width);
    }
    if (NS_FRAME_IS_NOT_COMPLETE(aStatus)) {
      printf("status=0x%x", aStatus);
    }
    if (aFrame->GetStateBits() & NS_FRAME_OUTSIDE_CHILDREN) {
       DR_state->PrettyUC(aMetrics.mOverflowArea.x, x);
       DR_state->PrettyUC(aMetrics.mOverflowArea.y, y);
       DR_state->PrettyUC(aMetrics.mOverflowArea.width, width);
       DR_state->PrettyUC(aMetrics.mOverflowArea.height, height);
       printf("o=(%s,%s) %s x %s", x, y, width, height);
       nsRect* storedOverflow = aFrame->GetOverflowAreaProperty();
       if (storedOverflow) {
         if (aMetrics.mOverflowArea != *storedOverflow) {
           DR_state->PrettyUC(storedOverflow->x, x);
           DR_state->PrettyUC(storedOverflow->y, y);
           DR_state->PrettyUC(storedOverflow->width, width);
           DR_state->PrettyUC(storedOverflow->height, height);
           printf("sto=(%s,%s) %s x %s", x, y, width, height);
         }
       }
    }
    printf("\n");
    if (DR_state->mDisplayPixelErrors) {
      float p2t = aPresContext->ScaledPixelsToTwips();
      CheckPixelError(aMetrics.width, p2t);
      CheckPixelError(aMetrics.height, p2t);
      if (aMetrics.mComputeMEW) 
        CheckPixelError(aMetrics.mMaxElementWidth, p2t);
      if (aMetrics.mFlags & NS_REFLOW_CALC_MAX_WIDTH) 
        CheckPixelError(aMetrics.mMaximumWidth, p2t);
    }
  }
  DR_state->DeleteTreeNode(*treeNode);
}

/* static */ void
nsFrame::DisplayReflowStartup()
{
  DR_state = new DR_State();
}

/* static */ void
nsFrame::DisplayReflowShutdown()
{
  delete DR_state;
  DR_state = nsnull;
}

void DR_cookie::Change() const
{
  DR_FrameTreeNode* treeNode = (DR_FrameTreeNode*)mValue;
  if (treeNode && treeNode->mDisplay) {
    DisplayReflowEnterPrint(mPresContext, mFrame, mReflowState, *treeNode, PR_TRUE);
  }
}

#endif
// End Display Reflow

#endif
