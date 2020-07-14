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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
#ifndef nsGenericElement_h___
#define nsGenericElement_h___

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsIXMLContent.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOM3EventTarget.h"
#include "nsIDOM3Node.h"
#include "nsIDOMNSEventTarget.h"
#include "nsILinkHandler.h"
#include "nsGenericDOMNodeList.h"
#include "nsIEventListenerManager.h"
#include "nsContentUtils.h"
#include "pldhash.h"
#include "nsAttrAndChildArray.h"

class nsIDOMAttr;
class nsIDOMEventListener;
class nsIFrame;
class nsISupportsArray;
class nsIDOMNamedNodeMap;
class nsDOMCSSDeclaration;
class nsIDOMCSSStyleDeclaration;
class nsDOMAttributeMap;
class nsIURI;
class nsVoidArray;
class nsINodeInfo;
class nsIControllers;
class nsIDOMNSFeatureFactory;

typedef unsigned long PtrBits;

/**
 * This bit will be set if the nsGenericElement doesn't have nsDOMSlots
 */
#define GENERIC_ELEMENT_DOESNT_HAVE_DOMSLOTS   0x00000001U

/**
 * This bit will be set if the element has a range list in the range list hash
 */
#define GENERIC_ELEMENT_HAS_RANGELIST          0x00000002U

/**
 * This bit will be set if the element has a listener manager in the listener
 * manager hash
 */
#define GENERIC_ELEMENT_HAS_LISTENERMANAGER    0x00000004U

/** Whether this content is anonymous */
#define GENERIC_ELEMENT_IS_ANONYMOUS           0x00000008U

/** Whether this content has had any properties set on it */
#define GENERIC_ELEMENT_HAS_PROPERTIES         0x00000010U

/** Whether this content may have a frame */
#define GENERIC_ELEMENT_MAY_HAVE_FRAME         0x00000020U

/** Three bits are element type specific. */
#define ELEMENT_TYPE_SPECIFIC_BITS_OFFSET      6

/** The number of bits to shift the bit field to get at the content ID */
#define GENERIC_ELEMENT_CONTENT_ID_BITS_OFFSET 9

/** This mask masks out the bits that are used for the content ID */
#define GENERIC_ELEMENT_CONTENT_ID_MASK \
  ((~PtrBits(0)) << GENERIC_ELEMENT_CONTENT_ID_BITS_OFFSET)

/**
 * The largest value for content ID that fits in
 * GENERIC_ELEMENT_CONTENT_ID_MASK
 */
#define GENERIC_ELEMENT_CONTENT_ID_MAX_VALUE \
  ((PRUint32)((~PtrBits(0)) >> GENERIC_ELEMENT_CONTENT_ID_BITS_OFFSET))


#define PARENT_BIT_INDOCUMENT ((PtrBits)0x1 << 0)

/**
 * Class that implements the nsIDOMNodeList interface (a list of children of
 * the content), by holding a reference to the content and delegating GetLength
 * and Item to its existing child list.
 * @see nsIDOMNodeList
 */
class nsChildContentList : public nsGenericDOMNodeList 
{
public:
  /**
   * @param aContent the content whose children will make up the list
   */
  nsChildContentList(nsIContent *aContent);
  virtual ~nsChildContentList();

  // nsIDOMNodeList interface
  NS_DECL_NSIDOMNODELIST
  
  /** Drop the reference to the content */
  void DropReference();

private:
  /** The content whose children make up the list (weak reference) */
  nsIContent *mContent;
};

/**
 * There are a set of DOM- and scripting-specific instance variables
 * that may only be instantiated when a content object is accessed
 * through the DOM. Rather than burn actual slots in the content
 * objects for each of these instance variables, we put them off
 * in a side structure that's only allocated when the content is
 * accessed through the DOM.
 */
class nsDOMSlots
{
public:
  nsDOMSlots(PtrBits aFlags);
  ~nsDOMSlots();

  PRBool IsEmpty();

  PtrBits mFlags;

  /**
   * An object implementing nsIDOMNodeList for this content (childNodes)
   * @see nsIDOMNodeList
   * @see nsGenericHTMLElement::GetChildNodes
   */
  nsRefPtr<nsChildContentList> mChildNodes;

  /**
   * The .style attribute (an interface that forwards to the actual
   * style rules)
   * @see nsGenericHTMLElement::GetStyle */
  nsRefPtr<nsDOMCSSDeclaration> mStyle;

  /**
   * An object implementing nsIDOMNamedNodeMap for this content (attributes)
   * @see nsGenericElement::GetAttributes
   */
  nsRefPtr<nsDOMAttributeMap> mAttributeMap;

  union {
    /**
    * The nearest enclosing content node with a binding that created us.
    * @see nsGenericElement::GetBindingParent
    */
    nsIContent* mBindingParent;  // [Weak]

    /**
    * The controllers of the XUL Element.
    */
    nsIControllers* mControllers; // [OWNER]
  };

  // DEPRECATED, DON'T USE THIS
  PRUint32 mContentID;
};

class RangeListMapEntry : public PLDHashEntryHdr
{
public:
  RangeListMapEntry(const void *aKey)
    : mKey(aKey), mRangeList(nsnull)
  {
  }

  ~RangeListMapEntry()
  {
    delete mRangeList;
  }

private:
  const void *mKey; // must be first to look like PLDHashEntryStub

public:
  // We want mRangeList to be an nsAutoVoidArray but we can't make an
  // nsAutoVoidArray a direct member of RangeListMapEntry since it
  // will be moved around in memory, and nsAutoVoidArray can't deal
  // with that.
  nsVoidArray *mRangeList;
};

class EventListenerManagerMapEntry : public PLDHashEntryHdr
{
public:
  EventListenerManagerMapEntry(const void *aKey)
    : mKey(aKey)
  {
  }

  ~EventListenerManagerMapEntry()
  {
    NS_ASSERTION(!mListenerManager, "caller must release and disconnect ELM");
  }

private:
  const void *mKey; // must be first, to look like PLDHashEntryStub

public:
  nsCOMPtr<nsIEventListenerManager> mListenerManager;
};


/**
 * A tearoff class for nsGenericElement to implement the nsIDOM3Node functions
 */
class nsNode3Tearoff : public nsIDOM3Node
{
public:
  NS_DECL_ISUPPORTS

  NS_DECL_NSIDOM3NODE

  nsNode3Tearoff(nsIContent *aContent) : mContent(aContent)
  {
  }

  static nsresult GetTextContent(nsIContent *aContent,
                                 nsAString &aTextContent);

  static nsresult SetTextContent(nsIContent *aContent,
                                 const nsAString &aTextContent);

protected:
  virtual ~nsNode3Tearoff() {};

private:
  nsCOMPtr<nsIContent> mContent;
};


#define NS_EVENT_TEAROFF_CACHE_SIZE 4

/**
 * nsDOMEventRTTearoff is a tearoff class used by nsGenericElement and
 * nsGenericDOMDataNode classes for implementing the interfaces
 * nsIDOMEventReceiver and nsIDOMEventTarget
 *
 * Use the method nsDOMEventRTTearoff::Create() to create one of these babies.
 * @see nsDOMEventRTTearoff::Create
 */

class nsDOMEventRTTearoff : public nsIDOMEventReceiver,
                            public nsIDOM3EventTarget,
                            public nsIDOMNSEventTarget
{
private:
  // This class uses a caching scheme so we don't let users of this
  // class create new instances with 'new', in stead the callers
  // should use the static method
  // nsDOMEventRTTearoff::Create(). That's why the constructor and
  // destrucor of this class is private.

  nsDOMEventRTTearoff(nsIContent *aContent);

  static nsDOMEventRTTearoff *mCachedEventTearoff[NS_EVENT_TEAROFF_CACHE_SIZE];
  static PRUint32 mCachedEventTearoffCount;

  /**
   * This method gets called by Release() when it's time to delete the
   * this object, in stead of always deleting the object we'll put the
   * object in the cache if unless the cache is already full.
   */
  void LastRelease();

  nsresult GetEventReceiver(nsIDOMEventReceiver **aReceiver);
  nsresult GetDOM3EventTarget(nsIDOM3EventTarget **aTarget);

public:
  virtual ~nsDOMEventRTTearoff();

  /**
   * Use this static method to create instances of nsDOMEventRTTearoff.
   * @param aContent the content to create a tearoff for
   */
  static nsDOMEventRTTearoff *Create(nsIContent *aContent);

  /**
   * Call before shutdown to clear the cache and free memory for this class.
   */
  static void Shutdown();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMEventTarget
  NS_DECL_NSIDOMEVENTTARGET

  // nsIDOM3EventTarget
  NS_DECL_NSIDOM3EVENTTARGET

  // nsIDOMEventReceiver
  NS_IMETHOD AddEventListenerByIID(nsIDOMEventListener *aListener,
                                   const nsIID& aIID);
  NS_IMETHOD RemoveEventListenerByIID(nsIDOMEventListener *aListener,
                                      const nsIID& aIID);
  NS_IMETHOD GetListenerManager(nsIEventListenerManager** aResult);
  NS_IMETHOD HandleEvent(nsIDOMEvent *aEvent);
  NS_IMETHOD GetSystemEventGroup(nsIDOMEventGroup** aGroup);

  // nsIDOMNSEventTarget
  NS_DECL_NSIDOMNSEVENTTARGET

private:
  /**
   * Strong reference back to the content object from where an instance of this
   * class was 'torn off'
   */
  nsCOMPtr<nsIContent> mContent;
};

/**
 * Class used to detect unexpected mutations. To use the class create an
 * nsMutationGuard on the stack before unexpected mutations could occur.
 * You can then at any time call Mutated to check if any unexpected mutations
 * have occured.
 *
 * When a guard is instantiated sMutationCount is set to 300. It is then
 * decremented by every mutation (capped at 0). This means that we can only
 * detect 300 mutations during the lifetime of a single guard, however that
 * should be more then we ever care about as we usually only care if more then
 * one mutation has occured.
 *
 * When the guard goes out of scope it will adjust sMutationCount so that over
 * the lifetime of the guard the guard itself has not affected sMutationCount,
 * while mutations that happened while the guard was alive still will. This
 * allows a guard to be instantiated even if there is another guard higher up
 * on the callstack watching for mutations.
 *
 * The only thing that has to be avoided is for an outer guard to be used
 * while an inner guard is alive. This can be avoided by only ever
 * instantiating a single guard per scope and only using the guard in the
 * current scope.
 */
class nsMutationGuard {
public:
  nsMutationGuard()
  {
    mDelta = eMaxMutations - sMutationCount;
    sMutationCount = eMaxMutations;
  }
  ~nsMutationGuard()
  {
    sMutationCount =
      mDelta > sMutationCount ? 0 : sMutationCount - mDelta;
  }

  /**
   * Returns true if any unexpected mutations have occured. You can pass in
   * an 8-bit ignore count to ignore a number of expected mutations.
   */
  PRBool Mutated(PRUint8 aIgnoreCount)
  {
    return sMutationCount < NS_STATIC_CAST(PRUint32, eMaxMutations - aIgnoreCount);
  }

  // This function should be called whenever a mutation that we want to keep
  // track of happen. For now this is only done when children are added or
  // removed, but we might do it for attribute changes too in the future.
  static void DidMutate()
  {
    if (sMutationCount) {
      --sMutationCount;
    }
  }

private:
  // mDelta is the amount sMutationCount was adjusted when the guard was
  // initialized. It is needed so that we can undo that adjustment once
  // the guard dies.
  PRUint32 mDelta;

  // The value 300 is not important, as long as it is bigger then anything
  // ever passed to Mutated().
  enum { eMaxMutations = 300 };

  
  // sMutationCount is a global mutation counter which is decreased by one at
  // every mutation. It is capped at 0 to avoid wrapping.
  // It's value is always between 0 and 300, inclusive.
  static PRUint32 sMutationCount;
};

/**
 * A generic base class for DOM elements, implementing many nsIContent,
 * nsIDOMNode and nsIDOMElement methods.
 */
class nsGenericElement : public nsIXMLContent
{
public:
  nsGenericElement(nsINodeInfo *aNodeInfo);
  virtual ~nsGenericElement();

  NS_DECL_ISUPPORTS

  /**
   * Called during QueryInterface to give the binding manager a chance to
   * get an interface for this element.
   */
  nsresult PostQueryInterface(REFNSIID aIID, void** aInstancePtr);

  /** Free globals, to be called from module destructor */
  static void Shutdown();

  // nsIDOMGCParticipant interface methods
  virtual nsIDOMGCParticipant* GetSCCIndex();
  virtual void AppendReachableList(nsCOMArray<nsIDOMGCParticipant>& aArray);

  // nsIContent interface methods
  nsIDocument* GetDocument() const
  {
    return IsInDoc() ? GetOwnerDoc() : nsnull;
  }
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              PRBool aCompileEventHandlers);
  virtual void UnbindFromTree(PRBool aDeep = PR_TRUE,
                              PRBool aNullParent = PR_TRUE);
  PRBool IsInDoc() const
  {
    return mParentPtrBits & PARENT_BIT_INDOCUMENT;
  }
  nsIDocument *GetOwnerDoc() const
  {
    return nsContentUtils::GetDocument(mNodeInfo);
  }
  virtual PRBool IsNativeAnonymous() const;
  virtual void SetNativeAnonymous(PRBool aAnonymous);
  virtual PRInt32 GetNameSpaceID() const;
  virtual nsIAtom *Tag() const;
  virtual nsINodeInfo *GetNodeInfo() const;
  virtual PRUint32 GetChildCount() const;
  virtual nsIContent *GetChildAt(PRUint32 aIndex) const;
  virtual PRInt32 IndexOf(nsIContent* aPossibleChild) const;
  virtual nsresult InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                 PRBool aNotify);
  virtual nsresult AppendChildTo(nsIContent* aKid, PRBool aNotify);
  virtual nsresult RemoveChildAt(PRUint32 aIndex, PRBool aNotify);
  virtual nsIAtom *GetIDAttributeName() const;
  virtual nsIAtom *GetClassAttributeName() const;
  virtual already_AddRefed<nsINodeInfo> GetExistingAttrNameFromQName(const nsAString& aStr) const;
  nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                   const nsAString& aValue, PRBool aNotify)
  {
    return SetAttr(aNameSpaceID, aName, nsnull, aValue, aNotify);
  }
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, nsIAtom* aPrefix,
                           const nsAString& aValue, PRBool aNotify);
  virtual nsresult GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                           nsAString& aResult) const;
  virtual PRBool HasAttr(PRInt32 aNameSpaceID, nsIAtom* aName) const;
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             PRBool aNotify);
  virtual nsresult GetAttrNameAt(PRUint32 aIndex, PRInt32* aNameSpaceID,
                                 nsIAtom** aName, nsIAtom** aPrefix) const;
  virtual PRUint32 GetAttrCount() const;
  virtual nsresult RangeAdd(nsIDOMRange* aRange);
  virtual void RangeRemove(nsIDOMRange* aRange);
  virtual const nsVoidArray *GetRangeList() const;
  virtual nsresult HandleDOMEvent(nsPresContext* aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus* aEventStatus);
  virtual PRUint32 ContentID() const;
  virtual void SetContentID(PRUint32 aID);
  virtual void SetFocus(nsPresContext* aContext);
  virtual nsIContent *GetBindingParent() const;
  virtual PRBool IsContentOfType(PRUint32 aFlags) const;
  virtual nsresult GetListenerManager(nsIEventListenerManager** aResult);
  virtual already_AddRefed<nsIURI> GetBaseURI() const;
  virtual void* GetProperty(nsIAtom  *aPropertyName,
                            nsresult *aStatus = nsnull) const;
  virtual nsresult SetProperty(nsIAtom            *aPropertyName,
                               void               *aValue,
                               NSPropertyDtorFunc  aDtor);
  virtual nsresult DeleteProperty(nsIAtom  *aPropertyName);
  virtual void*    UnsetProperty(nsIAtom *aPropertyName,
                                 nsresult *aStatus = nsnull);
  virtual void SetMayHaveFrame(PRBool aMayHaveFrame);
  virtual PRBool MayHaveFrame() const;

#ifdef DEBUG
  virtual void List(FILE* out, PRInt32 aIndent) const;
  virtual void DumpContent(FILE* out, PRInt32 aIndent,PRBool aDumpAll) const;
#endif

  // nsIStyledContent interface methods
  virtual nsIAtom* GetID() const;
  virtual const nsAttrValue* GetClasses() const;
  NS_IMETHOD_(PRBool) HasClass(nsIAtom* aClass, PRBool aCaseSensitive) const;
  NS_IMETHOD WalkContentStyleRules(nsRuleWalker* aRuleWalker);
  virtual nsICSSStyleRule* GetInlineStyleRule();
  NS_IMETHOD SetInlineStyleRule(nsICSSStyleRule* aStyleRule, PRBool aNotify);
  NS_IMETHOD_(PRBool)
    IsAttributeMapped(const nsIAtom* aAttribute) const;
  virtual nsChangeHint GetAttributeChangeHint(const nsIAtom* aAttribute, 
                                              PRInt32 aModType) const;
  /*
   * Attribute Mapping Helpers
   */
  struct MappedAttributeEntry {
    nsIAtom** attribute;
  };
  
  /**
   * A common method where you can just pass in a list of maps to check
   * for attribute dependence. Most implementations of
   * IsAttributeMapped should use this function as a default
   * handler.
   */
  static PRBool
  FindAttributeDependence(const nsIAtom* aAttribute,
                          const MappedAttributeEntry* const aMaps[],
                          PRUint32 aMapCount);

  // nsIXMLContent interface methods
  NS_IMETHOD MaybeTriggerAutoLink(nsIDocShell *aShell);

  // nsIDOMNode method implementation
  NS_IMETHOD GetNodeName(nsAString& aNodeName);
  NS_IMETHOD GetLocalName(nsAString& aLocalName);
  NS_IMETHOD GetNodeValue(nsAString& aNodeValue);
  NS_IMETHOD SetNodeValue(const nsAString& aNodeValue);
  NS_IMETHOD GetNodeType(PRUint16* aNodeType);
  NS_IMETHOD GetParentNode(nsIDOMNode** aParentNode);
  NS_IMETHOD GetAttributes(nsIDOMNamedNodeMap** aAttributes);
  NS_IMETHOD GetPreviousSibling(nsIDOMNode** aPreviousSibling);
  NS_IMETHOD GetNextSibling(nsIDOMNode** aNextSibling);
  NS_IMETHOD GetOwnerDocument(nsIDOMDocument** aOwnerDocument);
  NS_IMETHOD GetNamespaceURI(nsAString& aNamespaceURI);
  NS_IMETHOD GetPrefix(nsAString& aPrefix);
  NS_IMETHOD SetPrefix(const nsAString& aPrefix);
  NS_IMETHOD Normalize();
  NS_IMETHOD IsSupported(const nsAString& aFeature,
                         const nsAString& aVersion, PRBool* aReturn);
  NS_IMETHOD HasAttributes(PRBool* aHasAttributes);
  NS_IMETHOD GetChildNodes(nsIDOMNodeList** aChildNodes);
  NS_IMETHOD HasChildNodes(PRBool* aHasChildNodes);
  NS_IMETHOD GetFirstChild(nsIDOMNode** aFirstChild);
  NS_IMETHOD GetLastChild(nsIDOMNode** aLastChild);
  NS_IMETHOD InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild,
                          nsIDOMNode** aReturn);
  NS_IMETHOD ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,
                          nsIDOMNode** aReturn);
  NS_IMETHOD RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn);
  NS_IMETHOD AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
  {
    return InsertBefore(aNewChild, nsnull, aReturn);
  }

  // nsIDOMElement method implementation
  NS_IMETHOD GetTagName(nsAString& aTagName);
  NS_IMETHOD GetAttribute(const nsAString& aName,
                          nsAString& aReturn);
  NS_IMETHOD SetAttribute(const nsAString& aName,
                          const nsAString& aValue);
  NS_IMETHOD RemoveAttribute(const nsAString& aName);
  NS_IMETHOD GetAttributeNode(const nsAString& aName,
                              nsIDOMAttr** aReturn);
  NS_IMETHOD SetAttributeNode(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn);
  NS_IMETHOD RemoveAttributeNode(nsIDOMAttr* aOldAttr, nsIDOMAttr** aReturn);
  NS_IMETHOD GetElementsByTagName(const nsAString& aTagname,
                                  nsIDOMNodeList** aReturn);
  NS_IMETHOD GetAttributeNS(const nsAString& aNamespaceURI,
                            const nsAString& aLocalName,
                            nsAString& aReturn);
  NS_IMETHOD SetAttributeNS(const nsAString& aNamespaceURI,
                            const nsAString& aQualifiedName,
                            const nsAString& aValue);
  NS_IMETHOD RemoveAttributeNS(const nsAString& aNamespaceURI,
                               const nsAString& aLocalName);
  NS_IMETHOD GetAttributeNodeNS(const nsAString& aNamespaceURI,
                                const nsAString& aLocalName,
                                nsIDOMAttr** aReturn);
  NS_IMETHOD SetAttributeNodeNS(nsIDOMAttr* aNewAttr, nsIDOMAttr** aReturn);
  NS_IMETHOD GetElementsByTagNameNS(const nsAString& aNamespaceURI,
                                    const nsAString& aLocalName,
                                    nsIDOMNodeList** aReturn);
  NS_IMETHOD HasAttribute(const nsAString& aName, PRBool* aReturn);
  NS_IMETHOD HasAttributeNS(const nsAString& aNamespaceURI,
                            const nsAString& aLocalName,
                            PRBool* aReturn);

  //----------------------------------------

  /**
   * Add a script event listener with the given attr name (like onclick)
   * and with the value as JS
   * @param aAttribute the event listener name
   * @param aValue the JS to attach
   */
  nsresult AddScriptEventListener(nsIAtom* aAttribute,
                                  const nsAString& aValue);

  /**
   * Trigger a link with uri aLinkURI.  If aClick is false, this triggers a
   * mouseover on the link, otherwise it triggers a load, after doing a
   * security check.
   * @param aPresContext the pres context.
   * @param aVerb how the link will be loaded (replace page, new window, etc.)
   * @param aLinkURI the URI of the link
   * @param aTargetSpec the target (like target=, may be empty)
   * @param aClick whether this was a click or not (if false, it assumes you
   *        just hovered over the link)
   * @param aIsUserTriggered whether the user triggered the link.
   *        This would be false for loads from auto XLinks or from the
   *        click() method if we ever implement it.
   */
  nsresult TriggerLink(nsPresContext* aPresContext,
                       nsLinkVerb aVerb,
                       nsIURI* aLinkURI,
                       const nsAFlatString& aTargetSpec,
                       PRBool aClick,
                       PRBool aIsUserTriggered);
  /**
   * Do whatever needs to be done when the mouse leaves a link
   */
  nsresult LeaveLink(nsPresContext* aPresContext);

  /**
   * Take two text nodes and append the second to the first.
   * @param aFirst the node which will contain first + second [INOUT]
   * @param aSecond the node which will be appended
   */
  nsresult JoinTextNodes(nsIContent* aFirst,
                         nsIContent* aSecond);

  /**
   * Set .document in the immediate children of said content (but not in
   * content itself).  SetDocument() in the children will recursively call
   * this.
   *
   * @param aContent the content to get the children of
   * @param aDocument the document to set
   * @param aCompileEventHandlers whether to initialize the event handlers in
   *        the document (used by nsXULElement)
   */
  static void SetDocumentInChildrenOf(nsIContent* aContent, 
				      nsIDocument* aDocument, PRBool aCompileEventHandlers);

  /**
   * Check whether a spec feature/version is supported.
   * @param aObject the object, which should support the feature,
   *        for example nsIDOMNode or nsIDOMDOMImplementation
   * @param aFeature the feature ("Views", "Core", "HTML", "Range" ...)
   * @param aVersion the version ("1.0", "2.0", ...)
   * @param aReturn whether the feature is supported or not [OUT]
   */
  static nsresult InternalIsSupported(nsISupports* aObject,
                                      const nsAString& aFeature,
                                      const nsAString& aVersion,
                                      PRBool* aReturn);

  static nsresult InternalGetFeature(nsISupports* aObject,
                                     const nsAString& aFeature,
                                     const nsAString& aVersion,
                                     nsISupports** aReturn);
  
  static already_AddRefed<nsIDOMNSFeatureFactory>
    GetDOMFeatureFactory(const nsAString& aFeature, const nsAString& aVersion);

  /**
   * Quick helper to determine whether there are any mutation listeners
   * of a given type that apply to this content (are at or above it).
   * @param aContent the content to look for listeners for
   * @param aType the type of listener (NS_EVENT_BITS_MUTATION_*)
   * @return whether there are mutation listeners of the specified type for
   *         this content or not
   */
  static PRBool HasMutationListeners(nsIContent* aContent,
                                     PRUint32 aType);

  static PRBool ShouldFocus(nsIContent *aContent);

  /**
   * Actual implementation of the DOM InsertBefore and ReplaceChild methods.
   * Shared by nsDocument. When called from nsDocument, aParent will be null.
   *
   * @param aReplace  True if aNewChild should replace aRefChild. False if
   *                  aNewChild should be inserted before aRefChild.
   * @param aNewChild The child to insert
   * @param aRefChild The child to insert before or replace
   * @param aParent The parent to use for the new child
   * @param aDocument The document to use for the new child.
   *                  Must be non-null, if aParent is null and must match
   *                  aParent->GetCurrentDoc() if aParent is not null.
   * @param aChildArray The child array to work with
   * @param aReturn [out] the child we insert
   */
  static nsresult doReplaceOrInsertBefore(PRBool aReplace,
                                          nsIDOMNode* aNewChild,
                                          nsIDOMNode* aRefChild,
                                          nsIContent* aParent,
                                          nsIDocument* aDocument,
                                          nsAttrAndChildArray& aChildArray,
                                          nsIDOMNode** aReturn);

  static nsresult InitHashes();

  static PLDHashTable sEventListenerManagersHash;
  static PLDHashTable sRangeListsHash;

protected:
  /**
   * Copy attributes and children from another content object
   * @param aSrcContent the object to copy from
   * @param aDeep whether to copy children
   */
  nsresult CopyInnerTo(nsGenericElement* aDest, PRBool aDeep);

  /**
   * Internal hook for converting an attribute name-string to an atomized name
   */
  virtual const nsAttrName* InternalGetExistingAttrNameFromQName(const nsAString& aStr) const;

  PRBool HasDOMSlots() const
  {
    return !(mFlagsOrSlots & GENERIC_ELEMENT_DOESNT_HAVE_DOMSLOTS);
  }

  nsDOMSlots *GetDOMSlots()
  {
    if (!HasDOMSlots()) {
      nsDOMSlots *slots = new nsDOMSlots(mFlagsOrSlots);

      if (!slots) {
        return nsnull;
      }

      mFlagsOrSlots = NS_REINTERPRET_CAST(PtrBits, slots);
    }

    return NS_REINTERPRET_CAST(nsDOMSlots *, mFlagsOrSlots);
  }

  nsDOMSlots *GetExistingDOMSlots() const
  {
    if (!HasDOMSlots()) {
      return nsnull;
    }

    return NS_REINTERPRET_CAST(nsDOMSlots *, mFlagsOrSlots);
  }

  PtrBits GetFlags() const
  {
    if (HasDOMSlots()) {
      return NS_REINTERPRET_CAST(nsDOMSlots *, mFlagsOrSlots)->mFlags;
    }

    return mFlagsOrSlots;
  }

  void SetFlags(PtrBits aFlagsToSet)
  {
    NS_ASSERTION(!((aFlagsToSet & GENERIC_ELEMENT_CONTENT_ID_MASK) &&
                   (aFlagsToSet & ~GENERIC_ELEMENT_CONTENT_ID_MASK)),
                 "Whaaa, don't set content ID bits and flags together!!!");

    nsDOMSlots *slots = GetExistingDOMSlots();

    if (slots) {
      slots->mFlags |= aFlagsToSet;

      return;
    }

    mFlagsOrSlots |= aFlagsToSet;
  }

  void UnsetFlags(PtrBits aFlagsToUnset)
  {
    NS_ASSERTION(!((aFlagsToUnset & GENERIC_ELEMENT_CONTENT_ID_MASK) &&
                   (aFlagsToUnset & ~GENERIC_ELEMENT_CONTENT_ID_MASK)),
                 "Whaaa, don't set content ID bits and flags together!!!");

    nsDOMSlots *slots = GetExistingDOMSlots();

    if (slots) {
      slots->mFlags &= ~aFlagsToUnset;

      return;
    }

    mFlagsOrSlots &= ~aFlagsToUnset;
  }

  PRBool HasRangeList() const
  {
    PtrBits flags = GetFlags();

    return (flags & GENERIC_ELEMENT_HAS_RANGELIST && sRangeListsHash.ops);
  }

  PRBool HasEventListenerManager() const
  {
    PtrBits flags = GetFlags();

    return (flags & GENERIC_ELEMENT_HAS_LISTENERMANAGER &&
            sEventListenerManagersHash.ops);
  }

  PRBool HasProperties() const
  {
    PtrBits flags = GetFlags();

    return (flags & GENERIC_ELEMENT_HAS_PROPERTIES) != 0;
  }

  /**
   * GetContentsAsText will take all the textnodes that are children
   * of |this| and concatenate the text in them into aText.  It
   * completely ignores any non-text-node children of |this|; in
   * particular it does not descend into any children of |this| that
   * happen to be container elements.
   *
   * @param aText the resulting text [OUT]
   */
  void GetContentsAsText(nsAString& aText);

  /**
   * Information about this type of node
   */
  nsCOMPtr<nsINodeInfo> mNodeInfo;          // OWNER

  /**
   * Used for either storing flags for this element or a pointer to
   * this elements nsDOMSlots. See the definition of the
   * GENERIC_ELEMENT_* macros for the layout of the bits in this
   * member.
   */
  PtrBits mFlagsOrSlots;

  /**
   * Array containing all attributes and children for this element
   */
  nsAttrAndChildArray mAttrsAndChildren;
};

#define NS_FORWARD_NSIDOMNODE_NO_CLONENODE(_to)                               \
  NS_IMETHOD GetNodeName(nsAString& aNodeName) {                              \
    return _to GetNodeName(aNodeName);                                        \
  }                                                                           \
  NS_IMETHOD GetNodeValue(nsAString& aNodeValue) {                            \
    return _to GetNodeValue(aNodeValue);                                      \
  }                                                                           \
  NS_IMETHOD SetNodeValue(const nsAString& aNodeValue) {                      \
    return _to SetNodeValue(aNodeValue);                                      \
  }                                                                           \
  NS_IMETHOD GetNodeType(PRUint16* aNodeType) {                               \
    return _to GetNodeType(aNodeType);                                        \
  }                                                                           \
  NS_IMETHOD GetParentNode(nsIDOMNode** aParentNode) {                        \
    return _to GetParentNode(aParentNode);                                    \
  }                                                                           \
  NS_IMETHOD GetChildNodes(nsIDOMNodeList** aChildNodes) {                    \
    return _to GetChildNodes(aChildNodes);                                    \
  }                                                                           \
  NS_IMETHOD GetFirstChild(nsIDOMNode** aFirstChild) {                        \
    return _to GetFirstChild(aFirstChild);                                    \
  }                                                                           \
  NS_IMETHOD GetLastChild(nsIDOMNode** aLastChild) {                          \
    return _to GetLastChild(aLastChild);                                      \
  }                                                                           \
  NS_IMETHOD GetPreviousSibling(nsIDOMNode** aPreviousSibling) {              \
    return _to GetPreviousSibling(aPreviousSibling);                          \
  }                                                                           \
  NS_IMETHOD GetNextSibling(nsIDOMNode** aNextSibling) {                      \
    return _to GetNextSibling(aNextSibling);                                  \
  }                                                                           \
  NS_IMETHOD GetAttributes(nsIDOMNamedNodeMap** aAttributes) {                \
    return _to GetAttributes(aAttributes);                                    \
  }                                                                           \
  NS_IMETHOD GetOwnerDocument(nsIDOMDocument** aOwnerDocument) {              \
    return _to GetOwnerDocument(aOwnerDocument);                              \
  }                                                                           \
  NS_IMETHOD GetNamespaceURI(nsAString& aNamespaceURI) {                      \
    return _to GetNamespaceURI(aNamespaceURI);                                \
  }                                                                           \
  NS_IMETHOD GetPrefix(nsAString& aPrefix) {                                  \
    return _to GetPrefix(aPrefix);                                            \
  }                                                                           \
  NS_IMETHOD SetPrefix(const nsAString& aPrefix) {                            \
    return _to SetPrefix(aPrefix);                                            \
  }                                                                           \
  NS_IMETHOD GetLocalName(nsAString& aLocalName) {                            \
    return _to GetLocalName(aLocalName);                                      \
  }                                                                           \
  NS_IMETHOD InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild,       \
                          nsIDOMNode** aReturn) {                             \
    return _to InsertBefore(aNewChild, aRefChild, aReturn);                   \
  }                                                                           \
  NS_IMETHOD ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,       \
                          nsIDOMNode** aReturn) {                             \
    return _to ReplaceChild(aNewChild, aOldChild, aReturn);                   \
  }                                                                           \
  NS_IMETHOD RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn) {       \
    return _to RemoveChild(aOldChild, aReturn);                               \
  }                                                                           \
  NS_IMETHOD AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn) {       \
    return _to AppendChild(aNewChild, aReturn);                               \
  }                                                                           \
  NS_IMETHOD HasChildNodes(PRBool* aReturn) {                                 \
    return _to HasChildNodes(aReturn);                                        \
  }                                                                           \
  NS_IMETHOD Normalize() {                                                    \
    return _to Normalize();                                                   \
  }                                                                           \
  NS_IMETHOD IsSupported(const nsAString& aFeature,                           \
                         const nsAString& aVersion, PRBool* aReturn) {        \
    return _to IsSupported(aFeature, aVersion, aReturn);                      \
  }                                                                           \
  NS_IMETHOD HasAttributes(PRBool* aReturn) {                                 \
    return _to HasAttributes(aReturn);                                        \
  }                                                                           \
  NS_IMETHOD CloneNode(PRBool aDeep, nsIDOMNode** aReturn);

/**
 * Macros to implement CloneNode().
 */
#define NS_IMPL_DOM_CLONENODE(_elementName)                                 \
NS_IMPL_DOM_CLONENODE_AMBIGUOUS(_elementName, nsIDOMNode)

#define NS_IMPL_DOM_CLONENODE_AMBIGUOUS(_elementName, _implClass)           \
NS_IMETHODIMP                                                               \
_elementName::CloneNode(PRBool aDeep, nsIDOMNode **aResult)                 \
{                                                                           \
  *aResult = nsnull;                                                        \
                                                                            \
  _elementName *it = new _elementName(mNodeInfo);                           \
  if (!it) {                                                                \
    return NS_ERROR_OUT_OF_MEMORY;                                          \
  }                                                                         \
                                                                            \
  nsCOMPtr<nsIDOMNode> kungFuDeathGrip = NS_STATIC_CAST(_implClass*, it);   \
                                                                            \
  nsresult rv = CopyInnerTo(it, aDeep);                                     \
  if (NS_SUCCEEDED(rv)) {                                                   \
    kungFuDeathGrip.swap(*aResult);                                         \
  }                                                                         \
                                                                            \
  return rv;                                                                \
}

#define NS_IMPL_DOM_CLONENODE_WITH_INIT(_elementName)                       \
NS_IMETHODIMP                                                               \
_elementName::CloneNode(PRBool aDeep, nsIDOMNode **aResult)                 \
{                                                                           \
  *aResult = nsnull;                                                        \
                                                                            \
  _elementName *it = new _elementName(mNodeInfo);                           \
  if (!it) {                                                                \
    return NS_ERROR_OUT_OF_MEMORY;                                          \
  }                                                                         \
                                                                            \
  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);                                 \
                                                                            \
  nsresult rv = it->Init();                                                 \
                                                                            \
  rv |= CopyInnerTo(it, aDeep);                                             \
  if (NS_SUCCEEDED(rv)) {                                                   \
    kungFuDeathGrip.swap(*aResult);                                         \
  }                                                                         \
                                                                            \
  return rv;                                                                \
}

#endif /* nsGenericElement_h___ */
