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
 *   Alex Fritze <alex.fritze@crocodile-clips.com>
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

#include "nsSVGElement.h"
#include "nsIDocument.h"
#include "nsRange.h"
#include "nsIDOMAttr.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMMutationEvent.h"
#include "nsMutationEvent.h"
#include "nsIBindingManager.h"
#include "nsXBLBinding.h"
#include "nsStyleConsts.h"
#include "nsDOMError.h"
#include "nsIPresShell.h"
#include "nsIDOMViewCSS.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIServiceManager.h"
#include "nsIXBLService.h"
#include "nsSVGAtoms.h"
#include "nsLayoutAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsICSSStyleRule.h"
#include "nsISVGSVGElement.h"
#include "nsRuleWalker.h"
#include "nsCSSDeclaration.h"
#include "nsCSSProps.h"
#include "nsICSSParser.h"
#include "nsICSSLoader.h"
#include "nsGenericHTMLElement.h"
#include "nsNodeInfoManager.h"
#include "nsIScriptGlobalObject.h"

nsSVGElement::nsSVGElement(nsINodeInfo *aNodeInfo)
  : nsGenericElement(aNodeInfo)
{

}

nsSVGElement::~nsSVGElement()
{
  PRUint32 i, count = mMappedAttributes.AttrCount();
  for (i = 0; i < count; ++i) {
    mMappedAttributes.AttrAt(i)->GetSVGValue()->RemoveObserver(this);
  }
}

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGElement,nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsSVGElement,nsGenericElement)

NS_INTERFACE_MAP_BEGIN(nsSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIXMLContent)
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOM3Node, new nsNode3Tearoff(this))
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
  NS_INTERFACE_MAP_ENTRY(nsISVGContent)
// provided by nsGenericElement:
//  NS_INTERFACE_MAP_ENTRY(nsIStyledContent)
//  NS_INTERFACE_MAP_ENTRY(nsIContent)
NS_INTERFACE_MAP_END_INHERITING(nsGenericElement)

//----------------------------------------------------------------------
// Implementation
  
//----------------------------------------------------------------------
// nsIContent methods

nsresult
nsSVGElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                         nsIContent* aBindingParent,
                         PRBool aCompileEventHandlers)
{
  nsresult rv = nsGenericElement::BindToTree(aDocument, aParent,
                                             aBindingParent,
                                             aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  ParentChainChanged();

  return rv;
}

void
nsSVGElement::UnbindFromTree(PRBool aDeep, PRBool aNullParent)
{
  nsGenericElement::UnbindFromTree(aDeep, aNullParent);
  ParentChainChanged();
}

nsIAtom *
nsSVGElement::GetIDAttributeName() const
{
  return nsSVGAtoms::id;
}

nsIAtom *
nsSVGElement::GetClassAttributeName() const
{
  return nsSVGAtoms::_class;
}

nsresult
nsSVGElement::SetAttr(PRInt32 aNamespaceID, nsIAtom* aName, nsIAtom* aPrefix,
                      const nsAString& aValue,
                      PRBool aNotify)
{
  NS_ENSURE_ARG_POINTER(aName);
  NS_ASSERTION(aNamespaceID != kNameSpaceID_Unknown,
               "Don't call SetAttr with unknown namespace");
  
  nsresult rv;
  nsAutoString oldValue;
  PRBool hasListeners = PR_FALSE;
  PRBool modification = PR_FALSE;
  
  PRInt32 index = mAttrsAndChildren.IndexOfAttr(aName, aNamespaceID);
  
  if (IsInDoc()) {
    hasListeners = nsGenericElement::HasMutationListeners(this,
      NS_EVENT_BITS_MUTATION_ATTRMODIFIED);
    
    // If we have no listeners and aNotify is false, we are almost certainly
    // coming from the content sink and will almost certainly have no previous
    // value.  Even if we do, setting the value is cheap when we have no
    // listeners and don't plan to notify.  The check for aNotify here is an
    // optimization, the check for haveListeners is a correctness issue.
    if (index >= 0 && (hasListeners || aNotify)) {
      modification = PR_TRUE;
      // don't do any update if old == new.
      mAttrsAndChildren.AttrAt(index)->ToString(oldValue);
      if (aValue.Equals(oldValue) &&
          aPrefix == mAttrsAndChildren.GetSafeAttrNameAt(index)->GetPrefix()) {
        return NS_OK;
      }
    }
  }
  
  // If this is an svg presentation attribute we need to map it into
  // the content stylerule.
  // XXX For some reason incremental mapping doesn't work, so for now
  // just delete the style rule and lazily reconstruct it in
  // GetContentStyleRule()
  if(aNamespaceID == kNameSpaceID_None && IsAttributeMapped(aName)) 
    mContentStyleRule = nsnull;
  
  // Parse value
  nsAttrValue attrValue;
  nsCOMPtr<nsISVGValue> svg_value;
  if (index >= 0) {
    // Found the attr in the list.
    const nsAttrValue* currVal = mAttrsAndChildren.AttrAt(index);
    if (currVal->Type() == nsAttrValue::eSVGValue) {
      svg_value = currVal->GetSVGValue();
    }
  }
  else {
    // Could be a mapped attribute.
    svg_value = GetMappedAttribute(aNamespaceID, aName);
  }
  
  if (svg_value) {
    if (NS_FAILED(svg_value->SetValueString(aValue))) {
      // The value was rejected. This happens e.g. in a XUL template
      // when trying to set a value like "?x" on a value object that
      // expects a length.
      // To accomodate this "erronous" value, we'll insert a proxy
      // object between ourselves and the actual value object:
      nsCOMPtr<nsISVGValue> proxy;
      rv = NS_CreateSVGStringProxyValue(svg_value, getter_AddRefs(proxy));
      NS_ENSURE_SUCCESS(rv, rv);

      svg_value->RemoveObserver(this);
      proxy->SetValueString(aValue);
      proxy->AddObserver(this);
      attrValue.SetTo(proxy);
    }
    else {
      attrValue.SetTo(svg_value);
    }
  }
  else if (aName == nsSVGAtoms::style && aNamespaceID == kNameSpaceID_None) {
    nsGenericHTMLElement::ParseStyleAttribute(this, PR_TRUE, aValue, attrValue);
  }
  // We don't have an nsISVGValue attribute.
  else if (aName == nsSVGAtoms::id && aNamespaceID == kNameSpaceID_None){
    attrValue.ParseAtom(aValue);
  } 
  else {
    attrValue.SetTo(aValue);
  }

  if (aNamespaceID == kNameSpaceID_None && IsEventName(aName)) {
    nsCOMPtr<nsIEventListenerManager> manager;
    GetListenerManager(getter_AddRefs(manager));
    if (manager) {
      nsIAtom* eventName = GetEventNameForAttr(aName);
      nsIDocument *ownerDoc = GetOwnerDoc();
      manager->AddScriptEventListener(NS_STATIC_CAST(nsIContent*, this), eventName,
                                      aValue, PR_TRUE,
                                      !nsContentUtils::IsChromeDoc(ownerDoc));
    }
  }
  
  return SetAttrAndNotify(aNamespaceID, aName, aPrefix, oldValue, attrValue,
                          modification, hasListeners, aNotify);
}

nsresult
nsSVGElement::UnsetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                        PRBool aNotify)
{
  if (aNamespaceID == kNameSpaceID_None && IsEventName(aName)) {
    nsCOMPtr<nsIEventListenerManager> manager;
    GetListenerManager(getter_AddRefs(manager));
    if (manager) {
      nsIAtom* eventName = GetEventNameForAttr(aName);
      manager->RemoveScriptEventListener(eventName);
    }
  }

  return nsGenericElement::UnsetAttr(aNamespaceID, aName, aNotify);
}

PRBool
nsSVGElement::IsContentOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~(eELEMENT | eSVG));
}

//----------------------------------------------------------------------
// nsIStyledContent methods

NS_IMETHODIMP
nsSVGElement::WalkContentStyleRules(nsRuleWalker* aRuleWalker)
{
#ifdef DEBUG
//  printf("nsSVGElement(%p)::WalkContentStyleRules()\n", this);
#endif
  if (!mContentStyleRule)
    UpdateContentStyleRule();

  if (mContentStyleRule)  
    aRuleWalker->Forward(mContentStyleRule);

  return NS_OK;
}

NS_IMETHODIMP
nsSVGElement::SetInlineStyleRule(nsICSSStyleRule* aStyleRule, PRBool aNotify)
{
  PRBool hasListeners = PR_FALSE;
  PRBool modification = PR_FALSE;
  nsAutoString oldValueStr;

  if (IsInDoc()) {
    hasListeners = nsGenericElement::HasMutationListeners(this,
      NS_EVENT_BITS_MUTATION_ATTRMODIFIED);

    // There's no point in comparing the stylerule pointers since we're always
    // getting a new stylerule here. And we can't compare the stringvalues of
    // the old and the new rules since both will point to the same declaration
    // and thus will be the same.
    if (hasListeners || aNotify) {
      // save the old attribute so we can set up the mutation event properly
      const nsAttrValue* value = mAttrsAndChildren.GetAttr(nsSVGAtoms::style);
      if (value) {
        modification = PR_TRUE;
        if (hasListeners) {
          value->ToString(oldValueStr);
        }
      }
    }
  }

  nsAttrValue attrValue(aStyleRule);

  return SetAttrAndNotify(kNameSpaceID_None, nsSVGAtoms::style, nsnull,
                          oldValueStr, attrValue, modification, hasListeners,
                          aNotify);
}

nsICSSStyleRule*
nsSVGElement::GetInlineStyleRule()
{
  const nsAttrValue* attrVal = mAttrsAndChildren.GetAttr(nsSVGAtoms::style);

  if (attrVal && attrVal->Type() == nsAttrValue::eCSSStyleRule) {
    return attrVal->GetCSSStyleRuleValue();
  }

  return nsnull;
}

// PresentationAttributes-FillStroke
/* static */ const nsGenericElement::MappedAttributeEntry
nsSVGElement::sFillStrokeMap[] = {
  { &nsSVGAtoms::fill },
  { &nsSVGAtoms::fill_opacity },
  { &nsSVGAtoms::fill_rule },
  { &nsSVGAtoms::stroke },
  { &nsSVGAtoms::stroke_dasharray },
  { &nsSVGAtoms::stroke_dashoffset },
  { &nsSVGAtoms::stroke_linecap },
  { &nsSVGAtoms::stroke_linejoin },
  { &nsSVGAtoms::stroke_miterlimit },
  { &nsSVGAtoms::stroke_opacity },
  { &nsSVGAtoms::stroke_width },
  { nsnull }
};

// PresentationAttributes-Graphics
/* static */ const nsGenericElement::MappedAttributeEntry
nsSVGElement::sGraphicsMap[] = {
  { &nsSVGAtoms::clip_path },
  { &nsSVGAtoms::clip_rule },
  { &nsSVGAtoms::cursor },
  { &nsSVGAtoms::display },
  { &nsSVGAtoms::filter },
  { &nsSVGAtoms::image_rendering },
  { &nsSVGAtoms::mask },
  { &nsSVGAtoms::opacity },
  { &nsSVGAtoms::pointer_events },
  { &nsSVGAtoms::shape_rendering },
  { &nsSVGAtoms::text_rendering },
  { &nsSVGAtoms::visibility },
  { nsnull }
};

// PresentationAttributes-TextContentElements
/* static */ const nsGenericElement::MappedAttributeEntry
nsSVGElement::sTextContentElementsMap[] = {
  { &nsSVGAtoms::alignment_baseline },
  { &nsSVGAtoms::baseline_shift },
  { &nsSVGAtoms::direction },
  { &nsSVGAtoms::dominant_baseline },
  { &nsSVGAtoms::glyph_orientation_horizontal },
  { &nsSVGAtoms::glyph_orientation_vertical },
  { &nsSVGAtoms::kerning },
  { &nsSVGAtoms::letter_spacing },
  { &nsSVGAtoms::text_anchor },
  { &nsSVGAtoms::text_decoration },
  { &nsSVGAtoms::unicode_bidi },
  { &nsSVGAtoms::word_spacing },
  { nsnull }
};

// PresentationAttributes-FontSpecification
/* static */ const nsGenericElement::MappedAttributeEntry
nsSVGElement::sFontSpecificationMap[] = {
  { &nsSVGAtoms::font_family },
  { &nsSVGAtoms::font_size },
  { &nsSVGAtoms::font_size_adjust },
  { &nsSVGAtoms::font_stretch },
  { &nsSVGAtoms::font_style },
  { &nsSVGAtoms::font_variant },
  { &nsSVGAtoms::font_weight },  
  { nsnull }
};

// PresentationAttributes-GradientStop
/* static */ const nsGenericElement::MappedAttributeEntry
nsSVGElement::sGradientStopMap[] = {
  { &nsSVGAtoms::stop_color },
  { &nsSVGAtoms::stop_opacity },
  { nsnull }
};

// PresentationAttributes-Viewports
/* static */ const nsGenericElement::MappedAttributeEntry
nsSVGElement::sViewportsMap[] = {
  { &nsSVGAtoms::overflow },
  { &nsSVGAtoms::clip },
  { nsnull }
};

// PresentationAttributes-Makers
/* static */ const nsGenericElement::MappedAttributeEntry
nsSVGElement::sMarkersMap[] = {
  { &nsSVGAtoms::marker_end },
  { &nsSVGAtoms::marker_mid },
  { &nsSVGAtoms::marker_start },
  { nsnull }
};

// PresentationAttributes-Color
/* static */ const nsGenericElement::MappedAttributeEntry
nsSVGElement::sColorMap[] = {
  { &nsSVGAtoms::color },
  { nsnull }
};


//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMETHODIMP
nsSVGElement::IsSupported(const nsAString& aFeature, const nsAString& aVersion, PRBool* aReturn)
{
  NS_NOTYETIMPLEMENTED("nsSVGElement::IsSupported");
  return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------
// nsIDOMElement methods

// forwarded to nsGenericElement implementations


//----------------------------------------------------------------------
// nsIDOMSVGElement methods

/* attribute DOMString id; */
NS_IMETHODIMP nsSVGElement::GetId(nsAString & aId)
{
  return GetAttr(kNameSpaceID_None, nsSVGAtoms::id, aId);
}

NS_IMETHODIMP nsSVGElement::SetId(const nsAString & aId)
{
  return SetAttr(kNameSpaceID_None, nsSVGAtoms::id, aId, PR_TRUE);
}

/* readonly attribute nsIDOMSVGSVGElement ownerSVGElement; */
NS_IMETHODIMP
nsSVGElement::GetOwnerSVGElement(nsIDOMSVGSVGElement * *aOwnerSVGElement)
{
  *aOwnerSVGElement = nsnull;

  nsIBindingManager *bindingManager = nsnull;
  // XXXbz I _think_ this is right.  We want to be using the binding manager
  // that would have attached the binding that gives us our anonymous parent.
  // That's the binding manager for the document we actually belong to, which
  // is our owner doc.
  nsIDocument* ownerDoc = GetOwnerDoc();
  if (ownerDoc) {
    bindingManager = ownerDoc->BindingManager();
  }

  nsCOMPtr<nsIContent> parent;
  
  if (bindingManager) {
    // we have a binding manager -- do we have an anonymous parent?
    bindingManager->GetInsertionParent(this, getter_AddRefs(parent));
  }

  if (!parent) {
    // if we didn't find an anonymous parent, use the explicit one,
    // whether it's null or not...
    parent = GetParent();
  }

  while (parent) {    
    nsCOMPtr<nsIDOMSVGSVGElement> SVGSVGElement = do_QueryInterface(parent);
    if (SVGSVGElement) {
      *aOwnerSVGElement = SVGSVGElement;
      NS_ADDREF(*aOwnerSVGElement);
      return NS_OK;
    }
    nsCOMPtr<nsIContent> next;

    if (bindingManager) {
      bindingManager->GetInsertionParent(parent, getter_AddRefs(next));
    }

    if (!next) {
      // no anonymous parent, so use explicit one
      next = parent->GetParent();
    }
    
    parent = next;
  }

  // we don't have a parent SVG element...

  // are _we_ the outermost SVG element? If yes, return nsnull, but don't fail
  nsCOMPtr<nsIDOMSVGSVGElement> SVGSVGElement = do_QueryInterface((nsIDOMSVGElement*)this);
  if (SVGSVGElement) return NS_OK;
  
  // no owner found and we aren't the outermost SVG element either.
  // this situation can e.g. occur during content tree teardown. 
  return NS_ERROR_FAILURE;
}

/* readonly attribute nsIDOMSVGElement viewportElement; */
NS_IMETHODIMP
nsSVGElement::GetViewportElement(nsIDOMSVGElement * *aViewportElement)
{
  *aViewportElement = nsnull;
  nsCOMPtr<nsIDOMSVGSVGElement> SVGSVGElement;
  nsresult rv = GetOwnerSVGElement(getter_AddRefs(SVGSVGElement));
  NS_ENSURE_SUCCESS(rv,rv);
  if (SVGSVGElement) {
    nsCOMPtr<nsIDOMSVGElement> SVGElement = do_QueryInterface(SVGSVGElement);
    *aViewportElement = SVGElement;
    NS_IF_ADDREF(*aViewportElement);
  }
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods:

NS_IMETHODIMP
nsSVGElement::WillModifySVGObservable(nsISVGValue* observable,
                                      nsISVGValue::modificationType aModType)
{
  return NS_OK;
}


NS_IMETHODIMP
nsSVGElement::DidModifySVGObservable(nsISVGValue* aObservable,
                                     nsISVGValue::modificationType aModType)
{
  // Return without setting DOM attributes as markup attributes if the
  // attribute's element is being inserted into an SVG document fragment,
  // which provides a context which percentage lengths are relative to.
  // Bug 274886
  if (aModType == nsISVGValue::mod_context)
    return NS_OK;

  PRUint32 i, count = mMappedAttributes.AttrCount();
  const nsAttrValue* attrValue = nsnull;
  for (i = 0; i < count; ++i) {
    attrValue = mMappedAttributes.AttrAt(i);
    if (attrValue->GetSVGValue() == aObservable) {
      break;
    }
  }

  if (i == count) {
    NS_NOTREACHED("unknown nsISVGValue");

    return NS_ERROR_UNEXPECTED;
  }

  const nsAttrName* attrName = mMappedAttributes.GetSafeAttrNameAt(i);
  PRBool modification = PR_FALSE;
  PRBool hasListeners = PR_FALSE;
  if (IsInDoc()) {
    modification = !!mAttrsAndChildren.GetAttr(attrName->LocalName(),
                                               attrName->NamespaceID());
    hasListeners = nsGenericElement::HasMutationListeners(this,
      NS_EVENT_BITS_MUTATION_ATTRMODIFIED);
  }

  nsAttrValue newValue(aObservable);

  return SetAttrAndNotify(attrName->NamespaceID(), attrName->LocalName(),
                          attrName->GetPrefix(), EmptyString(), newValue,
                          modification, hasListeners, PR_TRUE);
}

//----------------------------------------------------------------------
// nsISVGContent methods:

// recursive helper to call ParentChainChanged across non-SVG elements
static void CallParentChainChanged(nsIContent*elem)
{
  NS_ASSERTION(elem, "null element");
  
  PRUint32 count = elem->GetChildCount();
  for (PRUint32 i=0; i<count; ++i) {
    nsIContent* child = elem->GetChildAt(i);

    nsCOMPtr<nsISVGContent> svgChild = do_QueryInterface(child);
    if (svgChild) {
      svgChild->ParentChainChanged();
    }
    else {
      // non-svg element might have an svg child, so recurse
      CallParentChainChanged(child);
    }
  }
}

void
nsSVGElement::ParentChainChanged()
{
  CallParentChainChanged(this);
}


//----------------------------------------------------------------------
// Implementation Helpers:

PRBool
nsSVGElement::IsEventName(nsIAtom* aName)
{
  return PR_FALSE;
}

nsresult
nsSVGElement::SetAttrAndNotify(PRInt32 aNamespaceID, nsIAtom* aAttribute,
                               nsIAtom* aPrefix, const nsAString& aOldValue,
                               nsAttrValue& aParsedValue, PRBool aModification,
                               PRBool aFireMutation, PRBool aNotify)
{
  nsresult rv;
  PRUint8 modType = aModification ?
    NS_STATIC_CAST(PRUint8, nsIDOMMutationEvent::MODIFICATION) :
    NS_STATIC_CAST(PRUint8, nsIDOMMutationEvent::ADDITION);

  nsIDocument* document = GetCurrentDoc();
  mozAutoDocUpdate updateBatch(document, UPDATE_CONTENT_MODEL, aNotify);
  if (aNotify && document) {
    document->AttributeWillChange(this, aNamespaceID, aAttribute);
  }

  if (aNamespaceID == kNameSpaceID_None) {
    // XXX doesn't check IsAttributeMapped here.
    rv = mAttrsAndChildren.SetAndTakeAttr(aAttribute, aParsedValue);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    nsCOMPtr<nsINodeInfo> ni;
    rv = mNodeInfo->NodeInfoManager()->GetNodeInfo(aAttribute, aPrefix,
                                                   aNamespaceID,
                                                   getter_AddRefs(ni));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mAttrsAndChildren.SetAndTakeAttr(ni, aParsedValue);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (document) {
    nsXBLBinding *binding = document->BindingManager()->GetBinding(this);
    if (binding) {
      binding->AttributeChanged(aAttribute, aNamespaceID, PR_FALSE, aNotify);
    }

    if (aFireMutation) {
      nsCOMPtr<nsIDOMEventTarget> node = do_QueryInterface(NS_STATIC_CAST(nsIContent *, this));
      nsMutationEvent mutation(PR_TRUE, NS_MUTATION_ATTRMODIFIED, node);

      nsAutoString attrName;
      aAttribute->ToString(attrName);
      nsCOMPtr<nsIDOMAttr> attrNode;
      GetAttributeNode(attrName, getter_AddRefs(attrNode));
      mutation.mRelatedNode = attrNode;

      mutation.mAttrName = aAttribute;
      nsAutoString newValue;
      // We don't really need to call GetAttr here, but lets try to keep this
      // code as similar to nsGenericHTMLElement::SetAttrAndNotify as possible
      // so that they can merge sometime in the future.
      GetAttr(aNamespaceID, aAttribute, newValue);
      if (!newValue.IsEmpty()) {
        mutation.mNewAttrValue = do_GetAtom(newValue);
      }
      if (!aOldValue.IsEmpty()) {
        mutation.mPrevAttrValue = do_GetAtom(aOldValue);
      }
      mutation.mAttrChange = modType;
      nsEventStatus status = nsEventStatus_eIgnore;
      HandleDOMEvent(nsnull, &mutation, nsnull,
                     NS_EVENT_FLAG_INIT, &status);
    }

    if (aNotify) {
      document->AttributeChanged(this, aNamespaceID, aAttribute, modType);
    }
  }

  if (aNamespaceID == kNameSpaceID_XMLEvents && 
      aAttribute == nsHTMLAtoms::_event && GetOwnerDoc()) {
    GetOwnerDoc()->AddXMLEventsContent(this);
  }

  return NS_OK;
}

void
nsSVGElement::UpdateContentStyleRule()
{
  NS_ASSERTION(!mContentStyleRule, "we already have a content style rule");
  
  // Bail early if there are no child attributes.
  // If this code were moved into the for-loop we could
  // avoid creating an mDeclaration even when there 
  // are non-style related attributes. See bug 270251.
  if (mAttrsAndChildren.AttrCount() == 0)
    return;

  nsIDocument* doc = GetOwnerDoc();
  if (!doc) {
    NS_ERROR("SVG element without owner document");
    return;
  }
  
  nsCOMPtr<nsIURI> baseURI = GetBaseURI();
  nsIURI *docURI = doc->GetDocumentURI();
  
  // Create the nsCSSDeclaration.
  nsCSSDeclaration* declaration = new nsCSSDeclaration();
  if (!declaration) {
    NS_WARNING("Failed to allocate nsCSSDeclaration");
    return;
  }
  if(!declaration->InitializeEmpty()) {
    NS_WARNING("could not initialize nsCSSDeclaration");
    declaration->RuleAbort();
    return;
  }

  // Try to fetch the CSS Parser from the document.
  nsICSSLoader* cssLoader = doc->CSSLoader();
  
  nsCOMPtr<nsICSSParser> parser;
  nsresult rv = NS_OK; 
  rv = cssLoader->GetParserFor(nsnull, getter_AddRefs(parser));
  
  if (NS_FAILED(rv)) {
    NS_WARNING("failed to get a css parser");
    declaration->RuleAbort();
    return;
  }

  // SVG and CSS differ slightly in their interpretation of some of
  // the attributes.  SVG allows attributes of the form: font-size="5" 
  // (style="font-size: 5" if using a style attribute)
  // where CSS requires units: font-size="5pt" (style="font-size: 5pt")
  // Set a flag to pass information to the parser so that we can use
  // the CSS parser to parse the font-size attribute.  Note that this
  // does *not* effect the use of CSS stylesheets, which will still
  // require units
  parser->SetSVGMode(PR_TRUE);

  PRUint32 attrCount = mAttrsAndChildren.AttrCount();
  for (PRUint32 i = 0; i < attrCount; ++i) {
    const nsAttrName* attrName = mAttrsAndChildren.GetSafeAttrNameAt(i);
    if (!attrName->IsAtom() || !IsAttributeMapped(attrName->Atom()))
      continue;

    nsAutoString name;
    attrName->Atom()->ToString(name);

    nsAutoString value;
    mAttrsAndChildren.AttrAt(i)->ToString(value);

    PRBool changed;
    parser->ParseProperty(nsCSSProps::LookupProperty(name), value,
                          docURI, baseURI,
                          declaration, &changed);
  }
  
  rv = NS_NewCSSStyleRule(getter_AddRefs(mContentStyleRule), nsnull, declaration);
  if (NS_FAILED(rv)) {
    NS_WARNING("could not create contentstylerule");
    declaration->RuleAbort();
  }
  
  // Recycle the parser
  parser->SetSVGMode(PR_FALSE);
  cssLoader->RecycleParser(parser);
}

nsISVGValue*
nsSVGElement::GetMappedAttribute(PRInt32 aNamespaceID, nsIAtom* aName)
{
  const nsAttrValue* attrVal = mMappedAttributes.GetAttr(aName, aNamespaceID);
  if (!attrVal)
    return nsnull;

  return attrVal->GetSVGValue();
}

nsresult
nsSVGElement::AddMappedSVGValue(nsIAtom* aName, nsISupports* aValue,
                                PRInt32 aNamespaceID)
{
  nsresult rv;
  nsCOMPtr<nsISVGValue> svg_value = do_QueryInterface(aValue);
  svg_value->AddObserver(this);
  nsAttrValue attrVal(svg_value);

  if (aNamespaceID == kNameSpaceID_None) {
    rv = mMappedAttributes.SetAndTakeAttr(aName, attrVal);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    nsCOMPtr<nsINodeInfo> ni;
    rv = mNodeInfo->NodeInfoManager()->GetNodeInfo(aName, nsnull,
                                                   aNamespaceID,
                                                   getter_AddRefs(ni));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mMappedAttributes.SetAndTakeAttr(ni, attrVal);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

/* static */
PRBool
nsSVGElement::IsGraphicElementEventName(nsIAtom* aName)
{
  const char* name;
  aName->GetUTF8String(&name);

  if (name[0] != 'o' || name[1] != 'n') {
    return PR_FALSE;
  }

  return (aName == nsSVGAtoms::onabort     ||
          aName == nsSVGAtoms::onclick     ||
          aName == nsSVGAtoms::onerror     ||
          aName == nsSVGAtoms::onload      ||
          aName == nsSVGAtoms::onmousedown ||
          aName == nsSVGAtoms::onmouseup   ||
          aName == nsSVGAtoms::onmouseover ||
          aName == nsSVGAtoms::onmousemove ||
          aName == nsSVGAtoms::onmouseout);
}

/* static */
nsIAtom* nsSVGElement::GetEventNameForAttr(nsIAtom* aAttr)
{
  if (aAttr == nsSVGAtoms::onload)
    return nsLayoutAtoms::onSVGLoad;
  if (aAttr == nsSVGAtoms::onunload)
    return nsLayoutAtoms::onSVGUnload;
  if (aAttr == nsSVGAtoms::onabort)
    return nsLayoutAtoms::onSVGAbort;
  if (aAttr == nsSVGAtoms::onerror)
    return nsLayoutAtoms::onSVGError;
  if (aAttr == nsSVGAtoms::onresize)
    return nsLayoutAtoms::onSVGResize;
  if (aAttr == nsSVGAtoms::onscroll)
    return nsLayoutAtoms::onSVGScroll;
  if (aAttr == nsSVGAtoms::onzoom)
    return nsLayoutAtoms::onSVGZoom;

  return aAttr;
}
