/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Johnny Stenback <jst@netscape.com>
 *   Christopher A. Aillon <christopher@aillon.com>
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

/* A namespace class for static layout utilities. */

#include "nsJSUtils.h"
#include "nsCOMPtr.h"
#include "nsAString.h"
#include "nsPrintfCString.h"
#include "nsUnicharUtils.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefLocalizedString.h"
#include "nsServiceManagerUtils.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsDOMCID.h"
#include "nsContentUtils.h"
#include "nsIXPConnect.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsINodeInfo.h"
#include "nsReadableUtils.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMNode.h"
#include "nsIDOM3Node.h"
#include "nsIIOService.h"
#include "nsIURI.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsIScriptSecurityManager.h"
#include "nsDOMError.h"
#include "nsPIDOMWindow.h"
#include "nsIJSContextStack.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsParserCIID.h"
#include "nsIParserService.h"
#include "nsIServiceManager.h"
#include "nsIAttribute.h"
#include "nsContentList.h"
#include "nsIHTMLDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIForm.h"
#include "nsIFormControl.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLParts.h"
#include "nsISupportsPrimitives.h"
#include "nsLayoutAtoms.h"
#include "imgIDecoderObserver.h"
#include "imgIRequest.h"
#include "imgIContainer.h"
#include "imgILoader.h"
#include "nsIImage.h"
#include "gfxIImageFrame.h"
#include "nsIImageLoadingContent.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsILink.h"
#include "nsILoadGroup.h"
#include "nsContentPolicyUtils.h"
#include "nsDOMString.h"
#include "nsGenericElement.h"
#include "nsNodeInfoManager.h"
#include "nsCRT.h"
#ifdef MOZ_XTF
#include "nsIXTFService.h"
static NS_DEFINE_CID(kXTFServiceCID, NS_XTFSERVICE_CID);
#endif
#include "nsIMIMEService.h"
#include "jsdbgapi.h"
#include "nsIJSRuntimeService.h"
#include "nsIFragmentContentSink.h"

// for ReportToConsole
#include "nsIStringBundle.h"
#include "nsIScriptError.h"
#include "nsIConsoleService.h"

static const char kJSStackContractID[] = "@mozilla.org/js/xpc/ContextStack;1";
static NS_DEFINE_IID(kParserServiceCID, NS_PARSERSERVICE_CID);

nsIDOMScriptObjectFactory *nsContentUtils::sDOMScriptObjectFactory = nsnull;
nsIXPConnect *nsContentUtils::sXPConnect = nsnull;
nsIScriptSecurityManager *nsContentUtils::sSecurityManager = nsnull;
nsIThreadJSContextStack *nsContentUtils::sThreadJSContextStack = nsnull;
nsIParserService *nsContentUtils::sParserService = nsnull;
nsINameSpaceManager *nsContentUtils::sNameSpaceManager = nsnull;
nsIIOService *nsContentUtils::sIOService = nsnull;
#ifdef MOZ_XTF
nsIXTFService *nsContentUtils::sXTFService = nsnull;
#endif
nsIPrefBranch *nsContentUtils::sPrefBranch = nsnull;
nsIPref *nsContentUtils::sPref = nsnull;
imgILoader *nsContentUtils::sImgLoader = nsnull;
nsIConsoleService *nsContentUtils::sConsoleService;
nsIStringBundleService *nsContentUtils::sStringBundleService;
nsIStringBundle *nsContentUtils::sStringBundles[PropertiesFile_COUNT];
nsVoidArray *nsContentUtils::sPtrsToPtrsToRelease;
nsIJSRuntimeService *nsContentUtils::sJSRuntimeService;
JSRuntime *nsContentUtils::sScriptRuntime;
PRInt32 nsContentUtils::sScriptRootCount = 0;


PRBool nsContentUtils::sInitialized = PR_FALSE;

// static
nsresult
nsContentUtils::Init()
{
  if (sInitialized) {
    NS_WARNING("Init() called twice");

    return NS_OK;
  }

  nsresult rv = CallGetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID,
                               &sSecurityManager);
  NS_ENSURE_SUCCESS(rv, rv);

  // It's ok to not have a pref service.
  CallGetService(NS_PREFSERVICE_CONTRACTID, &sPrefBranch);

  // It's ok to not have prefs too.
  CallGetService(NS_PREF_CONTRACTID, &sPref);

  rv = NS_GetNameSpaceManager(&sNameSpaceManager);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = nsGenericElement::InitHashes();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = CallGetService(nsIXPConnect::GetCID(), &sXPConnect);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = CallGetService(kJSStackContractID, &sThreadJSContextStack);
  if (NS_FAILED(rv) && sXPConnect) {
    // However, if we can't get a context stack after getting
    // an nsIXPConnect, things are broken, so let's fail here.

    return rv;
  }

  rv = CallGetService(NS_IOSERVICE_CONTRACTID, &sIOService);
  if (NS_FAILED(rv)) {
    // This makes life easier, but we can live without it.

    sIOService = nsnull;
  }

  // Ignore failure and just don't load images
  rv = CallGetService("@mozilla.org/image/loader;1", &sImgLoader);
  if (NS_FAILED(rv)) {
    // no image loading for us.  Oh, well.
    sImgLoader = nsnull;
  }

  sPtrsToPtrsToRelease = new nsVoidArray();
  if (!sPtrsToPtrsToRelease) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  sInitialized = PR_TRUE;

  return NS_OK;
}

/**
 * Access a cached parser service. Don't addref. We need only one
 * reference to it and this class has that one.
 */
/* static */
nsIParserService*
nsContentUtils::GetParserServiceWeakRef()
{
  // XXX: This isn't accessed from several threads, is it?
  if (!sParserService) {
    // Lock, recheck sCachedParserService and aquire if this should be
    // safe for multiple threads.
    nsresult rv = CallGetService(kParserServiceCID, &sParserService);
    if (NS_FAILED(rv)) {
      sParserService = nsnull;
    }
  }

  return sParserService;
}

#ifdef MOZ_XTF
nsIXTFService*
nsContentUtils::GetXTFServiceWeakRef()
{
  if (!sXTFService) {
    nsresult rv = CallGetService(kXTFServiceCID, &sXTFService);
    if (NS_FAILED(rv)) {
      sXTFService = nsnull;
    }
  }

  return sXTFService;
}
#endif

template <class OutputIterator>
struct NormalizeNewlinesCharTraits {
  public:
    typedef typename OutputIterator::value_type value_type;

  public:
    NormalizeNewlinesCharTraits(OutputIterator& aIterator) : mIterator(aIterator) { }
    void writechar(typename OutputIterator::value_type aChar) {
      *mIterator++ = aChar;
    }

  private:
    OutputIterator mIterator;
};

#ifdef HAVE_CPP_PARTIAL_SPECIALIZATION

template <class CharT>
struct NormalizeNewlinesCharTraits<CharT*> {
  public:
    typedef CharT value_type;

  public:
    NormalizeNewlinesCharTraits(CharT* aCharPtr) : mCharPtr(aCharPtr) { }
    void writechar(CharT aChar) {
      *mCharPtr++ = aChar;
    }

  private:
    CharT* mCharPtr;
};

#else

NS_SPECIALIZE_TEMPLATE
struct NormalizeNewlinesCharTraits<char*> {
  public:
    typedef char value_type;

  public:
    NormalizeNewlinesCharTraits(char* aCharPtr) : mCharPtr(aCharPtr) { }
    void writechar(char aChar) {
      *mCharPtr++ = aChar;
    }

  private:
    char* mCharPtr;
};

NS_SPECIALIZE_TEMPLATE
struct NormalizeNewlinesCharTraits<PRUnichar*> {
  public:
    typedef PRUnichar value_type;

  public:
    NormalizeNewlinesCharTraits(PRUnichar* aCharPtr) : mCharPtr(aCharPtr) { }
    void writechar(PRUnichar aChar) {
      *mCharPtr++ = aChar;
    }

  private:
    PRUnichar* mCharPtr;
};

#endif

template <class OutputIterator>
class CopyNormalizeNewlines
{
  public:
    typedef typename OutputIterator::value_type value_type;

  public:
    CopyNormalizeNewlines(OutputIterator* aDestination,
                          PRBool aLastCharCR=PR_FALSE) :
      mLastCharCR(aLastCharCR),
      mDestination(aDestination),
      mWritten(0)
    { }

    PRUint32 GetCharsWritten() {
      return mWritten;
    }

    PRBool IsLastCharCR() {
      return mLastCharCR;
    }

    PRUint32 write(const typename OutputIterator::value_type* aSource, PRUint32 aSourceLength) {

      const typename OutputIterator::value_type* done_writing = aSource + aSourceLength;

      // If the last source buffer ended with a CR...
      if (mLastCharCR) {
        // ..and if the next one is a LF, then skip it since
        // we've already written out a newline
        if (aSourceLength && (*aSource == value_type('\n'))) {
          ++aSource;
        }
        mLastCharCR = PR_FALSE;
      }

      PRUint32 num_written = 0;
      while ( aSource < done_writing ) {
        if (*aSource == value_type('\r')) {
          mDestination->writechar('\n');
          ++aSource;
          // If we've reached the end of the buffer, record
          // that we wrote out a CR
          if (aSource == done_writing) {
            mLastCharCR = PR_TRUE;
          }
          // If the next character is a LF, skip it
          else if (*aSource == value_type('\n')) {
            ++aSource;
          }
        }
        else {
          mDestination->writechar(*aSource++);
        }
        ++num_written;
      }

      mWritten += num_written;
      return aSourceLength;
    }

  private:
    PRBool mLastCharCR;
    OutputIterator* mDestination;
    PRUint32 mWritten;
};

// static
PRUint32
nsContentUtils::CopyNewlineNormalizedUnicodeTo(const nsAString& aSource,
                                               PRUint32 aSrcOffset,
                                               PRUnichar* aDest,
                                               PRUint32 aLength,
                                               PRBool& aLastCharCR)
{
  typedef NormalizeNewlinesCharTraits<PRUnichar*> sink_traits;

  sink_traits dest_traits(aDest);
  CopyNormalizeNewlines<sink_traits> normalizer(&dest_traits,aLastCharCR);
  nsReadingIterator<PRUnichar> fromBegin, fromEnd;
  copy_string(aSource.BeginReading(fromBegin).advance( PRInt32(aSrcOffset) ),
              aSource.BeginReading(fromEnd).advance( PRInt32(aSrcOffset+aLength) ),
              normalizer);
  aLastCharCR = normalizer.IsLastCharCR();
  return normalizer.GetCharsWritten();
}

// static
PRUint32
nsContentUtils::CopyNewlineNormalizedUnicodeTo(nsReadingIterator<PRUnichar>& aSrcStart, const nsReadingIterator<PRUnichar>& aSrcEnd, nsAString& aDest)
{
  typedef nsWritingIterator<PRUnichar> WritingIterator;
  typedef NormalizeNewlinesCharTraits<WritingIterator> sink_traits;

  WritingIterator iter;
  aDest.BeginWriting(iter);
  sink_traits dest_traits(iter);
  CopyNormalizeNewlines<sink_traits> normalizer(&dest_traits);
  copy_string(aSrcStart, aSrcEnd, normalizer);
  return normalizer.GetCharsWritten();
}

// static
void
nsContentUtils::Shutdown()
{
  sInitialized = PR_FALSE;

  NS_HTMLParanoidFragmentSinkShutdown();
  NS_XHTMLParanoidFragmentSinkShutdown();

  PRInt32 i;
  for (i = 0; i < PRInt32(PropertiesFile_COUNT); ++i)
    NS_IF_RELEASE(sStringBundles[i]);
  NS_IF_RELEASE(sStringBundleService);
  NS_IF_RELEASE(sConsoleService);
  NS_IF_RELEASE(sDOMScriptObjectFactory);
  NS_IF_RELEASE(sXPConnect);
  NS_IF_RELEASE(sSecurityManager);
  NS_IF_RELEASE(sThreadJSContextStack);
  NS_IF_RELEASE(sNameSpaceManager);
  NS_IF_RELEASE(sParserService);
  NS_IF_RELEASE(sIOService);
#ifdef MOZ_XTF
  NS_IF_RELEASE(sXTFService);
#endif
  NS_IF_RELEASE(sImgLoader);
  NS_IF_RELEASE(sPrefBranch);
  NS_IF_RELEASE(sPref);
  if (sPtrsToPtrsToRelease) {
    for (i = 0; i < sPtrsToPtrsToRelease->Count(); ++i) {
      nsISupports** ptrToPtr =
        NS_STATIC_CAST(nsISupports**, sPtrsToPtrsToRelease->ElementAt(i));
      NS_RELEASE(*ptrToPtr);
    }
    delete sPtrsToPtrsToRelease;
    sPtrsToPtrsToRelease = nsnull;
  }
}

// static
nsISupports *
nsContentUtils::GetClassInfoInstance(nsDOMClassInfoID aID)
{
  if (!sDOMScriptObjectFactory) {
    static NS_DEFINE_CID(kDOMScriptObjectFactoryCID,
                         NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

    CallGetService(kDOMScriptObjectFactoryCID, &sDOMScriptObjectFactory);

    if (!sDOMScriptObjectFactory) {
      return nsnull;
    }
  }

  return sDOMScriptObjectFactory->GetClassInfoInstance(aID);
}

// static
nsresult
nsContentUtils::GetDocumentAndPrincipal(nsIDOMNode* aNode,
                                        nsIDocument** aDocument,
                                        nsIPrincipal** aPrincipal)
{
  // For performance reasons it's important to try to QI the node to
  // nsIContent before trying to QI to nsIDocument since a QI miss on
  // a node is potentially expensive.
  nsCOMPtr<nsIContent> content = do_QueryInterface(aNode);
  nsCOMPtr<nsIAttribute> attr;

  if (!content) {
    CallQueryInterface(aNode, aDocument);

    if (!*aDocument) {
      attr = do_QueryInterface(aNode);
      if (!attr) {
        // aNode is not a nsIContent, a nsIAttribute or a nsIDocument,
        // something weird is going on...

        NS_ERROR("aNode is not nsIContent, nsIAttribute or nsIDocument!");

        return NS_ERROR_UNEXPECTED;
      }
    }
  }

  if (!*aDocument) {
    nsCOMPtr<nsIDOMDocument> domDoc;
    aNode->GetOwnerDocument(getter_AddRefs(domDoc));
    if (!domDoc) {
      // if we can't get a doc then lets try to get principal through nodeinfo
      // manager
      nsINodeInfo *ni;
      if (content) {
        ni = content->GetNodeInfo();
      }
      else {
        ni = attr->NodeInfo();
      }

      if (!ni) {
        // we can't get to the principal so we'll give up

        return NS_OK;
      }

      *aPrincipal = ni->NodeInfoManager()->GetDocumentPrincipal();
      if (!*aPrincipal) {
        // we can't get to the principal so we'll give up

        return NS_OK;
      }

      NS_ADDREF(*aPrincipal);
    }
    else {
      CallQueryInterface(domDoc, aDocument);
      if (!*aDocument) {
        NS_ERROR("QI to nsIDocument failed");

        return NS_ERROR_UNEXPECTED;
      }
    }
  }

  if (!*aPrincipal) {
    NS_IF_ADDREF(*aPrincipal = (*aDocument)->GetPrincipal());
  }

  return NS_OK;
}

/**
 * Checks whether two nodes come from the same origin. aTrustedNode is
 * considered 'safe' in that a user can operate on it and that it isn't
 * a js-object that implements nsIDOMNode.
 * Never call this function with the first node provided by script, it
 * must always be known to be a 'real' node!
 */
// static
nsresult
nsContentUtils::CheckSameOrigin(nsIDOMNode *aTrustedNode,
                                nsIDOMNode *aUnTrustedNode)
{
  NS_PRECONDITION(aTrustedNode, "There must be a trusted node");

  PRBool isSystem = PR_FALSE;
  sSecurityManager->SubjectPrincipalIsSystem(&isSystem);
  if (isSystem) {
    // we're running as system, grant access to the node.

    return NS_OK;
  }

  /*
   * Get hold of each node's document or principal
   */

  // In most cases this is a document, so lets try that first
  nsCOMPtr<nsIDocument> trustedDoc = do_QueryInterface(aTrustedNode);
  nsIPrincipal* trustedPrincipal = nsnull;

  if (!trustedDoc) {
#ifdef DEBUG
    nsCOMPtr<nsIContent> trustCont = do_QueryInterface(aTrustedNode);
    NS_ASSERTION(trustCont,
                 "aTrustedNode is neither nsIContent nor nsIDocument!");
#endif
    nsCOMPtr<nsIDOMDocument> domDoc;
    aTrustedNode->GetOwnerDocument(getter_AddRefs(domDoc));

    if (!domDoc) {
      // In theory this should never happen. But since theory and reality are
      // different for XUL elements we'll try to get the principal from the
      // nsNodeInfoManager.

      nsCOMPtr<nsIContent> cont = do_QueryInterface(aTrustedNode);
      NS_ENSURE_TRUE(cont, NS_ERROR_UNEXPECTED);

      nsINodeInfo *ni = cont->GetNodeInfo();
      NS_ENSURE_TRUE(ni, NS_ERROR_UNEXPECTED);

      trustedPrincipal = ni->NodeInfoManager()->GetDocumentPrincipal();

      if (!trustedPrincipal) {
        // Can't get principal of aTrustedNode so we can't check security
        // against it

        return NS_ERROR_UNEXPECTED;
      }
    } else {
      trustedDoc = do_QueryInterface(domDoc);
      NS_ASSERTION(trustedDoc, "QI to nsIDocument failed");
    }
  }

  nsCOMPtr<nsIDocument> unTrustedDoc;
  nsCOMPtr<nsIPrincipal> unTrustedPrincipal;

  nsresult rv = GetDocumentAndPrincipal(aUnTrustedNode,
                                        getter_AddRefs(unTrustedDoc),
                                        getter_AddRefs(unTrustedPrincipal));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!unTrustedDoc && !unTrustedPrincipal) {
    // We can't get hold of the principal for this node. This should happen
    // very rarely, like for textnodes out of the tree and <option>s created
    // using 'new Option'.
    // If we didn't allow access to nodes like this you wouldn't be able to
    // insert these nodes into a document.

    return NS_OK;
  }

  /*
   * Compare the principals
   */

  // If they are in the same document then everything is just fine
  if (trustedDoc == unTrustedDoc && trustedDoc) {
    return NS_OK;
  }

  if (!trustedPrincipal) {
    trustedPrincipal = trustedDoc->GetPrincipal();

    if (!trustedPrincipal) {
      // If the trusted node doesn't have a principal we can't check
      // security against it

      return NS_ERROR_DOM_SECURITY_ERR;
    }
  }

  if (!unTrustedPrincipal) {
    unTrustedPrincipal = unTrustedDoc->GetPrincipal();

    if (!unTrustedPrincipal) {
      // If the trusted node doesn't have a principal we can't check
      // security against it

      return NS_ERROR_DOM_SECURITY_ERR;
    }
  }

  return sSecurityManager->CheckSameOriginPrincipal(trustedPrincipal,
                                                    unTrustedPrincipal);
}

// static
PRBool
nsContentUtils::CanCallerAccess(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIPrincipal> subjectPrincipal;
  sSecurityManager->GetSubjectPrincipal(getter_AddRefs(subjectPrincipal));

  if (!subjectPrincipal) {
    // we're running as system, grant access to the node.

    return PR_TRUE;
  }

  nsCOMPtr<nsIPrincipal> systemPrincipal;
  sSecurityManager->GetSystemPrincipal(getter_AddRefs(systemPrincipal));

  if (subjectPrincipal == systemPrincipal) {
    // we're running as system, grant access to the node.

    return PR_TRUE;
  }

  nsCOMPtr<nsIDocument> document;
  nsCOMPtr<nsIPrincipal> principal;

  nsresult rv = GetDocumentAndPrincipal(aNode,
                                        getter_AddRefs(document),
                                        getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  if (!document && !principal) {
    // We can't get hold of the principal for this node. This should happen
    // very rarely, like for textnodes out of the tree and <option>s created
    // using 'new Option'.
    // If we didn't allow access to nodes like this you wouldn't be able to
    // insert these nodes into a document.

    return PR_TRUE;
  }

  PRBool enabled = PR_FALSE;
  if (principal == systemPrincipal) {
    // we know subjectPrincipal != systemPrincipal so we can only
    // access the object if UniversalXPConnect is enabled. We can
    // avoid wasting time in CheckSameOriginPrincipal

    rv = sSecurityManager->IsCapabilityEnabled("UniversalXPConnect", &enabled);
    return NS_SUCCEEDED(rv) && enabled;
  }

  rv = sSecurityManager->CheckSameOriginPrincipal(subjectPrincipal, principal);
  if (NS_SUCCEEDED(rv)) {
    return PR_TRUE;
  }

  // see if the caller has otherwise been given the ability to touch
  // input args to DOM methods

  rv = sSecurityManager->IsCapabilityEnabled("UniversalBrowserRead", &enabled);
  return NS_SUCCEEDED(rv) && enabled;
}

//static
PRBool
nsContentUtils::InProlog(nsIDOMNode *aNode)
{
  NS_PRECONDITION(aNode, "missing node to nsContentUtils::InProlog");

  // Check that there is an ancestor and that it is a document
  nsCOMPtr<nsIDOMNode> parent;
  aNode->GetParentNode(getter_AddRefs(parent));
  if (!parent) {
    return PR_FALSE;
  }

  PRUint16 type;
  parent->GetNodeType(&type);
  if (type != nsIDOMNode::DOCUMENT_NODE) {
    return PR_FALSE;
  }

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(parent);
  NS_ASSERTION(doc, "document doesn't implement nsIDocument");
  nsCOMPtr<nsIContent> cont = do_QueryInterface(aNode);
  NS_ASSERTION(cont, "node doesn't implement nsIContent");

  // Check that there are no elements before aNode to make sure we are not
  // in the epilog
  PRInt32 pos = doc->IndexOf(cont);

  while (pos > 0) {
    --pos;
    nsIContent *sibl = doc->GetChildAt(pos);
    if (sibl->IsContentOfType(nsIContent::eELEMENT)) {
      return PR_FALSE;
    }
  }

  return PR_TRUE;
}

// static
nsresult
nsContentUtils::doReparentContentWrapper(nsIContent *aNode,
                                         JSContext *cx,
                                         JSObject *aOldGlobal,
                                         JSObject *aNewGlobal)
{
  nsCOMPtr<nsIXPConnectJSObjectHolder> old_wrapper;

  nsresult rv;

  rv = sXPConnect->ReparentWrappedNativeIfFound(cx, aOldGlobal, aNewGlobal,
                                                aNode,
                                                getter_AddRefs(old_wrapper));
  NS_ENSURE_SUCCESS(rv, rv);

  // Whether or not aChild is already wrapped we must iterate through
  // its descendants since there's no guarantee that a descendant isn't
  // wrapped even if this child is not wrapped. That used to be true
  // when every DOM node's JSObject was parented at the DOM node's
  // parent's JSObject, but that's no longer the case.

  PRUint32 i, count = aNode->GetChildCount();

  for (i = 0; i < count; i++) {
    nsIContent *child = aNode->GetChildAt(i);
    NS_ENSURE_TRUE(child, NS_ERROR_UNEXPECTED);

    rv = doReparentContentWrapper(child, cx, aOldGlobal, aNewGlobal);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return rv;
}

static JSContext *
GetContextFromDocument(nsIDocument *aDocument, JSObject** aGlobalObject)
{
  nsCOMPtr<nsIDocument_MOZILLA_1_8_0_BRANCH> doc18 =
    do_QueryInterface(aDocument);
  if (!doc18) {
    NS_ERROR("What kind of crazy document was passed in?");
    return nsnull;
  }
  nsIScriptGlobalObject *sgo = doc18->GetScopeObject();
  if (!sgo) {
    // No script global, no context.

    *aGlobalObject = nsnull;

    return nsnull;
  }

  *aGlobalObject = sgo->GetGlobalJSObject();

  nsIScriptContext *scx = sgo->GetContext();
  if (!scx) {
    // No context left in the old scope...

    return nsnull;
  }

  return (JSContext *)scx->GetNativeContext();
}

// static
nsresult
nsContentUtils::ReparentContentWrapper(nsIContent *aContent,
                                       nsIContent *aNewParent,
                                       nsIDocument *aNewDocument,
                                       nsIDocument *aOldDocument)
{
  if (!aNewDocument || aNewDocument == aOldDocument) {
    return NS_OK;
  }


  nsCOMPtr<nsIDocument_MOZILLA_1_8_0_BRANCH> doc18 =
    do_QueryInterface(aNewDocument);
  if (!doc18) {
    NS_ERROR("Crazy document passed in");
    return NS_ERROR_UNEXPECTED;
  }
  nsIScriptGlobalObject *newSGO = doc18->GetScopeObject();
  JSObject *newScope;

  // If we can't find our old document we don't know what our old
  // scope was so there's no way to find the old wrapper, and if there
  // is no new scope there's no reason to reparent.
  if (!aOldDocument || !newSGO || !(newScope = newSGO->GetGlobalJSObject())) {
    return NS_OK;
  }

  NS_ENSURE_TRUE(sXPConnect, NS_ERROR_NOT_INITIALIZED);

  // Make sure to get our hands on the right scope object, since
  // GetWrappedNativeOfNativeObject doesn't call PreCreate and hence won't get
  // the right scope if we pass in something bogus.  The right scope lives on
  // the script global of the old document.
  // XXXbz note that if GetWrappedNativeOfNativeObject did call PreCreate it
  // would get the wrong scope (that of the _new_ document), so we should be
  // glad it doesn't!
  JSObject *globalObj;
  JSContext *cx = GetContextFromDocument(aOldDocument, &globalObj);

  if (!globalObj) {
    // No global object around; can't find the old wrapper w/o the old
    // global object

    return NS_OK;
  }

  if (!cx) {
    JSObject *dummy;
    cx = GetContextFromDocument(aNewDocument, &dummy);

    if (!cx) {
      // No context reachable from the old or new document, use the
      // calling context, or the safe context if no caller can be
      // found.

      sThreadJSContextStack->Peek(&cx);

      if (!cx) {
        sThreadJSContextStack->GetSafeJSContext(&cx);
      }
    }
  }

  return doReparentContentWrapper(aContent, cx, globalObj, newScope);
}

nsresult
nsContentUtils::ReparentContentWrappersInScope(nsIScriptGlobalObject *aOldScope,
                                               nsIScriptGlobalObject *aNewScope)
{
  JSContext *cx = nsnull;

  // Try really hard to find a context to work on.
  nsIScriptContext *context = aOldScope->GetContext();
  if (context) {
    cx = NS_STATIC_CAST(JSContext *, context->GetNativeContext());
  }

  if (!cx) {
    context = aNewScope->GetContext();
    if (context) {
      cx = NS_STATIC_CAST(JSContext *, context->GetNativeContext());
    }

    if (!cx) {
      sThreadJSContextStack->Peek(&cx);

      if (!cx) {
        sThreadJSContextStack->GetSafeJSContext(&cx);

        if (!cx) {
          // Wow, this is really bad!
          NS_WARNING("No context reachable in ReparentContentWrappers()!");

          return NS_ERROR_NOT_AVAILABLE;
        }
      }
    }
  }

  // Now that we have a context, let's get the global objects from the two
  // scopes and ask XPConnect to do the rest of the work.

  JSObject *oldScopeObj = aOldScope->GetGlobalJSObject();
  JSObject *newScopeObj = aNewScope->GetGlobalJSObject();

  if (!newScopeObj || !oldScopeObj) {
    // We can't really do anything without the JSObjects.

    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIXPConnect_MOZILLA_1_8_BRANCH> xpconnect18 =
    do_QueryInterface(sXPConnect);
  if (!xpconnect18) {
    NS_ERROR("Weird things are happening in XPConnect");
    return NS_ERROR_FAILURE;
  }
  return xpconnect18->ReparentScopeAwareWrappers(cx, oldScopeObj, newScopeObj);
}

nsIDocShell *
nsContentUtils::GetDocShellFromCaller()
{
  if (!sThreadJSContextStack) {
    return nsnull;
  }

  JSContext *cx = nsnull;
  sThreadJSContextStack->Peek(&cx);

  if (cx) {
    nsIScriptGlobalObject *sgo = nsJSUtils::GetDynamicScriptGlobal(cx);

    if (sgo) {
      return sgo->GetDocShell();
    }
  }

  return nsnull;
}

nsIDOMDocument *
nsContentUtils::GetDocumentFromCaller()
{
  JSContext *cx = nsnull;
  sThreadJSContextStack->Peek(&cx);

  nsIDOMDocument *doc = nsnull;

  if (cx) {
    JSObject *callee = nsnull;
    JSStackFrame *fp = nsnull;
    while (!callee && (fp = ::JS_FrameIterator(cx, &fp))) {
      callee = ::JS_GetFrameCalleeObject(cx, fp);
    }

    nsCOMPtr<nsPIDOMWindow> win =
      do_QueryInterface(nsJSUtils::GetStaticScriptGlobal(cx, callee));
    if (win) {
      doc = win->GetExtantDocument();
    }
  }

  return doc;
}

nsIDOMDocument *
nsContentUtils::GetDocumentFromContext()
{
  if (!sThreadJSContextStack) {
    return nsnull;
  }

  JSContext *cx = nsnull;
  sThreadJSContextStack->Peek(&cx);

  nsCOMPtr<nsIDOMDocument> doc;

  if (cx) {
    nsIScriptGlobalObject *sgo = nsJSUtils::GetDynamicScriptGlobal(cx);

    nsCOMPtr<nsIDOMWindowInternal> win(do_QueryInterface(sgo));
    if (win) {
      win->GetDocument(getter_AddRefs(doc));
    }
  }

  // This will return a pointer to something we're about to release,
  // but that's ok here.
  return doc;
}

PRBool
nsContentUtils::IsCallerChrome()
{
  PRBool is_caller_chrome = PR_FALSE;

  if (sSecurityManager) {
    nsresult rv = sSecurityManager->SubjectPrincipalIsSystem(&is_caller_chrome);
    if (NS_FAILED(rv)) {
      return PR_FALSE;
    }
  }

  return is_caller_chrome;
}

static PRBool IsCallerTrustedForCapability(const char* aCapability)
{
  if (nsContentUtils::IsCallerChrome())
    return PR_TRUE;

  // The secman really should handle UniversalXPConnect case, since that
  // should include UniversalBrowserRead... doesn't right now, though.
  PRBool hasCap;
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  if (NS_FAILED(ssm->IsCapabilityEnabled(aCapability, &hasCap)))
    return PR_FALSE;
  if (hasCap)
    return PR_TRUE;
    
  if (NS_FAILED(ssm->IsCapabilityEnabled("UniversalXPConnect", &hasCap)))
    return PR_FALSE;
  return hasCap;
}

PRBool
nsContentUtils::IsCallerTrustedForRead()
{
  return IsCallerTrustedForCapability("UniversalBrowserRead");
}

PRBool
nsContentUtils::IsCallerTrustedForWrite()
{
  return IsCallerTrustedForCapability("UniversalBrowserWrite");
}

// static
PRBool
nsContentUtils::InSameDoc(nsIDOMNode* aNode, nsIDOMNode* aOther)
{
  if (!aNode || !aOther) {
    return PR_FALSE;
  }

  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
  nsCOMPtr<nsIContent> other(do_QueryInterface(aOther));

  if (content && other) {
    // XXXcaa Don't bother to check that either node is in a
    // document.  Editor relies on us returning true if neither
    // node is in a document.  See bug 154401.
    return content->GetDocument() == other->GetDocument();
  }

  return PR_FALSE;
}

// static
PRBool
nsContentUtils::ContentIsDescendantOf(nsIContent* aPossibleDescendant,
                                      nsIContent* aPossibleAncestor)
{
  NS_PRECONDITION(aPossibleDescendant, "The possible descendant is null!");
  NS_PRECONDITION(aPossibleAncestor, "The possible ancestor is null!");

  do {
    if (aPossibleDescendant == aPossibleAncestor)
      return PR_TRUE;
    aPossibleDescendant = aPossibleDescendant->GetParent();
  } while (aPossibleDescendant);

  return PR_FALSE;
}


// static
nsresult
nsContentUtils::GetAncestors(nsIDOMNode* aNode,
                             nsVoidArray* aArray)
{
  NS_ENSURE_ARG_POINTER(aNode);

  nsCOMPtr<nsIDOMNode> node(aNode);
  nsCOMPtr<nsIDOMNode> ancestor;

  do {
    aArray->AppendElement(node.get());
    node->GetParentNode(getter_AddRefs(ancestor));
    node.swap(ancestor);
  } while (node);

  return NS_OK;
}

// static
nsresult
nsContentUtils::GetAncestorsAndOffsets(nsIDOMNode* aNode,
                                       PRInt32 aOffset,
                                       nsVoidArray* aAncestorNodes,
                                       nsVoidArray* aAncestorOffsets)
{
  NS_ENSURE_ARG_POINTER(aNode);

  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));

  if (!content) {
    return NS_ERROR_FAILURE;
  }

  if (aAncestorNodes->Count() != 0) {
    NS_WARNING("aAncestorNodes is not empty");
    aAncestorNodes->Clear();
  }

  if (aAncestorOffsets->Count() != 0) {
    NS_WARNING("aAncestorOffsets is not empty");
    aAncestorOffsets->Clear();
  }

  // insert the node itself
  aAncestorNodes->AppendElement(content.get());
  aAncestorOffsets->AppendElement(NS_INT32_TO_PTR(aOffset));

  // insert all the ancestors
  nsIContent* child = content;
  nsIContent* parent = child->GetParent();
  while (parent) {
    aAncestorNodes->AppendElement(parent);
    aAncestorOffsets->AppendElement(NS_INT32_TO_PTR(parent->IndexOf(child)));
    child = parent;
    parent = parent->GetParent();
  }

  return NS_OK;
}

// static
nsresult
nsContentUtils::GetCommonAncestor(nsIDOMNode *aNode,
                                  nsIDOMNode *aOther,
                                  nsIDOMNode** aCommonAncestor)
{
  *aCommonAncestor = nsnull;

  nsCOMArray<nsIDOMNode> nodeArray;
  nsresult rv = GetFirstDifferentAncestors(aNode, aOther, nodeArray);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIDOMNode *common = nodeArray[0];

  NS_ASSERTION(common, "The common ancestor is null!  Very bad!");

  *aCommonAncestor = common;
  NS_IF_ADDREF(*aCommonAncestor);

  return NS_OK;
}

// static
nsresult
nsContentUtils::GetFirstDifferentAncestors(nsIDOMNode *aNode,
                                           nsIDOMNode *aOther,
                                           nsCOMArray<nsIDOMNode>& aDifferentNodes)
{
  NS_ENSURE_ARG_POINTER(aNode);
  NS_ENSURE_ARG_POINTER(aOther);

  if (aDifferentNodes.Count() != 0) {
    NS_WARNING("The aDifferentNodes array passed in is not empty!");
    aDifferentNodes.Clear();
  }

  // Test if both are the same node.
  if (aNode == aOther) {
    aDifferentNodes.AppendObject(aNode);
    return NS_OK;
  }

  nsCOMArray<nsIDOMNode> nodeAncestors;
  nsCOMArray<nsIDOMNode> otherAncestors;

  // Insert all the ancestors of |aNode|
  nsCOMPtr<nsIDOMNode> node(aNode);
  nsCOMPtr<nsIDOMNode> ancestor(node);
  do {
    nodeAncestors.AppendObject(node);
    node->GetParentNode(getter_AddRefs(ancestor));
    if (ancestor == aOther) {
      aDifferentNodes.AppendObject(aOther);
      return NS_OK;
    }
    node.swap(ancestor);
  } while (node);

  // Insert all the ancestors of |aOther|
  nsCOMPtr<nsIDOMNode> other(aOther);
  ancestor = other;
  do {
    otherAncestors.AppendObject(other);
    other->GetParentNode(getter_AddRefs(ancestor));
    if (ancestor == aNode) {
      aDifferentNodes.AppendObject(aNode);
      return NS_OK;
    }
    other.swap(ancestor);
  } while (other);

  PRInt32 nodeIdx  = nodeAncestors.Count() - 1;
  PRInt32 otherIdx = otherAncestors.Count() - 1;

  if (nodeAncestors[nodeIdx] != otherAncestors[otherIdx]) {
    // These two nodes are disconnected.  We can't get a common ancestor.
    return NS_ERROR_FAILURE;
  }

  // Go back through the ancestors, starting from the root,
  // until the first different ancestor found.
  do {
    --nodeIdx;
    --otherIdx;
  } while (nodeAncestors[nodeIdx] == otherAncestors[otherIdx]);

  NS_ASSERTION(nodeIdx >= 0 && otherIdx >= 0,
               "Something's wrong: our indices should not be negative here!");

  aDifferentNodes.AppendObject(nodeAncestors[nodeIdx + 1]);
  aDifferentNodes.AppendObject(nodeAncestors[nodeIdx]);
  aDifferentNodes.AppendObject(otherAncestors[otherIdx]);

  return NS_OK;
}

PRUint16
nsContentUtils::ComparePositionWithAncestors(nsIDOMNode *aNode,
                                             nsIDOMNode *aOther)
{
#ifdef DEBUG
  PRUint16 nodeType = 0;
  PRUint16 otherType = 0;
  aNode->GetNodeType(&nodeType);
  aOther->GetNodeType(&otherType);

  NS_PRECONDITION(nodeType != nsIDOMNode::ATTRIBUTE_NODE &&
                  nodeType != nsIDOMNode::DOCUMENT_NODE &&
                  nodeType != nsIDOMNode::DOCUMENT_FRAGMENT_NODE &&
                  nodeType != nsIDOMNode::ENTITY_NODE &&
                  nodeType != nsIDOMNode::NOTATION_NODE &&
                  otherType != nsIDOMNode::ATTRIBUTE_NODE &&
                  otherType != nsIDOMNode::DOCUMENT_NODE &&
                  otherType != nsIDOMNode::DOCUMENT_FRAGMENT_NODE &&
                  otherType != nsIDOMNode::ENTITY_NODE &&
                  otherType != nsIDOMNode::NOTATION_NODE,
                  "Bad.  Go read the documentation in the header!");
#endif // DEBUG

  PRUint16 mask = 0;

  nsCOMArray<nsIDOMNode> nodeAncestors;

  nsresult rv =
    nsContentUtils::GetFirstDifferentAncestors(aNode, aOther, nodeAncestors);

  if (NS_FAILED(rv)) {
    // If there is no common container node, then the order is based upon
    // order between the root container of each node that is in no container.
    // In this case, the result is disconnected and implementation-dependent.
    mask = (nsIDOM3Node::DOCUMENT_POSITION_DISCONNECTED |
            nsIDOM3Node::DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC);

    return mask;
  }

  nsIDOMNode* commonAncestor = nodeAncestors[0];

  if (commonAncestor == aNode) {
    mask = (nsIDOM3Node::DOCUMENT_POSITION_CONTAINED_BY |
            nsIDOM3Node::DOCUMENT_POSITION_FOLLOWING);

    return mask;
  }

  if (commonAncestor == aOther) {
    mask |= (nsIDOM3Node::DOCUMENT_POSITION_CONTAINS |
             nsIDOM3Node::DOCUMENT_POSITION_PRECEDING);

    return mask;
  }

  // GetFirstDifferentAncestors should only succeed if one of the
  // two nodes is the common ancestor, or if we have a common ancestor
  // and the two first different ancestors.  We checked the case above
  // where one of the two nodes were the common ancestor, so we must
  // have three items in our array now.
  NS_ASSERTION(commonAncestor && nodeAncestors.Count() == 3,
               "Something's wrong");

  nsIDOMNode* nodeAncestor = nodeAncestors[1];
  nsIDOMNode* otherAncestor = nodeAncestors[2];

  if (nodeAncestor && otherAncestor) {
    // Find out which of the two nodes comes first in the document order.
    // First get the children of the common ancestor.
    nsCOMPtr<nsIDOMNodeList> children;
    commonAncestor->GetChildNodes(getter_AddRefs(children));
    PRUint32 numKids;
    children->GetLength(&numKids);
    for (PRUint32 i = 0; i < numKids; ++i) {
      // Then go through the children one at a time to see which we hit first.
      nsCOMPtr<nsIDOMNode> childNode;
      children->Item(i, getter_AddRefs(childNode));
      if (childNode == nodeAncestor) {
        mask |= nsIDOM3Node::DOCUMENT_POSITION_FOLLOWING;
        break;
      }

      if (childNode == otherAncestor) {
        mask |= nsIDOM3Node::DOCUMENT_POSITION_PRECEDING;
        break;
      }
    }
  }

  return mask;
}

PRUint16
nsContentUtils::ReverseDocumentPosition(PRUint16 aDocumentPosition)
{
  // Disconnected and implementation-specific flags cannot be reversed.
  // Keep them.
  PRUint16 reversedPosition =
    aDocumentPosition & (nsIDOM3Node::DOCUMENT_POSITION_DISCONNECTED |
                         nsIDOM3Node::DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC);

  // Following/preceding
  if (aDocumentPosition & nsIDOM3Node::DOCUMENT_POSITION_FOLLOWING) {
    reversedPosition |= nsIDOM3Node::DOCUMENT_POSITION_PRECEDING;
  }
  else if (aDocumentPosition & nsIDOM3Node::DOCUMENT_POSITION_PRECEDING) {
    reversedPosition |= nsIDOM3Node::DOCUMENT_POSITION_FOLLOWING;
  }

  // Is contained/contains.
  if (aDocumentPosition & nsIDOM3Node::DOCUMENT_POSITION_CONTAINS) {
    reversedPosition |= nsIDOM3Node::DOCUMENT_POSITION_CONTAINED_BY;
  }
  else if (aDocumentPosition & nsIDOM3Node::DOCUMENT_POSITION_CONTAINED_BY) {
    reversedPosition |= nsIDOM3Node::DOCUMENT_POSITION_CONTAINS;
  }

  return reversedPosition;
}

inline PRBool
IsCharInSet(const char* aSet,
            const PRUnichar aChar)
{
  PRUnichar ch;
  while ((ch = *aSet)) {
    if (aChar == PRUnichar(ch)) {
      return PR_TRUE;
    }
    ++aSet;
  }
  return PR_FALSE;
}

/**
 * This method strips leading/trailing chars, in given set, from string.
 */

// static
const nsDependentSubstring
nsContentUtils::TrimCharsInSet(const char* aSet,
                               const nsAString& aValue)
{
  nsAString::const_iterator valueCurrent, valueEnd;

  aValue.BeginReading(valueCurrent);
  aValue.EndReading(valueEnd);

  // Skip charaters in the beginning
  while (valueCurrent != valueEnd) {
    if (!IsCharInSet(aSet, *valueCurrent)) {
      break;
    }
    ++valueCurrent;
  }

  if (valueCurrent != valueEnd) {
    for (;;) {
      --valueEnd;
      if (!IsCharInSet(aSet, *valueEnd)) {
        break;
      }
    }
    ++valueEnd; // Step beyond the last character we want in the value.
  }

  // valueEnd should point to the char after the last to copy
  return Substring(valueCurrent, valueEnd);
}

/**
 * This method strips leading and trailing whitespace from a string.
 */

// static
const nsDependentSubstring
nsContentUtils::TrimWhitespace(const nsAString& aStr, PRBool aTrimTrailing)
{
  nsAString::const_iterator start, end;

  aStr.BeginReading(start);
  aStr.EndReading(end);

  // Skip whitespace charaters in the beginning
  while (start != end && nsCRT::IsAsciiSpace(*start)) {
    ++start;
  }

  if (aTrimTrailing) {
    // Skip whitespace characters in the end.
    while (end != start) {
      --end;

      if (!nsCRT::IsAsciiSpace(*end)) {
        // Step back to the last non-whitespace character.
        ++end;

        break;
      }
    }
  }

  // Return a substring for the string w/o leading and/or trailing
  // whitespace

  return Substring(start, end);
}

static inline void KeyAppendSep(nsACString& aKey)
{
  if (!aKey.IsEmpty()) {
    aKey.Append('>');
  }
}

static inline void KeyAppendString(const nsAString& aString, nsACString& aKey)
{
  KeyAppendSep(aKey);

  // Could escape separator here if collisions happen.  > is not a legal char
  // for a name or type attribute, so we should be safe avoiding that extra work.

  AppendUTF16toUTF8(aString, aKey);
}

static inline void KeyAppendString(const nsACString& aString, nsACString& aKey)
{
  KeyAppendSep(aKey);

  // Could escape separator here if collisions happen.  > is not a legal char
  // for a name or type attribute, so we should be safe avoiding that extra work.

  aKey.Append(aString);
}

static inline void KeyAppendInt(PRInt32 aInt, nsACString& aKey)
{
  KeyAppendSep(aKey);

  aKey.Append(nsPrintfCString("%d", aInt));
}

static inline void KeyAppendAtom(nsIAtom* aAtom, nsACString& aKey)
{
  NS_PRECONDITION(aAtom, "KeyAppendAtom: aAtom can not be null!\n");

  const char* atomString = nsnull;
  aAtom->GetUTF8String(&atomString);

  KeyAppendString(nsDependentCString(atomString), aKey);
}

static inline PRBool IsAutocompleteOff(nsIDOMElement* aElement)
{
  nsAutoString autocomplete;
  aElement->GetAttribute(NS_LITERAL_STRING("autocomplete"), autocomplete);
  return autocomplete.LowerCaseEqualsLiteral("off");
}

/*static*/ nsresult
nsContentUtils::GenerateStateKey(nsIContent* aContent,
                                 nsIDocument* aDocument,
                                 nsIStatefulFrame::SpecialStateID aID,
                                 nsACString& aKey)
{
  aKey.Truncate();

  PRUint32 partID = aDocument ? aDocument->GetPartID() : 0;

  // SpecialStateID case - e.g. scrollbars around the content window
  // The key in this case is the special state id (always < min(contentID))
  if (nsIStatefulFrame::eNoID != aID) {
    KeyAppendInt(partID, aKey);  // first append a partID
    KeyAppendInt(aID, aKey);
    return NS_OK;
  }

  // We must have content if we're not using a special state id
  NS_ENSURE_TRUE(aContent, NS_ERROR_FAILURE);

  // Don't capture state for anonymous content
  PRUint32 contentID = aContent->ContentID();
  if (!contentID) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(aContent));
  if (element && IsAutocompleteOff(element)) {
    return NS_OK;
  }

  nsCOMPtr<nsIHTMLDocument> htmlDocument(do_QueryInterface(aContent->GetCurrentDoc()));

  KeyAppendInt(partID, aKey);  // first append a partID
  PRBool generatedUniqueKey = PR_FALSE;

  if (htmlDocument) {
    // Flush our content model so it'll be up to date
    aContent->GetCurrentDoc()->FlushPendingNotifications(Flush_Content);

    nsContentList *htmlForms = htmlDocument->GetForms();
    nsRefPtr<nsContentList> htmlFormControls =
      GetFormControlElements(aDocument);

    NS_ENSURE_TRUE(htmlForms && htmlFormControls, NS_ERROR_OUT_OF_MEMORY);

    // If we have a form control and can calculate form information, use
    // that as the key - it is more reliable than contentID.
    // Important to have a unique key, and tag/type/name may not be.
    //
    // If the control has a form, the format of the key is:
    // type>IndOfFormInDoc>IndOfControlInForm>FormName>name
    // else:
    // type>IndOfControlInDoc>name
    //
    // XXX We don't need to use index if name is there
    //
    nsCOMPtr<nsIFormControl> control(do_QueryInterface(aContent));
    if (control && htmlFormControls && htmlForms) {

      // Append the control type
      KeyAppendInt(control->GetType(), aKey);

      // If in a form, add form name / index of form / index in form
      PRInt32 index = -1;
      nsCOMPtr<nsIDOMHTMLFormElement> formElement;
      control->GetForm(getter_AddRefs(formElement));
      if (formElement) {

        if (IsAutocompleteOff(formElement)) {
          aKey.Truncate();
          return NS_OK;
        }

        // Append the index of the form in the document
        nsCOMPtr<nsIContent> formContent(do_QueryInterface(formElement));
        index = htmlForms->IndexOf(formContent, PR_FALSE);
        if (index <= -1) {
          //
          // XXX HACK this uses some state that was dumped into the document
          // specifically to fix bug 138892.  What we are trying to do is *guess*
          // which form this control's state is found in, with the highly likely
          // guess that the highest form parsed so far is the one.
          // This code should not be on trunk, only branch.
          //
          index = htmlDocument->GetNumFormsSynchronous() - 1;
        }
        if (index > -1) {
          KeyAppendInt(index, aKey);

          // Append the index of the control in the form
          nsCOMPtr<nsIForm> form(do_QueryInterface(formElement));
          form->IndexOfControl(control, &index);

          if (index > -1) {
            KeyAppendInt(index, aKey);
            generatedUniqueKey = PR_TRUE;
          }
        }

        // Append the form name
        nsAutoString formName;
        formElement->GetName(formName);
        KeyAppendString(formName, aKey);

      } else {

        // If not in a form, add index of control in document
        // Less desirable than indexing by form info.

        // Hash by index of control in doc (we are not in a form)
        // These are important as they are unique, and type/name may not be.

        // Note that we've already flushed content, so there's no
        // reason to flush it again.
        index = htmlFormControls->IndexOf(aContent, PR_FALSE);
        if (index > -1) {
          KeyAppendInt(index, aKey);
          generatedUniqueKey = PR_TRUE;
        }
      }

      // Append the control name
      nsAutoString name;
      aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::name, name);
      KeyAppendString(name, aKey);
    }
  }

  if (!generatedUniqueKey) {

    // Either we didn't have a form control or we aren't in an HTML document
    // so we can't figure out form info, hash by content ID instead :(
    KeyAppendInt(contentID, aKey);
  }

  return NS_OK;
}

// static
nsresult
nsContentUtils::NewURIWithDocumentCharset(nsIURI** aResult,
                                          const nsAString& aSpec,
                                          nsIDocument* aDocument,
                                          nsIURI* aBaseURI)
{
  return NS_NewURI(aResult, aSpec,
                   aDocument ? aDocument->GetDocumentCharacterSet().get() : nsnull,
                   aBaseURI, sIOService);
}

// static
PRBool
nsContentUtils::BelongsInForm(nsIDOMHTMLFormElement *aForm,
                              nsIContent *aContent)
{
  NS_PRECONDITION(aForm, "Must have a form");
  NS_PRECONDITION(aContent, "Must have a content node");

  nsCOMPtr<nsIContent> form(do_QueryInterface(aForm));

  if (!form) {
    NS_ERROR("This should not happen, form is not an nsIContent!");

    return PR_TRUE;
  }

  if (form == aContent) {
    // A form does not belong inside itself, so we return false here

    return PR_FALSE;
  }

  nsIContent* content = aContent->GetParent();

  while (content) {
    if (content == form) {
      // aContent is contained within the form so we return true.

      return PR_TRUE;
    }

    if (content->Tag() == nsHTMLAtoms::form &&
        content->IsContentOfType(nsIContent::eHTML)) {
      // The child is contained within a form, but not the right form
      // so we ignore it.

      return PR_FALSE;
    }

    content = content->GetParent();
  }

  if (form->GetChildCount() > 0) {
    // The form is a container but aContent wasn't inside the form,
    // return false

    return PR_FALSE;
  }

  // The form is a leaf and aContent wasn't inside any other form so
  // we check whether the content comes after the form.  If it does,
  // return true.  If it does not, then it couldn't have been inside
  // the form in the HTML.
  nsCOMPtr<nsIDOM3Node> contentAsDOM3(do_QueryInterface(aContent));
  PRUint16 comparisonFlags = 0;
  nsresult rv = NS_OK;
  if (contentAsDOM3) {
    rv = contentAsDOM3->CompareDocumentPosition(aForm, &comparisonFlags);
  }
  if (NS_FAILED(rv) ||
      comparisonFlags & nsIDOM3Node::DOCUMENT_POSITION_PRECEDING) {
    // We could be in this form!
    // In the future, we may want to get document.forms, look at the
    // form after aForm, and if aContent is after that form after
    // aForm return false here....
    return PR_TRUE;
  }

  return PR_FALSE;
}

// static
nsresult
nsContentUtils::CheckQName(const nsAString& aQualifiedName,
                           PRBool aNamespaceAware)
{
  nsIParserService *parserService = GetParserServiceWeakRef();
  NS_ENSURE_TRUE(parserService, NS_ERROR_FAILURE);

  const PRUnichar *colon;
  return parserService->CheckQName(PromiseFlatString(aQualifiedName),
                                   aNamespaceAware, &colon);
}

//static
nsresult
nsContentUtils::SplitQName(nsIContent* aNamespaceResolver,
                           const nsAFlatString& aQName,
                           PRInt32 *aNamespace, nsIAtom **aLocalName)
{
  nsIParserService* parserService = GetParserServiceWeakRef();
  NS_ENSURE_TRUE(parserService, NS_ERROR_FAILURE);

  const PRUnichar* colon;
  nsresult rv = parserService->CheckQName(aQName, PR_TRUE, &colon);
  NS_ENSURE_SUCCESS(rv, rv);

  if (colon) {
    const PRUnichar* end;
    aQName.EndReading(end);
    nsAutoString nameSpace;
    rv = LookupNamespaceURI(aNamespaceResolver, Substring(aQName.get(), colon),
                            nameSpace);
    NS_ENSURE_SUCCESS(rv, rv);

    GetNSManagerWeakRef()->GetNameSpaceID(nameSpace, aNamespace);
    if (*aNamespace == kNameSpaceID_Unknown)
      return NS_ERROR_FAILURE;

    *aLocalName = NS_NewAtom(Substring(colon + 1, end));
  }
  else {
    *aNamespace = kNameSpaceID_None;
    *aLocalName = NS_NewAtom(aQName);
  }
  NS_ENSURE_TRUE(aLocalName, NS_ERROR_OUT_OF_MEMORY);
  return NS_OK;
}

// static
nsresult
nsContentUtils::LookupNamespaceURI(nsIContent* aNamespaceResolver,
                                   const nsAString& aNamespacePrefix,
                                   nsAString& aNamespaceURI)
{
  nsCOMPtr<nsIAtom> name;
  if (!aNamespacePrefix.IsEmpty()) {
    name = do_GetAtom(aNamespacePrefix);
    NS_ENSURE_TRUE(name, NS_ERROR_OUT_OF_MEMORY);
  }
  else {
    name = nsLayoutAtoms::xmlnsNameSpace;
  }
  // Trace up the content parent chain looking for the namespace
  // declaration that declares aNamespacePrefix.
  for (nsIContent* content = aNamespaceResolver; content;
       content = content->GetParent()) {
    if (content->GetAttr(kNameSpaceID_XMLNS, name, aNamespaceURI) ==
        NS_CONTENT_ATTR_HAS_VALUE)
      return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

// static
nsresult
nsContentUtils::GetNodeInfoFromQName(const nsAString& aNamespaceURI,
                                     const nsAString& aQualifiedName,
                                     nsNodeInfoManager* aNodeInfoManager,
                                     nsINodeInfo** aNodeInfo)
{
  nsIParserService* parserService = GetParserServiceWeakRef();
  NS_ENSURE_TRUE(parserService, NS_ERROR_FAILURE);

  const nsAFlatString& qName = PromiseFlatString(aQualifiedName);
  const PRUnichar* colon;
  nsresult rv = parserService->CheckQName(qName, PR_TRUE, &colon);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 nsID;
  sNameSpaceManager->RegisterNameSpace(aNamespaceURI, nsID);
  if (colon) {
    const PRUnichar* end;
    qName.EndReading(end);

    nsCOMPtr<nsIAtom> prefix = do_GetAtom(Substring(qName.get(), colon));

    rv = aNodeInfoManager->GetNodeInfo(Substring(colon + 1, end), prefix,
                                       nsID, aNodeInfo);
  }
  else {
    rv = aNodeInfoManager->GetNodeInfo(aQualifiedName, nsnull, nsID,
                                       aNodeInfo);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  nsIAtom* prefix = (*aNodeInfo)->GetPrefixAtom();
  nsIAtom* nil = nsnull;

  // NAMESPACE_ERR: Raised if the qualifiedName is a malformed qualified name,
  // if the qualifiedName has a prefix and the namespaceURI is null, if the
  // qualifiedName has a prefix that is "xml" and the namespaceURI is different
  // from "http://www.w3.org/XML/1998/namespace", if the qualifiedName or its
  // prefix is "xmlns" and the namespaceURI is different from
  // "http://www.w3.org/2000/xmlns/", or if the namespaceURI is
  // "http://www.w3.org/2000/xmlns/" and neither the qualifiedName nor its
  // prefix is "xmlns".
  PRBool xmlPrefix = prefix == nsLayoutAtoms::xmlNameSpace;
  PRBool xmlns = (*aNodeInfo)->Equals(nsLayoutAtoms::xmlnsNameSpace, nil) ||
                 prefix == nsLayoutAtoms::xmlnsNameSpace;

  return (prefix && DOMStringIsNull(aNamespaceURI)) ||
         (xmlPrefix && nsID != kNameSpaceID_XML) ||
         (xmlns && nsID != kNameSpaceID_XMLNS) ||
         (nsID == kNameSpaceID_XMLNS && !xmlns) ?
         NS_ERROR_DOM_NAMESPACE_ERR : NS_OK;
}

// static
void
nsContentUtils::SplitExpatName(const PRUnichar *aExpatName, nsIAtom **aPrefix,
                               nsIAtom **aLocalName, PRInt32* aNameSpaceID)
{
  /**
   *  Expat can send the following:
   *    localName
   *    namespaceURI<separator>localName
   *    namespaceURI<separator>localName<separator>prefix
   *
   *  and we use 0xFFFF for the <separator>.
   *
   */

  const PRUnichar *uriEnd = nsnull;
  const PRUnichar *nameEnd = nsnull;
  const PRUnichar *pos;
  for (pos = aExpatName; *pos; ++pos) {
    if (*pos == 0xFFFF) {
      if (uriEnd) {
        nameEnd = pos;
      }
      else {
        uriEnd = pos;
      }
    }
  }

  const PRUnichar *nameStart;
  if (uriEnd) {
    if (sNameSpaceManager) {
      sNameSpaceManager->RegisterNameSpace(nsDependentSubstring(aExpatName,
                                                                uriEnd),
                                           *aNameSpaceID);
    }
    else {
      *aNameSpaceID = kNameSpaceID_Unknown;
    }

    nameStart = (uriEnd + 1);
    if (nameEnd)  {
      const PRUnichar *prefixStart = nameEnd + 1;
      *aPrefix = NS_NewAtom(NS_ConvertUTF16toUTF8(prefixStart,
                                                  pos - prefixStart));
    }
    else {
      nameEnd = pos;
      *aPrefix = nsnull;
    }
  }
  else {
    *aNameSpaceID = kNameSpaceID_None;
    nameStart = aExpatName;
    nameEnd = pos;
    *aPrefix = nsnull;
  }
  *aLocalName = NS_NewAtom(NS_ConvertUTF16toUTF8(nameStart,
                                                 nameEnd - nameStart));
}

// static
PRBool
nsContentUtils::CanLoadImage(nsIURI* aURI, nsISupports* aContext,
                             nsIDocument* aLoadingDocument,
                             PRInt16* aImageBlockingStatus)
{
  NS_PRECONDITION(aURI, "Must have a URI");
  NS_PRECONDITION(aLoadingDocument, "Must have a document");

  nsresult rv;

  PRUint32 appType = nsIDocShell::APP_TYPE_UNKNOWN;

  {
    nsCOMPtr<nsISupports> container = aLoadingDocument->GetContainer();
    nsCOMPtr<nsIDocShellTreeItem> docShellTreeItem =
      do_QueryInterface(container);

    if (docShellTreeItem) {
      nsCOMPtr<nsIDocShellTreeItem> root;
      docShellTreeItem->GetRootTreeItem(getter_AddRefs(root));

      nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(root));

      if (!docShell || NS_FAILED(docShell->GetAppType(&appType))) {
        appType = nsIDocShell::APP_TYPE_UNKNOWN;
      }
    }
  }

  if (appType != nsIDocShell::APP_TYPE_EDITOR) {
    // Editor apps get special treatment here, editors can load images
    // from anywhere.
    rv = sSecurityManager->
      CheckLoadURIWithPrincipal(aLoadingDocument->GetPrincipal(), aURI,
                                nsIScriptSecurityManager::ALLOW_CHROME);
    if (NS_FAILED(rv)) {
      if (aImageBlockingStatus) {
        // Reject the request itself, not all requests to the relevant
        // server...
        *aImageBlockingStatus = nsIContentPolicy::REJECT_REQUEST;
      }
      return PR_FALSE;
    }
  }

  PRInt16 decision = nsIContentPolicy::ACCEPT;

  rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_IMAGE,
                                 aURI,
                                 aLoadingDocument->GetDocumentURI(),
                                 aContext,
                                 EmptyCString(), //mime guess
                                 nsnull,         //extra
                                 &decision);

  if (aImageBlockingStatus) {
    *aImageBlockingStatus =
      NS_FAILED(rv) ? nsIContentPolicy::REJECT_REQUEST : decision;
  }
  return NS_FAILED(rv) ? PR_FALSE : NS_CP_ACCEPTED(decision);
}

// static
nsresult
nsContentUtils::LoadImage(nsIURI* aURI, nsIDocument* aLoadingDocument,
                          nsIURI* aReferrer, imgIDecoderObserver* aObserver,
                          PRInt32 aLoadFlags, imgIRequest** aRequest)
{
  NS_PRECONDITION(aURI, "Must have a URI");
  NS_PRECONDITION(aLoadingDocument, "Must have a document");
  NS_PRECONDITION(aRequest, "Null out param");

  if (!sImgLoader) {
    // nothing we can do here
    return NS_OK;
  }

  nsCOMPtr<nsILoadGroup> loadGroup = aLoadingDocument->GetDocumentLoadGroup();
  NS_WARN_IF_FALSE(loadGroup, "Could not get loadgroup; onload may fire too early");

  nsIURI *documentURI = aLoadingDocument->GetDocumentURI();

  // XXXbz using "documentURI" for the initialDocumentURI is not quite
  // right, but the best we can do here...
  return sImgLoader->LoadImage(aURI,                 /* uri to load */
                               documentURI,          /* initialDocumentURI */
                               aReferrer,            /* referrer */
                               loadGroup,            /* loadgroup */
                               aObserver,            /* imgIDecoderObserver */
                               aLoadingDocument,     /* uniquification key */
                               aLoadFlags,           /* load flags */
                               nsnull,               /* cache key */
                               nsnull,               /* existing request*/
                               aRequest);
}

// static
already_AddRefed<nsIImage>
nsContentUtils::GetImageFromContent(nsIImageLoadingContent* aContent,
                                    imgIRequest **aRequest)
{
  if (aRequest) {
    *aRequest = nsnull;
  }

  NS_ENSURE_TRUE(aContent, nsnull);

  nsCOMPtr<imgIRequest> imgRequest;
  aContent->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                      getter_AddRefs(imgRequest));
  if (!imgRequest) {
    return nsnull;
  }

  nsCOMPtr<imgIContainer> imgContainer;
  imgRequest->GetImage(getter_AddRefs(imgContainer));

  if (!imgContainer) {
    return nsnull;
  }

  nsCOMPtr<gfxIImageFrame> imgFrame;
  imgContainer->GetFrameAt(0, getter_AddRefs(imgFrame));

  if (!imgFrame) {
    return nsnull;
  }

  nsCOMPtr<nsIInterfaceRequestor> ir = do_QueryInterface(imgFrame);

  if (!ir) {
    return nsnull;
  }

  if (aRequest) {
    imgRequest.swap(*aRequest);
  }

  nsIImage* image = nsnull;
  CallGetInterface(ir.get(), &image);
  return image;
}

// static
PRBool
nsContentUtils::IsDraggableImage(nsIContent* aContent)
{
  NS_PRECONDITION(aContent, "Must have content node to test");

  nsCOMPtr<nsIImageLoadingContent> imageContent(do_QueryInterface(aContent));
  if (!imageContent) {
    return PR_FALSE;
  }

  nsCOMPtr<imgIRequest> imgRequest;
  imageContent->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                           getter_AddRefs(imgRequest));

  // XXXbz It may be draggable even if the request resulted in an error.  Why?
  // Not sure; that's what the old nsContentAreaDragDrop/nsFrame code did.
  return imgRequest != nsnull;
}

// static
PRBool
nsContentUtils::IsDraggableLink(nsIContent* aContent) {
  nsCOMPtr<nsIURI> linkURI = GetLinkURI(aContent);

  // Does it have a URI?  If not, it's not draggable
  return linkURI != nsnull;
}

// static
already_AddRefed<nsIURI>
nsContentUtils::GetLinkURI(nsIContent* aContent)
{
  NS_PRECONDITION(aContent, "Must have content node to work with");

  nsCOMPtr<nsILink> link(do_QueryInterface(aContent));
  if (link) {
    nsIURI* uri = nsnull;
    link->GetHrefURI(&uri);
    if (uri) {
      return uri;
    }
  }

  // It could still be an XLink
  return GetXLinkURI(aContent);
}

// static
already_AddRefed<nsIURI>
nsContentUtils::GetXLinkURI(nsIContent* aContent)
{
  NS_PRECONDITION(aContent, "Must have content node to work with");

  nsAutoString value;
  aContent->GetAttr(kNameSpaceID_XLink, nsHTMLAtoms::type, value);
  if (value.EqualsLiteral("simple")) {
    // Check that we have a URI
    aContent->GetAttr(kNameSpaceID_XLink, nsHTMLAtoms::href, value);

    if (!value.IsEmpty()) {
      //  Resolve it relative to aContent's base URI.
      nsCOMPtr<nsIURI> baseURI = aContent->GetBaseURI();

      nsIURI* uri = nsnull;
      nsContentUtils::NewURIWithDocumentCharset(&uri, value,
                                                aContent->GetDocument(),
                                                baseURI);
      return uri;
    }
  }

  return nsnull;
}

// static
nsAdoptingCString
nsContentUtils::GetCharPref(const char *aPref)
{
  nsAdoptingCString result;

  if (sPrefBranch) {
    sPrefBranch->GetCharPref(aPref, getter_Copies(result));
  }

  return result;
}

// static
PRPackedBool
nsContentUtils::GetBoolPref(const char *aPref, PRBool aDefault)
{
  PRBool result;

  if (!sPrefBranch ||
      NS_FAILED(sPrefBranch->GetBoolPref(aPref, &result))) {
    result = aDefault;
  }

  return (PRPackedBool)result;
}

// static
PRInt32
nsContentUtils::GetIntPref(const char *aPref, PRInt32 aDefault)
{
  PRInt32 result;

  if (!sPrefBranch ||
      NS_FAILED(sPrefBranch->GetIntPref(aPref, &result))) {
    result = aDefault;
  }

  return result;
}

// static
nsAdoptingString
nsContentUtils::GetLocalizedStringPref(const char *aPref)
{
  nsAdoptingString result;

  if (sPrefBranch) {
    nsCOMPtr<nsIPrefLocalizedString> prefLocalString;
    sPrefBranch->GetComplexValue(aPref, NS_GET_IID(nsIPrefLocalizedString),
                                 getter_AddRefs(prefLocalString));
    if (prefLocalString) {
      prefLocalString->GetData(getter_Copies(result));
    }
  }

  return result;
}

// static
nsAdoptingString
nsContentUtils::GetStringPref(const char *aPref)
{
  nsAdoptingString result;

  if (sPrefBranch) {
    nsCOMPtr<nsISupportsString> theString;
    sPrefBranch->GetComplexValue(aPref, NS_GET_IID(nsISupportsString),
                                 getter_AddRefs(theString));
    if (theString) {
      theString->ToString(getter_Copies(result));
    }
  }

  return result;
}

// static
void
nsContentUtils::RegisterPrefCallback(const char *aPref,
                                     PrefChangedFunc aCallback,
                                     void * aClosure)
{
  if (sPref)
    sPref->RegisterCallback(aPref, aCallback, aClosure);
}

// static
void
nsContentUtils::UnregisterPrefCallback(const char *aPref,
                                       PrefChangedFunc aCallback,
                                       void * aClosure)
{
  if (sPref)
    sPref->UnregisterCallback(aPref, aCallback, aClosure);
}


static const char gEventName[] = "event";
static const char gSVGEventName[] = "evt";

// static
const char *
nsContentUtils::GetEventArgName(PRInt32 aNameSpaceID)
{
  if (aNameSpaceID == kNameSpaceID_SVG)
    return gSVGEventName;

  return gEventName;
}

void
nsCxPusher::Push(nsISupports *aCurrentTarget)
{
  if (mScx) {
    NS_ERROR("Whaaa! No double pushing with nsCxPusher::Push()!");

    return;
  }

  nsCOMPtr<nsIScriptGlobalObject> sgo;
  nsCOMPtr<nsIContent> content(do_QueryInterface(aCurrentTarget));
  nsCOMPtr<nsIDocument> document;

  if (content) {
    document = content->GetDocument();
  }

  if (!document) {
    document = do_QueryInterface(aCurrentTarget);
  }

  if (document) {
    sgo = document->GetScriptGlobalObject();
  }

  if (!document && !sgo) {
    sgo = do_QueryInterface(aCurrentTarget);
  }

  JSContext *cx = nsnull;

  if (sgo) {
    mScx = sgo->GetContext();

    if (mScx) {
      cx = (JSContext *)mScx->GetNativeContext();
    }
  }

  if (cx) {
    if (!mStack) {
      mStack = do_GetService(kJSStackContractID);
    }

    if (mStack) {
      JSContext *current = nsnull;
      mStack->Peek(&current);

      if (current) {
        // If there's a context on the stack, that means that a script
        // is running at the moment.

        mScriptIsRunning = PR_TRUE;
      }

      mStack->Push(cx);
    }
  } else {
    // If there's no native context in the script context it must be
    // in the process or being torn down. We don't want to notify the
    // script context about scripts having been evaluated in such a
    // case, so null out mScx.

    mScx = nsnull;
  }
}

void
nsCxPusher::Pop()
{
  if (!mScx || !mStack) {
    mScx = nsnull;

    NS_ASSERTION(!mScriptIsRunning, "Huh, this can't be happening, "
                 "mScriptIsRunning can't be set here!");

    return;
  }

  JSContext *unused;
  mStack->Pop(&unused);

  if (!mScriptIsRunning) {
    // No JS is running, but executing the event handler might have
    // caused some JS to run. Tell the script context that it's done.

    mScx->ScriptEvaluated(PR_TRUE);
  }

  mScx = nsnull;
  mScriptIsRunning = PR_FALSE;
}

static const char gPropertiesFiles[nsContentUtils::PropertiesFile_COUNT][56] = {
  // Must line up with the enum values in |PropertiesFile| enum.
  "chrome://global/locale/css.properties",
  "chrome://global/locale/xbl.properties",
  "chrome://global/locale/xul.properties",
  "chrome://global/locale/layout_errors.properties",
  "chrome://global/locale/layout/HtmlForm.properties",
  "chrome://global/locale/printing.properties",
  "chrome://global/locale/dom/dom.properties",
  "chrome://branding/locale/brand.properties"
};

/* static */ nsresult
nsContentUtils::EnsureStringBundle(PropertiesFile aFile)
{
  if (!sStringBundles[aFile]) {
    if (!sStringBundleService) {
      nsresult rv =
        CallGetService(NS_STRINGBUNDLE_CONTRACTID, &sStringBundleService);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    nsIStringBundle *bundle;
    nsresult rv =
      sStringBundleService->CreateBundle(gPropertiesFiles[aFile], &bundle);
    NS_ENSURE_SUCCESS(rv, rv);
    sStringBundles[aFile] = bundle; // transfer ownership
  }
  return NS_OK;
}

/* static */
nsresult nsContentUtils::GetLocalizedString(PropertiesFile aFile,
                                            const char* aKey,
                                            nsXPIDLString& aResult)
{
  nsresult rv = EnsureStringBundle(aFile);
  NS_ENSURE_SUCCESS(rv, rv);
  nsIStringBundle *bundle = sStringBundles[aFile];

  return bundle->GetStringFromName(NS_ConvertASCIItoUCS2(aKey).get(),
                                   getter_Copies(aResult));
}

/* static */
nsresult nsContentUtils::FormatLocalizedString(PropertiesFile aFile,
                                               const char* aKey,
                                               const PRUnichar **aParams,
                                               PRUint32 aParamsLength,
                                               nsXPIDLString& aResult)
{
  nsresult rv = EnsureStringBundle(aFile);
  NS_ENSURE_SUCCESS(rv, rv);
  nsIStringBundle *bundle = sStringBundles[aFile];

  return bundle->FormatStringFromName(NS_ConvertASCIItoUCS2(aKey).get(),
                                      aParams, aParamsLength,
                                      getter_Copies(aResult));
}

/* static */ nsresult
nsContentUtils::ReportToConsole(PropertiesFile aFile,
                                const char *aMessageName,
                                const PRUnichar **aParams,
                                PRUint32 aParamsLength,
                                nsIURI* aURI,
                                const nsAFlatString& aSourceLine,
                                PRUint32 aLineNumber,
                                PRUint32 aColumnNumber,
                                PRUint32 aErrorFlags,
                                const char *aCategory)
{
  nsresult rv;
  if (!sConsoleService) { // only need to bother null-checking here
    rv = CallGetService(NS_CONSOLESERVICE_CONTRACTID, &sConsoleService);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsXPIDLString errorText;
  rv = FormatLocalizedString(aFile, aMessageName, aParams, aParamsLength,
                             errorText);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString spec;
  if (aURI)
    aURI->GetSpec(spec);

  nsCOMPtr<nsIScriptError> errorObject =
      do_CreateInstance(NS_SCRIPTERROR_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = errorObject->Init(errorText.get(),
                         NS_ConvertUTF8toUTF16(spec).get(), // file name
                         aSourceLine.get(),
                         aLineNumber, aColumnNumber,
                         aErrorFlags, aCategory);
  NS_ENSURE_SUCCESS(rv, rv);

  return sConsoleService->LogMessage(errorObject);
}

static PRBool MatchFormControls(nsIContent* aContent, PRInt32 aNamespaceID,
                                nsIAtom* aAtom, const nsAString& aData)
{
  return aContent->IsContentOfType(nsIContent::eHTML_FORM_CONTROL);
}

/* static */ already_AddRefed<nsContentList>
nsContentUtils::GetFormControlElements(nsIDocument *aDocument)
{
  nsContentList *list = new nsContentList(aDocument,
                                          MatchFormControls, EmptyString());
  NS_IF_ADDREF(list);
  return list;
}

PRBool
nsContentUtils::IsChromeDoc(nsIDocument *aDocument)
{
  nsIPrincipal *principal;
  if (!aDocument || !(principal = aDocument->GetPrincipal())) {
    return PR_FALSE;
  }

  nsCOMPtr<nsIPrincipal> systemPrincipal;
  sSecurityManager->GetSystemPrincipal(getter_AddRefs(systemPrincipal));

  return principal == systemPrincipal;
}

void
nsContentUtils::NotifyXPCIfExceptionPending(JSContext* aCx)
{
  if (!::JS_IsExceptionPending(aCx)) {
    return;
  }

  nsCOMPtr<nsIXPCNativeCallContext> nccx;
  XPConnect()->GetCurrentNativeCallContext(getter_AddRefs(nccx));
  if (nccx) {
    // Check to make sure that the JSContext that nccx will mess with is the
    // same as the JSContext we've set an exception on.  If they're not the
    // same, don't mess with nccx.
    JSContext* cx;
    nccx->GetJSContext(&cx);
    if (cx == aCx) {
      nccx->SetExceptionWasThrown(PR_TRUE);
    }
  }
}

// static
nsresult
nsContentUtils::AddJSGCRoot(void* aPtr, const char* aName)
{
  if (!sScriptRuntime) {
    nsresult rv = CallGetService("@mozilla.org/js/xpc/RuntimeService;1",
                                 &sJSRuntimeService);
    NS_ENSURE_TRUE(sJSRuntimeService, rv);

    sJSRuntimeService->GetRuntime(&sScriptRuntime);
    if (!sScriptRuntime) {
      NS_RELEASE(sJSRuntimeService);
      NS_WARNING("Unable to get JS runtime from JS runtime service");
      return NS_ERROR_FAILURE;
    }
  }

  PRBool ok;
  ok = ::JS_AddNamedRootRT(sScriptRuntime, aPtr, aName);
  if (!ok) {
    if (sScriptRootCount == 0) {
      // We just got the runtime... Just null things out, since no
      // one's expecting us to have a runtime yet
      NS_RELEASE(sJSRuntimeService);
      sScriptRuntime = nsnull;
    }
    NS_WARNING("JS_AddNamedRootRT failed");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // We now have one more root we added to the runtime
  ++sScriptRootCount;

  return NS_OK;
}

/* static */
nsresult
nsContentUtils::RemoveJSGCRoot(void* aPtr)
{
  if (!sScriptRuntime) {
    NS_NOTREACHED("Trying to remove a JS GC root when none were added");
    return NS_ERROR_UNEXPECTED;
  }

  ::JS_RemoveRootRT(sScriptRuntime, aPtr);

  if (--sScriptRootCount == 0) {
    NS_RELEASE(sJSRuntimeService);
    sScriptRuntime = nsnull;
  }

  return NS_OK;
}
