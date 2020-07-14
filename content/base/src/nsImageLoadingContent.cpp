/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim: ft=cpp tw=78 sw=2 et ts=2
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
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
 * Boris Zbarsky <bzbarsky@mit.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsImageLoadingContent.h"
#include "nsAutoPtr.h"
#include "nsContentErrors.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsINodeInfo.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindow.h"
#include "nsServiceManagerUtils.h"
#include "nsContentPolicyUtils.h"
#include "nsIURI.h"
#include "nsILoadGroup.h"
#include "imgIContainer.h"
#include "gfxIImageFrame.h"
#include "imgILoader.h"
#include "plevent.h"
#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"
#include "nsNetUtil.h"

#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsGUIEvent.h"
#include "nsDummyLayoutRequest.h"

#include "nsIChannel.h"
#include "nsIStreamListener.h"

#include "nsLayoutAtoms.h"
#include "nsIFrame.h"
#include "nsIDOMNode.h"

#include "nsContentUtils.h"
#include "nsIContentPolicy.h"
#include "nsContentPolicyUtils.h"
#include "nsDOMClassInfo.h"

#ifdef DEBUG_chb
static void PrintReqURL(imgIRequest* req) {
  if (!req) {
    printf("(null req)\n");
    return;
  }

  nsCOMPtr<nsIURI> uri;
  req->GetURI(getter_AddRefs(uri));
  if (!uri) {
    printf("(null uri)\n");
    return;
  }

  nsCAutoString spec;
  uri->GetSpec(spec);
  printf("spec='%s'\n", spec.get());
}
#endif /* DEBUG_chb */


nsImageLoadingContent::nsImageLoadingContent()
  : mObserverList(nsnull),
    mImageBlockingStatus(nsIContentPolicy::ACCEPT),
    mRootRefCount(0),
    mLoadingEnabled(PR_TRUE)
{
  if (!nsContentUtils::GetImgLoader())
    mLoadingEnabled = PR_FALSE;
}

void
nsImageLoadingContent::DestroyImageLoadingContent()
{
  // Cancel our requests so they won't hold stale refs to us
  if (mCurrentRequest) {
    mCurrentRequest->Cancel(NS_ERROR_FAILURE);
    mCurrentRequest = nsnull;
  }
  if (mPendingRequest) {
    mPendingRequest->Cancel(NS_ERROR_FAILURE);
    mPendingRequest = nsnull;
  }

  // This can actually fire for multipart/x-mixed-replace, since if the
  // load is canceled between parts (e.g., by cancelling the load
  // group), we won't get any notification.  See bug 321054 comment 31
  // and bug 339610.  *If* that multipart/x-mixed-replace image has
  // event handlers, we won't even get to this warning; we'll leak
  // instead.
  NS_WARN_IF_FALSE(mRootRefCount == 0,
                   "unbalanced handler preservation refcount");
  if (mRootRefCount != 0) {
    mRootRefCount = 1;
    UnpreserveLoadHandlers();
  }
}

nsImageLoadingContent::~nsImageLoadingContent()
{
  NS_ASSERTION(!mCurrentRequest && !mPendingRequest,
               "DestroyImageLoadingContent not called");
  NS_ASSERTION(!mObserverList.mObserver && !mObserverList.mNext,
               "Observers still registered?");
}

// Macro to call some func on each observer.  This handles observers
// removing themselves.
#define LOOP_OVER_OBSERVERS(func_)                                       \
  PR_BEGIN_MACRO                                                         \
    for (ImageObserver* observer = &mObserverList, *next; observer;      \
         observer = next) {                                              \
      next = observer->mNext;                                            \
      if (observer->mObserver) {                                         \
        observer->mObserver->func_;                                      \
      }                                                                  \
    }                                                                    \
  PR_END_MACRO


/*
 * imgIContainerObserver impl
 */
NS_IMETHODIMP
nsImageLoadingContent::FrameChanged(imgIContainer* aContainer,
                                    gfxIImageFrame* aFrame,
                                    nsRect* aDirtyRect)
{
  LOOP_OVER_OBSERVERS(FrameChanged(aContainer, aFrame, aDirtyRect));
  return NS_OK;
}
            
/*
 * imgIDecoderObserver impl
 */
NS_IMETHODIMP
nsImageLoadingContent::OnStartRequest(imgIRequest* aRequest)
{
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::OnStartDecode(imgIRequest* aRequest)
{
  LOOP_OVER_OBSERVERS(OnStartDecode(aRequest));
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::OnStartContainer(imgIRequest* aRequest,
                                        imgIContainer* aContainer)
{
  LOOP_OVER_OBSERVERS(OnStartContainer(aRequest, aContainer));
  return NS_OK;    
}

NS_IMETHODIMP
nsImageLoadingContent::OnStartFrame(imgIRequest* aRequest,
                                    gfxIImageFrame* aFrame)
{
  LOOP_OVER_OBSERVERS(OnStartFrame(aRequest, aFrame));
  return NS_OK;    
}

NS_IMETHODIMP
nsImageLoadingContent::OnDataAvailable(imgIRequest* aRequest,
                                       gfxIImageFrame* aFrame,
                                       const nsRect* aRect)
{
  LOOP_OVER_OBSERVERS(OnDataAvailable(aRequest, aFrame, aRect));
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::OnStopFrame(imgIRequest* aRequest,
                                   gfxIImageFrame* aFrame)
{
  LOOP_OVER_OBSERVERS(OnStopFrame(aRequest, aFrame));
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::OnStopContainer(imgIRequest* aRequest,
                                       imgIContainer* aContainer)
{
  LOOP_OVER_OBSERVERS(OnStopContainer(aRequest, aContainer));
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::OnStopDecode(imgIRequest* aRequest,
                                    nsresult aStatus,
                                    const PRUnichar* aStatusArg)
{
  NS_PRECONDITION(aRequest == mCurrentRequest || aRequest == mPendingRequest,
                  "Unknown request");
  LOOP_OVER_OBSERVERS(OnStopDecode(aRequest, aStatus, aStatusArg));

  if (aRequest == mPendingRequest) {
    mCurrentRequest->Cancel(NS_ERROR_IMAGE_SRC_CHANGED);
    mPendingRequest.swap(mCurrentRequest);
    mPendingRequest = nsnull;
  }

  // XXXldb What's the difference between when OnStopDecode and OnStopRequest
  // fire?  Should we do this work there instead?  Should they just be the
  // same?

  if (NS_SUCCEEDED(aStatus)) {
    FireEvent(NS_LITERAL_STRING("load"));
  } else {
    FireEvent(NS_LITERAL_STRING("error"));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::OnStopRequest(imgIRequest* aRequest, PRBool aLastPart)
{
  if (aLastPart)
    UnpreserveLoadHandlers();

  return NS_OK;
}

/*
 * nsIImageLoadingContent impl
 */

NS_IMETHODIMP
nsImageLoadingContent::GetLoadingEnabled(PRBool *aLoadingEnabled)
{
  *aLoadingEnabled = mLoadingEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::SetLoadingEnabled(PRBool aLoadingEnabled)
{
  if (nsContentUtils::GetImgLoader())
    mLoadingEnabled = aLoadingEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::GetImageBlockingStatus(PRInt16* aStatus)
{
  NS_PRECONDITION(aStatus, "Null out param");
  *aStatus = mImageBlockingStatus;
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::AddObserver(imgIDecoderObserver* aObserver)
{
  NS_ENSURE_ARG_POINTER(aObserver);

  if (!mObserverList.mObserver) {
    mObserverList.mObserver = aObserver;
    // Don't touch the linking of the list!
    return NS_OK;
  }

  // otherwise we have to create a new entry

  ImageObserver* observer = &mObserverList;
  while (observer->mNext) {
    observer = observer->mNext;
  }

  observer->mNext = new ImageObserver(aObserver);
  if (! observer->mNext) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::RemoveObserver(imgIDecoderObserver* aObserver)
{
  NS_ENSURE_ARG_POINTER(aObserver);

  if (mObserverList.mObserver == aObserver) {
    mObserverList.mObserver = nsnull;
    // Don't touch the linking of the list!
    return NS_OK;
  }

  // otherwise have to find it and splice it out
  ImageObserver* observer = &mObserverList;
  while (observer->mNext && observer->mNext->mObserver != aObserver) {
    observer = observer->mNext;
  }

  // At this point, we are pointing to the list element whose mNext is
  // the right observer (assuming of course that mNext is not null)
  if (observer->mNext) {
    // splice it out
    ImageObserver* oldObserver = observer->mNext;
    observer->mNext = oldObserver->mNext;
    oldObserver->mNext = nsnull;  // so we don't destroy them all
    delete oldObserver;
  }
#ifdef DEBUG
  else {
    NS_WARNING("Asked to remove non-existent observer");
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::GetRequest(PRInt32 aRequestType,
                                  imgIRequest** aRequest)
{
  switch(aRequestType) {
  case CURRENT_REQUEST:
    *aRequest = mCurrentRequest;
    break;
  case PENDING_REQUEST:
    *aRequest = mPendingRequest;
    break;
  default:
    NS_ERROR("Unknown request type");
    *aRequest = nsnull;
    return NS_ERROR_UNEXPECTED;
  }
  
  NS_IF_ADDREF(*aRequest);
  return NS_OK;
}


NS_IMETHODIMP
nsImageLoadingContent::GetRequestType(imgIRequest* aRequest,
                                      PRInt32* aRequestType)
{
  NS_PRECONDITION(aRequestType, "Null out param");
  
  if (aRequest == mCurrentRequest) {
    *aRequestType = CURRENT_REQUEST;
    return NS_OK;
  }

  if (aRequest == mPendingRequest) {
    *aRequestType = PENDING_REQUEST;
    return NS_OK;
  }

  *aRequestType = UNKNOWN_REQUEST;
  NS_ERROR("Unknown request");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsImageLoadingContent::GetCurrentURI(nsIURI** aURI)
{
  if (mCurrentRequest)
    return mCurrentRequest->GetURI(aURI);

  NS_IF_ADDREF(*aURI = mCurrentURI);
  return NS_OK;
}

NS_IMETHODIMP
nsImageLoadingContent::LoadImageWithChannel(nsIChannel* aChannel,
                                            nsIStreamListener** aListener)
{
  NS_PRECONDITION(aListener, "null out param");
  
  NS_ENSURE_ARG_POINTER(aChannel);

  if (!nsContentUtils::GetImgLoader())
    return NS_ERROR_NULL_POINTER;

  // XXX what should we do with content policies here, if anything?
  // Shouldn't that be done before the start of the load?
  
  nsCOMPtr<nsIDocument> doc = GetOurDocument();
  if (!doc) {
    // Don't bother
    return NS_OK;
  }

  PreserveLoadHandlers();

  // Null out our mCurrentURI, in case we have no image requests right now.
  mCurrentURI = nsnull;
  
  CancelImageRequests(NS_ERROR_IMAGE_SRC_CHANGED, PR_FALSE,
                      nsIContentPolicy::ACCEPT);

  nsCOMPtr<imgIRequest> & req = mCurrentRequest ? mPendingRequest : mCurrentRequest;

  nsresult rv = nsContentUtils::GetImgLoader()->LoadImageWithChannel(aChannel, this, doc, aListener, getter_AddRefs(req));

  if (NS_FAILED(rv))
    UnpreserveLoadHandlers();

  return rv;
}

// XXX This should be a protected method, not an interface method!!!
NS_IMETHODIMP
nsImageLoadingContent::ImageURIChanged(const nsAString& aNewURI) {
  return ImageURIChanged(aNewURI, PR_TRUE);
}

/*
 * Non-interface methods
 */
nsresult
nsImageLoadingContent::ImageURIChanged(const nsAString& aNewURI,
                                       PRBool aForce)
{
  if (!mLoadingEnabled) {
    return NS_OK;
  }

  // First, get a document (needed for security checks and the like)
  nsCOMPtr<nsIDocument> doc = GetOurDocument();
  if (!doc) {
    // No reason to bother, I think...
    return NS_OK;
  }

  nsresult rv;   // XXXbz Should failures in this method fire onerror?

  nsCOMPtr<nsIURI> imageURI;
  rv = StringToURI(aNewURI, doc, getter_AddRefs(imageURI));
  NS_ENSURE_SUCCESS(rv, rv);

  // Skip the URI equality check if our current image was blocked.  If
  // that happened, we really do want to try loading again.
  if (!aForce && NS_CP_ACCEPTED(mImageBlockingStatus)) {
    nsCOMPtr<nsIURI> currentURI;
    GetCurrentURI(getter_AddRefs(currentURI));
    PRBool equal;
    if (currentURI &&
        NS_SUCCEEDED(currentURI->Equals(imageURI, &equal)) &&
        equal) {
      // Nothing to do here.
      return NS_OK;
    }
  }

  // Remember the URL of this request, in case someone asks us for it later
  // But this only matters if we are affecting the current request
  if (!mCurrentRequest)
    mCurrentURI = imageURI;
  
  // If we'll be loading a new image, we want to cancel our existing
  // requests; the question is what reason to pass in.  If everything
  // is going smoothly, that reason should be
  // NS_ERROR_IMAGE_SRC_CHANGED so that our frame (if any) will know
  // not to show the broken image icon.  If the load is blocked by the
  // content policy or security manager, we will want to cancel with
  // the error code from those.

  PRInt16 newImageStatus;
  PRBool loadImage = nsContentUtils::CanLoadImage(imageURI,
                                                  NS_STATIC_CAST(nsIImageLoadingContent*, this),
                                                  doc,
                                                  &newImageStatus);
  NS_ASSERTION(loadImage || !NS_CP_ACCEPTED(newImageStatus),
               "CanLoadImage lied");

  nsresult cancelResult = loadImage ? NS_ERROR_IMAGE_SRC_CHANGED
                                    : NS_ERROR_IMAGE_BLOCKED;

  CancelImageRequests(cancelResult, PR_FALSE, newImageStatus);

  if (!loadImage) {
    // Don't actually load anything!  This was blocked by CanLoadImage.
    return NS_OK;
  }

  PreserveLoadHandlers();

  nsCOMPtr<imgIRequest> & req = mCurrentRequest ? mPendingRequest : mCurrentRequest;

  nsCOMPtr<nsIContent> thisContent = do_QueryInterface(NS_STATIC_CAST(nsIImageLoadingContent*, this), &rv);
  NS_ENSURE_TRUE(thisContent, rv);

  // It may be that one of our frames has replaced itself with alt text... This
  // would only have happened if our mCurrentRequest had issues, and we would
  // have set it to null by now in that case.  Have to save that information
  // here, since LoadImage may clobber the value of mCurrentRequest.  On the
  // other hand, if we've never had an observer, we know there aren't any frames
  // that have changed to alt text on us yet.
  PRBool mayNeedReframe = thisContent->MayHaveFrame() && !mCurrentRequest;
  
  rv = nsContentUtils::LoadImage(imageURI, doc, doc->GetDocumentURI(),
                                 this, nsIRequest::LOAD_NORMAL,
                                 getter_AddRefs(req));
  if (NS_FAILED(rv)) {
    UnpreserveLoadHandlers();
  }

  // If we now have a current request, we don't need to store the URI, since
  // we can get it off the request. Release it.
  if (mCurrentRequest) {
    mCurrentURI = nsnull;
  }

  if (!mayNeedReframe) {
    // We're all set
    return NS_OK;
  }

  // Only continue if we're in a document -- that would mean we're a useful
  // chunk of the content model and _may_ have a frame.  This should eliminate
  // things like SetAttr calls during the parsing process, as well as things
  // like setting src on |new Image()|-type things.
  if (!thisContent->IsInDoc()) {
    return NS_OK;
  }

  // OK, now for each PresShell, see whether we have a frame -- this tends to
  // be expensive, which is why it's the last check....  If we have a frame
  // and it's not of the right type, reframe it.
  PRInt32 numShells = doc->GetNumberOfShells();
  for (PRInt32 i = 0; i < numShells; ++i) {
    nsIPresShell *shell = doc->GetShellAt(i);
    if (shell) {
      nsIFrame* frame = nsnull;
      shell->GetPrimaryFrameFor(thisContent, &frame);
      if (frame) {
        // XXXbz I don't like this one bit... we really need a better way of
        // doing the CantRenderReplacedElement stuff.. In particular, it needs
        // to be easily detectable.  For example, I suspect that this code will
        // fail for <object> in the current CantRenderReplacedElement
        // implementation...
        nsIAtom* frameType = frame->GetType();
        if (frameType != nsLayoutAtoms::imageFrame &&
            frameType != nsLayoutAtoms::imageControlFrame &&
            frameType != nsLayoutAtoms::objectFrame) {
          shell->RecreateFramesFor(thisContent);
        }
      }
    }
  }

  return NS_OK;
}

void
nsImageLoadingContent::CancelImageRequests()
{
  // Make sure to null out mCurrentURI here, so we no longer look like an image
  mCurrentURI = nsnull;
  CancelImageRequests(NS_BINDING_ABORTED, PR_TRUE, nsIContentPolicy::ACCEPT);
}

void
nsImageLoadingContent::CancelImageRequests(nsresult aReason,
                                           PRBool   aEvenIfSizeAvailable,
                                           PRInt16  aNewImageStatus)
{
  // Cancel the pending request, if any
  if (mPendingRequest) {
    mPendingRequest->Cancel(aReason);
    mPendingRequest = nsnull;
  }

  // Cancel the current request if it has not progressed enough to
  // have a size yet
  if (mCurrentRequest) {
    PRUint32 loadStatus = imgIRequest::STATUS_ERROR;
    mCurrentRequest->GetImageStatus(&loadStatus);

    NS_ASSERTION(NS_CP_ACCEPTED(mImageBlockingStatus),
                 "Have current request but blocked image?");
    
    if (aEvenIfSizeAvailable ||
        !(loadStatus & imgIRequest::STATUS_SIZE_AVAILABLE)) {
      // The new image is going to become the current request.  Make sure to
      // set mImageBlockingStatus _before_ we cancel the request... if we set
      // it after, things that are watching the mCurrentRequest will get wrong
      // data.
      mImageBlockingStatus = aNewImageStatus;
      mCurrentRequest->Cancel(aReason);
      mCurrentRequest = nsnull;
    }
  } else {
    // No current request so the new image status will become the
    // status of the current request
    mImageBlockingStatus = aNewImageStatus;
  }

  // Note that the only way we could have avoided setting the image blocking
  // status above is if we have a current request and have kept it as the
  // current request.  In that case, we want to leave our old status, since the
  // status corresponds to the current request.  Even if we plan to do a
  // pending request load, having an mCurrentRequest means that our current
  // status is not a REJECT_* status, and doing the load shouldn't change that.
  // XXXbz there is an issue here if different ACCEPT statuses are used, but...
}

nsIDocument*
nsImageLoadingContent::GetOurDocument()
{
  nsCOMPtr<nsIContent> thisContent = do_QueryInterface(NS_STATIC_CAST(nsIImageLoadingContent*, this));
  NS_ENSURE_TRUE(thisContent, nsnull);

  return thisContent->GetOwnerDoc();
}

nsresult
nsImageLoadingContent::StringToURI(const nsAString& aSpec,
                                   nsIDocument* aDocument,
                                   nsIURI** aURI)
{
  NS_PRECONDITION(aDocument, "Must have a document");
  NS_PRECONDITION(aURI, "Null out param");

  // (1) Get the base URI
  nsCOMPtr<nsIContent> thisContent = do_QueryInterface(NS_STATIC_CAST(nsIImageLoadingContent*, this));
  NS_ASSERTION(thisContent, "An image loading content must be an nsIContent");
  nsCOMPtr<nsIURI> baseURL = thisContent->GetBaseURI();

  // (2) Get the charset
  const nsACString &charset = aDocument->GetDocumentCharacterSet();

  // (3) Construct the silly thing
  return NS_NewURI(aURI,
                   aSpec,
                   charset.IsEmpty() ? nsnull : PromiseFlatCString(charset).get(),
                   baseURL,
                   nsContentUtils::GetIOServiceWeakRef());
}


/**
 * Struct used to dispatch events
 */
MOZ_DECL_CTOR_COUNTER(ImageEvent)

class nsImageLoadingContent::Event : public PLEvent
{
public:
  Event(nsPresContext* aPresContext, nsImageLoadingContent* aContent,
             const nsAString& aMessage, nsIDocument* aDocument)
    : mPresContext(aPresContext),
      mContent(aContent),
      mMessage(aMessage),
      mDocument(aDocument)
  {
    MOZ_COUNT_CTOR(nsImageLoadingContent::Event);
    PL_InitEvent(this, aContent, Handle, Destroy);
  }
  ~Event()
  {
    MOZ_COUNT_DTOR(nsImageLoadingContent::Event);
    mDocument->UnblockOnload();
    mContent->UnpreserveLoadHandlers();
  }
  
  PR_STATIC_CALLBACK(void*) Handle(PLEvent* aEvent);
  PR_STATIC_CALLBACK(void) Destroy(PLEvent* aEvent);

  nsCOMPtr<nsPresContext> mPresContext;
  nsRefPtr<nsImageLoadingContent> mContent;
  nsString mMessage;
  // Need to hold on to the document in case our event outlives document
  // teardown... Wantto be able to get back to the document even if the
  // prescontext and content can't.
  nsCOMPtr<nsIDocument> mDocument;
};

/* static */ void * PR_CALLBACK
nsImageLoadingContent::Event::Handle(PLEvent* aEvent)
{
  nsImageLoadingContent::Event* evt =
    NS_STATIC_CAST(nsImageLoadingContent::Event*, aEvent);
  nsEventStatus estatus = nsEventStatus_eIgnore;
  PRUint32 eventMsg;

  if (evt->mMessage.EqualsLiteral("load")) {
    eventMsg = NS_IMAGE_LOAD;
  } else {
    eventMsg = NS_IMAGE_ERROR;
  }

  nsCOMPtr<nsIContent> ourContent = do_QueryInterface(NS_STATIC_CAST(nsIImageLoadingContent*, evt->mContent));

  nsEvent event(PR_TRUE, eventMsg);
  ourContent->HandleDOMEvent(evt->mPresContext, &event, nsnull,
                             NS_EVENT_FLAG_INIT, &estatus);

  return nsnull;
}

/* static */ void PR_CALLBACK
nsImageLoadingContent::Event::Destroy(PLEvent* aEvent)
{
  nsImageLoadingContent::Event* evt =
    NS_STATIC_CAST(nsImageLoadingContent::Event*, aEvent);
  delete evt;
}

nsresult
nsImageLoadingContent::FireEvent(const nsAString& aEventType)
{
  // We have to fire the event asynchronously so that we won't go into infinite
  // loops in cases when onLoad handlers reset the src and the new src is in
  // cache.

  nsCOMPtr<nsIDocument> document = GetOurDocument();
  if (!document) {
    // no use to fire events if there is no document....
    return NS_OK;
  }                                                                             
  nsresult rv;
  nsCOMPtr<nsIEventQueueService> eventQService =
    do_GetService("@mozilla.org/event-queue-service;1", &rv);
  NS_ENSURE_TRUE(eventQService, rv);

  nsCOMPtr<nsIEventQueue> eventQ;
  // Use the UI thread event queue (though we should not be getting called from
  // off the UI thread in any case....)
  rv = eventQService->GetSpecialEventQueue(nsIEventQueueService::UI_THREAD_EVENT_QUEUE,
                                           getter_AddRefs(eventQ));
  NS_ENSURE_TRUE(eventQ, rv);

  nsIPresShell *shell = document->GetShellAt(0);
  NS_ENSURE_TRUE(shell, NS_ERROR_FAILURE);

  nsPresContext *presContext = shell->GetPresContext();
  NS_ENSURE_TRUE(presContext, NS_ERROR_FAILURE);

  nsImageLoadingContent::Event* evt =
    new Event(presContext, this, aEventType, document);
  NS_ENSURE_TRUE(evt, NS_ERROR_OUT_OF_MEMORY);

  // Block onload for our event.  Since we unblock in the event destructor, we
  // want to block now, even if posting will fail.
  document->BlockOnload();
  PreserveLoadHandlers();
  
  rv = eventQ->PostEvent(evt);

  if (NS_FAILED(rv)) {
    PL_DestroyEvent(evt);
  }

  return rv;
}

void
nsImageLoadingContent::PreserveLoadHandlers()
{
  ++mRootRefCount;
  NS_LOG_ADDREF(&mRootRefCount, mRootRefCount,
                "nsImageLoadingContent::mRootRefCount", sizeof(mRootRefCount));
  if (mRootRefCount == 1) {
    nsCOMPtr<nsIDOMGCParticipant> part = do_QueryInterface(NS_STATIC_CAST(nsIImageLoadingContent*, this));
    nsresult rv = nsDOMClassInfo::SetExternallyReferenced(part);
    // The worst that will happen if we ignore this failure is that
    // onload or onerror will fail to fire.  I suppose we could fire
    // onerror now as a result of that, but the only reason it would
    // actually fail is out-of-memory, and it seems silly to bother and
    // unlikely to work in that case.
    NS_ASSERTION(NS_SUCCEEDED(rv), "ignoring failure to root participant");
  }
}

void
nsImageLoadingContent::UnpreserveLoadHandlers()
{
  NS_ASSERTION(mRootRefCount != 0,
               "load handler preservation refcount underflow");
  --mRootRefCount;
  NS_LOG_RELEASE(&mRootRefCount, mRootRefCount,
                 "nsImageLoadingContent::mRootRefCount");
  if (mRootRefCount == 0) {
    nsCOMPtr<nsIDOMGCParticipant> part = do_QueryInterface(NS_STATIC_CAST(nsIImageLoadingContent*, this));
    nsDOMClassInfo::UnsetExternallyReferenced(part);
  }
}
