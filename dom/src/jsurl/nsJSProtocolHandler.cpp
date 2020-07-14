/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 sw=4 et tw=78: */
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
#include "nsAutoPtr.h"
#include "jsapi.h"
#include "nsCRT.h"
#include "nsDOMError.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsJSProtocolHandler.h"
#include "nsNetUtil.h"

#include "nsIComponentManager.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsIURI.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIStringStream.h"
#include "nsIWindowMediator.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsIJSConsoleService.h"
#include "nsIConsoleService.h"
#include "nsXPIDLString.h"
#include "prprf.h"
#include "nsEscape.h"
#include "nsIJSContextStack.h"
#include "nsIWebNavigation.h"
#include "nsIDocShell.h"
#include "nsIContentViewer.h"
#include "nsIStringStream.h"
#include "nsIXPConnect.h"
#include "nsContentUtils.h"
#include "nsJSUtils.h"


static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);
static NS_DEFINE_CID(kWindowMediatorCID, NS_WINDOWMEDIATOR_CID);



class nsJSThunk : public nsIInputStream
{
public:
    nsJSThunk();

    NS_DECL_ISUPPORTS
    NS_FORWARD_SAFE_NSIINPUTSTREAM(mInnerStream)

    nsresult Init(nsIURI* uri);
    nsresult EvaluateScript(nsIChannel *aChannel);
    nsresult BringUpConsole(nsIDOMWindow *aDomWindow);

protected:
    virtual ~nsJSThunk();

    nsCOMPtr<nsIURI>            mURI;
    nsCOMPtr<nsIInputStream>    mInnerStream;
};

//
// nsISupports implementation...
//
NS_IMPL_THREADSAFE_ISUPPORTS1(nsJSThunk, nsIInputStream)


nsJSThunk::nsJSThunk()
{
}

nsJSThunk::~nsJSThunk()
{
}

nsresult nsJSThunk::Init(nsIURI* uri)
{
    NS_ENSURE_ARG_POINTER(uri);

    mURI = uri;
    return NS_OK;
}

static PRBool
IsISO88591(const nsString& aString)
{
    for (nsString::const_char_iterator c = aString.BeginReading(),
                                   c_end = aString.EndReading();
         c < c_end; ++c) {
        if (*c > 255)
            return PR_FALSE;
    }
    return PR_TRUE;
}

nsresult nsJSThunk::EvaluateScript(nsIChannel *aChannel)
{
    nsresult rv;

    NS_ENSURE_ARG_POINTER(aChannel);

    // Get the script string to evaluate...
    nsCAutoString script;
    rv = mURI->GetPath(script);
    if (NS_FAILED(rv)) return rv;

    // The the global object owner from the channel
    nsCOMPtr<nsIScriptGlobalObjectOwner> globalOwner;
    NS_QueryNotificationCallbacks(aChannel, globalOwner);
    NS_ASSERTION(globalOwner, 
                 "Unable to get an nsIScriptGlobalObjectOwner from the "
                 "channel!");
    if (!globalOwner) {
        return NS_ERROR_FAILURE;
    }

    // So far so good: get the script context from its owner.
    nsIScriptGlobalObject* global = globalOwner->GetScriptGlobalObject();

    NS_ASSERTION(global,
                 "Unable to get an nsIScriptGlobalObject from the "
                 "ScriptGlobalObjectOwner!");
    if (!global) {
        return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(global));

    // Get the document out of the window to make sure we create a new
    // inner window if one doesn't already exist (see bug 306630).
    nsCOMPtr<nsIDOMDocument> doc;
    win->GetDocument(getter_AddRefs(doc));

    nsPIDOMWindow *innerWin = win->GetCurrentInnerWindow();

    if (!innerWin) {
        return NS_ERROR_UNEXPECTED;
    }

    nsCOMPtr<nsIScriptGlobalObject> innerGlobal = do_QueryInterface(innerWin);

    JSObject *globalJSObject = innerGlobal->GetGlobalJSObject();

    nsCOMPtr<nsIDOMWindow> domWindow(do_QueryInterface(global, &rv));
    if (NS_FAILED(rv)) {
        return NS_ERROR_FAILURE;
    }

    // If mURI is just "javascript:", we bring up the Error console
    // and return NS_ERROR_DOM_RETVAL_UNDEFINED.
    if (script.IsEmpty()) {
        rv = BringUpConsole(domWindow);
        if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
        return NS_ERROR_DOM_RETVAL_UNDEFINED;
    }

    // Now get the DOM Document.  Accessing the document will create one
    // if necessary.  So, basically, this call ensures that a document gets
    // created -- if necessary.
    rv = domWindow->GetDocument(getter_AddRefs(doc));
    NS_ASSERTION(doc, "No DOMDocument!");
    if (NS_FAILED(rv)) {
        return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIScriptContext> scriptContext = global->GetContext();
    if (!scriptContext)
        return NS_ERROR_FAILURE;

    // Unescape the script
    NS_UnescapeURL(script);

    // Get the url.
    nsCAutoString url;
    rv = mURI->GetSpec(url);
    if (NS_FAILED(rv)) return rv;

    // Get principal of code for execution
    nsCOMPtr<nsISupports> owner;
    rv = aChannel->GetOwner(getter_AddRefs(owner));
    nsCOMPtr<nsIPrincipal> principal;
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIScriptSecurityManager> securityManager;
    securityManager = do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv))
        return rv;

    PRBool useSandbox = PR_TRUE;

    if (owner) {
        principal = do_QueryInterface(owner, &rv);
        NS_ASSERTION(principal, "Channel's owner is not a principal");
        if (!principal)
            return NS_ERROR_FAILURE;

        //-- Don't run if the script principal is different from the
        //   principal of the context, with two exceptions: we allow
        //   the script to run if the script has the system principal
        //   or the context is about:blank.
        nsCOMPtr<nsIPrincipal> objectPrincipal;
        rv = securityManager->GetObjectPrincipal(
                                (JSContext*)scriptContext->GetNativeContext(),
                                globalJSObject,
                                getter_AddRefs(objectPrincipal));
        if (NS_FAILED(rv))
            return rv;

        nsCOMPtr<nsIPrincipal> systemPrincipal;
        securityManager->GetSystemPrincipal(getter_AddRefs(systemPrincipal));
        if (principal != systemPrincipal) {
            rv = securityManager->CheckSameOriginPrincipal(principal,
                                                           objectPrincipal);
            if (NS_SUCCEEDED(rv)) {
                useSandbox = PR_FALSE;
            }
        } else {
            useSandbox = PR_FALSE;
        }
    }

    nsString result;
    PRBool isUndefined;

    // Finally, we have everything needed to evaluate the expression.

    if (useSandbox) {
        // No owner from channel, or we have a principal
        // mismatch. Evaluate the javascript URL in a sandbox to
        // prevent it from accessing data it doesn't have permissions
        // to access.

        // First check to make sure it's OK to evaluate this script to
        // start with.  For example, script could be disabled.
        nsCOMPtr<nsIPrincipal> enabledCheckPrincipal = principal;
        if (!enabledCheckPrincipal) {
            // We just need a principal that's not the system principal and
            // isn't whitelisted by CanExecuteScripts.  An about:blank
            // principal will do nicely.
            nsCOMPtr<nsIURI> uri;
            rv = NS_NewURI(getter_AddRefs(uri), "about:blank");
            NS_ENSURE_SUCCESS(rv, rv);
            rv = securityManager->
                GetCodebasePrincipal(uri,
                                     getter_AddRefs(enabledCheckPrincipal));
            NS_ENSURE_SUCCESS(rv, rv);
        }

        JSContext *cx = (JSContext*)scriptContext->GetNativeContext();

        PRBool ok;
        rv = securityManager->CanExecuteScripts(cx, enabledCheckPrincipal,
                                                &ok);
        if (NS_FAILED(rv)) {
            return rv;
        }

        if (!ok) {
            // Treat this as returning undefined from the script.  That's what
            // nsJSContext does.
            return NS_ERROR_DOM_RETVAL_UNDEFINED;
        }

        nsIXPConnect *xpc = nsContentUtils::XPConnect();
        nsCOMPtr<nsIXPConnect_MOZILLA_1_8_BRANCH> xpc_18 =
            do_QueryInterface(xpc);

        nsCOMPtr<nsIXPConnectJSObjectHolder> sandbox;
        rv = xpc_18->CreateSandbox(cx, principal, getter_AddRefs(sandbox));
        NS_ENSURE_SUCCESS(rv, rv);

        jsval rval = JSVAL_VOID;
        nsAutoGCRoot root(&rval, &rv);
        if (NS_FAILED(rv)) {
            return rv;
        }

        // Push our JSContext on the context stack so the JS_ValueToString call
        // (and JS_ReportPendingException, if relevant) will use the principal
        // of cx.  Note that we do this as late as possible to make popping
        // simpler.
        nsCOMPtr<nsIJSContextStack> stack =
            do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
        if (NS_SUCCEEDED(rv)) {
            rv = stack->Push(cx);
        }
        if (NS_FAILED(rv)) {
            return rv;
        }    

        rv = xpc_18->EvalInSandboxObject(NS_ConvertUTF8toUTF16(script), cx,
                                         sandbox, &rval);

        // Propagate and report exceptions that happened in the
        // sandbox.
        if (JS_IsExceptionPending(cx)) {
            JS_ReportPendingException(cx);
        }

        isUndefined = rval == JSVAL_VOID;

        if (!isUndefined && NS_SUCCEEDED(rv)) {
            JSString *str = JS_ValueToString(cx, rval);
            if (!str) {
                // Report any pending exceptions.
                if (JS_IsExceptionPending(cx)) {
                    JS_ReportPendingException(cx);
                }

                // We don't know why this failed, so just use a
                // generic error code. It'll be translated to a
                // different one below anyways.
                rv = NS_ERROR_FAILURE;
            } else {
                result = nsDependentJSString(str);
            }
        }

        stack->Pop(nsnull);
    } else {
        // No need to use the sandbox, evaluate the script directly in
        // the given scope.
        rv = scriptContext->EvaluateString(NS_ConvertUTF8toUTF16(script),
                                           globalJSObject, // obj
                                           principal,
                                           url.get(),      // url
                                           1,              // line no
                                           nsnull,
                                           &result,
                                           &isUndefined);
    }

    if (NS_FAILED(rv)) {
        rv = NS_ERROR_MALFORMED_URI;
    }
    else if (isUndefined) {
        rv = NS_ERROR_DOM_RETVAL_UNDEFINED;
    }
    else {
        char *bytes;
        PRUint32 bytesLen;
        NS_NAMED_LITERAL_CSTRING(isoCharset, "ISO-8859-1");
        NS_NAMED_LITERAL_CSTRING(utf8Charset, "UTF-8");
        const nsCString *charset;
        if (IsISO88591(result)) {
            // For compatibility, if the result is ISO-8859-1, we use
            // ISO-8859-1, so that people can compatibly create images
            // using javascript: URLs.
            bytes = ToNewCString(result);
            bytesLen = result.Length();
            charset = &isoCharset;
        }
        else {
            bytes = ToNewUTF8String(result, &bytesLen);
            charset = &utf8Charset;
        }
        aChannel->SetContentCharset(*charset);
        if (bytes) {
            rv = NS_NewByteInputStream(getter_AddRefs(mInnerStream),
                                       bytes, bytesLen);
            if (mInnerStream) {
                nsCOMPtr<nsIStringInputStream> sis
                    = do_QueryInterface(mInnerStream);
                sis->AdoptData(bytes, bytesLen); // Previous call was |ShareData|
            }
        }
        else
            rv = NS_ERROR_OUT_OF_MEMORY;
    }

    return rv;
}

nsresult nsJSThunk::BringUpConsole(nsIDOMWindow *aDomWindow)
{
    nsresult rv;

    // First, get the Window Mediator service.
    nsCOMPtr<nsIWindowMediator> windowMediator =
        do_GetService(kWindowMediatorCID, &rv);

    if (NS_FAILED(rv)) return rv;

    // Next, find out whether there's a console already open.
    nsCOMPtr<nsIDOMWindowInternal> console;
    rv = windowMediator->GetMostRecentWindow(NS_LITERAL_STRING("global:console").get(),
                                             getter_AddRefs(console));
    if (NS_FAILED(rv)) return rv;

    if (console) {
        // If the console is already open, bring it to the top.
        rv = console->Focus();
    } else {
        nsCOMPtr<nsIJSConsoleService> jsconsole;

        jsconsole = do_GetService("@mozilla.org/embedcomp/jsconsole-service;1", &rv);
        if (NS_FAILED(rv) || !jsconsole) return rv;
        jsconsole->Open(aDomWindow);
    }
    return rv;
}

////////////////////////////////////////////////////////////////////////////////

class nsJSChannel : public nsIChannel
{
public:
    nsJSChannel();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL

    nsresult Init(nsIURI *aURI);

protected:
    virtual ~nsJSChannel();

    nsresult StopAll();

    nsresult InternalOpen(PRBool aIsAsync, nsIStreamListener *aListener,
                          nsISupports *aContext, nsIInputStream **aResult);

protected:
    nsCOMPtr<nsIChannel>    mStreamChannel;

    nsLoadFlags             mLoadFlags;

    nsRefPtr<nsJSThunk>     mIOThunk;
    PRPackedBool            mIsActive;
    PRPackedBool            mWasCanceled;
};

nsJSChannel::nsJSChannel() :
    mLoadFlags(LOAD_NORMAL),
    mIsActive(PR_FALSE),
    mWasCanceled(PR_FALSE)
{
}

nsJSChannel::~nsJSChannel()
{
}

nsresult nsJSChannel::StopAll()
{
    nsresult rv = NS_ERROR_UNEXPECTED;
    nsCOMPtr<nsIWebNavigation> webNav;
    NS_QueryNotificationCallbacks(mStreamChannel, webNav);

    NS_ASSERTION(webNav, "Can't get nsIWebNavigation from channel!");
    if (webNav) {
        rv = webNav->Stop(nsIWebNavigation::STOP_ALL);
    }

    return rv;
}

nsresult nsJSChannel::Init(nsIURI *aURI)
{
    nsresult rv;

    // Create the nsIStreamIO layer used by the nsIStreamIOChannel.
    mIOThunk = new nsJSThunk();
    if (!mIOThunk)
        return NS_ERROR_OUT_OF_MEMORY;

    // Create a stock input stream channel...
    // Remember, until AsyncOpen is called, the script will not be evaluated
    // and the underlying Input Stream will not be created...
    nsCOMPtr<nsIChannel> channel;

    // If the resultant script evaluation actually does return a value, we
    // treat it as html.
    rv = NS_NewInputStreamChannel(getter_AddRefs(channel), aURI, mIOThunk,
                                  NS_LITERAL_CSTRING("text/html"));
    if (NS_FAILED(rv)) return rv;

    rv = mIOThunk->Init(aURI);
    if (NS_SUCCEEDED(rv)) {
        mStreamChannel = channel;
    }

    return rv;
}

//
// nsISupports implementation...
//

NS_IMPL_ADDREF(nsJSChannel)
NS_IMPL_RELEASE(nsJSChannel)

NS_INTERFACE_MAP_BEGIN(nsJSChannel)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIChannel)
    NS_INTERFACE_MAP_ENTRY(nsIRequest)
    NS_INTERFACE_MAP_ENTRY(nsIChannel)
NS_INTERFACE_MAP_END

//
// nsIRequest implementation...
//

NS_IMETHODIMP
nsJSChannel::GetName(nsACString &aResult)
{
    return mStreamChannel->GetName(aResult);
}

NS_IMETHODIMP
nsJSChannel::IsPending(PRBool *aResult)
{
    *aResult = mIsActive;
    return NS_OK;
}

NS_IMETHODIMP
nsJSChannel::GetStatus(nsresult *aResult)
{
    // We're always ok. Our status is independent of our underlying
    // stream's status.
    *aResult = NS_OK;
    return NS_OK;
}

NS_IMETHODIMP
nsJSChannel::Cancel(nsresult aStatus)
{
    // If we're canceled just record the fact that we were canceled,
    // the underlying stream will be canceled later, if needed. And we
    // don't care about the reason for the canceling, i.e. ignore
    // aStatus.

    mWasCanceled = PR_TRUE;

    return NS_OK;
}

NS_IMETHODIMP
nsJSChannel::Suspend()
{
    return mStreamChannel->Suspend();
}

NS_IMETHODIMP
nsJSChannel::Resume()
{
    return mStreamChannel->Resume();
}

//
// nsIChannel implementation
//

NS_IMETHODIMP
nsJSChannel::GetOriginalURI(nsIURI * *aURI)
{
    return mStreamChannel->GetOriginalURI(aURI);
}

NS_IMETHODIMP
nsJSChannel::SetOriginalURI(nsIURI *aURI)
{
    return mStreamChannel->SetOriginalURI(aURI);
}

NS_IMETHODIMP
nsJSChannel::GetURI(nsIURI * *aURI)
{
    return mStreamChannel->GetURI(aURI);
}

NS_IMETHODIMP
nsJSChannel::Open(nsIInputStream **aResult)
{
    return InternalOpen(PR_FALSE, nsnull, nsnull, aResult);
}

NS_IMETHODIMP
nsJSChannel::AsyncOpen(nsIStreamListener *aListener, nsISupports *aContext)
{
    return InternalOpen(PR_TRUE, aListener, aContext, nsnull);
}

nsresult
nsJSChannel::InternalOpen(PRBool aIsAsync, nsIStreamListener *aListener,
                          nsISupports *aContext, nsIInputStream **aResult)
{
    nsCOMPtr<nsILoadGroup> loadGroup;

    // Temporarily set the LOAD_BACKGROUND flag to suppress load group observer
    // notifications (and hence nsIWebProgressListener notifications) from
    // being dispatched.  This is required since we suppress LOAD_DOCUMENT_URI,
    // which means that the DocLoader would not generate document start and
    // stop notifications (see bug 257875).
    PRUint32 oldLoadFlags = mLoadFlags;
    mLoadFlags |= LOAD_BACKGROUND;

    // Add the javascript channel to its loadgroup so that we know if
    // network loads were canceled or not...
    mStreamChannel->GetLoadGroup(getter_AddRefs(loadGroup));
    if (loadGroup) {
        loadGroup->AddRequest(this, aContext);
    }

    // Synchronously execute the script...
    // mIsActive is used to indicate the the request is 'busy' during the
    // the script evaluation phase.  This means that IsPending() will 
    // indicate the the request is busy while the script is executing...
    mIsActive = PR_TRUE;
    nsresult rv = mIOThunk->EvaluateScript(mStreamChannel);

    // Remove the javascript channel from its loadgroup...
    if (loadGroup) {
        loadGroup->RemoveRequest(this, aContext, rv);
    }

    // Reset load flags to their original value...
    mLoadFlags = oldLoadFlags;

    // We're no longer active, it's now up to the stream channel to do
    // the loading, if needed.
    mIsActive = PR_FALSE;

    if (NS_SUCCEEDED(rv) && !mWasCanceled) {
        // EvaluateScript() succeeded, and we were not canceled, that
        // means there's data to parse as a result of evaluating the
        // script.

        // Get the stream channels load flags (!= mLoadFlags).
        nsLoadFlags loadFlags;
        mStreamChannel->GetLoadFlags(&loadFlags);

        if (loadFlags & LOAD_DOCUMENT_URI) {
            // We're loaded as the document channel. If we go on,
            // we'll blow away the current document. Make sure that's
            // ok. If so, stop all pending network loads.

            nsCOMPtr<nsIDocShell> docShell;
            NS_QueryNotificationCallbacks(mStreamChannel, docShell);
            if (docShell) {
                nsCOMPtr<nsIContentViewer> cv;
                docShell->GetContentViewer(getter_AddRefs(cv));

                if (cv) {
                    PRBool okToUnload;

                    if (NS_SUCCEEDED(cv->PermitUnload(&okToUnload)) &&
                        !okToUnload) {
                        // The user didn't want to unload the current
                        // page, translate this into an undefined
                        // return from the javascript: URL...
                        rv = NS_ERROR_DOM_RETVAL_UNDEFINED;
                    }
                }
            }

            if (NS_SUCCEEDED(rv)) {
                rv = StopAll();
            }
        }

        if (NS_SUCCEEDED(rv)) {
            // This will add mStreamChannel to the load group.

            if (aIsAsync) {
                rv = mStreamChannel->AsyncOpen(aListener, aContext);
            } else {
                rv = mStreamChannel->Open(aResult);
            }
        }
    }

    if (NS_FAILED(rv)) {
        // Propagate the failure down to the underlying channel...
        mStreamChannel->Cancel(rv);
    }

    return rv;
}

NS_IMETHODIMP
nsJSChannel::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
    *aLoadFlags = mLoadFlags;

    return NS_OK;
}

NS_IMETHODIMP
nsJSChannel::SetLoadFlags(nsLoadFlags aLoadFlags)
{
    // Since the javascript channel is never the actual channel that
    // any data is loaded through, don't ever set the
    // LOAD_DOCUMENT_URI flag on it, since that could lead to two
    // 'document channels' in the loadgroup if a javascript: URL is
    // loaded while a document is being loaded in the same window.

    mLoadFlags = aLoadFlags & ~LOAD_DOCUMENT_URI;

    // ... but the underlying stream channel should get this bit, if
    // set, since that'll be the real document channel if the
    // javascript: URL generated data.

    return mStreamChannel->SetLoadFlags(aLoadFlags);
}

NS_IMETHODIMP
nsJSChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
    return mStreamChannel->GetLoadGroup(aLoadGroup);
}

NS_IMETHODIMP
nsJSChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
    return mStreamChannel->SetLoadGroup(aLoadGroup);
}

NS_IMETHODIMP
nsJSChannel::GetOwner(nsISupports* *aOwner)
{
    return mStreamChannel->GetOwner(aOwner);
}

NS_IMETHODIMP
nsJSChannel::SetOwner(nsISupports* aOwner)
{
    return mStreamChannel->SetOwner(aOwner);
}

NS_IMETHODIMP
nsJSChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aCallbacks)
{
    return mStreamChannel->GetNotificationCallbacks(aCallbacks);
}

NS_IMETHODIMP
nsJSChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aCallbacks)
{
    return mStreamChannel->SetNotificationCallbacks(aCallbacks);
}

NS_IMETHODIMP 
nsJSChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    return mStreamChannel->GetSecurityInfo(aSecurityInfo);
}

NS_IMETHODIMP
nsJSChannel::GetContentType(nsACString &aContentType)
{
    return mStreamChannel->GetContentType(aContentType);
}

NS_IMETHODIMP
nsJSChannel::SetContentType(const nsACString &aContentType)
{
    return mStreamChannel->SetContentType(aContentType);
}

NS_IMETHODIMP
nsJSChannel::GetContentCharset(nsACString &aContentCharset)
{
    return mStreamChannel->GetContentCharset(aContentCharset);
}

NS_IMETHODIMP
nsJSChannel::SetContentCharset(const nsACString &aContentCharset)
{
    return mStreamChannel->SetContentCharset(aContentCharset);
}

NS_IMETHODIMP
nsJSChannel::GetContentLength(PRInt32 *aContentLength)
{
    return mStreamChannel->GetContentLength(aContentLength);
}

NS_IMETHODIMP
nsJSChannel::SetContentLength(PRInt32 aContentLength)
{
    return mStreamChannel->SetContentLength(aContentLength);
}


////////////////////////////////////////////////////////////////////////////////

nsJSProtocolHandler::nsJSProtocolHandler()
{
}

nsresult
nsJSProtocolHandler::Init()
{
    return NS_OK;
}

nsJSProtocolHandler::~nsJSProtocolHandler()
{
}

NS_IMPL_ISUPPORTS1(nsJSProtocolHandler, nsIProtocolHandler)

NS_METHOD
nsJSProtocolHandler::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsJSProtocolHandler* ph = new nsJSProtocolHandler();
    if (!ph)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(ph);
    nsresult rv = ph->Init();
    if (NS_SUCCEEDED(rv)) {
        rv = ph->QueryInterface(aIID, aResult);
    }
    NS_RELEASE(ph);
    return rv;
}

nsresult 
nsJSProtocolHandler::EnsureUTF8Spec(const nsAFlatCString &aSpec, const char *aCharset, 
                                    nsACString &aUTF8Spec)
{
  aUTF8Spec.Truncate();

  nsresult rv;
  
  if (!mTextToSubURI) {
    mTextToSubURI = do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  nsAutoString uStr;
  rv = mTextToSubURI->UnEscapeNonAsciiURI(nsDependentCString(aCharset), aSpec, uStr);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!IsASCII(uStr))
    NS_EscapeURL(NS_ConvertUCS2toUTF8(uStr), esc_AlwaysCopy | esc_OnlyNonASCII, aUTF8Spec);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:

NS_IMETHODIMP
nsJSProtocolHandler::GetScheme(nsACString &result)
{
    result = "javascript";
    return NS_OK;
}

NS_IMETHODIMP
nsJSProtocolHandler::GetDefaultPort(PRInt32 *result)
{
    *result = -1;        // no port for javascript: URLs
    return NS_OK;
}

NS_IMETHODIMP
nsJSProtocolHandler::GetProtocolFlags(PRUint32 *result)
{
    *result = URI_NORELATIVE | URI_NOAUTH;
    return NS_OK;
}

NS_IMETHODIMP
nsJSProtocolHandler::NewURI(const nsACString &aSpec,
                            const char *aCharset, // ignore charset info
                            nsIURI *aBaseURI,
                            nsIURI **result)
{
    nsresult rv;

    // javascript: URLs (currently) have no additional structure beyond that
    // provided by standard URLs, so there is no "outer" object given to
    // CreateInstance.

    nsIURI* url;
    rv = CallCreateInstance(kSimpleURICID, &url);

    if (NS_FAILED(rv))
        return rv;

    if (!aCharset || !nsCRT::strcasecmp("UTF-8", aCharset))
      rv = url->SetSpec(aSpec);
    else {
      nsCAutoString utf8Spec;
      rv = EnsureUTF8Spec(PromiseFlatCString(aSpec), aCharset, utf8Spec);
      if (NS_SUCCEEDED(rv)) {
        if (utf8Spec.IsEmpty())
          rv = url->SetSpec(aSpec);
        else
          rv = url->SetSpec(utf8Spec);
      }
    }

    if (NS_FAILED(rv)) {
        NS_RELEASE(url);
        return rv;
    }

    *result = url;
    return rv;
}

NS_IMETHODIMP
nsJSProtocolHandler::NewChannel(nsIURI* uri, nsIChannel* *result)
{
    nsresult rv;
    nsJSChannel * channel;

    NS_ENSURE_ARG_POINTER(uri);

    channel = new nsJSChannel();
    if (!channel) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(channel);

    rv = channel->Init(uri);
    if (NS_SUCCEEDED(rv)) {
        *result = channel;
        NS_ADDREF(*result);
    }
    NS_RELEASE(channel);
    return rv;
}

NS_IMETHODIMP 
nsJSProtocolHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    // don't override anything.  
    *_retval = PR_FALSE;
    return NS_OK;
}

