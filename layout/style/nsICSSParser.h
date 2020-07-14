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
 * The Original Code is mozilla.org code.
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
#ifndef nsCSS1Parser_h___
#define nsCSS1Parser_h___

#include "nsISupports.h"
#include "nsAString.h"
#include "nsCSSProperty.h"
#include "nsColor.h"

class nsICSSStyleRule;
class nsICSSStyleSheet;
class nsIUnicharInputStream;
class nsIURI;
class nsCSSDeclaration;
class nsICSSLoader;
class nsICSSRule;
class nsISupportsArray;
class nsMediaList;

#define NS_ICSS_PARSER_IID    \
{ 0x94d1d921, 0xd6f6, 0x435f, \
  {0xa5, 0xe8, 0x85, 0x3f, 0x6e, 0x34, 0x57, 0xf6} }

// Rule processing function
typedef void (*PR_CALLBACK RuleAppendFunc) (nsICSSRule* aRule, void* aData);

// Interface to the css parser.
class nsICSSParser : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICSS_PARSER_IID)

  // Set a style sheet for the parser to fill in. The style sheet must
  // implement the nsICSSStyleSheet interface
  NS_IMETHOD SetStyleSheet(nsICSSStyleSheet* aSheet) = 0;

  // Set whether or not tags & classes are case sensitive or uppercased
  NS_IMETHOD SetCaseSensitive(PRBool aCaseSensitive) = 0;

  // Set whether or not to emulate Nav quirks
  NS_IMETHOD SetQuirkMode(PRBool aQuirkMode) = 0;

#ifdef  MOZ_SVG
  // Set whether or not we are in an SVG element
  NS_IMETHOD SetSVGMode(PRBool aSVGMode) = 0;
#endif

  // Set loader to use for child sheets
  NS_IMETHOD SetChildLoader(nsICSSLoader* aChildLoader) = 0;

  NS_IMETHOD Parse(nsIUnicharInputStream* aInput,
                   nsIURI*                aSheetURL,
                   nsIURI*                aBaseURI,
                   PRUint32               aLineNumber,
                   nsICSSStyleSheet*&     aResult) = 0;

  // Parse HTML style attribute or its equivalent in other markup
  // languages.  aBaseURL is the base url to use for relative links in
  // the declaration.
  NS_IMETHOD ParseStyleAttribute(const nsAString&         aAttributeValue,
                                 nsIURI*                  aDocURL,
                                 nsIURI*                  aBaseURL,
                                 nsICSSStyleRule**        aResult) = 0;

  NS_IMETHOD ParseAndAppendDeclaration(const nsAString&         aBuffer,
                                       nsIURI*                  aSheetURL,
                                       nsIURI*                  aBaseURL,
                                       nsCSSDeclaration*        aDeclaration,
                                       PRBool                   aParseOnlyOneDecl,
                                       PRBool*                  aChanged,
                                       PRBool                   aClearOldDecl) = 0;

  NS_IMETHOD ParseRule(const nsAString&   aRule,
                       nsIURI*            aSheetURL,
                       nsIURI*            aBaseURL,
                       nsISupportsArray** aResult) = 0;

  NS_IMETHOD ParseProperty(const nsCSSProperty aPropID,
                           const nsAString& aPropValue,
                           nsIURI* aSheetURL,
                           nsIURI* aBaseURL,
                           nsCSSDeclaration* aDeclaration,
                           PRBool* aChanged) = 0;

  /**
   * Parse aBuffer into a media list |aMediaList|, which must be
   * non-null, replacing its current contents.  If aHTMLMode is true,
   * parse according to HTML rules, with commas as the most important
   * delimiter.  Otherwise, parse according to CSS rules, with
   * parentheses and strings more important than commas.
   */
  NS_IMETHOD ParseMediaList(const nsSubstring& aBuffer,
                            nsIURI* aURL, // for error reporting
                            PRUint32 aLineNumber, // for error reporting
                            nsMediaList* aMediaList,
                            PRBool aHTMLMode) = 0;

  /**
   * Parse aBuffer into a nscolor |aColor|.  If aHandleAlphaColors is
   * set, handle rgba()/hsla(). Will return NS_ERROR_FAILURE if
   * aBuffer is not a valid CSS color specification.
   *
   * Will also currently return NS_ERROR_FAILURE if it is not
   * self-contained (i.e.  doesn't reference any external style state,
   * such as "initial" or "inherit").
   */
  NS_IMETHOD ParseColorString(const nsSubstring& aBuffer,
                              nsIURI* aURL, // for error reporting
                              PRUint32 aLineNumber, // for error reporting
                              PRBool aHandleAlphaColors,
                              nscolor* aColor) = 0;
};

// IID for the nsICSSParser_MOZILLA_1_8_BRANCH interface
// fcef7cd9-9ac4-4283-91f6-8eea74c15892
#define NS_ICSS_PARSER_MOZILLA_1_8_BRANCH_IID     \
{0xfcef7cd9, 0x9ac4, 0x4283, {0x91, 0xf6, 0x8e, 0xea, 0x74, 0xc1, 0x58, 0x92}}

class nsICSSParser_MOZILLA_1_8_BRANCH : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICSS_PARSER_MOZILLA_1_8_BRANCH_IID)

  /**
   * @param aAllowUnsafeRules see aEnableUnsafeRules in
   * nsICSSLoader::LoadSheetSync
   */
  NS_IMETHOD Parse(nsIUnicharInputStream* aInput,
                   nsIURI*                aSheetURL,
                   nsIURI*                aBaseURI,
                   PRUint32               aLineNumber,
                   PRBool                 aAllowUnsafeRules,
                   nsICSSStyleSheet*&     aResult) = 0;
};

nsresult
NS_NewCSSParser(nsICSSParser** aInstancePtrResult);

#endif /* nsCSS1Parser_h___ */
